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

## Syntax Tree

The parser builds a syntax tree while it evaluates each expression. The tree
is stored in four parallel arrays (no structs or dynamic allocation):

```text
char nodeLabel[100][20]   – the text of each node: "+", "*", "3", "a", "neg"
int  nodeLeft[100]        – index of the left child, -1 if none
int  nodeRight[100]       – index of the right child, -1 if none
int  nodeCount            – number of nodes created so far
```

`newNode(label, left, right)` stores a node in the next free slot and returns
its index. Each parser function (`factor`, `term`, `expr`) sets the global
`lastNode` to the index of the node it just built. Parentheses do not create
a node; unary minus creates a `"neg"` node with one child.

`printTree(node, depth)` prints the tree recursively. It indents by 4 spaces
per depth level, prints the left child first, then the right child. The tree
is printed only for syntactically valid expressions, after the Value line.

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
