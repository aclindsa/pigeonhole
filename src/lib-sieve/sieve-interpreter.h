#ifndef __SIEVE_INTERPRETER_H
#define __SIEVE_INTERPRETER_H

#include "lib.h"
#include "array.h"
#include "buffer.h"
#include "mail-storage.h"

#include "sieve-common.h"

/* FIXME: make dumps use some interpreter function */
#include <stdio.h>

struct sieve_interpreter;

struct sieve_runtime_env {
	struct sieve_interpreter *interp;
	struct sieve_binary *sbin;
	const struct sieve_message_data *msgdata;
	const struct sieve_mail_environment *mailenv;
	struct sieve_result *result;
};

struct sieve_interpreter *sieve_interpreter_create(struct sieve_binary *sbin);
void sieve_interpreter_free(struct sieve_interpreter *interp);
inline pool_t sieve_interpreter_pool(struct sieve_interpreter *interp);

inline void sieve_interpreter_reset
	(struct sieve_interpreter *interp);
inline void sieve_interpreter_stop
	(struct sieve_interpreter *interp);
inline sieve_size_t sieve_interpreter_program_counter
	(struct sieve_interpreter *interp);
inline bool sieve_interpreter_program_jump
	(struct sieve_interpreter *interp, bool jump);
	
inline void sieve_interpreter_set_test_result
	(struct sieve_interpreter *interp, bool result);
inline bool sieve_interpreter_get_test_result
	(struct sieve_interpreter *interp);
	
/* Extension support */

inline void sieve_interpreter_extension_set_context
	(struct sieve_interpreter *interp, int ext_id, void *context);
inline const void *sieve_interpreter_extension_get_context
	(struct sieve_interpreter *interp, int ext_id);
	
/* Opcodes and operands */

bool sieve_interpreter_read_offset_operand
	(struct sieve_interpreter *interp, int *offset);

/* Code dump (debugging purposes) */

void sieve_interpreter_dump_code(struct sieve_interpreter *interp);

/* Code execute */

bool sieve_interpreter_execute_operation(struct sieve_interpreter *interp); 

bool sieve_interpreter_run
(struct sieve_interpreter *interp, const struct sieve_message_data *msgdata,
	const struct sieve_mail_environment *menv, struct sieve_result **result);

#endif /* __SIEVE_INTERPRETER_H */
