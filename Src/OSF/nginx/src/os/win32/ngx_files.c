/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */
#include <ngx_config.h>
#include <ngx_core.h>
#pragma hdrstop

#define NGX_UTF16_BUFLEN  256

static ngx_int_t ngx_win32_check_filename(uchar * name, ushort * u, size_t len);
static ushort * ngx_utf8_to_utf16(ushort * utf16, const uchar * utf8, size_t * len);

/* FILE_FLAG_BACKUP_SEMANTICS allows to obtain a handle to a directory */

ngx_fd_t ngx_open_file(uchar * name, ulong mode, ulong create, ulong access)
{
	ngx_fd_t fd;
	ngx_err_t err;
	ushort utf16[NGX_UTF16_BUFLEN];
	size_t len = NGX_UTF16_BUFLEN;
	ushort * u = ngx_utf8_to_utf16(utf16, name, &len);
	if(u == NULL) {
		return INVALID_HANDLE_VALUE;
	}
	fd = INVALID_HANDLE_VALUE;
	if(create == NGX_FILE_OPEN && ngx_win32_check_filename(name, u, len) != NGX_OK) {
		goto failed;
	}
	fd = CreateFileW((LPCWSTR)u, mode, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, create, FILE_FLAG_BACKUP_SEMANTICS, NULL);
failed:
	if(u != utf16) {
		err = ngx_errno;
		SAlloc::F(u);
		ngx_set_errno(err);
	}
	return fd;
}

ssize_t ngx_read_file(ngx_file_t * file, uchar * buf, size_t size, nginx_off_t offset)
{
	ulong n;
	ngx_err_t err;
	OVERLAPPED ovlp, * povlp;
	ovlp.Internal = 0;
	ovlp.InternalHigh = 0;
	ovlp.Offset = (ulong)offset;
	ovlp.OffsetHigh = (ulong)(offset >> 32);
	ovlp.hEvent = NULL;
	povlp = &ovlp;
	if(ReadFile(file->fd, buf, size, &n, povlp) == 0) {
		err = ngx_errno;
		if(err == ERROR_HANDLE_EOF) {
			return 0;
		}
		ngx_log_error(NGX_LOG_ERR, file->log, err, "ReadFile() \"%s\" failed", file->name.data);
		return NGX_ERROR;
	}
	file->offset += n;
	return n;
}

ssize_t ngx_write_file(ngx_file_t * file, uchar * buf, size_t size, nginx_off_t offset)
{
	ulong n;
	OVERLAPPED ovlp, * povlp;
	ovlp.Internal = 0;
	ovlp.InternalHigh = 0;
	ovlp.Offset = (ulong)offset;
	ovlp.OffsetHigh = (ulong)(offset >> 32);
	ovlp.hEvent = NULL;
	povlp = &ovlp;
	if(WriteFile(file->fd, buf, size, &n, povlp) == 0) {
		ngx_log_error(NGX_LOG_ERR, file->log, ngx_errno, "WriteFile() \"%s\" failed", file->name.data);
		return NGX_ERROR;
	}
	if(n != size) {
		ngx_log_error(NGX_LOG_CRIT, file->log, 0, "WriteFile() \"%s\" has written only %ul of %uz", file->name.data, n, size);
		return NGX_ERROR;
	}
	file->offset += n;
	return n;
}

ssize_t ngx_write_chain_to_file(ngx_file_t * file, ngx_chain_t * cl, nginx_off_t offset, ngx_pool_t * pool)
{
	uchar * buf, * prev;
	size_t size;
	ssize_t n;
	ssize_t total = 0;
	while(cl) {
		buf = cl->buf->pos;
		prev = buf;
		size = 0;
		/* coalesce the neighbouring bufs */
		while(cl && prev == cl->buf->pos) {
			size += cl->buf->last - cl->buf->pos;
			prev = cl->buf->last;
			cl = cl->next;
		}
		n = ngx_write_file(file, buf, size, offset);
		if(n == NGX_ERROR) {
			return NGX_ERROR;
		}
		total += n;
		offset += n;
	}
	return total;
}

ssize_t ngx_read_fd(ngx_fd_t fd, void * buf, size_t size)
{
	ulong n;
	if(ReadFile(fd, buf, size, &n, NULL) != 0) {
		return (size_t)n;
	}
	return -1;
}

