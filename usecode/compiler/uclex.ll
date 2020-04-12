%top {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif  // __GNUC__
}

%{
/**
 **	Uclex.ll - Usecode lexical scanner.
 **
 **	Written: 12/30/2000 - JSF
 **/

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

/*
Copyright (C) 2000 The Exult Team

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

#include <string>
#include <cstring>
#include <vector>
#include <set>
#include "ucparse.h"
#include "ucloc.h"
#include "ucfun.h"

using std::string;

extern std::vector<char *> include_dirs;	// -I directories.

/*
 *	Want a stack of files for implementing '#include'.
 */
std::vector<Uc_location *> locstack;
std::vector<YY_BUFFER_STATE> bufstack;
std::set<string> inclfiles;

/*
 *	Parse out a name in quotes.
 *
 *	Output:	->name, with a null replacing the ending quote.
 *		0 if not found.
 */

static char *Find_name
	(
	char *name,
	char *& ename			// ->null at end of name returned.
	)
	{
	while (*name && *name != '"')	// Find start of filename.
		name++;
	if (!*name)
		return nullptr;
	name++;				// Point to name.
	ename = name;			// Find end.
	while (*ename && *ename != '"')
		ename++;
	if (!*ename)
		return nullptr;
	*ename = 0;
	return name;
	}

/*
 *	Set location from a preprocessor string.
 */

static void Set_location
	(
	char *text			// ->first digit of line #.
	)
	{
	char *name;
	int line = strtol(text, &name, 10);
	char *ename;
	name = Find_name(name, ename);
	if (!name)
		return;
//cout << "Setting location at line " << line - 1 << endl;
					// We're 0-based.
	Uc_location::set_cur(name, line - 1);
	*name = '"';			// Restore text.
	}

/*
 *	Include another source. Each file is included ONCE.
 */

static void Include
	(
	char *yytext			// ->text containing name.
	)
	{
	char msg[180];
	if (bufstack.size() > 20)
		{
		Uc_location::yyerror("#includes are nested too deeply");
		exit(1);
		}
	char *ename;
	char *name = Find_name(yytext, ename);
	if (!name)
		{
		Uc_location::yyerror("No file in #include");
		return;
		}
					// Check if file has already been included.
	std::set<string>::iterator it = inclfiles.find(name);
	if (it != inclfiles.end())
		return;
	locstack.push_back(new Uc_location());
	bufstack.push_back(YY_CURRENT_BUFFER);
	yyin = fopen(name, "r");
					// Look in -I list if not found here.
	for (std::vector<char *>::const_iterator it = include_dirs.begin();
				!yyin && it != include_dirs.end(); ++it)
		{
		string path(*it);
		path += '/';
		path += name;
		yyin = fopen(path.c_str(), "r");
		}
	if (!yyin)
		{
		sprintf(msg, "Can't open '%s'", name);
		Uc_location::yyerror(msg);
		exit(1);
		}
					// Add file to list of included files.
	inclfiles.insert(name);
					// Set location to new file.
	Uc_location::set_cur(name, 0);
	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
	}

/*
 *	Handle #game directive.
 */

static void Set_game
	(
	char *yytext			// Contains name.
	)
	{
	char *ename;
	char *name = Find_name(yytext, ename);
	if (!name)
		Uc_location::yyerror("No name in #game");
	else if (strcmp(name, "blackgate") == 0)
		Uc_function::set_intrinsic_type(Uc_function::bg);
	else if (strcmp(name, "serpentisle") == 0)
		Uc_function::set_intrinsic_type(Uc_function::si);
	else if (strcmp(name, "serpentbeta") == 0)
		Uc_function::set_intrinsic_type(Uc_function::sib);
	else
		Uc_location::yyerror(
			"Specify \"blackgate\", \"serpentisle\" or \"serpentbeta\" "
				"with #game.");
	}

/*
 *	Handle #autonumber directive
 */

static void Set_autonum
	(
	char *text		// Contains number.
	)
	{
	char *name;
	int fun_number = strtol(text, &name, 0);
	if (fun_number<=0)
		Uc_location::yyerror("Starting function number too low in #autonumber");
	
	Uc_function_symbol::set_last_num(fun_number - 1);
	}

/*
 *	Make a copy of a string, interpreting '\' codes.
 *
 *	Output:	->allocated string.
 */

char *Handle_string
	(
	const char *from			// Ends with a '"'.
	)
	{
	char *to = new char[1 + strlen(from)];	// (Bigger than needed.)
	char *str = to;

	while (*from && *from != '\"')
		{
		if (*from != '\\')
			{
			*to++ = *from++;
			continue;
			}
		switch (*++from)
			{
		case 'n':
			*to++ = '\n'; break;
		case 't':
			*to++ = '\t'; break;
		case 'r':
			*to++ = '\r'; break;
		case '\"':
		case '\'':
		case '\\':
			*to++ = *from; break;
		default:
			{
			char buf[150];
			sprintf(buf, "Unknown escape sequence '\\%c'. If you are trying "
			             "to insert a literal backslash ('\\') into text, "
			             "write it as '\\\\'.", *from);
			Uc_location::yywarning(buf);
			*to++ = '\\';
			*to++ = *from; break;
			}
			}
		++from;
		}
	*to = 0;
	return str;
	}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#if !defined(__llvm__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wuseless-cast"
#else
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#endif
#endif  // __GNUC__

%}

