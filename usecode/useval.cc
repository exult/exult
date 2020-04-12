/*
 *  useval.cc - Values used in Usecode interpreter.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
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

#include <fstream>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <limits>
#include "useval.h"
#include "utils.h"
#include "ucsymtbl.h"
#include "gamewin.h"
#include "ucmachine.h"
#include "databuf.h"
#include "ios_state.hpp"

using std::hex;
using std::ostream;
using std::setfill;
using std::setw;
using std::strcmp;
using std::strlen;
using std::string;
using std::vector;

/*
 *  Destructor
 *
 */

Usecode_value::~Usecode_value() {
	destroy();
}

/*
 *  Steals an array from another usecode value. If the other
 *  usecode value is not an array, it will be converted into
 *  one and then its data will be stolen.
 *
 *  Warning: is left undefined after this.
 */

void Usecode_value::steal_array(Usecode_value &v2) {
	destroy();
	undefined = false;
	type = array_type;
	if (v2.type == array_type) {
		// Swipe array.
		construct(arrayval, std::move(v2.arrayval));
	} else {
		construct(arrayval, 1, std::move(v2));
	}
}

/*
 *  Resize array (or turn single element into an array).  The new values
 *  are (integer) 0.
 *
 *  Output: Always true.
 */

int Usecode_value::resize(
    int new_size
) {
	if (type != array_type) { // Turn it into an array.
		Usecode_value elem(*this);
		*this = Usecode_value(new_size, &elem);
		return 1;
	}
	arrayval.resize(new_size);
	return 1;
}

/*
 *  Comparator.
 *
 *  Output: 1 if they match, else 0.
 */

bool Usecode_value::operator==(
    const Usecode_value &v2
) const {
	if (&v2 == this)
		return true;        // Same object.
	switch (type) {
	case int_type:
		switch (v2.type) {
		case int_type:
			return intval == v2.intval;
		case pointer_type:
			// Allow 0 == ptr
			return intval == 0 && v2.ptrval == nullptr;
		case array_type:
			// Okay if 0 == empty array.
			// Otherwise, compare first element of array versus value
			// (happens in blacksword usecode).
			return *this == v2.get_elem0();
		default:
			return false;
		}
	case pointer_type:
		switch (v2.type) {
		case int_type:
			// Allow ptr == 0
			return ptrval == nullptr && v2.intval == 0;
		case pointer_type:
			return ptrval == v2.ptrval;
		case array_type:
			// On pointer == array, compare pointer to first element of array.
			return v2.get_array_size() && *this == v2.get_elem(0);
		default:
			return false;
		}
	case array_type:
		switch (v2.type) {
		case int_type:
			// Allow empty array == 0
			// Otherwise, compare first element of array versus value
			// (makes it symmetric).
			return get_elem0() == v2;
		case pointer_type:
			// On array == pointer, compare first element of array to pointer.
			return get_array_size() && get_elem(0) == v2;
		case array_type: {
			// On array == array, arrays must be equal.
			return arrayval == v2.arrayval;
		}
		default:
			return false;
		}
	case string_type:
		return v2.type == string_type && strval == v2.strval;
	case class_sym_type:
		return v2.type == class_sym_type && clssym == v2.clssym;
	case class_obj_type:
		return v2.type == class_obj_type && ptrval == v2.ptrval;
	default:
		return false;
	}
}

/*
 *  Search an array for a given value.
 *
 *  Output: Index if found, else -1.
 */

int Usecode_value::find_elem(
    const Usecode_value &val
) {
	if (type != array_type)
		return -1;        // Not an array.
	for (size_t i = 0; i < arrayval.size(); i++)
		if (arrayval[i] == val)
			return i;
	return -1;
}

/*
 *  Concatenate another value onto this.
 *
 *  Output: This.
 */

Usecode_value &Usecode_value::concat(
    Usecode_value &val2     // Concat. val2 onto end.
) {
	if (type != array_type) {   // Not an array?  Create one.
		// Current value becomes 1st elem.
		Usecode_value tmp(1, this);
		*this = std::move(tmp);
	}
	if (val2.type != array_type) {  // Appending a single value?
		arrayval.push_back(val2);
	} else {            // Appending an array.
		arrayval.insert(arrayval.end(), val2.arrayval.cbegin(), val2.arrayval.cend());
	}
	return *this;
}

/*
 *  Append a list of integer values.
 */

void Usecode_value::append(
    int *vals,
    int cnt
) {
	assert(type == array_type);
	arrayval.insert(arrayval.end(), vals, vals + cnt);
}