ssize_t ngx_write_fd(ngx_fd_t fd, const void * buf, size_t size)
{
	ulong n;
	if(WriteFile(fd, buf, size, &n, NULL) != 0) {
		return (size_t)n;
	}
	return -1;
}

ssize_t ngx_write_console(ngx_fd_t fd, const void * buf, size_t size)
{
	ulong n;
	(void)CharToOemBuffA((LPCSTR)buf, (LPSTR)buf, size); // @v10.3.11 CharToOemBuff-->CharToOemBuffA
	if(WriteFile(fd, buf, size, &n, NULL) != 0) {
		return (size_t)n;
	}
	return -1;
}

ngx_err_t ngx_win32_rename_file(ngx_str_t * from, ngx_str_t * to, ngx_log_t * log)
{
	ngx_err_t err;
	ngx_uint_t collision;
	ngx_atomic_uint_t num;
	uchar * name = static_cast<uchar *>(ngx_alloc(to->len + 1 + NGX_ATOMIC_T_LEN + 1 + sizeof("DELETE"), log));
	if(!name) {
		return NGX_ENOMEM;
	}
	memcpy(name, to->data, to->len);
	collision = 0;
	/* mutex_lock() (per cache or single ?) */
	for(;;) {
		num = ngx_next_temp_number(collision);
		ngx_sprintf(name + to->len, ".%0muA.DELETE%Z", num);
		if(MoveFile(SUcSwitch(reinterpret_cast<const char *>(to->data)), SUcSwitch(reinterpret_cast<const char *>(name))) != 0) {
			break;
		}
		collision = 1;
		ngx_log_error(NGX_LOG_CRIT, log, ngx_errno, "MoveFile() \"%s\" to \"%s\" failed", to->data, name);
	}
	if(MoveFile(SUcSwitch(reinterpret_cast<const char *>(from->data)), SUcSwitch(reinterpret_cast<const char *>(to->data))) == 0) {
		err = ngx_errno;
	}
	else {
		err = 0;
	}
	if(DeleteFile(SUcSwitch(reinterpret_cast<const char *>(name))) == 0) {
		ngx_log_error(NGX_LOG_CRIT, log, ngx_errno, "DeleteFile() \"%s\" failed", name);
	}
	/* mutex_unlock() */
	SAlloc::F(name);
	return err;
}

ngx_int_t ngx_file_info(uchar * file, ngx_file_info_t * sb)
{
	long rc;
	ngx_err_t err;
	WIN32_FILE_ATTRIBUTE_DATA fa;
	ushort utf16[NGX_UTF16_BUFLEN];
	size_t len = NGX_UTF16_BUFLEN;
	ushort  * u = ngx_utf8_to_utf16(utf16, file, &len);
	if(u == NULL) {
		return NGX_FILE_ERROR;
	}
	rc = NGX_FILE_ERROR;
	if(ngx_win32_check_filename(file, u, len) != NGX_OK) {
		goto failed;
	}
	rc = GetFileAttributesExW((LPCWSTR)u, GetFileExInfoStandard, &fa);
	sb->dwFileAttributes = fa.dwFileAttributes;
	sb->ftCreationTime = fa.ftCreationTime;
	sb->ftLastAccessTime = fa.ftLastAccessTime;
	sb->ftLastWriteTime = fa.ftLastWriteTime;
	sb->nFileSizeHigh = fa.nFileSizeHigh;
	sb->nFileSizeLow = fa.nFileSizeLow;
failed:
	if(u != utf16) {
		err = ngx_errno;
		SAlloc::F(u);
		ngx_set_errno(err);
	}
	return rc;
}

ngx_int_t ngx_set_file_time(uchar * name, ngx_fd_t fd, time_t s)
{
	FILETIME ft;
	/* 116444736000000000 is commented in src/os/win32/ngx_time.c */
	const uint64_t intervals = s * 10000000 + /*116444736000000000*/SlConst::Epoch1600_1970_Offs_100Ns;
	ft.dwLowDateTime = (DWORD)intervals;
	ft.dwHighDateTime = (DWORD)(intervals >> 32);
	if(SetFileTime(fd, NULL, NULL, &ft) != 0) {
		return NGX_OK;
	}
	return NGX_ERROR;
}

