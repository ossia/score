%{
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "glsl_parser.h" //For context struct
#include "glsl.parser.h" //For GLSL_STYPE and GLSL_LTYPE
#include "glsl.lexer.h" //For glsl_lex()

static void glsl_error(GLSL_LTYPE *loc, struct glsl_parse_context *c, const char *s);

#define GLSL_STACK_BUFFER_SIZE (1024*1024)
#define GLSL_STACK_BUFFER_PAYLOAD_SIZE (GLSL_STACK_BUFFER_SIZE - sizeof(intptr_t))

uint8_t *glsl_parse_alloc(struct glsl_parse_context *context, size_t size, int align)
{
	uint8_t *ret;

	if (size + align > (context->cur_buffer_end - context->cur_buffer)) {
		uint8_t *next_buffer = (uint8_t *)malloc(GLSL_STACK_BUFFER_SIZE);
		if (context->cur_buffer) {
			uint8_t **pnext = (uint8_t **)context->cur_buffer_end;
			*pnext = next_buffer;
		}
		context->cur_buffer_start = next_buffer;
		context->cur_buffer = next_buffer;
		context->cur_buffer_end = next_buffer + GLSL_STACK_BUFFER_PAYLOAD_SIZE;
		if (!context->first_buffer) {
			context->first_buffer = context->cur_buffer;
		}
		*((uint8_t **)context->cur_buffer_end) = NULL;
	}

	ret = context->cur_buffer;

	uint8_t *trunc = (uint8_t *)((~((intptr_t)align - 1)) & ((intptr_t)ret));
	if (trunc != ret) {
		ret = trunc + align;
	}
	context->cur_buffer = ret + size;
	return ret;
}

void glsl_parse_dealloc(struct glsl_parse_context *context)
{
	uint8_t *buffer = context->first_buffer;
	while (buffer) {
		uint8_t *next = *((uint8_t **)(buffer + GLSL_STACK_BUFFER_PAYLOAD_SIZE));
		free(buffer);
		buffer = next;
	}
}

static char *glsl_parse_strdup(struct glsl_parse_context *context, const char *c)
{
	int len = strlen(c);
	char *ret = (char *)glsl_parse_alloc(context, len + 1, 1);
	strcpy(ret, c);
	return ret;
}

struct glsl_node *new_glsl_node(struct glsl_parse_context *context, int code, ...)
{
	struct glsl_node *temp;
	int i;
	int n = 0;
	va_list vl;
	va_start(vl, code);
	while (1) {
		temp = va_arg(vl, struct glsl_node *);
		if (temp)
			n++;
		else
			break;
	}
	va_end(vl);
	struct glsl_node *g = (struct glsl_node *)glsl_parse_alloc(context, offsetof(struct glsl_node, children[n]), 8);
	g->code = code;
	g->child_count = n;
	va_start(vl, code);
	for (i = 0; i < n; i++) {
		temp = va_arg(vl, struct glsl_node *);
		g->children[i] = temp;
	}
	va_end(vl);
	return g;
}

static struct glsl_node *new_glsl_identifier(struct glsl_parse_context *context, const char *str)
{
	struct glsl_node *n = new_glsl_node(context, IDENTIFIER, NULL);
	if (!str)
		n->data.str = NULL;
	else
		n->data.str = glsl_parse_strdup(context, str);
	return n;
}

static struct glsl_node *new_glsl_string(struct glsl_parse_context *context, int code, const char *str)
{
	struct glsl_node *n = new_glsl_node(context, code, NULL);
	n->data.str = glsl_parse_strdup(context, str);
	return n;
}

#define scanner context->scanner //To allow the scanner to find it's context

%}

%defines

%define api.prefix {glsl_}

%define api.value.type union

%pure-parser
%parse-param { struct glsl_parse_context * context }
%lex-param { void * scanner }
%locations

%type <struct glsl_node *> translation_unit

%type <struct glsl_node *> external_declaration
%type <struct glsl_node *> function_definition
%type <struct glsl_node *> compound_statement_no_new_scope
%type <struct glsl_node *> statement
%type <struct glsl_node *> statement_list
%type <struct glsl_node *> compound_statement
%type <struct glsl_node *> simple_statement
%type <struct glsl_node *> declaration
%type <struct glsl_node *> identifier_list
%type <struct glsl_node *> init_declarator_list
%type <struct glsl_node *> single_declaration
%type <struct glsl_node *> initializer
%type <struct glsl_node *> initializer_list

%type <struct glsl_node *> expression_statement
%type <struct glsl_node *> selection_statement
%type <struct glsl_node *> switch_statement
%type <struct glsl_node *> switch_statement_list
%type <struct glsl_node *> case_label
%type <struct glsl_node *> iteration_statement
%type <struct glsl_node *> statement_no_new_scope
%type <struct glsl_node *> for_init_statement
%type <struct glsl_node *> conditionopt

%type <struct glsl_node *> condition
%type <struct glsl_node *> for_rest_statement
%type <struct glsl_node *> jump_statement
%type <struct glsl_node *> function_prototype
%type <struct glsl_node *> function_declarator
%type <struct glsl_node *> parameter_declaration
%type <struct glsl_node *> parameter_declarator
%type <struct glsl_node *> function_header
%type <struct glsl_node *> function_parameter_list
%type <struct glsl_node *> fully_specified_type
%type <struct glsl_node *> parameter_type_specifier

%type <struct glsl_node *> primary_expression
%type <struct glsl_node *> expression
%type <struct glsl_node *> assignment_expression
%type <struct glsl_node *> conditional_expression
%type <struct glsl_node *> logical_or_expression
%type <struct glsl_node *> logical_xor_expression
%type <struct glsl_node *> logical_and_expression
%type <struct glsl_node *> exclusive_or_expression
%type <struct glsl_node *> constant_expression
%type <struct glsl_node *> and_expression
%type <struct glsl_node *> equality_expression
%type <struct glsl_node *> relational_expression
%type <struct glsl_node *> shift_expression
%type <struct glsl_node *> additive_expression
%type <struct glsl_node *> multiplicative_expression
%type <struct glsl_node *> unary_expression
%type <struct glsl_node *> postfix_expression
%type <struct glsl_node *> integer_expression
%type <struct glsl_node *> inclusive_or_expression

%type <struct glsl_node *> function_call
%type <struct glsl_node *> function_call_or_method
%type <struct glsl_node *> function_call_generic
%type <struct glsl_node *> function_call_parameter_list
%type <struct glsl_node *> function_identifier

%type <struct glsl_node *> type_specifier
%type <struct glsl_node *> type_specifier_nonarray
%type <struct glsl_node *> struct_specifier
%type <struct glsl_node *> array_specifier
%type <struct glsl_node *> array_specifier_list

%type <struct glsl_node *> struct_declaration_list
%type <struct glsl_node *> struct_declaration
%type <struct glsl_node *> struct_declarator_list
%type <struct glsl_node *> struct_declarator
%type <struct glsl_node *> type_qualifier
%type <struct glsl_node *> single_type_qualifier
%type <struct glsl_node *> layout_qualifier
%type <struct glsl_node *> layout_qualifier_id_list
%type <struct glsl_node *> layout_qualifier_id

%type <struct glsl_node *> precision_qualifier
%type <struct glsl_node *> invariant_qualifier
%type <struct glsl_node *> precise_qualifier
%type <struct glsl_node *> storage_qualifier
%type <struct glsl_node *> interpolation_qualifier
%type <struct glsl_node *> type_name_list

%type <struct glsl_node *> variable_identifier
%type <struct glsl_node *> decl_identifier
%type <struct glsl_node *> block_identifier
%type <struct glsl_node *> struct_name
%type <struct glsl_node *> type_name
%type <struct glsl_node *> param_name
%type <struct glsl_node *> function_name
%type <struct glsl_node *> field_identifier
%type <struct glsl_node *> type_specifier_identifier
%type <struct glsl_node *> layout_identifier

