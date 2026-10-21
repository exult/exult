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
static UsecodeOps Integer_literal_opcode(int value, UsecodeOps opcode, Uc_location loc);
static Uc_array_expression* Create_array(int, Uc_expression*);
static Uc_array_expression* Create_array(int, Uc_expression*, Uc_expression*);
static Uc_class*            Find_class(const char* nm, Uc_location loc);
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

extern std::vector<int> save_lex_state();
extern void restore_lex_state(const std::vector<int>&);
extern void reset_lex_state();

static void Reset_declaration_context(Uc_class* cls = nullptr, Uc_struct_symbol* structure = nullptr) {
	cur_fun = nullptr;
	cur_class = cls;
	cur_struct = structure;
	class_type = nullptr;
	struct_type = nullptr;
	has_ret = is_extern = false;
	enum_val = -1;
	repeat_nesting = converse = 0;
	const_opcode.clear();
	nested_if.clear();
	reset_lex_state();
}

// Each statement list is a recovery boundary. Save the enclosing context so
// that discarding a partial statement also discards its unfinished scopes,
// lexer modes, constant types, and conversation/script nesting.
class Uc_recovery_block : public Uc_block_statement {
	Uc_scope* scope = cur_fun->get_scope();
	std::vector<int> lex_state = save_lex_state();
	std::vector<UsecodeOps> saved_const_opcode = const_opcode;
	std::vector<bool> saved_nested_if = nested_if;
	Uc_class* saved_class_type = class_type;
	Uc_struct_symbol* saved_struct_type = struct_type;
	bool saved_has_ret = has_ret;
	bool saved_is_extern = is_extern;
	int saved_repeat_nesting = repeat_nesting;
	int saved_converse = converse;

public:
	void recover() const {
		cur_fun->restore_scope(scope);
		restore_lex_state(lex_state);
		const_opcode = saved_const_opcode;
		nested_if = saved_nested_if;
		class_type = saved_class_type;
		struct_type = saved_struct_type;
		has_ret = saved_has_ret;
		is_extern = saved_is_extern;
		repeat_nesting = saved_repeat_nesting;
		converse = saved_converse;
	}
};

struct Fun_id_info {
	Uc_function_symbol::Function_kind kind;
	int                               id;

	Fun_id_info(Uc_function_symbol::Function_kind k, int i) : kind(k), id(i) {}
};

struct Member_selector {
	Uc_expression* expr;
	char*          name;
	Uc_location location;

	Member_selector(Uc_expression* e, char* n, Uc_location loc) : expr(e), name(n), location(loc) {}
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
%locations
%define api.location.type {Uc_parser_location}

%code requires {
#include "ucloc.h"
#include <type_traits>

// Use Bison's conventional fields for initialization and debug printing. The
// extra filename follows tokens through includes. Trivial copying allows the
// C parser skeleton to grow its stacks when compiling deeply nested input.
struct Uc_parser_location {
	int first_line, first_column, last_line, last_column;
	const char* source = nullptr;

	static Uc_parser_location current() {
		Uc_location loc;
		return {loc.get_line() + 1, loc.get_column(), loc.get_line() + 1,
				loc.get_column() + loc.get_length(), loc.get_source()};
	}

	Uc_location loc() const {
		return Uc_location(source, first_line - 1, first_column, last_column - first_column);
	}
};
static_assert(std::is_trivially_copyable<Uc_parser_location>::value);
#define YYLTYPE_IS_TRIVIAL 1

// A production inherits its first token's source location. Keep filenames in
// the location too, since one parse can span several included files.
#define YYLLOC_DEFAULT(Current, Rhs, N) \
	do { (Current) = YYRHSLOC(Rhs, (N) ? 1 : 0); } while (false)
}

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
	class Uc_recovery_block *recovery;
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
%type <recovery> statement_list noncase_statement_list statement_recovery
%type <block> terminated_statement_list
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
		Uc_location::Context diagnostic_context(@$.loc());
		if (!Uc_function::add_global_function_symbol($1)) {
			delete $1;
		}
		}
	| const_int_decl
	| enum_decl
	| static_decl
	| class_definition
	| struct_definition
	| global_recovery ';'
		{ yyerrok; }
	| global_recovery '}'
		{ yyerrok; }
	;

global_recovery:
	error
		{ Reset_declaration_context(); }
	;

class_definition:
	CLASS IDENTIFIER opt_inheritance
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if ($3) {
			cur_class = new Uc_class($2, $3);
		} else {
			cur_class = new Uc_class($2);
		}
		units.push_back(cur_class);
		}
		class_body
		{
		Uc_location::Context diagnostic_context(@$.loc());
		// Add to 'globals' symbol table.
		Uc_class_symbol::create($2, cur_class);
		cur_class = nullptr;
		}
	;

class_body:
	'{' class_item_list '}'
	| '{' class_item_list class_recovery '}'
		{ yyerrok; }
	;

class_recovery:
	error
		{ Reset_declaration_context(cur_class); }
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_class->add_alias($3, $5);
		}
	| STRUCT '<' defined_struct '>' { struct_type = $3; } class_struct_def
		{ struct_type = nullptr; }
	| STRUCT '<' defined_struct '>' alias_tok IDENTIFIER '=' declared_var ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_class->add_alias($6, $8, $3);
		}
	| CLASS '<' defined_class '>' { class_type = $3; } method
		{ class_type = nullptr; }
	| opt_void method
	| class_recovery ';'
		{ yyerrok; }
	;

class_var_def:
	var_decl_list declaration_end
	| method
	;

class_struct_def:
	struct_decl_list declaration_end
	| method
	;

method:
	IDENTIFIER '(' opt_param_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		} else {
			funsym->set_ret_type(false);
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
		Uc_location::Context diagnostic_context(@$.loc());
		cur_struct = new Uc_struct_symbol($2);
		Uc_function::add_global_struct_symbol(cur_struct);
		}
		struct_body
		{
		Uc_location::Context diagnostic_context(@$.loc());
		// Add to 'globals' symbol table.
		cur_struct = nullptr;
		}
	;

struct_body:
	'{' struct_item_list '}'
	| '{' struct_item_list struct_recovery '}'
		{ yyerrok; }
	;

struct_recovery:
	error
		{ Reset_declaration_context(nullptr, cur_struct); }
	;

struct_item_list:
	struct_item_list struct_item
	| %empty
	;

struct_item:
	VAR IDENTIFIER declaration_end
		{
		Uc_location::Context diagnostic_context(@2.loc());
		cur_struct->add($2);
		}
	| STRUCT '<' defined_struct '>' declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_struct->merge_struct($3);
		}
	| STRUCT '<' defined_struct '>' IDENTIFIER declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_struct->merge_struct($3);
		}
	| struct_recovery ';'
		{ yyerrok; }
	;

function:
	function_proto
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun = new Uc_function($1);
		units.push_back(cur_fun);
		}
	function_body
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun = nullptr;
		}
	;

