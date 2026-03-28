/* ─────────────────────────────────────────────────────────────────
   interp.c  —  Tree-walking Interpreter
   
   This is Phase 3. The interpreter walks the AST recursively.
   For every node type, it knows what to do:
   
     Int(42)         → return Value(42)
     BinOp(+, a, b)  → eval(a) + eval(b)
     Let(x, expr)    → env_set(env, "x", eval(expr))
     If(cond,t,e)    → if eval(cond) then eval(t) else eval(e)
     Call(f, args)   → create new env, bind params, eval body
   
   The magic is the ENVIRONMENT — a chained hash map that
   implements lexical scoping (variables live in the scope they
   were defined in, and inner scopes can see outer scopes).
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "interp.h"
#include "stdlib.h"

/* ─────────────────────────────────────────────────────────────────
   RETURN SIGNAL
   
   When we hit 'return expr' inside a function, we need to unwind
   the call stack immediately. In C we do this with a flag + value
   stored in the interpreter struct, checked after every eval().
   ───────────────────────────────────────────────────────────────── */
static int    g_returning = 0;
static Value  g_return_val;

static int    g_throwing  = 0;   /* 1 when a throw/error is in flight */
static Value  g_throw_val;       /* the thrown value                  */
static int    g_breaking   = 0;   /* 1 when break is in flight         */
static int    g_continuing = 0;   /* 1 when continue is in flight      */

/* Set a runtime error — also triggers throw so try/catch can intercept */
static Value runtime_error(Interpreter *interp, const char *msg) {
    if (!interp->error) {
        interp->error = 1;
        strncpy(interp->errmsg, msg, 255);
        /* Make it catchable */
        if (!g_throwing) {
            g_throwing = 1;
            g_throw_val = val_string(msg);
        }
    }
    return val_null();
}

/* ─────────────────────────────────────────────────────────────────
   BUILT-IN FUNCTIONS
   
   These are native C functions exposed to your language.
   They follow the NativeFn signature: Value fn(Value *args, int n)
   ───────────────────────────────────────────────────────────────── */

static Value builtin_sqrt(Value *args, int n) {
    if (n < 1) return val_null();
    double x = args[0].type == VAL_INT ? (double)args[0].as.i : args[0].as.f;
    return val_float(sqrt(x));
}

static Value builtin_abs(Value *args, int n) {
    if (n < 1) return val_null();
    if (args[0].type == VAL_INT)   return val_int(llabs(args[0].as.i));
    if (args[0].type == VAL_FLOAT) return val_float(fabs(args[0].as.f));
    return val_null();
}

static Value builtin_pow(Value *args, int n) {
    if (n < 2) return val_null();
    double base = args[0].type == VAL_INT ? (double)args[0].as.i : args[0].as.f;
    double exp_ = args[1].type == VAL_INT ? (double)args[1].as.i : args[1].as.f;
    double r = pow(base, exp_);
    /* Return int if both args were ints and result is whole */
    if (args[0].type == VAL_INT && args[1].type == VAL_INT && r == (long long)r)
        return val_int((long long)r);
    return val_float(r);
}

static Value builtin_floor(Value *args, int n) {
    if (n < 1) return val_null();
    double x = args[0].type == VAL_INT ? (double)args[0].as.i : args[0].as.f;
    return val_int((long long)floor(x));
}

static Value builtin_ceil(Value *args, int n) {
    if (n < 1) return val_null();
    double x = args[0].type == VAL_INT ? (double)args[0].as.i : args[0].as.f;
    return val_int((long long)ceil(x));
}

static Value builtin_len(Value *args, int n) {
    if (n < 1) return val_null();
    if (args[0].type == VAL_STRING)
        return val_int((long long)strlen(args[0].as.s));
    return val_null();
}

static Value builtin_int(Value *args, int n) {
    if (n < 1) return val_null();
    switch (args[0].type) {
        case VAL_INT:    return args[0];
        case VAL_FLOAT:  return val_int((long long)args[0].as.f);
        case VAL_STRING: return val_int(atoll(args[0].as.s));
        case VAL_BOOL:   return val_int(args[0].as.b);
        default:         return val_int(0);
    }
}

static Value builtin_float(Value *args, int n) {
    if (n < 1) return val_null();
    switch (args[0].type) {
        case VAL_FLOAT:  return args[0];
        case VAL_INT:    return val_float((double)args[0].as.i);
        case VAL_STRING: return val_float(atof(args[0].as.s));
        default:         return val_float(0.0);
    }
}

static Value builtin_str(Value *args, int n) {
    if (n < 1) return val_null();
    char buf[256];
    switch (args[0].type) {
        case VAL_INT:    snprintf(buf, 256, "%lld", args[0].as.i); break;
        case VAL_FLOAT:
            if (args[0].as.f == (long long)args[0].as.f)
                snprintf(buf, 256, "%.1f", args[0].as.f);
            else
                snprintf(buf, 256, "%g", args[0].as.f);
            break;
        case VAL_BOOL:   snprintf(buf, 256, "%s", args[0].as.b ? "true" : "false"); break;
        case VAL_NULL:   snprintf(buf, 256, "null"); break;
        case VAL_STRING: return args[0];
        default:         snprintf(buf, 256, "<fn>"); break;
    }
    return val_string(buf);
}


