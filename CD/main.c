#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define TOKEN_LIMIT 100
#define TEXT_LIMIT 20
#define OUTPUT_LIMIT 512
#define INPUT_LIMIT 512
#define NODE_LIMIT 100

char tokType[TOKEN_LIMIT], tokText[TOKEN_LIMIT][TEXT_LIMIT];
int tokPos[TOKEN_LIMIT], tokCount, cur, errorFlag, hasIdentifier, divisionByZero;
char errorMessage[128];

/* Syntax-tree storage: parallel arrays instead of structs. */
char nodeLabel[NODE_LIMIT][TEXT_LIMIT];
int  nodeLeft[NODE_LIMIT];
int  nodeRight[NODE_LIMIT];
int  nodeCount = 0;
int  lastNode  = -1;  /* index of the most recently built node */

/* Creates a new tree node and returns its index. */
int newNode(char label[], int left, int right) {
    int id = nodeCount++;
    strcpy(nodeLabel[id], label);
    nodeLeft[id]  = left;
    nodeRight[id] = right;
    return id;
}

/* Prints the tree recursively: left child first, then right child. */
void printTree(int node, int depth) {
    int i;
    if (node < 0) return;
    for (i = 0; i < depth * 4; i++) putchar(' ');
    printf("%s\n", nodeLabel[node]);
    printTree(nodeLeft[node],  depth + 1);
    printTree(nodeRight[node], depth + 1);
}

/* Converts a position number to digits at the end of the error message. */
void add_position(int position) {
    char digits[12];
    int count = 0;
    do { digits[count++] = (char)('0' + position % 10); position /= 10; }
    while (position > 0 && count < 11);
    while (count > 0) { int n = (int)strlen(errorMessage); errorMessage[n] = digits[--count]; errorMessage[n + 1] = '\0'; }
}

/* Adds the standard position suffix. */
void position_suffix(int position) { strcat(errorMessage, " at position "); add_position(position); }

/* Creates an error containing a message and a position. */
void position_error(char *message, int position) {
    strcpy(errorMessage, message);
    position_suffix(position);
    errorFlag = 1;
}

/* Creates an error containing a token and its position. */
void token_error(char *beginning, char *text, int position) {
    strcpy(errorMessage, beginning);
    strcat(errorMessage, text);
    strcat(errorMessage, "'");
    position_suffix(position);
    errorFlag = 1;
}

/* Adds text to an output array when there is enough room. */
void append_text(char *target, char *text) {
    if ((int)strlen(target) + (int)strlen(text) < OUTPUT_LIMIT) strcat(target, text);
    else if (!errorFlag) { strcpy(errorMessage, "expression is too long"); errorFlag = 1; }
}

/* Scans the input and stores each token and its position in global arrays. */
void lexer(char *input) {
    int i = 0;
    tokCount = 0; errorFlag = 0; errorMessage[0] = '\0';
    /* Skip spaces and read numbers, identifiers, and operators. */
    while (input[i] != '\0' && !errorFlag) {
        int start = i, position = i + 1, length = 0;
        char current = input[i], type = current, text[TEXT_LIMIT];
        if (isspace((unsigned char)current)) { i++; continue; }
        if (isdigit((unsigned char)current) || (current == '.' && isdigit((unsigned char)input[i + 1]))) {
            int dots = 0;
            type = 'N';
            while (isdigit((unsigned char)input[i]) || input[i] == '.') { if (input[i] == '.') dots++; i++; }
            length = i - start;
            if (dots > 1) position_error("malformed number", position);
        } else if (isalpha((unsigned char)current) || current == '_') {
            type = 'I';
            while (isalpha((unsigned char)input[i]) || isdigit((unsigned char)input[i]) || input[i] == '_') i++;
            length = i - start;
        } else if (current == '+' || current == '-' || current == '*' || current == '/' || current == '(' || current == ')') {
            i++; length = 1;
        } else {
            char bad[2] = { current, '\0' };
            token_error("illegal character '", bad, position);
        }
        /* Keep only tokens that fit the fixed array sizes. */
        if (!errorFlag) {
            int j;
            if (length >= TEXT_LIMIT) position_error("token is too long", position);
            else if (tokCount >= TOKEN_LIMIT - 1) position_error("too many tokens", position);
            else {
                for (j = 0; j < length; j++) text[j] = input[start + j];
                text[length] = '\0'; tokType[tokCount] = type;
                strcpy(tokText[tokCount], text); tokPos[tokCount++] = position;
            }
        }
    }
    /* Add E so the parser can recognize the end of input. */
    if (!errorFlag) {
        tokType[tokCount] = 'E'; strcpy(tokText[tokCount], "end of input");
        tokPos[tokCount++] = i + 1;
    }
}

/* Builds the parenthesized text around a binary operator. */
void make_binary_text(char *output, char *left, char operation, char *right) {
    char operator_text[2] = { operation, '\0' };
    strcpy(output, "("); append_text(output, left); append_text(output, operator_text);
    append_text(output, right); append_text(output, ")");
}

double expr(char *output);
double term(char *output);