opt_trailing_label:
	label_statement
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_fun->has_ret()) {
			Uc_location::yyerror(
					"label '%s' at the end of value-returning function '%s' needs a following statement",
					$1->get_label().c_str(), cur_fun->get_name());
			$$ = nullptr;
		}
		}
	| %empty
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = nullptr;
		}
	;

function_body:
	'{' terminated_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->set_statement($2);
		}
	;

					/* opt_const_int_val assigns function #. */
function_proto:
	ret_type IDENTIFIER {
		Uc_location::Context diagnostic_context(@$.loc());
		start_fun_id();
		} opt_funid '(' opt_param_list ')'
		{
		Uc_location::Context diagnostic_context(@2.loc());
		end_fun_id();
		if ($4->kind != Uc_function_symbol::utility_fun) {
			if (has_ret || struct_type) {
				Uc_location::yyerror(
					"functions declared with '%s#' cannot return a value",
					$4->kind == Uc_function_symbol::shape_fun ? "shape"
																  : "object");
			}
			if (!$6->empty()) {
				Uc_location::yyerror(
						"functions declared with '%s#' cannot have parameters",
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
		} else {
			$$->set_ret_type(false);
		}
		delete $6;    // A copy was made.
		delete $4;
		has_ret     = false;
		struct_type = nullptr;
		}
	| CLASS '<' defined_class '>' IDENTIFIER opt_const_int_val '(' opt_param_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Fun_id_info(Uc_function_symbol::shape_fun, $3 < 0 ? -1 : $3);
		if ($3 < 0) {
			Uc_location::yyerror("shape number cannot be negative");
		}
		}
	| OBJECTNUM '(' opt_const_int_expr ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Fun_id_info(Uc_function_symbol::object_fun, $3);
		}
	| opt_const_int_val
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Fun_id_info(Uc_function_symbol::utility_fun, $1);
		}
	| IDNUM '(' const_int_expr ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Fun_id_info(Uc_function_symbol::utility_fun, $3);
		}
	;

opt_const_int_expr:
	const_int_expr
	| %empty
		{ $$ = -1; }
	;

const_int_expr:
	const_int_val
	| const_int_expr '+' const_int_expr
		{ $$ = usecode_int32(static_cast<uint32_t>($1) + static_cast<uint32_t>($3)); }
	| const_int_expr '-' const_int_expr
		{ $$ = usecode_int32(static_cast<uint32_t>($1) - static_cast<uint32_t>($3)); }
	| const_int_expr '*' const_int_expr
		{ $$ = usecode_int32(static_cast<uint32_t>($1) * static_cast<uint32_t>($3)); }
	| const_int_expr '/' const_int_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if ($3 == 0) {
			@3.loc().error("division by zero in constant expression");
			$$ = -1;
		} else if ($1 == INT32_MIN && $3 == -1) {
			Uc_location::yyerror("integer overflow in constant division");
			$$ = -1;
		} else {
			$$ = $1 / $3;
		}
		}
	| const_int_expr '%' const_int_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if ($3 == 0) {
			@3.loc().error("division by zero in constant expression");
			$$ = -1;
		} else if ($1 == INT32_MIN && $3 == -1) {
			Uc_location::yyerror("integer overflow in constant remainder");
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
		Uc_location::Context diagnostic_context(@$.loc());
		Uc_symbol*           sym = Uc_function::search_globals($1);
		Uc_const_int_symbol* var;
		if (!sym) {
			@1.loc().error("use of undeclared identifier '%s'", $1);
			$$ = -1;
		} else if ((var = dynamic_cast<Uc_const_int_symbol*>(sym)) == nullptr) {
			@1.loc().error("'%s' is not an integer constant", $1);
			$$ = -1;
		} else {
			$$ = var->get_value();
		}
		}
	;

statement_block:
	statement_block_start terminated_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());	// Block ends in label.
		$$ = $2;
		nested_if.pop_back();
		cur_fun->pop_scope();
		}
	| label_statement statement
		{
		Uc_location::Context diagnostic_context(@$.loc());	// Label followed by statements; "grab" next statement for label.
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
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope();
		nested_if.push_back(false);
		}
	;

statement_list:
	statement_list statement
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if ($2) {
			$$->add($2);
		}
		}
	| statement_list statement_recovery ';'
		{ $$ = $1; yyerrok; }
	| %empty
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_recovery_block();
		}
	;

// Keep the list on the parser stack while searching for a delimiter. Otherwise
// discarding the first non-delimiter also discards the only recovery boundary.
statement_recovery:
	error
		{
		$<recovery>0->recover();
		$$ = $<recovery>0;
		}
	;

// Keep the closing brace as a synchronization point, so an error just before
// it does not eat the following function while looking for a semicolon.
terminated_statement_list:
	statement_list opt_trailing_label '}'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if ($2) {
			$1->add($2);
		}
		$$ = $1;
		}
	| statement_list statement_recovery '}'
		{ $$ = $1; yyerrok; }
	;

noncase_statement_list:
	noncase_statement_list noncase_statement
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if ($2) {
			$$->add($2);
		}
		}
	| noncase_statement_list statement_recovery ';'
		{ $$ = $1; yyerrok; }
	| %empty
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_recovery_block();
		}
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
		Uc_location::Context diagnostic_context(@$.loc());
		if (!can_break()) {
			Uc_location::yyerror("'break' statement not allowed outside of "
					"loops/converse/breakable/forever statements");
		}
		$$ = $1;
		}
	| continue_statement
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_say_statement($3);
		}
	| MESSAGE '(' opt_nonclass_expr_list ')' ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_message_statement($3);
		}
	| answer_statement
	| ENDCONV ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_endconv_statement();
		}
	| throwabort_statement ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_abort_statement();
		}
	| throwabort_statement expression ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_abort_statement($2);
		}
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
	DECLARE_ VAR IDENTIFIER declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_fun) {
			cur_fun->add_symbol($3, false);
		} else {
			cur_class->add_symbol($3);
		}
		$$ = nullptr;
		}
	| DECLARE_ STRUCT '<' defined_struct '>' { struct_type = $4; } IDENTIFIER declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_fun) {
			cur_fun->add_symbol($7, struct_type, false);
		} else {
			cur_class->add_symbol($7, struct_type);
		}
		struct_type = nullptr;
		$$          = nullptr;
		}
	| DECLARE_ CLASS '<' defined_class '>' { class_type = $4; } IDENTIFIER declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (class_type && cur_fun) {
			cur_fun->add_symbol($7, class_type, false);
		} else {
			// Unsupported for now
		}
		class_type = nullptr;
		$$         = nullptr;
		}
	| VAR var_decl_list declaration_end
		{ $$ = $2; }
	| VAR alias_tok IDENTIFIER '=' declared_var declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->add_alias($3, $5); $$ = nullptr;
		}
	| STRUCT '<' defined_struct '>' { struct_type = $3; } struct_decl_list declaration_end
		{ struct_type = nullptr; $$ = $6; }
	| STRUCT '<' defined_struct '>' alias_tok IDENTIFIER '=' declared_var declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->add_alias($6, $8, $3); $$ = nullptr;
		}
	| CLASS '<' defined_class '>' { class_type = $3; } class_decl_list declaration_end
		{ class_type = nullptr; $$ = $6; }
	| CLASS '<' defined_class '>' alias_tok IDENTIFIER '=' declared_var declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (!$8->get_cls()) {
			Uc_location::yyerror("cannot convert a 'var' value to a class type");
		} else if (!Incompatible_classes_error($8->get_cls(), $3)) {
			// Alias may be of different (compatible) class.
			cur_fun->add_alias($6, $8, $3);
		}
		$$ = nullptr;
		}
	| STRING string_decl_list opt_comma declaration_end
		{ $$ = nullptr; }
	| const_int_decl
		{ $$ = nullptr; }
	| enum_decl
		{ $$ = nullptr; }
	| function_decl
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
			opt_enum_type '{' enum_item_list opt_comma '}' declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		const_opcode.pop_back();
		}
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		const_opcode.push_back(static_cast<UsecodeOps>($2));
		}
	| %empty
		{
		Uc_location::Context diagnostic_context(@$.loc());
		const_opcode.push_back(UC_PUSHI);
		}
	;