%type <int> assignment_operator
%type <int> unary_operator

%token CONST
%token BOOL
%token FLOAT
%token DOUBLE
%token INT
%token UINT
%token BREAK
%token CONTINUE
%token DO
%token ELSE
%token FOR
%token IF
%token DISCARD
%token RETURN
%token RETURN_VALUE
%token SWITCH
%token CASE
%token DEFAULT
%token SUBROUTINE
%token BVEC2
%token BVEC3
%token BVEC4
%token IVEC2
%token IVEC3
%token IVEC4
%token UVEC2
%token UVEC3
%token UVEC4
%token VEC2
%token VEC3
%token VEC4
%token MAT2
%token MAT3
%token MAT4
%token CENTROID
%token IN
%token OUT
%token INOUT
%token UNIFORM
%token PATCH
%token SAMPLE
%token BUFFER
%token SHARED
%token COHERENT
%token VOLATILE
%token RESTRICT
%token READONLY
%token WRITEONLY
%token DVEC2
%token DVEC3
%token DVEC4
%token DMAT2
%token DMAT3
%token DMAT4
%token NOPERSPECTIVE
%token FLAT
%token SMOOTH
%token LAYOUT
%token MAT2X2
%token MAT2X3
%token MAT2X4
%token MAT3X2
%token MAT3X3
%token MAT3X4
%token MAT4X2
%token MAT4X3
%token MAT4X4
%token DMAT2X2
%token DMAT2X3
%token DMAT2X4
%token DMAT3X2
%token DMAT3X3
%token DMAT3X4
%token DMAT4X2
%token DMAT4X3
%token DMAT4X4
%token ATOMIC_UINT
%token SAMPLER1D
%token SAMPLER2D
%token SAMPLER3D
%token SAMPLERCUBE
%token SAMPLER1DSHADOW
%token SAMPLER2DSHADOW
%token SAMPLERCUBESHADOW
%token SAMPLER1DARRAY
%token SAMPLER2DARRAY
%token SAMPLER1DARRAYSHADOW
%token SAMPLER2DARRAYSHADOW
%token ISAMPLER1D
%token ISAMPLER2D
%token ISAMPLER3D
%token ISAMPLERCUBE
%token ISAMPLER1DARRAY
%token ISAMPLER2DARRAY
%token USAMPLER1D
%token USAMPLER2D
%token USAMPLER3D
%token USAMPLERCUBE
%token USAMPLER1DARRAY
%token USAMPLER2DARRAY
%token SAMPLER2DRECT
%token SAMPLER2DRECTSHADOW
%token ISAMPLER2DRECT
%token USAMPLER2DRECT
%token SAMPLERBUFFER
%token ISAMPLERBUFFER
%token USAMPLERBUFFER
%token SAMPLERCUBEARRAY
%token SAMPLERCUBEARRAYSHADOW
%token ISAMPLERCUBEARRAY
%token USAMPLERCUBEARRAY
%token SAMPLER2DMS
%token ISAMPLER2DMS
%token USAMPLER2DMS
%token SAMPLER2DMSARRAY
%token ISAMPLER2DMSARRAY
%token USAMPLER2DMSARRAY
%token IMAGE1D
%token IIMAGE1D
%token UIMAGE1D
%token IMAGE2D
%token IIMAGE2D
%token UIMAGE2D
%token IMAGE3D
%token IIMAGE3D
%token UIMAGE3D
%token IMAGE2DRECT
%token IIMAGE2DRECT
%token UIMAGE2DRECT
%token IMAGECUBE
%token IIMAGECUBE
%token UIMAGECUBE
%token IMAGEBUFFER
%token IIMAGEBUFFER
%token UIMAGEBUFFER
%token IMAGE1DARRAY
%token IIMAGE1DARRAY
%token UIMAGE1DARRAY
%token IMAGE2DARRAY
%token IIMAGE2DARRAY
%token UIMAGE2DARRAY
%token IMAGECUBEARRAY
%token IIMAGECUBEARRAY
%token UIMAGECUBEARRAY
%token IMAGE2DMS
%token IIMAGE2DMS
%token UIMAGE2DMS
%token IMAGE2DMSARRAY
%token IIMAGE2DMSARRAY
%token UIMAGE2DMSARRAY
%token STRUCT
%token VOID
%token WHILE
%token <char *> IDENTIFIER
%token <float> FLOATCONSTANT
%token <double> DOUBLECONSTANT
%token <int> INTCONSTANT
%token <unsigned int> UINTCONSTANT
%token TRUE_VALUE
%token FALSE_VALUE
%token LEFT_OP
%token RIGHT_OP
%token INC_OP
%token DEC_OP
%token LE_OP
%token GE_OP
%token EQ_OP
%token NE_OP
%token AND_OP
%token OR_OP
%token XOR_OP
%token MUL_ASSIGN
%token DIV_ASSIGN
%token ADD_ASSIGN
%token MOD_ASSIGN
%token LEFT_ASSIGN
%token RIGHT_ASSIGN
%token AND_ASSIGN
%token XOR_ASSIGN
%token OR_ASSIGN
%token SUB_ASSIGN
%token LEFT_PAREN
%token RIGHT_PAREN
%token LEFT_BRACKET
%token RIGHT_BRACKET
%token LEFT_BRACE
%token RIGHT_BRACE
%token DOT
%token COMMA
%token COLON
%token EQUAL
%token SEMICOLON
%token BANG
%token DASH
%token TILDE
%token PLUS
%token STAR
%token SLASH
%token PERCENT
%token LEFT_ANGLE
%token RIGHT_ANGLE
%token VERTICAL_BAR
%token CARET
%token AMPERSAND
%token QUESTION
%token INVARIANT
%token PRECISE
%token HIGHP
%token MEDIUMP
%token LOWP
%token PRECISION
%token AT

%token UNARY_PLUS
%token UNARY_DASH
%token PRE_INC_OP
%token PRE_DEC_OP
%token POST_DEC_OP
%token POST_INC_OP
%token ARRAY_REF_OP
%token FUNCTION_CALL
%token TYPE_NAME_LIST
%token TYPE_SPECIFIER
%token POSTFIX_EXPRESSION
%token TYPE_QUALIFIER_LIST
%token STRUCT_DECLARATION
%token STRUCT_DECLARATOR
%token STRUCT_SPECIFIER
%token FUNCTION_DEFINITION
%token DECLARATION
%token STATEMENT_LIST
%token TRANSLATION_UNIT
%token PRECISION_DECLARATION
%token BLOCK_DECLARATION
%token TYPE_QUALIFIER_DECLARATION
%token IDENTIFIER_LIST
%token INIT_DECLARATOR_LIST
%token FULLY_SPECIFIED_TYPE
%token SINGLE_DECLARATION
%token SINGLE_INIT_DECLARATION
%token INITIALIZER_LIST
%token EXPRESSION_STATEMENT
%token SELECTION_STATEMENT
%token SELECTION_STATEMENT_ELSE
%token SWITCH_STATEMENT
%token FOR_REST_STATEMENT
%token WHILE_STATEMENT
%token DO_STATEMENT
%token FOR_STATEMENT
%token CASE_LABEL
%token CONDITION_OPT
%token ASSIGNMENT_CONDITION
%token EXPRESSION_CONDITION
%token FUNCTION_HEADER
%token FUNCTION_DECLARATION
%token FUNCTION_PARAMETER_LIST
%token PARAMETER_DECLARATION
%token PARAMETER_DECLARATOR
%token UNINITIALIZED_DECLARATION
%token ARRAY_SPECIFIER
%token ARRAY_SPECIFIER_LIST
%token STRUCT_DECLARATOR_LIST
%token FUNCTION_CALL_PARAMETER_LIST
%token STRUCT_DECLARATION_LIST
%token LAYOUT_QUALIFIER_ID
%token LAYOUT_QUALIFIER_ID_LIST
%token SUBROUTINE_TYPE
%token PAREN_EXPRESSION
%token INIT_DECLARATOR
%token INITIALIZER
%token TERNARY_EXPRESSION
%token FIELD_IDENTIFIER

