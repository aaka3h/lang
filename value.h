#ifndef VALUE_H
#define VALUE_H

/* ─────────────────────────────────────────────────────────────────
   value.h  —  Runtime values and the Environment (scope)
   
   When your language runs, every expression produces a VALUE.
   A value is one of: int, float, string, bool, null, or function.
   
   The ENVIRONMENT is a hash map: variable name → value.
   Environments are chained (parent pointer) to implement scoping:
   
     global env
       └── function call env
               └── inner block env
   
   When we look up a variable, we search inward → outward.
   This is exactly how Python, JS, and C scoping works.
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* ─────────────────────────────────────────────────────────────────
   Value types
   ───────────────────────────────────────────────────────────────── */
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_NULL,
    VAL_FUNCTION,   /* user-defined fn                */
    VAL_NATIVE,     /* built-in C function            */
    VAL_ARRAY,      /* [1, 2, 3]                      */
    VAL_DICT,       /* {"key": val}                   */
    VAL_CLASS,      /* class definition               */
    VAL_INSTANCE,   /* class instance                 */
} ValueType;

/* Forward declaration */
typedef struct Value Value;
typedef struct Env   Env;

/* A native (built-in) function: takes array of Values, returns a Value */
typedef Value (*NativeFn)(Value *args, int arg_count);

/* ─────────────────────────────────────────────────────────────────
   The Value struct
   ───────────────────────────────────────────────────────────────── */
struct Value {
    ValueType type;
    union {
        long long  i;               /* VAL_INT              */
        double     f;               /* VAL_FLOAT            */
        char       s[256];          /* VAL_STRING           */
        int        b;               /* VAL_BOOL (1/0)       */
        struct {                    /* VAL_FUNCTION         */
            Node *def;              /*   the FnDef AST node */
            Env  *closure;          /*   captured scope     */
            int   chunk_idx;        /*   compiled chunk (-1 if tree-walking) */
        } fn;
        NativeFn   native;          /* VAL_NATIVE           */
        struct {                    /* VAL_ARRAY            */
            struct Value *items;
            int           len;
            int           cap;
        } arr;
        struct {                    /* VAL_DICT             */
            char         **dkeys;
            struct Value  *dvals;
            int            dlen;
            int            dcap;
        } dict;
        struct {                    /* VAL_CLASS            */
            char  name[128];
            /* methods stored as dict: name -> fn value */
            char         **mkeys;
            struct Value  *mvals;
            int            mlen;
        } cls;
        struct {                    /* VAL_INSTANCE         */
            char  class_name[128];
            /* fields + methods as flat arrays */
            char         **fkeys;
            struct Value  *fvals;
            int            flen;
            int            fcap;
        } inst;
    } as;
};

/* ─────────────────────────────────────────────────────────────────
   Value constructors — convenient helpers
   ───────────────────────────────────────────────────────────────── */
static inline Value val_int(long long i)   { Value v; v.type=VAL_INT;   v.as.i=i;  return v; }
static inline Value val_float(double f)    { Value v; v.type=VAL_FLOAT; v.as.f=f;  return v; }
static inline Value val_bool(int b)        { Value v; v.type=VAL_BOOL;  v.as.b=b;  return v; }
static inline Value val_null(void)         { Value v; v.type=VAL_NULL;             return v; }
static inline Value val_native(NativeFn fn){ Value v; v.type=VAL_NATIVE;v.as.native=fn; return v; }

static inline Value val_array_empty(void) {
    Value v; v.type=VAL_ARRAY;
    v.as.arr.cap = 8;
    v.as.arr.len = 0;
    v.as.arr.items = (Value*)malloc(8 * sizeof(Value));
    return v;
}

static inline Value val_dict_empty(void) {
    Value v; v.type = VAL_DICT;
    v.as.dict.dcap  = 8;
    v.as.dict.dlen  = 0;
    v.as.dict.dkeys = (char**)malloc(8 * sizeof(char*));
    v.as.dict.dvals = (Value*)malloc(8 * sizeof(Value));
    return v;
}

static inline void val_dict_set(Value *d, const char *key, Value val) {
    /* update if exists */
    for (int i = 0; i < d->as.dict.dlen; i++) {
        if (strcmp(d->as.dict.dkeys[i], key) == 0) {
            d->as.dict.dvals[i] = val;
            return;
        }
    }
    /* grow if needed */
    if (d->as.dict.dlen >= d->as.dict.dcap) {
        d->as.dict.dcap *= 2;
        d->as.dict.dkeys = (char**)realloc(d->as.dict.dkeys,
            d->as.dict.dcap * sizeof(char*));
        d->as.dict.dvals = (Value*)realloc(d->as.dict.dvals,
            d->as.dict.dcap * sizeof(Value));
    }
    char *_k = (char*)malloc(strlen(key)+1); strcpy(_k, key); d->as.dict.dkeys[d->as.dict.dlen] = _k;
    d->as.dict.dvals[d->as.dict.dlen] = val;
    d->as.dict.dlen++;
}

