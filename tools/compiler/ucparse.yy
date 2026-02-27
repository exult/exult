%{
/**
 **	Ucparse.y - Usecode parser.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "opcodes.h"
#include "ucclass.h"
#include "ucexpr.h"
#include "ucfun.h"
#include "ucscriptop.h"
#include "ucstmt.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using std::string;
using std::string_view;
using std::vector;

void                        yyerror(const char*);
void                        yywarning(const char*);
extern int                  yylex();
extern void                 start_script();
extern void                 end_script();
extern void                 start_converse();
extern void                 end_converse();
extern void                 start_loop();
extern void                 end_loop();
extern void                 start_breakable();
extern void                 end_breakable();
extern void                 start_fun_id();
extern void                 end_fun_id();
extern bool                 can_break();
extern bool                 can_continue();
static Uc_var_symbol*       Get_variable(const char*);
static Uc_array_expression* Create_array(int, Uc_expression*);
static Uc_array_expression* Create_array(int, Uc_expression*, Uc_expression*);
static Uc_class*            Find_class(const char* nm);
static bool                 Class_unexpected_error(Uc_expression* expr);
static bool                 Nonclass_unexpected_error(Uc_expression* expr);
static bool Incompatible_classes_error(Uc_class* src, Uc_class* trg);
static Uc_call_expression* cls_method_call(
		Uc_expression* ths, Uc_class* curcls, Uc_class* clsscope, char* nm,
		Uc_array_expression* parms);
static Uc_call_expression* cls_function_call(
		Uc_expression* ths, Uc_class* curcls, char* nm, bool original,
		Uc_array_expression* parms);

std::vector<Uc_design_unit*> units;    // THIS is what we produce.

static Uc_function* cur_fun = nullptr;    // Current function being parsed.
static Uc_class* cur_class  = nullptr;    // ...or, current class being parsed.
static Uc_struct_symbol* cur_struct
		= nullptr;                // ...or, current struct being parsed.
static int  enum_val  = -1;       // Keeps track of enum elements.
static bool is_extern = false;    // Marks a function symbol as being an extern
static Uc_class* class_type = nullptr;    // For declaration of class variables.
static Uc_struct_symbol* struct_type
		= nullptr;    // For declaration of struct variables.
static bool                    has_ret        = false;
static int                     repeat_nesting = 0;
static std::vector<UsecodeOps> const_opcode;
static std::vector<bool>       nested_if;
static int converse = 0;    // If we are in a converse block.

struct Fun_id_info {
	Uc_function_symbol::Function_kind kind;
	int                               id;

	Fun_id_info(Uc_function_symbol::Function_kind k, int i) : kind(k), id(i) {}
};

struct Member_selector {
	Uc_expression* expr;
	char*          name;

	Member_selector(Uc_expression* e, char* n) : expr(e), name(n) {}
};

struct Loop_Init {
	const char*    var;
	Uc_var_symbol* array;

	Loop_Init(const char* v, Uc_var_symbol* a) : var(v), array(a) {}
};

struct Loop_Vars {
	Uc_var_symbol* var;
	Uc_var_symbol* array;
	Uc_var_symbol* index;
	Uc_var_symbol* length;

	Loop_Vars(
			Uc_var_symbol* v, Uc_var_symbol* a, Uc_var_symbol* i = nullptr,
			Uc_var_symbol* l = nullptr)
			: var(v), array(a), index(i), length(l) {}
};

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wcast-qual"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wredundant-tags"
#	endif
#endif    // __GNUC__
%}

%define parse.lac full
%define parse.error detailed

%union {
	class Uc_symbol *sym;
	class Uc_var_symbol *var;
	class Uc_class *cls;
	class Uc_struct_symbol *struc;
	class Uc_expression *expr;
	class Uc_call_expression *funcall;
	class Uc_function_symbol *funsym;
	class Uc_statement *stmt;
	class std::vector<Uc_var_symbol *> *varvec;
	class Uc_block_statement *block;
	class Uc_array_expression *exprlist;
	class Uc_label_statement *label;
	class std::vector<int> *intlist;
	class std::vector<Uc_statement *> *stmtlist;
	struct Fun_id_info *funid;
	struct Member_selector *membersel;
	struct Loop_Init *loopinit;
	struct Loop_Vars *loopvars;
	int intval;
	char *strval;
}

/*
 *	Keywords:
 */
%token IF "'if'"
%token ELSE "'else'"
%token RETURN "'return'"
%token DO "'do'"
%token WHILE "'while'"
%token FOR "'for'"
%token WITH "'with'"
%token TO "'to'"
%token EXTERN "'extern'"
%token DECLARE_ "'declare'"
%token BREAK "'break'"
%token GOTO "'goto'"
%token CASE "'case'"
%token VAR "'var'"
%token VOID "'void'"
%token ALIAS "'alias'"
%token STRUCT "'struct'"
%token UCC_CHAR "'char'"
%token UCC_BYTE "'byte'"
%token UCC_INT "'int'"
%token UCC_LONG "'long'"
%token UCC_CONST "'const'"
%token STRING "'string'"
%token ENUM "'enum'"
%token CONVERSE "'converse'"
%token NESTED "'nested'"
%token SAY "'say'"
%token MESSAGE "'message'"
%token RESPONSE "'response'"
%token EVENT "'event'"
%token FLAG "'gflags'"
%token ITEM "'item'"
%token UCTRUE "'true'"
%token UCFALSE "'false'"
%token REMOVE "'remove'"
%token ADD "'add'"
%token HIDE "'hide'"
%token SCRIPT "'script'"
%token AFTER "'after'"
%token TICKS "'ticks'"
%token STATIC_ "'static'"
%token ORIGINAL "'original'"
%token SHAPENUM "'shape#'"
%token OBJECTNUM "'object#'"
%token IDNUM "'id#'"
%token CLASS "'class'"
%token RUNSCRIPT "'runscript'"
%token SWITCH "'switch'"
%token DEFAULT "'default'"
%token FALLTHROUGH "'fallthrough'"
%token ALWAYS "'always'"
%token CHOICE "'user_choice'"
%token TRY "'try'"
%token CATCH "'catch'"
%token ABORT "'abort'"
%token THROW "'throw'"
%token ATTEND "'attend'"
%token ENDCONV "'endconv'"

/*
 *	Script keywords:
 */
					/* Script commands. */
%token NOBREAK "'nobreak'"
%token CONTINUE "'continue'"
%token REPEAT "'repeat'"
%token NOP "'nop'"
%token NOHALT "'nohalt'"
%token WAIT "'wait'"
%token RISE "'rise'"
%token DESCEND "'descend'"
%token FRAME "'frame'"
%token HATCH "'hatch'"
%token NEXT "'next'"
%token PREVIOUS "'previous'"
%token CYCLE "'cycle'"
%token STEP "'step'"
%token MUSIC "'music'"
%token CALL "'call'"
%token SPEECH "'speech'"
%token SFX "'sfx'"
%token FACE "'face'"
%token HIT "'hit'"
%token HOURS "'hours'"
%token ACTOR "'actor'"
%token ATTACK "'attack'"
%token FINISH "'finish'"
%token RESURRECT "'resurrect'"
%token SETEGG "'setegg'"
%token MINUTES "'minutes'"
%token RESET "'reset'"
%token WEATHER "'weather'"
%token NEAR "'near'"
%token FAR "'far'"
%token NORTH "'north'"
%token SOUTH "'south'"
%token EAST "'east'"
%token WEST "'west'"
%token NE "'ne'"
%token NW "'nw'"
%token SE "'se'"
%token SW "'sw'"
%token NOP2 "'nop2'"
%token RAW "'raw'"
%token STANDING "'standing'"
%token STEP_RIGHT "'step_right'"
%token STEP_LEFT "'step_left'"
%token READY "'ready'"
%token RAISE_1H "'raise_1h'"
%token REACH_1H "'reach_1h'"
%token STRIKE_1H "'strike_1h'"
%token RAISE_2H "'raise_2h'"
%token REACH_2H "'reach_2h'"
%token STRIKE_2H "'strike_2h'"
%token SITTING "'sitting'"
%token BOWING "'bowing'"
%token KNEELING "'kneeling'"
%token SLEEPING "'sleeping'"
%token CAST_UP "'cast_up'"
%token CAST_OUT "'cast_out'"
%token CACHED_IN "'cached_in'"
%token PARTY_NEAR "'party_near'"
%token AVATAR_NEAR "'avatar_near'"
%token AVATAR_FAR "'avatar_far'"
%token AVATAR_FOOTPAD "'avatar_footpad'"
%token PARTY_FOOTPAD "'party_footpad'"
%token SOMETHING_ON "'something_on'"
%token EXTERNAL_CRITERIA "'external_criteria'"
%token NORMAL_DAMAGE "'normal_damage'"
%token FIRE_DAMAGE "'fire_damage'"
%token MAGIC_DAMAGE "'magic_damage'"
%token LIGHTNING_DAMAGE "'lightning_damage'"
%token POISON_DAMAGE "'poison_damage'"
%token STARVATION_DAMAGE "'starvation_damage'"
%token FREEZING_DAMAGE "'freezing_damage'"
%token ETHEREAL_DAMAGE "'ethereal_damage'"
%token SONIC_DAMAGE "'sonic_damage'"
%token FOREVER "'forever'"
%token BREAKABLE "'breakable'"

/*
 *	Operators
 */
%token UCC_INSERT "'<<'"
%token ADD_EQ "'+='"
%token SUB_EQ "'-='"
%token MUL_EQ "'*='"
%token DIV_EQ "'/='"
%token MOD_EQ "'%='"
%token AND_EQ "'&='"
%token AND "'&&'"
%token OR "'||'"
%token EQUALS "'=='"
%token NEQUALS "'!='"
%token LTEQUALS "'<='"
%token GTEQUALS "'>='"
%token UCC_IN "'in'"
%token NOT "'!'"
%token ADDRESSOF "'&'"
%token UMINUS "'-'"
%token UPLUS "'+'"
%token NEW "'new'"
%token DELETE "'delete'"
%token UCC_POINTS "'->'"
%token UCC_SCOPE "'::'"

/*
 *	Other tokens:
 */
%token <strval> STRING_LITERAL "string literal"
%token <strval> STRING_PREFIX "string prefix"
%token <strval> IDENTIFIER "identifier"
%token <intval> INT_LITERAL "integer"

/*
 *	Handle if-then-else conflict.
 */
%precedence IF
%precedence ELSE

/*
 *	Handle <loop>-nobreak conflict.
 */

%precedence LOOP
%precedence NOBREAK

/*
 *	Expression precedence rules (lowest to highest):
 */
%left UCC_INSERT ADD_EQ SUB_EQ MUL_EQ DIV_EQ MOD_EQ AND_EQ '='
%left AND OR
%left EQUALS NEQUALS
%left LTEQUALS GTEQUALS '<' '>' UCC_IN
%left '-' '+' '&'
%left '*' '/' '%'
%right NOT ADDRESSOF UMINUS UPLUS NEW DELETE UCC_CAST
%left UCC_SYM
%left UCC_POINTS '.' '(' ')' '[' ']'
%left UCC_SCOPE

/*
 *	Production types:
 */
%type <expr> expression primary declared_var_value opt_script_delay item
%type <expr> script_command start_call new_expr class_expr
%type <expr> nonclass_expr opt_delay appended_element int_literal
%type <expr> opt_primary_expression conv_expression repeat_count
%type <intval> direction converse_options actor_frames egg_criteria
%type <intval> opt_original assignment_operator const_int_val opt_const_int_val
%type <intval> const_int_type int_cast dam_type opt_nest opt_int_value
%type <intval> sign_int_literal const_int_expr opt_const_int_expr addressof
%type <funid> opt_funid
%type <membersel> member_selector
%type <intlist> string_list response_expression
%type <sym> declared_sym
%type <var> declared_var param
%type <cls> opt_inheritance defined_class
%type <struc> defined_struct
%type <funsym> function_proto function_decl
%type <varvec> param_list opt_param_list
%type <stmt> statement assignment_statement if_statement while_statement
%type <stmt> statement_block return_statement function_call_statement
%type <stmt> special_method_call_statement trycatch_statement trystart_statement
%type <stmt> try_statement simple_statement noncase_statement simple_or_if_statement
%type <stmt> array_loop_statement var_decl var_decl_list stmt_declaration
%type <stmt> class_decl class_decl_list struct_decl_list struct_decl
%type <stmt> break_statement converse_statement opt_nobreak opt_nobreak_do
%type <stmt> converse_case switch_case script_statement switch_statement
%type <stmt> goto_statement answer_statement delete_statement opt_nobreak_conv
%type <stmt> continue_statement response_case fallthrough_statement
%type <stmt> scoped_statement converse_case_attend
%type <stmt> array_enum_statement opt_trailing_label
%type <block> statement_list noncase_statement_list
%type <label> label_statement
%type <loopvars> start_array_variables
%type <loopinit> start_array_loop
%type <exprlist> opt_expression_list expression_list script_command_list
%type <exprlist> opt_nonclass_expr_list nonclass_expr_list appended_element_list
%type <stmtlist> switch_case_list converse_case_list response_case_list
%type <funcall> function_call script_expr run_script_expression
%type <strval> string_literal string_prefix

%%

design:
	design global_decl
	| global_decl
	;

global_decl:
	function
	| function_decl
		{
		if (!Uc_function::add_global_function_symbol($1)) {
			delete $1;
		}
		}
	| const_int_decl
	| enum_decl
	| static_decl
	| class_definition
	| struct_definition
	;

class_definition:
	CLASS IDENTIFIER opt_inheritance
		{
		if ($3) {
			cur_class = new Uc_class($2, $3);
		} else {
			cur_class = new Uc_class($2);
		}
		units.push_back(cur_class);
		}
		'{' class_item_list '}'
		{
		// Add to 'globals' symbol table.
		Uc_class_symbol::create($2, cur_class);
		cur_class = nullptr;
		}
	;

opt_inheritance:
	':' defined_class
		{ $$ = $2; }
	| %empty
		{ $$ = nullptr; }
	;

class_item_list:
	class_item_list class_item
	| %empty
	;

class_item:
	VAR { has_ret = true; } class_var_def
		{ has_ret = false; }
	| VAR alias_tok IDENTIFIER '=' declared_var ';'
		{ cur_class->add_alias($3, $5); }
	| STRUCT '<' defined_struct '>' { struct_type = $3; } class_struct_def
		{ struct_type = nullptr; }
	| STRUCT '<' defined_struct '>' alias_tok IDENTIFIER '=' declared_var ';'
		{ cur_class->add_alias($6, $8, $3); }
	| CLASS '<' defined_class '>' { class_type = $3; } method
		{ class_type = nullptr; }
	| opt_void method
	;

class_var_def:
	var_decl_list ';'
	| method
	;

class_struct_def:
	struct_decl_list ';'
	| method
	;

method:
	IDENTIFIER '(' opt_param_list ')'
		{
		$3->insert(
				$3->begin(),    // So it's local[0].
				new Uc_class_inst_symbol("this", cur_class, 0));
		auto* funsym = Uc_function_symbol::create(
				$1, -1, *$3, false, cur_class->get_scope(),
				Uc_function_symbol::utility_fun);
		delete $3;    // A copy was made.

		// Set return type.
		if (has_ret) {
			funsym->set_ret_type(true);
		} else if (class_type) {
			funsym->set_ret_type(class_type);
		} else if (struct_type) {
			funsym->set_ret_type(struct_type);
		}
		has_ret     = false;
		class_type  = nullptr;
		struct_type = nullptr;

		cur_fun = new Uc_function(funsym, cur_class->get_scope());
		cur_class->add_method(cur_fun);
		}
	function_body
		{ cur_fun = nullptr; }
	;

struct_definition:
	STRUCT IDENTIFIER
		{
		cur_struct = new Uc_struct_symbol($2);
		Uc_function::add_global_struct_symbol(cur_struct);
		}
		'{' struct_item_list '}'
		{
		// Add to 'globals' symbol table.
		cur_struct = nullptr;
		}
	;

struct_item_list:
	struct_item_list struct_item
	| %empty
	;

struct_item:
	VAR IDENTIFIER ';'
		{ cur_struct->add($2); }
	| STRUCT '<' defined_struct '>' ';'
		{ cur_struct->merge_struct($3); }
	| STRUCT '<' defined_struct '>' IDENTIFIER ';'
		{ cur_struct->merge_struct($3); }
	;

function:
	function_proto
		{
		cur_fun = new Uc_function($1);
		units.push_back(cur_fun);
		}
	function_body
		{
		cur_fun = nullptr;
		}
	;

opt_trailing_label:
	label_statement
		{
		if (cur_fun->has_ret()) {
			Uc_location::yyerror(
					"Trailing label '%s' in non-void function '%s' is not "
					"allowed",
					$1->get_label().c_str(), cur_fun->get_name());
			$$ = nullptr;
		}
		}
	| %empty
		{
		$$ = nullptr;
		}
	;

function_body:
	'{' statement_list opt_trailing_label '}'
		{
		if ($3 != nullptr) {
			$2->add($3);
		}
		cur_fun->set_statement($2);
		}
	;

					/* opt_const_int_val assigns function #. */
function_proto:
	ret_type IDENTIFIER { start_fun_id(); } opt_funid '(' opt_param_list ')'
		{
		end_fun_id();
		if ($4->kind != Uc_function_symbol::utility_fun) {
			if (has_ret || struct_type) {
				Uc_location::yyerror(
					"Functions declared with '%s#' cannot return a value",
					$4->kind == Uc_function_symbol::shape_fun ? "shape"
																  : "object");
			}
			if (!$6->empty()) {
				Uc_location::yyerror(
						"Functions declared with '%s#' cannot have arguments",
						$4->kind == Uc_function_symbol::shape_fun ? "shape"
																  : "object");
			}
		}
		$$ = Uc_function_symbol::create(
				$2, $4->id, *$6, is_extern, nullptr, $4->kind);
		if (has_ret) {
			$$->set_ret_type(true);
		} else if (struct_type) {
			$$->set_ret_type(struct_type);
		}
		delete $6;    // A copy was made.
		delete $4;
		has_ret     = false;
		struct_type = nullptr;
		}
	| CLASS '<' defined_class '>' IDENTIFIER opt_const_int_val '(' opt_param_list ')'
		{
		$$ = Uc_function_symbol::create(
				$5, $6, *$8, is_extern, nullptr,
				Uc_function_symbol::utility_fun);
		$$->set_ret_type($3);
		delete $8;    // A copy was made.
		}
	;

opt_funid:
	SHAPENUM '(' const_int_expr ')'
		{
		$$ = new Fun_id_info(Uc_function_symbol::shape_fun, $3 < 0 ? -1 : $3);
		if ($3 < 0) {
			Uc_location::yyerror("Shape number cannot be negative");
		}
		}
	| OBJECTNUM '(' opt_const_int_expr ')'
		{ $$ = new Fun_id_info(Uc_function_symbol::object_fun, $3); }
	| opt_const_int_val
		{ $$ = new Fun_id_info(Uc_function_symbol::utility_fun, $1); }
	| IDNUM '(' const_int_expr ')'
		{ $$ = new Fun_id_info(Uc_function_symbol::utility_fun, $3); }
	;

opt_const_int_expr:
	const_int_expr
	| %empty
		{ $$ = -1; }
	;

const_int_expr:
	const_int_val
	| const_int_expr '+' const_int_expr
		{ $$ = $1 + $3; }
	| const_int_expr '-' const_int_expr
		{ $$ = $1 - $3; }
	| const_int_expr '*' const_int_expr
		{ $$ = $1 * $3; }
	| const_int_expr '/' const_int_expr
		{
		if ($3 == 0) {
			Uc_location::yyerror("Division by 0");
			$$ = -1;
		} else {
			$$ = $1 / $3;
		}
		}
	| const_int_expr '%' const_int_expr
		{
		if ($3 == 0) {
			Uc_location::yyerror("Division by 0");
			$$ = -1;
		} else {
			$$ = $1 % $3;
		}
		}
	| const_int_expr EQUALS const_int_expr
		{ $$ = $1 == $3; }
	| const_int_expr NEQUALS const_int_expr
		{ $$ = $1 != $3; }
	| const_int_expr '<' const_int_expr
		{ $$ = $1 < $3; }
	| const_int_expr LTEQUALS const_int_expr
		{ $$ = $1 <= $3; }
	| const_int_expr '>' const_int_expr
		{ $$ = $1 > $3; }
	| const_int_expr GTEQUALS const_int_expr
		{ $$ = $1 >= $3; }
	| const_int_expr AND const_int_expr
		{ $$ = $1 && $3; }
	| const_int_expr OR const_int_expr
		{ $$ = $1 || $3; }
	| NOT const_int_expr
		{ $$ = $2 == 0; }
	| '(' const_int_expr ')'
		{ $$ = $2; }
	| addressof
	;

opt_const_int_val:
	const_int_val
	| %empty
		{ $$ = -1; }
	;

const_int_val:
	sign_int_literal
	| IDENTIFIER
		{
		Uc_symbol*           sym = Uc_function::search_globals($1);
		Uc_const_int_symbol* var;
		if (!sym) {
			Uc_location::yyerror("'%s' not declared", $1);
			$$ = -1;
		} else if ((var = dynamic_cast<Uc_const_int_symbol*>(sym)) == nullptr) {
			Uc_location::yyerror("'%s' is not a constant integer", $1);
			$$ = -1;
		} else {
			$$ = var->get_value();
		}
		}
	;

statement_block:
	statement_block_start statement_list opt_trailing_label '}'
		{	// Block ends in label.
		if ($3 != nullptr) {
			$2->add($3);
		}
		$$ = $2;
		nested_if.pop_back();
		cur_fun->pop_scope();
		}
	| label_statement statement
		{	// Label followed by statements; "grab" next statement for label.
		if ($2) {
			$2->set_loop_name($1->get_label());
			auto* stmt = new Uc_block_statement();
			stmt->add($1);
			stmt->add($2);
			$$ = stmt;
		} else {    // This is the case for the "null" statement ';'.
			$$ = $1;
		}
		}
	;

statement_block_start:
	'{'
		{
		cur_fun->push_scope();
		nested_if.push_back(false);
		}
	;

statement_list:
	statement_list statement
		{
		if ($2) {
			$$->add($2);
		}
		}
	| %empty
		{ $$ = new Uc_block_statement(); }
	;

noncase_statement_list:
	noncase_statement_list noncase_statement
		{
		if ($2) {
			$$->add($2);
		}
		}
	| %empty
		{ $$ = new Uc_block_statement(); }
	;

simple_or_if_statement:
	simple_statement
	| if_statement
	;

simple_statement:
	stmt_declaration
	| assignment_statement
	| trycatch_statement
	| while_statement
	| array_loop_statement
	| array_enum_statement
	| function_call_statement
	| special_method_call_statement
	| return_statement
	| converse_statement
	| switch_statement
	| script_statement
	| break_statement
		{
		if (!can_break()) {
			Uc_location::yyerror("'break' statement not allowed outside of "
					"loops/converse/breakable/forever statements");
		}
		$$ = $1;
		}
	| continue_statement
		{
		if (!can_continue()) {
			Uc_location::yyerror("'continue' statement not allowed outside of "
					"loops/converse/breakable/forever statements");
		}
		$$ = $1;
		}
	| fallthrough_statement
	| goto_statement
	| delete_statement
	| SAY  '(' opt_nonclass_expr_list ')' ';'
		{ $$ = new Uc_say_statement($3); }
	| MESSAGE '(' opt_nonclass_expr_list ')' ';'
		{ $$ = new Uc_message_statement($3); }
	| answer_statement
	| ENDCONV ';'
		{ $$ = new Uc_endconv_statement(); }
	| throwabort_statement ';'
		{ $$ = new Uc_abort_statement(); }
	| throwabort_statement expression ';'
		{ $$ = new Uc_abort_statement($2); }
	| ';'				/* Null statement */
		{ $$ = nullptr; }
	;

statement:
	simple_or_if_statement
	| statement_block
	| converse_case_attend
	;

noncase_statement:
	simple_or_if_statement
	| statement_block
	;

throwabort_statement:
	ABORT
	| THROW
	;

alias_tok:
	ALIAS
	| '&'
	;

stmt_declaration:
	DECLARE_ VAR IDENTIFIER ';'
		{
		if (cur_fun) {
			cur_fun->add_symbol($3, false);
		} else {
			cur_class->add_symbol($3);
		}
		$$ = nullptr;
		}
	| DECLARE_ STRUCT '<' defined_struct '>' { struct_type = $4; } IDENTIFIER ';'
		{
		if (cur_fun) {
			cur_fun->add_symbol($7, struct_type, false);
		} else {
			cur_class->add_symbol($7, struct_type);
		}
		struct_type = nullptr;
		$$          = nullptr;
		}
	| DECLARE_ CLASS '<' defined_class '>' { class_type = $4; } IDENTIFIER ';'
		{
		if (class_type && cur_fun) {
			cur_fun->add_symbol($7, class_type, false);
		} else {
			// Unsupported for now
		}
		class_type = nullptr;
		$$         = nullptr;
		}
	| VAR var_decl_list ';'
		{ $$ = $2; }
	| VAR alias_tok IDENTIFIER '=' declared_var ';'
		{ cur_fun->add_alias($3, $5); $$ = nullptr; }
	| STRUCT '<' defined_struct '>' { struct_type = $3; } struct_decl_list ';'
		{ struct_type = nullptr; $$ = $6; }
	| STRUCT '<' defined_struct '>' alias_tok IDENTIFIER '=' declared_var ';'
		{ cur_fun->add_alias($6, $8, $3); $$ = nullptr; }
	| CLASS '<' defined_class '>' { class_type = $3; } class_decl_list ';'
		{ class_type = nullptr; $$ = $6; }
	| CLASS '<' defined_class '>' alias_tok IDENTIFIER '=' declared_var ';'
		{
		if (!$8->get_cls()) {
			Uc_location::yyerror("Can't convert non-class into class.");
		} else if (!Incompatible_classes_error($8->get_cls(), $3)) {
			// Alias may be of different (compatible) class.
			cur_fun->add_alias($6, $8, $3);
		}
		$$ = nullptr;
		}
	| STRING string_decl_list opt_comma ';'
		{ $$ = nullptr; }
	| const_int_decl
		{ $$ = nullptr; }
	| enum_decl
		{ $$ = nullptr; }
	| function_decl
		{
		if (!cur_fun->add_function_symbol(
					$1, cur_class ? cur_class->get_scope() : nullptr)) {
			delete $1;
		}
		$$ = nullptr;
		}
	| static_decl
		{ $$ = nullptr; }
	;

var_decl_list:
	var_decl_list ',' var_decl
		{
		if (!$3) {
			$$ = $1;
		} else if (!$1) {
			$$ = $3;
		} else /* Both nonzero.  Need a list. */
		{
			auto* b = dynamic_cast<Uc_block_statement*>($1);
			if (!b) {
				b = new Uc_block_statement();
				b->add($1);
			}
			b->add($3);
			$$ = b;
		}
		}
	| var_decl
		{ $$ = $1; }
	;

opt_comma:
	','
	| %empty
	;

enum_decl:				/* Decls. the elems, not the enum. */
	ENUM IDENTIFIER { enum_val = -1; }
			opt_enum_type '{' enum_item_list opt_comma '}' ';'
		{ const_opcode.pop_back(); }
	;

const_int_type:
	UCC_INT
		{ $$ = UC_PUSHI; }
	| UCC_CHAR
		{ $$ = UC_PUSHB; }
	| UCC_BYTE
		{ $$ = UC_PUSHB; }
	| UCC_LONG
		{ $$ = UC_PUSHI32; }
	| UCC_LONG UCC_INT
		{ $$ = UC_PUSHI32; }
	;

opt_enum_type:
	':' const_int_type
		{ const_opcode.push_back(static_cast<UsecodeOps>($2)); }
	| %empty
		{ const_opcode.push_back(UC_PUSHI); }
	;

enum_item_list:
	enum_item_list ',' enum_item
	| enum_item
	;

enum_item:
	const_int
	| IDENTIFIER
		{			/* Increment last value.	*/
		++enum_val;
		int op = const_opcode.back();
		if (cur_fun) {
			cur_fun->add_int_const_symbol($1, enum_val, op);
		} else {    // Global.
			Uc_function::add_global_int_const_symbol($1, enum_val, op);
		}
		}
	;

const_int_decl:
	UCC_CONST const_int_type { const_opcode.push_back(static_cast<UsecodeOps>($2)); }
		const_int_decl_list opt_comma ';'
		{ const_opcode.pop_back(); }
	;

const_int_decl_list:
	const_int_decl_list ',' const_int
	| const_int
	;

const_int:
	IDENTIFIER '=' const_int_expr
		{
		int op = const_opcode.back();
		if (cur_fun) {
			cur_fun->add_int_const_symbol($1, $3, op);
		} else {    // Global.
			Uc_function::add_global_int_const_symbol($1, $3, op);
		}
		enum_val = $3;    // In case we're in an enum.
		}
	| IDENTIFIER '=' int_cast const_int_expr
		{
		auto op = static_cast<UsecodeOps>($3);
		if (cur_fun) {
			cur_fun->add_int_const_symbol($1, $4, op);
		} else {    // Global.
			Uc_function::add_global_int_const_symbol($1, $4, op);
		}
		enum_val = $4;    // In case we're in an enum.
		}
	;

var_decl:
	IDENTIFIER
		{
		if (cur_fun) {
			cur_fun->add_symbol($1, true);
		} else {
			cur_class->add_symbol($1);
		}
		$$ = nullptr;
		}
	| IDENTIFIER '=' nonclass_expr
		{
		if (cur_class && !cur_fun) {
			Uc_location::yyerror(
					"Initialization of class member var '%s' must be done "
					"through constructor",
					$1);
			$$ = nullptr;
		} else {
			auto* var = cur_fun ? cur_fun->add_symbol($1, true)
								: cur_class->add_symbol($1);
			var->set_is_obj_fun($3->is_object_function(false));
			$$ = new Uc_assignment_statement(new Uc_var_expression(var), $3);
		}
		}
	| IDENTIFIER '=' script_expr
		{
		if (cur_class && !cur_fun) {
			Uc_location::yyerror(
					"Initialization of class member var '%s' must be done "
					"through constructor",
					$1);
			$$ = nullptr;
		} else {
			auto* var = cur_fun ? cur_fun->add_symbol($1, true)
								: cur_class->add_symbol($1);
			$$ = new Uc_assignment_statement(new Uc_var_expression(var), $3);
		}
		}
	;

class_decl_list:
	class_decl_list ',' class_decl
		{
		if (!$3) {
			$$ = $1;
		} else if (!$1) {
			$$ = $3;
		} else /*	Both nonzero; need a list.	*/
		{
			auto* b = dynamic_cast<Uc_block_statement*>($1);
			if (!b) {
				b = new Uc_block_statement();
				b->add($1);
			}
			b->add($3);
			$$ = b;
		}
		}
	| class_decl
		{ $$ = $1; }
	;

class_decl:
	IDENTIFIER
		{
		if (class_type && cur_fun) {
			cur_fun->add_symbol($1, class_type, true);
		} else {
			// Unsupported for now
		}
		$$ = nullptr;
		}
	| IDENTIFIER '=' class_expr
		{
		if (!class_type || !$3 || Nonclass_unexpected_error($3)) {
			$$ = nullptr;
		} else {
			Uc_class* src = $3->get_cls();
			if (Incompatible_classes_error(src, class_type)) {
				$$ = nullptr;
			} else {
				auto* v = cur_fun->add_symbol($1, class_type, true);
				$$      = new Uc_assignment_statement(
				                        new Uc_class_expression(v), $3);
			}
		}
		}
	;

struct_decl_list:
	struct_decl_list ',' struct_decl
		{
		if (!$3) {
			$$ = $1;
		} else if (!$1) {
			$$ = $3;
		} else /*	Both nonzero; need a list.	*/
		{
			auto* b = dynamic_cast<Uc_block_statement*>($1);
			if (!b) {
				b = new Uc_block_statement();
				b->add($1);
			}
			b->add($3);
			$$ = b;
		}
		}
	| struct_decl
		{ $$ = $1; }
	;

struct_decl:
	IDENTIFIER
		{
		if (cur_fun) {
			cur_fun->add_symbol($1, struct_type, true);
		} else {
			cur_class->add_symbol($1, struct_type);
		}
		$$ = nullptr;
		}
	| IDENTIFIER '=' nonclass_expr
		{
		if (cur_class && !cur_fun) {
			Uc_location::yyerror(
					"Initialization of class member struct '%s' must be done "
					"through constructor",
					$1);
			$$ = nullptr;
		} else {
			auto* var = cur_fun ? cur_fun->add_symbol($1, struct_type, true)
								: cur_class->add_symbol($1, struct_type);
			var->set_is_obj_fun($3->is_object_function(false));
			$$ = new Uc_assignment_statement(new Uc_var_expression(var), $3);
		}
		}
	;


class_expr:
	new_expr
		{ $$ = $1; }
	| IDENTIFIER
		{
		Uc_symbol* sym = cur_fun->search_up($1);
		if (!sym) {
			Uc_location::yyerror("'%s' not declared", $1);
			cur_fun->add_symbol($1, false);
			$$ = nullptr;
		} else if (sym->get_sym_type() != Uc_symbol::Class) {
			Uc_location::yyerror("'%s' not a class", $1);
			$$ = nullptr;
		} else {
			// Tests above guarantee this will always work.
			auto* cls = dynamic_cast<Uc_class_inst_symbol*>(sym->get_sym());
			$$        = new Uc_class_expression(cls);
		}
		}
	| function_call
		{
		$$ = $1;
		}
	;

static_decl:
	STATIC_ VAR static_var_decl_list ';'
	| STATIC_ STRUCT '<' defined_struct '>'
		{ struct_type = $4; }
		static_struct_var_decl_list ';'
		{ struct_type = nullptr; }
	| STATIC_ CLASS '<' defined_class '>'
		{ class_type = $4; }
		static_cls_decl_list ';'
		{ class_type = nullptr; }
	;

static_var_decl_list:
	static_var
	| static_var_decl_list ',' static_var
	;

static_var:
	IDENTIFIER
		{
		if (cur_fun) {
			cur_fun->add_static($1);
		} else {
			Uc_function::add_global_static($1);
		}
		}
	;

static_struct_var_decl_list:
	static_struct_var
	| static_struct_var_decl_list ',' static_struct_var
	;

static_struct_var:
	IDENTIFIER
		{
		if (cur_fun) {
			cur_fun->add_static($1, struct_type);
		} else {
			Uc_function::add_global_static($1, struct_type);
		}
		}
	;

static_cls_decl_list:
	static_cls
	| static_cls_decl_list ',' static_cls
	;

static_cls:
	IDENTIFIER
		{
		if (cur_fun) {
			cur_fun->add_static($1, class_type);
		} else {
			Uc_function::add_global_static($1, class_type);
		}
		}
	;

string_literal:
	string_literal STRING_LITERAL
		{
		size_t left_len  = strlen($1);
		size_t right_len = strlen($2);
		char*  ret       = new char[left_len + right_len + 1];
		std::memcpy(ret, $1, left_len);
		std::memcpy(ret + left_len, $2, right_len);
		ret[left_len + right_len] = '\0';
		delete[] $1;
		delete[] $2;
		$$ = ret;
		}
	| STRING_LITERAL
	;

string_prefix:
	string_literal STRING_PREFIX
		{
		size_t left_len  = strlen($1);
		size_t right_len = strlen($2);
		char*  ret       = new char[left_len + right_len + 1];
		std::memcpy(ret, $1, left_len);
		std::memcpy(ret + left_len, $2, right_len);
		ret[left_len + right_len] = '\0';
		delete[] $1;
		delete[] $2;
		$$ = ret;
		}
	| STRING_PREFIX
	;

string_decl_list:
	string_decl_list ',' string_decl
	| string_decl
	;

string_decl:
	IDENTIFIER '=' string_literal
		{
		cur_fun->add_string_symbol($1, $3);
		}
	;

function_decl:
	EXTERN { is_extern = true; } function_proto ';'
		{ $$ = $3; is_extern = false; }
	;

assignment_statement:
	expression '=' expression ';'
		{
		// Some rudimentary type-checking for classes
		if ($1->is_class()) {
			if (Nonclass_unexpected_error($3)) {
				$$ = nullptr;
			} else {
				Uc_class* trg = $1->get_cls();
				Uc_class* src = $3->get_cls();
				if (Incompatible_classes_error(src, trg)) {
					$$ = nullptr;
				} else {
					$1->set_is_obj_fun($3->is_object_function(false));
					$$ = new Uc_assignment_statement($1, $3);
				}
			}
		} else if (Class_unexpected_error($3)) {
			$$ = nullptr;
		} else {
			$1->set_is_obj_fun($3->is_object_function(false));
			$$ = new Uc_assignment_statement($1, $3);
		}
		}
	| expression '=' script_expr ';'
		{
		if (Class_unexpected_error($1)) {
			$$ = nullptr;
		} else {
			$$ = new Uc_assignment_statement($1, $3);
		}
		}
	| nonclass_expr assignment_operator nonclass_expr ';'
		{
		$1->set_is_obj_fun(-1);
		$$ = new Uc_assignment_statement(
				$1,
				new Uc_binary_expression(static_cast<UsecodeOps>($2), $1, $3));
		}
	| nonclass_expr UCC_INSERT appended_element_list ';'
		{
		$1->set_is_obj_fun(-1);
		$$ = new Uc_assignment_statement($1, new Uc_array_expression($1, $3));
		}
	;

assignment_operator:
	ADD_EQ
		{ $$ = UC_ADD; }
	| SUB_EQ
		{ $$ = UC_SUB; }
	| MUL_EQ
		{ $$ = UC_MUL; }
	| DIV_EQ
		{ $$ = UC_DIV; }
	| MOD_EQ
		{ $$ = UC_MOD; }
	| AND_EQ
		{ $$ = UC_ARRA; }
	;

appended_element_list:
	appended_element_list UCC_INSERT appended_element
		{ $$->add($3); }
	| appended_element
		{
		$$ = new Uc_array_expression();
		$$->add($1);
		}
	;

appended_element:
	nonclass_expr
	| '{' { start_script(); } script_command_list '}'
		{
		$$ = $3;
		end_script();
		}
	;

scoped_statement:
	statement_block_start statement_list opt_trailing_label '}'
		{
		if ($3 != nullptr) {
			$2->add($3);
		}
		cur_fun->pop_scope();
		nested_if.pop_back();
		$$ = $2;
		}
	| { cur_fun->push_scope(); nested_if.push_back(false); } simple_statement
		{
		if (Uc_location::get_strict_mode()) {
			Uc_location::yyerror("A Statements must be surrounded by braces in strict mode");
		}
		cur_fun->pop_scope();
		nested_if.pop_back();
		$$ = $2;
		}
	| { cur_fun->push_scope(); nested_if.push_back(false); } if_statement
		{
		nested_if.pop_back();
		if (Uc_location::get_strict_mode() && (nested_if.empty() || !nested_if.back())) {
			Uc_location::yyerror("B Statements must be surrounded by braces in strict mode");
		}
		cur_fun->pop_scope();
		$$ = $2;
		}
	;

if_statement:
	IF '(' expression ')' scoped_statement %prec IF
		{
		int val;
		if ($3->eval_const(val)) {
			if (val) {
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning("'if' clause will always be executed");
				}
				$$ = $5;
			} else {    // Need this because of those pesky GOTOs...
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning("'if' clause may never execute");
				}
				$$ = new Uc_if_statement(nullptr, $5, nullptr);
			}
			delete $3;
		} else {
			$$ = new Uc_if_statement($3, $5, nullptr);
		}
		}
	| IF '(' expression ')' scoped_statement
		ELSE { nested_if.push_back(true); } scoped_statement
		{
		int val;
		if ($3->eval_const(val)) {
			if (val) {
				// Need this because of those pesky GOTOs...
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning("'else' clause may never execute");
				}
				$$ = new Uc_if_statement(
						new Uc_int_expression(val != 0), $5, $8);
			} else {
				// Need this because of those pesky GOTOs...
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning("'if' clause may never execute");
				}
				$$ = new Uc_if_statement(nullptr, $5, $8);
			}
			delete $3;
		} else {
			$$ = new Uc_if_statement($3, $5, $8);
		}
		nested_if.pop_back();
		}
	;

