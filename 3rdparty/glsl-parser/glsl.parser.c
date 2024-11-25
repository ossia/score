/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         GLSL_STYPE
#define YYLTYPE         GLSL_LTYPE
/* Substitute the variable and function names.  */
#define yyparse         glsl_parse
#define yylex           glsl_lex
#define yyerror         glsl_error
#define yydebug         glsl_debug
#define yynerrs         glsl_nerrs

/* First part of user prologue.  */
#line 1 "glsl.y"

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


#line 192 "glsl.parser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "glsl.parser.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_CONST = 3,                      /* CONST  */
  YYSYMBOL_BOOL = 4,                       /* BOOL  */
  YYSYMBOL_FLOAT = 5,                      /* FLOAT  */
  YYSYMBOL_DOUBLE = 6,                     /* DOUBLE  */
  YYSYMBOL_INT = 7,                        /* INT  */
  YYSYMBOL_UINT = 8,                       /* UINT  */
  YYSYMBOL_BREAK = 9,                      /* BREAK  */
  YYSYMBOL_CONTINUE = 10,                  /* CONTINUE  */
  YYSYMBOL_DO = 11,                        /* DO  */
  YYSYMBOL_ELSE = 12,                      /* ELSE  */
  YYSYMBOL_FOR = 13,                       /* FOR  */
  YYSYMBOL_IF = 14,                        /* IF  */
  YYSYMBOL_DISCARD = 15,                   /* DISCARD  */
  YYSYMBOL_RETURN = 16,                    /* RETURN  */
  YYSYMBOL_RETURN_VALUE = 17,              /* RETURN_VALUE  */
  YYSYMBOL_SWITCH = 18,                    /* SWITCH  */
  YYSYMBOL_CASE = 19,                      /* CASE  */
  YYSYMBOL_DEFAULT = 20,                   /* DEFAULT  */
  YYSYMBOL_SUBROUTINE = 21,                /* SUBROUTINE  */
  YYSYMBOL_BVEC2 = 22,                     /* BVEC2  */
  YYSYMBOL_BVEC3 = 23,                     /* BVEC3  */
  YYSYMBOL_BVEC4 = 24,                     /* BVEC4  */
  YYSYMBOL_IVEC2 = 25,                     /* IVEC2  */
  YYSYMBOL_IVEC3 = 26,                     /* IVEC3  */
  YYSYMBOL_IVEC4 = 27,                     /* IVEC4  */
  YYSYMBOL_UVEC2 = 28,                     /* UVEC2  */
  YYSYMBOL_UVEC3 = 29,                     /* UVEC3  */
  YYSYMBOL_UVEC4 = 30,                     /* UVEC4  */
  YYSYMBOL_VEC2 = 31,                      /* VEC2  */
  YYSYMBOL_VEC3 = 32,                      /* VEC3  */
  YYSYMBOL_VEC4 = 33,                      /* VEC4  */
  YYSYMBOL_MAT2 = 34,                      /* MAT2  */
  YYSYMBOL_MAT3 = 35,                      /* MAT3  */
  YYSYMBOL_MAT4 = 36,                      /* MAT4  */
  YYSYMBOL_CENTROID = 37,                  /* CENTROID  */
  YYSYMBOL_IN = 38,                        /* IN  */
  YYSYMBOL_OUT = 39,                       /* OUT  */
  YYSYMBOL_INOUT = 40,                     /* INOUT  */
  YYSYMBOL_UNIFORM = 41,                   /* UNIFORM  */
  YYSYMBOL_PATCH = 42,                     /* PATCH  */
  YYSYMBOL_SAMPLE = 43,                    /* SAMPLE  */
  YYSYMBOL_BUFFER = 44,                    /* BUFFER  */
  YYSYMBOL_SHARED = 45,                    /* SHARED  */
  YYSYMBOL_COHERENT = 46,                  /* COHERENT  */
  YYSYMBOL_VOLATILE = 47,                  /* VOLATILE  */
  YYSYMBOL_RESTRICT = 48,                  /* RESTRICT  */
  YYSYMBOL_READONLY = 49,                  /* READONLY  */
  YYSYMBOL_WRITEONLY = 50,                 /* WRITEONLY  */
  YYSYMBOL_DVEC2 = 51,                     /* DVEC2  */
  YYSYMBOL_DVEC3 = 52,                     /* DVEC3  */
  YYSYMBOL_DVEC4 = 53,                     /* DVEC4  */
  YYSYMBOL_DMAT2 = 54,                     /* DMAT2  */
  YYSYMBOL_DMAT3 = 55,                     /* DMAT3  */
  YYSYMBOL_DMAT4 = 56,                     /* DMAT4  */
  YYSYMBOL_NOPERSPECTIVE = 57,             /* NOPERSPECTIVE  */
  YYSYMBOL_FLAT = 58,                      /* FLAT  */
  YYSYMBOL_SMOOTH = 59,                    /* SMOOTH  */
  YYSYMBOL_LAYOUT = 60,                    /* LAYOUT  */
  YYSYMBOL_MAT2X2 = 61,                    /* MAT2X2  */
  YYSYMBOL_MAT2X3 = 62,                    /* MAT2X3  */
  YYSYMBOL_MAT2X4 = 63,                    /* MAT2X4  */
  YYSYMBOL_MAT3X2 = 64,                    /* MAT3X2  */
  YYSYMBOL_MAT3X3 = 65,                    /* MAT3X3  */
  YYSYMBOL_MAT3X4 = 66,                    /* MAT3X4  */
  YYSYMBOL_MAT4X2 = 67,                    /* MAT4X2  */
  YYSYMBOL_MAT4X3 = 68,                    /* MAT4X3  */
  YYSYMBOL_MAT4X4 = 69,                    /* MAT4X4  */
  YYSYMBOL_DMAT2X2 = 70,                   /* DMAT2X2  */
  YYSYMBOL_DMAT2X3 = 71,                   /* DMAT2X3  */
  YYSYMBOL_DMAT2X4 = 72,                   /* DMAT2X4  */
  YYSYMBOL_DMAT3X2 = 73,                   /* DMAT3X2  */
  YYSYMBOL_DMAT3X3 = 74,                   /* DMAT3X3  */
  YYSYMBOL_DMAT3X4 = 75,                   /* DMAT3X4  */
  YYSYMBOL_DMAT4X2 = 76,                   /* DMAT4X2  */
  YYSYMBOL_DMAT4X3 = 77,                   /* DMAT4X3  */
  YYSYMBOL_DMAT4X4 = 78,                   /* DMAT4X4  */
  YYSYMBOL_ATOMIC_UINT = 79,               /* ATOMIC_UINT  */
  YYSYMBOL_SAMPLER1D = 80,                 /* SAMPLER1D  */
  YYSYMBOL_SAMPLER2D = 81,                 /* SAMPLER2D  */
  YYSYMBOL_SAMPLER3D = 82,                 /* SAMPLER3D  */
  YYSYMBOL_SAMPLERCUBE = 83,               /* SAMPLERCUBE  */
  YYSYMBOL_SAMPLER1DSHADOW = 84,           /* SAMPLER1DSHADOW  */
  YYSYMBOL_SAMPLER2DSHADOW = 85,           /* SAMPLER2DSHADOW  */
  YYSYMBOL_SAMPLERCUBESHADOW = 86,         /* SAMPLERCUBESHADOW  */
  YYSYMBOL_SAMPLER1DARRAY = 87,            /* SAMPLER1DARRAY  */
  YYSYMBOL_SAMPLER2DARRAY = 88,            /* SAMPLER2DARRAY  */
  YYSYMBOL_SAMPLER1DARRAYSHADOW = 89,      /* SAMPLER1DARRAYSHADOW  */
  YYSYMBOL_SAMPLER2DARRAYSHADOW = 90,      /* SAMPLER2DARRAYSHADOW  */
  YYSYMBOL_ISAMPLER1D = 91,                /* ISAMPLER1D  */
  YYSYMBOL_ISAMPLER2D = 92,                /* ISAMPLER2D  */
  YYSYMBOL_ISAMPLER3D = 93,                /* ISAMPLER3D  */
  YYSYMBOL_ISAMPLERCUBE = 94,              /* ISAMPLERCUBE  */
  YYSYMBOL_ISAMPLER1DARRAY = 95,           /* ISAMPLER1DARRAY  */
  YYSYMBOL_ISAMPLER2DARRAY = 96,           /* ISAMPLER2DARRAY  */
  YYSYMBOL_USAMPLER1D = 97,                /* USAMPLER1D  */
  YYSYMBOL_USAMPLER2D = 98,                /* USAMPLER2D  */
  YYSYMBOL_USAMPLER3D = 99,                /* USAMPLER3D  */
  YYSYMBOL_USAMPLERCUBE = 100,             /* USAMPLERCUBE  */
  YYSYMBOL_USAMPLER1DARRAY = 101,          /* USAMPLER1DARRAY  */
  YYSYMBOL_USAMPLER2DARRAY = 102,          /* USAMPLER2DARRAY  */
  YYSYMBOL_SAMPLER2DRECT = 103,            /* SAMPLER2DRECT  */
  YYSYMBOL_SAMPLER2DRECTSHADOW = 104,      /* SAMPLER2DRECTSHADOW  */
  YYSYMBOL_ISAMPLER2DRECT = 105,           /* ISAMPLER2DRECT  */
  YYSYMBOL_USAMPLER2DRECT = 106,           /* USAMPLER2DRECT  */
  YYSYMBOL_SAMPLERBUFFER = 107,            /* SAMPLERBUFFER  */
  YYSYMBOL_ISAMPLERBUFFER = 108,           /* ISAMPLERBUFFER  */
  YYSYMBOL_USAMPLERBUFFER = 109,           /* USAMPLERBUFFER  */
  YYSYMBOL_SAMPLERCUBEARRAY = 110,         /* SAMPLERCUBEARRAY  */
  YYSYMBOL_SAMPLERCUBEARRAYSHADOW = 111,   /* SAMPLERCUBEARRAYSHADOW  */
  YYSYMBOL_ISAMPLERCUBEARRAY = 112,        /* ISAMPLERCUBEARRAY  */
  YYSYMBOL_USAMPLERCUBEARRAY = 113,        /* USAMPLERCUBEARRAY  */
  YYSYMBOL_SAMPLER2DMS = 114,              /* SAMPLER2DMS  */
  YYSYMBOL_ISAMPLER2DMS = 115,             /* ISAMPLER2DMS  */
  YYSYMBOL_USAMPLER2DMS = 116,             /* USAMPLER2DMS  */
  YYSYMBOL_SAMPLER2DMSARRAY = 117,         /* SAMPLER2DMSARRAY  */
  YYSYMBOL_ISAMPLER2DMSARRAY = 118,        /* ISAMPLER2DMSARRAY  */
  YYSYMBOL_USAMPLER2DMSARRAY = 119,        /* USAMPLER2DMSARRAY  */
  YYSYMBOL_IMAGE1D = 120,                  /* IMAGE1D  */
  YYSYMBOL_IIMAGE1D = 121,                 /* IIMAGE1D  */
  YYSYMBOL_UIMAGE1D = 122,                 /* UIMAGE1D  */
  YYSYMBOL_IMAGE2D = 123,                  /* IMAGE2D  */
  YYSYMBOL_IIMAGE2D = 124,                 /* IIMAGE2D  */
  YYSYMBOL_UIMAGE2D = 125,                 /* UIMAGE2D  */
  YYSYMBOL_IMAGE3D = 126,                  /* IMAGE3D  */
  YYSYMBOL_IIMAGE3D = 127,                 /* IIMAGE3D  */
  YYSYMBOL_UIMAGE3D = 128,                 /* UIMAGE3D  */
  YYSYMBOL_IMAGE2DRECT = 129,              /* IMAGE2DRECT  */
  YYSYMBOL_IIMAGE2DRECT = 130,             /* IIMAGE2DRECT  */
  YYSYMBOL_UIMAGE2DRECT = 131,             /* UIMAGE2DRECT  */
  YYSYMBOL_IMAGECUBE = 132,                /* IMAGECUBE  */
  YYSYMBOL_IIMAGECUBE = 133,               /* IIMAGECUBE  */
  YYSYMBOL_UIMAGECUBE = 134,               /* UIMAGECUBE  */
  YYSYMBOL_IMAGEBUFFER = 135,              /* IMAGEBUFFER  */
  YYSYMBOL_IIMAGEBUFFER = 136,             /* IIMAGEBUFFER  */
  YYSYMBOL_UIMAGEBUFFER = 137,             /* UIMAGEBUFFER  */
  YYSYMBOL_IMAGE1DARRAY = 138,             /* IMAGE1DARRAY  */
  YYSYMBOL_IIMAGE1DARRAY = 139,            /* IIMAGE1DARRAY  */
  YYSYMBOL_UIMAGE1DARRAY = 140,            /* UIMAGE1DARRAY  */
  YYSYMBOL_IMAGE2DARRAY = 141,             /* IMAGE2DARRAY  */
  YYSYMBOL_IIMAGE2DARRAY = 142,            /* IIMAGE2DARRAY  */
  YYSYMBOL_UIMAGE2DARRAY = 143,            /* UIMAGE2DARRAY  */
  YYSYMBOL_IMAGECUBEARRAY = 144,           /* IMAGECUBEARRAY  */
  YYSYMBOL_IIMAGECUBEARRAY = 145,          /* IIMAGECUBEARRAY  */
  YYSYMBOL_UIMAGECUBEARRAY = 146,          /* UIMAGECUBEARRAY  */
  YYSYMBOL_IMAGE2DMS = 147,                /* IMAGE2DMS  */
  YYSYMBOL_IIMAGE2DMS = 148,               /* IIMAGE2DMS  */
  YYSYMBOL_UIMAGE2DMS = 149,               /* UIMAGE2DMS  */
  YYSYMBOL_IMAGE2DMSARRAY = 150,           /* IMAGE2DMSARRAY  */
  YYSYMBOL_IIMAGE2DMSARRAY = 151,          /* IIMAGE2DMSARRAY  */
  YYSYMBOL_UIMAGE2DMSARRAY = 152,          /* UIMAGE2DMSARRAY  */
  YYSYMBOL_STRUCT = 153,                   /* STRUCT  */
  YYSYMBOL_VOID = 154,                     /* VOID  */
  YYSYMBOL_WHILE = 155,                    /* WHILE  */
  YYSYMBOL_IDENTIFIER = 156,               /* IDENTIFIER  */
  YYSYMBOL_FLOATCONSTANT = 157,            /* FLOATCONSTANT  */
  YYSYMBOL_DOUBLECONSTANT = 158,           /* DOUBLECONSTANT  */
  YYSYMBOL_INTCONSTANT = 159,              /* INTCONSTANT  */
  YYSYMBOL_UINTCONSTANT = 160,             /* UINTCONSTANT  */
  YYSYMBOL_TRUE_VALUE = 161,               /* TRUE_VALUE  */
  YYSYMBOL_FALSE_VALUE = 162,              /* FALSE_VALUE  */
  YYSYMBOL_LEFT_OP = 163,                  /* LEFT_OP  */
  YYSYMBOL_RIGHT_OP = 164,                 /* RIGHT_OP  */
  YYSYMBOL_INC_OP = 165,                   /* INC_OP  */
  YYSYMBOL_DEC_OP = 166,                   /* DEC_OP  */
  YYSYMBOL_LE_OP = 167,                    /* LE_OP  */
  YYSYMBOL_GE_OP = 168,                    /* GE_OP  */
  YYSYMBOL_EQ_OP = 169,                    /* EQ_OP  */
  YYSYMBOL_NE_OP = 170,                    /* NE_OP  */
  YYSYMBOL_AND_OP = 171,                   /* AND_OP  */
  YYSYMBOL_OR_OP = 172,                    /* OR_OP  */
  YYSYMBOL_XOR_OP = 173,                   /* XOR_OP  */
  YYSYMBOL_MUL_ASSIGN = 174,               /* MUL_ASSIGN  */
  YYSYMBOL_DIV_ASSIGN = 175,               /* DIV_ASSIGN  */
  YYSYMBOL_ADD_ASSIGN = 176,               /* ADD_ASSIGN  */
  YYSYMBOL_MOD_ASSIGN = 177,               /* MOD_ASSIGN  */
  YYSYMBOL_LEFT_ASSIGN = 178,              /* LEFT_ASSIGN  */
  YYSYMBOL_RIGHT_ASSIGN = 179,             /* RIGHT_ASSIGN  */
  YYSYMBOL_AND_ASSIGN = 180,               /* AND_ASSIGN  */
  YYSYMBOL_XOR_ASSIGN = 181,               /* XOR_ASSIGN  */
  YYSYMBOL_OR_ASSIGN = 182,                /* OR_ASSIGN  */
  YYSYMBOL_SUB_ASSIGN = 183,               /* SUB_ASSIGN  */
  YYSYMBOL_LEFT_PAREN = 184,               /* LEFT_PAREN  */
  YYSYMBOL_RIGHT_PAREN = 185,              /* RIGHT_PAREN  */
  YYSYMBOL_LEFT_BRACKET = 186,             /* LEFT_BRACKET  */
  YYSYMBOL_RIGHT_BRACKET = 187,            /* RIGHT_BRACKET  */
  YYSYMBOL_LEFT_BRACE = 188,               /* LEFT_BRACE  */
  YYSYMBOL_RIGHT_BRACE = 189,              /* RIGHT_BRACE  */
  YYSYMBOL_DOT = 190,                      /* DOT  */
  YYSYMBOL_COMMA = 191,                    /* COMMA  */
  YYSYMBOL_COLON = 192,                    /* COLON  */
  YYSYMBOL_EQUAL = 193,                    /* EQUAL  */
  YYSYMBOL_SEMICOLON = 194,                /* SEMICOLON  */
  YYSYMBOL_BANG = 195,                     /* BANG  */
  YYSYMBOL_DASH = 196,                     /* DASH  */
  YYSYMBOL_TILDE = 197,                    /* TILDE  */
  YYSYMBOL_PLUS = 198,                     /* PLUS  */
  YYSYMBOL_STAR = 199,                     /* STAR  */
  YYSYMBOL_SLASH = 200,                    /* SLASH  */
  YYSYMBOL_PERCENT = 201,                  /* PERCENT  */
  YYSYMBOL_LEFT_ANGLE = 202,               /* LEFT_ANGLE  */
  YYSYMBOL_RIGHT_ANGLE = 203,              /* RIGHT_ANGLE  */
  YYSYMBOL_VERTICAL_BAR = 204,             /* VERTICAL_BAR  */
  YYSYMBOL_CARET = 205,                    /* CARET  */
  YYSYMBOL_AMPERSAND = 206,                /* AMPERSAND  */
  YYSYMBOL_QUESTION = 207,                 /* QUESTION  */
  YYSYMBOL_INVARIANT = 208,                /* INVARIANT  */
  YYSYMBOL_PRECISE = 209,                  /* PRECISE  */
  YYSYMBOL_HIGHP = 210,                    /* HIGHP  */
  YYSYMBOL_MEDIUMP = 211,                  /* MEDIUMP  */
  YYSYMBOL_LOWP = 212,                     /* LOWP  */
  YYSYMBOL_PRECISION = 213,                /* PRECISION  */
  YYSYMBOL_AT = 214,                       /* AT  */
  YYSYMBOL_UNARY_PLUS = 215,               /* UNARY_PLUS  */
  YYSYMBOL_UNARY_DASH = 216,               /* UNARY_DASH  */
  YYSYMBOL_PRE_INC_OP = 217,               /* PRE_INC_OP  */
  YYSYMBOL_PRE_DEC_OP = 218,               /* PRE_DEC_OP  */
  YYSYMBOL_POST_DEC_OP = 219,              /* POST_DEC_OP  */
  YYSYMBOL_POST_INC_OP = 220,              /* POST_INC_OP  */
  YYSYMBOL_ARRAY_REF_OP = 221,             /* ARRAY_REF_OP  */
  YYSYMBOL_FUNCTION_CALL = 222,            /* FUNCTION_CALL  */
  YYSYMBOL_TYPE_NAME_LIST = 223,           /* TYPE_NAME_LIST  */
  YYSYMBOL_TYPE_SPECIFIER = 224,           /* TYPE_SPECIFIER  */
  YYSYMBOL_POSTFIX_EXPRESSION = 225,       /* POSTFIX_EXPRESSION  */
  YYSYMBOL_TYPE_QUALIFIER_LIST = 226,      /* TYPE_QUALIFIER_LIST  */
  YYSYMBOL_STRUCT_DECLARATION = 227,       /* STRUCT_DECLARATION  */
  YYSYMBOL_STRUCT_DECLARATOR = 228,        /* STRUCT_DECLARATOR  */
  YYSYMBOL_STRUCT_SPECIFIER = 229,         /* STRUCT_SPECIFIER  */
  YYSYMBOL_FUNCTION_DEFINITION = 230,      /* FUNCTION_DEFINITION  */
  YYSYMBOL_DECLARATION = 231,              /* DECLARATION  */
  YYSYMBOL_STATEMENT_LIST = 232,           /* STATEMENT_LIST  */
  YYSYMBOL_TRANSLATION_UNIT = 233,         /* TRANSLATION_UNIT  */
  YYSYMBOL_PRECISION_DECLARATION = 234,    /* PRECISION_DECLARATION  */
  YYSYMBOL_BLOCK_DECLARATION = 235,        /* BLOCK_DECLARATION  */
  YYSYMBOL_TYPE_QUALIFIER_DECLARATION = 236, /* TYPE_QUALIFIER_DECLARATION  */
  YYSYMBOL_IDENTIFIER_LIST = 237,          /* IDENTIFIER_LIST  */
  YYSYMBOL_INIT_DECLARATOR_LIST = 238,     /* INIT_DECLARATOR_LIST  */
  YYSYMBOL_FULLY_SPECIFIED_TYPE = 239,     /* FULLY_SPECIFIED_TYPE  */
  YYSYMBOL_SINGLE_DECLARATION = 240,       /* SINGLE_DECLARATION  */
  YYSYMBOL_SINGLE_INIT_DECLARATION = 241,  /* SINGLE_INIT_DECLARATION  */
  YYSYMBOL_INITIALIZER_LIST = 242,         /* INITIALIZER_LIST  */
  YYSYMBOL_EXPRESSION_STATEMENT = 243,     /* EXPRESSION_STATEMENT  */
  YYSYMBOL_SELECTION_STATEMENT = 244,      /* SELECTION_STATEMENT  */
  YYSYMBOL_SELECTION_STATEMENT_ELSE = 245, /* SELECTION_STATEMENT_ELSE  */
  YYSYMBOL_SWITCH_STATEMENT = 246,         /* SWITCH_STATEMENT  */
  YYSYMBOL_FOR_REST_STATEMENT = 247,       /* FOR_REST_STATEMENT  */
  YYSYMBOL_WHILE_STATEMENT = 248,          /* WHILE_STATEMENT  */
  YYSYMBOL_DO_STATEMENT = 249,             /* DO_STATEMENT  */
  YYSYMBOL_FOR_STATEMENT = 250,            /* FOR_STATEMENT  */
  YYSYMBOL_CASE_LABEL = 251,               /* CASE_LABEL  */
  YYSYMBOL_CONDITION_OPT = 252,            /* CONDITION_OPT  */
  YYSYMBOL_ASSIGNMENT_CONDITION = 253,     /* ASSIGNMENT_CONDITION  */
  YYSYMBOL_EXPRESSION_CONDITION = 254,     /* EXPRESSION_CONDITION  */
  YYSYMBOL_FUNCTION_HEADER = 255,          /* FUNCTION_HEADER  */
  YYSYMBOL_FUNCTION_DECLARATION = 256,     /* FUNCTION_DECLARATION  */
  YYSYMBOL_FUNCTION_PARAMETER_LIST = 257,  /* FUNCTION_PARAMETER_LIST  */
  YYSYMBOL_PARAMETER_DECLARATION = 258,    /* PARAMETER_DECLARATION  */
  YYSYMBOL_PARAMETER_DECLARATOR = 259,     /* PARAMETER_DECLARATOR  */
  YYSYMBOL_UNINITIALIZED_DECLARATION = 260, /* UNINITIALIZED_DECLARATION  */
  YYSYMBOL_ARRAY_SPECIFIER = 261,          /* ARRAY_SPECIFIER  */
  YYSYMBOL_ARRAY_SPECIFIER_LIST = 262,     /* ARRAY_SPECIFIER_LIST  */
  YYSYMBOL_STRUCT_DECLARATOR_LIST = 263,   /* STRUCT_DECLARATOR_LIST  */
  YYSYMBOL_FUNCTION_CALL_PARAMETER_LIST = 264, /* FUNCTION_CALL_PARAMETER_LIST  */
  YYSYMBOL_STRUCT_DECLARATION_LIST = 265,  /* STRUCT_DECLARATION_LIST  */
  YYSYMBOL_LAYOUT_QUALIFIER_ID = 266,      /* LAYOUT_QUALIFIER_ID  */
  YYSYMBOL_LAYOUT_QUALIFIER_ID_LIST = 267, /* LAYOUT_QUALIFIER_ID_LIST  */
  YYSYMBOL_SUBROUTINE_TYPE = 268,          /* SUBROUTINE_TYPE  */
  YYSYMBOL_PAREN_EXPRESSION = 269,         /* PAREN_EXPRESSION  */
  YYSYMBOL_INIT_DECLARATOR = 270,          /* INIT_DECLARATOR  */
  YYSYMBOL_INITIALIZER = 271,              /* INITIALIZER  */
  YYSYMBOL_TERNARY_EXPRESSION = 272,       /* TERNARY_EXPRESSION  */
  YYSYMBOL_FIELD_IDENTIFIER = 273,         /* FIELD_IDENTIFIER  */
  YYSYMBOL_NUM_GLSL_TOKEN = 274,           /* NUM_GLSL_TOKEN  */
  YYSYMBOL_YYACCEPT = 275,                 /* $accept  */
  YYSYMBOL_root = 276,                     /* root  */
  YYSYMBOL_translation_unit = 277,         /* translation_unit  */
  YYSYMBOL_block_identifier = 278,         /* block_identifier  */
  YYSYMBOL_decl_identifier = 279,          /* decl_identifier  */
  YYSYMBOL_struct_name = 280,              /* struct_name  */
  YYSYMBOL_type_name = 281,                /* type_name  */
  YYSYMBOL_param_name = 282,               /* param_name  */
  YYSYMBOL_function_name = 283,            /* function_name  */
  YYSYMBOL_field_identifier = 284,         /* field_identifier  */
  YYSYMBOL_variable_identifier = 285,      /* variable_identifier  */
  YYSYMBOL_layout_identifier = 286,        /* layout_identifier  */
  YYSYMBOL_type_specifier_identifier = 287, /* type_specifier_identifier  */
  YYSYMBOL_external_declaration = 288,     /* external_declaration  */
  YYSYMBOL_function_definition = 289,      /* function_definition  */
  YYSYMBOL_compound_statement_no_new_scope = 290, /* compound_statement_no_new_scope  */
  YYSYMBOL_statement = 291,                /* statement  */
  YYSYMBOL_statement_list = 292,           /* statement_list  */
  YYSYMBOL_compound_statement = 293,       /* compound_statement  */
  YYSYMBOL_simple_statement = 294,         /* simple_statement  */
  YYSYMBOL_declaration = 295,              /* declaration  */
  YYSYMBOL_identifier_list = 296,          /* identifier_list  */
  YYSYMBOL_init_declarator_list = 297,     /* init_declarator_list  */
  YYSYMBOL_single_declaration = 298,       /* single_declaration  */
  YYSYMBOL_initializer = 299,              /* initializer  */
  YYSYMBOL_initializer_list = 300,         /* initializer_list  */
  YYSYMBOL_expression_statement = 301,     /* expression_statement  */
  YYSYMBOL_selection_statement = 302,      /* selection_statement  */
  YYSYMBOL_switch_statement = 303,         /* switch_statement  */
  YYSYMBOL_switch_statement_list = 304,    /* switch_statement_list  */
  YYSYMBOL_case_label = 305,               /* case_label  */
  YYSYMBOL_iteration_statement = 306,      /* iteration_statement  */
  YYSYMBOL_statement_no_new_scope = 307,   /* statement_no_new_scope  */
  YYSYMBOL_for_init_statement = 308,       /* for_init_statement  */
  YYSYMBOL_conditionopt = 309,             /* conditionopt  */
  YYSYMBOL_condition = 310,                /* condition  */
  YYSYMBOL_for_rest_statement = 311,       /* for_rest_statement  */
  YYSYMBOL_jump_statement = 312,           /* jump_statement  */
  YYSYMBOL_function_prototype = 313,       /* function_prototype  */
  YYSYMBOL_function_declarator = 314,      /* function_declarator  */
  YYSYMBOL_function_parameter_list = 315,  /* function_parameter_list  */
  YYSYMBOL_parameter_declaration = 316,    /* parameter_declaration  */
  YYSYMBOL_parameter_declarator = 317,     /* parameter_declarator  */
  YYSYMBOL_function_header = 318,          /* function_header  */
  YYSYMBOL_fully_specified_type = 319,     /* fully_specified_type  */
  YYSYMBOL_parameter_type_specifier = 320, /* parameter_type_specifier  */
  YYSYMBOL_type_specifier = 321,           /* type_specifier  */
  YYSYMBOL_array_specifier_list = 322,     /* array_specifier_list  */
  YYSYMBOL_array_specifier = 323,          /* array_specifier  */
  YYSYMBOL_type_specifier_nonarray = 324,  /* type_specifier_nonarray  */
  YYSYMBOL_struct_specifier = 325,         /* struct_specifier  */
  YYSYMBOL_struct_declaration_list = 326,  /* struct_declaration_list  */
  YYSYMBOL_struct_declaration = 327,       /* struct_declaration  */
  YYSYMBOL_struct_declarator_list = 328,   /* struct_declarator_list  */
  YYSYMBOL_struct_declarator = 329,        /* struct_declarator  */
  YYSYMBOL_type_qualifier = 330,           /* type_qualifier  */
  YYSYMBOL_single_type_qualifier = 331,    /* single_type_qualifier  */
  YYSYMBOL_layout_qualifier = 332,         /* layout_qualifier  */
  YYSYMBOL_layout_qualifier_id_list = 333, /* layout_qualifier_id_list  */
  YYSYMBOL_layout_qualifier_id = 334,      /* layout_qualifier_id  */
  YYSYMBOL_precision_qualifier = 335,      /* precision_qualifier  */
  YYSYMBOL_interpolation_qualifier = 336,  /* interpolation_qualifier  */
  YYSYMBOL_invariant_qualifier = 337,      /* invariant_qualifier  */
  YYSYMBOL_precise_qualifier = 338,        /* precise_qualifier  */
  YYSYMBOL_storage_qualifier = 339,        /* storage_qualifier  */
  YYSYMBOL_type_name_list = 340,           /* type_name_list  */
  YYSYMBOL_expression = 341,               /* expression  */
  YYSYMBOL_assignment_expression = 342,    /* assignment_expression  */
  YYSYMBOL_assignment_operator = 343,      /* assignment_operator  */
  YYSYMBOL_constant_expression = 344,      /* constant_expression  */
  YYSYMBOL_conditional_expression = 345,   /* conditional_expression  */
  YYSYMBOL_logical_or_expression = 346,    /* logical_or_expression  */
  YYSYMBOL_logical_xor_expression = 347,   /* logical_xor_expression  */
  YYSYMBOL_logical_and_expression = 348,   /* logical_and_expression  */
  YYSYMBOL_inclusive_or_expression = 349,  /* inclusive_or_expression  */
  YYSYMBOL_exclusive_or_expression = 350,  /* exclusive_or_expression  */
  YYSYMBOL_and_expression = 351,           /* and_expression  */
  YYSYMBOL_equality_expression = 352,      /* equality_expression  */
  YYSYMBOL_relational_expression = 353,    /* relational_expression  */
  YYSYMBOL_shift_expression = 354,         /* shift_expression  */
  YYSYMBOL_additive_expression = 355,      /* additive_expression  */
  YYSYMBOL_multiplicative_expression = 356, /* multiplicative_expression  */
  YYSYMBOL_unary_expression = 357,         /* unary_expression  */
  YYSYMBOL_unary_operator = 358,           /* unary_operator  */
  YYSYMBOL_postfix_expression = 359,       /* postfix_expression  */
  YYSYMBOL_integer_expression = 360,       /* integer_expression  */
  YYSYMBOL_function_call = 361,            /* function_call  */
  YYSYMBOL_function_call_or_method = 362,  /* function_call_or_method  */
  YYSYMBOL_function_call_generic = 363,    /* function_call_generic  */
  YYSYMBOL_function_call_parameter_list = 364, /* function_call_parameter_list  */
  YYSYMBOL_function_identifier = 365,      /* function_identifier  */
  YYSYMBOL_primary_expression = 366        /* primary_expression  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined GLSL_LTYPE_IS_TRIVIAL && GLSL_LTYPE_IS_TRIVIAL \
             && defined GLSL_STYPE_IS_TRIVIAL && GLSL_STYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  177
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   4503

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  275
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  92
/* YYNRULES -- Number of rules.  */
#define YYNRULES  360
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  492

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   529


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int16 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274
};