/* Parses a number, identifier, parenthesized expression, or unary minus. */
double factor(char *output) {
    double value = 0.0;
    if (tokType[cur] == 'N') {
        value = atof(tokText[cur]);
        lastNode = newNode(tokText[cur], -1, -1);
        strcpy(output, tokText[cur++]);
    } else if (tokType[cur] == 'I') {
        lastNode = newNode(tokText[cur], -1, -1);
        strcpy(output, tokText[cur++]);
        hasIdentifier = 1;
    } else if (tokType[cur] == '-') {
        char inside[OUTPUT_LIMIT];
        int childNode;
        cur++; value = -factor(inside);
        childNode = lastNode;
        lastNode = newNode("neg", childNode, -1);
        strcpy(output, "(-"); append_text(output, inside); append_text(output, ")");
    } else if (tokType[cur] == '(') {
        cur++; value = expr(output);
        /* parentheses do not get a tree node; lastNode stays as inner expr */
        if (!errorFlag && tokType[cur] == ')') cur++;
        else if (!errorFlag) token_error("expected ')', found '", tokText[cur], tokPos[cur]);
    } else token_error("expected operand, found '", tokText[cur], tokPos[cur]);
    return value;
}

/* Parses multiplication and division from left to right. */
double term(char *output) {
    double value = factor(output);
    int leftNode = lastNode;
    while (!errorFlag && (tokType[cur] == '*' || tokType[cur] == '/')) {
        char operation = tokType[cur], right_text[OUTPUT_LIMIT], left_text[OUTPUT_LIMIT];
        char op_str[2] = { operation, '\0' };
        double right_value;
        int rightNode;
        cur++; right_value = factor(right_text);
        rightNode = lastNode;
        if (!errorFlag) {
            strcpy(left_text, output); make_binary_text(output, left_text, operation, right_text);
            leftNode = newNode(op_str, leftNode, rightNode);
            if (operation == '*') value *= right_value;
            else if (right_value == 0.0) divisionByZero = 1;
            else value /= right_value;
        }
    }
    lastNode = leftNode;
    return value;
}

/* Parses addition and subtraction from left to right. */
double expr(char *output) {
    double value = term(output);
    int leftNode = lastNode;
    while (!errorFlag && (tokType[cur] == '+' || tokType[cur] == '-')) {
        char operation = tokType[cur], right_text[OUTPUT_LIMIT], left_text[OUTPUT_LIMIT];
        char op_str[2] = { operation, '\0' };
        double right_value;
        int rightNode;
        cur++; right_value = term(right_text);
        rightNode = lastNode;
        if (!errorFlag) {
            strcpy(left_text, output); make_binary_text(output, left_text, operation, right_text);
            leftNode = newNode(op_str, leftNode, rightNode);
            if (operation == '+') value += right_value; else value -= right_value;
        }
    }
    lastNode = leftNode;
    return value;
}

/* Runs both phases and prints the same syntax, precedence, and value lines. */
void process_expression(char *input) {
    char precedence[OUTPUT_LIMIT];
    double value = 0.0;
    printf("Expression : %s\n", input);
    lexer(input); hasIdentifier = 0; divisionByZero = 0; cur = 0; precedence[0] = '\0';
    nodeCount = 0; lastNode = -1;
    /* Give a blank line its own error message. */
    if (!errorFlag && tokCount == 1) { strcpy(errorMessage, "empty expression"); errorFlag = 1; }
    if (!errorFlag) value = expr(precedence);
    if (!errorFlag && tokType[cur] != 'E') token_error("unexpected token '", tokText[cur], tokPos[cur]);
    /* Print the value only when the complete input is syntactically valid. */
    if (errorFlag) printf("Syntax     : INVALID (%s)\n\n", errorMessage);
    else {
        printf("Syntax     : VALID\nPrecedence : %s\n", precedence);
        if (hasIdentifier) printf("Value      : not available (contains identifiers)\n");
        else if (divisionByZero) printf("Value      : Division by zero\n");
        else if (value >= -2147483648.0 && value <= 2147483647.0 && value == (double)(int)value)
            printf("Value      : %.0f\n", value);
        else printf("Value      : %g\n", value);
        printf("Tree       :\n");
        printTree(lastNode, 0);
        printf("\n");
    }
}

/* Compares a line with a command without using another library function. */
int is_command(char *line, char *command) {
    int i = 0;
    while (line[i] != '\0' && command[i] != '\0' && line[i] == command[i]) i++;
    return line[i] == '\0' && command[i] == '\0';
}

/* Reads one argument or processes input lines until end of input. */
int main(int argc, char *argv[]) {
    char input[INPUT_LIMIT];
    if (argc > 1) { process_expression(argv[1]); return 0; }
    /* Remove line endings and retain comment and quit handling. */
    while (fgets(input, sizeof(input), stdin) != NULL) {
        int length = (int)strlen(input);
        while (length > 0 && (input[length - 1] == '\n' || input[length - 1] == '\r')) input[--length] = '\0';
        if (is_command(input, "quit") || is_command(input, "exit")) break;
        if (input[0] != '#') process_expression(input);
    }
    return 0;
}
