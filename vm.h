/* ─────────────────────────────────────────────────────────────────
   vm.c / vm.h  —  Stack-based Virtual Machine
   
   The VM executes bytecode chunks.
   It has:
     - a VALUE STACK for intermediate results
     - a CALL STACK (frames) for function calls
     - an ENVIRONMENT for variable storage
   
   The main loop: fetch instruction → dispatch → repeat.
   This is the "fetch-decode-execute" cycle — same as real CPUs.
   ───────────────────────────────────────────────────────────────── */

#ifndef VM_H
#define VM_H

#include "compiler.h"

#define VM_STACK_MAX  1024   /* max values on the value stack    */
#define VM_FRAMES_MAX   64   /* max call depth (recursion limit) */

/* ─────────────────────────────────────────────────────────────────
   Call frame — one entry in the call stack
   
   When a function is called, we push a new frame.
   When it returns, we pop the frame and resume the caller.
   ───────────────────────────────────────────────────────────────── */
typedef struct {
    Chunk  *chunk;      /* the chunk (function) being executed */
    int     ip;         /* instruction pointer (index into chunk->code) */
    Env    *env;        /* this frame's variable environment   */
} CallFrame;

/* ─────────────────────────────────────────────────────────────────
   The VM struct
   ───────────────────────────────────────────────────────────────── */
typedef struct {
    /* Value stack */
    Value       stack[VM_STACK_MAX];
    int         stack_top;          /* index of next free slot */

    /* Call stack */
    CallFrame   frames[VM_FRAMES_MAX];
    int         frame_count;

    /* All compiled chunks (functions) */
    Compiler   *compiler;

    /* Global environment */
    Env        *globals;

    /* Error state */
    int         error;
    char        errmsg[256];
} VM;

/* ── Public API ────────────────────────────────────────────────── */
void vm_init(VM *vm, Compiler *compiler, Env *globals);
void vm_run(VM *vm, int chunk_idx);    /* run a chunk by index */
void vm_free(VM *vm);

#endif /* VM_H */
