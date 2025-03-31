/*
 *  tqueue.cc - A queue of time-based events for animation.
 *
 *  Copyright (C) 2000-2022  The Exult Team
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
#	include <config.h>
#endif

#include "tqueue.h"

#include "actors.h"
#include "cheat.h"
#include "gamewin.h"

#include <algorithm>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

/*
 *  Remove all entries.
 */

void Time_queue::clear() {
	for (auto& ent : data) {
		ent.handler->dequeue();
	}
	data.clear();
}

/*
 *  Add an entry to the queue.
 */

void Time_queue::add(
		uint32 t, std::shared_ptr<Time_sensitive> obj, uintptr ud) {
	obj->queue_cnt++;    // It's going in, no matter what.
	Queue_entry newent;
	if (paused && !obj->always) {    // Paused?
		// Messy, but we need to fix time.
		t -= SDL_GetTicks() - pause_time;
	}
	newent.set(t, nullptr, ud, obj);
	auto insertionPoint = std::upper_bound(data.begin(), data.end(), newent);
	data.insert(insertionPoint, newent);
}

void Time_queue::add(
		uint32          t,      // When entry is to be activated.
		Time_sensitive* obj,    // Object to be added.
		uintptr         ud      // User data.
) {
	obj->queue_cnt++;    // It's going in, no matter what.
	Queue_entry newent;
	if (paused && !obj->always) {    // Paused?
		// Messy, but we need to fix time.
		t -= SDL_GetTicks() - pause_time;
	}
	newent.set(t, obj, ud, nullptr);
	auto insertionPoint = std::upper_bound(data.begin(), data.end(), newent);
	data.insert(insertionPoint, newent);
}

bool operator<(const Queue_entry& q1, const Queue_entry& q2) {
	return q1.time < q2.time;
}

/*
 *  Remove first entry containing a given object.
 *
 *  Output: 1 if found, else 0.
 */

bool Time_queue::remove(Time_sensitive* obj) {
	auto toRemove
			= std::find_if(data.begin(), data.end(), [obj](const auto& el) {
				  return el.handler == obj || el.sp_handler.get() == obj;
			  });
	auto found = toRemove != data.end();
	if (found) {
		toRemove->handler->queue_cnt--;
		data.erase(toRemove);
	}
	return found;
}

bool Time_queue::remove(std::shared_ptr<Time_sensitive> obj) {
	auto toRemove
			= std::find_if(data.begin(), data.end(), [obj](const auto& el) {
				  return el.handler == obj.get() || el.sp_handler == obj;
			  });
	auto found = toRemove != data.end();
	if (found) {
		toRemove->handler->queue_cnt--;
		data.erase(toRemove);
	}
	return found;
}

/*
 *  Remove first entry containing a given object and data.
 *
 *  Output: 1 if found, else 0.
 */

bool Time_queue::remove(Time_sensitive* obj, uintptr udata) {
	auto       it = std::find_if(data.begin(), data.end(), [&](const auto& el) {
        return el.handler == obj && el.udata == udata;
    });
	const bool found = it != data.end();
	if (found) {
		obj->queue_cnt--;
		data.erase(it);
	}
	return found;
}

bool Time_queue::remove(std::shared_ptr<Time_sensitive> obj, uintptr udata) {
	auto       it = std::find_if(data.begin(), data.end(), [&](const auto& el) {
        return (el.handler == obj.get() || el.sp_handler == obj)
               && el.udata == udata;
    });
	const bool found = it != data.end();
	if (found) {
		obj->queue_cnt--;
		data.erase(it);
	}
	return found;
}

/*
 *  See if a given entry is in the queue.
 *
 *  Output: true if found, else false.
 */

bool Time_queue::find(const Time_sensitive* obj) const {
	return std::find_if(
				   data.begin(), data.end(),
				   [&](const auto& el) {
					   return el.handler == obj || el.sp_handler.get() == obj;
				   })
		   != data.end();
}

/*
 *  Find when an entry is due.
 *
 *  Output: delay in msecs. when due, or -1 if not in queue.
 */

