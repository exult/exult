%top {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#if !defined(__llvm__) && !defined(__clang__)
#	pragma GCC diagnostic ignored "-Wredundant-tags"
#	pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
}

%{
/**
 **	Uclex.ll - Usecode lexical scanner.
 **
 **	Written: 12/30/2000 - JSF
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

#include "ucfun.h"
#include "ucloc.h"
#include "ucparse.h"

#include <cstring>
#include <set>
#include <string>
#include <vector>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif    // __GNUC__

using std::string;

extern std::vector<char*> include_dirs;    // -I directories.

/*
 *	Want a stack of files for implementing '#include'.
 */
std::vector<Uc_location*>    locstack;
std::vector<YY_BUFFER_STATE> bufstack;
std::set<string>             inclfiles;

/*
 *	Parse out a name in quotes.
 *
 *	Output:	->name, with a null replacing the ending quote.
 *		0 if not found.
 */

static char* Find_name(
		char*  name,
		char*& ename    // ->null at end of name returned.
) {
	while (*name && *name != '"') {    // Find start of filename.
		name++;
	}
	if (!*name) {
		return nullptr;
	}
	name++;          // Point to name.
	ename = name;    // Find end.
	while (*ename && *ename != '"') {
		ename++;
	}
	if (!*ename) {
		return nullptr;
	}
	*ename = 0;
	return name;
}

/*
 *	Set location from a preprocessor string.
 */

static void Set_location(char* text    // ->first digit of line #.
) {
	char* name;
	int   line = strtol(text, &name, 10);
	char* ename;
	name = Find_name(name, ename);
	if (!name) {
		return;
	}
	// cout << "Setting location at line " << line - 1 << endl;
	//  We're 0-based.
	Uc_location::set_cur(name, line - 1);
	*name = '"';    // Restore text.
}

/*
 *	Include another source. Each file is included ONCE.
 */

static void Include(char* yytext    // ->text containing name.
) {
	if (bufstack.size() > 20) {
		Uc_location::yyerror("#includes are nested too deeply");
		exit(1);
	}
	char* ename;
	char* name = Find_name(yytext, ename);
	if (!name) {
		Uc_location::yyerror("No file in #include");
		return;
	}
	// Check if file has already been included.
	auto it = inclfiles.find(name);
	if (it != inclfiles.end()) {
		return;
	}
	locstack.push_back(new Uc_location());
	bufstack.push_back(YY_CURRENT_BUFFER);
	yyin = fopen(name, "r");
	// Look in -I list if not found here.
	if (yyin == nullptr) {
		for (char* dir : include_dirs) {
			string path(dir);
			path += '/';
			path += name;
			yyin = fopen(path.c_str(), "r");
			if (yyin != nullptr) {
				break;
			}
		}
	}

	if (!yyin) {
		Uc_location::yyerror("Can't open '%s'", name);
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

static void Set_game(char* yytext    // Contains name.
) {
	char* ename;
	char* name = Find_name(yytext, ename);
	if (!name) {
		Uc_location::yyerror("No name in #game");
	} else if (strcmp(name, "blackgate") == 0) {
		Uc_function::set_intrinsic_type(Uc_function::bg);
	} else if (strcmp(name, "serpentisle") == 0) {
		Uc_function::set_intrinsic_type(Uc_function::si);
	} else if (strcmp(name, "serpentbeta") == 0) {
		Uc_function::set_intrinsic_type(Uc_function::sib);
	} else {
		Uc_location::yyerror(
				"Specify \"blackgate\", \"serpentisle\", or \"serpentbeta\" "
				"with #game.");
	}
}

/*
 *	Handle #strictbraces directive.
 */

static void Set_strict_mode(char* yytext    // Contains name.
) {
	char* ename;
	char* name = Find_name(yytext, ename);
	if (!name) {
		Uc_location::yyerror("No parameter in #strictbraces");
	} else if (strcmp(name, "true") == 0) {
		Uc_location::set_strict_mode(true);
	} else if (strcmp(name, "false") == 0) {
		Uc_location::set_strict_mode(false);
	} else {
		Uc_location::yyerror(
				"Specify \"true\", or \"false\" with #strictbraces.");
	}
}

/*
 *	Handle #autonumber directive
 */

static void Set_autonum(char* text    // Contains number.
) {
	char* name;
	int   fun_number = strtol(text, &name, 0);
	if (fun_number <= 0) {
		Uc_location::yyerror("Starting function number too low in #autonumber");
	}

	Uc_function_symbol::set_last_num(fun_number - 1);
}

/*
 *	Make a copy of a string, interpreting '\' codes.
 *
 *	Output:	->allocated string.
 */

char* Handle_string(
		const char* from,    // Ends with a '"'.
		size_t      length) {
	char* to  = new char[length];    // (Bigger than needed.)
	char* str = to;

	// Note: strings from the official translations can contain almost all
	// control characters. So we can only filter out nulls here.
	// TODO: Let UCC specify a codepage for source files, a codepage for output,
	// and convert between them here. Once this is done, default to UTF-8 for
	// source and the "code page" of the official U7/SI translations.
	while (*from && *from != '\"') {
		if (*from != '\\') {
			*to++ = *from++;
			continue;
		}
		switch (*++from) {
		case 'n':
			*to++ = '\n';
			break;
		case 't':
			*to++ = '\t';
			break;
		case 'r':
			*to++ = '\r';
			break;
		case '\"':
		case '\'':
		case '\\':
			*to++ = *from;
			break;
		case '{': {
			// We know that this will be of the form \\{xxx}
			// where xxx is one of dot, ea, ee, ng, st, or th.
			const char* term = from;
			while (*term != '\0' && *term != '}') {
				++term;
			}
			if (*term != '}') {
				// Just in case.
				Uc_location::yyerror(
						"Unterminated rune escape sequence in string. This is "
						"probably a stray '\\{' which should be fixed.");
				*to++ = '{';
				break;
			}
			std::string escape(from + 1, term);
			if (escape == "dot") {
				*to++ = '|';
			} else if (escape == "ea") {
				*to++ = '+';
			} else if (escape == "ee") {
				*to++ = ')';
			} else if (escape == "ng") {
				*to++ = '*';
			} else if (escape == "st") {
				*to++ = ',';
			} else if (escape == "th") {
				*to++ = '(';
			} else {
				if (escape.size() != 2 && escape.size() != 3) {
					// Just in case.
					Uc_location::yyerror(
							"Invalid rune escape sequence '\\{%s}'. "
							"Valid escapes are: '\\{dot}', '\\{ea}', '\\{ee}', "
							"'\\{ng}', '\\{st}', '\\{th}'",
							escape.c_str());
					*to++ = '{';
					break;
				}
			}
			from = term;
			break;
		}
		default: {
			Uc_location::yywarning(
					"Unknown escape sequence '\\%c'. If you are trying "
					"to insert a literal backslash ('\\') into text, "
					"write it as '\\\\'.",
					*from);
			*to++ = '\\';
			*to++ = *from;
			break;
		}
		}
		++from;
	}
	*to = 0;
	return str;
}

void assert_no_embedded_nulls(const char* strval, size_t length) {
	if (length != strlen(strval) + 1) {
		Uc_location::yyerror("Stray null character found in string literal. "
							"The file may have been corrupted. "
							"Compilation cannot continue.");
		exit(1);
	}
}

[[noreturn]]
void assert_ascii_clean(const char* text) {
	unsigned chr = static_cast<unsigned char>(*text);
	Uc_location::yyerror("Invalid character found in source file: "
							"'\\x%02X'. The file may have been corrupted. "
							"Compilation cannot continue.", chr);
	exit(1);
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#if !defined(__llvm__) && !defined(__clang__)
#	pragma GCC diagnostic ignored "-Wredundant-tags"
#endif
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#if !defined(__llvm__) && !defined(__clang__)
#	pragma GCC diagnostic ignored "-Wuseless-cast"
#else
#	pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#endif
#endif    // __GNUC__

%}

%option nounput
%option noyy_top_state

%option stack
%x comment
%s fun_id
%s in_script
%s in_loop
%s in_conversation
%s in_breakable

string_literal		\"([^"]|\\\{(dot|ea|ee|ng|st|th)\}|\\[^\{])*\"
control_chars		[\x00-\x08\x0B\x0C\x0E-\x1F]
non_ascii_chars		[\x80-\xFF]
invalid_chars		{control_chars}|{non_ascii_chars}

%%

{invalid_chars}		{ assert_ascii_clean(yytext); }

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
byte	return UCC_BYTE;
long	return UCC_LONG;
const		return UCC_CONST;
string		return STRING;
enum		return ENUM;
declare		return DECLARE_;
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
always		return ALWAYS;
attend		return ATTEND;
forever		return FOREVER;
breakable	return BREAKABLE;

converse	return CONVERSE;
endconv		return ENDCONV;
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
nobreak		return NOBREAK;
<fun_id>{
"shape#"	return SHAPENUM;
"object#"	return OBJECTNUM;
"id#"	return IDNUM;
}
break		return BREAK;
continue	return CONTINUE;

<in_conversation>{
fallthrough	return FALLTHROUGH;
}
					/* Script commands. */
<in_script>{
raw		return RAW;
nop		return NOP;
nop2		return NOP2;
nohalt		return NOHALT;
next		return NEXT;
finish		return FINISH;
resurrect	return RESURRECT;
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
poison_damage	return POISON_DAMAGE;
starvation_damage	return STARVATION_DAMAGE;
freezing_damage	return FREEZING_DAMAGE;
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
{string_literal}		{
			// Remove starting quote.
			const char* strval = yytext + 1;
			assert_no_embedded_nulls(strval, yyleng);
			yylval.strval      = Handle_string(strval, yyleng);
			return STRING_LITERAL;
		}
{string_literal}\*		{
			// Remove starting quote.
			const char* strval = yytext + 1;
			// Detect null characters.
			assert_no_embedded_nulls(strval, yyleng);
			yylval.strval      = Handle_string(strval, yyleng);
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
"&="			{ return AND_EQ; }

"#"[ \t]+[0-9]+[ \t]+\"[^"]*\".*\n	{
			Set_location(yytext + 2);
			Uc_location::increment_cur_line();
		}
"#line"[ \t]+[0-9]+[ \t]+\"[^"]*\".*\n	{
			Set_location(yytext + 6);
			Uc_location::increment_cur_line();
		}
