/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_GLSL_GLSL_PARSER_H_INCLUDED
# define YY_GLSL_GLSL_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef GLSL_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define GLSL_DEBUG 1
#  else
#   define GLSL_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define GLSL_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined GLSL_DEBUG */
#if GLSL_DEBUG
extern int glsl_debug;
#endif

/* Token kinds.  */
#ifndef GLSL_TOKENTYPE
# define GLSL_TOKENTYPE
  enum glsl_tokentype
  {
    GLSL_EMPTY = -2,
    GLSL_EOF = 0,                  /* "end of file"  */
    GLSL_error = 256,              /* error  */
    GLSL_UNDEF = 257,              /* "invalid token"  */
    CONST = 258,                   /* CONST  */
    BOOL = 259,                    /* BOOL  */
    FLOAT = 260,                   /* FLOAT  */
    DOUBLE = 261,                  /* DOUBLE  */
    INT = 262,                     /* INT  */
    UINT = 263,                    /* UINT  */
    BREAK = 264,                   /* BREAK  */
    CONTINUE = 265,                /* CONTINUE  */
    DO = 266,                      /* DO  */
    ELSE = 267,                    /* ELSE  */
    FOR = 268,                     /* FOR  */
    IF = 269,                      /* IF  */
    DISCARD = 270,                 /* DISCARD  */
    RETURN = 271,                  /* RETURN  */
    RETURN_VALUE = 272,            /* RETURN_VALUE  */
    SWITCH = 273,                  /* SWITCH  */
    CASE = 274,                    /* CASE  */
    DEFAULT = 275,                 /* DEFAULT  */
    SUBROUTINE = 276,              /* SUBROUTINE  */
    BVEC2 = 277,                   /* BVEC2  */
    BVEC3 = 278,                   /* BVEC3  */
    BVEC4 = 279,                   /* BVEC4  */
    IVEC2 = 280,                   /* IVEC2  */
    IVEC3 = 281,                   /* IVEC3  */
    IVEC4 = 282,                   /* IVEC4  */
    UVEC2 = 283,                   /* UVEC2  */
    UVEC3 = 284,                   /* UVEC3  */
    UVEC4 = 285,                   /* UVEC4  */
    VEC2 = 286,                    /* VEC2  */
    VEC3 = 287,                    /* VEC3  */
    VEC4 = 288,                    /* VEC4  */
    MAT2 = 289,                    /* MAT2  */
    MAT3 = 290,                    /* MAT3  */
    MAT4 = 291,                    /* MAT4  */
    CENTROID = 292,                /* CENTROID  */
    IN = 293,                      /* IN  */
    OUT = 294,                     /* OUT  */
    INOUT = 295,                   /* INOUT  */
    UNIFORM = 296,                 /* UNIFORM  */
    PATCH = 297,                   /* PATCH  */
    SAMPLE = 298,                  /* SAMPLE  */
    BUFFER = 299,                  /* BUFFER  */
    SHARED = 300,                  /* SHARED  */
    COHERENT = 301,                /* COHERENT  */
    VOLATILE = 302,                /* VOLATILE  */
    RESTRICT = 303,                /* RESTRICT  */
    READONLY = 304,                /* READONLY  */
    WRITEONLY = 305,               /* WRITEONLY  */
    DVEC2 = 306,                   /* DVEC2  */
    DVEC3 = 307,                   /* DVEC3  */
    DVEC4 = 308,                   /* DVEC4  */
    DMAT2 = 309,                   /* DMAT2  */
    DMAT3 = 310,                   /* DMAT3  */
    DMAT4 = 311,                   /* DMAT4  */
    NOPERSPECTIVE = 312,           /* NOPERSPECTIVE  */
    FLAT = 313,                    /* FLAT  */
    SMOOTH = 314,                  /* SMOOTH  */
    LAYOUT = 315,                  /* LAYOUT  */
    MAT2X2 = 316,                  /* MAT2X2  */
    MAT2X3 = 317,                  /* MAT2X3  */
    MAT2X4 = 318,                  /* MAT2X4  */
    MAT3X2 = 319,                  /* MAT3X2  */
    MAT3X3 = 320,                  /* MAT3X3  */
    MAT3X4 = 321,                  /* MAT3X4  */
    MAT4X2 = 322,                  /* MAT4X2  */
    MAT4X3 = 323,                  /* MAT4X3  */
    MAT4X4 = 324,                  /* MAT4X4  */
    DMAT2X2 = 325,                 /* DMAT2X2  */
    DMAT2X3 = 326,                 /* DMAT2X3  */
    DMAT2X4 = 327,                 /* DMAT2X4  */
    DMAT3X2 = 328,                 /* DMAT3X2  */
    DMAT3X3 = 329,                 /* DMAT3X3  */
    DMAT3X4 = 330,                 /* DMAT3X4  */
    DMAT4X2 = 331,                 /* DMAT4X2  */
    DMAT4X3 = 332,                 /* DMAT4X3  */
    DMAT4X4 = 333,                 /* DMAT4X4  */
    ATOMIC_UINT = 334,             /* ATOMIC_UINT  */
    SAMPLER1D = 335,               /* SAMPLER1D  */
    SAMPLER2D = 336,               /* SAMPLER2D  */
    SAMPLER3D = 337,               /* SAMPLER3D  */
    SAMPLERCUBE = 338,             /* SAMPLERCUBE  */
    SAMPLER1DSHADOW = 339,         /* SAMPLER1DSHADOW  */
    SAMPLER2DSHADOW = 340,         /* SAMPLER2DSHADOW  */
    SAMPLERCUBESHADOW = 341,       /* SAMPLERCUBESHADOW  */
    SAMPLER1DARRAY = 342,          /* SAMPLER1DARRAY  */
    SAMPLER2DARRAY = 343,          /* SAMPLER2DARRAY  */
    SAMPLER1DARRAYSHADOW = 344,    /* SAMPLER1DARRAYSHADOW  */
    SAMPLER2DARRAYSHADOW = 345,    /* SAMPLER2DARRAYSHADOW  */
    ISAMPLER1D = 346,              /* ISAMPLER1D  */
    ISAMPLER2D = 347,              /* ISAMPLER2D  */
    ISAMPLER3D = 348,              /* ISAMPLER3D  */
    ISAMPLERCUBE = 349,            /* ISAMPLERCUBE  */
    ISAMPLER1DARRAY = 350,         /* ISAMPLER1DARRAY  */
    ISAMPLER2DARRAY = 351,         /* ISAMPLER2DARRAY  */
    USAMPLER1D = 352,              /* USAMPLER1D  */
    USAMPLER2D = 353,              /* USAMPLER2D  */
    USAMPLER3D = 354,              /* USAMPLER3D  */
    USAMPLERCUBE = 355,            /* USAMPLERCUBE  */
    USAMPLER1DARRAY = 356,         /* USAMPLER1DARRAY  */
    USAMPLER2DARRAY = 357,         /* USAMPLER2DARRAY  */
    SAMPLER2DRECT = 358,           /* SAMPLER2DRECT  */
    SAMPLER2DRECTSHADOW = 359,     /* SAMPLER2DRECTSHADOW  */
    ISAMPLER2DRECT = 360,          /* ISAMPLER2DRECT  */
    USAMPLER2DRECT = 361,          /* USAMPLER2DRECT  */
    SAMPLERBUFFER = 362,           /* SAMPLERBUFFER  */
    ISAMPLERBUFFER = 363,          /* ISAMPLERBUFFER  */
    USAMPLERBUFFER = 364,          /* USAMPLERBUFFER  */
    SAMPLERCUBEARRAY = 365,        /* SAMPLERCUBEARRAY  */
    SAMPLERCUBEARRAYSHADOW = 366,  /* SAMPLERCUBEARRAYSHADOW  */
    ISAMPLERCUBEARRAY = 367,       /* ISAMPLERCUBEARRAY  */
    USAMPLERCUBEARRAY = 368,       /* USAMPLERCUBEARRAY  */
    SAMPLER2DMS = 369,             /* SAMPLER2DMS  */
    ISAMPLER2DMS = 370,            /* ISAMPLER2DMS  */
    USAMPLER2DMS = 371,            /* USAMPLER2DMS  */
    SAMPLER2DMSARRAY = 372,        /* SAMPLER2DMSARRAY  */
    ISAMPLER2DMSARRAY = 373,       /* ISAMPLER2DMSARRAY  */
    USAMPLER2DMSARRAY = 374,       /* USAMPLER2DMSARRAY  */
    IMAGE1D = 375,                 /* IMAGE1D  */
    IIMAGE1D = 376,                /* IIMAGE1D  */
    UIMAGE1D = 377,                /* UIMAGE1D  */
    IMAGE2D = 378,                 /* IMAGE2D  */
    IIMAGE2D = 379,                /* IIMAGE2D  */
    UIMAGE2D = 380,                /* UIMAGE2D  */
    IMAGE3D = 381,                 /* IMAGE3D  */
    IIMAGE3D = 382,                /* IIMAGE3D  */
    UIMAGE3D = 383,                /* UIMAGE3D  */
    IMAGE2DRECT = 384,             /* IMAGE2DRECT  */
    IIMAGE2DRECT = 385,            /* IIMAGE2DRECT  */
    UIMAGE2DRECT = 386,            /* UIMAGE2DRECT  */
    IMAGECUBE = 387,               /* IMAGECUBE  */
    IIMAGECUBE = 388,              /* IIMAGECUBE  */
    UIMAGECUBE = 389,              /* UIMAGECUBE  */
    IMAGEBUFFER = 390,             /* IMAGEBUFFER  */
    IIMAGEBUFFER = 391,            /* IIMAGEBUFFER  */
    UIMAGEBUFFER = 392,            /* UIMAGEBUFFER  */
    IMAGE1DARRAY = 393,            /* IMAGE1DARRAY  */
    IIMAGE1DARRAY = 394,           /* IIMAGE1DARRAY  */
    UIMAGE1DARRAY = 395,           /* UIMAGE1DARRAY  */
    IMAGE2DARRAY = 396,            /* IMAGE2DARRAY  */
    IIMAGE2DARRAY = 397,           /* IIMAGE2DARRAY  */
    UIMAGE2DARRAY = 398,           /* UIMAGE2DARRAY  */
    IMAGECUBEARRAY = 399,          /* IMAGECUBEARRAY  */
    IIMAGECUBEARRAY = 400,         /* IIMAGECUBEARRAY  */
    UIMAGECUBEARRAY = 401,         /* UIMAGECUBEARRAY  */
    IMAGE2DMS = 402,               /* IMAGE2DMS  */
    IIMAGE2DMS = 403,              /* IIMAGE2DMS  */
    UIMAGE2DMS = 404,              /* UIMAGE2DMS  */
    IMAGE2DMSARRAY = 405,          /* IMAGE2DMSARRAY  */
    IIMAGE2DMSARRAY = 406,         /* IIMAGE2DMSARRAY  */
    UIMAGE2DMSARRAY = 407,         /* UIMAGE2DMSARRAY  */
    STRUCT = 408,                  /* STRUCT  */
    VOID = 409,                    /* VOID  */
    WHILE = 410,                   /* WHILE  */
    IDENTIFIER = 411,              /* IDENTIFIER  */
    FLOATCONSTANT = 412,           /* FLOATCONSTANT  */
    DOUBLECONSTANT = 413,          /* DOUBLECONSTANT  */
    INTCONSTANT = 414,             /* INTCONSTANT  */
    UINTCONSTANT = 415,            /* UINTCONSTANT  */
    TRUE_VALUE = 416,              /* TRUE_VALUE  */
    FALSE_VALUE = 417,             /* FALSE_VALUE  */
    LEFT_OP = 418,                 /* LEFT_OP  */
    RIGHT_OP = 419,                /* RIGHT_OP  */
    INC_OP = 420,                  /* INC_OP  */
    DEC_OP = 421,                  /* DEC_OP  */
    LE_OP = 422,                   /* LE_OP  */
    GE_OP = 423,                   /* GE_OP  */
    EQ_OP = 424,                   /* EQ_OP  */
    NE_OP = 425,                   /* NE_OP  */
    AND_OP = 426,                  /* AND_OP  */
    OR_OP = 427,                   /* OR_OP  */
    XOR_OP = 428,                  /* XOR_OP  */
    MUL_ASSIGN = 429,              /* MUL_ASSIGN  */
    DIV_ASSIGN = 430,              /* DIV_ASSIGN  */
    ADD_ASSIGN = 431,              /* ADD_ASSIGN  */
    MOD_ASSIGN = 432,              /* MOD_ASSIGN  */
    LEFT_ASSIGN = 433,             /* LEFT_ASSIGN  */
    RIGHT_ASSIGN = 434,            /* RIGHT_ASSIGN  */
    AND_ASSIGN = 435,              /* AND_ASSIGN  */
    XOR_ASSIGN = 436,              /* XOR_ASSIGN  */
    OR_ASSIGN = 437,               /* OR_ASSIGN  */
    SUB_ASSIGN = 438,              /* SUB_ASSIGN  */
    LEFT_PAREN = 439,              /* LEFT_PAREN  */
    RIGHT_PAREN = 440,             /* RIGHT_PAREN  */
    LEFT_BRACKET = 441,            /* LEFT_BRACKET  */
    RIGHT_BRACKET = 442,           /* RIGHT_BRACKET  */
    LEFT_BRACE = 443,              /* LEFT_BRACE  */
    RIGHT_BRACE = 444,             /* RIGHT_BRACE  */
    DOT = 445,                     /* DOT  */
    COMMA = 446,                   /* COMMA  */
    COLON = 447,                   /* COLON  */
    EQUAL = 448,                   /* EQUAL  */
    SEMICOLON = 449,               /* SEMICOLON  */
    BANG = 450,                    /* BANG  */
    DASH = 451,                    /* DASH  */
    TILDE = 452,                   /* TILDE  */
    PLUS = 453,                    /* PLUS  */
    STAR = 454,                    /* STAR  */
    SLASH = 455,                   /* SLASH  */
    PERCENT = 456,                 /* PERCENT  */
    LEFT_ANGLE = 457,              /* LEFT_ANGLE  */
    RIGHT_ANGLE = 458,             /* RIGHT_ANGLE  */
    VERTICAL_BAR = 459,            /* VERTICAL_BAR  */
    CARET = 460,                   /* CARET  */
    AMPERSAND = 461,               /* AMPERSAND  */
    QUESTION = 462,                /* QUESTION  */
    INVARIANT = 463,               /* INVARIANT  */
    PRECISE = 464,                 /* PRECISE  */
    HIGHP = 465,                   /* HIGHP  */
    MEDIUMP = 466,                 /* MEDIUMP  */
    LOWP = 467,                    /* LOWP  */
    PRECISION = 468,               /* PRECISION  */
    AT = 469,                      /* AT  */
    UNARY_PLUS = 470,              /* UNARY_PLUS  */
    UNARY_DASH = 471,              /* UNARY_DASH  */
    PRE_INC_OP = 472,              /* PRE_INC_OP  */
    PRE_DEC_OP = 473,              /* PRE_DEC_OP  */
    POST_DEC_OP = 474,             /* POST_DEC_OP  */
    POST_INC_OP = 475,             /* POST_INC_OP  */
    ARRAY_REF_OP = 476,            /* ARRAY_REF_OP  */
    FUNCTION_CALL = 477,           /* FUNCTION_CALL  */
    TYPE_NAME_LIST = 478,          /* TYPE_NAME_LIST  */
    TYPE_SPECIFIER = 479,          /* TYPE_SPECIFIER  */
    POSTFIX_EXPRESSION = 480,      /* POSTFIX_EXPRESSION  */
    TYPE_QUALIFIER_LIST = 481,     /* TYPE_QUALIFIER_LIST  */
    STRUCT_DECLARATION = 482,      /* STRUCT_DECLARATION  */
    STRUCT_DECLARATOR = 483,       /* STRUCT_DECLARATOR  */
    STRUCT_SPECIFIER = 484,        /* STRUCT_SPECIFIER  */
    FUNCTION_DEFINITION = 485,     /* FUNCTION_DEFINITION  */
    DECLARATION = 486,             /* DECLARATION  */
    STATEMENT_LIST = 487,          /* STATEMENT_LIST  */
    TRANSLATION_UNIT = 488,        /* TRANSLATION_UNIT  */
    PRECISION_DECLARATION = 489,   /* PRECISION_DECLARATION  */
    BLOCK_DECLARATION = 490,       /* BLOCK_DECLARATION  */
    TYPE_QUALIFIER_DECLARATION = 491, /* TYPE_QUALIFIER_DECLARATION  */
    IDENTIFIER_LIST = 492,         /* IDENTIFIER_LIST  */
    INIT_DECLARATOR_LIST = 493,    /* INIT_DECLARATOR_LIST  */
    FULLY_SPECIFIED_TYPE = 494,    /* FULLY_SPECIFIED_TYPE  */
    SINGLE_DECLARATION = 495,      /* SINGLE_DECLARATION  */
    SINGLE_INIT_DECLARATION = 496, /* SINGLE_INIT_DECLARATION  */
    INITIALIZER_LIST = 497,        /* INITIALIZER_LIST  */
    EXPRESSION_STATEMENT = 498,    /* EXPRESSION_STATEMENT  */
    SELECTION_STATEMENT = 499,     /* SELECTION_STATEMENT  */
    SELECTION_STATEMENT_ELSE = 500, /* SELECTION_STATEMENT_ELSE  */
    SWITCH_STATEMENT = 501,        /* SWITCH_STATEMENT  */
    FOR_REST_STATEMENT = 502,      /* FOR_REST_STATEMENT  */
    WHILE_STATEMENT = 503,         /* WHILE_STATEMENT  */
    DO_STATEMENT = 504,            /* DO_STATEMENT  */
    FOR_STATEMENT = 505,           /* FOR_STATEMENT  */
    CASE_LABEL = 506,              /* CASE_LABEL  */
    CONDITION_OPT = 507,           /* CONDITION_OPT  */
    ASSIGNMENT_CONDITION = 508,    /* ASSIGNMENT_CONDITION  */
    EXPRESSION_CONDITION = 509,    /* EXPRESSION_CONDITION  */
    FUNCTION_HEADER = 510,         /* FUNCTION_HEADER  */
    FUNCTION_DECLARATION = 511,    /* FUNCTION_DECLARATION  */
    FUNCTION_PARAMETER_LIST = 512, /* FUNCTION_PARAMETER_LIST  */
    PARAMETER_DECLARATION = 513,   /* PARAMETER_DECLARATION  */
    PARAMETER_DECLARATOR = 514,    /* PARAMETER_DECLARATOR  */
    UNINITIALIZED_DECLARATION = 515, /* UNINITIALIZED_DECLARATION  */
    ARRAY_SPECIFIER = 516,         /* ARRAY_SPECIFIER  */
    ARRAY_SPECIFIER_LIST = 517,    /* ARRAY_SPECIFIER_LIST  */
    STRUCT_DECLARATOR_LIST = 518,  /* STRUCT_DECLARATOR_LIST  */
    FUNCTION_CALL_PARAMETER_LIST = 519, /* FUNCTION_CALL_PARAMETER_LIST  */
    STRUCT_DECLARATION_LIST = 520, /* STRUCT_DECLARATION_LIST  */
    LAYOUT_QUALIFIER_ID = 521,     /* LAYOUT_QUALIFIER_ID  */
    LAYOUT_QUALIFIER_ID_LIST = 522, /* LAYOUT_QUALIFIER_ID_LIST  */
    SUBROUTINE_TYPE = 523,         /* SUBROUTINE_TYPE  */
    PAREN_EXPRESSION = 524,        /* PAREN_EXPRESSION  */
    INIT_DECLARATOR = 525,         /* INIT_DECLARATOR  */
    INITIALIZER = 526,             /* INITIALIZER  */
    TERNARY_EXPRESSION = 527,      /* TERNARY_EXPRESSION  */
    FIELD_IDENTIFIER = 528,        /* FIELD_IDENTIFIER  */
    NUM_GLSL_TOKEN = 529           /* NUM_GLSL_TOKEN  */
  };
  typedef enum glsl_tokentype glsl_token_kind_t;
