#ifdef VITA

#include <SDL.h>
#include <Configuration.h>
using namespace std;

extern char *strdup(const char *s);
extern int isascii (int c);
Uint32 joystick_callback (Uint32 interval, void *param); 
extern struct vita_mouse_pos getJoyMouseDiff(void);
extern bool isJoyMotion(void);
extern bool isJoyMouse1Pressed(void);
extern bool joy2KeyMouse(SDL_Event const &ev, bool dokeys, Configuration const *config);
extern void setVitaDefaultConfig(Configuration *config);

struct vita_mouse_pos {
  int x;
  int y;
  bool motion;
};


#define VITA_BUTTON_START			11
#define VITA_BUTTON_SELECT 		10
#define VITA_BUTTON_SQUARE 		3
#define VITA_BUTTON_TRIANGLE 	0
#define VITA_BUTTON_CIRCLE		1
#define VITA_BUTTON_CROSS			2
#define VITA_BUTTON_TOP_L			4		// is JoyMouse1
#define VITA_BUTTON_TOP_R			5		// is JoyMouse3
#define VITA_BUTTON_UP				8
#define	VITA_BUTTON_DOWN			6
#define VITA_BUTTON_LEFT			7
#define VITA_BUTTON_RIGHT 		9

#define VITA_BUTTON_START_DEFAULT "esc"
#define VITA_BUTTON_SELECT_DEFAULT "n"
#define VITA_BUTTON_SQUARE_DEFAULT "i"
#define VITA_BUTTON_TRIANGLE_DEFAULT "t"
//#define VITA_BUTTON_CIRCLE_DEFAULT "esc"
//#define VITA_BUTTON_CROSS_DEFAULT "mouse1"
#define VITA_BUTTON_LEFT_DEFAULT "p"
#define VITA_BUTTON_RIGHT_DEFAULT "f"
#define VITA_BUTTON_UP_DEFAULT "b"
#define VITA_BUTTON_DOWN_DEFAULT "Z"
//#define VITA_BUTTON_TOP_L_DEFAULT "mouse1"
#define VITA_BUTTON_TOP_R_DEFAULT "space"


#define VITA_LEFT_AXIS_X			0
#define VITA_LEFT_AXIS_Y      1
#define VITA_RIGHT_AXIS_X      2
#define VITA_RIGHT_AXIS_Y      3


#define VITA_JOYSTICK_DEAD_ZONE_DEFAULT 5000
#define VITA_JOYSTICK_SPEED_DEFAULT 6000
#define VITA_JOYSTICK_USER_EVENT 0x564A5545		// "VJUE"

#endif