"#include"[ \t]+.*\n		{
			Uc_location::increment_cur_line();
			Include(yytext + 8);
		}
"#game"[ \t]+.*\n		{
			Set_game(yytext + 5);
			Uc_location::increment_cur_line();
		}
"#autonumber"[ \t]+"0x"[0-9a-fA-F]+.*\n	{
			Set_autonum(yytext + 11);
			Uc_location::increment_cur_line();
		}
"#strictbraces"[ \t]+.*\n		{
			Set_strict_mode(yytext + 13);
			Uc_location::increment_cur_line();
		}

\#[^\n]*	{
			Uc_location::yyerror(
					"Directives require a terminating new-line character "
					"before the end of file");
		}
\#[^\n]*\n	{
			Uc_location::yywarning("Unknown directive is being ignored");
			Uc_location::increment_cur_line();
		}

[ \t\r]+					/* Ignore spaces. */
"//"[^\n]*					/* Comments. */
"//"[^\n]*\n	Uc_location::increment_cur_line();
"/*"			yy_push_state(comment);

<comment>{control_chars}	{ assert_ascii_clean(yytext); }
<comment>[^*\n\x00]*				/* All but '*'. */
<comment>[^*\n\x00]*\n			Uc_location::increment_cur_line();
<comment>"*"+[^*/\n\x00]*			/* *'s not followed by '/'. */
<comment>"*"+[^*/\n\x00]*\n		Uc_location::increment_cur_line();
<comment>"*"+"/"			yy_pop_state();
<comment><<EOF>>			{
			Uc_location::yyerror("Comment not terminated");
			yyterminate();
		}
