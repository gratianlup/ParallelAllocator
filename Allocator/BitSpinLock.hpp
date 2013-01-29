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
// Implements a spin-lock that spins around a single bit,
// the rest of the integer being available for other data.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_BIT_SPIN_LOCK_HPP
#define PC_BASE_ALLOCATOR_BIT_SPIN_LOCK_HPP

#include "Memory.hpp"
#include "Atomic.hpp"

namespace Base {

// Data layout in memory:
// |aaaaaaaaaXbbbbbbbbbbbbbbbb|
//     ^     ^       ^
//     |     |       |
// High part |   Low part
//         Lock
template <class T, unsigned int Index>
class BitSpinLock {
public:
    // Implements the atomic operations, based on the integer type.
    template<class Type> // Gives a compiler error if an invalid type is used.
    struct AtomicSelector;

    template <>
    struct AtomicSelector<unsigned short> {
        static short CompareExchange(volatile unsigned short* location, 
                                     unsigned short value, 
                                     unsigned short comparand) {
            return Atomic::CompareExchange16(location, value, comparand);
        }
    };

    template <>
    struct AtomicSelector<unsigned int>	{
        static unsigned int CompareExchange(volatile unsigned int* location, 
                                            unsigned int value, 
                                            unsigned int comparand)	{
            return Atomic::CompareExchange(location, value, comparand);
        }
    };

    template <>
    struct AtomicSelector<unsigned __int64> {
        static unsigned __int64 CompareExchange(volatile unsigned __int64* location, 
                                                unsigned __int64 value, 
                                                unsigned __int64 comparand) {
            return Atomic::CompareExchange64(location, value, comparand);
        }
    };

    // Verifies whether the a type is an accepted one (signed/unsigned integer).
    template <class U>
    struct TypeValidator {
        template<class V>
        struct ValidType { enum { Valid = false }; };

        template<> struct ValidType<short>   { enum { Valid = true }; };
        template<> struct ValidType<int>     { enum { Valid = true }; };
        template<> struct ValidType<__int64> { enum { Valid = true }; };

        template<> struct ValidType<unsigned short>   { enum { Valid = true }; };
        template<> struct ValidType<unsigned int>     { enum { Valid = true }; };
        template<> struct ValidType<unsigned __int64> { enum { Valid = true }; };

        enum { Valid = ValidType<U>::Valid };
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static const T DataMask = ((((T)1 << (sizeof(T) * 8)) - 1)) - ((T)1 << Index); // 11101...111
    static const T LockMask = ~DataMask;
    static const T LowPartMask = ((T)1 << Index) - 1;
    static const T HighPartMask =  ((((T)1 << (sizeof(T) * 8)) - 1)) - 
                                   (((T)1 << (Index + 1)) - 1);
    T lockValue_;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsLockedSet(T value) { return (value & LockMask);   }
    T SetLocked(T value)      { return (value |= LockMask);  }
    T ResetLocked(T value)    { return (value &= ~LockMask); }

public:
    BitSpinLock(T initialValue) : lockValue_(initialValue) {
        StaticAssert<TypeValidator<T>::Valid>();
        StaticAssert<Index<(sizeof(T) * 8)>();
    }

    BitSpinLock() : lockValue_(0) {
        StaticAssert<TypeValidator<T>::Valid>();
        StaticAssert<Index<(sizeof(T) * 8)>();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Waits until the lock could be successfully acquired.
    void Lock() {
        T oldValue = lockValue_;
        T newValue = SetLocked(oldValue);

        // In order to properly acquire the lock, it should be released.
        oldValue = ResetLocked(oldValue);

        if(lockValue_ != newValue) {
            ThreadUtils::SwitchToThread();
        }

        while((newValue = AtomicSelector<T>::CompareExchange(&lockValue_, newValue, 
                                                             oldValue)) != oldValue) {
            oldValue = newValue;
            newValue = SetLocked(oldValue);
            oldValue = ResetLocked(oldValue);
            ThreadUtils::Wait();
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Releases the lock.
    void Unlock() {
        T oldValue = lockValue_;
        T newValue = ResetLocked(oldValue);

        while((newValue = AtomicSelector<T>::CompareExchange(&lockValue_, newValue, 
                                                             oldValue)) != oldValue) {
            oldValue = newValue;
            newValue = ResetLocked(oldValue);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Extracts the low part (from LSB to the lock bit).
    T GetLowPart() {
        // Extracts the low part of the lock value (not including the lock bit).
        return Memory::ReadValue(&lockValue_) & LowPartMask;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Sets the low part (from LSB to the lock bit) to the specified value.
    void SetLowPart(T value) {
        T oldValue = lockValue_;
        T newValue = (oldValue & ~LowPartMask) | value;

        while((newValue = AtomicSelector<T>::CompareExchange(&lockValue_, newValue, 
                                                             oldValue)) != oldValue) {
            oldValue = newValue;
            newValue = (oldValue & ~LowPartMask) | value;
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Increments the low part (from LSB to the lock bit) with the specified value.
    void AddLowPart(T value) {
        T oldValue = lockValue_;
        T newValue = (oldValue & ~LowPartMask) | ((oldValue&  LowPartMask) + value);

        while((newValue = AtomicSelector<T>::CompareExchange(&lockValue_, newValue, 
                                                             oldValue)) != oldValue) {
            oldValue = newValue;
            newValue = (oldValue & ~LowPartMask) | ((oldValue & LowPartMask) + value);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Extracts the low part (from LSB to the lock bit).
    T GetHighPart() {
        // Extracts the low part of the lock value (not including the lock bit).
        return Memory::ReadValue(&lockValue_) & HighPartMask;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Sets the low part (from LSB to the lock bit) to the specified value.
    void SetHighPart(T value) {
        T oldValue = lockValue_;
        T newValue = (oldValue & ~HighPartMask) | (value << (Index + 1));

        while((newValue = AtomicSelector<T>::CompareExchange(&lockValue_, newValue, 
                                                             oldValue)) != oldValue) {
            oldValue = newValue;
            newValue = (oldValue & ~HighPartMask) | (value << Index);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Increments the low part (from LSB to the lock bit) with the specified value.
    void AddHighPart(T value) {
        T oldValue = lockValue_;
        T newValue = (oldValue & ~HighPartMask) | 
                     ((((oldValue & HighPartMask) >> (Index + 1)) + value) << (Index + 1));

        while((newValue = AtomicSelector<T>::CompareExchange(&lockValue_, newValue, 
                                                             oldValue)) != oldValue) {
            oldValue = newValue;
            newValue = (oldValue & ~HighPartMask) | 
                       ((((oldValue & HighPartMask) >> Index) + value) << Index);
        }
    }
};


// Wrapper that provides RAAI functionality for a BitSpinLock.
template <class T, unsigned int Index>
class BSLHolder {
private:
    BitSpinLock<T, Index>* lock_;

public:
    BSLHolder(BitSpinLock<T, Index>* bitLock) : lock_(bitLock) {}

   ~BSLHolder() { 
       lock_->Unlock(); 
   }
};

} // namespace Base
#endif
