/* $XTermId: charset.c,v 1.41 2013/01/10 00:47:28 tom Exp $ */

/*
Copyright 2010-2012,2013 by Thomas E. Dickey
Copyright (c) 2001 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <charset.h>

#include <sys.h>
#include <parser.h>

static unsigned int
IdentityRecode(unsigned int n, const CharsetRec * self GCC_UNUSED)
{
    return n;
}

static int
NullReverse(unsigned int n GCC_UNUSED, const CharsetRec * self GCC_UNUSED)
{
    return -1;
}

static const CharsetRec Unknown94Charset =
{"Unknown (94)", T_94, 0, IdentityRecode, NullReverse, 0, 0, 0, 0, 0, 0};
static const CharsetRec Unknown96Charset =
{"Unknown (96)", T_96, 0, IdentityRecode, NullReverse, 0, 0, 0, 0, 0, 0};
static const CharsetRec Unknown9494Charset =
{"Unknown (94x94)", T_9494, 0, IdentityRecode, NullReverse, 0, 0, 0, 0, 0, 0};
static const CharsetRec Unknown9696Charset =
{"Unknown (96x96)", T_9696, 0, IdentityRecode, NullReverse, 0, 0, 0, 0, 0, 0};

typedef struct _FontencCharset {
    const char *name;
    int type;
    unsigned char final;
    const char *xlfd;
    unsigned shift;
    FontMapPtr mapping;
    FontMapReversePtr reverse;
} FontencCharsetRec, *FontencCharsetPtr;

/*
 * The "name" given is useful on the command-line.
 * The "xlfd" column is the name given in the X font-encoding package.
 */
/* *INDENT-OFF* */
static FontencCharsetRec fontencCharsets[] =
{
    {"ISO 646 (1973)", T_94,    '@', "iso646.1973-0",    0x00,   0, 0},
    {"ASCII",          T_94,    'B', "iso8859-1",        0x00,   0, 0},	/* bug */
    {"JIS X 0201:GL",  T_94,    'J', "jisx0201.1976-0",  0x00,   0, 0},
    {"JIS X 0201:GR",  T_94,    'I', "jisx0201.1976-0",  0x80,   0, 0},
    {"DEC Special",    T_94,    '0', "dec-special",      0x00,   0, 0},
    {"DEC Technical",  T_94,    '>', "dec-dectech",      0x00,   0, 0},

    {"ISO 8859-1",     T_96,    'A', "iso8859-1",        0x80,   0, 0},
    {"ISO 8859-2",     T_96,    'B', "iso8859-2",        0x80,   0, 0},
    {"ISO 8859-3",     T_96,    'C', "iso8859-3",        0x80,   0, 0},
    {"ISO 8859-4",     T_96,    'D', "iso8859-4",        0x80,   0, 0},
    {"ISO 8859-5",     T_96,    'L', "iso8859-5",        0x80,   0, 0},
    {"ISO 8859-6",     T_96,    'G', "iso8859-6",        0x80,   0, 0},
    {"ISO 8859-7",     T_96,    'F', "iso8859-7",        0x80,   0, 0},
    {"ISO 8859-8",     T_96,    'H', "iso8859-8",        0x80,   0, 0},
    {"ISO 8859-9",     T_96,    'M', "iso8859-9",        0x80,   0, 0},
    {"ISO 8859-10",    T_96,    'V', "iso8859-10",       0x80,   0, 0},
    {"ISO 8859-11",    T_96,    'T', "iso8859-11",       0x80,   0, 0},
    {"TIS 620",        T_96,    'T', "iso8859-11",       0x80,   0, 0},
    {"ISO 8859-13",    T_96,    'Y', "iso8859-13",       0x80,   0, 0},
    {"ISO 8859-14",    T_96,    '_', "iso8859-14",       0x80,   0, 0},
    {"ISO 8859-15",    T_96,    'b', "iso8859-15",       0x80,   0, 0},
    {"ISO 8859-16",    T_96,    'f', "iso8859-16",       0x80,   0, 0},
    {"KOI8-E",         T_96,    '@', "koi8-e",           0x80,   0, 0},
    {"TCVN",           T_96,    'Z', "tcvn-0",           0x80,   0, 0},

    {"GB 2312",        T_9494,  'A', "gb2312.1980-0",    0x0000, 0, 0},
    {"JIS X 0208",     T_9494,  'B', "jisx0208.1990-0",  0x0000, 0, 0},
    {"KSC 5601",       T_9494,  'C', "ksc5601.1987-0",   0x0000, 0, 0},
    {"JIS X 0212",     T_9494,  'D', "jisx0212.1990-0",  0x0000, 0, 0},

    {"GB 2312",        T_9696,  'A', "gb2312.1980-0",    0x0000, 0, 0},
    {"JIS X 0208",     T_9696,  'B', "jisx0208.1990-0",  0x0000, 0, 0},
    {"KSC 5601",       T_9696,  'C', "ksc5601.1987-0",   0x0000, 0, 0},
    {"JIS X 0212",     T_9696,  'D', "jisx0212.1990-0",  0x0000, 0, 0},

    {"KOI8-R",         T_128,   0,   "koi8-r",           0x80,   0, 0},
    {"KOI8-U",         T_128,   0,   "koi8-u",           0x80,   0, 0},
    {"KOI8-RU",        T_128,   0,   "koi8-ru",          0x80,   0, 0},
    {"CP 1252",        T_128,   0,   "microsoft-cp1252", 0x80,   0, 0},
    {"CP 1251",        T_128,   0,   "microsoft-cp1251", 0x80,   0, 0},
    {"CP 1250",        T_128,   0,   "microsoft-cp1250", 0x80,   0, 0},

    {"CP 437",         T_128,   0,   "ibm-cp437",        0x80,   0, 0},
    {"CP 850",         T_128,   0,   "ibm-cp850",        0x80,   0, 0},
    {"CP 852",         T_128,   0,   "ibm-cp852",        0x80,   0, 0},
    {"CP 866",         T_128,   0,   "ibm-cp866",        0x80,   0, 0},

    {"Big 5",          T_94192, 0,   "big5.eten-0",      0x8000, 0, 0},
    {0,                0,       0,   0,                  0,      0, 0}
};
/* *INDENT-ON* */

