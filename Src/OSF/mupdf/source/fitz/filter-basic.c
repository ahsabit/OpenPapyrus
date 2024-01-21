//
//
#include "mupdf/fitz.h"
#pragma hdrstop

/* null filter */

struct null_filter {
	fz_stream * chain;
	size_t remain;
	int64_t offset;
	uchar buffer[4096];
};

static int next_null(fz_context * ctx, fz_stream * stm, size_t max)
{
	struct null_filter * state = (struct null_filter *)stm->state;
	size_t n;
	if(state->remain == 0)
		return EOF;
	fz_seek(ctx, state->chain, state->offset, 0);
	n = fz_available(ctx, state->chain, max);
	if(!n)
		return EOF;
	if(n > state->remain)
		n = state->remain;
	if(n > sizeof(state->buffer))
		n = sizeof(state->buffer);
	memcpy(state->buffer, state->chain->rp, n);
	stm->rp = state->buffer;
	stm->wp = stm->rp + n;
	state->chain->rp += n;
	state->remain -= n;
	state->offset += n;
	stm->pos += n;
	return *stm->rp++;
}

static void close_null(fz_context * ctx, void * state_)
{
	struct null_filter * state = (struct null_filter *)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream * fz_open_null_filter(fz_context * ctx, fz_stream * chain, int len, int64_t offset)
{
	struct null_filter * state = fz_malloc_struct(ctx, struct null_filter);
	state->chain = fz_keep_stream(ctx, chain);
	state->remain = len;
	state->offset = offset;
	return fz_new_stream(ctx, state, next_null, close_null);
}

/* range filter */

struct range_filter {
	fz_stream * chain;
	fz_range * ranges;
	int nranges;
	int next_range;
	size_t remain;
	int64_t offset;
	uchar buffer[4096];
};

static int next_range(fz_context * ctx, fz_stream * stm, size_t max)
{
	struct range_filter * state = (struct range_filter *)stm->state;
	size_t n;
	while(state->remain == 0 && state->next_range < state->nranges) {
		fz_range * range = &state->ranges[state->next_range++];
		state->remain = range->length;
		state->offset = range->offset;
	}

	if(state->remain == 0)
		return EOF;
	fz_seek(ctx, state->chain, state->offset, 0);
	n = fz_available(ctx, state->chain, max);
	if(n > state->remain)
		n = state->remain;
	if(n > sizeof(state->buffer))
		n = sizeof(state->buffer);
	memcpy(state->buffer, state->chain->rp, n);
	stm->rp = state->buffer;
	stm->wp = stm->rp + n;
	if(!n)
		return EOF;
	state->chain->rp += n;
	state->remain -= n;
	state->offset += n;
	stm->pos += n;
	return *stm->rp++;
}

static void close_range(fz_context * ctx, void * state_)
{
	struct range_filter * state = (struct range_filter *)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state->ranges);
	fz_free(ctx, state);
}

fz_stream * fz_open_range_filter(fz_context * ctx, fz_stream * chain, fz_range * ranges, int nranges)
{
	struct range_filter * state = NULL;

	state = fz_malloc_struct(ctx, struct range_filter);
	fz_try(ctx)
	{
		if(nranges > 0) {
			state->ranges = (fz_range *)fz_calloc(ctx, nranges, sizeof(*ranges));
			memcpy(state->ranges, ranges, nranges * sizeof(*ranges));
			state->nranges = nranges;
			state->next_range = 1;
			state->remain = ranges[0].length;
			state->offset = ranges[0].offset;
		}
		else {
			state->ranges = NULL;
			state->nranges = 0;
			state->next_range = 1;
			state->remain = 0;
			state->offset = 0;
		}
		state->chain = fz_keep_stream(ctx, chain);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state->ranges);
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_range, close_range);
}

/* endstream filter */

#define END_CHECK_SIZE 32

struct endstream_filter {
	fz_stream * chain;
	size_t remain, extras, size;
	int64_t offset;
	int warned;
	uchar buffer[4096];
};

