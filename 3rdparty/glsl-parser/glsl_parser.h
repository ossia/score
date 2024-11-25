#ifndef GLSL_PARSER_H
#define GLSL_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

struct glsl_node {
	//Type of this node. These values are all members of the enum
	int code:16;

	//Number of child nodes
	int child_count:16;

	//Meta data for this node. Only uses for some nodes. The
	//field in this unions that should be read (if any)
	//is determined by 'code'
	union {
		double d;
		float f;
		int i;
		unsigned int ui;
		bool b;
		const char *str;
	} data;

	//
	// Child nodes. Extra data will be allocated past the end
	// of the structure to hold the child nodes.
	//
	struct glsl_node *children[];
};

typedef void (*glsl_parse_error_cb_t)(const char *error_str, int lineno, int start_col, int end_col);

struct glsl_parse_context {
	struct glsl_node *root;
	glsl_parse_error_cb_t error_cb;

	void *scanner; //Opaque handle to lexer context

	/* Internal state of the parser's stack allocator */
	uint8_t *first_buffer;
	uint8_t *cur_buffer_start;
	uint8_t *cur_buffer;
	uint8_t *cur_buffer_end;
	bool error;
};

//
// Create a new node for the AST. All values following 'code' will be
// placed in the node's child array.
//
struct glsl_node *new_glsl_node(struct glsl_parse_context *context, int code, ...) __attribute__ ((sentinel));

//
// Allocate memory in the parser's stack allocator
//
uint8_t *glsl_parse_alloc(struct glsl_parse_context *context, size_t size, int align);

//
// Deallocate all memory previously allocated by glsl_parse_alloc()
// The parser internally uses this allocator so any generated
// AST data will become invalid when glsl_parse_dealloc() is called.
//
void glsl_parse_dealloc(struct glsl_parse_context *context);

//
// Initialize a parsing context
//
void glsl_parse_context_init(struct glsl_parse_context *context);


//
// Set the callback to invoke when a parsing error occurs
//
void glsl_parse_set_error_cb(struct glsl_parse_context *context, glsl_parse_error_cb_t error_cb);


//
// Destroy a parsing context.
//
void glsl_parse_context_destroy(struct glsl_parse_context *context);

//
// Parse the supplied file and generate an AST in context->root.
//
// Returns false if a parsing error occured
//
bool glsl_parse_file(struct glsl_parse_context *context, FILE *file);

//
// Parse the supplied string and generate an AST in context->root.
//
// Returns false if a parsing error occured
//
bool glsl_parse_string(struct glsl_parse_context *context, const char *str);

//
// Include glsl.parse.h to get the enum values that are stored in the 'code'
// field of glsl_node.
//
#ifdef GLSL_EMPTY
#undef GLSL_EMPTY
#endif

#ifdef GLSL_EOF
#undef GLSL_EOF
#endif

#ifdef GLSL_error
#undef GLSL_error
#endif

#ifdef GLSL_UNDEF
#undef GLSL_UNDEF
#endif

#ifdef CONST
#undef CONST
#endif

#ifdef BOOL
#undef BOOL
#endif

#ifdef FLOAT
#undef FLOAT
#endif

#ifdef DOUBLE
#undef DOUBLE
#endif

#ifdef INT
#undef INT
#endif

#ifdef UINT
#undef UINT
#endif

#ifdef BREAK
#undef BREAK
#endif

#ifdef CONTINUE
#undef CONTINUE
#endif

#ifdef DO
#undef DO
#endif

#ifdef ELSE
#undef ELSE
#endif

#ifdef FOR
#undef FOR
#endif

#ifdef IF
#undef IF
#endif

#ifdef DISCARD
#undef DISCARD
#endif

#ifdef RETURN
#undef RETURN
#endif

#ifdef RETURN_VALUE
#undef RETURN_VALUE
#endif

#ifdef SWITCH
#undef SWITCH
#endif

#ifdef CASE
#undef CASE
#endif

#ifdef DEFAULT
#undef DEFAULT
#endif

#ifdef SUBROUTINE
#undef SUBROUTINE
#endif

#ifdef BVEC2
#undef BVEC2
#endif