enum_item_list:
	enum_item_list ',' enum_item
	| enum_item
	;

enum_item:
	const_int
	| IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());			/* Increment last value.	*/
		enum_val = usecode_int32(static_cast<uint32_t>(enum_val) + 1);
		int op = const_opcode.back();
		if (cur_fun) {
			cur_fun->add_int_const_symbol($1, enum_val, op);
		} else {    // Global.
			Uc_function::add_global_int_const_symbol($1, enum_val, op);
		}
		}
	;

const_int_decl:
	UCC_CONST const_int_type {
		Uc_location::Context diagnostic_context(@$.loc());
		const_opcode.push_back(static_cast<UsecodeOps>($2));
		}
		const_int_decl_list opt_comma declaration_end
		{
		Uc_location::Context diagnostic_context(@$.loc());
		const_opcode.pop_back();
		}
	;

const_int_decl_list:
	const_int_decl_list ',' const_int
	| const_int
	;

const_int:
	IDENTIFIER '=' const_int_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_fun) {
			cur_fun->add_symbol($1, true);
		} else {
			cur_class->add_symbol($1);
		}
		$$ = nullptr;
		}
	| IDENTIFIER '=' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_class && !cur_fun) {
			Uc_location::yyerror(
					"class member '%s' must be initialized through the constructor",
					$1);
			$$ = nullptr;
		} else {
			auto* var = cur_fun ? cur_fun->add_symbol($1, true)
								: cur_class->add_symbol($1);
			if (!var) { YYERROR; }
			var->set_is_obj_fun($3->is_object_function(false));
			$$ = new Uc_assignment_statement(new Uc_var_expression(var), $3);
		}
		}
	| IDENTIFIER '=' script_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_class && !cur_fun) {
			Uc_location::yyerror(
					"class member '%s' must be initialized through the constructor",
					$1);
			$$ = nullptr;
		} else {
			auto* var = cur_fun ? cur_fun->add_symbol($1, true)
								: cur_class->add_symbol($1);
			if (!var) { YYERROR; }
			$$ = new Uc_assignment_statement(new Uc_var_expression(var), $3);
		}
		}
	;

class_decl_list:
	class_decl_list ',' class_decl
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		if (class_type && cur_fun) {
			cur_fun->add_symbol($1, class_type, true);
		} else {
			// Unsupported for now
		}
		$$ = nullptr;
		}
	| IDENTIFIER '=' class_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (!class_type || !$3 || Nonclass_unexpected_error($3)) {
			$$ = nullptr;
		} else {
			Uc_class* src = $3->get_cls();
			if (Incompatible_classes_error(src, class_type)) {
				$$ = nullptr;
			} else {
				auto* v = cur_fun->add_symbol($1, class_type, true);
				if (!v) { YYERROR; }
				$$      = new Uc_assignment_statement(
				                        new Uc_class_expression(v), $3);
			}
		}
		}
	;

struct_decl_list:
	struct_decl_list ',' struct_decl
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_fun) {
			cur_fun->add_symbol($1, struct_type, true);
		} else {
			cur_class->add_symbol($1, struct_type);
		}
		$$ = nullptr;
		}
	| IDENTIFIER '=' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_class && !cur_fun) {
			Uc_location::yyerror(
					"class member '%s' must be initialized through the constructor",
					$1);
			$$ = nullptr;
		} else {
			auto* var = cur_fun ? cur_fun->add_symbol($1, struct_type, true)
								: cur_class->add_symbol($1, struct_type);
			if (!var) { YYERROR; }
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
		Uc_location::Context diagnostic_context(@$.loc());
		Uc_symbol* sym = cur_fun->search_up($1);
		if (!sym) {
			Uc_location::yyerror("use of undeclared identifier '%s'", $1);
			auto* placeholder = cur_fun->add_symbol($1, false);
			placeholder->error_placeholder = true;
			YYERROR;
		} else if (sym->get_sym_type() != Uc_symbol::Class) {
			Uc_location::yyerror("'%s' is not a class instance", $1);
			YYERROR;
		} else {
			// Tests above guarantee this will always work.
			auto* cls = dynamic_cast<Uc_class_inst_symbol*>(sym->get_sym());
			$$        = new Uc_class_expression(cls);
		}
		}
	| function_call
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = $1;
		}
	;

static_decl:
	STATIC_ VAR static_var_decl_list declaration_end
	| STATIC_ STRUCT '<' defined_struct '>'
		{ struct_type = $4; }
		static_struct_var_decl_list declaration_end
		{ struct_type = nullptr; }
	| STATIC_ CLASS '<' defined_class '>'
		{ class_type = $4; }
		static_cls_decl_list declaration_end
		{ class_type = nullptr; }
	;

static_var_decl_list:
	static_var
	| static_var_decl_list ',' static_var
	;

static_var:
	IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->add_string_symbol($1, $3);
		}
	;

// Only use this after a complete declaration; an error-only empty statement
// would accept the same bad lookahead repeatedly without consuming input.
declaration_end:
	';'
	| error
		{ yyerrok; }
	;

function_decl:
	EXTERN { is_extern = true; } function_proto declaration_end
		{ $$ = $3; is_extern = false; }
	;

assignment_statement:
	expression '=' expression ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		if (Class_unexpected_error($1)) {
			$$ = nullptr;
		} else {
			$$ = new Uc_assignment_statement($1, $3);
		}
		}
	| nonclass_expr assignment_operator nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$1->set_is_obj_fun(-1);
		$$ = new Uc_assignment_statement(
				$1,
				new Uc_binary_expression(static_cast<UsecodeOps>($2), $1, $3));
		}
	| nonclass_expr UCC_INSERT appended_element_list ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$->add($3);
		}
	| appended_element
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_array_expression();
		$$->add($1);
		}
	;

appended_element:
	nonclass_expr
	| '{' {
		Uc_location::Context diagnostic_context(@$.loc());
		start_script();
		} script_command_list '}'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = $3;
		end_script();
		}
	;

