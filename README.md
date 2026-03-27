# Lang

A compiled programming language built from scratch in C.

Lang has a full pipeline: lexer → parser → AST → bytecode compiler → stack-based VM. It also includes a tree-walking interpreter, pretty error messages, and a standard library.

## Features

- Lexer, recursive descent parser, AST
- Bytecode compiler + stack VM (like CPython)
- Tree-walking interpreter
- Standard library: math, string, io, sys
- Pretty error messages with line/column highlighting
- Interactive REPL with :ast, :dis, :tokens debug commands

## Build

    gcc -Wall -std=c99 -O2 lexer.c parser.c interp.c compiler.c vm_impl.c main.c -o lang -lm
    sudo cp lang /usr/local/bin/lang

## Usage

    lang                      # interactive REPL
    lang file.lang            # run a program
    lang --disasm file.lang   # show bytecode
    lang --test               # run test suite

## Example

    fn factorial(n) {
        if n <= 1 { return 1 }
        return n * factorial(n - 1)
    }
    print factorial(10)

## Architecture

    Source → Lexer → Tokens → Parser → AST → Compiler → Bytecode → VM → Output

Built as a learning project. Every phase written from scratch, no libraries.
