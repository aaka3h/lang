#ifndef AST_H
#define AST_H

/* ─────────────────────────────────────────────────────────────────
   ast.h  —  Abstract Syntax Tree node definitions
   
   Every construct in your language becomes one of these node types.
   The parser builds a tree of these nodes.
   The interpreter (Phase 6) will walk this tree and execute it.
   
   Each node is a C struct. We use a tagged union pattern:
     - NodeType tag  tells you WHICH kind of node it is
     - A union holds the data for that specific kind
   ───────────────────────────────────────────────────────────────── */

#include <stdlib.h>

/* Forward declaration — nodes point to other nodes */
typedef struct Node Node;

/* ─────────────────────────────────────────────────────────────────
   Every possible kind of AST node
   ───────────────────────────────────────────────────────────────── */
typedef enum {

    /* ── Literals ──────────────────── */
    NODE_INT,       /* 42                     */
    NODE_FLOAT,     /* 3.14                   */
    NODE_STRING,    /* "hello"                */
    NODE_BOOL,      /* true / false           */
    NODE_NULL,      /* null                   */

    /* ── Variables ─────────────────── */
    NODE_IDENT,     /* x  (read a variable)   */
    NODE_LET,       /* let x = expr           */
    NODE_ASSIGN,    /* x = expr  (reassign)   */

    /* ── Expressions ───────────────── */
    NODE_BINOP,     /* a + b, a * b, a == b   */
    NODE_UNARY,     /* -x, not x              */
    NODE_CALL,      /* add(1, 2)              */
    NODE_ARRAY,     /* [1, 2, 3]              */
    NODE_INDEX,     /* a[0]                   */
    NODE_INDEX_ASSIGN, /* a[0] = 5            */
    NODE_DICT,      /* {"k": v, ...}          */
    NODE_TRY,       /* try { } catch (e) { }  */
    NODE_THROW,     /* throw expr              */
    NODE_CLASS_DEF, /* class Foo { fn ... }   */
    NODE_NEW,       /* Foo(args)  — same as call but creates instance */
    NODE_GET_ATTR,  /* obj.field               */
    NODE_SET_ATTR,  /* obj.field = val         */
    NODE_SELF,      /* self                    */

    /* ── Statements ────────────────── */
    NODE_IMPORT,    /* import "module"         */
    NODE_PRINT,     /* print expr             */
    NODE_RETURN,    /* return expr            */
    NODE_BLOCK,     /* { stmt; stmt; ... }    */

    /* ── Control flow ──────────────── */
    NODE_IF,        /* if cond { } else { }   */
    NODE_WHILE,     /* while cond { }         */
    NODE_FOR,       /* for i = 0; i<n; i=i+1 */

    /* ── Functions ─────────────────── */
    NODE_FN_DEF,    /* fn add(a, b) { }       */

} NodeType;

/* ─────────────────────────────────────────────────────────────────
   Max sizes
   ───────────────────────────────────────────────────────────────── */
#define MAX_PARAMS   16   /* max parameters in a function          */
#define MAX_ARGS     16   /* max arguments in a function call      */
#define MAX_STMTS   256   /* max statements in a block             */
#define MAX_NAME    256   /* max length of an identifier           */

/* ─────────────────────────────────────────────────────────────────
   The Node struct — a tagged union.
   
   'type' tells you which field of the union to read.
   Every node also stores line/col for error messages.
   ───────────────────────────────────────────────────────────────── */
struct Node {
    NodeType type;
    int      line;
    int      col;

    union {

        /* NODE_INT */
        struct { long long value; } int_lit;

        /* NODE_FLOAT */
        struct { double value; } float_lit;

        /* NODE_STRING */
        struct { char value[512]; } str_lit;

        /* NODE_BOOL */
        struct { int value; } bool_lit;   /* 1 = true, 0 = false */

        /* NODE_IDENT — just a name, e.g. "x" */
        struct { char name[MAX_NAME]; } ident;

        /* NODE_LET — let x = expr */
        struct {
            char  name[MAX_NAME];
            Node *value;            /* the expression on the right */
        } let_stmt;

