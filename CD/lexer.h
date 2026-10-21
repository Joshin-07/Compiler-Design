/*
 * lexer.h  –  Arithmetic Expression Compiler (Front End)
 *
 * Public interface for the hand-written lexer.
 * The lexer converts a source string into a flat array of Token objects.
 *
 * Token types (TokenKind):
 *   NUM     – integer or decimal literal  (e.g. 42, 3.14)
 *   ID      – identifier                  (letter/underscore followed by
 *                                          letters/digits/underscores)
 *   PLUS    – '+'
 *   MINUS   – '-'
 *   STAR    – '*'
 *   SLASH   – '/'
 *   LPAREN  – '('
 *   RPAREN  – ')'
 *   EOF_TOK – sentinel at end of token stream
 *   ERROR   – illegal character or malformed number (diagnostic already printed)
 */

#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>   /* size_t */

/* ------------------------------------------------------------------ */
/*  Token kinds                                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    TOK_NUM,
    TOK_ID,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EOF,
    TOK_ERROR          /* lex-time error; message already printed */
} TokenKind;

/* ------------------------------------------------------------------ */
/*  Token                                                               */
/* ------------------------------------------------------------------ */

#define MAX_LEXEME 256   /* maximum lexeme length (including '\0') */

typedef struct {
    TokenKind   kind;
    char        lexeme[MAX_LEXEME]; /* raw text of the token            */
    int         line;               /* 1-based line number              */
    int         col;                /* 1-based column of first char     */
} Token;

/* ------------------------------------------------------------------ */
/*  TokenList – dynamic array returned by lex()                        */
/* ------------------------------------------------------------------ */

typedef struct {
    Token  *tokens;    /* heap-allocated array                          */
    size_t  count;     /* number of tokens (including the EOF sentinel) */
    int     had_error; /* non-zero if at least one LEX error occurred   */
} TokenList;

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * lex(src)
 *   Tokenise the NUL-terminated string `src`.
 *   Returns a TokenList whose `tokens` array is heap-allocated.
 *   The caller must free it with token_list_free().
 */
TokenList lex(const char *src);

/*
 * token_list_free(tl)
 *   Release the heap memory owned by `tl`.
 */
void token_list_free(TokenList *tl);

/*
 * token_kind_name(kind)
 *   Return a human-readable name for a TokenKind (e.g. "NUM", "PLUS").
 */
const char *token_kind_name(TokenKind kind);

/*
 * print_token_table(tl)
 *   Pretty-print the token stream as a formatted table:
 *       Lexeme         | Type    | Position
 *       ---------------+---------+---------
 *       42             | NUM     | 1:1
 *       ...
 */
void print_token_table(const TokenList *tl);

#endif /* LEXER_H */
