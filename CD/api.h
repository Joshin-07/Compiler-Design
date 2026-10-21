/*
 * api.h  –  Arithmetic Expression Compiler (Front End)
 *
 * Public interface for the JSON analysis API.
 *
 * This function is used by:
 *   a) the native CLI in --json mode
 *   b) the Node.js web server (web/server.js)
 */

#ifndef API_H
#define API_H

/*
 * analyze(expr)
 *   Run the full compiler front end (lex → parse → semantic checks)
 *   on the NUL-terminated string `expr` and return a JSON string.
 *
 *   The returned pointer is valid until the next call to analyze().
 *   The caller must NOT free it.
 *
 *   JSON schema:
 *   {
 *     "valid":         true | false,
 *     "errors":        [ {"kind":"lex"|"syntax","message":"...","line":N,"col":N} ],
 *     "warnings":      [ {"message":"...","line":N,"col":N} ],
 *     "tokens":        [ {"lexeme":"...","type":"...","line":N,"col":N} ],
 *     "ast":           { ... } | null,
 *     "parenthesized": "..." | null,
 *     "postfix":       "..." | null,
 *     "result":        N | null,
 *     "symbols":       [ "..." ]
 *   }
 */
const char *analyze(const char *expr);

#endif /* API_H */
