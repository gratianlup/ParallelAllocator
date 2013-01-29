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
// Defines helpers for performing atomic operations on integers.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_ATOMIC_HPP
#define PC_BASE_ALLOCATOR_ATOMIC_HPP

#include "ThreadUtils.hpp"
#if defined(PLATFORM_WINDOWS)
    #include <windows.h>
    #include <intrin.h>

    extern "C" long _InterlockedAdd(long volatile* , long);
    extern "C" __int64 _InterlockedAdd64(__int64 volatile* , __int64);
    extern "C" long _InterlockedDecrement(long volatile*);
    extern "C" __int64 _InterlockedDecrement64(__int64 volatile*);
    extern "C" long _InterlockedExchange(long volatile* , long);
    extern "C" __int64 _InterlockedExchange64(__int64 volatile* , __int64);
    extern "C" void*  _InterlockedExchangePointer(void*  volatile* , void*);
    extern "C" long _InterlockedExchangeAdd(long volatile* , long);
    extern "C" __int64 _InterlockedExchangeAdd64(__int64 volatile* , __int64);
    extern "C" long _InterlockedCompareExchange (long volatile* , long, long);
    extern "C" __int64 _InterlockedCompareExchange64(__int64 volatile* , __int64, __int64);
    extern "C" __int64 _InterlockedCompareExchange128(__int64 volatile* ,__int64, __int64, __int64*);
    extern "C" long _InterlockedIncrement(long volatile*);
    extern "C" __int64 _InterlockedIncrement64(__int64 volatile*);
    extern "C" long _InterlockedOr(long volatile* , long);
    extern "C" char _InterlockedOr8(char volatile* , char);
    extern "C" short _InterlockedOr16(short volatile* , short);
    extern "C" __int64 _InterlockedOr64(__int64 volatile* , __int64);
    extern "C" void* _InterlockedCompareExchangePointer (void*  volatile* , void* , void*);
    extern "C" long _InterlockedExchangeAdd(long volatile* , long);
    extern "C" __int64 _InterlockedExchangeAdd64(__int64 volatile* , __int64);
    extern "C" long _InterlockedAnd(long volatile* , long);
    extern "C" char _InterlockedAnd8(char volatile* , char);
    extern "C" short _InterlockedAnd16(short volatile* , short);
    extern "C" __int64 _InterlockedAnd64(__int64 volatile* , __int64);
    extern "C" long _InterlockedXor(long volatile* , long);
    extern "C" char _InterlockedXor8(char volatile* , char);
    extern "C" short _InterlockedXor16(short volatile* , short);
    extern "C" __int64 _InterlockedXor64(__int64 volatile* , __int64);

    #pragma intrinsic(_InterlockedCompareExchange, _InterlockedCompareExchange16, _InterlockedCompareExchange64, _InterlockedCompareExchange128)
    #pragma intrinsic(_InterlockedExchange, _InterlockedExchange64)
    #pragma intrinsic(_InterlockedCompareExchangePointer)
    #pragma intrinsic(_InterlockedAdd, _InterlockedAdd64)
    #pragma intrinsic(_InterlockedIncrement, _InterlockedIncrement16, _InterlockedIncrement64)
    #pragma intrinsic(_InterlockedDecrement, _InterlockedDecrement16, _InterlockedDecrement64)
    #pragma intrinsic(_InterlockedAnd, _InterlockedAnd8, _InterlockedAnd16, _InterlockedAnd64)
    #pragma intrinsic(_InterlockedOr, _InterlockedOr8, _InterlockedOr16, _InterlockedOr64)
    #pragma intrinsic(_InterlockedXor, _InterlockedXor8, _InterlockedXor16, _InterlockedXor64)*/
#else
    static_assert(false, "Not yet implemented.");
#endif

