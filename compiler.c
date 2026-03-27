/* ─────────────────────────────────────────────────────────────────
   compiler.c  —  AST → Bytecode compiler
   
   This is Phase 5. We walk the AST (same tree the interpreter uses)
   and emit bytecode instructions instead of executing directly.
   
   KEY CONCEPT: The Jump Patch
   
   When we compile  "if cond { then } else { else_ }"  we don't yet
   know WHERE the else block starts when we emit the jump. So we:
     1. Emit JUMP_IF_FALSE with a placeholder target (0)
     2. Compile the then-block
     3. NOW we know where else starts → patch the jump target
   
   This "emit then patch" pattern is used in every real compiler.
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

/* ─────────────────────────────────────────────────────────────────
   Internal helpers
   ───────────────────────────────────────────────────────────────── */

static void comp_error(Compiler *c, int line, const char *msg) {
    if (!c->error) {
        c->error = 1;
        snprintf(c->errmsg, 255, "Line %d: %s", line, msg);
    }
}

/* Start compiling into a fresh chunk, return its index */
static int new_chunk(Compiler *c, const char *name) {
    if (c->chunk_count >= MAX_CHUNKS) {
        fprintf(stderr, "Too many functions\n"); return -1;
    }
    int idx = c->chunk_count++;
    chunk_init(&c->chunks[idx], name);
    c->current = &c->chunks[idx];
    return idx;
}

/* Shorthand emitters */
#define EMIT(op)              chunk_emit(c->current, op, node ? node->line : 0)
#define EMIT_OP(op, operand)  chunk_emit_op(c->current, op, operand, node ? node->line : 0)
#define EMIT_PUSH(op, val)    chunk_emit_push(c->current, op, val, node ? node->line : 0)
#define EMIT_NAME(op, nm)     chunk_emit_name(c->current, op, nm, node ? node->line : 0)
#define HERE()                (c->current->count)

/* Forward declaration */
static void compile_node(Compiler *c, Node *node);

/* ─────────────────────────────────────────────────────────────────
   compile_node — the main recursive compiler function
   
   For each node type, we emit the bytecode that produces
   the same result as the interpreter would.
   ───────────────────────────────────────────────────────────────── */