trycatch_statement:
	trystart_statement '{' statement_list opt_trailing_label '}'
		{
		if ($4 != nullptr) {
			$3->add($4);
		}
		auto* stmt = dynamic_cast<Uc_trycatch_statement*>($1);
		if (!stmt) {
			Uc_location::yyerror("try/catch statement is not a try/catch statement");
		} else {
			stmt->set_catch_statement($3);
		}
		cur_fun->pop_scope();
		$$ = stmt;
		}
	;

trystart_statement:
	try_statement statement_list opt_trailing_label '}' CATCH '(' ')'
		{
		if ($3 != nullptr) {
			$2->add($3);
		}
		cur_fun->pop_scope();
		cur_fun->push_scope();
		$$ = new Uc_trycatch_statement($2);
		}
	| try_statement statement_list opt_trailing_label '}' CATCH '(' IDENTIFIER ')'
		{
		if ($3 != nullptr) {
			$2->add($3);
		}
		cur_fun->pop_scope();
		cur_fun->push_scope();
		auto* stmt = new Uc_trycatch_statement($2);
		stmt->set_catch_variable(cur_fun->add_symbol($7, true));
		$$ = stmt;
		}
	;

try_statement:
	TRY '{'
		{
		cur_fun->push_scope();
		}
	;

opt_nobreak:
	NOBREAK statement_block
		{ $$ = $2; }
	| %empty %prec LOOP
		{ $$ = nullptr; }
	;

