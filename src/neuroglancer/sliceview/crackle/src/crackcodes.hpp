#ifndef __CRACKLE_CRACKCODE_HXX__
#define __CRACKLE_CRACKCODE_HXX__

#include <vector>
#include <unordered_map>
#include <stack>
#include <cstdint>

#include "robin_hood.hpp"
#include "lib.hpp"
#include "cc3d.hpp"

namespace crackle {
namespace crackcodes {

enum DirectionCode {
	LEFT = 0b11,
	RIGHT = 0b01,
	UP = 0b00,
	DOWN = 0b10,
	NONE = 255
};

std::vector<uint64_t> read_boc_index(
	const std::vector<unsigned char>& binary,
	const uint64_t sx, const uint64_t sy
) {
	std::vector<uint64_t> nodes;

	const uint64_t sxe = sx + 1;

	const uint64_t x_width = crackle::lib::compute_byte_width(sx+1);
	const uint64_t y_width = crackle::lib::compute_byte_width(sy+1);

	uint64_t idx = 4; // skip over index size
	uint64_t num_y = crackle::lib::ctoid(binary, idx, y_width);
	idx += y_width;

	uint64_t y = 0; 

	for (uint64_t yi = 0; yi < num_y; yi++) {
		y += crackle::lib::ctoid(binary, idx, y_width);
		idx += y_width;

		uint64_t num_x = crackle::lib::ctoid(binary, idx, x_width);
		idx += x_width;

		uint64_t x = 0;
		for (uint64_t xi = 0; xi < num_x; xi++) {
			x += crackle::lib::ctoid(binary, idx, x_width);
			idx += x_width;
			nodes.push_back(x + sxe * y);
		}
	}

	return nodes;
}

std::vector<std::pair<uint64_t, std::vector<unsigned char>> >
codepoints_to_symbols(
	const std::vector<uint64_t>& sorted_nodes,
	const std::vector<uint8_t>& codepoints
) {

	std::vector<std::pair<uint64_t, std::vector<unsigned char>> > chains;

	std::vector<unsigned char> symbols;
	symbols.reserve(codepoints.size() * 4 * 2);

	uint64_t branches_taken = 0;
	uint64_t node = 0;

	char remap[4] = { 'u', 'r', 'd', 'l' };

	uint64_t node_i = 0;

	uint8_t last_move = DirectionCode::NONE;

	for (uint64_t i = 0; i < codepoints.size(); i++) {
		if (branches_taken == 0) {
			if (node_i >= sorted_nodes.size()) {
				break;
			}
			node = sorted_nodes[node_i];
			node_i++;
			i--; // b/c i will be incremented
			branches_taken = 1;
			continue;
		}

		uint8_t move = codepoints[i];
	
		// by chance, up^down and left^right 
		// both evaluate to 0b10
		if ((move ^ last_move) != 0b10) {
			symbols.push_back(remap[move]);
			last_move = move;
			continue;
		}
		else if (
			(move == DirectionCode::UP && last_move == DirectionCode::DOWN)
			|| (move == DirectionCode::LEFT && last_move == DirectionCode::RIGHT)
		) {
			symbols.back() = 't';
			branches_taken--;
			last_move = DirectionCode::NONE;
		}
		else { // the code here is DOWN+UP or RIGHT+LEFT
			symbols.back() = 'b';
			branches_taken++;
			last_move = DirectionCode::NONE;
		}

		if (branches_taken == 0) {
			chains.push_back(std::make_pair(node, symbols));
			symbols.clear();
		}
	}

	return chains;
}

std::vector<uint8_t> unpack_codepoints(
	const std::vector<unsigned char> &code, 
	const uint64_t sx, const uint64_t sy
) {
	if (code.size() == 0) {
		return std::vector<uint8_t>();
	}

	uint32_t index_size = 4 + crackle::lib::ctoid(code, 0, 4);

	std::vector<uint8_t> codepoints;
	codepoints.reserve(4 * (code.size() - index_size));

	uint8_t last = 0;

	for (uint64_t i = index_size; i < code.size(); i++) {
		for (uint64_t j = 0; j < 4; j++) {
			uint8_t codepoint = static_cast<uint8_t>((code[i] >> (2*j)) & 0b11);
			codepoint += last;
			codepoint &= 0b11;
			last = codepoint;
			codepoints.push_back(codepoint);
		}
	}

	return codepoints;
}

void decode_permissible_crack_code(
	const std::vector<std::pair<uint64_t, std::vector<unsigned char>> > &chains,
	const int64_t sx, const int64_t sy,
	uint8_t* edges, int& err
) {
	// voxel connectivity
	// four bits: -y-x+y+x true is passable
	std::fill(edges, edges + sx * sy, 0);

	int64_t sxe = sx + 1;

	// graph is of corners and edges
	// origin is located at top left
	// corner of the image
	for (auto& [node, symbols]: chains) {
		int64_t y = node / sxe;
		int64_t x = node - (sxe * y);

		std::stack<int64_t> revisit;
		for (unsigned char symbol : symbols) {
			int64_t loc = x + sx * y;
			if (loc < 0 || loc >= (sx+1) * (sy+1)) {
				err = 201; 
				return;
			}

			if (symbol == 'u') {
				if (x > 0 && y > 0) {
					edges[loc - 1 - sx] |= 0b0001;
				}
				if (y > 0) {
					edges[loc - sx] |= 0b0010;
				}
				y--;
			}
			else if (symbol == 'd') {
				if (x > 0) {
					edges[loc - 1] |= 0b0001;
				}
				edges[loc] |= 0b0010;
				y++;
			}
			else if (symbol == 'l') {
				if (x > 0 && y > 0) {
					edges[loc-1-sx] |= 0b0100;
				}
				if (x > 0) {
					edges[loc-1] |= 0b1000;
				}
				x--;
			}
			else if (symbol == 'r') {
				if (y > 0) {
					edges[loc-sx] |= 0b0100;
				}
				edges[loc] |= 0b1000;
				x++;
			}
			else if (symbol == 'b') {
				revisit.push(loc);
			}
			else if (symbol =='t') {
				if (!revisit.empty()) {
					loc = revisit.top();
					revisit.pop();
					y = loc / sx;
					x = loc - (sx * y);
				}
			}
		}
	}
}

void decode_impermissible_crack_code(
	const std::vector<std::pair<uint64_t, std::vector<unsigned char>> > &chains,
	const int64_t sx, const int64_t sy,
	uint8_t* edges, int& err
) {
	// voxel connectivity
	// four bits: -y-x+y+x true is passable
	std::fill(edges, edges + sx * sy, 0b1111);

	int64_t sxe = sx + 1;

	// graph is of corners and edges
	// origin is located at top left
	// corner of the image
	for (auto& [node, symbols]: chains) {
		int64_t y = node / sxe;
		int64_t x = node - (sxe * y);

		std::stack<int64_t> revisit;
		for (unsigned char symbol : symbols) {
			int64_t loc = x + sx * y;

			if (loc < 0 || loc >= (sx+1) * (sy+1)) {
				err = 200;
				return;
			}

			if (symbol == 'u') {
				if (x > 0 && y > 0) {
					edges[loc - 1 - sx] &= 0b1110;
				}
				if (y > 0) {
					edges[loc - sx] &= 0b1101;
				}
				y--;
			}
			else if (symbol == 'd') {
				if (x > 0) {
					edges[loc - 1] &= 0b1110;
				}
				edges[loc] &= 0b1101;
				y++;
			}
			else if (symbol == 'l') {
				if (x > 0 && y > 0) {
					edges[loc - 1 - sx] &= 0b1011;
				}
				if (x > 0) {
					edges[loc-1] &= 0b0111;
				}
				x--;
			}
			else if (symbol == 'r') {
				if (y > 0) {
					edges[loc - sx] &= 0b1011;
				}
				edges[loc] &= 0b0111;
				x++;
			}
			else if (symbol == 'b') {
				revisit.push(loc);
			}
			else if (symbol =='t') {
				if (!revisit.empty()) {
					loc = revisit.top();
					revisit.pop();
					y = loc / sx;
					x = loc - (sx * y);
				}
			}
		}
	}
}

void decode_crack_code(
	const std::vector<std::pair<uint64_t, std::vector<unsigned char>> > &chains,
	const uint64_t sx, const uint64_t sy,
	const bool permissible, 
	uint8_t* slice_edges, int& err
) {
	if (permissible) {
		decode_permissible_crack_code(chains, sx, sy, slice_edges, err);
	}
	else {
		decode_impermissible_crack_code(chains, sx, sy, slice_edges, err);
	}
}

};
};

#endif