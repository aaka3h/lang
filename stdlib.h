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
#include "lint_module.h"

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


/* format("Hello {0}, you are {1}", name, age) */
static Value std_format(Value *a, int n) {
    if (n < 1 || a[0].type != VAL_STRING) return val_null();
    const char *fmt = a[0].as.s;
    char result[512]; result[0] = ' ';
    int ri = 0;
    while (*fmt && ri < 510) {
        if (*fmt == '{' && *(fmt+1) >= '0' && *(fmt+1) <= '9' && *(fmt+2) == '}') {
            int idx = *(fmt+1) - '0' + 1; /* args start at a[1] */
            if (idx < n) {
                char tmp[128]; tmp[0] = ' ';
                Value v = a[idx];
                if (v.type == VAL_STRING) snprintf(tmp, 127, "%s", v.as.s);
                else if (v.type == VAL_INT) snprintf(tmp, 127, "%lld", v.as.i);
                else if (v.type == VAL_FLOAT) snprintf(tmp, 127, "%g", v.as.f);
                else if (v.type == VAL_BOOL) snprintf(tmp, 127, "%s", v.as.b?"true":"false");
                else snprintf(tmp, 127, "null");
                int tlen = (int)strlen(tmp);
                for (int i=0;i<tlen && ri<510;i++) result[ri++] = tmp[i];
            }
            fmt += 3;
        } else {
            result[ri++] = *fmt++;
        }
    }
    result[ri] = ' ';
    return val_string(result);
}

/* split(str, delim) -> array of strings */
static Value std_split(Value *a, int n) {
    if (n < 2 || a[0].type != VAL_STRING || a[1].type != VAL_STRING)
        return val_null();
    const char *s = a[0].as.s;
    const char *delim = a[1].as.s;
    int dlen = (int)strlen(delim);
    Value arr = val_array_empty();
    if (dlen == 0) {
        /* split into chars */
        char ch[2] = {0};
        while (*s) { ch[0] = *s++; val_array_push(&arr, val_string(ch)); }
        return arr;
    }
    char buf[256]; int bi = 0;
    while (*s) {
        if (strncmp(s, delim, dlen) == 0) {
            buf[bi] = ' ';
            val_array_push(&arr, val_string(buf));
            bi = 0; s += dlen;
        } else {
            if (bi < 254) buf[bi++] = *s;
            s++;
        }
    }
    buf[bi] = ' ';
    val_array_push(&arr, val_string(buf));
    return arr;
}

/* join(array, sep) -> string */
static Value std_join(Value *a, int n) {
    if (n < 2 || a[0].type != VAL_ARRAY || a[1].type != VAL_STRING)
        return val_null();
    char result[512]; result[0] = ' ';
    const char *sep = a[1].as.s;
    for (int i = 0; i < a[0].as.arr.len; i++) {
        Value v = a[0].as.arr.items[i];
        if (v.type == VAL_STRING) strncat(result, v.as.s, 511-strlen(result));
        if (i < a[0].as.arr.len-1) strncat(result, sep, 511-strlen(result));
    }
    return val_string(result);
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
    env_set(env, "format",     val_native(std_format));
    env_set(env, "split",      val_native(std_split));
    env_set(env, "join",       val_native(std_join));
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

/* Load a user .lang file as a module */
static int stdlib_import_file(Env *env, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f); rewind(f);
    char *src_buf = (char*)malloc(sz + 1);
    if (!src_buf) { fclose(f); return 0; }
    if (fread(src_buf, 1, sz, f) != (size_t)sz) { free(src_buf); fclose(f); return 0; }
    src_buf[sz] = '\0'; fclose(f);
    /* lex + parse + interpret in current env */
    extern int interp_run_source(Env *env, const char *src);
    int ok = interp_run_source(env, src_buf);
    free(src_buf);
    return ok;
}


/* String interpolation helper — used by the interpreter for "...{var}..." */
static char *std_interpolate(const char *s, Env *env) {
    static char result[512];
    int ri = 0;
    while (*s && ri < 510) {
        if (*s == '{' && *(s+1) != '{') {
            s++; /* skip { */
            char vname[128]; int vi = 0;
            while (*s && *s != '}' && vi < 127) vname[vi++] = *s++;
            vname[vi] = '\0';
            if (*s == '}') s++;
            Value v; 
            if (env_get(env, vname, &v)) {
                char tmp[128]; tmp[0] = '\0';
                if (v.type == VAL_STRING) snprintf(tmp, 127, "%s", v.as.s);
                else if (v.type == VAL_INT) snprintf(tmp, 127, "%lld", v.as.i);
                else if (v.type == VAL_FLOAT) snprintf(tmp, 127, "%g", v.as.f);
                else if (v.type == VAL_BOOL) snprintf(tmp, 127, "%s", v.as.b?"true":"false");
                int tl = strlen(tmp);
                for (int i=0;i<tl&&ri<510;i++) result[ri++]=tmp[i];
            }
        } else {
            result[ri++] = *s++;
        }
    }
    result[ri] = '\0';
    return result;
}