opt_nobreak_do:
	NOBREAK statement_block
		{ $$ = $2; }
	| ';' %prec LOOP
		{ $$ = nullptr; }
	;

opt_nobreak_conv:
	NOBREAK statement_block
		{ $$ = $2; }
	| %empty %prec LOOP
		{ $$ = nullptr; }
	;

while_statement:
	WHILE '(' nonclass_expr ')' { start_loop(); } scoped_statement { end_loop(); } opt_nobreak
		{
		int val;
		if ($3->eval_const(val)) {
			if (val) {
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning("Infinite loop detected");
				}
				$$ = new Uc_infinite_loop_statement($6, $8);
			} else {    // Need 'may' because of those pesky GOTOs...
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning("Body of 'while' statement may never execute");
				}
				$$ = new Uc_while_statement(nullptr, $6, $8);
			}
			delete $3;
		} else {
			$$ = new Uc_while_statement($3, $6, $8);
		}
		}
	| FOREVER { start_loop(); } scoped_statement { end_loop(); } opt_nobreak
		{ $$ = new Uc_infinite_loop_statement($3, $5); }
	| DO { start_loop(); } scoped_statement WHILE '(' nonclass_expr ')' { end_loop(); } opt_nobreak_do
		{
		int val;
		if ($6->eval_const(val)) {
			if (val) {
				if (dynamic_cast<Uc_bool_expression*>($6) == nullptr) {
					$6->warning("Infinite loop detected");
				}
				$$ = new Uc_infinite_loop_statement($3, $9);
			} else    // Optimize loop away.
			{
				$$ = new Uc_breakable_statement($3, $9);
			}
			delete $6;
		} else {
			$$ = new Uc_dowhile_statement($6, $3, $9);
		}
		}
	| BREAKABLE { start_loop(); } scoped_statement { end_loop(); } opt_nobreak
		{ $$ = new Uc_breakable_statement($3, $5); }
	;

