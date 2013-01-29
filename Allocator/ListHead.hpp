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

#ifndef LIST_HEAD_H
#define LIST_HEAD_H

/**
** Describes a list of public locations (used when freeing public locations).
** ---------------------------------------------------------------------------------------------------------*/
template <class T>
class ListHead {
private:
#if defined(PLATFORM_32)
	unsigned int Count;
	unsigned int First;
	typedef int PtrType;
#else
	unsigned __int64 Count:16;
	unsigned __int64 First:48; // Big enough for the pointers used in current 64-bit systems.
	typedef unsigned __int64 PtrType;
#endif

public:
	ListHead() {}

	ListHead(unsigned __int64 value) {
		*((unsigned __int64*)&Count) = value;
	}

	ListHead(const ListHead<T>& other) {
		*((unsigned __int64*)&Count) = *((unsigned __int64*)&other.Count);
	}

	ListHead(const volatile ListHead<T>& other) {
		*((unsigned __int64*)&Count) = *((unsigned __int64*)&other.Count);
	}

	ListHead(int count, void* first) : Count(count), First((PtrType)first) { }

	int GetCount() { return Count; }
	void SetCount(int count) { Count = count; }
	T GetFirst() { return reinterpret_cast<T>(First); }
	void SetFirst(T* address) { First = (PtrType)address; }

	bool operator== (const ListHead<T>& other)	{
		return *(reinterpret_cast<unsigned __int64*>((void*)&other)) ==
               *(reinterpret_cast<unsigned __int64*>((void*)this));
	}

	bool operator!= (const ListHead<T>& other)	{
		return !this->operator ==(other);
	}

	operator unsigned __int64() {
		return* (reinterpret_cast<unsigned __int64*>((void*)this));
	}

	static const ListHead<T> ListEnd;
};

// Definition of the list end (or list empty) marker.
template <class T>
const ListHead<T> ListHead<T>::ListEnd = ListHead<T>(0, Constants::LIST_END);


/**
** Head used with timed stacks.
** ---------------------------------------------------------------------------------------------------------*/
template <class T>
class TimedListHead
{
private:
#if defined(PLATFORM_32)
	unsigned int Count : 8;
	unsigned int Time  : 24; // Enough for second-based resolution.
	unsigned int First;
	typedef unsigned int PtrType;
#else
	unsigned int Count;
	unsigned int Time;
	unsigned unsigned __int64 First;
	typedef unsigned unsigned __int64 PtrType;
#endif

public:
#if defined(PLATFORM_32)
	static const unsigned int MaxTime = 0x7FFFFF;
#else
	static const unsigned int MaxTime = 0x7FFFFFFF;
#endif

	//* ******************************************************************************************************
	// Constructors
	//* ******************************************************************************************************
	TimedListHead() {}

	TimedListHead(unsigned __int64 value) {
#if defined(PLATFORM_32)
		*((unsigned __int64*)this) = value;
		Time = MaxTime;
#else
		*((unsigned __int64*)this) = value;
		*(((unsigned __int64*)this) + 1) = value;
		Time = MaxTime;
#endif
	}

	TimedListHead(const TimedListHead<T>& other) {
#if defined(PLATFORM_32)
		*((unsigned int*)this) = *((unsigned int*)&other);
		*(((unsigned int*)this) + 1) = *(((unsigned int*)&other) + 1);
#else
		*((unsigned __int64*)this) = *((unsigned __int64*)&other);
		*(((unsigned __int64*)this) + 1) = *(((unsigned __int64*)&other) + 1);
#endif
	}

	TimedListHead(const volatile TimedListHead<T>& other) {
#if defined(PLATFORM_32)
		*((unsigned int*)this) = *((unsigned int*)&other);
		*(((unsigned int*)this) + 1) = *(((unsigned int*)&other) + 1);
#else
		*((unsigned __int64*)this) = *((unsigned __int64*)&other);
		*(((unsigned __int64*)this) + 1) = *(((unsigned __int64*)&other) + 1);
#endif
	}

	TimedListHead(unsigned int count, void* first) : Count(count), First((PtrType)first) {}
	TimedListHead(unsigned int count, void* first, int time) : Count(count), First((PtrType)first), Time(time) { }

	//* ******************************************************************************************************
	// Accessors
	//* ******************************************************************************************************
	unsigned int GetCount() { return Count; }
	void SetCount(unsigned int count) { Count = count; }
	unsigned int GetTime() { return Time; }
	void SetTime(unsigned int value) { Time = value; }
	T GetFirst() { return reinterpret_cast<T>(First); }
	void SetFirst(T* address) { First = (PtrType)address; }

	//* ******************************************************************************************************
	// Overloaded operators
	//* ******************************************************************************************************
	bool operator== (const TimedListHead<T>& other)	{
#if defined(PLATFORM_32)
		return *((unsigned __int64*)&other) == *((unsigned __int64*)this);
#else
		return *((unsigned __int64*)&other) == *((unsigned __int64*)this) &&
			   * (((unsigned __int64*)&other) + 1) == *(((unsigned __int64*)this) + 1);
#endif
	}

	bool operator!= (const TimedListHead<T>& other)	{
		return !this->operator ==(other);
	}

	operator unsigned __int64() {
		return* (reinterpret_cast<unsigned __int64*>((void*)this));
	}
};

#endif