scoped_statement:
	statement_block_start terminated_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->pop_scope();
		nested_if.pop_back();
		$$ = $2;
		}
	| {
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope(); nested_if.push_back(false);
		} simple_statement
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (Uc_location::get_strict_mode()) {
			Uc_location::yyerror("expected '{' before statement in strict-braces mode");
		}
		cur_fun->pop_scope();
		nested_if.pop_back();
		$$ = $2;
		}
	| {
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope(); nested_if.push_back(false);
		} if_statement
		{
		Uc_location::Context diagnostic_context(@$.loc());
		nested_if.pop_back();
		if (Uc_location::get_strict_mode() && (nested_if.empty() || !nested_if.back())) {
			Uc_location::yyerror("expected '{' before statement in strict-braces mode");
		}
		cur_fun->pop_scope();
		$$ = $2;
		}
	;

if_statement:
	IF '(' expression ')' scoped_statement %prec IF
		{
		Uc_location::Context diagnostic_context(@$.loc());
		int val;
		if ($3->eval_const(val)) {
			if (val) {
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning(Uc_location::Warning::constant_condition, "'if' condition is always true");
				}
				$$ = $5;
			} else {    // Need this because of those pesky GOTOs...
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning(Uc_location::Warning::constant_condition, "'if' condition is always false");
				}
				$$ = new Uc_if_statement(nullptr, $5, nullptr);
			}
			delete $3;
		} else {
			$$ = new Uc_if_statement($3, $5, nullptr);
		}
		}
	| IF '(' expression ')' scoped_statement
		ELSE {
		Uc_location::Context diagnostic_context(@$.loc());
		nested_if.push_back(true);
		} scoped_statement
		{
		Uc_location::Context diagnostic_context(@$.loc());
		int val;
		if ($3->eval_const(val)) {
			if (val) {
				// Need this because of those pesky GOTOs...
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning(Uc_location::Warning::constant_condition, "'else' branch is unreachable because the condition is always true");
				}
				$$ = new Uc_if_statement(
						new Uc_int_expression(val != 0), $5, $8);
			} else {
				// Need this because of those pesky GOTOs...
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning(Uc_location::Warning::constant_condition, "'if' condition is always false");
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
	trystart_statement '{' terminated_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		auto* stmt = dynamic_cast<Uc_trycatch_statement*>($1);
		if (!stmt) {
			Uc_location::yyerror("internal compiler error: invalid try/catch statement");
		} else {
			stmt->set_catch_statement($3);
		}
		cur_fun->pop_scope();
		$$ = stmt;
		}
	;

trystart_statement:
	try_statement terminated_statement_list CATCH '(' ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->pop_scope();
		cur_fun->push_scope();
		$$ = new Uc_trycatch_statement($2);
		}
	| try_statement terminated_statement_list CATCH '(' IDENTIFIER ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->pop_scope();
		cur_fun->push_scope();
		auto* stmt = new Uc_trycatch_statement($2);
		stmt->set_catch_variable(cur_fun->add_symbol($5, true));
		$$ = stmt;
		}
	;

try_statement:
	TRY '{'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
	WHILE '(' nonclass_expr ')' {
		Uc_location::Context diagnostic_context(@$.loc());
		start_loop();
		} scoped_statement {
		Uc_location::Context diagnostic_context(@$.loc());
		end_loop();
		} opt_nobreak
		{
		Uc_location::Context diagnostic_context(@$.loc());
		int val;
		if ($3->eval_const(val)) {
			if (val) {
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning(Uc_location::Warning::constant_condition, "loop condition is always true");
				}
				$$ = new Uc_infinite_loop_statement($6, $8);
			} else {    // Need 'may' because of those pesky GOTOs...
				if (dynamic_cast<Uc_bool_expression*>($3) == nullptr) {
					$3->warning(Uc_location::Warning::constant_condition, "'while' condition is always false");
				}
				$$ = new Uc_while_statement(nullptr, $6, $8);
			}
			delete $3;
		} else {
			$$ = new Uc_while_statement($3, $6, $8);
		}
		}
	| FOREVER {
		Uc_location::Context diagnostic_context(@$.loc());
		start_loop();
		} scoped_statement {
		Uc_location::Context diagnostic_context(@$.loc());
		end_loop();
		} opt_nobreak
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_infinite_loop_statement($3, $5);
		}
	| DO {
		Uc_location::Context diagnostic_context(@$.loc());
		start_loop();
		} scoped_statement WHILE '(' nonclass_expr ')' {
		Uc_location::Context diagnostic_context(@$.loc());
		end_loop();
		} opt_nobreak_do
		{
		Uc_location::Context diagnostic_context(@$.loc());
		int val;
		if ($6->eval_const(val)) {
			if (val) {
				if (dynamic_cast<Uc_bool_expression*>($6) == nullptr) {
					$6->warning(Uc_location::Warning::constant_condition, "loop condition is always true");
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
	| BREAKABLE {
		Uc_location::Context diagnostic_context(@$.loc());
		start_loop();
		} scoped_statement {
		Uc_location::Context diagnostic_context(@$.loc());
		end_loop();
		} opt_nobreak
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_breakable_statement($3, $5);
		}
	;

array_enum_statement:
	ENUM '(' ')' ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new array_enum_statement();
		}
	;

array_loop_statement:
	start_array_variables {
		Uc_location::Context diagnostic_context(@$.loc());
		start_loop();
		} scoped_statement {
		Uc_location::Context diagnostic_context(@$.loc());
		end_loop();
		} opt_nobreak
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		Uc_var_symbol* index = Get_variable($3);
		Uc_var_symbol* length
				= cur_fun->add_symbol("$$for_loop_length_tempvar", true);
		Uc_var_symbol* var = Get_variable($1->var);
		$$                 = new Loop_Vars(var, $1->array, index, length);
		delete $1;
		}
	| start_array_loop WITH IDENTIFIER TO IDENTIFIER ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		if ($4->get_cls()) {
			Uc_location::yyerror(
					"class instance '%s' cannot be used as an array",
					$4->get_name());
		}
		$$ = new Loop_Init($2, $4);
		}
	;

start_for:
	FOR '('
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope();
		}
	;

function_call_statement:
	function_call ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_call_statement($1);
		}
	;

special_method_call_statement:
					/* Have 'primary' say something.*/
	primary hierarchy_tok SAY '(' opt_nonclass_expr_list ')' ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_call_statement(new Uc_call_expression(
				Uc_function::get_remove_face(), new Uc_array_expression($1),
				cur_fun));
		}
	| run_script_expression ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_call_statement($1);
		}
	;

