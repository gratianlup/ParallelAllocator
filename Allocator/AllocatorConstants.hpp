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
// Defines the constants used by all allocator modules.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_CONSTANTS_HPP
#define PC_BASE_ALLOCATOR_CONSTANTS_HPP

namespace Base {

struct AllocationInfo {
	unsigned int Size;
	unsigned int Bin;

	AllocationInfo() {}

	AllocationInfo(unsigned int size, unsigned int bin) : 
            Size(size), Bin(bin) {}
};


class Constants {
public:
    // The most common size for cache lines nowadays.
	static const unsigned int CACHE_LINE_SIZE = 64;                    
	static const unsigned int MAX_NUMA_NODES = 256;

#if defined(PLATFORM_NUMA)
	static const bool NUMA_ENABLED = true;
#else
	static const bool NUMA_ENABLED = false;
#endif

	static const unsigned __int64 GROUP_RETURN_PARTIAL = 0x3FFFEA200;

	static const unsigned int BLOCK_DESCRIPTOR_ALLOCATION_SIZE = 4096; // 1 page file on x86.
	static const unsigned int BLOCK_DESCRIPTOR_SIZE = 64;              // 1 cache line
	static const unsigned int BLOCK_DESCRIPTOR_CACHE = 4;              // 252 block descriptors.
	static const unsigned int BLOCK_SMALL_CACHE = 16;
	static const unsigned int BLOCK_LARGE_CACHE = 8;
	
	static const unsigned int THREAD_CONTEXT_ALLOCATION_SIZE = 64*  1024; // Enough for 27 threads.
	static const unsigned int THREAD_CONTEXT_SIZE = 2368;
	static const unsigned int THREAD_CONTEXT_CACHE = 1;

	static const unsigned int BA_ALLOCATION_SIZE = 8192;
	static const unsigned int BA_SIZE = 4032;
	static const unsigned int BA_CACHE = 1;

	static const unsigned int BLOCK_SIZE = 1024*  1024;      // 1 MB
	static const unsigned int SMALL_GROUP_SIZE = 16*  1024;  // 16 KB
	static const unsigned int LARGE_GROUP_SIZE = 64*  1024;  // 64 KB
	static const unsigned int SMALL_GROUP_HEADER_SIZE = 256; // 4 cache lines.
	static const unsigned int LARGE_GROUP_HEADER_SIZE = 192; // 3 cache lines.
	static const unsigned int GROUPS_PER_BLOCK = 64;

	static const unsigned int HUGE_GRANULARITY = 4096;         // 1 page file on x86/x64.
	static const unsigned int HUGE_HEADER_SIZE = 64;
	static const unsigned int WINDOWS_GRANULARITY = 64*  1024; // VirtualAlloc uses 64KB blocks.
	static const unsigned int HUGE_SPLIT_POSITION = 32*  1024; // ~32KB

#if defined(SORT)
	// If sort is defined, we used the number of the location instead of it's address.
	// It's slower, but saves a lot of space (2 bytes instead of 8 - on 64 bit).
	static const int LIST_END = -1;
#else
	static const int LIST_END = 0;
#endif	

	static const unsigned int SMALL_BINS = 31;
	static const unsigned int LARGE_BINS = 4;
	static const unsigned int BIN_NUMBER = SMALL_BINS + LARGE_BINS;
	static const unsigned int AFTER_SEGREGATED_START_BIN = 26;

	static const size_t MAX_TINY_SIZE       = 64;
	static const size_t MAX_SEGREGATED_SIZE = 896;
	static const size_t MAX_SMALL_SIZE      = 2688;
	static const size_t MAX_LARGE_SIZE      = 8128; // ~8 KB

	static const size_t ALLOCATION_SIZE_1 = 1152;
	static const size_t ALLOCATION_SIZE_2 = 1472;
	static const size_t ALLOCATION_SIZE_3 = 1792;
	static const size_t ALLOCATION_SIZE_4 = 2304;
	static const size_t ALLOCATION_SIZE_5 = 2688;

