/* ─────────────────────────────────────────────────────────────────
   parser.c  —  Recursive Descent Parser + AST utilities
   
   OPERATOR PRECEDENCE (lowest → highest, like real languages):
   
     or                      (lowest)
     and
     not
     == != < > <= >=
     + -
     * / %
     unary:  -x
     primary: literals, identifiers, (grouped), calls    (highest)
   
   The trick: lower-precedence rules call higher-precedence rules.
   This makes the tree naturally encode precedence — 
   deeper in the tree = binds tighter.
   ───────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

/* ─────────────────────────────────────────────────────────────────
   INTERNAL HELPERS
   ───────────────────────────────────────────────────────────────── */

/* What token are we currently looking at? */
static Token cur(Parser *p) {
    if (p->pos >= p->count) {
        Token eof = {TOK_EOF, "EOF", 0, 0};
        return eof;
    }
    return p->tokens[p->pos];
}

/* One token ahead (without consuming) */
static Token peek_next(Parser *p) {
    if (p->pos + 1 >= p->count) {
        Token eof = {TOK_EOF, "EOF", 0, 0};
        return eof;
    }
    return p->tokens[p->pos + 1];
}

/* Move to the next token */
static Token advance(Parser *p) {
    Token t = cur(p);
    if (p->pos < p->count) p->pos++;
    return t;
}

/* Check type without consuming */
static int check(Parser *p, TokenType type) {
    return cur(p).type == type;
}

/* Check keyword value without consuming */
static int check_kw(Parser *p, TokenType type, const char *val) {
    return cur(p).type == type && strcmp(cur(p).value, val) == 0;
}

/* Consume if it matches, else set error */
static Token expect(Parser *p, TokenType type, const char *what) {
    if (cur(p).type == type) return advance(p);
    snprintf(p->errmsg, sizeof(p->errmsg),
             "Line %d: expected %s but got '%s'",
             cur(p).line, what, cur(p).value);
    p->error = 1;
    return cur(p);
}

/* Skip all newline tokens (they don't matter between statements) */
static void skip_newlines(Parser *p) {
    while (cur(p).type == TOK_NEWLINE) advance(p);
}

/* If the current token matches, consume and return 1. Else return 0. */
static int match(Parser *p, TokenType type) {
    if (check(p, type)) { advance(p); return 1; }
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
   FORWARD DECLARATIONS
   (functions call each other, so we need to declare them first)
   ───────────────────────────────────────────────────────────────── */
static Node *parse_stmt(Parser *p);
static Node *parse_block(Parser *p);
static Node *parse_expr(Parser *p);
static Node *parse_or(Parser *p);
static Node *parse_and(Parser *p);
static Node *parse_not(Parser *p);
static Node *parse_comparison(Parser *p);
static Node *parse_addition(Parser *p);
static Node *parse_multiplication(Parser *p);
static Node *parse_unary(Parser *p);
static Node *parse_call_or_primary(Parser *p);
static Node *parse_primary(Parser *p);

/* ─────────────────────────────────────────────────────────────────
   STATEMENT PARSERS
   ───────────────────────────────────────────────────────────────── */

/*
   parse_let — let x = expr
   
   Consumes: LET IDENT EQ expr
*/
static Node *parse_let(Parser *p) {
    Token kw = advance(p);  /* consume 'let' */
    Token name = expect(p, TOK_IDENT, "variable name");
    expect(p, TOK_EQ, "'='");
    Node *value = parse_expr(p);
    if (p->error) return NULL;

    Node *n = node_alloc(NODE_LET, kw.line, kw.col);
    strncpy(n->as.let_stmt.name, name.value, MAX_NAME - 1);
    n->as.let_stmt.value = value;
    return n;
}

/*
   parse_print — print expr
*/
static Node *parse_print(Parser *p) {
    Token kw = advance(p);  /* consume 'print' */
    Node *value = parse_expr(p);
    if (p->error) return NULL;

    Node *n = node_alloc(NODE_PRINT, kw.line, kw.col);
    n->as.print_stmt.value = value;
    return n;
}

/*
   parse_return — return expr
*/
static Node *parse_return(Parser *p) {
    Token kw = advance(p);  /* consume 'return' */

    /* 'return' with no value → return null */
    Node *value = NULL;
    if (!check(p, TOK_NEWLINE) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF))
        value = parse_expr(p);
    if (p->error) return NULL;

    Node *n = node_alloc(NODE_RETURN, kw.line, kw.col);
    n->as.return_stmt.value = value;
    return n;
}