#if GLSL_DEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   504,   504,   505,   508,   511,   515,   518,   521,   524,
     527,   530,   533,   536,   539,   542,   545,   546,   549,   554,
     561,   562,   565,   566,   569,   570,   573,   574,   577,   578,
     579,   580,   581,   582,   583,   586,   587,   588,   595,   605,
     615,   625,   632,   640,   650,   651,   655,   656,   664,   672,
     681,   692,   699,   706,   709,   712,   721,   722,   723,   726,
     728,   732,   733,   736,   739,   743,   747,   748,   751,   752,
     755,   758,   761,   765,   766,   769,   770,   773,   774,   777,
     780,   784,   787,   791,   794,   797,   800,   803,   807,   810,
     816,   823,   826,   830,   833,   839,   842,   849,   852,   856,
     860,   866,   870,   874,   880,   884,   887,   891,   894,   898,
     899,   900,   901,   902,   903,   904,   905,   906,   907,   908,
     909,   910,   911,   912,   913,   914,   915,   916,   917,   918,
     919,   920,   921,   922,   923,   924,   925,   926,   927,   928,
     929,   930,   931,   932,   933,   934,   935,   936,   937,   938,
     939,   940,   941,   942,   943,   944,   945,   946,   947,   948,
     949,   950,   951,   952,   953,   954,   955,   956,   957,   958,
     959,   960,   961,   962,   963,   964,   965,   966,   967,   968,
     969,   970,   971,   972,   973,   974,   975,   976,   977,   978,
     979,   980,   981,   982,   983,   984,   985,   986,   987,   988,
     989,   990,   991,   992,   993,   994,   995,   996,   997,   998,
     999,  1000,  1001,  1002,  1003,  1004,  1005,  1006,  1007,  1008,
    1009,  1010,  1011,  1012,  1013,  1014,  1015,  1016,  1017,  1018,
    1021,  1024,  1031,  1033,  1037,  1044,  1048,  1051,  1055,  1058,
    1062,  1064,  1068,  1069,  1070,  1071,  1072,  1073,  1076,  1079,
    1081,  1085,  1088,  1091,  1095,  1096,  1097,  1100,  1101,  1102,
    1105,  1108,  1111,  1112,  1113,  1114,  1115,  1116,  1117,  1118,
    1119,  1120,  1121,  1122,  1123,  1124,  1125,  1126,  1127,  1133,
    1134,  1138,  1139,  1143,  1144,  1148,  1149,  1150,  1151,  1152,
    1153,  1154,  1155,  1156,  1157,  1158,  1161,  1164,  1165,  1169,
    1170,  1174,  1175,  1179,  1180,  1184,  1185,  1189,  1190,  1194,
    1195,  1199,  1201,  1204,  1208,  1210,  1213,  1216,  1219,  1223,
    1225,  1228,  1232,  1234,  1237,  1241,  1243,  1246,  1249,  1253,
    1255,  1258,  1261,  1265,  1266,  1267,  1268,  1271,  1273,  1276,
    1278,  1281,  1284,  1288,  1291,  1294,  1297,  1300,  1306,  1313,
    1316,  1320,  1322,  1326,  1328,  1331,  1334,  1337,  1340,  1343,
    1346
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if GLSL_DEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "CONST", "BOOL",
  "FLOAT", "DOUBLE", "INT", "UINT", "BREAK", "CONTINUE", "DO", "ELSE",
  "FOR", "IF", "DISCARD", "RETURN", "RETURN_VALUE", "SWITCH", "CASE",
  "DEFAULT", "SUBROUTINE", "BVEC2", "BVEC3", "BVEC4", "IVEC2", "IVEC3",
  "IVEC4", "UVEC2", "UVEC3", "UVEC4", "VEC2", "VEC3", "VEC4", "MAT2",
  "MAT3", "MAT4", "CENTROID", "IN", "OUT", "INOUT", "UNIFORM", "PATCH",
  "SAMPLE", "BUFFER", "SHARED", "COHERENT", "VOLATILE", "RESTRICT",
  "READONLY", "WRITEONLY", "DVEC2", "DVEC3", "DVEC4", "DMAT2", "DMAT3",
  "DMAT4", "NOPERSPECTIVE", "FLAT", "SMOOTH", "LAYOUT", "MAT2X2", "MAT2X3",
  "MAT2X4", "MAT3X2", "MAT3X3", "MAT3X4", "MAT4X2", "MAT4X3", "MAT4X4",
  "DMAT2X2", "DMAT2X3", "DMAT2X4", "DMAT3X2", "DMAT3X3", "DMAT3X4",
  "DMAT4X2", "DMAT4X3", "DMAT4X4", "ATOMIC_UINT", "SAMPLER1D", "SAMPLER2D",
  "SAMPLER3D", "SAMPLERCUBE", "SAMPLER1DSHADOW", "SAMPLER2DSHADOW",
  "SAMPLERCUBESHADOW", "SAMPLER1DARRAY", "SAMPLER2DARRAY",
  "SAMPLER1DARRAYSHADOW", "SAMPLER2DARRAYSHADOW", "ISAMPLER1D",
  "ISAMPLER2D", "ISAMPLER3D", "ISAMPLERCUBE", "ISAMPLER1DARRAY",
  "ISAMPLER2DARRAY", "USAMPLER1D", "USAMPLER2D", "USAMPLER3D",
  "USAMPLERCUBE", "USAMPLER1DARRAY", "USAMPLER2DARRAY", "SAMPLER2DRECT",
  "SAMPLER2DRECTSHADOW", "ISAMPLER2DRECT", "USAMPLER2DRECT",
  "SAMPLERBUFFER", "ISAMPLERBUFFER", "USAMPLERBUFFER", "SAMPLERCUBEARRAY",
  "SAMPLERCUBEARRAYSHADOW", "ISAMPLERCUBEARRAY", "USAMPLERCUBEARRAY",
  "SAMPLER2DMS", "ISAMPLER2DMS", "USAMPLER2DMS", "SAMPLER2DMSARRAY",
  "ISAMPLER2DMSARRAY", "USAMPLER2DMSARRAY", "IMAGE1D", "IIMAGE1D",
  "UIMAGE1D", "IMAGE2D", "IIMAGE2D", "UIMAGE2D", "IMAGE3D", "IIMAGE3D",
  "UIMAGE3D", "IMAGE2DRECT", "IIMAGE2DRECT", "UIMAGE2DRECT", "IMAGECUBE",
  "IIMAGECUBE", "UIMAGECUBE", "IMAGEBUFFER", "IIMAGEBUFFER",
  "UIMAGEBUFFER", "IMAGE1DARRAY", "IIMAGE1DARRAY", "UIMAGE1DARRAY",
  "IMAGE2DARRAY", "IIMAGE2DARRAY", "UIMAGE2DARRAY", "IMAGECUBEARRAY",
  "IIMAGECUBEARRAY", "UIMAGECUBEARRAY", "IMAGE2DMS", "IIMAGE2DMS",
  "UIMAGE2DMS", "IMAGE2DMSARRAY", "IIMAGE2DMSARRAY", "UIMAGE2DMSARRAY",
  "STRUCT", "VOID", "WHILE", "IDENTIFIER", "FLOATCONSTANT",
  "DOUBLECONSTANT", "INTCONSTANT", "UINTCONSTANT", "TRUE_VALUE",
  "FALSE_VALUE", "LEFT_OP", "RIGHT_OP", "INC_OP", "DEC_OP", "LE_OP",
  "GE_OP", "EQ_OP", "NE_OP", "AND_OP", "OR_OP", "XOR_OP", "MUL_ASSIGN",
  "DIV_ASSIGN", "ADD_ASSIGN", "MOD_ASSIGN", "LEFT_ASSIGN", "RIGHT_ASSIGN",
  "AND_ASSIGN", "XOR_ASSIGN", "OR_ASSIGN", "SUB_ASSIGN", "LEFT_PAREN",
  "RIGHT_PAREN", "LEFT_BRACKET", "RIGHT_BRACKET", "LEFT_BRACE",
  "RIGHT_BRACE", "DOT", "COMMA", "COLON", "EQUAL", "SEMICOLON", "BANG",
  "DASH", "TILDE", "PLUS", "STAR", "SLASH", "PERCENT", "LEFT_ANGLE",
  "RIGHT_ANGLE", "VERTICAL_BAR", "CARET", "AMPERSAND", "QUESTION",
  "INVARIANT", "PRECISE", "HIGHP", "MEDIUMP", "LOWP", "PRECISION", "AT",
  "UNARY_PLUS", "UNARY_DASH", "PRE_INC_OP", "PRE_DEC_OP", "POST_DEC_OP",
  "POST_INC_OP", "ARRAY_REF_OP", "FUNCTION_CALL", "TYPE_NAME_LIST",
  "TYPE_SPECIFIER", "POSTFIX_EXPRESSION", "TYPE_QUALIFIER_LIST",
  "STRUCT_DECLARATION", "STRUCT_DECLARATOR", "STRUCT_SPECIFIER",
  "FUNCTION_DEFINITION", "DECLARATION", "STATEMENT_LIST",
  "TRANSLATION_UNIT", "PRECISION_DECLARATION", "BLOCK_DECLARATION",
  "TYPE_QUALIFIER_DECLARATION", "IDENTIFIER_LIST", "INIT_DECLARATOR_LIST",
  "FULLY_SPECIFIED_TYPE", "SINGLE_DECLARATION", "SINGLE_INIT_DECLARATION",
  "INITIALIZER_LIST", "EXPRESSION_STATEMENT", "SELECTION_STATEMENT",
  "SELECTION_STATEMENT_ELSE", "SWITCH_STATEMENT", "FOR_REST_STATEMENT",
  "WHILE_STATEMENT", "DO_STATEMENT", "FOR_STATEMENT", "CASE_LABEL",
  "CONDITION_OPT", "ASSIGNMENT_CONDITION", "EXPRESSION_CONDITION",
  "FUNCTION_HEADER", "FUNCTION_DECLARATION", "FUNCTION_PARAMETER_LIST",
  "PARAMETER_DECLARATION", "PARAMETER_DECLARATOR",
  "UNINITIALIZED_DECLARATION", "ARRAY_SPECIFIER", "ARRAY_SPECIFIER_LIST",
  "STRUCT_DECLARATOR_LIST", "FUNCTION_CALL_PARAMETER_LIST",
  "STRUCT_DECLARATION_LIST", "LAYOUT_QUALIFIER_ID",
  "LAYOUT_QUALIFIER_ID_LIST", "SUBROUTINE_TYPE", "PAREN_EXPRESSION",
  "INIT_DECLARATOR", "INITIALIZER", "TERNARY_EXPRESSION",
  "FIELD_IDENTIFIER", "NUM_GLSL_TOKEN", "$accept", "root",
  "translation_unit", "block_identifier", "decl_identifier", "struct_name",
  "type_name", "param_name", "function_name", "field_identifier",
  "variable_identifier", "layout_identifier", "type_specifier_identifier",
  "external_declaration", "function_definition",
  "compound_statement_no_new_scope", "statement", "statement_list",
  "compound_statement", "simple_statement", "declaration",
  "identifier_list", "init_declarator_list", "single_declaration",
  "initializer", "initializer_list", "expression_statement",
  "selection_statement", "switch_statement", "switch_statement_list",
  "case_label", "iteration_statement", "statement_no_new_scope",
  "for_init_statement", "conditionopt", "condition", "for_rest_statement",
  "jump_statement", "function_prototype", "function_declarator",
  "function_parameter_list", "parameter_declaration",
  "parameter_declarator", "function_header", "fully_specified_type",
  "parameter_type_specifier", "type_specifier", "array_specifier_list",
  "array_specifier", "type_specifier_nonarray", "struct_specifier",
  "struct_declaration_list", "struct_declaration",
  "struct_declarator_list", "struct_declarator", "type_qualifier",
  "single_type_qualifier", "layout_qualifier", "layout_qualifier_id_list",
  "layout_qualifier_id", "precision_qualifier", "interpolation_qualifier",
  "invariant_qualifier", "precise_qualifier", "storage_qualifier",
  "type_name_list", "expression", "assignment_expression",
  "assignment_operator", "constant_expression", "conditional_expression",
  "logical_or_expression", "logical_xor_expression",
  "logical_and_expression", "inclusive_or_expression",
  "exclusive_or_expression", "and_expression", "equality_expression",
  "relational_expression", "shift_expression", "additive_expression",
  "multiplicative_expression", "unary_expression", "unary_operator",
  "postfix_expression", "integer_expression", "function_call",
  "function_call_or_method", "function_call_generic",
  "function_call_parameter_list", "function_identifier",
  "primary_expression", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-429)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-353)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    1930,  -429,  -429,  -429,  -429,  -429,  -429,  -119,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -106,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -133,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,   -67,    34,  1930,
    -429,  -429,  -429,  -429,  -118,  -429,  -159,  -147,  3087,  -105,
    -429,   -76,  -429,  2319,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,   -62,   -28,  -429,  3087,   -48,  4347,  -429,  -429,   -39,
    -429,   483,  -429,  -429,  -429,   -44,  -429,  -429,  -429,     8,
    3087,   -13,  -154,    -9,  3435,   -76,  -429,  -134,  -429,   -11,
    -102,  -429,  -429,  -429,  -429,  -117,  -429,  -429,    -2,   -92,
    -429,    36,  2511,  -429,  3087,  3087,    -1,  -429,  -140,     5,
       7,  1327,    11,    12,     9,  3619,    18,  3985,    13,    20,
      38,  -429,  -429,  -429,  -429,  -429,  -429,  3985,  3985,  3985,
     694,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,   905,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,    14,
      22,   -73,  -429,  -429,  -151,    37,    42,     3,    15,    16,
     -14,  -143,     2,   -50,   -47,   -95,  3985,  -123,  -429,  -429,
    -429,    25,  -429,  3087,  -429,   -76,  -429,  -429,  3800,  -122,
    -429,  -429,  -429,    29,  -429,  -429,  -429,  3087,   -39,  -429,
     -61,  -429,   -62,  3985,  -429,   -28,  -429,   -76,   -60,  -429,
    -429,  -429,    36,  2703,  -429,  3800,  -116,  -429,  -429,    62,
    1734,  3985,  -429,  -429,   -55,  3985,   -89,  -429,  2123,  -429,
    -429,   -80,  -429,  1116,  -429,  -429,  3985,  -429,  3985,  3985,
    3985,  3985,  3985,  3985,  3985,  3985,  3985,  3985,  3985,  3985,
    3985,  3985,  3985,  3985,  3985,  3985,  3985,  3985,  -429,  -429,
    -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  -429,  3985,
    -429,  -429,  -429,  3985,    36,  4166,  -429,   -76,  3800,  -429,
    -429,  3800,  -429,  2895,  -429,   -39,  -429,  -429,  -429,  -429,
     -76,    36,  -429,   -53,  -429,  -429,  3800,    35,  -429,  -429,
    2123,   -79,  -429,   -71,  -429,    33,    67,  3087,    39,  -429,
    -429,  -429,    37,   -22,    42,     3,    15,    16,   -14,  -143,
    -143,     2,     2,     2,     2,   -50,   -50,   -47,   -47,  -429,
    -429,  -429,  -429,    39,    40,  -429,    41,  3985,  -429,   -59,
    -429,   -29,  -429,  -136,  -429,  -429,  -429,  -429,  3985,    30,
    -429,    43,  1327,    44,  1538,  -429,    46,  3985,  -429,  -429,
    -429,  3985,  -429,  3249,  -429,  -150,   -56,  3985,  1538,   217,
    1327,  -429,  -429,  -429,  3800,  -429,  -429,  -429,  -429,  -429,
    -149,    49,    39,  -429,  1327,  1327,    55,  -429,  -429,  -429,
    -429,  -429
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       2,   262,   114,   110,   111,   112,   113,   277,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   115,   116,   117,
     130,   131,   132,   266,   264,   265,   263,   269,   267,   268,
     270,   271,   272,   273,   274,   275,   276,   118,   119,   120,
     142,   143,   144,   259,   258,   257,     0,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   145,   146,   147,   148,
     149,   150,   151,   152,   153,   154,   155,   156,   157,   158,
     159,   160,   161,   162,   163,   164,   165,   168,   169,   170,
     171,   172,   173,   175,   176,   177,   178,   179,   180,   182,
     183,   184,   185,   186,   187,   188,   166,   167,   174,   181,
     189,   190,   191,   192,   193,   194,   195,   196,   197,   198,
     199,   200,   201,   202,   203,   204,   205,   206,   207,   208,
     209,   210,   211,   212,   213,   214,   215,   216,   217,   218,
     219,   220,   221,   222,   223,   224,   225,   226,   227,     0,
     109,    15,   260,   261,   254,   255,   256,     0,     0,     3,
     229,     4,    16,    17,     0,    46,    19,     0,    89,    51,
     100,   103,   228,     0,   240,   243,   244,   245,   246,   247,
     242,     0,     0,     8,     0,     0,     0,     1,     5,     0,
      36,     0,    35,    18,    88,    90,    91,    94,    96,   102,
       0,     7,    52,     0,     0,   104,   105,     9,    41,     0,
       0,   101,   241,     9,   279,     0,   253,    14,   251,     0,
     249,     0,     0,   232,     0,     0,     0,     7,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      13,   356,   359,   354,   355,   357,   358,     0,     0,     0,
       0,    20,    61,   335,   334,   336,   333,   353,    24,     0,
      22,    23,    28,    29,    30,    31,    32,    33,    34,     0,
     100,     0,   281,   283,   297,   299,   301,   303,   305,   307,
     309,   311,   314,   319,   322,   325,     0,   329,   339,   344,
     345,     0,   337,     0,    10,    97,    93,    95,     0,    53,
      99,   107,   351,     0,   296,   325,   106,     0,     0,    42,
       0,   278,     0,     0,   248,     0,    12,   238,     0,   236,
     231,   233,     0,     0,    37,     0,    48,    84,    83,     0,
       0,     0,    87,    85,     0,     0,     0,    69,     0,   330,
     331,     0,    26,     0,    21,    25,     0,    62,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   286,   287,
     289,   288,   291,   292,   293,   294,   295,   290,   285,     0,
     332,   341,   342,     0,     0,     0,    92,    98,     0,    55,
      56,     0,   108,     0,    44,     0,    43,   280,   252,   250,
     239,     0,   234,     0,   230,    50,     0,     0,    76,    75,
      78,     0,    86,     0,    68,     0,     0,     0,    79,   360,
      27,   282,   300,     0,   302,   304,   306,   308,   310,   312,
     313,   317,   318,   315,   316,   320,   321,   324,   323,   326,
     327,   328,   284,   343,     0,   340,   109,   347,   349,     0,
      59,     0,    54,     0,    45,   237,   235,    49,     0,     0,
      77,     0,     0,     0,     0,    13,     0,     0,   338,   348,
     346,     0,    57,     0,    38,     0,     0,    81,     0,    63,
      66,    73,    74,    70,     0,   298,   350,    58,    60,    39,
       0,     0,    82,    72,     0,    67,     0,    80,    40,    71,
      64,    65
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -429,  -429,  -429,  -429,  -174,  -429,  -160,  -429,  -429,  -141,
    -175,  -429,  -429,    86,  -429,    89,  -218,  -232,  -429,  -428,
       1,  -429,  -429,  -429,  -306,  -429,   -74,  -429,  -429,  -429,
    -429,  -429,  -221,  -429,  -429,  -152,  -429,  -429,    10,  -429,
    -429,   -32,    63,  -429,  -309,    66,     0,  -188,  -193,  -429,
    -429,  -197,  -205,   -54,  -139,  -146,  -157,  -429,  -429,   -46,
     110,  -429,  -429,  -429,  -429,  -429,  -212,  -274,  -429,   -43,
    -178,  -429,   -77,   -78,   -70,   -72,   -75,   -81,  -173,  -300,
    -172,  -168,     4,  -429,  -429,  -429,  -429,  -429,  -429,  -429,
    -429,  -429
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,   148,   149,   199,   192,   175,   200,   285,   193,   307,
     247,   208,   150,   151,   152,   471,   248,   249,   250,   251,
     252,   300,   154,   155,   379,   441,   253,   254,   255,   486,
     256,   257,   473,   400,   449,   405,   451,   258,   259,   157,
     185,   186,   187,   158,   159,   188,   292,   195,   196,   161,
     162,   212,   213,   308,   309,   163,   164,   165,   209,   210,
     166,   167,   168,   169,   170,   205,   261,   262,   369,   293,
     263,   264,   265,   266,   267,   268,   269,   270,   271,   272,
     273,   274,   275,   276,   277,   434,   278,   279,   280,   439,
     281,   282
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     160,   153,   296,   319,   289,   218,   202,   311,   333,   395,
     156,   204,   190,   324,   380,   326,   294,   206,   313,   406,
     217,   338,   -15,   173,   347,   348,   472,   331,   214,   181,
     316,   335,   194,   202,   177,   182,   194,   194,   184,   288,
     472,   380,   371,   372,   479,   488,   194,   421,   422,   423,
     424,   191,   -15,   315,    -6,   174,   339,   202,   464,   349,
     350,  -352,   411,   373,   194,   171,   214,   374,   301,   214,
     194,   381,   440,   179,   302,   442,   180,   396,   172,   358,
     359,   360,   361,   362,   363,   364,   365,   366,   367,   298,
     447,   406,   299,   304,   203,   432,   296,   377,   368,   305,
     383,   438,   336,   404,   380,   409,   452,   380,   311,   401,
     194,   336,   336,   403,   453,   335,   408,   217,   336,   390,
     336,   337,   380,   296,   384,   294,   460,   413,   207,   481,
     385,   391,   461,   386,   392,   336,   336,   190,   391,   402,
     215,   446,   387,   144,   145,   146,   353,   283,   354,   160,
     153,   214,   355,   356,   357,   345,   346,   478,   189,   156,
     462,   433,   463,   201,   284,   351,   352,   214,   487,   336,
     457,   -11,   419,   420,   211,   290,   216,   297,   311,   425,
     426,   260,   407,   475,   296,   427,   428,   476,   408,   380,
     189,   303,   306,   314,   -15,   320,   321,   296,   295,   317,
     380,   318,   325,   322,   328,   327,  -351,   342,   182,   375,
     340,   444,   211,   341,   312,   211,   382,   397,   454,   448,
     343,   260,   344,   455,   467,   331,   459,   458,   468,   484,
     336,   456,   470,   435,   469,   178,   466,   214,   485,   474,
     260,   329,   330,   489,   491,   183,   399,   483,   450,   260,
     202,   376,   445,   286,   407,   482,   287,   176,   393,   389,
     388,   412,   414,   418,     0,     0,   490,   335,   417,   465,
     416,   415,     0,     0,     0,     0,     0,   480,     0,     0,
     370,     0,     0,   189,     0,     0,     0,   296,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   211,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   295,     0,     0,
       0,     0,     0,   211,     0,     0,     0,     0,     0,     0,
     260,   398,     0,     0,     0,     0,     0,     0,   260,     0,
       0,     0,     0,   260,     0,     0,     0,     0,     0,     0,
       0,     0,   295,     0,   295,   295,   295,   295,   295,   295,
     295,   295,   295,   295,   295,   295,   295,   295,   295,   429,
     430,   431,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   211,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     260,     0,     0,     0,     0,     0,     0,   201,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   260,     0,   260,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   260,     0,
     260,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   260,   260,     1,     2,     3,     4,
       5,     6,   219,   220,   221,     0,   222,   223,   224,   225,
       0,   226,   227,   228,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   229,   230,
     231,   232,   233,   234,   235,   236,     0,     0,   237,   238,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   239,     0,     0,
       0,   240,   241,     0,     0,     0,     0,   242,   243,   244,
     245,   246,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   142,   143,   144,   145,   146,   147,     1,     2,     3,
       4,     5,     6,   219,   220,   221,     0,   222,   223,   224,
     225,     0,   226,   227,   228,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   229,
     230,   231,   232,   233,   234,   235,   236,     0,     0,   237,
     238,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   239,     0,
       0,     0,   240,   332,     0,     0,     0,     0,   242,   243,
     244,   245,   246,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   142,   143,   144,   145,   146,   147,     1,     2,
       3,     4,     5,     6,   219,   220,   221,     0,   222,   223,
     224,   225,     0,   226,   227,   228,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     229,   230,   231,   232,   233,   234,   235,   236,     0,     0,
     237,   238,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   239,
       0,     0,     0,   240,   334,     0,     0,     0,     0,   242,
     243,   244,   245,   246,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   142,   143,   144,   145,   146,   147,     1,
       2,     3,     4,     5,     6,   219,   220,   221,     0,   222,
     223,   224,   225,     0,   226,   227,   228,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   229,   230,   231,   232,   233,   234,   235,   236,     0,
       0,   237,   238,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     239,     0,     0,     0,   240,   410,     0,     0,     0,     0,
     242,   243,   244,   245,   246,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   142,   143,   144,   145,   146,   147,
       1,     2,     3,     4,     5,     6,   219,   220,   221,     0,
     222,   223,   224,   225,     0,   226,   227,   228,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   229,   230,   231,   232,   233,   234,   235,   236,
       0,     0,   237,   238,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   239,     0,     0,     0,   240,     0,     0,     0,     0,
       0,   242,   243,   244,   245,   246,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   142,   143,   144,   145,   146,
     147,     1,     2,     3,     4,     5,     6,   219,   220,   221,
       0,   222,   223,   224,   225,     0,   226,   227,   228,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   229,   230,   231,   232,   233,   234,   235,
     236,     0,     0,   237,   238,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   239,     0,     0,     0,   181,     0,     0,     0,
       0,     0,   242,   243,   244,   245,   246,     1,     2,     3,
       4,     5,     6,     0,     0,     0,   142,   143,   144,   145,
     146,   147,     0,     0,     0,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,     0,
     230,   231,   232,   233,   234,   235,   236,     0,     0,   237,
     238,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   239,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   242,   243,
     244,   245,   246,     1,     2,     3,     4,     5,     6,     0,
       0,     0,   142,   143,   144,   145,   146,   147,     0,     0,
       0,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,     0,   141,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     1,     2,     3,     4,
       5,     6,     0,     0,     0,     0,     0,     0,   142,   143,
     144,   145,   146,   147,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,     0,   230,
     231,   232,   233,   234,   235,   236,     0,     0,   237,   238,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   239,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   243,   244,
     245,   246,     1,     2,     3,     4,     5,     6,     0,     0,
       0,   142,   143,   144,   145,   146,     0,     0,     0,     0,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,     0,   197,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   198,     1,     2,     3,     4,     5,     6,
       0,     0,     0,     0,     0,     0,     0,   142,   143,   144,
     145,   146,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,     0,   141,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     310,     0,     0,     0,     0,     0,     1,     2,     3,     4,
       5,     6,     0,     0,     0,     0,     0,     0,     0,   142,
     143,   144,   145,   146,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,     0,   141,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   394,     0,     0,     0,     0,     0,     1,     2,
       3,     4,     5,     6,     0,     0,     0,     0,     0,     0,
       0,   142,   143,   144,   145,   146,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
       0,   141,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   443,     0,     0,     0,     0,     0,
       1,     2,     3,     4,     5,     6,     0,     0,     0,     0,
       0,     0,     0,   142,   143,   144,   145,   146,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,     0,   141,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     2,     3,     4,     5,     6,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   142,   143,   144,   145,   146,
      37,    38,    39,    40,    41,    42,     0,     0,     0,     0,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,     0,   230,   231,   232,   233,   234,
     235,   236,     0,     0,   237,   238,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   239,     0,     0,     0,   378,   477,     2,
       3,     4,     5,     6,   243,   244,   245,   246,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    37,    38,    39,    40,
      41,    42,     0,     0,     0,     0,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
       0,   230,   231,   232,   233,   234,   235,   236,     0,     0,
     237,   238,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   239,
       0,     0,   291,     2,     3,     4,     5,     6,     0,     0,
     243,   244,   245,   246,     0,     0,     0,     0,     0,     0,
       0,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      37,    38,    39,    40,    41,    42,     0,     0,     0,     0,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,     0,   230,   231,   232,   233,   234,
     235,   236,     0,     0,   237,   238,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   239,     2,     3,     4,     5,     6,     0,
       0,     0,     0,   323,   243,   244,   245,   246,     0,     0,
       0,     0,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    37,    38,    39,    40,    41,    42,     0,     0,     0,
       0,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,     0,   230,   231,   232,   233,
     234,   235,   236,     0,     0,   237,   238,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   239,     0,     0,     0,   378,     2,
       3,     4,     5,     6,     0,   243,   244,   245,   246,     0,
       0,     0,     0,     0,     0,     0,     0,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    37,    38,    39,    40,
      41,    42,     0,     0,     0,     0,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
       0,   230,   231,   232,   233,   234,   235,   236,     0,     0,
     237,   238,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   239,
       2,     3,     4,     5,     6,     0,     0,     0,     0,     0,
     243,   244,   245,   246,     0,     0,     0,     0,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    37,    38,    39,
      40,    41,    42,     0,     0,     0,     0,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     436,     0,   230,   231,   232,   233,   234,   235,   236,     0,
       0,   237,   238,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     437,     2,     3,     4,     5,     6,     0,     0,     0,     0,
       0,   243,   244,   245,   246,     0,     0,     0,     0,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    37,    38,
      39,    40,    41,    42,     0,     0,     0,     0,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,     0,   141
};

