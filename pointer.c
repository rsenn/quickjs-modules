#define _GNU_SOURCE

#include "quickjs.h"
#include "cutils.h"
#include "libregexp.h"
#include <string.h>

JSClassID js_pointer_class_id;
JSClassID js_dereferenceerror_class_id;
JSValue pointer_proto, pointer_ctor, pointer_class;
JSValue dereferenceerror_proto, dereferenceerror_ctor, dereferenceerror_class;

enum pointer_methods { DEREF = 0, TO_STRING, TO_ARRAY, INSPECT, SHIFT, SLICE, KEYS, VALUES };
enum pointer_getters { PROP_LENGTH = 0, PROP_PATH };

typedef struct {
  int64_t n;
  JSAtom* atoms;
} Pointer;

typedef struct {
  BOOL done;
  JSValue value;
} IteratorValue;
#define is_digit_char(c) ((c) >= '0' && (c) <= '9')

static int
is_integer(const char* str) {
  if(!(*str >= '1' && *str <= '9') && !(*str == '0' && str[1] == '\0'))
    return 0;
  while(*++str) {
    if(!is_digit_char(*str))
      return 0;
  }
  return 1;
}

static void
js_atom_dump(JSContext* ctx, JSAtom atom, DynBuf* db, BOOL color) {
  const char* str;
  str = JS_AtomToCString(ctx, atom);

  if(color)
    dbuf_putstr(db, is_integer(str) ? "\x1b[33m" : "\x1b[1;30m");

  dbuf_putstr(db, str);
  if(color)
    dbuf_putstr(db, "\x1b[m");

  JS_FreeCString(ctx, str);
}

static JSValue
js_symbol_get_static(JSContext* ctx, const char* name) {
  JSValue global_obj, symbol_ctor, ret;
  global_obj = JS_GetGlobalObject(ctx);
  symbol_ctor = JS_GetPropertyStr(ctx, global_obj, "Symbol");
  ret = JS_GetPropertyStr(ctx, symbol_ctor, name);

  JS_FreeValue(ctx, symbol_ctor);
  JS_FreeValue(ctx, global_obj);
  return ret;
}

static JSAtom
js_symbol_atom(JSContext* ctx, const char* name) {
  JSValue sym = js_symbol_get_static(ctx, name);
  JSAtom ret = JS_ValueToAtom(ctx, sym);
  JS_FreeValue(ctx, sym);
  return ret;
}

static JSValue
js_iterator_method(JSContext* ctx, JSValueConst obj) {
  JSAtom atom;
  JSValue ret = JS_UNDEFINED;
  atom = js_symbol_atom(ctx, "iterator");
  if(JS_HasProperty(ctx, obj, atom))
    ret = JS_GetProperty(ctx, obj, atom);
  JS_FreeAtom(ctx, atom);
  if(!JS_IsFunction(ctx, ret)) {
    atom = js_symbol_atom(ctx, "asyncIterator");
    if(JS_HasProperty(ctx, obj, atom))
      ret = JS_GetProperty(ctx, obj, atom);
    JS_FreeAtom(ctx, atom);
  }
  return ret;
}

static JSValue
js_iterator_new(JSContext* ctx, JSValueConst obj) {
  JSValue fn, ret;
  fn = js_iterator_method(ctx, obj);

  ret = JS_Call(ctx, fn, obj, 0, 0);
  JS_FreeValue(ctx, fn);
  return ret;
}

static IteratorValue
js_iterator_next(JSContext* ctx, JSValueConst obj) {
  JSValue fn, result, done;
  IteratorValue ret;

  fn = JS_GetPropertyStr(ctx, obj, "next");

  result = JS_Call(ctx, fn, obj, 0, 0);
  JS_FreeValue(ctx, fn);

  done = JS_GetPropertyStr(ctx, result, "done");
  ret.value = JS_GetPropertyStr(ctx, result, "value");
  JS_FreeValue(ctx, result);

  ret.done = JS_ToBool(ctx, done);
  JS_FreeValue(ctx, done);

  return ret;
}

static int64_t
js_int64_default(JSContext* ctx, JSValueConst value, int64_t i) {
  if(!JS_IsUndefined(value))
    JS_ToInt64(ctx, &i, value);
  return i;
}

