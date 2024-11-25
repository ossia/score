#include "glsl_ast.h"
#include "glsl_parser.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static const char *token_to_str[4096] = {
	[CONST] = "const",
	[BOOL] = "bool",
	[FLOAT] = "float",
	[DOUBLE] = "double",
	[INT] = "int",
	[UINT] = "uint",
	[BREAK] = "break",
	[CONTINUE] = "continue",
	[DO] = "do",
	[ELSE] = "else",
	[FOR] = "for",
	[IF] = "if",
	[DISCARD] = "discard",
	[RETURN] = "return",
	[TRUE_VALUE] = "true",
	[FALSE_VALUE] = "false",
	[SWITCH] = "switch",
	[CASE] = "case",
	[DEFAULT] = "default",
	[SUBROUTINE] = "subroutine",
	[BVEC2] = "bvec2",
	[BVEC3] = "bvec3",
	[BVEC4] = "bvec4",
	[IVEC2] = "ivec2",
	[IVEC3] = "ivec3",
	[IVEC4] = "ivec4",
	[UVEC2] = "uvec2",
	[UVEC3] = "uvec3",
	[UVEC4] = "uvec4",
	[VEC2] = "vec2",
	[VEC3] = "vec3",
	[VEC4] = "vec4",
	[MAT2] = "mat2",
	[MAT3] = "mat3",
	[MAT4] = "mat4",
	[CENTROID] = "centroid",
	[IN] = "in",
	[OUT] = "out",
	[INOUT] = "inout",
	[UNIFORM] = "uniform",
	[PATCH] = "patch",
	[SAMPLE] = "sample",
	[BUFFER] = "buffer",
	[SHARED] = "shared",
	[COHERENT] = "coherent",
	[VOLATILE] = "volatile",
	[RESTRICT] = "restrict",
	[READONLY] = "readonly",
	[WRITEONLY] = "writeonly",
	[DVEC2] = "dvec2",
	[DVEC3] = "dvec3",
	[DVEC4] = "dvec4",
	[DMAT2] = "dmat2",
	[DMAT3] = "dmat3",
	[DMAT4] = "dmat4",
	[NOPERSPECTIVE] = "noperspective",
	[FLAT] = "flat",
	[SMOOTH] = "smooth",
	[LAYOUT] = "layout",
	[MAT2X2] = "mat2x2",
	[MAT2X3] = "mat2x3",
	[MAT2X4] = "mat2x4",
	[MAT3X2] = "mat3x2",
	[MAT3X3] = "mat3x3",
	[MAT3X4] = "mat3x4",
	[MAT4X2] = "mat4x2",
	[MAT4X3] = "mat4x3",
	[MAT4X4] = "mat4x4",
	[DMAT2X2] = "dmat2x2",
	[DMAT2X3] = "dmat2x3",
	[DMAT2X4] = "dmat2x4",
	[DMAT3X2] = "dmat3x2",
	[DMAT3X3] = "dmat3x3",
	[DMAT3X4] = "dmat3x4",
	[DMAT4X2] = "dmat4x2",
	[DMAT4X3] = "dmat4x3",
	[DMAT4X4] = "dmat4x4",
	[ATOMIC_UINT] = "atomic_uint",
	[SAMPLER1D] = "sampler1D",
	[SAMPLER2D] = "sampler2D",
	[SAMPLER3D] = "sampler3D",
	[SAMPLERCUBE] = "samplercube",
	[SAMPLER1DSHADOW] = "sampler1Dshadow",
	[SAMPLER2DSHADOW] = "sampler2Dshadow",
	[SAMPLERCUBESHADOW] = "samplercubeshadow",
	[SAMPLER1DARRAY] = "sampler1Darray",
	[SAMPLER2DARRAY] = "sampler2Darray",
	[SAMPLER1DARRAYSHADOW] = "sampler1Darrayshadow",
	[SAMPLER2DARRAYSHADOW] = "sampler2Darrayshadow",
	[ISAMPLER1D] = "isampler1D",
	[ISAMPLER2D] = "isampler2D",
	[ISAMPLER3D] = "isampler3D",
	[ISAMPLERCUBE] = "isamplercube",
	[ISAMPLER1DARRAY] = "isampler1Darray",
	[ISAMPLER2DARRAY] = "isampler2Darray",
	[USAMPLER1D] = "usampler1D",
	[USAMPLER2D] = "usampler2D",
	[USAMPLER3D] = "usampler3D",
	[USAMPLERCUBE] = "usamplercube",
	[USAMPLER1DARRAY] = "usampler1Darray",
	[USAMPLER2DARRAY] = "usampler2Darray",
	[SAMPLER2DRECT] = "sampler2Drect",
	[SAMPLER2DRECTSHADOW] = "sampler2Drectshadow",
	[ISAMPLER2DRECT] = "isampler2Drect",
	[USAMPLER2DRECT] = "usampler2Drect",
	[SAMPLERBUFFER] = "samplerbuffer",
	[ISAMPLERBUFFER] = "isamplerbuffer",
	[USAMPLERBUFFER] = "usamplerbuffer",
	[SAMPLERCUBEARRAY] = "samplercubearray",
	[SAMPLERCUBEARRAYSHADOW] = "samplercubearrayshadow",
	[ISAMPLERCUBEARRAY] = "isamplercubearray",
	[USAMPLERCUBEARRAY] = "usamplercubearray",
	[SAMPLER2DMS] = "sampler2Dms",
	[ISAMPLER2DMS] = "isampler2Dms",
	[USAMPLER2DMS] = "usampler2Dms",
	[SAMPLER2DMSARRAY] = "sampler2Dmsarray",
	[ISAMPLER2DMSARRAY] = "isampler2Dmsarray",
	[USAMPLER2DMSARRAY] = "usampler2Dmsarray",
	[IMAGE1D] = "image1D",
	[IIMAGE1D] = "iimage1D",
	[UIMAGE1D] = "uimage1D",
	[IMAGE2D] = "image2D",
	[IIMAGE2D] = "iimage2D",
	[UIMAGE2D] = "uimage2D",
	[IMAGE3D] = "image3D",
	[IIMAGE3D] = "iimage3D",
	[UIMAGE3D] = "uimage3D",
	[IMAGE2DRECT] = "image2Drect",
	[IIMAGE2DRECT] = "iimage2Drect",
	[UIMAGE2DRECT] = "uimage2Drect",
	[IMAGECUBE] = "imagecube",
	[IIMAGECUBE] = "iimagecube",
	[UIMAGECUBE] = "uimagecube",
	[IMAGEBUFFER] = "imagebuffer",
	[IIMAGEBUFFER] = "iimagebuffer",
	[UIMAGEBUFFER] = "uimagebuffer",
	[IMAGE1DARRAY] = "image1Darray",
	[IIMAGE1DARRAY] = "iimage1Darray",
	[UIMAGE1DARRAY] = "uimage1Darray",
	[IMAGE2DARRAY] = "image2Darray",
	[IIMAGE2DARRAY] = "iimage2Darray",
	[UIMAGE2DARRAY] = "uimage2Darray",
	[IMAGECUBEARRAY] = "imagecubearray",
	[IIMAGECUBEARRAY] = "iimagecubearray",
	[UIMAGECUBEARRAY] = "uimagecubearray",
	[IMAGE2DMS] = "image2Dms",
	[IIMAGE2DMS] = "iimage2Dms",
	[UIMAGE2DMS] = "uimage2Dms",
	[IMAGE2DMSARRAY] = "image2Dmsarray",
	[IIMAGE2DMSARRAY] = "iimage2Dmsarray",
	[UIMAGE2DMSARRAY] = "uimage2Dmsarray",
	[STRUCT] = "struct",
	[VOID] = "void",
	[WHILE] = "while",
	[LEFT_OP] = "<<",
	[RIGHT_OP] = ">>",
	[PRE_INC_OP] = "++",
	[PRE_DEC_OP] = "--",
	[POST_INC_OP] = "++",
	[POST_DEC_OP] = "--",
	[LE_OP] = "<=",
	[GE_OP] = ">=",
	[EQ_OP] = "==",
	[NE_OP] = "!=",
	[AND_OP] = "&&",
	[OR_OP] = "||",
	[XOR_OP] = "^^",
	[MUL_ASSIGN] = "*=",
	[DIV_ASSIGN] = "/=",
	[ADD_ASSIGN] = "+=",
	[MOD_ASSIGN] = "%=",
	[LEFT_ASSIGN] = "<<=",
	[RIGHT_ASSIGN] = ">>=",
	[AND_ASSIGN] = "&=",
	[XOR_ASSIGN] = "^=",
	[OR_ASSIGN] = "|=",
	[SUB_ASSIGN] = "-=",
	[LEFT_PAREN] = "(",
	[RIGHT_PAREN] = ")",
	[LEFT_BRACKET] = "[",
	[RIGHT_BRACKET] = "]",
	[LEFT_BRACE] = "{",
	[RIGHT_BRACE] = "}",
	[DOT] = ".",
	[COMMA] = ",",
	[COLON] = ":",
	[EQUAL] = "=",
	[SEMICOLON] = ";",
	[BANG] = "!",
	[DASH] = "-",
	[TILDE] = "~",
	[PLUS] = "+",
	[STAR] = "*",
	[SLASH] = "/",
	[PERCENT] = "%",
	[LEFT_ANGLE] = "<",
	[RIGHT_ANGLE] = ">",
	[VERTICAL_BAR] = "|",
	[CARET] = "^",
	[AMPERSAND] = "&",
	[QUESTION] = "?",
	[INVARIANT] = "invariant",
	[PRECISE] = "precise",
	[HIGHP] = "highp",
	[MEDIUMP] = "mediump",
	[LOWP] = "lowp",
	[PRECISION] = "precision",
	[UNARY_PLUS] = "+",
	[UNARY_DASH] = "-",
	[NUM_GLSL_TOKEN] = ""
};

