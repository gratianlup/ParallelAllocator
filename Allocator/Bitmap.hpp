// Copyright (c) 2009 Gratian Lup. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials provided
// with the distribution.
//
// * The name "DocumentClustering" must not be used to endorse or promote
// products derived from this software without prior written permission.
//
// * Products derived from this software may not be called "DocumentClustering" nor
// may "DocumentClustering" appear in their names without prior written
// permission of the author.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Defines a helper for manipulating bit maps.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_BITMAP_HPP
#define PC_BASE_ALLOCATOR_BITMAP_HPP 

#if defined(PLATFORM_WINDOWS)
	#include <intrin.h>
#else
    static_assert(false, "Not yet implemented.");
#endif

namespace Base {

class Bitmap {
private:
	static const unsigned int Mask32[];

public:
	// Searches a 32-bit integer from most significant bit
    // to least significant bit for a set bit.
	static unsigned int SearchReverse(unsigned int mask) {
#if defined(PLATFORM_WINDOWS)
		unsigned int index;

		if(_BitScanReverse((unsigned long*)&index, mask)) {
			return index;
		}

		return -1; // Not found.
#else
        static_assert(false, "Not yet implemented.");
#endif
	}

	// Searches a 64-bit integer from most significant bit
    // to least significant bit for a set bit.
	static unsigned int SearchReverse(unsigned __int64 mask) {
#if defined(PLATFORM_32)
	#if defined(PLATFORM_WINDOWS)
		unsigned int index;

		if(_BitScanReverse((unsigned long*)&index, 
                          * (((unsigned int*)&mask) + 1))) {	
			return index + 32;
		}
		else if(_BitScanReverse((unsigned long*)&index, 
                               * ((unsigned int*)&mask))) {
			return index;
		}

		return -1; // Not found.
	#else
        static_assert(false, "Not yet implemented.");
	#endif
#else
	#if defined(PLATFORM_WINDOWS)
		unsigned int index;

		if(_BitScanReverse64(&index, mask))	{
			return index;
		}

		return -1; // Not found.
	#else
        static_assert(false, "Not yet implemented.");
	#endif
#endif
	}

	// Searches a 32-bit integer from least significant bit 
    // to most significant bit for a set bit.
	static unsigned int SearchForward(unsigned int mask) {
#if defined(PLATFORM_WINDOWS)
		unsigned int index;

		if(_BitScanForward((unsigned long*)&index, mask)) {
			return index;
		}

		return -1; // Not found.
#else
		static_assert(false, "Not yet implemented.");
#endif
	}

	// Searches a 64-bit integer from least significant bit
    // to most significant bit for a set bit.
	static unsigned int SearchForward(unsigned __int64 mask) {
#if defined(PLATFORM_32)
	#if defined(PLATFORM_WINDOWS)
		unsigned int index;

		if(_BitScanForward((unsigned long*)&index, 
                          * ((unsigned int*)&mask))) {	
			return index;
		}
		else if(_BitScanForward((unsigned long*)&index, 
                               * (((unsigned int*)&mask) + 1))) {
			return index + 32;
		}

		return -1; // Not found.
	#else
		static_assert(false, "Not yet implemented.");
	#endif
#else
	#if defined(PLATFORM_WINDOWS)
		unsigned int index;

		if(_BitScanReverse64(&index, mask)) {
			return index;
		}

		return -1; // Not found.
	#else
        static_assert(false, "Not yet implemented.");
	#endif
#endif
	}

	// Searches a 64-bit integer from the specified start bit
    // to least significant one for a set bit.
	static unsigned int SearchReverse(unsigned __int64 mask, unsigned int start) {
#if defined(PLATFORM_32)
	#if defined(PLATFORM_WINDOWS)
		unsigned int index;
		if(start < 32) {
			// Only the first 32 bits need to be checked.
			int data = *((unsigned int*)&mask) & ((1 << start) - 1);

			if(_BitScanReverse((unsigned long*)&index, data)) {
				return index; // Found!
			}
		}
		else {
			// Check the last 32 bits.
			unsigned int data = *((unsigned int*)&mask + 1) & Mask32[start - 32];

			if(_BitScanReverse((unsigned long*)&index, data)) {
				return index + 32; // Add 32 because we are in the last 32 bits.
			}
			else if(_BitScanReverse((unsigned long*)&index, 
                                    * ((unsigned int*)&mask))) {
				return index; // Found in the first 32 bits.
			}
		}

		return -1; // Not found.
	#else
        static_assert(false, "Not yet implemented.");
	#endif
#else
	#if defined(PLATFORM_WINDOWS)
		// On 64 bit we can check the bits using a single operation.
		unsigned __int64 dataMask = (start < 64) ? ~(-1 - ((1ULL << start) - 1)) : -1;

		if(_BitScanForward64((unsigned long*)&index, mask&  dataMask)) {
			return index; // Found!
		}

		return -1; // Not found.
	#else
        static_assert(false, "Not yet implemented.");
	#endif
#endif
	}