array_enum_statement:
	ENUM '(' ')' ';'
		{ $$ = new array_enum_statement(); }
	;

array_loop_statement:
	start_array_variables { start_loop(); } scoped_statement { end_loop(); } opt_nobreak
		{
		auto* expr = new Uc_arrayloop_statement($1->var, $1->array);
		expr->set_statement($3);
		expr->set_nobreak($5);
		expr->set_index($1->index);
		expr->set_array_size($1->length);
		expr->finish(cur_fun);
		cur_fun->pop_scope();
		delete $1;    // No onger needed
		$$ = expr;
		}
	| start_array_variables ATTEND IDENTIFIER ';'
		{
		auto* expr = new Uc_arrayloop_attend_statement($1->var, $1->array);
		expr->set_index($1->index);
		expr->set_array_size($1->length);
		expr->set_target($3);
		cur_fun->pop_scope();
		expr->finish(cur_fun);
		delete $1;    // No onger needed
		$$ = expr;
		}
	;

start_array_variables:
	start_array_loop ')'
		{
		Uc_var_symbol* index
				= cur_fun->add_symbol("$$for_loop_index_tempvar", true);
		Uc_var_symbol* length
				= cur_fun->add_symbol("$$for_loop_length_tempvar", true);
		Uc_var_symbol* var = Get_variable($1->var);
		$$                 = new Loop_Vars(var, $1->array, index, length);
		delete $1;
		}
	| start_array_loop WITH IDENTIFIER ')'
		{
		Uc_var_symbol* index = Get_variable($3);
		Uc_var_symbol* length
				= cur_fun->add_symbol("$$for_loop_length_tempvar", true);
		Uc_var_symbol* var = Get_variable($1->var);
		$$                 = new Loop_Vars(var, $1->array, index, length);
		delete $1;
		}
	| start_array_loop WITH IDENTIFIER TO IDENTIFIER ')'
		{
		Uc_var_symbol* index  = Get_variable($3);
		Uc_var_symbol* length = Get_variable($5);
		Uc_var_symbol* var    = Get_variable($1->var);
		$$                    = new Loop_Vars(var, $1->array, index, length);
		delete $1;
		}
	;

