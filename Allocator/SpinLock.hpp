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
// Implements a very fast spin lock.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_SPIN_LOCK_HPP
#define PC_BASE_ALLOCATOR_SPIN_LOCK_HPP

#include "Atomic.hpp"
#include "Memory.hpp"
#include "ThreadUtils.hpp"

namespace Base {

class SpinLock {
private:
    unsigned int* lockValue_;

public:
    SpinLock(unsigned int* lock) : lockValue_(lock) {
        Lock();
    }

    ~SpinLock()	{
        Unlock();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Waits until the spin lock is acquired.
    void Lock() {
#if defined(PLATFORM_WINDOWS)
        if(Atomic::CompareExchange(lockValue_, 1, 0) != 0) {
            unsigned int waitCount = 1;
            ThreadUtils::Wait();

            while(true) {
                // Spin on the lock value without using CAS because it's faster.
                if(*(volatile unsigned int*)lockValue_ == 0) {
                    // Do a CAS read to be really sure the lock is availalble.
                    if(Atomic::CompareExchange(lockValue_, 1, 0) == 0) {
                        return; // Lock acquired.
                    }
                }

                // The lock is not free, wait for it using exponential back-off.
                ThreadUtils::SpinWait(waitCount);
                waitCount *= 2;

                if(waitCount > 1024) {
                    waitCount = 1024;
                }
            }
        }
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Releases the spin lock.
    void Unlock() {
        Atomic::CompareExchange(lockValue_, 0, 1);
    }
};

} // namespace Base
#endif