\n			Uc_location::increment_cur_line();
.			return *yytext;		/* Being lazy. */
<<EOF>>			{
			if (locstack.empty()) {
				yyterminate();
			} else {
				// Restore buffer and location.
				Uc_location* loc = locstack.back();
				locstack.pop_back();
				const char* nm = loc->get_source();
				Uc_location::set_cur(nm, loc->get_line());
				delete loc;
				// Close currently opened file.
				if (yyin && yyin != stdin && bufstack.size()) {
					fclose(yyin);
				}
				yy_delete_buffer(YY_CURRENT_BUFFER);
				yy_switch_to_buffer(bufstack.back());
				bufstack.pop_back();
			}
		}

%%

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif    // __GNUC__

/*
 *	Start/end 'script' mode.
 */
void start_script() {
	yy_push_state(in_script);
}

void end_script() {
	yy_pop_state();
}

void start_loop() {
	yy_push_state(in_loop);
}

void end_loop() {
	yy_pop_state();
}

void start_converse() {
	yy_push_state(in_conversation);
}

void end_converse() {
	yy_pop_state();
}

void start_breakable() {
	yy_push_state(in_breakable);
}

void end_breakable() {
	yy_pop_state();
}

void start_fun_id() {
	yy_push_state(fun_id);
}

void end_fun_id() {
	yy_pop_state();
}

bool can_break() {
	const int state = YYSTATE;
	return state == in_loop || state == in_conversation
		   || state == in_breakable;
}

bool can_continue() {
	const int state = YYSTATE;
	return state == in_loop || state == in_conversation;
}

extern "C" int yywrap() {
	return 1;
} /* Stop at EOF. */
