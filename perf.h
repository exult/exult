/**
**  perf.h - PerformanceTimer class to generate per frame Perfomance metrics
**
**/

/*
Copyright (C) 2026 The Exult Team

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA  02111-1307, USA.
*/
#ifndef PERF_H_INCLUDED
#define PERF_H_INCLUDED

#include "common_types.h"

#include <string>
#include <string_view>

class PerformanceTimer {
	uint64 start_time = 0;
	uint64 value      = 0;
	bool   used       = false;

	std::string              name = {};
	static PerformanceTimer* all;
	static PerformanceTimer* active;
	static int               mode;
	PerformanceTimer*        next = nullptr;
	PerformanceTimer*        prev = nullptr;

	// Private default constructor
	PerformanceTimer() {
		start_frame();
	}

	void RemoveFromList(PerformanceTimer*& head) {
		if (next) {
			next->prev = prev;
		}
		if (prev) {
			prev->next = next;
		}
		if (this == head) {
			head = next;
		}
		next = nullptr;
		prev = nullptr;
	}

	void AddToList(PerformanceTimer*& head) {
		if (this == head) {
			return;
		}
		next = head;
		head = this;
		prev = nullptr;
		if (next) {
			next->prev = this;
		}
	}

	static uint64 GetNS();

public:
	static void IncMode() {
		mode = (mode + 1) % 3;
	}

	void start_frame() {
		if (!mode) {
			return;
		}
		start_time = 0;
		value      = 0;
		used       = false;
	}

	void start_phase(bool swaplists = true) {
		if (!mode) {
			return;
		}
		start_time = GetNS();
		used       = true;

		if (swaplists) {
			RemoveFromList(all);
			AddToList(active);
		}
	}

	void end_phase() {
		if (!mode) {
			return;
		}
		auto delta = GetNS() - start_time;
		value += delta;

		RemoveFromList(active);
		AddToList(all);

		// remove our time from the timers that are active
		for (auto node = active; node; node = node->next) {
			node->start_time += delta;
		}
	}

	struct ScopedPerfTimer {
		PerformanceTimer& counter;

		ScopedPerfTimer(PerformanceTimer& counter) : counter(counter) {
			counter.start_phase();
		}

		~ScopedPerfTimer() {
			counter.end_phase();
		}
	};

	static PerformanceTimer& GetPerfTimer(std::string_view name, std::string_view name2 = {}, bool unique = false);

	static ScopedPerfTimer GetScopedPerfTimer(std::string_view name, std::string_view name2 = {}, bool unique = false) {
		return GetPerfTimer(name, name2, unique);
	}

	static void paintPerfMetrics();
};

#endif    // PERF_H_INCLUDED