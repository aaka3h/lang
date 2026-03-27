#ifndef COMPILER_H
#define COMPILER_H

#include "bytecode.h"
#include "parser.h"

/* Max number of compiled functions (chunks) in a program */
#define MAX_CHUNKS 32

typedef struct {
    Chunk  *current;
    Chunk   chunks[MAX_CHUNKS];
    int     chunk_count;
    int     error;
    char    errmsg[256];
} Compiler;

void  compiler_init(Compiler *c);
int   compiler_compile(Compiler *c, Node *ast);
void  compiler_free(Compiler *c);

/* Heap-allocate a compiler (avoids giant stack frame) */
static inline Compiler* compiler_new(void) {
    Compiler *c = (Compiler*)calloc(1, sizeof(Compiler));
    if (!c) { fprintf(stderr, "Out of memory\n"); exit(1); }
    compiler_init(c);
    return c;
}
static inline void compiler_delete(Compiler *c) { free(c); }

#endif /* COMPILER_H */