static const yytype_int16 yycheck[] =
{
       0,     0,   195,   221,   192,   179,   163,   212,   240,   315,
       0,   171,   158,   225,   288,   227,   194,    45,   215,   328,
     156,   172,   156,   156,   167,   168,   454,   239,   174,   188,
     218,   249,   186,   190,     0,   194,   186,   186,   185,   193,
     468,   315,   165,   166,   194,   194,   186,   347,   348,   349,
     350,   156,   186,   193,   188,   188,   207,   214,   194,   202,
     203,   184,   336,   186,   186,   184,   212,   190,   185,   215,
     186,   193,   378,   191,   191,   381,   194,   193,   184,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   191,
     396,   400,   194,   185,   156,   369,   289,   285,   193,   191,
     297,   375,   191,   192,   378,   185,   185,   381,   313,   321,
     186,   191,   191,   325,   185,   333,   328,   156,   191,   307,
     191,   194,   396,   316,   298,   303,   185,   339,   156,   185,
     191,   191,   191,   194,   194,   191,   191,   283,   191,   194,
     188,   194,   302,   210,   211,   212,   196,   191,   198,   149,
     149,   297,   199,   200,   201,   169,   170,   463,   158,   149,
     189,   373,   191,   163,   156,   163,   164,   313,   474,   191,
     192,   184,   345,   346,   174,   184,   176,   188,   383,   351,
     352,   181,   328,   457,   377,   353,   354,   461,   400,   463,
     190,   193,   156,   194,   156,   184,   184,   390,   194,   194,
     474,   194,   184,   194,   184,   192,   184,   204,   194,   184,
     173,   385,   212,   171,   214,   215,   187,   155,   185,   184,
     205,   221,   206,   156,   194,   437,   185,   187,   185,    12,
     191,   406,   188,   374,   452,   149,   448,   383,   470,   193,
     240,   237,   238,   194,   189,   156,   320,   468,   400,   249,
     407,   283,   391,   190,   400,   467,   190,   147,   312,   305,
     303,   338,   340,   344,    -1,    -1,   484,   485,   343,   443,
     342,   341,    -1,    -1,    -1,    -1,    -1,   465,    -1,    -1,
     276,    -1,    -1,   283,    -1,    -1,    -1,   480,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   297,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   303,    -1,    -1,
      -1,    -1,    -1,   313,    -1,    -1,    -1,    -1,    -1,    -1,
     320,   320,    -1,    -1,    -1,    -1,    -1,    -1,   328,    -1,
      -1,    -1,    -1,   333,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   338,    -1,   340,   341,   342,   343,   344,   345,
     346,   347,   348,   349,   350,   351,   352,   353,   354,   355,
     356,   357,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   383,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     400,    -1,    -1,    -1,    -1,    -1,    -1,   407,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   452,    -1,   454,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   468,    -1,
     470,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   484,   485,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    -1,    13,    14,    15,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,   161,   162,    -1,    -1,   165,   166,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   184,    -1,    -1,
      -1,   188,   189,    -1,    -1,    -1,    -1,   194,   195,   196,
     197,   198,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   208,   209,   210,   211,   212,   213,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    -1,    13,    14,    15,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,    -1,    -1,   165,
     166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   184,    -1,
      -1,    -1,   188,   189,    -1,    -1,    -1,    -1,   194,   195,
     196,   197,   198,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   208,   209,   210,   211,   212,   213,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    -1,    13,    14,
      15,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,    -1,    -1,
     165,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   184,
      -1,    -1,    -1,   188,   189,    -1,    -1,    -1,    -1,   194,
     195,   196,   197,   198,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   208,   209,   210,   211,   212,   213,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    -1,    13,
      14,    15,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,    -1,
      -1,   165,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     184,    -1,    -1,    -1,   188,   189,    -1,    -1,    -1,    -1,
     194,   195,   196,   197,   198,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   208,   209,   210,   211,   212,   213,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    -1,
      13,    14,    15,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     153,   154,   155,   156,   157,   158,   159,   160,   161,   162,
      -1,    -1,   165,   166,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   184,    -1,    -1,    -1,   188,    -1,    -1,    -1,    -1,
      -1,   194,   195,   196,   197,   198,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   208,   209,   210,   211,   212,
     213,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      -1,    13,    14,    15,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,    -1,    -1,   165,   166,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   184,    -1,    -1,    -1,   188,    -1,    -1,    -1,
      -1,    -1,   194,   195,   196,   197,   198,     3,     4,     5,
       6,     7,     8,    -1,    -1,    -1,   208,   209,   210,   211,
     212,   213,    -1,    -1,    -1,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,    -1,
     156,   157,   158,   159,   160,   161,   162,    -1,    -1,   165,
     166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   184,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   194,   195,
     196,   197,   198,     3,     4,     5,     6,     7,     8,    -1,
      -1,    -1,   208,   209,   210,   211,   212,   213,    -1,    -1,
      -1,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,    -1,   156,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,     8,    -1,    -1,    -1,    -1,    -1,    -1,   208,   209,
     210,   211,   212,   213,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,    -1,   156,
     157,   158,   159,   160,   161,   162,    -1,    -1,   165,   166,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   184,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   195,   196,
     197,   198,     3,     4,     5,     6,     7,     8,    -1,    -1,
      -1,   208,   209,   210,   211,   212,    -1,    -1,    -1,    -1,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,    -1,   156,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   194,     3,     4,     5,     6,     7,     8,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   208,   209,   210,
     211,   212,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   151,   152,   153,   154,    -1,   156,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     189,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,     8,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   208,
     209,   210,   211,   212,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,    -1,   156,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   189,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,     8,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   208,   209,   210,   211,   212,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
      -1,   156,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   189,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   208,   209,   210,   211,   212,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     153,   154,    -1,   156,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     4,     5,     6,     7,     8,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   208,   209,   210,   211,   212,
      51,    52,    53,    54,    55,    56,    -1,    -1,    -1,    -1,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,    -1,   156,   157,   158,   159,   160,
     161,   162,    -1,    -1,   165,   166,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   184,    -1,    -1,    -1,   188,   189,     4,
       5,     6,     7,     8,   195,   196,   197,   198,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    51,    52,    53,    54,
      55,    56,    -1,    -1,    -1,    -1,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
      -1,   156,   157,   158,   159,   160,   161,   162,    -1,    -1,
     165,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   184,
      -1,    -1,   187,     4,     5,     6,     7,     8,    -1,    -1,
     195,   196,   197,   198,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      51,    52,    53,    54,    55,    56,    -1,    -1,    -1,    -1,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,    -1,   156,   157,   158,   159,   160,
     161,   162,    -1,    -1,   165,   166,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   184,     4,     5,     6,     7,     8,    -1,
      -1,    -1,    -1,   194,   195,   196,   197,   198,    -1,    -1,
      -1,    -1,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    51,    52,    53,    54,    55,    56,    -1,    -1,    -1,
      -1,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,    -1,   156,   157,   158,   159,
     160,   161,   162,    -1,    -1,   165,   166,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   184,    -1,    -1,    -1,   188,     4,
       5,     6,     7,     8,    -1,   195,   196,   197,   198,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    51,    52,    53,    54,
      55,    56,    -1,    -1,    -1,    -1,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
      -1,   156,   157,   158,   159,   160,   161,   162,    -1,    -1,
     165,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   184,
       4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,    -1,
     195,   196,   197,   198,    -1,    -1,    -1,    -1,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    51,    52,    53,
      54,    55,    56,    -1,    -1,    -1,    -1,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,    -1,   156,   157,   158,   159,   160,   161,   162,    -1,
      -1,   165,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     184,     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,
      -1,   195,   196,   197,   198,    -1,    -1,    -1,    -1,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    51,    52,
      53,    54,    55,    56,    -1,    -1,    -1,    -1,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     153,   154,    -1,   156
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,     3,     4,     5,     6,     7,     8,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   156,   208,   209,   210,   211,   212,   213,   276,   277,
     287,   288,   289,   295,   297,   298,   313,   314,   318,   319,
     321,   324,   325,   330,   331,   332,   335,   336,   337,   338,
     339,   184,   184,   156,   188,   280,   335,     0,   288,   191,
     194,   188,   194,   290,   185,   315,   316,   317,   320,   321,
     330,   156,   279,   283,   186,   322,   323,   156,   194,   278,
     281,   321,   331,   156,   281,   340,    45,   156,   286,   333,
     334,   321,   326,   327,   330,   188,   321,   156,   279,     9,
      10,    11,    13,    14,    15,    16,    18,    19,    20,   155,
     156,   157,   158,   159,   160,   161,   162,   165,   166,   184,
     188,   189,   194,   195,   196,   197,   198,   285,   291,   292,
     293,   294,   295,   301,   302,   303,   305,   306,   312,   313,
     321,   341,   342,   345,   346,   347,   348,   349,   350,   351,
     352,   353,   354,   355,   356,   357,   358,   359,   361,   362,
     363,   365,   366,   191,   156,   282,   317,   320,   193,   322,
     184,   187,   321,   344,   345,   357,   323,   188,   191,   194,
     296,   185,   191,   193,   185,   191,   156,   284,   328,   329,
     189,   327,   321,   326,   194,   193,   322,   194,   194,   291,
     184,   184,   194,   194,   341,   184,   341,   192,   184,   357,
     357,   341,   189,   292,   189,   291,   191,   194,   172,   207,
     173,   171,   204,   205,   206,   169,   170,   167,   168,   202,
     203,   163,   164,   196,   198,   199,   200,   201,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   193,   343,
     357,   165,   166,   186,   190,   184,   316,   322,   188,   299,
     342,   193,   187,   326,   279,   191,   194,   281,   344,   334,
     322,   191,   194,   328,   189,   299,   193,   155,   295,   301,
     308,   341,   194,   341,   192,   310,   319,   330,   341,   185,
     189,   342,   347,   341,   348,   349,   350,   351,   352,   353,
     353,   354,   354,   354,   354,   355,   355,   356,   356,   357,
     357,   357,   342,   341,   360,   284,   154,   184,   342,   364,
     299,   300,   299,   189,   279,   329,   194,   299,   184,   309,
     310,   311,   185,   185,   185,   156,   285,   192,   187,   185,
     185,   191,   189,   191,   194,   279,   341,   194,   185,   291,
     188,   290,   294,   307,   193,   342,   342,   189,   299,   194,
     322,   185,   341,   307,    12,   292,   304,   299,   194,   194,
     291,   189
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int16 yyr1[] =
{
       0,   275,   276,   276,   277,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   288,   289,   289,
     290,   290,   291,   291,   292,   292,   293,   293,   294,   294,
     294,   294,   294,   294,   294,   295,   295,   295,   295,   295,
     295,   295,   295,   295,   296,   296,   297,   297,   297,   297,
     297,   298,   298,   298,   298,   298,   299,   299,   299,   300,
     300,   301,   301,   302,   302,   303,   304,   304,   305,   305,
     306,   306,   306,   307,   307,   308,   308,   309,   309,   310,
     310,   311,   311,   312,   312,   312,   312,   312,   313,   314,
     314,   315,   315,   316,   316,   316,   316,   317,   317,   318,
     319,   319,   320,   321,   321,   322,   322,   323,   323,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     324,   324,   324,   324,   324,   324,   324,   324,   324,   324,
     325,   325,   326,   326,   327,   327,   328,   328,   329,   329,
     330,   330,   331,   331,   331,   331,   331,   331,   332,   333,
     333,   334,   334,   334,   335,   335,   335,   336,   336,   336,
     337,   338,   339,   339,   339,   339,   339,   339,   339,   339,
     339,   339,   339,   339,   339,   339,   339,   339,   339,   340,
     340,   341,   341,   342,   342,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   344,   345,   345,   346,
     346,   347,   347,   348,   348,   349,   349,   350,   350,   351,
     351,   352,   352,   352,   353,   353,   353,   353,   353,   354,
     354,   354,   355,   355,   355,   356,   356,   356,   356,   357,
     357,   357,   357,   358,   358,   358,   358,   359,   359,   359,
     359,   359,   359,   360,   361,   362,   363,   363,   363,   364,
     364,   365,   365,   366,   366,   366,   366,   366,   366,   366,
     366
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     1,     1,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     1,
       2,     3,     1,     1,     1,     2,     2,     3,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     4,     6,     7,
       8,     2,     3,     4,     2,     3,     1,     3,     4,     6,
       5,     1,     2,     3,     5,     4,     1,     3,     4,     1,
       3,     1,     2,     5,     7,     7,     0,     1,     3,     2,
       5,     7,     6,     1,     1,     1,     1,     1,     0,     1,
       4,     2,     3,     2,     2,     2,     3,     2,     2,     1,
       2,     1,     3,     2,     1,     2,     1,     2,     3,     3,
       1,     2,     1,     1,     2,     1,     2,     2,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       5,     4,     1,     2,     3,     4,     1,     3,     1,     2,
       1,     2,     1,     1,     1,     1,     1,     1,     4,     1,
       3,     1,     3,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     4,     1,
       3,     1,     3,     1,     3,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     5,     1,
       3,     1,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     3,     3,     1,     3,     3,     3,     3,     1,
       3,     3,     1,     3,     3,     1,     3,     3,     3,     1,
       2,     2,     2,     1,     1,     1,     1,     1,     4,     1,
       3,     2,     2,     1,     1,     1,     4,     3,     4,     1,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = GLSL_EMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == GLSL_EMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, context, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use GLSL_error or GLSL_UNDEF. */
#define YYERRCODE GLSL_UNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if GLSL_DEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined GLSL_LTYPE_IS_TRIVIAL && GLSL_LTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, context); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct glsl_parse_context * context)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (context);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct glsl_parse_context * context)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, context);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, struct glsl_parse_context * context)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), context);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, context); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !GLSL_DEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !GLSL_DEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, struct glsl_parse_context * context)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (context);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (struct glsl_parse_context * context)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined GLSL_LTYPE_IS_TRIVIAL && GLSL_LTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = GLSL_EMPTY; /* Cause a token to be read.  */

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == GLSL_EMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, &yylloc, scanner);
    }

  if (yychar <= GLSL_EOF)
    {
      yychar = GLSL_EOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == GLSL_error)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = GLSL_UNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = GLSL_EMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* root: %empty  */
