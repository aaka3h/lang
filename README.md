# Lang

A programming language built from scratch in C.

Full pipeline: lexer → parser → AST → bytecode compiler → stack VM.

## Features

- Lexer, recursive descent parser, AST
- Bytecode compiler + stack VM (like CPython)
- Tree-walking interpreter
- Arrays, dictionaries, classes, inheritance
- try / catch / throw error handling
- Standard library: math, string, io, sys
- Pretty error messages with line/column highlighting
- Interactive REPL

## Build

    gcc -Wall -std=c99 -O2 lexer.c parser.c interp.c compiler.c vm_impl.c main.c -o lang -lm
    sudo cp lang /usr/local/bin/lang

## Usage

    lang                      # interactive REPL
    lang file.lang            # run a program
    lang --tree file.lang     # run with tree interpreter
    lang --disasm file.lang   # show bytecode
    lang --test               # run test suite

## Syntax

    # Variables
    let x = 42
    let name = "Lang"

    # Functions
    fn add(a, b) {
        return a + b
    }

    # Arrays
    let nums = [1, 2, 3]
    push(nums, 4)
    print nums[0]

    # Dictionaries
    let person = {"name": "Lang", "version": 1}
    print person["name"]

    # Classes
    class Animal {
        fn init(name) {
            self.name = name
        }
        fn speak() {
            print self.name + " speaks!"
        }
    }
    let dog = Animal("Rex")
    dog.speak()

    # Inheritance
    class Dog extends Animal {
        fn init(name) {
            super(name)
        }
        fn speak() {
            print self.name + " barks"
        }
    }
    let d = Dog("Rex")
    d.speak()

    # try/catch
    try {
        throw "something went wrong"
    } catch (err) {
        print "caught: " + err
    }

    # Loops
    for let i = 0; i < 10; i = i + 1 {
        print i
    }

## Modules

    import "math"    # sin cos tan log sqrt pi e
    import "string"  # upper lower trim replace contains
    import "io"      # readfile writefile input
    import "sys"     # clock exit

## Architecture

    Source → Lexer → Tokens → Parser → AST → Compiler → Bytecode → VM → Output

Built from scratch. No libraries. Every phase hand-written in C.
