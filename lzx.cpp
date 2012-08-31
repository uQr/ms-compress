// ms-compress: implements Microsoft compression algorithms
// Copyright (C) 2012  Jeffrey Bush  jeff@coderforlife.com
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Decompression code is adapted from 7-zip. See http://www.7-zip.org/.

#include "lzx.h"

#include "Bitstream.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif

#define VERBATIM_BLOCK			1
#define ALIGNED_OFFSET_BLOCK	2
#define UNCOMPRESSED_BLOCK		3

#define TRANSLATION_SIZE		12000000

typedef struct _Decoder { // 2700 bytes => ~2.6 kb
	uint32_t numSymbols;
	uint32_t limits[17];    // m_Limits[i] = value limit for symbols with length = i
	uint32_t positions[17]; // m_Positions[i] = index in m_Symbols[] of first symbol with length = i
	uint32_t symbols[512];  // 512 is just above max of 8, 20, 249, 496
	byte lengths[1 << 9];	// Table oh length for short codes. (512)
} Decoder;

static bool lzx_set_code_lengths(Decoder *decoder, uint32_t n, const_bytes codeLengths) {
	int lenCounts[17], i;
	uint32_t tmpPositions[17], symbol;
	uint32_t startPos = 0, index = 0;
	decoder->numSymbols = n;
	memset(lenCounts+1, 0, 16*sizeof(int));
	for (symbol = 0; symbol < n; symbol++) {
		int len = codeLengths[symbol];
		if (len > 16) { return false; }
		lenCounts[len]++;
		decoder->symbols[symbol] = 0xFFFFFFFF;
	}
	lenCounts[0] = 0;
	decoder->positions[0] = 0;
	decoder->limits[0] = 0;
	for (i = 1; i <= 16; i++) {
		startPos += lenCounts[i] << (16 - i);
		if (startPos > (1 << 16)) { return false; }
		decoder->limits[i] = (i == 16) ? (1 << 16) : startPos;
		decoder->positions[i] = decoder->positions[i - 1] + lenCounts[i - 1];
		tmpPositions[i] = decoder->positions[i];
		if (i <= 9) {
			uint32_t limit = (decoder->limits[i] >> (16 - 9));
			for (; index < limit; index++)
				decoder->lengths[index] = (byte)i;
		}
	}
	for (symbol = 0; symbol < n; symbol++) {
		int len = codeLengths[symbol];
		if (len != 0)
			decoder->symbols[tmpPositions[len]++] = symbol;
	}
	return true;
}

static uint32_t lzx_decode_symbol(const Decoder *decoder, InputBitstream *bits) {
	byte n;
	uint32_t value = bits->Peek(16), index;
	if (value < decoder->limits[9])
		n = decoder->lengths[value >> 7];
	else
		for (n = 10; value >= decoder->limits[n]; n++);
	bits->Skip(n);
	index = decoder->positions[n] + ((value - decoder->limits[n - 1]) >> (16 - n));
	if (index >= decoder->numSymbols)
		return 0xFFFFFFFF;
	return decoder->symbols[index];
}

static bool lzx_read_table(InputBitstream *in, bytes newLevels, uint32_t numSymbols) {
	Decoder LevelDecoder;
	byte levelLevels[20], symbol = 0;
	uint32_t i, num = 0;
	for (i = 0; i < ARRAYSIZE(levelLevels); i++)
		levelLevels[i] = (byte)in->ReadBits(4);
	lzx_set_code_lengths(&LevelDecoder, 20, levelLevels);
	for (i = 0; i < numSymbols;) {
		uint32_t number;
		if (num != 0) {
			newLevels[i++] = symbol;
			num--;
			continue;
		}
		number = lzx_decode_symbol(&LevelDecoder, in);
		if (number == 17) {
			num = 4 + in->ReadBits(4);
			symbol = 0;
		} else if (number == 18) {
			num = 20 + in->ReadBits(5);
			symbol = 0;
		} else {
			if (number <= 16) {
				num = 1;
			} else if (number == 19) {
				number = lzx_decode_symbol(&LevelDecoder, in);
				if (number > 16)
					return false;
				num = 4 + in->ReadBit();
			} else {
				return false;
			}
			symbol = (byte)((number==0) ? 0 : (17 - number)); //byte((17 - number) % (NUM_HUFFMAN_BITS + 1));
		}
	}
	return true;
}