%token NUM_GLSL_TOKEN

%%

root			: { context->root = new_glsl_node(context, TRANSLATION_UNIT, NULL); }
			| translation_unit { context->root = $1; }
			;

translation_unit	: external_declaration
				{ $$ = new_glsl_node(context, TRANSLATION_UNIT, $1, NULL); }

			| translation_unit external_declaration
				{ $$ = new_glsl_node(context, TRANSLATION_UNIT, $1, $2, NULL); }
			;

block_identifier	: IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

decl_identifier		: IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

struct_name		: IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

type_name		: IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

param_name		: IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

function_name		: IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

field_identifier	: IDENTIFIER { $$ = new_glsl_string(context, FIELD_IDENTIFIER, $1); }
			;

variable_identifier	: IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

layout_identifier	: IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

type_specifier_identifier : IDENTIFIER { $$ = new_glsl_identifier(context, $1); }
			;

external_declaration	: function_definition { $$ = $1; }
			| declaration { $$ = $1; }
			;

function_definition	: function_prototype compound_statement_no_new_scope
				{ $$ = new_glsl_node(context, FUNCTION_DEFINITION,
					$1,
					$2,
					NULL); }
			| function_prototype
				{ $$ = new_glsl_node(context, FUNCTION_DEFINITION,
					$1,
					new_glsl_node(context, STATEMENT_LIST, NULL),
					NULL); }
			;

compound_statement_no_new_scope : LEFT_BRACE RIGHT_BRACE { $$ = new_glsl_node(context, STATEMENT_LIST, NULL); }
			| LEFT_BRACE statement_list RIGHT_BRACE { $$ = $2; }
			;

statement		: compound_statement { $$ = $1; }
			| simple_statement { $$ = $1; }
			;

statement_list		: statement { $$ = new_glsl_node(context, STATEMENT_LIST, $1, NULL); }
			| statement_list statement { $$ = new_glsl_node(context, STATEMENT_LIST, $1, $2, NULL); }
			;

compound_statement	: LEFT_BRACE RIGHT_BRACE { $$ = new_glsl_node(context, STATEMENT_LIST, NULL); }
			| LEFT_BRACE statement_list RIGHT_BRACE { $$ = $2; }
			;

simple_statement	: declaration { $$ = $1; }
			| expression_statement { $$ = $1; }
			| selection_statement { $$ = $1; }
			| switch_statement { $$ = $1; }
			| case_label { $$= $1; }
			| iteration_statement { $$ = $1; }
			| jump_statement { $$ = $1; }
			;

declaration		: function_prototype SEMICOLON { $$ = new_glsl_node(context, DECLARATION, $1, NULL); }
			| init_declarator_list SEMICOLON { $$ = new_glsl_node(context, DECLARATION, $1, NULL); }
			| PRECISION precision_qualifier type_specifier SEMICOLON
				{ $$ = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, PRECISION_DECLARATION,
							$2,
							$3,
							NULL),
						NULL); }
			| type_qualifier block_identifier LEFT_BRACE struct_declaration_list RIGHT_BRACE SEMICOLON
				{ $$ = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, BLOCK_DECLARATION,
							$1,
							$2,
							$4,
							new_glsl_identifier(context, NULL),
							new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
							NULL),
						NULL); }
			| type_qualifier block_identifier LEFT_BRACE struct_declaration_list RIGHT_BRACE decl_identifier SEMICOLON
				{ $$ = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, BLOCK_DECLARATION,
							$1,
							$2,
							$4,
							$6,
							new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
							NULL),
						NULL); }
			| type_qualifier block_identifier LEFT_BRACE struct_declaration_list RIGHT_BRACE decl_identifier array_specifier_list SEMICOLON
				{ $$ = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, BLOCK_DECLARATION,
							$1,
							$2,
							$4,
							$6,
							$7,
							NULL),
						NULL); }
			| type_qualifier SEMICOLON
				{ $$ = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, UNINITIALIZED_DECLARATION,
							$1,
							new_glsl_identifier(context, NULL),
							NULL),
						NULL); }
			| type_qualifier type_name SEMICOLON
				{ $$ = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, UNINITIALIZED_DECLARATION,
							$1,
							$2,
							new_glsl_node(context, IDENTIFIER_LIST, NULL),
							NULL),
						NULL); }
			| type_qualifier type_name identifier_list SEMICOLON
				{ $$ = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, UNINITIALIZED_DECLARATION,
							$1,
							$2,
							$3,
							NULL),
						NULL); }
			;

identifier_list		: COMMA decl_identifier { $$ = new_glsl_node(context, IDENTIFIER_LIST, $2, NULL); }
			| identifier_list COMMA decl_identifier
				{ $$ = new_glsl_node(context, IDENTIFIER_LIST, $1, $3, NULL); }
			;

init_declarator_list	: single_declaration { $$ = new_glsl_node(context, INIT_DECLARATOR_LIST, $1, NULL); }
			| init_declarator_list COMMA decl_identifier
				{ $$ = new_glsl_node(context, INIT_DECLARATOR_LIST,
						$1,
						new_glsl_node(context, INIT_DECLARATOR,
							$3,
							new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
							NULL),
						NULL); }
			| init_declarator_list COMMA decl_identifier array_specifier_list
				{ $$ = new_glsl_node(context, INIT_DECLARATOR_LIST,
						$1,
						new_glsl_node(context, INIT_DECLARATOR,
							$3,
							$4,
							NULL),
						NULL); }
			| init_declarator_list COMMA decl_identifier array_specifier_list EQUAL initializer
				{ $$ = new_glsl_node(context, INIT_DECLARATOR_LIST,
						$1,
						new_glsl_node(context, INIT_DECLARATOR,
							$3,
							$4,
							$6,
							NULL),
						NULL); }
			| init_declarator_list COMMA decl_identifier EQUAL initializer
				{ $$ = new_glsl_node(context, INIT_DECLARATOR_LIST,
						$1,
						new_glsl_node(context, INIT_DECLARATOR,
							$3,
							new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
							$5,
							NULL),
						NULL); }
			;

single_declaration	: fully_specified_type
				{ $$ = new_glsl_node(context, SINGLE_DECLARATION,
					$1,
					new_glsl_identifier(context, NULL),
					new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
					NULL); }

			| fully_specified_type decl_identifier
				{ $$ = new_glsl_node(context, SINGLE_DECLARATION,
					$1,
					$2,
					new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
					NULL); }

			| fully_specified_type decl_identifier array_specifier_list
				{ $$ = new_glsl_node(context, SINGLE_DECLARATION, $1, $2, $3, NULL); }

			| fully_specified_type decl_identifier array_specifier_list EQUAL initializer
				{ $$ = new_glsl_node(context, SINGLE_INIT_DECLARATION, $1, $2, $3, $5, NULL); }

			| fully_specified_type decl_identifier EQUAL initializer
				{ $$ = new_glsl_node(context, SINGLE_INIT_DECLARATION,
					$1,
					$2,
					new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
					$4,
					NULL); }
			;

initializer		: assignment_expression { $$ = new_glsl_node(context, INITIALIZER, $1, NULL); }
			| LEFT_BRACE initializer_list RIGHT_BRACE { $$ = new_glsl_node(context, INITIALIZER, $2, NULL); }
			| LEFT_BRACE initializer_list COMMA RIGHT_BRACE { $$ = new_glsl_node(context, INITIALIZER, $2, NULL); }
			;

