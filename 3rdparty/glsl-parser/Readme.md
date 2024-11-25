glsl-parser
=========

This is a GLSL parser implemented with `flex` and `bison`. The grammar is based on the OpenGL 4.5 reference specs. The parser generates an AST represented in C data structures. The AST structure is documented in AstNodes.md.

The header file `glsl_parser.h` documents the interface to the parser and the header file `glsl_ast.h` defines some useful functions for working with the AST it generates.

The included Makefile builds a test program that reads a shader from a file or standard input and prints out a human readable version of the shader's AST and attempts to regenerate the input shader from the AST.
