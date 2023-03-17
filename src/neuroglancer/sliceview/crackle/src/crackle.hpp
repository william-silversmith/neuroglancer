#ifndef __CRACKLE_HXX__
#define __CRACKLE_HXX__

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>

#include "robin_hood.hpp"

#include "cc3d.hpp"
#include "header.hpp"
#include "crackcodes.hpp"
#include "lib.hpp"
#include "labels.hpp"
#include "pins.hpp"
#include "markov.hpp"

namespace crackle {


std::vector<uint64_t> get_crack_code_offsets(
	const CrackleHeader &header,
	const std::vector<unsigned char> &binary,
	int& err
) {
	uint64_t offset = CrackleHeader::header_size;

	const uint64_t z_width = sizeof(uint32_t);
	const uint64_t zindex_bytes = z_width * header.sz;

	std::vector<uint64_t> z_index(header.sz + 1);

	if (offset + zindex_bytes > binary.size()) {
		err = 20;
		return z_index;
	}
	const unsigned char* buf = binary.data();

	for (uint64_t z = 0; z < header.sz; z++) {
		z_index[z+1] = lib::ctoi<uint32_t>(buf, offset + z_width * z);
	}
	for (uint64_t z = 0; z < header.sz; z++) {
		z_index[z+1] += z_index[z];
	}

	uint64_t markov_model_offset = 0;
	if (header.markov_model_order > 0) {
		markov_model_offset = header.markov_model_bytes();
	}

	for (uint64_t i = 0; i < header.sz + 1; i++) {
		z_index[i] += (
			offset + zindex_bytes + 
			header.num_label_bytes + markov_model_offset
		);
	}
	return z_index;
}

std::vector<std::vector<unsigned char>> get_crack_codes(
	const CrackleHeader &header,
	const std::vector<unsigned char> &binary,
	const uint64_t z_start, const uint64_t z_end,
	int& err
) {
	std::vector<uint64_t> z_index = get_crack_code_offsets(header, binary, err);

	if (err > 0) {
		return std::vector<std::vector<unsigned char>>();
	}

	if (z_index.back() > binary.size()) {
		err = 30;
		return std::vector<std::vector<unsigned char>>();
	}

	std::vector<std::vector<unsigned char>> crack_codes(z_end - z_start);

	for (uint64_t z = z_start; z < z_end; z++) {
		uint64_t code_size = z_index[z+1] - z_index[z];
		std::vector<unsigned char> code;
		code.reserve(code_size);
		for (uint64_t i = z_index[z]; i < z_index[z+1]; i++) {
			code.push_back(binary[i]);
		}
		crack_codes[z - z_start] = std::move(code);
	}

	return crack_codes;
}

std::vector<std::vector<uint8_t>> decode_markov_model(
	const CrackleHeader &header,
	const std::vector<unsigned char> &binary
) {
	if (header.markov_model_order == 0) {
		return std::vector<std::vector<uint8_t>>();
	}

	uint64_t model_offset = header.header_size + header.num_label_bytes;
	const uint64_t z_width = sizeof(uint32_t);
	const uint64_t zindex_bytes = z_width * header.sz;
	model_offset += zindex_bytes;

	std::vector<unsigned char> stored_model(
		binary.begin() + model_offset,
		binary.begin() + model_offset + header.markov_model_bytes()
	);
	return crackle::markov::from_stored_model(stored_model, header.markov_model_order);
}

template <typename CCL>
std::vector<CCL> crack_codes_to_cc_labels(
  const std::vector<std::vector<unsigned char>>& crack_codes,
  const uint64_t sx, const uint64_t sy, const uint64_t sz,
  const bool permissible, uint64_t &N,
  const std::vector<std::vector<uint8_t>>& markov_model,
  int& err
) {
	const uint64_t sxy = sx * sy;

	std::vector<uint8_t> edges(sx*sy*sz);

	for (uint64_t z = 0; z < crack_codes.size(); z++) {
		if (crack_codes[z].size() == 0) {
			continue;
		}

		auto code = crack_codes[z];

		std::vector<uint64_t> nodes = crackle::crackcodes::read_boc_index(code, sx, sy);

		std::vector<uint8_t> codepoints;
		if (markov_model.size() == 0) {
			codepoints = crackle::crackcodes::unpack_codepoints(code, sx, sy);
		}
		else {
			uint32_t index_size = 4 + crackle::lib::ctoid(code, 0, 4);
			std::vector<uint8_t> markov_stream(code.begin() + index_size, code.end());
			codepoints = crackle::markov::decode_codepoints(markov_stream, markov_model);
		}

		auto symbol_stream = crackle::crackcodes::codepoints_to_symbols(nodes, codepoints);
		std::vector<uint8_t> slice_edges = crackle::crackcodes::decode_crack_code(
			symbol_stream, sx, sy, 
			permissible, err
		);
	  if (err > 0) {
			return std::vector<CCL>();
		}
		for (uint64_t i = 0; i < sxy; i++) {
			edges[i + sxy*z] = slice_edges[i];
		}
	}

	return crackle::cc3d::color_connectivity_graph<CCL>(
		edges, sx, sy, sz, N
	);
}

template <typename LABEL>
LABEL* decompress(
	const unsigned char* buffer, 
	const size_t num_bytes,
	LABEL* output,
	int& err,
	int64_t z_start = -1,
	int64_t z_end = -1
) {
	const CrackleHeader header(buffer);

	z_start = std::max(std::min(z_start, static_cast<int64_t>(header.sz - 1)), static_cast<int64_t>(0));
	z_end = z_end < 0 ? static_cast<int64_t>(header.sz) : z_end;
	z_end = std::max(std::min(z_end, static_cast<int64_t>(header.sz)), static_cast<int64_t>(0));

	if (z_start >= z_end) {
		err = 10;
		return output;
	}

	const int64_t szr = z_end - z_start;

	const uint64_t voxels = (
		static_cast<uint64_t>(header.sx) 
		* static_cast<uint64_t>(header.sy) 
		* static_cast<uint64_t>(szr)
	);

	if (voxels == 0) {
		return output;
	}

	std::vector<unsigned char> binary(buffer, buffer + num_bytes);

	// only used for markov compressed streams
	std::vector<std::vector<uint8_t>> markov_model = decode_markov_model(header, binary);

	auto crack_codes = get_crack_codes(header, binary, z_start, z_end, err);
	if (err > 0) {
		return output;
	}

	uint64_t N = 0;
	std::vector<uint32_t> cc_labels = crack_codes_to_cc_labels<uint32_t>(
		crack_codes, header.sx, header.sy, szr, 
		/*permissible=*/(header.crack_format == CrackFormat::PERMISSIBLE), 
		/*N=*/N,
		/*markov_model*/markov_model,
		/*err=*/err
	);

	if (err) {
		return output;
	}

	std::vector<LABEL> label_map;
	if (header.is_signed) {
		if (header.stored_data_width == 1) {
			label_map = crackle::labels::decode_label_map<LABEL, int8_t>(
				header, binary, cc_labels, N, z_start, z_end, err
			);
		}
		else if (header.stored_data_width == 2) {
			label_map = crackle::labels::decode_label_map<LABEL, int16_t>(
				header, binary, cc_labels, N, z_start, z_end, err
			);
		}
		else if (header.stored_data_width == 4) {
			label_map = crackle::labels::decode_label_map<LABEL, int32_t>(
				header, binary, cc_labels, N, z_start, z_end, err
			);
		}
		else {
			label_map = crackle::labels::decode_label_map<LABEL, int64_t>(
				header, binary, cc_labels, N, z_start, z_end, err
			);
		}
	}
	else {
		if (header.stored_data_width == 1) {
			label_map = crackle::labels::decode_label_map<LABEL, uint8_t>(
				header, binary, cc_labels, N, z_start, z_end, err
			);
		}
		else if (header.stored_data_width == 2) {
			label_map = crackle::labels::decode_label_map<LABEL, uint16_t>(
				header, binary, cc_labels, N, z_start, z_end, err
			);
		}
		else if (header.stored_data_width == 4) {
			label_map = crackle::labels::decode_label_map<LABEL, uint32_t>(
				header, binary, cc_labels, N, z_start, z_end, err
			);
		}
		else {
			label_map = crackle::labels::decode_label_map<LABEL, uint64_t>(
				header, binary, cc_labels, N, z_start, z_end, err
			);
		}
	}

	if (err > 0) {
		return output;
	}

	// always decode in fortran order
	// b/c that's what Neuroglancer likes
	for (uint64_t i = 0; i < voxels; i++) {
		output[i] = label_map[cc_labels[i]];
	}

	return output;
}

int decompress(
	const unsigned char* buffer, 
	const size_t num_bytes,
	void* output,
	const uint64_t output_num_bytes
) {
	if (num_bytes < CrackleHeader::header_size) {
		return 1;
	}

	if (!CrackleHeader::valid_header(buffer)) {
		return 2;
	}

	if (output == NULL) {
		return 3;
	}

	const CrackleHeader header(buffer);

	if (output_num_bytes < header.nbytes()) {
		return 4;
	}

	int err = 0;

	if (header.data_width == 1) {
		decompress<uint8_t>(buffer, num_bytes, reinterpret_cast<uint8_t*>(output), err);
	}
	else if (header.data_width == 2) {
		decompress<uint16_t>(buffer, num_bytes, reinterpret_cast<uint16_t*>(output), err);
	}
	else if (header.data_width == 4) {
		decompress<uint32_t>(buffer, num_bytes, reinterpret_cast<uint32_t*>(output), err);
	}
	else {
		decompress<uint64_t>(buffer, num_bytes, reinterpret_cast<uint64_t*>(output), err);
	}

	return err;
} 


};

#endif