/*
   parse_if — if expr { block } [else { block }]
                                [else if expr { block }]
*/
static Node *parse_if(Parser *p) {
    Token kw = advance(p);  /* consume 'if' */
    Node *cond = parse_expr(p);
    if (p->error) return NULL;
    Node *then_block = parse_block(p);
    if (p->error) return NULL;

    Node *else_block = NULL;
    skip_newlines(p);
    if (check(p, TOK_ELSE)) {
        advance(p);  /* consume 'else' */
        skip_newlines(p);
        if (check(p, TOK_IF)) {
            /* else if → recursively parse another if */
            else_block = parse_if(p);
        } else {
            else_block = parse_block(p);
        }
        if (p->error) return NULL;
    }

    Node *n = node_alloc(NODE_IF, kw.line, kw.col);
    n->as.if_stmt.cond       = cond;
    n->as.if_stmt.then_block = then_block;
    n->as.if_stmt.else_block = else_block;
    return n;
}

/*
   parse_while — while expr { block }
*/
static Node *parse_while(Parser *p) {
    Token kw = advance(p);  /* consume 'while' */
    Node *cond = parse_expr(p);
    if (p->error) return NULL;
    Node *body = parse_block(p);
    if (p->error) return NULL;

    Node *n = node_alloc(NODE_WHILE, kw.line, kw.col);
    n->as.while_stmt.cond = cond;
    n->as.while_stmt.body = body;
    return n;
}

/*
   parse_for — for let i = 0; i < 10; i = i + 1 { block }
*/
static Node *parse_for(Parser *p) {
    Token kw = advance(p);  /* consume 'for' */

    Node *init = parse_stmt(p);
    expect(p, TOK_SEMICOLON, "';'");
    Node *cond = parse_expr(p);
    expect(p, TOK_SEMICOLON, "';'");
    Node *step = parse_stmt(p);  /* step is a statement e.g. i = i + 1 */
    if (p->error) return NULL;
    Node *body = parse_block(p);
    if (p->error) return NULL;

    Node *n = node_alloc(NODE_FOR, kw.line, kw.col);
    n->as.for_stmt.init = init;
    n->as.for_stmt.cond = cond;
    n->as.for_stmt.step = step;
    n->as.for_stmt.body = body;
    return n;
}

/*
   parse_fn_def — fn name(param, param) { block }
*/
static Node *parse_fn_def(Parser *p) {
    Token kw = advance(p);  /* consume 'fn' */
    Token name = expect(p, TOK_IDENT, "function name");
    expect(p, TOK_LPAREN, "'('");

    char params[MAX_PARAMS][MAX_NAME];
    int  param_count = 0;

    if (!check(p, TOK_RPAREN)) {
        Token param = expect(p, TOK_IDENT, "parameter name");
        strncpy(params[param_count++], param.value, MAX_NAME - 1);

        while (match(p, TOK_COMMA)) {
            if (param_count >= MAX_PARAMS) {
                snprintf(p->errmsg, sizeof(p->errmsg),
                         "Too many parameters (max %d)", MAX_PARAMS);
                p->error = 1;
                return NULL;
            }
            Token pm = expect(p, TOK_IDENT, "parameter name");
            strncpy(params[param_count++], pm.value, MAX_NAME - 1);
        }
    }

    expect(p, TOK_RPAREN, "')'");
    if (p->error) return NULL;
    Node *body = parse_block(p);
    if (p->error) return NULL;

    Node *n = node_alloc(NODE_FN_DEF, kw.line, kw.col);
    strncpy(n->as.fn_def.name, name.value, MAX_NAME - 1);
    n->as.fn_def.param_count = param_count;
    for (int i = 0; i < param_count; i++)
        strncpy(n->as.fn_def.params[i], params[i], MAX_NAME - 1);
    n->as.fn_def.body = body;
    return n;
}

/*
   parse_block — { stmt stmt stmt ... }
   
   A block is a { } delimited sequence of statements.
   Newlines between statements are ignored.
*/

