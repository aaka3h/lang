#ifndef BYTECODE_H
#define BYTECODE_H

/* ─────────────────────────────────────────────────────────────────
   bytecode.h  —  Instruction set for our stack-based virtual machine
   
   This is the "machine language" of your language.
   The compiler (Phase 5) turns AST nodes into these instructions.
   The VM (Phase 6) executes them one by one.
   
   Every instruction operates on a VALUE STACK:
     - PUSH instructions add values to the top
     - Math/logic instructions pop operands and push results
     - STORE/LOAD move values between the stack and variables
     - JUMP instructions move the instruction pointer
   
   This is exactly how CPython, the JVM, and WebAssembly work.
   ───────────────────────────────────────────────────────────────── */

#include "value.h"

/* ─────────────────────────────────────────────────────────────────
   The full instruction set (opcode = operation code)
   ───────────────────────────────────────────────────────────────── */
typedef enum {

    /* ── Push literals onto the stack ──────────────────────────── */
    OP_PUSH_INT,        /* push an integer constant                 */
    OP_PUSH_FLOAT,      /* push a float constant                    */
    OP_PUSH_STRING,     /* push a string constant                   */
    OP_PUSH_BOOL,       /* push true or false                       */
    OP_PUSH_NULL,       /* push null                                */

    /* ── Variable access ───────────────────────────────────────── */
    OP_LOAD,            /* push value of a named variable           */
    OP_STORE,           /* pop top of stack, store in variable      */
    OP_DEFINE,          /* like STORE but creates new variable(let) */

    /* ── Arithmetic ────────────────────────────────────────────── */
    OP_ADD,             /* pop b, pop a, push a + b                 */
    OP_SUB,             /* pop b, pop a, push a - b                 */
    OP_MUL,             /* pop b, pop a, push a * b                 */
    OP_DIV,             /* pop b, pop a, push a / b                 */
    OP_MOD,             /* pop b, pop a, push a % b                 */
    OP_NEG,             /* pop a, push -a                           */

    /* ── Comparison ────────────────────────────────────────────── */
    OP_EQ,              /* pop b, pop a, push a == b                */
    OP_NEQ,             /* pop b, pop a, push a != b                */
    OP_LT,              /* pop b, pop a, push a < b                 */
    OP_GT,              /* pop b, pop a, push a > b                 */
    OP_LTEQ,            /* pop b, pop a, push a <= b                */
    OP_GTEQ,            /* pop b, pop a, push a >= b                */

    /* ── Logic ─────────────────────────────────────────────────── */
    OP_AND,             /* pop b, pop a, push a and b               */
    OP_OR,              /* pop b, pop a, push a or b                */
    OP_NOT,             /* pop a, push not a                        */

    /* ── Control flow ──────────────────────────────────────────── */
    OP_JUMP,            /* unconditional jump to instruction N      */
    OP_JUMP_IF_FALSE,   /* pop a, jump to N if a is falsy           */
    OP_JUMP_IF_TRUE,    /* pop a, jump to N if a is truthy          */

    /* ── Functions ─────────────────────────────────────────────── */
    OP_CALL,            /* call function: operand = arg count       */
    OP_RETURN,          /* return top of stack to caller            */
    OP_RETURN_NULL,     /* return null (for void functions)         */
    OP_MAKE_FN,         /* create a function object from a chunk    */

    /* ── I/O ────────────────────────────────────────────────────── */
    OP_PRINT,           /* pop and print top of stack               */

    /* ── Stack management ──────────────────────────────────────── */
    OP_POP,             /* discard the top of the stack             */
    OP_DUP,             /* duplicate the top of the stack           */

    OP_HALT,            /* stop execution                           */

} OpCode;

/* ─────────────────────────────────────────────────────────────────
   Instruction — one bytecode instruction
   
   op       — what to do (the opcode)
   operand  — optional integer argument (e.g. jump target, arg count)
   value    — optional Value argument (for PUSH_* instructions)
   name     — optional string argument (for LOAD/STORE/DEFINE)
   line     — source line (for error messages)
   ───────────────────────────────────────────────────────────────── */
typedef struct {
    OpCode  op;
    int     operand;        /* integer argument                      */
    Value   value;          /* constant value (for PUSH_*)           */
    char    name[128];      /* variable/function name                */
    int     line;           /* source line number                    */
} Instruction;

/* ─────────────────────────────────────────────────────────────────
   Chunk — a compiled sequence of instructions
   
   This is the compiled form of one function (or the top-level program).
   Each function compiles to its own Chunk.
   The top-level program is also a Chunk.
   ───────────────────────────────────────────────────────────────── */
#define MAX_INSTRUCTIONS 1024
#define MAX_CHUNK_NAME   64

typedef struct {
    Instruction  code[MAX_INSTRUCTIONS];
    int          count;
    char         name[MAX_CHUNK_NAME];  /* function name or "<main>" */
} Chunk;

