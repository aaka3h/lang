# Lang

A programming language built from scratch in C.

Full pipeline: lexer → parser → AST → bytecode compiler → stack VM.

## Features

- Lexer, recursive descent parser, AST
- Bytecode compiler + stack VM
- Tree-walking interpreter
- Variables, strings, booleans, null
- Arrays with negative indexing `arr[-1]`
- Dictionaries
- Classes, inheritance, super()
- try / catch / throw
- break and continue
- Integer division `//`
- Compound assignment `+=` `-=` `*=` `/=`
- String interpolation `"Hello {name}"`
- Multi-line strings `"""..."""`
- User-defined modules `import "myfile.lang"`
- else if chains
- Accurate error messages with line numbers
- Built-in static analyzer (lint module)
- Standard library: math, string, io, sys, lint
- Interactive REPL

## Install

    curl -fsSL https://raw.githubusercontent.com/aaka3h/lang/main/install.sh | bash

## Build (from source)

    gcc -Wall -std=c99 -O2 lexer.c parser.c interp.c compiler.c vm_impl.c main.c -o lang -lm
    sudo cp lang /usr/local/bin/lang

## Usage

    lang                     # REPL
    lang file.lang           # run a file
    lang --disasm file.lang  # show bytecode
    lang --vm file.lang      # run with bytecode VM

## Syntax

    # Variables
    let x = 42
    let name = "Lang"

    # String interpolation
    print "Hello {name}!"

    # Multi-line strings
    let text = """line one
    line two
    line three"""

    # Compound assignment
    let n = 10
    n += 5
    n *= 2

    # Functions
    fn add(a, b) {
        return a + b
    }

    # if / else if / else
    if x == 1 { print "one" }
    else if x == 2 { print "two" }
    else { print "other" }

    # Loops with break and continue
    let i = 0
    while i < 10 {
        if i == 5 { break }
        i += 1
    }

    # Arrays
    let nums = [1, 2, 3]
    push(nums, 4)
    print nums[-1]

    # Dictionaries
    let person = {"name": "Lang", "version": 2}
    print person["name"]

    # Classes and inheritance
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

    # Error handling
    try {
        throw "something went wrong"
    } catch (err) {
        print "caught: " + err
    }

    # User modules
    import "myutils.lang"

## Modules

    import "math"    # sin cos tan sqrt pi e
    import "string"  # upper lower trim replace split join format
    import "io"      # readfile writefile input
    import "sys"     # clock exit
    import "lint"    # built-in static code analyzer
    import "json"    # json_encode json_decode
    import "random"  # random randint shuffle choice
    import "http"    # http_get time_now

## Architecture

    Source → Lexer → Tokens → Parser → AST → Interpreter → Output
                                          ↓
                                       Compiler → Bytecode → VM → Output

Built from scratch. No libraries. Every phase hand-written in C.