/*
   parse_import — import "module"
*/
static Node *parse_import(Parser *p) {
    Token kw = advance(p);  /* consume 'import' */
    Token mod = expect(p, TOK_STRING, "module name string");
    if (p->error) return NULL;
    Node *n = node_alloc(NODE_IMPORT, kw.line, kw.col);
    strncpy(n->as.import_stmt.module, mod.value, MAX_NAME - 1);
    return n;
}

static Node *parse_block(Parser *p) {
    Token brace = expect(p, TOK_LBRACE, "'{'");
    if (p->error) return NULL;

    Node *block = node_alloc(NODE_BLOCK, brace.line, brace.col);
    block->as.block.count = 0;

    skip_newlines(p);
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        Node *stmt = parse_stmt(p);
        if (p->error) return NULL;
        if (stmt && block->as.block.count < MAX_STMTS)
            block->as.block.stmts[block->as.block.count++] = stmt;
        skip_newlines(p);
    }

    expect(p, TOK_RBRACE, "'}'");
    return block;
}

/*
   parse_stmt — dispatches to the right statement parser
*/
static Node *parse_stmt(Parser *p) {
    skip_newlines(p);

    /* keywords */
    if (check(p, TOK_IMPORT)) return parse_import(p);
    if (check(p, TOK_LET))    return parse_let(p);
    if (check(p, TOK_PRINT))  return parse_print(p);
    if (check(p, TOK_RETURN)) return parse_return(p);
    if (check(p, TOK_IF))     return parse_if(p);
    if (check(p, TOK_WHILE))  return parse_while(p);
    if (check(p, TOK_FOR))    return parse_for(p);
    if (check(p, TOK_FN))     return parse_fn_def(p);

    /* assignment: x = expr  (IDENT followed by =, NOT ==) */
    if (check(p, TOK_IDENT) && peek_next(p).type == TOK_EQ) {
        Token name = advance(p);  /* consume IDENT */
        advance(p);               /* consume = */
        Node *value = parse_expr(p);
        if (p->error) return NULL;
        Node *n = node_alloc(NODE_ASSIGN, name.line, name.col);
        strncpy(n->as.assign.name, name.value, MAX_NAME - 1);
        n->as.assign.value = value;
        return n;
    }

    /* expression statement (e.g. a function call on its own line) */
    return parse_expr(p);
}

/* ─────────────────────────────────────────────────────────────────
   EXPRESSION PARSERS  (ordered by precedence, lowest first)
   
   This is the most important part of the parser.
   Each level calls the next-higher level, building the tree
   such that higher-precedence operators end up deeper.
   ───────────────────────────────────────────────────────────────── */

static Node *parse_expr(Parser *p) {
    return parse_or(p);
}

/* or — lowest precedence binary op */
static Node *parse_or(Parser *p) {
    Node *left = parse_and(p);
    while (!p->error && check(p, TOK_OR)) {
        Token op = advance(p);
        Node *right = parse_and(p);
        Node *n = node_alloc(NODE_BINOP, op.line, op.col);
        n->as.binop.left  = left;
        n->as.binop.right = right;
        strncpy(n->as.binop.op, "or", 7);
        left = n;
    }
    return left;
}

static Node *parse_and(Parser *p) {
    Node *left = parse_not(p);
    while (!p->error && check(p, TOK_AND)) {
        Token op = advance(p);
        Node *right = parse_not(p);
        Node *n = node_alloc(NODE_BINOP, op.line, op.col);
        n->as.binop.left  = left;
        n->as.binop.right = right;
        strncpy(n->as.binop.op, "and", 7);
        left = n;
    }
    return left;
}

static Node *parse_not(Parser *p) {
    if (check(p, TOK_NOT)) {
        Token op = advance(p);
        Node *operand = parse_not(p);
        Node *n = node_alloc(NODE_UNARY, op.line, op.col);
        n->as.unary.operand = operand;
        strncpy(n->as.unary.op, "not", 7);
        return n;
    }
    return parse_comparison(p);
}