long Time_queue::find_delay(const Time_sensitive* obj, uint32 curtime) const {
	auto found = std::find_if(data.begin(), data.end(), [&](const auto& el) {
		return obj == el.handler || obj == el.sp_handler.get();
	});
	if (found == data.end()) {
		return -1;
	}
	if (pause_time) {    // Watch for case when paused.
		curtime = pause_time;
	}
	const long delay = found->time - curtime;
	return delay >= 0 ? delay : 0;
}

void Time_queue::activate(uint32 curtime) {
	if (cheat.in_map_editor()) {
		activate_mapedit(curtime);
	} else if (paused > 0) {
		activate_always(curtime);
	} else if (!data.empty() && !(curtime < data.front().time)) {
		activate0(curtime);
	}
}

/*
 *  Remove & activate entries that are due, starting with head (already
 *  known to be due).
 */

void Time_queue::activate0(uint32 curtime    // Current time.
) {
	do {
		Queue_entry                     ent    = data.front();
		Time_sensitive*                 obj    = ent.handler;
		std::shared_ptr<Time_sensitive> sp_obj = ent.sp_handler;
		const uintptr                   udata  = ent.udata;
		data.pop_front();    // Remove from chain.

		if (!obj && sp_obj) {
			obj = sp_obj.get();
		}
		if (obj) {
			obj->queue_cnt--;
			obj->handle_event(curtime, udata);
		}

	} while (!data.empty() && !(curtime < data.front().time));
}

/*
 *  Remove & activate entries marked 'always'.  This is called when
 *  the queue is paused.
 */

void Time_queue::activate_always(uint32 curtime    // Current time.
) {
	if (data.empty()) {
		return;
	}
	for (auto it = data.begin(); it != data.end() && !(curtime < it->time);) {
		auto next = it;
		++next;    // Get ->next in case we erase.
		Queue_entry                     ent    = *it;
		std::shared_ptr<Time_sensitive> sp_obj = ent.sp_handler;
		Time_sensitive*                 obj    = ent.handler;
		if (obj == nullptr && sp_obj) {
			obj = sp_obj.get();
		}
		if (obj != nullptr && obj->always) {
			obj->queue_cnt--;
			const uintptr udata = ent.udata;
			data.erase(it);
			obj->handle_event(curtime, udata);
		}
		it = next;
	}
}

/*
 *  Remove & activate only the avatar.  This is called when
 *  map edit mode is enabled.
 */

void Time_queue::activate_mapedit(uint32 curtime    // Current time.
) {
	if (data.empty()) {
		return;
	}

	const Main_actor* const avatar
			= Game_window::get_instance()->get_main_actor();
	for (auto it = data.begin(); it != data.end() && !(curtime < it->time);) {
		auto next = it;
		++next;    // Get ->next in case we erase.
		Queue_entry                     ent    = *it;
		std::shared_ptr<Time_sensitive> sp_obj = ent.sp_handler;
		Time_sensitive*                 obj    = ent.handler;
		if (obj == nullptr && sp_obj) {
			obj = sp_obj.get();
		}
		if (obj != nullptr && (obj == avatar || obj->always)) {
			obj->queue_cnt--;
			const uintptr udata = ent.udata;
			data.erase(it);
			obj->handle_event(curtime, udata);
		}
		it = next;
	}
}

/*
 *  Resume after a pause.
 */

void Time_queue::resume(uint32 curtime) {
	if (paused == 0 || --paused > 0) {    // Only unpause when stack empty.
		return;                           // Not paused.
	}
	const int diff = curtime - pause_time;
	pause_time     = 0;
	if (diff < 0) {    // Should not happen.
		return;
	}
	for (auto& it : data) {
		if (!it.handler->always) {
			it.time += diff;    // Push entries ahead.
		}
	}
}

/*
 *  Get next element in queue.
 */

bool Time_queue_iterator::operator()(
		Time_sensitive*& obj,    // Main object.
		uintptr&         data    // Data that was added with it.
) {
	auto remain = iter;
	iter        = this_obj == nullptr
						  ? remain
						  : std::find_if(
                             iter, tqueue->data.end(), [&](const auto& el) {
                                 return el.handler == this_obj
                                        || el.sp_handler.get() == this_obj;
                             });
	if (iter == tqueue->data.end()) {
		return false;
	}
	obj  = iter->handler;    // Return fields.
	data = iter->udata;
	++iter;    // On to the next.
	return true;
}
