%{

#include <math.h>
#include <stdbool.h>
#include <string.h>

#define YYSTYPE GLSL_STYPE
#define YYLTYPE GLSL_LTYPE

#define YY_USER_ACTION \
    yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column; \
    for(int i = 0; yytext[i] != '\0'; i++) { \
        if(yytext[i] == '\n') { \
            yylloc->last_line++; \
            yylloc->last_column = 0; \
        } \
        else { \
            yylloc->last_column++; \
        } \
    }

#include "glsl_parser.h"
#include "glsl.parser.h"

%}


%option reentrant
%option bison-bridge
%option bison-locations
%option yylineno
%option noyywrap
%option prefix="glsl_"

ws			[ \t]+
digit			[0-9]
nondigit		[_a-zA-Z]
identifier		{nondigit}({nondigit}|{digit})*
integer_constant	{digit}+
floating_constant	{digit}+\.{digit}+

%x COMMENT

%%

[\t\n ]+

#.*\n

\/\/.*\n

<INITIAL>"/*" BEGIN(COMMENT);
<COMMENT>"*/" BEGIN(INITIAL);
<COMMENT>[^*\n]+ { }

const		return CONST;
uniform		return UNIFORM;
buffer		return BUFFER;
shared		return SHARED;
coherent	return COHERENT;
volatile	return VOLATILE;
restrict	return RESTRICT;
readonly	return READONLY;
writeonly	return WRITEONLY;
atomic_uint	return ATOMIC_UINT;
layout		return LAYOUT;
centroid	return CENTROID;
flat		return FLAT;
smooth		return SMOOTH;
noperspective	return NOPERSPECTIVE;
patch		return PATCH;
sample		return SAMPLE;
break		return BREAK;
continue	return CONTINUE;
do		return DO;
for		return FOR;
while		return WHILE;
switch		return SWITCH;
case		return CASE;
default		return DEFAULT;
if		return IF;
else		return ELSE;
subroutine	return SUBROUTINE;
in		return IN;
out		return OUT;
inout		return INOUT;
float		return FLOAT;
double		return DOUBLE;
int		return INT;
void		return VOID;
bool		return BOOL;
invariant	return INVARIANT;
precise		return PRECISE;
discard		return DISCARD;
return		return RETURN;
mat2		return MAT2;
mat3		return MAT3;
mat4		return MAT4;
dmat2		return DMAT2;
dmat3		return DMAT3;
dmat4		return DMAT4;
mat2x2		return MAT2X2;
mat2x3		return MAT2X3;
mat2x4		return MAT2X4;
dmat2x2		return DMAT2X2;
dmat2x3		return DMAT2X3;
dmat2x4		return DMAT2X4;
mat3x2		return MAT3X2;
mat3x3		return MAT3X3;
mat3x4		return MAT3X4;
dmat3x2		return DMAT3X2;
dmat3x3		return DMAT3X3;
dmat3x4		return DMAT3X4;
mat4x2		return MAT4X2;
mat4x3		return MAT4X3;
mat4x4		return MAT4X4;
dmat4x2		return DMAT4X2;
dmat4x3		return DMAT4X3;
dmat4x4		return DMAT4X4;
vec2		return VEC2;
vec3		return VEC3;
vec4		return VEC4;
ivec2		return IVEC2;
ivec3		return IVEC3;
ivec4		return IVEC4;
bvec2		return BVEC2;
bvec3		return BVEC3;
bvec4		return BVEC4;
dvec2		return DVEC2;
dvec3		return DVEC3;
dvec4		return DVEC4;
uint		return UINT;
uvec2		return UVEC2;
uvec3		return UVEC3;
uvec4		return UVEC4;
lowp			return LOWP;
mediump			return MEDIUMP;
highp			return HIGHP;
precision		return PRECISION;
sampler1D		return SAMPLER1D;
sampler2D		return SAMPLER2D;
sampler3D		return SAMPLER3D;
samplerCube		return SAMPLERCUBE;
sampler1DShadow		return SAMPLER1DSHADOW;
sampler2DShadow		return SAMPLER2DSHADOW;
samplerCubeShadow	return SAMPLERCUBESHADOW;
sampler1DArray		return SAMPLER1DARRAY;
sampler2DArray		return SAMPLER2DARRAY;
sampler1DArrayShadow	return SAMPLER1DARRAYSHADOW;
sampler2DArrayShadow	return SAMPLER2DARRAYSHADOW;
isampler1D		return ISAMPLER1D;
isampler2D		return ISAMPLER2D;
isampler3D		return ISAMPLER3D;
isamplerCube		return ISAMPLERCUBE;
isampler1DArray		return ISAMPLER1DARRAY;
isampler2DArray		return ISAMPLER2DARRAY;
usampler1D		return USAMPLER1D;
usampler2D		return USAMPLER2D;
usampler3D		return USAMPLER3D;
usamplerCube		return USAMPLERCUBE;
usampler1DArray		return USAMPLER1DARRAY;
usampler2DArray		return USAMPLER2DARRAY;
sampler2DRect		return SAMPLER2DRECT;
sampler2DRectShadow	return SAMPLER2DRECTSHADOW;
isampler2DRect		return ISAMPLER2DRECT;
usampler2DRect		return USAMPLER2DRECT;
samplerBuffer		return SAMPLERBUFFER;
isamplerBuffer		return ISAMPLERBUFFER;
usamplerBuffer		return USAMPLERBUFFER;
sampler2DMS		return SAMPLER2DMS;
isampler2DMS		return ISAMPLER2DMS;
usampler2DMS		return USAMPLER2DMS;
sampler2DMSArray	return SAMPLER2DMSARRAY;
isampler2DMSArray	return ISAMPLER2DMSARRAY;
usampler2DMSArray	return USAMPLER2DMSARRAY;
samplerCubeArray	return SAMPLERCUBEARRAY;
samplerCubeArrayShadow	return SAMPLERCUBEARRAYSHADOW;
isamplerCubeArray	return ISAMPLERCUBEARRAY;
usamplerCubeArray	return USAMPLERCUBEARRAY;
image1D			return IMAGE1D;
iimage1D		return IIMAGE1D;
uimage1D		return UIMAGE1D;
image2D			return IMAGE2D;
iimage2D		return IIMAGE2D;
uimage2D		return UIMAGE2D;
image3D			return IMAGE3D;
iimage3D		return IIMAGE3D;
uimage3D		return UIMAGE3D;
image2DRect		return IMAGE2DRECT;
iimage2DRect		return IIMAGE2DRECT;
uimage2DRect		return UIMAGE2DRECT;
imageCube		return IMAGECUBE;
iimageCube		return IIMAGECUBE;
uimageCube		return UIMAGECUBE;
imageBuffer		return IMAGEBUFFER;
iimageBuffer		return IIMAGEBUFFER;
uimageBuffer		return UIMAGEBUFFER;
image1DArray		return IMAGE1DARRAY;
iimage1DArray		return IIMAGE1DARRAY;
uimage1DArray		return UIMAGE1DARRAY;
image2DArray		return IMAGE2DARRAY;
iimage2DArray		return IIMAGE2DARRAY;
uimage2DArray		return UIMAGE2DARRAY;
imageCubeArray		return IMAGECUBEARRAY;
iimageCubeArray		return IIMAGECUBEARRAY;
uimageCubeArray		return UIMAGECUBEARRAY;
image2DMS		return IMAGE2DMS;
iimage2DMS		return IIMAGE2DMS;
uimage2DMS		return UIMAGE2DMS;
image2DMSArray		return IMAGE2DMSARRAY;
iimage2DMSArray		return IIMAGE2DMSARRAY;
uimage2DMSArray		return UIMAGE2DMSARRAY;
struct			return STRUCT;