static bool lzx_decompress_block(InputBitstream *in, bytes out, size_t out_len, Decoder *MainDecoder, Decoder *LenDecoder, Decoder *AlignDecoder, uint32_t repDists[]) {
	bytes end = out + out_len;
	while (out < end) {
		uint32_t sym = lzx_decode_symbol(MainDecoder, in);
		if (sym < 0x100) {
			*out++ = (byte)sym;
		} else {
			uint32_t pos, len;
			sym -= 0x100;
			if (sym >= 240)
				return false;
			len = sym & 0x7 + 2; // posLenSlot % kNumLenSlots;
			if (len == 9) {
				len = 9 + lzx_decode_symbol(LenDecoder, in);
				if (len >= 258)
					return false;
			}
			pos = sym >> 3; // posLenSlot / kNumLenSlots;
			if (pos < 3) {
				uint32_t dist = repDists[pos];
				repDists[pos] = repDists[0];
				repDists[0] = dist;
			} else {
				uint32_t dist;
				byte n;
				if (pos < 0x26) {
					n = (byte)(pos >> 1) - 1;
					dist = (2 | (pos & 1)) << n;
				} else {
					n = 17;
					dist = (pos - 0x22) << 17;
				}
				if (AlignDecoder && n >= 3) {
					uint32_t alignTemp;
					dist += in->ReadBits(n - 3) << 3;
					alignTemp = lzx_decode_symbol(AlignDecoder, in);
					if (alignTemp >= 8)
						return false;
					dist += alignTemp;
				} else
					dist += in->ReadBits(n);
				repDists[2] = repDists[1];
				repDists[1] = repDists[0];
				repDists[0] = dist - 3;
			}

			if (out + len > end)
				return false;

			memmove(out, out - repDists[0] - 1, len);
			out += len;
		}
	}

	return true;
}

static void lzx_decompress_translation(bytes out, size_t out_len) {
	if (out_len >= 6) {
		const_bytes start = out, end = out + out_len - 6;
		while ((out = (bytes)memchr(out, 0xE8, end - out)) != NULL) {
			int32_t pos = (int32_t)(out++ - start);
			int32_t absValue = GET_UINT32(out);
			if (absValue >= -pos && absValue < TRANSLATION_SIZE) {
				uint32_t offset = (absValue >= 0) ? absValue - pos : absValue + TRANSLATION_SIZE;
				SET_UINT32(out, offset);
			}
			out += 4;
		}
	}
}

size_t lzx_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len) { // approximately 11 kb of memory on stack
	InputBitstream bits(in, in_len);
	byte type;
	bool isAlignType;
	uint32_t uncompressedSize;

	if (!in_len) { return 0; }
	
	type = (byte)bits.ReadBits(3);
	isAlignType = type == ALIGNED_OFFSET_BLOCK;
	uncompressedSize = (bits.ReadBits(1) & 0x8) ? 0x8000 : bits.ReadBits(16);
	if (uncompressedSize > out_len || uncompressedSize == 0) { return 0; }

	if (type == UNCOMPRESSED_BLOCK && uncompressedSize + 20 <= in_len) {
		memcpy(out, in + 20, uncompressedSize); // copy the uncompressed data, aligned to 32-bit boundary and skipping past the rep distances
	} else if (type == VERBATIM_BLOCK || isAlignType) {
		Decoder MainDecoder;  // 496
		Decoder LenDecoder;   // 249
		Decoder AlignDecoder; // 8
		byte newLevels[512];
		uint32_t repDists[3]= { 0, 0, 0 };

		if (isAlignType) {
			int i;
			for (i = 0; i < 8; i++)
				newLevels[i] = (byte)bits.ReadBits(3);
			if (!lzx_set_code_lengths(&AlignDecoder, 8, newLevels)) { return false; }
		}

		lzx_read_table(&bits, newLevels, 256);
		lzx_read_table(&bits, newLevels + 256, 240);
		if (!lzx_set_code_lengths(&MainDecoder, 496, newLevels)) { return false; }
	
		lzx_read_table(&bits, newLevels, 249);
		if (!lzx_set_code_lengths(&LenDecoder, 249, newLevels)) { return false; }

		if (!lzx_decompress_block(&bits, out, uncompressedSize, &MainDecoder, &LenDecoder, isAlignType ? &AlignDecoder : NULL, repDists))
			return false;
	} else {
		return 0;
	}

	lzx_decompress_translation(out, uncompressedSize);
	return uncompressedSize;
}

size_t lzx_uncompressed_size(const_bytes in, size_t in_len) {
	byte type;
	if (!in_len) { return 0; }
	type = *in & 0x7;
	return (type == VERBATIM_BLOCK || type == ALIGNED_OFFSET_BLOCK || type == UNCOMPRESSED_BLOCK) ?
		((*in & 0x8) ? 0x8000 :
		(in_len < 3 ? 0 : (((in[2] << 12) | (in[1] << 4) | (*in >> 4)) & 0xFFFF))) : 0;
}

size_t lzx_compress(const_bytes in, size_t in_len, bytes out, size_t out_len) {
	OutputBitstream bits(out, out_len);

	if (in_len > 0x8000 || out_len < 3) { return 0; }
	
	// Write header
	bits.WriteBits(UNCOMPRESSED_BLOCK, 3);
	if (in_len == 0x8000) {
		bits.WriteBits(1, 1);
	} else {
		bits.WriteBits(0, 1);
		bits.WriteBits((uint32_t)in_len, 16);
	}

	// Write block
	if (out_len < in_len + 20) { return 0; }
	memcpy(out + 20, in, in_len);
	return in_len;
}

#ifdef COMPRESSION_API_EXPORT
size_t lzx_max_compressed_size(size_t in_len) { return in_len + 20; }
#endif