# Makefile – Arithmetic Expression Compiler (Front End)
#
# Targets:
#   all     – build compiler binary (aec.exe on Windows, aec on Linux)
#   test    – run test suite against tests.txt
#   clean   – remove compiled object files and executable
#   run-web – start the Node.js web server

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -g
LDFLAGS = -lm

ifeq ($(OS),Windows_NT)
    TARGET    = aec.exe
    CLEAN_CMD = cmd /c del /f /q $(OBJS) $(TARGET) aec 2>nul || exit 0
else
    TARGET    = aec
    CLEAN_CMD = rm -f $(OBJS) $(TARGET) aec.exe
endif

SRCS    = main.c lexer.c parser.c ast.c errors.c api.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all test clean run-web

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

main.o:   main.c   lexer.h parser.h ast.h errors.h api.h
lexer.o:  lexer.c  lexer.h errors.h
parser.o: parser.c parser.h lexer.h ast.h errors.h
ast.o:    ast.c    ast.h
errors.o: errors.c errors.h
api.o:    api.c    api.h lexer.h parser.h ast.h errors.h

test: $(TARGET)
	./$(TARGET) tests.txt

clean:
	$(CLEAN_CMD)

run-web: $(TARGET)
	node web/server.js