%option nounput
%option noyy_top_state

%option stack
%x comment
%s fun_id
%s in_script
%s in_loop
%s in_breakable

%%

if		return IF;
else		return ELSE;
return		return RETURN;
while		return WHILE;
do		return DO;
for		return FOR;
in		return UCC_IN;
with		return WITH;
to		return TO;
var		return VAR;
void	return VOID;
alias	return ALIAS;
struct	return STRUCT;
int		return UCC_INT;
char	return UCC_CHAR;
byte	return UCC_CHAR;
long	return UCC_LONG;
const		return UCC_CONST;
string		return STRING;
enum		return ENUM;
extern		return EXTERN;
true		return UCTRUE;
false		return UCFALSE;
case		return CASE;
static		return STATIC_;
class		return CLASS;
new			return NEW;
delete		return DELETE;
switch		return SWITCH;
default		return DEFAULT;

converse	return CONVERSE;
user_choice	return CHOICE;
nested		return NESTED;
say		return SAY;
remove		return REMOVE;
add		return ADD;
hide		return HIDE;
run_script	return RUNSCRIPT;
message		return MESSAGE;
response	return RESPONSE;
script		return SCRIPT;
after		return AFTER;
ticks		return TICKS;
minutes		return MINUTES;
hours		return HOURS;
event		return EVENT;
gflags		return FLAG;
item		return ITEM;
goto		return GOTO;
try			return TRY;
catch		return CATCH;
abort		return ABORT;
throw		return THROW;
".original"	return ORIGINAL;
<fun_id>{
"shape#"	return SHAPENUM;
"object#"	return OBJECTNUM;
}

<in_breakable>{
break		return BREAK;
}
<in_loop>{
break		return BREAK;
continue	return CONTINUE;
}
					/* Script commands. */
<in_script>{
nop		return NOP;
nohalt		return NOHALT;
next		return NEXT;
finish		return FINISH;
resurrect	return RESURRECT;
continue	return CONTINUE;
reset	return RESET;
repeat		return REPEAT;
wait		return WAIT;
rise		return RISE;
descent		return DESCEND;
frame		return FRAME;
hatch		return HATCH;
setegg		return SETEGG;
previous	return PREVIOUS;
cycle		return CYCLE;
step		return STEP;
music		return MUSIC;
call		return CALL;
speech		return SPEECH;
sfx		return SFX;
face		return FACE;
weather		return WEATHER;
hit		return HIT;
attack		return ATTACK;
near	return NEAR;
far		return FAR;
actor		return ACTOR;
north		return NORTH;
south		return SOUTH;
east		return EAST;
west		return WEST;
nw		return NW;
ne		return NE;
sw		return SW;
se		return SE;
standing	return STANDING;
step_right	return STEP_RIGHT;
step_left	return STEP_LEFT;
ready		return READY;
raise_1h	return RAISE_1H;
reach_1h	return REACH_1H;
strike_1h	return STRIKE_1H;
raise_2h	return RAISE_2H;
reach_2h	return REACH_2H;
strike_2h	return STRIKE_2H;
sitting		return SITTING;
bowing		return BOWING;
kneeling	return KNEELING;
sleeping	return SLEEPING;
cast_up		return CAST_UP;
cast_out	return CAST_OUT;
cached_in		return CACHED_IN;
party_near		return PARTY_NEAR;
avatar_near		return AVATAR_NEAR;
avatar_far		return AVATAR_FAR;
avatar_footpad	return AVATAR_FOOTPAD;
party_footpad	return PARTY_FOOTPAD;
something_on	return SOMETHING_ON;
external_criteria	return EXTERNAL_CRITERIA;
normal_damage	return NORMAL_DAMAGE;
fire_damage		return FIRE_DAMAGE;
magic_damage	return MAGIC_DAMAGE;
lightning_damage	return LIGHTNING_DAMAGE;
ethereal_damage	return ETHEREAL_DAMAGE;
sonic_damage	return SONIC_DAMAGE;
}


