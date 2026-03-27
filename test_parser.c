/* ─────────────────────────────────────────────────────────────────
   test_parser.c — tests for the parser
   
   Compile:  gcc -Wall -std=c99 lexer.c parser.c test_parser.c -o test_parser
   Run:      ./test_parser
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static int passed = 0, failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { passed++; printf("  [PASS] %s\n", msg); } \
         else      { failed++; printf("  [FAIL] %s\n", msg); } } while(0)

/* Parse src and return the first statement in the root block */
static Node *first_stmt(const char *src) {
    Node *root = parse_source(src);
    if (!root || root->as.block.count == 0) return NULL;
    return root->as.block.stmts[0];
}

/* ── Tests ─────────────────────────────────────────────────────── */

void test_int_literal() {
    Node *n = first_stmt("42");
    CHECK(n && n->type == NODE_INT && n->as.int_lit.value == 42,
          "integer literal 42");
}

void test_float_literal() {
    Node *n = first_stmt("3.14");
    CHECK(n && n->type == NODE_FLOAT, "float literal 3.14");
}

void test_string_literal() {
    Node *n = first_stmt("\"hello\"");
    CHECK(n && n->type == NODE_STRING &&
          strcmp(n->as.str_lit.value, "hello") == 0,
          "string literal");
}

void test_bool_true() {
    Node *n = first_stmt("true");
    CHECK(n && n->type == NODE_BOOL && n->as.bool_lit.value == 1, "bool true");
}

void test_bool_false() {
    Node *n = first_stmt("false");
    CHECK(n && n->type == NODE_BOOL && n->as.bool_lit.value == 0, "bool false");
}

void test_null_literal() {
    Node *n = first_stmt("null");
    CHECK(n && n->type == NODE_NULL, "null literal");
}

void test_ident() {
    Node *n = first_stmt("myVar");
    CHECK(n && n->type == NODE_IDENT &&
          strcmp(n->as.ident.name, "myVar") == 0,
          "identifier");
}

void test_let() {
    Node *n = first_stmt("let x = 99");
    CHECK(n && n->type == NODE_LET &&
          strcmp(n->as.let_stmt.name, "x") == 0 &&
          n->as.let_stmt.value->as.int_lit.value == 99,
          "let declaration");
}

void test_assign() {
    Node *n = first_stmt("x = 5");
    CHECK(n && n->type == NODE_ASSIGN &&
          strcmp(n->as.assign.name, "x") == 0,
          "reassignment");
}

void test_binop_add() {
    Node *n = first_stmt("1 + 2");
    CHECK(n && n->type == NODE_BINOP &&
          strcmp(n->as.binop.op, "+") == 0,
          "addition binop");
}

void test_binop_precedence() {
    /* 2 + 3 * 4 → the * should be DEEPER (higher precedence) */
    Node *n = first_stmt("2 + 3 * 4");
    /* root should be + */
    CHECK(n && n->type == NODE_BINOP &&
          strcmp(n->as.binop.op, "+") == 0,
          "precedence: root is +");
    /* right child of + should be * */
    CHECK(n && n->as.binop.right &&
          n->as.binop.right->type == NODE_BINOP &&
          strcmp(n->as.binop.right->as.binop.op, "*") == 0,
          "precedence: right child is *");
}

void test_grouping() {
    /* (2 + 3) * 4 → the + should be deeper now */
    Node *n = first_stmt("(2 + 3) * 4");
    CHECK(n && n->type == NODE_BINOP &&
          strcmp(n->as.binop.op, "*") == 0,
          "grouping: root is *");
    CHECK(n && n->as.binop.left &&
          n->as.binop.left->type == NODE_BINOP &&
          strcmp(n->as.binop.left->as.binop.op, "+") == 0,
          "grouping: left child is +");
}

void test_unary_minus() {
    Node *n = first_stmt("-x");
    CHECK(n && n->type == NODE_UNARY &&
          strcmp(n->as.unary.op, "-") == 0,
          "unary minus");
}

void test_unary_not() {
    Node *n = first_stmt("not true");
    CHECK(n && n->type == NODE_UNARY &&
          strcmp(n->as.unary.op, "not") == 0,
          "unary not");
}

void test_comparison() {
    Node *n = first_stmt("x == 5");
    CHECK(n && n->type == NODE_BINOP &&
          strcmp(n->as.binop.op, "==") == 0,
          "equality comparison");
}

void test_print_stmt() {
    Node *n = first_stmt("print 42");
    CHECK(n && n->type == NODE_PRINT &&
          n->as.print_stmt.value->as.int_lit.value == 42,
          "print statement");
}

void test_return_stmt() {
    Node *n = first_stmt("return 1");
    CHECK(n && n->type == NODE_RETURN &&
          n->as.return_stmt.value->as.int_lit.value == 1,
          "return statement");
}