initializer_list	: initializer
				{ $$ = new_glsl_node(context, INITIALIZER_LIST, $1, NULL); }
			| initializer_list COMMA initializer
				{ $$ = new_glsl_node(context, INITIALIZER_LIST, $1, $3, NULL); }
			;

expression_statement	: SEMICOLON { $$ = new_glsl_node(context, EXPRESSION_STATEMENT, NULL); }
			| expression SEMICOLON { $$ = new_glsl_node(context, EXPRESSION_STATEMENT, $1, NULL); }
			;

selection_statement	: IF LEFT_PAREN expression RIGHT_PAREN statement
				{ $$ = new_glsl_node(context, SELECTION_STATEMENT, $3, $5, NULL); }

			| IF LEFT_PAREN expression RIGHT_PAREN statement ELSE statement
				{ $$ = new_glsl_node(context, SELECTION_STATEMENT_ELSE, $3, $5, $7, NULL); }
			;

switch_statement	: SWITCH LEFT_PAREN expression RIGHT_PAREN LEFT_BRACE switch_statement_list RIGHT_BRACE
				{ $$ = new_glsl_node(context, SWITCH_STATEMENT, $3, $6, NULL); }
			;

switch_statement_list	: { $$ = new_glsl_node(context, STATEMENT_LIST, NULL); }
			| statement_list { $$ = $1; }
			;

case_label		: CASE expression COLON { $$ = new_glsl_node(context, CASE_LABEL, $2, NULL); }
			| DEFAULT COLON { $$ = new_glsl_node(context, CASE_LABEL, NULL); }
			;

iteration_statement	: WHILE LEFT_PAREN condition RIGHT_PAREN statement_no_new_scope
				{ $$ = new_glsl_node(context, WHILE_STATEMENT, $3, $5, NULL); }

			| DO statement WHILE LEFT_PAREN expression RIGHT_PAREN SEMICOLON
				{ $$ = new_glsl_node(context, DO_STATEMENT, $2, $5, NULL); }

			| FOR LEFT_PAREN for_init_statement for_rest_statement RIGHT_PAREN statement_no_new_scope
				{ $$ = new_glsl_node(context, FOR_STATEMENT, $3, $4, $6, NULL); }
			;

statement_no_new_scope	: compound_statement_no_new_scope { $$ = $1; }
			| simple_statement { $$ = $1; }
			;

for_init_statement	: expression_statement { $$ = $1; }
			| declaration { $$ = $1; }
			;

conditionopt		: condition { $$ = new_glsl_node(context, CONDITION_OPT, $1, NULL); }
			| { $$ = new_glsl_node(context, CONDITION_OPT, NULL); }
			;

condition		: expression
				{ $$ = new_glsl_node(context, EXPRESSION_CONDITION, $1, NULL); }

			| fully_specified_type variable_identifier EQUAL initializer
				{ $$ = new_glsl_node(context, ASSIGNMENT_CONDITION, $1, $2, $4, NULL); }
			;

for_rest_statement	: conditionopt SEMICOLON
				{ $$ = new_glsl_node(context, FOR_REST_STATEMENT, $1, NULL); }

			| conditionopt SEMICOLON expression
				{ $$ = new_glsl_node(context, FOR_REST_STATEMENT, $1, $3, NULL); }
			;

jump_statement		: CONTINUE SEMICOLON
				{ $$ = new_glsl_node(context, CONTINUE, NULL); }

			| BREAK SEMICOLON
				{ $$ = new_glsl_node(context, BREAK, NULL); }

			| RETURN SEMICOLON
				{ $$ = new_glsl_node(context, RETURN, NULL); }

			| RETURN expression SEMICOLON
				{ $$ = new_glsl_node(context, RETURN_VALUE, $2, NULL); }

			| DISCARD SEMICOLON
				{ $$ = new_glsl_node(context, DISCARD, NULL); }
			;

function_prototype	: function_declarator RIGHT_PAREN { $$ = $1; }
			;

function_declarator	: function_header
				{ $$ = new_glsl_node(context, FUNCTION_DECLARATION,
					$1,
					new_glsl_node(context, FUNCTION_PARAMETER_LIST, NULL),
					NULL); }

			| function_header function_parameter_list
				{ $$ = new_glsl_node(context, FUNCTION_DECLARATION,
					$1,
					$2,
					NULL); }
			;

function_parameter_list : parameter_declaration
				{ $$ = new_glsl_node(context, FUNCTION_PARAMETER_LIST, $1, NULL); }

			| function_parameter_list COMMA parameter_declaration
				{ $$ = new_glsl_node(context, FUNCTION_PARAMETER_LIST, $1, $3, NULL); }
			;

parameter_declaration	: type_qualifier parameter_declarator
				{ $$ = new_glsl_node(context, PARAMETER_DECLARATION, $1, $2, NULL); }

			| parameter_declarator
				{ $$ = new_glsl_node(context, PARAMETER_DECLARATION,
					new_glsl_node(context, TYPE_QUALIFIER_LIST, NULL),
					$1,
					NULL); }

			| type_qualifier parameter_type_specifier
				{ $$ = new_glsl_node(context, PARAMETER_DECLARATION, $1, $2, NULL); }

			| parameter_type_specifier
				{ $$ = new_glsl_node(context, PARAMETER_DECLARATION,
					new_glsl_node(context, TYPE_QUALIFIER_LIST, NULL),
					$1,
					NULL); }
			;

parameter_declarator	: type_specifier param_name
				{ $$ = new_glsl_node(context, PARAMETER_DECLARATOR, $1, $2, NULL); }

			| type_specifier param_name array_specifier_list
				{ $$ = new_glsl_node(context, PARAMETER_DECLARATOR, $1, $2, $3, NULL);}
			;

function_header		: fully_specified_type function_name LEFT_PAREN
				{ $$ = new_glsl_node(context, FUNCTION_HEADER, $1, $2, NULL); }
			;

fully_specified_type	: type_specifier
				{ $$ = new_glsl_node(context, FULLY_SPECIFIED_TYPE,
					new_glsl_node(context, TYPE_QUALIFIER_LIST, NULL),
					$1,
					NULL); }

			| type_qualifier type_specifier
				{ $$ = new_glsl_node(context, FULLY_SPECIFIED_TYPE, $1, $2, NULL); }
			;

parameter_type_specifier : type_specifier
				{ $$ = new_glsl_node(context, PARAMETER_DECLARATOR, $1, NULL); }
			;

type_specifier		: type_specifier_nonarray
				{ $$ = new_glsl_node(context, TYPE_SPECIFIER,
					$1,
					new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
					NULL); }

			| type_specifier_nonarray array_specifier_list
				{ $$ = new_glsl_node(context, TYPE_SPECIFIER, $1, $2, NULL); }
			;

array_specifier_list	: array_specifier
				{ $$ = new_glsl_node(context, ARRAY_SPECIFIER_LIST, $1, NULL); }

			| array_specifier_list array_specifier
				{ $$ = new_glsl_node(context, ARRAY_SPECIFIER_LIST, $1, $2, NULL); }
			;

array_specifier		: LEFT_BRACKET RIGHT_BRACKET
				{ $$ = new_glsl_node(context, ARRAY_SPECIFIER, NULL); }

			| LEFT_BRACKET constant_expression RIGHT_BRACKET
				{ $$ = new_glsl_node(context, ARRAY_SPECIFIER, $2, NULL); }
			;

