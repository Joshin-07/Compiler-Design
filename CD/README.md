# Arithmetic Expression Compiler — Front End

A hand-written **lexer + recursive-descent parser** for arithmetic expressions, written in standard **C99** with no external libraries. Includes an interactive CLI, test suite runner, and a modern zero-dependency web interface.

---

## Grammar (BNF)

```bnf
<expr>   ::= <term>   { ('+' | '-') <term>   }
<term>   ::= <factor> { ('*' | '/') <factor> }
<factor> ::= NUMBER
           | IDENTIFIER
           | '(' <expr> ')'
           | '-' <factor>
```

**Precedence (low → high):**

| Level | Operators | Associativity |
|-------|-----------|---------------|
| 1     | `+`  `-`  | Left          |
| 2     | `*`  `/`  | Left          |
| 3     | unary `-` | Right         |
| 4     | `(` `)` grouping | n/a    |

---

## Token Types

| Token    | Pattern                               | Example      |
|----------|---------------------------------------|--------------|
| `NUM`    | Integer or decimal literal            | `42`, `3.14` |
| `ID`     | `[A-Za-z_][A-Za-z0-9_]*`              | `x`, `my_var`|
| `PLUS`   | `+`                                   |              |
| `MINUS`  | `-`                                   |              |
| `STAR`   | `*`                                   |              |
| `SLASH`  | `/`                                   |              |
| `LPAREN` | `(`                                   |              |
| `RPAREN` | `)`                                   |              |
| `EOF`    | End of input sentinel                 |              |
| `ERROR`  | Illegal character or malformed number |              |

---

## Project Structure

```
.
├── api.h        – JSON analysis API header (for CLI --json mode and web server)
├── api.c        – Implementation of analyze() (runs full compiler pipeline)
├── errors.h     – Central diagnostic collector header (errors & warnings)
├── errors.c     – Diagnostic collector implementation
├── lexer.h      – Lexer interface (TokenKind, Token, TokenList)
├── lexer.c      – Hand-written tokenizer
├── ast.h        – AST node definitions and evaluation helpers
├── ast.c        – Node allocation, print, parenthesized/postfix, eval, JSON
├── parser.h     – Parser interface (ParseResult, SymbolTable)
├── parser.c     – Recursive-descent parser (one function per rule)
├── main.c       – CLI driver: REPL, file-mode runner, and --json mode
├── Makefile     – Build system (all, test, clean, run-web)
├── tests.txt    – Test suite containing 35 expression cases
├── aec.exe      – Compiled compiler binary (Windows)
├── web/
│   ├── index.html – Modern dark Web UI with live compiler visualizer
│   ├── style.css  – Responsive vanilla CSS design
│   ├── app.js     – Web UI controller (connects to local HTTP server)
│   └── server.js  – Zero-dependency Node.js HTTP server
└── README.md    – Project documentation
```

---

## How to Build

### Windows (MinGW-w64)

```bash
mingw32-make
```

This compiles all C sources into `aec.exe` using `gcc -std=c99 -Wall -Wextra -pedantic -g`.

### Linux / macOS

```bash
make
```

This compiles all C sources into `aec`.

### Clean Build Artifacts

```bash
mingw32-make clean
# or on Linux:
make clean
```

Deletes all intermediate object files (`*.o`) and the compiled binary.

---

## How to Run the CLI

### 1. Run Against Test Suite

```bash
aec.exe tests.txt
# or via make:
mingw32-make test
```

### 2. Interactive REPL Mode

Run without arguments to enter the interactive shell:

```text
aec.exe
Arithmetic Expression Compiler  (type 'quit' or 'exit' to stop)
============================================================

aec> 2 + 3 * 4
...
aec> quit
```

### 3. JSON API Mode (`--json`)

Used by the web server to analyze expressions programmatically:

```bash
aec.exe --json "2 + 3 * 4"
```

Outputs a structured JSON payload containing tokens, AST hierarchy, parenthesized form, postfix notation, evaluated result, symbol table, warnings, and syntax errors.

---

## How to Run the Web UI

1. Make sure `aec.exe` is built:
   ```bash
   mingw32-make
   ```

2. Start the local web server:
   ```bash
   node web/server.js
   # or:
   mingw32-make run-web
   ```

3. Open your browser and navigate to:
   ```
   http://localhost:8080/
   ```

