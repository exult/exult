/*
Copyright (C) 2025  The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef XMIDIRECYCLABLE_H_INCLUDED
#define XMIDIRECYCLABLE_H_INCLUDED

#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

struct XMidiEvent;

/// Base class for various Xmidi classes to allow objects to be reused
/// instead of needing to call new and delete hundreds of times on tiny objects
/// Freed objects are added to the free list and when a new object is needed
/// an object is popped from the freelist if there is one otherwise a new one is
/// allocated with new
/// \tparam T The type of the recyclable class. This class must directly inherit from XMidiRecyclable
template <class T>
class XMidiRecyclable {
public:
	// Free this object. Adds it to the free list
	void FreeThis() noexcept {
		static_assert(std::is_base_of<XMidiRecyclable<T>, T>());

		auto lock = lockMutex();
		// if failed to get lock do not add to list just delete
		T* obj = static_cast<T*>(this);
		if (lock) {
			FreeList::FreeIt(obj);
		} else {
			delete obj;
		}
	}

	// Create an object of the Type (with default constructor)
	static T* Create() {
		auto lock = lockMutex();

		// No lock or no list then create new
		if (!lock || FreeList::empty()) {
			return new T();
		}

		// Pop List head using std::exchange and Return calling placement new
		return new (FreeList::Pop()) T();
	}

	// Create an object of the Type (with constructor arguments)
	template <typename TArg1, typename... TArgs>
	static T* Create(TArg1 arg1, TArgs... args) {
		auto lock = lockMutex();

		// No lock or no list then create new
		if (!lock || FreeList::empty()) {
			return new T(arg1, args...);
		}

		// Pop List head using std::exchange and Return calling placement new
		return new (FreeList::Pop()) T(arg1, args...);
	}

	using MutexSpecialization = XMidiRecyclable<XMidiEvent>;

	// Lock the Mutex
	static std::unique_lock<std::recursive_mutex> lockMutex() {
		// If disabled, return unowned lock
#ifdef XMIDI_DISABLE_RECYCLING
		return std::unique_lock();
#endif

		// Create Mutex if needed
		// Mutex will only ever created by the first call to one of the Create
		// functions It will never ned to be created by a call to FreeThis.
		// Can't free something if it wasn't created
		if (!MutexSpecialization::Mutex) {
			MutexSpecialization::Mutex
					= std::make_unique<std::recursive_mutex>();
		}
		return std::unique_lock(*MutexSpecialization::Mutex);
	}

private:
	// mutex is only instantiated by XMidiRecyclable<struct XMidiEvent>
	static std::unique_ptr<std::recursive_mutex> Mutex;

	// We are friends to all our specializations
	template <typename Tf>
	friend class XMidiRecyclable;

	class FreeList {
		int count = 0;
		T*  Head  = nullptr;

	public:
		static FreeList instance;

		static bool empty() {
			return instance.Head == nullptr;
		}

		static void FreeIt(T* obj) {
			assert(obj->next == nullptr);
			// call destructor on the object
			obj->~T();
			// push it to the front of the list
			obj->next     = instance.Head;
			instance.Head = obj;
			instance.count++;
		}

		static T* Pop() {
			if (instance.Head == nullptr) {
				return nullptr;
			}
			instance.count--;
			return std::exchange<T*>(
					instance.Head,
					std::exchange<T*>(instance.Head->next, nullptr));
		}

		// Destructor should clean up the allocated memory on program exit when
		// the runtime calls the destructors of all the globals and class
		// statics. If some compiler doesn't emit the calls to the destructors
		// the runtime might report memory leaks but that is not my problem
		~FreeList() {
			while (Head) {
				// directly call operator delete so destructor is not called
				operator delete(Pop());
			}
		}
	};

public:
	T* next = nullptr;    // Linked list next pointer
};

// needed explicit instantiation declaration to supress warnings from clang
template <>
XMidiRecyclable<XMidiEvent>::FreeList
		XMidiRecyclable<XMidiEvent>::FreeList::instance;

template <>
std::unique_ptr<std::recursive_mutex> XMidiRecyclable<XMidiEvent>::Mutex;
#endif
