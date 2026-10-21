/*
 * ast.c  –  Arithmetic Expression Compiler (Front End)
 *
 * Implementation of AST node allocation, freeing, printing, and
 * evaluation helpers.
 */

#include "ast.h"

#include <math.h>     /* fabs           */
#include <stdio.h>    /* printf         */
#include <stdlib.h>   /* malloc, free, exit */
#include <string.h>   /* strncpy, snprintf  */

/* ------------------------------------------------------------------ */
/*  Node allocation                                                     */
/* ------------------------------------------------------------------ */

static AstNode *ast_alloc(void) {
    AstNode *n = (AstNode *)malloc(sizeof(AstNode));
    if (!n) { fprintf(stderr, "Out of memory in ast_alloc\n"); exit(1); }
    n->left  = NULL;
    n->right = NULL;
    n->op    = 0;
    n->line  = 0;
    n->col   = 0;
    return n;
}

AstNode *ast_new_num(const char *text, double val, int line, int col) {
    AstNode *n  = ast_alloc();
    n->kind     = AST_NUM;
    n->num_val  = val;
    n->line     = line;
    n->col      = col;
    strncpy(n->num_text, text, sizeof(n->num_text) - 1);
    n->num_text[sizeof(n->num_text) - 1] = '\0';
    return n;
}

AstNode *ast_new_id(const char *name, int line, int col) {
    AstNode *n = ast_alloc();
    n->kind    = AST_ID;
    n->line    = line;
    n->col     = col;
    strncpy(n->id_name, name, sizeof(n->id_name) - 1);
    n->id_name[sizeof(n->id_name) - 1] = '\0';
    return n;
}

AstNode *ast_new_binop(char op, AstNode *left, AstNode *right,
                        int line, int col) {
    AstNode *n = ast_alloc();
    n->kind    = AST_BINOP;
    n->op      = op;
    n->left    = left;
    n->right   = right;
    n->line    = line;
    n->col     = col;
    return n;
}

AstNode *ast_new_unary(char op, AstNode *operand, int line, int col) {
    AstNode *n = ast_alloc();
    n->kind    = AST_UNARY;
    n->op      = op;
    n->left    = operand;   /* operand stored in left child             */
    n->right   = NULL;
    n->line    = line;
    n->col     = col;
    return n;
}

/* ------------------------------------------------------------------ */
/*  ast_free() – post-order recursive deallocation                      */
/* ------------------------------------------------------------------ */

void ast_free(AstNode *node) {
    if (!node) return;
    ast_free(node->left);
    ast_free(node->right);
    free(node);
}

/* ------------------------------------------------------------------ */
/*  ast_print() – indented tree dump                                    */
/* ------------------------------------------------------------------ */

void ast_print(const AstNode *node, int indent) {
    if (!node) return;

    /* Print leading spaces (2 spaces per indent level) */
    for (int i = 0; i < indent; i++) printf("  ");

    switch (node->kind) {
        case AST_NUM:
            printf("NUM(%s)\n", node->num_text);
            break;
        case AST_ID:
            printf("ID(%s)\n", node->id_name);
            break;
        case AST_BINOP:
            printf("BINOP('%c')\n", node->op);
            ast_print(node->left,  indent + 1);
            ast_print(node->right, indent + 1);
            break;
        case AST_UNARY:
            printf("UNARY('%c')\n", node->op);
            ast_print(node->left, indent + 1);
            break;
    }
}

/* ------------------------------------------------------------------ */
/*  ast_to_parens() – fully parenthesized form                          */
/* ------------------------------------------------------------------ */

/* Safe append into a fixed buffer.
   `pos` is a pointer to the current write position within buf. */
static void buf_append(char *buf, size_t size, size_t *pos, const char *s) {
    while (*s && *pos < size - 1)
        buf[(*pos)++] = *s++;
    buf[*pos] = '\0';
}

static void to_parens_r(const AstNode *node, char *buf, size_t size, size_t *pos) {
    if (!node) return;

    char tmp[128];
    switch (node->kind) {
        case AST_NUM:
            buf_append(buf, size, pos, node->num_text);
            break;
        case AST_ID:
            buf_append(buf, size, pos, node->id_name);
            break;
        case AST_BINOP:
            buf_append(buf, size, pos, "(");
            to_parens_r(node->left,  buf, size, pos);
            snprintf(tmp, sizeof(tmp), "%c", node->op);
            buf_append(buf, size, pos, tmp);
            to_parens_r(node->right, buf, size, pos);
            buf_append(buf, size, pos, ")");
            break;
        case AST_UNARY:
            buf_append(buf, size, pos, "(-");
            to_parens_r(node->left, buf, size, pos);
            buf_append(buf, size, pos, ")");
            break;
    }
}

void ast_to_parens(const AstNode *node, char *buf, size_t size) {
    size_t pos = 0;
    buf[0] = '\0';
    to_parens_r(node, buf, size, &pos);
}

/* ------------------------------------------------------------------ */
/*  ast_to_postfix() – RPN form                                         */
/* ------------------------------------------------------------------ */

