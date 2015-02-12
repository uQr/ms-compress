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


// General include file which includes necessary files, defines, and typedefs for internal use.

// This has been made for use with MSVC, GCC, and Clang with fallbacks for other compilers.
// To make other compilers work at their full potential, you should add stuff below, particularly
// to the INLINE/FORCE_INLINE and Builtins/Intrinsics sections.

#ifndef MSCOMP_INTERNAL_H
#define MSCOMP_INTERNAL_H

// For MSVC
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

// For GCC
#define __STDC_LIMIT_MACROS

#include "general.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <new>

// Check that it is 8 bits to the byte
#if CHAR_BIT != 8
	#error Unsupported char size
#endif

// Determine the endianness of the compilation, however this isn't very accurate
// It would be much better to define LITTLE_ENDIAN, BIG_ENDIAN, or PDP_ENDIAN yourself
// LITTLE_ENDIAN is what the program is developed for and tested with
// BIG_ENDIAN and PDP_ENDIAN are untested
#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN) && !defined(PDP_ENDIAN) && !defined(_MSC_VER) && !defined(_WIN32) && !defined(__LITTLE_ENDIAN__) && !defined(__IEEE_LITTLE_ENDIAN)
	#if defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__) || defined(__IEEE_BIG_ENDIAN)
		#define BIG_ENDIAN
	#else
		#include <endian.h>
		#if (defined(__PDP_ENDIAN) && !defined(__LITTLE_ENDIAN)) || __BYTE_ORDER == __PDP_ENDIAN
			#define PDP_ENDIAN
		#elif (defined(__BIG_ENDIAN) && !defined(__LITTLE_ENDIAN)) || __BYTE_ORDER == __BIG_ENDIAN
			#define BIG_ENDIAN
		#endif
	#endif
#endif

// Get ints from a byte stream
// These assume that the byte stream is little-endian
#if defined(BIG_ENDIAN)
	#define GET_UINT16(x)		byte_swap(*(const uint16_t*)(x)) // or ((x)[0]|((x)[1]<<8))
	#define GET_UINT32(x)		byte_swap(*(const uint32_t*)(x)) // or ((x)[0]|((x)[1]<<8)|((x)[2]<<16)|((x)[3]<<24))
	#define SET_UINT16(x,val)	(*(uint16_t*)(x) = byte_swap((uint16_t)(val))) // or (((byte*)(x))[0]=(byte)(val), ((byte*)(x))[1]=(byte)((val) >> 8))
	#define SET_UINT32(x,val)	(*(uint32_t*)(x) = byte_swap((uint32_t)(val))) // or (((byte*)(x))[0]=(byte)(val), ((byte*)(x))[1]=(byte)((val) >> 8), ((byte*)(x))[2]=(byte)((val) >> 16), ((byte*)(x))[3]=(byte)((val) >> 24))
#elif defined(PDP_ENDIAN) // for 16-bit ints its the same as little-endian
	#define GET_UINT16(x)		(*(const uint16_t*)(x))
	#define GET_UINT32(x)		(*(const uint16_t*)(x)|(*(const uint16_t*)((x)+2)<<16))
	#define SET_UINT16(x,val)	(*(uint16_t*)(x) = (uint16_t)(val))
	#define SET_UINT32(x,val)	(*(uint16_t*)(x) = (uint16_t)(val), *(((uint16_t*)(x))+1) = (uint16_t)((val) >> 16))
#else
	#ifndef LITTLE_ENDIAN
		#define LITTLE_ENDIAN
	#endif
	#define GET_UINT16(x)		(*(const uint16_t*)(x))
	#define GET_UINT32(x)		(*(const uint32_t*)(x))
	#define SET_UINT16(x,val)	(*(uint16_t*)(x) = (uint16_t)(val))
	#define SET_UINT32(x,val)	(*(uint32_t*)(x) = (uint32_t)(val))
#endif

// Determine the number of bits used by pointers
#ifndef PNTR_BITS
	#if SIZE_MAX == UINT64_MAX
		#define PNTR_BITS 64
	#elif SIZE_MAX == UINT32_MAX
		#define PNTR_BITS 32
	#elif SIZE_MAX == UINT16_MAX
		#define PNTR_BITS 16
	#else
		#error You must define PNTR_BITS to be the number of bits used for pointers
	#endif