static void
pointer_reset(Pointer* ptr, JSContext* ctx) {
  size_t i;

  if(ptr->atoms) {
    for(i = 0; i < ptr->n; i++) JS_FreeAtom(ctx, ptr->atoms[i]);
    free(ptr->atoms);
    ptr->atoms = 0;
  }
  ptr->n = 0;
}

static void
pointer_atom_add(Pointer* ptr, JSContext* ctx, JSAtom atom) {
  ptr->atoms = realloc(ptr->atoms, (ptr->n + 1) * sizeof(JSAtom));
  ptr->atoms[ptr->n++] = atom;
}

static void
pointer_dump(Pointer* ptr, JSContext* ctx, DynBuf* db, BOOL color) {
  size_t i;

  dbuf_printf(db, "Pointer(%lu) ", ptr->n);

  for(i = 0; i < ptr->n; i++) {
    if(i > 0)
      dbuf_putstr(db, color ? "\x1b[1;36m.\x1b[m" : ".");
    js_atom_dump(ctx, ptr->atoms[i], db, color);
  }
}

static Pointer*
pointer_slice(Pointer* ptr, JSContext* ctx, int64_t start, int64_t end) {
  Pointer* ret = js_mallocz(ctx, sizeof(Pointer));
  int64_t i;

  start = start < 0 ? (start % ptr->n) + ptr->n : start % ptr->n;
  end = end < 0 ? (end % ptr->n) + ptr->n : end % ptr->n;
  if(end == 0)
    end = ptr->n;

  ret->n = end - start;
  ret->atoms = ret->n ? malloc(sizeof(JSAtom) * ret->n) : 0;

  for(i = start; i < end; i++) ret->atoms[i - start] = JS_DupAtom(ctx, ptr->atoms[i]);

  return ret;
}

static JSValue
pointer_shift(Pointer* ptr, JSContext* ctx, JSValueConst obj) {
  JSValue ret;
  if(ptr->n) {
    JSAtom atom;
    int64_t i;
    atom = ptr->atoms[0];
    for(i = 1; i < ptr->n; i++) { ptr->atoms[i - 1] = ptr->atoms[i]; }
    ptr->n--;
    ret = JS_GetProperty(ctx, obj, atom);
    JS_FreeAtom(ctx, atom);
  }
  return ret;
}

static Pointer*
js_pointer_data(JSContext* ctx, JSValueConst value) {
  return JS_GetOpaque2(ctx, value, js_pointer_class_id);
}

static JSValue
js_pointer_new(JSContext* ctx, Pointer* ptr) {

  JSValue obj;

  obj = JS_NewObjectProtoClass(ctx, pointer_proto, js_pointer_class_id);

  JS_SetOpaque(obj, ptr);
  return obj;
}

static JSValue
js_pointer_tostring(JSContext* ctx, JSValueConst this_val, BOOL color) {
  Pointer* ptr;
  DynBuf dbuf;
  JSValue ret;
  if(!(ptr = JS_GetOpaque2(ctx, this_val, js_pointer_class_id)))
    return JS_EXCEPTION;

  dbuf_init(&dbuf);

  pointer_dump(ptr, ctx, &dbuf, color);
  // dbuf_put(&dbuf, "\0", 1);
  ret = JS_NewStringLen(ctx, dbuf.buf, dbuf.size);
  dbuf_free(&dbuf);

  return ret;
}

static JSValue
js_dereferenceerror_ctor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  Pointer* ptr;
  JSValue obj = JS_UNDEFINED;
  JSValue proto;

  ptr = pointer_slice(js_pointer_data(ctx, argv[0]), ctx, 0, js_int64_default(ctx, argv[1], 0));

  /* using new_target to get the prototype is necessary when the
     class is extended. */
  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if(JS_IsException(proto))
    proto = JS_DupValue(ctx, dereferenceerror_proto);

  obj = JS_NewObjectProtoClass(ctx, proto, js_dereferenceerror_class_id);
  JS_FreeValue(ctx, proto);
  if(JS_IsException(obj))
    goto fail;
  JS_SetOpaque(obj, ptr);

  return obj;
fail:
  js_free(ctx, ptr);
  JS_FreeValue(ctx, obj);
  return JS_EXCEPTION;
}

