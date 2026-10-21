/*
 * errors.c  –  Arithmetic Expression Compiler (Front End)
 *
 * Implementation of the diagnostic collector.
 *
 * When g_diag is non-NULL, errors and warnings are appended to the
 * list instead of being printed.  This decouples the lexer/parser
 * from the output destination (stderr vs. JSON buffer).
 */

#include "errors.h"

#include <stdarg.h>   /* va_list, va_start, va_end, vsnprintf */
#include <stdio.h>    /* fprintf, vfprintf, stderr            */

/* ------------------------------------------------------------------ */
/*  Global diagnostic pointer (NULL until a caller sets it)            */
/* ------------------------------------------------------------------ */

DiagList *g_diag = NULL;

/* ------------------------------------------------------------------ */
/*  Internal helper                                                     */
/* ------------------------------------------------------------------ */

static void diag_push(DiagList *dl, DiagKind kind,
                      int line, int col,
                      const char *fmt, va_list ap)
{
    if (!dl || dl->count >= MAX_DIAG) return;

    Diag *d = &dl->items[dl->count++];
    d->kind = kind;
    d->line = line;
    d->col  = col;
    vsnprintf(d->message, MAX_DIAG_MSG, fmt, ap);

    if (kind == DIAG_WARNING) dl->had_warning = 1;
    else                       dl->had_error   = 1;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void diag_clear(DiagList *dl) {
    if (!dl) return;
    dl->count       = 0;
    dl->had_error   = 0;
    dl->had_warning = 0;
}

/* Lexer error -------------------------------------------------------- */
void diag_lex_error(DiagList *dl, int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (dl) {
        diag_push(dl, DIAG_LEX_ERROR, line, col, fmt, ap);
    } else {
        /* Fallback: print directly so callers never silently swallow errors */
        fprintf(stderr, "Lex error at %d:%d: ", line, col);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
    }
    va_end(ap);
}

/* Parser/syntax error ------------------------------------------------ */
void diag_syntax_error(DiagList *dl, int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (dl) {
        diag_push(dl, DIAG_SYNTAX_ERROR, line, col, fmt, ap);
    } else {
        fprintf(stderr, "Syntax error at %d:%d: ", line, col);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
    }
    va_end(ap);
}

/* Semantic warning --------------------------------------------------- */
void diag_warning(DiagList *dl, int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (dl) {
        diag_push(dl, DIAG_WARNING, line, col, fmt, ap);
    } else {
        fprintf(stderr, "Warning at %d:%d: ", line, col);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
    }
    va_end(ap);
}

/* Flush to stderr (CLI mode) ---------------------------------------- */
void diag_flush_stderr(const DiagList *dl) {
    if (!dl) return;
    for (int i = 0; i < dl->count; i++) {
        const Diag *d = &dl->items[i];
        switch (d->kind) {
            case DIAG_LEX_ERROR:
                fprintf(stderr, "Lex error at %d:%d: %s\n",
                        d->line, d->col, d->message);
                break;
            case DIAG_SYNTAX_ERROR:
                fprintf(stderr, "Syntax error at %d:%d: %s\n",
                        d->line, d->col, d->message);
                break;
            case DIAG_WARNING:
                fprintf(stderr, "Warning at %d:%d: %s\n",
                        d->line, d->col, d->message);
                break;
        }
    }
}