ngx_int_t ngx_create_file_mapping(ngx_file_mapping_t * fm)
{
	LARGE_INTEGER size;
	fm->fd = ngx_open_file(fm->name, NGX_FILE_RDWR, NGX_FILE_TRUNCATE, NGX_FILE_DEFAULT_ACCESS);
	if(fm->fd == NGX_INVALID_FILE) {
		ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno, ngx_open_file_n " \"%s\" failed", fm->name);
		return NGX_ERROR;
	}
	fm->handle = NULL;
	size.QuadPart = fm->size;
	if(SetFilePointerEx(fm->fd, size, NULL, FILE_BEGIN) == 0) {
		ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno, "SetFilePointerEx(\"%s\", %uz) failed", fm->name, fm->size);
		goto failed;
	}
	if(SetEndOfFile(fm->fd) == 0) {
		ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno, "SetEndOfFile() \"%s\" failed", fm->name);
		goto failed;
	}
	fm->handle = CreateFileMapping(fm->fd, NULL, PAGE_READWRITE, (ulong)((nginx_off_t)fm->size >> 32),
	    (ulong)((nginx_off_t)fm->size & 0xffffffff), NULL);
	if(fm->handle == NULL) {
		ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno, "CreateFileMapping(%s, %uz) failed", fm->name, fm->size);
		goto failed;
	}
	fm->addr = MapViewOfFile(fm->handle, FILE_MAP_WRITE, 0, 0, 0);
	if(fm->addr) {
		return NGX_OK;
	}
	ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno, "MapViewOfFile(%uz) of file mapping \"%s\" failed", fm->size, fm->name);
failed:
	if(fm->handle) {
		if(CloseHandle(fm->handle) == 0) {
			ngx_log_error(NGX_LOG_ALERT, fm->log, ngx_errno, "CloseHandle() of file mapping \"%s\" failed", fm->name);
		}
	}
	if(ngx_close_file(fm->fd) == NGX_FILE_ERROR) {
		ngx_log_error(NGX_LOG_ALERT, fm->log, ngx_errno, ngx_close_file_n " \"%s\" failed", fm->name);
	}
	return NGX_ERROR;
}

void ngx_close_file_mapping(ngx_file_mapping_t * fm)
{
	if(UnmapViewOfFile(fm->addr) == 0) {
		ngx_log_error(NGX_LOG_ALERT, fm->log, ngx_errno, "UnmapViewOfFile(%p) of file mapping \"%s\" failed", fm->addr, &fm->name);
	}
	if(CloseHandle(fm->handle) == 0) {
		ngx_log_error(NGX_LOG_ALERT, fm->log, ngx_errno, "CloseHandle() of file mapping \"%s\" failed", &fm->name);
	}
	if(ngx_close_file(fm->fd) == NGX_FILE_ERROR) {
		ngx_log_error(NGX_LOG_ALERT, fm->log, ngx_errno, ngx_close_file_n " \"%s\" failed", fm->name);
	}
}

uchar * ngx_realpath(uchar * path, uchar * resolved)
{
	/* STUB */
	return path;
}

ngx_int_t ngx_open_dir(ngx_str_t * name, ngx_dir_t * dir)
{
	ngx_cpystrn(name->data + name->len, NGX_DIR_MASK, NGX_DIR_MASK_LEN + 1);
	dir->dir = FindFirstFile(SUcSwitch(reinterpret_cast<const char *>(name->data)), &dir->finddata);
	name->data[name->len] = '\0';
	if(dir->dir == INVALID_HANDLE_VALUE) {
		return NGX_ERROR;
	}
	dir->valid_info = 1;
	dir->ready = 1;
	return NGX_OK;
}

ngx_int_t ngx_read_dir(ngx_dir_t * dir)
{
	if(dir->ready) {
		dir->ready = 0;
		return NGX_OK;
	}
	if(FindNextFile(dir->dir, &dir->finddata) != 0) {
		dir->type = 1;
		return NGX_OK;
	}
	return NGX_ERROR;
}

ngx_int_t ngx_close_dir(ngx_dir_t * dir)
{
	return (FindClose(dir->dir) == 0) ? NGX_ERROR : NGX_OK;
}

