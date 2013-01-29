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
// Implements optimized routines for implementing the 'realloc' function.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_REALLOC_HPP
#define PC_BASE_ALLOCATOR_REALLOC_HPP

#include "Bitmap.hpp"

#if defined(PLATFORM_WINDOWS)
	#include <intrin.h>
#endif

namespace Base {

// Routines implemented in assembly.
extern "C" void ReallocX86_16(void* src, void* dst, int size);
extern "C" void ReallocSSE_64(void* src, void* dst, int size);
extern "C" void ReallocSSE_B64(void* src, void* dst, int size);
extern "C" void ReallocSSE2_64(void* src, void* dst, int size);
extern "C" void ReallocSSE2_B64(void* src, void* dst, int size);


struct ReallocX86 {
	static void Realloc(void* source, void* destination, unsigned int size) {
		char* srcPtr = (char*)source;
		char* dstPtr = (char*)destination;
		char* srcEndPtr = srcPtr + size;

		while(srcPtr < srcEndPtr) {
			// Try to copy up to 128 bytes on each step.
			unsigned int copySize;

			if(size >= 128) { 
                copySize = 128; 
            }
			else if(size >= 64) { 
                copySize = 64;  
            }
			else if(size >= 32) { 
                copySize = 32;  
            }
			else if(size >= 16) { 
                copySize = 16;  
            }
			else {
				// Copy only 4 bytes.
				*((int*)dstPtr) = *((int*)srcPtr);
				srcPtr += 4;
				dstPtr += 4;
				size -= 4;
				continue;
			}

			ReallocX86_16(srcPtr, dstPtr, copySize);
			srcPtr += copySize;
			dstPtr += copySize;
			size -= copySize;
		}
	}
};


struct ReallocSSE {
	static void Realloc(void* source, void* destination, unsigned int size) {
		char* srcPtr = (char*)source;
		char* dstPtr = (char*)destination;
		char* srcEndPtr = srcPtr + size;

		while(srcPtr < srcEndPtr) {
			// Try to copy up to 256 bytes on each step.
			unsigned int copySize;

			if(size >= 256) { 
                copySize = 256; 
            }
			else if(size >= 64) { 
                copySize = size; 
            }
			else {
				// Copy less than 64 bytes.
				ReallocSSE_B64(srcPtr, dstPtr, size);
				return;
			}
			
			ReallocSSE_64(srcPtr, dstPtr, copySize);
			srcPtr += copySize;
			dstPtr += copySize;
			size -= copySize;
		}
	}
};


struct ReallocSSE2 {
	static void Realloc(void* source, void* destination, unsigned int size) {
		char* srcPtr = (char*)source;
		char* dstPtr = (char*)destination;
		char* srcEndPtr = srcPtr + size;

		while(srcPtr < srcEndPtr) {
			// Try to copy up to 256 bytes on each step.
			unsigned int copySize;

			if(size >= 256) {
                copySize = 256; 
            }
			else if(size >= 64) { 
                copySize = size; 
            }
			else {
				// Copy less than 64 bytes.
				ReallocSSE2_B64(srcPtr, dstPtr, size);
				return;
			}
			
			ReallocSSE2_64(srcPtr, dstPtr, copySize);
			srcPtr += copySize;
			dstPtr += copySize;
			size -= copySize;
		}
	}
};


struct Realloc {
	typedef void (*REALLOC_FUNCTION)(void* src, void* dst, unsigned int size);
	static REALLOC_FUNCTION ReallocImpl;

	static void Initialize() {
        // Detect if there if SSE is supported and use
        // the optimized versions of the copy routines if possible.
		bool hasSSE = false;
		bool hasSSE2 = false;

#if defined(PLATFORM_WINDOWS)
		int cpuInfo[4];
		__cpuid(cpuInfo, 1);

		hasSSE  = (cpuInfo[3]&  (1 << 25)) != 0;
		hasSSE2 = (cpuInfo[3]&  (1 << 26)) != 0;
#else
#endif

		if(hasSSE2) {
			ReallocImpl = ReallocSSE2::Realloc;
		}
		else if(hasSSE) {
			ReallocImpl = ReallocSSE::Realloc;
		}
		else {
			ReallocImpl = ReallocX86::Realloc;
		}
	}

	static void Execute(void* source, void* destination, unsigned int size) {
		ReallocImpl(source, destination, size);
	}
};

Realloc::REALLOC_FUNCTION Realloc::ReallocImpl = nullptr;

} // namespace Base
#endif
