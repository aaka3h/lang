# Lang

A programming language built from scratch in C — with a built-in web framework.

## Install

    curl -fsSL https://raw.githubusercontent.com/aaka3h/lang/main/install.sh | bash

## Build from source

    gcc -Wall -std=c99 -O2 lexer.c parser.c interp.c compiler.c vm_impl.c langweb.c main.c -o lang -lm
    sudo cp lang /usr/local/bin/lang

## Usage

    lang                     # REPL
    lang file.lang           # run a file
    lang --disasm file.lang  # show bytecode

## Web Server

    import "web"

    fn home() {
        html("<h1>Hello from Lang!</h1>")
    }

    fn api() {
        import "json"
        json_response(json_encode({"status": "ok", "lang": "awesome"}))
    }

    route("/", home)
    route("/api", api)
    serve(8080)

Run with `lang app.lang` — visit http://localhost:8080

## Language Features

    # Variables
    let x = 42
    let name = "Lang"

    # String interpolation
    print "Hello {name}!"

    # Compound assignment
    let n = 10
    n += 5

    # Multi-line strings
    let text = """line one
    line two"""

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

    # Functions
    fn add(a, b) {
        return a + b
    }

    # Arrays with negative indexing
    let nums = [1, 2, 3]
    push(nums, 4)
    print nums[-1]

    # Dictionaries
    let person = {"name": "Lang", "version": 2}

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
        throw "oops"
    } catch (err) {
        print "caught: " + err
    }

    # User modules
    import "myutils.lang"

## Standard Library

    import "math"    # sin cos tan sqrt pi e
    import "string"  # upper lower trim split join format
    import "io"      # readfile writefile input
    import "sys"     # clock exit
    import "lint"    # static code analyzer
    import "json"    # json_encode json_decode
    import "random"  # random randint shuffle choice
    import "http"    # http_get time_now
    import "web"     # route serve html json_response

## Architecture

    Source → Lexer → Tokens → Parser → AST → Interpreter → Output
                                          ↓
                                       Compiler → Bytecode → VM → Output

Built from scratch. No libraries.

## Database

    import "db"

    let d = db_open("app.db")
    db_exec(d, "CREATE TABLE IF NOT EXISTS notes (id INTEGER PRIMARY KEY, title TEXT)")
    db_exec(d, "INSERT INTO notes (title) VALUES ('Hello Lang')")

    let rows = db_query(d, "SELECT * FROM notes")
    let note = rows[0]
    print note["title"]

    db_close(d)

## Example Apps

See the `examples/` folder:

- `fibonacci.lang` — recursion
- `calculator.lang` — classes and OOP
- `todo.lang` — arrays and file I/O
- `guess.lang` — random numbers and input
- `webapp.lang` — web server
- `database.lang` — SQLite
- `notes/` — full notes app with web + database + static CSS
