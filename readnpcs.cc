/*
 *  readnpcs.cc - Read in NPC's from npc.dat & schedule.dat.  Also writes npc.dat back out.
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

#include <cstring>

#include "gamewin.h"
#include "game.h"
#include "monsters.h"
#include "ucmachine.h"
#include "utils.h"
#include "fnames.h"
#include "schedule.h"
#include "databuf.h"
#include "miscinf.h"
//#include "items.h"            /* Debugging only */

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::ofstream;
using std::vector;

/*
 *  Read in the NPC's, plus the monster info.
 */

void Game_window::read_npcs(
) {
	npcs.resize(1);         // Create main actor.
	Main_actor_shared ava = std::make_shared<Main_actor>("", 0);
	npcs[0] = ava;
	camera_actor = main_actor = ava.get();
	int num_npcs;
	bool fix_unused = false;    // Get set for old savegames.
	{
		IFileDataSource nfile(NPC_DAT);
		if (nfile.good()) {
			num_npcs1 = nfile.read2();  // Get counts.
			num_npcs = num_npcs1 + nfile.read2();
			main_actor->read(&nfile, 0, false, fix_unused);
		} else {
			if (!Game::is_editing())
				throw file_read_exception(NPC_DAT);
			num_npcs1 = num_npcs = 1;
			if (Game::get_avname())
				main_actor->set_npc_name(Game::get_avname());
			main_actor->set_shape(Shapeinfo_lookup::GetMaleAvShape());
			main_actor->set_invalid();  // Put in middle of world.
			main_actor->move(c_num_tiles / 2, c_num_tiles / 2, 0);
		}
		npcs.resize(num_npcs);
		bodies.resize(num_npcs);

		// Don't like it... no i don't.
		center_view(main_actor->get_tile());
		for (int i = 1; i < num_npcs; i++) { // Create the rest.
			npcs[i] = std::make_shared<Npc_actor>("", 0);
			Actor *npc = static_cast<Actor*>(npcs[i].get());
			npc->read(&nfile, i, i < num_npcs1, fix_unused);
			if (npc->is_unused()) {
				// Not part of the game.
				Game_object_shared keep;
				npc->remove_this(&keep);
				npc->set_schedule_type(Schedule::wait);
			} else
				npc->restore_schedule();
			CYCLE_RED_PLASMA();
		}
	}
	main_actor->set_actor_shape();
	{
		IFileDataSource nfile(MONSNPCS); // Monsters.
		if (nfile.good()) {
			// (Won't exist the first time; in this case U7open throws
			int cnt = nfile.read2();
			nfile.skip(1);// Read 1 ahead to test.
			bool okay = nfile.good();
			nfile.skip(-1);
			while (okay && cnt--) {
				// Read ahead to get shape.
				nfile.skip(2);
				unsigned short shnum = nfile.read2() & 0x3ff;
				okay = nfile.good();
				nfile.skip(-4);
				ShapeID sid(shnum, 0);
				if (!okay || sid.get_num_frames() < 16)
					break;  // Watch for corrupted file.
				Game_object_shared new_monster = Monster_actor::create(shnum);
				Monster_actor *act = static_cast<Monster_actor*>(new_monster.get());
				act->read(&nfile, -1, false, fix_unused);
				act->set_schedule_loc(act->get_tile());
				act->restore_schedule();
				CYCLE_RED_PLASMA();
			}
		} else {
#ifdef DEBUG
			cerr << "Error reading saved monsters.  Clearing list." << endl;
#endif
			Monster_actor::give_up();
		}
	}
	if (moving_barge) {     // Gather all NPC's on barge.
		Barge_object *b = moving_barge;
		moving_barge = nullptr;
		set_moving_barge(b);
	}
	read_schedules();       // Now get their schedules.
	center_view(main_actor->get_tile());
}

/*
 *  Write NPC (and monster) data back out.
 *
 *  Output: false if error, already reported.
 */

void Game_window::write_npcs(
) {
	int num_npcs = npcs.size();
	{
		OFileDataSource nfile(NPC_DAT);

		nfile.write2(num_npcs1);    // Start with counts.
		nfile.write2(num_npcs - num_npcs1);
		int i;
		std::cout << "NPC write " << std::endl;
		for (i = 0; i < num_npcs; i++)
			get_npc(i)->write(&nfile);
		nfile.flush();
		if (!nfile.good())
			throw file_write_exception(NPC_DAT);
	}
	write_schedules();      // Write schedules
	{
		// Now write out monsters in world.
		OFileDataSource nfile(MONSNPCS);
		int cnt = 0;
		nfile.write2(0);        // Write 0 as a place holder.
		for (Monster_actor *mact = Monster_actor::get_first_in_world();
				mact; mact = mact->get_next_in_world())
			if (!mact->is_dead()) { // Alive?
				mact->write(&nfile);
				cnt++;
			}
		nfile.seek(0);          // Back to start.
		nfile.write2(cnt);      // Write actual count.
		nfile.flush();
		if (!nfile.good())
			throw file_write_exception(MONSNPCS);
	}
}

/*
 *  Read in offsets.  When done, file is set to start of script names (if
 *  there are any).
 */