static JSValue
js_dereferenceerror_tostring(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  Pointer* ptr;
  DynBuf dbuf;
  JSValue ret;
  if(!(ptr = JS_GetOpaque2(ctx, this_val, js_dereferenceerror_class_id)))
    return JS_EXCEPTION;

  dbuf_init(&dbuf);

  dbuf_putstr(&dbuf, "DereferenceError ");

  pointer_dump(ptr, ctx, &dbuf, FALSE);
  ret = JS_NewStringLen(ctx, dbuf.buf, dbuf.size);
  dbuf_free(&dbuf);

  return ret;
}

static JSValue
js_dereferenceerror_method(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
  Pointer* ptr;

  if(!(ptr = JS_GetOpaque2(ctx, this_val, js_dereferenceerror_class_id)))
    return JS_EXCEPTION;

  switch(magic) {

    case TO_STRING: {
      return js_dereferenceerror_tostring(ctx, this_val, argc, argv);
    }
  }

  return JS_EXCEPTION;
}

static JSValue
js_dereferenceerror_throw(JSContext* ctx, Pointer* ptr, int64_t index) {
  JSValue obj = JS_NewObjectProtoClass(ctx, dereferenceerror_proto, js_dereferenceerror_class_id);

  JS_SetOpaque(obj, pointer_slice(ptr, ctx, 0, index + 1));
  return JS_Throw(ctx, obj);
}

static JSValue
js_dereferenceerror_get(JSContext* ctx, JSValueConst this_val, int magic) {
  Pointer* ptr;

  if(!(ptr = JS_GetOpaque2(ctx, this_val, js_dereferenceerror_class_id)))
    return JS_EXCEPTION;

  return js_pointer_new(ctx, ptr);
}
static JSValue
js_pointer_toarray(JSContext* ctx, Pointer* ptr) {
  size_t i;
  JSValue array = JS_NewArray(ctx);
  for(i = 0; i < ptr->n; i++) JS_SetPropertyUint32(ctx, array, i, JS_AtomToValue(ctx, ptr->atoms[i]));
  return array;
}

static void
js_pointer_fromarray(JSContext* ctx, Pointer* ptr, JSValueConst array) {
  int64_t i, len;
  JSValue prop;
  JSAtom atom;
  prop = JS_GetPropertyStr(ctx, array, "length");
  JS_ToInt64(ctx, &len, prop);
  JS_FreeValue(ctx, prop);
  pointer_reset(ptr, ctx);
  ptr->atoms = malloc(sizeof(JSAtom) * len);
  for(i = 0; i < len; i++) {
    prop = JS_GetPropertyUint32(ctx, array, i);
    ptr->atoms[i] = JS_ValueToAtom(ctx, prop);
    JS_FreeValue(ctx, prop);
  }
}

static void
js_pointer_fromiterable(JSContext* ctx, Pointer* ptr, JSValueConst arg) {
  IteratorValue item;
  JSValue iter = js_iterator_new(ctx, arg);

  pointer_reset(ptr, ctx);

  for(;;) {
    item = js_iterator_next(ctx, iter);
    if(item.done)
      break;
    pointer_atom_add(ptr, ctx, JS_ValueToAtom(ctx, item.value));
    JS_FreeValue(ctx, item.value);
  }
}

static JSValue
js_pointer_ctor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  Pointer* ptr;
  JSValue obj = JS_UNDEFINED;
  JSValue proto;

  if(!(ptr = js_mallocz(ctx, sizeof(Pointer))))
    return JS_EXCEPTION;

  pointer_reset(ptr, ctx);

  /* using new_target to get the prototype is necessary when the
     class is extended. */
  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if(JS_IsException(proto))
    proto = JS_DupValue(ctx, pointer_proto);

  obj = JS_NewObjectProtoClass(ctx, proto, js_pointer_class_id);
  JS_FreeValue(ctx, proto);
  if(JS_IsException(obj))
    goto fail;
  JS_SetOpaque(obj, ptr);

  if(argc > 0) {
    size_t i;

    if(JS_IsArray(ctx, argv[0])) {
      int64_t len;
      JSValue length = JS_GetPropertyStr(ctx, argv[0], "length");
      JS_ToInt64(ctx, &len, length);
      JS_FreeValue(ctx, length);
      for(i = 0; i < len; i++) {
        JSValue arg = JS_GetPropertyUint32(ctx, argv[0], i);
        JSAtom atom;
        atom = JS_ValueToAtom(ctx, arg);
        pointer_atom_add(ptr, ctx, atom);
        JS_FreeValue(ctx, arg);
      }
    } else {

      for(i = 0; i < argc; i++) {
        JSAtom atom;
        atom = JS_ValueToAtom(ctx, argv[i]);
        pointer_atom_add(ptr, ctx, atom);
        // JS_FreeAtom(ctx, atom);
      }
    }
  }

  return obj;
