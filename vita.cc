#ifdef VITA
#include "vita.h"
#include <string.h>
#include <stdlib.h>
#include <SDL.h>
#include <psp2/system_param.h>
#include <psp2/apputil.h>
#include <iostream>
#include <string>
#include <Configuration.h>

using namespace std;


char *strdup(const char *s) {
	int len;
	int i1;
	char *z2;

	len=strlen(s);
	len++;

	char *dup = static_cast<char *>(malloc(static_cast<size_t>(len)));
	if(dup != NULL) {
		z2=dup;
		i1=0;
		while(i1 < static_cast<int>(strlen(s))) {
			*z2=s[i1];	
			z2++;
		}
		*z2=0x0;	
	}	
		
	return(dup);
}

int isascii (int c) {
        if(c >= 0 && c <= 127) 
                return(1);
        else
                return(0);
}

Uint32 joystick_callback (Uint32 interval, void *param) {
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = VITA_JOYSTICK_USER_EVENT;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    return(0);   
}

struct vita_mouse_pos getJoyMouseDiff(void) {
  struct vita_mouse_pos vmp;
  SDL_Joystick* js0;
  int joyx,joyy;
	int deadzone=VITA_JOYSTICK_DEAD_ZONE_DEFAULT;
	int joyspeed=VITA_JOYSTICK_SPEED_DEFAULT;


	config->value("config/vita/joystick/dead_zone", deadzone, VITA_JOYSTICK_DEAD_ZONE_DEFAULT);
	config->value("config/vita/joystick/speed", joyspeed, VITA_JOYSTICK_SPEED_DEFAULT);


  vmp.x=0;
  vmp.y=0;
  vmp.motion=false;

  js0 = SDL_JoystickOpen( 0 );
  joyx=SDL_JoystickGetAxis(js0, VITA_RIGHT_AXIS_X);
  joyy=SDL_JoystickGetAxis(js0, VITA_RIGHT_AXIS_Y);
  SDL_JoystickClose(js0);
  if(abs(joyx) > deadzone ) {
    vmp.x=(joyx / joyspeed);
  }
  if(abs(joyy) > deadzone ) {
    vmp.y=(joyy / joyspeed);
  }
  if(vmp.x !=0 || vmp.y != 0)
    vmp.motion=true;
  return(vmp);
}

bool isJoyMotion(void) {
  struct vita_mouse_pos vmp;
  vmp=getJoyMouseDiff();
  return(vmp.motion);
}

bool isJoyMouse1Pressed(void) {
  int pressed;
  SDL_Joystick* js0;
  js0 = SDL_JoystickOpen( 0 );
  pressed = SDL_JoystickGetButton(js0, VITA_BUTTON_TOP_L);
  SDL_JoystickClose(js0);
  return(pressed);
}