run_script_expression:
	opt_primary_expression RUNSCRIPT '(' nonclass_expr opt_delay ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		if (!cur_fun->has_valid_return_type()) {
			$$ = new Uc_return_statement($2);
		} else if (!cur_fun->has_ret()) {
			Uc_location::yyerror(
					"void function '%s' cannot return a value",
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
					const std::string actual = src ? "class<" + std::string(src->get_name()) + ">" : "var";
					const std::string expected = trg ? "class<" + std::string(trg->get_name()) + ">" : "var";
					$2->error("cannot return value of type '%s' from function '%s' returning '%s'",
							actual.c_str(), cur_fun->get_name(), expected.c_str());
					cur_fun->declaration_location().note("function declared here");
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
		Uc_location::Context diagnostic_context(@$.loc());
		if (!cur_fun->has_valid_return_type()) {
			$$ = new Uc_return_statement();
		} else if (cur_fun->has_ret()) {
			Uc_class* cls = cur_fun->get_cls();
			Uc_location::yyerror(
					"function '%s' must return a value of type '%s'",
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
			noncase_statement_list response_case_list '}' {
		Uc_location::Context diagnostic_context(@$.loc());
		end_converse();
		} opt_nobreak_conv
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->pop_scope();
		--converse;
		$$ = new Uc_converse_statement(nullptr, $3, $4, false, $7);
		}
	| CONVERSE opt_nest conv_expression start_conv
			noncase_statement_list converse_case_list '}' {
		Uc_location::Context diagnostic_context(@$.loc());
		end_converse();
		} opt_nobreak_conv
		{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->pop_scope();
		--converse;
		$$ = new Uc_converse_statement($3, $5, $6, $2, $9);
		}
	| CONVERSE ATTEND IDENTIFIER ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_converse_attend_statement($3);
		}
	;

start_conv:
	'{'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		start_converse();
		cur_fun->push_scope();
		++converse;
		}
	;

conv_expression:
	'(' expression ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		if ($2) {
			$$->push_back($2);
		}
		}
	| %empty
		{ $$ = new vector<Uc_statement *>; }
	;

converse_case:
	CASE declared_var_value converse_options ':'
			{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope();
		} noncase_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		Uc_expression* expr = $2;
		if (Class_unexpected_error(expr)) {
			expr = nullptr;
		}
		$$ = new Uc_converse_variable_case_statement(
				expr, $6, ($3 ? true : false));
		cur_fun->pop_scope();
		}
	| CASE string_list converse_options ':'
			{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope();
		} noncase_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_converse_strings_case_statement(
				*$2, $6, ($3 ? true : false));
		delete $2;    // A copy was made.
		cur_fun->pop_scope();
		}
	| ALWAYS ':'
			{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope();
		} noncase_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_converse_always_case_statement($4);
		cur_fun->pop_scope();
		}
	| DEFAULT ':'
			{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope();
		} noncase_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_converse_default_case_statement($4);
		cur_fun->pop_scope();
		}
	;

opt_int_value:
	'(' int_literal ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		int ival;
		if (!$2->eval_const(ival)) {
			Uc_location::yyerror("expected an integer constant expression");
		}
		$$ = ival;
		}
	| %empty
		{ $$ = 1; }
	;

converse_case_attend:
	CASE declared_var_value converse_options ATTEND IDENTIFIER ':'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		Uc_expression* expr = $2;
		if (Class_unexpected_error(expr)) {
			expr = nullptr;
		}
		$$ = new Uc_converse_variable_case_attend_statement(
				expr, $5, ($3 ? true : false));
		}
	| CASE string_list converse_options ATTEND IDENTIFIER ':'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_converse_strings_case_attend_statement(
				*$2, $5, ($3 ? true : false));
		delete $2;    // A copy was made.
		}
	| DEFAULT opt_int_value ATTEND IDENTIFIER ':'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_converse_default_case_attend_statement($4, $2);
		}
	;

response_case_list:
	response_case_list ELSE {
		Uc_location::Context diagnostic_context(@$.loc());
		nested_if.push_back(true);
		} response_case
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if ($4) {
			$$->push_back($4);
		}
		nested_if.pop_back();
		}
	| response_case %prec IF
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new vector<Uc_statement*>;
		$$->push_back($1);
		}
	;

response_case:
	response_expression
			{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope();
		} statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_converse_strings_case_statement(*$1, $3, false);
		delete $1;    // A copy was made.
		cur_fun->pop_scope();
		}
	;

response_expression:
	IF '(' RESPONSE EQUALS string_literal ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new vector<int>;
		$$->push_back(cur_fun->add_string($5));
		}
	| IF '(' RESPONSE UCC_IN '[' string_list ']' ')'
		{ $$ = $6; }
	;

string_list:
	string_list ',' string_literal
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$->push_back(cur_fun->add_string($3));
		}
	| string_literal
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
			{
		Uc_location::Context diagnostic_context(@$.loc());
		cur_fun->push_scope();
		}
			expression ')' '{'
			{
		Uc_location::Context diagnostic_context(@$.loc());
		start_breakable();
		} switch_case_list '}'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$->push_back($2);
		}
	| switch_case
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new vector<Uc_statement*>;
		$$->push_back($1);
		}
	;

switch_case:
	CASE int_literal ':' noncase_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_switch_expression_case_statement($2, $4);
		}
	| CASE string_literal ':' noncase_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc()); $$ = new Uc_switch_expression_case_statement(
				new Uc_string_expression(cur_fun->add_string($2)), $4); }
	| DEFAULT ':' noncase_statement_list
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_switch_default_case_statement($3);
		}
	;

script_expr:
	SCRIPT {
		Uc_location::Context diagnostic_context(@$.loc());
		start_script();
		} item opt_script_delay script_command
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_call_statement($1);
		}
	;

item:					/* Any object, NPC.	*/
	nonclass_expr
	;

script_command_list:
	script_command_list script_command
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$->concat($2);
		}
	| script_command
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_array_expression();
		$$->concat($1);
		}
	;