static Value builtin_push(Value *args, int n) {
    if (n < 2 || args[0].type != VAL_ARRAY) return val_null();
    val_array_push(&args[0], args[1]);
    return args[0];
}
/* NOTE: push works via NODE_CALL which passes args by value.
   For push to persist, we handle it specially in interp_eval CALL case. */

static Value builtin_pop(Value *args, int n) {
    if (n < 1 || args[0].type != VAL_ARRAY) return val_null();
    if (args[0].as.arr.len == 0) return val_null();
    return args[0].as.arr.items[--args[0].as.arr.len];
}

static Value builtin_len2(Value *args, int n) {
    if (n < 1) return val_null();
    if (args[0].type == VAL_STRING) return val_int((long long)strlen(args[0].as.s));
    if (args[0].type == VAL_ARRAY)  return val_int((long long)args[0].as.arr.len);
    return val_null();
}


static Value builtin_has(Value *args, int n) {
    if (n < 2 || args[0].type != VAL_DICT || args[1].type != VAL_STRING)
        return val_bool(0);
    Value dummy;
    return val_bool(val_dict_get(&args[0], args[1].as.s, &dummy));
}

static Value builtin_del(Value *args, int n) {
    if (n < 2 || args[0].type != VAL_DICT || args[1].type != VAL_STRING)
        return val_bool(0);
    return val_bool(val_dict_del(&args[0], args[1].as.s));
}

static Value builtin_keys(Value *args, int n) {
    if (n < 1 || args[0].type != VAL_DICT) return val_null();
    Value arr = val_array_empty();
    for (int i = 0; i < args[0].as.dict.dlen; i++)
        val_array_push(&arr, val_string(args[0].as.dict.dkeys[i]));
    return arr;
}

static Value builtin_values(Value *args, int n) {
    if (n < 1 || args[0].type != VAL_DICT) return val_null();
    Value arr = val_array_empty();
    for (int i = 0; i < args[0].as.dict.dlen; i++)
        val_array_push(&arr, args[0].as.dict.dvals[i]);
    return arr;
}

static Value builtin_type(Value *args, int n) {
    if (n < 1) return val_null();
    switch (args[0].type) {
        case VAL_INT:      return val_string("int");
        case VAL_FLOAT:    return val_string("float");
        case VAL_STRING:   return val_string("string");
        case VAL_BOOL:     return val_string("bool");
        case VAL_NULL:     return val_string("null");
        case VAL_FUNCTION: return val_string("function");
        case VAL_NATIVE:   return val_string("function");
        default:           return val_string("unknown");
    }
}

/* ─────────────────────────────────────────────────────────────────
   interp_init — set up the interpreter + load built-ins
   ───────────────────────────────────────────────────────────────── */
void interp_init(Interpreter *interp) {
    interp->globals   = env_new(NULL);
    interp->error     = 0;
    interp->errmsg[0] = '\0';

    /* Register built-in functions in the global scope */
    env_set(interp->globals, "sqrt",  val_native(builtin_sqrt));
    env_set(interp->globals, "abs",   val_native(builtin_abs));
    env_set(interp->globals, "pow",   val_native(builtin_pow));
    env_set(interp->globals, "floor", val_native(builtin_floor));
    env_set(interp->globals, "ceil",  val_native(builtin_ceil));
    env_set(interp->globals, "len",   val_native(builtin_len));
    env_set(interp->globals, "int",   val_native(builtin_int));
    env_set(interp->globals, "float", val_native(builtin_float));
    env_set(interp->globals, "str",   val_native(builtin_str));
    env_set(interp->globals, "type",  val_native(builtin_type));
    env_set(interp->globals, "push",  val_native(builtin_push));
    env_set(interp->globals, "has",   val_native(builtin_has));
    env_set(interp->globals, "del",   val_native(builtin_del));
    env_set(interp->globals, "keys",  val_native(builtin_keys));
    env_set(interp->globals, "values",val_native(builtin_values));
    env_set(interp->globals, "pop",   val_native(builtin_pop));
    env_set(interp->globals, "len",   val_native(builtin_len2));

    /* import function for VM use */
    /* (tree interpreter handles NODE_IMPORT directly) */
}

void interp_free(Interpreter *interp) {
    env_free(interp->globals);
}

/* ─────────────────────────────────────────────────────────────────
   BINARY OPERATION EVALUATION
   ───────────────────────────────────────────────────────────────── */
