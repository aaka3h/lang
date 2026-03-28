/* ─────────────────────────────────────────────────────────────────
   main.c  —  Phase 6: VM-powered entry point
   
   Compile:
     gcc -Wall -std=c99 lexer.c parser.c interp.c compiler.c vm_impl.c main2.c -o lang -lm
   
   Usage:
     ./lang                  → REPL (tree interpreter)
     ./lang file.lang        → run file (tree interpreter)
     ./lang --test           → test suite
     ./lang --vm file.lang   → run with bytecode VM
     ./lang --disasm file.lang → show bytecode then run
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "interp.h"
#include "error.h"

/* ─────────────────────────────────────────────────────────────────
   SHARED STATE  (globals + built-ins, used by both engines)
   ───────────────────────────────────────────────────────────────── */
static Interpreter g_interp;   /* owns the global Env + built-ins */

static void lang_init(void)  { interp_init(&g_interp); }
static void lang_free(void)  { interp_free(&g_interp); }

/* ─────────────────────────────────────────────────────────────────
   FILE UTILITIES
   ───────────────────────────────────────────────────────────────── */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f); rewind(f);
    char *buf = (char*)malloc(sz + 1);
    fread(buf, 1, sz, f); buf[sz] = '\0';
    fclose(f); return buf;
}

/* Lex a source string, report errors, return token count or -1 */
static int lex_source(const char *src, Token *tokens, int max) {
    Lexer l; lexer_init(&l, src);
    int n = lexer_tokenize_all(&l, tokens, max);
    for (int i = 0; i < n; i++) {
        if (tokens[i].type == TOK_ERROR) {
            err_report(src, ERR_LEXER, tokens[i].line, tokens[i].col, tokens[i].value);
            return -1;
        }
    }
    return n;
}

/* Parse tokens, report errors, return AST or NULL */
static Node *parse_tokens(const char *src, Token *tokens, int n) {
    Parser p; parser_init(&p, tokens, n);
    Node *tree = parser_parse(&p);
    if (p.error) {
        Token bad = (p.pos < n) ? tokens[p.pos] : tokens[n-1];
        err_report(src, ERR_SYNTAX, bad.line, bad.col, p.errmsg);
        return NULL;
    }
    return tree;
}

/* ─────────────────────────────────────────────────────────────────
   VM ENGINE  — compile + run with the bytecode VM
   ───────────────────────────────────────────────────────────────── */
static int run_vm(const char *src, int show_disasm) {
    Token tokens[MAX_TOKENS];
    int n = lex_source(src, tokens, MAX_TOKENS);
    if (n < 0) return 1;

    Node *tree = parse_tokens(src, tokens, n);
    if (!tree) return 1;

    Compiler *comp = compiler_new();
    int main_idx = compiler_compile(comp, tree);
    if (comp->error) {
        fprintf(stderr, "\n  [CompileError] %s\n\n", comp->errmsg);
        compiler_delete(comp); return 1;
    }

    if (show_disasm) {
        printf("\n── Bytecode ─────────────────────────────────\n");
        for (int i = 0; i < comp->chunk_count; i++)
            chunk_disasm(&comp->chunks[i]);
        printf("─────────────────────────────────────────────\n\n");
    }

    VM *vm = (VM*)calloc(1, sizeof(VM));
    vm_init(vm, comp, g_interp.globals);
    vm_run(vm, main_idx);

    int ok = 1;
    if (vm->error) {
        int line = 0;
        sscanf(vm->errmsg, "%*[^0-9]%d", &line);
        err_report(src, ERR_RUNTIME, line > 0 ? line : 1, 1, vm->errmsg);
        ok = 0;
    }

    free(vm);
    compiler_delete(comp);
    return ok ? 0 : 1;
}

/* ─────────────────────────────────────────────────────────────────
   TREE ENGINE  — original tree-walking interpreter
   ───────────────────────────────────────────────────────────────── */