static const char *code_to_str[4096] = {
	[CONST] = "CONST",
	[BOOL] = "BOOL",
	[FLOAT] = "FLOAT",
	[DOUBLE] = "DOUBLE",
	[INT] = "INT",
	[UINT] = "UINT",
	[BREAK] = "BREAK",
	[CONTINUE] = "CONTINUE",
	[DO] = "DO",
	[ELSE] = "ELSE",
	[FOR] = "FOR",
	[IF] = "IF",
	[DISCARD] = "DISCARD",
	[RETURN] = "RETURN",
	[RETURN_VALUE] = "RETURN_VALUE",
	[SWITCH] = "SWITCH",
	[CASE] = "CASE",
	[DEFAULT] = "DEFAULT",
	[SUBROUTINE] = "SUBROUTINE",
	[BVEC2] = "BVEC2",
	[BVEC3] = "BVEC3",
	[BVEC4] = "BVEC4",
	[IVEC2] = "IVEC2",
	[IVEC3] = "IVEC3",
	[IVEC4] = "IVEC4",
	[UVEC2] = "UVEC2",
	[UVEC3] = "UVEC3",
	[UVEC4] = "UVEC4",
	[VEC2] = "VEC2",
	[VEC3] = "VEC3",
	[VEC4] = "VEC4",
	[MAT2] = "MAT2",
	[MAT3] = "MAT3",
	[MAT4] = "MAT4",
	[CENTROID] = "CENTROID",
	[IN] = "IN",
	[OUT] = "OUT",
	[INOUT] = "INOUT",
	[UNIFORM] = "UNIFORM",
	[PATCH] = "PATCH",
	[SAMPLE] = "SAMPLE",
	[BUFFER] = "BUFFER",
	[SHARED] = "SHARED",
	[COHERENT] = "COHERENT",
	[VOLATILE] = "VOLATILE",
	[RESTRICT] = "RESTRICT",
	[READONLY] = "READONLY",
	[WRITEONLY] = "WRITEONLY",
	[DVEC2] = "DVEC2",
	[DVEC3] = "DVEC3",
	[DVEC4] = "DVEC4",
	[DMAT2] = "DMAT2",
	[DMAT3] = "DMAT3",
	[DMAT4] = "DMAT4",
	[NOPERSPECTIVE] = "NOPERSPECTIVE",
	[FLAT] = "FLAT",
	[SMOOTH] = "SMOOTH",
	[LAYOUT] = "LAYOUT",
	[MAT2X2] = "MAT2X2",
	[MAT2X3] = "MAT2X3",
	[MAT2X4] = "MAT2X4",
	[MAT3X2] = "MAT3X2",
	[MAT3X3] = "MAT3X3",
	[MAT3X4] = "MAT3X4",
	[MAT4X2] = "MAT4X2",
	[MAT4X3] = "MAT4X3",
	[MAT4X4] = "MAT4X4",
	[DMAT2X2] = "DMAT2X2",
	[DMAT2X3] = "DMAT2X3",
	[DMAT2X4] = "DMAT2X4",
	[DMAT3X2] = "DMAT3X2",
	[DMAT3X3] = "DMAT3X3",
	[DMAT3X4] = "DMAT3X4",
	[DMAT4X2] = "DMAT4X2",
	[DMAT4X3] = "DMAT4X3",
	[DMAT4X4] = "DMAT4X4",
	[ATOMIC_UINT] = "ATOMIC_UINT",
	[SAMPLER1D] = "SAMPLER1D",
	[SAMPLER2D] = "SAMPLER2D",
	[SAMPLER3D] = "SAMPLER3D",
	[SAMPLERCUBE] = "SAMPLERCUBE",
	[SAMPLER1DSHADOW] = "SAMPLER1DSHADOW",
	[SAMPLER2DSHADOW] = "SAMPLER2DSHADOW",
	[SAMPLERCUBESHADOW] = "SAMPLERCUBESHADOW",
	[SAMPLER1DARRAY] = "SAMPLER1DARRAY",
	[SAMPLER2DARRAY] = "SAMPLER2DARRAY",
	[SAMPLER1DARRAYSHADOW] = "SAMPLER1DARRAYSHADOW",
	[SAMPLER2DARRAYSHADOW] = "SAMPLER2DARRAYSHADOW",
	[ISAMPLER1D] = "ISAMPLER1D",
	[ISAMPLER2D] = "ISAMPLER2D",
	[ISAMPLER3D] = "ISAMPLER3D",
	[ISAMPLERCUBE] = "ISAMPLERCUBE",
	[ISAMPLER1DARRAY] = "ISAMPLER1DARRAY",
	[ISAMPLER2DARRAY] = "ISAMPLER2DARRAY",
	[USAMPLER1D] = "USAMPLER1D",
	[USAMPLER2D] = "USAMPLER2D",
	[USAMPLER3D] = "USAMPLER3D",
	[USAMPLERCUBE] = "USAMPLERCUBE",
	[USAMPLER1DARRAY] = "USAMPLER1DARRAY",
	[USAMPLER2DARRAY] = "USAMPLER2DARRAY",
	[SAMPLER2DRECT] = "SAMPLER2DRECT",
	[SAMPLER2DRECTSHADOW] = "SAMPLER2DRECTSHADOW",
	[ISAMPLER2DRECT] = "ISAMPLER2DRECT",
	[USAMPLER2DRECT] = "USAMPLER2DRECT",
	[SAMPLERBUFFER] = "SAMPLERBUFFER",
	[ISAMPLERBUFFER] = "ISAMPLERBUFFER",
	[USAMPLERBUFFER] = "USAMPLERBUFFER",
	[SAMPLERCUBEARRAY] = "SAMPLERCUBEARRAY",
	[SAMPLERCUBEARRAYSHADOW] = "SAMPLERCUBEARRAYSHADOW",
	[ISAMPLERCUBEARRAY] = "ISAMPLERCUBEARRAY",
	[USAMPLERCUBEARRAY] = "USAMPLERCUBEARRAY",
	[SAMPLER2DMS] = "SAMPLER2DMS",
	[ISAMPLER2DMS] = "ISAMPLER2DMS",
	[USAMPLER2DMS] = "USAMPLER2DMS",
	[SAMPLER2DMSARRAY] = "SAMPLER2DMSARRAY",
	[ISAMPLER2DMSARRAY] = "ISAMPLER2DMSARRAY",
	[USAMPLER2DMSARRAY] = "USAMPLER2DMSARRAY",
	[IMAGE1D] = "IMAGE1D",
	[IIMAGE1D] = "IIMAGE1D",
	[UIMAGE1D] = "UIMAGE1D",
	[IMAGE2D] = "IMAGE2D",
	[IIMAGE2D] = "IIMAGE2D",
	[UIMAGE2D] = "UIMAGE2D",
	[IMAGE3D] = "IMAGE3D",
	[IIMAGE3D] = "IIMAGE3D",
	[UIMAGE3D] = "UIMAGE3D",
	[IMAGE2DRECT] = "IMAGE2DRECT",
	[IIMAGE2DRECT] = "IIMAGE2DRECT",
	[UIMAGE2DRECT] = "UIMAGE2DRECT",
	[IMAGECUBE] = "IMAGECUBE",
	[IIMAGECUBE] = "IIMAGECUBE",
	[UIMAGECUBE] = "UIMAGECUBE",
	[IMAGEBUFFER] = "IMAGEBUFFER",
	[IIMAGEBUFFER] = "IIMAGEBUFFER",
	[UIMAGEBUFFER] = "UIMAGEBUFFER",
	[IMAGE1DARRAY] = "IMAGE1DARRAY",
	[IIMAGE1DARRAY] = "IIMAGE1DARRAY",
	[UIMAGE1DARRAY] = "UIMAGE1DARRAY",
	[IMAGE2DARRAY] = "IMAGE2DARRAY",
	[IIMAGE2DARRAY] = "IIMAGE2DARRAY",
	[UIMAGE2DARRAY] = "UIMAGE2DARRAY",
	[IMAGECUBEARRAY] = "IMAGECUBEARRAY",
	[IIMAGECUBEARRAY] = "IIMAGECUBEARRAY",
	[UIMAGECUBEARRAY] = "UIMAGECUBEARRAY",
	[IMAGE2DMS] = "IMAGE2DMS",
	[IIMAGE2DMS] = "IIMAGE2DMS",
	[UIMAGE2DMS] = "UIMAGE2DMS",
	[IMAGE2DMSARRAY] = "IMAGE2DMSARRAY",
	[IIMAGE2DMSARRAY] = "IIMAGE2DMSARRAY",
	[UIMAGE2DMSARRAY] = "UIMAGE2DMSARRAY",
	[STRUCT] = "STRUCT",
	[VOID] = "VOID",
	[WHILE] = "WHILE",
	[TRUE_VALUE] = "TRUE_VALUE",
	[FALSE_VALUE] = "FALSE_VALUE",
	[LEFT_OP] = "LEFT_OP",
	[RIGHT_OP] = "RIGHT_OP",
	[INC_OP] = "INC_OP",
	[DEC_OP] = "DEC_OP",
	[LE_OP] = "LE_OP",
	[GE_OP] = "GE_OP",
	[EQ_OP] = "EQ_OP",
	[NE_OP] = "NE_OP",
	[AND_OP] = "AND_OP",
	[OR_OP] = "OR_OP",
	[XOR_OP] = "XOR_OP",
	[MUL_ASSIGN] = "MUL_ASSIGN",
	[DIV_ASSIGN] = "DIV_ASSIGN",
	[ADD_ASSIGN] = "ADD_ASSIGN",
	[MOD_ASSIGN] = "MOD_ASSIGN",
	[LEFT_ASSIGN] = "LEFT_ASSIGN",
	[RIGHT_ASSIGN] = "RIGHT_ASSIGN",
	[AND_ASSIGN] = "AND_ASSIGN",
	[XOR_ASSIGN] = "XOR_ASSIGN",
	[OR_ASSIGN] = "OR_ASSIGN",
	[SUB_ASSIGN] = "SUB_ASSIGN",
	[LEFT_PAREN] = "LEFT_PAREN",
	[RIGHT_PAREN] = "RIGHT_PAREN",
	[LEFT_BRACKET] = "LEFT_BRACKET",
	[RIGHT_BRACKET] = "RIGHT_BRACKET",
	[LEFT_BRACE] = "LEFT_BRACE",
	[RIGHT_BRACE] = "RIGHT_BRACE",
	[DOT] = "DOT",
	[COMMA] = "COMMA",
	[COLON] = "COLON",
	[EQUAL] = "EQUAL",
	[SEMICOLON] = "SEMICOLON",
	[BANG] = "BANG",
	[DASH] = "DASH",
	[TILDE] = "TILDE",
	[PLUS] = "PLUS",
	[STAR] = "STAR",
	[SLASH] = "SLASH",
	[PERCENT] = "PERCENT",
	[LEFT_ANGLE] = "LEFT_ANGLE",
	[RIGHT_ANGLE] = "RIGHT_ANGLE",
	[VERTICAL_BAR] = "VERTICAL_BAR",
	[CARET] = "CARET",
	[AMPERSAND] = "AMPERSAND",
	[QUESTION] = "QUESTION",
	[INVARIANT] = "INVARIANT",
	[PRECISE] = "PRECISE",
	[HIGHP] = "HIGHP",
	[MEDIUMP] = "MEDIUMP",
	[LOWP] = "LOWP",
	[PRECISION] = "PRECISION",
	[AT] = "AT",

	[UNARY_PLUS] = "UNARY_PLUS",
	[UNARY_DASH] = "UNARY_DASH",
	[PRE_INC_OP] = "PRE_INC_OP",
	[PRE_DEC_OP] = "PRE_DEC_OP",
	[POST_DEC_OP] = "POST_DEC_OP",
	[POST_INC_OP] = "POST_INC_OP",
	[ARRAY_REF_OP] = "ARRAY_REF_OP",
	[FUNCTION_CALL] = "FUNCTION_CALL",
	[TYPE_NAME_LIST] = "TYPE_NAME_LIST",
	[TYPE_SPECIFIER] = "TYPE_SPECIFIER",
	[POSTFIX_EXPRESSION] = "POSTFIX_EXPRESSION",
	[TYPE_QUALIFIER_LIST] = "TYPE_QUALIFIER_LIST",
	[STRUCT_DECLARATION] = "STRUCT_DECLARATION",
	[STRUCT_DECLARATOR] = "STRUCT_DECLARATOR",
	[STRUCT_SPECIFIER] = "STRUCT_SPECIFIER",
	[FUNCTION_DEFINITION] = "FUNCTION_DEFINITION",
	[DECLARATION] = "DECLARATION",
	[STATEMENT_LIST] = "STATEMENT_LIST",
	[TRANSLATION_UNIT] = "TRANSLATION_UNIT",
	[PRECISION_DECLARATION] = "PRECISION_DECLARATION",
	[BLOCK_DECLARATION] = "BLOCK_DECLARATION",
	[TYPE_QUALIFIER_DECLARATION] = "TYPE_QUALIFIER_DECLARATION",
	[IDENTIFIER_LIST] = "IDENTIFIER_LIST",
	[INIT_DECLARATOR_LIST] = "INIT_DECLARATOR_LIST",
	[FULLY_SPECIFIED_TYPE] = "FULLY_SPECIFIED_TYPE",
	[SINGLE_DECLARATION] = "SINGLE_DECLARATION",
	[SINGLE_INIT_DECLARATION] = "SINGLE_INIT_DECLARATION",
	[INITIALIZER_LIST] = "INITIALIZER_LIST",
	[EXPRESSION_STATEMENT] = "EXPRESSION_STATEMENT",
	[SELECTION_STATEMENT] = "SELECTION_STATEMENT",
	[SELECTION_STATEMENT_ELSE] = "SELECTION_STATEMENT_ELSE",
	[SWITCH_STATEMENT] = "SWITCH_STATEMENT",
	[FOR_REST_STATEMENT] = "FOR_REST_STATEMENT",
	[WHILE_STATEMENT] = "WHILE_STATEMENT",
	[DO_STATEMENT] = "DO_STATEMENT",
	[FOR_STATEMENT] = "FOR_STATEMENT",
	[CASE_LABEL] = "CASE_LABEL",
	[CONDITION_OPT] = "CONDITION_OPT",
	[ASSIGNMENT_CONDITION] = "ASSIGNMENT_CONDITION",
	[EXPRESSION_CONDITION] = "EXPRESSION_CONDITION",
	[FUNCTION_HEADER] = "FUNCTION_HEADER",
	[FUNCTION_DECLARATION] = "FUNCTION_DECLARATION",
	[FUNCTION_PARAMETER_LIST] = "FUNCTION_PARAMETER_LIST",
	[PARAMETER_DECLARATION] = "PARAMETER_DECLARATION",
	[PARAMETER_DECLARATOR] = "PARAMETER_DECLARATOR",
	[UNINITIALIZED_DECLARATION] = "UNINITIALIZED_DECLARATION",
	[ARRAY_SPECIFIER] = "ARRAY_SPECIFIER",
	[ARRAY_SPECIFIER_LIST] = "ARRAY_SPECIFIER_LIST",
	[STRUCT_DECLARATOR_LIST] = "STRUCT_DECLARATOR_LIST",
	[FUNCTION_CALL_PARAMETER_LIST] = "FUNCTION_CALL_PARAMETER_LIST",
	[STRUCT_DECLARATION_LIST] = "STRUCT_DECLARATION_LIST",
	[LAYOUT_QUALIFIER_ID] = "LAYOUT_QUALIFIER_ID",
	[LAYOUT_QUALIFIER_ID_LIST] = "LAYOUT_QUALIFIER_ID_LIST",
	[SUBROUTINE_TYPE] = "SUBROUTINE_TYPE",
	[PAREN_EXPRESSION] = "PAREN_EXPRESSION",
	[INIT_DECLARATOR] = "INIT_DECLARATOR",
	[INITIALIZER] = "INITIALIZER",
	[TERNARY_EXPRESSION] = "TERNARY_EXPRESSION",
	[FIELD_IDENTIFIER] = "FIELD_IDENTIFIER",
	[NUM_GLSL_TOKEN] = ""
};