static Value eval_binop(Interpreter *interp, const char *op, Value L, Value R) {
    /* String concatenation with + */
    if (strcmp(op, "+") == 0 &&
        (L.type == VAL_STRING || R.type == VAL_STRING)) {
        char buf[256];
        /* Convert both sides to string then concat */
        Value ls = builtin_str(&L, 1);
        Value rs = builtin_str(&R, 1);
        snprintf(buf, 256, "%s%s", ls.as.s, rs.as.s);
        return val_string(buf);
    }

    /* Logical operators */
    if (strcmp(op, "and") == 0) return val_bool(val_truthy(L) && val_truthy(R));
    if (strcmp(op, "or")  == 0) return val_bool(val_truthy(L) || val_truthy(R));

    /* Numeric operations — promote int to float if mixed */
    int both_int = (L.type == VAL_INT && R.type == VAL_INT);
    double lf = L.type == VAL_INT ? (double)L.as.i : L.as.f;
    double rf = R.type == VAL_INT ? (double)R.as.i : R.as.f;
    long long li = L.type == VAL_INT ? L.as.i : (long long)L.as.f;
    long long ri = R.type == VAL_INT ? R.as.i : (long long)R.as.f;

    if (strcmp(op, "+") == 0) return both_int ? val_int(li + ri)  : val_float(lf + rf);
    if (strcmp(op, "-") == 0) return both_int ? val_int(li - ri)  : val_float(lf - rf);
    if (strcmp(op, "*") == 0) return both_int ? val_int(li * ri)  : val_float(lf * rf);
    if (strcmp(op, "/") == 0) {
        if (rf == 0.0) return runtime_error(interp, "Division by zero");
        if (both_int && li % ri == 0) return val_int(li / ri);
        return val_float(lf / rf);
    }
    if (strcmp(op, "%") == 0) {
        if (ri == 0) return runtime_error(interp, "Modulo by zero");
        return val_int(li % ri);
    }

    /* Comparisons */
    if (strcmp(op, "==") == 0) {
        if (L.type == VAL_STRING && R.type == VAL_STRING)
            return val_bool(strcmp(L.as.s, R.as.s) == 0);
        if (L.type == VAL_BOOL && R.type == VAL_BOOL)
            return val_bool(L.as.b == R.as.b);
        if (L.type == VAL_NULL && R.type == VAL_NULL) return val_bool(1);
        return val_bool(lf == rf);
    }
    if (strcmp(op, "!=") == 0) {
        if (L.type == VAL_STRING && R.type == VAL_STRING)
            return val_bool(strcmp(L.as.s, R.as.s) != 0);
        if (L.type == VAL_NULL && R.type == VAL_NULL) return val_bool(0);
        return val_bool(lf != rf);
    }
    if (strcmp(op, "<")  == 0) return val_bool(lf <  rf);
    if (strcmp(op, ">")  == 0) return val_bool(lf >  rf);
    if (strcmp(op, "<=") == 0) return val_bool(lf <= rf);
    if (strcmp(op, ">=") == 0) return val_bool(lf >= rf);

    char msg[64];
    snprintf(msg, 64, "Unknown operator: %s", op);
    return runtime_error(interp, msg);
}

/* ─────────────────────────────────────────────────────────────────
   interp_eval — the heart of the interpreter
   
   Recursively evaluates a node in an environment.
   Returns the resulting Value.
   ───────────────────────────────────────────────────────────────── */
