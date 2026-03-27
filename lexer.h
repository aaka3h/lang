#ifndef LEXER_H
#define LEXER_H

/* ─────────────────────────────────────────────────────────────────
   lexer.h  —  the Lexer's public interface
   
   The Lexer is a struct that holds:
     - a pointer to the source string
     - current position (which character we're looking at)
     - line and column tracking (for error messages)
   
   You create one Lexer per source file/string.
   You call lexer_next_token() to get tokens one by one.
   When the source is exhausted, it returns TOK_EOF.
   ───────────────────────────────────────────────────────────────── */

#include "token.h"

/* Max tokens we'll store in one go (for the full tokenize pass) */
#define MAX_TOKENS 4096

/* ─────────────────────────────────────────────────────────────────
   The Lexer struct.
   
   src    — pointer to the source string (we don't own this memory)
   len    — total length of the source
   pos    — current character index  (the character we're AT)
   line   — current line number (starts at 1)
   col    — current column number (starts at 1)
   ───────────────────────────────────────────────────────────────── */
typedef struct {
    const char *src;
    int         len;
    int         pos;
    int         line;
    int         col;
} Lexer;

/* ── Public API ────────────────────────────────────────────────── */

/* Initialize a lexer with a source string */
void  lexer_init(Lexer *l, const char *source);

/* Get the next token from the stream */
Token lexer_next_token(Lexer *l);

/* Tokenize the entire source at once into an array.
   Returns the number of tokens written into `out`.
   `out` must be at least MAX_TOKENS in size.           */
int   lexer_tokenize_all(Lexer *l, Token *out, int max);

/* Print a token (for debugging) */
void  token_print(Token t);

#endif /* LEXER_H */
