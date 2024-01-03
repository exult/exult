/**
 ** U7drag.h - Common defines for drag-and-drop of U7 shapes.
 **
 ** Written: 12/13/00 - JSF
 **/

/*
Copyright (C) 2000-2022 The Exult Team

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

#ifndef INCL_U7DRAG
#define INCL_U7DRAG 1

//	Generic Data Length is :
//	  Per number = 7 : 1 dot separator + 1 sign + 5 digits
//	  Prefix = 18 : 8 file:/// + 9 U7CHUNKID + 1 terminating null
#define U7DND_DATA_LENGTH(n) (18 + ((n) * 7))

//	Generic FILE_MIME, TEXT_MIME Targets
#if defined(MACOSX)
#	define U7_TARGET_DROPFILE_NAME_MIME    "public.file-url"
#	define U7_TARGET_DROPTEXT_NAME_GENERIC "UTF8_STRING"
#	define U7_TARGET_DROPTEXT_NAME_MIME    "public.utf8-plain-text"
#else
#	define U7_TARGET_DROPFILE_NAME_MIME    "text/uri-list"
#	define U7_TARGET_DROPTEXT_NAME_GENERIC "UTF8_STRING"
#	define U7_TARGET_DROPTEXT_NAME_MIME    "text/plain;charset=utf-8"
#endif    // MACOSX

//	Target information for dragging a shape:
#define U7_TARGET_SHAPEID_NAME "U7SHAPEID"
#define U7_TARGET_SHAPEID      137

//	Shape files:
#define U7_SHAPE_UNK      -1    // Unknown.
#define U7_SHAPE_SHAPES   0     // shapes.vga
#define U7_SHAPE_GUMPS    1     // gumps.vga
#define U7_SHAPE_FONTS    2     // fonts.vga
#define U7_SHAPE_FACES    3     // faces.vga
#define U7_SHAPE_SPRITES  4     // sprites.vga
#define U7_SHAPE_PAPERDOL 5     // paperdol.vga

//	Store/get shapeid.
int  Store_u7_shapeid(unsigned char* data, int file, int shape, int frame);
void Get_u7_shapeid(
		const unsigned char* data, int& file, int& shape, int& frame);
bool Is_u7_shapeid(const unsigned char* data);

//	Target information for dragging a chunk:
#define U7_TARGET_CHUNKID_NAME "U7CHUNKID"
#define U7_TARGET_CHUNKID      138

//	Store/get chunk #.
int  Store_u7_chunkid(unsigned char* data, int cnum);
void Get_u7_chunkid(const unsigned char* data, int& cnum);
bool Is_u7_chunkid(const unsigned char* data);

//	Target information for dragging an npc:
#define U7_TARGET_NPCID_NAME "U7NPCID"
#define U7_TARGET_NPCID      140

//	Store/get npc #.
int  Store_u7_npcid(unsigned char* data, int npcnum);
void Get_u7_npcid(const unsigned char* data, int& npcnum);
bool Is_u7_npcid(const unsigned char* data);

//	Target information for dragging a 'combo' (group of shapes):
#define U7_TARGET_COMBOID_NAME "U7COMBOID"
#define U7_TARGET_COMBOID      139

//	Store/get combo and its elements:
struct U7_combo_data {
	int tx, ty, tz, shape, frame;
};

int Store_u7_comboid(
		unsigned char* data, int xtiles, int ytiles, int tiles_right,
		int tiles_below, int cnt, U7_combo_data* ents);
void Get_u7_comboid(
		const unsigned char* data, int& xtiles, int& ytiles, int& tiles_right,
		int& tiles_below, int& cnt, U7_combo_data*& ents);
bool Is_u7_comboid(const unsigned char* data);

// Put these here since they are shared between XWin and Win32

using Move_shape_handler_fun = void (*)(
		int shape, int frame, int x, int y, int prevx, int prevy, bool show);
using Move_combo_handler_fun = void (*)(
		int xtiles, int ytiles, int tiles_right, int tiles_below, int x, int y,
		int prevx, int prevy, bool show);
using Drop_shape_handler_fun = void (*)(int shape, int frame, int x, int y);
using Drop_chunk_handler_fun = void (*)(int chunk, int x, int y);
using Drop_npc_handler_fun   = void (*)(int npc, int x, int y);
using Drop_combo_handler_fun
		= void (*)(int cnt, U7_combo_data* combo, int x, int y);

#endif
