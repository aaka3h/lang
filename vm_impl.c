#include <stdio.h>
#include <string.h>
#include <math.h>
#include "vm.h"
#include "error.h"
#include "stdlib.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "error.h"

/* Stack operations */
static void push(VM *vm, Value v) {
    if (vm->stack_top >= VM_STACK_MAX) {
        vm->error = 1;
        strncpy(vm->errmsg, "Stack overflow", 255);
        return;
    }
    vm->stack[vm->stack_top++] = v;
}

static Value pop(VM *vm) {
    if (vm->stack_top <= 0) {
        vm->error = 1;
        strncpy(vm->errmsg, "Stack underflow", 255);
        return val_null();
    }
    return vm->stack[--vm->stack_top];
}

static Value peek(VM *vm) {
    if (vm->stack_top <= 0) return val_null();
    return vm->stack[vm->stack_top - 1];
}

/* Runtime error helper */
static void vm_error(VM *vm, const char *msg) {
    if (!vm->error) {
        vm->error = 1;
        strncpy(vm->errmsg, msg, 255);
    }
}

/* ─────────────────────────────────────────────────────────────────
   Arithmetic helper — same logic as the tree-walking interpreter
   ───────────────────────────────────────────────────────────────── */
static Value do_arith(VM *vm, OpCode op, Value L, Value R) {
    /* String + anything = concatenation */
    if (op == OP_ADD &&
        (L.type == VAL_STRING || R.type == VAL_STRING)) {
        char buf[256];
        char ls[256], rs[512];
        /* Convert to string */
        if (L.type == VAL_STRING) strncpy(ls, L.as.s, 255);
        else if (L.type == VAL_INT) snprintf(ls, 256, "%lld", L.as.i);
        else if (L.type == VAL_FLOAT) snprintf(ls, 256, "%g", L.as.f);
        else if (L.type == VAL_BOOL) snprintf(ls, 256, "%s", L.as.b?"true":"false");
        else strncpy(ls, "null", 255);

        if (R.type == VAL_STRING) strncpy(rs, R.as.s, 255);
        else if (R.type == VAL_INT) snprintf(rs, 256, "%lld", R.as.i);
        else if (R.type == VAL_FLOAT) snprintf(rs, 256, "%g", R.as.f);
        else if (R.type == VAL_BOOL) snprintf(rs, 256, "%s", R.as.b?"true":"false");
        else strncpy(rs, "null", 511);

        snprintf(buf, 256, "%.255s%.255s", ls, rs);
        return val_string(buf);
    }

    int both_int = (L.type == VAL_INT && R.type == VAL_INT);
    long long li = L.type == VAL_INT ? L.as.i : (long long)L.as.f;
    long long ri = R.type == VAL_INT ? R.as.i : (long long)R.as.f;
    double    lf = L.type == VAL_INT ? (double)L.as.i : L.as.f;
    double    rf = R.type == VAL_INT ? (double)R.as.i : R.as.f;

    switch (op) {
        case OP_ADD:  return both_int ? val_int(li+ri)  : val_float(lf+rf);
        case OP_SUB:  return both_int ? val_int(li-ri)  : val_float(lf-rf);
        case OP_MUL:  return both_int ? val_int(li*ri)  : val_float(lf*rf);
        case OP_DIV:
            if (rf == 0.0) { vm_error(vm, "Division by zero"); return val_null(); }
            if (both_int && li % ri == 0) return val_int(li/ri);
            return val_float(lf/rf);
        case OP_MOD:
            if (ri == 0) { vm_error(vm, "Modulo by zero"); return val_null(); }
            return val_int(li % ri);
        case OP_EQ:   return val_bool(
            L.type == VAL_STRING && R.type == VAL_STRING
                ? strcmp(L.as.s, R.as.s) == 0
                : lf == rf);
        case OP_NEQ:  return val_bool(lf != rf);
        case OP_LT:   return val_bool(lf <  rf);
        case OP_GT:   return val_bool(lf >  rf);
        case OP_LTEQ: return val_bool(lf <= rf);
        case OP_GTEQ: return val_bool(lf >= rf);
        default:      vm_error(vm, "Unknown opcode in arith"); return val_null();
    }
}

/* ─────────────────────────────────────────────────────────────────
   vm_run — the main execution loop
   
   This is the FETCH → DECODE → EXECUTE cycle.
   ───────────────────────────────────────────────────────────────── */
