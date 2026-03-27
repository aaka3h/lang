#ifndef STDLIB_H
#define STDLIB_H

/* ─────────────────────────────────────────────────────────────────
   stdlib.h  —  Standard Library for your language
   
   Modules:
     math    — sqrt, sin, cos, tan, log, floor, ceil, round, pi, e
     string  — len, upper, lower, trim, split, replace, contains,
               startswith, endswith, repeat
     io      — readfile, writefile, appendfile, input (stdin)
     sys     — clock, exit, args
   
   Usage in your language:
     import "math"
     import "string"
     import "io"
     import "sys"
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#ifndef M_PI
#define M_PI  3.14159265358979323846
#define M_E   2.71828182845904523536
#endif
#include <ctype.h>
#include <time.h>
#include "value.h"

/* ─────────────────────────────────────────────────────────────────
   MATH MODULE
   ───────────────────────────────────────────────────────────────── */

static Value std_sin(Value *a, int n)   { return n<1?val_null():val_float(sin(n&&a[0].type==VAL_INT?(double)a[0].as.i:a[0].as.f)); }
static Value std_cos(Value *a, int n)   { return n<1?val_null():val_float(cos(n&&a[0].type==VAL_INT?(double)a[0].as.i:a[0].as.f)); }
static Value std_tan(Value *a, int n)   { return n<1?val_null():val_float(tan(n&&a[0].type==VAL_INT?(double)a[0].as.i:a[0].as.f)); }
static Value std_log(Value *a, int n)   { return n<1?val_null():val_float(log(n&&a[0].type==VAL_INT?(double)a[0].as.i:a[0].as.f)); }
static Value std_log2(Value *a, int n)  { return n<1?val_null():val_float(log2(a[0].type==VAL_INT?(double)a[0].as.i:a[0].as.f)); }
static Value std_log10(Value *a, int n) { return n<1?val_null():val_float(log10(a[0].type==VAL_INT?(double)a[0].as.i:a[0].as.f)); }
static Value std_exp(Value *a, int n)   { return n<1?val_null():val_float(exp(a[0].type==VAL_INT?(double)a[0].as.i:a[0].as.f)); }

static Value std_round(Value *a, int n) {
    if (n < 1) return val_null();
    double x = a[0].type==VAL_INT ? (double)a[0].as.i : a[0].as.f;
    int dp = (n >= 2 && a[1].type==VAL_INT) ? (int)a[1].as.i : 0;
    double factor = pow(10.0, dp);
    return val_float(round(x * factor) / factor);
}

static Value std_min(Value *a, int n) {
    if (n < 1) return val_null();
    double m = a[0].type==VAL_INT ? (double)a[0].as.i : a[0].as.f;
    for (int i=1;i<n;i++) {
        double v = a[i].type==VAL_INT ? (double)a[i].as.i : a[i].as.f;
        if (v < m) m = v;
    }
    return val_float(m);
}

static Value std_max(Value *a, int n) {
    if (n < 1) return val_null();
    double m = a[0].type==VAL_INT ? (double)a[0].as.i : a[0].as.f;
    for (int i=1;i<n;i++) {
        double v = a[i].type==VAL_INT ? (double)a[i].as.i : a[i].as.f;
        if (v > m) m = v;
    }
    return val_float(m);
}

static Value std_clamp(Value *a, int n) {
    if (n < 3) return val_null();
    double v  = a[0].type==VAL_INT?(double)a[0].as.i:a[0].as.f;
    double lo = a[1].type==VAL_INT?(double)a[1].as.i:a[1].as.f;
    double hi = a[2].type==VAL_INT?(double)a[2].as.i:a[2].as.f;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return val_float(v);
}

static void stdlib_load_math(Env *env) {
    env_set(env, "sin",   val_native(std_sin));
    env_set(env, "cos",   val_native(std_cos));
    env_set(env, "tan",   val_native(std_tan));
    env_set(env, "log",   val_native(std_log));
    env_set(env, "log2",  val_native(std_log2));
    env_set(env, "log10", val_native(std_log10));
    env_set(env, "exp",   val_native(std_exp));
    env_set(env, "round", val_native(std_round));
    env_set(env, "min",   val_native(std_min));
    env_set(env, "max",   val_native(std_max));
    env_set(env, "clamp", val_native(std_clamp));
    env_set(env, "pi",    val_float(M_PI));
    env_set(env, "e",     val_float(M_E));
    env_set(env, "inf",   val_float(HUGE_VAL));
}