script_command:
	FINISH ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::finish, UC_PUSHB);
		}
	| RESURRECT ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::resurrect, UC_PUSHB);
		}
	| CONTINUE ';'			/* Continue script without painting. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::cont, UC_PUSHB);
		}
	| RESET ';'			/* Go back to the beginning of the script */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::reset, UC_PUSHB);
		}
	| REPEAT repeat_count { repeat_nesting++; } script_command  ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::nop2, UC_PUSHB);
		}
	| NOP2 ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::nop1, UC_PUSHB);
		}
	| NOHALT  ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::dont_halt, UC_PUSHB);
		}
	| WAIT nonclass_expr  ';'		/* Ticks. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::delay_ticks, $2);
		}
	| WAIT nonclass_expr MINUTES  ';'	/* Game minutes. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::delay_minutes, $2);
		}
	| WAIT nonclass_expr HOURS  ';'	/* Game hours. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::delay_hours, $2);
		}
	| WAIT WHILE NEAR nonclass_expr ';'	/* Wait while avatar is near. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::wait_while_near, $4);
		}
	| WAIT WHILE FAR nonclass_expr ';'	/* Wait while avatar is far. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::wait_while_far, $4);
		}
	| REMOVE ';'			/* Remove item. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::remove, UC_PUSHB);
		}
	| RISE ';'			/* For flying barges. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::rise, UC_PUSHB);
		}
	| DESCEND ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::descend, UC_PUSHB);
		}
	| FRAME nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::frame, $2);
		}
	| ACTOR FRAME nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(
				UC_ADD, new Uc_int_expression(Ucscript::npc_frame),
				new Uc_binary_expression(UC_MOD, $3, new Uc_int_expression(16)),
				UC_PUSHB);    // Want byte.
		}
	| ACTOR FRAME actor_frames ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::npc_frame + ($3 & 15), UC_PUSHB);
		}
	| HATCH ';'			/* Assumes item is an egg. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::egg, UC_PUSHB);
		}
	| SETEGG nonclass_expr ',' nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::set_egg, $2, $4);
		}
	| SETEGG egg_criteria ',' nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::set_egg, new Uc_int_expression($2), $4);
		}
	| NEXT FRAME ';'		/* Next, but stop at last. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::next_frame_max, UC_PUSHB);
		}
	| NEXT FRAME CYCLE ';'		/* Next, or back to 0. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::next_frame, UC_PUSHB);
		}
	| PREVIOUS FRAME ';'		/* Prev. but stop at 0. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::prev_frame_min, UC_PUSHB);
		}
	| PREVIOUS FRAME CYCLE ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::prev_frame, UC_PUSHB);
		}
	| SAY nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::say, $2);
		}
	| STEP nonclass_expr ';'		/* Step in given direction (0-7). */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(
				Ucscript::step,
				new Uc_binary_expression(
						UC_ADD, $2, new Uc_int_expression(0x30), UC_PUSHB),
				new Uc_int_expression(0));
		}
	| STEP nonclass_expr ',' nonclass_expr ';'	/* Step + dz. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::step_n + $2, UC_PUSHB);
		}
	| MUSIC nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::music, $2, new Uc_int_expression(0));
		}
	| MUSIC nonclass_expr ',' nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		int ival;
		if ($1->eval_const(ival) && ival >= 0 && ival < 0x100) {
			// Matches original usecode.
			$$ = Create_array(Ucscript::usecode, $1, new Uc_int_expression(0));
		} else {
			$$ = Create_array(Ucscript::usecode, $1);
		}
		}
	| start_call ',' nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::usecode2, $1, $3);
		}
	| SPEECH nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::speech, $2);
		}
	| SFX nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::sfx, $2);
		}
	| FACE nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::face_dir, $2);
		}
	| FACE direction ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::face_dir, new Uc_int_expression($2));
		}
	| WEATHER nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::weather, $2);
		}
	| HIT nonclass_expr ',' nonclass_expr ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::hit, $2, $4);
		}
	| HIT nonclass_expr ',' dam_type ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Create_array(Ucscript::hit, $2, new Uc_int_expression($4));
		}
	| ATTACK ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression(Ucscript::attack, UC_PUSHB);
		}
	| declared_var_value ';'
	| '{' script_command_list '}'
		{ $$ = $2; }
	;

start_call:
	CALL nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (!$2) {
			$$ = new Uc_int_expression(0);
		} else {
			// May generate errors.
			if ($2->is_object_function() == -1 && Uc_location::get_shapefun_warn()) {
				// Don't know.
				Uc_location::yywarning(Uc_location::Warning::shape_to_function,
						"script call target cannot be verified as a 'shape#' or 'object#' function");
			}
			$$ = $2;
		}
		}
	;

repeat_count:
	nonclass_expr
	| FOREVER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (repeat_nesting != 0) {
			Uc_location::yyerror("'repeat forever' cannot be nested inside another 'repeat' command");
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_break_statement();
		}
	| BREAK IDENTIFIER ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_break_statement($2);
		}
	;

continue_statement:
	CONTINUE ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_continue_statement();
		}
	| CONTINUE IDENTIFIER ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_continue_statement($2);
		}
	;

fallthrough_statement:
	FALLTHROUGH ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_fallthrough_statement();
		}
	;

label_statement:
	IDENTIFIER ':'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (cur_fun->search_label($1)) {
			Uc_location::yyerror("redefinition of label '%s'", $1);
			cur_fun->label_location($1).note("previous label is here");
			$$ = new Uc_label_statement($1);
		} else {
			cur_fun->add_label($1);
			$$ = new Uc_label_statement($1);
		}
		}
	;

goto_statement:
	GOTO IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (Uc_location::get_goto_warn()) {
			@1.loc().warning(Uc_location::Warning::goto_statement, "use of 'goto' statement");
		}
		$$ = new Uc_goto_statement($2);
		}
	;

delete_statement:
	DELETE declared_var ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		auto* cls = dynamic_cast<Uc_class_inst_symbol*>($2->get_sym());
		if (!cls) {
			Uc_location::yyerror("'%s' is not a class instance", $2->get_name());
			$$ = nullptr;
		} else {
			$$ = new Uc_delete_statement(new Uc_del_expression(cls));
		}
		}
	;

answer_statement:
	ADD '(' nonclass_expr_list ')' ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_call_statement(new Uc_call_expression(
				Uc_function::get_add_answer(), $3, cur_fun));
		}
	| REMOVE '(' nonclass_expr_list ')' ';'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_call_statement(new Uc_call_expression(
				Uc_function::get_remove_answer(), $3, cur_fun));
		}
	;

opt_nonclass_expr_list:
	nonclass_expr_list
	| %empty
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_array_expression();
		}
	;

nonclass_expr_list:
	nonclass_expr_list ',' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$->add($3);
		}
	| nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_array_expression();
		$$->add($1);
		}
	;

nonclass_expr:
	expression
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (Class_unexpected_error($1)) {
			YYERROR;
		} else {
			$$ = $1;
		}
		}
	;

expression:
	primary
		{ $$ = $1; }
	| nonclass_expr '+' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_ADD, $1, $3);
		}
	| nonclass_expr '-' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_SUB, $1, $3);
		}
	| nonclass_expr '*' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_MUL, $1, $3);
		}
	| nonclass_expr '/' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_DIV, $1, $3);
		}
	| nonclass_expr '%' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_MOD, $1, $3);
		}
	| nonclass_expr EQUALS nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_CMPEQ, $1, $3);
		}
	| NEW SCRIPT {
		Uc_location::Context diagnostic_context(@$.loc());
		start_script();
		} script_command
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = $4;
		end_script();
		}
	| CHOICE
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (!converse) {
			/* Only valid in converse blocks */
			Uc_location::yyerror("'user_choice' is only valid inside a 'converse' block");
			$$ = nullptr;
		}
		$$ = new Uc_choice_expression();
		}
	| nonclass_expr NEQUALS nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_CMPNE, $1, $3);
		}
	| nonclass_expr '<' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_CMPLT, $1, $3);
		}
	| nonclass_expr LTEQUALS nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_CMPLE, $1, $3);
		}
	| nonclass_expr '>' nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_CMPGT, $1, $3);
		}
	| nonclass_expr GTEQUALS nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_CMPGE, $1, $3);
		}
	| nonclass_expr AND nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_AND, $1, $3);
		}
	| nonclass_expr OR nonclass_expr
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_OR, $1, $3);
		}
	| nonclass_expr UCC_IN nonclass_expr	/* Value in array. */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_IN, $1, $3);
		}
	| nonclass_expr '&' nonclass_expr	/* append arrays */
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_binary_expression(UC_ARRA, $1, $3);
		}
	| '+' primary %prec UPLUS
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (Class_unexpected_error($2)) {
			$$ = nullptr;
		} else {
			$$ = $2;
		}
		}
	| '-' primary %prec UMINUS
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if (Class_unexpected_error($2)) {
			$$ = nullptr;
		} else {
			$$ = new Uc_binary_expression(UC_SUB, new Uc_int_expression(0), $2);
		}
		}
	| addressof
		{
		Uc_location::Context diagnostic_context(@$.loc());
		int        funid = $1;
		UsecodeOps op    = is_int_32bit(funid) ? UC_PUSHI32 : UC_PUSHI;
		$$               = new Uc_int_expression(funid, op);
		}
	| NOT primary
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_unary_expression(UC_NOT, $2);
		}
	| '[' opt_expression_list ']'	/* Concat. into an array. */
		{ $$ = $2; }
	| string_literal
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_string_expression(cur_fun->add_string($1));
		}
	| string_prefix
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_string_prefix_expression(cur_fun, $1);
		}
	| new_expr
	| run_script_expression
		{ $$ = $1; }
	;