"&&"		return AND;
"||"		return OR;
"!"		return NOT;

[a-zA-Z_][a-zA-Z0-9_]*	{
			yylval.strval = strdup(yytext);
			return IDENTIFIER;
			}
\"([^"]|\\.)*\"		{
					// Remove ending quote.
			yylval.strval = Handle_string(yytext + 1);
			return STRING_LITERAL;
			}
\"([^"]|\\.)*\"\*		{
					// Remove ending quote and asterisk.
			yylval.strval = strdup(yytext + 1);
			yylval.strval[strlen(yylval.strval) - 2] = 0;
			return STRING_PREFIX;
			}
[0-9]+			{
			yylval.intval = atoi(yytext);
			return INT_LITERAL;
			}
0x[0-9a-fA-F]+		{
			yylval.intval = strtol(yytext + 2, nullptr, 16);
			return INT_LITERAL;
			}

"=="			{ return EQUALS; }
"!="			{ return NEQUALS; }
"<="			{ return LTEQUALS; }
">="			{ return GTEQUALS; }
"->"			{ return UCC_POINTS; }
"::"			{ return UCC_SCOPE; }
"<<"			{ return UCC_INSERT; }
"+="			{ return ADD_EQ; }
"-="			{ return SUB_EQ; }
"*="			{ return MUL_EQ; }
"/="			{ return DIV_EQ; }
"%="			{ return MOD_EQ; }

"#"[ \t]+[0-9]+[ \t]+\"[^"]*\".*\n	{ Set_location(yytext + 2); Uc_location::increment_cur_line(); }
"#line"[ \t]+[0-9]+[ \t]+\"[^"]*\".*\n	{ Set_location(yytext + 6); Uc_location::increment_cur_line(); }
"#include"[ \t]+.*\n		{ Uc_location::increment_cur_line(); Include(yytext + 8); }
"#game"[ \t]+.*\n		{ Set_game(yytext + 5); Uc_location::increment_cur_line(); }
"#autonumber"[ \t]+"0x"[0-9a-fA-F]+.*\n	{ Set_autonum(yytext + 11); Uc_location::increment_cur_line(); }

\#[^\n]*	{ Uc_location::yyerror("Directives require a terminating new-line character before the end of file"); }
\#[^\n]*\n	{ Uc_location::yywarning("Unknown directive is being ignored"); Uc_location::increment_cur_line(); }

[ \t\r]+					/* Ignore spaces. */
"//"[^\n]*					/* Comments. */
"//"[^\n]*\n	Uc_location::increment_cur_line();
"/*"			yy_push_state(comment);

<comment>[^*\n]*				/* All but '*'. */
<comment>[^*\n]*\n			Uc_location::increment_cur_line();
<comment>"*"+[^*/\n]*			/* *'s not followed by '/'. */
<comment>"*"+[^*/\n]*\n		Uc_location::increment_cur_line();
<comment>"*"+"/"			yy_pop_state();
<comment><<EOF>>			{ Uc_location::yyerror("Comment not terminated");
								yyterminate(); }
\n			Uc_location::increment_cur_line();
.			return *yytext;		/* Being lazy. */
<<EOF>>			{
			if (locstack.empty())
				yyterminate();
			else		// Restore buffer and location.
				{
				Uc_location *loc = locstack.back();
				locstack.pop_back();
				const char *nm = loc->get_source();
				loc->set_cur(nm, loc->get_line());
				delete loc;
				// Close currently opened file.
				if (yyin && yyin != stdin && bufstack.size()) fclose(yyin);
				yy_delete_buffer(YY_CURRENT_BUFFER);
				yy_switch_to_buffer(bufstack.back());
				bufstack.pop_back();
				}
			}

%%

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

/*
 *	Start/end 'script' mode.
 */
void start_script()
	{
	yy_push_state(in_script);
	}
void end_script()
	{
	yy_pop_state();
	}
void start_loop()
	{
	yy_push_state(in_loop);
	}
void end_loop()
	{
	yy_pop_state();
	}
void start_breakable()
	{
	yy_push_state(in_breakable);
	}
void end_breakable()
	{
	yy_pop_state();
	}
void start_fun_id()
	{
	yy_push_state(fun_id);
	}
void end_fun_id()
	{
	yy_pop_state();
	}

extern "C" int yywrap() { return 1; }		/* Stop at EOF. */