namespace Base {

class Atomic {
public:
    static unsigned int Increment(volatile unsigned int* location) {
#if defined(PLATFORM_WINDOWS)
        return (int)_InterlockedIncrement((long*)location);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned __int64 Increment64(volatile unsigned __int64* location) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedIncrement64((__int64*)location);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned int Decrement(volatile unsigned int* location) {
#if defined(PLATFORM_WINDOWS)
        return (unsigned int)_InterlockedDecrement((long*)location);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }
    
    static unsigned __int64 Decrement64(volatile unsigned __int64* location) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedDecrement64((__int64*)location);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned int Add(volatile unsigned int* location, unsigned int value) {
#if defined(PLATFORM_WINDOWS)
        return (unsigned int)_InterlockedExchangeAdd((long*)location, (long)value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static __int64 Add64(volatile __int64* location, __int64 value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedExchangeAdd64(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned int Exchange(volatile unsigned int* location, unsigned int value) {
#if defined(PLATFORM_WINDOWS)
        return (unsigned int)InterlockedExchange((long*)location, (long)value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }
    
    static __int64 Exchange64(volatile __int64* location, __int64 value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedExchange64(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned int CompareExchange(volatile unsigned int* location, 
                                        unsigned int value, unsigned int comparand) {
#if defined(PLATFORM_WINDOWS)
        return (unsigned int)_InterlockedCompareExchange((long*)location, (long)value, 
                                                         (long)comparand);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned __int64 CompareExchange64(volatile unsigned __int64* location, 
                                              unsigned __int64 value,
                                              unsigned __int64 comparand) {
#if defined(PLATFORM_WINDOWS)
        return (unsigned __int64)_InterlockedCompareExchange64((__int64*)location, 
                                                               value, comparand);
#else
    static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned __int64 CompareExchange128(volatile unsigned __int64* location, 
                                               unsigned __int64 valueHigh, 
                                               unsigned __int64 valueLow,
                                               unsigned __int64* comparand) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedCompareExchange128((__int64*)location, valueHigh, valueLow,
                                              (__int64*)comparand);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static void* CompareExchangePointer(void* volatile* location, void* value, 
                                        void* comparand) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedCompareExchangePointer(location, value, comparand);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned int And(volatile unsigned int* location, unsigned int value) {
#if defined(PLATFORM_WINDOWS)
        return (unsigned int)_InterlockedAnd((long*)location, (long)value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static char And8(volatile char* location, char value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedAnd8(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static short And16(volatile short* location, short value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedAnd16(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static __int64 And64(volatile __int64* location, __int64 value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedAnd64(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned int Or(volatile unsigned int* location, unsigned int value) {
#if defined(PLATFORM_WINDOWS)
        return (unsigned int)_InterlockedOr((long*)location, (long)value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static char Or8(volatile char* location, char value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedOr8(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static short Or16(volatile short* location, short value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedOr16(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static __int64 Or64(volatile __int64* location, __int64 value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedOr64(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned int Xor(volatile unsigned int* location, unsigned int value) {
#if defined(PLATFORM_WINDOWS)
        return (unsigned int)_InterlockedXor((long*)location, (long)value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static char Xor8(volatile char* location, char value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedXor8(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static short Xor16(volatile short* location, short value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedXor16(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static __int64 Xor64(volatile __int64* location, __int64 value) {
#if defined(PLATFORM_WINDOWS)
        return _InterlockedXor64(location, value);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned __int64 SetBit64(volatile unsigned __int64* location, 
                                     unsigned int position) {
#if defined(PLATFORM_WINDOWS)
        unsigned __int64 temp;
        unsigned __int64 mask = 1ULL << position;
        unsigned int waitCount = 0;

        while(true) {
            unsigned __int64 oldValue = *location;
            unsigned __int64 newValue = oldValue | mask;

            temp = CompareExchange64(location, newValue, oldValue);

            if(temp == oldValue) {
                break; // Value could be changed.
            }

            ThreadUtils::SpinWait(waitCount);
        }
        return temp;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    static unsigned __int64 ResetBit64(volatile unsigned __int64* location, 
                                       unsigned int position) {
#if defined(PLATFORM_WINDOWS)
        unsigned __int64 temp;
        unsigned __int64 mask =  ~(1ULL << position);
        unsigned int waitCount = 0;

        while(true) {
            unsigned __int64 oldValue = *location;
            unsigned __int64 newValue = oldValue & mask;

            temp = CompareExchange64(location, newValue, oldValue);

            if(temp == oldValue) {
                break; // Value could be changed.
            }

            ThreadUtils::SpinWait(waitCount);
        }
        return temp;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }
};

} // namespace Base
#endif
