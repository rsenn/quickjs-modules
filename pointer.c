#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "quickjs-pointer.h"
#include "pointer.h"
#include "utils.h"

void
pointer_reset(Pointer* ptr, JSContext* ctx) {
  size_t i;

  if(ptr->atoms) {
    for(i = 0; i < ptr->n; i++) JS_FreeAtom(ctx, ptr->atoms[i]);
    js_free(ctx, ptr->atoms);
    ptr->atoms = 0;
  }
  ptr->n = 0;
}

void
pointer_copy(Pointer* dst, Pointer* src, JSContext* ctx) {
  if(dst->n)
    pointer_reset(dst, ctx);

  if((dst->atoms = js_mallocz(ctx, sizeof(JSAtom) * src->n))) {
    size_t i;
    dst->n = src->n;
    for(i = 0; i < src->n; i++) dst->atoms[i] = JS_DupAtom(ctx, src->atoms[i]);
  }
}

void
pointer_truncate(Pointer* ptr, JSContext* ctx, size_t size) {
  if(size == 0) {
    pointer_reset(ptr, ctx);
    return;
  }
  if(ptr->atoms) {
    size_t i;
    for(i = ptr->n - 1; i >= size; i--) JS_FreeAtom(ctx, ptr->atoms[i]);

    ptr->atoms = js_realloc(ctx, ptr->atoms, sizeof(JSAtom) * size);
    ptr->n = size;
  }
}

#define pointer_color(s)                                                                                               \
  (/*(index) >= 0 &&*/ (i) >= (index) ? "\x1b[31m" : (is_integer(s) ? "\x1b[1;30m" : "\x1b[0;33m"))

void
pointer_dump(Pointer* ptr, JSContext* ctx, DynBuf* db, BOOL color, size_t index) {
  size_t i;
  const char* s;
  for(i = 0; i < ptr->n; i++) {
    BOOL is_int;
    s = JS_AtomToCString(ctx, ptr->atoms[i]);
    is_int = is_integer(s);
    dbuf_putstr(db, color ? (is_int ? "\x1b[1;36m[" : "\x1b[1;36m.") : (is_integer(s) ? "[" : "."));
    dbuf_putstr(db, color ? pointer_color(s) : "");
    dbuf_putstr(db, s);
    if(is_int)
      dbuf_putstr(db, color ? "\x1b[1;36m]" : "]");
    js_cstring_free(ctx, s);
  }
  dbuf_putstr(db, color ? "\x1b[m" : "");
}

void
pointer_debug(Pointer* ptr, JSContext* ctx) {
  DynBuf db;
  js_dbuf_init(ctx, &db);
  pointer_dump(ptr, ctx, &db, TRUE, -1);
  dbuf_0(&db);

  puts((const char*)db.buf);

  dbuf_free(&db);
}

void
pointer_tostring(Pointer* ptr, JSContext* ctx, DynBuf* db) {
  size_t i, j;
  const char* str;
  for(i = 0; i < ptr->n; i++) {
    if(js_atom_isint(ptr->atoms[i])) {
      dbuf_printf(db, "[%" PRIu32 "]", js_atom_toint(ptr->atoms[i]));
      continue;
    }
    if(i > 0)
      dbuf_putc(db, '.');
    str = JS_AtomToCString(ctx, ptr->atoms[i]);
    for(j = 0; str[j]; j++) {
      if(str[j] == '.')
        dbuf_putc(db, '\\');
      dbuf_putc(db, str[j]);
    }
  }
}

JSValue
pointer_toarray(Pointer* ptr, JSContext* ctx) {
  size_t i;
  JSValue array = JS_NewArray(ctx);
  for(i = 0; i < ptr->n; i++) { JS_SetPropertyUint32(ctx, array, i, js_atom_tovalue(ctx, ptr->atoms[i])); }
  return array;
}
size_t
pointer_parse(Pointer* ptr, JSContext* ctx, const char* str, size_t len) {
  size_t start, delim, n;
  JSAtom atom;
  char* endptr;
  unsigned long val;

  while(len) {
    char c = str[0];
    start = c == '[' ? 1 : 0;
    delim = start;
    for(;;) {
      delim += byte_chrs(&str[delim], len - delim, c == '[' ? "." : ".[", c == '[' ? 2 : 2);
      if(delim < len && delim > 0 && str[delim - 1] == '\\') {
        ++delim;
        continue;
      }
      break;
    }

    n = delim - start;
    endptr = 0;
    val = 0;
    if(delim && str[delim - 1] == ']') {
      n--;
    }
    if(is_digit_char(str[start]))
      val = strtoul(&str[start], &endptr, 10);

    if(endptr == &str[start + n])
      atom = js_atom_fromint(val);
    else
      atom = JS_NewAtomLen(ctx, &str[start], n);

    pointer_push(ptr, atom);

    str += delim;
    len -= delim;

    if(len > 0) {
      ++str;
      --len;
    }
  }
  return ptr->n;
}

