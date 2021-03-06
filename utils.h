#ifndef UTILS_H
#define UTILS_H

#include "quickjs.h"
#include "quickjs-internal.h"
#include "cutils.h"
#include <string.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifndef offsetof
#define offsetof(type, field) ((size_t) & ((type*)0)->field)
#endif

#define JS_CGETSET_ENUMERABLE_DEF(prop_name, fgetter, fsetter, magic_num)                                              \
  {                                                                                                                    \
    .name = prop_name, .prop_flags = JS_PROP_ENUMERABLE | JS_PROP_CONFIGURABLE, .def_type = JS_DEF_CGETSET_MAGIC,      \
    .magic = magic_num, .u = {                                                                                         \
      .getset = {.get = {.getter_magic = fgetter}, .set = {.setter_magic = fsetter}}                                   \
    }                                                                                                                  \
  }

#define JS_CGETSET_ENUMERABLE_DEF(prop_name, fgetter, fsetter, magic_num)                                              \
  {                                                                                                                    \
    .name = prop_name, .prop_flags = JS_PROP_ENUMERABLE | JS_PROP_CONFIGURABLE, .def_type = JS_DEF_CGETSET_MAGIC,      \
    .magic = magic_num, .u = {                                                                                         \
      .getset = {.get = {.getter_magic = fgetter}, .set = {.setter_magic = fsetter}}                                   \
    }                                                                                                                  \
  }

#if defined(_WIN32) || defined(__MINGW32__)
#define VISIBLE __declspec(dllexport)
#define HIDDEN
#else
#define VISIBLE __attribute__((visibility("default")))
#define HIDDEN __attribute__((visibility("hidden")))
#endif

#define max_num(a, b) ((a) > (b) ? (a) : (b))

#define is_control_char(c)                                                                                             \
  ((c) == '\a' || (c) == '\b' || (c) == '\t' || (c) == '\n' || (c) == '\v' || (c) == '\f' || (c) == '\r')
#define is_alphanumeric_char(c) ((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z')
#define is_digit_char(c) ((c) >= '0' && (c) <= '9')
#define is_newline_char(c) ((c) == '\n')
#define is_identifier_char(c) (is_alphanumeric_char(c) || is_digit_char(c) || (c) == '$' || (c) == '_')
#define is_whitespace_char(c) ((c) == ' ' || (c) == '\t' || (c) == '\v' || (c) == '\n' || (c) == '\r')

typedef struct {
  BOOL done;
  JSValue value;
} IteratorValue;

static inline int
escape_char_pred(int c) {
  switch(c) {
    case 8: return 'b';
    case 12: return 'f';
    case 10: return 'n';
    case 13: return 'r';
    case 9: return 't';
    case 11: return 'v';
    case 39: return '\'';
    case 92: return '\\';
  }
  if(c < 0x20 || c == 127)
    return 'x';

  return 0;
}

static inline int
unescape_char_pred(int c) {
  switch(c) {
    case 'b': return 8;
    case 'f': return 12;
    case 'n': return 10;
    case 'r': return 13;
    case 't': return 9;
    case 'v': return 11;
    case '\'': return 39;
    case '\\': return 92;
  }
  return 0;
}

static inline int
is_escape_char(int c) {
  return is_control_char(c) || c == '\\' || c == '\'' || c == 0x1b || c == 0;
}

static inline int
is_backslash_char(int c) {
  return c == '\\';
}

//#define is_dot_char(c) ((c) == '.')0
//#define is_backslash_char(c) ((c) == '\\')

static inline int
is_dot_char(int c) {
  return c == '.';
}