#line 504 "glsl.y"
                          { context->root = new_glsl_node(context, TRANSLATION_UNIT, NULL); }
#line 2948 "glsl.parser.c"
    break;

  case 3: /* root: translation_unit  */
#line 505 "glsl.y"
                                           { context->root = (yyvsp[0].translation_unit); }
#line 2954 "glsl.parser.c"
    break;

  case 4: /* translation_unit: external_declaration  */
#line 509 "glsl.y"
                                { (yyval.translation_unit) = new_glsl_node(context, TRANSLATION_UNIT, (yyvsp[0].external_declaration), NULL); }
#line 2960 "glsl.parser.c"
    break;

  case 5: /* translation_unit: translation_unit external_declaration  */
#line 512 "glsl.y"
                                { (yyval.translation_unit) = new_glsl_node(context, TRANSLATION_UNIT, (yyvsp[-1].translation_unit), (yyvsp[0].external_declaration), NULL); }
#line 2966 "glsl.parser.c"
    break;

  case 6: /* block_identifier: IDENTIFIER  */
#line 515 "glsl.y"
                                     { (yyval.block_identifier) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 2972 "glsl.parser.c"
    break;

  case 7: /* decl_identifier: IDENTIFIER  */
#line 518 "glsl.y"
                                     { (yyval.decl_identifier) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 2978 "glsl.parser.c"
    break;

  case 8: /* struct_name: IDENTIFIER  */
#line 521 "glsl.y"
                                     { (yyval.struct_name) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 2984 "glsl.parser.c"
    break;

  case 9: /* type_name: IDENTIFIER  */
#line 524 "glsl.y"
                                     { (yyval.type_name) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 2990 "glsl.parser.c"
    break;

  case 10: /* param_name: IDENTIFIER  */
#line 527 "glsl.y"
                                     { (yyval.param_name) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 2996 "glsl.parser.c"
    break;

  case 11: /* function_name: IDENTIFIER  */
#line 530 "glsl.y"
                                     { (yyval.function_name) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 3002 "glsl.parser.c"
    break;

  case 12: /* field_identifier: IDENTIFIER  */
#line 533 "glsl.y"
                                     { (yyval.field_identifier) = new_glsl_string(context, FIELD_IDENTIFIER, (yyvsp[0].IDENTIFIER)); }
#line 3008 "glsl.parser.c"
    break;

  case 13: /* variable_identifier: IDENTIFIER  */
#line 536 "glsl.y"
                                     { (yyval.variable_identifier) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 3014 "glsl.parser.c"
    break;

  case 14: /* layout_identifier: IDENTIFIER  */
#line 539 "glsl.y"
                                     { (yyval.layout_identifier) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 3020 "glsl.parser.c"
    break;

  case 15: /* type_specifier_identifier: IDENTIFIER  */
#line 542 "glsl.y"
                                       { (yyval.type_specifier_identifier) = new_glsl_identifier(context, (yyvsp[0].IDENTIFIER)); }
#line 3026 "glsl.parser.c"
    break;

  case 16: /* external_declaration: function_definition  */
#line 545 "glsl.y"
                                              { (yyval.external_declaration) = (yyvsp[0].function_definition); }
#line 3032 "glsl.parser.c"
    break;

  case 17: /* external_declaration: declaration  */
#line 546 "glsl.y"
                                      { (yyval.external_declaration) = (yyvsp[0].declaration); }
#line 3038 "glsl.parser.c"
    break;

  case 18: /* function_definition: function_prototype compound_statement_no_new_scope  */
#line 550 "glsl.y"
                                { (yyval.function_definition) = new_glsl_node(context, FUNCTION_DEFINITION,
					(yyvsp[-1].function_prototype),
					(yyvsp[0].compound_statement_no_new_scope),
					NULL); }
#line 3047 "glsl.parser.c"
    break;

  case 19: /* function_definition: function_prototype  */
#line 555 "glsl.y"
                                { (yyval.function_definition) = new_glsl_node(context, FUNCTION_DEFINITION,
					(yyvsp[0].function_prototype),
					new_glsl_node(context, STATEMENT_LIST, NULL),
					NULL); }
#line 3056 "glsl.parser.c"
    break;

  case 20: /* compound_statement_no_new_scope: LEFT_BRACE RIGHT_BRACE  */
#line 561 "glsl.y"
                                                         { (yyval.compound_statement_no_new_scope) = new_glsl_node(context, STATEMENT_LIST, NULL); }
#line 3062 "glsl.parser.c"
    break;

  case 21: /* compound_statement_no_new_scope: LEFT_BRACE statement_list RIGHT_BRACE  */
#line 562 "glsl.y"
                                                                { (yyval.compound_statement_no_new_scope) = (yyvsp[-1].statement_list); }
#line 3068 "glsl.parser.c"
    break;

  case 22: /* statement: compound_statement  */
#line 565 "glsl.y"
                                             { (yyval.statement) = (yyvsp[0].compound_statement); }
#line 3074 "glsl.parser.c"
    break;

  case 23: /* statement: simple_statement  */
#line 566 "glsl.y"
                                           { (yyval.statement) = (yyvsp[0].simple_statement); }
#line 3080 "glsl.parser.c"
    break;

  case 24: /* statement_list: statement  */
#line 569 "glsl.y"
                                    { (yyval.statement_list) = new_glsl_node(context, STATEMENT_LIST, (yyvsp[0].statement), NULL); }
#line 3086 "glsl.parser.c"
    break;

  case 25: /* statement_list: statement_list statement  */
#line 570 "glsl.y"
                                                   { (yyval.statement_list) = new_glsl_node(context, STATEMENT_LIST, (yyvsp[-1].statement_list), (yyvsp[0].statement), NULL); }
#line 3092 "glsl.parser.c"
    break;

  case 26: /* compound_statement: LEFT_BRACE RIGHT_BRACE  */
#line 573 "glsl.y"
                                                 { (yyval.compound_statement) = new_glsl_node(context, STATEMENT_LIST, NULL); }
#line 3098 "glsl.parser.c"
    break;

  case 27: /* compound_statement: LEFT_BRACE statement_list RIGHT_BRACE  */
#line 574 "glsl.y"
                                                                { (yyval.compound_statement) = (yyvsp[-1].statement_list); }
#line 3104 "glsl.parser.c"
    break;

  case 28: /* simple_statement: declaration  */
#line 577 "glsl.y"
                                      { (yyval.simple_statement) = (yyvsp[0].declaration); }
#line 3110 "glsl.parser.c"
    break;

  case 29: /* simple_statement: expression_statement  */
#line 578 "glsl.y"
                                               { (yyval.simple_statement) = (yyvsp[0].expression_statement); }
#line 3116 "glsl.parser.c"
    break;

  case 30: /* simple_statement: selection_statement  */
#line 579 "glsl.y"
                                              { (yyval.simple_statement) = (yyvsp[0].selection_statement); }
#line 3122 "glsl.parser.c"
    break;

  case 31: /* simple_statement: switch_statement  */
#line 580 "glsl.y"
                                           { (yyval.simple_statement) = (yyvsp[0].switch_statement); }
#line 3128 "glsl.parser.c"
    break;

  case 32: /* simple_statement: case_label  */
#line 581 "glsl.y"
                                     { (yyval.simple_statement)= (yyvsp[0].case_label); }
#line 3134 "glsl.parser.c"
    break;

  case 33: /* simple_statement: iteration_statement  */
#line 582 "glsl.y"
                                              { (yyval.simple_statement) = (yyvsp[0].iteration_statement); }
#line 3140 "glsl.parser.c"
    break;

  case 34: /* simple_statement: jump_statement  */
#line 583 "glsl.y"
                                         { (yyval.simple_statement) = (yyvsp[0].jump_statement); }
#line 3146 "glsl.parser.c"
    break;

  case 35: /* declaration: function_prototype SEMICOLON  */
#line 586 "glsl.y"
                                                       { (yyval.declaration) = new_glsl_node(context, DECLARATION, (yyvsp[-1].function_prototype), NULL); }
#line 3152 "glsl.parser.c"
    break;

  case 36: /* declaration: init_declarator_list SEMICOLON  */
#line 587 "glsl.y"
                                                         { (yyval.declaration) = new_glsl_node(context, DECLARATION, (yyvsp[-1].init_declarator_list), NULL); }
#line 3158 "glsl.parser.c"
    break;

  case 37: /* declaration: PRECISION precision_qualifier type_specifier SEMICOLON  */
#line 589 "glsl.y"
                                { (yyval.declaration) = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, PRECISION_DECLARATION,
							(yyvsp[-2].precision_qualifier),
							(yyvsp[-1].type_specifier),
							NULL),
						NULL); }
#line 3169 "glsl.parser.c"
    break;

  case 38: /* declaration: type_qualifier block_identifier LEFT_BRACE struct_declaration_list RIGHT_BRACE SEMICOLON  */
#line 596 "glsl.y"
                                { (yyval.declaration) = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, BLOCK_DECLARATION,
							(yyvsp[-5].type_qualifier),
							(yyvsp[-4].block_identifier),
							(yyvsp[-2].struct_declaration_list),
							new_glsl_identifier(context, NULL),
							new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
							NULL),
						NULL); }
#line 3183 "glsl.parser.c"
    break;

  case 39: /* declaration: type_qualifier block_identifier LEFT_BRACE struct_declaration_list RIGHT_BRACE decl_identifier SEMICOLON  */
#line 606 "glsl.y"
                                { (yyval.declaration) = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, BLOCK_DECLARATION,
							(yyvsp[-6].type_qualifier),
							(yyvsp[-5].block_identifier),
							(yyvsp[-3].struct_declaration_list),
							(yyvsp[-1].decl_identifier),
							new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
							NULL),
						NULL); }
#line 3197 "glsl.parser.c"
    break;

  case 40: /* declaration: type_qualifier block_identifier LEFT_BRACE struct_declaration_list RIGHT_BRACE decl_identifier array_specifier_list SEMICOLON  */
#line 616 "glsl.y"
                                { (yyval.declaration) = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, BLOCK_DECLARATION,
							(yyvsp[-7].type_qualifier),
							(yyvsp[-6].block_identifier),
							(yyvsp[-4].struct_declaration_list),
							(yyvsp[-2].decl_identifier),
							(yyvsp[-1].array_specifier_list),
							NULL),
						NULL); }
