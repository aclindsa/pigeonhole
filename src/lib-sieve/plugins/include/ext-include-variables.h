#ifndef __EXT_INCLUDE_VARIABLES_H
#define __EXT_INCLUDE_VARIABLES_H

#include "sieve-common.h"
#include "sieve-ext-variables.h"

#include "ext-include-common.h"

/* 
 * AST Context
 */
 
struct ext_include_ast_context *ext_include_create_ast_context
	(struct sieve_ast *ast, struct sieve_ast *parent);

/* 
 * Variable import-export
 */
 
bool ext_include_variable_import_global
	(struct sieve_validator *valdtr, struct sieve_command_context *cmd, 
		const char *variable, bool export);
		
#endif /* __EXT_INCLUDE_VARIABLES_H */