type_specifier_nonarray : VOID { $$ = new_glsl_node(context, VOID, NULL); }
			| FLOAT { $$ = new_glsl_node(context, FLOAT, NULL); }
			| DOUBLE { $$ = new_glsl_node(context, DOUBLE, NULL); }
			| INT { $$ = new_glsl_node(context, INT, NULL); }
			| UINT { $$ = new_glsl_node(context, UINT, NULL); }
			| BOOL { $$ = new_glsl_node(context, BOOL, NULL); }
			| VEC2 { $$ = new_glsl_node(context, VEC2, NULL); }
			| VEC3 { $$ = new_glsl_node(context, VEC3, NULL); }
			| VEC4 { $$ = new_glsl_node(context, VEC4, NULL); }
			| DVEC2 { $$ = new_glsl_node(context, DVEC2, NULL); }
			| DVEC3 { $$ = new_glsl_node(context, DVEC3, NULL); }
			| DVEC4 { $$ = new_glsl_node(context, DVEC4, NULL); }
			| BVEC2 { $$ = new_glsl_node(context, BVEC2, NULL); }
			| BVEC3 { $$ = new_glsl_node(context, BVEC3, NULL); }
			| BVEC4 { $$ = new_glsl_node(context, BVEC4, NULL); }
			| IVEC2 { $$ = new_glsl_node(context, IVEC2, NULL); }
			| IVEC3 { $$ = new_glsl_node(context, IVEC3, NULL); }
			| IVEC4 { $$ = new_glsl_node(context, IVEC4, NULL); }
			| UVEC2 { $$ = new_glsl_node(context, UVEC2, NULL); }
			| UVEC3 { $$ = new_glsl_node(context, UVEC3, NULL); }
			| UVEC4 { $$ = new_glsl_node(context, UVEC4, NULL); }
			| MAT2 { $$ = new_glsl_node(context, MAT2, NULL); }
			| MAT3 { $$ = new_glsl_node(context, MAT3, NULL); }
			| MAT4 { $$ = new_glsl_node(context, MAT4, NULL); }
			| MAT2X2 { $$ = new_glsl_node(context, MAT2X2, NULL); }
			| MAT2X3 { $$ = new_glsl_node(context, MAT2X3, NULL); }
			| MAT2X4 { $$ = new_glsl_node(context, MAT2X4, NULL); }
			| MAT3X2 { $$ = new_glsl_node(context, MAT3X2, NULL); }
			| MAT3X3 { $$ = new_glsl_node(context, MAT3X3, NULL); }
			| MAT3X4 { $$ = new_glsl_node(context, MAT3X4, NULL); }
			| MAT4X2 { $$ = new_glsl_node(context, MAT4X2, NULL); }
			| MAT4X3 { $$ = new_glsl_node(context, MAT4X3, NULL); }
			| MAT4X4 { $$ = new_glsl_node(context, MAT4X4, NULL); }
			| DMAT2 { $$ = new_glsl_node(context, DMAT2, NULL); }
			| DMAT3 { $$ = new_glsl_node(context, DMAT3, NULL); }
			| DMAT4 { $$ = new_glsl_node(context, DMAT4, NULL); }
			| DMAT2X2 { $$ = new_glsl_node(context, DMAT2X2, NULL); }
			| DMAT2X3 { $$ = new_glsl_node(context, DMAT2X3, NULL); }
			| DMAT2X4 { $$ = new_glsl_node(context, DMAT2X4, NULL); }
			| DMAT3X2 { $$ = new_glsl_node(context, DMAT3X2, NULL); }
			| DMAT3X3 { $$ = new_glsl_node(context, DMAT3X3, NULL); }
			| DMAT3X4 { $$ = new_glsl_node(context, DMAT3X4, NULL); }
			| DMAT4X2 { $$ = new_glsl_node(context, DMAT4X2, NULL); }
			| DMAT4X3 { $$ = new_glsl_node(context, DMAT4X3, NULL); }
			| DMAT4X4 { $$ = new_glsl_node(context, DMAT4X4, NULL); }
			| ATOMIC_UINT { $$ = new_glsl_node(context, UINT, NULL); }
			| SAMPLER1D { $$ = new_glsl_node(context, SAMPLER1D, NULL); }
			| SAMPLER2D { $$ = new_glsl_node(context, SAMPLER2D, NULL); }
			| SAMPLER3D { $$ = new_glsl_node(context, SAMPLER3D, NULL); }
			| SAMPLERCUBE { $$ = new_glsl_node(context, SAMPLERCUBE, NULL); }
			| SAMPLER1DSHADOW { $$ = new_glsl_node(context, SAMPLER1DSHADOW, NULL); }
			| SAMPLER2DSHADOW { $$ = new_glsl_node(context, SAMPLER2DSHADOW, NULL); }
			| SAMPLERCUBESHADOW { $$ = new_glsl_node(context, SAMPLERCUBESHADOW, NULL); }
			| SAMPLER1DARRAY { $$ = new_glsl_node(context, SAMPLER1DARRAY, NULL); }
			| SAMPLER2DARRAY { $$ = new_glsl_node(context, SAMPLER2DARRAY, NULL); }
			| SAMPLER1DARRAYSHADOW { $$ = new_glsl_node(context, SAMPLER1DARRAYSHADOW, NULL); }
			| SAMPLER2DARRAYSHADOW { $$ = new_glsl_node(context, SAMPLER2DARRAYSHADOW, NULL); }
			| SAMPLERCUBEARRAY { $$ = new_glsl_node(context, SAMPLERCUBEARRAY, NULL); }
			| SAMPLERCUBEARRAYSHADOW { $$ = new_glsl_node(context, SAMPLERCUBEARRAYSHADOW, NULL); }
			| ISAMPLER1D { $$ = new_glsl_node(context, ISAMPLER1D, NULL); }
			| ISAMPLER2D { $$ = new_glsl_node(context, ISAMPLER2D, NULL); }
			| ISAMPLER3D { $$ = new_glsl_node(context, ISAMPLER3D, NULL); }
			| ISAMPLERCUBE { $$ = new_glsl_node(context, ISAMPLERCUBE, NULL); }
			| ISAMPLER1DARRAY { $$ = new_glsl_node(context, ISAMPLER1DARRAY, NULL); }
			| ISAMPLER2DARRAY { $$ = new_glsl_node(context, ISAMPLER2DARRAY, NULL); }
			| ISAMPLERCUBEARRAY { $$ = new_glsl_node(context, ISAMPLERCUBEARRAY, NULL); }
			| USAMPLER1D { $$ = new_glsl_node(context, USAMPLER1D, NULL); }
			| USAMPLER2D { $$ = new_glsl_node(context, USAMPLER2D, NULL); }
			| USAMPLER3D { $$ = new_glsl_node(context, USAMPLER3D, NULL); }
			| USAMPLERCUBE { $$ = new_glsl_node(context, USAMPLERCUBE, NULL); }
			| USAMPLER1DARRAY { $$ = new_glsl_node(context, USAMPLER1DARRAY, NULL); }
			| USAMPLER2DARRAY { $$ = new_glsl_node(context, USAMPLER2DARRAY, NULL); }
			| USAMPLERCUBEARRAY { $$ = new_glsl_node(context, USAMPLERCUBEARRAY, NULL); }
			| SAMPLER2DRECT { $$ = new_glsl_node(context, SAMPLER2DRECT, NULL); }
			| SAMPLER2DRECTSHADOW { $$ = new_glsl_node(context, SAMPLER2DRECTSHADOW, NULL); }
			| ISAMPLER2DRECT { $$ = new_glsl_node(context, ISAMPLER2DRECT, NULL); }
			| USAMPLER2DRECT { $$ = new_glsl_node(context, USAMPLER2DRECT, NULL); }
			| SAMPLERBUFFER { $$ = new_glsl_node(context, SAMPLERBUFFER, NULL); }
			| ISAMPLERBUFFER { $$ = new_glsl_node(context, ISAMPLERBUFFER, NULL); }
			| USAMPLERBUFFER { $$ = new_glsl_node(context, USAMPLERBUFFER, NULL); }
			| SAMPLER2DMS { $$ = new_glsl_node(context, SAMPLER2DMS, NULL); }
			| ISAMPLER2DMS { $$ = new_glsl_node(context, ISAMPLER2DMS, NULL); }
			| USAMPLER2DMS { $$ = new_glsl_node(context, USAMPLER2DMS, NULL); }
			| SAMPLER2DMSARRAY { $$ = new_glsl_node(context, SAMPLER2DMSARRAY, NULL); }
			| ISAMPLER2DMSARRAY { $$ = new_glsl_node(context, ISAMPLER2DMSARRAY, NULL); }
			| USAMPLER2DMSARRAY { $$ = new_glsl_node(context, USAMPLER2DMSARRAY, NULL); }
			| IMAGE1D { $$ = new_glsl_node(context, IMAGE1D, NULL); }
			| IIMAGE1D { $$ = new_glsl_node(context, IIMAGE1D, NULL); }
			| UIMAGE1D { $$ = new_glsl_node(context, UIMAGE1D, NULL); }
			| IMAGE2D { $$ = new_glsl_node(context, IMAGE2D, NULL); }
			| IIMAGE2D { $$ = new_glsl_node(context, IIMAGE2D, NULL); }
			| UIMAGE2D { $$ = new_glsl_node(context, UIMAGE2D, NULL); }
			| IMAGE3D { $$ = new_glsl_node(context, IMAGE3D, NULL); }
			| IIMAGE3D { $$ = new_glsl_node(context, IIMAGE3D, NULL); }
			| UIMAGE3D { $$ = new_glsl_node(context, UIMAGE3D, NULL); }
			| IMAGE2DRECT { $$ = new_glsl_node(context, IMAGE2DRECT, NULL); }
			| IIMAGE2DRECT { $$ = new_glsl_node(context, IIMAGE2DRECT, NULL); }
			| UIMAGE2DRECT { $$ = new_glsl_node(context, UIMAGE2DRECT, NULL); }
			| IMAGECUBE { $$ = new_glsl_node(context, IMAGECUBE, NULL); }
			| IIMAGECUBE { $$ = new_glsl_node(context, IIMAGECUBE, NULL); }
			| UIMAGECUBE { $$ = new_glsl_node(context, UIMAGECUBE, NULL); }
			| IMAGEBUFFER { $$ = new_glsl_node(context, IMAGEBUFFER, NULL); }
			| IIMAGEBUFFER { $$ = new_glsl_node(context, IIMAGEBUFFER, NULL); }
			| UIMAGEBUFFER { $$ = new_glsl_node(context, UIMAGEBUFFER, NULL); }
			| IMAGE1DARRAY { $$ = new_glsl_node(context, IMAGE1DARRAY, NULL); }
			| IIMAGE1DARRAY { $$ = new_glsl_node(context, IIMAGE1DARRAY, NULL); }
			| UIMAGE1DARRAY { $$ = new_glsl_node(context, UIMAGE1DARRAY, NULL); }
			| IMAGE2DARRAY { $$ = new_glsl_node(context, IMAGE2DARRAY, NULL); }
			| IIMAGE2DARRAY { $$ = new_glsl_node(context, IIMAGE2DARRAY, NULL); }
			| UIMAGE2DARRAY { $$ = new_glsl_node(context, UIMAGE2DARRAY, NULL); }
			| IMAGECUBEARRAY { $$ = new_glsl_node(context, IMAGECUBEARRAY, NULL); }
			| IIMAGECUBEARRAY { $$ = new_glsl_node(context, IIMAGECUBEARRAY, NULL); }
			| UIMAGECUBEARRAY { $$ = new_glsl_node(context, UIMAGECUBEARRAY, NULL); }
			| IMAGE2DMS { $$ = new_glsl_node(context, IMAGE2DMS, NULL); }
			| IIMAGE2DMS { $$ = new_glsl_node(context, IIMAGE2DMS, NULL); }
			| UIMAGE2DMS { $$ = new_glsl_node(context, UIMAGE2DMS, NULL); }
			| IMAGE2DMSARRAY { $$ = new_glsl_node(context, IMAGE2DMSARRAY, NULL); }
			| IIMAGE2DMSARRAY { $$ = new_glsl_node(context, IIMAGE2DMSARRAY, NULL); }
			| UIMAGE2DMSARRAY { $$ = new_glsl_node(context, UIMAGE2DMSARRAY, NULL); }
			| struct_specifier { $$ = $1; }
			| type_specifier_identifier { $$ = $1; }
			;