#line 3211 "glsl.parser.c"
    break;

  case 41: /* declaration: type_qualifier SEMICOLON  */
#line 626 "glsl.y"
                                { (yyval.declaration) = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, UNINITIALIZED_DECLARATION,
							(yyvsp[-1].type_qualifier),
							new_glsl_identifier(context, NULL),
							NULL),
						NULL); }
#line 3222 "glsl.parser.c"
    break;

  case 42: /* declaration: type_qualifier type_name SEMICOLON  */
#line 633 "glsl.y"
                                { (yyval.declaration) = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, UNINITIALIZED_DECLARATION,
							(yyvsp[-2].type_qualifier),
							(yyvsp[-1].type_name),
							new_glsl_node(context, IDENTIFIER_LIST, NULL),
							NULL),
						NULL); }
#line 3234 "glsl.parser.c"
    break;

  case 43: /* declaration: type_qualifier type_name identifier_list SEMICOLON  */
#line 641 "glsl.y"
                                { (yyval.declaration) = new_glsl_node(context, DECLARATION,
						new_glsl_node(context, UNINITIALIZED_DECLARATION,
							(yyvsp[-3].type_qualifier),
							(yyvsp[-2].type_name),
							(yyvsp[-1].identifier_list),
							NULL),
						NULL); }
#line 3246 "glsl.parser.c"
    break;

  case 44: /* identifier_list: COMMA decl_identifier  */
#line 650 "glsl.y"
                                                { (yyval.identifier_list) = new_glsl_node(context, IDENTIFIER_LIST, (yyvsp[0].decl_identifier), NULL); }
#line 3252 "glsl.parser.c"
    break;

  case 45: /* identifier_list: identifier_list COMMA decl_identifier  */
#line 652 "glsl.y"
                                { (yyval.identifier_list) = new_glsl_node(context, IDENTIFIER_LIST, (yyvsp[-2].identifier_list), (yyvsp[0].decl_identifier), NULL); }
#line 3258 "glsl.parser.c"
    break;

  case 46: /* init_declarator_list: single_declaration  */
#line 655 "glsl.y"
                                             { (yyval.init_declarator_list) = new_glsl_node(context, INIT_DECLARATOR_LIST, (yyvsp[0].single_declaration), NULL); }
#line 3264 "glsl.parser.c"
    break;

  case 47: /* init_declarator_list: init_declarator_list COMMA decl_identifier  */
#line 657 "glsl.y"
                                { (yyval.init_declarator_list) = new_glsl_node(context, INIT_DECLARATOR_LIST,
						(yyvsp[-2].init_declarator_list),
						new_glsl_node(context, INIT_DECLARATOR,
							(yyvsp[0].decl_identifier),
							new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
							NULL),
						NULL); }
#line 3276 "glsl.parser.c"
    break;

  case 48: /* init_declarator_list: init_declarator_list COMMA decl_identifier array_specifier_list  */
#line 665 "glsl.y"
                                { (yyval.init_declarator_list) = new_glsl_node(context, INIT_DECLARATOR_LIST,
						(yyvsp[-3].init_declarator_list),
						new_glsl_node(context, INIT_DECLARATOR,
							(yyvsp[-1].decl_identifier),
							(yyvsp[0].array_specifier_list),
							NULL),
						NULL); }
#line 3288 "glsl.parser.c"
    break;

  case 49: /* init_declarator_list: init_declarator_list COMMA decl_identifier array_specifier_list EQUAL initializer  */
#line 673 "glsl.y"
                                { (yyval.init_declarator_list) = new_glsl_node(context, INIT_DECLARATOR_LIST,
						(yyvsp[-5].init_declarator_list),
						new_glsl_node(context, INIT_DECLARATOR,
							(yyvsp[-3].decl_identifier),
							(yyvsp[-2].array_specifier_list),
							(yyvsp[0].initializer),
							NULL),
						NULL); }
#line 3301 "glsl.parser.c"
    break;

  case 50: /* init_declarator_list: init_declarator_list COMMA decl_identifier EQUAL initializer  */
#line 682 "glsl.y"
                                { (yyval.init_declarator_list) = new_glsl_node(context, INIT_DECLARATOR_LIST,
						(yyvsp[-4].init_declarator_list),
						new_glsl_node(context, INIT_DECLARATOR,
							(yyvsp[-2].decl_identifier),
							new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
							(yyvsp[0].initializer),
							NULL),
						NULL); }
#line 3314 "glsl.parser.c"
    break;

  case 51: /* single_declaration: fully_specified_type  */
#line 693 "glsl.y"
                                { (yyval.single_declaration) = new_glsl_node(context, SINGLE_DECLARATION,
					(yyvsp[0].fully_specified_type),
					new_glsl_identifier(context, NULL),
					new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
					NULL); }
#line 3324 "glsl.parser.c"
    break;

  case 52: /* single_declaration: fully_specified_type decl_identifier  */
#line 700 "glsl.y"
                                { (yyval.single_declaration) = new_glsl_node(context, SINGLE_DECLARATION,
					(yyvsp[-1].fully_specified_type),
					(yyvsp[0].decl_identifier),
					new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
					NULL); }
#line 3334 "glsl.parser.c"
    break;

  case 53: /* single_declaration: fully_specified_type decl_identifier array_specifier_list  */
#line 707 "glsl.y"
                                { (yyval.single_declaration) = new_glsl_node(context, SINGLE_DECLARATION, (yyvsp[-2].fully_specified_type), (yyvsp[-1].decl_identifier), (yyvsp[0].array_specifier_list), NULL); }
#line 3340 "glsl.parser.c"
    break;

  case 54: /* single_declaration: fully_specified_type decl_identifier array_specifier_list EQUAL initializer  */
#line 710 "glsl.y"
                                { (yyval.single_declaration) = new_glsl_node(context, SINGLE_INIT_DECLARATION, (yyvsp[-4].fully_specified_type), (yyvsp[-3].decl_identifier), (yyvsp[-2].array_specifier_list), (yyvsp[0].initializer), NULL); }
#line 3346 "glsl.parser.c"
    break;

  case 55: /* single_declaration: fully_specified_type decl_identifier EQUAL initializer  */
#line 713 "glsl.y"
                                { (yyval.single_declaration) = new_glsl_node(context, SINGLE_INIT_DECLARATION,
					(yyvsp[-3].fully_specified_type),
					(yyvsp[-2].decl_identifier),
					new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
					(yyvsp[0].initializer),
					NULL); }
#line 3357 "glsl.parser.c"
    break;

  case 56: /* initializer: assignment_expression  */
#line 721 "glsl.y"
                                                { (yyval.initializer) = new_glsl_node(context, INITIALIZER, (yyvsp[0].assignment_expression), NULL); }
#line 3363 "glsl.parser.c"
    break;

  case 57: /* initializer: LEFT_BRACE initializer_list RIGHT_BRACE  */
#line 722 "glsl.y"
                                                                  { (yyval.initializer) = new_glsl_node(context, INITIALIZER, (yyvsp[-1].initializer_list), NULL); }
#line 3369 "glsl.parser.c"
    break;

  case 58: /* initializer: LEFT_BRACE initializer_list COMMA RIGHT_BRACE  */
#line 723 "glsl.y"
                                                                        { (yyval.initializer) = new_glsl_node(context, INITIALIZER, (yyvsp[-2].initializer_list), NULL); }
#line 3375 "glsl.parser.c"
    break;

  case 59: /* initializer_list: initializer  */
#line 727 "glsl.y"
                                { (yyval.initializer_list) = new_glsl_node(context, INITIALIZER_LIST, (yyvsp[0].initializer), NULL); }
#line 3381 "glsl.parser.c"
    break;

  case 60: /* initializer_list: initializer_list COMMA initializer  */
#line 729 "glsl.y"
                                { (yyval.initializer_list) = new_glsl_node(context, INITIALIZER_LIST, (yyvsp[-2].initializer_list), (yyvsp[0].initializer), NULL); }
#line 3387 "glsl.parser.c"
    break;

  case 61: /* expression_statement: SEMICOLON  */
#line 732 "glsl.y"
                                    { (yyval.expression_statement) = new_glsl_node(context, EXPRESSION_STATEMENT, NULL); }
#line 3393 "glsl.parser.c"
    break;

  case 62: /* expression_statement: expression SEMICOLON  */
#line 733 "glsl.y"
                                               { (yyval.expression_statement) = new_glsl_node(context, EXPRESSION_STATEMENT, (yyvsp[-1].expression), NULL); }
#line 3399 "glsl.parser.c"
    break;

  case 63: /* selection_statement: IF LEFT_PAREN expression RIGHT_PAREN statement  */
#line 737 "glsl.y"
                                { (yyval.selection_statement) = new_glsl_node(context, SELECTION_STATEMENT, (yyvsp[-2].expression), (yyvsp[0].statement), NULL); }
#line 3405 "glsl.parser.c"
    break;

  case 64: /* selection_statement: IF LEFT_PAREN expression RIGHT_PAREN statement ELSE statement  */
#line 740 "glsl.y"
                                { (yyval.selection_statement) = new_glsl_node(context, SELECTION_STATEMENT_ELSE, (yyvsp[-4].expression), (yyvsp[-2].statement), (yyvsp[0].statement), NULL); }
#line 3411 "glsl.parser.c"
    break;

  case 65: /* switch_statement: SWITCH LEFT_PAREN expression RIGHT_PAREN LEFT_BRACE switch_statement_list RIGHT_BRACE  */
#line 744 "glsl.y"
                                { (yyval.switch_statement) = new_glsl_node(context, SWITCH_STATEMENT, (yyvsp[-4].expression), (yyvsp[-1].switch_statement_list), NULL); }
#line 3417 "glsl.parser.c"
    break;

  case 66: /* switch_statement_list: %empty  */
#line 747 "glsl.y"
                          { (yyval.switch_statement_list) = new_glsl_node(context, STATEMENT_LIST, NULL); }
#line 3423 "glsl.parser.c"
    break;

  case 67: /* switch_statement_list: statement_list  */
#line 748 "glsl.y"
                                         { (yyval.switch_statement_list) = (yyvsp[0].statement_list); }
#line 3429 "glsl.parser.c"
    break;

  case 68: /* case_label: CASE expression COLON  */
#line 751 "glsl.y"
                                                { (yyval.case_label) = new_glsl_node(context, CASE_LABEL, (yyvsp[-1].expression), NULL); }
#line 3435 "glsl.parser.c"
    break;

  case 69: /* case_label: DEFAULT COLON  */
#line 752 "glsl.y"
                                        { (yyval.case_label) = new_glsl_node(context, CASE_LABEL, NULL); }
#line 3441 "glsl.parser.c"
    break;

  case 70: /* iteration_statement: WHILE LEFT_PAREN condition RIGHT_PAREN statement_no_new_scope  */
#line 756 "glsl.y"
                                { (yyval.iteration_statement) = new_glsl_node(context, WHILE_STATEMENT, (yyvsp[-2].condition), (yyvsp[0].statement_no_new_scope), NULL); }
#line 3447 "glsl.parser.c"
    break;

  case 71: /* iteration_statement: DO statement WHILE LEFT_PAREN expression RIGHT_PAREN SEMICOLON  */
#line 759 "glsl.y"
                                { (yyval.iteration_statement) = new_glsl_node(context, DO_STATEMENT, (yyvsp[-5].statement), (yyvsp[-2].expression), NULL); }
#line 3453 "glsl.parser.c"
    break;

  case 72: /* iteration_statement: FOR LEFT_PAREN for_init_statement for_rest_statement RIGHT_PAREN statement_no_new_scope  */
#line 762 "glsl.y"
                                { (yyval.iteration_statement) = new_glsl_node(context, FOR_STATEMENT, (yyvsp[-3].for_init_statement), (yyvsp[-2].for_rest_statement), (yyvsp[0].statement_no_new_scope), NULL); }
#line 3459 "glsl.parser.c"
    break;

  case 73: /* statement_no_new_scope: compound_statement_no_new_scope  */
#line 765 "glsl.y"
                                                          { (yyval.statement_no_new_scope) = (yyvsp[0].compound_statement_no_new_scope); }
#line 3465 "glsl.parser.c"
    break;

  case 74: /* statement_no_new_scope: simple_statement  */
#line 766 "glsl.y"
                                           { (yyval.statement_no_new_scope) = (yyvsp[0].simple_statement); }
#line 3471 "glsl.parser.c"
    break;

  case 75: /* for_init_statement: expression_statement  */
#line 769 "glsl.y"
                                               { (yyval.for_init_statement) = (yyvsp[0].expression_statement); }
#line 3477 "glsl.parser.c"
    break;

  case 76: /* for_init_statement: declaration  */
#line 770 "glsl.y"
                                      { (yyval.for_init_statement) = (yyvsp[0].declaration); }
#line 3483 "glsl.parser.c"
    break;

  case 77: /* conditionopt: condition  */
#line 773 "glsl.y"
                                    { (yyval.conditionopt) = new_glsl_node(context, CONDITION_OPT, (yyvsp[0].condition), NULL); }
#line 3489 "glsl.parser.c"
    break;

  case 78: /* conditionopt: %empty  */
#line 774 "glsl.y"
                          { (yyval.conditionopt) = new_glsl_node(context, CONDITION_OPT, NULL); }
#line 3495 "glsl.parser.c"
    break;

  case 79: /* condition: expression  */
#line 778 "glsl.y"
                                { (yyval.condition) = new_glsl_node(context, EXPRESSION_CONDITION, (yyvsp[0].expression), NULL); }
#line 3501 "glsl.parser.c"
    break;

  case 80: /* condition: fully_specified_type variable_identifier EQUAL initializer  */
#line 781 "glsl.y"
                                { (yyval.condition) = new_glsl_node(context, ASSIGNMENT_CONDITION, (yyvsp[-3].fully_specified_type), (yyvsp[-2].variable_identifier), (yyvsp[0].initializer), NULL); }
#line 3507 "glsl.parser.c"
    break;

  case 81: /* for_rest_statement: conditionopt SEMICOLON  */
#line 785 "glsl.y"
                                { (yyval.for_rest_statement) = new_glsl_node(context, FOR_REST_STATEMENT, (yyvsp[-1].conditionopt), NULL); }
#line 3513 "glsl.parser.c"
    break;

  case 82: /* for_rest_statement: conditionopt SEMICOLON expression  */
#line 788 "glsl.y"
                                { (yyval.for_rest_statement) = new_glsl_node(context, FOR_REST_STATEMENT, (yyvsp[-2].conditionopt), (yyvsp[0].expression), NULL); }
#line 3519 "glsl.parser.c"
    break;

  case 83: /* jump_statement: CONTINUE SEMICOLON  */
#line 792 "glsl.y"
                                { (yyval.jump_statement) = new_glsl_node(context, CONTINUE, NULL); }
#line 3525 "glsl.parser.c"
    break;

  case 84: /* jump_statement: BREAK SEMICOLON  */
#line 795 "glsl.y"
                                { (yyval.jump_statement) = new_glsl_node(context, BREAK, NULL); }
#line 3531 "glsl.parser.c"
    break;

  case 85: /* jump_statement: RETURN SEMICOLON  */
#line 798 "glsl.y"
                                { (yyval.jump_statement) = new_glsl_node(context, RETURN, NULL); }
#line 3537 "glsl.parser.c"
    break;

  case 86: /* jump_statement: RETURN expression SEMICOLON  */
#line 801 "glsl.y"
                                { (yyval.jump_statement) = new_glsl_node(context, RETURN_VALUE, (yyvsp[-1].expression), NULL); }
#line 3543 "glsl.parser.c"
    break;

  case 87: /* jump_statement: DISCARD SEMICOLON  */
#line 804 "glsl.y"
                                { (yyval.jump_statement) = new_glsl_node(context, DISCARD, NULL); }
#line 3549 "glsl.parser.c"
    break;

  case 88: /* function_prototype: function_declarator RIGHT_PAREN  */
#line 807 "glsl.y"
                                                          { (yyval.function_prototype) = (yyvsp[-1].function_declarator); }
#line 3555 "glsl.parser.c"
    break;

  case 89: /* function_declarator: function_header  */
#line 811 "glsl.y"
                                { (yyval.function_declarator) = new_glsl_node(context, FUNCTION_DECLARATION,
					(yyvsp[0].function_header),
					new_glsl_node(context, FUNCTION_PARAMETER_LIST, NULL),
					NULL); }
#line 3564 "glsl.parser.c"
    break;

  case 90: /* function_declarator: function_header function_parameter_list  */
#line 817 "glsl.y"
                                { (yyval.function_declarator) = new_glsl_node(context, FUNCTION_DECLARATION,
					(yyvsp[-1].function_header),
					(yyvsp[0].function_parameter_list),
					NULL); }
#line 3573 "glsl.parser.c"
    break;

  case 91: /* function_parameter_list: parameter_declaration  */
#line 824 "glsl.y"
                                { (yyval.function_parameter_list) = new_glsl_node(context, FUNCTION_PARAMETER_LIST, (yyvsp[0].parameter_declaration), NULL); }
#line 3579 "glsl.parser.c"
    break;

  case 92: /* function_parameter_list: function_parameter_list COMMA parameter_declaration  */
#line 827 "glsl.y"
                                { (yyval.function_parameter_list) = new_glsl_node(context, FUNCTION_PARAMETER_LIST, (yyvsp[-2].function_parameter_list), (yyvsp[0].parameter_declaration), NULL); }
#line 3585 "glsl.parser.c"
    break;

  case 93: /* parameter_declaration: type_qualifier parameter_declarator  */
#line 831 "glsl.y"
                                { (yyval.parameter_declaration) = new_glsl_node(context, PARAMETER_DECLARATION, (yyvsp[-1].type_qualifier), (yyvsp[0].parameter_declarator), NULL); }
#line 3591 "glsl.parser.c"
    break;

  case 94: /* parameter_declaration: parameter_declarator  */
#line 834 "glsl.y"
                                { (yyval.parameter_declaration) = new_glsl_node(context, PARAMETER_DECLARATION,
					new_glsl_node(context, TYPE_QUALIFIER_LIST, NULL),
					(yyvsp[0].parameter_declarator),
					NULL); }
#line 3600 "glsl.parser.c"
    break;

  case 95: /* parameter_declaration: type_qualifier parameter_type_specifier  */
#line 840 "glsl.y"
                                { (yyval.parameter_declaration) = new_glsl_node(context, PARAMETER_DECLARATION, (yyvsp[-1].type_qualifier), (yyvsp[0].parameter_type_specifier), NULL); }
#line 3606 "glsl.parser.c"
    break;

  case 96: /* parameter_declaration: parameter_type_specifier  */
#line 843 "glsl.y"
                                { (yyval.parameter_declaration) = new_glsl_node(context, PARAMETER_DECLARATION,
					new_glsl_node(context, TYPE_QUALIFIER_LIST, NULL),
					(yyvsp[0].parameter_type_specifier),
					NULL); }
#line 3615 "glsl.parser.c"
    break;

  case 97: /* parameter_declarator: type_specifier param_name  */
#line 850 "glsl.y"
                                { (yyval.parameter_declarator) = new_glsl_node(context, PARAMETER_DECLARATOR, (yyvsp[-1].type_specifier), (yyvsp[0].param_name), NULL); }
#line 3621 "glsl.parser.c"
    break;

  case 98: /* parameter_declarator: type_specifier param_name array_specifier_list  */
#line 853 "glsl.y"
                                { (yyval.parameter_declarator) = new_glsl_node(context, PARAMETER_DECLARATOR, (yyvsp[-2].type_specifier), (yyvsp[-1].param_name), (yyvsp[0].array_specifier_list), NULL);}