bool glsl_ast_is_list_node(struct glsl_node *n)
{
	switch(n->code) {
	case TYPE_NAME_LIST:
	case TYPE_QUALIFIER_LIST:
	case STATEMENT_LIST:
	case IDENTIFIER_LIST:
	case INIT_DECLARATOR_LIST:
	case INITIALIZER_LIST:
	case FUNCTION_PARAMETER_LIST:
	case ARRAY_SPECIFIER_LIST:
	case STRUCT_DECLARATOR_LIST:
	case STRUCT_DECLARATION_LIST:
	case TRANSLATION_UNIT:
	case FUNCTION_CALL_PARAMETER_LIST:
		return true;
	default:
		return false;
	}
}

void glsl_ast_print(struct glsl_node *n, int depth)
{
	int i;

	for (i = 0; i < depth; i++) {
		printf("\t");
	}

	if (code_to_str[n->code])
		printf("%s", code_to_str[n->code]);

	switch(n->code) {
	case IDENTIFIER:
		if (n->data.str) {
			if (code_to_str[n->code])
				printf(": ");
			printf("%s", n->data.str);
		}
		break;
	case FLOATCONSTANT:
		if (code_to_str[n->code])
			printf(": ");
		printf("%f", n->data.f);
		break;
	case DOUBLECONSTANT:
		if (code_to_str[n->code])
			printf(": ");
		printf("%f", n->data.d);
		break;
	case INTCONSTANT:
		if (code_to_str[n->code])
			printf(": ");
		printf("%d", n->data.i);
		break;
	case UINTCONSTANT:
		if (code_to_str[n->code])
			printf(": ");
		printf("%u", n->data.ui);
		break;
	}
	printf("\n");

	for (i = 0; i < n->child_count; i++) {
		glsl_ast_print((struct glsl_node *)n->children[i], depth + 1);
	}
}

