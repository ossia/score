#ifndef GLSL_AST_H
#define GLSL_AST_H

#include "glsl_parser.h"

//
// glsl_is_list_node()
//
// Returns true if the children of this node form
// a list
//
bool glsl_ast_is_list_node(struct glsl_node *n);

//
// glsl_ast_print()
//
// Print the AST tree as text for debugging purposes.
// The 'depth' parameter represents amount to indent the
// the printed text.
//
void glsl_ast_print(struct glsl_node *n, int depth);

//
// glsl_ast_generate_glsl()
//
// Translate AST into GLSL
//
// Returns a string containing the GLSL corresponding to
// the AST or NULL on error. The returned string must be
// deallocataed with free()
//
char *glsl_ast_generate_glsl(struct glsl_node *n);

#endif
