#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "glsl_parser.h"
#include "glsl_ast.h"

void error_cb(const char *str, int lineno, int first_col, int last_col)
{
	fprintf(stderr, "GLSL parse error line %d(%d-%d): %s\n", lineno, first_col, last_col, str);
}

void parse_file(struct glsl_parse_context *context, FILE *f)
{
	bool error = glsl_parse_file(context, f);

	if (!error && context->root) {
		printf("\nAST tree:\n\n");
		glsl_ast_print(context->root, 0);

		printf("\nRegenerated GLSL:\n\n");
		char *out = glsl_ast_generate_glsl(context->root);
		printf("%s", out);
		free(out);
	}
}

int main(int argc, char **argv, char **envp)
{
	struct glsl_parse_context context;

	glsl_parse_context_init(&context);

	glsl_parse_set_error_cb(&context, error_cb);

	if (argc == 1) {
		parse_file(&context, stdin);
	}
	else {
		int i;
		for (i = 1; i < argc; i++) {
			FILE *f = fopen(argv[i], "rt");
			if (!f) {
				fprintf(stderr, "Couldn't open file %s: %s\n", argv[i], strerror(errno));
				continue;
			}
			printf("Input file: %s\n", argv[i]);
			parse_file(&context, f);
			fclose(f);
		}
	}

	glsl_parse_context_destroy(&context);
}