typedef struct _OtherCharset {
    const char *name;
    int (*init) (OtherStatePtr);
    unsigned int (*mapping) (unsigned int, OtherStatePtr);
    unsigned int (*reverse) (unsigned int, OtherStatePtr);
    int (*stack) (unsigned, OtherStatePtr);
} OtherCharsetRec, *OtherCharsetPtr;
/* *INDENT-OFF* */
static const OtherCharsetRec otherCharsets[] =
{
    {"GBK",        init_gbk,     mapping_gbk,     reverse_gbk,     stack_gbk},
    {"UTF-8",      init_utf8,    mapping_utf8,    reverse_utf8,    stack_utf8},
    {"SJIS",       init_sjis,    mapping_sjis,    reverse_sjis,    stack_sjis},
    {"BIG5-HKSCS", init_hkscs,   mapping_hkscs,   reverse_hkscs,   stack_hkscs},
    {"GB18030",    init_gb18030, mapping_gb18030, reverse_gb18030, stack_gb18030},
    {0, 0, 0, 0, 0}
};
/* *INDENT-ON* */

static int
compare(const char *s, const char *t)
{
    while (*s || *t) {
	if (*s && (isspace(UChar(*s)) || *s == '-' || *s == '_'))
	    s++;
	else if (*t && (isspace(UChar(*t)) || *t == '-' || *t == '_'))
	    t++;
	else if (*s && *t && tolower(UChar(*s)) == tolower(UChar(*t))) {
	    s++;
	    t++;
	} else
	    return 1;
    }
    return 0;
}

static int
compare1(const char *s, const char *t, size_t n)
{
    while (n && (*s || *t)) {
	--n;
	if (*s && (isspace(UChar(*s)) || *s == '-' || *s == '_'))
	    s++;
	else if (*t && (isspace(UChar(*t)) || *t == '-' || *t == '_'))
	    t++;
	else if (*s && *t && tolower(UChar(*s)) == tolower(UChar(*t))) {
	    s++;
	    t++;
	} else
	    return 1;
    }
    return 0;
}

