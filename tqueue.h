/*
 *  tqueue.h - A queue of time-based events for animation.
 *
 *  Copyright (C) 2000-2022 The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef TQUEUE_H
#define TQUEUE_H 1

#include "common_types.h"

#include <list>
#include <memory>

/*
 *  An interface for entries in the queue:
 */
class Time_sensitive {
	int  queue_cnt = 0;        // # of entries for this in queue.
	bool always    = false;    // Always do this, even if paused.
protected:
	virtual void dequeue() {
		queue_cnt--;
	}

public:
	friend class Time_queue;
	virtual ~Time_sensitive() = default;

	bool in_queue() const {
		return queue_cnt > 0;
	}

	void set_always(bool tf)    // Should be called before placing in
	//   queue.
	{
		always = tf;
	}

	virtual void handle_event(unsigned long curtime, uintptr udata) = 0;
};

class Time_queue;

/*
 *  A queue entry:
 */
struct Queue_entry {
	// Queue_entry *next, *prev;    // Next, prev. in queue.
	Time_sensitive* handler;    // Object to activate.
	std::shared_ptr<Time_sensitive>
			sp_handler;    // Shared pointer to object to activate
	uintptr udata;         // Data to pass to handler.
	uint32  time;          // Time when this is due.

	inline void set(
			uint32 t, Time_sensitive* h, uintptr ud,
			std::shared_ptr<Time_sensitive> sp) {
		time       = t;
		handler    = h;
		udata      = ud;
		sp_handler = sp;
	}
};

bool operator<(const Queue_entry& q1, const Queue_entry& q2);

/*
 *  Time-based queue.  The entries are kept sorted in increasing order
 *  by time.
 */
class Time_queue {
	using Temporal_sequence = std::list<Queue_entry>;
	Temporal_sequence data;
	uint32            pause_time = 0;    // Time when paused.
	int               paused     = 0;    // Count of calls to 'pause()'.

	// Activate head + any others due.
	void activate0(uint32 curtime);
	void activate_always(uint32 curtime);
	void activate_mapedit(uint32 curtime);

public:
	friend class Time_queue_iterator;
	void clear();    // Remove all entries.

	// Add an entry.
	void add(uint32 t, Time_sensitive* obj) {
		add(t, obj, static_cast<uintptr>(0));
	}

	void add(uint32 t, std::shared_ptr<Time_sensitive> obj) {
		add(t, obj, static_cast<uintptr>(0));
	}

	void add(uint32 t, Time_sensitive* obj, void* ud) {
		add(t, obj, reinterpret_cast<uintptr>(ud));
	}

	void add(uint32 t, std::shared_ptr<Time_sensitive> obj, void* ud) {
		add(t, obj, reinterpret_cast<uintptr>(ud));
	}

	void add(uint32 t, std::shared_ptr<Time_sensitive> obj, uintptr ud);
	void add(uint32 t, Time_sensitive* obj, uintptr ud);
	// Remove object's entry.
	bool remove(Time_sensitive* obj);
	bool remove(std::shared_ptr<Time_sensitive> obj);

	void remove(Time_sensitive* obj, void* ud) {
		remove(obj, reinterpret_cast<uintptr>(ud));
	}

	void remove(std::shared_ptr<Time_sensitive> obj, void* ud) {
		remove(obj, reinterpret_cast<uintptr>(ud));
	}

	bool remove(Time_sensitive* obj, uintptr udata);
	bool remove(std::shared_ptr<Time_sensitive> obj, uintptr udata);
	bool find(const Time_sensitive* obj) const;    // Find an entry.
	// Find delay when obj. is due.
	long find_delay(const Time_sensitive* obj, uint32 curtime) const;

	// Activate entries that are 'due'.
	void activate(uint32 curtime);

	void pause(uint32 curtime) {    // Game paused.
		if (!paused++) {
			pause_time = curtime;
		}
	}

	void resume(uint32 curtime);
};

class Time_queue_iterator {
	Time_queue::Temporal_sequence::iterator iter;
	Time_queue*                             tqueue;
	Time_sensitive* this_obj;    // Only return entries for this obj.
public:
	Time_queue_iterator(Time_queue* tq, Time_sensitive* obj)
			: iter(tq->data.begin()), tqueue(tq), this_obj(obj) {}

	bool operator()(Time_sensitive*& obj, uintptr& data);
};

#endif /* TQUEUE_H */
