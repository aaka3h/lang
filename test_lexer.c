/* ─────────────────────────────────────────────────────────────────
   test_lexer.c  —  tests for every feature of the lexer
   
   Run:  gcc lexer.c test_lexer.c -o test_lexer && ./test_lexer
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

/* Simple test counter */
static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define PASS() do { tests_run++; tests_passed++; printf("  [PASS] %s\n", __func__); } while(0)
#define FAIL(msg) do { tests_run++; tests_failed++; printf("  [FAIL] %s — %s\n", __func__, msg); } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

/* Helper: tokenize a string and return the Nth token */
static Token nth_token(const char *src, int n) {
    Lexer  l;
    Token  tokens[MAX_TOKENS];
    lexer_init(&l, src);
    int count = lexer_tokenize_all(&l, tokens, MAX_TOKENS);
    if (n < count) return tokens[n];
    Token eof = {TOK_EOF, "EOF", 0, 0};
    return eof;
}

/* ── Tests ───────────────────────────────────────────────────────── */

void test_integers() {
    Token t = nth_token("42", 0);
    CHECK(t.type == TOK_INT && strcmp(t.value, "42") == 0,
          "expected INT 42");
}

void test_float() {
    Token t = nth_token("3.14", 0);
    CHECK(t.type == TOK_FLOAT && strcmp(t.value, "3.14") == 0,
          "expected FLOAT 3.14");
}

void test_string() {
    Token t = nth_token("\"hello\"", 0);
    CHECK(t.type == TOK_STRING && strcmp(t.value, "hello") == 0,
          "expected STRING hello");
}

void test_string_escape() {
    Token t = nth_token("\"hi\\nbye\"", 0);
    CHECK(t.type == TOK_STRING && t.value[2] == '\n',
          "expected newline escape in string");
}

void test_ident() {
    Token t = nth_token("myVariable", 0);
    CHECK(t.type == TOK_IDENT && strcmp(t.value, "myVariable") == 0,
          "expected IDENT myVariable");
}

void test_keywords() {
    CHECK(nth_token("let",    0).type == TOK_LET,    "let");
    CHECK(nth_token("fn",     0).type == TOK_FN,     "fn");
    CHECK(nth_token("if",     0).type == TOK_IF,     "if");
    CHECK(nth_token("else",   0).type == TOK_ELSE,   "else");
    CHECK(nth_token("while",  0).type == TOK_WHILE,  "while");
    CHECK(nth_token("return", 0).type == TOK_RETURN, "return");
    CHECK(nth_token("true",   0).type == TOK_TRUE,   "true");
    CHECK(nth_token("false",  0).type == TOK_FALSE,  "false");
    CHECK(nth_token("null",   0).type == TOK_NULL,   "null");
    CHECK(nth_token("and",    0).type == TOK_AND,    "and");
    CHECK(nth_token("or",     0).type == TOK_OR,     "or");
    CHECK(nth_token("not",    0).type == TOK_NOT,    "not");
    CHECK(nth_token("print",  0).type == TOK_PRINT,  "print");
}

void test_operators() {
    CHECK(nth_token("+",  0).type == TOK_PLUS,     "+");
    CHECK(nth_token("-",  0).type == TOK_MINUS,    "-");
    CHECK(nth_token("*",  0).type == TOK_STAR,     "*");
    CHECK(nth_token("/",  0).type == TOK_SLASH,    "/");
    CHECK(nth_token("%",  0).type == TOK_PERCENT,  "%");
    CHECK(nth_token("=",  0).type == TOK_EQ,       "=");
    CHECK(nth_token("==", 0).type == TOK_EQEQ,     "==");
    CHECK(nth_token("!=", 0).type == TOK_BANGEQ,   "!=");
    CHECK(nth_token("<",  0).type == TOK_LT,       "<");
    CHECK(nth_token(">",  0).type == TOK_GT,       ">");
    CHECK(nth_token("<=", 0).type == TOK_LTEQ,     "<=");
    CHECK(nth_token(">=", 0).type == TOK_GTEQ,     ">=");
}

void test_delimiters() {
    CHECK(nth_token("(", 0).type == TOK_LPAREN,    "(");
    CHECK(nth_token(")", 0).type == TOK_RPAREN,    ")");
    CHECK(nth_token("{", 0).type == TOK_LBRACE,    "{");
    CHECK(nth_token("}", 0).type == TOK_RBRACE,    "}");
    CHECK(nth_token(",", 0).type == TOK_COMMA,     ",");
    CHECK(nth_token(";", 0).type == TOK_SEMICOLON, ";");
    CHECK(nth_token(".", 0).type == TOK_DOT,       ".");
}

