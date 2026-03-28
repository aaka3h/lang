# Lang

A programming language built from scratch in C.

Full pipeline: lexer → parser → AST → bytecode compiler → stack VM.

## Features

- Lexer, recursive descent parser, AST
- Bytecode compiler + stack VM
- Tree-walking interpreter
- Variables, strings, booleans, null
- Arrays with negative indexing
- Dictionaries
- Classes, inheritance, super()
- try / catch / throw
- break and continue
- Integer division //
- String format(), split(), join()
- Built-in static analyzer (lint module)
- Standard library: math, string, io, sys, lint
- Pretty error messages
- Interactive REPL

## Build

    gcc -Wall -std=c99 -O2 lexer.c parser.c interp.c compiler.c vm_impl.c main.c -o lang -lm
    sudo cp lang /usr/local/bin/lang

## Usage

    lang                   # REPL
    lang file.lang         # run a file
    lang --disasm file.lang  # show bytecode
    lang --vm file.lang    # run with bytecode VM

## Example

    fn fibonacci(n) {
        if n <= 1 { return n }
        return fibonacci(n - 1) + fibonacci(n - 2)
    }

    let i = 0
    while i < 10 {
        print fibonacci(i)
        i = i + 1
    }

## Classes

    class Animal {
        fn init(name) { self.name = name }
        fn speak() { print self.name + " speaks" }
    }

    class Dog extends Animal {
        fn init(name) { super(name) }
        fn speak() { print self.name + " barks" }
    }

    let d = Dog("Rex")
    d.speak()

## Modules

    import "math"    # sin cos tan sqrt pi e
    import "string"  # upper lower trim replace split join format
    import "io"      # readfile writefile input
    import "sys"     # clock exit
    import "lint"    # static code analyzer

## Architecture

    Source → Lexer → Tokens → Parser → AST → Interpreter → Output
                                          ↓
                                       Compiler → Bytecode → VM → Output

Built from scratch. No libraries. Every phase hand-written in C.
