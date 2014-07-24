/* Copyright (c) 2002-2014 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "abspath.h"
#include "ioloop.h"
#include "hostpid.h"
#include "file-copy.h"

#include "sieve-file-storage.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int _file_path_cmp(const char *path1, const char *path2)
{
	const char *p1, *p2;
	int ret;

	p1 = path1; p2 = path2;
	if (*p2 == '\0' && *p1 != '\0')
		return 1;
	if (*p1 == '\0' && *p2 != '\0')
		return -1;
	if (*p1 == '/' && *p2 != '/')
		return 1;
	if (*p2 == '/' && *p1 != '/')
		return -1;
	for (;;) {
		const char *s1, *s2;
		size_t size1, size2;

		/* skip repeated slashes */
		for (; *p1 == '/'; p1++);
		for (; *p2 == '/'; p2++);
		/* check for end of comparison */
		if (*p1 == '\0' || *p2 == '\0')
			break;
		/* mark start of path element */
		s1 = p1;
		s2 = p2;
		/* scan to end of path elements */
		for (; *p1 != '\0' && *p1 != '/'; p1++);
		for (; *p2 != '\0' && *p2 != '/'; p2++);
		/* compare sizes */
		size1 = p1 - s1;
		size2 = p2 - s2;
		if (size1 != size2)
			return size1 - size2;
		/* compare */
		if (size1 > 0 && (ret=memcmp(s1, s2, size1)) != 0)
			return ret;
	}
	if (*p1 == '\0') {
		if (*p2 == '\0')
			return 0;
		return -1;
	}
	return 1;
}

/*
 * Symlink manipulation
 */

static int sieve_file_storage_active_read_link
(struct sieve_file_storage *fstorage, const char **link_r)
{
	struct sieve_storage *storage = &fstorage->storage;
	int ret;

	ret = t_readlink(fstorage->active_path, link_r);

	if ( ret < 0 ) {
		*link_r = NULL;

		if ( errno == EINVAL ) {
			/* Our symlink is no symlink. Report 'no active script'.
			 * Activating a script will automatically resolve this, so
			 * there is no need to panic on this one.
			 */
			if ( (storage->flags & SIEVE_STORAGE_FLAG_SYNCHRONIZING) == 0 ) {
				sieve_storage_sys_warning(storage,
					"Active sieve script symlink %s is no symlink.",
				  fstorage->active_path);
			}
			return 0;
		}

		if ( errno == ENOENT ) {
			/* Symlink not found */
			return 0;
		}

		/* We do need to panic otherwise */
		sieve_storage_set_critical(storage,
			"Performing readlink() on active sieve symlink '%s' failed: %m",
			fstorage->active_path);
		return -1;
	}

	/* ret is now assured to be valid, i.e. > 0 */
	return 1;
}

static const char *sieve_file_storage_active_parse_link
(struct sieve_file_storage *fstorage, const char *link,
	const char **scriptname_r)
{
	struct sieve_storage *storage = &fstorage->storage;
	const char *fname, *scriptname, *scriptpath;

	/* Split link into path and filename */
	fname = strrchr(link, '/');
	if ( fname != NULL ) {
		scriptpath = t_strdup_until(link, fname+1);
		fname++;
	} else {
		scriptpath = "";
		fname = link;
	}

	/* Check the script name */
	scriptname = sieve_script_file_get_scriptname(fname);

	/* Warn if link is deemed to be invalid */
	if ( scriptname == NULL ) {
		sieve_storage_sys_warning(storage,
			"Active sieve script symlink %s is broken: "
			"invalid scriptname (points to %s).",
			fstorage->active_path, link);
		return NULL;
	}

	/* Check whether the path is any good */
	if ( _file_path_cmp(scriptpath, fstorage->link_path) != 0 &&
		_file_path_cmp(scriptpath, fstorage->path) != 0 ) {
		sieve_storage_sys_warning(storage,
			"Active sieve script symlink %s is broken: "
			"invalid/unknown path to storage (points to %s).",
			fstorage->active_path, link);
		return NULL;
	}

	if ( scriptname_r != NULL )
		*scriptname_r = scriptname;

	return fname;
}