/*
 *  Given an array and an index, and a 2nd value, add the new value at that
 *  index, or if the new value is an array itself, add its values.
 *
 *  Output: # elements added.
 */

int Usecode_value::add_values(
    int index,
    Usecode_value &val2
) {
	int size = get_array_size();
	if (!val2.is_array()) {     // Simple case?
		if (index >= size) {
			arrayval.push_back(val2);
		} else {
			arrayval[index] = val2;
		}
		return 1;
	}
	// Add each element.
	int size2 = val2.get_array_size();
	if (index + size2 > size) {
		arrayval.resize(index + size2);
	}
	std::copy(val2.arrayval.cbegin(), val2.arrayval.cend(), arrayval.begin() + index);
	return size2;         // Return # added.
}

/*
 *  Print in ASCII.
 */

void Usecode_value::print(
    ostream &out, bool shortformat
) const {
	auto print_array = [&out, shortformat](auto begin, auto count) {
		auto end = begin;
		if (shortformat && count > 2) {
			end += 2;
		} else {
			end += count;
		}
		out << "[ ";
		for (auto elem = begin; elem != end; ++elem) {
			if (elem != begin) {
				out << ", ";
			}
			elem->print(out);
		}
		if (shortformat && count > 2)
			out << ", ... (size " << count << ")";
		out << " ]";
	};
	boost::io::ios_flags_saver flags(out);
	boost::io::ios_fill_saver fill(out);
	switch (type) {
	case int_type:
		out << hex << setfill('0') << setw(4);
		out << (intval & 0xffff);
		break;
	case pointer_type:
		out << hex << setfill('0') << setw(8);
		out << reinterpret_cast<uintptr>(ptrval.get());
		break;
	case string_type:
		out << '"' << strval << '"';
		break;
	case array_type:
		print_array(arrayval.cbegin(), arrayval.size());
		break;
	case class_sym_type:
		// TODO: Implement this.
		out << "->";
		if (clssym)
			out << clssym->get_name();
		else
			out << "null obj?";
		break;
	case class_obj_type:
		print_array(clsrefval.elems, clsrefval.cnt);
		break;
	default:
		break;
	}
}

Usecode_value& Usecode_value::operator+=(const Usecode_value &v2) {
	Usecode_value &v1 = *this;

	if (v1.is_undefined()) {
		v1 = v2;
		return v1;
	} else if (v2.is_undefined()) {
		return v1;
	} else if (v1.get_type() == Usecode_value::int_type) {
		if (v2.get_type() == Usecode_value::int_type) {
			v1.intval += v2.intval;
		} else if (v2.get_type() == Usecode_value::string_type) {
			// Note: this actually seems wrong compared to the originals,
			// but I am leaving this the way it is unless it causes a bug.
			string ret(std::to_string(v1.intval));
			ret += v2.strval;
			v1 = std::move(ret);
		}
		return v1;
	} else if (v1.get_type() == Usecode_value::string_type) {
		if (v2.get_type() == Usecode_value::int_type) {
			v1.strval += std::to_string(v2.intval);
		} else if (v2.get_type() == Usecode_value::string_type) {
			v1.strval += v2.strval;
		}
		return v1;
	} else {
		v1 = Usecode_value(0);
	}
	return v1;
}

template <typename T, template <typename> class Op>
struct safe_divide : Op<T> {
	T operator()(const T &x, const T &y) const {
		// Watch for division by zero. Originals do this, but they return
		// 32768 in all cases.
		if (y == 0)
			return x < 0 ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
		else
			return Op<T>::operator()(x, y);
	}
};

template <typename Op>
Usecode_value& Usecode_value::operate(const Usecode_value &v2) {
	Op op;
	Usecode_value &v1 = *this;

	if (v1.is_undefined())
		// Seems correct.
		v1 = op(0, v2.need_int_value());
	else if (v2.is_undefined()) {
		// Just return zero
		v1 = Usecode_value(0);
	} else if (v1.get_type() == Usecode_value::int_type) {
		if (v2.get_type() == Usecode_value::int_type) {
			v1.intval = op(v1.intval, v2.intval);
		} else if (v2.get_type() == Usecode_value::string_type) {
			v1.intval = op(v1.intval, v2.need_int_value());
		}
	} else if (v1.get_type() == Usecode_value::string_type) {
		if (v2.get_type() == Usecode_value::string_type) {
			// Note: SI usecode seems to assume this in two cases: selling
			// leather stuff to Krayg in Monitor and party members when
			// you ask Boydon to leave. However, the engine behaves unlike
			// what the usecode expects.
			// I decided to go the way usecode expects, adding a space along
			// the way, to "fix" this as it seems to be the only places in
			// the originals where this matters.
			v1.strval += ' ';
			v1.strval += v2.strval;
		}
	} else {
		v1 = Usecode_value(0);
	}
	return v1;
}

