/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2008 Joerg Sonnenberger
 * All rights reserved.
 */
#include "archive_platform.h"
#pragma hdrstop
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_format_shar.c 189438 2009-03-06 05:58:56Z kientzle $");
//#include "archive_write_set_format_private.h"

struct shar {
	int dump;
	int end_of_line;
	ArchiveEntry    * entry;
	int has_data;
	char * last_dir;
	/* Line buffer for uuencoded dump format */
	char outbuff[45];
	size_t outpos;
	int wrote_header;
	archive_string work;
	archive_string quoted_name;
};

static int archive_write_shar_close(struct archive_write *);
static int archive_write_shar_free(struct archive_write *);
static int archive_write_shar_header(struct archive_write *, ArchiveEntry *);
static ssize_t  archive_write_shar_data_sed(struct archive_write *, const void * buff, size_t);
static ssize_t  archive_write_shar_data_uuencode(struct archive_write *, const void * buff, size_t);
static int archive_write_shar_finish_entry(struct archive_write *);
/*
 * Copy the given string to the buffer, quoting all shell meta characters found.
 */
static void shar_quote(archive_string * buf, const char * str, int in_shell)
{
	static const char meta[] = "\n \t'`\";&<>()|*?{}[]\\$!#^~";
	size_t len;
	while(*str != '\0') {
		if((len = strcspn(str, meta)) != 0) {
			archive_strncat(buf, str, len);
			str += len;
		}
		else if(*str == '\n') {
			if(in_shell)
				archive_strcat(buf, "\"\n\"");
			else
				archive_strcat(buf, "\\n");
			++str;
		}
		else {
			archive_strappend_char(buf, '\\');
			archive_strappend_char(buf, *str);
			++str;
		}
	}
}

/*
 * Set output format to 'shar' format.
 */
int archive_write_set_format_shar(Archive * _a)
{
	struct archive_write * a = (struct archive_write *)_a;
	struct shar * shar;
	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_NEW, __FUNCTION__);
	/* If someone else was already registered, unregister them. */
	if(a->format_free)
		(a->format_free)(a);
	shar = (struct shar *)SAlloc::C(1, sizeof(*shar));
	if(shar == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate shar data");
		return ARCHIVE_FATAL;
	}
	archive_string_init(&shar->work);
	archive_string_init(&shar->quoted_name);
	a->format_data = shar;
	a->format_name = "shar";
	a->format_write_header = archive_write_shar_header;
	a->format_close = archive_write_shar_close;
	a->format_free = archive_write_shar_free;
	a->format_write_data = archive_write_shar_data_sed;
	a->format_finish_entry = archive_write_shar_finish_entry;
	a->archive.archive_format = ARCHIVE_FORMAT_SHAR_BASE;
	a->archive.archive_format_name = "shar";
	return ARCHIVE_OK;
}

/*
 * An alternate 'shar' that uses uudecode instead of 'sed' to encode
 * file contents and can therefore be used to archive binary files.
 * In addition, this variant also attempts to restore ownership, file modes,
 * and other extended file information.
 */
int archive_write_set_format_shar_dump(Archive * _a)
{
	struct archive_write * a = (struct archive_write *)_a;
	struct shar * shar;
	archive_write_set_format_shar(&a->archive);
	shar = (struct shar *)a->format_data;
	shar->dump = 1;
	a->format_write_data = archive_write_shar_data_uuencode;
	a->archive.archive_format = ARCHIVE_FORMAT_SHAR_DUMP;
	a->archive.archive_format_name = "shar dump";
	return ARCHIVE_OK;
}

