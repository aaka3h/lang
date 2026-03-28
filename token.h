#ifndef TOKEN_H
#define TOKEN_H

/* ─────────────────────────────────────────────────────────────────
   token.h  —  defines every kind of token our language understands
   
   A TOKEN is the smallest meaningful unit of your language.
   The lexer's job is to turn raw text into a list of these.
   
   Think of it like turning a sentence into individual words +
   punctuation marks that a grammar can reason about.
   ───────────────────────────────────────────────────────────────── */

/* Every possible token type in our language */
typedef enum {

    /* ── Literals ──────────────────────────────────── */
    TOK_INT,        /* 42, 100, 0                       */
    TOK_FLOAT,      /* 3.14, 0.5                        */
    TOK_STRING,     /* "hello world"                    */
    TOK_IDENT,      /* variable names: x, myVar, count  */

    /* ── Keywords ──────────────────────────────────── */
    TOK_LET,        /* let                              */
    TOK_FN,         /* fn                               */
    TOK_RETURN,     /* return                           */
    TOK_IF,         /* if                               */
    TOK_ELSE,       /* else                             */
    TOK_WHILE,      /* while                            */
    TOK_FOR,        /* for                              */
    TOK_TRUE,       /* true                             */
    TOK_FALSE,      /* false                            */
    TOK_NULL,       /* null                             */
    TOK_AND,        /* and                              */
    TOK_OR,         /* or                               */
    TOK_NOT,        /* not                              */
    TOK_PRINT,      /* print  (built-in statement)      */
    TOK_IMPORT,     /* import (load a stdlib module)     */
    TOK_TRY,        /* try                               */
    TOK_CATCH,      /* catch                             */
    TOK_THROW,      /* throw                             */
    TOK_CLASS,      /* class                             */
    TOK_SELF,       /* self                              */
    TOK_EXTENDS,    /* extends                           */
    TOK_SUPER,      /* super                             */
    TOK_BREAK,      /* break                             */
    TOK_CONTINUE,   /* continue                          */

    /* ── Arithmetic operators ──────────────────────── */
    TOK_PLUS,       /* +                                */
    TOK_MINUS,      /* -                                */
    TOK_STAR,       /* *                                */
    TOK_SLASH,      /* /                                */
    TOK_IDIV,       /* // (integer division)            */
    TOK_PLUS_EQ,    /* +=                               */
    TOK_MINUS_EQ,   /* -=                               */
    TOK_STAR_EQ,    /* *=                               */
    TOK_SLASH_EQ,   /* /=                               */
    TOK_PERCENT,    /* %                                */

    /* ── Comparison operators ──────────────────────── */
    TOK_EQ,         /* =   (assignment)                 */
    TOK_EQEQ,       /* ==  (equality check)             */
    TOK_BANGEQ,     /* !=  (not equal)                  */
    TOK_LT,         /* <                                */
    TOK_GT,         /* >                                */
    TOK_LTEQ,       /* <=                               */
    TOK_GTEQ,       /* >=                               */

    /* ── Delimiters ────────────────────────────────── */
    TOK_LPAREN,     /* (                                */
    TOK_RPAREN,     /* )                                */
    TOK_LBRACE,     /* {                                */
    TOK_RBRACE,     /* }                                */
    TOK_LBRACKET,   /* [                                */
    TOK_RBRACKET,   /* ]                                */
    TOK_COMMA,      /* ,                                */
    TOK_SEMICOLON,  /* ;                                */
    TOK_COLON,      /* :                                */
    TOK_DOT,        /* .                                */

    /* ── Special ────────────────────────────────────── */
    TOK_NEWLINE,    /* \n  (we track lines)             */
    TOK_EOF,        /* end of file — always last        */
    TOK_ERROR,      /* lexer hit something it can't read*/

} TokenType;

/* ─────────────────────────────────────────────────────────────────
   The Token struct.
   
   Each token carries:
     - its TYPE  (what kind of thing it is)
     - its VALUE (the actual text, e.g. the variable name or number)
     - LINE/COL  (where in the source file it appeared — for errors)
   ───────────────────────────────────────────────────────────────── */
#define MAX_TOKEN_LEN 256

typedef struct {
    TokenType   type;
    char        value[MAX_TOKEN_LEN];  /* e.g. "myVar", "42", "hello" */
    int         line;                  /* 1-indexed line number        */
    int         col;                   /* 1-indexed column number      */
} Token;

/* ─────────────────────────────────────────────────────────────────
   Utility: return a human-readable name for a token type.
   Incredibly useful when printing errors and debug output.
   ───────────────────────────────────────────────────────────────── */
static inline const char* token_type_name(TokenType t) {
    switch (t) {
        case TOK_INT:       return "INT";
        case TOK_FLOAT:     return "FLOAT";
        case TOK_STRING:    return "STRING";
        case TOK_IDENT:     return "IDENT";
        case TOK_LET:       return "LET";
        case TOK_FN:        return "FN";
        case TOK_RETURN:    return "RETURN";
        case TOK_IF:        return "IF";
        case TOK_ELSE:      return "ELSE";
        case TOK_WHILE:     return "WHILE";
        case TOK_FOR:       return "FOR";
        case TOK_TRUE:      return "TRUE";
        case TOK_FALSE:     return "FALSE";
        case TOK_NULL:      return "NULL";
        case TOK_AND:       return "AND";
        case TOK_OR:        return "OR";
        case TOK_NOT:       return "NOT";
        case TOK_PRINT:     return "PRINT";
        case TOK_IMPORT:    return "IMPORT";
        case TOK_TRY:       return "TRY";
        case TOK_CATCH:     return "CATCH";
        case TOK_THROW:     return "THROW";
        case TOK_CLASS:     return "CLASS";
        case TOK_SELF:      return "SELF";
        case TOK_EXTENDS:   return "EXTENDS";
        case TOK_SUPER:     return "SUPER";
        case TOK_BREAK:     return "BREAK";
        case TOK_CONTINUE:  return "CONTINUE";
        case TOK_PLUS:      return "PLUS";
        case TOK_MINUS:     return "MINUS";
        case TOK_STAR:      return "STAR";
        case TOK_SLASH:     return "SLASH";
        case TOK_PERCENT:   return "PERCENT";
        case TOK_EQ:        return "EQ";
        case TOK_EQEQ:      return "EQEQ";
        case TOK_BANGEQ:    return "BANGEQ";
        case TOK_LT:        return "LT";
        case TOK_GT:        return "GT";
        case TOK_LTEQ:      return "LTEQ";
        case TOK_GTEQ:      return "GTEQ";
        case TOK_LPAREN:    return "LPAREN";
        case TOK_RPAREN:    return "RPAREN";
        case TOK_LBRACE:    return "LBRACE";
        case TOK_RBRACE:    return "RBRACE";
        case TOK_LBRACKET:  return "LBRACKET";
        case TOK_RBRACKET:  return "RBRACKET";
        case TOK_COMMA:     return "COMMA";
        case TOK_SEMICOLON: return "SEMICOLON";
        case TOK_COLON:     return "COLON";
        case TOK_DOT:       return "DOT";
        case TOK_NEWLINE:   return "NEWLINE";
        case TOK_EOF:       return "EOF";
        case TOK_ERROR:     return "ERROR";
        default:            return "UNKNOWN";
    }
}

#endif /* TOKEN_H */