void test_if_stmt() {
    Node *n = first_stmt("if x > 0 { print x }");
    CHECK(n && n->type == NODE_IF &&
          n->as.if_stmt.cond != NULL &&
          n->as.if_stmt.then_block != NULL &&
          n->as.if_stmt.else_block == NULL,
          "if without else");
}

void test_if_else() {
    Node *n = first_stmt("if x > 0 { print x } else { print 0 }");
    CHECK(n && n->type == NODE_IF &&
          n->as.if_stmt.else_block != NULL,
          "if with else");
}

void test_while_stmt() {
    Node *n = first_stmt("while x > 0 { x = x - 1 }");
    CHECK(n && n->type == NODE_WHILE &&
          n->as.while_stmt.cond != NULL &&
          n->as.while_stmt.body != NULL,
          "while loop");
}

void test_fn_def_no_params() {
    Node *n = first_stmt("fn greet() { print 1 }");
    CHECK(n && n->type == NODE_FN_DEF &&
          strcmp(n->as.fn_def.name, "greet") == 0 &&
          n->as.fn_def.param_count == 0,
          "fn def no params");
}

void test_fn_def_with_params() {
    Node *n = first_stmt("fn add(a, b) { return a }");
    CHECK(n && n->type == NODE_FN_DEF &&
          n->as.fn_def.param_count == 2 &&
          strcmp(n->as.fn_def.params[0], "a") == 0 &&
          strcmp(n->as.fn_def.params[1], "b") == 0,
          "fn def with params");
}

void test_fn_call() {
    Node *n = first_stmt("add(1, 2)");
    CHECK(n && n->type == NODE_CALL &&
          strcmp(n->as.call.name, "add") == 0 &&
          n->as.call.arg_count == 2,
          "function call with args");
}

void test_fn_call_no_args() {
    Node *n = first_stmt("greet()");
    CHECK(n && n->type == NODE_CALL &&
          n->as.call.arg_count == 0,
          "function call no args");
}

void test_nested_call() {
    Node *n = first_stmt("print factorial(10)");
    CHECK(n && n->type == NODE_PRINT &&
          n->as.print_stmt.value->type == NODE_CALL,
          "nested call in print");
}

void test_logical_and_or() {
    Node *n = first_stmt("a and b or c");
    /* or is lower precedence, so root = or */
    CHECK(n && n->type == NODE_BINOP &&
          strcmp(n->as.binop.op, "or") == 0,
          "and/or precedence: root is or");
}

void test_multiline_program() {
    const char *src =
        "let x = 10\n"
        "let y = 20\n"
        "print x + y\n";
    Node *root = parse_source(src);
    CHECK(root && root->type == NODE_BLOCK &&
          root->as.block.count == 3,
          "multiline program has 3 stmts");
}

void test_parse_error() {
    /* '(' with no matching ')' should fail */
    Node *n = parse_source("let x = (1 + 2");
    CHECK(n == NULL, "unclosed paren gives parse error");
}

/* ── Demo: print the AST for a real program ─────────────────────── */
void demo_ast() {
    const char *src =
        "fn factorial(n) {\n"
        "    if n <= 1 { return 1 }\n"
        "    return n * factorial(n - 1)\n"
        "}\n"
        "let result = factorial(10)\n"
        "print result\n";

    printf("\n══════════════════════════════════════════\n");
    printf("  AST for the factorial program\n");
    printf("══════════════════════════════════════════\n");
    Node *root = parse_source(src);
    if (root) ast_print(root, 0);
    printf("══════════════════════════════════════════\n\n");
    ast_free(root);
}

/* ── Main ─────────────────────────────────────────────────────── */
int main(void) {
    printf("═══════════════════════════════\n");
    printf("  Parser Test Suite\n");
    printf("═══════════════════════════════\n");

    test_int_literal();
    test_float_literal();
    test_string_literal();
    test_bool_true();
    test_bool_false();
    test_null_literal();
    test_ident();
    test_let();
    test_assign();
    test_binop_add();
    test_binop_precedence();
    test_grouping();
    test_unary_minus();
    test_unary_not();
    test_comparison();
    test_print_stmt();
    test_return_stmt();
    test_if_stmt();
    test_if_else();
    test_while_stmt();
    test_fn_def_no_params();
    test_fn_def_with_params();
    test_fn_call();
    test_fn_call_no_args();
    test_nested_call();
    test_logical_and_or();
    test_multiline_program();
    test_parse_error();

    printf("\n  %d/%d tests passed", passed, passed + failed);
    if (failed) printf("  (%d FAILED)", failed);
    printf("\n═══════════════════════════════\n");

    demo_ast();
    return failed > 0 ? 1 : 0;
}
