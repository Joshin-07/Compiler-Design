/*
 * parser.h  –  Arithmetic Expression Compiler (Front End)
 *
 * Public interface for the recursive-descent parser.
 *
 * Grammar (enforces precedence and left-associativity):
 *
 *   expr   -> term   (('+' | '-') term)*
 *   term   -> factor (('*' | '/') factor)*
 *   factor -> NUMBER
 *           | IDENTIFIER
 *           | '(' expr ')'
 *           | '-' factor          (unary minus)
 *
 * Semantic checks performed during/after parsing:
 *   - Warn on division by the literal zero (divisor is a NUM node with
 *     value 0).
 *   - Build a symbol table of every unique identifier used.
 */

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

/* ------------------------------------------------------------------ */
/*  Symbol table                                                        */
/* ------------------------------------------------------------------ */

#define MAX_SYMBOLS 256
#define MAX_SYM_LEN 256

typedef struct {
    char names[MAX_SYMBOLS][MAX_SYM_LEN];
    int  count;
} SymbolTable;

/* ------------------------------------------------------------------ */
/*  ParseResult                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    AstNode    *root;        /* NULL if parse failed                    */
    SymbolTable syms;        /* identifiers seen                        */
    int         had_error;   /* non-zero if a syntax error occurred     */
    int         had_warning; /* non-zero if a semantic warning occurred */
} ParseResult;

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * parse(tl)
 *   Parse the token list produced by lex().
 *   Returns a ParseResult. On success, result.root is the AST root.
 *   On failure, result.root is NULL and result.had_error is non-zero.
 *   All diagnostic messages are printed to stderr.
 *
 *   The caller is responsible for freeing result.root with ast_free().
 */
ParseResult parse(const TokenList *tl);

/*
 * print_symbol_table(syms)
 *   Pretty-print the symbol table to stdout.
 */
void print_symbol_table(const SymbolTable *syms);

#endif /* PARSER_H */
