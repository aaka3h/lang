/* ─────────────────────────────────────────────────────────────────
   lexer.c  —  the Lexer implementation
   
   HOW A LEXER WORKS:
   We have a pointer into the source string.
   We look at the current character, decide what token it starts,
   consume characters until the token is complete, return it.
   
   This is a hand-written lexer — same approach as CPython, GCC,
   Clang, and Go. No regex, no magic library. Just char-by-char.
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

/* ─────────────────────────────────────────────────────────────────
   INTERNAL HELPERS  (static = private to this file)
   ───────────────────────────────────────────────────────────────── */

/* What character are we currently looking at? */
static char cur(Lexer *l) {
    if (l->pos >= l->len) return '\0';  /* past end = null terminator */
    return l->src[l->pos];
}

/* One character ahead — used to distinguish = from ==, < from <= */
static char peek(Lexer *l) {
    if (l->pos + 1 >= l->len) return '\0';
    return l->src[l->pos + 1];
}

/* Move forward one character, updating line/col tracking */
static void advance(Lexer *l) {
    if (l->pos >= l->len) return;
    if (l->src[l->pos] == '\n') {
        l->line++;
        l->col = 1;          /* reset column at each new line */
    } else {
        l->col++;
    }
    l->pos++;
}

/* Build a Token with the given type, value, and current position */
static Token make_token(Lexer *l, TokenType type, const char *value) {
    Token t;
    t.type = type;
    t.line = l->line;
    t.col  = l->col;
    strncpy(t.value, value, MAX_TOKEN_LEN - 1);
    t.value[MAX_TOKEN_LEN - 1] = '\0';  /* always null-terminate */
    return t;
}

/* Build an error token with a message */
static Token error_token(Lexer *l, const char *msg) {
    return make_token(l, TOK_ERROR, msg);
}

/* Skip whitespace (but NOT newlines — newlines are tokens for us) */
static void skip_whitespace(Lexer *l) {
    while (cur(l) == ' ' || cur(l) == '\t' || cur(l) == '\r')
        advance(l);
}

/* Skip a comment from # to end of line */
static void skip_comment(Lexer *l) {
    while (cur(l) != '\n' && cur(l) != '\0')
        advance(l);
}

/* ─────────────────────────────────────────────────────────────────
   KEYWORD TABLE
   
   After we scan an identifier like "let" or "if", we check this
   table to see if it's a reserved keyword.
   
   If it's not in the table, it's just an IDENT (variable name).
   ───────────────────────────────────────────────────────────────── */
typedef struct { const char *word; TokenType type; } Keyword;

static Keyword KEYWORDS[] = {
    { "let",    TOK_LET    },
    { "fn",     TOK_FN     },
    { "return", TOK_RETURN },
    { "if",     TOK_IF     },
    { "else",   TOK_ELSE   },
    { "while",  TOK_WHILE  },
    { "for",    TOK_FOR    },
    { "true",   TOK_TRUE   },
    { "false",  TOK_FALSE  },
    { "null",   TOK_NULL   },
    { "and",    TOK_AND    },
    { "or",     TOK_OR     },
    { "not",    TOK_NOT    },
    { "print",  TOK_PRINT  },
    { "import", TOK_IMPORT },
    { "try",    TOK_TRY    },
    { "catch",  TOK_CATCH  },
    { "throw",  TOK_THROW  },
    { "class",  TOK_CLASS  },
    { "self",   TOK_SELF   },
    { "extends", TOK_EXTENDS },
    { "super",  TOK_SUPER  },
    { NULL,     TOK_ERROR  },   /* sentinel — marks end of table */
};

static TokenType keyword_lookup(const char *word) {
    for (int i = 0; KEYWORDS[i].word != NULL; i++) {
        if (strcmp(KEYWORDS[i].word, word) == 0)
            return KEYWORDS[i].type;
    }
    return TOK_IDENT;  /* not a keyword → it's an identifier */
}

/* ─────────────────────────────────────────────────────────────────
   TOKEN SCANNERS
   
   Each of these functions handles one category of token.
   They all assume the lexer is positioned at the START of the token.
   ───────────────────────────────────────────────────────────────── */

/*
   scan_number — reads an integer or float.
   
   We consume digits. If we hit a '.', it's a float.
   Examples: 42, 3.14, 100, 0.001
*/
static Token scan_number(Lexer *l) {
    char buf[MAX_TOKEN_LEN];
    int  i = 0;
    int  is_float = 0;

    while (isdigit(cur(l)) || (cur(l) == '.' && !is_float)) {
        if (cur(l) == '.') {
            /* peek ahead: make sure it's really a decimal, not range (..) */
            if (!isdigit(peek(l))) break;
            is_float = 1;
        }
        if (i < MAX_TOKEN_LEN - 1)
            buf[i++] = cur(l);
        advance(l);
    }
    buf[i] = '\0';

    return make_token(l, is_float ? TOK_FLOAT : TOK_INT, buf);
}