void setVitaDefaultConfig(void) {
  string str;
  int i1;
  config->value("config/video/force_bpp", str, "16");
  config->set("config/video/force_bpp", str, false);
  //config->value("config/video/fullscreen", str, "yes");
  //config->set("config/video/fullscreen",str,false);
  config->value("config/video/game/width", str, "320");
  config->set("config/video/game/width", str, false);
  config->value("config/video/game/height", str, "200");
  config->set("config/video/game/height", str, false);
  config->value("config/video/display/width", str, "960");  // was 1024
  config->set("config/video/display/width", str, false);
  config->value("config/video/display/height", str, "544"); // was 768
  config->set("config/video/display/height", str, false);
  config->value("config/video/scale", str, "1");
  config->set("config/video/scale", str, false);
  config->value("config/video/scale_method", str, "point");
  config->set("config/video/scale_method", str, false);
  config->value("config/video/fill_mode", str, "fill");
  config->set("config/video/fill_mode", str, false);
  config->value("config/video/fill_scaler", str, "Point");
  config->set("config/video/fill_scaler", str,false);
  config->value("config/video/highdpi", str, "yes");
  config->set("config/video/highdpi", str, false);
  config->value("config/video/share_video_settings", str, "yes");
  config->set("config/video/share_video_settings", str, false);
  config->value("config/video/fps", str, "10");
  config->set("config/video/fps", str, false);
  config->value("config/gameplay/facestats", str, "0");
  config->set("config/gameplay/facestats", str, false);
  config->value("config/gameplay/textbackground", str, "-1");
  config->set("config/gameplay/textbackground", str, false);
  config->value("config/gameplay/gumps_dont_pause_game", str, "no");
  config->set("config/gameplay/gumps_dont_pause_game", str, false);
  config->value("config/gameplay/double_click_closes_gumps", str, "yes");
  config->set("config/gameplay/double_click_closes_gumps", str, false);
  config->value("config/gameplay/smooth_scrolling", str, "0");
  config->set("config/gameplay/smooth_scrolling", str, false);
  config->value("config/gameplay/allow_autonotes", str, "yes");
  config->set("config/gameplay/allow_autonotes", str, false);
  config->value("config/gameplay/scroll_with_mouse", str, "yes");
  config->set("config/gameplay/scroll_with_mouse", str, false);
  config->value("config/gameplay/extended_intro", str, "no");
  config->set("config/gameplay/extended_intro", str, false);
  config->value("config/gameplay/bg_paperdolls", str, "no");
  config->set("config/gameplay/bg_paperdolls", str, false);
  config->value("config/gameplay/skip_intro", str, "yes");
  config->set("config/gameplay/skip_intro", str, false);
  config->value("config/gameplay/skip_splash", str, "yes");
  config->set("config/gameplay/skip_splash", str, false);
  config->value("config/gameplay/cheat", str, "no");
  config->set("config/gameplay/cheat", str, false);
  config->value("config/gameplay/mouse3rd", str, "no");
  config->set("config/gameplay/mouse3rd", str, false);
  config->value("config/audio/enabled", str, "yes");
  config->set("config/audio/enabled", str, false);
  config->value("config/audio/midi/enabled", str, "yes");
  config->set("config/audio/midi/enabled", str, false);
  config->value("config/audio/midi/convert", str, "gm");
  config->set("config/audio/midi/convert", str, false);
  config->value("config/audio/midi/use_oggs", str, "no");
  config->set("config/audio/midi/use_oggs", str, false);
  config->value("config/audio/midi/driver", str, "Timidity");
  config->set("config/audio/midi/driver", str, false);
  config->value("config/audio/midi/reverb/enabled", str, "no");
  config->set("config/audio/midi/reverb/enabled", str, false);
  config->value("config/audio/midi/chorus/enabled", str, "no");
  config->set("config/audio/midi/chorus/enabled", str, false);
  config->value("config/audio/midi/colume_curve", str, "1.0000");
  config->set("config/audio/midi/colume_curve", str, false);
  config->value("config/audio/midi/chorus/looping", str, "yes");
  config->set("config/audio/midi/chorus/looping", str, false);
  config->value("config/audio/sample_rate", str, "11025");
  config->set("config/audio/sample_rate", str, false);
  config->value("config/audio/stereo", str, "yes");
  config->set("config/audio/stereo", str, false);
  config->value("config/audio/effects/enabled", str, "yes");
  config->set("config/audio/effects/enabled", str, false);
  config->value("config/audio/speech/enabled", str, "no");
  config->set("config/audio/speech/enabled", str, false);
  config->value("config/touch/item_menu", str, "no");
  config->set("config/touch/item_menu", str, false);
  config->value("config/touch/dpad_location", str, "no");
  config->set("config/touch/dpad_location", str, false);
  config->value("config/touch/touch_pathfind", str, "yes");
  config->set("config/touch/touch_pathfind", str, false);
  config->value("config/shortcutbar/use_shortcutbar", str, "yes");
  config->set("config/shortcutbar/use_shortcutbar", str, false);
  config->value("config/shortcutbar/use_outline_color", str, "Black");
  config->set("config/shortcutbar/use_outline_color", str, false);
  config->value("config/shortcutbar/hide_missing_items", str, "yes");
  config->set("config/shortcutbar/hide_missing_items", str, false);

  config->value("config/vita/button/start", str, VITA_BUTTON_START_DEFAULT);
  config->set("config/vita/button/start", str, false);
  config->value("config/vita/button/select", str, VITA_BUTTON_SELECT_DEFAULT);
  config->set("config/vita/button/select", str, false);
  config->value("config/vita/button/square", str, VITA_BUTTON_SQUARE_DEFAULT);
  config->set("config/vita/button/square", str, false);
  config->value("config/vita/button/triangle", str, VITA_BUTTON_TRIANGLE_DEFAULT);
  config->set("config/vita/button/triangle", str, false);
	// these buttons are fixed!
  //config->value("config/vita/button/circle", str, VITA_BUTTON_CIRCLE_DEFAULT);
  //config->set("config/vita/button/circle", str, false);
  //config->value("config/vita/button/cross", str, VITA_BUTTON_CROSS_DEFAULT);
  //config->set("config/vita/button/cross", str, false);
  config->value("config/vita/button/left", str, VITA_BUTTON_LEFT_DEFAULT);
  config->set("config/vita/button/left", str, false);
  config->value("config/vita/button/right", str, VITA_BUTTON_RIGHT_DEFAULT);
  config->set("config/vita/button/right", str, false);
  config->value("config/vita/button/up", str, VITA_BUTTON_UP_DEFAULT);
  config->set("config/vita/button/up", str, false);
  config->value("config/vita/button/down", str, VITA_BUTTON_DOWN_DEFAULT);
  config->set("config/vita/button/down", str, false);
	// this button is fixed!
  //config->value("config/vita/button/top_l", str, VITA_BUTTON_TOP_L_DEFAULT);
  //config->set("config/vita/button/top_l", str, false);
  config->value("config/vita/button/top_r", str, VITA_BUTTON_TOP_R_DEFAULT);
  config->set("config/vita/button/top_r", str, false);
  config->value("config/vita/joystick/dead_zone", i1, VITA_JOYSTICK_DEAD_ZONE_DEFAULT);
  config->set("config/vita/joystick/dead_zone", i1, false);
  config->value("config/vita/joystick/speed", i1, VITA_JOYSTICK_SPEED_DEFAULT);
  config->set("config/vita/joystick/speed", i1, false);
}


