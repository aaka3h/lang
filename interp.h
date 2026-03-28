#ifndef INTERP_H
#define INTERP_H

/* ─────────────────────────────────────────────────────────────────
   interp.h  —  the Interpreter's public interface
   
   The interpreter has one job: take a Node (AST) + an Environment,
   and return a Value.
   
   eval(node, env) is the core recursive function.
   It pattern-matches on node->type and handles each case.
   ───────────────────────────────────────────────────────────────── */

#include "value.h"
#include "parser.h"

/* ─────────────────────────────────────────────────────────────────
   The Interpreter struct
   ───────────────────────────────────────────────────────────────── */
typedef struct {
    Env  *globals;    /* top-level scope                    */
    int   error;      /* 1 if a runtime error occurred      */
    int   error_line; /* line number where error occurred    */
    char  errmsg[256];
} Interpreter;

/* ── Public API ────────────────────────────────────────────────── */

/* Create an interpreter with built-in functions loaded */
void  interp_init(Interpreter *interp);

/* Evaluate a node in the given environment, return its value */
Value interp_eval(Interpreter *interp, Node *node, Env *env);

/* Run a full program from source string */
void  interp_run(Interpreter *interp, const char *src);

/* Clean up */
void  interp_free(Interpreter *interp);

#endif /* INTERP_H */