struct_specifier	: STRUCT struct_name LEFT_BRACE struct_declaration_list RIGHT_BRACE
				{ $$ = new_glsl_node(context, STRUCT_SPECIFIER, $2, $4, NULL);}

			| STRUCT LEFT_BRACE struct_declaration_list RIGHT_BRACE
				{ $$ = new_glsl_node(context, STRUCT_SPECIFIER,
						new_glsl_identifier(context, NULL),
						$3,
						NULL); }
			;

struct_declaration_list : struct_declaration
				{ $$ = new_glsl_node(context, STRUCT_DECLARATION_LIST, $1, NULL); }
			| struct_declaration_list struct_declaration
				{ $$ = new_glsl_node(context, STRUCT_DECLARATION_LIST, $1, $2, NULL); }
			;

struct_declaration	: type_specifier struct_declarator_list SEMICOLON
				{ $$ = new_glsl_node(context, STRUCT_DECLARATION,
					new_glsl_node(context, TYPE_QUALIFIER_LIST, NULL),
					$1,
					$2,
					NULL); }

			| type_qualifier type_specifier struct_declarator_list SEMICOLON
				{ $$ = new_glsl_node(context, STRUCT_DECLARATION, $1, $2, $3, NULL); }
			;

struct_declarator_list	: struct_declarator
				{ $$ = new_glsl_node(context, STRUCT_DECLARATOR_LIST, $1, NULL); }

			| struct_declarator_list COMMA struct_declarator
				{ $$ = new_glsl_node(context, STRUCT_DECLARATOR_LIST, $1, $3, NULL); }
			;

struct_declarator	: field_identifier
				{ $$ = new_glsl_node(context, STRUCT_DECLARATOR, $1, NULL); }

			| field_identifier array_specifier_list
				{ $$ = new_glsl_node(context, STRUCT_DECLARATOR, $1, $2, NULL); }
			;

type_qualifier		: single_type_qualifier
				{ $$ = new_glsl_node(context, TYPE_QUALIFIER_LIST, $1, NULL); }
			| type_qualifier single_type_qualifier
				{ $$ = new_glsl_node(context, TYPE_QUALIFIER_LIST, $1, $2, NULL); }
			;

single_type_qualifier	: storage_qualifier { $$ = $1; }
			| layout_qualifier { $$ = $1; }
			| precision_qualifier { $$ = $1; }
			| interpolation_qualifier { $$ = $1; }
			| invariant_qualifier { $$ = $1; }
			| precise_qualifier { $$ = $1; }
			;

layout_qualifier	: LAYOUT LEFT_PAREN layout_qualifier_id_list RIGHT_PAREN { $$ = $3; }
			;

layout_qualifier_id_list: layout_qualifier_id { $$ = $1; }

			| layout_qualifier_id_list COMMA layout_qualifier_id
				{ $$ = new_glsl_node(context, LAYOUT_QUALIFIER_ID_LIST, $1, $3, NULL); }
			;

layout_qualifier_id	: layout_identifier
				{ $$ = new_glsl_node(context, LAYOUT_QUALIFIER_ID, $1, NULL); }

			| layout_identifier EQUAL constant_expression
				{ $$ = new_glsl_node(context, LAYOUT_QUALIFIER_ID, $1, $3, NULL);}

			| SHARED
				{ $$ = new_glsl_node(context, SHARED, NULL); }
			;

