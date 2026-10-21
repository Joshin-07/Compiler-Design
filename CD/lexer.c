/*
 * lexer.c  –  Arithmetic Expression Compiler (Front End)
 *
 * Hand-written lexer for arithmetic expressions.
 * Unchanged from the original except that all diagnostic output
 * now goes through the errors.h collector instead of directly to
 * fprintf(stderr, ...).  When g_diag is NULL (old CLI code paths
 * that haven't set it yet) the collector falls back to stderr
 * automatically.
 */

#include "lexer.h"
#include "errors.h"   /* diag_lex_error(), g_diag */

#include <ctype.h>    /* isdigit, isalpha, isspace */
#include <stdio.h>    /* fprintf, printf            */
#include <stdlib.h>   /* malloc, realloc, free      */
#include <string.h>   /* strncpy, snprintf         */

/* ------------------------------------------------------------------ */
/*  Internal scanner state                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *src;
    size_t      pos;
    int         line;
    int         col;
} Scanner;

static char sc_peek(const Scanner *sc)  { return sc->src[sc->pos]; }
static char sc_peek2(const Scanner *sc) {
    return sc->src[sc->pos] ? sc->src[sc->pos + 1] : '\0';
}
static char sc_advance(Scanner *sc) {
    char c = sc->src[sc->pos++];
    if (c == '\n') { sc->line++; sc->col = 1; } else { sc->col++; }
    return c;
}

/* ------------------------------------------------------------------ */
/*  Dynamic token-array helpers                                         */
/* ------------------------------------------------------------------ */

static void tl_push(TokenList *tl, size_t *cap, Token tok) {
    if (tl->count >= *cap) {
        size_t new_cap = (*cap) * 2;
        Token *p = (Token *)realloc(tl->tokens, new_cap * sizeof(Token));
        if (!p) { fprintf(stderr, "Out of memory in lexer\n"); exit(1); }
        tl->tokens = p;
        *cap = new_cap;
    }
    tl->tokens[tl->count++] = tok;
}

/* ------------------------------------------------------------------ */
/*  Token constructor                                                   */
/* ------------------------------------------------------------------ */

static Token make_tok(TokenKind kind, const char *lex, int line, int col) {
    Token t;
    t.kind = kind;
    t.line = line;
    t.col  = col;
    strncpy(t.lexeme, lex, MAX_LEXEME - 1);
    t.lexeme[MAX_LEXEME - 1] = '\0';
    return t;
}

/* ------------------------------------------------------------------ */
/*  Whitespace                                                          */
/* ------------------------------------------------------------------ */

static void skip_whitespace(Scanner *sc) {
    while (sc_peek(sc) != '\0' && isspace((unsigned char)sc_peek(sc)))
        sc_advance(sc);
}

/* ------------------------------------------------------------------ */
/*  Number  (integer or decimal; double-dot is a lex error)            */
/* ------------------------------------------------------------------ */

static Token lex_number(Scanner *sc) {
    int    start_line = sc->line;
    int    start_col  = sc->col;
    char   buf[MAX_LEXEME];
    size_t len = 0;
    int    dot_seen = 0;
    int    error    = 0;

    while (isdigit((unsigned char)sc_peek(sc)) || sc_peek(sc) == '.') {
        if (sc_peek(sc) == '.') {
            if (dot_seen) {
                if (len < MAX_LEXEME - 1) buf[len++] = sc_peek(sc);
                sc_advance(sc);
                error = 1;
                while (isdigit((unsigned char)sc_peek(sc)) || sc_peek(sc) == '.')
                    sc_advance(sc);
                break;
            }
            dot_seen = 1;
        }
        if (len < MAX_LEXEME - 1) buf[len++] = sc_peek(sc);
        sc_advance(sc);
    }
    buf[len] = '\0';

    if (error) {
        /* Route through the diagnostic collector */
        diag_lex_error(g_diag, start_line, start_col,
                       "malformed number '%s'", buf);
        return make_tok(TOK_ERROR, buf, start_line, start_col);
    }
    return make_tok(TOK_NUM, buf, start_line, start_col);
}

/* ------------------------------------------------------------------ */
/*  Identifier  [A-Za-z_][A-Za-z0-9_]*                                 */
/* ------------------------------------------------------------------ */

static Token lex_id(Scanner *sc) {
    int    start_line = sc->line;
    int    start_col  = sc->col;
    char   buf[MAX_LEXEME];
    size_t len = 0;

    while (isalnum((unsigned char)sc_peek(sc)) || sc_peek(sc) == '_') {
        if (len < MAX_LEXEME - 1) buf[len++] = sc_peek(sc);
        sc_advance(sc);
    }
    buf[len] = '\0';
    return make_tok(TOK_ID, buf, start_line, start_col);
}