static void stdlib_load_json(Env *env);
static void stdlib_load_random(Env *env);
static void stdlib_load_http(Env *env);

static int stdlib_import(Env *env, const char *module) {
    if (strcmp(module, "math")   == 0) { stdlib_load_math(env);   return 1; }
    if (strcmp(module, "string") == 0) { stdlib_load_string(env); return 1; }
    if (strcmp(module, "io")     == 0) { stdlib_load_io(env);     return 1; }
    if (strcmp(module, "sys")    == 0) { stdlib_load_sys(env);    return 1; }
    if (strcmp(module, "lint")   == 0) { stdlib_load_lint(env);   return 1; }
    if (strcmp(module, "json")   == 0) { stdlib_load_json(env);   return 1; }
    if (strcmp(module, "random") == 0) { stdlib_load_random(env); return 1; }
    if (strcmp(module, "http")   == 0) { stdlib_load_http(env);   return 1; }
    /* Try as a .lang file */
    if (stdlib_import_file(env, module)) return 1;
    /* Try appending .lang */
    char path[300];
    snprintf(path, 299, "%s.lang", module);
    if (stdlib_import_file(env, path)) return 1;
    return 0;
}

#endif /* STDLIB_H */

/* ═══════════════════════════════════════════════════════════════
   JSON MODULE
   ═══════════════════════════════════════════════════════════════ */

static void json_encode_value(Value v, char *out, int max, int *pos) {
    char tmp[256];
    if (*pos >= max - 2) return;
    if (v.type == VAL_NULL) {
        int l = snprintf(tmp, 255, "null"); if (*pos+l<max) { memcpy(out+*pos,tmp,l); *pos+=l; }
    } else if (v.type == VAL_BOOL) {
        int l = snprintf(tmp, 255, "%s", v.as.b?"true":"false"); if (*pos+l<max) { memcpy(out+*pos,tmp,l); *pos+=l; }
    } else if (v.type == VAL_INT) {
        int l = snprintf(tmp, 255, "%lld", v.as.i); if (*pos+l<max) { memcpy(out+*pos,tmp,l); *pos+=l; }
    } else if (v.type == VAL_FLOAT) {
        int l = snprintf(tmp, 255, "%g", v.as.f); if (*pos+l<max) { memcpy(out+*pos,tmp,l); *pos+=l; }
    } else if (v.type == VAL_STRING) {
        out[(*pos)++] = '"';
        for (const char *p = v.as.s; *p && *pos < max-2; p++) {
            if (*p == '"' || *p == '\\') out[(*pos)++] = '\\';
            out[(*pos)++] = *p;
        }
        out[(*pos)++] = '"';
    } else if (v.type == VAL_ARRAY) {
        out[(*pos)++] = '[';
        for (int i = 0; i < v.as.arr.len && *pos < max-2; i++) {
            if (i > 0) out[(*pos)++] = ',';
            json_encode_value(v.as.arr.items[i], out, max, pos);
        }
        out[(*pos)++] = ']';
    } else if (v.type == VAL_DICT) {
        out[(*pos)++] = '{';
        int first = 1;
        for (int i = 0; i < v.as.dict.dlen && *pos < max-4; i++) {
            if (!first) out[(*pos)++] = ',';
            first = 0;
            out[(*pos)++] = '"';
            const char *k = v.as.dict.dkeys[i];
            while (*k && *pos < max-2) out[(*pos)++] = *k++;
            out[(*pos)++] = '"';
            out[(*pos)++] = ':';
            json_encode_value(v.as.dict.dvals[i], out, max, pos);
        }
        out[(*pos)++] = '}';
    } else {
        int l = snprintf(tmp, 255, "null"); if (*pos+l<max) { memcpy(out+*pos,tmp,l); *pos+=l; }
    }
}

static Value std_json_encode(Value *a, int n) {
    if (n < 1) return val_string("null");
    char buf[4096]; int pos = 0;
    json_encode_value(a[0], buf, sizeof(buf)-1, &pos);
    buf[pos] = '\0';
    return val_string(buf);
}