int sieve_file_storage_active_replace_link
(struct sieve_file_storage *fstorage, const char *link_path)
{
	struct sieve_storage *storage = &fstorage->storage;
	const char *active_path_new;
	struct timeval *tv, tv_now;
	int ret = 0;

	tv = &ioloop_timeval;

	for (;;) {
		/* First the new symlink is created with a different filename */
		active_path_new = t_strdup_printf
			("%s-new.%s.P%sM%s.%s",
				fstorage->active_path,
				dec2str(tv->tv_sec), my_pid,
				dec2str(tv->tv_usec), my_hostname);

		ret = symlink(link_path, active_path_new);

		if ( ret < 0 ) {
			/* If link exists we try again later */
			if ( errno == EEXIST ) {
				/* Wait and try again - very unlikely */
				sleep(2);
				tv = &tv_now;
				if (gettimeofday(&tv_now, NULL) < 0)
					i_fatal("gettimeofday(): %m");
				continue;
			}

			/* Other error, critical */
			sieve_storage_set_critical(storage,
				"Creating symlink() %s to %s failed: %m",
				active_path_new, link_path);
			return -1;
		}

		/* Link created */
		break;
	}

	/* Replace the existing link. This activates the new script */
	ret = rename(active_path_new, fstorage->active_path);

	if ( ret < 0 ) {
		/* Failed; created symlink must be deleted */
		(void)unlink(active_path_new);
		sieve_storage_set_critical(storage,
			"Performing rename() %s to %s failed: %m",
			active_path_new, fstorage->active_path);
		return -1;
	}

	return 1;
}

/*
 * Active script properties
 */

int sieve_file_storage_active_script_get_file
(struct sieve_file_storage *fstorage, const char **file_r)
{
	const char *link, *scriptfile;
	int ret;

	*file_r = NULL;

	/* Read the active link */
	if ( (ret=sieve_file_storage_active_read_link(fstorage, &link)) <= 0 )
		return ret;

	/* Parse the link */
	scriptfile = sieve_file_storage_active_parse_link(fstorage, link, NULL);

	if (scriptfile == NULL) {
		/* Obviously, someone has been playing with our symlink:
		 * ignore this situation and report 'no active script'.
		 * Activation should fix this situation.
		 */
		return 0;
	}

	*file_r = scriptfile;
	return 1;
}

int sieve_file_storage_active_script_get_name
(struct sieve_storage *storage, const char **name_r)
{
	struct sieve_file_storage *fstorage =
		(struct sieve_file_storage *)storage;
	const char *link;
	int ret;

	*name_r = NULL;

	/* Read the active link */
	if ( (ret=sieve_file_storage_active_read_link
		(fstorage, &link)) <= 0 )
		return ret;

	if ( sieve_file_storage_active_parse_link
		(fstorage, link, name_r) == NULL ) {
		/* Obviously, someone has been playing with our symlink:
		 * ignore this situation and report 'no active script'.
		 * Activation should fix this situation.
		 */
		return 0;
	}

	return 1;
}

/*
 * Active script
 */ 

struct sieve_script *sieve_file_storage_active_script_open
(struct sieve_storage *storage)
{
	struct sieve_file_storage *fstorage =
		(struct sieve_file_storage *)storage;
	struct sieve_file_script *fscript;
	const char *scriptfile, *link;
	int ret;

	sieve_storage_clear_error(storage);

	/* Read the active link */
	if ( (ret=sieve_file_storage_active_read_link(fstorage, &link)) <= 0 ) {
		if ( ret < 0 )
			return NULL;

		/* Try to open the active_path as a regular file */
		fscript = sieve_file_script_open_from_path(fstorage,
			fstorage->active_path, NULL, NULL);
		if ( fscript == NULL ) {
			if ( storage->error_code != SIEVE_ERROR_NOT_FOUND ) {
				sieve_storage_set_critical(storage,
					"Failed to open active path `%s' as regular file: %s",
					fstorage->active_path, storage->error);
			}
			return NULL;
		}

		return &fscript->script;
	} 

	/* Parse the link */
	scriptfile = sieve_file_storage_active_parse_link(fstorage, link, NULL);
	if (scriptfile == NULL) {
		/* Obviously someone has been playing with our symlink,
		 * ignore this situation and report 'no active script'.
		 * Activation should fix this situation.
		 */
		return NULL;
	}

	fscript = sieve_file_script_open_from_path(fstorage,
		fstorage->active_path,
		sieve_script_file_get_scriptname(scriptfile),
		NULL);
	if ( fscript == NULL && storage->error_code == SIEVE_ERROR_NOT_FOUND ) {
		sieve_storage_sys_warning(storage,
			"Active sieve script symlink %s points to non-existent script "
			"(points to %s).", fstorage->active_path, link);
	}
	return (fscript != NULL ? &fscript->script : NULL);
}