/* ─────────────────────────────────────────────────────────────────
   STRING MODULE
   ───────────────────────────────────────────────────────────────── */

static Value std_upper(Value *a, int n) {
    if (n<1 || a[0].type!=VAL_STRING) return val_null();
    char buf[256]; strncpy(buf, a[0].as.s, 255); buf[255]='\0';
    for (int i=0;buf[i];i++) buf[i]=(char)toupper((unsigned char)buf[i]);
    return val_string(buf);
}

static Value std_lower(Value *a, int n) {
    if (n<1 || a[0].type!=VAL_STRING) return val_null();
    char buf[256]; strncpy(buf, a[0].as.s, 255); buf[255]='\0';
    for (int i=0;buf[i];i++) buf[i]=(char)tolower((unsigned char)buf[i]);
    return val_string(buf);
}

static Value std_trim(Value *a, int n) {
    if (n<1 || a[0].type!=VAL_STRING) return val_null();
    const char *s = a[0].as.s;
    while (*s && isspace((unsigned char)*s)) s++;
    char buf[256]; strncpy(buf, s, 255); buf[255]='\0';
    int len = (int)strlen(buf);
    while (len > 0 && isspace((unsigned char)buf[len-1])) buf[--len]='\0';
    return val_string(buf);
}

static Value std_replace(Value *a, int n) {
    if (n<3 || a[0].type!=VAL_STRING ||
        a[1].type!=VAL_STRING || a[2].type!=VAL_STRING) return val_null();
    const char *src = a[0].as.s;
    const char *from = a[1].as.s;
    const char *to   = a[2].as.s;
    int flen = (int)strlen(from);
    if (flen == 0) return a[0];
    char buf[256]; int bi=0;
    while (*src && bi < 254) {
        if (strncmp(src, from, flen)==0) {
            int tlen=(int)strlen(to);
            for (int i=0;i<tlen && bi<254;i++) buf[bi++]=to[i];
            src+=flen;
        } else { buf[bi++]=*src++; }
    }
    buf[bi]='\0';
    return val_string(buf);
}

static Value std_contains(Value *a, int n) {
    if (n<2 || a[0].type!=VAL_STRING || a[1].type!=VAL_STRING) return val_bool(0);
    return val_bool(strstr(a[0].as.s, a[1].as.s) != NULL);
}

static Value std_startswith(Value *a, int n) {
    if (n<2 || a[0].type!=VAL_STRING || a[1].type!=VAL_STRING) return val_bool(0);
    return val_bool(strncmp(a[0].as.s, a[1].as.s, strlen(a[1].as.s))==0);
}

static Value std_endswith(Value *a, int n) {
    if (n<2 || a[0].type!=VAL_STRING || a[1].type!=VAL_STRING) return val_bool(0);
    int sl=(int)strlen(a[0].as.s), pl=(int)strlen(a[1].as.s);
    if (pl>sl) return val_bool(0);
    return val_bool(strcmp(a[0].as.s+sl-pl, a[1].as.s)==0);
}

static Value std_repeat(Value *a, int n) {
    if (n<2 || a[0].type!=VAL_STRING || a[1].type!=VAL_INT) return val_null();
    char buf[256]; buf[0]='\0';
    int times=(int)a[1].as.i;
    for (int i=0;i<times && strlen(buf)+strlen(a[0].as.s)<254;i++)
        strncat(buf, a[0].as.s, 254-strlen(buf));
    return val_string(buf);
}

static Value std_substr(Value *a, int n) {
    if (n<2 || a[0].type!=VAL_STRING || a[1].type!=VAL_INT) return val_null();
    const char *s = a[0].as.s;
    int len=(int)strlen(s);
    int start=(int)a[1].as.i;
    if (start<0) start=0; if (start>=len) return val_string("");
    int count = (n>=3 && a[2].type==VAL_INT) ? (int)a[2].as.i : len-start;
    if (count<0) count=0;
    char buf[256]; int bi=0;
    for (int i=start; i<start+count && i<len && bi<255; i++) buf[bi++]=s[i];
    buf[bi]='\0';
    return val_string(buf);
}