static Node *parse_comparison(Parser *p) {
    Node *left = parse_addition(p);
    while (!p->error) {
        const char *op_str = NULL;
        if      (check(p, TOK_EQEQ))  op_str = "==";
        else if (check(p, TOK_BANGEQ)) op_str = "!=";
        else if (check(p, TOK_LT))     op_str = "<";
        else if (check(p, TOK_GT))     op_str = ">";
        else if (check(p, TOK_LTEQ))   op_str = "<=";
        else if (check(p, TOK_GTEQ))   op_str = ">=";
        else break;

        Token op = advance(p);
        Node *right = parse_addition(p);
        if (p->error) break;
        Node *n = node_alloc(NODE_BINOP, op.line, op.col);
        n->as.binop.left  = left;
        n->as.binop.right = right;
        strncpy(n->as.binop.op, op_str, 7);
        left = n;
    }
    return left;
}

static Node *parse_addition(Parser *p) {
    Node *left = parse_multiplication(p);
    while (!p->error && (check(p, TOK_PLUS) || check(p, TOK_MINUS))) {
        Token op = advance(p);
        Node *right = parse_multiplication(p);
        if (p->error) break;
        Node *n = node_alloc(NODE_BINOP, op.line, op.col);
        n->as.binop.left  = left;
        n->as.binop.right = right;
        n->as.binop.op[0] = op.value[0];
        n->as.binop.op[1] = '\0';
        left = n;
    }
    return left;
}

static Node *parse_multiplication(Parser *p) {
    Node *left = parse_unary(p);
    while (!p->error &&
           (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT))) {
        Token op = advance(p);
        Node *right = parse_unary(p);
        if (p->error) break;
        Node *n = node_alloc(NODE_BINOP, op.line, op.col);
        n->as.binop.left  = left;
        n->as.binop.right = right;
        n->as.binop.op[0] = op.value[0];
        n->as.binop.op[1] = '\0';
        left = n;
    }
    return left;
}

static Node *parse_unary(Parser *p) {
    if (check(p, TOK_MINUS)) {
        Token op = advance(p);
        Node *operand = parse_unary(p);
        Node *n = node_alloc(NODE_UNARY, op.line, op.col);
        n->as.unary.operand = operand;
        strncpy(n->as.unary.op, "-", 7);
        return n;
    }
    return parse_call_or_primary(p);
}

/*
   parse_call_or_primary
   
   If we see IDENT followed by '(' it's a function call.
   Otherwise fall through to primary.
*/
static Node *parse_call_or_primary(Parser *p) {
    if (check(p, TOK_IDENT) && peek_next(p).type == TOK_LPAREN) {
        Token name = advance(p);  /* consume function name */
        advance(p);               /* consume '(' */

        Node *call = node_alloc(NODE_CALL, name.line, name.col);
        strncpy(call->as.call.name, name.value, MAX_NAME - 1);
        call->as.call.arg_count = 0;

        if (!check(p, TOK_RPAREN)) {
            call->as.call.args[call->as.call.arg_count++] = parse_expr(p);
            while (!p->error && match(p, TOK_COMMA)) {
                if (call->as.call.arg_count >= MAX_ARGS) {
                    snprintf(p->errmsg, sizeof(p->errmsg),
                             "Too many arguments (max %d)", MAX_ARGS);
                    p->error = 1;
                    return NULL;
                }
                call->as.call.args[call->as.call.arg_count++] = parse_expr(p);
            }
        }
        expect(p, TOK_RPAREN, "')'");
        return call;
    }
    return parse_primary(p);
}