precision_qualifier	: HIGHP { $$ = new_glsl_node(context, HIGHP, NULL); }
			| MEDIUMP { $$ = new_glsl_node(context, MEDIUMP, NULL); }
			| LOWP { $$ = new_glsl_node(context, LOWP, NULL); }
			;

interpolation_qualifier : SMOOTH { $$ = new_glsl_node(context, SMOOTH, NULL); }
			| FLAT { $$ = new_glsl_node(context, FLAT, NULL); }
			| NOPERSPECTIVE { $$ = new_glsl_node(context, NOPERSPECTIVE, NULL); }
			;

invariant_qualifier	: INVARIANT { $$ = new_glsl_node(context, INVARIANT, NULL); }
			;

precise_qualifier	: PRECISE { $$ = new_glsl_node(context, PRECISE, NULL); }
			;

storage_qualifier	: CONST { $$ = new_glsl_node(context, CONST, NULL); }
			| INOUT { $$ = new_glsl_node(context, INOUT, NULL); }
			| IN { $$ = new_glsl_node(context, IN, NULL); }
			| OUT { $$ = new_glsl_node(context, OUT, NULL); }
			| CENTROID { $$ = new_glsl_node(context, CENTROID, NULL); }
			| PATCH { $$ = new_glsl_node(context, PATCH, NULL); }
			| SAMPLE { $$ = new_glsl_node(context, SAMPLE, NULL); }
			| UNIFORM { $$ = new_glsl_node(context, UNIFORM, NULL); }
			| BUFFER { $$ = new_glsl_node(context, BUFFER, NULL); }
			| SHARED { $$ = new_glsl_node(context, SHARED, NULL); }
			| COHERENT { $$ = new_glsl_node(context, COHERENT, NULL); }
			| VOLATILE { $$ = new_glsl_node(context, VOLATILE, NULL); }
			| RESTRICT { $$ = new_glsl_node(context, RESTRICT, NULL); }
			| READONLY { $$ = new_glsl_node(context, READONLY, NULL); }
			| WRITEONLY { $$ = new_glsl_node(context, WRITEONLY, NULL); }
			| SUBROUTINE { $$ = new_glsl_node(context, SUBROUTINE, NULL); }
			| SUBROUTINE LEFT_PAREN type_name_list RIGHT_PAREN
				{ $$ = new_glsl_node(context, SUBROUTINE_TYPE,
					new_glsl_node(context, TYPE_NAME_LIST, $3, NULL),
					NULL); }
			;

type_name_list		: type_name { $$ = $1; }
			| type_name_list COMMA type_name
				{ $$ = new_glsl_node(context, TYPE_NAME_LIST, $1, $3, NULL); }
			;

expression		: assignment_expression { $$ = $1; }
			| expression COMMA assignment_expression
				{ $$ = new_glsl_node(context, COMMA, $1, $3, NULL); }
			;

assignment_expression	: conditional_expression { $$ = $1; }
			| unary_expression assignment_operator assignment_expression
				{ $$ = new_glsl_node(context,$2, $1, $3, NULL); }
			;

assignment_operator	: EQUAL { $$ = EQUAL; }
			| MUL_ASSIGN { $$ = MUL_ASSIGN; }
			| DIV_ASSIGN { $$ = DIV_ASSIGN; }
			| MOD_ASSIGN { $$ = MOD_ASSIGN; }
			| ADD_ASSIGN { $$ = ADD_ASSIGN; }
			| SUB_ASSIGN { $$ = SUB_ASSIGN; }
			| LEFT_ASSIGN { $$ = LEFT_ASSIGN; }
			| RIGHT_ASSIGN { $$ = RIGHT_ASSIGN; }
			| AND_ASSIGN { $$ = AND_ASSIGN; }
			| XOR_ASSIGN { $$ = XOR_ASSIGN; }
			| OR_ASSIGN { $$ = OR_ASSIGN; }
			;

constant_expression	: conditional_expression { $$ = $1; }
			;

conditional_expression	: logical_or_expression { $$ = $1; }
			| logical_or_expression QUESTION expression COLON assignment_expression
				{ $$ = new_glsl_node(context, TERNARY_EXPRESSION, $1, $3, $5, NULL); }
			;

logical_or_expression	: logical_xor_expression { $$ = $1; }
			| logical_or_expression OR_OP logical_xor_expression
				{ $$ = new_glsl_node(context, OR_OP, $1, $3, NULL); }
			;

logical_xor_expression	: logical_and_expression { $$ = $1; }
			| logical_xor_expression XOR_OP logical_and_expression
				{ $$ = new_glsl_node(context, XOR_OP, $1, $3, NULL); }
			;

logical_and_expression	: inclusive_or_expression { $$ = $1; }
			| logical_and_expression AND_OP inclusive_or_expression
				{ $$ = new_glsl_node(context, AND_OP, $1, $3, NULL); }
			;

inclusive_or_expression : exclusive_or_expression { $$ = $1; }
			| inclusive_or_expression VERTICAL_BAR exclusive_or_expression
				{ $$ = new_glsl_node(context, VERTICAL_BAR, $1, $3, NULL); }
			;

exclusive_or_expression	: and_expression { $$ = $1; }
			| exclusive_or_expression CARET and_expression
				{ $$ = new_glsl_node(context, CARET, $1, $3, NULL); }
			;

and_expression		: equality_expression { $$ = $1; }
			| and_expression AMPERSAND equality_expression
				{ $$ = new_glsl_node(context, AMPERSAND, $1, $3, NULL); }
			;

equality_expression	: relational_expression { $$ = $1; }

			| equality_expression EQ_OP relational_expression
				{ $$ = new_glsl_node(context, EQ_OP, $1, $3, NULL); }

			| equality_expression NE_OP relational_expression
				{ $$ = new_glsl_node(context, NE_OP, $1, $3, NULL); }
			;

relational_expression	: shift_expression { $$ = $1; }

			| relational_expression LEFT_ANGLE shift_expression
				{ $$ = new_glsl_node(context, LEFT_ANGLE, $1, $3, NULL); }

			| relational_expression RIGHT_ANGLE shift_expression
				{ $$ = new_glsl_node(context, RIGHT_ANGLE, $1, $3, NULL); }

			| relational_expression LE_OP shift_expression
				{ $$ = new_glsl_node(context, LE_OP, $1, $3, NULL); }

			| relational_expression GE_OP shift_expression
				{ $$ = new_glsl_node(context, GE_OP, $1, $3, NULL); }
			;

shift_expression	: additive_expression { $$ = $1; }

			| shift_expression LEFT_OP additive_expression
				{ $$ = new_glsl_node(context, LEFT_OP, $1, $3, NULL); }

			| shift_expression RIGHT_OP additive_expression
				{ $$ = new_glsl_node(context, RIGHT_OP, $1, $3, NULL); }
			;

additive_expression	: multiplicative_expression { $$ = $1; }

			| additive_expression PLUS multiplicative_expression
				{ $$ = new_glsl_node(context, PLUS, $1, $3, NULL); }

			| additive_expression DASH multiplicative_expression
				{ $$ = new_glsl_node(context, DASH, $1, $3, NULL); }
			;

multiplicative_expression : unary_expression { $$ = $1; }

			| multiplicative_expression STAR unary_expression
				{ $$ = new_glsl_node(context, STAR, $1, $3, NULL); }

			| multiplicative_expression SLASH unary_expression
				{ $$ = new_glsl_node(context, SLASH, $1, $3, NULL); }

			| multiplicative_expression PERCENT unary_expression
				{ $$ = new_glsl_node(context, PERCENT, $1, $3, NULL); }
			;