start_array_loop:
	start_for IDENTIFIER UCC_IN declared_var
		{
		if ($4->get_cls()) {
			Uc_location::yyerror(
					"Can't convert class '%s' into non-class",
					$4->get_name());
		}
		$$ = new Loop_Init($2, $4);
		}
	;

start_for:
	FOR '('
		{ cur_fun->push_scope(); }
	;

function_call_statement:
	function_call ';'
		{ $$ = new Uc_call_statement($1);  }
	;

special_method_call_statement:
					/* Have 'primary' say something.*/
	primary hierarchy_tok SAY '(' opt_nonclass_expr_list ')' ';'
		{
		auto* stmts = new Uc_block_statement();
		/* Set up 'show' call.		*/
		stmts->add(new Uc_call_statement(new Uc_call_expression(
				Uc_function::get_show_face(),
				new Uc_array_expression($1, new Uc_int_expression(0)),
				cur_fun)));
		stmts->add(new Uc_say_statement($5));
		$$ = stmts;
		}
	| primary hierarchy_tok HIDE '(' ')' ';'
		{
		$$ = new Uc_call_statement(new Uc_call_expression(
				Uc_function::get_remove_face(), new Uc_array_expression($1),
				cur_fun));
		}
	| run_script_expression ';'
		{ $$ = new Uc_call_statement($1); }
	;

run_script_expression:
	opt_primary_expression RUNSCRIPT '(' nonclass_expr opt_delay ')'
		{
		auto* parms = new Uc_array_expression();
		parms->add($1);    // Itemref.
		parms->add($4);    // Script.
		if ($5) {
			parms->add($5);    // Delay.
							   // Get the script intrinsic.
		}
		Uc_symbol* sym = Uc_function::get_intrinsic($5 ? 2 : 1);
		$$             = new Uc_call_expression(sym, parms, cur_fun);
		}
	;

opt_delay:
	',' nonclass_expr
		{ $$ = $2; }
	| %empty
		{ $$ = nullptr; }
	;

return_statement:
	RETURN expression ';'
		{
		if (!cur_fun->has_ret()) {
			Uc_location::yyerror(
					"Function '%s' can't return a value",
					cur_fun->get_name());
			$$ = nullptr;
		} else {
			Uc_class* src = $2->get_cls();
			Uc_class* trg = cur_fun->get_cls();
			if (!src && !trg) {
				$$ = new Uc_return_statement($2);
			} else if (!src || !trg) {
				int ival;
				if (trg && $2->eval_const(ival) && ival == 0) {
					$$ = new Uc_return_statement($2);
				} else {
					Uc_location::yyerror(
							"Function '%s' expects a return of %s '%s' but "
							"supplied value is %s'%s'",
							cur_fun->get_name(), trg ? "class" : "type",
							trg ? trg->get_name() : "var", src ? "class " : "",
							src ? src->get_name() : "var");
					$$ = nullptr;
				}
			} else if (Incompatible_classes_error(src, trg)) {
				$$ = nullptr;
			} else {
				$$ = new Uc_return_statement($2);
			}
		}
		}
	| RETURN ';'
		{
		if (cur_fun->has_ret()) {
			Uc_class* cls = cur_fun->get_cls();
			Uc_location::yyerror(
					"Function '%s' must return a '%s'",
					cur_fun->get_name(), cls ? cls->get_name() : "var");
			$$ = nullptr;
		} else {
			$$ = new Uc_return_statement();
		}
		}
	;

opt_nest:
	':' NESTED
		{ $$ = 1; }
	| %empty
		{ $$ = 0; }
	;

converse_statement:
	CONVERSE start_conv
			noncase_statement_list response_case_list '}' { end_converse(); } opt_nobreak_conv
		{
		cur_fun->pop_scope();
		--converse;
		$$ = new Uc_converse_statement(nullptr, $3, $4, false, $7);
		}
	| CONVERSE opt_nest conv_expression start_conv
			noncase_statement_list converse_case_list '}' { end_converse(); } opt_nobreak_conv
		{
		cur_fun->pop_scope();
		--converse;
		$$ = new Uc_converse_statement($3, $5, $6, $2, $9);
		}
	| CONVERSE ATTEND IDENTIFIER ';'
		{
		$$ = new Uc_converse_attend_statement($3);
		}
	;

start_conv:
	'{'
		{
		start_converse();
		cur_fun->push_scope();
		++converse;
		}
	;

conv_expression:
	'(' expression ')'
		{
		if (Class_unexpected_error($2)) {
			$$ = nullptr;
		} else {
			int ival;
			if ($2->eval_const(ival)) {
				$$ = nullptr;
			} else {
				$$ = $2;
			}
		}
		}
	;

converse_case_list:
	converse_case_list converse_case
		{
		if ($2) {
			$$->push_back($2);
		}
		}
	| %empty
		{ $$ = new vector<Uc_statement *>; }
	;

converse_case:
	CASE declared_var_value converse_options ':'
			{ cur_fun->push_scope(); } noncase_statement_list
		{
		Uc_expression* expr = $2;
		if (Class_unexpected_error(expr)) {
			expr = nullptr;
		}
		$$ = new Uc_converse_variable_case_statement(
				expr, $6, ($3 ? true : false));
		cur_fun->pop_scope();
		}
	| CASE string_list converse_options ':'
			{ cur_fun->push_scope(); } noncase_statement_list
		{
		$$ = new Uc_converse_strings_case_statement(
				*$2, $6, ($3 ? true : false));
		delete $2;    // A copy was made.
		cur_fun->pop_scope();
		}
	| ALWAYS ':'
			{ cur_fun->push_scope(); } noncase_statement_list
		{
		$$ = new Uc_converse_always_case_statement($4);
		cur_fun->pop_scope();
		}
	| DEFAULT ':'
			{ cur_fun->push_scope(); } noncase_statement_list
		{
		$$ = new Uc_converse_default_case_statement($4);
		cur_fun->pop_scope();
		}
	;

opt_int_value:
	'(' int_literal ')'
		{
		int ival;
		if (!$2->eval_const(ival)) {
			Uc_location::yyerror("Failed to obtain value from integer constant");
		}
		$$ = ival;
		}
	| %empty
		{ $$ = 1; }
	;

converse_case_attend:
	CASE declared_var_value converse_options ATTEND IDENTIFIER ':'
		{
		Uc_expression* expr = $2;
		if (Class_unexpected_error(expr)) {
			expr = nullptr;
		}
		$$ = new Uc_converse_variable_case_attend_statement(
				expr, $5, ($3 ? true : false));
		}
	| CASE string_list converse_options ATTEND IDENTIFIER ':'
		{
		$$ = new Uc_converse_strings_case_attend_statement(
				*$2, $5, ($3 ? true : false));
		delete $2;    // A copy was made.
		}
	| DEFAULT opt_int_value ATTEND IDENTIFIER ':'
		{ $$ = new Uc_converse_default_case_attend_statement($4, $2); }
	;

response_case_list:
	response_case_list ELSE { nested_if.push_back(true); } response_case
		{
		if ($4) {
			$$->push_back($4);
		}
		nested_if.pop_back();
		}
	| response_case %prec IF
		{
		$$ = new vector<Uc_statement*>;
		$$->push_back($1);
		}
	;

response_case:
	response_expression
			{ cur_fun->push_scope(); } statement_list
		{
		$$ = new Uc_converse_strings_case_statement(*$1, $3, false);
		delete $1;    // A copy was made.
		cur_fun->pop_scope();
		}
	;

response_expression:
	IF '(' RESPONSE EQUALS string_literal ')'
		{
		$$ = new vector<int>;
		$$->push_back(cur_fun->add_string($5));
		}
	| IF '(' RESPONSE UCC_IN '[' string_list ']' ')'
		{ $$ = $6; }
	;

string_list:
	string_list ',' string_literal
		{ $$->push_back(cur_fun->add_string($3)); }
	| string_literal
		{
		$$ = new vector<int>;
		$$->push_back(cur_fun->add_string($1));
		}
	;

converse_options:
	'(' REMOVE ')'			/* For now, just one.		*/
		{ $$ = 1; }
	| %empty
		{ $$ = 0; }
	;

switch_statement:
	SWITCH '('
			{ cur_fun->push_scope(); }
			expression ')' '{'
			{ start_breakable(); } switch_case_list '}'
		{
		if (Class_unexpected_error($4)) {
			$$ = nullptr;
		} else {
			end_breakable();
			$$ = new Uc_switch_statement($4, $8);
			delete $8;    // a copy has been made.
			cur_fun->pop_scope();
		}
		}
	;

switch_case_list:
	switch_case_list switch_case
		{ $$->push_back($2); }
	| switch_case
		{
		$$ = new vector<Uc_statement*>;
		$$->push_back($1);
		}
	;

switch_case:
	CASE int_literal ':' noncase_statement_list
		{ $$ = new Uc_switch_expression_case_statement($2, $4); }
	| CASE string_literal ':' noncase_statement_list
		{ $$ = new Uc_switch_expression_case_statement(
				new Uc_string_expression(cur_fun->add_string($2)), $4); }
	| DEFAULT ':' noncase_statement_list
		{ $$ = new Uc_switch_default_case_statement($3); }
	;

script_expr:
	SCRIPT { start_script(); } item opt_script_delay script_command
		{
		end_script();
		auto* parms = new Uc_array_expression();
		parms->add($3);    // Itemref.
		parms->add($5);    // Script.
		if ($4) {          // Delay?
			parms->add($4);
		}
		// Get the script intrinsic.
		Uc_symbol* sym = Uc_function::get_intrinsic($4 ? 2 : 1);
		$$             = new Uc_call_expression(sym, parms, cur_fun);
		}
	;

script_statement:			/* Yes, this could be an intrinsic. */
	script_expr
		{ $$ = new Uc_call_statement($1); }
	;

item:					/* Any object, NPC.	*/
	nonclass_expr
	;

script_command_list:
	script_command_list script_command
		{ $$->concat($2); }
	| script_command
		{
		$$ = new Uc_array_expression();
		$$->concat($1);
		}
	;