/*
   parse_primary — the highest-precedence level.
   
   Handles: literals, identifiers, and grouped expressions (...)
*/
static Node *parse_primary(Parser *p) {
    Token t = cur(p);

    /* Integer literal */
    if (t.type == TOK_INT) {
        advance(p);
        Node *n = node_alloc(NODE_INT, t.line, t.col);
        n->as.int_lit.value = atoll(t.value);
        return n;
    }

    /* Float literal */
    if (t.type == TOK_FLOAT) {
        advance(p);
        Node *n = node_alloc(NODE_FLOAT, t.line, t.col);
        n->as.float_lit.value = atof(t.value);
        return n;
    }

    /* String literal */
    if (t.type == TOK_STRING) {
        advance(p);
        Node *n = node_alloc(NODE_STRING, t.line, t.col);
        strncpy(n->as.str_lit.value, t.value, 511);
        return n;
    }

    /* true / false */
    if (t.type == TOK_TRUE) {
        advance(p);
        Node *n = node_alloc(NODE_BOOL, t.line, t.col);
        n->as.bool_lit.value = 1;
        return n;
    }
    if (t.type == TOK_FALSE) {
        advance(p);
        Node *n = node_alloc(NODE_BOOL, t.line, t.col);
        n->as.bool_lit.value = 0;
        return n;
    }

    /* null */
    if (t.type == TOK_NULL) {
        advance(p);
        return node_alloc(NODE_NULL, t.line, t.col);
    }

    /* Identifier (variable read) */
    if (t.type == TOK_IDENT) {
        advance(p);
        Node *n = node_alloc(NODE_IDENT, t.line, t.col);
        strncpy(n->as.ident.name, t.value, MAX_NAME - 1);
        return n;
    }

    /* Grouped expression: ( expr ) */
    if (t.type == TOK_LPAREN) {
        advance(p);  /* consume ( */
        Node *inner = parse_expr(p);
        expect(p, TOK_RPAREN, "')'");
        return inner;
    }

    snprintf(p->errmsg, sizeof(p->errmsg),
             "Line %d: unexpected token '%s'", t.line, t.value);
    p->error = 1;
    return NULL;
}

/* ─────────────────────────────────────────────────────────────────
   PUBLIC API
   ───────────────────────────────────────────────────────────────── */

void parser_init(Parser *p, Token *tokens, int count) {
    p->tokens = tokens;
    p->count  = count;
    p->pos    = 0;
    p->error  = 0;
    p->errmsg[0] = '\0';
}

Node *parser_parse(Parser *p) {
    Node *root = node_alloc(NODE_BLOCK, 1, 1);
    root->as.block.count = 0;

    skip_newlines(p);
    while (!p->error && !check(p, TOK_EOF)) {
        Node *stmt = parse_stmt(p);
        if (p->error) break;
        if (stmt && root->as.block.count < MAX_STMTS)
            root->as.block.stmts[root->as.block.count++] = stmt;
        skip_newlines(p);
    }
    return root;
}

/* Convenience: lex + parse in one call */
Node *parse_source(const char *src) {
    static Token tokens[MAX_TOKENS];
    Lexer l;
    lexer_init(&l, src);
    int count = lexer_tokenize_all(&l, tokens, MAX_TOKENS);

    static Parser p;
    parser_init(&p, tokens, count);
    Node *tree = parser_parse(&p);

    if (p.error) {
        fprintf(stderr, "Parse error: %s\n", p.errmsg);
        ast_free(tree);
        return NULL;
    }
    return tree;
}

/* ─────────────────────────────────────────────────────────────────
   AST UTILITIES  (ast_print + ast_free)
   ───────────────────────────────────────────────────────────────── */

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