std::unique_ptr<short[]> Set_to_read_schedules(
    IStreamDataSource &sfile,
    int &num_npcs,          // # npc's returnes.
    int &entsize,           // Entry size returned.
    int &num_script_names      // # of usecode script names ret'd.
) {
	entsize = 4;            // 4 is U7's size.
	num_script_names = 0;
	num_npcs = sfile.read4();   // # of NPC's, not include Avatar.
	if (num_npcs == -1) {       // Exult format?
		entsize = 8;
		num_npcs = sfile.read4();
	} else if (num_npcs == -2) {
		entsize = 8;
		num_npcs = sfile.read4();
		num_script_names = sfile.read2();
	}
	auto offsets = std::make_unique<short[]>(num_npcs);
	int i;              // Read offsets with list of scheds.
	for (i = 0; i < num_npcs; i++)
		offsets[i] = sfile.read2();
	return offsets;
}

/*
 *  Read one NPC's schedule.
 */

void Read_a_schedule(
    IStreamDataSource &sfile,
    int index,
    Actor *npc,
    int entsize,
    const short *offsets
) {
	int cnt = offsets[index] - offsets[index - 1];
	// Read schedules into this array.
	Schedule_change *schedules = cnt ? new Schedule_change[cnt] : nullptr;
	unsigned char ent[10];
	if (entsize == 4) { // U7 format?
		for (int j = 0; j < cnt; j++) {
			sfile.read(reinterpret_cast<char *>(ent), 4);
			schedules[j].set4(ent);
		}
	} else {        // Exult formats.
		for (int j = 0; j < cnt; j++) {
			sfile.read(reinterpret_cast<char *>(ent), 8);
			schedules[j].set8(ent);
		}
	}
	if (npc)            // Store in NPC.
		npc->set_schedules(schedules, cnt);
	else
		delete [] schedules;
}

/*
 *  Read NPC schedules.
 */

void Game_window::read_schedules(
) {
	std::unique_ptr<IFileDataSource> sfile = std::make_unique<IFileDataSource>(GSCHEDULE);
	if (!sfile->good()) {
#ifdef DEBUG
		cerr << "Couldn't open " << GSCHEDULE << ". Falling back to "
		     << SCHEDULE_DAT << "." << endl;
#endif
		sfile = std::make_unique<IFileDataSource>(SCHEDULE_DAT);
		if (!sfile->good()) {
			if (!Game::is_editing())
				throw file_open_exception(get_system_path(SCHEDULE_DAT));
			else
				return;
		}
	}
	int num_npcs = 0;
	int entsize;
	int num_script_names;
	auto offsets = Set_to_read_schedules(*sfile, num_npcs, entsize, num_script_names);
	Schedule_change::clear();
	vector<std::string> &script_names = Schedule_change::get_script_names();
	if (num_script_names) {
		sfile->read2();   // Skip past total size.
		script_names.reserve(num_script_names);
		for (int i = 0; i < num_script_names; ++i) {
			int sz = sfile->read2();
			std::string nm;
			sfile->read(nm, sz);
			script_names.push_back(std::move(nm));
		}
	}

	for (int i = 0; i < num_npcs - 1; i++) { // Do each NPC, except Avatar.
		// Avatar isn't included here.
		Actor *npc = get_npc(i + 1);
		Read_a_schedule(*sfile, i + 1, npc, entsize, offsets.get());
		CYCLE_RED_PLASMA();
	}
	cout.flush();
}

/*
 *  Write NPC schedules.
 */

void Game_window::write_schedules() {
	Schedule_change *schedules;
	int cnt;
	short offset = 0;
	int i;
	int num;

	// So do I allow for all NPCs (type1 and type2) - Yes i will
	num = npcs.size();

	OFileDataSource sfile(GSCHEDULE);
	vector<std::string> &script_names = Schedule_change::get_script_names();

	sfile.write4(static_cast<unsigned int>(-2));        // Exult version #.
	sfile.write4(num);      // # of NPC's, not include Avatar.
	sfile.write2(script_names.size());
	sfile.write2(0);        // First offset

	for (i = 1; i < num; i++) { // write offsets with list of scheds.
		get_npc(i)->get_schedules(schedules, cnt);
		offset += cnt;
		sfile.write2(offset);
	}
	if (!script_names.empty()) {
		int total = 0;      // Figure total size.
		for (auto& elem : script_names)
			total += 2 + elem.size();
		sfile.write2(total);
		for (auto& elem : script_names) {
			sfile.write2(elem.size());
			sfile.write(elem);
		}
	}
	for (i = 1; i < num; i++) { // Do each NPC, except Avatar.
		get_npc(i)->get_schedules(schedules, cnt);
		for (int j = 0; j < cnt; j++) {
			unsigned char ent[20];
			schedules[j].write8(ent);
			sfile.write(reinterpret_cast<char *>(ent), 8);
		}
	}
}

void Game_window::revert_schedules(Actor *npc) {
	// Can't do this if <= 0
	if (npc->get_npc_num() <= 0) return;

	IFileDataSource sfile(SCHEDULE_DAT);
	if (!sfile.good()) {
		throw file_read_exception(SCHEDULE_DAT);
	}

	int num_npcs;
	int entsize;
	int num_script_names;
	auto offsets = Set_to_read_schedules(sfile, num_npcs, entsize, num_script_names);
	if (num_script_names) {
		int sz = sfile.read2();
		sfile.skip(sz);
	}
	// Seek to the right place
	sfile.skip(offsets[npc->get_npc_num() - 1]*entsize);

	Read_a_schedule(sfile, npc->get_npc_num(), npc, entsize, offsets.get());
}