Usecode_value& Usecode_value::operator-=(const Usecode_value &v2) {
	return operate<std::minus<long> >(v2);
}

Usecode_value& Usecode_value::operator*=(const Usecode_value &v2) {
	return operate<std::multiplies<long> >(v2);
}

Usecode_value& Usecode_value::operator/=(const Usecode_value &v2) {
	return operate<safe_divide<long, std::divides> >(v2);
}

Usecode_value& Usecode_value::operator%=(const Usecode_value &v2) {
	return operate<safe_divide<long, std::modulus> >(v2);
}

/*
 *  Serialize out.
 *
 *  Output: # bytes stored, or -1 if error.
 */

bool Usecode_value::save(
    ODataSource *out
) {
	out->write1(static_cast<int>(type));
	switch (type) {
	case int_type:
		out->write4(intval);
		break;
	case pointer_type:
		// TODO: proper serialization.
		out->write4(0);
		break;
	case class_sym_type: {
		const char *classname = clssym->get_name();
		int len = std::strlen(classname);
		out->write2(len);
		out->write(classname, len);
		break;
	}
	case string_type: {
		out->write2(strval.size());
		out->write(strval);
		break;
	}
	case array_type:
		out->write2(arrayval.size()); // first length, then length Usecode_values
		for (auto& elem : arrayval) {
			if (!elem.save(out))
				return false;
		}
		break;
	case class_obj_type: {
		// TODO: This creates many copies without need, as every class
		// instance points to the same array.
		// Need to serialize this properly.
		int len = clsrefval.cnt;
		out->write2(len); // first length, then length Usecode_values
		for (int i = 0; i < len; i++) {
			if (!clsrefval.elems[i].save(out))
				return false;
		}
		break;
	}
	default:
		return false;
	}
	return true;
}

/*
 *  Serialize in.  Assumes 'this' contains no data yet.
 *
 *  Output: False if error.
 */

bool Usecode_value::restore(
    IDataSource *in
) {
	undefined = false;
	type = static_cast<Val_type>(in->read1());
	switch (type) {
	case int_type:
		intval = in->read4();
		return true;
	case pointer_type:
		// TODO: proper deserialization.
		in->read4();
		construct(ptrval); //DON'T dereference this pointer!
		// Maybe add a new type "serialized_pointer" to prevent "accidents"?
		return true;
	case class_sym_type: {
		int len = in->read2();
		char *nm = new char[len + 1];
		in->read(nm, len);
		nm[len] = 0;
		Game_window *gwin = Game_window::get_instance();
		clssym = gwin->get_usecode()->get_class(nm);
		return true;
	}
	case string_type: {
		int len = in->read2();
		construct(strval);
		in->read(strval, len);
		return true;
	}
	case array_type:
		construct(arrayval, Usecode_vector(in->read2()));
		for (size_t i = 0; i < arrayval.size(); i++) {
			if (!arrayval[i].restore(in)) {
				return false;
			}
		}
		return true;
	case class_obj_type: {
		// TODO: Because every copy of an instance was independently
		// serialized, every copy will also be independently deserialized.
		// This will duplicate the instance variables, and they will no
		// longer point to the same instance.
		// Need to deserialize this properly.
		int len = in->read2();
		clsrefval.cnt = len;  // Stores class, class vars.
		clsrefval.elems = new Usecode_value[clsrefval.cnt];
		for (int i = 0; i < len; i++)
			if (!clsrefval.elems[i].restore(in))
				return false;
		return true;
	}
	default:
		return false;
	}
}


ostream &operator<<(ostream &out, Usecode_value &val) {
	val.print(out, true);
	return out;
}

/*
 *  Create/delete class objects.
 */
void Usecode_value::class_new(Usecode_class_symbol *cls, int nvars) {
	assert(type == int_type);
	type = class_obj_type;
	clsrefval.cnt = nvars + 1;    // Stores class, class vars.
	clsrefval.elems = new Usecode_value[clsrefval.cnt];
	clsrefval.elems[0] = Usecode_value(cls);
}

void Usecode_value::class_delete() {
	assert(type == class_obj_type);
	delete [] clsrefval.elems;
	*this = Usecode_value(0);
}