#ifdef BVEC3
#undef BVEC3
#endif

#ifdef BVEC4
#undef BVEC4
#endif

#ifdef IVEC2
#undef IVEC2
#endif

#ifdef IVEC3
#undef IVEC3
#endif

#ifdef IVEC4
#undef IVEC4
#endif

#ifdef UVEC2
#undef UVEC2
#endif

#ifdef UVEC3
#undef UVEC3
#endif

#ifdef UVEC4
#undef UVEC4
#endif

#ifdef VEC2
#undef VEC2
#endif

#ifdef VEC3
#undef VEC3
#endif

#ifdef VEC4
#undef VEC4
#endif

#ifdef MAT2
#undef MAT2
#endif

#ifdef MAT3
#undef MAT3
#endif

#ifdef MAT4
#undef MAT4
#endif

#ifdef CENTROID
#undef CENTROID
#endif

#ifdef IN
#undef IN
#endif

#ifdef OUT
#undef OUT
#endif

#ifdef INOUT
#undef INOUT
#endif

#ifdef UNIFORM
#undef UNIFORM
#endif

#ifdef PATCH
#undef PATCH
#endif

#ifdef SAMPLE
#undef SAMPLE
#endif

#ifdef BUFFER
#undef BUFFER
#endif

#ifdef SHARED
#undef SHARED
#endif

#ifdef COHERENT
#undef COHERENT
#endif

#ifdef VOLATILE
#undef VOLATILE
#endif

#ifdef RESTRICT
#undef RESTRICT
#endif

#ifdef READONLY
#undef READONLY
#endif

#ifdef WRITEONLY
#undef WRITEONLY
#endif

#ifdef DVEC2
#undef DVEC2
#endif

#ifdef DVEC3
#undef DVEC3
#endif

#ifdef DVEC4
#undef DVEC4
#endif

#ifdef DMAT2
#undef DMAT2
#endif

#ifdef DMAT3
#undef DMAT3
#endif

#ifdef DMAT4
#undef DMAT4
#endif

#ifdef NOPERSPECTIVE
#undef NOPERSPECTIVE
#endif

#ifdef FLAT
#undef FLAT
#endif

#ifdef SMOOTH
#undef SMOOTH
#endif

#ifdef LAYOUT
#undef LAYOUT
#endif

#ifdef MAT2X2
#undef MAT2X2
#endif

#ifdef MAT2X3
#undef MAT2X3
#endif

#ifdef MAT2X4
#undef MAT2X4
#endif

#ifdef MAT3X2
#undef MAT3X2
#endif

#ifdef MAT3X3
#undef MAT3X3
#endif

#ifdef MAT3X4
#undef MAT3X4
#endif

#ifdef MAT4X2
#undef MAT4X2
#endif

#ifdef MAT4X3
#undef MAT4X3
#endif

#ifdef MAT4X4
#undef MAT4X4
#endif

#ifdef DMAT2X2
#undef DMAT2X2
#endif

#ifdef DMAT2X3
#undef DMAT2X3
#endif

#ifdef DMAT2X4
#undef DMAT2X4
#endif

#ifdef DMAT3X2
#undef DMAT3X2
#endif

#ifdef DMAT3X3
#undef DMAT3X3
#endif

#ifdef DMAT3X4
#undef DMAT3X4
#endif

#ifdef DMAT4X2
#undef DMAT4X2
#endif

#ifdef DMAT4X3
#undef DMAT4X3
#endif

#ifdef DMAT4X4
#undef DMAT4X4
#endif

#ifdef ATOMIC_UINT
#undef ATOMIC_UINT
#endif

#ifdef SAMPLER1D
#undef SAMPLER1D
#endif

#ifdef SAMPLER2D
#undef SAMPLER2D
#endif

#ifdef SAMPLER3D
#undef SAMPLER3D
#endif

#ifdef SAMPLERCUBE
#undef SAMPLERCUBE
#endif

#ifdef SAMPLER1DSHADOW
#undef SAMPLER1DSHADOW
#endif

#ifdef SAMPLER2DSHADOW
#undef SAMPLER2DSHADOW
#endif

