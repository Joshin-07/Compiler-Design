/*
 * api.c  –  Arithmetic Expression Compiler (Front End)
 *
 * Single exported function: analyze(expr) → JSON string.
 *
 * Linked into the native CLI binary (aec.exe) for --json mode,
 * which is called by the Node.js web server.
 *
 * The function is re-entrant in the sense that each call fully resets
 * the static output buffer and the diagnostic collector.  It is NOT
 * thread-safe (static buffer, global g_diag).
 *
 * JSON layout:
 * {
 *   "valid":         true | false,
 *   "errors":        [ {"kind":"lex"|"syntax","message":"...","line":N,"col":N} ],
 *   "warnings":      [ {"message":"...","line":N,"col":N} ],
 *   "tokens":        [ {"lexeme":"...","type":"...","line":N,"col":N} ],
 *   "ast":           { ... } | null,
 *   "parenthesized": "..." | null,
 *   "postfix":       "..." | null,
 *   "result":        N | null,
 *   "symbols":       [ "..." ]
 * }
 */

#include "api.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "errors.h"

#include <math.h>     /* isinf, isnan */
#include <stdio.h>    /* snprintf     */

/* ------------------------------------------------------------------ */
/*  Output buffer (static – valid until next call to analyze())        */
/* ------------------------------------------------------------------ */

#define JSON_BUF 131072   /* 128 KB – plenty for any expression */

static char out_buf[JSON_BUF];

/* ------------------------------------------------------------------ */
/*  Minimal JSON string builder (JB)                                   */
/* ------------------------------------------------------------------ */

typedef struct { char *buf; size_t size; size_t pos; } JB;

static void jb_init(JB *jb) {
    jb->buf  = out_buf;
    jb->size = JSON_BUF;
    jb->pos  = 0;
    out_buf[0] = '\0';
}

/* Append a raw (unescaped) string. */
static void jb_raw(JB *jb, const char *s) {
    while (*s && jb->pos < jb->size - 1)
        jb->buf[jb->pos++] = *s++;
    jb->buf[jb->pos] = '\0';
}

/* Append a single character. */
static void jb_ch(JB *jb, char c) {
    if (jb->pos < jb->size - 1) {
        jb->buf[jb->pos++] = c;
        jb->buf[jb->pos]   = '\0';
    }
}

/* Append a JSON-escaped string (with surrounding double-quotes). */
static void jb_str(JB *jb, const char *s) {
    jb_ch(jb, '"');
    for (; *s; s++) {
        switch (*s) {
            case '"':  jb_raw(jb, "\\\""); break;
            case '\\': jb_raw(jb, "\\\\"); break;
            case '\n': jb_raw(jb, "\\n");  break;
            case '\r': jb_raw(jb, "\\r");  break;
            case '\t': jb_raw(jb, "\\t");  break;
            default:
                if ((unsigned char)*s < 0x20) {
                    char tmp[8];
                    snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned char)*s);
                    jb_raw(jb, tmp);
                } else {
                    jb_ch(jb, *s);
                }
        }
    }
    jb_ch(jb, '"');
}

/* Append a JSON integer. */
static void jb_int(JB *jb, int n) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%d", n);
    jb_raw(jb, tmp);
}

/* Append a JSON boolean. */
static void jb_bool(JB *jb, int v) { jb_raw(jb, v ? "true" : "false"); }

/* Append a JSON number for a double, or null if not finite. */
static void jb_double(JB *jb, double d) {
    if (isnan(d) || isinf(d)) { jb_raw(jb, "null"); return; }
    char tmp[64];
    /* Use integer representation when value is whole */
    if (d == (double)(long long)d && d >= -1e15 && d <= 1e15)
        snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
    else
        snprintf(tmp, sizeof(tmp), "%.10g", d);
    jb_raw(jb, tmp);
}

/* ------------------------------------------------------------------ */
/*  Section builders                                                    */
/* ------------------------------------------------------------------ */

/* Emit the "errors" array from the diagnostic list. */
static void emit_errors(JB *jb, const DiagList *dl) {
    jb_raw(jb, "\"errors\":[");
    int first = 1;
    for (int i = 0; i < dl->count; i++) {
        const Diag *d = &dl->items[i];
        if (d->kind == DIAG_WARNING) continue;
        if (!first) jb_ch(jb, ',');
        first = 0;
        jb_raw(jb, "{\"kind\":");
        jb_str(jb, d->kind == DIAG_LEX_ERROR ? "lex" : "syntax");
        jb_raw(jb, ",\"message\":");
        jb_str(jb, d->message);
        jb_raw(jb, ",\"line\":");  jb_int(jb, d->line);
        jb_raw(jb, ",\"col\":");   jb_int(jb, d->col);
        jb_ch(jb, '}');
    }
    jb_ch(jb, ']');
}