static inline int
is_identifier(const char* str) {
  if(!((*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z') || *str == '$'))
    return 0;
  while(*++str) {
    if(!is_identifier_char(*str))
      return 0;
  }
  return 1;
}

static inline int
is_integer(const char* str) {
  if(!(*str >= '1' && *str <= '9') && !(*str == '0' && str[1] == '\0'))
    return 0;
  while(*++str) {
    if(!is_digit_char(*str))
      return 0;
  }
  return 1;
}

static inline size_t
min_size(size_t a, size_t b) {
  if(a < b)
    return a;
  else
    return b;
}

static inline uint64_t
abs_int64(int64_t a) {
  return a < 0 ? -a : a;
}

static inline uint32_t
abs_int32(int32_t i) {
  return i < 0 ? -i : i;
}

static inline int32_t
sign_int32(uint32_t i) {
  return (i & 0x80000000) ? -1 : 1;
}

static inline int32_t
mod_int32(int32_t a, int32_t b) {
  int32_t c = a % b;
  return (c < 0) ? c + b : c;
}

static inline size_t
byte_count(const void* s, size_t n, char c) {
  const unsigned char* t;
  unsigned char ch = (unsigned char)c;
  size_t count;
  for(t = (unsigned char*)s, count = 0; n; ++t, --n) {
    if(*t == ch)
      ++count;
  }
  return count;
}

static inline size_t
byte_chr(const char* str, size_t len, char c) {
  const char *s, *t;
  for(s = str, t = s + len; s < t; ++s)
    if(*s == c)
      break;
  return s - str;
}

static inline size_t
byte_rchr(const void* str, size_t len, char c) {
  const char *s, *t;
  for(s = (const char*)str, t = s + len; --t >= s;)
    if(*t == c)
      return (size_t)(t - s);
  return len;
}

static inline size_t
byte_chrs(const char* str, size_t len, char needle[], size_t nl) {
  const char *s, *t;
  for(s = str, t = str + len; s != t; s++)
    if(byte_chr(needle, nl, *s) < nl)
      break;
  return s - (const char*)str;
}

static inline size_t
str_chr(const char* in, char needle) {
  const char* t = in;
  const char c = needle;
  for(;;) {
    if(!*t || *t == c) {
      break;
    };
    ++t;
  }
  return (size_t)(t - in);
}

static inline size_t
str_rchr(const char* s, char needle) {
  const char *in = s, *found = 0;
  for(;;) {
    if(!*in)
      break;
    if(*in == needle)
      found = in;
    ++in;
  }
  return (size_t)((found ? found : in) - s);
}

static inline size_t
str_rchrs(const char* in, const char needles[], size_t nn) {
  const char *s = in, *found = 0;
  size_t i;
  for(;;) {
    if(!*s)
      break;
    for(i = 0; i < nn; ++i) {
      if(*s == needles[i])
        found = s;
    }
    ++s;
  }
  return (size_t)((found ? found : s) - in);
}

static inline int
str_endb(const char* a, const char* x, size_t n) {
  size_t alen = strlen(a);
  a += alen - n;
  return alen >= n && !memcmp(a, x, n);
}

/* str_end returns 1 if the b is a suffix of a, 0 otherwise */
static inline int
str_end(const char* a, const char* b) {
  return str_endb(a, b, strlen(b));
}

#define str_contains(s, needle) (!!strchr((s), (needle)))

static inline char*
str_insert(char* s, JSContext* ctx, size_t pos, const char* t, size_t tlen) {
  size_t slen = strlen(s);
  char* x;
  if(!(x = js_malloc(ctx, slen + tlen + 1)))
    return 0;
  if(pos > 0)
    memcpy(x, s, pos);
  if(pos < slen)
    memmove(&x[pos + tlen], &s[pos], slen - pos);
  memcpy(&x[pos], t, tlen);
  x[slen + tlen] = 0;
  return x;
}

static inline char*
str_append(char* s, JSContext* ctx, const char* t) {
  return str_insert(s, ctx, strlen(s), t, strlen(t));
}

static inline char*
str_prepend(char* s, JSContext* ctx, const char* t) {
  return str_insert(s, ctx, 0, t, strlen(t));
}

static inline size_t
str0_insert(char** s, JSContext* ctx, size_t pos, const char* t, size_t tlen) {
  size_t slen = strlen(*s);
  char* x;
  if(!(x = js_realloc(ctx, *s, slen + tlen + 1))) {
    *s = 0;
    return 0;
  }
  if(pos < slen)
    memmove(&x[pos + tlen], &x[pos], slen - pos);
  memcpy(&x[pos], t, tlen);
  x[slen + tlen] = 0;
  *s = x;
  return slen + tlen;
}

static inline int
str0_append(char** s, JSContext* ctx, const char* t) {
  return str0_insert(s, ctx, strlen(*s), t, strlen(t));
}

static inline size_t
str0_prepend(char** s, JSContext* ctx, const char* t) {
  return str0_insert(s, ctx, 0, t, strlen(t));
}

#define COLOR_RED "\x1b[31m"
#define COLOR_LIGHTRED "\x1b[1;31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_MARINE "\x1b[36m"
#define COLOR_GRAY "\x1b[1;30m"
#define COLOR_NONE "\x1b[m"

size_t ansi_skip(const char* str, size_t len);
size_t ansi_length(const char* str, size_t len);
size_t ansi_truncate(const char* str, size_t len, size_t limit);

static inline char*
str_ndup(const char* s, size_t n) {
  char* r = malloc(n + 1);
  if(r == NULL)
    return NULL;
  memcpy(r, s, n);
  r[n] = '\0';
  return r;
}

uint64_t time_us(void);

static inline size_t
predicate_find(const char* str, size_t len, int (*pred)(int32_t)) {
  size_t pos;
  for(pos = 0; pos < len; pos++)
    if(pred(str[pos]))
      break;
  return pos;
}

static inline char
escape_char_letter(char c) {
  switch(c) {
    case '\0': return '0';
    case '\a': return 'a';
    case '\b': return 'b';
    case '\t': return 't';
    case '\n': return 'n';
    case '\v': return 'v';
    case '\f': return 'f';
    case '\r': return 'r';
    case '\\': return '\\';
    case '\'': return '\'';
  }
  return 0;
}

size_t token_length(const char* str, size_t len, char delim);
int64_t array_search(void* a, size_t m, size_t elsz, void* needle);
#define array_contains(a, m, elsz, needle) (array_search((a), (m), (elsz), (needle)) != -1)
#define dbuf_append(d, x, n) dbuf_put((d), (const uint8_t*)(x), (n))
void dbuf_put_escaped_pred(DynBuf* db, const char* str, size_t len, int (*pred)(int));
void dbuf_put_unescaped_pred(DynBuf* db, const char* str, size_t len, int (*pred)(int));

static inline void
js_dbuf_init_rt(JSRuntime* rt, DynBuf* s) {
  dbuf_init2(s, rt, (DynBufReallocFunc*)js_realloc_rt);
}

static inline void
js_dbuf_init(JSContext* ctx, DynBuf* s) {
  dbuf_init2(s, ctx, (DynBufReallocFunc*)js_realloc);
}

static inline void
dbuf_put_escaped(DynBuf* db, const char* str, size_t len) {
  return dbuf_put_escaped_pred(db, str, len, escape_char_pred);
}

void dbuf_put_value(DynBuf* db, JSContext* ctx, JSValueConst value);
size_t dbuf_token_push(DynBuf* db, const char* str, size_t len, char delim);
size_t dbuf_token_pop(DynBuf* db, char delim);

static inline size_t
dbuf_count(DynBuf* db, int ch) {
  return byte_count(db->buf, db->size, ch);
}

static inline void
dbuf_0(DynBuf* db) {
  dbuf_putc(db, '\0');
  db->size--;
}

static inline void
dbuf_zero(DynBuf* db) {
  dbuf_realloc(db, 0);
}

char* dbuf_at_n(const DynBuf* db, size_t i, size_t* n, char sep);
const char* dbuf_last_line(DynBuf* db, size_t* len);
void dbuf_put_colorstr(DynBuf* db, const char* str, const char* color, int with_color);
int dbuf_reserve_start(DynBuf* s, size_t len);
int dbuf_prepend(DynBuf* s, const uint8_t* data, size_t len);
JSValue dbuf_tostring_free(DynBuf* s, JSContext* ctx);

static inline int32_t
dbuf_get_column(DynBuf* db) {
  size_t len;
  const char* str;
  if(db->size) {
    str = dbuf_last_line(db, &len);
    return ansi_length(str, len);
  }
  return 0;
}

static inline size_t
dbuf_bitflags(DynBuf* db, uint32_t bits, const char* const names[]) {
  size_t i, n = 0;
  for(i = 0; i < sizeof(bits) * 8; i++) {
    if(bits & (1 << i)) {
      size_t len = strlen(names[i]);
      if(n) {
        n++;
        dbuf_putstr(db, "|");
      }
      dbuf_append(db, names[i], len);
      n += len;
    }
  }
  return n;
}

typedef struct {
  char* source;
  size_t len;
  int flags;
} RegExp;

int regexp_flags_tostring(int, char*);
int regexp_flags_fromstring(const char*);
RegExp regexp_from_argv(int argc, JSValueConst argv[], JSContext* ctx);
RegExp regexp_from_dbuf(DynBuf* dbuf, int flags);
uint8_t* regexp_compile(RegExp re, JSContext* ctx);

static inline void
regexp_free_rt(RegExp re, JSRuntime* rt) {
  js_free_rt(rt, re.source);
}
static inline void
regexp_free(RegExp re, JSContext* ctx) {
  regexp_free_rt(re, JS_GetRuntime(ctx));
}

JSValue js_global_get(JSContext* ctx, const char* prop);
JSValue js_global_prototype(JSContext* ctx, const char* class_name);

enum value_types {
  FLAG_UNDEFINED = 0,
  FLAG_NULL,        // 1
  FLAG_BOOL,        // 2
  FLAG_INT,         // 3
  FLAG_OBJECT,      // 4
  FLAG_STRING,      // 5
  FLAG_SYMBOL,      // 6
  FLAG_BIG_FLOAT,   // 7
  FLAG_BIG_INT,     // 8
  FLAG_BIG_DECIMAL, // 9
  FLAG_FLOAT64,     // 10
  FLAG_NAN,         // 11
  FLAG_FUNCTION,    // 12
  FLAG_ARRAY        // 13
};

enum value_mask {
  TYPE_UNDEFINED = (1 << FLAG_UNDEFINED),
  TYPE_NULL = (1 << FLAG_NULL),
  TYPE_BOOL = (1 << FLAG_BOOL),
  TYPE_INT = (1 << FLAG_INT),
  TYPE_OBJECT = (1 << FLAG_OBJECT),
  TYPE_STRING = (1 << FLAG_STRING),
  TYPE_SYMBOL = (1 << FLAG_SYMBOL),
  TYPE_BIG_FLOAT = (1 << FLAG_BIG_FLOAT),
  TYPE_BIG_INT = (1 << FLAG_BIG_INT),
  TYPE_BIG_DECIMAL = (1 << FLAG_BIG_DECIMAL),
  TYPE_FLOAT64 = (1 << FLAG_FLOAT64),
  TYPE_NAN = (1 << FLAG_NAN),
  TYPE_NUMBER = (TYPE_INT | TYPE_BIG_FLOAT | TYPE_BIG_INT | TYPE_BIG_DECIMAL | TYPE_FLOAT64),
  TYPE_PRIMITIVE = (TYPE_UNDEFINED | TYPE_NULL | TYPE_BOOL | TYPE_INT | TYPE_STRING | TYPE_SYMBOL | TYPE_BIG_FLOAT |
                    TYPE_BIG_INT | TYPE_BIG_DECIMAL | TYPE_NAN),
  TYPE_ALL = (TYPE_PRIMITIVE | TYPE_OBJECT),
  TYPE_FUNCTION = (1 << FLAG_FUNCTION),
  TYPE_ARRAY = (1 << FLAG_ARRAY),
};

int32_t js_value_type_flag(JSValueConst value);
int32_t js_value_type_get(JSContext* ctx, JSValueConst value);

static inline int32_t
js_value_type2flag(uint32_t type) {
  int32_t flag;
  for(flag = 0; (type >>= 1); flag++) {}
  return flag;
}

enum value_mask js_value_type(JSContext* ctx, JSValueConst value);

static inline const char* const*
js_value_types() {
  return (const char* const[]){"UNDEFINED",
                               "NULL",
                               "BOOL",
                               "INT",
                               "OBJECT",
                               "STRING",
                               "SYMBOL",
                               "BIG_FLOAT",
                               "BIG_INT",
                               "BIG_DECIMAL",
                               "FLOAT64",
                               "NAN",
                               "FUNCTION",
                               "ARRAY",
                               0};
}

const char* js_value_type_name(int32_t type);

const char* js_value_typestr(JSContext* ctx, JSValueConst value);

VISIBLE void* js_value_get_ptr(JSValue v);
VISIBLE int32_t js_value_get_tag(JSValue v);
BOOL js_value_has_ref_count(JSValue v);

void js_value_free(JSContext* ctx, JSValue v);
void js_value_free_rt(JSRuntime* rt, JSValue v);

BOOL js_value_equals(JSContext* ctx, JSValueConst a, JSValueConst b);
void js_value_dump(JSContext* ctx, JSValueConst value, DynBuf* db);
void js_value_print(JSContext* ctx, JSValueConst value);
JSValue js_value_clone(JSContext* ctx, JSValueConst valpe);
JSValue* js_values_dup(JSContext* ctx, int nvalues, JSValueConst* values);
void js_values_free(JSRuntime* rt, int nvalues, JSValueConst* values);
JSValue js_values_toarray(JSContext* ctx, int nvalues, JSValueConst* values);

typedef struct InputBuffer {
  uint8_t* data;
  size_t size;
  size_t pos;
  void (*free)(JSContext*, const char*, JSValue);
  JSValue value;
} InputBuffer;

static inline void
input_buffer_free_default(JSContext* ctx, const char* str, JSValue val) {
  if(!JS_IsUndefined(val))
    JS_FreeValue(ctx, val);
}

InputBuffer js_input_buffer(JSContext* ctx, JSValueConst value);
InputBuffer input_buffer_dup(const InputBuffer* in, JSContext* ctx);
BOOL input_buffer_valid(const InputBuffer* in);
void input_buffer_dump(const InputBuffer* in, DynBuf* db);
void input_buffer_free(InputBuffer* in, JSContext* ctx);
const uint8_t* input_buffer_get(InputBuffer* in, size_t* lenp);
const uint8_t* input_buffer_peek(InputBuffer* in, size_t* lenp);

static inline int
input_buffer_peekc(InputBuffer* in, size_t* lenp) {
  const uint8_t *pos, *end, *next;
  int cp;
  pos = in->data + in->pos;
  end = in->data + in->size;
  cp = unicode_from_utf8(pos, end - pos, &next);
  if(lenp)
    *lenp = next - pos;
  return cp;
}

static inline int
input_buffer_getc(InputBuffer* in) {
  size_t n;
  int ret;
  ret = input_buffer_peekc(in, &n);
  in->pos += n;
  return ret;
}

static inline const uint8_t*
input_buffer_begin(const InputBuffer* in) {
  return in->data;
}
static inline const uint8_t*
input_buffer_end(const InputBuffer* in) {
  return in->data + in->size;
}
static inline BOOL
input_buffer_eof(const InputBuffer* in) {
  return in->pos == in->size;
}
static inline size_t
input_buffer_remain(const InputBuffer* in) {
  return in->size - in->pos;
}

static inline const char*
js_cstring_new(JSContext* ctx, const char* str) {
  JSValue v = JS_NewString(ctx, str);
  const char* s = JS_ToCString(ctx, v);
  JS_FreeValue(ctx, v);
  return s;
}
static inline const char*
js_cstring_newlen(JSContext* ctx, const char* str, size_t len) {
  JSValue v = JS_NewStringLen(ctx, str, len);
  const char* s = JS_ToCString(ctx, v);
  JS_FreeValue(ctx, v);
  return s;
}

char* js_cstring_dup(JSContext* ctx, const char* str);
static inline void
js_cstring_free(JSContext* ctx, const char* ptr) {
  if(!ptr)
    return;

  JS_FreeValue(ctx, JS_MKPTR(JS_TAG_STRING, (void*)(ptr - offsetof(JSString, u))));
}
JSValueConst js_cstring_value(const char* ptr);
void js_cstring_dump(JSContext* ctx, JSValueConst value, DynBuf* db);

static inline char*
js_tostringlen(JSContext* ctx, size_t* lenp, JSValueConst value) {
  size_t len;
  const char* cstr;
  char* ret = 0;
  if((cstr = JS_ToCStringLen(ctx, &len, value))) {
    ret = js_strndup(ctx, cstr, len);
    if(lenp)
      *lenp = len;
    js_cstring_free(ctx, cstr);
  }
  return ret;
}

static inline char*
js_tostring(JSContext* ctx, JSValueConst value) {
  return js_tostringlen(ctx, 0, value);
}

static inline JSValue
js_value_tostring(JSContext* ctx, const char* class_name, JSValueConst value) {
  JSAtom atom;
  JSValue proto, tostring, str;
  proto = js_global_prototype(ctx, class_name);
  atom = JS_NewAtom(ctx, "toString");
  tostring = JS_GetProperty(ctx, proto, atom);
  JS_FreeValue(ctx, proto);
  JS_FreeAtom(ctx, atom);
  str = JS_Call(ctx, tostring, value, 0, 0);
  JS_FreeValue(ctx, tostring);
  return str;
}
int js_value_to_size(JSContext* ctx, size_t* sz, JSValueConst value);
JSValue js_value_from_char(JSContext* ctx, int c);
static inline int
js_value_cmpstring(JSContext* ctx, JSValueConst value, const char* other) {
  const char* str = JS_ToCString(ctx, value);
  int ret = strcmp(str, other);
  JS_FreeCString(ctx, str);
  return ret;
}

#define JS_VALUE_FREE(ctx, value)                                                                                      \
  do {                                                                                                                 \
    JS_FreeValue((ctx), (value));                                                                                      \
    (value) = JS_UNDEFINED;                                                                                            \
  } while(0);
#define JS_VALUE_FREE_RT(ctx, value)                                                                                   \
  do {                                                                                                                 \
    JS_FreeValueRT((ctx), (value));                                                                                    \
    (value) = JS_UNDEFINED;                                                                                            \
  } while(0);

#define js_object_tmpmark_set(value)                                                                                   \
  do { ((uint8_t*)JS_VALUE_GET_OBJ((value)))[5] |= 0x40; } while(0);
#define js_object_tmpmark_clear(value)                                                                                 \
  do { ((uint8_t*)JS_VALUE_GET_OBJ((value)))[5] &= ~0x40; } while(0);
#define js_object_tmpmark_isset(value) (((uint8_t*)JS_VALUE_GET_OBJ((value)))[5] & 0x40)

#define js_runtime_exception_set(rt, value)                                                                            \
  do { *(JSValue*)((uint8_t*)(rt) + 216) = value; } while(0);
#define js_runtime_exception_get(rt) (*(JSValue*)((uint8_t*)(rt) + 216))
#define js_runtime_exception_clear(rt)                                                                                 \
  do {                                                                                                                 \
    if(!JS_IsNull(js_runtime_exception_get(rt)))                                                                       \
      JS_FreeValueRT((rt), js_runtime_exception_get(rt));                                                              \
    js_runtime_exception_set(rt, JS_NULL);                                                                             \
  } while(0)

void js_propertyenums_free(JSContext* ctx, JSPropertyEnum* props, size_t len);
void js_propertydescriptor_free(JSContext* ctx, JSPropertyDescriptor* desc);

JSValue js_symbol_ctor(JSContext* ctx);

JSValue js_symbol_invoke_static(JSContext* ctx, const char* name, JSValueConst arg);

JSValue js_symbol_to_string(JSContext* ctx, JSValueConst sym);

const char* js_symbol_to_cstring(JSContext* ctx, JSValueConst sym);

JSValue js_symbol_get_static(JSContext* ctx, const char* name);
JSAtom js_symbol_atom(JSContext* ctx, const char* name);
BOOL js_is_iterable(JSContext* ctx, JSValueConst obj);
JSValue js_iterator_method(JSContext* ctx, JSValueConst obj);
JSValue js_iterator_new(JSContext* ctx, JSValueConst obj);
IteratorValue js_iterator_next(JSContext* ctx, JSValueConst obj);
JSValue js_symbol_for(JSContext* ctx, const char* sym_for);
JSAtom js_symbol_for_atom(JSContext* ctx, const char* sym_for);

JSValue js_symbol_operatorset_value(JSContext* ctx);

JSAtom js_symbol_operatorset_atom(JSContext* ctx);

JSValue js_operators_create(JSContext* ctx);

static inline int64_t
js_int64_default(JSContext* ctx, JSValueConst value, int64_t i) {
  if(JS_IsNumber(value))
    JS_ToInt64(ctx, &i, value);
  return i;
}

static inline JSValue
js_new_number(JSContext* ctx, int32_t n) {
  if(n == INT32_MAX)
    return JS_NewFloat64(ctx, INFINITY);
  return JS_NewInt32(ctx, n);
}

static inline JSValue
js_new_bool_or_number(JSContext* ctx, int32_t n) {
  if(n == 0)
    return JS_NewBool(ctx, FALSE);
  return js_new_number(ctx, n);
}

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)

#define js_atom_isint(i) ((JSAtom)((i)&JS_ATOM_TAG_INT))
#define js_atom_fromint(i) ((JSAtom)((i)&JS_ATOM_MAX_INT) | JS_ATOM_TAG_INT)
#define js_atom_toint(i) (unsigned int)(((JSAtom)(i) & (~(JS_ATOM_TAG_INT))))

int js_atom_toint64(JSContext* ctx, int64_t* i, JSAtom atom);
int32_t js_atom_toint32(JSContext* ctx, JSAtom atom);
JSValue js_atom_tovalue(JSContext* ctx, JSAtom atom);

unsigned int js_atom_tobinary(JSAtom atom);
const char* js_atom_to_cstringlen(JSContext* ctx, size_t* len, JSAtom atom);
void js_atom_dump(JSContext* ctx, JSAtom atom, DynBuf* db, BOOL color);
const char* js_object_tostring(JSContext* ctx, JSValueConst value);
const char* js_function_name(JSContext* ctx, JSValueConst value);
char* js_object_classname(JSContext* ctx, JSValueConst value);
int js_object_is(JSContext* ctx, JSValueConst value, const char* cmp);
JSValue js_object_construct(JSContext* ctx, JSValueConst ctor);
JSValue js_object_error(JSContext* ctx, const char* message);
JSValue js_object_stack(JSContext* ctx);

static inline BOOL
js_object_same(JSValueConst a, JSValueConst b) {
  JSObject *aobj, *bobj;
  if(!JS_IsObject(a) || !JS_IsObject(b))
    return FALSE;

  aobj = JS_VALUE_GET_OBJ(a);
  bobj = JS_VALUE_GET_OBJ(b);
  return aobj == bobj;
}

BOOL js_has_propertystr(JSContext* ctx, JSValueConst obj, const char* str);
BOOL js_get_propertystr_bool(JSContext* ctx, JSValueConst obj, const char* str);
void js_set_propertyint_string(JSContext* ctx, JSValueConst obj, uint32_t i, const char* str);
void js_set_propertyint_int(JSContext* ctx, JSValueConst obj, uint32_t i, int32_t value);
void js_set_propertystr_string(JSContext* ctx, JSValueConst obj, const char* prop, const char* str);
void js_set_propertystr_stringlen(JSContext* ctx, JSValueConst obj, const char* prop, const char* str, size_t len);
const char* js_get_propertyint_cstring(JSContext* ctx, JSValueConst obj, uint32_t i);
const char* js_get_propertystr_cstring(JSContext* ctx, JSValueConst obj, const char* prop);
const char* js_get_propertystr_cstringlen(JSContext* ctx, JSValueConst obj, const char* prop, size_t* lenp);
char* js_get_propertystr_string(JSContext* ctx, JSValueConst obj, const char* prop);
char* js_get_propertystr_stringlen(JSContext* ctx, JSValueConst obj, const char* prop, size_t* lenp);
int32_t js_get_propertystr_int32(JSContext* ctx, JSValueConst obj, const char* prop);
uint64_t js_get_propertystr_uint64(JSContext* ctx, JSValueConst obj, const char* prop);

static inline void
js_set_inspect_method(JSContext* ctx, JSValueConst obj, JSCFunction* func) {
  JSAtom inspect_symbol = js_symbol_for_atom(ctx, "quickjs.inspect.custom");
  JS_SetProperty(ctx, obj, inspect_symbol, JS_NewCFunction(ctx, func, "inspect", 1));
  JS_FreeAtom(ctx, inspect_symbol);
}

int js_class_id(JSContext* ctx, int id);

static inline BOOL
js_object_isclass(JSValue obj, int32_t class_id) {
  return JS_GetOpaque(obj, class_id) != 0;
}

static inline BOOL
js_value_isclass(JSContext* ctx, JSValue obj, int id) {
  return js_object_isclass(obj, js_class_id(ctx, id));
}

BOOL js_is_arraybuffer(JSContext*, JSValue);
BOOL js_is_sharedarraybuffer(JSContext*, JSValue);
BOOL js_is_map(JSContext*, JSValue);
BOOL js_is_set(JSContext*, JSValue);
BOOL js_is_generator(JSContext*, JSValue);
BOOL js_is_regexp(JSContext*, JSValue);
BOOL js_is_promise(JSContext*, JSValue);

BOOL js_is_typedarray(JSContext* ctx, JSValueConst value);

JSValue js_typedarray_prototype(JSContext* ctx);
JSValue js_typedarray_constructor(JSContext* ctx);

static inline BOOL
js_is_array(JSContext* ctx, JSValueConst value) {
  return JS_IsArray(ctx, value) || js_is_typedarray(ctx, value);
}

BOOL js_is_input(JSContext* ctx, JSValueConst value);

int js_propenum_cmp(const void* a, const void* b, void* ptr);
BOOL js_object_equals(JSContext* ctx, JSValueConst a, JSValueConst b);
int64_t js_array_length(JSContext* ctx, JSValueConst array);

size_t js_argv_length(char** strv);

char** js_argv_dup(JSContext* ctx, char** strv);

void js_argv_free(JSContext* ctx, char** strv);
JSValue js_argv_to_array(JSContext* ctx, char** strv);
JSValue js_intv_to_array(JSContext* ctx, int* intv);
char** js_array_to_argv(JSContext* ctx, int* argcp, JSValueConst array);

JSValue js_module_name(JSContext*, JSValueConst);
char* js_module_namestr(JSContext* ctx, JSValueConst value);

JSValue js_invoke(JSContext* ctx, JSValueConst this_obj, const char* method, int argc, JSValueConst* argv);

#endif /* defined(UTILS_H) */
