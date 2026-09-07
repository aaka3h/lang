# Lang

Lang is an experimental programming language written in C, with a lexer, parser, tree-walking interpreter, bytecode compiler, and stack-based virtual machine. It also includes small web-server and SQLite modules.

## Build from source

The source uses POSIX APIs. Use a Unix-like development environment with Git, Make, a C99 compiler, and the SQLite development headers and library. The HTTP client module additionally needs the `curl` command at runtime.

```bash
git clone https://github.com/aaka3h/lang.git
cd lang
make
./lang examples/fibonacci.lang
```

The [Makefile](Makefile) includes the web and database modules and links both the math and SQLite libraries. The equivalent compiler command is:

```bash
gcc -Wall -std=c99 -O2 lexer.c parser.c interp.c compiler.c vm_impl.c langweb.c langdb.c main.c -o lang -lm -lsqlite3
```

To install the built executable in `/usr/local/bin`:

```bash
make install
```

Use the source build above: the current `install.sh` omits required web/database source files and SQLite linkage.

## Usage

```bash
./lang                         # start the bytecode-VM REPL
./lang file.lang               # run a file with the tree interpreter
./lang --tree file.lang        # explicitly select the tree interpreter
./lang --vm file.lang          # run supported syntax with the bytecode VM
./lang --disasm file.lang      # print bytecode, then execute it with the VM
./lang --test                  # run the built-in integration checks and demos
```

After installation, use `lang` in place of `./lang`. The REPL supports `:help`, `:ast <code>`, `:dis <code>`, `:tokens <code>`, and `exit`.

The tree interpreter supports more language features than the compiler/VM. Use normal file execution for the web, database, class, and other feature examples. The `make test` target currently refers to a missing `tests.lang`; use `./lang --test` to invoke the checks embedded in [main.c](main.c), and inspect their reported pass/fail results.

## Language at a glance

Save this as `hello.lang` and run `./lang hello.lang`:

```text
let name = "Lang"

fn add(a, b) {
    return a + b
}

print "Hello {name}!"
print add(2, 3)

let nums = [1, 2, 3]
push(nums, 4)
print nums[-1]
```

The tree interpreter implements variables, arithmetic and comparisons, string interpolation, multiline strings, arrays, dictionaries, functions, conditionals, loops, `break` and `continue`, classes and inheritance, `try`/`catch`/`throw`, and imports of user `.lang` files.

## Standard library

Import a module to make its functions available in the current environment.

| Import | Examples |
| --- | --- |
| `import "math"` | `sin`, `cos`, `tan`, `sqrt`, `pi`, `e` |
| `import "string"` | `upper`, `lower`, `trim`, `split`, `join`, `format` |
| `import "io"` | `readfile`, `writefile`, `appendfile`, `input` |
| `import "sys"` | `clock`, `exit` |
| `import "lint"` | Static code analysis helpers |
| `import "json"` | `json_encode`, `json_decode` |
| `import "random"` | `random`, `randint`, `shuffle`, `choice` |
| `import "http"` | `http_get` (runs curl), `time_now` |
| `import "web"` | `route`, `serve`, `html`, `json_response`, `redirect`, `route_static` |
| `import "db"` | `db_open`, `db_exec`, `db_query`, `db_close` |

The modules are small experimental implementations. For example, JSON decoding supports a limited set of flat objects and arrays.

## Web example

The included [app.lang](app.lang) defines a home page, a JSON time endpoint, and a hello page:

```bash
./lang app.lang
```

Visit [localhost:8080](http://localhost:8080), [/api/time](http://localhost:8080/api/time), or [/hello](http://localhost:8080/hello). Stop the server with Ctrl+C. The server binds to all network interfaces.

A minimal route looks like this:

```text
import "web"

fn home() {
    html("<h1>Hello from Lang!</h1>")
}

route("/", home)
serve(8080)
```

## Database example

```text
import "db"

let d = db_open("app.db")
db_exec(d, "CREATE TABLE IF NOT EXISTS notes (id INTEGER PRIMARY KEY, title TEXT)")
db_exec(d, "INSERT INTO notes (title) VALUES ('Hello Lang')")

let rows = db_query(d, "SELECT * FROM notes")
let note = rows[0]
print note["title"]

db_close(d)
```

Database and file paths are relative to the process's working directory.

## Examples

- [fibonacci.lang](examples/fibonacci.lang): recursion.
- [calculator.lang](examples/calculator.lang): classes and arithmetic.
- [todo.lang](examples/todo.lang): arrays and file I/O.
- [guess.lang](examples/guess.lang): random numbers and input.
- [webapp.lang](examples/webapp.lang): HTML and JSON routes.
- [database.lang](examples/database.lang): SQLite queries.
- [notes.lang](examples/notes/notes.lang): an unfinished web/database demo. Its save route inserts fixed placeholder text, and the referenced `public/style.css` is not included.

## Architecture

```text
Source -> Lexer -> Tokens -> Parser -> AST -> Tree interpreter -> Output
                                      |
                                      +-> Compiler -> Bytecode -> VM -> Output
```

Core files: [lexer.c](lexer.c), [parser.c](parser.c), [interp.c](interp.c), [compiler.c](compiler.c), [vm_impl.c](vm_impl.c), [stdlib.h](stdlib.h), [langweb.c](langweb.c), and [langdb.c](langdb.c).