fail:
  js_free(ctx, ptr);
  JS_FreeValue(ctx, obj);
  return JS_EXCEPTION;
}

static JSValue
js_pointer_next(JSContext* ctx, Pointer* ptr, JSValueConst this_arg, JSValueConst obj) {
  size_t i;
  for(i = 0; i < ptr->n; i++) {
    JSAtom atom = ptr->atoms[i];

    obj = JS_GetProperty(ctx, obj, atom);

    if(JS_IsException(obj)) {
      JS_Throw(ctx, js_dereferenceerror_ctor(ctx, JS_UNDEFINED, 1, &obj));
      break;
    }
  }
  return obj;
}

static JSValue
js_pointer_deref(JSContext* ctx, Pointer* ptr, JSValueConst this_arg, JSValueConst obj) {
  size_t i;
  for(i = 0; i < ptr->n; i++) {
    JSAtom atom = ptr->atoms[i];

    obj = JS_GetProperty(ctx, obj, atom);

    if(JS_IsException(obj)) {
      js_dereferenceerror_throw(ctx, ptr, i);

      break;
    }
  }
  return obj;
}

static JSValue
js_pointer_method(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
  Pointer* ptr;

  if(!(ptr = JS_GetOpaque2(ctx, this_val, js_pointer_class_id)))
    return JS_EXCEPTION;

  switch(magic) {
    case DEREF: return js_pointer_deref(ctx, ptr, this_val, argv[0]);

    case TO_STRING: return js_pointer_tostring(ctx, this_val, FALSE);
    case TO_ARRAY: return js_pointer_toarray(ctx, ptr);
    case INSPECT: return js_pointer_tostring(ctx, this_val, TRUE);
    case SLICE:
      return js_pointer_new(
          ctx, pointer_slice(ptr, ctx, js_int64_default(ctx, argv[0], 0), js_int64_default(ctx, argv[1], 0)));
    case KEYS: {
      JSValue array = js_pointer_toarray(ctx, ptr);
      JSValue iter = js_iterator_new(ctx, array);
      JS_FreeValue(ctx, array);
      return iter;
    }
    case VALUES: {
      JSValue array = js_pointer_toarray(ctx, ptr);
      JSValue iter = js_iterator_new(ctx, array);
      JS_FreeValue(ctx, array);
      return iter;
    }
    case SHIFT: {
      return pointer_shift(ptr, ctx, argv[0]);
    }
  }
  return JS_EXCEPTION;
}

static JSValue
js_pointer_get(JSContext* ctx, JSValueConst this_val, int magic) {
  Pointer* ptr;

  if(!(ptr = JS_GetOpaque2(ctx, this_val, js_pointer_class_id)))
    return JS_EXCEPTION;

  switch(magic) {
    case PROP_LENGTH: {
      return JS_NewInt64(ctx, ptr->n);
    }
    case PROP_PATH: {
      return js_pointer_toarray(ctx, ptr);
    }
  }
  return JS_UNDEFINED;
}

static JSValue
js_pointer_set(JSContext* ctx, JSValueConst this_val, JSValueConst value, int magic) {
  Pointer* ptr;

  if(!(ptr = JS_GetOpaque2(ctx, this_val, js_pointer_class_id)))
    return JS_EXCEPTION;

  switch(magic) {
    case PROP_PATH: {
      js_pointer_fromiterable(ctx, ptr, value);
      break;
    }
  }
  return JS_UNDEFINED;
}

static JSValue
js_pointer_funcs(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
  JSValue ret = JS_UNDEFINED;
  switch(magic) {
    case 0: {
      ret = js_pointer_ctor(ctx, JS_UNDEFINED, 0, 0);
      if(JS_IsArray(ctx, argv[0]))
        js_pointer_fromarray(ctx, js_pointer_data(ctx, ret), argv[0]);
      else
        js_pointer_fromiterable(ctx, js_pointer_data(ctx, ret), argv[0]);
      break;
    }
  }
  return ret;
}

