/* Copyright (c) 2002-2008 Dovecot Sieve authors, see the included COPYING file
 */

#include "lib.h"
#include "str-sanitize.h"
#include "array.h"
#include "mail-storage.h"

#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-result.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-actions.h"

#include "ext-imapflags-common.h"

#include <ctype.h>

/* 
 * Flags tagged argument
 */

static bool tag_flags_validate
	(struct sieve_validator *validator,	struct sieve_ast_argument **arg, 
		struct sieve_command_context *cmd);
static bool tag_flags_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg,
		struct sieve_command_context *cmd);

const struct sieve_argument tag_flags = { 
	"flags", 
	NULL, NULL,
	tag_flags_validate, 
	NULL, 
	tag_flags_generate 
};

/* 
 * Side effect 
 */

static bool seff_flags_dump_context
	(const struct sieve_side_effect *seffect,
    	const struct sieve_dumptime_env *denv, sieve_size_t *address);
static bool seff_flags_read_context
	(const struct sieve_side_effect *seffect, 
		const struct sieve_runtime_env *renv, sieve_size_t *address,
		void **se_context);
static void seff_flags_print
	(const struct sieve_side_effect *seffect, const struct sieve_action *action,
		const struct sieve_result_print_env *rpenv, void *se_context, bool *keep);
static bool seff_flags_pre_execute
	(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
		const struct sieve_action_exec_env *aenv, 
		void **se_context, void *tr_context);

const struct sieve_side_effect flags_side_effect = {
	SIEVE_OBJECT("flags", &flags_side_effect_operand, 0),
	&act_store,

	seff_flags_dump_context,
	seff_flags_read_context,
	seff_flags_print,
	seff_flags_pre_execute, 
	NULL, NULL, NULL
};

/*
 * Operand
 */

static const struct sieve_extension_obj_registry ext_side_effects =
	SIEVE_EXT_DEFINE_SIDE_EFFECT(flags_side_effect);

const struct sieve_operand flags_side_effect_operand = { 
	"flags operand", 
	&imapflags_extension,
	0, 
	&sieve_side_effect_operand_class,
	&ext_side_effects
};

/* 
 * Tag validation 
 */

static bool tag_flags_validate
(struct sieve_validator *validator,	struct sieve_ast_argument **arg, 
	struct sieve_command_context *cmd)
{
	struct sieve_ast_argument *tag = *arg;

	/* Detach the tag itself */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check syntax:
	 *   :flags <list-of-flags: string-list>
	 */
	if ( !sieve_validate_tag_parameter
		(validator, cmd, tag, *arg, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	tag->parameters = *arg;
	
	/* Detach parameter */
	*arg = sieve_ast_arguments_detach(*arg,1);

	return TRUE;
}

/* 
 * Code generation 
 */

static bool tag_flags_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg,
	struct sieve_command_context *cmd)
{
	struct sieve_ast_argument *param;

	if ( sieve_ast_argument_type(arg) != SAAT_TAG ) {
		return FALSE;
	}

	sieve_opr_side_effect_emit(cgenv->sbin, &flags_side_effect);
	  
	param = arg->parameters;

	/* Call the generation function for the argument */ 
	if ( param->argument != NULL && param->argument->generate != NULL && 
		!param->argument->generate(cgenv, param, cmd) ) 
		return FALSE;
	
	return TRUE;
}

/* 
 * Side effect implementation
 */
 
/* Context data */

struct seff_flags_context {
	ARRAY_DEFINE(keywords, const char *);
	enum mail_flags flags;
};

/* Context coding */

static bool seff_flags_dump_context
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	return sieve_opr_stringlist_dump(denv, address);
}

static bool seff_flags_read_context
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_runtime_env *renv, sieve_size_t *address,
	void **se_context)
{
	bool result = TRUE;
	pool_t pool = sieve_result_pool(renv->result);
	struct seff_flags_context *ctx;
	string_t *flags_item;
	struct sieve_coded_stringlist *flag_list;
	
	ctx = p_new(pool, struct seff_flags_context, 1);
	p_array_init(&ctx->keywords, pool, 2);
	
	t_push();
	
	/* Read flag-list */
	if ( (flag_list=sieve_opr_stringlist_read(renv, address)) == NULL ) {
		t_pop();
		return FALSE;
	}
	
	/* Unpack */
	flags_item = NULL;
	while ( (result=sieve_coded_stringlist_next_item(flag_list, &flags_item)) && 
		flags_item != NULL ) {
		const char *flag;
		struct ext_imapflags_iter flit;

		ext_imapflags_iter_init(&flit, flags_item);
	
		while ( (flag=ext_imapflags_iter_get_flag(&flit)) != NULL ) {		
			if (flag != NULL && *flag != '\\') {
				/* keyword */
				const char *keyword = p_strdup(pool, flag);

				/* FIXME: should check for duplicates (cannot trust variables) */
				array_append(&ctx->keywords, &keyword, 1);

			} else {
				/* system flag */
				if (flag == NULL || strcasecmp(flag, "\\flagged") == 0)
					ctx->flags |= MAIL_FLAGGED;
				else if (strcasecmp(flag, "\\answered") == 0)
					ctx->flags |= MAIL_ANSWERED;
				else if (strcasecmp(flag, "\\deleted") == 0)
					ctx->flags |= MAIL_DELETED;
				else if (strcasecmp(flag, "\\seen") == 0)
					ctx->flags |= MAIL_SEEN;
				else if (strcasecmp(flag, "\\draft") == 0)
					ctx->flags |= MAIL_DRAFT;
			}
		}
	}
	
	*se_context = (void *) ctx;

	t_pop();
	
	return result;
}