static int archive_write_shar_header(struct archive_write * a, ArchiveEntry * entry)
{
	const char * linkname;
	const char * name;
	char * p, * pp;
	struct shar * shar = (struct shar *)a->format_data;
	if(!shar->wrote_header) {
		archive_strcat(&shar->work, "#!/bin/sh\n");
		archive_strcat(&shar->work, "# This is a shell archive\n");
		shar->wrote_header = 1;
	}
	/* Save the entry for the closing. */
	archive_entry_free(shar->entry);
	shar->entry = archive_entry_clone(entry);
	name = archive_entry_pathname(entry);

	/* Handle some preparatory issues. */
	switch(archive_entry_filetype(entry)) {
		case AE_IFREG:
		    /* Only regular files have non-zero size. */
		    break;
		case AE_IFDIR:
		    archive_entry_set_size(entry, 0);
		    /* Don't bother trying to recreate '.' */
		    if(sstreq(name, ".") || sstreq(name, "./"))
			    return ARCHIVE_OK;
		    break;
		case AE_IFIFO:
		case AE_IFCHR:
		case AE_IFBLK:
		    /* All other file types have zero size in the archive. */
		    archive_entry_set_size(entry, 0);
		    break;
		default:
		    archive_entry_set_size(entry, 0);
		    if(archive_entry_hardlink(entry) == NULL && archive_entry_symlink(entry) == NULL) {
			    __archive_write_entry_filetype_unsupported(&a->archive, entry, "shar");
			    return ARCHIVE_WARN;
		    }
	}

	archive_string_empty(&shar->quoted_name);
	shar_quote(&shar->quoted_name, name, 1);

	/* Stock preparation for all file types. */
	archive_string_sprintf(&shar->work, "echo x %s\n", shar->quoted_name.s);

	if(archive_entry_filetype(entry) != AE_IFDIR) {
		/* Try to create the dir. */
		p = sstrdup(name);
		pp = sstrrchr(p, '/');
		/* If there is a / character, try to create the dir. */
		if(pp) {
			*pp = '\0';
			/* Try to avoid a lot of redundant mkdir commands. */
			if(sstreq(p, ".")) {
				/* Don't try to "mkdir ." */
				SAlloc::F(p);
			}
			else if(shar->last_dir == NULL) {
				archive_strcat(&shar->work, "mkdir -p ");
				shar_quote(&shar->work, p, 1);
				archive_strcat(&shar->work, " > /dev/null 2>&1\n");
				shar->last_dir = p;
			}
			else if(sstreq(p, shar->last_dir)) {
				// We've already created this exact dir.
				SAlloc::F(p);
			}
			else if(strlen(p) < strlen(shar->last_dir) && strncmp(p, shar->last_dir, strlen(p)) == 0) {
				/* We've already created a subdir. */
				SAlloc::F(p);
			}
			else {
				archive_strcat(&shar->work, "mkdir -p ");
				shar_quote(&shar->work, p, 1);
				archive_strcat(&shar->work, " > /dev/null 2>&1\n");
				shar->last_dir = p;
			}
		}
		else {
			SAlloc::F(p);
		}
	}

	/* Handle file-type specific issues. */
	shar->has_data = 0;
	if((linkname = archive_entry_hardlink(entry)) != NULL) {
		archive_strcat(&shar->work, "ln -f ");
		shar_quote(&shar->work, linkname, 1);
		archive_string_sprintf(&shar->work, " %s\n", shar->quoted_name.s);
	}
	else if((linkname = archive_entry_symlink(entry)) != NULL) {
		archive_strcat(&shar->work, "ln -fs ");
		shar_quote(&shar->work, linkname, 1);
		archive_string_sprintf(&shar->work, " %s\n", shar->quoted_name.s);
	}
	else {
		switch(archive_entry_filetype(entry)) {
			case AE_IFREG:
			    if(archive_entry_size(entry) == 0) {
				    /* More portable than "touch." */
				    archive_string_sprintf(&shar->work, "test -e \"%s\" || :> \"%s\"\n", shar->quoted_name.s, shar->quoted_name.s);
			    }
			    else {
				    if(shar->dump) {
					    uint mode = archive_entry_mode(entry) & 0777;
					    archive_string_sprintf(&shar->work, "uudecode -p > %s << 'SHAR_END'\n", shar->quoted_name.s);
					    archive_string_sprintf(&shar->work, "begin %o ", mode);
					    shar_quote(&shar->work, name, 0);
					    archive_strcat(&shar->work, "\n");
				    }
				    else {
					    archive_string_sprintf(&shar->work, "sed 's/^X//' > %s << 'SHAR_END'\n", shar->quoted_name.s);
				    }
				    shar->has_data = 1;
				    shar->end_of_line = 1;
				    shar->outpos = 0;
			    }
			    break;
			case AE_IFDIR:
			    archive_string_sprintf(&shar->work, "mkdir -p %s > /dev/null 2>&1\n", shar->quoted_name.s);
			    /* Record that we just created this directory. */
			    SAlloc::F(shar->last_dir);
			    shar->last_dir = sstrdup(name);
			    /* Trim a trailing '/'. */
			    pp = sstrrchr(shar->last_dir, '/');
			    if(pp && pp[1] == '\0')
				    *pp = '\0';
			    /*
			     * TODO: Put dir name/mode on a list to be fixed up at end of archive.
			     */
			    break;
			case AE_IFIFO:
			    archive_string_sprintf(&shar->work, "mkfifo %s\n", shar->quoted_name.s);
			    break;
			case AE_IFCHR:
			    archive_string_sprintf(&shar->work, "mknod %s c %ju %ju\n", shar->quoted_name.s,
					(uintmax_t)archive_entry_rdevmajor(entry), (uintmax_t)archive_entry_rdevminor(entry));
			    break;
			case AE_IFBLK:
			    archive_string_sprintf(&shar->work, "mknod %s b %ju %ju\n", shar->quoted_name.s,
					(uintmax_t)archive_entry_rdevmajor(entry), (uintmax_t)archive_entry_rdevminor(entry));
			    break;
			default:
			    return ARCHIVE_WARN;
		}
	}

	return ARCHIVE_OK;
}

