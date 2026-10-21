/*
 * parser.c  –  Arithmetic Expression Compiler (Front End)
 *
 * Recursive-descent parser. All diagnostic output now goes through
 * the errors.h collector (diag_syntax_error / diag_warning) instead
 * of fprintf(stderr, ...) so the same code can serve both the CLI
 * and the JSON API.
 *
 * Grammar (unchanged):
 *   expr   -> term   (('+' | '-') term)*
 *   term   -> factor (('*' | '/') factor)*
 *   factor -> NUMBER | IDENTIFIER | '(' expr ')' | '-' factor
 */

#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "errors.h"  /* diag_syntax_error(), diag_warning(), g_diag */

#include <math.h>     /* fabs    */
#include <stdio.h>    /* printf, sscanf */
#include <string.h>   /* strcmp, strncpy */

/* ------------------------------------------------------------------ */
/*  Internal parser state                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    const TokenList *tl;
    size_t           pos;
    int              had_error;
    int              had_warning;
    SymbolTable      syms;
} Parser;

/* ------------------------------------------------------------------ */
/*  Cursor helpers                                                      */
/* ------------------------------------------------------------------ */

static const Token *current(const Parser *p)  { return &p->tl->tokens[p->pos]; }
static TokenKind    cur_kind(const Parser *p)  { return current(p)->kind; }

static const Token *advance(Parser *p) {
    const Token *t = current(p);
    if (t->kind != TOK_EOF && p->pos + 1 < p->tl->count) p->pos++;
    return t;
}