script_command:
	FINISH ';'
		{ $$ = new Uc_int_expression(Ucscript::finish, UC_PUSHB); }
	| RESURRECT ';'
		{ $$ = new Uc_int_expression(Ucscript::resurrect, UC_PUSHB); }
	| CONTINUE ';'			/* Continue script without painting. */
		{ $$ = new Uc_int_expression(Ucscript::cont, UC_PUSHB); }
	| RESET ';'			/* Go back to the beginning of the script */
		{ $$ = new Uc_int_expression(Ucscript::reset, UC_PUSHB); }
	| REPEAT repeat_count { repeat_nesting++; } script_command  ';'
		{
		repeat_nesting--;
		auto* result = new Uc_array_expression();
		result->concat($4);    // Start with cmnds. to repeat.
		int sz = result->get_exprs().size();
		result->add(new Uc_int_expression(
				repeat_nesting ? Ucscript::repeat2 : Ucscript::repeat,
				UC_PUSHB));
		// Then -offset to start.
		result->add(new Uc_int_expression(-sz));
		result->add($2);    // Loop var for repeat2.
		if (repeat_nesting) {
			result->add($2);    // Then #times to repeat.
		}
		$$ = result;
		}
	| REPEAT nonclass_expr ',' nonclass_expr { repeat_nesting++; }
		script_command  ';'
		{
		// Allow setting a different initial number of repeats.
		repeat_nesting--;
		auto* result = new Uc_array_expression();
		result->concat($6);    // Start with cmnds. to repeat.
		int sz = result->get_exprs().size();
		result->add(new Uc_int_expression(Ucscript::repeat2, UC_PUSHB));
		// Then -offset to start.
		result->add(new Uc_int_expression(-sz));
		result->add($2);    // Loop var for repeat2.
		result->add($4);    // Then #times to repeat.
		$$ = result;
		}
	| RAW '(' int_literal ')' ';'
		{ $$ = $3; }
	| NOP  ';'
		{ $$ = new Uc_int_expression(Ucscript::nop2, UC_PUSHB); }
	| NOP2 ';'
		{ $$ = new Uc_int_expression(Ucscript::nop1, UC_PUSHB); }
	| NOHALT  ';'
		{ $$ = new Uc_int_expression(Ucscript::dont_halt, UC_PUSHB); }
	| WAIT nonclass_expr  ';'		/* Ticks. */
		{ $$ = Create_array(Ucscript::delay_ticks, $2); }
	| WAIT nonclass_expr MINUTES  ';'	/* Game minutes. */
		{ $$ = Create_array(Ucscript::delay_minutes, $2); }
	| WAIT nonclass_expr HOURS  ';'	/* Game hours. */
		{ $$ = Create_array(Ucscript::delay_hours, $2); }
	| WAIT WHILE NEAR nonclass_expr ';'	/* Wait while avatar is near. */
		{ $$ = Create_array(Ucscript::wait_while_near, $4); }
	| WAIT WHILE FAR nonclass_expr ';'	/* Wait while avatar is far. */
		{ $$ = Create_array(Ucscript::wait_while_far, $4); }
	| REMOVE ';'			/* Remove item. */
		{ $$ = new Uc_int_expression(Ucscript::remove, UC_PUSHB); }
	| RISE ';'			/* For flying barges. */
		{ $$ = new Uc_int_expression(Ucscript::rise, UC_PUSHB); }
	| DESCEND ';'
		{ $$ = new Uc_int_expression(Ucscript::descend, UC_PUSHB); }
	| FRAME nonclass_expr ';'
		{ $$ = Create_array(Ucscript::frame, $2); }
	| ACTOR FRAME nonclass_expr ';'
		{
		$$ = new Uc_binary_expression(
				UC_ADD, new Uc_int_expression(Ucscript::npc_frame),
				new Uc_binary_expression(UC_MOD, $3, new Uc_int_expression(16)),
				UC_PUSHB);    // Want byte.
		}
	| ACTOR FRAME actor_frames ';'
		{ $$ = new Uc_int_expression(Ucscript::npc_frame + ($3 & 15), UC_PUSHB); }
	| HATCH ';'			/* Assumes item is an egg. */
		{ $$ = new Uc_int_expression(Ucscript::egg, UC_PUSHB); }
	| SETEGG nonclass_expr ',' nonclass_expr ';'
		{ $$ = Create_array(Ucscript::set_egg, $2, $4); }
	| SETEGG egg_criteria ',' nonclass_expr ';'
		{ $$ = Create_array(Ucscript::set_egg, new Uc_int_expression($2), $4); }
	| NEXT FRAME ';'		/* Next, but stop at last. */
		{ $$ = new Uc_int_expression(Ucscript::next_frame_max, UC_PUSHB); }
	| NEXT FRAME CYCLE ';'		/* Next, or back to 0. */
		{ $$ = new Uc_int_expression(Ucscript::next_frame, UC_PUSHB); }
	| PREVIOUS FRAME ';'		/* Prev. but stop at 0. */
		{ $$ = new Uc_int_expression(Ucscript::prev_frame_min, UC_PUSHB); }
	| PREVIOUS FRAME CYCLE ';'
		{ $$ = new Uc_int_expression(Ucscript::prev_frame, UC_PUSHB); }
	| SAY nonclass_expr ';'
		{ $$ = Create_array(Ucscript::say, $2); }
	| STEP nonclass_expr ';'		/* Step in given direction (0-7). */
		{
		$$ = Create_array(
				Ucscript::step,
				new Uc_binary_expression(
						UC_ADD, $2, new Uc_int_expression(0x30), UC_PUSHB),
				new Uc_int_expression(0));
		}
	| STEP nonclass_expr ',' nonclass_expr ';'	/* Step + dz. */
		{
		$$ = Create_array(
				Ucscript::step,
				new Uc_binary_expression(
						UC_ADD,
						new Uc_binary_expression(
								UC_MOD, $2,
								new Uc_int_expression(8)),    // dir is 0-7.
						new Uc_int_expression(0x30), UC_PUSHB),
				new Uc_binary_expression(
						UC_MOD, $4,
						new Uc_int_expression(16)));    // Allow max |dz| == 15.
		}
	| STEP direction ';'
		{ $$ = new Uc_int_expression(Ucscript::step_n + $2, UC_PUSHB); }
	| MUSIC nonclass_expr ';'
		{ $$ = Create_array(Ucscript::music, $2, new Uc_int_expression(0)); }
	| MUSIC nonclass_expr ',' nonclass_expr ';'
		{
		// This is the 'repeat' flag.
		Uc_expression* expr;
		int            ival;
		if ($4->eval_const(ival)) {
			expr = new Uc_int_expression(ival ? 1 : 0);
		} else {    // Argh.
			expr = $4;
		}
		$$ = Create_array(Ucscript::music, $2, expr);
		}
	| start_call ';'
		{
		int ival;
		if ($1->eval_const(ival) && ival >= 0 && ival < 0x100) {
			// Matches original usecode.
			$$ = Create_array(Ucscript::usecode, $1, new Uc_int_expression(0));
		} else {
			$$ = Create_array(Ucscript::usecode, $1);
		}
		}
	| start_call ',' nonclass_expr ';'
		{ $$ = Create_array(Ucscript::usecode2, $1, $3); }
	| SPEECH nonclass_expr ';'
		{ $$ = Create_array(Ucscript::speech, $2); }
	| SFX nonclass_expr ';'
		{ $$ = Create_array(Ucscript::sfx, $2); }
	| FACE nonclass_expr ';'
		{ $$ = Create_array(Ucscript::face_dir, $2); }
	| FACE direction ';'
		{ $$ = Create_array(Ucscript::face_dir, new Uc_int_expression($2)); }
	| WEATHER nonclass_expr ';'
		{ $$ = Create_array(Ucscript::weather, $2); }
	| HIT nonclass_expr ',' nonclass_expr ';'
		{ $$ = Create_array(Ucscript::hit, $2, $4); }
	| HIT nonclass_expr ',' dam_type ';'
		{ $$ = Create_array(Ucscript::hit, $2, new Uc_int_expression($4)); }
	| ATTACK ';'
		{ $$ = new Uc_int_expression(Ucscript::attack, UC_PUSHB); }
	| declared_var_value ';'
	| '{' script_command_list '}'
		{ $$ = $2; }
	;

start_call:
	CALL nonclass_expr
		{
		if (!$2) {
			$$ = new Uc_int_expression(0);
		} else {
			// May generate errors.
			if ($2->is_object_function() == -1 && Uc_location::get_shapefun_warn()) {
				// Don't know.
				Uc_location::yywarning(
						"Please ensure that 'call' uses a function declared "
						"with 'shape#' or 'object#'");
			}
			$$ = $2;
		}
		}
	;

repeat_count:
	nonclass_expr
	| FOREVER
		{
		if (repeat_nesting != 0) {
			Uc_location::yyerror("'repeat forever' is not allowed for nested 'repeat' "
					"commands");
		}
		$$ = new Uc_int_expression(255);
		}
	;

dam_type:
	NORMAL_DAMAGE
		{ $$ = 0; }
	| FIRE_DAMAGE
		{ $$ = 1; }
	| MAGIC_DAMAGE
		{ $$ = 2; }
	| LIGHTNING_DAMAGE
		{ $$ = 3; }
	| POISON_DAMAGE
		{ $$ = 3; }
	| STARVATION_DAMAGE
		{ $$ = 3; }
	| FREEZING_DAMAGE
		{ $$ = 3; }
	| ETHEREAL_DAMAGE
		{ $$ = 4; }
	| SONIC_DAMAGE
		{ $$ = 5; }
	;

direction:
	NORTH
		{ $$ = 0; }
	| NE
		{ $$ = 1; }
	| EAST
		{ $$ = 2; }
	| SE
		{ $$ = 3; }
	| SOUTH
		{ $$ = 4; }
	| SW
		{ $$ = 5; }
	| WEST
		{ $$ = 6; }
	| NW
		{ $$ = 7; }
	;

actor_frames:
	STANDING
		{ $$ = 0; }
	| STEP_RIGHT
		{ $$ = 1; }
	| STEP_LEFT
		{ $$ = 2; }
	| READY
		{ $$ = 3; }
	| RAISE_1H
		{ $$ = 4; }
	| REACH_1H
		{ $$ = 5; }
	| STRIKE_1H
		{ $$ = 6; }
	| RAISE_2H
		{ $$ = 7; }
	| REACH_2H
		{ $$ = 8; }
	| STRIKE_2H
		{ $$ = 9; }
	| SITTING
		{ $$ = 10; }
	| BOWING
		{ $$ = 11; }
	| KNEELING
		{ $$ = 12; }
	| SLEEPING
		{ $$ = 13; }
	| CAST_UP
		{ $$ = 14; }
	| CAST_OUT
		{ $$ = 15; }
	;

egg_criteria:
	CACHED_IN
		{ $$ = 0; }
	| PARTY_NEAR
		{ $$ = 1; }
	| AVATAR_NEAR
		{ $$ = 2; }
	| AVATAR_FAR
		{ $$ = 3; }
	| AVATAR_FOOTPAD
		{ $$ = 4; }
	| PARTY_FOOTPAD
		{ $$ = 5; }
	| SOMETHING_ON
		{ $$ = 6; }
	| EXTERNAL_CRITERIA
		{ $$ = 7; }
	;

opt_script_delay:
	AFTER nonclass_expr TICKS
		{ $$ = $2; }
	| %empty
		{ $$ = nullptr; }
	;

break_statement:
	BREAK ';'
		{ $$ = new Uc_break_statement(); }
	| BREAK IDENTIFIER ';'
		{ $$ = new Uc_break_statement($2); }
	;

