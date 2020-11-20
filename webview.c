#include "quickjs-libc.h"
#include "cutils.h"

#define WEBVIEW_IMPLEMENTATION
#include "webview.h"

//#define QJLS_MOD_TRACE 1
#ifdef QJLS_MOD_TRACE
#include <stdio.h>
#define trace(...) printf(__VA_ARGS__)
#else
#define trace(...) ((void)0)
#endif

static void *memdup(const void* mem, size_t size) { 
	void *dup = malloc(size);
	if (dup != NULL) {
		memcpy(dup, mem, size);
	}
	return dup;
}

#define QJS_ARG_IS_DEFINED(_argc, _argv, _index) ((_argc > _index) && (!JS_IsUndefined(_argv[_index])))

#define QJS_ARG_AS_BOOL(_ctx, _argc, _argv, _index) (QJS_ARG_IS_DEFINED(_argc, _argv, _index) && (JS_ToBool(_ctx, _argv[_index]) > 0))

static const char *qjs_newCString(JSContext *ctx, JSValueConst value) {
	size_t len;
	const char *p = NULL;
	const char *cStr = JS_ToCStringLen(ctx, &len, value);
	if (cStr != NULL) {
		p = memdup(cStr, len + 1);
		JS_FreeCString(ctx, cStr);
	}
	return p;
}

typedef struct {
	JSContext *ctx;
	JSValue fn;
	struct webview wv;
} QJSWebView;

#define QJS_WEBVIEW_PTR(_cp) ((QJSWebView *) ((char *) (_cp) - offsetof(QJSWebView, wv)))

static void qjs_webview_free_rt(struct webview *pwv, JSRuntime *rt) {
	trace("qjs_webview_free(%p)\n", rt);
	trace("qjs_webview_free() url: %p, title: %p\n", pwv->url, pwv->title);
	free((void *)pwv->url);
	pwv->url = NULL;
	free((void *)pwv->title);
	pwv->title = NULL;
	trace("qjs_webview_free() completed\n");
}

#define qjs_webview_free(_pwv, _ctx) qjs_webview_free_rt(_pwv, JS_GetRuntime(_ctx))

static int qjs_webview_fillDefault(struct webview *pwv) {
	trace("qjs_webview_fillDefault(%p)\n", pwv);
	if (pwv->url == NULL) {
		pwv->url = strdup("about:blank");
		if (pwv->url == NULL) {
			return 1;
		}
	}
	if (pwv->title == NULL) {
		pwv->title = strdup("QuickJS Web View");
		if (pwv->title == NULL) {
			return 1;
		}
	}
	if (pwv->width <= 0) {
		pwv->width = 800;
	}
	if (pwv->height <= 0) {
		pwv->height = 600;
	}
	return 0;
}

static int qjs_webview_fill(struct webview *pwv, JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_fill(%p, %p, ?, %d, ?)\n", pwv, ctx, argc);
	if (QJS_ARG_IS_DEFINED(argc, argv, 0)) {
		free((void *)pwv->url);
		pwv->url = qjs_newCString(ctx, argv[0]);
		if (pwv->url == NULL) {
			return 1;
		}
	}
	if (QJS_ARG_IS_DEFINED(argc, argv, 1)) {
		free((void *)pwv->title);
		pwv->title = qjs_newCString(ctx, argv[1]);
		if (pwv->title == NULL) {
			return 1;
		}
	}
	if (QJS_ARG_IS_DEFINED(argc, argv, 2) && JS_ToInt32(ctx, &pwv->width, argv[2])) {
		return 1;
	}
	if (QJS_ARG_IS_DEFINED(argc, argv, 3) && JS_ToInt32(ctx, &pwv->height, argv[3])) {
		return 1;
	}
	if (QJS_ARG_IS_DEFINED(argc, argv, 4) && ((pwv->resizable = JS_ToBool(ctx, argv[3])) < 0)) {
		return 1;
	}
	return 0;
}

static JSValue qjs_webview_open(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_open(%p, ?, %d, ?)\n", ctx, argc);
  struct webview webview;
  memset(&webview, 0, sizeof(webview));
	if (qjs_webview_fill(&webview, ctx, this_val, argc, argv)) {
		qjs_webview_free(&webview, ctx);
		return JS_EXCEPTION;
	}
	if (qjs_webview_fillDefault(&webview)) {
		qjs_webview_free(&webview, ctx);
		return JS_EXCEPTION;
	}
  int r = webview_init(&webview);
  if (r != 0) {
		qjs_webview_free(&webview, ctx);
		return JS_UNDEFINED;
  }
  while (webview_loop(&webview, 1) == 0) {}
  webview_exit(&webview);

	qjs_webview_free(&webview, ctx);
	return JS_UNDEFINED;
}

