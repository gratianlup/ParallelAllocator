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
// * The name "ParallelAllocator" must not be used to endorse or promote
// products derived from this software without prior written permission.
//
// * Products derived from this software may not be called "ParallelAllocator" nor
// may "ParallelAllocator" appear in their names without prior written
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
// Defines a set of helpers for implementing unrolled versions of operations on arrays.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_UNROLLED_LOOPS_HPP
#define PC_BASE_ALLOCATOR_UNROLLED_LOOPS_HPP

#if defined(PLATFORM_64)
    #if defined PLATFORM_WINDOWS
        #include <intrin.h>
    #else
        static_assert(false, "Not yet implemented.");
    #endif
#endif


// Performs a memory block copy using unrolled instructions.
// About 6x faster than 'memcpy' and 3x faster than with loops.
template<class T, unsigned int Left, unsigned int Right>
struct UnrolledCopy {
private:
    enum { go = (Left < (Right - 1)) };

public:
    static void Execute(T* destination, T* source) {
        *destination = *source;
        UnrolledCopy<T, go ? Left + 1 : 0, go ? Right : 0>::Execute(destination + 1, source + 1);
    }
};


// Specialization for the memory block copy so that the recursion stops.
template<class T>
struct UnrolledCopy<T, 0, 0> {
public:
    static void Execute(T* destination, T* source) {}
};


// Performs a logical OR between all items 
// of the given memory blocks using unrolled instructions.
template<class T,unsigned int Left, unsigned int Right>
struct UnrolledOr {
private:
    enum { go = (Left < (Right - 1)) };

public:
    static void Execute(T* destination, T* source) {
        *destination |= *source;
        UnrolledOr<T, go ? Left + 1 : 0, go ? Right : 0>::Execute(destination + 1, source + 1);
    }
};


// Specialization for the memory block OR so that the recursion stops.
template<class T>
struct UnrolledOr<T, 0, 0> {
public:
    static void Execute(T* destination, T* source) {}
};


// Sets all items of the memory block 
// to the specified value using unrolled instructions.
template<class T, T Value, unsigned int Left, unsigned int Right>
struct UnrolledSet {
private:
    enum { go = (Left < (Right - 1)) };

public:
    static void Execute(T* destination) {
        *destination = Value;
        UnrolledSet<T, Value, go ? Left + 1 : 0, go ? Right : 0>::Execute(destination + 1);
    }
};


// Specialization for the memory block OR so that the recursion stops.
template<class T, T Value>
struct UnrolledSet<T, Value, 0, 0> {
public:
    static void Execute(T* destination) {}
};


#if defined(PLATFORM_64)
// Sets all items of the memory block to the specified value
// using unrolled instructions. SSE2 version (sets 16 bytes at a time).
template<unsigned int Left, unsigned int Right>
struct UnrolledSet128 {
private:
    enum { go = (Left < (Right - 1)) };

public:
    static void Execute(__m128i* destination, __m128i value) {
        _mm_store_si128(destination, value);
        UnrolledSet128<go ? Left + 1 : 0, go ? Right : 0>::Execute(destination + 1, value);
    }
};


// Specialization for the memory block OR so that the recursion stops.
template<>
struct UnrolledSet128<0, 0> {
public:
    static void Execute(__m128i* destination, __m128i value) {}
};
#endif

#endif