/* Emit the "warnings" array. */
static void emit_warnings(JB *jb, const DiagList *dl) {
    jb_raw(jb, "\"warnings\":[");
    int first = 1;
    for (int i = 0; i < dl->count; i++) {
        const Diag *d = &dl->items[i];
        if (d->kind != DIAG_WARNING) continue;
        if (!first) jb_ch(jb, ',');
        first = 0;
        jb_raw(jb, "{\"message\":");
        jb_str(jb, d->message);
        jb_raw(jb, ",\"line\":"); jb_int(jb, d->line);
        jb_raw(jb, ",\"col\":");  jb_int(jb, d->col);
        jb_ch(jb, '}');
    }
    jb_ch(jb, ']');
}

/* Emit the "tokens" array (excluding EOF). */
static void emit_tokens(JB *jb, const TokenList *tl) {
    jb_raw(jb, "\"tokens\":[");
    int first = 1;
    for (size_t i = 0; i < tl->count; i++) {
        const Token *t = &tl->tokens[i];
        if (t->kind == TOK_EOF) continue;
        if (!first) jb_ch(jb, ',');
        first = 0;
        jb_raw(jb, "{\"lexeme\":");
        jb_str(jb, t->lexeme);
        jb_raw(jb, ",\"type\":");
        jb_str(jb, token_kind_name(t->kind));
        jb_raw(jb, ",\"line\":"); jb_int(jb, t->line);
        jb_raw(jb, ",\"col\":");  jb_int(jb, t->col);
        jb_ch(jb, '}');
    }
    jb_ch(jb, ']');
}

/* Emit the "symbols" array. */
static void emit_symbols(JB *jb, const SymbolTable *st) {
    jb_raw(jb, "\"symbols\":[");
    for (int i = 0; i < st->count; i++) {
        if (i > 0) jb_ch(jb, ',');
        jb_str(jb, st->names[i]);
    }
    jb_ch(jb, ']');
}

/* ------------------------------------------------------------------ */
/*  Public API: analyze()                                               */
/* ------------------------------------------------------------------ */

const char *analyze(const char *expr) {
    if (!expr) expr = "";

    /* ---- Set up diagnostic collector ----------------------------- */
    DiagList dl;
    diag_clear(&dl);
    g_diag = &dl;

    /* ---- Lex ----------------------------------------------------- */
    TokenList tl = lex(expr);

    /* ---- Parse --------------------------------------------------- */
    ParseResult pr = parse(&tl);

    /* ---- Reset global diag pointer ------------------------------ */
    g_diag = NULL;

    /* ---- Build JSON --------------------------------------------- */
    JB jb;
    jb_init(&jb);
    jb_ch(&jb, '{');

    /* "valid" */
    jb_raw(&jb, "\"valid\":");
    jb_bool(&jb, !pr.had_error);
    jb_ch(&jb, ',');

    /* "errors" */
    emit_errors(&jb, &dl);
    jb_ch(&jb, ',');

    /* "warnings" */
    emit_warnings(&jb, &dl);
    jb_ch(&jb, ',');

    /* "tokens" */
    emit_tokens(&jb, &tl);
    jb_ch(&jb, ',');

    /* "ast" */
    jb_raw(&jb, "\"ast\":");
    if (pr.root) {
        /* Use a temporary side-buffer so the AST JSON can be inserted
           without breaking the ongoing JB position tracking. */
        char ast_tmp[65536];
        ast_to_json(pr.root, ast_tmp, sizeof(ast_tmp));
        jb_raw(&jb, ast_tmp);
    } else {
        jb_raw(&jb, "null");
    }
    jb_ch(&jb, ',');

    /* "parenthesized" */
    jb_raw(&jb, "\"parenthesized\":");
    if (pr.root) {
        char pbuf[4096];
        ast_to_parens(pr.root, pbuf, sizeof(pbuf));
        jb_str(&jb, pbuf);
    } else {
        jb_raw(&jb, "null");
    }
    jb_ch(&jb, ',');

    /* "postfix" */
    jb_raw(&jb, "\"postfix\":");
    if (pr.root) {
        char rbuf[4096];
        ast_to_postfix(pr.root, rbuf, sizeof(rbuf));
        jb_str(&jb, rbuf);
    } else {
        jb_raw(&jb, "null");
    }
    jb_ch(&jb, ',');

    /* "result" */
    jb_raw(&jb, "\"result\":");
    if (pr.root && ast_is_const(pr.root)) {
        int ok = 0;
        double val = ast_eval(pr.root, &ok);
        if (ok) jb_double(&jb, val);
        else    jb_raw(&jb, "null");
    } else {
        jb_raw(&jb, "null");
    }
    jb_ch(&jb, ',');

    /* "symbols" */
    emit_symbols(&jb, &pr.syms);

    jb_ch(&jb, '}');

    /* ---- Cleanup ------------------------------------------------- */
    ast_free(pr.root);
    token_list_free(&tl);

    return out_buf;
}