void vm_run(VM *vm, int chunk_idx) {
    if (chunk_idx < 0 || chunk_idx >= vm->compiler->chunk_count) {
        vm_error(vm, "Invalid chunk index"); return;
    }

    /* Push initial call frame */
    if (vm->frame_count >= VM_FRAMES_MAX) {
        vm_error(vm, "Call stack overflow"); return;
    }
    CallFrame *frame = &vm->frames[vm->frame_count++];
    frame->chunk = &vm->compiler->chunks[chunk_idx];
    frame->ip    = 0;
    frame->env   = vm->globals;

    /* ── The main loop ── */
    while (!vm->error) {
        CallFrame  *f   = &vm->frames[vm->frame_count - 1];
        Chunk      *c   = f->chunk;

        if (f->ip >= c->count) break;

        Instruction *ins = &c->code[f->ip++];

        switch (ins->op) {

            /* ── Pushes ────────────────────────────────────── */
            case OP_PUSH_INT:
            case OP_PUSH_FLOAT:
            case OP_PUSH_STRING:
            case OP_PUSH_BOOL:    push(vm, ins->value); break;
            case OP_PUSH_NULL:    push(vm, val_null()); break;

            /* ── Variable access ────────────────────────────── */
            case OP_LOAD: {
                /* __import__ is handled specially in OP_CALL */
                if (strcmp(ins->name, "__import__") == 0) {
                    push(vm, val_null());
                    break;
                }
                Value v;
                if (!env_get(f->env, ins->name, &v)) {
                    char msg[128];
                    snprintf(msg, 128, "Undefined variable '%s' on line %d",
                             ins->name, ins->line);
                    vm_error(vm, msg);
                } else {
                    push(vm, v);
                }
                break;
            }

            case OP_DEFINE: {
                Value v = pop(vm);
                env_set(f->env, ins->name, v);
                break;
            }

            case OP_STORE: {
                Value v = pop(vm);
                if (!env_assign(f->env, ins->name, v))
                    env_set(f->env, ins->name, v);
                break;
            }

            /* ── Arithmetic & comparison ────────────────────── */
            case OP_ADD: case OP_SUB: case OP_MUL:
            case OP_DIV: case OP_MOD:
            case OP_EQ:  case OP_NEQ:
            case OP_LT:  case OP_GT:
            case OP_LTEQ: case OP_GTEQ: {
                Value R = pop(vm);
                Value L = pop(vm);
                push(vm, do_arith(vm, ins->op, L, R));
                break;
            }

            case OP_NEG: {
                Value v = pop(vm);
                if (v.type == VAL_INT)   push(vm, val_int(-v.as.i));
                else if (v.type == VAL_FLOAT) push(vm, val_float(-v.as.f));
                else vm_error(vm, "Cannot negate non-number");
                break;
            }

            /* ── Logic ──────────────────────────────────────── */
            case OP_AND: {
                Value R = pop(vm); Value L = pop(vm);
                push(vm, val_bool(val_truthy(L) && val_truthy(R)));
                break;
            }
            case OP_OR: {
                Value R = pop(vm); Value L = pop(vm);
                push(vm, val_bool(val_truthy(L) || val_truthy(R)));
                break;
            }
            case OP_NOT: {
                Value v = pop(vm);
                push(vm, val_bool(!val_truthy(v)));
                break;
            }

            /* ── Jumps ──────────────────────────────────────── */
            case OP_JUMP:
                f->ip = ins->operand;
                break;

            case OP_JUMP_IF_FALSE: {
                Value v = pop(vm);
                if (!val_truthy(v)) f->ip = ins->operand;
                break;
            }

            case OP_JUMP_IF_TRUE: {
                Value v = pop(vm);
                if (val_truthy(v)) f->ip = ins->operand;
                break;
            }

            /* ── Print ──────────────────────────────────────── */
            case OP_PRINT: {
                Value v = pop(vm);
                val_print(v);
                printf("\n");
                break;
            }

            /* ── Stack management ───────────────────────────── */
            case OP_POP: pop(vm); break;
            case OP_DUP: push(vm, peek(vm)); break;

            /* ── Function definition ────────────────────────── */
            case OP_MAKE_FN: {
                /* Push the function value — DEFINE (next instr) will store it */
                Value fn;
                fn.type            = VAL_FUNCTION;
                fn.as.fn.def       = NULL;
                fn.as.fn.closure   = f->env;
                fn.as.fn.chunk_idx = ins->operand;
                push(vm, fn);
                break;
            }

            /* ── Function call ──────────────────────────────── */
            case OP_CALL: {
                int arg_count = ins->operand;

                /* Handle import directly */
                if (strcmp(ins->name, "__import__") == 0) {
                    Value mod_arg = pop(vm);
                    if (mod_arg.type == VAL_STRING) {
                        if (!stdlib_import(f->env, mod_arg.as.s)) {
                            char msg[128];
                            snprintf(msg, 127, "Unknown module '%s'", mod_arg.as.s);
                            vm_error(vm, msg);
                        }
                    }
                    push(vm, val_null());
                    break;
                }

                /* Look up the function */
                Value fn;
                if (!env_get(f->env, ins->name, &fn)) {
                    /* Check globals for built-ins */
                    if (!env_get(vm->globals, ins->name, &fn)) {
                        char msg[128];
                        snprintf(msg, 128, "Undefined function '%s'", ins->name);
                        vm_error(vm, msg);
                        break;
                    }
                }

                /* Native built-in */
                if (fn.type == VAL_NATIVE) {
                    Value args[MAX_ARGS];
                    /* Pop args in reverse order */
                    for (int i = arg_count - 1; i >= 0; i--)
                        args[i] = pop(vm);
                    Value result = fn.as.native(args, arg_count);
                    push(vm, result);
                    break;
                }

                /* User-defined function (compiled chunk) */
                if (fn.type == VAL_FUNCTION) {
                    int fn_chunk_idx = fn.as.fn.chunk_idx;
                    Chunk *fn_chunk  = &vm->compiler->chunks[fn_chunk_idx];

                    /* Check arg count matches param count.
                       Params = number of DEFINE instructions at top of chunk. */
                    int param_count = 0;
                    for (int i = 0; i < fn_chunk->count; i++) {
                        if (fn_chunk->code[i].op == OP_DEFINE) param_count++;
                        else break;
                    }

                    if (arg_count != param_count) {
                        char msg[128];
                        snprintf(msg, 128, "'%s' expects %d arg(s), got %d",
                                 ins->name, param_count, arg_count);
                        vm_error(vm, msg);
                        break;
                    }

                    /* Create new environment for the call */
                    Env *call_env = env_new(fn.as.fn.closure);

                    /* Pop args and bind to params by name */
                    Value args[MAX_ARGS];
                    for (int i = arg_count - 1; i >= 0; i--)
                        args[i] = pop(vm);
                    for (int i = 0; i < param_count; i++)
                        env_set(call_env, fn_chunk->code[i].name, args[i]);

                    /* Push new call frame */
                    if (vm->frame_count >= VM_FRAMES_MAX) {
                        vm_error(vm, "Stack overflow (too much recursion)");
                        env_free(call_env);
                        break;
                    }
                    CallFrame *new_frame = &vm->frames[vm->frame_count++];
                    new_frame->chunk = fn_chunk;
                    new_frame->ip    = param_count; /* skip past DEFINE instructions */
                    new_frame->env   = call_env;
                    break;
                }

                char msg[128];
                snprintf(msg, 128, "'%s' is not a function", ins->name);
                vm_error(vm, msg);
                break;
            }

            /* ── Return ─────────────────────────────────────── */
            case OP_RETURN: {
                Value ret = pop(vm);
                Env  *old = vm->frames[vm->frame_count - 1].env;
                if (old != vm->globals) env_free(old);
                vm->frame_count--;
                if (vm->frame_count == 0) { push(vm, ret); goto done; }
                push(vm, ret);
                break;
            }

            case OP_RETURN_NULL: {
                Env *old = vm->frames[vm->frame_count - 1].env;
                if (old != vm->globals) env_free(old);
                vm->frame_count--;
                if (vm->frame_count == 0) goto done;
                push(vm, val_null());
                break;
            }

            case OP_HALT:
                goto done;

            default: {
                char msg[64];
                snprintf(msg, 64, "Unknown opcode %d", ins->op);
                vm_error(vm, msg);
                break;
            }
        }
    }
done:
    return;
}

void vm_init(VM *vm, Compiler *compiler, Env *globals) {
    vm->stack_top   = 0;
    vm->frame_count = 0;
    vm->compiler    = compiler;
    vm->globals     = globals;
    vm->error       = 0;
    vm->errmsg[0]   = '\0';
}

void vm_free(VM *vm) { (void)vm; }