static unsigned int
FontencCharsetRecode(unsigned int n, const CharsetRec * self)
{
    const FontencCharsetRec *fc = (const FontencCharsetRec *) (self->data);
    unsigned result;

    result = MapCodeValue(n + fc->shift, fc->mapping);

    TRACE(("FontencCharsetRecode %#x ->%#x%s\n",
	   n,
	   result,
	   (n != result) ? " map" : ""));

    return result;
}

static int
FontencCharsetReverse(unsigned int i, const CharsetRec * self)
{
    const FontencCharsetRec *fc = (const FontencCharsetRec *) (self->data);
    unsigned n;
    int result = -1;

    n = fc->reverse->reverse(i, fc->reverse->data);
    if (n != 0 && n >= fc->shift) {
	n -= fc->shift;

#define IS_GL(n) ((n) >= 0x20 && (n) < 0x80)
	switch (self->type) {
	case T_94:
	case T_96:
	    if (IS_GL(n))
		result = (int) n;
	    break;
	case T_128:
	    if (n < 0x80)
		result = (int) n;
	    break;
	case T_9494:
	case T_9696:
	    if (IS_GL(n >> 8) && IS_GL(n & 0xFF))
		result = (int) n;
	    break;
	case T_94192:
	    if (IS_GL(n >> 8) && IS_GL(n & 0x7F))
		result = (int) n;
	    break;
	default:
	    abort();
	    /* NOTREACHED */
	}
#undef IS_GL
    }

    TRACE(("FontencCharsetReverse %#x ->%#x%s\n",
	   i,
	   result,
	   ((int) i != result) ? " map" : ""));

    return result;
}

static CharsetPtr cachedCharsets = NULL;

static CharsetPtr
getCachedCharset(unsigned final, int type, const char *name)
{
    CharsetPtr c;
    for (c = cachedCharsets; c; c = c->next) {
	if (((c->type == type && c->final == final) ||
	     (name && !compare(c->name, name))) &&
	    (c->type != T_FAILED))
	    return c;
    }
    return NULL;
}

static void
cacheCharset(CharsetPtr c)
{
    c->next = cachedCharsets;
    cachedCharsets = c;
    VERBOSE(2, ("cachedCharset '%s'\n", c->name));
}

static CharsetPtr
getFontencCharset(unsigned final, int type, const char *name)
{
    FontencCharsetPtr fc;
    CharsetPtr c = NULL;
    FontMapPtr mapping;
    FontMapReversePtr reverse;
    CharsetPtr result = NULL;

    fc = fontencCharsets;
    while (fc->name) {
	if (((fc->type == type && fc->final == final) ||
	     (name && !compare(fc->name, name))) &&
	    (fc->type != T_FAILED))
	    break;
	fc++;
    }

    if (!fc->name) {
	VERBOSE(2, ("...no match for '%s'\n", NonNull(name)));
    } else if ((c = TypeCalloc(CharsetRec)) == 0) {
	VERBOSE(2, ("malloc failed\n"));
    } else if ((mapping = LookupMapping(fc->xlfd)) == NULL) {
	VERBOSE(2, ("...lookup mapping %s (%s) failed\n", NonNull(name), fc->xlfd));
	fc->type = T_FAILED;
    } else if ((reverse = LookupReverse(mapping)) == NULL) {
	VERBOSE(2, ("...lookup reverse %s failed\n", NonNull(name)));
	fc->type = T_FAILED;
    } else {
	fc->mapping = mapping;
	fc->reverse = reverse;

	c->name = fc->name;
	c->type = fc->type;
	c->final = fc->final;
	c->recode = FontencCharsetRecode;
	c->reverse = FontencCharsetReverse;
	c->data = fc;

	cacheCharset(c);
	result = c;
    }

    if (result == NULL && c != NULL)
	free(c);

    return result;
}