bool joy2KeyMouse(SDL_Event const &ev, bool dokeys) {
  SDL_Keysym sym;
  SDL_KeyboardEvent ke;
  SDL_Event transmit;
  SDL_MouseButtonEvent mbe;

  SDL_zero(transmit);
  SDL_zero(ke);
  SDL_zero(sym);
  SDL_zero(mbe);

  sym.mod=KMOD_NONE;
  sym.unused=0;
  ke.type=SDL_KEYDOWN;
  ke.timestamp=ev.jbutton.timestamp;
  ke.windowID=0;
  ke.state=SDL_PRESSED;
  ke.repeat=0;
  transmit.type=SDL_KEYDOWN;
  mbe.timestamp=ev.jbutton.timestamp;

  int mousex=0;
  int mousey=0;

  // translate vita keys to keyboard kys
  string kkey="none";
  if(ev.type == SDL_JOYBUTTONDOWN || ev.type == SDL_JOYBUTTONUP) {
  	switch(ev.jbutton.button) {
   	  case VITA_BUTTON_START:
   	    config->value("config/vita/button/start", kkey, VITA_BUTTON_START_DEFAULT);
   	  break;
   	  case VITA_BUTTON_SELECT:
   	    config->value("config/vita/button/select", kkey, VITA_BUTTON_SELECT_DEFAULT);
   	  break;
   	  case VITA_BUTTON_SQUARE:
   	    config->value("config/vita/button/square", kkey, VITA_BUTTON_SQUARE_DEFAULT);
   	  break;
   	  case VITA_BUTTON_TRIANGLE:
   	    config->value("config/vita/button/triangle", kkey, VITA_BUTTON_TRIANGLE_DEFAULT);
   	  break;
   	  case VITA_BUTTON_CIRCLE:
   	    //config->value("config/vita/button/circle", kkey, VITA_BUTTON_CIRCLE_DEFAULT);
				// this is always Mouse Button 2
				kkey="esc";
   	  break;
   	  case VITA_BUTTON_CROSS:
   	    //config->value("config/vita/button/cross", kkey, VITA_BUTTON_CROSS_DEFAULT);
				// this is always Mouse Button 1
				kkey="mouse1";
   	  break;
   	  case VITA_BUTTON_LEFT:
   	    config->value("config/vita/button/left", kkey, VITA_BUTTON_LEFT_DEFAULT);
   	  break;
   	  case VITA_BUTTON_RIGHT:
   	    config->value("config/vita/button/right", kkey, VITA_BUTTON_RIGHT_DEFAULT);
   	  break;
   	  case VITA_BUTTON_UP:
   	    config->value("config/vita/button/up", kkey, VITA_BUTTON_UP_DEFAULT);
   	  break;
   	  case VITA_BUTTON_DOWN:
   	    config->value("config/vita/button/down", kkey, VITA_BUTTON_DOWN_DEFAULT);
   	  break;
   	  case VITA_BUTTON_TOP_L:
   	    //config->value("config/vita/button/top_l", kkey, VITA_BUTTON_TOP_L_DEFAULT);
				// this is always Mouse Button 1
				kkey="mouse1";
   	  break;
   	  case VITA_BUTTON_TOP_R:
   	    config->value("config/vita/button/top_r", kkey, VITA_BUTTON_TOP_R_DEFAULT);
   	  break;
  	}
	}

  if(ev.type == SDL_JOYBUTTONDOWN) {
     if(kkey ==  "esc") {
        sym.scancode=SDL_SCANCODE_ESCAPE;
        sym.sym=SDLK_ESCAPE;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "enter") {
        sym.scancode=SDL_SCANCODE_RETURN;
        sym.sym=SDLK_RETURN;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "space") {
        sym.scancode=SDL_SCANCODE_SPACE;
        sym.sym=SDLK_SPACE;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "b") {
        sym.scancode=SDL_SCANCODE_B;
        sym.sym=SDLK_b;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "c") {
        sym.scancode=SDL_SCANCODE_C;
        sym.sym=SDLK_c;
        ke.keysym=sym;
        transmit.key=ke;
     } else if(kkey == "f") {
        sym.scancode=SDL_SCANCODE_F;
        sym.sym=SDLK_f;
        ke.keysym=sym;
        transmit.key=ke;
     } else if(kkey == "g") {
        sym.scancode=SDL_SCANCODE_G;
        sym.sym=SDLK_g;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "i") {
        sym.scancode=SDL_SCANCODE_I;
        sym.sym=SDLK_i;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "j") {
        sym.scancode=SDL_SCANCODE_J;
        sym.sym=SDLK_j;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "k") {
        sym.scancode=SDL_SCANCODE_K;
        sym.sym=SDLK_k;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "l") {
        sym.scancode=SDL_SCANCODE_L;
        sym.sym=SDLK_l;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "m") {
        sym.scancode=SDL_SCANCODE_M;
        sym.sym=SDLK_m;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "n") {
        sym.scancode=SDL_SCANCODE_N;
        sym.sym=SDLK_n;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "o") {
        sym.scancode=SDL_SCANCODE_O;
        sym.sym=SDLK_o;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "p") {
        sym.scancode=SDL_SCANCODE_P;
        sym.sym=SDLK_p;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "r") {
        sym.scancode=SDL_SCANCODE_R;
        sym.sym=SDLK_r;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "s") {
        sym.scancode=SDL_SCANCODE_S;
        sym.sym=SDLK_s;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "t") {
        sym.scancode=SDL_SCANCODE_T;
        sym.sym=SDLK_t;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "w") {
        sym.scancode=SDL_SCANCODE_W;
        sym.sym=SDLK_w;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "z") {
        sym.scancode=SDL_SCANCODE_Z;
        sym.sym=SDLK_z;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "1") {
        sym.scancode=SDL_SCANCODE_1;
        sym.sym=SDLK_1;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "2") {
        sym.scancode=SDL_SCANCODE_2;
        sym.sym=SDLK_2;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "3") {
        sym.scancode=SDL_SCANCODE_3;
        sym.sym=SDLK_3;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "4") {
        sym.scancode=SDL_SCANCODE_4;
        sym.sym=SDLK_4;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "5") {
        sym.scancode=SDL_SCANCODE_5;
        sym.sym=SDLK_5;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "6") {
        sym.scancode=SDL_SCANCODE_6;
        sym.sym=SDLK_6;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "7") {
        sym.scancode=SDL_SCANCODE_7;
        sym.sym=SDLK_7;
        ke.keysym=sym;
        transmit.key=ke;
      } else if(kkey == "8") {
        sym.scancode=SDL_SCANCODE_8;
        sym.sym=SDLK_1;
        ke.keysym=sym;
        transmit.key=ke;
     } else if(kkey == "mouse1") {
       mbe.button=SDL_BUTTON_LEFT;
        mbe.state=SDL_PRESSED;
        mbe.clicks=1;
        SDL_GetMouseState(&mousex,&mousey);
        mbe.x=mousex;
        mbe.y=mousey;
        mbe.type=SDL_MOUSEBUTTONDOWN;
        transmit.button=mbe;
        transmit.type=SDL_MOUSEBUTTONDOWN;
        dokeys=1;   // always allowed
      } else if(kkey == "mouse2") {
        mbe.button=SDL_BUTTON_RIGHT;
        mbe.state=SDL_PRESSED;
        mbe.clicks=1;
        SDL_GetMouseState(&mousex,&mousey);
        mbe.x=mousex;
        mbe.y=mousey;
        mbe.type=SDL_MOUSEBUTTONDOWN;
        transmit.button=mbe;
        transmit.type=SDL_MOUSEBUTTONDOWN;
        dokeys=1;   // always allowed
      } else if(kkey == "mouse3") {
        mbe.button=SDL_BUTTON_MIDDLE;
        mbe.state=SDL_PRESSED;
        mbe.clicks=1;
        SDL_GetMouseState(&mousex,&mousey);
        mbe.x=mousex;
        mbe.y=mousey;
        mbe.type=SDL_MOUSEBUTTONDOWN;
        transmit.button=mbe;
        transmit.type=SDL_MOUSEBUTTONDOWN;
        dokeys=1;   // always allowed
      }
  } else if(ev.type == SDL_JOYBUTTONUP) {
      if(kkey == "mouse1") {
        mbe.button=SDL_BUTTON_LEFT;
        mbe.state=SDL_RELEASED;
        mbe.clicks=1;
        SDL_GetMouseState(&mousex,&mousey);
        mbe.x=mousex;
        mbe.y=mousey;
        mbe.type=SDL_MOUSEBUTTONUP;
        transmit.button=mbe;
        transmit.type=SDL_MOUSEBUTTONUP;
        dokeys=1;   // always allowed
      } else if(kkey == "mouse2") {
        mbe.button=SDL_BUTTON_RIGHT;
        mbe.state=SDL_RELEASED;
        mbe.clicks=1;
        SDL_GetMouseState(&mousex,&mousey);
        mbe.x=mousex;
        mbe.y=mousey;
        mbe.type=SDL_MOUSEBUTTONUP;
        transmit.button=mbe;
        transmit.type=SDL_MOUSEBUTTONUP;
        dokeys=1;   // always allowed
      } else if(kkey == "mouse3") {
        mbe.button=SDL_BUTTON_MIDDLE;
        mbe.state=SDL_RELEASED;
        mbe.clicks=1;
        SDL_GetMouseState(&mousex,&mousey);
        mbe.x=mousex;
        mbe.y=mousey;
        mbe.type=SDL_MOUSEBUTTONUP;
        transmit.button=mbe;
        transmit.type=SDL_MOUSEBUTTONUP;
        dokeys=1;   // always allowed
      }
  }

  if(dokeys == 1) {
    SDL_PushEvent(&transmit);
    return true;
  } else {
    SDL_zero(transmit);
    return false;
  }
}



#endif
