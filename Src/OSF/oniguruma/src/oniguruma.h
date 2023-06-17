// oniguruma.h - Oniguruma (regular expression library)
// Copyright (c) 2002-2020  K.Kosako  All rights reserved.
//
#ifndef ONIGURUMA_H
#define ONIGURUMA_H

#ifdef __cplusplus
extern "C" {
#endif

#define ONIGURUMA
#define ONIGURUMA_VERSION_MAJOR   6
#define ONIGURUMA_VERSION_MINOR   9
#define ONIGURUMA_VERSION_TEENY   6
#define ONIGURUMA_VERSION_INT     60906
#define ONIG_STATIC // @sobolev
#ifndef ONIG_STATIC
	#ifndef ONIG_EXTERN
		#if defined(_WIN32) && !defined(__GNUC__)
			#if defined(ONIGURUMA_EXPORT)
				#define ONIG_EXTERN   extern __declspec(dllexport)
			#else
				#define ONIG_EXTERN   extern __declspec(dllimport)
			#endif
		#endif
	#endif
	#ifndef ONIG_EXTERN
		#define ONIG_EXTERN   extern
	#endif
#else
	#ifdef __cplusplus
		#define ONIG_EXTERN
	#else
		#define ONIG_EXTERN   extern
	#endif	
#endif
#ifndef ONIG_VARIADIC_FUNC_ATTR
	#define ONIG_VARIADIC_FUNC_ATTR
#endif
// PART: character encoding 
//#ifndef ONIG_ESCAPE_UCHAR_COLLISION
	//#define UChar_Removed uchar
//#endif
typedef uint  OnigCodePoint;
//typedef uchar OnigUChar_Removed;
typedef uint  OnigCtype;
typedef uint  OnigLen;

#define ONIG_INFINITE_DISTANCE  ~((OnigLen)0)

typedef uint OnigCaseFoldType; /* case fold flag */

extern OnigCaseFoldType OnigDefaultCaseFoldFlag;

#define ONIGENC_CASE_FOLD_ASCII_ONLY            (1)
/* #define ONIGENC_CASE_FOLD_HIRAGANA_KATAKANA  (1<<1) */
/* #define ONIGENC_CASE_FOLD_KATAKANA_WIDTH     (1<<2) */
#define ONIGENC_CASE_FOLD_TURKISH_AZERI         (1<<20)
#define INTERNAL_ONIGENC_CASE_FOLD_MULTI_CHAR   (1<<30)

#define ONIGENC_CASE_FOLD_MIN      INTERNAL_ONIGENC_CASE_FOLD_MULTI_CHAR
#define ONIGENC_CASE_FOLD_DEFAULT  OnigDefaultCaseFoldFlag

#define ONIGENC_MAX_COMP_CASE_FOLD_CODE_LEN       3
#define ONIGENC_GET_CASE_FOLD_CODES_MAX_NUM      13
/* 13 => Unicode:0x1ffc */

/* code range */
#define ONIGENC_CODE_RANGE_NUM(range)     ((int)range[0])
#define ONIGENC_CODE_RANGE_FROM(range, i)  range[((i)*2) + 1]
#define ONIGENC_CODE_RANGE_TO(range, i)    range[((i)*2) + 2]

typedef struct {
	int byte_len; /* argument(original) character(s) byte length */
	int code_len; /* number of code */
	OnigCodePoint code[ONIGENC_MAX_COMP_CASE_FOLD_CODE_LEN];
} OnigCaseFoldCodeItem;

typedef struct {
	OnigCodePoint esc;
	OnigCodePoint anychar;
	OnigCodePoint anytime;
	OnigCodePoint zero_or_one_time;
	OnigCodePoint one_or_more_time;
	OnigCodePoint anychar_anytime;
} OnigMetaCharTableType;

typedef int (* OnigApplyAllCaseFoldFunc)(OnigCodePoint from, OnigCodePoint* to, int to_len, void * arg);

typedef struct OnigEncodingTypeST {
	int (*mbc_enc_len)(const uchar * p);
	const char * name;
	int max_enc_len;
	int min_enc_len;
	int (*is_mbc_newline)(const uchar * p, const uchar * end);
	OnigCodePoint (* mbc_to_code)(const uchar * p, const uchar * end);
	int (*code_to_mbclen)(OnigCodePoint code);
	int (*code_to_mbc)(OnigCodePoint code, uchar *buf);
	int (*mbc_case_fold)(OnigCaseFoldType flag, const uchar **pp, const uchar * end, uchar * to);
	int (*apply_all_case_fold)(OnigCaseFoldType flag, OnigApplyAllCaseFoldFunc f, void * arg);
	int (*get_case_fold_codes_by_str)(OnigCaseFoldType flag, const uchar * p, const uchar * end, OnigCaseFoldCodeItem acs[]);
	int (*property_name_to_ctype)(struct OnigEncodingTypeST* enc, uchar * p, uchar * end);
	int (*is_code_ctype)(OnigCodePoint code, OnigCtype ctype);
	int (*get_ctype_code_range)(OnigCtype ctype, OnigCodePoint* sb_out, const OnigCodePoint* ranges[]);
	uchar * (*left_adjust_char_head)(const uchar * start, const uchar * p);
	int (*is_allowed_reverse_match)(const uchar * p, const uchar * end);
	int (*init)(void);
	int (*is_initialized)(void);
	int (*is_valid_mbc_string)(const uchar * s, const uchar * end);
	uint flag;
	OnigCodePoint sb_range;
	int index;
} OnigEncodingType;

typedef OnigEncodingType * OnigEncoding;

extern OnigEncodingType OnigEncodingASCII;
extern OnigEncodingType OnigEncodingISO_8859_1;
extern OnigEncodingType OnigEncodingISO_8859_2;
extern OnigEncodingType OnigEncodingISO_8859_3;
extern OnigEncodingType OnigEncodingISO_8859_4;
extern OnigEncodingType OnigEncodingISO_8859_5;
extern OnigEncodingType OnigEncodingISO_8859_6;
extern OnigEncodingType OnigEncodingISO_8859_7;
extern OnigEncodingType OnigEncodingISO_8859_8;
extern OnigEncodingType OnigEncodingISO_8859_9;
extern OnigEncodingType OnigEncodingISO_8859_10;
extern OnigEncodingType OnigEncodingISO_8859_11;
extern OnigEncodingType OnigEncodingISO_8859_13;
extern OnigEncodingType OnigEncodingISO_8859_14;
extern OnigEncodingType OnigEncodingISO_8859_15;
extern OnigEncodingType OnigEncodingISO_8859_16;
extern OnigEncodingType OnigEncodingUTF8;
extern OnigEncodingType OnigEncodingUTF16_BE;
extern OnigEncodingType OnigEncodingUTF16_LE;
extern OnigEncodingType OnigEncodingUTF32_BE;
extern OnigEncodingType OnigEncodingUTF32_LE;
extern OnigEncodingType OnigEncodingEUC_JP;
extern OnigEncodingType OnigEncodingEUC_TW;
extern OnigEncodingType OnigEncodingEUC_KR;
extern OnigEncodingType OnigEncodingEUC_CN;
extern OnigEncodingType OnigEncodingSJIS;
extern OnigEncodingType OnigEncodingKOI8;
extern OnigEncodingType OnigEncodingKOI8_R;
extern OnigEncodingType OnigEncodingCP1251;
extern OnigEncodingType OnigEncodingBIG5;
extern OnigEncodingType OnigEncodingGB18030;

#define ONIG_ENCODING_ASCII        (&OnigEncodingASCII)
#define ONIG_ENCODING_ISO_8859_1   (&OnigEncodingISO_8859_1)
#define ONIG_ENCODING_ISO_8859_2   (&OnigEncodingISO_8859_2)
#define ONIG_ENCODING_ISO_8859_3   (&OnigEncodingISO_8859_3)
#define ONIG_ENCODING_ISO_8859_4   (&OnigEncodingISO_8859_4)
#define ONIG_ENCODING_ISO_8859_5   (&OnigEncodingISO_8859_5)
#define ONIG_ENCODING_ISO_8859_6   (&OnigEncodingISO_8859_6)
#define ONIG_ENCODING_ISO_8859_7   (&OnigEncodingISO_8859_7)
#define ONIG_ENCODING_ISO_8859_8   (&OnigEncodingISO_8859_8)
#define ONIG_ENCODING_ISO_8859_9   (&OnigEncodingISO_8859_9)
#define ONIG_ENCODING_ISO_8859_10  (&OnigEncodingISO_8859_10)
#define ONIG_ENCODING_ISO_8859_11  (&OnigEncodingISO_8859_11)
#define ONIG_ENCODING_ISO_8859_13  (&OnigEncodingISO_8859_13)
#define ONIG_ENCODING_ISO_8859_14  (&OnigEncodingISO_8859_14)
#define ONIG_ENCODING_ISO_8859_15  (&OnigEncodingISO_8859_15)
#define ONIG_ENCODING_ISO_8859_16  (&OnigEncodingISO_8859_16)
#define ONIG_ENCODING_UTF8         (&OnigEncodingUTF8)
#define ONIG_ENCODING_UTF16_BE     (&OnigEncodingUTF16_BE)
#define ONIG_ENCODING_UTF16_LE     (&OnigEncodingUTF16_LE)
#define ONIG_ENCODING_UTF32_BE     (&OnigEncodingUTF32_BE)
#define ONIG_ENCODING_UTF32_LE     (&OnigEncodingUTF32_LE)
#define ONIG_ENCODING_EUC_JP       (&OnigEncodingEUC_JP)
#define ONIG_ENCODING_EUC_TW       (&OnigEncodingEUC_TW)
#define ONIG_ENCODING_EUC_KR       (&OnigEncodingEUC_KR)
#define ONIG_ENCODING_EUC_CN       (&OnigEncodingEUC_CN)
#define ONIG_ENCODING_SJIS         (&OnigEncodingSJIS)
#define ONIG_ENCODING_KOI8         (&OnigEncodingKOI8)
#define ONIG_ENCODING_KOI8_R       (&OnigEncodingKOI8_R)
#define ONIG_ENCODING_CP1251       (&OnigEncodingCP1251)
#define ONIG_ENCODING_BIG5         (&OnigEncodingBIG5)
#define ONIG_ENCODING_GB18030      (&OnigEncodingGB18030)

#define ONIG_ENCODING_UNDEF    ((OnigEncoding)0)

/* work size */
#define ONIGENC_CODE_TO_MBC_MAXLEN       7
#define ONIGENC_MBC_CASE_FOLD_MAXLEN    18
/* 18: 6(max-byte) * 3(case-fold chars) */

/* character types */
typedef enum {
	ONIGENC_CTYPE_NEWLINE = 0,
	ONIGENC_CTYPE_ALPHA   = 1,
	ONIGENC_CTYPE_BLANK   = 2,
	ONIGENC_CTYPE_CNTRL   = 3,
	ONIGENC_CTYPE_DIGIT   = 4,
	ONIGENC_CTYPE_GRAPH   = 5,
	ONIGENC_CTYPE_LOWER   = 6,
	ONIGENC_CTYPE_PRINT   = 7,
	ONIGENC_CTYPE_PUNCT   = 8,
	ONIGENC_CTYPE_SPACE   = 9,
	ONIGENC_CTYPE_UPPER   = 10,
	ONIGENC_CTYPE_XDIGIT  = 11,
	ONIGENC_CTYPE_WORD    = 12,
	ONIGENC_CTYPE_ALNUM   = 13,/* alpha || digit */
	ONIGENC_CTYPE_ASCII   = 14
} OnigEncCtype;

#define ONIGENC_MAX_STD_CTYPE  ONIGENC_CTYPE_ASCII

#define onig_enc_len(enc, p, end)        ONIGENC_MBC_ENC_LEN(enc, p)

#define ONIGENC_IS_UNDEF(enc)          ((enc) == ONIG_ENCODING_UNDEF)
#define ONIGENC_IS_SINGLEBYTE(enc)     (ONIGENC_MBC_MAXLEN(enc) == 1)
#define ONIGENC_IS_MBC_HEAD(enc, p)     (ONIGENC_MBC_ENC_LEN(enc, p) != 1)
#define ONIGENC_IS_MBC_ASCII(p)           (*(p)   < 128)
#define ONIGENC_IS_CODE_ASCII(code)       ((code) < 128)
#define ONIGENC_IS_MBC_WORD(enc, s, end)  ONIGENC_IS_CODE_WORD(enc, ONIGENC_MBC_TO_CODE(enc, s, end))
#define ONIGENC_IS_MBC_WORD_ASCII(enc, s, end) onigenc_is_mbc_word_ascii(enc, s, end)
#define ONIGENC_NAME(enc)                      ((enc)->name)
#define ONIGENC_MBC_CASE_FOLD(enc, flag, pp, end, buf) (enc)->mbc_case_fold(flag, (const uchar **)pp, end, buf)
#define ONIGENC_IS_ALLOWED_REVERSE_MATCH(enc, s, end) (enc)->is_allowed_reverse_match(s, end)
#define ONIGENC_LEFT_ADJUST_CHAR_HEAD(enc, start, s) (enc)->left_adjust_char_head(start, s)
#define ONIGENC_IS_VALID_MBC_STRING(enc, s, end) (enc)->is_valid_mbc_string(s, end)
#define ONIGENC_APPLY_ALL_CASE_FOLD(enc, case_fold_flag, f, arg) (enc)->apply_all_case_fold(case_fold_flag, f, arg)
#define ONIGENC_GET_CASE_FOLD_CODES_BY_STR(enc, case_fold_flag, p, end, acs) (enc)->get_case_fold_codes_by_str(case_fold_flag, p, end, acs)
#define ONIGENC_STEP_BACK(enc, start, s, n) onigenc_step_back((enc), (start), (s), (n))

#define ONIGENC_MBC_ENC_LEN(enc, p)             (enc)->mbc_enc_len(p)
#define ONIGENC_MBC_MAXLEN(enc)               ((enc)->max_enc_len)
#define ONIGENC_MBC_MAXLEN_DIST(enc)           ONIGENC_MBC_MAXLEN(enc)
#define ONIGENC_MBC_MINLEN(enc)               ((enc)->min_enc_len)
#define ONIGENC_IS_MBC_NEWLINE(enc, p, end)      (enc)->is_mbc_newline((p), (end))
#define ONIGENC_MBC_TO_CODE(enc, p, end)         (enc)->mbc_to_code((p), (end))
#define ONIGENC_CODE_TO_MBCLEN(enc, code)       (enc)->code_to_mbclen(code)
#define ONIGENC_CODE_TO_MBC(enc, code, buf)      (enc)->code_to_mbc(code, buf)
#define ONIGENC_PROPERTY_NAME_TO_CTYPE(enc, p, end) (enc)->property_name_to_ctype(enc, p, end)
#define ONIGENC_IS_CODE_CTYPE(enc, code, ctype)  (enc)->is_code_ctype(code, ctype)
#define ONIGENC_IS_CODE_NEWLINE(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_NEWLINE)
#define ONIGENC_IS_CODE_GRAPH(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_GRAPH)
#define ONIGENC_IS_CODE_PRINT(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_PRINT)
#define ONIGENC_IS_CODE_ALNUM(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_ALNUM)
#define ONIGENC_IS_CODE_ALPHA(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_ALPHA)
#define ONIGENC_IS_CODE_LOWER(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_LOWER)
#define ONIGENC_IS_CODE_UPPER(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_UPPER)
#define ONIGENC_IS_CODE_CNTRL(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_CNTRL)
#define ONIGENC_IS_CODE_PUNCT(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_PUNCT)
#define ONIGENC_IS_CODE_SPACE(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_SPACE)
#define ONIGENC_IS_CODE_BLANK(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_BLANK)
#define ONIGENC_IS_CODE_DIGIT(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_DIGIT)
#define ONIGENC_IS_CODE_XDIGIT(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_XDIGIT)
#define ONIGENC_IS_CODE_WORD(enc, code) ONIGENC_IS_CODE_CTYPE(enc, code, ONIGENC_CTYPE_WORD)

#define ONIGENC_GET_CTYPE_CODE_RANGE(enc, ctype, sbout, ranges) (enc)->get_ctype_code_range(ctype, sbout, ranges)

ONIG_EXTERN uchar * onigenc_step_back(OnigEncoding enc, const uchar * start, const uchar * s, int n);

/* encoding API */
ONIG_EXTERN int     onigenc_init(void);
ONIG_EXTERN int     onig_initialize_encoding(OnigEncoding enc);
ONIG_EXTERN int     onigenc_set_default_encoding(OnigEncoding enc);
ONIG_EXTERN OnigEncoding onigenc_get_default_encoding(void);
ONIG_EXTERN void    onigenc_set_default_caseconv_table(const uchar * table);
ONIG_EXTERN uchar * onigenc_get_right_adjust_char_head_with_prev(OnigEncoding enc, const uchar * start, const uchar * s, const uchar **prev);
ONIG_EXTERN uchar * onigenc_get_prev_char_head(OnigEncoding enc, const uchar * start, const uchar * s);
ONIG_EXTERN uchar * onigenc_get_left_adjust_char_head(OnigEncoding enc, const uchar * start, const uchar * s);
ONIG_EXTERN uchar * onigenc_get_right_adjust_char_head(OnigEncoding enc, const uchar * start, const uchar * s);
ONIG_EXTERN int     onigenc_strlen(OnigEncoding enc, const uchar * p, const uchar * end);
ONIG_EXTERN int     onigenc_strlen_null(OnigEncoding enc, const uchar * p);
ONIG_EXTERN int     onigenc_str_bytelen_null(OnigEncoding enc, const uchar * p);
ONIG_EXTERN int     onigenc_is_valid_mbc_string(OnigEncoding enc, const uchar * s, const uchar * end);
ONIG_EXTERN uchar * onigenc_strdup(OnigEncoding enc, const uchar * s, const uchar * end);

/* PART: regular expression */

/* config parameters */
#define ONIG_NREGION                          10
#define ONIG_MAX_CAPTURE_NUM          2147483647  /* 2**31 - 1 */
#define ONIG_MAX_BACKREF_NUM                1000
#define ONIG_MAX_REPEAT_NUM               100000
#define ONIG_MAX_MULTI_BYTE_RANGES_NUM     10000
/* constants */
#define ONIG_MAX_ERROR_MESSAGE_LEN            90

typedef uint OnigOptionType;

#define ONIG_OPTION_DEFAULT            ONIG_OPTION_NONE

/* options */
#define ONIG_OPTION_NONE                 0U
/* options (compile time) */
#define ONIG_OPTION_IGNORECASE           1U
#define ONIG_OPTION_EXTEND               (ONIG_OPTION_IGNORECASE << 1)
#define ONIG_OPTION_MULTILINE            (ONIG_OPTION_EXTEND << 1)
#define ONIG_OPTION_SINGLELINE           (ONIG_OPTION_MULTILINE << 1)
#define ONIG_OPTION_FIND_LONGEST         (ONIG_OPTION_SINGLELINE << 1)
#define ONIG_OPTION_FIND_NOT_EMPTY       (ONIG_OPTION_FIND_LONGEST << 1)
#define ONIG_OPTION_NEGATE_SINGLELINE    (ONIG_OPTION_FIND_NOT_EMPTY << 1)
#define ONIG_OPTION_DONT_CAPTURE_GROUP   (ONIG_OPTION_NEGATE_SINGLELINE << 1)
#define ONIG_OPTION_CAPTURE_GROUP        (ONIG_OPTION_DONT_CAPTURE_GROUP << 1)
/* options (search time) */
#define ONIG_OPTION_NOTBOL                    (ONIG_OPTION_CAPTURE_GROUP << 1)
#define ONIG_OPTION_NOTEOL                    (ONIG_OPTION_NOTBOL << 1)
#define ONIG_OPTION_POSIX_REGION              (ONIG_OPTION_NOTEOL << 1)
#define ONIG_OPTION_CHECK_VALIDITY_OF_STRING  (ONIG_OPTION_POSIX_REGION << 1)
/* options (compile time) */
#define ONIG_OPTION_IGNORECASE_IS_ASCII  (ONIG_OPTION_CHECK_VALIDITY_OF_STRING << 3)
#define ONIG_OPTION_WORD_IS_ASCII        (ONIG_OPTION_IGNORECASE_IS_ASCII << 1)
#define ONIG_OPTION_DIGIT_IS_ASCII       (ONIG_OPTION_WORD_IS_ASCII << 1)
#define ONIG_OPTION_SPACE_IS_ASCII       (ONIG_OPTION_DIGIT_IS_ASCII << 1)
#define ONIG_OPTION_POSIX_IS_ASCII       (ONIG_OPTION_SPACE_IS_ASCII << 1)
#define ONIG_OPTION_TEXT_SEGMENT_EXTENDED_GRAPHEME_CLUSTER  (ONIG_OPTION_POSIX_IS_ASCII << 1)
#define ONIG_OPTION_TEXT_SEGMENT_WORD    (ONIG_OPTION_TEXT_SEGMENT_EXTENDED_GRAPHEME_CLUSTER << 1)
/* options (search time) */
#define ONIG_OPTION_NOT_BEGIN_STRING     (ONIG_OPTION_TEXT_SEGMENT_WORD << 1)
#define ONIG_OPTION_NOT_END_STRING       (ONIG_OPTION_NOT_BEGIN_STRING << 1)
#define ONIG_OPTION_NOT_BEGIN_POSITION   (ONIG_OPTION_NOT_END_STRING << 1)

#define ONIG_OPTION_MAXBIT               ONIG_OPTION_NOT_BEGIN_POSITION

#define ONIG_OPTION_ON(options, regopt)      ((options) |= (regopt))
#define ONIG_OPTION_OFF(options, regopt)     ((options) &= ~(regopt))
#define ONIG_IS_OPTION_ON(options, option)   ((options) & (option))

/* syntax */
typedef struct {
	uint op;
	uint op2;
	uint behavior;
	OnigOptionType options; /* default option */
	OnigMetaCharTableType meta_char_table;
} OnigSyntaxType;

extern OnigSyntaxType OnigSyntaxASIS;
extern OnigSyntaxType OnigSyntaxPosixBasic;
extern OnigSyntaxType OnigSyntaxPosixExtended;
extern OnigSyntaxType OnigSyntaxEmacs;
extern OnigSyntaxType OnigSyntaxGrep;
extern OnigSyntaxType OnigSyntaxGnuRegex;
extern OnigSyntaxType OnigSyntaxJava;
extern OnigSyntaxType OnigSyntaxPerl;
extern OnigSyntaxType OnigSyntaxPerl_NG;
extern OnigSyntaxType OnigSyntaxRuby;
extern OnigSyntaxType OnigSyntaxOniguruma;

/* predefined syntaxes (see regsyntax.c) */
#define ONIG_SYNTAX_ASIS               (&OnigSyntaxASIS)
#define ONIG_SYNTAX_POSIX_BASIC        (&OnigSyntaxPosixBasic)
#define ONIG_SYNTAX_POSIX_EXTENDED     (&OnigSyntaxPosixExtended)
#define ONIG_SYNTAX_EMACS              (&OnigSyntaxEmacs)
#define ONIG_SYNTAX_GREP               (&OnigSyntaxGrep)
#define ONIG_SYNTAX_GNU_REGEX          (&OnigSyntaxGnuRegex)
#define ONIG_SYNTAX_JAVA               (&OnigSyntaxJava)
#define ONIG_SYNTAX_PERL               (&OnigSyntaxPerl)
#define ONIG_SYNTAX_PERL_NG            (&OnigSyntaxPerl_NG)
#define ONIG_SYNTAX_RUBY               (&OnigSyntaxRuby)
#define ONIG_SYNTAX_ONIGURUMA          (&OnigSyntaxOniguruma)

/* default syntax */
extern OnigSyntaxType * OnigDefaultSyntax;
#define ONIG_SYNTAX_DEFAULT   OnigDefaultSyntax

/* syntax (operators) */
#define ONIG_SYN_OP_VARIABLE_META_CHARACTERS    (1U<<0)
#define ONIG_SYN_OP_DOT_ANYCHAR                 (1U<<1)   /* . */
#define ONIG_SYN_OP_ASTERISK_ZERO_INF           (1U<<2)   /* * */
#define ONIG_SYN_OP_ESC_ASTERISK_ZERO_INF       (1U<<3)
#define ONIG_SYN_OP_PLUS_ONE_INF                (1U<<4)   /* + */
#define ONIG_SYN_OP_ESC_PLUS_ONE_INF            (1U<<5)
#define ONIG_SYN_OP_QMARK_ZERO_ONE              (1U<<6)   /* ? */
#define ONIG_SYN_OP_ESC_QMARK_ZERO_ONE          (1U<<7)
#define ONIG_SYN_OP_BRACE_INTERVAL              (1U<<8)   /* {lower,upper} */
#define ONIG_SYN_OP_ESC_BRACE_INTERVAL          (1U<<9)   /* \{lower,upper\} */
#define ONIG_SYN_OP_VBAR_ALT                    (1U<<10)   /* | */
#define ONIG_SYN_OP_ESC_VBAR_ALT                (1U<<11)  /* \| */
#define ONIG_SYN_OP_LPAREN_SUBEXP               (1U<<12)  /* (...)   */
#define ONIG_SYN_OP_ESC_LPAREN_SUBEXP           (1U<<13)  /* \(...\) */
#define ONIG_SYN_OP_ESC_AZ_BUF_ANCHOR           (1U<<14)  /* \A, \Z, \z */
#define ONIG_SYN_OP_ESC_CAPITAL_G_BEGIN_ANCHOR  (1U<<15)  /* \G     */
#define ONIG_SYN_OP_DECIMAL_BACKREF             (1U<<16)  /* \num   */
#define ONIG_SYN_OP_BRACKET_CC                  (1U<<17)  /* [...]  */
#define ONIG_SYN_OP_ESC_W_WORD                  (1U<<18)  /* \w, \W */
#define ONIG_SYN_OP_ESC_LTGT_WORD_BEGIN_END     (1U<<19)  /* \<. \> */
#define ONIG_SYN_OP_ESC_B_WORD_BOUND            (1U<<20)  /* \b, \B */
#define ONIG_SYN_OP_ESC_S_WHITE_SPACE           (1U<<21)  /* \s, \S */
#define ONIG_SYN_OP_ESC_D_DIGIT                 (1U<<22)  /* \d, \D */
#define ONIG_SYN_OP_LINE_ANCHOR                 (1U<<23)  /* ^, $   */
#define ONIG_SYN_OP_POSIX_BRACKET               (1U<<24)  /* [:xxxx:] */
#define ONIG_SYN_OP_QMARK_NON_GREEDY            (1U<<25)  /* ??,*?,+?,{n,m}? */
#define ONIG_SYN_OP_ESC_CONTROL_CHARS           (1U<<26)  /* \n,\r,\t,\a ... */
#define ONIG_SYN_OP_ESC_C_CONTROL               (1U<<27)  /* \cx  */
#define ONIG_SYN_OP_ESC_OCTAL3                  (1U<<28)  /* \OOO */
#define ONIG_SYN_OP_ESC_X_HEX2                  (1U<<29)  /* \xHH */
#define ONIG_SYN_OP_ESC_X_BRACE_HEX8            (1U<<30)  /* \x{7HHHHHHH} */
#define ONIG_SYN_OP_ESC_O_BRACE_OCTAL           (1U<<31)  /* \o{1OOOOOOOOOO} */

#define ONIG_SYN_OP2_ESC_CAPITAL_Q_QUOTE        (1U<<0)  /* \Q...\E */
#define ONIG_SYN_OP2_QMARK_GROUP_EFFECT         (1U<<1)  /* (?...) */
#define ONIG_SYN_OP2_OPTION_PERL                (1U<<2)  /* (?imsx),(?-imsx) */
#define ONIG_SYN_OP2_OPTION_RUBY                (1U<<3)  /* (?imx), (?-imx)  */
#define ONIG_SYN_OP2_PLUS_POSSESSIVE_REPEAT     (1U<<4)  /* ?+,*+,++ */
#define ONIG_SYN_OP2_PLUS_POSSESSIVE_INTERVAL   (1U<<5)  /* {n,m}+   */
#define ONIG_SYN_OP2_CCLASS_SET_OP              (1U<<6)  /* [...&&..[..]..] */
#define ONIG_SYN_OP2_QMARK_LT_NAMED_GROUP       (1U<<7)  /* (?<name>...) */
#define ONIG_SYN_OP2_ESC_K_NAMED_BACKREF        (1U<<8)  /* \k<name> */
#define ONIG_SYN_OP2_ESC_G_SUBEXP_CALL          (1U<<9)  /* \g<name>, \g<n> */
#define ONIG_SYN_OP2_ATMARK_CAPTURE_HISTORY     (1U<<10) /* (?@..),(?@<x>..) */
#define ONIG_SYN_OP2_ESC_CAPITAL_C_BAR_CONTROL  (1U<<11) /* \C-x */
#define ONIG_SYN_OP2_ESC_CAPITAL_M_BAR_META     (1U<<12) /* \M-x */
#define ONIG_SYN_OP2_ESC_V_VTAB                 (1U<<13) /* \v as VTAB */
#define ONIG_SYN_OP2_ESC_U_HEX4                 (1U<<14) /* \uHHHH */
#define ONIG_SYN_OP2_ESC_GNU_BUF_ANCHOR         (1U<<15) /* \`, \' */
#define ONIG_SYN_OP2_ESC_P_BRACE_CHAR_PROPERTY  (1U<<16) /* \p{...}, \P{...} */
#define ONIG_SYN_OP2_ESC_P_BRACE_CIRCUMFLEX_NOT (1U<<17) /* \p{^..}, \P{^..} */
/* #define ONIG_SYN_OP2_CHAR_PROPERTY_PREFIX_IS (1U<<18) */
#define ONIG_SYN_OP2_ESC_H_XDIGIT               (1U<<19) /* \h, \H */
#define ONIG_SYN_OP2_INEFFECTIVE_ESCAPE         (1U<<20) /* \ */
#define ONIG_SYN_OP2_QMARK_LPAREN_IF_ELSE       (1U<<21) /* (?(n)) (?(...)...|...) */
#define ONIG_SYN_OP2_ESC_CAPITAL_K_KEEP         (1U<<22) /* \K */
#define ONIG_SYN_OP2_ESC_CAPITAL_R_GENERAL_NEWLINE (1U<<23) /* \R \r\n else [\x0a-\x0d] */
#define ONIG_SYN_OP2_ESC_CAPITAL_N_O_SUPER_DOT  (1U<<24) /* \N (?-m:.), \O (?m:.) */
#define ONIG_SYN_OP2_QMARK_TILDE_ABSENT_GROUP   (1U<<25) /* (?~...) */
#define ONIG_SYN_OP2_ESC_X_Y_GRAPHEME_CLUSTER   (1U<<26) /* obsoleted: use next */
#define ONIG_SYN_OP2_ESC_X_Y_TEXT_SEGMENT       (1U<<26) /* \X \y \Y */
#define ONIG_SYN_OP2_QMARK_PERL_SUBEXP_CALL     (1U<<27) /* (?R), (?&name)... */
#define ONIG_SYN_OP2_QMARK_BRACE_CALLOUT_CONTENTS (1U<<28) /* (?{...}) (?{{...}}) */
#define ONIG_SYN_OP2_ASTERISK_CALLOUT_NAME      (1U<<29) /* (*name) (*name{a,..}) */
#define ONIG_SYN_OP2_OPTION_ONIGURUMA           (1U<<30) /* (?imxWDSPy) */

/* syntax (behavior) */
#define ONIG_SYN_CONTEXT_INDEP_ANCHORS           (1U<<31) /* not implemented */
#define ONIG_SYN_CONTEXT_INDEP_REPEAT_OPS        (1U<<0)  /* ?, *, +, {n,m} */
#define ONIG_SYN_CONTEXT_INVALID_REPEAT_OPS      (1U<<1)  /* error or ignore */
#define ONIG_SYN_ALLOW_UNMATCHED_CLOSE_SUBEXP    (1U<<2)  /* ...)... */
#define ONIG_SYN_ALLOW_INVALID_INTERVAL          (1U<<3)  /* {??? */
#define ONIG_SYN_ALLOW_INTERVAL_LOW_ABBREV       (1U<<4)  /* {,n} => {0,n} */
#define ONIG_SYN_STRICT_CHECK_BACKREF            (1U<<5)  /* /(\1)/,/\1()/ ..*/
#define ONIG_SYN_DIFFERENT_LEN_ALT_LOOK_BEHIND   (1U<<6)  /* (?<=a|bc) */
#define ONIG_SYN_CAPTURE_ONLY_NAMED_GROUP        (1U<<7)  /* see doc/RE */
#define ONIG_SYN_ALLOW_MULTIPLEX_DEFINITION_NAME (1U<<8)  /* (?<x>)(?<x>) */
#define ONIG_SYN_FIXED_INTERVAL_IS_GREEDY_ONLY   (1U<<9)  /* a{n}?=(?:a{n})? */
#define ONIG_SYN_ISOLATED_OPTION_CONTINUE_BRANCH (1U<<10) /* ..(?i)...|... */
#define ONIG_SYN_VARIABLE_LEN_LOOK_BEHIND        (1U<<11)  /* (?<=a+|..) */

/* syntax (behavior) in char class [...] */
#define ONIG_SYN_NOT_NEWLINE_IN_NEGATIVE_CC      (1U<<20) /* [^...] */
#define ONIG_SYN_BACKSLASH_ESCAPE_IN_CC          (1U<<21) /* [..\w..] etc.. */
#define ONIG_SYN_ALLOW_EMPTY_RANGE_IN_CC         (1U<<22)
#define ONIG_SYN_ALLOW_DOUBLE_RANGE_OP_IN_CC     (1U<<23) /* [0-9-a]=[0-9\-a] */
#define ONIG_SYN_ALLOW_INVALID_CODE_END_OF_RANGE_IN_CC (1U<<26)
/* syntax (behavior) warning */
#define ONIG_SYN_WARN_CC_OP_NOT_ESCAPED          (1U<<24) /* [,-,] */
#define ONIG_SYN_WARN_REDUNDANT_NESTED_REPEAT    (1U<<25) /* (?:a*)+ */

/* meta character specifiers (onig_set_meta_char()) */
#define ONIG_META_CHAR_ESCAPE               0
#define ONIG_META_CHAR_ANYCHAR              1
#define ONIG_META_CHAR_ANYTIME              2
#define ONIG_META_CHAR_ZERO_OR_ONE_TIME     3
#define ONIG_META_CHAR_ONE_OR_MORE_TIME     4
#define ONIG_META_CHAR_ANYCHAR_ANYTIME      5

#define ONIG_INEFFECTIVE_META_CHAR          0

/* error codes */
#define ONIG_IS_PATTERN_ERROR(ecode)   ((ecode) <= -100 && (ecode) > -1000)
/* normal return */
#define ONIG_NORMAL                                            0
#define ONIG_MISMATCH                                         -1
#define ONIG_NO_SUPPORT_CONFIG                                -2
#define ONIG_ABORT                                            -3

/* internal error */
#define ONIGERR_MEMORY                                         -5
#define ONIGERR_TYPE_BUG                                       -6
#define ONIGERR_PARSER_BUG                                    -11
#define ONIGERR_STACK_BUG                                     -12
#define ONIGERR_UNDEFINED_BYTECODE                            -13
#define ONIGERR_UNEXPECTED_BYTECODE                           -14
#define ONIGERR_MATCH_STACK_LIMIT_OVER                        -15
#define ONIGERR_PARSE_DEPTH_LIMIT_OVER                        -16
#define ONIGERR_RETRY_LIMIT_IN_MATCH_OVER                     -17
#define ONIGERR_RETRY_LIMIT_IN_SEARCH_OVER                    -18
#define ONIGERR_SUBEXP_CALL_LIMIT_IN_SEARCH_OVER              -19
#define ONIGERR_DEFAULT_ENCODING_IS_NOT_SETTED                -21
#define ONIGERR_SPECIFIED_ENCODING_CANT_CONVERT_TO_WIDE_CHAR  -22
#define ONIGERR_FAIL_TO_INITIALIZE                            -23
/* general error */
#define ONIGERR_INVALID_ARGUMENT                              -30
/* syntax error */
#define ONIGERR_END_PATTERN_AT_LEFT_BRACE                    -100
#define ONIGERR_END_PATTERN_AT_LEFT_BRACKET                  -101
#define ONIGERR_EMPTY_CHAR_CLASS                             -102
#define ONIGERR_PREMATURE_END_OF_CHAR_CLASS                  -103
#define ONIGERR_END_PATTERN_AT_ESCAPE                        -104
#define ONIGERR_END_PATTERN_AT_META                          -105
#define ONIGERR_END_PATTERN_AT_CONTROL                       -106
#define ONIGERR_META_CODE_SYNTAX                             -108
#define ONIGERR_CONTROL_CODE_SYNTAX                          -109
#define ONIGERR_CHAR_CLASS_VALUE_AT_END_OF_RANGE             -110
#define ONIGERR_CHAR_CLASS_VALUE_AT_START_OF_RANGE           -111
#define ONIGERR_UNMATCHED_RANGE_SPECIFIER_IN_CHAR_CLASS      -112
#define ONIGERR_TARGET_OF_REPEAT_OPERATOR_NOT_SPECIFIED      -113
#define ONIGERR_TARGET_OF_REPEAT_OPERATOR_INVALID            -114
#define ONIGERR_NESTED_REPEAT_OPERATOR                       -115
#define ONIGERR_UNMATCHED_CLOSE_PARENTHESIS                  -116
#define ONIGERR_END_PATTERN_WITH_UNMATCHED_PARENTHESIS       -117
#define ONIGERR_END_PATTERN_IN_GROUP                         -118
#define ONIGERR_UNDEFINED_GROUP_OPTION                       -119
#define ONIGERR_INVALID_POSIX_BRACKET_TYPE                   -121
#define ONIGERR_INVALID_LOOK_BEHIND_PATTERN                  -122
#define ONIGERR_INVALID_REPEAT_RANGE_PATTERN                 -123
/* values error (syntax error) */
#define ONIGERR_TOO_BIG_NUMBER                               -200
#define ONIGERR_TOO_BIG_NUMBER_FOR_REPEAT_RANGE              -201
#define ONIGERR_UPPER_SMALLER_THAN_LOWER_IN_REPEAT_RANGE     -202
#define ONIGERR_EMPTY_RANGE_IN_CHAR_CLASS                    -203
#define ONIGERR_MISMATCH_CODE_LENGTH_IN_CLASS_RANGE          -204
#define ONIGERR_TOO_MANY_MULTI_BYTE_RANGES                   -205
#define ONIGERR_TOO_SHORT_MULTI_BYTE_STRING                  -206
#define ONIGERR_TOO_BIG_BACKREF_NUMBER                       -207
#define ONIGERR_INVALID_BACKREF                              -208
#define ONIGERR_NUMBERED_BACKREF_OR_CALL_NOT_ALLOWED         -209
#define ONIGERR_TOO_MANY_CAPTURES                            -210
#define ONIGERR_TOO_LONG_WIDE_CHAR_VALUE                     -212
#define ONIGERR_EMPTY_GROUP_NAME                             -214
#define ONIGERR_INVALID_GROUP_NAME                           -215
#define ONIGERR_INVALID_CHAR_IN_GROUP_NAME                   -216
#define ONIGERR_UNDEFINED_NAME_REFERENCE                     -217
#define ONIGERR_UNDEFINED_GROUP_REFERENCE                    -218
#define ONIGERR_MULTIPLEX_DEFINED_NAME                       -219
#define ONIGERR_MULTIPLEX_DEFINITION_NAME_CALL               -220
#define ONIGERR_NEVER_ENDING_RECURSION                       -221
#define ONIGERR_GROUP_NUMBER_OVER_FOR_CAPTURE_HISTORY        -222
#define ONIGERR_INVALID_CHAR_PROPERTY_NAME                   -223
#define ONIGERR_INVALID_IF_ELSE_SYNTAX                       -224
#define ONIGERR_INVALID_ABSENT_GROUP_PATTERN                 -225
#define ONIGERR_INVALID_ABSENT_GROUP_GENERATOR_PATTERN       -226
#define ONIGERR_INVALID_CALLOUT_PATTERN                      -227
#define ONIGERR_INVALID_CALLOUT_NAME                         -228
#define ONIGERR_UNDEFINED_CALLOUT_NAME                       -229
#define ONIGERR_INVALID_CALLOUT_BODY                         -230
#define ONIGERR_INVALID_CALLOUT_TAG_NAME                     -231
#define ONIGERR_INVALID_CALLOUT_ARG                          -232
#define ONIGERR_INVALID_CODE_POINT_VALUE                     -400
#define ONIGERR_INVALID_WIDE_CHAR_VALUE                      -400
#define ONIGERR_TOO_BIG_WIDE_CHAR_VALUE                      -401
#define ONIGERR_NOT_SUPPORTED_ENCODING_COMBINATION           -402
#define ONIGERR_INVALID_COMBINATION_OF_OPTIONS               -403
#define ONIGERR_TOO_MANY_USER_DEFINED_OBJECTS                -404
#define ONIGERR_TOO_LONG_PROPERTY_NAME                       -405
#define ONIGERR_VERY_INEFFICIENT_PATTERN                     -406
#define ONIGERR_LIBRARY_IS_NOT_INITIALIZED                   -500

/* errors related to thread */
/* #define ONIGERR_OVER_THREAD_PASS_LIMIT_COUNT                -1001 */

/* must be smaller than MEM_STATUS_BITS_NUM (uint * 8) */
#define ONIG_MAX_CAPTURE_HISTORY_GROUP   31
#define ONIG_IS_CAPTURE_HISTORY_GROUP(r, i) ((i) <= ONIG_MAX_CAPTURE_HISTORY_GROUP && (r)->list && (r)->list[i])

typedef struct OnigCaptureTreeNodeStruct {
	int group; /* group number */
	int beg;
	int end;
	int allocated;
	int num_childs;
	struct OnigCaptureTreeNodeStruct** childs;
} OnigCaptureTreeNode;
//
// match result region type 
//
struct re_registers {
	int    allocated;
	int    num_regs;
	int  * beg;
	int  * end;
	/* extended */
	OnigCaptureTreeNode * history_root; /* capture history tree root */
};

/* capture tree traverse */
#define ONIG_TRAVERSE_CALLBACK_AT_FIRST   1
#define ONIG_TRAVERSE_CALLBACK_AT_LAST    2
#define ONIG_TRAVERSE_CALLBACK_AT_BOTH   (ONIG_TRAVERSE_CALLBACK_AT_FIRST | ONIG_TRAVERSE_CALLBACK_AT_LAST)
#define ONIG_REGION_NOTPOS            -1

typedef struct re_registers OnigRegion;

typedef struct {
	OnigEncoding enc;
	uchar * par;
	uchar * par_end;
} OnigErrorInfo;

typedef struct {
	int lower;
	int upper;
} OnigRepeatRange;

typedef void (*OnigWarnFunc)(const char * s);
extern void onig_null_warn(const char * s);
#define ONIG_NULL_WARN       onig_null_warn
#define ONIG_CHAR_TABLE_SIZE   256

struct re_pattern_buffer;

typedef struct re_pattern_buffer OnigRegexType;
typedef OnigRegexType * OnigRegex;

#ifndef ONIG_ESCAPE_REGEX_T_COLLISION
	typedef OnigRegexType regex_t;
#endif

struct OnigRegSetStruct;

typedef struct OnigRegSetStruct OnigRegSet;

typedef enum {
	ONIG_REGSET_POSITION_LEAD = 0,
	ONIG_REGSET_REGEX_LEAD    = 1,
	ONIG_REGSET_PRIORITY_TO_REGEX_ORDER = 2
} OnigRegSetLead;

typedef struct {
	int num_of_elements;
	OnigEncoding pattern_enc;
	OnigEncoding target_enc;
	OnigSyntaxType* syntax;
	OnigOptionType option;
	OnigCaseFoldType case_fold_flag;
} OnigCompileInfo;

/* types for callout */
typedef enum {
	ONIG_CALLOUT_IN_PROGRESS   = 1,/* 1<<0 */
	ONIG_CALLOUT_IN_RETRACTION = 2 /* 1<<1 */
} OnigCalloutIn;

#define ONIG_CALLOUT_IN_BOTH  (ONIG_CALLOUT_IN_PROGRESS | ONIG_CALLOUT_IN_RETRACTION)

typedef enum {
	ONIG_CALLOUT_OF_CONTENTS = 0,
	ONIG_CALLOUT_OF_NAME     = 1
} OnigCalloutOf;

typedef enum {
	ONIG_CALLOUT_TYPE_SINGLE      = 0,
	ONIG_CALLOUT_TYPE_START_CALL  = 1,
	ONIG_CALLOUT_TYPE_BOTH_CALL   = 2,
	ONIG_CALLOUT_TYPE_START_MARK_END_CALL = 3,
} OnigCalloutType;

#define ONIG_NON_NAME_ID        -1
#define ONIG_NON_CALLOUT_NUM     0

#define ONIG_CALLOUT_MAX_ARGS_NUM     4
#define ONIG_CALLOUT_DATA_SLOT_NUM    5

struct OnigCalloutArgsStruct;

typedef struct OnigCalloutArgsStruct OnigCalloutArgs;

typedef int (* OnigCalloutFunc)(OnigCalloutArgs * args, void * user_data);

/* callout function return values (less than -1: error code) */
typedef enum {
	ONIG_CALLOUT_FAIL     =  1,
	ONIG_CALLOUT_SUCCESS  =  0
} OnigCalloutResult;

typedef enum {
	ONIG_TYPE_VOID     = 0,
	ONIG_TYPE_LONG     = 1<<0,
	ONIG_TYPE_CHAR     = 1<<1,
	ONIG_TYPE_STRING   = 1<<2,
	ONIG_TYPE_POINTER  = 1<<3,
	ONIG_TYPE_TAG      = 1<<4,
} OnigType;

typedef union {
	long l;
	OnigCodePoint c;
	struct {
		uchar * start;
		uchar * end;
	} s;

	void * p;
	int tag; /* tag -> callout_num */
} OnigValue;

struct OnigMatchParamStruct;

typedef struct OnigMatchParamStruct OnigMatchParam;

/* Oniguruma Native API */

ONIG_EXTERN int onig_initialize(OnigEncoding encodings[], int number_of_encodings);
/* onig_init(): deprecated function. Use onig_initialize(). */
ONIG_EXTERN int onig_init(void);
ONIG_EXTERN int ONIG_VARIADIC_FUNC_ATTR onig_error_code_to_str(uchar * s, int err_code, ...);
ONIG_EXTERN int onig_is_error_code_needs_param(int code);
ONIG_EXTERN void onig_set_warn_func(OnigWarnFunc f);
ONIG_EXTERN void onig_set_verb_warn_func(OnigWarnFunc f);
ONIG_EXTERN int onig_new(OnigRegex*, const uchar * pattern, const uchar * pattern_end, OnigOptionType option, OnigEncoding enc, OnigSyntaxType* syntax, OnigErrorInfo* einfo);
ONIG_EXTERN int onig_reg_init(OnigRegex reg, OnigOptionType option, OnigCaseFoldType case_fold_flag, OnigEncoding enc, OnigSyntaxType* syntax);
int onig_new_without_alloc(OnigRegex, const uchar * pattern, const uchar * pattern_end, OnigOptionType option, OnigEncoding enc, OnigSyntaxType* syntax, OnigErrorInfo* einfo);
ONIG_EXTERN int onig_new_deluxe(OnigRegex* reg, const uchar * pattern, const uchar * pattern_end, OnigCompileInfo* ci, OnigErrorInfo* einfo);
ONIG_EXTERN void onig_free(OnigRegex);
ONIG_EXTERN void onig_free_body(OnigRegex);
ONIG_EXTERN int onig_scan(OnigRegex reg, const uchar * str, const uchar * end, OnigRegion* region, OnigOptionType option, int (* scan_callback)(
	int, int, OnigRegion*, void *), void * callback_arg);
ONIG_EXTERN int onig_search(OnigRegex, const uchar * str, const uchar * end, const uchar * start, const uchar * range, OnigRegion* region, OnigOptionType option);
ONIG_EXTERN int onig_search_with_param(OnigRegex, const uchar * str, const uchar * end, const uchar * start, const uchar * range,
    OnigRegion* region, OnigOptionType option, OnigMatchParam* mp);
ONIG_EXTERN int onig_match(OnigRegex, const uchar * str, const uchar * end, const uchar * at, OnigRegion* region, OnigOptionType option);
ONIG_EXTERN int onig_match_with_param(OnigRegex, const uchar * str, const uchar * end, const uchar * at, OnigRegion* region,
    OnigOptionType option, OnigMatchParam* mp);
ONIG_EXTERN int onig_regset_new(OnigRegSet**rset, int n, regex_t* regs[]);
ONIG_EXTERN int onig_regset_add(OnigRegSet* set, regex_t* reg);
ONIG_EXTERN int onig_regset_replace(OnigRegSet* set, int at, regex_t* reg);
ONIG_EXTERN void onig_regset_free(OnigRegSet* set);
ONIG_EXTERN int onig_regset_number_of_regex(OnigRegSet* set);
ONIG_EXTERN regex_t * onig_regset_get_regex(OnigRegSet* set, int at);
ONIG_EXTERN OnigRegion * onig_regset_get_region(OnigRegSet* set, int at);
ONIG_EXTERN int onig_regset_search(OnigRegSet* set, const uchar * str, const uchar * end, const uchar * start, const uchar * range,
    OnigRegSetLead lead, OnigOptionType option, int* rmatch_pos);
ONIG_EXTERN int onig_regset_search_with_param(OnigRegSet* set, const uchar * str, const uchar * end, const uchar * start,
    const uchar * range,  OnigRegSetLead lead, OnigOptionType option, OnigMatchParam* mps[], int* rmatch_pos);
ONIG_EXTERN OnigRegion* onig_region_new(void);
ONIG_EXTERN void onig_region_init(OnigRegion* region);
ONIG_EXTERN void onig_region_free(OnigRegion* region, int free_self);
ONIG_EXTERN void onig_region_copy(OnigRegion* to, OnigRegion* from);
ONIG_EXTERN void onig_region_clear(OnigRegion* region);
ONIG_EXTERN int onig_region_resize(OnigRegion* region, int n);
ONIG_EXTERN int onig_region_set(OnigRegion* region, int at, int beg, int end);
ONIG_EXTERN int onig_name_to_group_numbers(OnigRegex reg, const uchar * name, const uchar * name_end, int** nums);
ONIG_EXTERN int onig_name_to_backref_number(OnigRegex reg, const uchar * name, const uchar * name_end, OnigRegion * region);
ONIG_EXTERN int onig_foreach_name(OnigRegex reg, int (* func)(const uchar *, const uchar *, int, int*, OnigRegex, void *), void * arg);
ONIG_EXTERN int onig_number_of_names(OnigRegex reg);
ONIG_EXTERN int onig_number_of_captures(OnigRegex reg);
ONIG_EXTERN int onig_number_of_capture_histories(OnigRegex reg);
ONIG_EXTERN OnigCaptureTreeNode* onig_get_capture_tree(OnigRegion* region);
ONIG_EXTERN int onig_capture_tree_traverse(OnigRegion* region, int at, int (* callback_func)(int, int, int, int, int, void *), void * arg);
ONIG_EXTERN int onig_noname_group_capture_is_active(OnigRegex reg);
ONIG_EXTERN OnigEncoding onig_get_encoding(OnigRegex reg);
ONIG_EXTERN OnigOptionType onig_get_options(OnigRegex reg);
ONIG_EXTERN OnigCaseFoldType onig_get_case_fold_flag(OnigRegex reg);
ONIG_EXTERN OnigSyntaxType* onig_get_syntax(OnigRegex reg);
ONIG_EXTERN int onig_set_default_syntax(OnigSyntaxType* syntax);
ONIG_EXTERN void onig_copy_syntax(OnigSyntaxType* to, OnigSyntaxType* from);
ONIG_EXTERN uint onig_get_syntax_op(OnigSyntaxType* syntax);
ONIG_EXTERN uint onig_get_syntax_op2(OnigSyntaxType* syntax);
ONIG_EXTERN uint onig_get_syntax_behavior(OnigSyntaxType* syntax);
ONIG_EXTERN OnigOptionType onig_get_syntax_options(OnigSyntaxType* syntax);
ONIG_EXTERN void onig_set_syntax_op(OnigSyntaxType* syntax, uint op);
ONIG_EXTERN void onig_set_syntax_op2(OnigSyntaxType* syntax, uint op2);
ONIG_EXTERN void onig_set_syntax_behavior(OnigSyntaxType* syntax, uint behavior);
ONIG_EXTERN void onig_set_syntax_options(OnigSyntaxType* syntax, OnigOptionType options);
ONIG_EXTERN int onig_set_meta_char(OnigSyntaxType* syntax, uint what, OnigCodePoint code);
ONIG_EXTERN void onig_copy_encoding(OnigEncoding to, OnigEncoding from);
ONIG_EXTERN OnigCaseFoldType onig_get_default_case_fold_flag(void);
ONIG_EXTERN int onig_set_default_case_fold_flag(OnigCaseFoldType case_fold_flag);
ONIG_EXTERN uint onig_get_match_stack_limit_size(void);
ONIG_EXTERN int onig_set_match_stack_limit_size(uint size);
ONIG_EXTERN ulong onig_get_retry_limit_in_match(void);
ONIG_EXTERN int onig_set_retry_limit_in_match(ulong n);
ONIG_EXTERN ulong onig_get_retry_limit_in_search(void);
ONIG_EXTERN int onig_set_retry_limit_in_search(ulong n);
ONIG_EXTERN uint onig_get_parse_depth_limit(void);
ONIG_EXTERN int onig_set_capture_num_limit(int num);
ONIG_EXTERN int onig_set_parse_depth_limit(uint depth);
ONIG_EXTERN ulong onig_get_subexp_call_limit_in_search(void);
ONIG_EXTERN int onig_set_subexp_call_limit_in_search(ulong n);
ONIG_EXTERN int onig_get_subexp_call_max_nest_level(void);
ONIG_EXTERN int onig_set_subexp_call_max_nest_level(int level);
ONIG_EXTERN int onig_unicode_define_user_property(const char * name, OnigCodePoint* ranges);
ONIG_EXTERN int onig_end(void);
ONIG_EXTERN const char * onig_version(void);
ONIG_EXTERN const char * onig_copyright(void);

/* for OnigMatchParam */
ONIG_EXTERN OnigMatchParam* onig_new_match_param(void);
ONIG_EXTERN void onig_free_match_param(OnigMatchParam* p);
ONIG_EXTERN void onig_free_match_param_content(OnigMatchParam* p);
ONIG_EXTERN int onig_initialize_match_param(OnigMatchParam* mp);
ONIG_EXTERN int onig_set_match_stack_limit_size_of_match_param(OnigMatchParam* param, uint limit);
ONIG_EXTERN int onig_set_retry_limit_in_match_of_match_param(OnigMatchParam* param, ulong limit);
ONIG_EXTERN int onig_set_retry_limit_in_search_of_match_param(OnigMatchParam* param, ulong limit);
ONIG_EXTERN int onig_set_progress_callout_of_match_param(OnigMatchParam* param, OnigCalloutFunc f);
ONIG_EXTERN int onig_set_retraction_callout_of_match_param(OnigMatchParam* param, OnigCalloutFunc f);
ONIG_EXTERN int onig_set_callout_user_data_of_match_param(OnigMatchParam* param, void * user_data);

/* for callout functions */
ONIG_EXTERN OnigCalloutFunc onig_get_progress_callout(void);
ONIG_EXTERN int onig_set_progress_callout(OnigCalloutFunc f);
ONIG_EXTERN OnigCalloutFunc onig_get_retraction_callout(void);
ONIG_EXTERN int onig_set_retraction_callout(OnigCalloutFunc f);
ONIG_EXTERN int onig_set_callout_of_name(OnigEncoding enc, OnigCalloutType type, uchar * name, uchar * name_end, int callout_in,
    OnigCalloutFunc callout, OnigCalloutFunc end_callout, int arg_num, uint arg_types[], int optional_arg_num, OnigValue opt_defaults[]);
ONIG_EXTERN uchar * onig_get_callout_name_by_name_id(int id);
ONIG_EXTERN int onig_get_callout_num_by_tag(OnigRegex reg, const uchar * tag, const uchar * tag_end);
ONIG_EXTERN int onig_get_callout_data_by_tag(OnigRegex reg, OnigMatchParam* mp, const uchar * tag, const uchar * tag_end, int slot, OnigType* type, OnigValue* val);
ONIG_EXTERN int onig_set_callout_data_by_tag(OnigRegex reg, OnigMatchParam* mp, const uchar * tag, const uchar * tag_end, int slot, OnigType type, OnigValue* val);

/* used in callout functions */
ONIG_EXTERN int onig_get_callout_num_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN OnigCalloutIn onig_get_callout_in_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN int onig_get_name_id_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN const uchar * onig_get_contents_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN const uchar * onig_get_contents_end_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN int onig_get_args_num_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN int onig_get_passed_args_num_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN int onig_get_arg_by_callout_args(OnigCalloutArgs * args, int index, OnigType* type, OnigValue* val);
ONIG_EXTERN const uchar * onig_get_string_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN const uchar * onig_get_string_end_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN const uchar * onig_get_start_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN const uchar * onig_get_right_range_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN const uchar * onig_get_current_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN OnigRegex onig_get_regex_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN ulong onig_get_retry_counter_by_callout_args(OnigCalloutArgs * args);
ONIG_EXTERN int onig_callout_tag_is_exist_at_callout_num(OnigRegex reg, int callout_num);
ONIG_EXTERN const uchar * onig_get_callout_tag_start(OnigRegex reg, int callout_num);
ONIG_EXTERN const uchar * onig_get_callout_tag_end(OnigRegex reg, int callout_num);
ONIG_EXTERN int onig_get_callout_data_dont_clear_old(OnigRegex reg, OnigMatchParam* mp, int callout_num, int slot, OnigType* type, OnigValue* val);
ONIG_EXTERN int onig_get_callout_data_by_callout_args_self_dont_clear_old(OnigCalloutArgs * args, int slot, OnigType* type, OnigValue* val);
ONIG_EXTERN int onig_get_callout_data(OnigRegex reg, OnigMatchParam* mp, int callout_num, int slot, OnigType* type, OnigValue* val);
ONIG_EXTERN int onig_get_callout_data_by_callout_args(OnigCalloutArgs * args, int callout_num, int slot, OnigType* type, OnigValue* val);
ONIG_EXTERN int onig_get_callout_data_by_callout_args_self(OnigCalloutArgs * args, int slot, OnigType* type, OnigValue* val);
ONIG_EXTERN int onig_set_callout_data(OnigRegex reg, OnigMatchParam* mp, int callout_num, int slot, OnigType type, OnigValue* val);
ONIG_EXTERN int onig_set_callout_data_by_callout_args(OnigCalloutArgs * args, int callout_num, int slot, OnigType type, OnigValue* val);
ONIG_EXTERN int onig_set_callout_data_by_callout_args_self(OnigCalloutArgs * args, int slot, OnigType type, OnigValue* val);
ONIG_EXTERN int onig_get_capture_range_in_callout(OnigCalloutArgs * args, int mem_num, int* begin, int* end);
ONIG_EXTERN int onig_get_used_stack_size_in_callout(OnigCalloutArgs * args, int* used_num, int* used_bytes);

/* builtin callout functions */
ONIG_EXTERN int onig_builtin_fail(OnigCalloutArgs * args, void * user_data);
ONIG_EXTERN int onig_builtin_mismatch(OnigCalloutArgs * args, void * user_data);
ONIG_EXTERN int onig_builtin_error(OnigCalloutArgs * args, void * user_data);
ONIG_EXTERN int onig_builtin_count(OnigCalloutArgs * args, void * user_data);
ONIG_EXTERN int onig_builtin_total_count(OnigCalloutArgs * args, void * user_data);
ONIG_EXTERN int onig_builtin_max(OnigCalloutArgs * args, void * user_data);
ONIG_EXTERN int onig_builtin_cmp(OnigCalloutArgs * args, void * user_data);
ONIG_EXTERN int onig_setup_builtin_monitors_by_ascii_encoded_name(void * fp);

#ifdef __cplusplus
}
#endif

#endif /* ONIGURUMA_H */
