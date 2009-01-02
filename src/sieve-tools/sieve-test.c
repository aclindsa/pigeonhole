/* Copyright (c) 2002-2009 Dovecot Sieve authors, see the included COPYING file
 */

#include "lib.h"
#include "ostream.h"
#include "mail-namespace.h"
#include "mail-storage.h"

#include "sieve.h"
#include "sieve-binary.h"

#include "mail-raw.h"
#include "sieve-tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>

/* 
 * Configuration
 */

#define DEFAULT_SENDMAIL_PATH "/usr/lib/sendmail"
#define DEFAULT_ENVELOPE_SENDER "MAILER-DAEMON"

/*
 * Print help
 */

static void print_help(void)
{
#ifdef SIEVE_RUNTIME_TRACE
#  define SVTRACE " [-t]"
#else
#  define SVTRACE
#endif
	printf(
"Usage: sieve-test [-r <recipient address>] [-s <envelope sender>]\n"
"                  [-m <mailbox>] [-d <dump filename>] [-x <extensions>]\n"
"                  [-c]"SVTRACE" <scriptfile> <mailfile>\n"
	);
}

/*
 * Tool implementation
 */

int main(int argc, char **argv) 
{
	const char *scriptfile, *recipient, *sender, *mailbox, *dumpfile, *mailfile,
		*extensions; 
	const char *user;
	int i;
	struct mail_raw *mailr;
	struct sieve_binary *sbin;
	struct sieve_message_data msgdata;
	struct sieve_script_env scriptenv;
	struct sieve_exec_status estatus;
	struct sieve_error_handler *ehandler;
	struct ostream *teststream;
	bool force_compile = FALSE;
	int ret;
	bool trace = FALSE;
	struct ostream *trace_stream = FALSE;

	/* Parse arguments */
	scriptfile = recipient = sender = mailbox = dumpfile = mailfile = 
		extensions = NULL;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-r") == 0) {
			/* recipient address */
			i++;
			if (i == argc)
				i_fatal("Missing -r argument");
			recipient = argv[i];
		} else if (strcmp(argv[i], "-s") == 0) {
			/* envelope sender */
			i++;
			if (i == argc)
				i_fatal("Missing -s argument");
			sender = argv[i];
		} else if (strcmp(argv[i], "-m") == 0) {
			/* default mailbox (keep box) */
			i++;
			if (i == argc) 
				i_fatal("Missing -m argument");
			mailbox = argv[i];
		} else if (strcmp(argv[i], "-d") == 0) {
			/* dump file */
			i++;
			if (i == argc)
				i_fatal("Missing -d argument");
			dumpfile = argv[i];
		} else if (strcmp(argv[i], "-x") == 0) {
			/* extensions */
			i++;
			if (i == argc)
				i_fatal("Missing -x argument");
			extensions = argv[i];
		} else if (strcmp(argv[i], "-c") == 0) {
            /* force compile */
			force_compile = TRUE;
#ifdef SIEVE_RUNTIME_TRACE
		} else if (strcmp(argv[i], "-t") == 0) {
            /* runtime trace */
			trace = TRUE;
#endif
		} else if ( scriptfile == NULL ) {
			scriptfile = argv[i];
		} else if ( mailfile == NULL ) {
			mailfile = argv[i];
		} else {
			print_help();
			i_fatal("Unknown argument: %s", argv[i]);
		}
	}
	
	if ( scriptfile == NULL ) {
		print_help();
		i_fatal("Missing <scriptfile> argument");
	}
	
	if ( mailfile == NULL ) {
		print_help();
		i_fatal("Missing <mailfile> argument");
	}

	sieve_tool_init();

	if ( extensions != NULL ) {
		sieve_set_extensions(extensions);
	}
	
	/* Compile sieve script */
	if ( force_compile ) {
		sbin = sieve_tool_script_compile(scriptfile);
		(void) sieve_save(sbin, NULL);
	} else {
		sbin = sieve_tool_script_open(scriptfile);
	}

	/* Dump script */
	sieve_tool_dump_binary_to(sbin, dumpfile);
	
	user = sieve_tool_get_user();

	/* Initialize mail storages */
	mail_users_init(getenv("AUTH_SOCKET_PATH"), getenv("DEBUG") != NULL);
	mail_storage_init();
	mail_storage_register_all();
	mailbox_list_register_all();

	/* Initialize raw mail object */
	mail_raw_init(user);
	mailr = mail_raw_open_file(mailfile);

	sieve_tool_get_envelope_data(mailr->mail, &recipient, &sender);

	if ( mailbox == NULL )
		mailbox = "INBOX";

	/* Collect necessary message data */
	memset(&msgdata, 0, sizeof(msgdata));
	msgdata.mail = mailr->mail;
	msgdata.return_path = sender;
	msgdata.to_address = recipient;
	msgdata.auth_user = user;
	(void)mail_get_first_header(mailr->mail, "Message-ID", &msgdata.id);

	memset(&scriptenv, 0, sizeof(scriptenv));
	scriptenv.default_mailbox = "INBOX";
	scriptenv.username = user;

	ehandler = sieve_stderr_ehandler_create(0);	

	teststream = o_stream_create_fd(1, 0, FALSE);	

	if ( trace )
		trace_stream = teststream;
	else
		trace_stream = NULL;

	/* Run the test */
	ret = sieve_test
		(sbin, &msgdata, &scriptenv, &estatus, teststream, ehandler, trace_stream);

	if ( ret == SIEVE_EXEC_BIN_CORRUPT ) {
		i_info("Corrupt binary deleted.");
		(void) unlink(sieve_binary_path(sbin));		
	}

	o_stream_destroy(&teststream);

	sieve_close(&sbin);
	sieve_error_handler_unref(&ehandler);

	/* De-initialize raw mail object */
	mail_raw_close(mailr);
	mail_raw_deinit();

	/* De-initialize mail storages */
	mail_storage_deinit();
	mail_users_deinit();

	sieve_tool_deinit();  
	return 0;
}