#ifdef SAMPLERCUBESHADOW
#undef SAMPLERCUBESHADOW
#endif

#ifdef SAMPLER1DARRAY
#undef SAMPLER1DARRAY
#endif

#ifdef SAMPLER2DARRAY
#undef SAMPLER2DARRAY
#endif

#ifdef SAMPLER1DARRAYSHADOW
#undef SAMPLER1DARRAYSHADOW
#endif

#ifdef SAMPLER2DARRAYSHADOW
#undef SAMPLER2DARRAYSHADOW
#endif

#ifdef ISAMPLER1D
#undef ISAMPLER1D
#endif

#ifdef ISAMPLER2D
#undef ISAMPLER2D
#endif

#ifdef ISAMPLER3D
#undef ISAMPLER3D
#endif

#ifdef ISAMPLERCUBE
#undef ISAMPLERCUBE
#endif

#ifdef ISAMPLER1DARRAY
#undef ISAMPLER1DARRAY
#endif

#ifdef ISAMPLER2DARRAY
#undef ISAMPLER2DARRAY
#endif

#ifdef USAMPLER1D
#undef USAMPLER1D
#endif

#ifdef USAMPLER2D
#undef USAMPLER2D
#endif

#ifdef USAMPLER3D
#undef USAMPLER3D
#endif

#ifdef USAMPLERCUBE
#undef USAMPLERCUBE
#endif

#ifdef USAMPLER1DARRAY
#undef USAMPLER1DARRAY
#endif

#ifdef USAMPLER2DARRAY
#undef USAMPLER2DARRAY
#endif

#ifdef SAMPLER2DRECT
#undef SAMPLER2DRECT
#endif

#ifdef SAMPLER2DRECTSHADOW
#undef SAMPLER2DRECTSHADOW
#endif

#ifdef ISAMPLER2DRECT
#undef ISAMPLER2DRECT
#endif

#ifdef USAMPLER2DRECT
#undef USAMPLER2DRECT
#endif

#ifdef SAMPLERBUFFER
#undef SAMPLERBUFFER
#endif

#ifdef ISAMPLERBUFFER
#undef ISAMPLERBUFFER
#endif

#ifdef USAMPLERBUFFER
#undef USAMPLERBUFFER
#endif

#ifdef SAMPLERCUBEARRAY
#undef SAMPLERCUBEARRAY
#endif

#ifdef SAMPLERCUBEARRAYSHADOW
#undef SAMPLERCUBEARRAYSHADOW
#endif

#ifdef ISAMPLERCUBEARRAY
#undef ISAMPLERCUBEARRAY
#endif

#ifdef USAMPLERCUBEARRAY
#undef USAMPLERCUBEARRAY
#endif

#ifdef SAMPLER2DMS
#undef SAMPLER2DMS
#endif

#ifdef ISAMPLER2DMS
#undef ISAMPLER2DMS
#endif

#ifdef USAMPLER2DMS
#undef USAMPLER2DMS
#endif

#ifdef SAMPLER2DMSARRAY
#undef SAMPLER2DMSARRAY
#endif

#ifdef ISAMPLER2DMSARRAY
#undef ISAMPLER2DMSARRAY
#endif

#ifdef USAMPLER2DMSARRAY
#undef USAMPLER2DMSARRAY
#endif

#ifdef IMAGE1D
#undef IMAGE1D
#endif

#ifdef IIMAGE1D
#undef IIMAGE1D
#endif

#ifdef UIMAGE1D
#undef UIMAGE1D
#endif

#ifdef IMAGE2D
#undef IMAGE2D
#endif

#ifdef IIMAGE2D
#undef IIMAGE2D
#endif

#ifdef UIMAGE2D
#undef UIMAGE2D
#endif

#ifdef IMAGE3D
#undef IMAGE3D
#endif

#ifdef IIMAGE3D
#undef IIMAGE3D
#endif

#ifdef UIMAGE3D
#undef UIMAGE3D
#endif

#ifdef IMAGE2DRECT
#undef IMAGE2DRECT
#endif

#ifdef IIMAGE2DRECT
#undef IIMAGE2DRECT
#endif