static JSClassID qjs_webview_class_id;

static void qjs_webview_finalizer(JSRuntime *rt, JSValue val) {
	trace("qjs_webview_finalizer(%p)\n", rt);
	QJSWebView *p = JS_GetOpaque(val, qjs_webview_class_id);
	if (p) {
		trace("qjs_webview_finalizer() free webview\n");
		qjs_webview_free_rt(&p->wv, rt);
		if (!JS_IsUndefined(p->fn)) {
			trace("qjs_webview_finalizer() free fn\n");
			JS_FreeValueRT(rt, p->fn);
		}
		p->fn = JS_UNDEFINED;
		trace("qjs_webview_finalizer() free QJSWebView\n");
		free(p);
	}
	trace("qjs_webview_finalizer() completed\n");
}

static void qjs_webview_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
	trace("qjs_webview_mark(%p)\n", rt);
	QJSWebView *p = JS_GetOpaque(val, qjs_webview_class_id);
	if (p) {
			JS_MarkValue(rt, p->fn, mark_func);
	}
}

static JSClassDef qjs_webview_class = {
	"WebView",
	.finalizer = qjs_webview_finalizer,
	.gc_mark = qjs_webview_mark,
};

static JSValue qjs_webview_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_create(%p, ?, %d, ?)\n", ctx, argc);
	JSValue obj = JS_NewObjectClass(ctx, qjs_webview_class_id);
	if (JS_IsException(obj))
		return obj;

	QJSWebView *p = calloc(1, sizeof(QJSWebView)); // The memory is set to zero
	if (!p) {
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}
	p->fn = JS_UNDEFINED;
	if (qjs_webview_fill(&p->wv, ctx, this_val, argc, argv)) {
		qjs_webview_free(&p->wv, ctx);
		JS_FreeValue(ctx, obj);
		free(p);
		return JS_EXCEPTION;
	}
	JS_SetOpaque(obj, p);
	return obj;
}

static JSValue qjs_webview_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_init(%p)\n", ctx);
	QJSWebView *p = JS_GetOpaque2(ctx, this_val, qjs_webview_class_id);
	if (!p) {
		return JS_EXCEPTION;
	}
	if (qjs_webview_fill(&p->wv, ctx, this_val, argc, argv)) {
		return JS_EXCEPTION;
	}
	if (qjs_webview_fillDefault(&p->wv)) {
		return JS_EXCEPTION;
	}
  int r = webview_init(&p->wv);
  if (r != 0) {
		return JS_EXCEPTION;
  }
	return JS_UNDEFINED;
}

static JSValue qjs_webview_loop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_loop(%p)\n", ctx);
	QJSWebView *p = JS_GetOpaque2(ctx, this_val, qjs_webview_class_id);
	if (!p) {
		return JS_EXCEPTION;
	}
	trace("qjs_webview_loop() ctx: %p, %p %p\n", ctx, p->wv.url, p->wv.title);
  while (webview_loop(&p->wv, 1) == 0) {}
  webview_exit(&p->wv);
	return JS_UNDEFINED;
}

static void qjs_webview_invoke_cb(struct webview *pwv, const char *arg) {
	if ((pwv != NULL) && (arg != NULL)) {
		QJSWebView *p = QJS_WEBVIEW_PTR(pwv);
    JSContext *ctx = p->ctx;
    JSValue ret, fn, s;
		JSValueConst argv[1];
		s = JS_NewString(ctx, arg);
    fn = JS_DupValue(ctx, p->fn);
		argv[0] = (JSValueConst) s;
    ret = JS_Call(ctx, fn, JS_UNDEFINED, 1, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, s);
    if (JS_IsException(ret)) {
			trace("qjs_webview_invoke_cb() raise an exception\n");
		}
    JS_FreeValue(ctx, ret);
	}
}

static JSValue qjs_webview_callback(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_callback(%p)\n", ctx);
	QJSWebView *p = JS_GetOpaque2(ctx, this_val, qjs_webview_class_id);
	if (!p) {
		return JS_EXCEPTION;
	}
	if (JS_IsFunction(ctx, argv[0])) {
		p->fn = JS_DupValue(ctx, argv[0]);
		p->ctx = ctx;
		p->wv.external_invoke_cb = qjs_webview_invoke_cb;
	} else {
		//JS_IsObject
    if (!JS_IsUndefined(p->fn)) {
			JS_FreeValue(p->ctx, p->fn);
		}
		p->fn = JS_UNDEFINED;
		p->ctx = NULL;
		p->wv.external_invoke_cb = NULL;
	}
	return JS_UNDEFINED;
}

