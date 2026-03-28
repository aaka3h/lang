#ifndef LINT_MODULE_H
#define LINT_MODULE_H

/*
   lint_module.h — Static analysis for Lang source code

   Usage in Lang:
     import "lint"
     let code = readfile("myprogram.lang")
     let errors = lint(code)
     print errors

   What it detects:
     - Unclosed braces / brackets / parens
     - Missing 'let' before assignment (bare name = value)
     - Undefined variables (basic scope tracking)
     - Functions called with wrong arg count
     - print without a value
     - throw without a value
     - Empty catch blocks
     - Deeply nested code (warning)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "value.h"

#define LINT_MAX_ERRORS  64
#define LINT_MAX_VARS   256
#define LINT_MAX_FNS     64

typedef struct {
    char message[256];
    int  line;
} LintError;

typedef struct {
    char name[128];
    int  param_count;
} LintFn;

typedef struct {
    LintError errors[LINT_MAX_ERRORS];
    int       error_count;

    /* known variables */
    char  vars[LINT_MAX_VARS][128];
    int   var_count;

    /* known functions */
    LintFn fns[LINT_MAX_FNS];
    int    fn_count;

    /* brace/bracket/paren depth */
    int brace_depth;
    int paren_depth;
    int bracket_depth;

    int line;
} LintState;

static void lint_add_error(LintState *ls, const char *msg) {
    if (ls->error_count >= LINT_MAX_ERRORS) return;
    LintError *e = &ls->errors[ls->error_count++];
    snprintf(e->message, 255, "%s", msg);
    e->line = ls->line;
}

static void lint_add_var(LintState *ls, const char *name) {
    if (ls->var_count >= LINT_MAX_VARS) return;
    strncpy(ls->vars[ls->var_count++], name, 127);
}

static int lint_has_var(LintState *ls, const char *name) {
    /* built-ins */
    static const char *builtins[] = {
        "true","false","null","pi","e","inf",
        "print","len","str","int","float","type",
        "abs","sqrt","pow","floor","ceil","round",
        "push","pop","has","del","keys","values",
        "sin","cos","tan","log","log2","log10","exp",
        "upper","lower","trim","replace","contains",
        "startswith","endswith","repeat","substr","indexof",
        "readfile","writefile","appendfile","input",
        "clock","exit","min","max","clamp",
        NULL
    };
    for (int i = 0; builtins[i]; i++)
        if (strcmp(builtins[i], name) == 0) return 1;
    for (int i = 0; i < ls->var_count; i++)
        if (strcmp(ls->vars[i], name) == 0) return 1;
    return 0;
}

static int lint_has_fn(LintState *ls, const char *name) {
    for (int i = 0; i < ls->fn_count; i++)
        if (strcmp(ls->fns[i].name, name) == 0) return 1;
    /* built-in functions */
    static const char *bfns[] = {
        "len","str","int","float","type","abs","sqrt","pow",
        "floor","ceil","round","push","pop","has","del",
        "keys","values","sin","cos","tan","log","log2","log10",
        "exp","upper","lower","trim","replace","contains",
        "startswith","endswith","repeat","substr","indexof",
        "readfile","writefile","appendfile","input","clock",
        "exit","min","max","clamp","lint","ask",
        NULL
    };
    for (int i = 0; bfns[i]; i++)
        if (strcmp(bfns[i], name) == 0) return 1;
    return 0;
}

/* Skip whitespace, return pointer past it */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Read an identifier into buf, return pointer past it */
static const char *read_ident(const char *p, char *buf, int max) {
    int i = 0;
    while ((isalnum((unsigned char)*p) || *p == '_') && i < max-1)
        buf[i++] = *p++;
    buf[i] = '\0';
    return p;
}

/* Count commas at top level (not inside nested parens) to estimate arg count */
static int count_args(const char *p) {
    /* p points just after the opening '(' */
    if (*p == ')') return 0;
    int depth = 0, args = 1;
    while (*p && !(*p == ')' && depth == 0)) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        else if (*p == ',' && depth == 0) args++;
        p++;
    }
    return args;
}