static int next_endstream(fz_context * ctx, fz_stream * stm, size_t max)
{
	struct endstream_filter * state = (struct endstream_filter *)stm->state;
	size_t n, nbytes_in_buffer, size;
	uchar * rp;
	if(state->remain == 0)
		goto look_for_endstream;
	fz_seek(ctx, state->chain, state->offset, 0);
	n = fz_available(ctx, state->chain, max);
	if(!n)
		return EOF;
	if(n > state->remain)
		n = state->remain;
	if(n > sizeof(state->buffer))
		n = sizeof(state->buffer);
	memcpy(state->buffer, state->chain->rp, n);
	stm->rp = state->buffer;
	stm->wp = stm->rp + n;
	state->chain->rp += n;
	state->remain -= n;
	state->offset += n;
	stm->pos += n;
	return *stm->rp++;

look_for_endstream:
	/* We should distrust the stream length, and check for end
	 * marker before terminating the stream - this is to cope
	 * with files with duff "Length" values. */

	/* Move any data left over in our buffer down to the start.
	 * Ordinarily, there won't be any, but this allows for the
	 * case where we were part way through matching a stream end
	 * marker when the buffer filled before. */
	nbytes_in_buffer = state->extras;
	if(nbytes_in_buffer)
		memmove(state->buffer, stm->rp, nbytes_in_buffer);
	stm->rp = state->buffer;
	stm->wp = stm->rp + nbytes_in_buffer;

	/* In most sane files, we'll get "\nendstream" instantly. We
	 * should only need (say) 32 bytes to be sure. For crap files
	 * where we overread regularly, don't harm performance by
	 * working in small chunks. */
	size = state->size * 2;
	if(size > sizeof(state->buffer))
		size = sizeof(state->buffer);
	state->size = size;

	/* Read enough data into our buffer to start looking for the 'endstream' token. */
	fz_seek(ctx, state->chain, state->offset, 0);
	while(nbytes_in_buffer < size) {
		n = fz_available(ctx, state->chain, size - nbytes_in_buffer);
		if(!n)
			break;
		if(n > size - nbytes_in_buffer)
			n = size - nbytes_in_buffer;
		memcpy(stm->wp, state->chain->rp, n);
		stm->wp += n;
		state->chain->rp += n;
		nbytes_in_buffer += n;
		state->offset += n;
	}

	/* Look for the 'endstream' token. */
	rp = (uchar*)fz_memmem(state->buffer, nbytes_in_buffer, "endstream", 9);
	if(rp) {
		/* Include newline (CR|LF|CRLF) before 'endstream' token */
		if(rp > state->buffer && rp[-1] == '\n') --rp;
		if(rp > state->buffer && rp[-1] == '\r') --rp;
		n = rp - state->buffer;
		stm->eof = 1; /* We're done, don't call us again! */
	}
	else if(nbytes_in_buffer > 11)  /* 11 covers enough data to detect "\r?\n?endstream" */
		n = nbytes_in_buffer - 11; /* no endstream, but there is more data */
	else
		n = nbytes_in_buffer; /* no endstream, but at the end of the file */

	/* We have at least n bytes before we hit an end marker */
	state->extras = nbytes_in_buffer - n;
	stm->wp = stm->rp + n;
	stm->pos += n;

	if(!n)
		return EOF;

	if(!state->warned) {
		state->warned = 1;
		fz_warn(ctx, "PDF stream Length incorrect");
	}
	return *stm->rp++;
}

