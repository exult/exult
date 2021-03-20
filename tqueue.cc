/*
 *  tqueue.cc - A queue of time-based events for animation.
 *
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "tqueue.h"
#include <algorithm>
#include <SDL_timer.h>

/*
 *  Remove all entries.
 */

void Time_queue::clear(
) {
	for (auto& ent : data) {
		ent.handler->dequeue();
	}
	data.clear();
}

/*
 *  Add an entry to the queue.
 */

void Time_queue::add(
    uint32 t,       // When entry is to be activated.
    Time_sensitive *obj,        // Object to be added.
    uintptr ud             // User data.
) {
	obj->queue_cnt++;       // It's going in, no matter what.
	Queue_entry newent;
	if (paused && !obj->always) // Paused?
		// Messy, but we need to fix time.
		t -= SDL_GetTicks() - pause_time;
	newent.set(t, obj, ud);
	auto insertionPoint = std::lower_bound(data.begin(), data.end(), newent);
	data.insert(insertionPoint, newent);
}

bool    operator <(const Queue_entry &q1, const Queue_entry &q2) {
	return q1.time < q2.time;
}

/*
 *  Remove first entry containing a given object.
 *
 *  Output: 1 if found, else 0.
 */

int Time_queue::remove(
    Time_sensitive *obj
) {
	auto toRemove = std::find_if(data.begin(), data.end(),
			[obj](const auto& el) { return el.handler == obj; });
	if (toRemove == data.end()) {
		return 0;
	} else {
		(*toRemove).handler->queue_cnt--;
		data.erase(toRemove);
		return 1;
	}
	return 0;         // Not found.
}

/*
 *  Remove first entry containing a given object and data.
 *
 *  Output: 1 if found, else 0.
 */

int Time_queue::remove(
    Time_sensitive *obj,
    uintptr udata
) {
	if (data.empty())
		return 0;
	for (auto it = data.begin();
	        it != data.end(); ++it) {
		if (it->handler == obj && it->udata == udata) {
			obj->queue_cnt--;
			data.erase(it);
			return 1;
		}
	}
	return 0;         // Not found.
}

/*
 *  See if a given entry is in the queue.
 *
 *  Output: 1 if found, else 0.
 */

int Time_queue::find(
    Time_sensitive const *obj
) const {
	return std::find_if(data.begin(), data.end(),
			[&](const auto& el) {return el.handler==obj;})
		!= data.end();
}

/*
 *  Find when an entry is due.
 *
 *  Output: delay in msecs. when due, or -1 if not in queue.
 */

long Time_queue::find_delay(
    Time_sensitive const *obj,
    uint32 curtime
) const {
	auto found = std::find_if(data.begin(), data.end(),
		[&](const auto& el){return obj == el.handler;});
	if (found == data.end()) {
		return -1;
	}
	if (pause_time) { // Watch for case when paused.
		curtime = pause_time;
	}
	long delay = (*found).time - curtime;
	return delay >= 0 ? delay : 0;
}

/*
 *  Remove & activate entries that are due, starting with head (already
 *  known to be due).
 */

void Time_queue::activate0(
    uint32 curtime      // Current time.
) {
	Queue_entry ent;
	do {
		ent = data.front();
		Time_sensitive *obj = ent.handler;
		uintptr udata = ent.udata;
		data.pop_front();   // Remove from chain.
		obj->queue_cnt--;
		obj->handle_event(curtime, udata);
	} while (!data.empty() && !(curtime < data.front().time));
}

/*
 *  Remove & activate entries marked 'always'.  This is called when
 *  the queue is paused.
 */

void Time_queue::activate_always(
    uint32 curtime      // Current time.
) {
	if (data.empty())
		return;
	auto newStart = std::stable_partition(data.begin(), data.end(),
		[&] (const auto& el) { return !(curtime < el.time); });

	std::for_each(data.begin(), newStart, [curtime](auto& ent) {
		Time_sensitive *obj = ent.handler;
		if (obj->always) {
			obj->queue_cnt--;
			uintptr udata = ent.udata;
			obj->handle_event(curtime, udata);
		}
	});
	data.erase(data.begin(), newStart);
}

/*
 *  Resume after a pause.
 */

void Time_queue::resume(
    uint32 curtime
) {
	if (!paused || --paused > 0)    // Only unpause when stack empty.
		return;         // Not paused.
	int diff = curtime - pause_time;
	pause_time = 0;
	if (diff < 0)           // Should not happen.
		return;
	for (auto& it : data) {
		if (!it.handler->always)
			it.time += diff;   // Push entries ahead.
	}
}

/*
 *  Get next element in queue.
 */

int Time_queue_iterator::operator()(
    Time_sensitive  *&obj,      // Main object.
    uintptr &data          // Data that was added with it.
) {
	auto remain = iter;
	iter = this_obj == nullptr
		? remain
		: std::find_if(iter, tqueue->data.end(), [&](const auto& el) {
			return el.handler == this_obj;
		}
	);
	if (iter == tqueue->data.end())
		return 0;
	obj = (*iter).handler;      // Return fields.
	data = (*iter).udata;
	++iter;             // On to the next.
	return 1;
}