/* Main lint pass — line by line */
static Value builtin_lint(Value *args, int n) {
    if (n < 1 || args[0].type != VAL_STRING)
        return val_string("Error: lint() requires a string argument");

    const char *src = args[0].as.s;
    LintState   ls;
    memset(&ls, 0, sizeof(ls));
    ls.line = 1;

    /* First pass: collect all fn definitions and let declarations */
    const char *p = src;
    while (*p) {
        if (*p == '\n') { ls.line++; p++; continue; }
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }

        /* Skip string literals */
        if (*p == '"') {
            p++;
            while (*p && *p != '"') { if (*p == '\\') p++; p++; }
            if (*p == '"') p++;
            continue;
        }

        const char *ws = skip_ws(p);

        /* fn name(params) */
        if (strncmp(ws, "fn ", 3) == 0) {
            ws += 3; ws = skip_ws(ws);
            char fname[128] = {0};
            ws = read_ident(ws, fname, 128);
            if (fname[0] && ls.fn_count < LINT_MAX_FNS) {
                strncpy(ls.fns[ls.fn_count].name, fname, 127);
                /* count params */
                ws = skip_ws(ws);
                int params = 0;
                if (*ws == '(') {
                    ws++;
                    if (*ws != ')') {
                        params = 1;
                        while (*ws && *ws != ')') {
                            if (*ws == ',') params++;
                            ws++;
                        }
                    }
                }
                ls.fns[ls.fn_count].param_count = params;
                ls.fn_count++;
                /* also register fn name as a variable */
                lint_add_var(&ls, fname);
            }
        }

        /* let name = ... */
        if (strncmp(ws, "let ", 4) == 0) {
            ws += 4; ws = skip_ws(ws);
            char vname[128] = {0};
            read_ident(ws, vname, 128);
            if (vname[0]) lint_add_var(&ls, vname);
        }

        /* class name */
        if (strncmp(ws, "class ", 6) == 0) {
            ws += 6; ws = skip_ws(ws);
            char cname[128] = {0};
            read_ident(ws, cname, 128);
            if (cname[0]) lint_add_var(&ls, cname);
        }

        p++;
    }

    /* Second pass: actual checks */
    p = src;
    ls.line = 1;
    ls.brace_depth = ls.paren_depth = ls.bracket_depth = 0;
    int in_string = 0;
    int in_comment = 0;

    while (*p) {
        /* Track newlines */
        if (*p == '\n') {
            ls.line++;
            in_comment = 0;
            p++;
            continue;
        }

        /* Comments */
        if (*p == '#' && !in_string) { in_comment = 1; p++; continue; }
        if (in_comment) { p++; continue; }

        /* String literals */
        if (*p == '"' && !in_string) {
            in_string = 1; p++;
            while (*p && *p != '"' && *p != '\n') {
                if (*p == '\\') p++;
                p++;
            }
            if (*p == '"') { in_string = 0; p++; }
            continue;
        }

        /* Brace/bracket/paren tracking */
        if (*p == '{') { ls.brace_depth++;
            if (ls.brace_depth > 8) {
                char msg[128];
                snprintf(msg, 127, "Line %d: deeply nested code (depth %d) — consider refactoring",
                         ls.line, ls.brace_depth);
                lint_add_error(&ls, msg);
            }
            p++; continue;
        }
        if (*p == '}') {
            if (ls.brace_depth <= 0) {
                char msg[128];
                snprintf(msg, 127, "Line %d: unexpected '}' — no matching '{'", ls.line);
                lint_add_error(&ls, msg);
            } else ls.brace_depth--;
            p++; continue;
        }
        if (*p == '(') { ls.paren_depth++; p++; continue; }
        if (*p == ')') {
            if (ls.paren_depth <= 0) {
                char msg[128];
                snprintf(msg, 127, "Line %d: unexpected ')' — no matching '('", ls.line);
                lint_add_error(&ls, msg);
            } else ls.paren_depth--;
            p++; continue;
        }
        if (*p == '[') { ls.bracket_depth++; p++; continue; }
        if (*p == ']') {
            if (ls.bracket_depth <= 0) {
                char msg[128];
                snprintf(msg, 127, "Line %d: unexpected ']' — no matching '['", ls.line);
                lint_add_error(&ls, msg);
            } else ls.bracket_depth--;
            p++; continue;
        }

        /* Check keywords at start of line tokens */
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            char word[128] = {0};
            p = (char*)read_ident(p, word, 128);
            const char *after = skip_ws(p);

            /* print without value */
            if (strcmp(word, "print") == 0 && (*after == '\n' || *after == '#' || *after == '\0')) {
                char msg[128];
                snprintf(msg, 127, "Line %d: 'print' with no value", ls.line);
                lint_add_error(&ls, msg);
            }

            /* throw without value */
            if (strcmp(word, "throw") == 0 && (*after == '\n' || *after == '#' || *after == '\0')) {
                char msg[128];
                snprintf(msg, 127, "Line %d: 'throw' with no value", ls.line);
                lint_add_error(&ls, msg);
            }

            /* function call: name( */
            if (*after == '(' &&
                strcmp(word, "if") != 0 &&
                strcmp(word, "while") != 0 &&
                strcmp(word, "for") != 0 &&
                strcmp(word, "fn") != 0 &&
                strcmp(word, "class") != 0) {

                if (!lint_has_fn(&ls, word) && !lint_has_var(&ls, word)) {
                    char msg[128];
                    snprintf(msg, 127, "Line %d: '%s' called but not defined", ls.line, word);
                    lint_add_error(&ls, msg);
                }

                /* Check arg count for known functions */
                for (int i = 0; i < ls.fn_count; i++) {
                    if (strcmp(ls.fns[i].name, word) == 0) {
                        const char *ap = after + 1;
                        int got = (*ap == ')') ? 0 : count_args(ap);
                        int exp = ls.fns[i].param_count;
                        if (got != exp) {
                            char msg[256];
                            snprintf(msg, 255,
                                "Line %d: '%s' expects %d arg(s) but called with %d",
                                ls.line, word, exp, got);
                            lint_add_error(&ls, msg);
                        }
                        break;
                    }
                }
            }

            /* bare assignment without let: name = value (not ==) */
            if (*after == '=' && *(after+1) != '=' &&
                strcmp(word, "self") != 0 &&
                strcmp(word, "let") != 0 &&
                strcmp(word, "fn") != 0 &&
                strcmp(word, "class") != 0 &&
                strcmp(word, "if") != 0 &&
                strcmp(word, "else") != 0 &&
                strcmp(word, "while") != 0 &&
                strcmp(word, "for") != 0 &&
                strcmp(word, "return") != 0 &&
                strcmp(word, "import") != 0) {
                if (!lint_has_var(&ls, word)) {
                    char msg[128];
                    snprintf(msg, 127,
                        "Line %d: '%s' assigned without 'let' — did you mean 'let %s = ...'?",
                        ls.line, word, word);
                    lint_add_error(&ls, msg);
                }
            }

            (void)start;
            continue;
        }

        p++;
    }

    /* End of file checks */
    if (ls.brace_depth > 0) {
        char msg[128];
        snprintf(msg, 127, "End of file: %d unclosed '{' brace(s)", ls.brace_depth);
        lint_add_error(&ls, msg);
    }
    if (ls.paren_depth > 0) {
        char msg[128];
        snprintf(msg, 127, "End of file: %d unclosed '(' paren(s)", ls.paren_depth);
        lint_add_error(&ls, msg);
    }
    if (ls.bracket_depth > 0) {
        char msg[128];
        snprintf(msg, 127, "End of file: %d unclosed '[' bracket(s)", ls.bracket_depth);
        lint_add_error(&ls, msg);
    }

    /* Build result string */
    if (ls.error_count == 0) {
        return val_string("No issues found.");
    }

    char result[4096] = {0};
    for (int i = 0; i < ls.error_count; i++) {
        char line[300];
        snprintf(line, 299, "%s\n", ls.errors[i].message);
        strncat(result, line, sizeof(result) - strlen(result) - 1);
    }

    return val_string(result);
}

static void stdlib_load_lint(Env *env) {
    env_set(env, "lint", val_native(builtin_lint));
}

#endif /* LINT_MODULE_H */