Pointer*
pointer_slice(Pointer* ptr, JSContext* ctx, int64_t start, int64_t end) {
  Pointer* ret = js_mallocz(ctx, sizeof(Pointer));
  int64_t i;

  start = mod_int32(start, ptr->n);
  end = mod_int32(end, ptr->n);
  if(end == 0)
    end = ptr->n;

  ret->n = end - start;
  ret->atoms = ret->n ? js_mallocz(ctx, sizeof(JSAtom) * ret->n) : 0;

  for(i = start; i < end; i++) ret->atoms[i - start] = JS_DupAtom(ctx, ptr->atoms[i]);

  return ret;
}

JSValue
pointer_shift(Pointer* ptr, JSContext* ctx, JSValueConst obj) {
  JSValue ret = JS_UNDEFINED;
  if(ptr->n) {
    JSAtom atom;
    size_t i;
    atom = ptr->atoms[0];
    for(i = 1; i < ptr->n; i++) { ptr->atoms[i - 1] = ptr->atoms[i]; }
    ptr->n--;
    ret = JS_GetProperty(ctx, obj, atom);
    JS_FreeAtom(ctx, atom);
  }
  return ret;
}

JSValue
pointer_deref(Pointer* ptr, JSContext* ctx, JSValueConst arg) {
  size_t i;
  JSValue obj = JS_DupValue(ctx, arg);

  for(i = 0; i < ptr->n; i++) {
    JSAtom atom = ptr->atoms[i]; // JS_DupAtom(ctx, ptr->atoms[i]);

    if(!JS_HasProperty(ctx, obj, atom)) {
      DynBuf dbuf;
      js_dbuf_init(ctx, &dbuf);

      pointer_dump(ptr, ctx, &dbuf, TRUE, i);
      dbuf_0(&dbuf);
      obj = JS_ThrowReferenceError(ctx, "%s", dbuf.buf);
      dbuf_free(&dbuf);
      break;
    }

    JSValue child = JS_GetProperty(ctx, obj, atom);
    JS_FreeValue(ctx, obj);

    obj = child;
  }
  return obj;
}

JSValue
pointer_acquire(Pointer* ptr, JSContext* ctx, JSValueConst arg) {
  size_t i;
  JSValue child, obj = JS_DupValue(ctx, arg);

  for(i = 0; i < ptr->n; i++) {
    JSAtom atom = ptr->atoms[i]; // JS_DupAtom(ctx, ptr->atoms[i]);

    if(!JS_HasProperty(ctx, obj, atom)) {
      BOOL is_array = i + 1 < ptr->n && js_atom_isint(ptr->atoms[i + 1]);
      child = is_array ? JS_NewArray(ctx) : JS_NewObject(ctx);
      JS_SetProperty(ctx, obj, atom, JS_DupValue(ctx, child));
    } else {
      child = JS_GetProperty(ctx, obj, atom);
    }
    JS_FreeValue(ctx, obj);

    obj = child;
  }
  return obj;
}

void
pointer_fromstring(Pointer* ptr, JSContext* ctx, JSValueConst value) {
  size_t len;
  const char* str;
  str = JS_ToCStringLen(ctx, &len, value);
  pointer_parse(ptr, ctx, str, len);
  js_cstring_free(ctx, str);
}

void
pointer_fromarray(Pointer* ptr, JSContext* ctx, JSValueConst array) {
  size_t i, len;
  JSValueConst prop;
  len = js_array_length(ctx, array);
  pointer_reset(ptr, ctx);

  assert(len > 0);

  ptr->atoms = js_malloc(ctx, sizeof(JSAtom) * len);
  for(i = 0; i < len; i++) {
    prop = JS_GetPropertyUint32(ctx, array, i);
    ptr->atoms[i] = JS_ValueToAtom(ctx, prop);
    JS_FreeValue(ctx, prop);
  }
  ptr->n = len;
}

void
pointer_fromiterable(Pointer* ptr, JSContext* ctx, JSValueConst arg) {
  IteratorValue item;
  JSValue iter = js_iterator_new(ctx, arg);

  pointer_reset(ptr, ctx);

  for(;;) {
    item = js_iterator_next(ctx, iter);
    if(item.done)
      break;
    pointer_push(ptr, JS_ValueToAtom(ctx, item.value));
    JS_FreeValue(ctx, item.value);
  }
  JS_FreeValue(ctx, iter);
}

int
pointer_from(Pointer* ptr, JSContext* ctx, JSValueConst value, DataFunc* data) {
  Pointer* ptr2;

  if(data && (ptr2 = data(ctx, value)))
    pointer_copy(ptr, ptr2, ctx);
  else if(JS_IsString(value))
    pointer_fromstring(ptr, ctx, value);
  else if(JS_IsArray(ctx, value))
    pointer_fromarray(ptr, ctx, value);
  else if(!JS_IsUndefined(value))
    return 0;

  return 1;
}
