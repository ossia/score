#pragma once

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(UNICODE)
#define UNICODE 1
#endif
#if !defined(_UNICODE)
#define _UNICODE 1
#endif
#include <windows.h>
#endif

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

// keep in sync with glsl.y
// necessary as windows.h also defines FLOAT, etc.
enum class glsl_tokentype
{
  GLSL_EMPTY = -2,
  GLSL_EOF = 0,                       /* "end of file"  */
  GLSL_error = 256,                   /* error  */
  GLSL_UNDEF = 257,                   /* "invalid token"  */
  CONST = 258,                        /* CONST  */
  BOOL = 259,                         /* BOOL  */
  FLOAT = 260,                        /* FLOAT  */
  DOUBLE = 261,                       /* DOUBLE  */
  INT = 262,                          /* INT  */
  UINT = 263,                         /* UINT  */
  BREAK = 264,                        /* BREAK  */
  CONTINUE = 265,                     /* CONTINUE  */
  DO = 266,                           /* DO  */
  ELSE = 267,                         /* ELSE  */
  FOR = 268,                          /* FOR  */
  IF = 269,                           /* IF  */
  DISCARD = 270,                      /* DISCARD  */
  RETURN = 271,                       /* RETURN  */
  RETURN_VALUE = 272,                 /* RETURN_VALUE  */
  SWITCH = 273,                       /* SWITCH  */
  CASE = 274,                         /* CASE  */
  DEFAULT = 275,                      /* DEFAULT  */
  SUBROUTINE = 276,                   /* SUBROUTINE  */
  BVEC2 = 277,                        /* BVEC2  */
  BVEC3 = 278,                        /* BVEC3  */
  BVEC4 = 279,                        /* BVEC4  */
  IVEC2 = 280,                        /* IVEC2  */
  IVEC3 = 281,                        /* IVEC3  */
  IVEC4 = 282,                        /* IVEC4  */
  UVEC2 = 283,                        /* UVEC2  */
  UVEC3 = 284,                        /* UVEC3  */
  UVEC4 = 285,                        /* UVEC4  */
  VEC2 = 286,                         /* VEC2  */
  VEC3 = 287,                         /* VEC3  */
  VEC4 = 288,                         /* VEC4  */
  MAT2 = 289,                         /* MAT2  */
  MAT3 = 290,                         /* MAT3  */
  MAT4 = 291,                         /* MAT4  */
  CENTROID = 292,                     /* CENTROID  */
  IN = 293,                           /* IN  */
  OUT = 294,                          /* OUT  */
  INOUT = 295,                        /* INOUT  */
  UNIFORM = 296,                      /* UNIFORM  */
  PATCH = 297,                        /* PATCH  */
  SAMPLE = 298,                       /* SAMPLE  */
  BUFFER = 299,                       /* BUFFER  */
  SHARED = 300,                       /* SHARED  */
  COHERENT = 301,                     /* COHERENT  */
  VOLATILE = 302,                     /* VOLATILE  */
  RESTRICT = 303,                     /* RESTRICT  */
  READONLY = 304,                     /* READONLY  */
  WRITEONLY = 305,                    /* WRITEONLY  */
  DVEC2 = 306,                        /* DVEC2  */
  DVEC3 = 307,                        /* DVEC3  */
  DVEC4 = 308,                        /* DVEC4  */
  DMAT2 = 309,                        /* DMAT2  */
  DMAT3 = 310,                        /* DMAT3  */
  DMAT4 = 311,                        /* DMAT4  */
  NOPERSPECTIVE = 312,                /* NOPERSPECTIVE  */
  FLAT = 313,                         /* FLAT  */
  SMOOTH = 314,                       /* SMOOTH  */
  LAYOUT = 315,                       /* LAYOUT  */
  MAT2X2 = 316,                       /* MAT2X2  */
  MAT2X3 = 317,                       /* MAT2X3  */
  MAT2X4 = 318,                       /* MAT2X4  */
  MAT3X2 = 319,                       /* MAT3X2  */
  MAT3X3 = 320,                       /* MAT3X3  */
  MAT3X4 = 321,                       /* MAT3X4  */
  MAT4X2 = 322,                       /* MAT4X2  */
  MAT4X3 = 323,                       /* MAT4X3  */
  MAT4X4 = 324,                       /* MAT4X4  */
  DMAT2X2 = 325,                      /* DMAT2X2  */
  DMAT2X3 = 326,                      /* DMAT2X3  */
  DMAT2X4 = 327,                      /* DMAT2X4  */
  DMAT3X2 = 328,                      /* DMAT3X2  */
  DMAT3X3 = 329,                      /* DMAT3X3  */
  DMAT3X4 = 330,                      /* DMAT3X4  */
  DMAT4X2 = 331,                      /* DMAT4X2  */
  DMAT4X3 = 332,                      /* DMAT4X3  */
  DMAT4X4 = 333,                      /* DMAT4X4  */
  ATOMIC_UINT = 334,                  /* ATOMIC_UINT  */
  SAMPLER1D = 335,                    /* SAMPLER1D  */
  SAMPLER2D = 336,                    /* SAMPLER2D  */
  SAMPLER3D = 337,                    /* SAMPLER3D  */
  SAMPLERCUBE = 338,                  /* SAMPLERCUBE  */
  SAMPLER1DSHADOW = 339,              /* SAMPLER1DSHADOW  */
  SAMPLER2DSHADOW = 340,              /* SAMPLER2DSHADOW  */
  SAMPLERCUBESHADOW = 341,            /* SAMPLERCUBESHADOW  */
  SAMPLER1DARRAY = 342,               /* SAMPLER1DARRAY  */
  SAMPLER2DARRAY = 343,               /* SAMPLER2DARRAY  */
  SAMPLER1DARRAYSHADOW = 344,         /* SAMPLER1DARRAYSHADOW  */
  SAMPLER2DARRAYSHADOW = 345,         /* SAMPLER2DARRAYSHADOW  */
  ISAMPLER1D = 346,                   /* ISAMPLER1D  */
  ISAMPLER2D = 347,                   /* ISAMPLER2D  */
  ISAMPLER3D = 348,                   /* ISAMPLER3D  */
  ISAMPLERCUBE = 349,                 /* ISAMPLERCUBE  */
  ISAMPLER1DARRAY = 350,              /* ISAMPLER1DARRAY  */
  ISAMPLER2DARRAY = 351,              /* ISAMPLER2DARRAY  */
  USAMPLER1D = 352,                   /* USAMPLER1D  */
  USAMPLER2D = 353,                   /* USAMPLER2D  */
  USAMPLER3D = 354,                   /* USAMPLER3D  */
  USAMPLERCUBE = 355,                 /* USAMPLERCUBE  */
  USAMPLER1DARRAY = 356,              /* USAMPLER1DARRAY  */
  USAMPLER2DARRAY = 357,              /* USAMPLER2DARRAY  */
  SAMPLER2DRECT = 358,                /* SAMPLER2DRECT  */
  SAMPLER2DRECTSHADOW = 359,          /* SAMPLER2DRECTSHADOW  */
  ISAMPLER2DRECT = 360,               /* ISAMPLER2DRECT  */
  USAMPLER2DRECT = 361,               /* USAMPLER2DRECT  */
  SAMPLERBUFFER = 362,                /* SAMPLERBUFFER  */
  ISAMPLERBUFFER = 363,               /* ISAMPLERBUFFER  */
  USAMPLERBUFFER = 364,               /* USAMPLERBUFFER  */
  SAMPLERCUBEARRAY = 365,             /* SAMPLERCUBEARRAY  */
  SAMPLERCUBEARRAYSHADOW = 366,       /* SAMPLERCUBEARRAYSHADOW  */
  ISAMPLERCUBEARRAY = 367,            /* ISAMPLERCUBEARRAY  */
  USAMPLERCUBEARRAY = 368,            /* USAMPLERCUBEARRAY  */
  SAMPLER2DMS = 369,                  /* SAMPLER2DMS  */
  ISAMPLER2DMS = 370,                 /* ISAMPLER2DMS  */
  USAMPLER2DMS = 371,                 /* USAMPLER2DMS  */
  SAMPLER2DMSARRAY = 372,             /* SAMPLER2DMSARRAY  */
  ISAMPLER2DMSARRAY = 373,            /* ISAMPLER2DMSARRAY  */
  USAMPLER2DMSARRAY = 374,            /* USAMPLER2DMSARRAY  */
  IMAGE1D = 375,                      /* IMAGE1D  */
  IIMAGE1D = 376,                     /* IIMAGE1D  */
  UIMAGE1D = 377,                     /* UIMAGE1D  */
  IMAGE2D = 378,                      /* IMAGE2D  */
  IIMAGE2D = 379,                     /* IIMAGE2D  */
  UIMAGE2D = 380,                     /* UIMAGE2D  */
  IMAGE3D = 381,                      /* IMAGE3D  */
  IIMAGE3D = 382,                     /* IIMAGE3D  */
  UIMAGE3D = 383,                     /* UIMAGE3D  */
  IMAGE2DRECT = 384,                  /* IMAGE2DRECT  */
  IIMAGE2DRECT = 385,                 /* IIMAGE2DRECT  */
  UIMAGE2DRECT = 386,                 /* UIMAGE2DRECT  */
  IMAGECUBE = 387,                    /* IMAGECUBE  */
  IIMAGECUBE = 388,                   /* IIMAGECUBE  */
  UIMAGECUBE = 389,                   /* UIMAGECUBE  */
  IMAGEBUFFER = 390,                  /* IMAGEBUFFER  */
  IIMAGEBUFFER = 391,                 /* IIMAGEBUFFER  */
  UIMAGEBUFFER = 392,                 /* UIMAGEBUFFER  */
  IMAGE1DARRAY = 393,                 /* IMAGE1DARRAY  */
  IIMAGE1DARRAY = 394,                /* IIMAGE1DARRAY  */
  UIMAGE1DARRAY = 395,                /* UIMAGE1DARRAY  */
  IMAGE2DARRAY = 396,                 /* IMAGE2DARRAY  */
  IIMAGE2DARRAY = 397,                /* IIMAGE2DARRAY  */
  UIMAGE2DARRAY = 398,                /* UIMAGE2DARRAY  */
  IMAGECUBEARRAY = 399,               /* IMAGECUBEARRAY  */
  IIMAGECUBEARRAY = 400,              /* IIMAGECUBEARRAY  */
  UIMAGECUBEARRAY = 401,              /* UIMAGECUBEARRAY  */
  IMAGE2DMS = 402,                    /* IMAGE2DMS  */
  IIMAGE2DMS = 403,                   /* IIMAGE2DMS  */
  UIMAGE2DMS = 404,                   /* UIMAGE2DMS  */
  IMAGE2DMSARRAY = 405,               /* IMAGE2DMSARRAY  */
  IIMAGE2DMSARRAY = 406,              /* IIMAGE2DMSARRAY  */
  UIMAGE2DMSARRAY = 407,              /* UIMAGE2DMSARRAY  */
  STRUCT = 408,                       /* STRUCT  */
  VOID = 409,                         /* VOID  */
  WHILE = 410,                        /* WHILE  */
  IDENTIFIER = 411,                   /* IDENTIFIER  */
  FLOATCONSTANT = 412,                /* FLOATCONSTANT  */
  DOUBLECONSTANT = 413,               /* DOUBLECONSTANT  */
  INTCONSTANT = 414,                  /* INTCONSTANT  */
  UINTCONSTANT = 415,                 /* UINTCONSTANT  */
  TRUE_VALUE = 416,                   /* TRUE_VALUE  */
  FALSE_VALUE = 417,                  /* FALSE_VALUE  */
  LEFT_OP = 418,                      /* LEFT_OP  */
  RIGHT_OP = 419,                     /* RIGHT_OP  */
  INC_OP = 420,                       /* INC_OP  */
  DEC_OP = 421,                       /* DEC_OP  */
  LE_OP = 422,                        /* LE_OP  */
  GE_OP = 423,                        /* GE_OP  */
  EQ_OP = 424,                        /* EQ_OP  */
  NE_OP = 425,                        /* NE_OP  */
  AND_OP = 426,                       /* AND_OP  */
  OR_OP = 427,                        /* OR_OP  */
  XOR_OP = 428,                       /* XOR_OP  */
  MUL_ASSIGN = 429,                   /* MUL_ASSIGN  */
  DIV_ASSIGN = 430,                   /* DIV_ASSIGN  */
  ADD_ASSIGN = 431,                   /* ADD_ASSIGN  */
  MOD_ASSIGN = 432,                   /* MOD_ASSIGN  */
  LEFT_ASSIGN = 433,                  /* LEFT_ASSIGN  */
  RIGHT_ASSIGN = 434,                 /* RIGHT_ASSIGN  */
  AND_ASSIGN = 435,                   /* AND_ASSIGN  */
  XOR_ASSIGN = 436,                   /* XOR_ASSIGN  */
  OR_ASSIGN = 437,                    /* OR_ASSIGN  */
  SUB_ASSIGN = 438,                   /* SUB_ASSIGN  */
  LEFT_PAREN = 439,                   /* LEFT_PAREN  */
  RIGHT_PAREN = 440,                  /* RIGHT_PAREN  */
  LEFT_BRACKET = 441,                 /* LEFT_BRACKET  */
  RIGHT_BRACKET = 442,                /* RIGHT_BRACKET  */
  LEFT_BRACE = 443,                   /* LEFT_BRACE  */
  RIGHT_BRACE = 444,                  /* RIGHT_BRACE  */
  DOT = 445,                          /* DOT  */
  COMMA = 446,                        /* COMMA  */
  COLON = 447,                        /* COLON  */
  EQUAL = 448,                        /* EQUAL  */
  SEMICOLON = 449,                    /* SEMICOLON  */
  BANG = 450,                         /* BANG  */
  DASH = 451,                         /* DASH  */
  TILDE = 452,                        /* TILDE  */
  PLUS = 453,                         /* PLUS  */
  STAR = 454,                         /* STAR  */
  SLASH = 455,                        /* SLASH  */
  PERCENT = 456,                      /* PERCENT  */
  LEFT_ANGLE = 457,                   /* LEFT_ANGLE  */
  RIGHT_ANGLE = 458,                  /* RIGHT_ANGLE  */
  VERTICAL_BAR = 459,                 /* VERTICAL_BAR  */
  CARET = 460,                        /* CARET  */
  AMPERSAND = 461,                    /* AMPERSAND  */
  QUESTION = 462,                     /* QUESTION  */
  INVARIANT = 463,                    /* INVARIANT  */
  PRECISE = 464,                      /* PRECISE  */
  HIGHP = 465,                        /* HIGHP  */
  MEDIUMP = 466,                      /* MEDIUMP  */
  LOWP = 467,                         /* LOWP  */
  PRECISION = 468,                    /* PRECISION  */
  AT = 469,                           /* AT  */
  UNARY_PLUS = 470,                   /* UNARY_PLUS  */
  UNARY_DASH = 471,                   /* UNARY_DASH  */
  PRE_INC_OP = 472,                   /* PRE_INC_OP  */
  PRE_DEC_OP = 473,                   /* PRE_DEC_OP  */
  POST_DEC_OP = 474,                  /* POST_DEC_OP  */
  POST_INC_OP = 475,                  /* POST_INC_OP  */
  ARRAY_REF_OP = 476,                 /* ARRAY_REF_OP  */
  FUNCTION_CALL = 477,                /* FUNCTION_CALL  */
  TYPE_NAME_LIST = 478,               /* TYPE_NAME_LIST  */
  TYPE_SPECIFIER = 479,               /* TYPE_SPECIFIER  */
  POSTFIX_EXPRESSION = 480,           /* POSTFIX_EXPRESSION  */
  TYPE_QUALIFIER_LIST = 481,          /* TYPE_QUALIFIER_LIST  */
  STRUCT_DECLARATION = 482,           /* STRUCT_DECLARATION  */
  STRUCT_DECLARATOR = 483,            /* STRUCT_DECLARATOR  */
  STRUCT_SPECIFIER = 484,             /* STRUCT_SPECIFIER  */
  FUNCTION_DEFINITION = 485,          /* FUNCTION_DEFINITION  */
  DECLARATION = 486,                  /* DECLARATION  */
  STATEMENT_LIST = 487,               /* STATEMENT_LIST  */
  TRANSLATION_UNIT = 488,             /* TRANSLATION_UNIT  */
  PRECISION_DECLARATION = 489,        /* PRECISION_DECLARATION  */
  BLOCK_DECLARATION = 490,            /* BLOCK_DECLARATION  */
  TYPE_QUALIFIER_DECLARATION = 491,   /* TYPE_QUALIFIER_DECLARATION  */
  IDENTIFIER_LIST = 492,              /* IDENTIFIER_LIST  */
  INIT_DECLARATOR_LIST = 493,         /* INIT_DECLARATOR_LIST  */
  FULLY_SPECIFIED_TYPE = 494,         /* FULLY_SPECIFIED_TYPE  */
  SINGLE_DECLARATION = 495,           /* SINGLE_DECLARATION  */
  SINGLE_INIT_DECLARATION = 496,      /* SINGLE_INIT_DECLARATION  */
  INITIALIZER_LIST = 497,             /* INITIALIZER_LIST  */
  EXPRESSION_STATEMENT = 498,         /* EXPRESSION_STATEMENT  */
  SELECTION_STATEMENT = 499,          /* SELECTION_STATEMENT  */
  SELECTION_STATEMENT_ELSE = 500,     /* SELECTION_STATEMENT_ELSE  */
  SWITCH_STATEMENT = 501,             /* SWITCH_STATEMENT  */
  FOR_REST_STATEMENT = 502,           /* FOR_REST_STATEMENT  */
  WHILE_STATEMENT = 503,              /* WHILE_STATEMENT  */
  DO_STATEMENT = 504,                 /* DO_STATEMENT  */
  FOR_STATEMENT = 505,                /* FOR_STATEMENT  */
  CASE_LABEL = 506,                   /* CASE_LABEL  */
  CONDITION_OPT = 507,                /* CONDITION_OPT  */
  ASSIGNMENT_CONDITION = 508,         /* ASSIGNMENT_CONDITION  */
  EXPRESSION_CONDITION = 509,         /* EXPRESSION_CONDITION  */
  FUNCTION_HEADER = 510,              /* FUNCTION_HEADER  */
  FUNCTION_DECLARATION = 511,         /* FUNCTION_DECLARATION  */
  FUNCTION_PARAMETER_LIST = 512,      /* FUNCTION_PARAMETER_LIST  */
  PARAMETER_DECLARATION = 513,        /* PARAMETER_DECLARATION  */
  PARAMETER_DECLARATOR = 514,         /* PARAMETER_DECLARATOR  */
  UNINITIALIZED_DECLARATION = 515,    /* UNINITIALIZED_DECLARATION  */
  ARRAY_SPECIFIER = 516,              /* ARRAY_SPECIFIER  */
  ARRAY_SPECIFIER_LIST = 517,         /* ARRAY_SPECIFIER_LIST  */
  STRUCT_DECLARATOR_LIST = 518,       /* STRUCT_DECLARATOR_LIST  */
  FUNCTION_CALL_PARAMETER_LIST = 519, /* FUNCTION_CALL_PARAMETER_LIST  */
  STRUCT_DECLARATION_LIST = 520,      /* STRUCT_DECLARATION_LIST  */
  LAYOUT_QUALIFIER_ID = 521,          /* LAYOUT_QUALIFIER_ID  */
  LAYOUT_QUALIFIER_ID_LIST = 522,     /* LAYOUT_QUALIFIER_ID_LIST  */
  SUBROUTINE_TYPE = 523,              /* SUBROUTINE_TYPE  */
  PAREN_EXPRESSION = 524,             /* PAREN_EXPRESSION  */
  INIT_DECLARATOR = 525,              /* INIT_DECLARATOR  */
  INITIALIZER = 526,                  /* INITIALIZER  */
  TERNARY_EXPRESSION = 527,           /* TERNARY_EXPRESSION  */
  FIELD_IDENTIFIER = 528,             /* FIELD_IDENTIFIER  */
  NUM_GLSL_TOKEN = 529                /* NUM_GLSL_TOKEN  */
};