unary_expression	: postfix_expression { $$ = $1; }

			| INC_OP unary_expression
				{ $$ = new_glsl_node(context, PRE_INC_OP, $2, NULL); }

			| DEC_OP unary_expression
				{ $$ = new_glsl_node(context, PRE_DEC_OP, $2, NULL); }

			| unary_operator unary_expression
				{ $$ = new_glsl_node(context,$1, $2, NULL); }
			;

unary_operator		: PLUS { $$ = UNARY_PLUS; }
			| DASH { $$ = UNARY_DASH; }
			| BANG { $$ = BANG; }
			| TILDE { $$ = TILDE; }
			;

postfix_expression	: primary_expression { $$ = $1; }

			| postfix_expression LEFT_BRACKET integer_expression RIGHT_BRACKET
				{ $$ = new_glsl_node(context, ARRAY_REF_OP, $1, $3, NULL); }

			| function_call { $$ = $1; }

			| postfix_expression DOT field_identifier
				{ $$ = new_glsl_node(context, DOT, $1, $3, NULL);}

			| postfix_expression INC_OP
				{ $$ = new_glsl_node(context, POST_INC_OP, $1, NULL); }

			| postfix_expression DEC_OP
				{ $$ = new_glsl_node(context, POST_DEC_OP, $1, NULL); }
			;

integer_expression	: expression { $$ = $1; }
			;

function_call		: function_call_or_method { $$ = $1; }
			;

function_call_or_method	: function_call_generic { $$ = $1; }
			;

function_call_generic	: function_identifier LEFT_PAREN function_call_parameter_list RIGHT_PAREN
				{ $$ = new_glsl_node(context, FUNCTION_CALL, $1, $3, NULL); }

			| function_identifier LEFT_PAREN LEFT_PAREN
				{ $$ = new_glsl_node(context, FUNCTION_CALL,
					$1,
					new_glsl_node(context, FUNCTION_CALL_PARAMETER_LIST, NULL),
					NULL); }

			| function_identifier LEFT_PAREN VOID RIGHT_PAREN
				{ $$ = new_glsl_node(context, FUNCTION_CALL,
					$1,
					new_glsl_node(context, FUNCTION_CALL_PARAMETER_LIST, NULL),
					NULL); }
			;

function_call_parameter_list : assignment_expression
				{ $$ = new_glsl_node(context, FUNCTION_CALL_PARAMETER_LIST, $1, NULL); }

			| function_call_parameter_list COMMA assignment_expression
				{ $$ = new_glsl_node(context, FUNCTION_CALL_PARAMETER_LIST, $1, $3, NULL); }
			;

function_identifier	: type_specifier { $$ = $1; }

			| postfix_expression
				{ $$ = new_glsl_node(context, POSTFIX_EXPRESSION, $1, NULL); }
			;

primary_expression	: variable_identifier { $$ = $1; }

			| INTCONSTANT
				{ $$ = new_glsl_node(context, INTCONSTANT, NULL); $$->data.i = $1; }

			| UINTCONSTANT
				{ $$ = new_glsl_node(context, UINTCONSTANT, NULL); $$->data.ui = $1; }

			| FLOATCONSTANT
				{ $$ = new_glsl_node(context, FLOATCONSTANT, NULL); $$->data.f = $1; }

			| TRUE_VALUE
				{ $$ = new_glsl_node(context, TRUE_VALUE, NULL); }

			| FALSE_VALUE
				{ $$ = new_glsl_node(context, FALSE_VALUE, NULL); }

			| DOUBLECONSTANT
				{ $$ = new_glsl_node(context, DOUBLECONSTANT, NULL); $$->data.d = $1; }

			| LEFT_PAREN expression RIGHT_PAREN
				{ $$ = new_glsl_node(context, PAREN_EXPRESSION, $2, NULL); }
			;

%%

#include "glsl_ast.h"

//The scanner macro, needed for integration with flex, causes problems below
#undef scanner

static void glsl_error(GLSL_LTYPE *loc, struct glsl_parse_context *c, const char *s)
{
	c->error = true;
	if (c->error_cb)
		c->error_cb(s, loc->first_line, loc->first_column, loc->last_column);
}

int list_length(struct glsl_node *n, int list_token)
{
	if (n->code != list_token) {
		return 1;
	} else {
		int i;
		int count = 0;
		for (i = 0; i < n->child_count; i++) {
			count += list_length(n->children[i], list_token);
		}
		return count;
	}
}

static void list_gather(struct glsl_node *n, struct glsl_node *new_list, int list_token)
{
	int i;
	for (i = 0; i < n->child_count; i++) {
		struct glsl_node *child = n->children[i];
		if (child->code != list_token)
			new_list->children[new_list->child_count++] = child;
		else
			list_gather(child, new_list, list_token);
	}
}

static void list_collapse(struct glsl_parse_context *context, struct glsl_node *n)
{
	int i;
	for (i = 0; i < n->child_count; i++) {
		struct glsl_node *child = n->children[i];
		if (glsl_ast_is_list_node(child)) {
			int list_token = child->code;
			int length = list_length(child, list_token);
			struct glsl_node *g = (struct glsl_node *)glsl_parse_alloc(context, offsetof(struct glsl_node, children[length]), 8);
			g->code = list_token;
			g->child_count = 0;
			list_gather(child, g, list_token);
			n->children[i] = g;
			child = g;
		}
		list_collapse(context, child);
	}
}

static bool parse_internal(struct glsl_parse_context *context)
{
	context->error = false;
	glsl_parse(context);
	if (context->root) {
		if (glsl_ast_is_list_node(context->root)) {
			//
			// list_collapse() can't combine all the TRANSLATION_UNIT nodes
			// since it would need to replace g_glsl_node_root so we combine
			// the TRANSLATION_UNIT nodes here.
			//
			int list_code = context->root->code;
			int length = list_length(context->root, list_code);
			struct glsl_node *new_root = (struct glsl_node *)glsl_parse_alloc(context, offsetof(struct glsl_node, children[length]), 8);
			new_root->code = TRANSLATION_UNIT;
			new_root->child_count = 0;
			list_gather(context->root, new_root, list_code);
			assert(new_root->child_count == length);
			context->root = new_root;
		}
		//
		// Collapse other list nodes
		//
		list_collapse(context, context->root);
	}
	return context->error;
}

bool glsl_parse_file(struct glsl_parse_context *context, FILE *file)
{
	glsl_lex_init(&(context->scanner));

	glsl_set_in(file, context->scanner);

	bool error;

	error = parse_internal(context);

	glsl_lex_destroy(context->scanner);
	context->scanner = NULL;
	return error;
}

bool glsl_parse_string(struct glsl_parse_context *context, const char *str)
{
	char *text;
	size_t sz;
	bool error;

	glsl_lex_init(&(context->scanner));

	sz = strlen(str);
	text = malloc(sz + 2);
	strcpy(text, str);
	text[sz + 1] = 0;
	glsl__scan_buffer(text, sz + 2, context->scanner);

	error = parse_internal(context);

	free(text);
	glsl_lex_destroy(context->scanner);
	context->scanner = NULL;
	return error;
}

void glsl_parse_context_init(struct glsl_parse_context *context)
{
	context->root = NULL;
	context->scanner = NULL;
	context->first_buffer = NULL;
	context->cur_buffer_start = NULL;
	context->cur_buffer = NULL;
	context->cur_buffer_end = NULL;
	context->error_cb = NULL;
	context->error = false;
}

void glsl_parse_set_error_cb(struct glsl_parse_context *context, glsl_parse_error_cb_t error_cb)
{
	context->error_cb = error_cb;
}


void glsl_parse_context_destroy(struct glsl_parse_context *context)
{
	glsl_parse_dealloc(context);
}