#line 3627 "glsl.parser.c"
    break;

  case 99: /* function_header: fully_specified_type function_name LEFT_PAREN  */
#line 857 "glsl.y"
                                { (yyval.function_header) = new_glsl_node(context, FUNCTION_HEADER, (yyvsp[-2].fully_specified_type), (yyvsp[-1].function_name), NULL); }
#line 3633 "glsl.parser.c"
    break;

  case 100: /* fully_specified_type: type_specifier  */
#line 861 "glsl.y"
                                { (yyval.fully_specified_type) = new_glsl_node(context, FULLY_SPECIFIED_TYPE,
					new_glsl_node(context, TYPE_QUALIFIER_LIST, NULL),
					(yyvsp[0].type_specifier),
					NULL); }
#line 3642 "glsl.parser.c"
    break;

  case 101: /* fully_specified_type: type_qualifier type_specifier  */
#line 867 "glsl.y"
                                { (yyval.fully_specified_type) = new_glsl_node(context, FULLY_SPECIFIED_TYPE, (yyvsp[-1].type_qualifier), (yyvsp[0].type_specifier), NULL); }
#line 3648 "glsl.parser.c"
    break;

  case 102: /* parameter_type_specifier: type_specifier  */
#line 871 "glsl.y"
                                { (yyval.parameter_type_specifier) = new_glsl_node(context, PARAMETER_DECLARATOR, (yyvsp[0].type_specifier), NULL); }
#line 3654 "glsl.parser.c"
    break;

  case 103: /* type_specifier: type_specifier_nonarray  */
#line 875 "glsl.y"
                                { (yyval.type_specifier) = new_glsl_node(context, TYPE_SPECIFIER,
					(yyvsp[0].type_specifier_nonarray),
					new_glsl_node(context, ARRAY_SPECIFIER_LIST, NULL),
					NULL); }
#line 3663 "glsl.parser.c"
    break;

  case 104: /* type_specifier: type_specifier_nonarray array_specifier_list  */
#line 881 "glsl.y"
                                { (yyval.type_specifier) = new_glsl_node(context, TYPE_SPECIFIER, (yyvsp[-1].type_specifier_nonarray), (yyvsp[0].array_specifier_list), NULL); }
#line 3669 "glsl.parser.c"
    break;

  case 105: /* array_specifier_list: array_specifier  */
#line 885 "glsl.y"
                                { (yyval.array_specifier_list) = new_glsl_node(context, ARRAY_SPECIFIER_LIST, (yyvsp[0].array_specifier), NULL); }
#line 3675 "glsl.parser.c"
    break;

  case 106: /* array_specifier_list: array_specifier_list array_specifier  */
#line 888 "glsl.y"
                                { (yyval.array_specifier_list) = new_glsl_node(context, ARRAY_SPECIFIER_LIST, (yyvsp[-1].array_specifier_list), (yyvsp[0].array_specifier), NULL); }
#line 3681 "glsl.parser.c"
    break;

  case 107: /* array_specifier: LEFT_BRACKET RIGHT_BRACKET  */
#line 892 "glsl.y"
                                { (yyval.array_specifier) = new_glsl_node(context, ARRAY_SPECIFIER, NULL); }
#line 3687 "glsl.parser.c"
    break;

  case 108: /* array_specifier: LEFT_BRACKET constant_expression RIGHT_BRACKET  */
#line 895 "glsl.y"
                                { (yyval.array_specifier) = new_glsl_node(context, ARRAY_SPECIFIER, (yyvsp[-1].constant_expression), NULL); }
#line 3693 "glsl.parser.c"
    break;

  case 109: /* type_specifier_nonarray: VOID  */
#line 898 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, VOID, NULL); }
#line 3699 "glsl.parser.c"
    break;

  case 110: /* type_specifier_nonarray: FLOAT  */
#line 899 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, FLOAT, NULL); }
#line 3705 "glsl.parser.c"
    break;

  case 111: /* type_specifier_nonarray: DOUBLE  */
#line 900 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, DOUBLE, NULL); }
#line 3711 "glsl.parser.c"
    break;

  case 112: /* type_specifier_nonarray: INT  */
#line 901 "glsl.y"
                              { (yyval.type_specifier_nonarray) = new_glsl_node(context, INT, NULL); }
#line 3717 "glsl.parser.c"
    break;

  case 113: /* type_specifier_nonarray: UINT  */
#line 902 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, UINT, NULL); }
#line 3723 "glsl.parser.c"
    break;

  case 114: /* type_specifier_nonarray: BOOL  */
#line 903 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, BOOL, NULL); }
#line 3729 "glsl.parser.c"
    break;

  case 115: /* type_specifier_nonarray: VEC2  */
#line 904 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, VEC2, NULL); }
#line 3735 "glsl.parser.c"
    break;

  case 116: /* type_specifier_nonarray: VEC3  */
#line 905 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, VEC3, NULL); }
#line 3741 "glsl.parser.c"
    break;

  case 117: /* type_specifier_nonarray: VEC4  */
#line 906 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, VEC4, NULL); }
#line 3747 "glsl.parser.c"
    break;

  case 118: /* type_specifier_nonarray: DVEC2  */
#line 907 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, DVEC2, NULL); }
#line 3753 "glsl.parser.c"
    break;

  case 119: /* type_specifier_nonarray: DVEC3  */
#line 908 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, DVEC3, NULL); }
#line 3759 "glsl.parser.c"
    break;

  case 120: /* type_specifier_nonarray: DVEC4  */
#line 909 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, DVEC4, NULL); }
#line 3765 "glsl.parser.c"
    break;

  case 121: /* type_specifier_nonarray: BVEC2  */
#line 910 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, BVEC2, NULL); }
#line 3771 "glsl.parser.c"
    break;

  case 122: /* type_specifier_nonarray: BVEC3  */
#line 911 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, BVEC3, NULL); }
#line 3777 "glsl.parser.c"
    break;

  case 123: /* type_specifier_nonarray: BVEC4  */
#line 912 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, BVEC4, NULL); }
#line 3783 "glsl.parser.c"
    break;

  case 124: /* type_specifier_nonarray: IVEC2  */
#line 913 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, IVEC2, NULL); }
#line 3789 "glsl.parser.c"
    break;

  case 125: /* type_specifier_nonarray: IVEC3  */
#line 914 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, IVEC3, NULL); }
#line 3795 "glsl.parser.c"
    break;

  case 126: /* type_specifier_nonarray: IVEC4  */
#line 915 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, IVEC4, NULL); }
#line 3801 "glsl.parser.c"
    break;

  case 127: /* type_specifier_nonarray: UVEC2  */
#line 916 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, UVEC2, NULL); }
#line 3807 "glsl.parser.c"
    break;

  case 128: /* type_specifier_nonarray: UVEC3  */
#line 917 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, UVEC3, NULL); }
#line 3813 "glsl.parser.c"
    break;

  case 129: /* type_specifier_nonarray: UVEC4  */
#line 918 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, UVEC4, NULL); }
#line 3819 "glsl.parser.c"
    break;

  case 130: /* type_specifier_nonarray: MAT2  */
#line 919 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT2, NULL); }
#line 3825 "glsl.parser.c"
    break;

  case 131: /* type_specifier_nonarray: MAT3  */
#line 920 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT3, NULL); }
#line 3831 "glsl.parser.c"
    break;

  case 132: /* type_specifier_nonarray: MAT4  */
#line 921 "glsl.y"
                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT4, NULL); }
#line 3837 "glsl.parser.c"
    break;

  case 133: /* type_specifier_nonarray: MAT2X2  */
#line 922 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT2X2, NULL); }
#line 3843 "glsl.parser.c"
    break;

  case 134: /* type_specifier_nonarray: MAT2X3  */
#line 923 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT2X3, NULL); }
#line 3849 "glsl.parser.c"
    break;

  case 135: /* type_specifier_nonarray: MAT2X4  */
#line 924 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT2X4, NULL); }
#line 3855 "glsl.parser.c"
    break;

  case 136: /* type_specifier_nonarray: MAT3X2  */
#line 925 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT3X2, NULL); }
#line 3861 "glsl.parser.c"
    break;

  case 137: /* type_specifier_nonarray: MAT3X3  */
#line 926 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT3X3, NULL); }
#line 3867 "glsl.parser.c"
    break;

  case 138: /* type_specifier_nonarray: MAT3X4  */
#line 927 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT3X4, NULL); }
#line 3873 "glsl.parser.c"
    break;

  case 139: /* type_specifier_nonarray: MAT4X2  */
#line 928 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT4X2, NULL); }
#line 3879 "glsl.parser.c"
    break;

  case 140: /* type_specifier_nonarray: MAT4X3  */
#line 929 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT4X3, NULL); }
#line 3885 "glsl.parser.c"
    break;

  case 141: /* type_specifier_nonarray: MAT4X4  */
#line 930 "glsl.y"
                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, MAT4X4, NULL); }
#line 3891 "glsl.parser.c"
    break;

  case 142: /* type_specifier_nonarray: DMAT2  */
#line 931 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT2, NULL); }
#line 3897 "glsl.parser.c"
    break;

  case 143: /* type_specifier_nonarray: DMAT3  */
#line 932 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT3, NULL); }
#line 3903 "glsl.parser.c"
    break;

  case 144: /* type_specifier_nonarray: DMAT4  */
#line 933 "glsl.y"
                                { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT4, NULL); }
#line 3909 "glsl.parser.c"
    break;

  case 145: /* type_specifier_nonarray: DMAT2X2  */
#line 934 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT2X2, NULL); }
#line 3915 "glsl.parser.c"
    break;

  case 146: /* type_specifier_nonarray: DMAT2X3  */
#line 935 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT2X3, NULL); }
#line 3921 "glsl.parser.c"
    break;

  case 147: /* type_specifier_nonarray: DMAT2X4  */
#line 936 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT2X4, NULL); }
#line 3927 "glsl.parser.c"
    break;

  case 148: /* type_specifier_nonarray: DMAT3X2  */
#line 937 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT3X2, NULL); }
#line 3933 "glsl.parser.c"
    break;

  case 149: /* type_specifier_nonarray: DMAT3X3  */
#line 938 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT3X3, NULL); }
#line 3939 "glsl.parser.c"
    break;

  case 150: /* type_specifier_nonarray: DMAT3X4  */
#line 939 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT3X4, NULL); }
#line 3945 "glsl.parser.c"
    break;

  case 151: /* type_specifier_nonarray: DMAT4X2  */
#line 940 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT4X2, NULL); }
#line 3951 "glsl.parser.c"
    break;

  case 152: /* type_specifier_nonarray: DMAT4X3  */
#line 941 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT4X3, NULL); }
#line 3957 "glsl.parser.c"
    break;

  case 153: /* type_specifier_nonarray: DMAT4X4  */
#line 942 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, DMAT4X4, NULL); }
#line 3963 "glsl.parser.c"
    break;

  case 154: /* type_specifier_nonarray: ATOMIC_UINT  */
#line 943 "glsl.y"
                                      { (yyval.type_specifier_nonarray) = new_glsl_node(context, UINT, NULL); }
#line 3969 "glsl.parser.c"
    break;

  case 155: /* type_specifier_nonarray: SAMPLER1D  */
#line 944 "glsl.y"
                                    { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER1D, NULL); }
#line 3975 "glsl.parser.c"
    break;

  case 156: /* type_specifier_nonarray: SAMPLER2D  */
#line 945 "glsl.y"
                                    { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER2D, NULL); }
#line 3981 "glsl.parser.c"
    break;

  case 157: /* type_specifier_nonarray: SAMPLER3D  */
#line 946 "glsl.y"
                                    { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER3D, NULL); }
#line 3987 "glsl.parser.c"
    break;

  case 158: /* type_specifier_nonarray: SAMPLERCUBE  */
#line 947 "glsl.y"
                                      { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLERCUBE, NULL); }
#line 3993 "glsl.parser.c"
    break;

  case 159: /* type_specifier_nonarray: SAMPLER1DSHADOW  */
#line 948 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER1DSHADOW, NULL); }
#line 3999 "glsl.parser.c"
    break;

  case 160: /* type_specifier_nonarray: SAMPLER2DSHADOW  */
#line 949 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER2DSHADOW, NULL); }
#line 4005 "glsl.parser.c"
    break;

  case 161: /* type_specifier_nonarray: SAMPLERCUBESHADOW  */
#line 950 "glsl.y"
                                            { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLERCUBESHADOW, NULL); }
#line 4011 "glsl.parser.c"
    break;

  case 162: /* type_specifier_nonarray: SAMPLER1DARRAY  */
#line 951 "glsl.y"
                                         { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER1DARRAY, NULL); }
#line 4017 "glsl.parser.c"
    break;

  case 163: /* type_specifier_nonarray: SAMPLER2DARRAY  */
#line 952 "glsl.y"
                                         { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER2DARRAY, NULL); }
#line 4023 "glsl.parser.c"
    break;

  case 164: /* type_specifier_nonarray: SAMPLER1DARRAYSHADOW  */
#line 953 "glsl.y"
                                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER1DARRAYSHADOW, NULL); }
#line 4029 "glsl.parser.c"
    break;

  case 165: /* type_specifier_nonarray: SAMPLER2DARRAYSHADOW  */
#line 954 "glsl.y"
                                               { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER2DARRAYSHADOW, NULL); }
#line 4035 "glsl.parser.c"
    break;

  case 166: /* type_specifier_nonarray: SAMPLERCUBEARRAY  */
#line 955 "glsl.y"
                                           { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLERCUBEARRAY, NULL); }
#line 4041 "glsl.parser.c"
    break;

  case 167: /* type_specifier_nonarray: SAMPLERCUBEARRAYSHADOW  */
#line 956 "glsl.y"
                                                 { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLERCUBEARRAYSHADOW, NULL); }
#line 4047 "glsl.parser.c"
    break;

  case 168: /* type_specifier_nonarray: ISAMPLER1D  */
#line 957 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLER1D, NULL); }
#line 4053 "glsl.parser.c"
    break;

  case 169: /* type_specifier_nonarray: ISAMPLER2D  */
#line 958 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLER2D, NULL); }
#line 4059 "glsl.parser.c"
    break;

  case 170: /* type_specifier_nonarray: ISAMPLER3D  */
#line 959 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLER3D, NULL); }
#line 4065 "glsl.parser.c"
    break;

  case 171: /* type_specifier_nonarray: ISAMPLERCUBE  */
#line 960 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLERCUBE, NULL); }
#line 4071 "glsl.parser.c"
    break;

  case 172: /* type_specifier_nonarray: ISAMPLER1DARRAY  */
#line 961 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLER1DARRAY, NULL); }
#line 4077 "glsl.parser.c"
    break;

  case 173: /* type_specifier_nonarray: ISAMPLER2DARRAY  */
#line 962 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLER2DARRAY, NULL); }
#line 4083 "glsl.parser.c"
    break;

  case 174: /* type_specifier_nonarray: ISAMPLERCUBEARRAY  */
#line 963 "glsl.y"
                                            { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLERCUBEARRAY, NULL); }
#line 4089 "glsl.parser.c"
    break;

  case 175: /* type_specifier_nonarray: USAMPLER1D  */
#line 964 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLER1D, NULL); }
#line 4095 "glsl.parser.c"
    break;

  case 176: /* type_specifier_nonarray: USAMPLER2D  */
#line 965 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLER2D, NULL); }
#line 4101 "glsl.parser.c"
    break;

  case 177: /* type_specifier_nonarray: USAMPLER3D  */
#line 966 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLER3D, NULL); }
#line 4107 "glsl.parser.c"
    break;

  case 178: /* type_specifier_nonarray: USAMPLERCUBE  */
#line 967 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLERCUBE, NULL); }
#line 4113 "glsl.parser.c"
    break;

  case 179: /* type_specifier_nonarray: USAMPLER1DARRAY  */
#line 968 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLER1DARRAY, NULL); }
#line 4119 "glsl.parser.c"
    break;

  case 180: /* type_specifier_nonarray: USAMPLER2DARRAY  */
#line 969 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLER2DARRAY, NULL); }
#line 4125 "glsl.parser.c"
    break;

  case 181: /* type_specifier_nonarray: USAMPLERCUBEARRAY  */
#line 970 "glsl.y"
                                            { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLERCUBEARRAY, NULL); }
#line 4131 "glsl.parser.c"
    break;

  case 182: /* type_specifier_nonarray: SAMPLER2DRECT  */
#line 971 "glsl.y"
                                        { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER2DRECT, NULL); }
#line 4137 "glsl.parser.c"
    break;

  case 183: /* type_specifier_nonarray: SAMPLER2DRECTSHADOW  */
#line 972 "glsl.y"
                                              { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER2DRECTSHADOW, NULL); }
#line 4143 "glsl.parser.c"
    break;

  case 184: /* type_specifier_nonarray: ISAMPLER2DRECT  */
#line 973 "glsl.y"
                                         { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLER2DRECT, NULL); }
#line 4149 "glsl.parser.c"
    break;

  case 185: /* type_specifier_nonarray: USAMPLER2DRECT  */
#line 974 "glsl.y"
                                         { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLER2DRECT, NULL); }
#line 4155 "glsl.parser.c"
    break;

  case 186: /* type_specifier_nonarray: SAMPLERBUFFER  */
#line 975 "glsl.y"
                                        { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLERBUFFER, NULL); }
#line 4161 "glsl.parser.c"
    break;

  case 187: /* type_specifier_nonarray: ISAMPLERBUFFER  */
#line 976 "glsl.y"
                                         { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLERBUFFER, NULL); }
#line 4167 "glsl.parser.c"
    break;

  case 188: /* type_specifier_nonarray: USAMPLERBUFFER  */
#line 977 "glsl.y"
                                         { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLERBUFFER, NULL); }
#line 4173 "glsl.parser.c"
    break;

  case 189: /* type_specifier_nonarray: SAMPLER2DMS  */
#line 978 "glsl.y"
                                      { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER2DMS, NULL); }
#line 4179 "glsl.parser.c"
    break;

  case 190: /* type_specifier_nonarray: ISAMPLER2DMS  */
#line 979 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLER2DMS, NULL); }
#line 4185 "glsl.parser.c"
    break;

  case 191: /* type_specifier_nonarray: USAMPLER2DMS  */
#line 980 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLER2DMS, NULL); }
#line 4191 "glsl.parser.c"
    break;

  case 192: /* type_specifier_nonarray: SAMPLER2DMSARRAY  */
#line 981 "glsl.y"
                                           { (yyval.type_specifier_nonarray) = new_glsl_node(context, SAMPLER2DMSARRAY, NULL); }
#line 4197 "glsl.parser.c"
    break;

  case 193: /* type_specifier_nonarray: ISAMPLER2DMSARRAY  */
#line 982 "glsl.y"
                                            { (yyval.type_specifier_nonarray) = new_glsl_node(context, ISAMPLER2DMSARRAY, NULL); }
#line 4203 "glsl.parser.c"
    break;

  case 194: /* type_specifier_nonarray: USAMPLER2DMSARRAY  */
#line 983 "glsl.y"
                                            { (yyval.type_specifier_nonarray) = new_glsl_node(context, USAMPLER2DMSARRAY, NULL); }
#line 4209 "glsl.parser.c"
    break;

  case 195: /* type_specifier_nonarray: IMAGE1D  */
#line 984 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGE1D, NULL); }
#line 4215 "glsl.parser.c"
    break;

  case 196: /* type_specifier_nonarray: IIMAGE1D  */
#line 985 "glsl.y"
                                   { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGE1D, NULL); }
#line 4221 "glsl.parser.c"
    break;

  case 197: /* type_specifier_nonarray: UIMAGE1D  */
#line 986 "glsl.y"
                                   { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGE1D, NULL); }
#line 4227 "glsl.parser.c"
    break;

  case 198: /* type_specifier_nonarray: IMAGE2D  */
#line 987 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGE2D, NULL); }
#line 4233 "glsl.parser.c"
    break;

  case 199: /* type_specifier_nonarray: IIMAGE2D  */
#line 988 "glsl.y"
                                   { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGE2D, NULL); }
#line 4239 "glsl.parser.c"
    break;

  case 200: /* type_specifier_nonarray: UIMAGE2D  */
#line 989 "glsl.y"
                                   { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGE2D, NULL); }
#line 4245 "glsl.parser.c"
    break;

  case 201: /* type_specifier_nonarray: IMAGE3D  */
#line 990 "glsl.y"
                                  { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGE3D, NULL); }
#line 4251 "glsl.parser.c"
    break;

  case 202: /* type_specifier_nonarray: IIMAGE3D  */
#line 991 "glsl.y"
                                   { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGE3D, NULL); }
#line 4257 "glsl.parser.c"
    break;

  case 203: /* type_specifier_nonarray: UIMAGE3D  */
#line 992 "glsl.y"
                                   { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGE3D, NULL); }
#line 4263 "glsl.parser.c"
    break;

  case 204: /* type_specifier_nonarray: IMAGE2DRECT  */
#line 993 "glsl.y"
                                      { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGE2DRECT, NULL); }
#line 4269 "glsl.parser.c"
    break;

  case 205: /* type_specifier_nonarray: IIMAGE2DRECT  */