continue_statement:
	CONTINUE ';'
		{ $$ = new Uc_continue_statement(); }
	| CONTINUE IDENTIFIER ';'
		{ $$ = new Uc_continue_statement($2); }
	;

fallthrough_statement:
	FALLTHROUGH ';'
		{ $$ = new Uc_fallthrough_statement(); }
	;

label_statement:
	IDENTIFIER ':'
		{
		if (cur_fun->search_label($1)) {
			Uc_location::yyerror("duplicate label: '%s'", $1);
			$$ = nullptr;
		} else {
			cur_fun->add_label($1);
			$$ = new Uc_label_statement($1);
		}
		}
	;

goto_statement:
	GOTO IDENTIFIER
		{
		if (Uc_location::get_goto_warn()) {
			Uc_location::yywarning("You *really* shouldn't use goto statements...");
		}
		$$ = new Uc_goto_statement($2);
		}
	;

delete_statement:
	DELETE declared_var ';'
		{
		auto* cls = dynamic_cast<Uc_class_inst_symbol*>($2->get_sym());
		if (!cls) {
			Uc_location::yyerror("'%s' is not a class", $2->get_name());
			$$ = nullptr;
		} else {
			$$ = new Uc_delete_statement(new Uc_del_expression(cls));
		}
		}
	;

answer_statement:
	ADD '(' nonclass_expr_list ')' ';'
		{
		$$ = new Uc_call_statement(new Uc_call_expression(
				Uc_function::get_add_answer(), $3, cur_fun));
		}
	| REMOVE '(' nonclass_expr_list ')' ';'
		{
		$$ = new Uc_call_statement(new Uc_call_expression(
				Uc_function::get_remove_answer(), $3, cur_fun));
		}
	;

opt_nonclass_expr_list:
	nonclass_expr_list
	| %empty
		{ $$ = new Uc_array_expression(); }
	;

nonclass_expr_list:
	nonclass_expr_list ',' nonclass_expr
		{ $$->add($3); }
	| nonclass_expr
		{
		$$ = new Uc_array_expression();
		$$->add($1);
		}
	;

nonclass_expr:
	expression
		{
		if (Class_unexpected_error($1)) {
			$$ = nullptr;
		} else {
			$$ = $1;
		}
		}
	;

expression:
	primary
		{ $$ = $1; }
	| nonclass_expr '+' nonclass_expr
		{ $$ = new Uc_binary_expression(UC_ADD, $1, $3); }
	| nonclass_expr '-' nonclass_expr
		{ $$ = new Uc_binary_expression(UC_SUB, $1, $3); }
	| nonclass_expr '*' nonclass_expr
		{ $$ = new Uc_binary_expression(UC_MUL, $1, $3); }
	| nonclass_expr '/' nonclass_expr
		{ $$ = new Uc_binary_expression(UC_DIV, $1, $3); }
	| nonclass_expr '%' nonclass_expr
		{ $$ = new Uc_binary_expression(UC_MOD, $1, $3); }
	| nonclass_expr EQUALS nonclass_expr
		{ $$ = new Uc_binary_expression(UC_CMPEQ, $1, $3); }
	| NEW SCRIPT { start_script(); } script_command
		{
		$$ = $4;
		end_script();
		}
	| CHOICE
		{
		if (!converse) {
			/* Only valid in converse blocks */
			Uc_location::yyerror("'CHOICE' can only be used in a conversation block!");
			$$ = nullptr;
		}
		$$ = new Uc_choice_expression();
		}
	| nonclass_expr NEQUALS nonclass_expr
		{ $$ = new Uc_binary_expression(UC_CMPNE, $1, $3); }
	| nonclass_expr '<' nonclass_expr
		{ $$ = new Uc_binary_expression(UC_CMPLT, $1, $3); }
	| nonclass_expr LTEQUALS nonclass_expr
		{ $$ = new Uc_binary_expression(UC_CMPLE, $1, $3); }
	| nonclass_expr '>' nonclass_expr
		{ $$ = new Uc_binary_expression(UC_CMPGT, $1, $3); }
	| nonclass_expr GTEQUALS nonclass_expr
		{ $$ = new Uc_binary_expression(UC_CMPGE, $1, $3); }
	| nonclass_expr AND nonclass_expr
		{ $$ = new Uc_binary_expression(UC_AND, $1, $3); }
	| nonclass_expr OR nonclass_expr
		{ $$ = new Uc_binary_expression(UC_OR, $1, $3); }
	| nonclass_expr UCC_IN nonclass_expr	/* Value in array. */
		{ $$ = new Uc_binary_expression(UC_IN, $1, $3); }
	| nonclass_expr '&' nonclass_expr	/* append arrays */
		{ $$ = new Uc_binary_expression(UC_ARRA, $1, $3); }
	| '+' primary %prec UPLUS
		{
		if (Class_unexpected_error($2)) {
			$$ = nullptr;
		} else {
			$$ = $2;
		}
		}
	| '-' primary %prec UMINUS
		{
		if (Class_unexpected_error($2)) {
			$$ = nullptr;
		} else {
			$$ = new Uc_binary_expression(UC_SUB, new Uc_int_expression(0), $2);
		}
		}
	| addressof
		{
		int        funid = $1;
		UsecodeOps op    = is_int_32bit(funid) ? UC_PUSHI32 : UC_PUSHI;
		$$               = new Uc_int_expression(funid, op);
		}
	| NOT primary
		{ $$ = new Uc_unary_expression(UC_NOT, $2); }
	| '[' opt_expression_list ']'	/* Concat. into an array. */
		{ $$ = $2; }
	| string_literal
		{ $$ = new Uc_string_expression(cur_fun->add_string($1)); }
	| string_prefix
		{ $$ = new Uc_string_prefix_expression(cur_fun, $1); }
	| new_expr
	| run_script_expression
		{ $$ = $1; }
	;

addressof:
	'&' IDENTIFIER %prec ADDRESSOF
		{
		// A way to retrieve the function's assigned
		// usecode number
		Uc_symbol* sym = cur_fun->search_up($2);
		if (!sym) {
			/* See if the symbol is defined */
			Uc_location::yyerror("'%s' not declared", $2);
			$$ = -1;
		}
		auto* fun = dynamic_cast<Uc_function_symbol*>(sym);
		if (!fun) {
			/* See if the symbol is a function */
			Uc_location::yyerror("'%s' is not a function", $2);
			$$ = -1;
		} else {
			/* Output the function's assigned number */
			$$ = fun->get_usecode_num();
		}
		}
	| '&' STRUCT '<' defined_struct '>' UCC_SCOPE IDENTIFIER
		{
		auto* sym = $4;
		if (sym) {
			int value = sym->search($7);
			if (value < 0) {
				Uc_location::yyerror(
						"'struct<%s>::%s' is not a valid structure member",
						$4->get_name(), $7);
			}
			$$ = value;
		} else {
			$$ = -1;
		}
		}
	;

opt_expression_list:
	expression_list opt_comma
	| %empty
		{ $$ = new Uc_array_expression(); }
	;

expression_list:
	expression_list ',' expression
		{ $$->add($3); }
	| expression
		{
		$$ = new Uc_array_expression();
		$$->add($1);
		}
	;

int_cast:
	'(' const_int_type ')'
		{ $$ = $2; }
	;

primary:
	INT_LITERAL
		{
		UsecodeOps op = !const_opcode.empty() ? const_opcode.back() : UC_PUSHI;
		if (is_sint_32bit($1) && op != UC_PUSHI32 && Uc_location::get_integer_warn()) {
			if (is_int_32bit($1)) {
				Uc_location::yywarning(
						"Literal integer '%d' cannot be represented as 16-bit "
						"integer. Assuming '(long)' cast.",
						$1);
				op = UC_PUSHI32;
			} else {
				Uc_location::yywarning(
						"Interpreting integer '%d' as the signed 16-bit "
						"integer '%d'. If this is incorrect, use '(long)' "
						"cast.",
						$1, static_cast<short>($1));
			}
		}
		$$ = new Uc_int_expression($1, op);
		}
	| int_cast INT_LITERAL %prec UCC_CAST
		{ $$ = new Uc_int_expression($2, static_cast<UsecodeOps>($1)); }
	| member_selector
		{
		auto*             expr = dynamic_cast<Uc_var_expression*>($1->expr);
		Uc_struct_symbol* base;
		if (!expr || !(base = expr->get_struct())) {
			Uc_location::yyerror("Expression is not a 'struct'");
			$$ = new Uc_int_expression(0);
		} else {
			int offset = base->search($1->name);
			if (offset < 0) {
				Uc_location::yyerror(
						"'%s' does not belong to struct '%s'",
						$1->name, base->get_name());
				$$ = new Uc_int_expression(0);
			} else {
				auto* var   = expr->get_var();
				auto* index = new Uc_int_expression(offset);
				if (var->is_static()) {
					$$ = new Uc_static_arrayelem_expression(var, index);
				} else if (var->get_sym_type() == Uc_symbol::Member_var) {
					$$ = new Uc_class_arrayelem_expression(var, index);
				} else {
					$$ = new Uc_arrayelem_expression(var, index);
				}
			}
		}
		delete $1;
		}
	| declared_var_value
		{ $$ = $1; }
	| declared_var '[' expression ']'
		{
		if ($1->get_cls()) {
			Uc_location::yyerror(
					"Can't convert class '%s' into non-class",
					$1->get_name());
			$$ = new Uc_arrayelem_expression($1, $3);
		} else if ($1->is_static()) {
			$$ = new Uc_static_arrayelem_expression($1, $3);
		} else if ($1->get_sym_type() == Uc_symbol::Member_var) {
			$$ = new Uc_class_arrayelem_expression($1, $3);
		} else {
			$$ = new Uc_arrayelem_expression($1, $3);
		}
		}
	| FLAG '[' nonclass_expr ']'
		{ $$ = new Uc_flag_expression($3); }
	| function_call
		{
		if ($1) {
			$$ = $1;
		} else {
			$$ = new Uc_int_expression(0);
		}
		}
	| UCTRUE
		{ $$ = new Uc_bool_expression(true); }
	| UCFALSE
		{ $$ = new Uc_bool_expression(false); }
	| EVENT
		{ $$ = new Uc_event_expression(); }
	| ITEM
		{ $$ = new Uc_item_expression(); }
	| '(' expression ')'
		{ $$ = $2; }
	;

new_expr:
	NEW defined_class '(' opt_nonclass_expr_list ')'
		{
		if ($2) {
			$$ = new Uc_new_expression(new Uc_class_inst_symbol("", $2, 0), $4);
		} else {
			$$ = new Uc_int_expression(0);
		}
		}
	;

hierarchy_tok:
	UCC_POINTS
	| '.'
	;

member_selector:
	primary hierarchy_tok IDENTIFIER
		{ $$ = new Member_selector($1, $3); }
	;

opt_primary_expression:
	primary hierarchy_tok
		{ $$ = $1; }
	| %empty
		{ $$ = new Uc_item_expression(); }