void ast_print(Node *n, int indent) {
    if (!n) { print_indent(indent); printf("(null)\n"); return; }

    print_indent(indent);
    switch (n->type) {
        case NODE_INT:
            printf("Int(%lld)\n", n->as.int_lit.value); break;
        case NODE_FLOAT:
            printf("Float(%g)\n", n->as.float_lit.value); break;
        case NODE_STRING:
            printf("String(\"%s\")\n", n->as.str_lit.value); break;
        case NODE_BOOL:
            printf("Bool(%s)\n", n->as.bool_lit.value ? "true" : "false"); break;
        case NODE_NULL:
            printf("Null\n"); break;
        case NODE_IDENT:
            printf("Ident(%s)\n", n->as.ident.name); break;
        case NODE_LET:
            printf("Let(%s)\n", n->as.let_stmt.name);
            ast_print(n->as.let_stmt.value, indent + 1);
            break;
        case NODE_ASSIGN:
            printf("Assign(%s)\n", n->as.assign.name);
            ast_print(n->as.assign.value, indent + 1);
            break;
        case NODE_BINOP:
            printf("BinOp(%s)\n", n->as.binop.op);
            ast_print(n->as.binop.left,  indent + 1);
            ast_print(n->as.binop.right, indent + 1);
            break;
        case NODE_UNARY:
            printf("Unary(%s)\n", n->as.unary.op);
            ast_print(n->as.unary.operand, indent + 1);
            break;
        case NODE_CALL:
            printf("Call(%s, %d args)\n", n->as.call.name, n->as.call.arg_count);
            for (int i = 0; i < n->as.call.arg_count; i++)
                ast_print(n->as.call.args[i], indent + 1);
            break;
        case NODE_IMPORT:
            printf("Import(\"%s\")\n", n->as.import_stmt.module); break;
        case NODE_PRINT:
            printf("Print\n");
            ast_print(n->as.print_stmt.value, indent + 1);
            break;
        case NODE_RETURN:
            printf("Return\n");
            if (n->as.return_stmt.value)
                ast_print(n->as.return_stmt.value, indent + 1);
            break;
        case NODE_BLOCK:
            printf("Block (%d stmts)\n", n->as.block.count);
            for (int i = 0; i < n->as.block.count; i++)
                ast_print(n->as.block.stmts[i], indent + 1);
            break;
        case NODE_IF:
            printf("If\n");
            print_indent(indent + 1); printf("cond:\n");
            ast_print(n->as.if_stmt.cond,       indent + 2);
            print_indent(indent + 1); printf("then:\n");
            ast_print(n->as.if_stmt.then_block,  indent + 2);
            if (n->as.if_stmt.else_block) {
                print_indent(indent + 1); printf("else:\n");
                ast_print(n->as.if_stmt.else_block, indent + 2);
            }
            break;
        case NODE_WHILE:
            printf("While\n");
            print_indent(indent + 1); printf("cond:\n");
            ast_print(n->as.while_stmt.cond, indent + 2);
            print_indent(indent + 1); printf("body:\n");
            ast_print(n->as.while_stmt.body, indent + 2);
            break;
        case NODE_FOR:
            printf("For\n");
            print_indent(indent + 1); printf("init:\n");
            ast_print(n->as.for_stmt.init, indent + 2);
            print_indent(indent + 1); printf("cond:\n");
            ast_print(n->as.for_stmt.cond, indent + 2);
            print_indent(indent + 1); printf("step:\n");
            ast_print(n->as.for_stmt.step, indent + 2);
            print_indent(indent + 1); printf("body:\n");
            ast_print(n->as.for_stmt.body, indent + 2);
            break;
        case NODE_FN_DEF:
            printf("FnDef(%s, params:", n->as.fn_def.name);
            for (int i = 0; i < n->as.fn_def.param_count; i++)
                printf(" %s", n->as.fn_def.params[i]);
            printf(")\n");
            ast_print(n->as.fn_def.body, indent + 1);
            break;
        default:
            printf("Unknown node type %d\n", n->type);
    }
}

void ast_free(Node *n) {
    if (!n) return;
    switch (n->type) {
        case NODE_LET:    ast_free(n->as.let_stmt.value); break;
        case NODE_ASSIGN: ast_free(n->as.assign.value);   break;
        case NODE_BINOP:
            ast_free(n->as.binop.left);
            ast_free(n->as.binop.right);
            break;
        case NODE_UNARY:  ast_free(n->as.unary.operand); break;
        case NODE_CALL:
            for (int i = 0; i < n->as.call.arg_count; i++)
                ast_free(n->as.call.args[i]);
            break;
        case NODE_PRINT:  ast_free(n->as.print_stmt.value);  break;
        case NODE_RETURN: ast_free(n->as.return_stmt.value); break;
        case NODE_BLOCK:
            for (int i = 0; i < n->as.block.count; i++)
                ast_free(n->as.block.stmts[i]);
            break;
        case NODE_IF:
            ast_free(n->as.if_stmt.cond);
            ast_free(n->as.if_stmt.then_block);
            ast_free(n->as.if_stmt.else_block);
            break;
        case NODE_WHILE:
            ast_free(n->as.while_stmt.cond);
            ast_free(n->as.while_stmt.body);
            break;
        case NODE_FOR:
            ast_free(n->as.for_stmt.init);
            ast_free(n->as.for_stmt.cond);
            ast_free(n->as.for_stmt.step);
            ast_free(n->as.for_stmt.body);
            break;
        case NODE_FN_DEF: ast_free(n->as.fn_def.body); break;
        default: break;
    }
    free(n);
}
