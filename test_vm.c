/* test_vm.c — Phase 5: Bytecode Compiler + VM tests
   Compile:
     gcc -Wall -std=c99 lexer.c parser.c interp.c compiler.c vm_impl.c test_vm.c -o test_vm -lm
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vm.h"
#include "interp.h"

static int passed = 0, failed = 0;
#define CHECK(cond, msg) \
    do { if (cond) { passed++; printf("  [PASS] %s\n", msg); } \
         else      { failed++; printf("  [FAIL] %s  (got type=%d)\n", msg, (int)v.type); } } while(0)

/* Heap-allocate VM too to be safe */
static Value vm_eval(const char *src, Env *globals) {
    Compiler *comp = compiler_new();
    Node *tree = parse_source(src);
    if (!tree) { compiler_delete(comp); return val_null(); }

    int main_idx = compiler_compile(comp, tree);
    if (main_idx < 0) {
        printf("  [compile error] %s\n", comp->errmsg);
        compiler_delete(comp); return val_null();
    }

    VM *vm = (VM*)calloc(1, sizeof(VM));
    vm_init(vm, comp, globals);
    vm_run(vm, main_idx);

    Value result = val_null();
    if (vm->error) {
        printf("  [vm error] %s\n", vm->errmsg);
    } else if (vm->stack_top > 0) {
        result = vm->stack[vm->stack_top - 1];
    }

    free(vm);
    compiler_delete(comp);
    return result;
}

int main(void) {
    Interpreter interp;
    interp_init(&interp);
    Env *globals = interp.globals;

    printf("═══════════════════════════════════\n");
    printf("  Phase 5 — Bytecode Compiler + VM\n");
    printf("═══════════════════════════════════\n");

    Value v;

    v = vm_eval("42", globals);
    CHECK(v.type == VAL_INT && v.as.i == 42, "int literal");

    v = vm_eval("3.14", globals);
    CHECK(v.type == VAL_FLOAT, "float literal");

    v = vm_eval("\"hello\"", globals);
    CHECK(v.type == VAL_STRING && strcmp(v.as.s,"hello")==0, "string literal");

    v = vm_eval("true", globals);
    CHECK(v.type == VAL_BOOL && v.as.b == 1, "bool true");

    v = vm_eval("2 + 3 * 4", globals);
    CHECK(v.type == VAL_INT && v.as.i == 14, "2+3*4 = 14 (precedence)");

    v = vm_eval("(2 + 3) * 4", globals);
    CHECK(v.type == VAL_INT && v.as.i == 20, "(2+3)*4 = 20 (grouping)");

    v = vm_eval("10 / 4", globals);
    CHECK(v.type == VAL_FLOAT && v.as.f == 2.5, "10/4 = 2.5");

    v = vm_eval("10 % 3", globals);
    CHECK(v.type == VAL_INT && v.as.i == 1, "10%3 = 1");

    v = vm_eval("-7", globals);
    CHECK(v.type == VAL_INT && v.as.i == -7, "unary minus");

    v = vm_eval("\"hello\" + \" world\"", globals);
    CHECK(v.type == VAL_STRING && strcmp(v.as.s,"hello world")==0, "string concat");

    v = vm_eval("\"x = \" + 42", globals);
    CHECK(v.type == VAL_STRING && strcmp(v.as.s,"x = 42")==0, "string+int concat");

    v = vm_eval("10 > 5", globals);
    CHECK(v.type == VAL_BOOL && v.as.b == 1, "10 > 5 = true");

    v = vm_eval("3 == 3", globals);
    CHECK(v.type == VAL_BOOL && v.as.b == 1, "3 == 3 = true");

    v = vm_eval("not true", globals);
    CHECK(v.type == VAL_BOOL && v.as.b == 0, "not true = false");

    v = vm_eval("let x = 99\nx", globals);
    CHECK(v.type == VAL_INT && v.as.i == 99, "let + load");

    v = vm_eval("let r = 0\nif 10 > 5 { r = 1 } else { r = 2 }\nr", globals);
    CHECK(v.type == VAL_INT && v.as.i == 1, "if taken");

    v = vm_eval("let r = 0\nif 1 > 10 { r = 99 } else { r = 7 }\nr", globals);
    CHECK(v.type == VAL_INT && v.as.i == 7, "else taken");

    v = vm_eval("let s = 0\nlet i = 1\nwhile i <= 10 { s = s + i\ni = i + 1 }\ns", globals);
    CHECK(v.type == VAL_INT && v.as.i == 55, "while: sum 1..10 = 55");

    v = vm_eval("fn double(x) { return x * 2 }\ndouble(7)", globals);
    CHECK(v.type == VAL_INT && v.as.i == 14, "fn: double(7)=14");

    v = vm_eval("fn add(a, b) { return a + b }\nadd(3, 4)", globals);
    CHECK(v.type == VAL_INT && v.as.i == 7, "fn: add(3,4)=7");

    v = vm_eval(
        "fn factorial(n) {\n"
        "    if n <= 1 { return 1 }\n"
        "    return n * factorial(n - 1)\n"
        "}\n"
        "factorial(10)\n", globals);
    CHECK(v.type == VAL_INT && v.as.i == 3628800, "factorial(10)=3628800");

    v = vm_eval("sqrt(144)", globals);
    CHECK(v.type == VAL_FLOAT && v.as.f == 12.0, "builtin sqrt(144)");

    v = vm_eval("abs(-42)", globals);
    CHECK(v.type == VAL_INT && v.as.i == 42, "builtin abs(-42)");

    v = vm_eval("str(99)", globals);
    CHECK(v.type == VAL_STRING && strcmp(v.as.s,"99")==0, "builtin str(99)");

    /* Disassembler demo */
    printf("\n── Disassembly: 2 + 3 * 4 ────────────────────────\n");
    {
        Compiler *dc = compiler_new();
        Node *tree = parse_source("2 + 3 * 4");
        if (tree) { compiler_compile(dc, tree); chunk_disasm(&dc->chunks[0]); }
        compiler_delete(dc);
    }

    printf("── Disassembly: factorial ─────────────────────────\n");
    {
        Compiler *dc = compiler_new();
        Node *tree = parse_source(
            "fn factorial(n) {\n"
            "    if n <= 1 { return 1 }\n"
            "    return n * factorial(n - 1)\n"
            "}\nfactorial(5)\n");
        if (tree) {
            compiler_compile(dc, tree);
            for (int i = 0; i < dc->chunk_count; i++)
                chunk_disasm(&dc->chunks[i]);
        }
        compiler_delete(dc);
    }

    printf("\n  %d/%d tests passed", passed, passed + failed);
    if (failed) printf("  (%d FAILED)", failed);
    printf("\n═══════════════════════════════════\n\n");
    return failed > 0 ? 1 : 0;
}