/* Simple JSON decoder — handles flat objects and arrays */
static Value std_json_decode(Value *a, int n) {
    if (n < 1 || a[0].type != VAL_STRING) return val_null();
    const char *s = a[0].as.s;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '{') {
        Value dict = val_dict_empty();
        s++;
        while (*s && *s != '}') {
            while (*s == ' ' || *s == ',' || *s == '\t') s++;
            if (*s != '"') break;
            s++;
            char key[128]; int ki = 0;
            while (*s && *s != '"' && ki < 127) key[ki++] = *s++;
            key[ki] = '\0'; if (*s == '"') s++;
            while (*s == ' ' || *s == ':') s++;
            Value val;
            if (*s == '"') {
                s++; char vbuf[256]; int vi = 0;
                while (*s && *s != '"' && vi < 255) vbuf[vi++] = *s++;
                vbuf[vi] = '\0'; if (*s == '"') s++;
                val = val_string(vbuf);
            } else if (*s == 't') { val = val_bool(1); s+=4; }
            else if (*s == 'f') { val = val_bool(0); s+=5; }
            else if (*s == 'n') { val = val_null(); s+=4; }
            else { val = val_int(atoll(s)); while (*s && *s != ',' && *s != '}') s++; }
            val_dict_set(&dict, key, val);
        }
        return dict;
    } else if (*s == '[') {
        Value arr = val_array_empty();
        s++;
        while (*s && *s != ']') {
            while (*s == ' ' || *s == ',' || *s == '\t') s++;
            if (*s == '"') {
                s++; char vbuf[256]; int vi = 0;
                while (*s && *s != '"' && vi < 255) vbuf[vi++] = *s++;
                vbuf[vi] = '\0'; if (*s == '"') s++;
                val_array_push(&arr, val_string(vbuf));
            } else if (*s == 't') { val_array_push(&arr, val_bool(1)); s+=4; }
            else if (*s == 'f') { val_array_push(&arr, val_bool(0)); s+=5; }
            else if (*s == 'n') { val_array_push(&arr, val_null()); s+=4; }
            else if (*s == ']') break;
            else { val_array_push(&arr, val_int(atoll(s))); while (*s && *s != ',' && *s != ']') s++; }
        }
        return arr;
    }
    return val_null();
}

static void stdlib_load_json(Env *env) {
    env_set(env, "json_encode", val_native(std_json_encode));
    env_set(env, "json_decode", val_native(std_json_decode));
}

/* ═══════════════════════════════════════════════════════════════
   RANDOM MODULE
   ═══════════════════════════════════════════════════════════════ */
#include <stdlib.h>
#include <time.h>

static int rand_seeded = 0;
static void ensure_seeded(void) { if (!rand_seeded) { srand((unsigned)time(NULL)); rand_seeded = 1; } }

static Value std_random(Value *a, int n) {
    ensure_seeded(); (void)a; (void)n;
    return val_float((double)rand() / RAND_MAX);
}

static Value std_randint(Value *a, int n) {
    ensure_seeded();
    if (n < 2) return val_int(rand());
    long long lo = a[0].type==VAL_INT ? a[0].as.i : (long long)a[0].as.f;
    long long hi = a[1].type==VAL_INT ? a[1].as.i : (long long)a[1].as.f;
    if (hi <= lo) return val_int(lo);
    return val_int(lo + rand() % (hi - lo + 1));
}

static Value std_shuffle(Value *a, int n) {
    ensure_seeded();
    if (n < 1 || a[0].type != VAL_ARRAY) return val_null();
    Value arr = a[0];
    for (int i = arr.as.arr.len - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Value tmp = arr.as.arr.items[i];
        arr.as.arr.items[i] = arr.as.arr.items[j];
        arr.as.arr.items[j] = tmp;
    }
    return arr;
}

static Value std_choice(Value *a, int n) {
    ensure_seeded();
    if (n < 1 || a[0].type != VAL_ARRAY || a[0].as.arr.len == 0) return val_null();
    int idx = rand() % a[0].as.arr.len;
    return a[0].as.arr.items[idx];
}

static void stdlib_load_random(Env *env) {
    env_set(env, "random",  val_native(std_random));
    env_set(env, "randint", val_native(std_randint));
    env_set(env, "shuffle", val_native(std_shuffle));
    env_set(env, "choice",  val_native(std_choice));
}

/* ═══════════════════════════════════════════════════════════════
   HTTP MODULE (simple GET using curl)
   ═══════════════════════════════════════════════════════════════ */

static Value std_http_get(Value *a, int n) {
    if (n < 1 || a[0].type != VAL_STRING) return val_null();
    char cmd[512];
    snprintf(cmd, 511, "curl -fsSL --max-time 10 \"%s\" 2>/dev/null", a[0].as.s);
    FILE *f = popen(cmd, "r");
    if (!f) return val_null();
    char buf[4096]; int total = 0;
    int c;
    while ((c = fgetc(f)) != EOF && total < 4094) buf[total++] = c;
    buf[total] = '\0';
    pclose(f);
    return val_string(buf);
}

static Value std_time_now(Value *a, int n) {
    (void)a; (void)n;
    return val_int((long long)time(NULL));
}

static void stdlib_load_http(Env *env) {
    env_set(env, "http_get",  val_native(std_http_get));
    env_set(env, "time_now",  val_native(std_time_now));
}