The web interface provides real-time AST visualization, token breakdown, parenthesized/postfix expressions, symbol tables, and an interactive operator precedence walkthrough.

---

## Sample Outputs

### Sample 1: `2+3*4` (Operator Precedence)

```
============================================================
Expression #1: 2+3*4
------------------------------------------------------------

[1] TOKEN TABLE
  Lexeme               | Type       | Position
  ---------------------+------------+----------
  2                    | NUM        | 1:1
  +                    | PLUS       | 1:2
  3                    | NUM        | 1:3
  *                    | STAR       | 1:4
  4                    | NUM        | 1:5
                       | EOF        | 1:6

[2] PARSING

Result: VALID

[3] ABSTRACT SYNTAX TREE
  BINOP('+')
    NUM(2)
    BINOP('*')
      NUM(3)
      NUM(4)

[4] FULLY PARENTHESIZED FORM
  (2+(3*4))

[5] POSTFIX (RPN) FORM
  2 3 4 * +

[6] EVALUATION
  Result = 14

[7] SYMBOL TABLE (identifiers used)
  (no identifiers)
```

---

### Sample 2: `(2+3)*4` (Parenthesized Override)

```
============================================================
Expression #2: (2+3)*4
------------------------------------------------------------

[1] TOKEN TABLE
  Lexeme               | Type       | Position
  ---------------------+------------+----------
  (                    | LPAREN     | 1:1
  2                    | NUM        | 1:2
  +                    | PLUS       | 1:3
  3                    | NUM        | 1:4
  )                    | RPAREN     | 1:5
  *                    | STAR       | 1:6
  4                    | NUM        | 1:7
                       | EOF        | 1:8

[2] PARSING

Result: VALID

[3] ABSTRACT SYNTAX TREE
  BINOP('*')
    BINOP('+')
      NUM(2)
      NUM(3)
    NUM(4)

[4] FULLY PARENTHESIZED FORM
  ((2+3)*4)

[5] POSTFIX (RPN) FORM
  2 3 + 4 *

[6] EVALUATION
  Result = 20

[7] SYMBOL TABLE (identifiers used)
  (no identifiers)
```

---

### Sample 3: `2+*3` (Syntax Error)

```
============================================================
Expression #3: 2+*3
------------------------------------------------------------

[1] TOKEN TABLE
  Lexeme               | Type       | Position
  ---------------------+------------+----------
  2                    | NUM        | 1:1
  +                    | PLUS       | 1:2
  *                    | STAR       | 1:3
  3                    | NUM        | 1:4
                       | EOF        | 1:5

[2] PARSING
Syntax error at 1:3: expected operand after '+', found '*'

Result: INVALID
```

---

## Error Handling

| Situation | Example input | Diagnostic message |
|-----------|---------------|--------------------|
| Illegal character | `3 @ 4` | `Lex error at 1:3: illegal character '@'` |
| Malformed number | `1.2.3` | `Lex error at 1:1: malformed number '1.2.'` |
| Missing operand | `2+*3` | `Syntax error at 1:3: expected operand after '+', found '*'` |
| Empty input | *(blank line)* | `Syntax error at 1:1: empty input - no expression found` |
| Unbalanced `(` | `(2+3` | `Syntax error at 1:5: expected ')', found 'end of input'`<br>`Syntax error at 1:5: unmatched '(' opened at 1:1` |
| Unbalanced `)` | `2+3)` | `Syntax error at 1:4: unmatched ')' with no opening '('` |
| Empty parens | `()` | `Syntax error at 1:2: empty parentheses '()'` |
| Two operands | `4 5` | `Syntax error at 1:3: unexpected operand '5' (missing operator?)` |
| Division by zero | `10/0` | `Warning at 1:3: division by zero` |

---

## Design Principles

- **Lexer**: Single-pass character scanner with $O(1)$ lookahead. Dynamic token array with geometric growth; always ends with `TOK_EOF`.
- **Parser**: Strict recursive descent, one C function per grammar rule (`parse_expr`, `parse_term`, `parse_factor`). Errors report exact `line:col`.
- **AST**: Tagged-union heap nodes representing literals, identifiers, unary operations, and binary operations.
- **Symbol Table**: Tracks all unique variable names referenced in the expression.
- **Memory Safety**: Every `TokenList` and `AstNode` tree is explicitly freed between expressions.