\<\< return LEFT_OP;
\>\> return RIGHT_OP;
\+\+ return INC_OP;
\-\- return DEC_OP;
\<\= return LE_OP;
\>\= return GE_OP;
\=\= return EQ_OP;
\!\= return NE_OP;
\&\& return AND_OP;
\|\| return OR_OP;
\^\^ return XOR_OP;
\*\= return MUL_ASSIGN;
\/\= return DIV_ASSIGN;
\+\= return ADD_ASSIGN;
\%\= return MOD_ASSIGN;
\<\<\= return LEFT_ASSIGN;
\>\>\= return RIGHT_ASSIGN;
\&\= return AND_ASSIGN;
\^\= return XOR_ASSIGN;
\|\= return OR_ASSIGN;
\-\= return SUB_ASSIGN;

\+ return PLUS;
\- return DASH;
\% return PERCENT;
\* return STAR;
\/ return SLASH;
\~ return TILDE;
\! return BANG;
\^ return CARET;
\( return LEFT_PAREN;
\) return RIGHT_PAREN;
\{ return LEFT_BRACE;
\} return RIGHT_BRACE;
\; return SEMICOLON;
\< return LEFT_ANGLE;
\> return RIGHT_ANGLE;
\. return DOT;
\, return COMMA;
\[ return LEFT_BRACKET;
\] return RIGHT_BRACKET;
\| return VERTICAL_BAR;
\: return COLON;
\= return EQUAL;
\& return AMPERSAND;
\? return QUESTION;

true			{ return TRUE_VALUE; }
false			{ return FALSE_VALUE; }

{identifier}		{ (*yylval).IDENTIFIER = strdup(yytext); return IDENTIFIER; }
{integer_constant}	{ (*yylval).INTCONSTANT = atoi(yytext); return INTCONSTANT; }
{floating_constant}	{ (*yylval).FLOATCONSTANT = atof(yytext); return FLOATCONSTANT; }

%%
