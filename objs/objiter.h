/**
 ** Objiter.h - Game objects iterator.
 **
 ** Written: 7/29/2000 - JSF
 **/

/*
Copyright (C) 2000  Jeffrey S. Freedman

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

#ifndef INCL_OBJITER
#define INCL_OBJITER    1

#include "objlist.h"

class   Map_chunk;
class   Game_object;

/*
 *  Want to make all object iterators modification-safe.  For now, just
 *  trying to detect modification during iteration.
 */
template<class T>
class T_Safe_object_iterator {
	T_Object_list<T> &list;
public:
	T_Safe_object_iterator(T_Object_list<T> &l) : list(l) {
		list.add_iterator();
	}
	~T_Safe_object_iterator() {
		list.remove_iterator();
	}
};

/*
 *  Iterate through list of objects.
 */
template<class T>
class T_Object_iterator : public T_Safe_object_iterator<T> {
protected:
	T *first;
	T *stop;
	T *cur;      // Next to return.
public:
	void reset() {
		cur = first;
		stop = nullptr;
	}
	T_Object_iterator(T_Object_list<T> &objects)
		: T_Safe_object_iterator<T>(objects), first(objects.get_first()) {
		reset();
	}
	T *get_next() {
		if (cur == stop)
			return nullptr;
		T *ret = cur;
		cur = cur->next.get();
		stop = first;
		return ret;
	}
};

using Object_iterator = T_Object_iterator<Game_object>;

/*
 *  Iterate through a chunk's nonflat objects.
 */
template<class T, class L>
class T_Nonflat_object_iterator : public T_Object_iterator<T> {
	T *nonflats;
public:
	void reset() {
		this->cur = nonflats;
		this->stop = nullptr;
	}
	T_Nonflat_object_iterator(L chunk)
		: T_Object_iterator<T>(chunk->get_objects()), nonflats(chunk->get_first_nonflat()) {
		reset();
	}
};

using Nonflat_object_iterator = T_Nonflat_object_iterator<Game_object, Map_chunk *>;

/*
 *  Iterate through a chunk's flat objects.
 */
template<class T, class L>
class T_Flat_object_iterator : public T_Safe_object_iterator<T> {
	T *first;
	T *stop;
	T *cur;      // Next to return.
	T *stop_at;
public:
	void reset() {
		cur = first;
		stop = nullptr;
	}
	T_Flat_object_iterator(L chunk)
		: T_Safe_object_iterator<T>(chunk->get_objects()) {
		first = chunk->get_objects().get_first() == chunk->get_first_nonflat() ? nullptr :
		        chunk->get_objects().get_first();
		stop_at = chunk->get_first_nonflat() ? chunk->get_first_nonflat()
		          : chunk->get_objects().get_first();
		reset();
	}
	T *get_next() {
		if (cur == stop)
			return nullptr;
		T *ret = cur;
		cur = cur->get_next();
		stop = stop_at;
		return ret;
	}
};

using Flat_object_iterator = T_Flat_object_iterator<Game_object, Map_chunk *>;

/*
 *  Iterate backwards through list of objects.
 */
template<class T, class L>
class T_Object_iterator_backwards : public T_Safe_object_iterator<T> {
	T *first;
	T *stop;
	T *cur;      // Return prev. to this.
public:
	void reset() {
		cur = first;
		stop = nullptr;
	}
	T_Object_iterator_backwards(L chunk)
		: T_Safe_object_iterator<T>(chunk->get_objects()),
		  first(chunk->get_objects().get_first()) {
		reset();
	}
	T_Object_iterator_backwards(T_Object_list<T> &objects)
		: T_Safe_object_iterator<T>(objects),
		  first(objects.get_first()) {
		reset();
	}
	T *get_next() {
		if (cur == stop)
			return nullptr;
		cur = cur->prev;
		stop = first;
		return cur;
	}
};

using Object_iterator_backwards = T_Object_iterator_backwards<Game_object, Map_chunk *>;

/*
 *  Iterate through a list of objects (recursively).
 */
template<class D> class D_Recursive_object_iterator {
	// Child we're going through, or nullptr.
	D_Recursive_object_iterator<D> *child;
	D elems;            // Goes through our elements.
public:
	D_Recursive_object_iterator(Object_list &objs)
		: child(nullptr), elems(objs)
	{  }
	// Start at given object.
	D_Recursive_object_iterator(Game_object *start);
	Game_object *get_next();    // Get next, going into containers.
};

/*
 *  Iterate forwards/backwards through a list of objects (recursively).
 */
using Recursive_object_iterator_backwards = D_Recursive_object_iterator<Object_iterator_backwards>;
using Recursive_object_iterator = D_Recursive_object_iterator<Object_iterator>;

#endif
