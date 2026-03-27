#ifndef PARSER_H
#define PARSER_H

/* ─────────────────────────────────────────────────────────────────
   parser.h  —  the Parser's public interface
   
   The Parser takes a stream of tokens (from the Lexer) and builds
   an Abstract Syntax Tree (AST).
   
   It uses a technique called "Recursive Descent Parsing":
     - one function per grammar rule
     - functions call each other recursively
     - operator precedence is encoded in the call hierarchy
   
   This is exactly how CPython, GCC, Clang, and Go are parsed.
   ───────────────────────────────────────────────────────────────── */

#include "lexer.h"
#include "ast.h"

/* ─────────────────────────────────────────────────────────────────
   The Parser struct.
   
   tokens  — the full token array from the lexer
   count   — how many tokens there are
   pos     — current position in the token array
   error   — set to 1 if we hit a parse error
   errmsg  — description of the error
   ───────────────────────────────────────────────────────────────── */
typedef struct {
    Token  *tokens;
    int     count;
    int     pos;
    int     error;
    char    errmsg[256];
} Parser;

/* ── Public API ────────────────────────────────────────────────── */

/* Initialize parser with a token array */
void  parser_init(Parser *p, Token *tokens, int count);

/* Parse the entire program — returns a NODE_BLOCK (the root) */
Node *parser_parse(Parser *p);

/* Convenience: lex + parse a source string in one call */
Node *parse_source(const char *src);

#endif /* PARSER_H */