/* ------------------------------------------------------------------ */
/*  Public API: lex()                                                   */
/* ------------------------------------------------------------------ */

TokenList lex(const char *src) {
    TokenList tl;
    size_t cap   = 16;
    tl.tokens    = (Token *)malloc(cap * sizeof(Token));
    tl.count     = 0;
    tl.had_error = 0;
    if (!tl.tokens) { fprintf(stderr, "Out of memory in lexer\n"); exit(1); }

    Scanner sc;
    sc.src  = src ? src : "";
    sc.pos  = 0;
    sc.line = 1;
    sc.col  = 1;

    while (1) {
        skip_whitespace(&sc);
        if (sc_peek(&sc) == '\0') {
            tl_push(&tl, &cap, make_tok(TOK_EOF, "", sc.line, sc.col));
            break;
        }

        int  tok_line = sc.line;
        int  tok_col  = sc.col;
        char c        = sc_peek(&sc);

        if (c == '+') { sc_advance(&sc);
            tl_push(&tl, &cap, make_tok(TOK_PLUS,   "+", tok_line, tok_col)); continue; }
        if (c == '-') { sc_advance(&sc);
            tl_push(&tl, &cap, make_tok(TOK_MINUS,  "-", tok_line, tok_col)); continue; }
        if (c == '*') { sc_advance(&sc);
            tl_push(&tl, &cap, make_tok(TOK_STAR,   "*", tok_line, tok_col)); continue; }
        if (c == '/') { sc_advance(&sc);
            tl_push(&tl, &cap, make_tok(TOK_SLASH,  "/", tok_line, tok_col)); continue; }
        if (c == '(') { sc_advance(&sc);
            tl_push(&tl, &cap, make_tok(TOK_LPAREN, "(", tok_line, tok_col)); continue; }
        if (c == ')') { sc_advance(&sc);
            tl_push(&tl, &cap, make_tok(TOK_RPAREN, ")", tok_line, tok_col)); continue; }

        if (isdigit((unsigned char)c) ||
            (c == '.' && isdigit((unsigned char)sc_peek2(&sc)))) {
            Token t = lex_number(&sc);
            if (t.kind == TOK_ERROR) tl.had_error = 1;
            tl_push(&tl, &cap, t);
            continue;
        }

        if (isalpha((unsigned char)c) || c == '_') {
            tl_push(&tl, &cap, lex_id(&sc));
            continue;
        }

        /* Illegal character ----------------------------------------- */
        {
            char lex_buf[4] = { c, '\0', '\0', '\0' };
            sc_advance(&sc);
            diag_lex_error(g_diag, tok_line, tok_col,
                           "illegal character '%c'", c);
            tl_push(&tl, &cap, make_tok(TOK_ERROR, lex_buf, tok_line, tok_col));
            tl.had_error = 1;
        }
    }

    return tl;
}

/* ------------------------------------------------------------------ */
/*  Public API: token_list_free()                                       */
/* ------------------------------------------------------------------ */

void token_list_free(TokenList *tl) {
    if (tl) { free(tl->tokens); tl->tokens = NULL; tl->count = 0; }
}

/* ------------------------------------------------------------------ */
/*  Public API: token_kind_name()                                       */
/* ------------------------------------------------------------------ */

const char *token_kind_name(TokenKind kind) {
    switch (kind) {
        case TOK_NUM:    return "NUM";
        case TOK_ID:     return "ID";
        case TOK_PLUS:   return "PLUS";
        case TOK_MINUS:  return "MINUS";
        case TOK_STAR:   return "STAR";
        case TOK_SLASH:  return "SLASH";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_EOF:    return "EOF";
        case TOK_ERROR:  return "ERROR";
        default:         return "UNKNOWN";
    }
}

/* ------------------------------------------------------------------ */
/*  Public API: print_token_table()                                     */
/* ------------------------------------------------------------------ */

void print_token_table(const TokenList *tl) {
    const int W_LEX  = 20;
    const int W_TYPE = 10;

    printf("  %-*s | %-*s | %s\n", W_LEX, "Lexeme", W_TYPE, "Type", "Position");
    printf("  ");
    for (int i = 0; i < W_LEX; i++) putchar('-');
    printf("-+-");
    for (int i = 0; i < W_TYPE; i++) putchar('-');
    printf("-+----------\n");

    for (size_t i = 0; i < tl->count; i++) {
        const Token *t = &tl->tokens[i];
        char pos[32];
        snprintf(pos, sizeof(pos), "%d:%d", t->line, t->col);
        printf("  %-*s | %-*s | %s\n",
               W_LEX, t->lexeme,
               W_TYPE, token_kind_name(t->kind),
               pos);
    }
}
