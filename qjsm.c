/*
 * QuickJS stand alone interpreter
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#endif

#ifdef HAVE_QUICKJS_CONFIG_H
#include "quickjs-config.h"
#endif

#include "cutils.h"
#include "quickjs-libc.h"

#ifdef HAVE_MALLOC_USABLE_SIZE
#ifndef HAVE_MALLOC_USABLE_SIZE_DEFINITION
extern size_t malloc_usable_size();
#endif
#endif

extern const uint8_t qjsc_repl[];
extern const uint32_t qjsc_repl_size;
#ifdef CONFIG_BIGNUM
extern const uint8_t qjsc_qjscalc[];
extern const uint32_t qjsc_qjscalc_size;
static int bignum_ext = 1;
#endif

JSModuleDef* js_init_module_child_process(JSContext*, const char*);
JSModuleDef* js_init_module_deep(JSContext*, const char*);
JSModuleDef* js_init_module_inspect(JSContext*, const char*);
JSModuleDef* js_init_module_lexer(JSContext*, const char*);
JSModuleDef* js_init_module_mmap(JSContext*, const char*);
JSModuleDef* js_init_module_path(JSContext*, const char*);
JSModuleDef* js_init_module_pointer(JSContext*, const char*);
JSModuleDef* js_init_module_predicate(JSContext*, const char*);
JSModuleDef* js_init_module_repeater(JSContext*, const char*);
JSModuleDef* js_init_module_tree_walker(JSContext*, const char*);
JSModuleDef* js_init_module_xml(JSContext*, const char*);

static void
js_dump_obj(JSContext* ctx, FILE* f, JSValueConst val) {
  const char* str;

  str = JS_ToCString(ctx, val);
  if(str) {
    fprintf(f, "%s\n", str);
    JS_FreeCString(ctx, str);
  } else {
    fprintf(f, "[exception]\n");
  }
}

static void
js_std_dump_error1(JSContext* ctx, JSValueConst exception_val) {
  JSValue val;
  BOOL is_error;

  is_error = JS_IsError(ctx, exception_val);
  js_dump_obj(ctx, stderr, exception_val);
  if(is_error) {
    val = JS_GetPropertyStr(ctx, exception_val, "stack");
    if(!JS_IsUndefined(val)) {
      js_dump_obj(ctx, stderr, val);
    }
    JS_FreeValue(ctx, val);
  }
}

void js_std_dump_error(JSContext* ctx);

/* main loop which calls the user JS callbacks */
void
js_loop(JSContext* ctx) {
  JSContext* ctx1;
  int err;

  for(;;) {
    /* execute the pending jobs */
    for(;;) {
      err = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);
      if(err <= 0) {
        if(err < 0) {
          js_std_dump_error(ctx1);
        }
        break;
      }
    }

    /*if(!os_poll_func || os_poll_func(ctx))
      break;*/
  }
}
#include "quickjs.h"
#include "quickjs-libc.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char js_default_module_path[] = "."
#ifdef CONFIG_PREFIX
                                      ":" CONFIG_PREFIX "/lib/quickjs"
#endif
    ;

char*
js_find_module_ext(JSContext* ctx, const char* module_name, const char* ext) {
  const char *module_path, *p, *q;
  char* filename = NULL;
  size_t n, m;
  struct stat st;

  if((module_path = getenv("QUICKJS_MODULE_PATH")) == NULL)
    module_path = js_default_module_path;

  for(p = module_path; *p; p = q) {
    if((q = strchr(p, ':')) == NULL)
      q = p + strlen(p);
    n = q - p;
    filename = js_malloc(ctx, n + 1 + strlen(module_name) + 3 + 1);
    strncpy(filename, p, n);
    filename[n] = '/';
    strcpy(&filename[n + 1], module_name);
    m = strlen(module_name);
    if(!(m >= 3 && !strcmp(&module_name[m - 3], ext)))
      strcpy(&filename[n + 1 + m], ext);
    if(!stat(filename, &st))
      return filename;
    js_free(ctx, filename);
    if(*q == ':')
      ++q;
  }
  return NULL;
}

