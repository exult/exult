//
// MSVC Kludges... and precompiled header for Exult
//
// DO NOT INCLUDE
//
// This files allows Exult, and it's tools, to compile under the very non
// standards compilent MSVC. It also serves as a precompiled header to greatly
// improve compile times in MSVC.
//

#ifndef MSVC_KLUDGES_H
#define MSVC_KLUDGES_H

// Firstly some things that need to be defined
#define VERSION "1.6.0"
#define EXULT_DATADIR "data/"
//#define FUDGE_SAMPLE_RATES
#define USE_FMOPL_MIDI
#define USE_MT32EMU_MIDI
#define USE_TIMIDITY_MIDI

#ifndef _WIN32
#error WTF!
#endif

// No GDI
#define NOGDI

// Settings for debug builds
#ifndef NDEBUG

#define USE_EXULTSTUDIO
#define ENABLE_MIDISFX
#define COLOURLESS_REALLY_HATES_THE_BG_SFX
#define FORCE_44KHZ

#ifndef DEBUG
//#define DEBUG 1
#endif

#endif

// Yeah, lets do unicode compiles
//#define UNICODE

// Don't need everything in the windows headers
#define WIN32_LEAN_AND_MEAN

// Disable some warnings
#pragma warning (disable: 4786) // Debug Len > 255
#pragma warning (disable: 4355) // 'this' : used in base member initializer list

#ifndef ENABLE_EXTRA_WARNINGS
//#pragma warning (disable: 4101) // unreferenced local variable
//#pragma warning (disable: 4309) // truncation of constant value
#pragma warning (disable: 4305) // truncation from 'const int' to 'char'
#pragma warning (disable: 4290) // C++ exception specification ignored except to indicate a function is not __declspec(nothrow)
#endif

//
// Hacks for MSVC 6
//
#if (_MSC_VER == 1200)

// Define size_t, but don't define tm, we'll define that later
#define _TM_DEFINED
#include <wctype.h>
#undef _TM_DEFINED

#define _SIZE_T_DEFINED

// we can't put cstdio into std because MSVC MUST have it in the global namespace
#include <cstdio>

// Now put the headers that should be in std into the std namespace
namespace std {

extern "C" {

#include <wchar.h>
#include <sys/types.h>
	//#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <cstdarg>
#include <malloc.h>

};
// Kludge to make Exult think that size_t has been put into the
// std namespace
typedef ::size_t size_t2;
#define size_t size_t2

// These are declared/defined in <cstdio> and the need to be in
// the std namespace
using ::FILE;
using ::fread;
using ::va_list;
using ::fopen;
using ::fclose;
using ::fwrite;
using ::remove;
using ::printf;
}

// These get put in std when they otherwise should be, or are required by other headers
using std::memcmp;
using std::memset;
using std::mbstate_t;
using std::size_t;
using std::time_t;
using std::_fsize_t;
using std::tm;
using std::_dev_t;
using std::_ino_t;
using std::_off_t;
using std::isspace;
using std::_alloca;
using std::wcslen;
using std::strtol;

// Nope, stat isn't defined
#ifdef _STAT_DEFINED
#undef _STAT_DEFINED
#endif
#include <sys/stat.h>

//
// Hacks for MSVC 7
//
// Can't do anything under windows CE! :(
#elif (_MSC_VER == 1300 || _MSC_VER == 1400 || _MSC_VER > 1400)

#include <cctype>
#define HAVE_SYS_STAT_H
#define inline __forceinline

#else

#error Unknown Version of MSVC being used. Edit "msvc_include.h" and add your version

#endif

// When doing a DEBUG compile we will output to the console
// However, SDL doesn't want us to do that
#ifdef DEBUG
//#define SDL_main main
#endif

// Some often used headers that could be included in out precompiled header
#include <fstream>
#include <sstream>
#include <exception>
#include <vector>
#include <iostream>
#include <iomanip>
#include <set>
#include <map>
#include <assert.h>
#include <fcntl.h>
#include <direct.h>
#include <windows.h>
#include <mmsystem.h>
#include <windef.h>

using std::getline;

// Why oh why!
// MSVC thinks near and far are actually supposed to be used with pointers
// but because they are no longer used, it consequently causes an error
#undef near
#define near ex_near
#undef far
#define far ex_far
#undef FAR
#define FAR
#undef NEAR
#define NEAR

// We need this defined
#define TO_STRING(x) #x

#ifdef DEBUG
#define STRICTUNZIP
#define STRICTZIP
#endif

// Only include these headers if we are actually compiling Exult
#ifdef EXULT

#ifndef HAVE_ZIP_SUPPORT
#define HAVE_ZIP_SUPPORT
#endif

#ifdef USING_PRECOMPILED_HEADER
#include <SDL.h>