static void compile_node(Compiler *c, Node *node) {
    if (!node || c->error) return;

    switch (node->type) {

        /* ── Literals → just push them ───────────────────────── */
        case NODE_INT:
            EMIT_PUSH(OP_PUSH_INT, val_int(node->as.int_lit.value));
            break;

        case NODE_FLOAT:
            EMIT_PUSH(OP_PUSH_FLOAT, val_float(node->as.float_lit.value));
            break;

        case NODE_STRING:
            EMIT_PUSH(OP_PUSH_STRING, val_string(node->as.str_lit.value));
            break;

        case NODE_BOOL:
            EMIT_PUSH(OP_PUSH_BOOL, val_bool(node->as.bool_lit.value));
            break;

        case NODE_NULL:
            EMIT(OP_PUSH_NULL);
            break;

        /* ── Variable read → LOAD name ───────────────────────── */
        case NODE_IDENT:
            EMIT_NAME(OP_LOAD, node->as.ident.name);
            break;

        /* ── let x = expr → compile expr, DEFINE x ──────────── */
        case NODE_LET:
            compile_node(c, node->as.let_stmt.value);
            EMIT_NAME(OP_DEFINE, node->as.let_stmt.name);
            break;

        /* ── x = expr → compile expr, STORE x ───────────────── */
        case NODE_ASSIGN:
            compile_node(c, node->as.assign.value);
            EMIT_NAME(OP_STORE, node->as.assign.name);
            break;

        /* ── Binary operations ──────────────────────────────── */
        case NODE_BINOP: {
            const char *op = node->as.binop.op;

            /* Short-circuit AND: if left is false, skip right */
            if (strcmp(op, "and") == 0) {
                compile_node(c, node->as.binop.left);
                int jump = EMIT_OP(OP_JUMP_IF_FALSE, 0);  /* patch later */
                EMIT(OP_POP);
                compile_node(c, node->as.binop.right);
                chunk_patch_jump(c->current, jump, HERE());
                break;
            }

            /* Short-circuit OR: if left is true, skip right */
            if (strcmp(op, "or") == 0) {
                compile_node(c, node->as.binop.left);
                int jump = EMIT_OP(OP_JUMP_IF_TRUE, 0);
                EMIT(OP_POP);
                compile_node(c, node->as.binop.right);
                chunk_patch_jump(c->current, jump, HERE());
                break;
            }

            /* Normal binary: compile both sides, then the operator */
            compile_node(c, node->as.binop.left);
            compile_node(c, node->as.binop.right);

            if      (strcmp(op, "+")  == 0) EMIT(OP_ADD);
            else if (strcmp(op, "-")  == 0) EMIT(OP_SUB);
            else if (strcmp(op, "*")  == 0) EMIT(OP_MUL);
            else if (strcmp(op, "/")  == 0) EMIT(OP_DIV);
            else if (strcmp(op, "%")  == 0) EMIT(OP_MOD);
            else if (strcmp(op, "==") == 0) EMIT(OP_EQ);
            else if (strcmp(op, "!=") == 0) EMIT(OP_NEQ);
            else if (strcmp(op, "<")  == 0) EMIT(OP_LT);
            else if (strcmp(op, ">")  == 0) EMIT(OP_GT);
            else if (strcmp(op, "<=") == 0) EMIT(OP_LTEQ);
            else if (strcmp(op, ">=") == 0) EMIT(OP_GTEQ);
            else {
                comp_error(c, node->line, "Unknown binary operator");
            }
            break;
        }

        /* ── Unary operations ───────────────────────────────── */
        case NODE_UNARY:
            compile_node(c, node->as.unary.operand);
            if      (strcmp(node->as.unary.op, "-")   == 0) EMIT(OP_NEG);
            else if (strcmp(node->as.unary.op, "not") == 0) EMIT(OP_NOT);
            break;

        /* ── import stmt ────────────────────────────────────── */
        case NODE_IMPORT:
            /* imports are resolved at runtime by the VM via OP_IMPORT */
            chunk_emit_name(c->current, OP_LOAD, "__import__", node->line);
            chunk_emit_push(c->current, OP_PUSH_STRING,
                            val_string(node->as.import_stmt.module), node->line);
            chunk_emit_op(c->current, OP_CALL, 1, node->line);
            /* OP_CALL needs name field — patch it */
            c->current->code[c->current->count-1].op = OP_CALL;
            strncpy(c->current->code[c->current->count-1].name, "__import__", 127);
            break;

        /* ── print stmt ─────────────────────────────────────── */
        case NODE_PRINT:
            compile_node(c, node->as.print_stmt.value);
            EMIT(OP_PRINT);
            break;

        /* ── return stmt ────────────────────────────────────── */
        case NODE_RETURN:
            if (node->as.return_stmt.value) {
                compile_node(c, node->as.return_stmt.value);
                EMIT(OP_RETURN);
            } else {
                EMIT(OP_RETURN_NULL);
            }
            break;

        /* ── Block: compile each statement ─────────────────── */
        case NODE_BLOCK: {
            int last = node->as.block.count - 1;
            for (int i = 0; i < node->as.block.count && !c->error; i++) {
                compile_node(c, node->as.block.stmts[i]);
                Node *s = node->as.block.stmts[i];
                int is_expr = (s && s->type != NODE_LET    &&
                                    s->type != NODE_ASSIGN &&
                                    s->type != NODE_PRINT  &&
                                    s->type != NODE_RETURN &&
                                    s->type != NODE_IF     &&
                                    s->type != NODE_WHILE  &&
                                    s->type != NODE_FOR    &&
                                    s->type != NODE_FN_DEF);
                /* Pop all intermediate expr results; keep the last one on stack */
                if (is_expr && i < last)
                    EMIT(OP_POP);
            }
            break;
        }

        /* ── if / else ──────────────────────────────────────── */
        case NODE_IF: {
            /*
               Pattern:
                 compile cond
                 JUMP_IF_FALSE → [else or end]
                 compile then
                 JUMP → [end]          ← only if there's an else
               [else]:
                 compile else
               [end]:
            */
            compile_node(c, node->as.if_stmt.cond);

            int jump_to_else = EMIT_OP(OP_JUMP_IF_FALSE, 0);

            compile_node(c, node->as.if_stmt.then_block);

            if (node->as.if_stmt.else_block) {
                int jump_to_end = EMIT(OP_JUMP);
                chunk_patch_jump(c->current, jump_to_else, HERE());
                compile_node(c, node->as.if_stmt.else_block);
                chunk_patch_jump(c->current, jump_to_end, HERE());
            } else {
                chunk_patch_jump(c->current, jump_to_else, HERE());
            }
            break;
        }

        /* ── while loop ─────────────────────────────────────── */
        case NODE_WHILE: {
            /*
               Pattern:
               [loop_start]:
                 compile cond
                 JUMP_IF_FALSE → [end]
                 compile body
                 JUMP → [loop_start]
               [end]:
            */
            int loop_start = HERE();
            compile_node(c, node->as.while_stmt.cond);
            int jump_out = EMIT_OP(OP_JUMP_IF_FALSE, 0);
            compile_node(c, node->as.while_stmt.body);
            EMIT_OP(OP_JUMP, loop_start);
            chunk_patch_jump(c->current, jump_out, HERE());
            break;
        }

        /* ── for loop ───────────────────────────────────────── */
        case NODE_FOR: {
            /*
               for init; cond; step { body }
               →
                 compile init
               [loop_start]:
                 compile cond
                 JUMP_IF_FALSE → [end]
                 compile body
                 compile step
                 JUMP → [loop_start]
               [end]:
            */
            compile_node(c, node->as.for_stmt.init);
            int loop_start = HERE();
            compile_node(c, node->as.for_stmt.cond);
            int jump_out = EMIT_OP(OP_JUMP_IF_FALSE, 0);
            compile_node(c, node->as.for_stmt.body);
            /* compile step (it's a statement, pop its result) */
            compile_node(c, node->as.for_stmt.step);
            if (node->as.for_stmt.step->type == NODE_ASSIGN)
                ;  /* STORE already consumed the value */
            EMIT_OP(OP_JUMP, loop_start);
            chunk_patch_jump(c->current, jump_out, HERE());
            break;
        }

        /* ── Function definition ────────────────────────────── */
        case NODE_FN_DEF: {
            /*
               We compile the function body into a NEW chunk,
               then emit OP_MAKE_FN in the current chunk to create
               a function value pointing at that chunk.
            */
            Chunk *saved = c->current;      /* save where we were */
            int fn_idx = new_chunk(c, node->as.fn_def.name);
            if (fn_idx < 0) break;

            /* Emit parameter LOADs at the start of the function.
               The VM will push args in order before calling — we
               DEFINE them as local variables here. */
            for (int i = 0; i < node->as.fn_def.param_count; i++) {
                /* Parameters are already on the stack when called.
                   We just need to DEFINE them in the function's scope. */
                Node dummy; dummy.line = node->line;
                chunk_emit_name(c->current, OP_DEFINE,
                                node->as.fn_def.params[i], node->line);
            }

            /* Compile the function body */
            compile_node(c, node->as.fn_def.body);

            /* Always end with RETURN_NULL in case no explicit return */
            chunk_emit(c->current, OP_RETURN_NULL, node->line);

            /* Restore the outer chunk */
            c->current = saved;

            /* Emit MAKE_FN in the outer chunk, referencing fn_idx */
            int idx = chunk_emit_op(c->current, OP_MAKE_FN, fn_idx, node->line);
            strncpy(c->current->code[idx].name, node->as.fn_def.name, 127);
            /* Store the function in the environment */
            chunk_emit_name(c->current, OP_DEFINE, node->as.fn_def.name, node->line);
            break;
        }

        /* ── Function call ──────────────────────────────────── */
        case NODE_CALL: {
            /*
               Push all arguments onto the stack, then CALL.
               The VM will pop them off and bind to params.
            */
            for (int i = 0; i < node->as.call.arg_count; i++)
                compile_node(c, node->as.call.args[i]);

            int idx = chunk_emit_op(c->current, OP_CALL,
                                    node->as.call.arg_count, node->line);
            strncpy(c->current->code[idx].name, node->as.call.name, 127);
            break;
        }

        default:
            comp_error(c, node ? node->line : 0, "Unknown node type in compiler");
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────
   Public API
   ───────────────────────────────────────────────────────────────── */

void compiler_init(Compiler *c) {
    c->chunk_count = 0;
    c->error       = 0;
    c->errmsg[0]   = '\0';
    c->current     = NULL;
}

int compiler_compile(Compiler *c, Node *ast) {
    int main_idx = new_chunk(c, "<main>");
    if (main_idx < 0) return -1;

    compile_node(c, ast);
    chunk_emit(c->current, OP_HALT, 0);

    if (c->error) return -1;
    return main_idx;
}

void compiler_free(Compiler *c) {
    /* Chunks are stack-allocated in the struct, nothing to free */
    (void)c;
}
