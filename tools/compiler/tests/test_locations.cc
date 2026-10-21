// Standalone regression checks for diagnostic context and filename lifetimes.
#include "ucloc.h"

#include <cassert>
#include <stdexcept>
#include <string>

static void check_nested_contexts(int depth, bool unwind) {
	const Uc_location before;
	{
		Uc_location::Context context(Uc_location("semantic.uc", depth, 3, 2));
		const Uc_location    current;
		assert(current.get_line() == depth);
		assert(current.get_column() == 3);
		assert(current.get_length() == 2);
		if (depth) {
			check_nested_contexts(depth - 1, unwind);
		} else if (unwind) {
			throw std::runtime_error("unwind contexts");
		}
		assert(Uc_location().get_line() == depth);
	}
	const Uc_location after;
	assert(after.get_source() == before.get_source());
	assert(after.get_line() == before.get_line());
}

int main() {
	Uc_location::set_cur("physical.uc", 17, 5);
	const Uc_location physical;
	check_nested_contexts(256, false);    // Grow and relocate the context vector.
	try {
		check_nested_contexts(512, true);
		assert(false);
	} catch (const std::runtime_error&) {
		assert(Uc_location().get_source() == physical.get_source());
		assert(Uc_location().get_line() == 17);
		assert(Uc_location().get_column() == 5);
	}
	const std::string long_name(100000, 'x');
	Uc_location::set_cur(long_name.c_str(), 1);
	const Uc_location first;
	for (int i = 0; i < 4096; ++i) {
		Uc_location::set_cur(("source-" + std::to_string(i) + ".uc").c_str(), i);
	}
	assert(first.get_source() == long_name);
	assert(std::string(physical.get_source()) == "physical.uc");
	Uc_location::set_cur(long_name.c_str(), 2);
	assert(Uc_location().get_source() == first.get_source());    // Interned once.
}