ngx_int_t ngx_open_glob(ngx_glob_t * gl)
{
	uchar   * p;
	size_t len;
	ngx_err_t err;
	gl->dir = FindFirstFile(SUcSwitch(reinterpret_cast<const char *>(gl->pattern)), &gl->finddata);
	if(gl->dir == INVALID_HANDLE_VALUE) {
		err = ngx_errno;
		if((err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) && gl->test) {
			gl->no_match = 1;
			return NGX_OK;
		}
		return NGX_ERROR;
	}
	for(p = gl->pattern; *p; p++) {
		if(*p == '/') {
			gl->last = p + 1 - gl->pattern;
		}
	}
	len = ngx_strlen(gl->finddata.cFileName);
	gl->name.len = gl->last + len;
	gl->name.data = (uchar *)ngx_alloc(gl->name.len + 1, gl->log);
	if(gl->name.data == NULL) {
		return NGX_ERROR;
	}
	memcpy(gl->name.data, gl->pattern, gl->last);
	ngx_cpystrn(gl->name.data + gl->last, (uchar *)gl->finddata.cFileName, len + 1);
	gl->ready = 1;
	return NGX_OK;
}

ngx_int_t ngx_read_glob(ngx_glob_t * gl, ngx_str_t * name)
{
	size_t len;
	ngx_err_t err;
	if(gl->no_match) {
		return NGX_DONE;
	}
	if(gl->ready) {
		*name = gl->name;
		gl->ready = 0;
		return NGX_OK;
	}
	SAlloc::F(gl->name.data);
	gl->name.data = NULL;
	if(FindNextFile(gl->dir, &gl->finddata) != 0) {
		len = ngx_strlen(gl->finddata.cFileName);
		gl->name.len = gl->last + len;
		gl->name.data = (uchar *)ngx_alloc(gl->name.len + 1, gl->log);
		if(gl->name.data == NULL) {
			return NGX_ERROR;
		}
		memcpy(gl->name.data, gl->pattern, gl->last);
		ngx_cpystrn(gl->name.data + gl->last, (uchar *)gl->finddata.cFileName, len + 1);
		*name = gl->name;
		return NGX_OK;
	}
	err = ngx_errno;
	if(err == NGX_ENOMOREFILES) {
		return NGX_DONE;
	}
	ngx_log_error(NGX_LOG_ALERT, gl->log, err, "FindNextFile(%s) failed", gl->pattern);
	return NGX_ERROR;
}

void ngx_close_glob(ngx_glob_t * gl)
{
	if(gl->name.data) {
		SAlloc::F(gl->name.data);
	}
	if(gl->dir == INVALID_HANDLE_VALUE) {
		return;
	}
	if(FindClose(gl->dir) == 0) {
		ngx_log_error(NGX_LOG_ALERT, gl->log, ngx_errno, "FindClose(%s) failed", gl->pattern);
	}
}

ngx_int_t ngx_de_info(uchar * name, ngx_dir_t * dir) { return NGX_OK; }
ngx_int_t ngx_de_link_info(uchar * name, ngx_dir_t * dir) { return NGX_OK; }
ngx_int_t ngx_read_ahead(ngx_fd_t fd, size_t n) { return ~NGX_FILE_ERROR; }
ngx_int_t ngx_directio_on(ngx_fd_t fd) { return ~NGX_FILE_ERROR; }
ngx_int_t ngx_directio_off(ngx_fd_t fd) { return ~NGX_FILE_ERROR; }

size_t ngx_fs_bsize(uchar * name)
{
	uchar root[4];
	ulong sc, bs, nfree, ncl;
	if(name[2] == ':') {
		ngx_cpystrn(root, name, 4);
		name = root;
	}
	if(GetDiskFreeSpace(SUcSwitch(reinterpret_cast<const char *>(name)), &sc, &bs, &nfree, &ncl) == 0) {
		return 512;
	}
	return sc * bs;
}

