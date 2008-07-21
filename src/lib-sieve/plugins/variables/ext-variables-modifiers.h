#ifndef __EXT_VARIABLES_MODIFIERS_H
#define __EXT_VARIABLES_MODIFIERS_H

#include "ext-variables-common.h"
#include "sieve-ext-variables.h"

enum ext_variables_modifier_code {
	EXT_VARIABLES_MODIFIER_LOWER,
	EXT_VARIABLES_MODIFIER_UPPER,
	EXT_VARIABLES_MODIFIER_LOWERFIRST,
	EXT_VARIABLES_MODIFIER_UPPERFIRST,
	EXT_VARIABLES_MODIFIER_QUOTEWILDCARD,
	EXT_VARIABLES_MODIFIER_LENGTH
};

const struct sieve_variables_modifier *ext_variables_modifier_find
	(struct sieve_validator *validator, const char *identifier, int *ext_id_r);

void ext_variables_register_core_modifiers
	(struct ext_variables_validator_context *ctx);
	
/*
 * Modifier operand
 */

extern const struct sieve_operand_class 
	ext_variables_modifier_operand_class;
extern const struct sieve_operand modifier_operand;

static inline void ext_variables_opr_modifier_emit
(struct sieve_binary *sbin, const struct sieve_variables_modifier *modf, 
	int ext_id)
{ 
	sieve_opr_object_emit(sbin, &modf->object, ext_id);
}

static inline const struct sieve_variables_modifier *
	ext_variables_opr_modifier_read
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	return (const struct sieve_variables_modifier *) sieve_opr_object_read
		(renv, &ext_variables_modifier_operand_class, address);
}

static inline bool ext_variables_opr_modifier_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	return sieve_opr_object_dump
		(denv, &ext_variables_modifier_operand_class, address, NULL);
}
	
#endif /* __EXT_VARIABLES_MODIFIERS_H */