#ifdef UIMAGE2DRECT
#undef UIMAGE2DRECT
#endif

#ifdef IMAGECUBE
#undef IMAGECUBE
#endif

#ifdef IIMAGECUBE
#undef IIMAGECUBE
#endif

#ifdef UIMAGECUBE
#undef UIMAGECUBE
#endif

#ifdef IMAGEBUFFER
#undef IMAGEBUFFER
#endif

#ifdef IIMAGEBUFFER
#undef IIMAGEBUFFER
#endif

#ifdef UIMAGEBUFFER
#undef UIMAGEBUFFER
#endif

#ifdef IMAGE1DARRAY
#undef IMAGE1DARRAY
#endif

#ifdef IIMAGE1DARRAY
#undef IIMAGE1DARRAY
#endif

#ifdef UIMAGE1DARRAY
#undef UIMAGE1DARRAY
#endif

#ifdef IMAGE2DARRAY
#undef IMAGE2DARRAY
#endif

#ifdef IIMAGE2DARRAY
#undef IIMAGE2DARRAY
#endif

#ifdef UIMAGE2DARRAY
#undef UIMAGE2DARRAY
#endif

#ifdef IMAGECUBEARRAY
#undef IMAGECUBEARRAY
#endif

#ifdef IIMAGECUBEARRAY
#undef IIMAGECUBEARRAY
#endif

#ifdef UIMAGECUBEARRAY
#undef UIMAGECUBEARRAY
#endif

#ifdef IMAGE2DMS
#undef IMAGE2DMS
#endif

#ifdef IIMAGE2DMS
#undef IIMAGE2DMS
#endif

#ifdef UIMAGE2DMS
#undef UIMAGE2DMS
#endif

#ifdef IMAGE2DMSARRAY
#undef IMAGE2DMSARRAY
#endif

#ifdef IIMAGE2DMSARRAY
#undef IIMAGE2DMSARRAY
#endif

#ifdef UIMAGE2DMSARRAY
#undef UIMAGE2DMSARRAY
#endif

#ifdef STRUCT
#undef STRUCT
#endif

#ifdef VOID
#undef VOID
#endif

#ifdef WHILE
#undef WHILE
#endif

#ifdef IDENTIFIER
#undef IDENTIFIER
#endif

#ifdef FLOATCONSTANT
#undef FLOATCONSTANT
#endif

#ifdef DOUBLECONSTANT
#undef DOUBLECONSTANT
#endif

#ifdef INTCONSTANT
#undef INTCONSTANT
#endif

#ifdef UINTCONSTANT
#undef UINTCONSTANT
#endif

#ifdef TRUE_VALUE
#undef TRUE_VALUE
#endif

#ifdef FALSE_VALUE
#undef FALSE_VALUE
#endif

#ifdef LEFT_OP
#undef LEFT_OP
#endif

#ifdef RIGHT_OP
#undef RIGHT_OP
#endif

#ifdef INC_OP
#undef INC_OP
#endif

#ifdef DEC_OP
#undef DEC_OP
#endif

#ifdef LE_OP
#undef LE_OP
#endif

#ifdef GE_OP
#undef GE_OP
#endif

#ifdef EQ_OP
#undef EQ_OP
#endif

#ifdef NE_OP
#undef NE_OP
#endif

#ifdef AND_OP
#undef AND_OP
#endif

#ifdef OR_OP
#undef OR_OP
#endif

#ifdef XOR_OP
#undef XOR_OP
#endif

#ifdef MUL_ASSIGN
#undef MUL_ASSIGN
#endif

#ifdef DIV_ASSIGN
#undef DIV_ASSIGN
#endif

#ifdef ADD_ASSIGN
#undef ADD_ASSIGN
#endif

#ifdef MOD_ASSIGN
#undef MOD_ASSIGN
#endif

#ifdef LEFT_ASSIGN
#undef LEFT_ASSIGN
#endif

#ifdef RIGHT_ASSIGN
#undef RIGHT_ASSIGN
#endif

#ifdef AND_ASSIGN
#undef AND_ASSIGN
#endif

#ifdef XOR_ASSIGN
#undef XOR_ASSIGN
#endif