static bool is_optional_list(struct glsl_node *n)
{
	switch(n->code) {
	case ARRAY_SPECIFIER_LIST:
	case TYPE_QUALIFIER_LIST:
		return true;
	}
	return false;
}

struct string {
	char *s;
	int len;
	int capacity;
};

static void _glsl_ast_gen_glsl(struct glsl_node *n, struct string *out, int depth);

static void string_cat(struct string *str, const char *format, ...)
{
	int n;
	va_list vl;
	do {
		int left = str->capacity - str->len;
		va_start(vl, format);
		n = vsnprintf(str->s + str->len, left, format, vl);
		va_end(vl);
		if (n < left) {
			break;
		} else {
			str->capacity *= 2;
			str->s = realloc(str->s, str->capacity);
		}
	} while (1);
	str->len += n;
}

static void print_list_as_glsl(struct glsl_node *n, const char *prefix, const char *delim, const char *postfix, struct string *out, int depth)
{
	int i, c = 0;
	string_cat(out,"%s", prefix);
	for (i = 0; i < n->child_count; i++) {
		if (!n->children[i]->child_count && is_optional_list(n->children[i]))
			continue;
		if (c)
			string_cat(out,"%s", delim);
		c++;
		_glsl_ast_gen_glsl(n->children[i], out, depth);
	}
	string_cat(out,"%s", postfix);
}