#endif

/* Value type.  */
#if ! defined GLSL_STYPE && ! defined GLSL_STYPE_IS_DECLARED
union GLSL_STYPE
{
  char * IDENTIFIER;                       /* IDENTIFIER  */
  double DOUBLECONSTANT;                   /* DOUBLECONSTANT  */
  float FLOATCONSTANT;                     /* FLOATCONSTANT  */
  int INTCONSTANT;                         /* INTCONSTANT  */
  int assignment_operator;                 /* assignment_operator  */
  int unary_operator;                      /* unary_operator  */
  struct glsl_node * translation_unit;     /* translation_unit  */
  struct glsl_node * block_identifier;     /* block_identifier  */
  struct glsl_node * decl_identifier;      /* decl_identifier  */
  struct glsl_node * struct_name;          /* struct_name  */
  struct glsl_node * type_name;            /* type_name  */
  struct glsl_node * param_name;           /* param_name  */
  struct glsl_node * function_name;        /* function_name  */
  struct glsl_node * field_identifier;     /* field_identifier  */
  struct glsl_node * variable_identifier;  /* variable_identifier  */
  struct glsl_node * layout_identifier;    /* layout_identifier  */
  struct glsl_node * type_specifier_identifier; /* type_specifier_identifier  */
  struct glsl_node * external_declaration; /* external_declaration  */
  struct glsl_node * function_definition;  /* function_definition  */
  struct glsl_node * compound_statement_no_new_scope; /* compound_statement_no_new_scope  */
  struct glsl_node * statement;            /* statement  */
  struct glsl_node * statement_list;       /* statement_list  */
  struct glsl_node * compound_statement;   /* compound_statement  */
  struct glsl_node * simple_statement;     /* simple_statement  */
  struct glsl_node * declaration;          /* declaration  */
  struct glsl_node * identifier_list;      /* identifier_list  */
  struct glsl_node * init_declarator_list; /* init_declarator_list  */
  struct glsl_node * single_declaration;   /* single_declaration  */
  struct glsl_node * initializer;          /* initializer  */
  struct glsl_node * initializer_list;     /* initializer_list  */
  struct glsl_node * expression_statement; /* expression_statement  */
  struct glsl_node * selection_statement;  /* selection_statement  */
  struct glsl_node * switch_statement;     /* switch_statement  */
  struct glsl_node * switch_statement_list; /* switch_statement_list  */
  struct glsl_node * case_label;           /* case_label  */
  struct glsl_node * iteration_statement;  /* iteration_statement  */
  struct glsl_node * statement_no_new_scope; /* statement_no_new_scope  */
  struct glsl_node * for_init_statement;   /* for_init_statement  */
  struct glsl_node * conditionopt;         /* conditionopt  */
  struct glsl_node * condition;            /* condition  */
  struct glsl_node * for_rest_statement;   /* for_rest_statement  */
  struct glsl_node * jump_statement;       /* jump_statement  */
  struct glsl_node * function_prototype;   /* function_prototype  */
  struct glsl_node * function_declarator;  /* function_declarator  */
  struct glsl_node * function_parameter_list; /* function_parameter_list  */
  struct glsl_node * parameter_declaration; /* parameter_declaration  */
  struct glsl_node * parameter_declarator; /* parameter_declarator  */
  struct glsl_node * function_header;      /* function_header  */
  struct glsl_node * fully_specified_type; /* fully_specified_type  */
  struct glsl_node * parameter_type_specifier; /* parameter_type_specifier  */
  struct glsl_node * type_specifier;       /* type_specifier  */
  struct glsl_node * array_specifier_list; /* array_specifier_list  */
  struct glsl_node * array_specifier;      /* array_specifier  */
  struct glsl_node * type_specifier_nonarray; /* type_specifier_nonarray  */
  struct glsl_node * struct_specifier;     /* struct_specifier  */
  struct glsl_node * struct_declaration_list; /* struct_declaration_list  */
  struct glsl_node * struct_declaration;   /* struct_declaration  */
  struct glsl_node * struct_declarator_list; /* struct_declarator_list  */
  struct glsl_node * struct_declarator;    /* struct_declarator  */
  struct glsl_node * type_qualifier;       /* type_qualifier  */
  struct glsl_node * single_type_qualifier; /* single_type_qualifier  */
  struct glsl_node * layout_qualifier;     /* layout_qualifier  */
  struct glsl_node * layout_qualifier_id_list; /* layout_qualifier_id_list  */
  struct glsl_node * layout_qualifier_id;  /* layout_qualifier_id  */
  struct glsl_node * precision_qualifier;  /* precision_qualifier  */
  struct glsl_node * interpolation_qualifier; /* interpolation_qualifier  */
  struct glsl_node * invariant_qualifier;  /* invariant_qualifier  */
  struct glsl_node * precise_qualifier;    /* precise_qualifier  */
  struct glsl_node * storage_qualifier;    /* storage_qualifier  */
  struct glsl_node * type_name_list;       /* type_name_list  */
  struct glsl_node * expression;           /* expression  */
  struct glsl_node * assignment_expression; /* assignment_expression  */
  struct glsl_node * constant_expression;  /* constant_expression  */
  struct glsl_node * conditional_expression; /* conditional_expression  */
  struct glsl_node * logical_or_expression; /* logical_or_expression  */
  struct glsl_node * logical_xor_expression; /* logical_xor_expression  */
  struct glsl_node * logical_and_expression; /* logical_and_expression  */
  struct glsl_node * inclusive_or_expression; /* inclusive_or_expression  */
  struct glsl_node * exclusive_or_expression; /* exclusive_or_expression  */
  struct glsl_node * and_expression;       /* and_expression  */
  struct glsl_node * equality_expression;  /* equality_expression  */
  struct glsl_node * relational_expression; /* relational_expression  */
  struct glsl_node * shift_expression;     /* shift_expression  */
  struct glsl_node * additive_expression;  /* additive_expression  */
  struct glsl_node * multiplicative_expression; /* multiplicative_expression  */
  struct glsl_node * unary_expression;     /* unary_expression  */
  struct glsl_node * postfix_expression;   /* postfix_expression  */
  struct glsl_node * integer_expression;   /* integer_expression  */
  struct glsl_node * function_call;        /* function_call  */
  struct glsl_node * function_call_or_method; /* function_call_or_method  */
  struct glsl_node * function_call_generic; /* function_call_generic  */
  struct glsl_node * function_call_parameter_list; /* function_call_parameter_list  */
  struct glsl_node * function_identifier;  /* function_identifier  */
  struct glsl_node * primary_expression;   /* primary_expression  */
  unsigned int UINTCONSTANT;               /* UINTCONSTANT  */

#line 442 "glsl.parser.h"

};
typedef union GLSL_STYPE GLSL_STYPE;
# define GLSL_STYPE_IS_TRIVIAL 1
# define GLSL_STYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined GLSL_LTYPE && ! defined GLSL_LTYPE_IS_DECLARED
typedef struct GLSL_LTYPE GLSL_LTYPE;
struct GLSL_LTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define GLSL_LTYPE_IS_DECLARED 1
# define GLSL_LTYPE_IS_TRIVIAL 1
#endif




int glsl_parse (struct glsl_parse_context * context);


#endif /* !YY_GLSL_GLSL_PARSER_H_INCLUDED  */