#ifdef OR_ASSIGN
#undef OR_ASSIGN
#endif

#ifdef SUB_ASSIGN
#undef SUB_ASSIGN
#endif

#ifdef LEFT_PAREN
#undef LEFT_PAREN
#endif

#ifdef RIGHT_PAREN
#undef RIGHT_PAREN
#endif

#ifdef LEFT_BRACKET
#undef LEFT_BRACKET
#endif

#ifdef RIGHT_BRACKET
#undef RIGHT_BRACKET
#endif

#ifdef LEFT_BRACE
#undef LEFT_BRACE
#endif

#ifdef RIGHT_BRACE
#undef RIGHT_BRACE
#endif

#ifdef DOT
#undef DOT
#endif

#ifdef COMMA
#undef COMMA
#endif

#ifdef COLON
#undef COLON
#endif

#ifdef EQUAL
#undef EQUAL
#endif

#ifdef SEMICOLON
#undef SEMICOLON
#endif

#ifdef BANG
#undef BANG
#endif

#ifdef DASH
#undef DASH
#endif

#ifdef TILDE
#undef TILDE
#endif

#ifdef PLUS
#undef PLUS
#endif

#ifdef STAR
#undef STAR
#endif

#ifdef SLASH
#undef SLASH
#endif

#ifdef PERCENT
#undef PERCENT
#endif

#ifdef LEFT_ANGLE
#undef LEFT_ANGLE
#endif

#ifdef RIGHT_ANGLE
#undef RIGHT_ANGLE
#endif

#ifdef VERTICAL_BAR
#undef VERTICAL_BAR
#endif

#ifdef CARET
#undef CARET
#endif

#ifdef AMPERSAND
#undef AMPERSAND
#endif

#ifdef QUESTION
#undef QUESTION
#endif

#ifdef INVARIANT
#undef INVARIANT
#endif

#ifdef PRECISE
#undef PRECISE
#endif

#ifdef HIGHP
#undef HIGHP
#endif

#ifdef MEDIUMP
#undef MEDIUMP
#endif

#ifdef LOWP
#undef LOWP
#endif

#ifdef PRECISION
#undef PRECISION
#endif

#ifdef AT
#undef AT
#endif

#ifdef UNARY_PLUS
#undef UNARY_PLUS
#endif

#ifdef UNARY_DASH
#undef UNARY_DASH
#endif

#ifdef PRE_INC_OP
#undef PRE_INC_OP
#endif

#ifdef PRE_DEC_OP
#undef PRE_DEC_OP
#endif

#ifdef POST_DEC_OP
#undef POST_DEC_OP
#endif

#ifdef POST_INC_OP
#undef POST_INC_OP
#endif

#ifdef ARRAY_REF_OP
#undef ARRAY_REF_OP
#endif

#ifdef FUNCTION_CALL
#undef FUNCTION_CALL
#endif

#ifdef TYPE_NAME_LIST
#undef TYPE_NAME_LIST
#endif

#ifdef TYPE_SPECIFIER
#undef TYPE_SPECIFIER
#endif

#ifdef POSTFIX_EXPRESSION
#undef POSTFIX_EXPRESSION
#endif

#ifdef TYPE_QUALIFIER_LIST
#undef TYPE_QUALIFIER_LIST
#endif

#ifdef STRUCT_DECLARATION
#undef STRUCT_DECLARATION
#endif

#ifdef STRUCT_DECLARATOR
#undef STRUCT_DECLARATOR
#endif

#ifdef STRUCT_SPECIFIER
#undef STRUCT_SPECIFIER
#endif

#ifdef FUNCTION_DEFINITION
#undef FUNCTION_DEFINITION
#endif

#ifdef DECLARATION
#undef DECLARATION
#endif

#ifdef STATEMENT_LIST
#undef STATEMENT_LIST
#endif

#ifdef TRANSLATION_UNIT
#undef TRANSLATION_UNIT
#endif

#ifdef PRECISION_DECLARATION
#undef PRECISION_DECLARATION
#endif

#ifdef BLOCK_DECLARATION
#undef BLOCK_DECLARATION
#endif