static Value std_indexof(Value *a, int n) {
    if (n<2 || a[0].type!=VAL_STRING || a[1].type!=VAL_STRING) return val_int(-1);
    const char *p = strstr(a[0].as.s, a[1].as.s);
    return p ? val_int((long long)(p - a[0].as.s)) : val_int(-1);
}

static void stdlib_load_string(Env *env) {
    env_set(env, "upper",      val_native(std_upper));
    env_set(env, "lower",      val_native(std_lower));
    env_set(env, "trim",       val_native(std_trim));
    env_set(env, "replace",    val_native(std_replace));
    env_set(env, "contains",   val_native(std_contains));
    env_set(env, "startswith", val_native(std_startswith));
    env_set(env, "endswith",   val_native(std_endswith));
    env_set(env, "repeat",     val_native(std_repeat));
    env_set(env, "substr",     val_native(std_substr));
    env_set(env, "indexof",    val_native(std_indexof));
}

/* ─────────────────────────────────────────────────────────────────
   IO MODULE
   ───────────────────────────────────────────────────────────────── */

static Value std_readfile(Value *a, int n) {
    if (n<1 || a[0].type!=VAL_STRING) return val_null();
    FILE *f = fopen(a[0].as.s, "r");
    if (!f) return val_null();
    fseek(f,0,SEEK_END); long sz=ftell(f); rewind(f);
    char *buf=(char*)malloc(sz+1);
    fread(buf,1,sz,f); buf[sz]='\0'; fclose(f);
    Value v = val_string(buf);   /* truncates to 255 chars for now */
    free(buf); return v;
}

static Value std_writefile(Value *a, int n) {
    if (n<2 || a[0].type!=VAL_STRING || a[1].type!=VAL_STRING) return val_bool(0);
    FILE *f = fopen(a[0].as.s, "w");
    if (!f) return val_bool(0);
    fputs(a[1].as.s, f); fclose(f);
    return val_bool(1);
}

static Value std_appendfile(Value *a, int n) {
    if (n<2 || a[0].type!=VAL_STRING || a[1].type!=VAL_STRING) return val_bool(0);
    FILE *f = fopen(a[0].as.s, "a");
    if (!f) return val_bool(0);
    fputs(a[1].as.s, f); fclose(f);
    return val_bool(1);
}

static Value std_input(Value *a, int n) {
    if (n>=1 && a[0].type==VAL_STRING) { printf("%s", a[0].as.s); fflush(stdout); }
    char buf[256];
    if (!fgets(buf, sizeof(buf), stdin)) return val_null();
    buf[strcspn(buf,"\n")]='\0';
    return val_string(buf);
}

static void stdlib_load_io(Env *env) {
    env_set(env, "readfile",   val_native(std_readfile));
    env_set(env, "writefile",  val_native(std_writefile));
    env_set(env, "appendfile", val_native(std_appendfile));
    env_set(env, "input",      val_native(std_input));
}

/* ─────────────────────────────────────────────────────────────────
   SYS MODULE
   ───────────────────────────────────────────────────────────────── */

static Value std_clock(Value *a, int n) {
    (void)a; (void)n;
    return val_float((double)clock() / CLOCKS_PER_SEC);
}

static Value std_exit_fn(Value *a, int n) {
    int code = (n>=1 && a[0].type==VAL_INT) ? (int)a[0].as.i : 0;
    exit(code);
    return val_null();
}

static void stdlib_load_sys(Env *env) {
    env_set(env, "clock", val_native(std_clock));
    env_set(env, "exit",  val_native(std_exit_fn));
}

/* ─────────────────────────────────────────────────────────────────
   IMPORT DISPATCHER
   
   Call this when the interpreter/VM hits an `import "module"` stmt.
   Returns 1 on success, 0 if module not found.
   ───────────────────────────────────────────────────────────────── */
static int stdlib_import(Env *env, const char *module) {
    if (strcmp(module, "math")   == 0) { stdlib_load_math(env);   return 1; }
    if (strcmp(module, "string") == 0) { stdlib_load_string(env); return 1; }
    if (strcmp(module, "io")     == 0) { stdlib_load_io(env);     return 1; }
    if (strcmp(module, "sys")    == 0) { stdlib_load_sys(env);    return 1; }
    return 0;
}

#endif /* STDLIB_H */