static void to_postfix_r(const AstNode *node, char *buf, size_t size, size_t *pos) {
    if (!node) return;

    char tmp[128];
    switch (node->kind) {
        case AST_NUM:
            if (*pos > 0) buf_append(buf, size, pos, " ");
            buf_append(buf, size, pos, node->num_text);
            break;
        case AST_ID:
            if (*pos > 0) buf_append(buf, size, pos, " ");
            buf_append(buf, size, pos, node->id_name);
            break;
        case AST_BINOP:
            to_postfix_r(node->left,  buf, size, pos);
            to_postfix_r(node->right, buf, size, pos);
            snprintf(tmp, sizeof(tmp), " %c", node->op);
            buf_append(buf, size, pos, tmp);
            break;
        case AST_UNARY:
            to_postfix_r(node->left, buf, size, pos);
            buf_append(buf, size, pos, " NEG");
            break;
    }
}

void ast_to_postfix(const AstNode *node, char *buf, size_t size) {
    size_t pos = 0;
    buf[0] = '\0';
    to_postfix_r(node, buf, size, &pos);
}

/* ------------------------------------------------------------------ */
/*  ast_is_const() / ast_eval()                                         */
/* ------------------------------------------------------------------ */

int ast_is_const(const AstNode *node) {
    if (!node) return 1;
    switch (node->kind) {
        case AST_NUM:   return 1;
        case AST_ID:    return 0;
        case AST_BINOP: return ast_is_const(node->left) &&
                               ast_is_const(node->right);
        case AST_UNARY: return ast_is_const(node->left);
    }
    return 0;
}

double ast_eval(const AstNode *node, int *ok) {
    if (!node || !ok) { if (ok) *ok = 0; return 0.0; }

    switch (node->kind) {
        case AST_NUM:
            *ok = 1;
            return node->num_val;

        case AST_ID:
            *ok = 0;
            return 0.0;

        case AST_UNARY: {
            double v = ast_eval(node->left, ok);
            if (!*ok) return 0.0;
            return -v;
        }

        case AST_BINOP: {
            double l = ast_eval(node->left,  ok);
            if (!*ok) return 0.0;
            double r = ast_eval(node->right, ok);
            if (!*ok) return 0.0;

            switch (node->op) {
                case '+': return l + r;
                case '-': return l - r;
                case '*': return l * r;
                case '/':
                    if (fabs(r) < 1e-300) { *ok = 0; return 0.0; }
                    return l / r;
                default:  *ok = 0; return 0.0;
            }
        }
    }
    *ok = 0;
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  ast_to_json()  –  recursive AST → JSON serialiser                  */
/* ------------------------------------------------------------------ */

/*
 * Minimal JSON string escaping helper.
 * Writes a JSON-escaped version of `s` (including the surrounding
 * double-quotes) into the buffer at *pos.
 */
static void json_str(char *buf, size_t size, size_t *pos, const char *s) {
    /* opening quote */
    if (*pos < size - 1) buf[(*pos)++] = '"';

    for (; *s; s++) {
        const char *esc = NULL;
        char tmp[7];
        switch (*s) {
            case '"':  esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\n': esc = "\\n";  break;
            case '\r': esc = "\\r";  break;
            case '\t': esc = "\\t";  break;
            default:
                if ((unsigned char)*s < 0x20) {
                    /* control character → \uXXXX */
                    snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned char)*s);
                    esc = tmp;
                }
        }
        if (esc) {
            buf_append(buf, size, pos, esc);
        } else {
            if (*pos < size - 1) buf[(*pos)++] = *s;
        }
    }
    /* closing quote */
    if (*pos < size - 1) buf[(*pos)++] = '"';
    buf[*pos] = '\0';
}

/* Append an integer as a JSON number. */
static void json_int(char *buf, size_t size, size_t *pos, int n) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%d", n);
    buf_append(buf, size, pos, tmp);
}

/* Recursive traversal */
static void ast_json_r(const AstNode *node, char *buf, size_t size, size_t *pos) {
    if (!node) { buf_append(buf, size, pos, "null"); return; }

    buf_append(buf, size, pos, "{");

    switch (node->kind) {
        case AST_NUM:
            buf_append(buf, size, pos, "\"type\":\"NUM\",\"value\":");
            json_str(buf, size, pos, node->num_text);
            break;

        case AST_ID:
            buf_append(buf, size, pos, "\"type\":\"ID\",\"name\":");
            json_str(buf, size, pos, node->id_name);
            break;

        case AST_BINOP: {
            char op_s[3] = { node->op, '\0' };
            buf_append(buf, size, pos, "\"type\":\"BINOP\",\"op\":");
            json_str(buf, size, pos, op_s);
            buf_append(buf, size, pos, ",\"left\":");
            ast_json_r(node->left, buf, size, pos);
            buf_append(buf, size, pos, ",\"right\":");
            ast_json_r(node->right, buf, size, pos);
            break;
        }

        case AST_UNARY: {
            char op_s[3] = { node->op, '\0' };
            buf_append(buf, size, pos, "\"type\":\"UNARY\",\"op\":");
            json_str(buf, size, pos, op_s);
            buf_append(buf, size, pos, ",\"operand\":");
            ast_json_r(node->left, buf, size, pos);
            break;
        }
    }

    /* Append source position for every node */
    buf_append(buf, size, pos, ",\"line\":");
    json_int(buf, size, pos, node->line);
    buf_append(buf, size, pos, ",\"col\":");
    json_int(buf, size, pos, node->col);

    buf_append(buf, size, pos, "}");
}

void ast_to_json(const AstNode *node, char *buf, size_t size) {
    size_t pos = 0;
    buf[0] = '\0';
    ast_json_r(node, buf, size, &pos);
}