/*
   scan_string — reads a quoted string between " ".
   
   Supports escape sequences: \n \t \" \\
   The token value is the string CONTENT (without the quotes).
   
   Example: "hello\nworld"  →  value = "hello\nworld" (with real newline)
*/
static Token scan_string(Lexer *l) {
    advance(l);  /* consume the opening " */

    char buf[MAX_TOKEN_LEN];
    int  i = 0;

    while (cur(l) != '"' && cur(l) != '\0') {
        char c = cur(l);

        if (c == '\\') {
            advance(l);  /* consume the backslash */
            switch (cur(l)) {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                default:
                    return error_token(l, "Unknown escape sequence");
            }
        }

        if (i < MAX_TOKEN_LEN - 1)
            buf[i++] = c;
        advance(l);
    }

    if (cur(l) != '"')
        return error_token(l, "Unterminated string — missing closing \"");

    advance(l);  /* consume the closing " */
    buf[i] = '\0';

    return make_token(l, TOK_STRING, buf);
}

/*
   scan_ident — reads an identifier or keyword.
   
   Identifiers start with a letter or _, followed by letters/digits/_.
   After scanning, we check if it's a keyword.
   
   Examples:  myVar → IDENT("myVar")
              let   → LET
              true  → TRUE
*/
static Token scan_ident(Lexer *l) {
    char buf[MAX_TOKEN_LEN];
    int  i = 0;

    while (isalnum(cur(l)) || cur(l) == '_') {
        if (i < MAX_TOKEN_LEN - 1)
            buf[i++] = cur(l);
        advance(l);
    }
    buf[i] = '\0';

    TokenType type = keyword_lookup(buf);
    return make_token(l, type, buf);
}

/* ─────────────────────────────────────────────────────────────────
   lexer_next_token — the main function
   
   Called repeatedly to get tokens one by one.
   This is the public heart of the lexer.
   ───────────────────────────────────────────────────────────────── */
Token lexer_next_token(Lexer *l) {
    skip_whitespace(l);

    /* Record position BEFORE consuming */
    int line = l->line;
    int col  = l->col;

    char c = cur(l);

    /* ── End of source ──────────────────────────────── */
    if (c == '\0') return make_token(l, TOK_EOF, "EOF");

    /* ── Comment: skip entire line ──────────────────── */
    if (c == '#') { skip_comment(l); return lexer_next_token(l); }

    /* ── Newline ─────────────────────────────────────── */
    if (c == '\n') { advance(l); return make_token(l, TOK_NEWLINE, "\\n"); }

    /* ── Numbers ─────────────────────────────────────── */
    if (isdigit(c)) return scan_number(l);

    /* ── Strings ─────────────────────────────────────── */
    if (c == '"') return scan_string(l);

    /* ── Identifiers & keywords ──────────────────────── */
    if (isalpha(c) || c == '_') return scan_ident(l);

    /* ── Operators & delimiters ──────────────────────── */
    advance(l);  /* consume the character before making the token */

    switch (c) {
        case '+': return make_token(l, TOK_PLUS,      "+");
        case '-': return make_token(l, TOK_MINUS,     "-");
        case '*': return make_token(l, TOK_STAR,      "*");
        case '/': return make_token(l, TOK_SLASH,     "/");
        case '%': return make_token(l, TOK_PERCENT,   "%");
        case '(': return make_token(l, TOK_LPAREN,    "(");
        case ')': return make_token(l, TOK_RPAREN,    ")");
        case '{': return make_token(l, TOK_LBRACE,    "{");
        case '}': return make_token(l, TOK_RBRACE,    "}");
        case '[': return make_token(l, TOK_LBRACKET,  "[");
        case ']': return make_token(l, TOK_RBRACKET,  "]");
        case ',': return make_token(l, TOK_COMMA,     ",");
        case ';': return make_token(l, TOK_SEMICOLON, ";");
        case ':': return make_token(l, TOK_COLON,     ":");
        case '.': return make_token(l, TOK_DOT,       ".");

        /* Two-character operators — need to check the next char */
        case '=':
            if (cur(l) == '=') { advance(l); return make_token(l, TOK_EQEQ,   "=="); }
            return make_token(l, TOK_EQ, "=");

        case '!':
            if (cur(l) == '=') { advance(l); return make_token(l, TOK_BANGEQ, "!="); }
            return error_token(l, "Unexpected '!' — did you mean '!='?");

        case '<':
            if (cur(l) == '=') { advance(l); return make_token(l, TOK_LTEQ,   "<="); }
            return make_token(l, TOK_LT, "<");

        case '>':
            if (cur(l) == '=') { advance(l); return make_token(l, TOK_GTEQ,   ">="); }
            return make_token(l, TOK_GT, ">");

        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "Unknown character: '%c' (ASCII %d)", c, (int)c);
            return error_token(l, msg);
        }
    }

    (void)line; (void)col;  /* suppress unused-variable warnings */
}

/* ─────────────────────────────────────────────────────────────────
   PUBLIC API IMPLEMENTATIONS
   ───────────────────────────────────────────────────────────────── */

/* Initialize the lexer with a source string */
void lexer_init(Lexer *l, const char *source) {
    l->src  = source;
    l->len  = (int)strlen(source);
    l->pos  = 0;
    l->line = 1;
    l->col  = 1;
}

/* Scan all tokens at once into an array */
int lexer_tokenize_all(Lexer *l, Token *out, int max) {
    int count = 0;
    while (count < max) {
        Token t = lexer_next_token(l);
        out[count++] = t;
        if (t.type == TOK_EOF || t.type == TOK_ERROR) break;
    }
    return count;
}

/* Pretty-print a token — invaluable for debugging */
void token_print(Token t) {
    printf("[line %2d col %2d]  %-12s  \"%s\"\n",
           t.line, t.col, token_type_name(t.type), t.value);
}
