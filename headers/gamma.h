/*

Gamma Table Class

Copyright (C) 2000  Ryan Nunn
Copyright (C) 2002-2022  The Exult Team

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

#ifndef INCL_GAMMA
#define INCL_GAMMA 1

#include <cmath>

template <class T>
class GammaTable {
private:
	unsigned size;
	double   sizef;
	T*       table;
	double   gamma;

public:
	inline const double& get_gamma() {
		return gamma;
	}

	inline void set_gamma(double g) {
		if (g < 0.001) {
			g = 0.001;
		}
		if (g == gamma) {
			return;
		}
		gamma = g;

		for (unsigned i = 0; i < size; i++) {
			table[i] = static_cast<T>(std::pow(i / sizef, 1 / gamma) * sizef);
		}
	}

	GammaTable() = delete;

	GammaTable(unsigned int s, float g = 1) : sizef(-1), gamma(-1) {
		sizef += size = s > 2 ? s : 2;
		table         = new T[size];
		set_gamma(g);
	}

	~GammaTable() {
		delete[] table;
	}

	inline const T& operator[](const T& i) const {
		return table[i];
	}
};

#endif    // INCL_GAMMA
