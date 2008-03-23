/* Match-type ':is': 
 *
 */

#include "lib.h"

#include "sieve-match-types.h"
#include "sieve-comparators.h"

#include <string.h>
#include <stdio.h>

/* 
 * Forward declarations 
 */

static bool mcht_is_match
	(struct sieve_match_context *mctx, const char *val1, size_t val1_size, 
		const char *key, size_t key_size, int key_index);

/* 
 * Match-type object 
 */

const struct sieve_match_type is_match_type = {
	"is", TRUE,
	NULL,
	SIEVE_MATCH_TYPE_IS,
	NULL, NULL, NULL,
	mcht_is_match,
	NULL
};

/*
 * Match-type implementation
 */

static bool mcht_is_match
(struct sieve_match_context *mctx ATTR_UNUSED, 
	const char *val1, size_t val1_size, 
	const char *key, size_t key_size, int key_index ATTR_UNUSED)
{
	if ( mctx->comparator->compare != NULL )
		return (mctx->comparator->compare(mctx->comparator, 
			val1, val1_size, key, key_size) == 0);

	return FALSE;
}