static struct seff_flags_context *seff_flags_get_implicit_context
(struct sieve_result *result)
{
	pool_t pool = sieve_result_pool(result);
	struct seff_flags_context *ctx;
	const char *flag;
	struct ext_imapflags_iter flit;
	
	ctx = p_new(pool, struct seff_flags_context, 1);
	p_array_init(&ctx->keywords, pool, 2);
	
	t_push();
		
	/* Unpack */
	ext_imapflags_get_implicit_flags_init(&flit, result);
	while ( (flag=ext_imapflags_iter_get_flag(&flit)) != NULL ) {		
		if (flag != NULL && *flag != '\\') {
			/* keyword */
			const char *keyword = p_strdup(pool, flag);
			array_append(&ctx->keywords, &keyword, 1);
		} else {
			/* system flag */
			if (flag == NULL || strcasecmp(flag, "\\flagged") == 0)
				ctx->flags |= MAIL_FLAGGED;
			else if (strcasecmp(flag, "\\answered") == 0)
				ctx->flags |= MAIL_ANSWERED;
			else if (strcasecmp(flag, "\\deleted") == 0)
				ctx->flags |= MAIL_DELETED;
			else if (strcasecmp(flag, "\\seen") == 0)
				ctx->flags |= MAIL_SEEN;
			else if (strcasecmp(flag, "\\draft") == 0)
				ctx->flags |= MAIL_DRAFT;
		}
	}

	t_pop();
	
	return ctx;
}

/* Result printing */

static void seff_flags_print
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_result_print_env *rpenv,
	void *se_context, bool *keep ATTR_UNUSED)
{
	struct sieve_result *result = rpenv->result;
	struct seff_flags_context *ctx = (struct seff_flags_context *) se_context;
	unsigned int i;
	
	if ( ctx == NULL )
		ctx = seff_flags_get_implicit_context(result);
	
	if ( ctx->flags != 0 || array_count(&ctx->keywords) > 0 ) {
		T_BEGIN {
			string_t *flags = t_str_new(128);
 
			if ( (ctx->flags & MAIL_FLAGGED) > 0 )
				str_printfa(flags, " \\flagged\n");

			if ( (ctx->flags & MAIL_ANSWERED) > 0 )
				str_printfa(flags, " \\answered");
		
			if ( (ctx->flags & MAIL_DELETED) > 0 )
				str_printfa(flags, " \\deleted");
					
			if ( (ctx->flags & MAIL_SEEN) > 0 )
				str_printfa(flags, " \\seen");
			
			if ( (ctx->flags & MAIL_DRAFT) > 0 )
				str_printfa(flags, " \\draft");

			for ( i = 0; i < array_count(&ctx->keywords); i++ ) {
				const char *const *keyword = array_idx(&ctx->keywords, i);
				str_printfa(flags, " %s", str_sanitize(*keyword, 64));
			}

			sieve_result_seffect_printf(rpenv, "add IMAP flags:%s", str_c(flags));
		} T_END;
	}
}

/* Result execution */

static bool seff_flags_pre_execute
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_action_exec_env *aenv, 
	void **se_context, void *tr_context)
{	
	struct seff_flags_context *ctx = (struct seff_flags_context *) *se_context;
	struct act_store_transaction *trans = 
		(struct act_store_transaction *) tr_context;
		
	if ( ctx == NULL ) {
		ctx = seff_flags_get_implicit_context(aenv->result);
		*se_context = (void *) ctx;
	}

	/* Assign mail keywords for subsequent mailbox_copy() */
	if ( array_count(&ctx->keywords) > 0 ) {
		unsigned int i;

		if ( !array_is_created(&trans->keywords) ) {
			pool_t pool = sieve_result_pool(aenv->result); 
			p_array_init(&trans->keywords, pool, 2);
		}
		
		for ( i = 0; i < array_count(&ctx->keywords); i++ ) {		
			const char *const *keyword = array_idx(&ctx->keywords, i);
			const char *kw_error;

			if ( trans->box != NULL && 
				mailbox_keyword_is_valid(trans->box, *keyword, &kw_error) )
				array_append(&trans->keywords, keyword, 1);
			else {
				char *error = "";
				if ( kw_error != NULL && *kw_error != '\0' ) {
					error = t_strdup_noconst(kw_error);
					error[0] = i_tolower(error[0]);
				}
				
				sieve_result_warning(aenv, 
					"specified IMAP keyword '%s' is invalid (ignored): %s", 
					str_sanitize(*keyword, 64), error);
			}
		}
	}

	/* Assign mail flags for subsequent mailbox_copy() */
	trans->flags |= ctx->flags;
	
	return TRUE;
}


