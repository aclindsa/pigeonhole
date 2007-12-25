/* Extension encoded-character 
 * ---------------------------
 *
 * Authors: Stephan Bosch
 * Specification: draft-ietf-sieve-3028bis-13.txt
 * Implementation: skeleton  
 * Status: under development
 *
 */

#include "lib.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"

/* Forward declarations */

static bool ext_encoded_character_load(int ext_id);
static bool ext_encoded_character_validator_load(struct sieve_validator *validator);

/* Extension definitions */

static int ext_my_id;
	
struct sieve_extension encoded_character_extension = { 
	"encoded-character", 
	ext_encoded_character_load,
	ext_encoded_character_validator_load, 
	NULL, NULL, NULL, 
	SIEVE_EXT_DEFINE_NO_OPCODES, 
	NULL 
};

static bool ext_encoded_character_load(int ext_id) 
{
	ext_my_id = ext_id;
	return TRUE;
}

/* New argument */

bool arg_encoded_string_validate
	(struct sieve_validator *validator, struct sieve_ast_argument **arg, 
		struct sieve_command_context *context);

const struct sieve_argument encoded_string_argument =
	{ "@encoded-string", NULL, arg_encoded_string_validate, NULL, NULL };

static bool _skip_whitespace
	(const char **in, const char *inend)
{
	while ( *in < inend ) {
		if ( **in == '\r' ) {
			(*in)++;
			if ( **in != '\n' )
				return FALSE;
			continue;
		}
		
		if ( **in != ' ' && **in != '\n' ) /* Loose LF is non-standard */
			break;
			
		(*in)++;
	}
	
	return TRUE;
}

static bool _parse_hexint
(const char **in, const char *inend, int max_digits, unsigned int *result)
{
	int digit = 0;
	*result = 0;
		
	while ( *in < inend && digit < max_digits ) {
	
		if ( (**in) >= '0' && (**in) <= '9' ) 
			*result = ((*result) << 4) + (**in) - ((unsigned int) '0');
		else if ( (**in) >= 'a' && (**in) <= 'f' )
			*result = ((*result) << 4) + (**in) - ((unsigned int) 'a') + 0x0a;
		else if ( (**in) >= 'A' && (**in) <= 'F' )
			*result = ((*result) << 4) + (**in) - ((unsigned int) 'A') + 0x0a;
		else
			return ( digit > 0 );
	
		(*in)++;
		digit++;
	}
	
	if ( digit == max_digits ) {
		/* Hex digit _MUST_ end here */
		if ( (**in >= '0' && **in <= '9')	|| (**in >= 'a' && **in <= 'f') ||
			(**in >= 'A' && **in <= 'F') )
			return FALSE;
			
		return TRUE;
	}
	
	return ( digit > 0 );
}

static bool _decode_hex
(const char **in, const char *inend, string_t *result) 
{
	int values = 0;
	
	for (;;) {
		unsigned int hexpair;
		
		if ( !_skip_whitespace(in, inend) ) return FALSE;
		
		if ( !_parse_hexint(in, inend, 2, &hexpair) ) break;
		
		str_append_c(result, (unsigned char) hexpair);
		values++;
	}
	
	return ( values > 0 );
}

static bool _decode_unicode
(const char **in, const char *inend, string_t *result) 
{
	int values = 0;
	
	for (;;) {
		unsigned int unicode_hex;
		
		if ( !_skip_whitespace(in, inend) ) return FALSE;
		
		if ( !_parse_hexint(in, inend, 6, &unicode_hex) ) break;
		
		/* FIXME: unicode is unimplemented */
		str_append(result, ">>unicode unimplemented<<");
		values++;
	}
	
	return ( values > 0 );
}

bool arg_encoded_string_validate
(struct sieve_validator *validator ATTR_UNUSED, struct sieve_ast_argument **arg, 
		struct sieve_command_context *cmd)
{
	enum { ST_NONE, ST_OPEN, ST_TYPE, ST_CLOSE } 
		state = ST_NONE;
	string_t *str = sieve_ast_argument_str(*arg);
	string_t *tmpstr, *newstr = NULL;
	const char *p, *mark, *strstart, *substart = NULL;
	const char *strval = (const char *) str_data(str);
	const char *strend = strval + str_len(str);

	T_FRAME(		
		tmpstr = t_str_new(32);	
			
		p = strval;
		strstart = p;
		while ( p < strend ) {
			switch ( state ) {
			case ST_NONE:
				if ( *p == '$' ) {
					substart = p;
					state = ST_OPEN;
				}
				p++;
				break;
			case ST_OPEN:
				if ( *p == '{' ) {
					state = ST_TYPE;
					p++;
				} else 
					state = ST_NONE;
				break;
			case ST_TYPE:
				mark = p;
				while ( p < strend && *p != ':' && *p != '$' && *p != '}' ) p++;
					
				if ( *p == '$' || *p == '}' ) {
					state = ST_NONE;
					break;
				}
				
				state = ST_CLOSE;
				
				str_truncate(tmpstr, 0);
				if ( strncasecmp(mark, "hex", p - mark) == 0 ) {
					p++;
					if ( !_decode_hex(&p, strend, tmpstr) ) 
						state = ST_NONE;
				} else if ( strncasecmp(mark, "unicode", p - mark) == 0 ) {
					p++;
					if ( !_decode_unicode(&p, strend, tmpstr) ) 
						state = ST_NONE;
				} else {	
					p++;
					state = ST_NONE;
				}
				break;
			case ST_CLOSE:
				if ( *p == '}' ) {				
					/* We now know that the substitution is valid */	
					
					if ( newstr == NULL ) {
						newstr = str_new(sieve_ast_pool((*arg)->ast), str_len(str)*2);
					}
					
					str_append_n(newstr, strstart, substart-strstart);
					str_append_str(newstr, tmpstr);
					
					strstart = p + 1;
					substart = strstart;
				}
				state = ST_NONE;
				p++;	
			}
		}
	);
	
	if ( newstr != NULL ) {
		if ( strstart != strend )
			str_append_n(newstr, strstart, strend-strstart);	
	
		sieve_ast_argument_str_set(*arg, newstr);
	}
	
	return sieve_validator_argument_activate_super
		(validator, cmd, *arg, TRUE);
}

/* Load extension into validator */

static bool ext_encoded_character_validator_load
	(struct sieve_validator *validator ATTR_UNUSED)
{
	sieve_validator_argument_override(validator, SAT_CONST_STRING, 
		&encoded_string_argument); 
	return TRUE;
}