/* ─────────────────────────────────────────────────────────────────
   Chunk operations
   ───────────────────────────────────────────────────────────────── */

static inline void chunk_init(Chunk *c, const char *name) {
    c->count = 0;
    strncpy(c->name, name, MAX_CHUNK_NAME - 1);
    c->name[MAX_CHUNK_NAME - 1] = '\0';
}

/* Append an instruction, return its index */
static inline int chunk_emit(Chunk *c, OpCode op, int line) {
    if (c->count >= MAX_INSTRUCTIONS) {
        fprintf(stderr, "Chunk overflow in '%s'\n", c->name);
        return -1;
    }
    int idx = c->count;
    c->code[idx].op      = op;
    c->code[idx].operand = 0;
    c->code[idx].line    = line;
    c->code[idx].name[0] = '\0';
    c->count++;
    return idx;
}

/* Emit with an integer operand */
static inline int chunk_emit_op(Chunk *c, OpCode op, int operand, int line) {
    int idx = chunk_emit(c, op, line);
    if (idx >= 0) c->code[idx].operand = operand;
    return idx;
}

/* Emit a PUSH with a Value */
static inline int chunk_emit_push(Chunk *c, OpCode op, Value v, int line) {
    int idx = chunk_emit(c, op, line);
    if (idx >= 0) c->code[idx].value = v;
    return idx;
}

/* Emit a named instruction (LOAD/STORE/DEFINE) */
static inline int chunk_emit_name(Chunk *c, OpCode op, const char *name, int line) {
    int idx = chunk_emit(c, op, line);
    if (idx >= 0) strncpy(c->code[idx].name, name, 127);
    return idx;
}

/* Patch a jump instruction's target after we know where it lands */
static inline void chunk_patch_jump(Chunk *c, int idx, int target) {
    c->code[idx].operand = target;
}

/* ─────────────────────────────────────────────────────────────────
   Disassembler — print human-readable bytecode (for debugging)
   ───────────────────────────────────────────────────────────────── */
static inline const char* opcode_name(OpCode op) {
    switch (op) {
        case OP_PUSH_INT:      return "PUSH_INT";
        case OP_PUSH_FLOAT:    return "PUSH_FLOAT";
        case OP_PUSH_STRING:   return "PUSH_STRING";
        case OP_PUSH_BOOL:     return "PUSH_BOOL";
        case OP_PUSH_NULL:     return "PUSH_NULL";
        case OP_LOAD:          return "LOAD";
        case OP_STORE:         return "STORE";
        case OP_DEFINE:        return "DEFINE";
        case OP_ADD:           return "ADD";
        case OP_SUB:           return "SUB";
        case OP_MUL:           return "MUL";
        case OP_DIV:           return "DIV";
        case OP_MOD:           return "MOD";
        case OP_NEG:           return "NEG";
        case OP_EQ:            return "EQ";
        case OP_NEQ:           return "NEQ";
        case OP_LT:            return "LT";
        case OP_GT:            return "GT";
        case OP_LTEQ:          return "LTEQ";
        case OP_GTEQ:          return "GTEQ";
        case OP_AND:           return "AND";
        case OP_OR:            return "OR";
        case OP_NOT:           return "NOT";
        case OP_JUMP:          return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_JUMP_IF_TRUE:  return "JUMP_IF_TRUE";
        case OP_CALL:          return "CALL";
        case OP_RETURN:        return "RETURN";
        case OP_RETURN_NULL:   return "RETURN_NULL";
        case OP_MAKE_FN:       return "MAKE_FN";
        case OP_PRINT:         return "PRINT";
        case OP_POP:           return "POP";
        case OP_DUP:           return "DUP";
        case OP_HALT:          return "HALT";
        default:               return "UNKNOWN";
    }
}

static inline void chunk_disasm(Chunk *c) {
    printf("=== Bytecode: %s (%d instructions) ===\n", c->name, c->count);
    for (int i = 0; i < c->count; i++) {
        Instruction *ins = &c->code[i];
        printf("  %04d  %-16s", i, opcode_name(ins->op));
        switch (ins->op) {
            case OP_PUSH_INT:
                printf("  %lld", ins->value.as.i); break;
            case OP_PUSH_FLOAT:
                printf("  %g",   ins->value.as.f); break;
            case OP_PUSH_STRING:
                printf("  \"%s\"", ins->value.as.s); break;
            case OP_PUSH_BOOL:
                printf("  %s", ins->value.as.b ? "true" : "false"); break;
            case OP_LOAD: case OP_STORE: case OP_DEFINE:
                printf("  %s", ins->name); break;
            case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE:
                printf("  -> %d", ins->operand); break;
            case OP_CALL:
                printf("  %s (%d args)", ins->name, ins->operand); break;
            case OP_MAKE_FN:
                printf("  %s", ins->name); break;
            default: break;
        }
        printf("\n");
    }
    printf("\n");
}

#endif /* BYTECODE_H */