static void dispatched_eval(struct webview *pwv, void *arg) {
	if ((pwv != NULL) && (arg != NULL)) {
		webview_eval(pwv, (const char *) arg);
		free(arg);
	}
}

static JSValue qjs_webview_eval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	QJSWebView *p = JS_GetOpaque2(ctx, this_val, qjs_webview_class_id);
	char *js;
	const char *cStr;
	size_t len;
	if (!p) {
		return JS_EXCEPTION;
	}
	if ((argc == 0) || ((cStr = JS_ToCStringLen(ctx, &len, argv[0])) == NULL)) {
		return JS_EXCEPTION;
	}
	int dispatch = QJS_ARG_AS_BOOL(ctx, argc, argv, 1);
	trace("qjs_webview_eval(%p, '%s', %d)\n", ctx, cStr, dispatch);
	if (dispatch) {
		js = memdup(cStr, len + 1);
		JS_FreeCString(ctx, cStr);
		webview_dispatch(&p->wv, dispatched_eval, (void *)js);
		return JS_UNDEFINED;
	}
	int r = webview_eval(&p->wv, cStr);
	JS_FreeCString(ctx, cStr);
	if (r) {
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static void dispatched_terminate(struct webview *w, void *arg) {
	webview_terminate(w);
}

static JSValue qjs_webview_terminate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_terminate(%p)\n", ctx);
	QJSWebView *p = JS_GetOpaque2(ctx, this_val, qjs_webview_class_id);
	if (!p) {
		return JS_EXCEPTION;
	}
	int dispatch = QJS_ARG_AS_BOOL(ctx, argc, argv, 0);
	if (dispatch) {
		webview_dispatch(&p->wv, dispatched_terminate, NULL);
		return JS_UNDEFINED;
	}
	webview_terminate(&p->wv);
	return JS_UNDEFINED;
}

static JSValue qjs_webview_title(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_title(%p)\n", ctx);
	const char *title;
	QJSWebView *p = JS_GetOpaque2(ctx, this_val, qjs_webview_class_id);
	if (!p) {
		return JS_EXCEPTION;
	}
	if ((argc == 0) || ((title = JS_ToCString(ctx, argv[0])) == NULL)) {
		return JS_EXCEPTION;
	}
	webview_set_title(&p->wv, title);
	JS_FreeCString(ctx, title);
	return JS_UNDEFINED;
}

static JSValue qjs_webview_fullscreen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	trace("qjs_webview_fullscreen(%p)\n", ctx);
	QJSWebView *p = JS_GetOpaque2(ctx, this_val, qjs_webview_class_id);
	if (!p) {
		return JS_EXCEPTION;
	}
	int fullscreen = QJS_ARG_AS_BOOL(ctx, argc, argv, 0);
	webview_set_fullscreen(&p->wv, fullscreen);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry qjs_webview_proto_funcs[] = {
	JS_CFUNC_DEF("init", 0, qjs_webview_init),
	JS_CFUNC_DEF("loop", 0, qjs_webview_loop),
	JS_CFUNC_DEF("callback", 0, qjs_webview_callback),
	JS_CFUNC_DEF("eval", 0, qjs_webview_eval),
	JS_CFUNC_DEF("fullscreen", 0, qjs_webview_fullscreen),
	JS_CFUNC_DEF("title", 0, qjs_webview_title),
	JS_CFUNC_DEF("terminate", 0, qjs_webview_terminate),
};

static const JSCFunctionListEntry qjs_webview_funcs[] = {
	JS_CFUNC_DEF("open", 1, qjs_webview_open),
	JS_CFUNC_DEF("create", 0, qjs_webview_create),
};

static int qjs_webview_init_module(JSContext *ctx, JSModuleDef *m) {
	JS_NewClassID(&qjs_webview_class_id);
	JS_NewClass(JS_GetRuntime(ctx), qjs_webview_class_id, &qjs_webview_class);
	JSValue proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, qjs_webview_proto_funcs, countof(qjs_webview_proto_funcs));
	JS_SetClassProto(ctx, qjs_webview_class_id, proto);
	return JS_SetModuleExportList(ctx, m, qjs_webview_funcs, countof(qjs_webview_funcs));
}

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_webview
#endif

JSModuleDef *JS_INIT_MODULE(JSContext *ctx, const char *module_name) {
	JSModuleDef *m;
	m = JS_NewCModule(ctx, module_name, qjs_webview_init_module);
	if (!m) {
		return NULL;
	}
	JS_AddModuleExportList(ctx, m, qjs_webview_funcs, countof(qjs_webview_funcs));
	return m;
}