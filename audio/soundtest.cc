/*
 *  Copyright (C) 2000-2022 The Exult Team
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

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#endif    // __GNUC__
#include <SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "gamewin.h"
#include "Audio.h"
#include "soundtest.h"
#include "Scroll_gump.h"
#include "mouse.h"
#include "exult.h"
#include "font.h"
#include "tqueue.h"

void SoundTester::test_sound()
{
	Game_window *gwin = Game_window::get_instance();
	Image_buffer8 *ibuf = gwin->get_win()->get_ib8();
	Font *font = Shape_manager::get_instance()->get_font(4);

	Audio *audio = Audio::get_ptr();

	char buf[256];
	bool looping = true;
	bool redraw = true;
	SDL_Event event;

	int centerx = gwin->get_width()/2;
	int centery = gwin->get_height()/2;
	int left = centerx - 65;
	int first_line = centery-53;
	int line;
	int height = 6;
	int width = 6;
	
	Mouse::mouse->hide();

	gwin->get_tqueue()->pause(SDL_GetTicks());

	do
	{
		if (redraw)
		{
			Scroll_gump scroll;
			scroll.add_text(" ~");
			scroll.paint();

			line = first_line;
			font->paint_text_fixedwidth(ibuf, "Sound Tester", left, line, width);
			line += height;
			font->paint_text_fixedwidth(ibuf, "------------", left, line, width);

			line += height*2;
			font->paint_text_fixedwidth(ibuf, "   Up - Previous Type", left, line, width);
			
			line += height;
			font->paint_text_fixedwidth(ibuf, " Down - Next Type", left, line, width);
			
			line += height;
			font->paint_text_fixedwidth(ibuf, " Left - Previous Number", left, line, width);
			
			line += height;
			font->paint_text_fixedwidth(ibuf, "Right - Next Number", left, line, width);

			line += height;
			font->paint_text_fixedwidth(ibuf, "Enter - Play it", left, line, width);

			line += height;
			font->paint_text_fixedwidth(ibuf, "  ESC - Leave", left, line, width);

			line += height;
			font->paint_text_fixedwidth(ibuf, "    R - Repeat Music", left, line, width);

			line += height;
			font->paint_text_fixedwidth(ibuf, "    S - Stop Music", left, line, width);


			snprintf (buf, 256, "%2s Music %c %3i %c %s",
					active==0?"->":"",
					active==0?'<':' ',
					song,
					active==0?'>':' ',
					repeat?"- Repeat":"");
			line += height*2;
			font->paint_text_fixedwidth(ibuf, buf, left, line, width);
			
			
			snprintf (buf, 256, "%2s SFX   %c %3i %c",
					active==1?"->":"",
						active==1?'<':' ',
					sfx,
					active==1?'>':' ');

			line += height*2;
			font->paint_text_fixedwidth(ibuf, buf, left, line, width);


			snprintf (buf, 256, "%2s Voice %c %3i %c",
					active==2?"->":"",
					active==2?'<':' ',
					voice,
					active==2?'>':' ');

			line += height*2;
			font->paint_text_fixedwidth(ibuf, buf, left, line, width);

			gwin->show();
			redraw = false;
		}
		SDL_WaitEvent(&event);
		if(event.type==SDL_KEYDOWN)
		{
			redraw = true;
			switch(event.key.keysym.sym)
			{

			case SDLK_ESCAPE:
				looping = false;
				break;

			case SDLK_RETURN:
			case SDLK_KP_ENTER:
				if (active == 0)
				{
					audio->stop_music();
					audio->start_music (song, repeat);
				}
				else if (active == 1)
				{
					audio->play_sound_effect (sfx);
				}
				else if (active == 2)
				{
					audio->cancel_streams();
					audio->start_speech (voice, false);
				}
				break;
				
			case SDLK_r:
				repeat = !repeat;
				break;
			case SDLK_s:
				if ((event.key.keysym.mod & KMOD_ALT) && (event.key.keysym.mod & KMOD_CTRL)) 
					make_screenshot(true);
				else
					audio->stop_music();
				break;
			case SDLK_UP:
				active = (active + 2) % 3;
				break;
			case SDLK_DOWN:
				active = (active + 1) % 3;
				break;
			case SDLK_LEFT:
				if (active == 0)
				{
					song--;
					if (song < 0) song = 255;
				}
				else if (active == 1)
				{
					sfx--;
					if (sfx < 0) sfx = 255;
				}
				else if (active == 2)
				{
					voice--;
					if (voice < 0) voice = 255;
				}
				break;
			case SDLK_RIGHT:
				if (active == 0)
				{
					song++;
					if (song > 255) song = 0;
				}
				else if (active == 1)
				{
					sfx++;
					if (sfx > 255) sfx = 0;
				}
				else if (active == 2)
				{
					voice++;
					if (voice > 255) voice = 0;
				}
				break;
			default:
				break;
			}
		}
	} while(looping);

	gwin->get_tqueue()->resume(SDL_GetTicks());
	
	gwin->paint();
	Mouse::mouse->show();
	gwin->show();
}