#endif

// Get INLINE and FORCE_INLINE
#if defined(_MSC_VER)
#define INLINE __inline
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__)
#define INLINE inline
#define FORCE_INLINE inline __attribute__((always_inline))
#elif (__STDC_VERSION__ >= 199901L)
#define INLINE inline
#define FORCE_INLINE INLINE
#else
#define INLINE
#define FORCE_INLINE INLINE
#endif

// Intrinsic and Builtin functions
#if defined(_MSC_VER)
	// see https://msdn.microsoft.com/en-us/library/hh977022.aspx
	#include <intrin.h>
	#define ALWAYS(x)     __assume(x)
	#define LIKELY(x)     x
	#define UNLIKELY(x)   x
	#define NEVER(x)      __assume(!(x))
	#define UNREACHABLE() __assume(0)
	#pragma intrinsic(_rotl, memset, memcpy)
	int FORCE_INLINE rotl(uint8_t x,  int bits) { return _rotl8 (x, (unsigned char)bits); }
	int FORCE_INLINE rotl(uint16_t x, int bits) { return _rotl16(x, (unsigned char)bits); }
	int FORCE_INLINE rotl(uint32_t x, int bits) { return _rotl  (x, (unsigned char)bits); }
	int FORCE_INLINE rotl(uint64_t x, int bits) { return (int)_rotl64(x, (unsigned char)bits); }
	#if defined(_M_ARM)
		int FORCE_INLINE count_bits_set(uint32_t x) { return _CountOneBits(x); }
		int FORCE_INLINE count_bits_set(uint64_t x) { return _CountOneBits64(x); }
		int FORCE_INLINE count_leading_zeros(uint32_t x) { return _CountLeadingZeros(x; }
		int FORCE_INLINE count_leading_zeros(uint64_t x) { return _CountLeadingZeros64(x; }
	#elif defined(_M_IX86) || defined(_M_AMD64) || defined(_M_X64)
		int FORCE_INLINE count_bits_set(uint16_t x) { return __popcnt16(x); } // TODO: only available if bit 23 of CPUInfo[2] (ECX) is set after __cpuid(int cpuInfo[4], 0x00000001) [Nephalem]
		int FORCE_INLINE count_bits_set(uint32_t x) { return __popcnt(x); }   // TODO: only available if bit 23 of CPUInfo[2] (ECX) is set after __cpuid(int cpuInfo[4], 0x00000001) [Nephalem]
		int FORCE_INLINE count_leading_zeros(uint32_t x) { unsigned long r; _BitScanReverse(&r, x); return (31-r); }
		//int FORCE_INLINE count_leading_zeros(uint16_t x) { return __lzcnt16(x); } // TODO: only available if bit 5 of CPUInfo[2] (ECX) is set after __cpuid(int cpuInfo[4], 0x80000001) [Haswell]
		//int FORCE_INLINE count_leading_zeros(uint32_t x) { return __lzcnt(x); }   // TODO: only available if bit 5 of CPUInfo[2] (ECX) is set after __cpuid(int cpuInfo[4], 0x80000001) [Haswell]
		#if defined(_M_AMD64) || defined(_M_X64)
			int FORCE_INLINE count_bits_set(uint64_t x) { return (int)__popcnt64(x); } // TODO: only available if bit 23 of CPUInfo[2] (ECX) is set after __cpuid(int cpuInfo[4], 0x00000001) [Nephalem]
			int FORCE_INLINE count_leading_zeros(uint64_t x) { unsigned long r; _BitScanReverse64(&r, x); return (63-r); }
			//int FORCE_INLINE count_leading_zeros(uint64_t x) { return __lzcnt64(x); }   // TODO: only available if bit 5 of CPUInfo[2] (ECX) is set after __cpuid(int cpuInfo[4], 0x80000001) [Haswell]
		#endif
	#endif
	uint16_t FORCE_INLINE byte_swap(uint16_t x) { return _byteswap_ushort(x); }
	uint32_t FORCE_INLINE byte_swap(uint32_t x) { return _byteswap_ulong(x);  }
	uint64_t FORCE_INLINE byte_swap(uint64_t x) { return _byteswap_uint64(x); }
#elif defined(__GNUC__) // GCC and Clang
	// see https://gcc.gnu.org/onlinedocs/gcc-4.5.0/gcc/Other-Builtins.html
	#define ALWAYS(x)     if (!(x)) __builtin_unreachable()
	#define LIKELY(x)     __builtin_expect((x), 1)
	#define UNLIKELY(x)   __builtin_expect((x), 0)
	#define NEVER(x)      if (x) __builtin_unreachable()
	#define UNREACHABLE() __builtin_unreachable()
	int FORCE_INLINE rotl(uint8_t x,  int bits) { return ((x << bits) | (x >> (8  - bits))); } // the compiler detects these and optimizes, no need for a special builtin
	int FORCE_INLINE rotl(uint16_t x, int bits) { return ((x << bits) | (x >> (16 - bits))); }
	int FORCE_INLINE rotl(uint32_t x, int bits) { return ((x << bits) | (x >> (32 - bits))); }
	int FORCE_INLINE rotl(uint64_t x, int bits) { return ((x << bits) | (x >> (64 - bits))); }
	int FORCE_INLINE count_bits_set(uint32_t x) { return __builtin_popcount(x); }
	int FORCE_INLINE count_bits_set(uint64_t x) { return __builtin_popcountll(x); }
	int FORCE_INLINE count_leading_zeros(uint32_t x) { return __builtin_clz(x); }
	int FORCE_INLINE count_leading_zeros(uint64_t x) { return __builtin_clzll(x); }
	uint16_t FORCE_INLINE byte_swap(uint16_t x) { return (x<<8)|(x>>8); }
	uint32_t FORCE_INLINE byte_swap(uint32_t x) { return (uint32_t)__builtin_bswap32((int32_t)x); }
	uint64_t FORCE_INLINE byte_swap(uint64_t x) { return (uint64_t)__builtin_bswap64((int64_t)x); }
#else
	#define ALWAYS(x)     
	#define LIKELY(x)     x
	#define UNLIKELY(x)   x
	#define NEVER(x)      
	#define UNREACHABLE()
	int FORCE_INLINE rotl(uint8_t x,  int bits) { return ((x << bits) | (x >> (8  - bits))); }
	int FORCE_INLINE rotl(uint16_t x, int bits) { return ((x << bits) | (x >> (16 - bits))); }
	int FORCE_INLINE rotl(uint32_t x, int bits) { return ((x << bits) | (x >> (32 - bits))); }
	int FORCE_INLINE rotl(uint64_t x, int bits) { return ((x << bits) | (x >> (64 - bits))); }
	int FORCE_INLINE count_bits_set(uint32_t x) { x -= (x>>1)&0x55555555; x = (((x>>2)&0x33333333) + (x&0x33333333)); x = (((x>>4)+x)&0x0f0f0f0f); x += (x>>8); x += (x>>16); return x&0x0000003f; }
	int FORCE_INLINE count_bits_set(uint64_t x) { x -= (x>>1)&0x5555555555555555ull;  x = ((x>>2)&0x3333333333333333ull) + (x&0x3333333333333333ull); return (int)(((((x>>4)+x)&0xf0f0f0f0f0f0f0full)*0x101010101010101ull)>>56); }
	int FORCE_INLINE count_leading_zeros(uint32_t x) { x |= (x>>1); x |= (x>>2); x |= (x>>4); x |= (x>>8); x |= (x>>16); return 32 - count_bits_set(x); }
	int FORCE_INLINE count_leading_zeros(uint64_t x) { x |= (x >> 1); x |= (x >> 2); x |= (x >> 4); x |= (x >> 8); x |= (x >> 16); x |= (x >> 32); return 64 - count_bits_set(x); }
	uint16_t FORCE_INLINE byte_swap(uint16_t x) { return (x<<8)|(x>>8); }
	uint32_t FORCE_INLINE byte_swap(uint32_t x) { return (x<<24)|((x<<8)&0x00FF0000)|((x>>8)&0x0000FF00)|(x>>24); }
	uint64_t FORCE_INLINE byte_swap(uint64_t x) { return (x<<56)|((x<<40)&0x00FF000000000000)|((x<<24)&0x0000FF0000000000)|((x<<8)&0x000000FF00000000)|((x>>8)&0x00000000FF000000)|((x>>24)&0x0000000000FF0000)|((x>>40)&0x000000000000FF00)|(x>>56); }
#endif
// TODO: some other intrinsics to look into:
//  __builtin_ffs       one plus the index of the least significant 1-bit of x, or if x is zero, returns zero (similar to ctz)
//  __builtin_ctz       the number of trailing 0-bits in x, starting at the least significant bit position; if x is 0, the result is undefined (similar to ffs)
//  __builtin_parity    the parity of x, i.e. the number of 1-bits in x modulo 2
// (all operate on unsigned int and return int, add an l for unsigned long and ll for unsigned long-long arguments)

//  __builtin_choose_expr  like the ? operator except the condition needs to be constant and has some other benefits
//  __builtin_prefetch     pre-fetching data you will use can help

// Get ARRAYSIZE
#ifndef ARRAYSIZE
	#define ARRAYSIZE(x) sizeof(x)/sizeof(x[0])
#endif

// Get the minimum/maximum of 2
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// Get SIZE_T format specifier
#if defined(_WIN32) && (!defined(__USE_MINGW_ANSI_STDIO) || __USE_MINGW_ANSI_STDIO != 1)
#define SSIZE_T_FMT "I"
#else
#define SSIZE_T_FMT "z"
#endif

// Compile it right
#if defined(__cplusplus_cli)
#pragma unmanaged
#endif
#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif

// Warning disable support
#if defined(_MSC_VER)
#define WARNINGS_PUSH() __pragma(warning(push))
#define WARNINGS_POP()  __pragma(warning(pop))
#define WARNINGS_IGNORE_CONDITIONAL_EXPR_CONSTANT()         __pragma(warning(disable:4127))
#define WARNINGS_IGNORE_ASSIGNMENT_WITHIN_COND_EXPR()       __pragma(warning(disable:4706))
#define WARNINGS_IGNORE_TRUNCATED_OVERFLOW()                __pragma(warning(disable:4309))
#define WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED() __pragma(warning(disable:4512))
#define WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()   __pragma(warning(disable:4701))
#define WARNINGS_IGNORE_DIV_BY_0()				            __pragma(warning(disable:4723 4724))
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#define WARNINGS_PUSH() _Pragma("GCC diagnostic push")
#define WARNINGS_POP()  _Pragma("GCC diagnostic pop")
#define WARNINGS_IGNORE_CONDITIONAL_EXPR_CONSTANT()         
#define WARNINGS_IGNORE_ASSIGNMENT_WITHIN_COND_EXPR()       
#define WARNINGS_IGNORE_TRUNCATED_OVERFLOW()                _Pragma("GCC diagnostic ignored \"-Woverflow\"")
#define WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED() 
#define WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()   _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#define WARNINGS_IGNORE_DIV_BY_0()                          _Pragma("GCC diagnostic ignored \"-Wdiv-by-zero\"")
#else
#define WARNINGS_PUSH() 
#define WARNINGS_POP()  
#define WARNINGS_IGNORE_CONDITIONAL_EXPR_CONSTANT()         
#define WARNINGS_IGNORE_ASSIGNMENT_WITHIN_COND_EXPR()       
#define WARNINGS_IGNORE_TRUNCATED_OVERFLOW()                
#define WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED() 
#define WARNINGS_IGNORE_POTENTIAL_UNINIT_VALRIABLE_USED()   
#define WARNINGS_IGNORE_DIV_BY_0()                          
#endif

// Compile-time assert
#ifdef _DEBUG
#define CASSERT(expr)		char _UNIQUE_NAME[expr]
#define _UNIQUE_NAME		_MAKE_NAME(__LINE__)
#define _MAKE_NAME(line)	_MAKE_NAME2(line)
#define _MAKE_NAME2(line)	cassert_##line
#else
#define CASSERT(expr)
#endif

// Error and Warning Messages
#if defined(MSCOMP_WITH_ERROR_MESSAGES) || defined(MSCOMP_WITH_WARNING_MESSAGES)
#include <stdio.h>
#if _MSC_VER
#define snprintf _snprintf
#endif
#endif

#ifdef MSCOMP_WITH_ERROR_MESSAGES
#define SET_ERROR(s, ...)	snprintf(s->error, ARRAYSIZE(s->error), __VA_ARGS__)
#define INIT_STREAM_ERROR_MESSAGE(s) s->error[0] = 0
#else
#define SET_ERROR(s, ...)
#define INIT_STREAM_ERROR_MESSAGE(s)
#endif

#ifdef MSCOMP_WITH_WARNING_MESSAGES
#define SET_WARNING(s, ...)	snprintf(s->warning, ARRAYSIZE(s->warning), __VA_ARGS__)
#define INIT_STREAM_WARNING_MESSAGE(s) s->warning[0] = 0
#else
#define SET_WARNING(s, ...)
#define INIT_STREAM_WARNING_MESSAGE(s)
#endif

// Stream initialization and checking
#define INIT_STREAM(s, c, f) \
	if (UNLIKELY(s == NULL)) { SET_ERROR(s, "Error: Invalid stream provided"); return MSCOMP_ARG_ERROR; } \
	s->format = f; s->compressing = c; \
	s->in = NULL; s->out = NULL; \
	s->in_avail = 0; s->out_avail = 0; \
	s->in_total = 0; s->out_total = 0; \
	INIT_STREAM_ERROR_MESSAGE(s); INIT_STREAM_WARNING_MESSAGE(s); \
	s->state = NULL
#define CHECK_STREAM(s, c, f) \
	if (UNLIKELY(s == NULL || s->format != f || s->compressing != c || s->in == NULL || s->out == NULL)) { SET_ERROR(s, "Error: Invalid stream provided"); return MSCOMP_ARG_ERROR; }
#define CHECK_STREAM_PLUS(s, c, f, x) \
	if (UNLIKELY(s == NULL || s->format != f || s->compressing != c || s->in == NULL || s->out == NULL || (x))) { SET_ERROR(s, "Error: Invalid stream provided"); return MSCOMP_ARG_ERROR; }

#define ADVANCE_IN(s, x)      s->in  += (x);          s->in_total  += (x);          s->in_avail -= (x)
#define ADVANCE_IN_TO_END(s)  s->in  += s->in_avail;  s->in_total  += s->in_avail;  s->in_avail  = 0
#define ADVANCE_OUT(s, x)     s->out += (x);          s->out_total += (x);          s->out_avail -= (x)
#define ADVANCE_OUT_TO_END(s) s->out += s->out_avail; s->out_total += s->out_avail; s->out_avail  = 0

// Dump data from internal output buffer to stream
// Used by all inflate/deflate functions (except copy)
#define DUMP_OUT(state, stream) \
	if (state->out_avail) \
	{ \
		const size_t out_size = MIN(state->out_avail, stream->out_avail); \
		memcpy(stream->out, state->out + state->out_pos, out_size); \
		stream->out       += out_size; \
		stream->out_total += out_size; \
		if (state->out_avail == out_size) \
		{ \
			stream->out_avail -= out_size; \
			/*state->out_pos     = 0;*/ \
			state->out_avail   = 0; \
		} \
		else \
		{ \
			stream->out_avail  = 0; \
			state->out_pos    += out_size; \
			state->out_avail  -= out_size; \
			return MSCOMP_OK; \
		} \
	}

// Append data to internal input buffer
// Used by all inflate/deflate functions (except copy)
#define APPEND_IN(state, stream, OP) \
	if (state->in_avail) \
	{ \
		for (;;) \
		{ \
			const size_t copy = MIN(state->in_needed, stream->in_avail); \
			memcpy(state->in + state->in_avail, stream->in, copy); \
			state->in_avail  += copy; \
			state->in_needed -= copy; \
			ADVANCE_IN(stream, copy); \
			if (!finish && state->in_needed) { return MSCOMP_OK; } /* not enough to (de)compress yet */ \
			OP \
			break; \
		} \
		state->in_avail = 0; \
	}

#define ALL_AT_ONCE_WRAPPER(name, method) \
	MSCompStatus name(const_bytes in, size_t in_len, bytes out, size_t* _out_len) \
	{ \
		mscomp_stream strm; \
		MSCompStatus status = method##_init(&strm); \
		if (UNLIKELY(status != MSCOMP_OK)) { return status; } \
		strm.in = in; \
		strm.in_avail = in_len; \
		strm.out = out; \
		strm.out_avail = *_out_len; \
		status = method(&strm, true); \
		*_out_len = strm.out_total; \
		method##_end(&strm); \
		return LIKELY(status == MSCOMP_STREAM_END) ? MSCOMP_OK : (status == MSCOMP_OK ? MSCOMP_BUF_ERROR : status); \
	}

#endif