static inline int val_dict_get(Value *d, const char *key, Value *out) {
    for (int i = 0; i < d->as.dict.dlen; i++) {
        if (strcmp(d->as.dict.dkeys[i], key) == 0) {
            *out = d->as.dict.dvals[i]; return 1;
        }
    }
    return 0;
}

static inline int val_dict_del(Value *d, const char *key) {
    for (int i = 0; i < d->as.dict.dlen; i++) {
        if (strcmp(d->as.dict.dkeys[i], key) == 0) {
            free(d->as.dict.dkeys[i]);
            for (int j = i; j < d->as.dict.dlen-1; j++) {
                d->as.dict.dkeys[j] = d->as.dict.dkeys[j+1];
                d->as.dict.dvals[j] = d->as.dict.dvals[j+1];
            }
            d->as.dict.dlen--;
            return 1;
        }
    }
    return 0;
}

/* Instance field get/set */
static inline int inst_get(Value *inst, const char *key, Value *out) {
    for (int i=0;i<inst->as.inst.flen;i++) {
        if (strcmp(inst->as.inst.fkeys[i], key)==0) {
            *out = inst->as.inst.fvals[i]; return 1;
        }
    }
    return 0;
}

static inline void inst_set(Value *inst, const char *key, Value val) {
    for (int i=0;i<inst->as.inst.flen;i++) {
        if (strcmp(inst->as.inst.fkeys[i], key)==0) {
            inst->as.inst.fvals[i] = val; return;
        }
    }
    if (inst->as.inst.flen >= inst->as.inst.fcap) {
        inst->as.inst.fcap *= 2;
        inst->as.inst.fkeys = (char**)realloc(inst->as.inst.fkeys,
            inst->as.inst.fcap * sizeof(char*));
        inst->as.inst.fvals = (Value*)realloc(inst->as.inst.fvals,
            inst->as.inst.fcap * sizeof(Value));
    }
    char *_k = (char*)malloc(strlen(key)+1); strcpy(_k, key);
    inst->as.inst.fkeys[inst->as.inst.flen] = _k;
    inst->as.inst.fvals[inst->as.inst.flen] = val;
    inst->as.inst.flen++;
}

static inline Value val_new_instance(const char *class_name) {
    Value v; v.type = VAL_INSTANCE;
    strncpy(v.as.inst.class_name, class_name, 127);
    v.as.inst.fcap  = 16;
    v.as.inst.flen  = 0;
    v.as.inst.fkeys = (char**)malloc(16 * sizeof(char*));
    v.as.inst.fvals = (Value*)malloc(16 * sizeof(Value));
    return v;
}

static inline void val_array_push(Value *arr, Value item) {
    if (arr->as.arr.len >= arr->as.arr.cap) {
        arr->as.arr.cap *= 2;
        arr->as.arr.items = (Value*)realloc(arr->as.arr.items,
            arr->as.arr.cap * sizeof(Value));
    }
    arr->as.arr.items[arr->as.arr.len++] = item;
}

static inline Value val_string(const char *s) {
    Value v; v.type = VAL_STRING;
    strncpy(v.as.s, s, 255); v.as.s[255] = '\0';
    return v;
}

static inline Value val_function(Node *def, Env *closure) {
    Value v; v.type = VAL_FUNCTION;
    v.as.fn.def = def; v.as.fn.closure = closure; v.as.fn.chunk_idx = -1;
    return v;
}

/* Is this value truthy? (like Python's truthiness rules) */
static inline int val_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:   return v.as.b;
        case VAL_INT:    return v.as.i != 0;
        case VAL_FLOAT:  return v.as.f != 0.0;
        case VAL_STRING: return v.as.s[0] != '\0';
        case VAL_NULL:   return 0;
        case VAL_ARRAY:  return v.as.arr.len > 0;
        case VAL_DICT:   return v.as.dict.dlen > 0;
        default:         return 1;
    }
}

