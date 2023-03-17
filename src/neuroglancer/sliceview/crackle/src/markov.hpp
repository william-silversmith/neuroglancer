#ifndef __MARKOV_HXX__
#define __MARKOV_HXX__

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>

#include "robin_hood.hpp"

namespace crackle {
namespace markov {
	// Lookup tables are generated from the following python code:
	/*
		from itertools import permutations
		LUT = []
		for p in list(permutations([0,1,2,3])):
		    val = 0
		    for i in range(4):
		        val |= p[i] << 2*i
		    LUT.append(val)

		for x in LUT:
			print(bin(x))
		
		ILUT = [255] * 255
		for i, el in enumerate(LUT):
			ILUT[el] = i

		print(ILUT)
	*/

	// lookup tables for translating
	// the 24 possible UDLR positions
	// into a 5 bit representation
	constexpr uint8_t LUT[24] = {
		0b11100100,
		0b10110100,
		0b11011000,
		0b1111000,
		0b10011100,
		0b1101100,
		0b11100001,
		0b10110001,
		0b11001001,
		0b111001,
		0b10001101,
		0b101101,
		0b11010010,
		0b1110010,
		0b11000110,
		0b110110,
		0b1001110,
		0b11110,
		0b10010011,
		0b1100011,
		0b10000111,
		0b100111,
		0b1001011,
		0b11011
	};

	constexpr uint8_t DNE = 255;
	constexpr uint8_t ILUT[255] = {
		DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, 23, DNE, DNE, 17, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE, 21, DNE, DNE, DNE, DNE, 
		DNE, 11, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 15, 
		DNE, DNE, 9, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 22, DNE, 
		DNE, 16, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		19, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 5, DNE, 
		DNE, DNE, DNE, DNE, 13, DNE, DNE, DNE, DNE, DNE, 3, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, 20, DNE, DNE, DNE, DNE, DNE, 10, DNE, DNE, 
		DNE, DNE, DNE, 18, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, 4, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		7, DNE, DNE, 1, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 14, DNE, 
		DNE, 8, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 12, DNE, 
		DNE, DNE, DNE, DNE, 2, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, 6, DNE, DNE, 0, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, DNE, 
		DNE, DNE, DNE, DNE, DNE, DNE
	};

	struct CircularBuf {
		uint8_t* data;
		int length;
		int idx;
		int base_10_cached;

		CircularBuf(int model_order) {
			data = new uint8_t[model_order]();
			length = model_order;
			idx = 0;
			base_10_cached = 0;
		}

		~CircularBuf() {
			delete [] data;
		}

		void push_back(uint8_t elem) {
			data[idx] = elem;
			idx++;
			if (idx >= length) {
				idx = 0;
			}
		}

		uint8_t front() const {
			return data[idx];
		}

		uint8_t back() const {
			return (idx == 0)
				? data[length - 1]
				: data[idx - 1];
		}

		int push_back_and_update(uint8_t elem) {
			base_10_cached -= front();
			base_10_cached >>= 2;
			// 4^(len-1) = 2^2^(len-1) = 2^(2 * (len-1))
			base_10_cached += static_cast<int>(elem) * (1 << (2 * (length-1)));
			push_back(elem);
			return base_10_cached;
		}

		std::vector<uint8_t> read() const {
			std::vector<uint8_t> out;
			out.reserve(length);
			for (int i = 0, j = idx; i < length; i++, j++) {
				if (j >= length) {
					j = 0;
				}
				out.push_back(data[j]);
			}
			return out;
		}

		int change_to_base_10() {
			int base_10 = 0;
			for (int i = 0, j = idx; i < length; i++, j++) {
				if (j >= length) {
					j = 0;
				}
				base_10 += pow(4, i) * static_cast<int>(data[j]);
			}
			base_10_cached = base_10;
			return base_10;
		}
	};

	std::vector<uint8_t> decode_codepoints(
		std::vector<unsigned char>& crack_code,
		const std::vector<std::vector<uint8_t>>& model
	) {
		std::vector<uint8_t> data_stream;

		int model_order = static_cast<int>(log2(model.size()) / log2(4));
		CircularBuf buf(model_order);

		int pos = 2;
		uint8_t start_dir = crack_code[0] & 0b11;
		data_stream.push_back(start_dir);
		buf.push_back(start_dir);

		int model_row = buf.change_to_base_10();
		for (uint64_t i = 0; i < crack_code.size(); i++) {
			uint16_t byte = crack_code[i]; 
			if (i < crack_code.size() - 1) {
				byte |= (crack_code[i+1] << 8);
			}

			while (pos < 8) {
				uint8_t codepoint = (byte >> pos) & 0b111;

				if ((codepoint & 0b1) == 0) {
					data_stream.push_back(model[model_row][0]);
					pos++;
				}
				else if ((codepoint & 0b10) == 0) {
					data_stream.push_back(model[model_row][1]);
					pos += 2;
				}
				else if ((codepoint & 0b100) == 0) {
					data_stream.push_back(model[model_row][2]);	
					pos += 3;
				}
				else {
					data_stream.push_back(model[model_row][3]);
					pos += 3;
				}

				model_row = buf.push_back_and_update(data_stream.back());
			}

			pos -= 8;
		}

		for (uint64_t i = 1; i < data_stream.size(); i++) {
			data_stream[i] += data_stream[i-1];
			if (data_stream[i] > 3) {
				data_stream[i] -= 4;
			}
		}

		return data_stream;
	}

	std::vector<std::vector<uint8_t>> from_stored_model(
		const std::vector<unsigned char>& model_stream,
		const int markov_model_order
	) {
		std::vector<std::vector<uint8_t>> model;
		model.reserve(pow(4, markov_model_order));

		const uint64_t stream_size = model_stream.size();

		int pos = 0;
		for (uint64_t i = 0; i < stream_size; i++) {

			while (pos < 8) {
				int decoded = 0;
				if (pos + 5 > 8 && i < stream_size - 1) {
					decoded = (model_stream[i] >> pos) & 0b11111;
					decoded |= (model_stream[i+1] & ~(~0u << (pos + 5 - 8))) << (8 - pos);
					decoded &= 0b11111;
				}
				else {
					decoded = (model_stream[i] >> pos) & 0b11111;
				}

				uint8_t model_row = LUT[decoded];

				std::vector<uint8_t> row(4);
				row[0] = model_row & 0b11;
				row[1] = (model_row >> 2) & 0b11;
				row[2] = (model_row >> 4) & 0b11;
				row[3] = (model_row >> 6) & 0b11;
				model.push_back(std::move(row));

				pos += 5;
			}
			pos -= 8;
		}

		return model;
	}
};
};

#endif
