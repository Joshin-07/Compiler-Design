/*
 * errors.h  –  Arithmetic Expression Compiler (Front End)
 *
 * Centralised diagnostic collector.
 *
 * Both the lexer and the parser write diagnostics through this module
 * instead of calling fprintf(stderr, ...) directly.  This lets the CLI
 * driver flush errors to stderr (unchanged user experience) while the
 * JSON API silently collects them for serialisation.
 *
 * Usage:
 *   DiagList dl;
 *   diag_clear(&dl);
 *   g_diag = &dl;          // point global at local collector
 *   lex(src); parse(&tl);  // diagnostics go into dl
 *   g_diag = NULL;
 *   diag_flush_stderr(&dl); // CLI: print to stderr as usual
 *   // -- OR --
 *   // JSON mode: iterate dl.items[] and serialise
 */

#ifndef ERRORS_H
#define ERRORS_H

#define MAX_DIAG     64     /* max diagnostics per expression */
#define MAX_DIAG_MSG 512    /* max message length (bytes)     */

/* ------------------------------------------------------------------ */
/*  Diagnostic kind                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    DIAG_LEX_ERROR,     /* "Lex error …"    */
    DIAG_SYNTAX_ERROR,  /* "Syntax error …" */
    DIAG_WARNING        /* "Warning …"      */
} DiagKind;

/* ------------------------------------------------------------------ */
/*  Single diagnostic item                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    DiagKind kind;
    char     message[MAX_DIAG_MSG]; /* already-formatted message text  */
    int      line;
    int      col;
} Diag;

/* ------------------------------------------------------------------ */
/*  Diagnostic list                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    Diag items[MAX_DIAG];
    int  count;
    int  had_error;    /* 1 if any DIAG_LEX_ERROR or DIAG_SYNTAX_ERROR */
    int  had_warning;  /* 1 if any DIAG_WARNING                         */
} DiagList;

/* ------------------------------------------------------------------ */
/*  Global collector pointer                                            */
/*  Set this before calling lex()/parse(); reset to NULL afterwards.   */
/* ------------------------------------------------------------------ */

extern DiagList *g_diag;

/* ------------------------------------------------------------------ */
/*  API                                                                 */
/* ------------------------------------------------------------------ */

/* Reset the list to empty. */
void diag_clear(DiagList *dl);

/*
 * diag_lex_error(dl, line, col, fmt, ...)
 *   Record a lexer error.  If dl is NULL, print directly to stderr.
 */
void diag_lex_error(DiagList *dl, int line, int col, const char *fmt, ...);

/*
 * diag_syntax_error(dl, line, col, fmt, ...)
 *   Record a parser/syntax error.  If dl is NULL, print directly to stderr.
 */
void diag_syntax_error(DiagList *dl, int line, int col, const char *fmt, ...);

/*
 * diag_warning(dl, line, col, fmt, ...)
 *   Record a semantic warning.  If dl is NULL, print directly to stderr.
 */
void diag_warning(DiagList *dl, int line, int col, const char *fmt, ...);

/*
 * diag_flush_stderr(dl)
 *   Print every collected diagnostic to stderr in the same format the
 *   original code used (preserving backward compatibility for the CLI).
 */
void diag_flush_stderr(const DiagList *dl);

#endif /* ERRORS_H */