#line 994 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGE2DRECT, NULL); }
#line 4275 "glsl.parser.c"
    break;

  case 206: /* type_specifier_nonarray: UIMAGE2DRECT  */
#line 995 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGE2DRECT, NULL); }
#line 4281 "glsl.parser.c"
    break;

  case 207: /* type_specifier_nonarray: IMAGECUBE  */
#line 996 "glsl.y"
                                    { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGECUBE, NULL); }
#line 4287 "glsl.parser.c"
    break;

  case 208: /* type_specifier_nonarray: IIMAGECUBE  */
#line 997 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGECUBE, NULL); }
#line 4293 "glsl.parser.c"
    break;

  case 209: /* type_specifier_nonarray: UIMAGECUBE  */
#line 998 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGECUBE, NULL); }
#line 4299 "glsl.parser.c"
    break;

  case 210: /* type_specifier_nonarray: IMAGEBUFFER  */
#line 999 "glsl.y"
                                      { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGEBUFFER, NULL); }
#line 4305 "glsl.parser.c"
    break;

  case 211: /* type_specifier_nonarray: IIMAGEBUFFER  */
#line 1000 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGEBUFFER, NULL); }
#line 4311 "glsl.parser.c"
    break;

  case 212: /* type_specifier_nonarray: UIMAGEBUFFER  */
#line 1001 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGEBUFFER, NULL); }
#line 4317 "glsl.parser.c"
    break;

  case 213: /* type_specifier_nonarray: IMAGE1DARRAY  */
#line 1002 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGE1DARRAY, NULL); }
#line 4323 "glsl.parser.c"
    break;

  case 214: /* type_specifier_nonarray: IIMAGE1DARRAY  */
#line 1003 "glsl.y"
                                        { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGE1DARRAY, NULL); }
#line 4329 "glsl.parser.c"
    break;

  case 215: /* type_specifier_nonarray: UIMAGE1DARRAY  */
#line 1004 "glsl.y"
                                        { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGE1DARRAY, NULL); }
#line 4335 "glsl.parser.c"
    break;

  case 216: /* type_specifier_nonarray: IMAGE2DARRAY  */
#line 1005 "glsl.y"
                                       { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGE2DARRAY, NULL); }
#line 4341 "glsl.parser.c"
    break;

  case 217: /* type_specifier_nonarray: IIMAGE2DARRAY  */
#line 1006 "glsl.y"
                                        { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGE2DARRAY, NULL); }
#line 4347 "glsl.parser.c"
    break;

  case 218: /* type_specifier_nonarray: UIMAGE2DARRAY  */
#line 1007 "glsl.y"
                                        { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGE2DARRAY, NULL); }
#line 4353 "glsl.parser.c"
    break;

  case 219: /* type_specifier_nonarray: IMAGECUBEARRAY  */
#line 1008 "glsl.y"
                                         { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGECUBEARRAY, NULL); }
#line 4359 "glsl.parser.c"
    break;

  case 220: /* type_specifier_nonarray: IIMAGECUBEARRAY  */
#line 1009 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGECUBEARRAY, NULL); }
#line 4365 "glsl.parser.c"
    break;

  case 221: /* type_specifier_nonarray: UIMAGECUBEARRAY  */
#line 1010 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGECUBEARRAY, NULL); }
#line 4371 "glsl.parser.c"
    break;

  case 222: /* type_specifier_nonarray: IMAGE2DMS  */
#line 1011 "glsl.y"
                                    { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGE2DMS, NULL); }
#line 4377 "glsl.parser.c"
    break;

  case 223: /* type_specifier_nonarray: IIMAGE2DMS  */
#line 1012 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGE2DMS, NULL); }
#line 4383 "glsl.parser.c"
    break;

  case 224: /* type_specifier_nonarray: UIMAGE2DMS  */
#line 1013 "glsl.y"
                                     { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGE2DMS, NULL); }
#line 4389 "glsl.parser.c"
    break;

  case 225: /* type_specifier_nonarray: IMAGE2DMSARRAY  */
#line 1014 "glsl.y"
                                         { (yyval.type_specifier_nonarray) = new_glsl_node(context, IMAGE2DMSARRAY, NULL); }
#line 4395 "glsl.parser.c"
    break;

  case 226: /* type_specifier_nonarray: IIMAGE2DMSARRAY  */
#line 1015 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, IIMAGE2DMSARRAY, NULL); }
#line 4401 "glsl.parser.c"
    break;

  case 227: /* type_specifier_nonarray: UIMAGE2DMSARRAY  */
#line 1016 "glsl.y"
                                          { (yyval.type_specifier_nonarray) = new_glsl_node(context, UIMAGE2DMSARRAY, NULL); }
#line 4407 "glsl.parser.c"
    break;

  case 228: /* type_specifier_nonarray: struct_specifier  */
#line 1017 "glsl.y"
                                           { (yyval.type_specifier_nonarray) = (yyvsp[0].struct_specifier); }
#line 4413 "glsl.parser.c"
    break;

  case 229: /* type_specifier_nonarray: type_specifier_identifier  */
#line 1018 "glsl.y"
                                                    { (yyval.type_specifier_nonarray) = (yyvsp[0].type_specifier_identifier); }
#line 4419 "glsl.parser.c"
    break;

  case 230: /* struct_specifier: STRUCT struct_name LEFT_BRACE struct_declaration_list RIGHT_BRACE  */
#line 1022 "glsl.y"
                                { (yyval.struct_specifier) = new_glsl_node(context, STRUCT_SPECIFIER, (yyvsp[-3].struct_name), (yyvsp[-1].struct_declaration_list), NULL);}
#line 4425 "glsl.parser.c"
    break;

  case 231: /* struct_specifier: STRUCT LEFT_BRACE struct_declaration_list RIGHT_BRACE  */
#line 1025 "glsl.y"
                                { (yyval.struct_specifier) = new_glsl_node(context, STRUCT_SPECIFIER,
						new_glsl_identifier(context, NULL),
						(yyvsp[-1].struct_declaration_list),
						NULL); }
#line 4434 "glsl.parser.c"
    break;

  case 232: /* struct_declaration_list: struct_declaration  */
#line 1032 "glsl.y"
                                { (yyval.struct_declaration_list) = new_glsl_node(context, STRUCT_DECLARATION_LIST, (yyvsp[0].struct_declaration), NULL); }
#line 4440 "glsl.parser.c"
    break;

  case 233: /* struct_declaration_list: struct_declaration_list struct_declaration  */
#line 1034 "glsl.y"
                                { (yyval.struct_declaration_list) = new_glsl_node(context, STRUCT_DECLARATION_LIST, (yyvsp[-1].struct_declaration_list), (yyvsp[0].struct_declaration), NULL); }
#line 4446 "glsl.parser.c"
    break;

  case 234: /* struct_declaration: type_specifier struct_declarator_list SEMICOLON  */
#line 1038 "glsl.y"
                                { (yyval.struct_declaration) = new_glsl_node(context, STRUCT_DECLARATION,
					new_glsl_node(context, TYPE_QUALIFIER_LIST, NULL),
					(yyvsp[-2].type_specifier),
					(yyvsp[-1].struct_declarator_list),
					NULL); }
#line 4456 "glsl.parser.c"
    break;

  case 235: /* struct_declaration: type_qualifier type_specifier struct_declarator_list SEMICOLON  */
#line 1045 "glsl.y"
                                { (yyval.struct_declaration) = new_glsl_node(context, STRUCT_DECLARATION, (yyvsp[-3].type_qualifier), (yyvsp[-2].type_specifier), (yyvsp[-1].struct_declarator_list), NULL); }
#line 4462 "glsl.parser.c"
    break;

  case 236: /* struct_declarator_list: struct_declarator  */
#line 1049 "glsl.y"
                                { (yyval.struct_declarator_list) = new_glsl_node(context, STRUCT_DECLARATOR_LIST, (yyvsp[0].struct_declarator), NULL); }
#line 4468 "glsl.parser.c"
    break;

  case 237: /* struct_declarator_list: struct_declarator_list COMMA struct_declarator  */
#line 1052 "glsl.y"
                                { (yyval.struct_declarator_list) = new_glsl_node(context, STRUCT_DECLARATOR_LIST, (yyvsp[-2].struct_declarator_list), (yyvsp[0].struct_declarator), NULL); }
#line 4474 "glsl.parser.c"
    break;

  case 238: /* struct_declarator: field_identifier  */
#line 1056 "glsl.y"
                                { (yyval.struct_declarator) = new_glsl_node(context, STRUCT_DECLARATOR, (yyvsp[0].field_identifier), NULL); }
#line 4480 "glsl.parser.c"
    break;

  case 239: /* struct_declarator: field_identifier array_specifier_list  */
#line 1059 "glsl.y"
                                { (yyval.struct_declarator) = new_glsl_node(context, STRUCT_DECLARATOR, (yyvsp[-1].field_identifier), (yyvsp[0].array_specifier_list), NULL); }
#line 4486 "glsl.parser.c"
    break;

  case 240: /* type_qualifier: single_type_qualifier  */
#line 1063 "glsl.y"
                                { (yyval.type_qualifier) = new_glsl_node(context, TYPE_QUALIFIER_LIST, (yyvsp[0].single_type_qualifier), NULL); }
#line 4492 "glsl.parser.c"
    break;

  case 241: /* type_qualifier: type_qualifier single_type_qualifier  */
#line 1065 "glsl.y"
                                { (yyval.type_qualifier) = new_glsl_node(context, TYPE_QUALIFIER_LIST, (yyvsp[-1].type_qualifier), (yyvsp[0].single_type_qualifier), NULL); }
#line 4498 "glsl.parser.c"
    break;

  case 242: /* single_type_qualifier: storage_qualifier  */
#line 1068 "glsl.y"
                                            { (yyval.single_type_qualifier) = (yyvsp[0].storage_qualifier); }
#line 4504 "glsl.parser.c"
    break;

  case 243: /* single_type_qualifier: layout_qualifier  */
#line 1069 "glsl.y"
                                           { (yyval.single_type_qualifier) = (yyvsp[0].layout_qualifier); }
#line 4510 "glsl.parser.c"
    break;

  case 244: /* single_type_qualifier: precision_qualifier  */
#line 1070 "glsl.y"
                                              { (yyval.single_type_qualifier) = (yyvsp[0].precision_qualifier); }
#line 4516 "glsl.parser.c"
    break;

  case 245: /* single_type_qualifier: interpolation_qualifier  */
#line 1071 "glsl.y"
                                                  { (yyval.single_type_qualifier) = (yyvsp[0].interpolation_qualifier); }
#line 4522 "glsl.parser.c"
    break;

  case 246: /* single_type_qualifier: invariant_qualifier  */
#line 1072 "glsl.y"
                                              { (yyval.single_type_qualifier) = (yyvsp[0].invariant_qualifier); }
#line 4528 "glsl.parser.c"
    break;

  case 247: /* single_type_qualifier: precise_qualifier  */
#line 1073 "glsl.y"
                                            { (yyval.single_type_qualifier) = (yyvsp[0].precise_qualifier); }
#line 4534 "glsl.parser.c"
    break;

  case 248: /* layout_qualifier: LAYOUT LEFT_PAREN layout_qualifier_id_list RIGHT_PAREN  */
#line 1076 "glsl.y"
                                                                                 { (yyval.layout_qualifier) = (yyvsp[-1].layout_qualifier_id_list); }
#line 4540 "glsl.parser.c"
    break;

  case 249: /* layout_qualifier_id_list: layout_qualifier_id  */
#line 1079 "glsl.y"
                                              { (yyval.layout_qualifier_id_list) = (yyvsp[0].layout_qualifier_id); }
#line 4546 "glsl.parser.c"
    break;

  case 250: /* layout_qualifier_id_list: layout_qualifier_id_list COMMA layout_qualifier_id  */
#line 1082 "glsl.y"
                                { (yyval.layout_qualifier_id_list) = new_glsl_node(context, LAYOUT_QUALIFIER_ID_LIST, (yyvsp[-2].layout_qualifier_id_list), (yyvsp[0].layout_qualifier_id), NULL); }
#line 4552 "glsl.parser.c"
    break;

  case 251: /* layout_qualifier_id: layout_identifier  */
#line 1086 "glsl.y"
                                { (yyval.layout_qualifier_id) = new_glsl_node(context, LAYOUT_QUALIFIER_ID, (yyvsp[0].layout_identifier), NULL); }
#line 4558 "glsl.parser.c"
    break;

  case 252: /* layout_qualifier_id: layout_identifier EQUAL constant_expression  */
#line 1089 "glsl.y"
                                { (yyval.layout_qualifier_id) = new_glsl_node(context, LAYOUT_QUALIFIER_ID, (yyvsp[-2].layout_identifier), (yyvsp[0].constant_expression), NULL);}
#line 4564 "glsl.parser.c"
    break;

  case 253: /* layout_qualifier_id: SHARED  */
#line 1092 "glsl.y"
                                { (yyval.layout_qualifier_id) = new_glsl_node(context, SHARED, NULL); }
#line 4570 "glsl.parser.c"
    break;

  case 254: /* precision_qualifier: HIGHP  */
#line 1095 "glsl.y"
                                { (yyval.precision_qualifier) = new_glsl_node(context, HIGHP, NULL); }
#line 4576 "glsl.parser.c"
    break;

  case 255: /* precision_qualifier: MEDIUMP  */
#line 1096 "glsl.y"
                                  { (yyval.precision_qualifier) = new_glsl_node(context, MEDIUMP, NULL); }
#line 4582 "glsl.parser.c"
    break;

  case 256: /* precision_qualifier: LOWP  */
#line 1097 "glsl.y"
                               { (yyval.precision_qualifier) = new_glsl_node(context, LOWP, NULL); }
#line 4588 "glsl.parser.c"
    break;

  case 257: /* interpolation_qualifier: SMOOTH  */
#line 1100 "glsl.y"
                                 { (yyval.interpolation_qualifier) = new_glsl_node(context, SMOOTH, NULL); }
#line 4594 "glsl.parser.c"
    break;

  case 258: /* interpolation_qualifier: FLAT  */
#line 1101 "glsl.y"
                               { (yyval.interpolation_qualifier) = new_glsl_node(context, FLAT, NULL); }
#line 4600 "glsl.parser.c"
    break;

  case 259: /* interpolation_qualifier: NOPERSPECTIVE  */
#line 1102 "glsl.y"
                                        { (yyval.interpolation_qualifier) = new_glsl_node(context, NOPERSPECTIVE, NULL); }
#line 4606 "glsl.parser.c"
    break;

  case 260: /* invariant_qualifier: INVARIANT  */
#line 1105 "glsl.y"
                                    { (yyval.invariant_qualifier) = new_glsl_node(context, INVARIANT, NULL); }
#line 4612 "glsl.parser.c"
    break;

  case 261: /* precise_qualifier: PRECISE  */
#line 1108 "glsl.y"
                                  { (yyval.precise_qualifier) = new_glsl_node(context, PRECISE, NULL); }
#line 4618 "glsl.parser.c"
    break;

  case 262: /* storage_qualifier: CONST  */
#line 1111 "glsl.y"
                                { (yyval.storage_qualifier) = new_glsl_node(context, CONST, NULL); }
#line 4624 "glsl.parser.c"
    break;

  case 263: /* storage_qualifier: INOUT  */
#line 1112 "glsl.y"
                                { (yyval.storage_qualifier) = new_glsl_node(context, INOUT, NULL); }
#line 4630 "glsl.parser.c"
    break;

  case 264: /* storage_qualifier: IN  */
#line 1113 "glsl.y"
                             { (yyval.storage_qualifier) = new_glsl_node(context, IN, NULL); }
#line 4636 "glsl.parser.c"
    break;

  case 265: /* storage_qualifier: OUT  */
#line 1114 "glsl.y"
                              { (yyval.storage_qualifier) = new_glsl_node(context, OUT, NULL); }
#line 4642 "glsl.parser.c"
    break;

  case 266: /* storage_qualifier: CENTROID  */
#line 1115 "glsl.y"
                                   { (yyval.storage_qualifier) = new_glsl_node(context, CENTROID, NULL); }
#line 4648 "glsl.parser.c"
    break;

  case 267: /* storage_qualifier: PATCH  */
#line 1116 "glsl.y"
                                { (yyval.storage_qualifier) = new_glsl_node(context, PATCH, NULL); }
#line 4654 "glsl.parser.c"
    break;

  case 268: /* storage_qualifier: SAMPLE  */
#line 1117 "glsl.y"
                                 { (yyval.storage_qualifier) = new_glsl_node(context, SAMPLE, NULL); }
#line 4660 "glsl.parser.c"
    break;

  case 269: /* storage_qualifier: UNIFORM  */
#line 1118 "glsl.y"
                                  { (yyval.storage_qualifier) = new_glsl_node(context, UNIFORM, NULL); }
#line 4666 "glsl.parser.c"
    break;

  case 270: /* storage_qualifier: BUFFER  */
#line 1119 "glsl.y"
                                 { (yyval.storage_qualifier) = new_glsl_node(context, BUFFER, NULL); }
#line 4672 "glsl.parser.c"
    break;

  case 271: /* storage_qualifier: SHARED  */
#line 1120 "glsl.y"
                                 { (yyval.storage_qualifier) = new_glsl_node(context, SHARED, NULL); }
#line 4678 "glsl.parser.c"
    break;

  case 272: /* storage_qualifier: COHERENT  */
#line 1121 "glsl.y"
                                   { (yyval.storage_qualifier) = new_glsl_node(context, COHERENT, NULL); }
#line 4684 "glsl.parser.c"
    break;

  case 273: /* storage_qualifier: VOLATILE  */
#line 1122 "glsl.y"
                                   { (yyval.storage_qualifier) = new_glsl_node(context, VOLATILE, NULL); }
#line 4690 "glsl.parser.c"
    break;

  case 274: /* storage_qualifier: RESTRICT  */
#line 1123 "glsl.y"
                                   { (yyval.storage_qualifier) = new_glsl_node(context, RESTRICT, NULL); }
#line 4696 "glsl.parser.c"
    break;

  case 275: /* storage_qualifier: READONLY  */
#line 1124 "glsl.y"
                                   { (yyval.storage_qualifier) = new_glsl_node(context, READONLY, NULL); }
#line 4702 "glsl.parser.c"
    break;

  case 276: /* storage_qualifier: WRITEONLY  */
#line 1125 "glsl.y"
                                    { (yyval.storage_qualifier) = new_glsl_node(context, WRITEONLY, NULL); }
#line 4708 "glsl.parser.c"
    break;

  case 277: /* storage_qualifier: SUBROUTINE  */
#line 1126 "glsl.y"
                                     { (yyval.storage_qualifier) = new_glsl_node(context, SUBROUTINE, NULL); }
#line 4714 "glsl.parser.c"
    break;

  case 278: /* storage_qualifier: SUBROUTINE LEFT_PAREN type_name_list RIGHT_PAREN  */
#line 1128 "glsl.y"
                                { (yyval.storage_qualifier) = new_glsl_node(context, SUBROUTINE_TYPE,
					new_glsl_node(context, TYPE_NAME_LIST, (yyvsp[-1].type_name_list), NULL),
					NULL); }
#line 4722 "glsl.parser.c"
    break;

  case 279: /* type_name_list: type_name  */
#line 1133 "glsl.y"
                                    { (yyval.type_name_list) = (yyvsp[0].type_name); }
#line 4728 "glsl.parser.c"
    break;

  case 280: /* type_name_list: type_name_list COMMA type_name  */
#line 1135 "glsl.y"
                                { (yyval.type_name_list) = new_glsl_node(context, TYPE_NAME_LIST, (yyvsp[-2].type_name_list), (yyvsp[0].type_name), NULL); }
#line 4734 "glsl.parser.c"
    break;

  case 281: /* expression: assignment_expression  */
#line 1138 "glsl.y"
                                                { (yyval.expression) = (yyvsp[0].assignment_expression); }
#line 4740 "glsl.parser.c"
    break;

  case 282: /* expression: expression COMMA assignment_expression  */
#line 1140 "glsl.y"
                                { (yyval.expression) = new_glsl_node(context, COMMA, (yyvsp[-2].expression), (yyvsp[0].assignment_expression), NULL); }
#line 4746 "glsl.parser.c"
    break;

  case 283: /* assignment_expression: conditional_expression  */
#line 1143 "glsl.y"
                                                 { (yyval.assignment_expression) = (yyvsp[0].conditional_expression); }
#line 4752 "glsl.parser.c"
    break;

  case 284: /* assignment_expression: unary_expression assignment_operator assignment_expression  */
#line 1145 "glsl.y"
                                { (yyval.assignment_expression) = new_glsl_node(context,(yyvsp[-1].assignment_operator), (yyvsp[-2].unary_expression), (yyvsp[0].assignment_expression), NULL); }
#line 4758 "glsl.parser.c"
    break;

  case 285: /* assignment_operator: EQUAL  */
#line 1148 "glsl.y"
                                { (yyval.assignment_operator) = EQUAL; }
#line 4764 "glsl.parser.c"
    break;

  case 286: /* assignment_operator: MUL_ASSIGN  */
#line 1149 "glsl.y"
                                     { (yyval.assignment_operator) = MUL_ASSIGN; }
#line 4770 "glsl.parser.c"
    break;

  case 287: /* assignment_operator: DIV_ASSIGN  */
#line 1150 "glsl.y"
                                     { (yyval.assignment_operator) = DIV_ASSIGN; }
