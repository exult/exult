/*
 * mklink -- Create "linkdep1" and "linkdep2" files from the
 *           "usecode" file.
 *
 * Copyright (c) 1999 Keldon Jones
 * Copyright (c) 2017 Marzo Sette Torres Junior
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include "utils.h"

using namespace std;

// Info about one usecode function
struct usecode_func {
	vector<uint16> called;	// List of called functions
	uint32 where;			// Location in usecode
	uint16 size;			// Size of function in bytes
	uint16 num_call;		// Number of called functions
	bool visited;			// Have we been visited
};

struct usecode_functions {
private:
	vector<usecode_func> functions;

public:
	usecode_functions() : functions(4096) {}
	uint16 get_total_size(vector<uint16>& call_tree) const;
	vector<uint16> get_tree(uint16 func_num);
	void clear_visited();
	void fix_tree(vector<uint16>& tree) const;
	usecode_func& get(size_t num) {
		return functions[num];
	}
	const usecode_func& get(size_t num) const {
		return functions[num];
	}
};

/*
 * Return the sum of the sizes of all functions in this tree.
 */
uint16 usecode_functions::get_total_size(vector<uint16>& call_tree) const {
	uint16 size = 0;

	// Add size of each function
	for (vector<uint16>::const_iterator it = call_tree.begin();
	     it != call_tree.end(); ++it) {
		size += functions[*it].size;
	}
	return size;
}

/*
 * Return a list of all functions in the call tree of the given function.
 *
 * This function is recursive.
 */
vector<uint16> usecode_functions::get_tree(uint16 func_num) {
	vector<uint16> our_tree;
	usecode_func& u_ptr = functions[func_num];

	// No need to return a tree
	if (u_ptr.visited) {
		return our_tree;
	}

	// Start array with ourselves by copying our number into tree
	our_tree.push_back(func_num);

	// We've visited this function
	u_ptr.visited = true;

	// Add elements from each called function
	for (int i = 0; i < u_ptr.num_call; i++) {
		// Get the sub-tree
		vector<uint16> sub_tree(get_tree(u_ptr.called[i]));
		if (sub_tree.empty()) {
			continue;
		}

		// Copy elements from sub-tree
		our_tree.reserve(our_tree.size() + sub_tree.size());
		our_tree.insert(our_tree.end(), sub_tree.begin(), sub_tree.end());
	}

	return our_tree;
}

/*
 * Clear the "visited" flag from each function.
 */
void usecode_functions::clear_visited() {
	// Clear all "visited" flags
	for (size_t i = 0; i < 4096; i++) {
		functions[i].visited = false;
	}
}

/*
 * Sort a function call tree and remove duplicates.
 */
void usecode_functions::fix_tree(vector<uint16>& tree) const {
	// Just call sort (I'm feeling lazy)
	sort(tree.begin(), tree.end());
}

/*
 * Process the "usecode" file and create the two index files.
 */
int main() {
	// Open the usecode file for reading
	ifstream usecode("usecode", ios::in | ios::binary);

	// Bail out in case of failure
	if (!usecode.good()) {
		cerr << "Could not open usecode!" << endl;
		exit(1);
	}

	// Read the position and number of first function
	uint32 func_pos = usecode.tellg();
	uint16 func_num = Read2(usecode);
	uint16 max_func = 0;
	usecode_functions functions;

	// Process the usecode file
	while (!usecode.eof()) {
		usecode_func& u_ptr = functions.get(func_num);
		u_ptr.where = func_pos;				// Remember start of function
		u_ptr.size = Read2(usecode);		// Read the function size
		uint16 data_size = Read2(usecode);	// Read the data size
		// Skip the data section, number of args and local vars
		usecode.ignore(data_size + 4);
		u_ptr.num_call = Read2(usecode);	// Read number of called functions
		// Read table
		u_ptr.called.reserve(u_ptr.num_call);
		for (size_t i = 0; i < u_ptr.num_call; i++) {
			u_ptr.called.push_back(Read2(usecode));
		}
		// Skip past end of function
		usecode.seekg(u_ptr.where + u_ptr.size + 4, ios::beg);
		// Track highest function number
		if (max_func < func_num) {
			max_func = func_num;
		}
		// Read the position and number of next function
		func_pos = usecode.tellg();
		func_num = Read2(usecode);
	}

	// Open the linkdep files for writing
	ofstream linkdep1("linkdep1", ios::out | ios::binary);
	ofstream linkdep2("linkdep2", ios::out | ios::binary);

	uint16 lnk2_written = 0;
	// Write data about each function
	for (size_t i = 0; i <= max_func; i++) {
		usecode_func& u_ptr = functions.get(i);
		if (!u_ptr.size) {
			// Write null entry for null function
			Write2(linkdep1, lnk2_written);
			Write2(linkdep1, 0xffffu);
			continue;
		}
		// Get function tree
		functions.clear_visited();
		vector<uint16> func_tree = functions.get_tree(i);
		functions.fix_tree(func_tree);
		// Get total size of function + all called functions
		uint16 total_size = functions.get_total_size(func_tree);
		// Write to linkdep1
		Write2(linkdep1, lnk2_written);
		Write2(linkdep1, total_size);
		// Write each pointer
		for (size_t j = 0; j < func_tree.size(); j++) {
			Write4(linkdep2, functions.get(func_tree[j]).where);
			lnk2_written++;
		}
	}

	// Write ending on linkdep1
	Write2(linkdep1, lnk2_written);
	Write2(linkdep1, 0);

	return 0;
}
