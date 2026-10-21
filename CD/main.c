/*
 * main.c  –  Arithmetic Expression Compiler (Front End)
 *
 * Modes:
 *   Interactive  (no args)     – REPL, type "quit" to exit.
 *   File mode    (filename)    – one expression per line.
 *   JSON mode    (--json)      – read one line from stdin, print JSON to
 *                                stdout; used by the Node.js fallback server.
 *
 * All three modes share the same process_expression() helper which
 * now sets g_diag so diagnostics are collected instead of printed
 * inline, then flushes them to stderr at the end for the CLI modes.
 */

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "errors.h"
#include "api.h"      /* analyze() – used for --json mode */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Visual separator helpers                                            */
/* ------------------------------------------------------------------ */

#define RULE_WIDTH 60

static void print_rule(void) {
    for (int i = 0; i < RULE_WIDTH; i++) putchar('-');
    putchar('\n');
}

static void print_double_rule(void) {
    for (int i = 0; i < RULE_WIDTH; i++) putchar('=');
    putchar('\n');
}

/* ------------------------------------------------------------------ */
/*  Core processing for a single expression (CLI pretty-print)         */
/* ------------------------------------------------------------------ */

static void process_expression(const char *src, int expr_number) {
    print_double_rule();
    if (expr_number > 0)
        printf("Expression #%d: %s\n", expr_number, src);
    else
        printf("Expression: %s\n", src);
    print_rule();

    /* Set up the diagnostic collector for this expression */
    DiagList dl;
    diag_clear(&dl);
    g_diag = &dl;

    /* ---- Step 1: Lex -------------------------------------------- */
    printf("\n[1] TOKEN TABLE\n");
    TokenList tl = lex(src);
    print_token_table(&tl);

    /* ---- Step 2: Parse ------------------------------------------ */
    printf("\n[2] PARSING\n");
    ParseResult pr = parse(&tl);

    /* Flush all diagnostics (lex errors, parse errors, warnings) once */
    fflush(stdout);
    diag_flush_stderr(&dl);
    fflush(stderr);

    if (pr.had_error) {
        printf("\nResult: INVALID\n\n");
        ast_free(pr.root);
        token_list_free(&tl);
        g_diag = NULL;
        return;
    }

    printf("\nResult: VALID%s\n",
           pr.had_warning ? " (with warnings)" : "");

    /* ---- Step 3a: AST ------------------------------------------- */
    printf("\n[3] ABSTRACT SYNTAX TREE\n");
    ast_print(pr.root, 1);

    /* ---- Step 3b: Parenthesized --------------------------------- */
    printf("\n[4] FULLY PARENTHESIZED FORM\n");
    char parens_buf[2048];
    ast_to_parens(pr.root, parens_buf, sizeof(parens_buf));
    printf("  %s\n", parens_buf);

    /* ---- Step 3c: Postfix --------------------------------------- */
    printf("\n[5] POSTFIX (RPN) FORM\n");
    char postfix_buf[2048];
    ast_to_postfix(pr.root, postfix_buf, sizeof(postfix_buf));
    printf("  %s\n", postfix_buf);

    /* ---- Step 3d: Evaluation ------------------------------------ */
    printf("\n[6] EVALUATION\n");
    if (ast_is_const(pr.root)) {
        int ok = 0;
        double val = ast_eval(pr.root, &ok);
        if (ok) {
            if (val == (long long)val && !isinf(val) && !isnan(val))
                printf("  Result = %lld\n", (long long)val);
            else
                printf("  Result = %g\n", val);
        } else {
            printf("  Result = undefined (division by zero)\n");
        }
    } else {
        printf("  (expression contains identifiers - cannot evaluate)\n");
    }

    /* ---- Step 3e: Symbol table ---------------------------------- */
    printf("\n[7] SYMBOL TABLE (identifiers used)\n");
    print_symbol_table(&pr.syms);

    putchar('\n');

    ast_free(pr.root);
    token_list_free(&tl);
    g_diag = NULL;
}

/* ------------------------------------------------------------------ */
/*  Strip trailing CR/LF                                               */
/* ------------------------------------------------------------------ */

static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r'))
        s[--n] = '\0';
}

/* ------------------------------------------------------------------ */
/*  Interactive mode                                                    */
/* ------------------------------------------------------------------ */

static void run_interactive(void) {
    char buf[1024];
    int  expr_num = 1;

    printf("Arithmetic Expression Compiler  (type 'quit' or 'exit' to stop)\n");
    print_double_rule();

    while (1) {
        printf("\naec> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        rtrim(buf);
        if (strcmp(buf, "quit") == 0 || strcmp(buf, "exit") == 0) break;
        if (buf[0] == '#') continue;
        process_expression(buf, expr_num++);
    }
    printf("\nGoodbye.\n");
}

/* ------------------------------------------------------------------ */
/*  File mode                                                           */
/* ------------------------------------------------------------------ */

static void run_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s'\n", path);
        exit(1);
    }

    char buf[1024];
    int  expr_num = 1;

    printf("Arithmetic Expression Compiler - file: %s\n", path);

    while (fgets(buf, sizeof(buf), fp)) {
        rtrim(buf);
        if (buf[0] == '#') continue;
        if (buf[0] == '\0') {
            process_expression("", expr_num++);
            continue;
        }
        process_expression(buf, expr_num++);
    }
    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  JSON mode  (--json)                                                 */
/*  Reads one line from stdin, calls analyze(), writes JSON to stdout. */
/*  Used by web/server.js (the Node.js fallback server).               */
/* ------------------------------------------------------------------ */

static void run_json(void) {
    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) {
        puts("{\"valid\":false,\"errors\":[{\"kind\":\"syntax\",\"message\":"
             "\"no input\",\"line\":1,\"col\":1}],\"warnings\":[],"
             "\"tokens\":[],\"ast\":null,\"parenthesized\":null,"
             "\"postfix\":null,\"result\":null,\"symbols\":[]}");
        return;
    }
    rtrim(buf);
    const char *json = analyze(buf);
    puts(json);
}

/* ------------------------------------------------------------------ */
/*  main()                                                              */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    if (argc == 1) {
        run_interactive();
    } else if (argc == 2 && strcmp(argv[1], "--json") == 0) {
        run_json();
    } else if (argc == 3 && strcmp(argv[1], "--json") == 0) {
        const char *json = analyze(argv[2]);
        puts(json);
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s [file | --json [expr]]\n", argv[0]);
        return 1;
    }
    return 0;
}