static CharsetPtr
getOtherCharset(const char *name)
{
    const OtherCharsetRec *fc;
    CharsetPtr c = NULL;
    OtherStatePtr s = NULL;
    CharsetPtr result = NULL;

    fc = otherCharsets;
    while (fc->name) {
	if (name && !compare(fc->name, name))
	    break;
	fc++;
    }

    if (!fc->name) {
	VERBOSE(2, ("...no match for '%s'\n", NonNull(name)));
    } else if ((c = TypeCalloc(CharsetRec)) == NULL) {
	VERBOSE(2, ("malloc failed\n"));
    } else if ((s = TypeCalloc(OtherState)) == NULL) {
	VERBOSE(2, ("malloc failed\n"));
    } else {
	c->name = fc->name;
	c->type = T_OTHER;
	c->final = 0;
	c->data = fc;
	c->other_recode = fc->mapping;
	c->other_reverse = fc->reverse;
	c->other_stack = fc->stack;
	c->other_aux = s;

	if (!fc->init(s)) {
	    VERBOSE(2, ("...initialization %s failed\n", NonNull(name)));
	    c->type = T_FAILED;
	} else {
	    cacheCharset(c);
	    result = c;
	}
    }

    if (result == NULL) {
	if (c != NULL)
	    free(c);
	if (s != NULL)
	    free(s);
    }

    return result;
}

const CharsetRec *
getUnknownCharset(int type)
{
    TRACE(("getUnknownCharset(%d)\n", type));
    switch (type) {
    case T_94:
	VERBOSE(2, ("using unknown 94-charset\n"));
	return &Unknown94Charset;
    case T_96:
	VERBOSE(2, ("using unknown 96-charset\n"));
	return &Unknown96Charset;
    case T_9494:
	VERBOSE(2, ("using unknown 9494-charset\n"));
	return &Unknown9494Charset;
    case T_9696:
	VERBOSE(2, ("using unknown 9696-charset\n"));
	return &Unknown9696Charset;
    default:
	VERBOSE(2, ("using unknown 94-charset\n"));
	return &Unknown94Charset;
    }
}

const CharsetRec *
getCharset(unsigned final, int type)
{
    const CharsetRec *c;

    TRACE(("getCharset(final=%c, type=%d)\n", final, type));
    c = getCachedCharset(final, type, NULL);
    if (c)
	return c;

    c = getFontencCharset(final, type, NULL);
    if (c)
	return c;

    return getUnknownCharset(type);
}

