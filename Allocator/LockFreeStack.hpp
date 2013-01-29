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
// Implements a simple lock-free stack.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_LOCK_FREE_STACK_HPP
#define PC_BASE_ALLOCATOR_LOCK_FREE_STACK_HPP

#include "ListHead.hpp"
#include "Atomic.hpp"
#include "Memory.hpp"
#include "ObjectList.hpp"
#include "FreeObjectList.hpp"

namespace Base {

#if LOCK_FREE
template <class T>
class Stack {
private:
    typedef ListHead<T*> HeadType;
    HeadType head_;
    unsigned int time_;
    unsigned int maxObjects_;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    bool CompareExchange(HeadType& oldHead, HeadType& newHead) {
#if defined(PLATFORM_32)
        return Atomic::CompareExchange64((__int64*)&head_, (__int64)newHead, 
                                         (__int64)oldHead) == (__int64)oldHead;
#else
        return Atomic::CompareExchange128((__int64*)&head_,* (((__int64*)&newHead) + 1),
                                         * ((__int64*)&newHead), (__int64*)&oldHead) == 1;
#endif
    }

public:
    Stack() : head_(0), maxObjects_(0xFFFFFFFF) { }
    Stack(int maxObjects) : head_(0), maxObjects_(maxObjects) { }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Tries to insert an object in the stack.
    // If the maximum number of objects is reached, the object 
    // isn't inserted anymore and the method returns the address of the object.
    // If the object could be inserted, the method returns nullptr.
    T* Push(T* node) {
        int waitCount = 0; // Used for back off.
        time_ = ThreadUtils::GetSystemTime();

        while(true) {
            HeadType oldHead = Memory::ReadValue(&head_);

            if(oldHead.GetCount() >= maxObjects_) {
                return node; // The stack has reached the maximum number of objects.
            }

            // Set the new head of the stack and update the oldest location.
            HeadType newHead = HeadType(oldHead.GetCount() + 1, node);

            // Is 'head' still the same?
            if(oldHead == Memory::ReadValue(&head_)) {
                // Link the node to the current head of the stack.
                node->Next = oldHead.GetFirst();

                if(CompareExchange(oldHead, newHead)) {
                    T* temp = newHead.GetFirst();
                    temp->Next = temp->Next;
                    return nullptr; // The head was successfully updated.
                }
            }

            if((++waitCount % 50) == 0) {
                // Give threads with a lower priority a chance to run.
                ThreadUtils::SwitchToThread();
            }
            else {
                // Back off.
                for(int i = 0; i < waitCount; i++) {
                    ThreadUtils::Wait();
                }
            }
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to extract the top object of the stack.
    // If the stack is empty, the method returns nullptr.
    T* Pop() {
        T* node;
        int waitCount = 0; // Used for back off.
        time_ = ThreadUtils::GetSystemTime();

        while(true) {
            HeadType oldHead = Memory::ReadValue(&head_);
            node = oldHead.GetFirst();

            if(node == nullptr) {
                return nullptr; // The stack is empty;
            }

            // Is 'head' still the same?
            if(oldHead == Memory::ReadValue(&head_)) {
                HeadType newHead = HeadType(oldHead.GetCount() - 1, node->Next);

                if(CompareExchange(oldHead, newHead)) {
                    return node; // The head was successfully updated.
                }
            }

            if((++waitCount % 50) == 0) {
                // Give threads with a lower priority a chance to run.
                ThreadUtils::SwitchToThread();
            }
            else {
                // Backoff.
                for(int i = 0; i < waitCount; i++) {
                    ThreadUtils::Wait();
                }
            }
        }
    }

    T* Peek() {
        return head_.GetFirst();
    }

    int Count() {
        return Memory::ReadValue(&head_).GetCount();
    }

    int OldestTime() {
        return time_;
    }

    int MaxObjects() {
        return maxObjects_; 
    }

    void SetMaxObjects(int value) { 
        maxObjects_ =  value; 
    }
};

#else

// Implementation that uses a lock.
template <class T>
class Stack {
private:
    FreeObjectList<> list_;
    unsigned int time_;

public:
    Stack() : list_(FreeObjectList<>(0xFFFFFFF)), time_(0x7FFFFFFF) {}
    Stack(unsigned int maxObj) : list_(FreeObjectList<>(maxObj)), time_(0x7FFFFFFF) {}

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Tries to insert an object in the stack.
    // If the maximum number of objects is reached, the object 
    // isn't inserted anymore and the method returns the address of the object.
    // If the object could be inserted, the method returns nullptr.
    T Push(T node) {
        time_ = ThreadUtils::GetSystemTime();
        return reinterpret_cast<T>(list_.AddObject(node));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to extract the top object of the stack.
    // If the stack is empty, the method returns nullptr.
    T Pop() {
        time_ = ThreadUtils::GetSystemTime();
        return reinterpret_cast<T>(list_.RemoveFirst());
    }

    T Peek() {
        return nullptr;
    }

    unsigned int Count() {
        return list_.GetCount();
    }

    unsigned int OldestTime() {
        return time_;
    }

    unsigned int MaxObjects() {
        return list_.GetMaxObjects();
    }

    void SetMaxObjects(unsigned int value) { 
        list_.SetMaxObjects(value);
    }
};
#endif

} // namespace Base {
#endif