#ifdef TYPE_QUALIFIER_DECLARATION
#undef TYPE_QUALIFIER_DECLARATION
#endif

#ifdef IDENTIFIER_LIST
#undef IDENTIFIER_LIST
#endif

#ifdef INIT_DECLARATOR_LIST
#undef INIT_DECLARATOR_LIST
#endif

#ifdef FULLY_SPECIFIED_TYPE
#undef FULLY_SPECIFIED_TYPE
#endif

#ifdef SINGLE_DECLARATION
#undef SINGLE_DECLARATION
#endif

#ifdef SINGLE_INIT_DECLARATION
#undef SINGLE_INIT_DECLARATION
#endif

#ifdef INITIALIZER_LIST
#undef INITIALIZER_LIST
#endif

#ifdef EXPRESSION_STATEMENT
#undef EXPRESSION_STATEMENT
#endif

#ifdef SELECTION_STATEMENT
#undef SELECTION_STATEMENT
#endif

#ifdef SELECTION_STATEMENT_ELSE
#undef SELECTION_STATEMENT_ELSE
#endif

#ifdef SWITCH_STATEMENT
#undef SWITCH_STATEMENT
#endif

#ifdef FOR_REST_STATEMENT
#undef FOR_REST_STATEMENT
#endif

#ifdef WHILE_STATEMENT
#undef WHILE_STATEMENT
#endif

#ifdef DO_STATEMENT
#undef DO_STATEMENT
#endif

#ifdef FOR_STATEMENT
#undef FOR_STATEMENT
#endif

#ifdef CASE_LABEL
#undef CASE_LABEL
#endif

#ifdef CONDITION_OPT
#undef CONDITION_OPT
#endif

#ifdef ASSIGNMENT_CONDITION
#undef ASSIGNMENT_CONDITION
#endif

#ifdef EXPRESSION_CONDITION
#undef EXPRESSION_CONDITION
#endif

#ifdef FUNCTION_HEADER
#undef FUNCTION_HEADER
#endif

#ifdef FUNCTION_DECLARATION
#undef FUNCTION_DECLARATION
#endif

#ifdef FUNCTION_PARAMETER_LIST
#undef FUNCTION_PARAMETER_LIST
#endif

#ifdef PARAMETER_DECLARATION
#undef PARAMETER_DECLARATION
#endif

#ifdef PARAMETER_DECLARATOR
#undef PARAMETER_DECLARATOR
#endif

#ifdef UNINITIALIZED_DECLARATION
#undef UNINITIALIZED_DECLARATION
#endif

#ifdef ARRAY_SPECIFIER
#undef ARRAY_SPECIFIER
#endif

#ifdef ARRAY_SPECIFIER_LIST
#undef ARRAY_SPECIFIER_LIST
#endif

#ifdef STRUCT_DECLARATOR_LIST
#undef STRUCT_DECLARATOR_LIST
#endif

#ifdef FUNCTION_CALL_PARAMETER_LIST
#undef FUNCTION_CALL_PARAMETER_LIST
#endif

#ifdef STRUCT_DECLARATION_LIST
#undef STRUCT_DECLARATION_LIST
#endif

#ifdef LAYOUT_QUALIFIER_ID
#undef LAYOUT_QUALIFIER_ID
#endif

#ifdef LAYOUT_QUALIFIER_ID_LIST
#undef LAYOUT_QUALIFIER_ID_LIST
#endif

#ifdef SUBROUTINE_TYPE
#undef SUBROUTINE_TYPE
#endif

#ifdef PAREN_EXPRESSION
#undef PAREN_EXPRESSION
#endif

#ifdef INIT_DECLARATOR
#undef INIT_DECLARATOR
#endif

#ifdef INITIALIZER
#undef INITIALIZER
#endif

#ifdef TERNARY_EXPRESSION
#undef TERNARY_EXPRESSION
#endif

#ifdef FIELD_IDENTIFIER
#undef FIELD_IDENTIFIER
#endif

#ifdef NUM_GLSL_TOKEN
#undef NUM_GLSL_TOKEN
#endif

#include "glsl.parser.h"
#endif