addressof:
	'&' IDENTIFIER %prec ADDRESSOF
		{
		Uc_location::Context diagnostic_context(@$.loc());
		// A way to retrieve the function's assigned
		// usecode number
		Uc_symbol* sym = cur_fun->search_up($2);
		if (!sym) {
			/* See if the symbol is defined */
			Uc_location::yyerror("use of undeclared identifier '%s'", $2);
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
		Uc_location::Context diagnostic_context(@$.loc());
		auto* sym = $4;
		if (sym) {
			int value = sym->search($7);
			if (value < 0) {
				Uc_location::yyerror(
						"struct '%s' has no member named '%s'",
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_array_expression();
		}
	;

expression_list:
	expression_list ',' expression
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$->add($3);
		}
	| expression
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		Uc_location::Context diagnostic_context(@$.loc());
		const auto op = Integer_literal_opcode(
				$1, !const_opcode.empty() ? const_opcode.back() : UC_PUSHI, @1.loc());
		$$ = new Uc_int_expression($1, op);
		}
	| int_cast INT_LITERAL %prec UCC_CAST
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression($2, static_cast<UsecodeOps>($1));
		}
	| member_selector
		{
		Uc_location::Context diagnostic_context(@$.loc());
		auto*             expr = dynamic_cast<Uc_var_expression*>($1->expr);
		Uc_struct_symbol* base;
		if (!expr || !(base = expr->get_struct())) {
			Uc_location::yyerror("member access requires a struct value");
			$$ = new Uc_int_expression(0);
		} else {
			int offset = base->search($1->name);
			if (offset < 0) {
				$1->location.error(
						"no member named '%s' in struct '%s'",
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
		Uc_location::Context diagnostic_context(@$.loc());
		if ($1->get_cls()) {
			Uc_location::yyerror(
					"class instance '%s' cannot be used as an array",
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_flag_expression($3);
		}
	| function_call
		{
		Uc_location::Context diagnostic_context(@$.loc());
		if ($1) {
			$$ = $1;
		} else {
			$$ = new Uc_int_expression(0);
		}
		}
	| UCTRUE
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_bool_expression(true);
		}
	| UCFALSE
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_bool_expression(false);
		}
	| EVENT
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_event_expression();
		}
	| ITEM
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_item_expression();
		}
	| '(' expression ')'
		{ $$ = $2; }
	;

new_expr:
	NEW defined_class '(' opt_nonclass_expr_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Member_selector($1, $3, @3.loc());
		}
	;

opt_primary_expression:
	primary hierarchy_tok
		{ $$ = $1; }
	| %empty
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_item_expression();
		}

function_call:
	member_selector opt_original '(' opt_expression_list ')'
		{
		Uc_location::Context diagnostic_context($1->location);
		$$ = cls_function_call($1->expr, cur_class, $1->name, $2, $4);
		delete $1;
		if (!$$) { YYERROR; }
		}
	| IDENTIFIER opt_original '(' opt_expression_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = cls_function_call(nullptr, cur_class, $1, $2, $4);
		if (!$$) { YYERROR; }
		}
	| primary hierarchy_tok defined_class UCC_SCOPE IDENTIFIER '(' opt_expression_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = cls_method_call($1, $1->get_cls(), $3, $5, $7);
		if (!$$) { YYERROR; }
		}
	| defined_class UCC_SCOPE IDENTIFIER '(' opt_expression_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = cls_method_call(nullptr, cur_class, $1, $3, $5);
		if (!$$) { YYERROR; }
		}
	| primary hierarchy_tok '(' '*' primary ')' '(' opt_expression_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_call_expression($5, $8, cur_fun);
		$$->set_itemref($1);
		if (!$$) { YYERROR; }
		}
	| '(' '*' primary ')' '(' opt_expression_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_call_expression($3, $6, cur_fun);
		$$->set_itemref(nullptr);
		if (!$$) { YYERROR; }
		}
	| primary hierarchy_tok '(' '@' int_literal ')' '(' opt_expression_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		int num;
		if (!$5->eval_const(num)) {
			Uc_location::yyerror("expected an integer constant expression");
			$$ = nullptr;
		} else {
			auto* intrinsic = Uc_function::get_intrinsic(num);
			if (!intrinsic) {
				Uc_location::yyerror("intrinsic index %d is out of range", num);
				YYERROR;
			}
			$$ = new Uc_call_expression(
					intrinsic, $8, cur_fun);
			$$->set_itemref($1);
			$$->check_params();
		}
		if (!$$) { YYERROR; }
		}
	| '(' '@' int_literal ')' '(' opt_expression_list ')'
		{
		Uc_location::Context diagnostic_context(@$.loc());
		int num;
		if (!$3->eval_const(num)) {
			Uc_location::yyerror("expected an integer constant expression");
			$$ = nullptr;
		} else {
			auto* intrinsic = Uc_function::get_intrinsic(num);
			if (!intrinsic) {
				Uc_location::yyerror("intrinsic index %d is out of range", num);
				YYERROR;
			}
			$$ = new Uc_call_expression(
					intrinsic, $6, cur_fun);
			$$->check_params();
		}
		if (!$$) { YYERROR; }
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
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$1->push_back($3);
		}
	| param
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new std::vector<Uc_var_symbol *>;
		$$->push_back($1);
		}
	;

param:
	IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_var_symbol($1, 0);
		}
	| VAR IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_var_symbol($2, 0);
		}
	| STRUCT '<' defined_struct '>' IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_struct_var_symbol($5, $3, 0);
		}
	| CLASS '<' defined_class '>' IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_class_inst_symbol($5, $3, 0);
		}
	;

sign_int_literal:
	INT_LITERAL
	| '-' INT_LITERAL %prec UMINUS
		{ $$ = usecode_int32(0u - static_cast<uint32_t>($2)); }
	| '+' INT_LITERAL %prec UPLUS
		{ $$ = $2; }
	;

