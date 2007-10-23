#include <stdio.h>

#include "sieve-common.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"

/* Forward declarations */
static bool ext_vacation_validator_load(struct sieve_validator *validator);
static bool ext_vacation_opcode_dump(struct sieve_interpreter *interpreter);

static bool cmd_vacation_registered(struct sieve_validator *validator, struct sieve_command_registration *cmd_reg);
static bool cmd_vacation_validate(struct sieve_validator *validator, struct sieve_command_context *cmd);
static bool cmd_vacation_generate(struct sieve_generator *generator,	struct sieve_command_context *ctx);

/* Extension definitions */
const struct sieve_extension vacation_extension = 
	{ "vacation", ext_vacation_validator_load, NULL, { ext_vacation_opcode_dump, NULL } };
static const struct sieve_command vacation_command = 
	{ "vacation", SCT_COMMAND, cmd_vacation_registered, cmd_vacation_validate, cmd_vacation_generate, NULL };

/* Tag validation */

static bool cmd_vacation_validate_days_tag
	(struct sieve_validator *validator, 
	struct sieve_ast_argument **arg, 
	struct sieve_command_context *cmd)
{
	/* Only one possible tag, so we don't bother checking the identifier */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check syntax:
	 *   :days number
	 */
	if ( (*arg)->type != SAAT_NUMBER ) {
		sieve_command_validate_error(validator, cmd, 
			"the :days tag for the vacation command requires one number argument, but %s was found", sieve_ast_argument_name(*arg) );
		return FALSE;
	}
	
	/* FIXME: assign somewhere */
	
	return TRUE;
}

static bool cmd_vacation_validate_subject_tag
	(struct sieve_validator *validator, 
	struct sieve_ast_argument **arg, 
	struct sieve_command_context *cmd)
{
	/* Only one possible tag, so we don't bother checking the identifier */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check syntax:
	 *   :subject string
	 */
	if ( (*arg)->type != SAAT_STRING ) {
		sieve_command_validate_error(validator, cmd, 
			"the :subject tag for the vacation command requires one string argument, but %s was found", 
				sieve_ast_argument_name(*arg) );
		return FALSE;
	}
	
	/* FIXME: assign somewhere */
	
	return TRUE;
}

static bool cmd_vacation_validate_from_tag
	(struct sieve_validator *validator, 
	struct sieve_ast_argument **arg, 
	struct sieve_command_context *cmd)
{
	/* Only one possible tag, so we don't bother checking the identifier */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check syntax:
	 *   :from string
	 */
	if ( (*arg)->type != SAAT_STRING ) {
		sieve_command_validate_error(validator, cmd, 
			"the :from tag for the vacation command requires one string argument, but %s was found", 
				sieve_ast_argument_name(*arg) );
		return FALSE;
	}
	
	/* FIXME: assign somewhere */
	
	return TRUE;
}

static bool cmd_vacation_validate_addresses_tag
	(struct sieve_validator *validator, 
	struct sieve_ast_argument **arg, 
	struct sieve_command_context *cmd)
{
	/* Only one possible tag, so we don't bother checking the identifier */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check syntax:
	 *   :addresses string-list
	 */
	if ( (*arg)->type != SAAT_STRING && (*arg)->type != SAAT_STRING_LIST ) {
		sieve_command_validate_error(validator, cmd, 
			"the :addresses tag for the vacation command requires one string argument, but %s was found", 
				sieve_ast_argument_name(*arg) );
		return FALSE;
	}
	
	/* FIXME: assign somewhere */
	
	return TRUE;
}

static bool cmd_vacation_validate_mime_tag
	(struct sieve_validator *validator __attr_unused__, 
	struct sieve_ast_argument **arg __attr_unused__, 
	struct sieve_command_context *cmd __attr_unused__)
{
	/* FIXME: assign somewhere */
		
	return TRUE;
}

static bool cmd_vacation_validate_handle_tag
	(struct sieve_validator *validator, 
	struct sieve_ast_argument **arg, 
	struct sieve_command_context *cmd)
{
	/* Only one possible tag, so we don't bother checking the identifier */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check syntax:
	 *   :addresses string-list
	 */
	if ( (*arg)->type != SAAT_STRING ) {
		sieve_command_validate_error(validator, cmd, 
			"the :handle tag for the vacation command requires one string argument, but %s was found", 
				sieve_ast_argument_name(*arg) );
		return FALSE;
	}
	
	/* FIXME: assign somewhere */
	
	return TRUE;
}

/* Command registration */

static const struct sieve_tag vacation_days_tag = { "days", cmd_vacation_validate_days_tag };
static const struct sieve_tag vacation_subject_tag = { "subject", cmd_vacation_validate_subject_tag };
static const struct sieve_tag vacation_from_tag = { "from", cmd_vacation_validate_from_tag };
static const struct sieve_tag vacation_addresses_tag = { "addresses", cmd_vacation_validate_addresses_tag };
static const struct sieve_tag vacation_mime_tag = { "mime", cmd_vacation_validate_mime_tag };
static const struct sieve_tag vacation_handle_tag = { "handle", cmd_vacation_validate_handle_tag };

static bool cmd_vacation_registered(struct sieve_validator *validator, struct sieve_command_registration *cmd_reg) 
{
	sieve_validator_register_tag(validator, cmd_reg, &vacation_days_tag); 	
	sieve_validator_register_tag(validator, cmd_reg, &vacation_subject_tag); 	
	sieve_validator_register_tag(validator, cmd_reg, &vacation_from_tag); 	
	sieve_validator_register_tag(validator, cmd_reg, &vacation_addresses_tag); 	
	sieve_validator_register_tag(validator, cmd_reg, &vacation_mime_tag); 	
	sieve_validator_register_tag(validator, cmd_reg, &vacation_handle_tag); 	

	return TRUE;
}

/* Command validation */

static bool cmd_vacation_validate(struct sieve_validator *validator, struct sieve_command_context *cmd) 
{ 	
	struct sieve_ast_argument *arg;
	
	/* Check valid syntax: 
	 *    vacation [":days" number] [":subject" string]
   *                 [":from" string] [":addresses" string-list]
   *                 [":mime"] [":handle" string] <reason: string>
	 */
	if ( !sieve_validate_command_arguments(validator, cmd, 1, &arg) ||
		!sieve_validate_command_subtests(validator, cmd, 0) || 
	 	!sieve_validate_command_block(validator, cmd, FALSE, FALSE) ) {
	 	
		return FALSE;
	}
	
	cmd->data = (void *) arg;
	
	return TRUE;
}

/* Load extension into validator */
static bool ext_vacation_validator_load(struct sieve_validator *validator)
{
	/* Register new command */
	sieve_validator_register_command(validator, &vacation_command);

	return TRUE;
}

/*
 * Generation
 */
 
static bool cmd_vacation_generate
	(struct sieve_generator *generator,	struct sieve_command_context *ctx) 
{
	struct sieve_ast_argument *arg = (struct sieve_ast_argument *) ctx->data;
	
	sieve_generator_emit_ext_opcode(generator, &vacation_extension);

	/* Emit folder string */  	
	if ( !sieve_generator_emit_string_argument(generator, arg) ) 
		return FALSE;
	
	return TRUE;
}

/* 
 * Code dump
 */
 
static bool ext_vacation_opcode_dump(struct sieve_interpreter *interpreter)
{
	printf("VACATION\n");
	sieve_interpreter_dump_operand(interpreter);
	
	return TRUE;
}