	// Searches a 64-bit integer from the specified start bit
    // to most significant bit for a set bit.
	static unsigned int SearchForward(unsigned __int64 mask, unsigned int start) {
#if defined(PLATFORM_32)
	#if defined(PLATFORM_WINDOWS)
		unsigned int index = 0;

		if(start < 32)	{
			// Check the first 32 bits.
			unsigned int data = *((unsigned int*)&mask) & ~((1 << start) - 1);

			if(_BitScanForward((unsigned long*)&index, data)) {
				return index; 
			}
			else if(_BitScanForward((unsigned long*)&index, 
                                    *((unsigned int*)&mask + 1))) {
				return index + 32; // Add 32 because we are in the last 32 bits.
			}
		}
		else {
			// We need to check only the last 32 bits.
			unsigned int data = *((unsigned int*)&mask + 1) & ~Mask32[start - 32];

			if(_BitScanForward((unsigned long*)&index, data)) {
				return index + 32; // Found!
			}
		}

		return -1; // Not found.		
	#else
		static_assert(false, "Not yet implemented.");
	#endif
#else
	#if defined(PLATFORM_WINDOWS)
		// On 64 bit we can check the bits using a single operation.
		unsigned __int64 data = mask&  ~((1ULL << start) - 1);

		if(_BitScanForward64((unsigned long*)&index, data)) {
			return index; // Found!
		}

		return -1; // Not found.
	#else
        static_assert(false, "Not yet implemented.");
	#endif
#endif
	}

    // Methods for setting/resetting bits.
	static void SetBit(unsigned int& mask, unsigned int index) {
		mask |= 1 << index;
	}

	static void SetBit(unsigned __int64& mask, unsigned int index) {
		mask |= 1 << index;
	}
	
	static bool IsBitSet(unsigned int& mask, unsigned int index) {
		return (mask&  (1 << index)) != 0;
	}

	static bool IsBitSet(const unsigned __int64& mask, unsigned int index) {
		return (mask&  (1ULL << index)) != 0;
	}

	static void ResetBit(unsigned int& mask, unsigned int index) {
		mask &= ~(1 << index);
	}

	static void ResetBit(unsigned __int64& mask, unsigned int index) {
		mask &= ~(1ULL << index);
	}
	
    // Returns the number of bits set to one.
	static int NumberOfSetBits(unsigned int mask) {
		mask = mask - ((mask >> 1) & 0x55555555);
		mask = (mask&  0x33333333) + ((mask >> 2) & 0x33333333);
		return ((mask + (mask >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
	}

	static unsigned int NumberOfSetBits64(unsigned __int64 mask) {
		mask = mask - ((mask >> 1) & 0x5555555555555555);
		mask = (mask&  0x3333333333333333) + ((mask >> 2) & 0x3333333333333333);
		return (unsigned int)((mask + (mask >> 4) & 0xF0F0F0FF0F0F0F) * 0x10101011010101) >> 24;
	}
};


const unsigned int Bitmap::Mask32[] = {
	0x0, 0x1, 0x3, 0x7, 0xf, 
	0x1f, 0x3f, 0x7f, 0xff, 
	0x1ff, 0x3ff, 0x7ff, 0xfff, 
	0x1fff, 0x3fff, 0x7fff, 0xffff, 
	0x1ffff, 0x3ffff, 0x7ffff, 0xfffff, 
	0x1fffff, 0x3fffff, 0x7fffff, 0xffffff, 
	0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff, 
	0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff, 
};

} // namespace Base
#endif