#include "../actions.h"
#include "../actors.h"
#include "../args.h"
#include "../gamemgr/bggame.h"
#include "../browser.h"
#include "../cheat.h"
#include "../cheat_screen.h"
#include "../combat.h"
#include "../dir.h"
#include "../effects.h"
#include "../headers/exceptions.h"
#include "../headers/common_types.h"
#include "../exult.h"
#include "../exult_constants.h"
#include "../exultmenu.h"
#include "../fnames.h"
#include "../frameseq.h"
#include "../game.h"
#include "../gameclk.h"
#include "../gamewin.h"
#include "../headers/gamma.h"
#include "../hash_utils.h"
#include "../keyactions.h"
#include "../keys.h"
#include "../menulist.h"
#include "../mouse.h"
#include "../npcnear.h"
#include "../npctime.h"
#include "../palette.h"
#include "../paths.h"
#include "ready.h"
#include "../rect.h"
#include "../schedule.h"
#include "../shapeid.h"
#include "../gamemgr/sigame.h"
#include "../tiles.h"
#include "../tqueue.h"
#include "../txtscroll.h"
#include "../flic/playfli.h"
#include "../pathfinder/Astar.h"
#include "../pathfinder/PathFinder.h"
#include "../pathfinder/Zombie.h"
#include "../objs/animate.h"
#include "../objs/barge.h"
#include "../objs/chunks.h"
#include "../objs/chunkter.h"
#include "../objs/citerate.h"
#include "../objs/contain.h"
#include "../objs/egg.h"
#include "../objs/flags.h"
#include "../objs/iregobjs.h"
#include "../objs/jawbone.h"
#include "../objs/objiter.h"
#include "../objs/objlist.h"
#include "../objs/objs.h"
#include "../objs/ordinfo.h"
#include "../objs/spellbook.h"
#include "../objs/virstone.h"
#include "../conf/Configuration.h"
#include "../conf/XMLEntity.h"
#include "../usecode/conversation.h"
#include "../usecode/keyring.h"
#include "../usecode/ucinternal.h"
#include "../usecode/ucmachine.h"
#include "../usecode/ucsched.h"
#include "../usecode/ucscriptop.h"
#include "../usecode/useval.h"
#include "../gumps/Actor_gump.h"
#include "../gumps/AudioOptions_gump.h"
#include "../gumps/Book_gump.h"
#include "../gumps/CombatStats_gump.h"
#include "../gumps/Face_button.h"
#include "../gumps/Face_stats.h"
#include "../gumps/File_gump.h"
#include "../gumps/Gamemenu_gump.h"
#include "../gumps/Gump.h"
#include "../gumps/Gump_button.h"
#include "../gumps/Gump_manager.h"
#include "../gumps/Gump_ToggleButton.h"
#include "../gumps/gump_types.h"
#include "../gumps/gump_utils.h"
#include "../gumps/Gump_widget.h"
#include "../gumps/Jawbone_gump.h"
#include "../gumps/misc_buttons.h"
#include "../gumps/Modal_gump.h"
#include "../gumps/Newfile_gump.h"
#include "../gumps/Paperdoll_gump.h"
#include "../gumps/Scroll_gump.h"
#include "../gumps/Sign_gump.h"
#include "../gumps/Slider_gump.h"
#include "../gumps/Spellbook_gump.h"
#include "../gumps/Stats_gump.h"
#include "../gumps/Text_button.h"
#include "../gumps/Text_gump.h"
#include "../gumps/VideoOptions_gump.h"
#include "../gumps/Yesno_gump.h"
#include "../imagewin/ibuf8.h"
#include "../imagewin/imagebuf.h"
#include "../imagewin/imagewin.h"
#include "../imagewin/iwin8.h"
#include "../shapes/miscinf.h"
#include "../shapes/font.h"
#include "../shapes/fontvga.h"
#include "../shapes/items.h"
#include "../shapes/monstinf.h"
#include "../shapes/shapeinf.h"
#include "../shapes/shapevga.h"
#include "../shapes/u7drag.h"
#include "../shapes/vgafile.h"
#include "../shapes/ammoinf.h"
#include "../shapes/aniinf.h"
#include "../shapes/armorinf.h"
#include "../shapes/bodyinf.h"
#include "../shapes/continf.h"
#include "../shapes/effhpinf.h"
#include "../shapes/expinf.h"
#include "../shapes/frnameinf.h"
#include "../shapes/frpowers.h"
#include "../shapes/monstinf.h"
#include "../shapes/npcdollinf.h"
#include "../shapes/objdollinf.h"
#include "../shapes/sfxinf.h"
#include "../shapes/warminf.h"
#include "../shapes/weaponinf.h"

#endif //USING_PRECOMPILED_HEADER

//#include "../files/zip/zip_u7file.h"
#endif

// We will probably always want these
#ifdef USING_PRECOMPILED_HEADER
#include "../files/databuf.h"
#include "../files/Flat.h"
#include "../files/Flex.h"
#include "../files/IFF.h"
#include "../files/listfiles.h"
#include "../files/Table.h"
#include "../files/U7file.h"
#include "../files/utils.h"
#include "../files/crc.h"
#endif //USING_PRECOMPILED_HEADER
// Don't want SDL Parachute
#define NO_SDL_PARACHUTE

#define CHUNK_OBJ_DUMP

#endif /* !MSVC_KLUDGES_H */


