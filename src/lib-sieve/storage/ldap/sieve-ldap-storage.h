/* Copyright (c) 2002-2016 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_LDAP_STORAGE_H
#define __SIEVE_LDAP_STORAGE_H

#include "sieve.h"
#include "sieve-script-private.h"
#include "sieve-storage-private.h"

#define SIEVE_LDAP_SCRIPT_DEFAULT "default"

#if defined(SIEVE_BUILTIN_LDAP) || defined(PLUGIN_BUILD)

#include "sieve-ldap-db.h"

struct sieve_ldap_storage;

/*
 * LDAP settings
 */

struct sieve_ldap_storage_settings {
	const char *hosts;
	const char *uris;
	const char *dn;
	const char *dnpass;

	bool tls;
	bool sasl_bind;
	const char *sasl_mech;
	const char *sasl_realm;
	const char *sasl_authz_id;

	const char *tls_ca_cert_file;
	const char *tls_ca_cert_dir;
	const char *tls_cert_file;
	const char *tls_key_file;
	const char *tls_cipher_suite;
	const char *tls_require_cert;

	const char *deref;
	const char *scope;
	const char *base;
	unsigned int ldap_version;

	const char *ldaprc_path;
	const char *debug_level;

	const char *sieve_ldap_script_attr;
	const char *sieve_ldap_mod_attr;
	const char *sieve_ldap_filter;

	/* ... */
	int ldap_deref, ldap_scope, ldap_tls_require_cert;
};

int sieve_ldap_storage_read_settings
	(struct sieve_ldap_storage *lstorage, const char *config_path);

/*
 * Storage class
 */

struct sieve_ldap_storage {
	struct sieve_storage storage;

	struct sieve_ldap_storage_settings set;
	time_t set_mtime;

	const char *config_file;
	const char *username; // FIXME: needed?

	struct ldap_connection *conn;
};

struct sieve_script *sieve_ldap_storage_active_script_open
	(struct sieve_storage *storage);
int sieve_ldap_storage_active_script_get_name
	(struct sieve_storage *storage, const char **name_r);

/*
 * Script class
 */

struct sieve_ldap_script {
	struct sieve_script script;

	const char *dn;
	const char *modattr;

	const char *binpath;
};

struct sieve_ldap_script *sieve_ldap_script_init
	(struct sieve_ldap_storage *lstorage, const char *name);

/*
 * Script sequence
 */

struct sieve_script_sequence *sieve_ldap_storage_get_script_sequence
	(struct sieve_storage *storage, enum sieve_error *error_r);

struct sieve_script *sieve_ldap_script_sequence_next
    (struct sieve_script_sequence *seq, enum sieve_error *error_r);
void sieve_ldap_script_sequence_destroy(struct sieve_script_sequence *seq);

#endif

#endif