const CharsetRec *
getCharsetByName(const char *name)
{
    const CharsetRec *c;

    VERBOSE(2, ("getCharsetByName(%s)\n", NonNull(name)));
    if (name == NULL)
	return getUnknownCharset(T_94);

    c = getCachedCharset(0, 0, name);
    if (c)
	return c;

    c = getFontencCharset(0, 0, name);
    if (c)
	return c;

    c = getOtherCharset(name);
    if (c)
	return c;

    return getUnknownCharset(T_94);
}
/* *INDENT-OFF* */
static const LocaleCharsetRec localeCharsets[] =
{
    {"C",          0, 2, "ASCII", NULL,         "ISO 8859-1",    NULL,         NULL},
    {"POSIX",      0, 2, "ASCII", NULL,         "ISO 8859-1",    NULL,         NULL},
    {"US-ASCII",   0, 2, "ASCII", NULL,         "ISO 8859-1",    NULL,         NULL},

    {"ISO8859-1",  0, 2, "ASCII", NULL,         "ISO 8859-1",    NULL,         NULL},
    {"ISO8859-2",  0, 2, "ASCII", NULL,         "ISO 8859-2",    NULL,         NULL},
    {"ISO8859-3",  0, 2, "ASCII", NULL,         "ISO 8859-3",    NULL,         NULL},
    {"ISO8859-4",  0, 2, "ASCII", NULL,         "ISO 8859-4",    NULL,         NULL},
    {"ISO8859-5",  0, 2, "ASCII", NULL,         "ISO 8859-5",    NULL,         NULL},
    {"ISO8859-6",  0, 2, "ASCII", NULL,         "ISO 8859-6",    NULL,         NULL},
    {"ISO8859-7",  0, 2, "ASCII", NULL,         "ISO 8859-7",    NULL,         NULL},
    {"ISO8859-8",  0, 2, "ASCII", NULL,         "ISO 8859-8",    NULL,         NULL},
    {"ISO8859-9",  0, 2, "ASCII", NULL,         "ISO 8859-9",    NULL,         NULL},
    {"ISO8859-10", 0, 2, "ASCII", NULL,         "ISO 8859-10",   NULL,         NULL},
    {"ISO8859-11", 0, 2, "ASCII", NULL,         "ISO 8859-11",   NULL,         NULL},
    {"TIS620",     0, 2, "ASCII", NULL,         "ISO 8859-11",   NULL,         NULL},
    {"ISO8859-13", 0, 2, "ASCII", NULL,         "ISO 8859-13",   NULL,         NULL},
    {"ISO8859-14", 0, 2, "ASCII", NULL,         "ISO 8859-14",   NULL,         NULL},
    {"ISO8859-15", 0, 2, "ASCII", NULL,         "ISO 8859-15",   NULL,         NULL},
    {"ISO8859-16", 0, 2, "ASCII", NULL,         "ISO 8859-16",   NULL,         NULL},

    {"KOI8-R",     0, 2, "ASCII", NULL,         "KOI8-R",        NULL,         NULL},
    {"KOI8-U",     0, 2, "ASCII", NULL,         "KOI8-U",        NULL,         NULL},
    {"KOI8-RU",    0, 2, "ASCII", NULL,         "KOI8-RU",       NULL,         NULL},
    {"CP1250",     0, 2, "ASCII", NULL,         "CP 1250",       NULL,         NULL},
    {"CP1251",     0, 2, "ASCII", NULL,         "CP 1251",       NULL,         NULL},
    {"CP1252",     0, 2, "ASCII", NULL,         "CP 1252",       NULL,         NULL},
    {"CP437",      0, 2, "ASCII", NULL,         "CP 437",        NULL,         NULL},
    {"CP850",      0, 2, "ASCII", NULL,         "CP 850",        NULL,         NULL},
    {"CP852",      0, 2, "ASCII", NULL,         "CP 852",        NULL,         NULL},
    {"CP866",      0, 2, "ASCII", NULL,         "CP 866",        NULL,         NULL},
    {"TCVN",       0, 2, "ASCII", NULL,         "TCVN",          NULL,         NULL},

    {"eucCN",      0, 1, "ASCII", "GB 2312",    NULL,            NULL,         NULL},
    {"GB2312",     0, 1, "ASCII", "GB 2312",    NULL,            NULL,         NULL},
    {"eucJP",      0, 1, "ASCII", "JIS X 0208", "JIS X 0201:GR", "JIS X 0212", NULL},
    {"eucKR",      0, 1, "ASCII", "KSC 5601",   NULL,            NULL,         NULL},
    {"eucCN",      0, 1, "ASCII", "GB 2312",    NULL,            NULL,         NULL},
    {"Big5",       0, 1, "ASCII", "Big 5",      NULL,            NULL,         NULL},

    {"gbk",        0, 1, NULL,    NULL,         NULL,            NULL,         "GBK"},
    {"UTF-8",      0, 1, NULL,    NULL,         NULL,            NULL,         "UTF-8"},
    {"SJIS",       0, 1, NULL,    NULL,         NULL,            NULL,         "SJIS"},
    {"Big5-HKSCS", 0, 1, NULL,    NULL,         NULL,            NULL,         "BIG5-HKSCS"},
    {"gb18030",    0, 1, NULL,    NULL,         NULL,            NULL,         "GB18030"},

    {0,            0, 0, 0,       0,            0,               0,            0}
};
/* *INDENT-ON* */

void
reportCharsets(void)
{
    const LocaleCharsetRec *p;
    FontencCharsetPtr q;
    printf("Known locale encodings:\n\n");
    for (p = localeCharsets; p->name; p++) {
	if (p->other) {
	    printf("  %s (non-ISO-2022 encoding)\n", p->other);
	    continue;
	}
	printf("  %s: GL -> G%d, GR -> G%d", p->name, p->gl, p->gr);
	if (p->g0)
	    printf(", G0: %s", p->g0);
	if (p->g1)
	    printf(", G1: %s", p->g1);
	if (p->g2)
	    printf(", G2: %s", p->g2);
	if (p->g3)
	    printf(", G3: %s", p->g3);
	printf("\n");
    }

    printf("\n\nKnown charsets (not all may be available):\n\n");
    for (q = fontencCharsets; q->name; q++)
	printf("  %s%s\n",
	       q->name, q->final ? " (ISO 2022)" : "");
}

static const LocaleCharsetRec *
findLocaleCharset(const char *charset)
{
    const LocaleCharsetRec *p;

    for (p = localeCharsets; p->name; p++) {
	if (compare(p->name, charset) == 0)
	    break;
    }
    return p->name ? p : 0;
}