Value interp_eval(Interpreter *interp, Node *node, Env *env) {
    if (!node || interp->error) return val_null();

    switch (node->type) {

        /* ── Literals ─────────────────────────────────────────── */
        case NODE_INT:    return val_int(node->as.int_lit.value);
        case NODE_FLOAT:  return val_float(node->as.float_lit.value);
        case NODE_STRING: {
            const char *s = node->as.str_lit.value;
            /* Only interpolate if string contains { */
            if (strchr(s, '{')) {
                return val_string(std_interpolate(s, env));
            }
            return val_string(s);
        }
        case NODE_BOOL:   return val_bool(node->as.bool_lit.value);
        case NODE_NULL:   return val_null();

        /* ── Variable read ────────────────────────────────────── */
        case NODE_IDENT: {
            Value v;
            if (!env_get(env, node->as.ident.name, &v)) {
                char msg[128];
                snprintf(msg, 128, "Undefined variable '%s' on line %d",
                         node->as.ident.name, node->line);
                return runtime_error(interp, msg);
            }
            return v;
        }

        /* ── Variable declaration: let x = expr ──────────────── */
        case NODE_LET: {
            Value v = interp_eval(interp, node->as.let_stmt.value, env);
            if (interp->error) return val_null();
            env_set(env, node->as.let_stmt.name, v);
            return v;
        }

        /* ── Reassignment: x = expr ───────────────────────────── */
        case NODE_ASSIGN: {
            Value v = interp_eval(interp, node->as.assign.value, env);
            if (interp->error) return val_null();
            if (!env_assign(env, node->as.assign.name, v)) {
                /* Not found in any scope — create in current scope */
                env_set(env, node->as.assign.name, v);
            }
            return v;
        }

        /* ── Binary operation ────────────────────────────────── */
        case NODE_BINOP: {
            Value L = interp_eval(interp, node->as.binop.left,  env);
            if (interp->error) return val_null();
            /* Short-circuit for 'and' / 'or' */
            if (strcmp(node->as.binop.op, "and") == 0 && !val_truthy(L))
                return val_bool(0);
            if (strcmp(node->as.binop.op, "or") == 0 && val_truthy(L))
                return val_bool(1);
            Value R = interp_eval(interp, node->as.binop.right, env);
            if (interp->error) return val_null();
            return eval_binop(interp, node->as.binop.op, L, R);
        }

        /* ── Unary operation ─────────────────────────────────── */
        case NODE_UNARY: {
            Value operand = interp_eval(interp, node->as.unary.operand, env);
            if (interp->error) return val_null();
            if (strcmp(node->as.unary.op, "-") == 0) {
                if (operand.type == VAL_INT)   return val_int(-operand.as.i);
                if (operand.type == VAL_FLOAT) return val_float(-operand.as.f);
                return runtime_error(interp, "Unary minus on non-number");
            }
            if (strcmp(node->as.unary.op, "not") == 0)
                return val_bool(!val_truthy(operand));
            return val_null();
        }

        /* ── dict literal ───────────────────────────────────── */
        case NODE_DICT: {
            Value d = val_dict_empty();
            for (int i = 0; i < node->as.dict.count; i++) {
                Value v = interp_eval(interp, node->as.dict.values[i], env);
                if (interp->error) return val_null();
                val_dict_set(&d, node->as.dict.keys[i], v);
            }
            return d;
        }

        /* ── array literal ──────────────────────────────────── */
        case NODE_ARRAY: {
            Value arr = val_array_empty();
            for (int i = 0; i < node->as.array.count; i++) {
                Value v = interp_eval(interp, node->as.array.elements[i], env);
                if (interp->error) return val_null();
                val_array_push(&arr, v);
            }
            return arr;
        }

        /* ── array/string index: a[i] ────────────────────────────── */
        case NODE_INDEX: {
            Value container;
            if (!env_get(env, node->as.index_expr.name, &container)) {
                char msg[128];
                snprintf(msg, 127, "Undefined variable '%s'", node->as.index_expr.name);
                return runtime_error(interp, msg);
            }
            Value idx = interp_eval(interp, node->as.index_expr.index, env);
            if (interp->error) return val_null();
            long long i = idx.type == VAL_INT ? idx.as.i : (long long)idx.as.f;
            if (container.type == VAL_ARRAY) {
                if (i < 0) i += container.as.arr.len;
                if (i < 0 || i >= container.as.arr.len)
                    return runtime_error(interp, "Array index out of bounds");
                return container.as.arr.items[i];
            }
            if (container.type == VAL_DICT) {
                if (idx.type != VAL_STRING)
                    return runtime_error(interp, "Dict key must be a string");
                Value out;
                if (!val_dict_get(&container, idx.as.s, &out)) {
                    char msg[128];
                    snprintf(msg, 127, "Key not found: '%s'", idx.as.s);
                    return runtime_error(interp, msg);
                }
                return out;
            }
            if (container.type == VAL_STRING) {
                int slen = (int)strlen(container.as.s);
                if (i < 0) i += slen;
                if (i < 0 || i >= slen)
                    return runtime_error(interp, "String index out of bounds");
                char ch[2] = { container.as.s[i], 0 };
                return val_string(ch);
            }
            return runtime_error(interp, "Cannot index non-array/string");
        }

        /* ── array index assign: a[i] = v ───────────────────────── */
        case NODE_INDEX_ASSIGN: {
            Value container;
            if (!env_get(env, node->as.index_assign.name, &container)) {
                char msg[128];
                snprintf(msg, 127, "Undefined variable '%s'", node->as.index_assign.name);
                return runtime_error(interp, msg);
            }
            Value idx = interp_eval(interp, node->as.index_assign.index, env);
            Value val = interp_eval(interp, node->as.index_assign.value, env);
            if (interp->error) return val_null();
            long long i = idx.type == VAL_INT ? idx.as.i : (long long)idx.as.f;
            if (container.type == VAL_ARRAY) {
                if (i < 0) i += container.as.arr.len;
                if (i < 0 || i >= container.as.arr.len)
                    return runtime_error(interp, "Array index out of bounds");
                container.as.arr.items[i] = val;
                env_assign(env, node->as.index_assign.name, container);
                return val;
            }
            if (container.type == VAL_DICT) {
                if (idx.type != VAL_STRING)
                    return runtime_error(interp, "Dict key must be a string");
                val_dict_set(&container, idx.as.s, val);
                env_assign(env, node->as.index_assign.name, container);
                return val;
            }
            return runtime_error(interp, "Cannot index-assign non-array/dict");
        }

        /* ── break ──────────────────────────────────────────── */
        case NODE_BREAK:
            g_breaking = 1;
            return val_null();

        /* ── continue ────────────────────────────────────────── */
        case NODE_CONTINUE:
            g_continuing = 1;
            return val_null();

        /* ── super(args) — call parent's init ───────────────── */
        case NODE_SUPER: {
            /* Get current self */
            Value self_val;
            if (!env_get(env, "__self__", &self_val))
                return runtime_error(interp, "super() used outside a method");

            /* Get parent class name */
            Value parent_name_val;
            if (!env_get(env, "__parent_class__", &parent_name_val))
                return runtime_error(interp, "no parent class for super()");

            /* Look up parent class */
            Value parent_cls;
            if (!env_get(env, parent_name_val.as.s, &parent_cls))
                return runtime_error(interp, "parent class not found");

            /* Find parent's init */
            Value init_fn;
            int found = 0;
            for (int i = 0; i < parent_cls.as.cls.mlen; i++) {
                if (strcmp(parent_cls.as.cls.mkeys[i], "init") == 0) {
                    init_fn = parent_cls.as.cls.mvals[i];
                    found = 1; break;
                }
            }
            if (!found) return val_null();

            Node *def = init_fn.as.fn.def;
            Env *call_env = env_new(init_fn.as.fn.closure);

            int expected = def->as.fn_def.param_count;
            int got = node->as.super_call.arg_count;
            if (expected != got) {
                char msg[128];
                snprintf(msg, 127, "super() expects %d args got %d", expected, got);
                return runtime_error(interp, msg);
            }
            for (int i = 0; i < expected; i++) {
                Value a = interp_eval(interp, node->as.super_call.args[i], env);
                if (interp->error) { env_free(call_env); return val_null(); }
                env_set(call_env, def->as.fn_def.params[i], a);
            }
            env_set(call_env, "__self__", self_val);
            env_set(call_env, "__self_name__", val_string("__self__"));
            env_set(call_env, "__parent_class__", parent_name_val);

            interp_eval(interp, def->as.fn_def.body, call_env);

            /* Write updated self back */
            Value updated;
            if (env_get(call_env, "__self__", &updated)) {
                env_assign(env, "__self__", updated);
                /* Also update original variable */
                Value orig;
                if (env_get(env, "__self_name__", &orig) && orig.type == VAL_STRING)
                    env_assign(env, orig.as.s, updated);
            }
            env_free(call_env);
            if (g_returning) g_returning = 0;
            return val_null();
        }

        /* ── self ───────────────────────────────────────────── */
        case NODE_SELF: {
            Value self_val;
            if (!env_get(env, "__self__", &self_val))
                return runtime_error(interp, "'self' used outside a method");
            return self_val;
        }

        /* ── get attribute: obj.field ─────────────────────────────── */
        case NODE_GET_ATTR: {
            Value obj = interp_eval(interp, node->as.get_attr.object, env);
            if (interp->error) return val_null();
            if (obj.type == VAL_INSTANCE) {
                Value out;
                if (!inst_get(&obj, node->as.get_attr.field, &out)) {
                    char msg[128];
                    snprintf(msg, 127, "No attribute '%s'", node->as.get_attr.field);
                    return runtime_error(interp, msg);
                }
                return out;
            }
            return runtime_error(interp, "Cannot get attribute of non-instance");
        }

        /* ── set attribute: obj.field = val ───────────────────────── */
        case NODE_SET_ATTR: {
            Value obj = interp_eval(interp, node->as.set_attr.object, env);
            if (interp->error) return val_null();
            Value val = interp_eval(interp, node->as.set_attr.value, env);
            if (interp->error) return val_null();
            if (obj.type == VAL_INSTANCE) {
                inst_set(&obj, node->as.set_attr.field, val);
                /* Write back to env if object came from an ident */
                if (node->as.set_attr.object->type == NODE_IDENT)
                    env_assign(env, node->as.set_attr.object->as.ident.name, obj);
                else if (node->as.set_attr.object->type == NODE_SELF) {
                    Value self_val;
                    if (env_get(env, "__self__", &self_val)) {
                        inst_set(&self_val, node->as.set_attr.field, val);
                        env_assign(env, "__self__", self_val);
                        /* Also write back to the original variable */
                        Value orig_name_val;
                        if (env_get(env, "__self_name__", &orig_name_val))
                            env_assign(env, orig_name_val.as.s, self_val);
                    }
                }
                return val;
            }
            return runtime_error(interp, "Cannot set attribute of non-instance");
        }

        /* ── class definition ─────────────────────────────────────── */
        case NODE_CLASS_DEF: {/* parent stored in class_def.parent */
            Value cls;
            cls.type = VAL_CLASS;
            strncpy(cls.as.cls.name, node->as.class_def.name, 127);
            cls.as.cls.mlen  = node->as.class_def.method_count;
            cls.as.cls.mkeys = (char**)malloc(cls.as.cls.mlen * sizeof(char*));
            cls.as.cls.mvals = (Value*)malloc(cls.as.cls.mlen * sizeof(Value));
            for (int i = 0; i < node->as.class_def.method_count; i++) {
                Node *m = node->as.class_def.methods[i];
                char *_k = (char*)malloc(strlen(m->as.fn_def.name)+1);
                strcpy(_k, m->as.fn_def.name);
                cls.as.cls.mkeys[i] = _k;
                cls.as.cls.mvals[i] = val_function(m, env);
            }
            env_set(env, node->as.class_def.name, cls);

            /* If this class has a parent, store reference for inheritance */
            if (node->as.class_def.parent[0] != '\0') {
                Value parent_cls;
                if (env_get(env, node->as.class_def.parent, &parent_cls)) {
                    char parent_key[128];
                    snprintf(parent_key, 127, "__parent__%s", node->as.class_def.name);
                    env_set(env, parent_key, parent_cls);
                }
            }

            return cls;
        }

        /* ── throw ──────────────────────────────────────────── */
        case NODE_THROW: {
            Value v = interp_eval(interp, node->as.throw_stmt.value, env);
            if (!interp->error) {
                g_throwing = 1;
                g_throw_val = v;
                interp->error = 1;
                if (v.type == VAL_STRING)
                    strncpy(interp->errmsg, v.as.s, 255);
                else
                    strncpy(interp->errmsg, "thrown error", 255);
            }
            return val_null();
        }

        /* ── try / catch ─────────────────────────────────────────── */
        case NODE_TRY: {
            /* Save state */
            int   saved_error     = interp->error;
            int   saved_throwing  = g_throwing;
            char  saved_msg[256];
            strncpy(saved_msg, interp->errmsg, 255);

            /* Reset error state for the try block */
            interp->error     = 0;
            interp->errmsg[0] = 0;
            g_throwing        = 0;

            /* Execute try block */
            interp_eval(interp, node->as.try_stmt.try_block, env);

            if (interp->error || g_throwing) {
                /* An error was thrown — run the catch block */
                Value err_val = g_throw_val;
                if (err_val.type == VAL_NULL)
                    err_val = val_string(interp->errmsg);

                /* Reset error so catch block runs clean */
                interp->error = 0;
                interp->errmsg[0] = 0;
                g_throwing = 0;

                /* Bind error variable in a child scope */
                Env *catch_env = env_new(env);
                env_set(catch_env, node->as.try_stmt.err_name, err_val);
                interp_eval(interp, node->as.try_stmt.catch_block, catch_env);
                env_free(catch_env);
            }
            return val_null();
        }

        /* ── import statement ────────────────────────────────── */
        case NODE_IMPORT: {
            if (!stdlib_import(env, node->as.import_stmt.module)) {
                char msg[128];
                snprintf(msg, 128, "Unknown module '%s'", node->as.import_stmt.module);
                return runtime_error(interp, msg);
            }
            return val_null();
        }

        /* ── print statement ──────────────────────────────────── */
        case NODE_PRINT: {
            Value v = interp_eval(interp, node->as.print_stmt.value, env);
            if (!interp->error) { val_print(v); printf("\n"); }
            return v;
        }

        /* ── return statement ─────────────────────────────────── */
        case NODE_RETURN: {
            Value v = val_null();
            if (node->as.return_stmt.value)
                v = interp_eval(interp, node->as.return_stmt.value, env);
            if (!interp->error) { g_returning = 1; g_return_val = v; }
            return v;
        }

        /* ── Block: run each statement in order ─────────────── */
        case NODE_BLOCK: {
            Value last = val_null();
            for (int i = 0; i < node->as.block.count; i++) {
                last = interp_eval(interp, node->as.block.stmts[i], env);
                if (interp->error || g_returning) break;
            }
            return last;
        }

        /* ── if / else ────────────────────────────────────────── */
        case NODE_IF: {
            Value cond = interp_eval(interp, node->as.if_stmt.cond, env);
            if (interp->error) return val_null();
            if (val_truthy(cond)) {
                Env *child = env_new(env);
                Value r = interp_eval(interp, node->as.if_stmt.then_block, child);
                env_free(child);
                return r;
            } else if (node->as.if_stmt.else_block) {
                Env *child = env_new(env);
                Value r = interp_eval(interp, node->as.if_stmt.else_block, child);
                env_free(child);
                return r;
            }
            return val_null();
        }

        /* ── while loop ───────────────────────────────────────── */
        case NODE_WHILE: {
            while (!interp->error && !g_returning) {
                Value cond = interp_eval(interp, node->as.while_stmt.cond, env);
                if (interp->error || !val_truthy(cond)) break;
                Env *child = env_new(env);
                interp_eval(interp, node->as.while_stmt.body, child);
                env_free(child);
            }
            return val_null();
        }

        /* ── for loop ─────────────────────────────────────────── */
        case NODE_FOR: {
            Env *loop_env = env_new(env);
            interp_eval(interp, node->as.for_stmt.init, loop_env);
            while (!interp->error && !g_returning) {
                Value cond = interp_eval(interp, node->as.for_stmt.cond, loop_env);
                if (interp->error || !val_truthy(cond)) break;
                Env *body_env = env_new(loop_env);
                interp_eval(interp, node->as.for_stmt.body, body_env);
                env_free(body_env);
                if (interp->error || g_returning) break;
                interp_eval(interp, node->as.for_stmt.step, loop_env);
            }
            env_free(loop_env);
            return val_null();
        }

        /* ── Function definition ──────────────────────────────── */
        case NODE_FN_DEF: {
            /* Store the function as a value in the current scope */
            Value fn = val_function(node, env);
            env_set(env, node->as.fn_def.name, fn);
            return fn;
        }

        /* ── Function call ────────────────────────────────────── */
        case NODE_CALL: {
            /* ── obj.method(args) call ── */
            if (strchr(node->as.call.name, '.') != NULL) {
                /* Split "obj.method" or "__self__.method" */
                char obj_name[MAX_NAME], method_name[MAX_NAME];
                const char *dot = strchr(node->as.call.name, '.');
                int obj_len = (int)(dot - node->as.call.name);
                strncpy(obj_name, node->as.call.name, obj_len);
                obj_name[obj_len] = 0;
                strncpy(method_name, dot+1, MAX_NAME-1);

                Value obj;
                if (strcmp(obj_name, "__self__") == 0) {
                    if (!env_get(env, "__self__", &obj))
                        return runtime_error(interp, "self not available");
                } else {
                    if (!env_get(env, obj_name, &obj)) {
                        char msg[128]; snprintf(msg,127,"Undefined: '%s'",obj_name);
                        return runtime_error(interp, msg);
                    }
                }

                if (obj.type != VAL_INSTANCE) {
                    char msg[128];
                    snprintf(msg, 127, "'%s' is not an instance", obj_name);
                    return runtime_error(interp, msg);
                }

                /* Find method in instance (copied from class at init) */
                Value method;
                if (!inst_get(&obj, method_name, &method)) {
                    char msg[128];
                    snprintf(msg, 127, "No method '%s'", method_name);
                    return runtime_error(interp, msg);
                }

                if (method.type != VAL_FUNCTION)
                    return runtime_error(interp, "Attribute is not a method");

                Node *def = method.as.fn.def;
                Env *call_env = env_new(method.as.fn.closure);

                /* Bind params */
                int expected = def->as.fn_def.param_count;
                int got = node->as.call.arg_count;
                if (expected != got) {
                    char msg[128];
                    snprintf(msg,127,"Method expects %d args got %d",expected,got);
                    return runtime_error(interp, msg);
                }
                for (int i=0; i<expected; i++) {
                    Value a = interp_eval(interp, node->as.call.args[i], env);
                    if (interp->error) { env_free(call_env); return val_null(); }
                    env_set(call_env, def->as.fn_def.params[i], a);
                }

                /* Bind self */
                env_set(call_env, "__self__", obj);
                env_set(call_env, "__self_name__",
                    strcmp(obj_name,"__self__")==0 ? val_string("__self__") : val_string(obj_name));
                /* Pass parent class name for super() */
                char parent_key2[128];
                snprintf(parent_key2, 127, "__parent__%s", obj.as.inst.class_name);
                Value parent_ref;
                if (env_get(env, parent_key2, &parent_ref))
                    env_set(call_env, "__parent_class__", val_string(parent_ref.as.cls.name));

                /* Run body */
                interp_eval(interp, def->as.fn_def.body, call_env);

                /* Get updated self and write back */
                Value updated_self;
                if (env_get(call_env, "__self__", &updated_self)) {
                    if (strcmp(obj_name, "__self__") == 0)
                        env_assign(env, "__self__", updated_self);
                    else
                        env_assign(env, obj_name, updated_self);
                }

                env_free(call_env);
                Value ret = val_null();
                if (g_returning) { ret = g_return_val; g_returning = 0; }
                return ret;
            }
            /* Evaluate all arguments first */
            Value args[MAX_ARGS];
            for (int i = 0; i < node->as.call.arg_count; i++) {
                args[i] = interp_eval(interp, node->as.call.args[i], env);
                if (interp->error) return val_null();
            }

            /* Look up the function */
            Value fn;
            if (!env_get(env, node->as.call.name, &fn)) {
                char msg[128];
                snprintf(msg, 128, "Undefined function '%s' on line %d",
                         node->as.call.name, node->line);
                return runtime_error(interp, msg);
            }

            /* Class instantiation: ClassName(args) */
            if (fn.type == VAL_CLASS) {
                Value instance = val_new_instance(fn.as.cls.name);

                /* Copy parent methods first if class has a parent */
                Value parent_name_v;
                if (env_get(env, fn.as.cls.name, &parent_name_v)) {
                    /* look for __parent__ field in class */
                }
                /* Check if this class has a parent stored */
                char parent_key[128];
                snprintf(parent_key, 127, "__parent__%s", fn.as.cls.name);
                Value parent_cls_v;
                if (env_get(env, parent_key, &parent_cls_v) &&
                    parent_cls_v.type == VAL_CLASS) {
                    for (int i=0; i<parent_cls_v.as.cls.mlen; i++)
                        inst_set(&instance, parent_cls_v.as.cls.mkeys[i],
                                 parent_cls_v.as.cls.mvals[i]);
                }

                /* Copy child methods (overrides parent) */
                for (int i=0; i<fn.as.cls.mlen; i++)
                    inst_set(&instance, fn.as.cls.mkeys[i], fn.as.cls.mvals[i]);

                /* Call init if exists */
                Value init_fn;
                if (inst_get(&instance, "init", &init_fn) &&
                    init_fn.type == VAL_FUNCTION) {
                    Node *def = init_fn.as.fn.def;
                    Env *call_env = env_new(init_fn.as.fn.closure);
                    int expected = def->as.fn_def.param_count;
                    int got = node->as.call.arg_count;
                    if (expected != got) {
                        char msg[128];
                        snprintf(msg,127,"init expects %d args got %d",expected,got);
                        return runtime_error(interp, msg);
                    }
                    for (int i=0; i<expected; i++) {
                        Value a = interp_eval(interp, node->as.call.args[i], env);
                        if (interp->error) { env_free(call_env); return val_null(); }
                        env_set(call_env, def->as.fn_def.params[i], a);
                    }
                    env_set(call_env, "__self__", instance);
                    env_set(call_env, "__self_name__", val_string("__self__"));
                    /* Pass parent class for super() in init */
                    char pk[128];
                    snprintf(pk, 127, "__parent__%s", fn.as.cls.name);
                    Value pr;
                    if (env_get(env, pk, &pr))
                        env_set(call_env, "__parent_class__", val_string(pr.as.cls.name));
                    interp_eval(interp, def->as.fn_def.body, call_env);
                    Value updated;
                    if (env_get(call_env, "__self__", &updated))
                        instance = updated;
                    env_free(call_env);
                    if (g_returning) g_returning = 0;
                }
                return instance;
            }

            /* Special: push/pop mutate arrays in-place via env */
            if (fn.type == VAL_NATIVE &&
                (strcmp(node->as.call.name, "push") == 0 ||
                 strcmp(node->as.call.name, "pop")  == 0 ||
                 strcmp(node->as.call.name, "del")  == 0)) {
                if (node->as.call.arg_count >= 1 &&
                    node->as.call.args[0]->type == NODE_IDENT) {
                    const char *vname = node->as.call.args[0]->as.ident.name;
                    Value arr;
                    if (!env_get(env, vname, &arr)) {
                        return runtime_error(interp, "Undefined variable in push/pop");
                    }
                    Value result = val_null();
                    /* del mutates dict */
                    if (strcmp(node->as.call.name, "del") == 0 &&
                        node->as.call.arg_count >= 2) {
                        Value key = interp_eval(interp, node->as.call.args[1], env);
                        val_dict_del(&arr, key.as.s);
                        env_assign(env, vname, arr);
                        return val_bool(1);
                    }
                    if (strcmp(node->as.call.name, "push") == 0 &&
                        node->as.call.arg_count >= 2) {
                        Value item = interp_eval(interp, node->as.call.args[1], env);
                        if (interp->error) return val_null();
                        val_array_push(&arr, item);
                        result = arr;
                    } else if (strcmp(node->as.call.name, "pop") == 0) {
                        if (arr.as.arr.len > 0)
                            result = arr.as.arr.items[--arr.as.arr.len];
                    }
                    env_assign(env, vname, arr);
                    return result;
                }
            }

            /* Native (built-in) function */
            if (fn.type == VAL_NATIVE)
                return fn.as.native(args, node->as.call.arg_count);

            /* User-defined function */
            if (fn.type == VAL_FUNCTION) {
                Node *def = fn.as.fn.def;
                int expected = def->as.fn_def.param_count;
                int got      = node->as.call.arg_count;
                if (expected != got) {
                    char msg[128];
                    snprintf(msg, 128,
                             "'%s' expects %d arg(s), got %d",
                             def->as.fn_def.name, expected, got);
                    return runtime_error(interp, msg);
                }

                /* Create a new scope for this call, parented to the closure */
                Env *call_env = env_new(fn.as.fn.closure);
                for (int i = 0; i < expected; i++)
                    env_set(call_env, def->as.fn_def.params[i], args[i]);

                /* Execute the body */
                interp_eval(interp, def->as.fn_def.body, call_env);
                env_free(call_env);

                /* Collect the return value */
                Value ret = val_null();
                if (g_returning) { ret = g_return_val; g_returning = 0; }
                return ret;
            }

            char msg[128];
            snprintf(msg, 128, "'%s' is not a function", node->as.call.name);
            return runtime_error(interp, msg);
        }

        default: {
            char msg[64];
            snprintf(msg, 64, "Unknown node type %d", node->type);
            return runtime_error(interp, msg);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────
   interp_run — convenience: parse + run a source string
   ───────────────────────────────────────────────────────────────── */
/* Run source code in a given environment (for user module imports) */
int interp_run_source(Env *env, const char *src) {
    Node *tree = parse_source(src);
    if (!tree) return 0;
    Interpreter tmp_interp;
    tmp_interp.globals = env;
    tmp_interp.error   = 0;
    tmp_interp.errmsg[0] = '\0';
    interp_eval(&tmp_interp, tree, env);
    int ok = !tmp_interp.error;
    ast_free(tree);
    return ok;
}

void interp_run(Interpreter *interp, const char *src) {
    Node *tree = parse_source(src);
    if (!tree) { fprintf(stderr, "Parse failed\n"); return; }

    interp_eval(interp, tree, interp->globals);

    if (interp->error)
        fprintf(stderr, "Runtime error: %s\n", interp->errmsg);

    ast_free(tree);
}