        /* NODE_ASSIGN — x = expr (reassignment, no 'let') */
        struct {
            char  name[MAX_NAME];
            Node *value;
        } assign;

        /* NODE_BINOP — left OP right
           op is stored as a string: "+", "-", "*", "==", etc. */
        struct {
            Node *left;
            Node *right;
            char  op[8];
        } binop;

        /* NODE_UNARY — OP operand
           op: "-" or "not" */
        struct {
            Node *operand;
            char  op[8];
        } unary;

        /* NODE_CALL — name(arg0, arg1, ...) */
        struct {
            char  name[MAX_NAME];
            Node *args[MAX_ARGS];
            int   arg_count;
        } call;

        /* NODE_IMPORT */
        struct { char module[MAX_NAME]; } import_stmt;

        /* NODE_ARRAY */
        struct {
            Node *elements[256];
            int   count;
        } array;

        /* NODE_INDEX */
        struct {
            char  name[MAX_NAME];
            Node *index;
        } index_expr;

        /* NODE_INDEX_ASSIGN */
        struct {
            char  name[MAX_NAME];
            Node *index;
            Node *value;
        } index_assign;

        /* NODE_DICT */
        struct {
            char  keys[64][MAX_NAME];
            Node *values[64];
            int   count;
        } dict;

        /* NODE_TRY */
        struct {
            Node *try_block;
            char  err_name[MAX_NAME];
            Node *catch_block;
        } try_stmt;

        /* NODE_THROW */
        struct { Node *value; } throw_stmt;

        /* NODE_CLASS_DEF */
        struct {
            char  name[MAX_NAME];
            Node *methods[32];    /* each is a NODE_FN_DEF */
            int   method_count;
        } class_def;

        /* NODE_GET_ATTR — obj.field */
        struct {
            Node *object;
            char  field[MAX_NAME];
        } get_attr;

        /* NODE_SET_ATTR — obj.field = val */
        struct {
            Node *object;
            char  field[MAX_NAME];
            Node *value;
        } set_attr;

        /* NODE_PRINT — print expr */
        struct { Node *value; } print_stmt;

        /* NODE_RETURN — return expr */
        struct { Node *value; } return_stmt;

        /* NODE_BLOCK — a list of statements */
        struct {
            Node *stmts[MAX_STMTS];
            int   count;
        } block;

        /* NODE_IF — if cond { then } else { else_ } */
        struct {
            Node *cond;
            Node *then_block;
            Node *else_block;   /* NULL if no else */
        } if_stmt;

        /* NODE_WHILE — while cond { body } */
        struct {
            Node *cond;
            Node *body;
        } while_stmt;

        /* NODE_FOR — for init; cond; step { body } */
        struct {
            Node *init;   /* e.g. let i = 0     */
            Node *cond;   /* e.g. i < 10        */
            Node *step;   /* e.g. i = i + 1     */
            Node *body;
        } for_stmt;

        /* NODE_FN_DEF — fn name(p0, p1, ...) { body } */
        struct {
            char  name[MAX_NAME];
            char  params[MAX_PARAMS][MAX_NAME];
            int   param_count;
            Node *body;
        } fn_def;

    } as;
};

/* ─────────────────────────────────────────────────────────────────
   Memory allocation helpers.
   
   We allocate nodes on the heap with calloc (zero-initialised).
   In a real compiler you'd use an arena allocator for speed,
   but heap allocation is fine for learning.
   ───────────────────────────────────────────────────────────────── */
static inline Node* node_alloc(NodeType type, int line, int col) {
    Node *n = (Node*)calloc(1, sizeof(Node));
    if (!n) { fprintf(stderr, "Out of memory\n"); exit(1); }
    n->type = type;
    n->line = line;
    n->col  = col;
    return n;
}

/* Free an entire AST tree recursively */
void ast_free(Node *n);

/* Pretty-print an AST tree (for debugging) */
void ast_print(Node *n, int indent);

#endif /* AST_H */
