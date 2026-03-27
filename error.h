#ifndef ERROR_H
#define ERROR_H

/* ─────────────────────────────────────────────────────────────────
   error.h  —  Pretty error reporting
   
   Produces errors like this:
   
     [SyntaxError] line 3, col 12
         let y = x + 10
                 ^
     Undefined variable 'x'
   
   This is Phase 4 — making your language feel professional.
   Python, Rust, and Clang all do this. Now yours does too.
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Error categories */
typedef enum {
    ERR_LEXER,    /* bad character, unterminated string  */
    ERR_SYNTAX,   /* unexpected token, missing bracket   */
    ERR_RUNTIME,  /* undefined variable, div by zero     */
    ERR_TYPE,     /* wrong type for operation            */
} ErrorKind;

static inline const char *err_kind_name(ErrorKind k) {
    switch (k) {
        case ERR_LEXER:   return "LexError";
        case ERR_SYNTAX:  return "SyntaxError";
        case ERR_RUNTIME: return "RuntimeError";
        case ERR_TYPE:    return "TypeError";
        default:          return "Error";
    }
}

/* ─────────────────────────────────────────────────────────────────
   err_report — print a formatted error with source highlight
   
   src      — the full source string (so we can print the line)
   kind     — error category
   line     — 1-indexed line number
   col      — 1-indexed column number
   message  — the error description
   ───────────────────────────────────────────────────────────────── */
static inline void err_report(const char *src, ErrorKind kind,
                               int line, int col, const char *message) {
    fprintf(stderr, "\n");
    fprintf(stderr, "  [%s] line %d, col %d\n", err_kind_name(kind), line, col);

    /* Find and print the relevant source line */
    if (src && line > 0) {
        const char *p    = src;
        int         cur  = 1;

        /* Advance to the correct line */
        while (*p && cur < line) {
            if (*p == '\n') cur++;
            p++;
        }

        /* Print the line with 4-space indent */
        fprintf(stderr, "  ");
        const char *line_start = p;
        while (*p && *p != '\n') {
            fprintf(stderr, "%c", *p++);
        }
        fprintf(stderr, "\n");

        /* Print the caret under the error column */
        fprintf(stderr, "  ");
        int arrow_col = (col > 0) ? col - 1 : 0;
        /* Count display width up to arrow_col (handle tabs) */
        const char *q = line_start;
        int disp = 0;
        while (*q && *q != '\n' && disp < arrow_col) {
            if (*q == '\t') { fprintf(stderr, "    "); disp++; }
            else            { fprintf(stderr, " ");    disp++; }
            q++;
        }
        fprintf(stderr, "^\n");
    }

    fprintf(stderr, "  %s\n\n", message);
}

/* ─────────────────────────────────────────────────────────────────
   err_report_simple — when we don't have source (e.g. REPL)
   ───────────────────────────────────────────────────────────────── */
static inline void err_report_simple(ErrorKind kind,
                                      int line, int col,
                                      const char *message) {
    fprintf(stderr, "\n  [%s] line %d, col %d\n  %s\n\n",
            err_kind_name(kind), line, col, message);
}

#endif /* ERROR_H */