/* Print a value to stdout */
static inline void val_print(Value v) {
    switch (v.type) {
        case VAL_INT:    printf("%lld", v.as.i); break;
        case VAL_FLOAT: {
            /* Print without trailing .0 if it's a whole number */
            if (v.as.f == (long long)v.as.f)
                printf("%.1f", v.as.f);
            else
                printf("%g", v.as.f);
            break;
        }
        case VAL_STRING: printf("%s", v.as.s); break;
        case VAL_BOOL:   printf("%s", v.as.b ? "true" : "false"); break;
        case VAL_NULL:   printf("null"); break;
        case VAL_FUNCTION: printf("<fn %s>", v.as.fn.def->as.fn_def.name); break;
        case VAL_NATIVE:   printf("<native fn>"); break;
        case VAL_CLASS:
            printf("<class %s>", v.as.cls.name);
            break;
        case VAL_INSTANCE: {
            printf("<%s: {", v.as.inst.class_name);
            for (int i=0;i<v.as.inst.flen;i++) {
                /* skip method entries */
                if (v.as.inst.fvals[i].type == VAL_FUNCTION ||
                    v.as.inst.fvals[i].type == VAL_NATIVE) continue;
                printf("%s: ", v.as.inst.fkeys[i]);
                val_print(v.as.inst.fvals[i]);
                printf(", ");
            }
            printf("}>");
            break;
        }
        case VAL_DICT: {
            printf("{");
            for (int i=0;i<v.as.dict.dlen;i++) {
                printf("\"%s\": ", v.as.dict.dkeys[i]);
                val_print(v.as.dict.dvals[i]);
                if (i < v.as.dict.dlen-1) printf(", ");
            }
            printf("}");
            break;
        }
        case VAL_ARRAY:
            printf("[");
            for (int i=0;i<v.as.arr.len;i++) {
                val_print(v.as.arr.items[i]);
                if (i < v.as.arr.len-1) printf(", ");
            }
            printf("]");
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────
   Environment (scope) — a simple hash map + parent chain
   ───────────────────────────────────────────────────────────────── */
#define ENV_SIZE 64   /* hash table bucket count */

typedef struct EnvEntry {
    char            name[MAX_NAME];
    Value           value;
    struct EnvEntry *next;   /* chaining for collisions */
} EnvEntry;

struct Env {
    EnvEntry *buckets[ENV_SIZE];
    Env      *parent;   /* enclosing scope (NULL for global) */
};

/* Simple djb2 hash */
static inline unsigned int env_hash(const char *s) {
    unsigned int h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)*s++;
    return h % ENV_SIZE;
}

/* Create a new environment, optionally with a parent */
static inline Env* env_new(Env *parent) {
    Env *e = (Env*)calloc(1, sizeof(Env));
    if (!e) { fprintf(stderr, "Out of memory\n"); exit(1); }
    e->parent = parent;
    return e;
}

/* Define a variable in THIS scope (for 'let' and params) */
static inline void env_set(Env *e, const char *name, Value v) {
    unsigned int h = env_hash(name);
    /* Check if already exists in this scope → update */
    for (EnvEntry *en = e->buckets[h]; en; en = en->next) {
        if (strcmp(en->name, name) == 0) { en->value = v; return; }
    }
    /* New entry */
    EnvEntry *en = (EnvEntry*)malloc(sizeof(EnvEntry));
    if (!en) { fprintf(stderr, "Out of memory\n"); exit(1); }
    strncpy(en->name, name, MAX_NAME - 1);
    en->value = v;
    en->next  = e->buckets[h];
    e->buckets[h] = en;
}

/* Get a variable — searches this scope then all parents */
static inline int env_get(Env *e, const char *name, Value *out) {
    unsigned int h = env_hash(name);
    for (Env *scope = e; scope; scope = scope->parent) {
        for (EnvEntry *en = scope->buckets[h]; en; en = en->next) {
            if (strcmp(en->name, name) == 0) { *out = en->value; return 1; }
        }
    }
    return 0;  /* not found */
}

/* Assign to existing variable (searches up the scope chain) */
static inline int env_assign(Env *e, const char *name, Value v) {
    unsigned int h = env_hash(name);
    for (Env *scope = e; scope; scope = scope->parent) {
        for (EnvEntry *en = scope->buckets[h]; en; en = en->next) {
            if (strcmp(en->name, name) == 0) { en->value = v; return 1; }
        }
    }
    return 0;  /* not found */
}

/* Free an environment (not its parent) */
static inline void env_free(Env *e) {
    if (!e) return;
    for (int i = 0; i < ENV_SIZE; i++) {
        EnvEntry *en = e->buckets[i];
        while (en) { EnvEntry *next = en->next; free(en); en = next; }
    }
    free(e);
}

#endif /* VALUE_H */