char*
js_find_module(JSContext* ctx, const char* module_name) {
  char* ret = NULL;
  size_t len;

  while(!strncmp(module_name, "./", 2)) module_name += 2;
  len = strlen(module_name);

  if(strchr(module_name, '/') == NULL || (len >= 3 && !strcmp(&module_name[len - 3], ".so")))
    ret = js_find_module_ext(ctx, module_name, ".so");

  if(ret == NULL)
    ret = js_find_module_ext(ctx, module_name, ".js");
  return ret;
}

JSModuleDef*
js_module_loader_path(JSContext* ctx, const char* module_name, void* opaque) {
  char* filename;
  JSModuleDef* ret = NULL;
  filename = module_name[0] == '/' ? js_strdup(ctx, module_name) : js_find_module(ctx, module_name);
  if(filename) {
    ret = js_module_loader(ctx, filename, opaque);
    js_free(ctx, filename);
  }
  return ret;
}

static int
eval_buf(JSContext* ctx, const void* buf, int buf_len, const char* filename, int eval_flags) {
  JSValue val;
  int ret;

  if((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
    /* for the modules, we compile then run to be able to set
       import.meta */
    val = JS_Eval(ctx, buf, buf_len, filename, eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
    if(!JS_IsException(val)) {
      js_module_set_import_meta(ctx, val, TRUE, TRUE);
      val = JS_EvalFunction(ctx, val);
    }
  } else {
    val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
  }
  if(JS_IsException(val)) {
    js_std_dump_error(ctx);
    ret = -1;
  } else {
    ret = 0;
  }
  JS_FreeValue(ctx, val);
  return ret;
}

static int
eval_file(JSContext* ctx, const char* filename, int module) {
  uint8_t* buf;
  int ret, eval_flags;
  size_t buf_len;

  buf = js_load_file(ctx, &buf_len, filename);
  if(!buf) {
    perror(filename);
    exit(1);
  }

  if(module < 0) {
    module = (has_suffix(filename, ".mjs") || JS_DetectModule((const char*)buf, buf_len));
  }
  if(module)
    eval_flags = JS_EVAL_TYPE_MODULE;
  else
    eval_flags = JS_EVAL_TYPE_GLOBAL;
  ret = eval_buf(ctx, buf, buf_len, filename, eval_flags);
  js_free(ctx, buf);
  return ret;
}

/* also used to initialize the worker context */
static JSContext*
JS_NewCustomContext(JSRuntime* rt) {
  JSContext* ctx;
  ctx = JS_NewContext(rt);
  if(!ctx)
    return NULL;
#ifdef CONFIG_BIGNUM
  if(bignum_ext) {
    JS_AddIntrinsicBigFloat(ctx);
    JS_AddIntrinsicBigDecimal(ctx);
    JS_AddIntrinsicOperators(ctx);
    JS_EnableBignumExt(ctx, TRUE);
  }
#endif
  /* system modules */
  js_init_module_std(ctx, "std");
  js_init_module_os(ctx, "os");
  js_init_module_child_process(ctx, "child_process");
  js_init_module_deep(ctx, "deep");
  js_init_module_inspect(ctx, "inspect");
  js_init_module_lexer(ctx, "lexer");
  js_init_module_mmap(ctx, "mmap");
  js_init_module_path(ctx, "path");
  js_init_module_pointer(ctx, "pointer");
  js_init_module_predicate(ctx, "predicate");
  js_init_module_repeater(ctx, "repeater");
  js_init_module_tree_walker(ctx, "tree-walker");
  js_init_module_xml(ctx, "xml");

  return ctx;
}

#if defined(__APPLE__)
#define MALLOC_OVERHEAD 0
#else
#define MALLOC_OVERHEAD 8
#endif

struct trace_malloc_data {
  uint8_t* base;
};

static inline unsigned long long
js_trace_malloc_ptr_offset(uint8_t* ptr, struct trace_malloc_data* dp) {
  return ptr - dp->base;
}

/* default memory allocation functions with memory limitation */
static inline size_t
js_trace_malloc_usable_size(void* ptr) {
#if defined(__APPLE__)
  return malloc_size(ptr);
#elif defined(_WIN32)
  return _msize(ptr);
#elif defined(EMSCRIPTEN) || defined(__dietlibc__) || defined(__MSYS__) || defined(DONT_HAVE_MALLOC_USABLE_SIZE)
  return 0;
#elif defined(__linux__) || defined(HAVE_MALLOC_USABLE_SIZE)
  return malloc_usable_size(ptr);
#else
  /* change this to `return 0;` if compilation fails */
  return malloc_usable_size(ptr);
#endif
}

static void
#ifdef _WIN32
    /* mingw printf is used */
    __attribute__((format(gnu_printf, 2, 3)))
#else
    __attribute__((format(printf, 2, 3)))
#endif
    js_trace_malloc_printf(JSMallocState* s, const char* fmt, ...) {
  va_list ap;
  int c;

  va_start(ap, fmt);
  while((c = *fmt++) != '\0') {
    if(c == '%') {
      /* only handle %p and %zd */
      if(*fmt == 'p') {
        uint8_t* ptr = va_arg(ap, void*);
        if(ptr == NULL) {
          printf("NULL");
        } else {
          printf("H%+06lld.%zd", js_trace_malloc_ptr_offset(ptr, s->opaque), js_trace_malloc_usable_size(ptr));
        }
        fmt++;
        continue;
      }
      if(fmt[0] == 'z' && fmt[1] == 'd') {
        size_t sz = va_arg(ap, size_t);
        printf("%zd", sz);
        fmt += 2;
        continue;
      }
    }
    putc(c, stdout);
  }
  va_end(ap);
}

static void
js_trace_malloc_init(struct trace_malloc_data* s) {
  free(s->base = malloc(8));
}

static void*
js_trace_malloc(JSMallocState* s, size_t size) {
  void* ptr;

  /* Do not allocate zero bytes: behavior is platform dependent */
  assert(size != 0);

  if(unlikely(s->malloc_size + size > s->malloc_limit))
    return NULL;
  ptr = malloc(size);
  js_trace_malloc_printf(s, "A %zd -> %p\n", size, ptr);
  if(ptr) {
    s->malloc_count++;
    s->malloc_size += js_trace_malloc_usable_size(ptr) + MALLOC_OVERHEAD;
  }
  return ptr;
}

static void
js_trace_free(JSMallocState* s, void* ptr) {
  if(!ptr)
    return;

  js_trace_malloc_printf(s, "F %p\n", ptr);
  s->malloc_count--;
  s->malloc_size -= js_trace_malloc_usable_size(ptr) + MALLOC_OVERHEAD;
  free(ptr);
}

static void*
js_trace_realloc(JSMallocState* s, void* ptr, size_t size) {
  size_t old_size;

  if(!ptr) {
    if(size == 0)
      return NULL;
    return js_trace_malloc(s, size);
  }
  old_size = js_trace_malloc_usable_size(ptr);
  if(size == 0) {
    js_trace_malloc_printf(s, "R %zd %p\n", size, ptr);
    s->malloc_count--;
    s->malloc_size -= old_size + MALLOC_OVERHEAD;
    free(ptr);
    return NULL;
  }
  if(s->malloc_size + size - old_size > s->malloc_limit)
    return NULL;

  js_trace_malloc_printf(s, "R %zd %p", size, ptr);

  ptr = realloc(ptr, size);
  js_trace_malloc_printf(s, " -> %p\n", ptr);
  if(ptr) {
    s->malloc_size += js_trace_malloc_usable_size(ptr) - old_size;
  }
  return ptr;
}

static const JSMallocFunctions trace_mf = {
    js_trace_malloc,
    js_trace_free,
    js_trace_realloc,
#if defined(__APPLE__)
    malloc_size,
#elif defined(_WIN32)
    (size_t(*)(const void*))_msize,
#elif defined(EMSCRIPTEN) || defined(__dietlibc__) || defined(__MSYS__) ||                                             \
    defined(DONT_HAVE_MALLOC_USABLE_SIZE_DEFINITION)
    NULL,
#elif defined(__linux__) || defined(HAVE_MALLOC_USABLE_SIZE)
    (size_t(*)(const void*))malloc_usable_size,
#else
    /* change this to `NULL,` if compilation fails */
    malloc_usable_size,
#endif
};

#define PROG_NAME "qjsm"

void
help(void) {
  printf("QuickJS version " CONFIG_VERSION "\n"
         "usage: " PROG_NAME " [options] [file [args]]\n"
         "-h  --help         list options\n"
         "-e  --eval EXPR    evaluate EXPR\n"
         "-i  --interactive  go to interactive mode\n"
         "-m  --module       load as ES6 module (default=autodetect)\n"
         "    --script       load as ES6 script (default=autodetect)\n"
         "-I  --include file include an additional file\n"
         "    --std          make 'std' and 'os' available to the loaded script\n"
#ifdef CONFIG_BIGNUM
         "    --no-bignum    disable the bignum extensions (BigFloat, BigDecimal)\n"
         "    --qjscalc      load the QJSCalc runtime (default if invoked as qjscalc)\n"
#endif
         "-T  --trace        trace memory allocation\n"
         "-d  --dump         dump the memory usage stats\n"
         "    --memory-limit n       limit the memory usage to 'n' bytes\n"
         "    --stack-size n         limit the stack size to 'n' bytes\n"
         "    --unhandled-rejection  dump unhandled promise rejections\n"
         "-q  --quit         just instantiate the interpreter and quit\n");
  exit(1);
}

JSModuleDef* js_module_loader_path(JSContext* ctx, const char* module_name, void* opaque);

int
main(int argc, char** argv) {
  JSRuntime* rt;
  JSContext* ctx;
  struct trace_malloc_data trace_data = {NULL};
  int optind;
  char* expr = NULL;
  int interactive = 0;
  int dump_memory = 0;
  int trace_memory = 0;
  int empty_run = 0;
  int module = -1;
  int load_std = 0;
  int dump_unhandled_promise_rejection = 0;
  size_t memory_limit = 0;
  char* include_list[32];
  int i, include_count = 0;
#ifdef CONFIG_BIGNUM
  int load_jscalc;
#endif
  size_t stack_size = 0;

#ifdef CONFIG_BIGNUM
  /* load jscalc runtime if invoked as 'qjscalc' */
  {
    const char *p, *exename;
    exename = argv[0];
    p = strrchr(exename, '/');
    if(p)
      exename = p + 1;
    load_jscalc = !strcmp(exename, "qjscalc");
  }
#endif

  /* cannot use getopt because we want to pass the command line to
     the script */
  optind = 1;
  while(optind < argc && *argv[optind] == '-') {
    char* arg = argv[optind] + 1;
    const char* longopt = "";
    /* a single - is not an option, it also stops argument scanning */
    if(!*arg)
      break;
    optind++;
    if(*arg == '-') {
      longopt = arg + 1;
      arg += strlen(arg);
      /* -- stops argument scanning */
      if(!*longopt)
        break;
    }
    for(; *arg || *longopt; longopt = "") {
      char opt = *arg;
      if(opt)
        arg++;
      if(opt == 'h' || opt == '?' || !strcmp(longopt, "help")) {
        help();
        continue;
      }
      if(opt == 'e' || !strcmp(longopt, "eval")) {
        if(*arg) {
          expr = arg;
          break;
        }
        if(optind < argc) {
          expr = argv[optind++];
          break;
        }
        fprintf(stderr, "qjs: missing expression for -e\n");
        exit(2);
      }
      if(opt == 'I' || !strcmp(longopt, "include")) {
        if(optind >= argc) {
          fprintf(stderr, "expecting filename");
          exit(1);
        }
        if(include_count >= countof(include_list)) {
          fprintf(stderr, "too many included files");
          exit(1);
        }
        include_list[include_count++] = argv[optind++];
        continue;
      }
      if(opt == 'i' || !strcmp(longopt, "interactive")) {
        interactive++;
        continue;
      }
      if(opt == 'm' || !strcmp(longopt, "module")) {
        module = 1;
        continue;
      }
      if(!strcmp(longopt, "script")) {
        module = 0;
        continue;
      }
      if(opt == 'd' || !strcmp(longopt, "dump")) {
        dump_memory++;
        continue;
      }
      if(opt == 'T' || !strcmp(longopt, "trace")) {
        trace_memory++;
        continue;
      }
      if(!strcmp(longopt, "std")) {
        load_std = 1;
        continue;
      }
      if(!strcmp(longopt, "unhandled-rejection")) {
        dump_unhandled_promise_rejection = 1;
        continue;
      }
#ifdef CONFIG_BIGNUM
      if(!strcmp(longopt, "no-bignum")) {
        bignum_ext = 0;
        continue;
      }
      if(!strcmp(longopt, "qjscalc")) {
        load_jscalc = 1;
        continue;
      }
#endif
      if(opt == 'q' || !strcmp(longopt, "quit")) {
        empty_run++;
        continue;
      }
      if(!strcmp(longopt, "memory-limit")) {
        if(optind >= argc) {
          fprintf(stderr, "expecting memory limit");
          exit(1);
        }
        memory_limit = (size_t)strtod(argv[optind++], NULL);
        continue;
      }
      if(!strcmp(longopt, "stack-size")) {
        if(optind >= argc) {
          fprintf(stderr, "expecting stack size");
          exit(1);
        }
        stack_size = (size_t)strtod(argv[optind++], NULL);
        continue;
      }
      if(opt) {
        fprintf(stderr, "qjs: unknown option '-%c'\n", opt);
      } else {
        fprintf(stderr, "qjs: unknown option '--%s'\n", longopt);
      }
      help();
    }
  }

  if(load_jscalc)
    bignum_ext = 1;

  if(trace_memory) {
    js_trace_malloc_init(&trace_data);
    rt = JS_NewRuntime2(&trace_mf, &trace_data);
  } else {
    rt = JS_NewRuntime();
  }
  if(!rt) {
    fprintf(stderr, "qjs: cannot allocate JS runtime\n");
    exit(2);
  }
  if(memory_limit != 0)
    JS_SetMemoryLimit(rt, memory_limit);
  // if (stack_size != 0)
  JS_SetMaxStackSize(rt, stack_size != 0 ? stack_size : 8 * 1048576);
  js_std_set_worker_new_context_func(JS_NewCustomContext);
  js_std_init_handlers(rt);
  ctx = JS_NewCustomContext(rt);
  if(!ctx) {
    fprintf(stderr, "qjs: cannot allocate JS context\n");
    exit(2);
  }

  /* loader for ES6 modules */
  JS_SetModuleLoaderFunc(rt, NULL, js_module_loader_path, NULL);

  if(dump_unhandled_promise_rejection) {
    JS_SetHostPromiseRejectionTracker(rt, js_std_promise_rejection_tracker, NULL);
  }

  if(!empty_run) {
#ifdef CONFIG_BIGNUM
    if(load_jscalc) {
      js_std_eval_binary(ctx, qjsc_qjscalc, qjsc_qjscalc_size, 0);
    }
#endif
    js_std_add_helpers(ctx, argc - optind, argv + optind);

    /* make 'std' and 'os' visible to non module code */
    if(load_std) {
      const char* str = "import * as std from 'std';\n"
                        "import * as os from 'os';\n"
                        "globalThis.std = std;\n"
                        "globalThis.os = os;\n";
      eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
    }

    for(i = 0; i < include_count; i++) {
      if(eval_file(ctx, include_list[i], module))
        goto fail;
    }

    if(expr) {
      if(eval_buf(ctx, expr, strlen(expr), "<cmdline>", 0))
        goto fail;
    } else if(optind >= argc) {
      /* interactive mode */
      interactive = 1;
    } else {
      const char* filename;
      filename = argv[optind];
      if(eval_file(ctx, filename, module))
        goto fail;
    }
    if(interactive) {
      js_std_eval_binary(ctx, qjsc_repl, qjsc_repl_size, 0);
    }
    js_std_loop(ctx);
  }

  if(dump_memory) {
    JSMemoryUsage stats;
    JS_ComputeMemoryUsage(rt, &stats);
    JS_DumpMemoryUsage(stdout, &stats, rt);
  }
  js_std_free_handlers(rt);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);

  if(empty_run && dump_memory) {
    clock_t t[5];
    double best[5];
    int i, j;
    for(i = 0; i < 100; i++) {
      t[0] = clock();
      rt = JS_NewRuntime();
      t[1] = clock();
      ctx = JS_NewContext(rt);
      t[2] = clock();
      JS_FreeContext(ctx);
      t[3] = clock();
      JS_FreeRuntime(rt);
      t[4] = clock();
      for(j = 4; j > 0; j--) {
        double ms = 1000.0 * (t[j] - t[j - 1]) / CLOCKS_PER_SEC;
        if(i == 0 || best[j] > ms)
          best[j] = ms;
      }
    }
    printf("\nInstantiation times (ms): %.3f = %.3f+%.3f+%.3f+%.3f\n",
           best[1] + best[2] + best[3] + best[4],
           best[1],
           best[2],
           best[3],
           best[4]);
  }
  return 0;
fail:
  js_std_free_handlers(rt);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  return 1;
}
