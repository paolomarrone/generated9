#include "rayjs.h"

typedef struct Rejection Rejection;
struct Rejection {
	JSValue promise, reason;
	Rejection *next;
};
static Rejection *rejections;

static void
report(JSContext *ctx, JSValueConst error)
{
	const char *s;
	JSValue stack;

	s = JS_ToCString(ctx, error);
	fprint(2, "rayjs: %s\n", s != nil ? s : "exception");
	JS_FreeCString(ctx, s);
	if(JS_IsObject(error)){
		stack = JS_GetPropertyStr(ctx, error, "stack");
		if(!JS_IsUndefined(stack) && !JS_IsException(stack)){
			s = JS_ToCString(ctx, stack);
			if(s != nil)
				fprint(2, "%s\n", s);
			JS_FreeCString(ctx, s);
		}
		JS_FreeValue(ctx, stack);
	}
	if(JS_HasException(ctx))
		JS_FreeValue(ctx, JS_GetException(ctx));
}

static void
exception(JSContext *ctx)
{
	JSValue e;
	Rejection *r, **p;

	e = JS_GetException(ctx);
	report(ctx, e);
	/* A failed module can also leave its evaluation promise rejected. */
	for(p = &rejections; (r = *p) != nil; ){
		if(JS_IsObject(e) && JS_IsObject(r->reason) &&
		   JS_VALUE_GET_PTR(e) == JS_VALUE_GET_PTR(r->reason)){
			*p = r->next;
			JS_FreeValue(ctx, r->promise);
			JS_FreeValue(ctx, r->reason);
			free(r);
		}else
			p = &r->next;
	}
	JS_FreeValue(ctx, e);
}

static void
rejection(JSContext *ctx, JSValueConst promise, JSValueConst reason, int handled, void *arg)
{
	Rejection *r, **p;

	USED(arg);
	for(p = &rejections; (r = *p) != nil; p = &r->next)
		if(JS_VALUE_GET_PTR(r->promise) == JS_VALUE_GET_PTR(promise)){
			*p = r->next;
			JS_FreeValue(ctx, r->promise);
			JS_FreeValue(ctx, r->reason);
			free(r);
			break;
		}
	if(handled)
		return;
	r = malloc(sizeof *r);
	if(r == nil)
		sysfatal("out of memory");
	r->promise = JS_DupValue(ctx, promise);
	r->reason = JS_DupValue(ctx, reason);
	r->next = rejections;
	rejections = r;
}

static JSValue
jsprint(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv)
{
	const char *s;
	int i;

	USED(self);
	for(i = 0; i < argc; i++){
		s = JS_ToCString(ctx, argv[i]);
		if(s == nil)
			return JS_EXCEPTION;
		print("%s%s", i ? " " : "", s);
		JS_FreeCString(ctx, s);
	}
	print("\n");
	return JS_UNDEFINED;
}

static JSValue
load(JSContext *ctx, char *path, int ismodule)
{
	JSValue v;
	Dir *d;
	char *buf, err[ERRMAX];
	int fd;
	long n;
	int flags;

	fd = open(path, OREAD);
	if(fd < 0){
		rerrstr(err, sizeof err);
		return JS_ThrowReferenceError(ctx, "open %s: %s", path, err);
	}
	d = dirfstat(fd);
	if(d == nil){
		close(fd);
		return JS_ThrowReferenceError(ctx, "stat failed: %s", path);
	}
	if(d->length > 16*1024*1024 || (d->mode & DMDIR)){
		free(d);
		close(fd);
		return JS_ThrowRangeError(ctx, "module too large or not a file: %s", path);
	}
	n = d->length;
	free(d);
	buf = js_malloc(ctx, n+1);
	if(buf == nil){
		close(fd);
		return JS_EXCEPTION;
	}
	if(readn(fd, buf, n) != n){
		close(fd);
		js_free(ctx, buf);
		return JS_ThrowReferenceError(ctx, "short read: %s", path);
	}
	close(fd);
	buf[n] = 0;
	if(ismodule < 0)
		ismodule = (strlen(path) >= 4 && strcmp(path+strlen(path)-4, ".mjs") == 0) || JS_DetectModule(buf,n);
	flags = ismodule ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;
	v = JS_Eval(ctx, buf, n, path, flags | JS_EVAL_FLAG_COMPILE_ONLY);
	js_free(ctx, buf);
	return v;
}