static void close_endstream(fz_context * ctx, void * state_)
{
	struct endstream_filter * state = (struct endstream_filter *)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream * fz_open_endstream_filter(fz_context * ctx, fz_stream * chain, int len, int64_t offset)
{
	struct endstream_filter * state;

	if(len < 0)
		len = 0;

	state = fz_malloc_struct(ctx, struct endstream_filter);
	state->chain = fz_keep_stream(ctx, chain);
	state->remain = len;
	state->offset = offset;
	state->extras = 0;
	state->size = END_CHECK_SIZE >> 1; /* size is doubled first thing when used */

	return fz_new_stream(ctx, state, next_endstream, close_endstream);
}

/* concat filter */

struct concat_filter {
	int max;
	int count;
	int current;
	int pad; /* 1 if we should add whitespace padding between streams */
	uchar ws_buf;
	fz_stream * chain[1];
};

static int next_concat(fz_context * ctx, fz_stream * stm, size_t max)
{
	struct concat_filter * state = (struct concat_filter *)stm->state;
	size_t n;

	while(state->current < state->count) {
		/* Read the next block of underlying data. */
		if(stm->wp == state->chain[state->current]->wp)
			state->chain[state->current]->rp = stm->wp;
		n = fz_available(ctx, state->chain[state->current], max);
		if(n) {
			stm->rp = state->chain[state->current]->rp;
			stm->wp = state->chain[state->current]->wp;
			stm->pos += n;
			return *stm->rp++;
		}
		else {
			if(state->chain[state->current]->error) {
				stm->error = 1;
				break;
			}
			state->current++;
			fz_drop_stream(ctx, state->chain[state->current-1]);
			if(state->pad) {
				stm->wp = stm->rp = (&state->ws_buf)+1;
				stm->pos++;
				return 32;
			}
		}
	}

	stm->rp = stm->wp;

	return EOF;
}

static void close_concat(fz_context * ctx, void * state_)
{
	struct concat_filter * state = (struct concat_filter *)state_;
	int i;

	for(i = state->current; i < state->count; i++) {
		fz_drop_stream(ctx, state->chain[i]);
	}
	fz_free(ctx, state);
}

fz_stream * fz_open_concat(fz_context * ctx, int len, int pad)
{
	struct concat_filter * state = (struct concat_filter *)fz_calloc(ctx, 1, sizeof(struct concat_filter) + (len-1)*sizeof(fz_stream *));
	state->max = len;
	state->count = 0;
	state->current = 0;
	state->pad = pad;
	state->ws_buf = 32;
	return fz_new_stream(ctx, state, next_concat, close_concat);
}

void fz_concat_push_drop(fz_context * ctx, fz_stream * concat, fz_stream * chain)
{
	struct concat_filter * state = (struct concat_filter *)concat->state;
	if(state->count == state->max) {
		fz_drop_stream(ctx, chain);
		fz_throw(ctx, FZ_ERROR_GENERIC, "Concat filter size exceeded");
	}

	state->chain[state->count++] = chain;
}

/* ASCII Hex Decode */

typedef struct {
	fz_stream * chain;
	int eod;
	uchar buffer[256];
} fz_ahxd;

static inline int iswhite(int a)
{
	switch(a) {
		case '\n': case '\r': case '\t': case ' ':
		case '\0': case '\f': case '\b': case 0177:
		    return 1;
	}
	return 0;
}

// @sobolev static inline int ishex(int a) { return (a >= 'A' && a <= 'F') || (a >= 'a' && a <= 'f') || (a >= '0' && a <= '9'); }

/* @sobolev (replaced with hex()) static inline int unhex(int a)
{
	if(a >= 'A' && a <= 'F') 
		return a - 'A' + 0xA;
	if(a >= 'a' && a <= 'f') 
		return a - 'a' + 0xA;
	if(a >= '0' && a <= '9') 
		return a - '0';
	return 0;
}*/

static int next_ahxd(fz_context * ctx, fz_stream * stm, size_t max)
{
	fz_ahxd * state = (fz_ahxd *)stm->state;
	uchar * p = state->buffer;
	uchar * ep;
	int a, b, c, odd;
	if(max > sizeof(state->buffer))
		max = sizeof(state->buffer);
	ep = p + max;
	odd = 0;
	while(p < ep) {
		if(state->eod)
			break;
		c = fz_read_byte(ctx, state->chain);
		if(c < 0)
			break;
		if(ishex(c)) {
			if(!odd) {
				a = /*unhex*/hex(c);
				odd = 1;
			}
			else {
				b = /*unhex*/hex(c);
				*p++ = (a << 4) | b;
				odd = 0;
			}
		}
		else if(c == '>') {
			if(odd)
				*p++ = (a << 4);
			state->eod = 1;
			break;
		}
		else if(!iswhite(c)) {
			fz_throw(ctx, FZ_ERROR_GENERIC, "bad data in ahxd: '%c'", c);
		}
	}
	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if(stm->rp != p)
		return *stm->rp++;
	return EOF;
}

static void close_ahxd(fz_context * ctx, void * state_)
{
	fz_ahxd * state = (fz_ahxd*)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream * fz_open_ahxd(fz_context * ctx, fz_stream * chain)
{
	fz_ahxd * state = fz_malloc_struct(ctx, fz_ahxd);
	state->chain = fz_keep_stream(ctx, chain);
	state->eod = 0;
	return fz_new_stream(ctx, state, next_ahxd, close_ahxd);
}

/* ASCII 85 Decode */

typedef struct {
	fz_stream * chain;
	uchar buffer[256];
	int eod;
} fz_a85d;

static int next_a85d(fz_context * ctx, fz_stream * stm, size_t max)
{
	fz_a85d * state = (fz_a85d *)stm->state;
	uchar * p = state->buffer;
	uchar * ep;
	int count = 0;
	int word = 0;
	int c;
	if(state->eod)
		return EOF;
	if(max > sizeof(state->buffer))
		max = sizeof(state->buffer);
	ep = p + max;
	while(p < ep) {
		c = fz_read_byte(ctx, state->chain);
		if(c < 0)
			break;

		if(c >= '!' && c <= 'u') {
			if(count == 4) {
				word = word * 85 + (c - '!');

				*p++ = (word >> 24) & 0xff;
				*p++ = (word >> 16) & 0xff;
				*p++ = (word >> 8) & 0xff;
				*p++ = (word) & 0xff;

				word = 0;
				count = 0;
			}
			else {
				word = word * 85 + (c - '!');
				count++;
			}
		}

		else if(c == 'z' && count == 0) {
			*p++ = 0;
			*p++ = 0;
			*p++ = 0;
			*p++ = 0;
		}

		else if(c == '~') {
			c = fz_read_byte(ctx, state->chain);
			if(c != '>')
				fz_warn(ctx, "bad eod marker in a85d");

			switch(count) {
				case 0:
				    break;
				case 1:
				    /* Specifically illegal in the spec, but adobe
				     * and gs both cope. See normal_87.pdf for a
				     * case where this matters. */
				    fz_warn(ctx, "partial final byte in a85d");
				    break;
				case 2:
				    word = word * (85 * 85 * 85) + 0xffffff;
				    *p++ = word >> 24;
				    break;
				case 3:
				    word = word * (85 * 85) + 0xffff;
				    *p++ = word >> 24;
				    *p++ = word >> 16;
				    break;
				case 4:
				    word = word * 85 + 0xff;
				    *p++ = word >> 24;
				    *p++ = word >> 16;
				    *p++ = word >> 8;
				    break;
			}
			state->eod = 1;
			break;
		}

		else if(!iswhite(c)) {
			fz_throw(ctx, FZ_ERROR_GENERIC, "bad data in a85d: '%c'", c);
		}
	}

	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if(p == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void close_a85d(fz_context * ctx, void * state_)
{
	fz_a85d * state = (fz_a85d*)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream * fz_open_a85d(fz_context * ctx, fz_stream * chain)
{
	fz_a85d * state = fz_malloc_struct(ctx, fz_a85d);
	state->chain = fz_keep_stream(ctx, chain);
	state->eod = 0;
	return fz_new_stream(ctx, state, next_a85d, close_a85d);
}

/* Run Length Decode */

typedef struct {
	fz_stream * chain;
	int run, n, c;
	uchar buffer[256];
} fz_rld;

static int next_rld(fz_context * ctx, fz_stream * stm, size_t max)
{
	fz_rld * state = (fz_rld *)stm->state;
	uchar * p = state->buffer;
	uchar * ep;
	if(state->run == 128)
		return EOF;
	if(max > sizeof(state->buffer))
		max = sizeof(state->buffer);
	ep = p + max;
	while(p < ep) {
		if(state->run == 128)
			break;

		if(state->n == 0) {
			state->run = fz_read_byte(ctx, state->chain);
			if(state->run < 0) {
				state->run = 128;
				break;
			}
			if(state->run < 128)
				state->n = state->run + 1;
			if(state->run > 128) {
				state->n = 257 - state->run;
				state->c = fz_read_byte(ctx, state->chain);
				if(state->c < 0)
					fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
			}
		}

		if(state->run < 128) {
			while(p < ep && state->n) {
				int c = fz_read_byte(ctx, state->chain);
				if(c < 0)
					fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
				*p++ = c;
				state->n--;
			}
		}

		if(state->run > 128) {
			while(p < ep && state->n) {
				*p++ = state->c;
				state->n--;
			}
		}
	}

	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if(p == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void close_rld(fz_context * ctx, void * state_)
{
	fz_rld * state = (fz_rld*)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream * fz_open_rld(fz_context * ctx, fz_stream * chain)
{
	fz_rld * state = fz_malloc_struct(ctx, fz_rld);
	state->chain = fz_keep_stream(ctx, chain);
	state->run = 0;
	state->n = 0;
	state->c = 0;
	return fz_new_stream(ctx, state, next_rld, close_rld);
}

/* RC4 Filter */

typedef struct {
	fz_stream * chain;
	fz_arc4 arc4;
	uchar buffer[256];
} fz_arc4c;

static int next_arc4(fz_context * ctx, fz_stream * stm, size_t max)
{
	fz_arc4c * state = (fz_arc4c *)stm->state;
	size_t n = fz_available(ctx, state->chain, max);
	if(!n)
		return EOF;
	if(n > sizeof(state->buffer))
		n = sizeof(state->buffer);
	stm->rp = state->buffer;
	stm->wp = state->buffer + n;
	fz_arc4_encrypt(&state->arc4, stm->rp, state->chain->rp, n);
	state->chain->rp += n;
	stm->pos += n;

	return *stm->rp++;
}

static void close_arc4(fz_context * ctx, void * state_)
{
	fz_arc4c * state = (fz_arc4c*)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream * fz_open_arc4(fz_context * ctx, fz_stream * chain, uchar * key, unsigned keylen)
{
	fz_arc4c * state = fz_malloc_struct(ctx, fz_arc4c);
	state->chain = fz_keep_stream(ctx, chain);
	fz_arc4_init(&state->arc4, key, keylen);
	return fz_new_stream(ctx, state, next_arc4, close_arc4);
}

/* AES Filter */

typedef struct {
	fz_stream * chain;
	fz_aes aes;
	uchar iv[16];
	int ivcount;
	uchar bp[16];
	uchar * rp, * wp;
	uchar buffer[256];
} fz_aesd;

static int next_aesd(fz_context * ctx, fz_stream * stm, size_t max)
{
	fz_aesd * state = (fz_aesd *)stm->state;
	uchar * p = state->buffer;
	uchar * ep;
	if(max > sizeof(state->buffer))
		max = sizeof(state->buffer);
	ep = p + max;
	while(state->ivcount < 16) {
		int c = fz_read_byte(ctx, state->chain);
		if(c < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in aes filter");
		state->iv[state->ivcount++] = c;
	}

	while(state->rp < state->wp && p < ep)
		*p++ = *state->rp++;

	while(p < ep) {
		size_t n = fz_read(ctx, state->chain, state->bp, 16);
		if(!n)
			break;
		else if(n < 16)
			fz_throw(ctx, FZ_ERROR_GENERIC, "partial block in aes filter");

		fz_aes_crypt_cbc(&state->aes, FZ_AES_DECRYPT, 16, state->iv, state->bp, state->bp);
		state->rp = state->bp;
		state->wp = state->bp + 16;

		/* strip padding at end of file */
		if(fz_is_eof(ctx, state->chain)) {
			int pad = state->bp[15];
			if(pad < 1 || pad > 16)
				fz_throw(ctx, FZ_ERROR_GENERIC, "aes padding out of range: %d", pad);
			state->wp -= pad;
		}

		while(state->rp < state->wp && p < ep)
			*p++ = *state->rp++;
	}

	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if(p == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void close_aesd(fz_context * ctx, void * state_)
{
	fz_aesd * state = (fz_aesd*)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream * fz_open_aesd(fz_context * ctx, fz_stream * chain, uchar * key, unsigned keylen)
{
	fz_aesd * state = fz_malloc_struct(ctx, fz_aesd);
	if(fz_aes_setkey_dec(&state->aes, key, keylen * 8)) {
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=%d)", keylen * 8);
	}
	state->ivcount = 0;
	state->rp = state->bp;
	state->wp = state->bp;
	state->chain = fz_keep_stream(ctx, chain);
	return fz_new_stream(ctx, state, next_aesd, close_aesd);
}
