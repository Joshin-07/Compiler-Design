# Arithmetic Expression Compiler

This project checks and evaluates arithmetic expressions with a small C program.
The lexer scans the input and stores each token and its position in fixed arrays.
The recursive-descent parser reads those tokens, builds the parenthesized
precedence text, and calculates the value when there are no identifiers.

## Grammar

```text
expr   -> term   (('+' | '-') term)*
term   -> factor (('*' | '/') factor)*
factor -> NUMBER | IDENTIFIER | '(' expr ')' | '-' factor
```

Multiplication and division have higher precedence than addition and subtraction.
Operators at the same level are evaluated from left to right.

## Build and run with gcc

```text
gcc main.c -o main
main "2+3*4"
main < tests.txt
```

The program also reads expressions one per line from standard input. To run the
web interface, use `node web/server.js`; the server compiles `main.c` with gcc
to a temporary executable when it starts.

## Files

```text
main.c
tests.txt
README.md
web/
  index.html
  style.css
  app.js
  server.js
```