static void _glsl_ast_gen_glsl(struct glsl_node *n, struct string *out, int depth)
{
	int i;
	int j;
	switch(n->code) {
	case FIELD_IDENTIFIER:
	case IDENTIFIER:
		if (n->data.str) {
			string_cat(out,"%s", n->data.str);
		}
		break;
	case FLOATCONSTANT:
		string_cat(out,"%f", n->data.f);
		break;
	case DOUBLECONSTANT:
		string_cat(out,"%f", n->data.d);
		break;
	case INTCONSTANT:
		string_cat(out,"%d", n->data.i);
		break;
	case UINTCONSTANT:
		string_cat(out,"%u", n->data.ui);
		break;
	case TRANSLATION_UNIT:
		print_list_as_glsl(n, "", "\n", "\n", out, depth);
		break;
	case ARRAY_SPECIFIER_LIST:
		print_list_as_glsl(n, "", "", "", out, depth);
		break;
	case ARRAY_SPECIFIER:
		print_list_as_glsl(n, "[", "", "]", out, depth);
		break;
	case EQUAL:
	case MUL_ASSIGN:
	case DIV_ASSIGN:
	case MOD_ASSIGN:
	case ADD_ASSIGN:
	case SUB_ASSIGN:
	case LEFT_ASSIGN:
	case RIGHT_ASSIGN:
	case AND_ASSIGN:
	case XOR_ASSIGN:
	case OR_ASSIGN:
	case PLUS:
	case DASH:
	case STAR:
	case SLASH:
	case PERCENT:
	case AMPERSAND:
	case EQ_OP:
	case NE_OP:
	case LEFT_ANGLE:
	case RIGHT_ANGLE:
	case LE_OP:
	case GE_OP:
	case LEFT_OP:
	case RIGHT_OP:
	case CARET:
	case VERTICAL_BAR:
	case AND_OP:
	case OR_OP:
	case XOR_OP:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		if (token_to_str[n->code]) {
			string_cat(out," %s ", token_to_str[n->code]);
		} else {
			string_cat(out," <unknown operator %d> ", n->code);
		}
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		break;
	case DOT:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out,".");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		break;
	case PRE_INC_OP:
	case PRE_DEC_OP:
	case UNARY_PLUS:
	case UNARY_DASH:
	case TILDE:
	case BANG:
		print_list_as_glsl(n, token_to_str[n->code], "", "", out, depth);
		break;
	case PAREN_EXPRESSION:
		print_list_as_glsl(n, "(", "", ")", out, depth);
		break;
	case POST_INC_OP:
	case POST_DEC_OP:
		print_list_as_glsl(n, "", "", token_to_str[n->code], out, depth);
		break;
	case FUNCTION_DECLARATION:
	case FUNCTION_HEADER:
	case FULLY_SPECIFIED_TYPE:
	case PARAMETER_DECLARATION:
	case PARAMETER_DECLARATOR:
	case TYPE_QUALIFIER_LIST:
		print_list_as_glsl(n, "", " ", "", out, depth);
		break;
	case FUNCTION_DEFINITION:
		print_list_as_glsl(n, "", " ", "\n", out, depth);
		break;
	case FUNCTION_CALL:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		break;
	case SELECTION_STATEMENT:
		string_cat(out,"if (");
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out,") ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		break;
	case ARRAY_REF_OP:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out,"[");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out,"]");
		break;
	case RETURN:
		string_cat(out,"return;\n");
		break;
	case RETURN_VALUE:
		string_cat(out,"return ");
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out,";");
		break;
	case SELECTION_STATEMENT_ELSE:
		string_cat(out,"if (");
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out,") ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out," else ");
		_glsl_ast_gen_glsl(n->children[2], out, depth);
		string_cat(out,"\n");
		break;
	case SINGLE_DECLARATION:
		print_list_as_glsl(n, "", " ", "", out, depth);
		break;
	case SINGLE_INIT_DECLARATION:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out," ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out," = ");
		_glsl_ast_gen_glsl(n->children[3], out, depth);
		break;
	case WHILE_STATEMENT:
		string_cat(out,"while (");
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out,") ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		break;
	case DO_STATEMENT:
		string_cat(out,"do ");
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out," while ( ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out," );");
		break;
	case FOR_STATEMENT:
		string_cat(out,"for (");
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out," ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out,") ");
		_glsl_ast_gen_glsl(n->children[2], out, depth);
		break;
	case ASSIGNMENT_CONDITION:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out," ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out," = ");
		_glsl_ast_gen_glsl(n->children[2], out, depth);
		break;
	case STATEMENT_LIST:
		string_cat(out,"{\n");
		for (i = 0; i < n->child_count; i++) {
			for (j = 0; j < depth + 1; j++) string_cat(out,"\t");
			_glsl_ast_gen_glsl(n->children[i], out, depth + 1);
			string_cat(out,"\n");
		}
		for (j = 0; j < depth; j++) string_cat(out,"\t");
		string_cat(out,"}");
		break;
	case STRUCT_DECLARATION_LIST:
		for (i = 0; i < n->child_count; i++) {
			for (j = 0; j < depth + 1; j++) string_cat(out,"\t");
			_glsl_ast_gen_glsl(n->children[i], out, depth + 1);
			string_cat(out,"\n");
		}
		for (j = 0; j < depth; j++) string_cat(out,"\t");
		break;
	case BLOCK_DECLARATION:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out," ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out," {\n");
		_glsl_ast_gen_glsl(n->children[2], out, depth);
		string_cat(out,"} ");
		_glsl_ast_gen_glsl(n->children[3], out, depth);
		if (n->children[4]->child_count) {
			string_cat(out," ");
			_glsl_ast_gen_glsl(n->children[4], out, depth);
		}
		break;
	case DECLARATION:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out,";");
		break;
	case BREAK:
		string_cat(out,"break;");
		break;
	case STRUCT_SPECIFIER:
		string_cat(out, "struct ");
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out, " {\n");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out, "}");
		break;
	case STRUCT_DECLARATOR:
		print_list_as_glsl(n, "", " ", "", out, depth);
		break;
	case STRUCT_DECLARATOR_LIST:
		print_list_as_glsl(n, "", ",", "", out, depth);
		break;
	case STRUCT_DECLARATION:
		print_list_as_glsl(n, "", " ", ";", out, depth);
		break;
	case FUNCTION_PARAMETER_LIST:
	case FUNCTION_CALL_PARAMETER_LIST:
		print_list_as_glsl(n, "(", ", ", ")", out, depth);
		break;
	case FOR_REST_STATEMENT:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out,"; ");
		if (n->child_count == 2) {
			_glsl_ast_gen_glsl(n->children[1], out, depth);
		}
		break;
	case INIT_DECLARATOR:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		if (n->children[1]->child_count) {
			_glsl_ast_gen_glsl(n->children[1], out, depth);
		}
		if (n->child_count > 2) {
			string_cat(out," = ");
			_glsl_ast_gen_glsl(n->children[2], out, depth);
		}
		break;
	case INIT_DECLARATOR_LIST:
		print_list_as_glsl(n, "", ", ", "", out, depth);
		break;
	case INITIALIZER_LIST:
		print_list_as_glsl(n, "{", ", ", "}", out, depth);
		break;
	case TERNARY_EXPRESSION:
		_glsl_ast_gen_glsl(n->children[0], out, depth);
		string_cat(out," ? ");
		_glsl_ast_gen_glsl(n->children[1], out, depth);
		string_cat(out," : ");
		_glsl_ast_gen_glsl(n->children[2], out, depth);
		break;
	case TYPE_SPECIFIER:
	case POSTFIX_EXPRESSION:
	case INITIALIZER:
	case CONDITION_OPT:
	case EXPRESSION_CONDITION:
		print_list_as_glsl(n, "", "", "", out, depth);
		break;
	case EXPRESSION_STATEMENT:
		print_list_as_glsl(n, "", "", ";", out, depth);
		break;
	default:
		if (token_to_str[n->code])
			string_cat(out,"%s", token_to_str[n->code]);
		else
			string_cat(out,"<unknown token %d>", n->code);
		break;
	}
}

char *glsl_ast_generate_glsl(struct glsl_node *n)
{
	struct string s;
	s.s = malloc(1024);
	s.len = 0;
	s.capacity = 1024;
	_glsl_ast_gen_glsl(n, &s, 0);
	return s.s;
}
