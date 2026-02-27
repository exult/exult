/*
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

#ifndef INCL_FRAMESEQ
#define INCL_FRAMESEQ 1

/*
 *  A sequence of frames.  Frame 0 is the resting state.
 */
class Frames_sequence {
	unsigned char* frames;
	int            num_frames;

public:
	Frames_sequence(int cnt, unsigned char* f);

	~Frames_sequence() {
		delete[] frames;
	}

	// Get resting frame.
	unsigned char get_resting() const {
		return frames[0];
	}

	// Get next frame.  Call
	//   with index = 0 for first one.
	unsigned char get_next(int& index) const {
		if (++index >= num_frames) {
			index = 1;
		}
		return frames[index];
	}

	void decrement(int& index) const {
		if (--index <= 0) {
			index = num_frames - 1;
		}
	}

	// Find frame, masking off rotation.
	// Rets. 0 if not found.
	int find_unrotated(unsigned char frame) {
		for (int i = num_frames - 1; i > 0; i--) {
			if (((frame ^ frames[i]) & 0xf) == 0) {
				return i;
			}
		}
		return 0;
	}
};
#endif