static ssize_t archive_write_shar_data_sed(struct archive_write * a, const void * buff, size_t n)
{
	static const size_t ensured = 65533;
	const char * src;
	char * buf, * buf_end;
	int ret;
	size_t written = n;
	struct shar * shar = (struct shar *)a->format_data;
	if(!shar->has_data || n == 0)
		return 0;
	src = (const char *)buff;
	/*
	 * ensure is the number of bytes in buffer before expanding the
	 * current character.  Each operation writes the current character
	 * and optionally the start-of-new-line marker.  This can happen
	 * twice before entering the loop, so make sure three additional
	 * bytes can be written.
	 */
	if(archive_string_ensure(&shar->work, ensured + 3) == NULL) {
		archive_set_error(&a->archive, ENOMEM, SlTxtOutOfMem);
		return ARCHIVE_FATAL;
	}
	if(shar->work.length > ensured) {
		ret = __archive_write_output(a, shar->work.s, shar->work.length);
		if(ret != ARCHIVE_OK)
			return ARCHIVE_FATAL;
		archive_string_empty(&shar->work);
	}
	buf = shar->work.s + shar->work.length;
	buf_end = shar->work.s + ensured;
	if(shar->end_of_line) {
		*buf++ = 'X';
		shar->end_of_line = 0;
	}
	while(n-- != 0) {
		if((*buf++ = *src++) == '\n') {
			if(n == 0)
				shar->end_of_line = 1;
			else
				*buf++ = 'X';
		}
		if(buf >= buf_end) {
			shar->work.length = buf - shar->work.s;
			ret = __archive_write_output(a, shar->work.s, shar->work.length);
			if(ret != ARCHIVE_OK)
				return ARCHIVE_FATAL;
			archive_string_empty(&shar->work);
			buf = shar->work.s;
		}
	}
	shar->work.length = buf - shar->work.s;
	return (written);
}

#define UUENC(c)        (((c)!=0) ? ((c) & 077) + ' ' : '`')

static void uuencode_group(const char _in[3], char out[4])
{
	const uchar * in = (const uchar *)_in;
	int    t = (in[0] << 16) | (in[1] << 8) | in[2];
	out[0] = UUENC(0x3f & (t >> 18) );
	out[1] = UUENC(0x3f & (t >> 12) );
	out[2] = UUENC(0x3f & (t >> 6) );
	out[3] = UUENC(0x3f & t);
}

static int _uuencode_line(struct archive_write * a, struct shar * shar, const char * inbuf, size_t len)
{
	char * buf;
	/* len <= 45 -> expanded to 60 + len byte + new line */
	size_t alloc_len = shar->work.length + 62;
	if(archive_string_ensure(&shar->work, alloc_len) == NULL) {
		archive_set_error(&a->archive, ENOMEM, SlTxtOutOfMem);
		return ARCHIVE_FATAL;
	}
	buf = shar->work.s + shar->work.length;
	*buf++ = UUENC(len);
	while(len >= 3) {
		uuencode_group(inbuf, buf);
		len -= 3;
		inbuf += 3;
		buf += 4;
	}
	if(len != 0) {
		char tmp_buf[3];
		tmp_buf[0] = inbuf[0];
		if(len == 1)
			tmp_buf[1] = '\0';
		else
			tmp_buf[1] = inbuf[1];
		tmp_buf[2] = '\0';
		uuencode_group(tmp_buf, buf);
		buf += 4;
	}
	*buf++ = '\n';
	if((buf - shar->work.s) > (ptrdiff_t)(shar->work.length + 62)) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC, "Buffer overflow");
		return ARCHIVE_FATAL;
	}
	shar->work.length = buf - shar->work.s;
	return ARCHIVE_OK;
}

#define uuencode_line(__a, __shar, __inbuf, __len) \
	do { \
		int r = _uuencode_line(__a, __shar, __inbuf, __len); \
		if(r != ARCHIVE_OK) \
			return ARCHIVE_FATAL; \
	} while(0)

