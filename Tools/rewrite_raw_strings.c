/*
  retest.c - TRE regression test program

  This software is released under a BSD-style license.
  See the file LICENSE for details and copyright.

*/

/*
   This is just a simple converter to rewrite a (test) C source file to prevent
   clashes and/or other mishaps due to mixed codepages in the string content.

   This is designed to work on retest.c and assumes you have a function
   called raw2str() available in there.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "monolithic_examples.h"


#if 0

// raw to string converter used to circumvent the various codepage issues/strings we're using in these tests.
static const char *
raw2str(unsigned int c0, ...)
{
	// non-threadsafe, only usable for the tests below. the array size is a rough guess.
	static char buf[2048];
	unsigned int i;
	va_list ap;

	va_start(ap, c0);

	buf[0] = c0;

	unsigned int c = ~0;
	for (i = 1; c != 0 && i < sizeof(buf) / sizeof(buf[0]); i++)
	{
		c = va_arg(ap, unsigned int);

		buf[i] = c;
	}
	if (c != 0)
	{
		fprintf(stderr, "FATAL: raw string too large for static buffer: redimension buf[] and recompile!\n");
		exit(9);
	}
	va_end(ap);
	return buf;
}

#endif


static const char* seek_eocc(const char* s)
{
	for (; *s; s++) {
		switch (*s) {
		case '\\':
			// skip next escaped char
			s++;
			continue;

		case '\'':
			return s;

		case '\n':
			return NULL;

		default:
			continue;
		}
	}
	return NULL;
}


// Note: we don't support otherwise-legal multi-segment strings, e.g.:
//
//     str = "segment-1"  "segment-2"   "Segment-3";
//
static const char* seek_eos(const char* s)
{
	for (; *s; s++) {
		switch (*s) {
		case '\\':
			// skip next escaped char or line-continuation
			if (s[1] == '\r' && s[2] == '\n')
				s += 2;
			else
				s++;
			continue;

		case '"':
			return s;

		case '\n':
			return NULL;

		default:
			continue;
		}
	}
	return NULL;
}


static int is_ignorable_string(const char* s, const char* se) {
	for (; s < se; s++) {
		int c = *s;
		if (c >= 32 && c < 127)
			continue;
		return 0;
	}
	return 1;
}

static int is_rewritable_ascii_string(const char* s, const char* se) {
	for (; s < se; s++) {
		int c = *s;
		if (c >= 32 && c < 127)
			continue;
		switch (c) {
		case 9:
		case 10:
		case 13:
			continue;

		default:
			return 0;
		}
	}
	return 1;
}

static int rewrite_ascii_string(const char* s, const char* se, char* d, const char* const d_end) {
	*d = 0;

	size_t len = se - s;
	// fast initial heuristic:
	if (d + 10 + len >= d_end)
		return 1;
	if (*s != '"')
		return 1;
	if (*se != '"')
		return 1;

	*d++ = *s++;
	// go and process the string content:

	while (s < se) {
		switch (*s) {
		case 9:
			*d++ = '\\';
			*d++ = 't';
			s++;
			continue;

		case 10:
			*d++ = '\\';
			*d++ = 'n';
			s++;
			continue;

		case 13:
			*d++ = '\\';
			*d++ = 'r';
			s++;
			continue;

		default:
			*d++ = *s++;
			continue;
		}
	}
	*d++ = '"';
	*d = 0;
	return 0;
}


static int rewrite_raw_string(const char* s, const char* se, char* d, const char* const d_end) {
	*d = 0;

	if (is_rewritable_ascii_string(s, se)) {
		return rewrite_ascii_string(s, se, d, d_end);
	}

	size_t len = se - s;
	// fast initial heuristic:
	if (d + 20 + len * 4 >= d_end)
		return 1;
	if (*s != '"')
		return 1;
	if (*se != '"')
		return 1;

	strcpy(d, "raw2str(");
	d += strlen(d);
	s++;
	// go and process the string content bytes:

	const uint8_t* p = (const uint8_t *)s;
	const uint8_t* pe = (const uint8_t*)se;

	while (p < pe) {
		unsigned int c = *p++;

		if (c == '\'') {
			if (d + 7 >= d_end)
				return 1;
			*d++ = '\'';
			*d++ = '\\';
			*d++ = c;
			*d++ = '\'';
			*d++ = ',';
			*d++ = ' ';
			*d = 0;
			continue;
		}
		if (c >= 32 && c < 127) {
			if (d + 6 >= d_end)
				return 1;
			*d++ = '\'';
			*d++ = c;
			*d++ = '\'';
			*d++ = ',';
			*d++ = ' ';
			*d = 0;
			continue;
		}
		if (d + 6 >= d_end)
			return 1;
		sprintf(d, "0x%02X, ", c);
		d += strlen(d);
	}
	if (d + 3 >= d_end)
		return 1;
	strcpy(d, "0)");
	return 0;
}



#if defined(BUILD_MONOLITHIC)
#define main         tre_rewrite_raw_strings_util_main
#endif

int
main(int argc, const char **argv)
{
	if (argc != 2) {
		fprintf(stderr,
			"rewrite_raw_strings FILE\n"
			"\n"
			"Rewrites any constant C strings in there which carry non-ASCII characters\n"
			"into calls to a user-defined raw2str() function, which is fed a variable\n"
			"length array of (hexadecimal) 'byte codes'.\n"
			"This prevents clashes and spurious mishaps at compile-time or run-time of\n"
			"the processed source file.\n"
			"\n"
			"Note: specifying '-' as the FILE name instructs this tool to use\n"
			"the stdin/stdout pipeline instead.\n"
		);
		return 1;
	}
	const char* fpath = argv[1];
	FILE* in;
	FILE* out;
	int using_stdio = (strcmp(fpath, "-") == 0);
	if (using_stdio) {
		in = stdin;
		out = stdout;
	}
	else {
		in = fopen(fpath, "r+b");
		if (!in) {
			fprintf(stderr, "Could not open {%s} for input+output, quitting.\n", fpath);
			return 1;
		}
		out = in;
	}

	int rv = 1;

	// first load the entire source file in memory:
	fseek(in, 0, SEEK_END);
	size_t flen = ftell(in);
	char* dst = NULL;
	char* src = calloc(flen + 16, 1);
	if (!src) {
		fprintf(stderr, "Heap allocation error while allocating input buffer @ %zu bytes, quitting.\n", flen + 16);
		goto ende;
	}
	// very rough estimate for needed output buffer:
	size_t dstsize = flen * 4 + 16;
	dst = malloc(dstsize);
	if (!dst) {
		fprintf(stderr, "Heap allocation error while allocating output buffer @ %zu bytes, quitting.\n", flen + 16);
		goto ende;
	}

	fseek(in, 0, SEEK_SET);
	size_t rl = fread(src, 1, flen, in);
	if (rl != flen || ferror(in)) {
		fprintf(stderr, "Error while reading the source FILE (%zu bytes), quitting.\n", flen);
		goto ende;
	}

	// now run a rough C parser + string rewriter on src[], writing into dst[]
	const char* s = src;
	char* d = dst;
	char* d_end = dst + dstsize - 2;

	const char* se;

	while (*s) {
		// carbon-copy anything that's not a C string, start-of-comment or end-of-line:
		size_t pos = strcspn(s, "\"'\n\\/");
		if (pos > 0) {
			if (d + pos >= d_end)
				goto out_of_bounds;
			memcpy(d, s, pos);
			s += pos;
			d += pos;
		}
		switch (*s) {
		case 0:
			continue;

		default:
			fprintf(stderr, "Internal error while processing file (offset: %zu bytes), quitting.\n", (size_t)(s - src));
			goto ende;

		case '\n':
			if (d + 1 >= d_end)
				goto out_of_bounds;
			*d++ = *s++;
			continue;

		case '/':
			if (s[1] == '/') {
				// in C++ comment: skip till EOL
				pos = strcspn(s, "\n");
				if (d + pos >= d_end)
					goto out_of_bounds;
				memcpy(d, s, pos);
				s += pos;
				d += pos;
				continue;
			}
			if (s[1] == '*') {
				// in C comment; skip till end marker.
				pos = strcspn(s + 2, "*") + 2;
				for (;;) {
					if (pos > 0) {
						if (d + pos >= d_end)
							goto out_of_bounds;
						memcpy(d, s, pos);
						s += pos;
						d += pos;
					}
					if (s[1] == '/') {
						if (d + 2 >= d_end)
							goto out_of_bounds;
						memcpy(d, s, 2);
						s += 2;
						d += 2;
						break;
					}
					if (d + 1 >= d_end)
						goto out_of_bounds;
					*d++ = *s++;
					pos = strcspn(s, "*");
				}
				continue;
			}
			// else: something else: carbon-copy and continue
			if (d + 1 >= d_end)
				goto out_of_bounds;
			*d++ = *s++;
			continue;

		case '\\':
			// escapes next char; carbon-copy
			if (d + 2 >= d_end)
				goto out_of_bounds;
			*d++ = *s++;
			*d++ = *s++;
			continue;

		case '\'':
			// C char? if it is, carbon-copy.
			se = seek_eocc(s + 1);
			if (se && se - s <= 8 /* blunt heuristic */) {
				pos = se - s + 1;
				if (d + pos >= d_end)
					goto out_of_bounds;
				memcpy(d, s, pos);
				s += pos;
				d += pos;
				continue;
			}
			// else: something else: carbon-copy and continue
			if (d + 1 >= d_end)
				goto out_of_bounds;
			*d++ = *s++;
			continue;

		case '"':
			// we've hit a C string, or so we believe...
			//
			// see if this is one we should care about.
			se = seek_eos(s + 1);

			if (se) {
				if (is_ignorable_string(s, se)) {
					pos = se - s + 1;
					if (d + pos >= d_end)
						goto out_of_bounds;
					memcpy(d, s, pos);
					s += pos;
					d += pos;
					continue;
				}
				// else: we need to convert this one!
				if (rewrite_raw_string(s, se, d, d_end))
					goto out_of_bounds;
				s = se + 1;
				d += strlen(d);
				continue;
			}
			// else: something else: carbon-copy and continue
			if (d + 1 >= d_end)
				goto out_of_bounds;
			*d++ = *s++;
			continue;


			pos = strcspn(s + 1, "\"") + 1;
			if (pos > 0) {
				if (d + pos >= d_end)
					goto out_of_bounds;
				memcpy(d, s, pos);
				s += pos;
				d += pos;
			}
		}
	}
	*d = 0;

	// successful conversion! now rewrite file
	fseek(out, 0, SEEK_SET);
	size_t dwlen = strlen(dst);
	size_t wl = fwrite(dst, 1, dwlen, out);
	if (wl != dwlen || ferror(out)) {
		fprintf(stderr, "Error while rewriting the source file, quitting.\n");

		// attempt to rewrite original data to recover write error above...
		fseek(out, 0, SEEK_SET);
		dwlen = strlen(src);
		fwrite(src, 1, dwlen, out);

		goto ende;
	}

	rv = 0;

	if (0) {
out_of_bounds:
		fprintf(stderr, "Error running out of output buffer space while processing source FILE (%zu bytes), quitting.\n", flen);
		goto ende;
	}

ende:
	if (NULL != out && stdout != out) {
		fclose(out);
		out = NULL;
		in = NULL;
	}
	free(src);
	free(dst);
	return rv;
}