#line 4776 "glsl.parser.c"
    break;

  case 288: /* assignment_operator: MOD_ASSIGN  */
#line 1151 "glsl.y"
                                     { (yyval.assignment_operator) = MOD_ASSIGN; }
#line 4782 "glsl.parser.c"
    break;

  case 289: /* assignment_operator: ADD_ASSIGN  */
#line 1152 "glsl.y"
                                     { (yyval.assignment_operator) = ADD_ASSIGN; }
#line 4788 "glsl.parser.c"
    break;

  case 290: /* assignment_operator: SUB_ASSIGN  */
#line 1153 "glsl.y"
                                     { (yyval.assignment_operator) = SUB_ASSIGN; }
#line 4794 "glsl.parser.c"
    break;

  case 291: /* assignment_operator: LEFT_ASSIGN  */
#line 1154 "glsl.y"
                                      { (yyval.assignment_operator) = LEFT_ASSIGN; }
#line 4800 "glsl.parser.c"
    break;

  case 292: /* assignment_operator: RIGHT_ASSIGN  */
#line 1155 "glsl.y"
                                       { (yyval.assignment_operator) = RIGHT_ASSIGN; }
#line 4806 "glsl.parser.c"
    break;

  case 293: /* assignment_operator: AND_ASSIGN  */
#line 1156 "glsl.y"
                                     { (yyval.assignment_operator) = AND_ASSIGN; }
#line 4812 "glsl.parser.c"
    break;

  case 294: /* assignment_operator: XOR_ASSIGN  */
#line 1157 "glsl.y"
                                     { (yyval.assignment_operator) = XOR_ASSIGN; }
#line 4818 "glsl.parser.c"
    break;

  case 295: /* assignment_operator: OR_ASSIGN  */
#line 1158 "glsl.y"
                                    { (yyval.assignment_operator) = OR_ASSIGN; }
#line 4824 "glsl.parser.c"
    break;

  case 296: /* constant_expression: conditional_expression  */
#line 1161 "glsl.y"
                                                 { (yyval.constant_expression) = (yyvsp[0].conditional_expression); }
#line 4830 "glsl.parser.c"
    break;

  case 297: /* conditional_expression: logical_or_expression  */
#line 1164 "glsl.y"
                                                { (yyval.conditional_expression) = (yyvsp[0].logical_or_expression); }
#line 4836 "glsl.parser.c"
    break;

  case 298: /* conditional_expression: logical_or_expression QUESTION expression COLON assignment_expression  */
#line 1166 "glsl.y"
                                { (yyval.conditional_expression) = new_glsl_node(context, TERNARY_EXPRESSION, (yyvsp[-4].logical_or_expression), (yyvsp[-2].expression), (yyvsp[0].assignment_expression), NULL); }
#line 4842 "glsl.parser.c"
    break;

  case 299: /* logical_or_expression: logical_xor_expression  */
#line 1169 "glsl.y"
                                                 { (yyval.logical_or_expression) = (yyvsp[0].logical_xor_expression); }
#line 4848 "glsl.parser.c"
    break;

  case 300: /* logical_or_expression: logical_or_expression OR_OP logical_xor_expression  */
#line 1171 "glsl.y"
                                { (yyval.logical_or_expression) = new_glsl_node(context, OR_OP, (yyvsp[-2].logical_or_expression), (yyvsp[0].logical_xor_expression), NULL); }
#line 4854 "glsl.parser.c"
    break;

  case 301: /* logical_xor_expression: logical_and_expression  */
#line 1174 "glsl.y"
                                                 { (yyval.logical_xor_expression) = (yyvsp[0].logical_and_expression); }
#line 4860 "glsl.parser.c"
    break;

  case 302: /* logical_xor_expression: logical_xor_expression XOR_OP logical_and_expression  */
#line 1176 "glsl.y"
                                { (yyval.logical_xor_expression) = new_glsl_node(context, XOR_OP, (yyvsp[-2].logical_xor_expression), (yyvsp[0].logical_and_expression), NULL); }
#line 4866 "glsl.parser.c"
    break;

  case 303: /* logical_and_expression: inclusive_or_expression  */
#line 1179 "glsl.y"
                                                  { (yyval.logical_and_expression) = (yyvsp[0].inclusive_or_expression); }
#line 4872 "glsl.parser.c"
    break;

  case 304: /* logical_and_expression: logical_and_expression AND_OP inclusive_or_expression  */
#line 1181 "glsl.y"
                                { (yyval.logical_and_expression) = new_glsl_node(context, AND_OP, (yyvsp[-2].logical_and_expression), (yyvsp[0].inclusive_or_expression), NULL); }
#line 4878 "glsl.parser.c"
    break;

  case 305: /* inclusive_or_expression: exclusive_or_expression  */
#line 1184 "glsl.y"
                                                  { (yyval.inclusive_or_expression) = (yyvsp[0].exclusive_or_expression); }
#line 4884 "glsl.parser.c"
    break;

  case 306: /* inclusive_or_expression: inclusive_or_expression VERTICAL_BAR exclusive_or_expression  */
#line 1186 "glsl.y"
                                { (yyval.inclusive_or_expression) = new_glsl_node(context, VERTICAL_BAR, (yyvsp[-2].inclusive_or_expression), (yyvsp[0].exclusive_or_expression), NULL); }
#line 4890 "glsl.parser.c"
    break;

  case 307: /* exclusive_or_expression: and_expression  */
#line 1189 "glsl.y"
                                         { (yyval.exclusive_or_expression) = (yyvsp[0].and_expression); }
#line 4896 "glsl.parser.c"
    break;

  case 308: /* exclusive_or_expression: exclusive_or_expression CARET and_expression  */
#line 1191 "glsl.y"
                                { (yyval.exclusive_or_expression) = new_glsl_node(context, CARET, (yyvsp[-2].exclusive_or_expression), (yyvsp[0].and_expression), NULL); }
#line 4902 "glsl.parser.c"
    break;

  case 309: /* and_expression: equality_expression  */
#line 1194 "glsl.y"
                                              { (yyval.and_expression) = (yyvsp[0].equality_expression); }
#line 4908 "glsl.parser.c"
    break;

  case 310: /* and_expression: and_expression AMPERSAND equality_expression  */
#line 1196 "glsl.y"
                                { (yyval.and_expression) = new_glsl_node(context, AMPERSAND, (yyvsp[-2].and_expression), (yyvsp[0].equality_expression), NULL); }
#line 4914 "glsl.parser.c"
    break;

  case 311: /* equality_expression: relational_expression  */
#line 1199 "glsl.y"
                                                { (yyval.equality_expression) = (yyvsp[0].relational_expression); }
#line 4920 "glsl.parser.c"
    break;

  case 312: /* equality_expression: equality_expression EQ_OP relational_expression  */
#line 1202 "glsl.y"
                                { (yyval.equality_expression) = new_glsl_node(context, EQ_OP, (yyvsp[-2].equality_expression), (yyvsp[0].relational_expression), NULL); }
#line 4926 "glsl.parser.c"
    break;

  case 313: /* equality_expression: equality_expression NE_OP relational_expression  */
#line 1205 "glsl.y"
                                { (yyval.equality_expression) = new_glsl_node(context, NE_OP, (yyvsp[-2].equality_expression), (yyvsp[0].relational_expression), NULL); }
#line 4932 "glsl.parser.c"
    break;

  case 314: /* relational_expression: shift_expression  */
#line 1208 "glsl.y"
                                           { (yyval.relational_expression) = (yyvsp[0].shift_expression); }
#line 4938 "glsl.parser.c"
    break;

  case 315: /* relational_expression: relational_expression LEFT_ANGLE shift_expression  */
#line 1211 "glsl.y"
                                { (yyval.relational_expression) = new_glsl_node(context, LEFT_ANGLE, (yyvsp[-2].relational_expression), (yyvsp[0].shift_expression), NULL); }
#line 4944 "glsl.parser.c"
    break;

  case 316: /* relational_expression: relational_expression RIGHT_ANGLE shift_expression  */
#line 1214 "glsl.y"
                                { (yyval.relational_expression) = new_glsl_node(context, RIGHT_ANGLE, (yyvsp[-2].relational_expression), (yyvsp[0].shift_expression), NULL); }
#line 4950 "glsl.parser.c"
    break;

  case 317: /* relational_expression: relational_expression LE_OP shift_expression  */
#line 1217 "glsl.y"
                                { (yyval.relational_expression) = new_glsl_node(context, LE_OP, (yyvsp[-2].relational_expression), (yyvsp[0].shift_expression), NULL); }
#line 4956 "glsl.parser.c"
    break;

  case 318: /* relational_expression: relational_expression GE_OP shift_expression  */
#line 1220 "glsl.y"
                                { (yyval.relational_expression) = new_glsl_node(context, GE_OP, (yyvsp[-2].relational_expression), (yyvsp[0].shift_expression), NULL); }
#line 4962 "glsl.parser.c"
    break;

  case 319: /* shift_expression: additive_expression  */
#line 1223 "glsl.y"
                                              { (yyval.shift_expression) = (yyvsp[0].additive_expression); }
#line 4968 "glsl.parser.c"
    break;

  case 320: /* shift_expression: shift_expression LEFT_OP additive_expression  */
#line 1226 "glsl.y"
                                { (yyval.shift_expression) = new_glsl_node(context, LEFT_OP, (yyvsp[-2].shift_expression), (yyvsp[0].additive_expression), NULL); }
#line 4974 "glsl.parser.c"
    break;

  case 321: /* shift_expression: shift_expression RIGHT_OP additive_expression  */
#line 1229 "glsl.y"
                                { (yyval.shift_expression) = new_glsl_node(context, RIGHT_OP, (yyvsp[-2].shift_expression), (yyvsp[0].additive_expression), NULL); }
#line 4980 "glsl.parser.c"
    break;

  case 322: /* additive_expression: multiplicative_expression  */
#line 1232 "glsl.y"
                                                    { (yyval.additive_expression) = (yyvsp[0].multiplicative_expression); }
#line 4986 "glsl.parser.c"
    break;

  case 323: /* additive_expression: additive_expression PLUS multiplicative_expression  */
#line 1235 "glsl.y"
                                { (yyval.additive_expression) = new_glsl_node(context, PLUS, (yyvsp[-2].additive_expression), (yyvsp[0].multiplicative_expression), NULL); }
#line 4992 "glsl.parser.c"
    break;

  case 324: /* additive_expression: additive_expression DASH multiplicative_expression  */
#line 1238 "glsl.y"
                                { (yyval.additive_expression) = new_glsl_node(context, DASH, (yyvsp[-2].additive_expression), (yyvsp[0].multiplicative_expression), NULL); }
#line 4998 "glsl.parser.c"
    break;

  case 325: /* multiplicative_expression: unary_expression  */
#line 1241 "glsl.y"
                                             { (yyval.multiplicative_expression) = (yyvsp[0].unary_expression); }
#line 5004 "glsl.parser.c"
    break;

  case 326: /* multiplicative_expression: multiplicative_expression STAR unary_expression  */
#line 1244 "glsl.y"
                                { (yyval.multiplicative_expression) = new_glsl_node(context, STAR, (yyvsp[-2].multiplicative_expression), (yyvsp[0].unary_expression), NULL); }
#line 5010 "glsl.parser.c"
    break;

  case 327: /* multiplicative_expression: multiplicative_expression SLASH unary_expression  */
#line 1247 "glsl.y"
                                { (yyval.multiplicative_expression) = new_glsl_node(context, SLASH, (yyvsp[-2].multiplicative_expression), (yyvsp[0].unary_expression), NULL); }
#line 5016 "glsl.parser.c"
    break;

  case 328: /* multiplicative_expression: multiplicative_expression PERCENT unary_expression  */
#line 1250 "glsl.y"
                                { (yyval.multiplicative_expression) = new_glsl_node(context, PERCENT, (yyvsp[-2].multiplicative_expression), (yyvsp[0].unary_expression), NULL); }
#line 5022 "glsl.parser.c"
    break;

  case 329: /* unary_expression: postfix_expression  */
#line 1253 "glsl.y"
                                             { (yyval.unary_expression) = (yyvsp[0].postfix_expression); }
#line 5028 "glsl.parser.c"
    break;

  case 330: /* unary_expression: INC_OP unary_expression  */
#line 1256 "glsl.y"
                                { (yyval.unary_expression) = new_glsl_node(context, PRE_INC_OP, (yyvsp[0].unary_expression), NULL); }
#line 5034 "glsl.parser.c"
    break;

  case 331: /* unary_expression: DEC_OP unary_expression  */
#line 1259 "glsl.y"
                                { (yyval.unary_expression) = new_glsl_node(context, PRE_DEC_OP, (yyvsp[0].unary_expression), NULL); }
#line 5040 "glsl.parser.c"
    break;

  case 332: /* unary_expression: unary_operator unary_expression  */
#line 1262 "glsl.y"
                                { (yyval.unary_expression) = new_glsl_node(context,(yyvsp[-1].unary_operator), (yyvsp[0].unary_expression), NULL); }
#line 5046 "glsl.parser.c"
    break;

  case 333: /* unary_operator: PLUS  */
#line 1265 "glsl.y"
                               { (yyval.unary_operator) = UNARY_PLUS; }
#line 5052 "glsl.parser.c"
    break;

  case 334: /* unary_operator: DASH  */
#line 1266 "glsl.y"
                               { (yyval.unary_operator) = UNARY_DASH; }
#line 5058 "glsl.parser.c"
    break;

  case 335: /* unary_operator: BANG  */
#line 1267 "glsl.y"
                               { (yyval.unary_operator) = BANG; }
#line 5064 "glsl.parser.c"
    break;

  case 336: /* unary_operator: TILDE  */
#line 1268 "glsl.y"
                                { (yyval.unary_operator) = TILDE; }
#line 5070 "glsl.parser.c"
    break;

  case 337: /* postfix_expression: primary_expression  */
#line 1271 "glsl.y"
                                             { (yyval.postfix_expression) = (yyvsp[0].primary_expression); }
#line 5076 "glsl.parser.c"
    break;

  case 338: /* postfix_expression: postfix_expression LEFT_BRACKET integer_expression RIGHT_BRACKET  */
#line 1274 "glsl.y"
                                { (yyval.postfix_expression) = new_glsl_node(context, ARRAY_REF_OP, (yyvsp[-3].postfix_expression), (yyvsp[-1].integer_expression), NULL); }
#line 5082 "glsl.parser.c"
    break;

  case 339: /* postfix_expression: function_call  */
#line 1276 "glsl.y"
                                        { (yyval.postfix_expression) = (yyvsp[0].function_call); }
#line 5088 "glsl.parser.c"
    break;

  case 340: /* postfix_expression: postfix_expression DOT field_identifier  */
#line 1279 "glsl.y"
                                { (yyval.postfix_expression) = new_glsl_node(context, DOT, (yyvsp[-2].postfix_expression), (yyvsp[0].field_identifier), NULL);}
#line 5094 "glsl.parser.c"
    break;

  case 341: /* postfix_expression: postfix_expression INC_OP  */
#line 1282 "glsl.y"
                                { (yyval.postfix_expression) = new_glsl_node(context, POST_INC_OP, (yyvsp[-1].postfix_expression), NULL); }
#line 5100 "glsl.parser.c"
    break;

  case 342: /* postfix_expression: postfix_expression DEC_OP  */
#line 1285 "glsl.y"
                                { (yyval.postfix_expression) = new_glsl_node(context, POST_DEC_OP, (yyvsp[-1].postfix_expression), NULL); }
#line 5106 "glsl.parser.c"
    break;

  case 343: /* integer_expression: expression  */
#line 1288 "glsl.y"
                                     { (yyval.integer_expression) = (yyvsp[0].expression); }
#line 5112 "glsl.parser.c"
    break;

  case 344: /* function_call: function_call_or_method  */
#line 1291 "glsl.y"
                                                  { (yyval.function_call) = (yyvsp[0].function_call_or_method); }
#line 5118 "glsl.parser.c"
    break;

  case 345: /* function_call_or_method: function_call_generic  */
#line 1294 "glsl.y"
                                                { (yyval.function_call_or_method) = (yyvsp[0].function_call_generic); }
#line 5124 "glsl.parser.c"
    break;

  case 346: /* function_call_generic: function_identifier LEFT_PAREN function_call_parameter_list RIGHT_PAREN  */
#line 1298 "glsl.y"
                                { (yyval.function_call_generic) = new_glsl_node(context, FUNCTION_CALL, (yyvsp[-3].function_identifier), (yyvsp[-1].function_call_parameter_list), NULL); }
#line 5130 "glsl.parser.c"
    break;

  case 347: /* function_call_generic: function_identifier LEFT_PAREN LEFT_PAREN  */
#line 1301 "glsl.y"
                                { (yyval.function_call_generic) = new_glsl_node(context, FUNCTION_CALL,
					(yyvsp[-2].function_identifier),
					new_glsl_node(context, FUNCTION_CALL_PARAMETER_LIST, NULL),
					NULL); }
#line 5139 "glsl.parser.c"
    break;

  case 348: /* function_call_generic: function_identifier LEFT_PAREN VOID RIGHT_PAREN  */
#line 1307 "glsl.y"
                                { (yyval.function_call_generic) = new_glsl_node(context, FUNCTION_CALL,
					(yyvsp[-3].function_identifier),
					new_glsl_node(context, FUNCTION_CALL_PARAMETER_LIST, NULL),
					NULL); }
#line 5148 "glsl.parser.c"
    break;

  case 349: /* function_call_parameter_list: assignment_expression  */
#line 1314 "glsl.y"
                                { (yyval.function_call_parameter_list) = new_glsl_node(context, FUNCTION_CALL_PARAMETER_LIST, (yyvsp[0].assignment_expression), NULL); }
#line 5154 "glsl.parser.c"
    break;

  case 350: /* function_call_parameter_list: function_call_parameter_list COMMA assignment_expression  */
#line 1317 "glsl.y"
                                { (yyval.function_call_parameter_list) = new_glsl_node(context, FUNCTION_CALL_PARAMETER_LIST, (yyvsp[-2].function_call_parameter_list), (yyvsp[0].assignment_expression), NULL); }
#line 5160 "glsl.parser.c"
    break;

  case 351: /* function_identifier: type_specifier  */
#line 1320 "glsl.y"
                                         { (yyval.function_identifier) = (yyvsp[0].type_specifier); }
#line 5166 "glsl.parser.c"
    break;

  case 352: /* function_identifier: postfix_expression  */
#line 1323 "glsl.y"
                                { (yyval.function_identifier) = new_glsl_node(context, POSTFIX_EXPRESSION, (yyvsp[0].postfix_expression), NULL); }
#line 5172 "glsl.parser.c"
    break;

  case 353: /* primary_expression: variable_identifier  */
#line 1326 "glsl.y"
                                              { (yyval.primary_expression) = (yyvsp[0].variable_identifier); }
#line 5178 "glsl.parser.c"
    break;

  case 354: /* primary_expression: INTCONSTANT  */
#line 1329 "glsl.y"
                                { (yyval.primary_expression) = new_glsl_node(context, INTCONSTANT, NULL); (yyval.primary_expression)->data.i = (yyvsp[0].INTCONSTANT); }
#line 5184 "glsl.parser.c"
    break;

  case 355: /* primary_expression: UINTCONSTANT  */
#line 1332 "glsl.y"
                                { (yyval.primary_expression) = new_glsl_node(context, UINTCONSTANT, NULL); (yyval.primary_expression)->data.ui = (yyvsp[0].UINTCONSTANT); }
#line 5190 "glsl.parser.c"
    break;

  case 356: /* primary_expression: FLOATCONSTANT  */
#line 1335 "glsl.y"
                                { (yyval.primary_expression) = new_glsl_node(context, FLOATCONSTANT, NULL); (yyval.primary_expression)->data.f = (yyvsp[0].FLOATCONSTANT); }
#line 5196 "glsl.parser.c"
    break;

  case 357: /* primary_expression: TRUE_VALUE  */
#line 1338 "glsl.y"
                                { (yyval.primary_expression) = new_glsl_node(context, TRUE_VALUE, NULL); }
#line 5202 "glsl.parser.c"
    break;

  case 358: /* primary_expression: FALSE_VALUE  */
#line 1341 "glsl.y"
                                { (yyval.primary_expression) = new_glsl_node(context, FALSE_VALUE, NULL); }
#line 5208 "glsl.parser.c"
    break;

  case 359: /* primary_expression: DOUBLECONSTANT  */
#line 1344 "glsl.y"
                                { (yyval.primary_expression) = new_glsl_node(context, DOUBLECONSTANT, NULL); (yyval.primary_expression)->data.d = (yyvsp[0].DOUBLECONSTANT); }
#line 5214 "glsl.parser.c"
    break;

  case 360: /* primary_expression: LEFT_PAREN expression RIGHT_PAREN  */
#line 1347 "glsl.y"
                                { (yyval.primary_expression) = new_glsl_node(context, PAREN_EXPRESSION, (yyvsp[-1].expression), NULL); }
#line 5220 "glsl.parser.c"
    break;


#line 5224 "glsl.parser.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == GLSL_EMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (&yylloc, context, YY_("syntax error"));
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= GLSL_EOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == GLSL_EOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, context);
          yychar = GLSL_EMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, context);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, context, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != GLSL_EMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, context);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, context);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1350 "glsl.y"


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
