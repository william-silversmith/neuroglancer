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
	LEFT = 0b10,
	RIGHT = 0b01,
	UP = 0b00,
	DOWN = 0b11
};

std::unordered_map<uint64_t, std::vector<unsigned char>> 
unpack_binary(
	const std::vector<unsigned char> &code, 
	const uint64_t sx, const uint64_t sy
) {
	std::unordered_map<uint64_t, std::vector<unsigned char>> chains;

	if (code.size() == 0) {
		return chains;
	}

	uint64_t index_width = crackle::lib::compute_byte_width((sx+1) * (sy+1));

	std::vector<unsigned char> symbols;
	symbols.reserve(code.size() * 4 * 2);

	uint64_t branches_taken = 0;
	uint64_t node = 0;

	char remap[4] = { 'u', 'r', 'l', 'd' };

	for (uint64_t i = 0; i < code.size(); i++) {
		if (branches_taken == 0) {
			node = crackle::lib::ctoid(code.data(), i, index_width);
			i += index_width - 1; // -1 b/c it will be incremented by for loop
			branches_taken = 1;
			continue;
		}

		for (uint64_t j = 0; j < 4; j++) {
			uint8_t move = static_cast<uint8_t>((code[i] >> (2*j)) & 0b11);

			if (symbols.size()) {
				if (
					(move == DirectionCode::UP && symbols.back() == 'd')
					|| (move == DirectionCode::LEFT && symbols.back() == 'r')
				) {
					symbols.back() = 't';
					branches_taken--;
					if (branches_taken == 0) {
						break;
					}
				}
				else if (
					(move == DirectionCode::DOWN && symbols.back() == 'u')
					|| (move == DirectionCode::RIGHT && symbols.back() == 'l')
				) {
					symbols.back() = 'b';
					branches_taken++;
				}
				else {
					symbols.push_back(remap[move]);
				}
			}
			else {
				symbols.push_back(remap[move]);
			}
		}

		if (branches_taken == 0) {
			auto vec = chains[node];
			vec.insert(vec.end(), symbols.begin(), symbols.end());
			chains[node] = vec;
			symbols.clear();
		}
	}

	return chains;
}

std::vector<uint8_t> decode_permissible_crack_code(
	const std::unordered_map<uint64_t, std::vector<unsigned char>> &chains,
	const int64_t sx, const int64_t sy,
	int& err
) {
	// voxel connectivity
	// four bits: -y-x+y+x true is passable
	std::vector<uint8_t> edges(sx * sy);

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
				return edges;
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

	return edges;
}

std::vector<uint8_t> decode_impermissible_crack_code(
	const std::unordered_map<uint64_t, std::vector<unsigned char>> &chains,
	const int64_t sx, const int64_t sy,
	int& err
) {
	// voxel connectivity
	// four bits: -y-x+y+x true is passable
	std::vector<uint8_t> edges(sx * sy, 0b1111);

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
				return edges;
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

	return edges;
}

std::vector<uint8_t> decode_crack_code(
	const std::unordered_map<uint64_t, std::vector<unsigned char>> &chains,
	const uint64_t sx, const uint64_t sy,
	const bool permissible, int& err
) {
	if (permissible) {
		return decode_permissible_crack_code(chains, sx, sy, err);
	}
	else {
		return decode_impermissible_crack_code(chains, sx, sy, err);
	}
}

};
};

#endif