static const Token *expect(Parser *p, TokenKind kind, const char *desc) {
    if (cur_kind(p) == kind) return advance(p);
    const Token *t = current(p);
    diag_syntax_error(g_diag, t->line, t->col,
        "expected %s, found '%s'", desc,
        t->kind == TOK_EOF ? "end of input" : t->lexeme);
    p->had_error = 1;
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Symbol table                                                        */
/* ------------------------------------------------------------------ */

static void sym_insert(SymbolTable *st, const char *name) {
    for (int i = 0; i < st->count; i++)
        if (strcmp(st->names[i], name) == 0) return;
    if (st->count < MAX_SYMBOLS) {
        strncpy(st->names[st->count], name, MAX_SYM_LEN - 1);
        st->names[st->count][MAX_SYM_LEN - 1] = '\0';
        st->count++;
    }
}

/* ------------------------------------------------------------------ */
/*  Forward declarations                                                */
/* ------------------------------------------------------------------ */

static AstNode *parse_expr  (Parser *p);
static AstNode *parse_term  (Parser *p);
static AstNode *parse_factor(Parser *p);

/* ------------------------------------------------------------------ */
/*  parse_factor                                                        */
/* ------------------------------------------------------------------ */

static AstNode *parse_factor(Parser *p) {
    const Token *t = current(p);

    /* NUMBER ------------------------------------------------------- */
    if (t->kind == TOK_NUM) {
        advance(p);
        double val = 0.0;
        sscanf(t->lexeme, "%lf", &val);
        return ast_new_num(t->lexeme, val, t->line, t->col);
    }

    /* IDENTIFIER --------------------------------------------------- */
    if (t->kind == TOK_ID) {
        advance(p);
        sym_insert(&p->syms, t->lexeme);
        return ast_new_id(t->lexeme, t->line, t->col);
    }

    /* Unary minus -------------------------------------------------- */
    if (t->kind == TOK_MINUS) {
        const Token *op = advance(p);
        const Token *nxt = current(p);

        if (nxt->kind == TOK_EOF) {
            diag_syntax_error(g_diag, op->line, op->col,
                "expected operand after unary '-', found end of input");
            p->had_error = 1;
            return NULL;
        }
        if (nxt->kind == TOK_PLUS  || nxt->kind == TOK_STAR  ||
            nxt->kind == TOK_SLASH || nxt->kind == TOK_RPAREN) {
            diag_syntax_error(g_diag, nxt->line, nxt->col,
                "expected operand after unary '-', found '%s'", nxt->lexeme);
            p->had_error = 1;
            return NULL;
        }

        AstNode *operand = parse_factor(p);
        if (!operand) return NULL;
        return ast_new_unary('-', operand, op->line, op->col);
    }

    /* Grouped expression ------------------------------------------- */
    if (t->kind == TOK_LPAREN) {
        const Token *lparen = advance(p);

        if (cur_kind(p) == TOK_RPAREN) {
            const Token *rp = current(p);
            diag_syntax_error(g_diag, rp->line, rp->col,
                "empty parentheses '()'");
            p->had_error = 1;
            advance(p);
            return NULL;
        }

        AstNode *inner = parse_expr(p);
        if (!inner) {
            while (cur_kind(p) != TOK_RPAREN && cur_kind(p) != TOK_EOF)
                advance(p);
            if (cur_kind(p) == TOK_RPAREN) advance(p);
            return NULL;
        }

        if (!expect(p, TOK_RPAREN, "')'")) {
            diag_syntax_error(g_diag, current(p)->line, current(p)->col,
                "unmatched '(' opened at %d:%d", lparen->line, lparen->col);
            ast_free(inner);
            return NULL;
        }
        return inner;
    }

    /* Error: unexpected token -------------------------------------- */
    if (t->kind == TOK_PLUS || t->kind == TOK_STAR || t->kind == TOK_SLASH) {
        diag_syntax_error(g_diag, t->line, t->col,
            "expected operand, found '%s'", t->lexeme);
    } else if (t->kind == TOK_RPAREN) {
        diag_syntax_error(g_diag, t->line, t->col,
            "unexpected ')' - unmatched closing parenthesis");
    } else if (t->kind == TOK_EOF) {
        diag_syntax_error(g_diag, t->line, t->col,
            "expected operand, found end of input");
    } else if (t->kind == TOK_ERROR) {
        advance(p);
        p->had_error = 1;
        return NULL;
    } else {
        diag_syntax_error(g_diag, t->line, t->col,
            "unexpected token '%s'", t->lexeme);
    }
    p->had_error = 1;
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  parse_term  ->  factor (('*' | '/') factor)*                       */
/* ------------------------------------------------------------------ */

static AstNode *parse_term(Parser *p) {
    AstNode *left = parse_factor(p);
    if (!left) return NULL;

    while (cur_kind(p) == TOK_STAR || cur_kind(p) == TOK_SLASH) {
        const Token *op   = advance(p);
        char         op_ch = op->lexeme[0];

        if (cur_kind(p) == TOK_STAR  || cur_kind(p) == TOK_SLASH ||
            cur_kind(p) == TOK_PLUS) {
            const Token *nxt = current(p);
            diag_syntax_error(g_diag, nxt->line, nxt->col,
                "expected operand after '%c', found '%s'", op_ch, nxt->lexeme);
            p->had_error = 1;
            ast_free(left);
            return NULL;
        }

        AstNode *right = parse_factor(p);
        if (!right) { ast_free(left); return NULL; }

        /* Semantic: warn on division by literal zero */
        if (op_ch == '/' && right->kind == AST_NUM &&
            fabs(right->num_val) < 1e-300) {
            diag_warning(g_diag, op->line, op->col, "division by zero");
            p->had_warning = 1;
        }

        left = ast_new_binop(op_ch, left, right, op->line, op->col);
    }
    return left;
}

/* ------------------------------------------------------------------ */
/*  parse_expr  ->  term (('+' | '-') term)*                           */
/* ------------------------------------------------------------------ */

static AstNode *parse_expr(Parser *p) {
    AstNode *left = parse_term(p);
    if (!left) return NULL;

    while (cur_kind(p) == TOK_PLUS || cur_kind(p) == TOK_MINUS) {
        const Token *op   = advance(p);
        char         op_ch = op->lexeme[0];

        if (cur_kind(p) == TOK_PLUS || cur_kind(p) == TOK_STAR ||
            cur_kind(p) == TOK_SLASH) {
            const Token *nxt = current(p);
            diag_syntax_error(g_diag, nxt->line, nxt->col,
                "expected operand after '%c', found '%s'", op_ch, nxt->lexeme);
            p->had_error = 1;
            ast_free(left);
            return NULL;
        }

        AstNode *right = parse_term(p);
        if (!right) { ast_free(left); return NULL; }
        left = ast_new_binop(op_ch, left, right, op->line, op->col);
    }
    return left;
}

/* ------------------------------------------------------------------ */
/*  Public API: parse()                                                 */
/* ------------------------------------------------------------------ */

ParseResult parse(const TokenList *tl) {
    ParseResult result;
    result.root        = NULL;
    result.had_error   = tl->had_error;
    result.had_warning = 0;
    result.syms.count  = 0;

    Parser p;
    p.tl          = tl;
    p.pos         = 0;
    p.had_error   = tl->had_error;
    p.had_warning = 0;
    p.syms.count  = 0;

    /* Empty input */
    if (cur_kind(&p) == TOK_EOF) {
        diag_syntax_error(g_diag, 1, 1,
            "empty input - no expression found");
        result.had_error = 1;
        return result;
    }

    /* Skip leading ERROR tokens */
    while (cur_kind(&p) == TOK_ERROR) advance(&p);
    if (cur_kind(&p) == TOK_EOF) {
        result.had_error = 1;
        return result;
    }

    AstNode *root = parse_expr(&p);

    /* Trailing content: "4 5", "2+3 )" etc. */
    if (root && !p.had_error && cur_kind(&p) != TOK_EOF) {
        const Token *t = current(&p);
        if (t->kind == TOK_NUM || t->kind == TOK_ID) {
            diag_syntax_error(g_diag, t->line, t->col,
                "unexpected operand '%s' (missing operator?)", t->lexeme);
        } else if (t->kind == TOK_RPAREN) {
            diag_syntax_error(g_diag, t->line, t->col,
                "unmatched ')' with no opening '('");
        } else {
            diag_syntax_error(g_diag, t->line, t->col,
                "unexpected token '%s'", t->lexeme);
        }
        p.had_error = 1;
        ast_free(root);
        root = NULL;
    }

    result.root        = root;
    result.had_error   = p.had_error;
    result.had_warning = p.had_warning;
    result.syms        = p.syms;
    return result;
}

/* ------------------------------------------------------------------ */
/*  Public API: print_symbol_table()                                    */
/* ------------------------------------------------------------------ */

void print_symbol_table(const SymbolTable *syms) {
    if (syms->count == 0) { printf("  (no identifiers)\n"); return; }
    for (int i = 0; i < syms->count; i++)
        printf("  [%d] %s\n", i + 1, syms->names[i]);
}