static JSModuleDef *
module(JSContext *ctx, const char *name, void *arg)
{
	JSValue v;
	JSModuleDef *m;

	USED(arg);
	v = load(ctx, (char *)name, 1);
	if(JS_IsException(v))
		return nil;
	m = JS_VALUE_GET_PTR(v);
	JS_FreeValue(ctx, v);
	return m;
}

static int
helpers(JSContext *ctx, int argc, char **argv)
{
	JSValue global, args, v;
	int i, ok;

	global = JS_GetGlobalObject(ctx);
	args = JS_NewArray(ctx);
	ok = 0;
	if(JS_IsException(args))
		goto out;
	for(i = 0; i < argc; i++){
		v = JS_NewString(ctx, argv[i]);
		if(JS_IsException(v) || JS_SetPropertyUint32(ctx, args, i, v) < 0)
			goto out;
	}
	if(JS_SetPropertyStr(ctx, global, "scriptArgs", JS_DupValue(ctx,args)) < 0)
		goto out;
	v = JS_NewCFunction(ctx, jsprint, "print", 1);
	if(JS_IsException(v) || JS_SetPropertyStr(ctx, global, "print", v) < 0)
		goto out;
	ok = 1;
out:
	JS_FreeValue(ctx, args);
	JS_FreeValue(ctx, global);
	return ok ? 0 : -1;
}

static void
usage(void)
{
	fprint(2, "usage: rayjs [-m megabytes] file.js [args ...]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	JSRuntime *rt;
	JSContext *ctx;
	JSValue v, result;
	Rejection *r, *other;
	int failed, memory, n;
	char *s, *end;

	memory = 128;
	ARGBEGIN{
	case 'm':
		s = EARGF(usage());
		memory = strtol(s, &end, 10);
		if(*end || end == s || memory < 4 || memory > 1024)
			usage();
		break;
	default:
		usage();
	}ARGEND
	if(argc < 1)
		usage();
	rt = JS_NewRuntime();
	if(rt == nil)
		sysfatal("create JavaScript runtime");
	JS_SetMemoryLimit(rt, (size_t)memory*1024*1024);
	ctx = JS_NewContext(rt);
	if(ctx == nil)
		sysfatal("create JavaScript context");
	JS_SetModuleLoaderFunc(rt, nil, module, nil);
	JS_SetHostPromiseRejectionTracker(rt, rejection, nil);
	failed = helpers(ctx,argc,argv) < 0 || rayinit(ctx) < 0;
	result = JS_UNDEFINED;
	if(!failed){
		v = load(ctx, argv[0], -1);
		if(JS_IsException(v))
			failed = 1;
		else{
			result = JS_EvalFunction(ctx, v);
			failed = JS_IsException(result);
		}
	}
	while(!failed && (n = JS_ExecutePendingJob(rt, nil)) != 0)
		if(n < 0)
			failed = 1;
	/* Diagnostics can invoke user accessors; execution and tracking are done. */
	JS_SetHostPromiseRejectionTracker(rt, nil, nil);
	if(failed)
		exception(ctx);
	if(!failed && JS_PromiseState(ctx, result) == JS_PROMISE_PENDING){
		fprint(2, "rayjs: module has an unresolved await\n");
		failed = 1;
	}
	while((r = rejections) != nil){
		rejections = r->next;
		for(other = rejections; other != nil; other = other->next)
			if(JS_IsObject(r->reason) && JS_IsObject(other->reason) &&
			   JS_VALUE_GET_PTR(r->reason) == JS_VALUE_GET_PTR(other->reason))
				break;
		if(other == nil)
			report(ctx, r->reason);
		JS_FreeValue(ctx, r->reason);
		JS_FreeValue(ctx, r->promise);
		free(r);
		failed = 1;
	}
	JS_FreeValue(ctx, result);
	/* Finalizers must run while their graphics and audio devices still exist. */
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);
	rayclose();
	exits(failed ? "JavaScript" : nil);
}
