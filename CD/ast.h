/*
 * ast.h  –  Arithmetic Expression Compiler (Front End)
 *
 * Abstract Syntax Tree (AST) node definitions.
 *
 * Four node types:
 *   AST_NUM    – numeric literal  (stores the text value)
 *   AST_ID     – identifier       (stores the name)
 *   AST_BINOP  – binary operation (+, -, *, /)
 *   AST_UNARY  – unary minus      (single child)
 */

#ifndef AST_H
#define AST_H

#include <stddef.h>   /* size_t */

/* ------------------------------------------------------------------ */
/*  AST node kinds                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    AST_NUM,
    AST_ID,
    AST_BINOP,
    AST_UNARY
} AstKind;

/* ------------------------------------------------------------------ */
/*  AstNode (tagged-union style)                                        */
/* ------------------------------------------------------------------ */

typedef struct AstNode AstNode;

struct AstNode {
    AstKind kind;

    /* --- AST_NUM --------------------------------------------------- */
    double num_val;          /* parsed value (valid when kind==AST_NUM) */
    char   num_text[64];     /* original lexeme, e.g. "3.14"            */

    /* --- AST_ID ---------------------------------------------------- */
    char   id_name[256];     /* identifier name (valid when kind==AST_ID)*/

    /* --- AST_BINOP ------------------------------------------------- */
    char   op;               /* '+', '-', '*', '/'                       */
    AstNode *left;           /* left child  (BINOP and UNARY use this)   */
    AstNode *right;          /* right child (BINOP only)                 */

    /* --- AST_UNARY ------------------------------------------------- */
    /* op  = '-'                                                         */
    /* left = operand of unary minus                                     */

    /* Source position of the primary token for this node               */
    int line;
    int col;
};

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * ast_new_num(text, val, line, col)
 *   Allocate a numeric literal node.
 */
AstNode *ast_new_num(const char *text, double val, int line, int col);

/*
 * ast_new_id(name, line, col)
 *   Allocate an identifier node.
 */
AstNode *ast_new_id(const char *name, int line, int col);

/*
 * ast_new_binop(op, left, right, line, col)
 *   Allocate a binary-operator node.
 */
AstNode *ast_new_binop(char op, AstNode *left, AstNode *right,
                        int line, int col);

/*
 * ast_new_unary(op, operand, line, col)
 *   Allocate a unary-operator node (only '-' for now).
 */
AstNode *ast_new_unary(char op, AstNode *operand, int line, int col);

/*
 * ast_free(node)
 *   Recursively free an AST (post-order).
 *   Safe to call with NULL.
 */
void ast_free(AstNode *node);

/*
 * ast_print(node, indent)
 *   Print an indented tree representation to stdout.
 *   `indent` should be 0 on the first call; each level adds 2 spaces.
 *
 *   Example:
 *     BINOP '+'
 *       NUM 2
 *       BINOP '*'
 *         NUM 3
 *         NUM 4
 */
void ast_print(const AstNode *node, int indent);

/*
 * ast_to_parens(node, buf, size)
 *   Write the fully-parenthesized form of `node` into `buf`.
 *   `size` is the buffer capacity.
 *   Example: "2+3*4"  ->  "(2+(3*4))"
 */
void ast_to_parens(const AstNode *node, char *buf, size_t size);

/*
 * ast_to_postfix(node, buf, size)
 *   Write the postfix (RPN) form of `node` into `buf`.
 *   Example: "2+3*4"  ->  "2 3 4 * +"
 */
void ast_to_postfix(const AstNode *node, char *buf, size_t size);

/*
 * ast_is_const(node)
 *   Return 1 if every leaf in the tree is AST_NUM (no identifiers),
 *   0 otherwise.
 */
int ast_is_const(const AstNode *node);

/*
 * ast_eval(node, ok)
 *   Numerically evaluate a purely-constant AST.
 *   Sets *ok to 1 on success, 0 on division-by-zero or non-const node.
 */
double ast_eval(const AstNode *node, int *ok);

/*
 * ast_to_json(node, buf, size)
 *   Serialise the AST to a JSON object and write it into `buf`.
 *   Leaf nodes:
 *     {"type":"NUM","value":"3.14","line":1,"col":1}
 *     {"type":"ID","name":"x","line":1,"col":1}
 *   BINOP:
 *     {"type":"BINOP","op":"+","line":1,"col":2,"left":{...},"right":{...}}
 *   UNARY:
 *     {"type":"UNARY","op":"-","line":1,"col":1,"operand":{...}}
 */
void ast_to_json(const AstNode *node, char *buf, size_t size);

#endif /* AST_H */