static int run_tree(const char *src) {
    Token tokens[MAX_TOKENS];
    int n = lex_source(src, tokens, MAX_TOKENS);
    if (n < 0) return 1;

    Node *tree = parse_tokens(src, tokens, n);
    if (!tree) return 1;

    g_interp.error = 0; g_interp.errmsg[0] = '\0';
    interp_eval(&g_interp, tree, g_interp.globals);

    if (g_interp.error) {
        int line = g_interp.error_line > 0 ? g_interp.error_line : 1;
        err_report(src, ERR_RUNTIME, line, 1, g_interp.errmsg);
        return 1;
    }
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
   REPL
   ───────────────────────────────────────────────────────────────── */
static void print_banner(void) {
    printf("\n");
    printf("  ██╗      █████╗ ███╗   ██╗ ██████╗\n");
    printf("  ██║     ██╔══██╗████╗  ██║██╔════╝\n");
    printf("  ██║     ███████║██╔██╗ ██║██║  ███╗\n");
    printf("  ██║     ██╔══██║██║╚██╗██║██║   ██║\n");
    printf("  ███████╗██║  ██║██║ ╚████║╚██████╔╝\n");
    printf("  ╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝ ╚═════╝\n");
    printf("  your language  v0.1  [bytecode VM]\n\n");
    printf("  :help   :ast <code>   :dis <code>   exit\n\n");
}

static void run_repl(void) {
    print_banner();
    char line[2048];

    /* Persistent VM for the REPL session */
    Compiler *comp = compiler_new();
    VM       *vm   = (VM*)calloc(1, sizeof(VM));
    vm_init(vm, comp, g_interp.globals);

    while (1) {
        printf(">> "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) { printf("\nBye!\n"); break; }
        line[strcspn(line, "\n")] = '\0';
        if (!strlen(line)) continue;
        if (!strcmp(line,"exit") || !strcmp(line,"quit")) { printf("Bye!\n"); break; }

        /* Meta commands */
        if (!strcmp(line, ":help")) {
            printf("\n  Built-ins: sqrt abs pow floor ceil len int float str type\n");
            printf("  :ast <code>   show AST\n");
            printf("  :dis <code>   show bytecode\n");
            printf("  :tokens <code> show tokens\n\n");
            continue;
        }
        if (!strncmp(line, ":ast ", 5)) {
            Node *t = parse_source(line+5);
            if (t) ast_print(t, 2);
            continue;
        }
        if (!strncmp(line, ":dis ", 5)) {
            Compiler *dc = compiler_new();
            Node *t = parse_source(line+5);
            if (t) { compiler_compile(dc, t); chunk_disasm(&dc->chunks[0]); }
            compiler_delete(dc);
            continue;
        }
        if (!strncmp(line, ":tokens ", 8)) {
            Lexer l; Token toks[256];
            lexer_init(&l, line+8);
            int n = lexer_tokenize_all(&l, toks, 256);
            for (int i = 0; i < n; i++)
                if (toks[i].type != TOK_NEWLINE) token_print(toks[i]);
            continue;
        }

        /* Compile + run */
        compiler_init(comp);
        Token tokens[MAX_TOKENS];
        int n = lex_source(line, tokens, MAX_TOKENS);
        if (n < 0) continue;

        Node *tree = parse_tokens(line, tokens, n);
        if (!tree) continue;

        int main_idx = compiler_compile(comp, tree);
        if (comp->error) {
            fprintf(stderr, "\n  [CompileError] %s\n\n", comp->errmsg);
            continue;
        }

        vm->stack_top   = 0;
        vm->frame_count = 0;
        vm->error       = 0;
        vm->errmsg[0]   = '\0';
        vm_run(vm, main_idx);

        if (vm->error) {
            err_report_simple(ERR_RUNTIME, 1, 1, vm->errmsg);
        } else if (vm->stack_top > 0) {
            Value r = vm->stack[vm->stack_top - 1];
            if (r.type != VAL_NULL) {
                printf("  = "); val_print(r); printf("\n");
            }
        }
    }

    free(vm);
    compiler_delete(comp);
}

/* ─────────────────────────────────────────────────────────────────
   TEST SUITE
   ───────────────────────────────────────────────────────────────── */
static int tp = 0, tf = 0;
#define TCHECK(cond, msg) \
    do { if(cond){tp++;printf("  [PASS] %s\n",msg);} \
         else   {tf++;printf("  [FAIL] %s\n",msg);} } while(0)

static void run_tests(void) {
    printf("═══════════════════════════════════\n");
    printf("  Phase 6 — Integration Tests\n");
    printf("═══════════════════════════════════\n");

    /* Run same program through both engines, compare output */
    struct { const char *src; long long expect; } cases[] = {
        { "2 + 3 * 4",                              14 },
        { "fn f(n){if n<=1{return 1} return n*f(n-1)}\nf(10)", 3628800 },
        { "let s=0\nlet i=1\nwhile i<=100{s=s+i\ni=i+1}\ns", 5050 },
        { "fn fib(n){if n<=1{return n} return fib(n-1)+fib(n-2)}\nfib(15)", 610 },
        { NULL, 0 }
    };

    for (int i = 0; cases[i].src; i++) {
        /* VM engine */
        Compiler *comp = compiler_new();
        Node *tree = parse_source(cases[i].src);
        int idx = compiler_compile(comp, tree);
        VM *vm = (VM*)calloc(1, sizeof(VM));
        vm_init(vm, comp, g_interp.globals);
        vm_run(vm, idx);
        long long vm_result = (!vm->error && vm->stack_top > 0)
                              ? vm->stack[vm->stack_top-1].as.i : -1;
        free(vm); compiler_delete(comp);

        /* Tree engine */
        g_interp.error = 0;
        Node *tree2 = parse_source(cases[i].src);
        Value tv = interp_eval(&g_interp, tree2, g_interp.globals);
        long long tree_result = (!g_interp.error) ? tv.as.i : -1;
        g_interp.error = 0;

        char label[128];
        snprintf(label, 128, "VM=%lld  tree=%lld  expect=%lld",
                 vm_result, tree_result, cases[i].expect);
        TCHECK(vm_result == cases[i].expect && tree_result == cases[i].expect, label);
    }

    /* Error handling */
    {
        Compiler *comp = compiler_new();
        Node *tree = parse_source("undefinedXYZ");
        compiler_compile(comp, tree);
        VM *vm = (VM*)calloc(1, sizeof(VM));
        vm_init(vm, comp, g_interp.globals);
        vm_run(vm, 0);
        TCHECK(vm->error && strstr(vm->errmsg, "undefinedXYZ"),
               "VM: undefined variable error");
        free(vm); compiler_delete(comp);
        g_interp.error = 0;
    }

    {
        Compiler *comp = compiler_new();
        Node *tree = parse_source("1 / 0");
        compiler_compile(comp, tree);
        VM *vm = (VM*)calloc(1, sizeof(VM));
        vm_init(vm, comp, g_interp.globals);
        vm_run(vm, 0);
        TCHECK(vm->error && strstr(vm->errmsg, "zero"),
               "VM: division by zero error");
        free(vm); compiler_delete(comp);
        g_interp.error = 0;
    }

    printf("\n  %d/%d tests passed", tp, tp+tf);
    if (tf) printf("  (%d FAILED)", tf);
    printf("\n═══════════════════════════════════\n\n");

    /* Demo programs */
    printf("── FizzBuzz (VM) ───────────────────\n");
    run_vm(
        "let n = 1\n"
        "while n <= 20 {\n"
        "    if n % 15 == 0 { print \"FizzBuzz\" }\n"
        "    else if n % 3 == 0 { print \"Fizz\" }\n"
        "    else if n % 5 == 0 { print \"Buzz\" }\n"
        "    else { print n }\n"
        "    n = n + 1\n"
        "}\n", 0);

    printf("\n── Primes up to 50 (VM) ────────────\n");
    run_vm(
        "fn is_prime(n) {\n"
        "    if n < 2 { return false }\n"
        "    let i = 2\n"
        "    while i * i <= n {\n"
        "        if n % i == 0 { return false }\n"
        "        i = i + 1\n"
        "    }\n"
        "    return true\n"
        "}\n"
        "let n = 2\n"
        "while n <= 50 {\n"
        "    if is_prime(n) { print n }\n"
        "    n = n + 1\n"
        "}\n", 0);
}

/* ─────────────────────────────────────────────────────────────────
   ENTRY POINT
   ───────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    lang_init();
    int exit_code = 0;

    if (argc >= 2 && !strcmp(argv[1], "--test")) {
        run_tests();

    } else if (argc >= 3 && !strcmp(argv[1], "--tree")) {
        /* --tree flag: explicit tree interpreter (same as default now) */
        char *src = read_file(argv[2]);
        if (!src) { fprintf(stderr, "Cannot open: %s\n", argv[2]); exit_code=1; }
        else { exit_code = run_tree(src); free(src); }

    } else if (argc >= 3 && !strcmp(argv[1], "--vm")) {
        /* --vm flag: explicit bytecode VM */
        char *src = read_file(argv[2]);
        if (!src) { fprintf(stderr, "Cannot open: %s\n", argv[2]); exit_code=1; }
        else { exit_code = run_vm(src, 0); free(src); }

    } else if (argc >= 3 && !strcmp(argv[1], "--disasm")) {
        char *src = read_file(argv[2]);
        if (!src) { fprintf(stderr, "Cannot open: %s\n", argv[2]); exit_code=1; }
        else { exit_code = run_vm(src, 1); free(src); }

    } else if (argc >= 2) {
        /* Default: tree interpreter — supports all features */
        char *src = read_file(argv[1]);
        if (!src) { fprintf(stderr, "Cannot open: %s\n", argv[1]); exit_code=1; }
        else { exit_code = run_tree(src); free(src); }

    } else {
        run_repl();
    }

    lang_free();
    return exit_code;
}