static const LocaleCharsetRec *
matchLocaleCharset(const char *charset)
{
    static const struct {
	const char *source;
	const char *target;
	size_t source_len;
	size_t target_len;
    } prefixes[] = {
#define DATA(source, target) { source, target, sizeof(source)-1, sizeof(target)-1 }
	DATA("ISO-", "ISO "),
	    DATA("IBM", "CP "),
	    DATA("CP-", "CP "),
	    DATA("ANSI", "CP "),	/* e.g., Solaris ANSI1251 */
#undef DATA
    };

    const LocaleCharsetRec *p = 0;

    if (*charset != '\0') {
	char *euro;
	char source[MAX_KEYWORD_LENGTH];

	strcpy(source, charset);
	if ((euro = strrchr(source, '@')) != 0 && !strcmp(euro, "@euro")) {
	    Warning("the euro character may not be supported\n");
	    *euro = 0;
	}

	p = findLocaleCharset(source);

	if (p == 0) {
	    size_t have = strlen(source);
	    size_t n;
	    char target[MAX_KEYWORD_LENGTH + 80];

	    for (n = 0; n < SizeOf(prefixes); ++n) {
		if (have > prefixes[n].source_len
		    && have < MAX_KEYWORD_LENGTH
		    && !compare1(source,
				 prefixes[n].source,
				 prefixes[n].source_len)) {
		    strcpy(target, prefixes[n].target);
		    strcpy(target + prefixes[n].target_len,
			   source + prefixes[n].source_len);
		    if ((p = findLocaleCharset(target)) != 0) {
			break;
		    }
		}
	    }
	}
    }
    return p;
}

int
getLocaleState(const char *locale,
	       const char *charset,
	       int *gl_return, int *gr_return,
	       const CharsetRec * *g0_return,
	       const CharsetRec * *g1_return,
	       const CharsetRec * *g2_return,
	       const CharsetRec * *g3_return,
	       const CharsetRec * *other_return)
{
    int result = 0;
    char *resolved = 0;
    const LocaleCharsetRec *p;

    if (!charset) {
	resolved = resolveLocale(locale);
	if (!resolved)
	    return -1;
	charset = strrchr(resolved, '.');
	if (charset)
	    charset++;
	else
	    charset = resolved;
    }

    if ((p = matchLocaleCharset(charset)) != 0) {
	*gl_return = p->gl;
	*gr_return = p->gr;
	*g0_return = getCharsetByName(p->g0);
	*g1_return = getCharsetByName(p->g1);
	*g2_return = getCharsetByName(p->g2);
	*g3_return = getCharsetByName(p->g3);
	if (p->other)
	    *other_return = getCharsetByName(p->other);
	else
	    *other_return = NULL;
    } else {
	result = -1;
    }

    if (resolved != 0)
	free(resolved);
    return result;
}

#ifdef NO_LEAKS
static int
isUnknownCharsetPtr(CharsetPtr p)
{
    return (p == &Unknown94Charset
	    || p == &Unknown96Charset
	    || p == &Unknown9494Charset
	    || p == &Unknown9696Charset);
}

static void
destroyFontencCharsetPtr(FontencCharsetPtr p)
{
#ifdef USE_ICONV
    if (p->reverse) {
	luitDestroyReverse(p->reverse);
    }
#else
    p->mapping = 0;

    /*
     * This should, but does not work -
     *     FontMapReverseFree(p->reverse)
     *
     * The iteration for map[] is based on reading the source of
     * FontMapReverse().
     */
    if (p->reverse) {
	int n;
	unsigned **map = p->reverse->data;
	for (n = 0; n < 256; ++n) {
	    if (map[n])
		free(map[n]);
	}
	free(p->reverse->data);
	free(p->reverse);
	p->reverse = 0;
    }
#endif
}

static void
destroyCharset(CharsetPtr p)
{
    if (!isUnknownCharsetPtr(p)) {
	if (p->type == T_OTHER) {
	    free(p->other_aux);
	} else {
	    destroyFontencCharsetPtr((FontencCharsetPtr) p->data);
	}
	free(p);
    }
}

void
charset_leaks(void)
{
    while (cachedCharsets != 0) {
	CharsetPtr next = cachedCharsets->next;
	destroyCharset(cachedCharsets);
	cachedCharsets = next;
    }
}
#endif /* NO_LEAKS */
