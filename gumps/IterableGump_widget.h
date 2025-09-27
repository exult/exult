/*
Copyright (C) 2025 The Exult Team

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

#ifndef ITERABLEGUMP_WIDGET_H
#define ITERABLEGUMP_WIDGET_H

#include "Gump_widget.h"

template <typename T_iterable>
class IterableGump_widget : public Gump_widget {
public:
	using iterator = typename T_iterable::iterator;

	IterableGump_widget(
			Gump_Base* parent, int shnum, int px, int py, int frnum = 0,
			ShapeFile shfile = SF_GUMPS_VGA)
			: Gump_widget(parent, shnum, px, py, frnum, shfile) {}

	Gump_widget* Input_first() override {
		Gump_widget* found = Gump_widget::Input_first();
		if (sort_order == Sort_Order::hidden) {
			return nullptr;
		}
		if (!found) {
			for (auto& child : *this) {
				if (!child) {
					continue;
				}
				found = child->Input_first();
				if (found) {
					break;
				}
			}
		}

		return found;
	}

	virtual iterator begin() = 0;
	virtual iterator end()   = 0;

	void paintSorted(
			Sort_Order sort, Sort_Order& next, Sort_Order& highest) override {
		Gump_widget::paintSorted(sort, next, highest);

		if (sort_order == Sort_Order::hidden) {
			return;
		}
		for (auto& child : *this) {
			child->paintSorted(sort, next, highest);
		}
	}

	bool run() override {
		bool result = Gump_widget::run();
		for (auto& child : *this) {
			result |= child->run();
		}
		return result;
	}

	Gump_widget* findSorted(
			int sx, int sy, Sort_Order sort, Sort_Order& next,
			Sort_Order& highest, Gump_widget* before) override {
		if (this == before) {
			return nullptr;
		}
		auto result
				= Gump_widget::findSorted(sx, sy, sort, next, highest, before);

		for (auto& child : *this) {
			if (!child) {
				continue;
			}	
			if (&*child == before) {
				break;
			}
			auto found = child->findSorted(sx, sy, sort, next, highest, before);
			if (found) {
				result = found;
			}
		}
		return result;
	}
};

#endif