int_literal:				/* A const. integer value.	*/
	sign_int_literal
		{
		Uc_location::Context diagnostic_context(@$.loc());
		const auto op = Integer_literal_opcode(
				$1, !const_opcode.empty() ? const_opcode.back() : UC_PUSHI, @1.loc());
		$$ = new Uc_int_expression($1, op);
		}
	| int_cast sign_int_literal %prec UCC_CAST
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_int_expression($2, static_cast<UsecodeOps>($1));
		}
	| declared_sym
		{
		Uc_location::Context diagnostic_context(@$.loc());
		auto* sym = dynamic_cast<Uc_const_int_symbol*>($1);
		if (!sym) {
			Uc_location::yyerror(
					"'%s' is not an integer constant",
					$1->get_name());
			$$ = nullptr;
		} else {
			$$ = sym->create_expression();
		}
		}
	| UCTRUE
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_bool_expression(true);
		}
	| UCFALSE
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = new Uc_bool_expression(false);
		}
	;

opt_void:
	VOID
	| %empty
		{
		Uc_location::Context diagnostic_context(Uc_parser_location::current().loc());
		Uc_location::yywarning(Uc_location::Warning::implicit_return_type, "function has no return type; assuming 'void'");
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
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = $1->create_expression();
		if (!$$) {
			Uc_location::yyerror("'%s' cannot be used as a value here", $1->get_name());
			$$ = new Uc_int_expression(0);
		}
		static_cast<Uc_location&>(*$$) = @1.loc();
		}
	;

declared_var:
	declared_sym %prec UCC_SYM
		{
		Uc_location::Context diagnostic_context(@$.loc());
		auto* var = dynamic_cast<Uc_var_symbol*>($1);
		if (!var) {
			Uc_location::yyerror("'%s' is not a variable", $1->get_name());
			$1->note("declared here");
			YYERROR;
		}
		$$ = var;
		}
	;

declared_sym:
	IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		Uc_symbol* sym = cur_fun->search_up($1);
		if (!sym) {
			if (auto* suggestion = cur_fun->suggest_symbol($1)) {
				@1.loc().error("use of undeclared identifier '%s'; did you mean '%s'?", $1, suggestion->get_name());
				suggestion->note("'%s' declared here", suggestion->get_name());
			} else { @1.loc().error("use of undeclared identifier '%s'", $1); }
			sym = cur_fun->add_symbol($1, false);
			sym->error_placeholder = true;
		}
		$$ = sym;
		}
	;

defined_class:
	IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		$$ = Find_class($1, @1.loc());
		}
	;

defined_struct:
	IDENTIFIER
		{
		Uc_location::Context diagnostic_context(@$.loc());
		auto* sym = dynamic_cast<Uc_struct_symbol*>(
				Uc_function::search_globals($1));
		if (!sym) {
			@1.loc().error("'%s' does not name a struct type", $1);
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

void yyerror(const char* s) {
	extern char* yytext;
	extern int   yyleng;
	std::string  message(s);
	for (const auto description : {"unexpected identifier", "unexpected integer literal"}) {
		const auto position = message.find(description);
		if (position != std::string::npos && yyleng > 0) {
			message.insert(position + strlen(description), " '" + std::string(yytext, yyleng) + "'");
			break;
		}
	}
	const auto invalid = message.find("unexpected invalid token");
	if (invalid != std::string::npos && yyleng == 1) {
		message.replace(invalid, strlen("unexpected invalid token"), "unexpected character '" + std::string(yytext, 1) + "'");
	}
	yylloc.loc().error("%s", message.c_str());
}

// Warning switches must never affect the width of the emitted operand.
static UsecodeOps Integer_literal_opcode(int value, UsecodeOps opcode, Uc_location loc) {
	if (opcode == UC_PUSHI32 || !is_sint_32bit(value)) {
		return opcode;
	}
	const bool promote = is_int_32bit(value);
	if (Uc_location::get_integer_warn()) {
		if (promote) {
			loc.warning(Uc_location::Warning::integer_coercion,
					"integer literal %d is outside the 16-bit range; treating it as '(long)'",
					value);
		} else {
			loc.warning(Uc_location::Warning::integer_coercion,
					"integer literal %d is interpreted as signed 16-bit value %d; use '(long)' to preserve its value",
					value, static_cast<short>(value));
		}
	}
	return promote ? UC_PUSHI32 : opcode;
}

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

static Uc_class* Find_class(const char* nm, Uc_location loc) {
	auto* csym
			= dynamic_cast<Uc_class_symbol*>(Uc_function::search_globals(nm));
	if (!csym) {
		loc.error("'%s' does not name a class type", nm);
		return nullptr;
	}
	return csym->get_cls();
}

static bool Class_unexpected_error(Uc_expression* expr) {
	if (expr->is_class()) {
		Uc_location::yyerror("cannot convert a class value to 'var'");
		return true;
	}
	return false;
}

static bool Nonclass_unexpected_error(Uc_expression* expr) {
	if (!expr->is_class()) {
		Uc_location::yyerror("cannot convert a 'var' value to a class type");
		return true;
	}
	return false;
}

static bool Incompatible_classes_error(Uc_class* src, Uc_class* trg) {
	if (!src || !trg) {
		return true; // An unknown class was already diagnosed.
	}
	if (!src->is_class_compatible(trg->get_name())) {
		Uc_location::yyerror(
				"cannot convert class '%s' to class '%s'",
				src->get_name(), trg->get_name());
		return true;
	}
	return false;
}

static Uc_call_expression* cls_method_call(
		Uc_expression* ths, Uc_class* curcls, Uc_class* clsscope, char* nm,
		Uc_array_expression* parms) {
	if (!clsscope) {
		return nullptr;
	}
	if (!curcls) {
		Uc_location::yyerror("member function '%s' requires a class instance", nm);
		return nullptr;
	}

	if (Incompatible_classes_error(curcls, clsscope)) {
		return nullptr;
	}

	Uc_symbol* sym = clsscope->get_scope()->search(nm);
	if (!sym) {
		Uc_location::yyerror("no member function named '%s' in class '%s'",
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
			Uc_location::yyerror("function '%s' must be declared with 'shape#' or 'object#' for a call with an item reference", nm);
			return false;
		} else if (ths) {
			Uc_location::yyerror("'%s' is not a member of class '%s'", nm,
					ths->get_cls()->get_name());
			return false;
		}
	} else {
		if (!ths) {
			Uc_location::yywarning(Uc_location::Warning::missing_itemref,
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
		if (auto* suggestion = cur_fun->suggest_symbol(nm)) {
			Uc_location::yyerror("use of undeclared identifier '%s'; did you mean '%s'?", nm, suggestion->get_name());
			suggestion->note("'%s' declared here", suggestion->get_name());
		} else { Uc_location::yyerror("use of undeclared identifier '%s'", nm); }
		return nullptr;
	} else {
		auto* ret = new Uc_call_expression(sym, parms, cur_fun, original);
		ret->set_itemref(ths);
		ret->check_params();
		return ret;
	}
}