static ngx_int_t ngx_win32_check_filename(uchar * name, ushort * u, size_t len)
{
	uchar * p, ch;
	ulong n;
	ushort * lu = 0;
	ngx_err_t err;
	enum {
		sw_start = 0,
		sw_normal,
		sw_after_slash,
		sw_after_colon,
		sw_after_dot
	} state;
	// check for NTFS streams (":"), trailing dots and spaces
	state = sw_start;
	for(p = name; *p; p++) {
		ch = *p;
		switch(state) {
			case sw_start:
			    // 
			    // skip till first "/" to allow paths starting with drive and relative path, like "c:html/"
			    // 
			    if(isdirslash(ch)) {
				    state = sw_after_slash;
			    }
			    break;
			case sw_normal:
			    if(ch == ':') {
				    state = sw_after_colon;
				    break;
			    }
			    if(ch == '.' || ch == ' ') {
				    state = sw_after_dot;
				    break;
			    }
			    if(isdirslash(ch)) {
				    state = sw_after_slash;
				    break;
			    }
			    break;
			case sw_after_slash:
			    if(isdirslash(ch)) {
				    break;
			    }
			    if(ch == '.') {
				    break;
			    }
			    if(ch == ':') {
				    state = sw_after_colon;
				    break;
			    }
			    state = sw_normal;
			    break;
			case sw_after_colon:
			    if(isdirslash(ch)) {
				    state = sw_after_slash;
				    break;
			    }
			    goto invalid;
			case sw_after_dot:
			    if(isdirslash(ch)) {
				    goto invalid;
			    }
			    if(ch == ':') {
				    goto invalid;
			    }
			    if(ch == '.' || ch == ' ') {
				    break;
			    }
			    state = sw_normal;
			    break;
		}
	}
	if(state == sw_after_dot) {
		goto invalid;
	}
	// check if long name match
	lu = (ushort *)SAlloc::M(len * 2);
	if(lu == NULL) {
		return NGX_ERROR;
	}
	n = GetLongPathNameW((LPCWSTR)u, (LPWSTR)lu, len);
	if(n == 0) {
		goto failed;
	}
	if(n != len - 1 || _wcsicmp((const wchar_t *)u, (const wchar_t *)lu) != 0) {
		goto invalid;
	}
	SAlloc::F(lu);
	return NGX_OK;
invalid:
	ngx_set_errno(NGX_ENOENT);
failed:
	if(lu) {
		err = ngx_errno;
		SAlloc::F(lu);
		ngx_set_errno(err);
	}
	return NGX_ERROR;
}

static ushort * ngx_utf8_to_utf16(ushort * utf16, const uchar * utf8, size_t * len)
{
	uint32_t n;
	const uchar * p = utf8;
	ushort * u = utf16;
	ushort * last = utf16 + *len;
	while(u < last) {
		if(*p < 0x80) {
			*u++ = (ushort)*p;
			if(*p == 0) {
				*len = u - utf16;
				return utf16;
			}
			p++;
			continue;
		}
		if(u + 1 == last) {
			*len = u - utf16;
			break;
		}
		n = ngx_utf8_decode(&p, 4);
		if(n > 0x10ffff) {
			ngx_set_errno(NGX_EILSEQ);
			return NULL;
		}
		if(n > 0xffff) {
			n -= 0x10000;
			*u++ = (ushort)(0xd800 + (n >> 10));
			*u++ = (ushort)(0xdc00 + (n & 0x03ff));
			continue;
		}
		*u++ = (ushort)n;
	}
	// the given buffer is not enough, allocate a new one 
	u = (ushort *)SAlloc::M(((p - utf8) + ngx_strlen(p) + 1) * sizeof(ushort));
	if(u == NULL) {
		return NULL;
	}
	memcpy(u, utf16, *len * 2);
	utf16 = u;
	u += *len;
	for(;;) {
		if(*p < 0x80) {
			*u++ = (ushort) *p;
			if(*p == 0) {
				*len = u - utf16;
				return utf16;
			}
			p++;
			continue;
		}
		n = ngx_utf8_decode(&p, 4);
		if(n > 0x10ffff) {
			SAlloc::F(utf16);
			ngx_set_errno(NGX_EILSEQ);
			return NULL;
		}
		if(n > 0xffff) {
			n -= 0x10000;
			*u++ = (ushort)(0xd800 + (n >> 10));
			*u++ = (ushort)(0xdc00 + (n & 0x03ff));
			continue;
		}
		*u++ = (ushort)n;
	}
	/* unreachable */
}