void test_comment_skip() {
    /* Comments should be skipped entirely */
    Token t = nth_token("# this is a comment\n42", 0);
    /* First real token should be NEWLINE then 42, but we skip newline too */
    Lexer l; Token tokens[MAX_TOKENS];
    lexer_init(&l, "# comment\n42");
    lexer_tokenize_all(&l, tokens, MAX_TOKENS);
    /* Find first non-newline token */
    int i = 0;
    while (tokens[i].type == TOK_NEWLINE) i++;
    CHECK(tokens[i].type == TOK_INT && strcmp(tokens[i].value, "42") == 0,
          "comment should be skipped, 42 should follow");
}

void test_line_tracking() {
    Lexer l; Token tokens[MAX_TOKENS];
    lexer_init(&l, "let\nx");
    lexer_tokenize_all(&l, tokens, MAX_TOKENS);
    /* tokens[0]=LET line 1, tokens[1]=NEWLINE, tokens[2]=IDENT line 2 */
    int x_idx = 2;
    CHECK(tokens[0].line == 1, "let should be on line 1");
    CHECK(tokens[x_idx].line == 2, "x should be on line 2");
}

void test_full_expression() {
    /* let result = 10 + 3 * 2 */
    Lexer l; Token tokens[MAX_TOKENS];
    lexer_init(&l, "let result = 10 + 3 * 2");
    int n = lexer_tokenize_all(&l, tokens, MAX_TOKENS);
    CHECK(tokens[0].type == TOK_LET,                      "0: LET");
    CHECK(tokens[1].type == TOK_IDENT,                    "1: IDENT");
    CHECK(tokens[2].type == TOK_EQ,                       "2: EQ");
    CHECK(tokens[3].type == TOK_INT,                      "3: INT 10");
    CHECK(tokens[4].type == TOK_PLUS,                     "4: PLUS");
    CHECK(tokens[5].type == TOK_INT,                      "5: INT 3");
    CHECK(tokens[6].type == TOK_STAR,                     "6: STAR");
    CHECK(tokens[7].type == TOK_INT,                      "7: INT 2");
    CHECK(tokens[8].type == TOK_EOF,                      "8: EOF");
    (void)n;
}

void test_function_def() {
    Lexer l; Token tokens[MAX_TOKENS];
    lexer_init(&l, "fn add(a, b) { return a + b }");
    lexer_tokenize_all(&l, tokens, MAX_TOKENS);
    CHECK(tokens[0].type == TOK_FN,     "0: FN");
    CHECK(tokens[1].type == TOK_IDENT,  "1: IDENT add");
    CHECK(tokens[2].type == TOK_LPAREN, "2: LPAREN");
    CHECK(tokens[3].type == TOK_IDENT,  "3: IDENT a");
    CHECK(tokens[4].type == TOK_COMMA,  "4: COMMA");
    CHECK(tokens[5].type == TOK_IDENT,  "5: IDENT b");
    CHECK(tokens[6].type == TOK_RPAREN, "6: RPAREN");
}

void test_error_token() {
    Token t = nth_token("@", 0);
    CHECK(t.type == TOK_ERROR, "@ should produce an error token");
}

void test_unterminated_string() {
    Token t = nth_token("\"hello", 0);
    CHECK(t.type == TOK_ERROR, "unterminated string should be an error");
}

/* ── Demonstration: print all tokens for a real program ─────────── */
void demo_print_tokens() {
    const char *src =
        "fn factorial(n) {\n"
        "    if n <= 1 { return 1 }\n"
        "    return n * factorial(n - 1)\n"
        "}\n"
        "let result = factorial(10)\n"
        "print result\n";

    printf("\n══════════════════════════════════════════\n");
    printf("  Token dump for a sample program\n");
    printf("══════════════════════════════════════════\n");

    Lexer l;
    Token tokens[MAX_TOKENS];
    lexer_init(&l, src);
    int count = lexer_tokenize_all(&l, tokens, MAX_TOKENS);
    for (int i = 0; i < count; i++) {
        if (tokens[i].type == TOK_NEWLINE) continue;  /* skip noise */
        token_print(tokens[i]);
    }
    printf("══════════════════════════════════════════\n\n");
}

/* ── Main ────────────────────────────────────────────────────────── */
int main(void) {
    printf("═══════════════════════════════\n");
    printf("  Lexer Test Suite\n");
    printf("═══════════════════════════════\n");

    test_integers();
    test_float();
    test_string();
    test_string_escape();
    test_ident();
    test_keywords();
    test_operators();
    test_delimiters();
    test_comment_skip();
    test_line_tracking();
    test_full_expression();
    test_function_def();
    test_error_token();
    test_unterminated_string();

    printf("\n  %d/%d tests passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf("  (%d FAILED)", tests_failed);
    printf("\n═══════════════════════════════\n");

    demo_print_tokens();

    return tests_failed > 0 ? 1 : 0;
}