void
js_pointer_finalizer(JSRuntime* rt, JSValue val) {
  Pointer* ptr = JS_GetOpaque(val, js_pointer_class_id);
  if(ptr) {

    if(ptr->atoms) {
      uint32_t i;
      for(i = 0; i < ptr->n; i++) JS_FreeAtomRT(rt, ptr->atoms[i]);
      free(ptr->atoms);
    }
    js_free_rt(rt, ptr);
  }

  // JS_FreeValueRT(rt, val);
}

JSClassDef js_pointer_class = {
    .class_name = "Pointer",
    .finalizer = js_pointer_finalizer,
};
JSClassDef js_dereferenceerror_class = {
    .class_name = "DereferenceError",
    .finalizer = js_pointer_finalizer,
};

static const JSCFunctionListEntry js_pointer_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("deref", 1, js_pointer_method, DEREF),
    JS_CFUNC_MAGIC_DEF("toString", 0, js_pointer_method, TO_STRING),
    JS_CFUNC_MAGIC_DEF("toArray", 0, js_pointer_method, TO_ARRAY),
    JS_CFUNC_MAGIC_DEF("inspect", 0, js_pointer_method, INSPECT),
    JS_CFUNC_MAGIC_DEF("shift", 1, js_pointer_method, SHIFT),
    JS_CFUNC_MAGIC_DEF("slice", 2, js_pointer_method, SLICE),
    JS_CFUNC_MAGIC_DEF("keys", 0, js_pointer_method, KEYS),
    JS_CFUNC_MAGIC_DEF("values", 0, js_pointer_method, VALUES),
    JS_ALIAS_DEF("[Symbol.iterator]", "keys"),
    JS_CGETSET_MAGIC_DEF("length", js_pointer_get, 0, PROP_LENGTH),
    JS_CGETSET_MAGIC_DEF("path", js_pointer_get, js_pointer_set, PROP_PATH),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Pointer", JS_PROP_C_W_E)};

static const JSCFunctionListEntry js_pointer_static_funcs[] = {JS_CFUNC_MAGIC_DEF("from", 1, js_pointer_funcs, 0)};

static const JSCFunctionListEntry js_dereferenceerror_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_dereferenceerror_tostring),
    JS_PROP_STRING_DEF("name", "DereferenceError", JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE),
    JS_CGETSET_MAGIC_DEF("pointer", js_dereferenceerror_get, 0, 0),
    JS_ALIAS_DEF("toPrimitive", "toString"),
    JS_ALIAS_DEF("valueOf", "toString"),
    JS_ALIAS_DEF("inspect", "toString"),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "DereferenceError", JS_PROP_CONFIGURABLE)};

static int
js_pointer_init(JSContext* ctx, JSModuleDef* m) {

  JS_NewClassID(&js_pointer_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_pointer_class_id, &js_pointer_class);

  pointer_proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, pointer_proto, js_pointer_proto_funcs, countof(js_pointer_proto_funcs));
  JS_SetClassProto(ctx, js_pointer_class_id, pointer_proto);

  pointer_class = JS_NewCFunction2(ctx, js_pointer_ctor, "Pointer", 1, JS_CFUNC_constructor, 0);

  JS_SetConstructor(ctx, pointer_class, pointer_proto);
  JS_SetPropertyFunctionList(ctx, pointer_class, js_pointer_static_funcs, countof(js_pointer_static_funcs));

  dereferenceerror_class =
      JS_NewCFunction2(ctx, js_dereferenceerror_ctor, "DereferenceError", 2, JS_CFUNC_constructor, 0);

  JS_SetConstructor(ctx, dereferenceerror_class, dereferenceerror_proto);
  JS_SetPropertyFunctionList(ctx,
                             dereferenceerror_proto,
                             js_dereferenceerror_proto_funcs,
                             countof(js_dereferenceerror_proto_funcs));

  if(m) {
    JS_SetModuleExport(ctx, m, "Pointer", pointer_class);
    JS_SetModuleExport(ctx, m, "DereferenceError", dereferenceerror_class);
  }

  return 0;
}

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_pointer
#endif

JSModuleDef*
JS_INIT_MODULE(JSContext* ctx, const char* module_name) {
  JSModuleDef* m;
  m = JS_NewCModule(ctx, module_name, &js_pointer_init);
  if(!m)
    return NULL;
  JS_AddModuleExport(ctx, m, "Pointer");
  JS_AddModuleExport(ctx, m, "DereferenceError");
  return m;
}