function_call:
	member_selector opt_original '(' opt_expression_list ')'
		{
		$$ = cls_function_call($1->expr, cur_class, $1->name, $2, $4);
		delete $1;
		}
	| IDENTIFIER opt_original '(' opt_expression_list ')'
		{
		$$ = cls_function_call(nullptr, cur_class, $1, $2, $4);
		}
	| primary hierarchy_tok defined_class UCC_SCOPE IDENTIFIER '(' opt_expression_list ')'
		{
		$$ = cls_method_call($1, $1->get_cls(), $3, $5, $7);
		}
	| defined_class UCC_SCOPE IDENTIFIER '(' opt_expression_list ')'
		{
		$$ = cls_method_call(nullptr, cur_class, $1, $3, $5);
		}
	| primary hierarchy_tok '(' '*' primary ')' '(' opt_expression_list ')'
		{
		$$ = new Uc_call_expression($5, $8, cur_fun);
		$$->set_itemref($1);
		}
	| '(' '*' primary ')' '(' opt_expression_list ')'
		{
		$$ = new Uc_call_expression($3, $6, cur_fun);
		$$->set_itemref(nullptr);
		}
	| primary hierarchy_tok '(' '@' int_literal ')' '(' opt_expression_list ')'
		{
		int num;
		if (!$5->eval_const(num)) {
			Uc_location::yyerror("Failed to obtain value from integer constant");
			$$ = nullptr;
		} else {
			$$ = new Uc_call_expression(
					Uc_function::get_intrinsic(num), $8, cur_fun);
			$$->set_itemref($1);
			$$->check_params();
		}
		}
	| '(' '@' int_literal ')' '(' opt_expression_list ')'
		{
		int num;
		if (!$3->eval_const(num)) {
			Uc_location::yyerror("Failed to obtain value from integer constant");
			$$ = nullptr;
		} else {
			$$ = new Uc_call_expression(
					Uc_function::get_intrinsic(num), $6, cur_fun);
			$$->check_params();
		}
		}
	;

opt_original:
	ORIGINAL
		{ $$ = 1; }
	| %empty
		{ $$ = 0; }
	;

opt_param_list:
	param_list
	| %empty
		{ $$ = new std::vector<Uc_var_symbol *>; }
	;

param_list:
	param_list ',' param
		{ $1->push_back($3); }
	| param
		{
		$$ = new std::vector<Uc_var_symbol *>;
		$$->push_back($1);
		}
	;

param:
	IDENTIFIER
		{ $$ = new Uc_var_symbol($1, 0); }
	| VAR IDENTIFIER
		{ $$ = new Uc_var_symbol($2, 0); }
	| STRUCT '<' defined_struct '>' IDENTIFIER
		{ $$ = new Uc_struct_var_symbol($5, $3, 0); }
	| CLASS '<' defined_class '>' IDENTIFIER
		{ $$ = new Uc_class_inst_symbol($5, $3, 0); }
	;

sign_int_literal:
	INT_LITERAL
	| '-' INT_LITERAL %prec UMINUS
		{ $$ = -$2; }
	| '+' INT_LITERAL %prec UPLUS
		{ $$ = $2; }
	;

int_literal:				/* A const. integer value.	*/
	sign_int_literal
		{
		UsecodeOps op = !const_opcode.empty() ? const_opcode.back() : UC_PUSHI;
		if (is_sint_32bit($1) && op != UC_PUSHI32) {
			if (is_int_32bit($1)) {
				Uc_location::yywarning(
						"Literal integer '%d' cannot be represented as 16-bit "
						"integer. Assuming '(long)' cast.",
						$1);
				op = UC_PUSHI32;
			} else {
				Uc_location::yywarning(
						"Interpreting integer '%d' as the signed 16-bit "
						"integer '%d'. If this is incorrect, use '(long)' "
						"cast.",
						$1, static_cast<short>($1));
			}
		}
		$$ = new Uc_int_expression($1, op);
		}
	| int_cast sign_int_literal %prec UCC_CAST
		{ $$ = new Uc_int_expression($2, static_cast<UsecodeOps>($1)); }
	| declared_sym
		{
		auto* sym = dynamic_cast<Uc_const_int_symbol*>($1);
		if (!sym) {
			Uc_location::yyerror(
					"'%s' is not a const int",
					$1->get_name());
			$$ = nullptr;
		} else {
			$$ = sym->create_expression();
		}
		}
	| UCTRUE
		{ $$ = new Uc_bool_expression(true); }
	| UCFALSE
		{ $$ = new Uc_bool_expression(false); }
	;

opt_void:
	VOID
	| %empty
		{
		Uc_location::yywarning("You should prepend 'void' for functions that do"
				  " not return a value.");
		}
	;

ret_type:
	VAR
		{ has_ret = true; }
	| STRUCT '<' defined_struct '>'
		{ struct_type = $3; }
	| opt_void
		{ has_ret = false; }
	;

declared_var_value:
	declared_sym %prec UCC_SYM
		{
		$$ = $1->create_expression();
		if (!$$) {
			Uc_location::yyerror("Can't use '%s' here", $1->get_name());
			$$ = new Uc_int_expression(0);
		}
		}
	;

declared_var:
	declared_sym %prec UCC_SYM
		{
		auto* var = dynamic_cast<Uc_var_symbol*>($1);
		if (!var) {
			Uc_location::yyerror("'%s' not a 'var'", $1->get_name());
			std::string buf($1->get_name());
			buf += "_needvar";
			var = cur_fun->add_symbol(buf.c_str(), false);
		}
		$$ = var;
		}
	;

declared_sym:
	IDENTIFIER
		{
		Uc_symbol* sym = cur_fun->search_up($1);
		if (!sym) {
			Uc_location::yyerror("'%s' not declared", $1);
			sym = cur_fun->add_symbol($1, false);
		}
		$$ = sym;
		}
	;

defined_class:
	IDENTIFIER
		{ $$ = Find_class($1); }
	;

defined_struct:
	IDENTIFIER
		{
		auto* sym = dynamic_cast<Uc_struct_symbol*>(
				Uc_function::search_globals($1));
		if (!sym) {
			Uc_location::yyerror("'%s' not found, or is not a struct.", $1);
			$$ = nullptr;
		} else {
			$$ = sym;
		}
		}
	;

%%

#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

static Uc_var_symbol* Get_variable(const char* ident) {
	Uc_symbol* sym = cur_fun->search_up(ident);
	auto*      var = dynamic_cast<Uc_var_symbol*>(sym);
	if (!var) {
		var = cur_fun->add_symbol(ident, true);
	}
	return var;
}

/*
 *	Create an array with an integer as the first element.
 */

static Uc_array_expression* Create_array(int e1, Uc_expression* e2) {
	auto* arr = new Uc_array_expression();
	arr->add(new Uc_int_expression(e1, UC_PUSHB));
	arr->add(e2);
	return arr;
}

static Uc_array_expression* Create_array(
		int e1, Uc_expression* e2, Uc_expression* e3) {
	auto* arr = new Uc_array_expression();
	arr->add(new Uc_int_expression(e1, UC_PUSHB));
	arr->add(e2);
	arr->add(e3);
	return arr;
}

static Uc_class* Find_class(const char* nm) {
	auto* csym
			= dynamic_cast<Uc_class_symbol*>(Uc_function::search_globals(nm));
	if (!csym) {
		Uc_location::yyerror("'%s' not found, or is not a class.", nm);
		return nullptr;
	}
	return csym->get_cls();
}

static bool Class_unexpected_error(Uc_expression* expr) {
	if (expr->is_class()) {
		Uc_location::yyerror("Can't convert class into non-class");
		return true;
	}
	return false;
}

static bool Nonclass_unexpected_error(Uc_expression* expr) {
	if (!expr->is_class()) {
		Uc_location::yyerror("Can't convert non-class into class.");
		return true;
	}
	return false;
}

static bool Incompatible_classes_error(Uc_class* src, Uc_class* trg) {
	if (!src->is_class_compatible(trg->get_name())) {
		Uc_location::yyerror(
				"Class '%s' can't be converted into class '%s'",
				src->get_name(), trg->get_name());
		return true;
	}
	return false;
}

static Uc_call_expression* cls_method_call(
		Uc_expression* ths, Uc_class* curcls, Uc_class* clsscope, char* nm,
		Uc_array_expression* parms) {
	if (!curcls) {
		Uc_location::yyerror("'%s' requires a class object", nm);
		return nullptr;
	}

	if (Incompatible_classes_error(curcls, clsscope)) {
		return nullptr;
	}

	Uc_symbol* sym = clsscope->get_scope()->search(nm);
	if (!sym) {
		Uc_location::yyerror("Function '%s' is not declared in class '%s'",
				nm, clsscope->get_name());
		return nullptr;
	}

	auto* fun = dynamic_cast<Uc_function_symbol*>(sym);
	if (!fun) {
		Uc_location::yyerror("'%s' is not a function", nm);
		return nullptr;
	}

	auto* ret = new Uc_call_expression(sym, parms, cur_fun, false);
	ret->set_itemref(ths);
	ret->set_call_scope(clsscope);
	ret->check_params();
	return ret;
}

static bool Uc_is_valid_calle(
		Uc_symbol* sym, Uc_expression*& ths, char* nm, bool original) {
	if (original) {
		return true;
	}
	auto* fun = dynamic_cast<Uc_function_symbol*>(sym);
	if (!fun) {    // Most likely an intrinsic.
		return true;
	}

	if (fun->get_function_type() == Uc_function_symbol::utility_fun) {
		if (ths && !ths->is_class()) {
			Uc_location::yyerror("'%s' is not an object or shape function", nm);
			return false;
		} else if (ths) {
			Uc_location::yyerror("'%s' is not a member of class '%s'", nm,
					ths->get_cls()->get_name());
			return false;
		}
	} else {
		if (!ths) {
			Uc_location::yywarning(
					"'%s' expects an itemref, but none was supplied; using "
					"current itemref",
					nm);
			ths = new Uc_item_expression();
			return true;
		}
	}
	return true;
}

static Uc_call_expression* cls_function_call(
		Uc_expression* ths, Uc_class* curcls, char* nm, bool original,
		Uc_array_expression* parms) {
	Uc_symbol* sym = nullptr;
	// Check class methods first.
	if (!ths && curcls) {
		sym = curcls->get_scope()->search(nm);
	} else if (ths && ths->is_class()) {
		sym = ths->get_cls()->get_scope()->search(nm);
	}

	// Search for defined functions.
	if (!sym) {
		sym = cur_fun->search_up(nm);
		if (original && !ths) {
			ths = new Uc_item_expression();
		}
		if (!Uc_is_valid_calle(sym, ths, nm, original)) {
			return nullptr;
		}
	}

	// Check for intrinsic name.
	if (!sym) {
		string iname = string("UI_") + nm;
		sym          = cur_fun->search_up(iname.c_str());
		// Treat as method call on 'item'.
		if (sym && !ths) {
			ths = new Uc_item_expression();
		}
	}

	// UI_UNKNOWN_XX
	constexpr const string_view uiUnk("UI_UNKNOWN_");
	if (!sym) {
		const string_view name(nm);
		if (name.compare(0, uiUnk.size(), uiUnk) == 0
			&& name.size() > uiUnk.size() && name.size() <= uiUnk.size() + 4) {
			string num(name.substr(uiUnk.size()));
			try {
				auto val = std::stoul(num, nullptr, 16);
				sym      = Uc_function::get_intrinsic(val);
			} catch (std::exception&) {
				sym = nullptr;
			}
		}
	}

	if (!sym) {
		Uc_location::yyerror("'%s' not declared", nm);
		return nullptr;
	} else {
		auto* ret = new Uc_call_expression(sym, parms, cur_fun, original);
		ret->set_itemref(ths);
		ret->check_params();
		return ret;
	}
}