	static const unsigned int LARGE_ALLOCATION_SIZE_1 = 3200;
	static const unsigned int LARGE_ALLOCATION_SIZE_2 = 4048;
	static const unsigned int LARGE_ALLOCATION_SIZE_3 = 5397;
	static const unsigned int LARGE_ALLOCATION_SIZE_4 = 8096;

	static const unsigned int NOT_STOLEN = 255;

	static const size_t SmallBinSize[];
	static const size_t LargeBinSize[];
	static const AllocationInfo SmallAllocTable[];
	static const AllocationInfo SmallAllocTable2[];

	static const unsigned int MAX_HUGE_SIZE = 1048512; // ~1MB;
	static const unsigned int HUGE_BINS = 255;
	static const unsigned int HUGE_START = 3;
	static const unsigned int HUGE_CLEANING_INTERVAL = 1280000;
	static const unsigned int HugeCacheSize[];
	static const unsigned int HugeCacheTime[];
	static const unsigned int CACHE_CLEANING_INTERVAL = 30*1000; // 30 seconds.
	static const char* CACHE_THREAD_NAME; // Used for debugging in Visual C++.
	static const unsigned int MAX_HUGE_CACHE = 512;
};


const char* Constants::CACHE_THREAD_NAME = "Allocator_Cache_Thread";


const size_t Constants::SmallBinSize[] = {
	8, 12, 16, 20, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 
	192, 224, 256, 320, 384, 448, 512, 640, 768, 896,
	Constants::ALLOCATION_SIZE_1, Constants::ALLOCATION_SIZE_2,
	Constants::ALLOCATION_SIZE_3, Constants::ALLOCATION_SIZE_4,
	Constants::ALLOCATION_SIZE_5
};


const size_t Constants::LargeBinSize[] = {
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 // Prevents error in the test code.
};


const AllocationInfo Constants::SmallAllocTable[] = {
	// 64 structures in format Size, Bin
	AllocationInfo(8, 0),  AllocationInfo(8, 0),  AllocationInfo(8, 0), 
	AllocationInfo(8, 0),  AllocationInfo(8, 0),  AllocationInfo(8, 0), 
	AllocationInfo(8, 0),  AllocationInfo(8, 0),  AllocationInfo(8, 0), 
	AllocationInfo(12, 1), AllocationInfo(12, 1), AllocationInfo(12, 1), 
	AllocationInfo(12, 1), AllocationInfo(16, 2), AllocationInfo(16, 2), 
	AllocationInfo(16, 2), AllocationInfo(16, 2), AllocationInfo(20, 3), 
	AllocationInfo(20, 3), AllocationInfo(20, 3), AllocationInfo(20, 3), 
	AllocationInfo(24, 4), AllocationInfo(24, 4), AllocationInfo(24, 4), 
	AllocationInfo(24, 4), AllocationInfo(32, 5), AllocationInfo(32, 5), 
	AllocationInfo(32, 5), AllocationInfo(32, 5), AllocationInfo(32, 5), 
	AllocationInfo(32, 5), AllocationInfo(32, 5), AllocationInfo(32, 5), 
	AllocationInfo(40, 6), AllocationInfo(40, 6), AllocationInfo(40, 6), 
	AllocationInfo(40, 6), AllocationInfo(40, 6), AllocationInfo(40, 6), 
	AllocationInfo(40, 6), AllocationInfo(40, 6), AllocationInfo(48, 7), 
	AllocationInfo(48, 7), AllocationInfo(48, 7), AllocationInfo(48, 7), 
	AllocationInfo(48, 7), AllocationInfo(48, 7), AllocationInfo(48, 7), 
	AllocationInfo(48, 7), AllocationInfo(56, 8), AllocationInfo(56, 8), 
	AllocationInfo(56, 8), AllocationInfo(56, 8), AllocationInfo(56, 8), 
	AllocationInfo(56, 8), AllocationInfo(56, 8), AllocationInfo(56, 8), 
	AllocationInfo(64, 9), AllocationInfo(64, 9), AllocationInfo(64, 9), 
	AllocationInfo(64, 9), AllocationInfo(64, 9), AllocationInfo(64, 9), 
	AllocationInfo(64, 9), AllocationInfo(64, 9)
};


const AllocationInfo Constants::SmallAllocTable2[] = {
	AllocationInfo(0, 0), // Not used
	AllocationInfo(0, 0),
	AllocationInfo(0, 0),
	AllocationInfo(Constants::ALLOCATION_SIZE_1, Constants::AFTER_SEGREGATED_START_BIN + 0),
	AllocationInfo(Constants::ALLOCATION_SIZE_2, Constants::AFTER_SEGREGATED_START_BIN + 1),
	AllocationInfo(Constants::ALLOCATION_SIZE_3, Constants::AFTER_SEGREGATED_START_BIN + 2),
	AllocationInfo(Constants::ALLOCATION_SIZE_3, Constants::AFTER_SEGREGATED_START_BIN + 2),
	AllocationInfo(Constants::ALLOCATION_SIZE_4, Constants::AFTER_SEGREGATED_START_BIN + 3),
	AllocationInfo(Constants::ALLOCATION_SIZE_5, Constants::AFTER_SEGREGATED_START_BIN + 4),
	AllocationInfo(Constants::ALLOCATION_SIZE_5, Constants::AFTER_SEGREGATED_START_BIN + 4)
};


const unsigned int Constants::HugeCacheSize[] = {
	0, 0, 0, 32, 32, 31, 31, 31, 30, 30, 29, 28, 27, 26, 24, 22, 20, 16, 14, 
	12, 12, 11, 11, 10, 10, 9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 
	7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 
	6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
	5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
	3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};


const unsigned int Constants::HugeCacheTime[] = {
	0, 0, 0, 480, 480, 479, 479, 478, 477, 476, 474, 471, 468, 463, 457, 449, 437, 420, 370, 
	341, 321, 305, 292, 281, 271, 263, 256, 249, 243, 237, 232, 227, 222, 218, 214, 
	210, 206, 203, 199, 196, 193, 190, 187, 185, 182, 180, 177, 175, 173, 171, 168, 
	166, 164, 162, 160, 159, 157, 155, 153, 152, 150, 148, 147, 145, 144, 142, 141, 
	139, 138, 137, 135, 134, 133, 132, 130, 129, 128, 127, 126, 124, 123, 122, 121, 
	120, 119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 107, 106, 
	105, 104, 103, 102, 101, 101, 100, 99, 98, 97, 97, 96, 95, 94, 94, 93, 
	92, 92, 91, 90, 89, 89, 88, 87, 87, 86, 85, 85, 84, 83, 83, 82, 
	82, 81, 80, 80, 79, 79, 78, 77, 77, 76, 76, 75, 75, 74, 73, 73, 
	72, 72, 71, 71, 70, 70, 69, 69, 68, 68, 67, 67, 66, 66, 65, 65, 
	64, 64, 63, 63, 62, 62, 61, 61, 60, 60, 59, 59, 59, 58, 58, 57, 
	57, 56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 52, 52, 51, 51, 50, 
	50, 50, 49, 49, 49, 48, 48, 47, 47, 47, 46, 46, 45, 45, 45, 44, 
	44, 44, 43, 43, 43, 42, 42, 41, 41, 41, 40, 40, 40, 39, 39, 39, 
	38, 38, 38, 37, 37, 37, 36, 36, 36, 35, 35, 35, 34, 34, 34, 33, 
	33, 33, 33, 32, 32, 32, 31, 31, 31, 30, 30, 30
};

} // namespace Base
#endif