int sieve_file_storage_active_script_get_last_change
(struct sieve_storage *storage, time_t *last_change_r)
{
	struct sieve_file_storage *fstorage =
		(struct sieve_file_storage *)storage;
	struct stat st;

	/* Try direct lstat first */
	if ( lstat(fstorage->active_path, &st) == 0 ) {
		if ( !S_ISLNK(st.st_mode) ) {
			*last_change_r = st.st_mtime;
			return 0;
		}
	}
	/* Check error */
	else if ( errno != ENOENT ) {
		sieve_storage_set_critical(storage,
			"lstat(%s) failed: %m", fstorage->active_path);
	}

	/* Fall back to statting storage directory */
	return sieve_storage_get_last_change(storage, last_change_r);
}

bool sieve_file_storage_active_rescue_regular
(struct sieve_file_storage *fstorage)
{
	struct sieve_storage *storage = &fstorage->storage;
	struct stat st;

	/* Stat the file */
	if ( lstat(fstorage->active_path, &st) != 0 ) {
		if ( errno != ENOENT ) {
			sieve_storage_set_critical(storage,
				"Failed to stat active sieve script symlink (%s): %m.",
				fstorage->active_path);
			return FALSE;
		}
		return TRUE;
	}

	if ( S_ISLNK( st.st_mode ) ) {
		sieve_storage_sys_debug(storage,
			"Nothing to rescue %s.", fstorage->active_path);
		return TRUE; /* Nothing to rescue */
	}

	/* Only regular files can be rescued */
	if ( S_ISREG( st.st_mode ) ) {
		const char *dstpath;
		bool result = TRUE;

 		T_BEGIN {

			dstpath = t_strconcat( fstorage->path, "/",
				sieve_script_file_from_name("dovecot.orig"), NULL );
			if ( file_copy(fstorage->active_path, dstpath, 1) < 1 ) {
				sieve_storage_set_critical(storage,
					"Active sieve script file '%s' is a regular file "
					"and copying it to the script storage as '%s' failed. "
					"This needs to be fixed manually.",
					fstorage->active_path, dstpath);
				result = FALSE;
			} else {
				sieve_storage_sys_info(storage,
					"Moved active sieve script file '%s' "
					"to script storage as '%s'.",
					fstorage->active_path, dstpath);
			}
		} T_END;

		return result;
	}

	sieve_storage_set_critical(storage,
		"Active sieve script file '%s' is no symlink nor a regular file. "
		"This needs to be fixed manually.", fstorage->active_path);
	return FALSE;
}

int sieve_file_storage_deactivate(struct sieve_storage *storage)
{
	struct sieve_file_storage *fstorage =
		(struct sieve_file_storage *)storage;
	int ret;

	if ( sieve_file_storage_pre_modify(storage) < 0 )
		return -1;

	if ( !sieve_file_storage_active_rescue_regular(fstorage) )
		return -1;

	/* Delete the symlink, so no script is active */
	ret = unlink(fstorage->active_path);

	if ( ret < 0 ) {
		if ( errno != ENOENT ) {
			sieve_storage_set_critical(storage,
				"Failed to deactivate Sieve: "
				"unlink(%s) failed: %m", fstorage->active_path);
			return -1;
		} else {
			return 0;
		}
	}
	return 1;
}