static ssize_t archive_write_shar_data_uuencode(struct archive_write * a, const void * buff, size_t length)
{
	const char * src;
	size_t n;
	int ret;
	struct shar * shar = (struct shar *)a->format_data;
	if(!shar->has_data)
		return ARCHIVE_OK;
	src = (const char *)buff;
	if(shar->outpos != 0) {
		n = 45 - shar->outpos;
		if(n > length)
			n = length;
		memcpy(shar->outbuff + shar->outpos, src, n);
		if(shar->outpos + n < 45) {
			shar->outpos += n;
			return length;
		}
		uuencode_line(a, shar, shar->outbuff, 45);
		src += n;
		n = length - n;
	}
	else {
		n = length;
	}
	while(n >= 45) {
		uuencode_line(a, shar, src, 45);
		src += 45;
		n -= 45;
		if(shar->work.length < 65536)
			continue;
		ret = __archive_write_output(a, shar->work.s,
			shar->work.length);
		if(ret != ARCHIVE_OK)
			return ARCHIVE_FATAL;
		archive_string_empty(&shar->work);
	}
	if(n != 0) {
		memcpy(shar->outbuff, src, n);
		shar->outpos = n;
	}
	return (length);
}

static int archive_write_shar_finish_entry(struct archive_write * a)
{
	const char * g, * p, * u;
	int ret;
	struct shar * shar = (struct shar *)a->format_data;
	if(shar->entry == NULL)
		return 0;
	if(shar->dump) {
		/* Finish uuencoded data. */
		if(shar->has_data) {
			if(shar->outpos > 0)
				uuencode_line(a, shar, shar->outbuff,
				    shar->outpos);
			archive_strcat(&shar->work, "`\nend\n");
			archive_strcat(&shar->work, "SHAR_END\n");
		}
		/* Restore file mode, owner, flags. */
		/*
		 * TODO: Don't immediately restore mode for
		 * directories; defer that to end of script.
		 */
		archive_string_sprintf(&shar->work, "chmod %o ",
		    (uint)(archive_entry_mode(shar->entry) & 07777));
		shar_quote(&shar->work, archive_entry_pathname(shar->entry), 1);
		archive_strcat(&shar->work, "\n");

		u = archive_entry_uname(shar->entry);
		g = archive_entry_gname(shar->entry);
		if(u || g) {
			archive_strcat(&shar->work, "chown ");
			if(u)
				shar_quote(&shar->work, u, 1);
			if(g) {
				archive_strcat(&shar->work, ":");
				shar_quote(&shar->work, g, 1);
			}
			archive_strcat(&shar->work, " ");
			shar_quote(&shar->work,
			    archive_entry_pathname(shar->entry), 1);
			archive_strcat(&shar->work, "\n");
		}

		if((p = archive_entry_fflags_text(shar->entry)) != NULL) {
			archive_string_sprintf(&shar->work, "chflags %s ", p);
			shar_quote(&shar->work, archive_entry_pathname(shar->entry), 1);
			archive_strcat(&shar->work, "\n");
		}
		/* @todo restore ACLs */
	}
	else {
		if(shar->has_data) {
			/* Finish sed-encoded data:  ensure last line ends. */
			if(!shar->end_of_line)
				archive_strappend_char(&shar->work, '\n');
			archive_strcat(&shar->work, "SHAR_END\n");
		}
	}
	archive_entry_free(shar->entry);
	shar->entry = NULL;
	if(shar->work.length < 65536)
		return ARCHIVE_OK;
	ret = __archive_write_output(a, shar->work.s, shar->work.length);
	if(ret != ARCHIVE_OK)
		return ARCHIVE_FATAL;
	archive_string_empty(&shar->work);
	return ARCHIVE_OK;
}

static int archive_write_shar_close(struct archive_write * a)
{
	int ret;
	/*
	 * TODO: Accumulate list of directory names/modes and
	 * fix them all up at end-of-archive.
	 */
	struct shar * shar = (struct shar *)a->format_data;
	/*
	 * Only write the end-of-archive markers if the archive was
	 * actually started.  This avoids problems if someone sets
	 * shar format, then sets another format (which would invoke
	 * shar_finish to free the format-specific data).
	 */
	if(shar->wrote_header == 0)
		return ARCHIVE_OK;
	archive_strcat(&shar->work, "exit\n");
	ret = __archive_write_output(a, shar->work.s, shar->work.length);
	if(ret != ARCHIVE_OK)
		return ARCHIVE_FATAL;
	/* Shar output is never padded. */
	archive_write_set_bytes_in_last_block(&a->archive, 1);
	/*
	 * TODO: shar should also suppress padding of
	 * uncompressed data within gzip/bzip2 streams.
	 */
	return ARCHIVE_OK;
}

static int archive_write_shar_free(struct archive_write * a)
{
	struct shar * shar = (struct shar *)a->format_data;
	if(shar == NULL)
		return ARCHIVE_OK;
	archive_entry_free(shar->entry);
	SAlloc::F(shar->last_dir);
	archive_string_free(&(shar->work));
	archive_string_free(&(shar->quoted_name));
	SAlloc::F(shar);
	a->format_data = NULL;
	return ARCHIVE_OK;
}
