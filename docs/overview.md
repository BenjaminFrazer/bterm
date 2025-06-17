# Overview

The repository implements a lightweight command-line interface (CLI) library written in C. It is structured like a typical embedded/PlatformIO library:

```
bterm/
├── LICENSE
├── Makefile
├── library.json        (metadata for PlatformIO)
├── headers/            (public headers)
│   └── cli.h
├── src/                (implementation)
│   └── cli.c
├── example/            (demo program)
│   └── stdin-out.c
└── lib/Unity/          (placeholder for Unity unit test framework)
```

## Key Components

1. **Header definitions (`headers/cli.h`):**
   - Defines limits such as buffer sizes and argument counts (`MAX_COMMANDS`, `BUFF_MAX_CHARS`, etc.)
   - Enumerates all CLI error codes (`CLI_ERR_*`)
   - Defines severity levels for debugging output (`CLI_DBG_LVL_*`)
   - Describes the main `Cli` state structure which holds buffers, function pointers for reading/writing characters, command registrations, and optional hooks
   - Declares the public API: `cli_init`, `cli_handle_input`, and a print helper `cli_print`

2. **Core implementation (`src/cli.c`):**
   - Maintains escape sequences for cursor movement and editing (e.g., cursor right/left, insert/delete)
   - Provides an error-logging mechanism that stores recent messages and prints them according to severity levels
   - Implements `cli_print` which safely displays text without disrupting the current line buffer
   - Includes a simple built-in command (`echo`) and an initialization routine (`cli_init`)
   - Contains routines for cursor movement, inserting and deleting characters, parsing commands, handling escape sequences, and processing input one byte at a time. The main input loop is exposed via `cli_handle_input`

3. **Example program (`example/stdin-out.c`):**
   - Demonstrates integrating the CLI with standard input/output using custom `read_from_stdin` and `write_to_stdout` functions.
   - Shows how to register a user command (`PRINT_HELLO`) and process keyboard input inside a loop

4. **Build files:**
   - `Makefile` builds the source files, optional tests, and the example program. It references the Unity test framework (folder currently empty)
   - `library.json` defines metadata for usage with PlatformIO (Arduino framework targeting ESP32)

## Pointers for Getting Started

- **Understand the `Cli` state struct:** review `cli.h` lines 72-91 to see which fields must be populated (I/O callbacks, command list, optional hooks).
- **Integrate with hardware or the host OS:** supply functions matching `Read_data_t` and `Write_data_t` that interact with your UART, USB, or standard I/O.
- **Register commands:** each command is a `Command_Func_t`. The example shows how to register `PRINT_HELLO` in the `commands` array.
- **Handling escape sequences:** the CLI expects characters such as arrow keys or delete to send escape sequences; the code in `cli.c` handles them to move the cursor or edit the buffer.

## Next Steps to Explore

1. **Error reporting and debugging:** check how `_handle_input_errors` filters and reports error codes based on the `debug` level.
2. **Extending functionality:** consider implementing features marked with `TODO` in `cli.c`, such as input buffer checks and auto-completion.
3. **Testing:** the repository references Unity for unit tests; adding actual tests under a `test/` directory (using the provided Makefile) would help validate behavior.
4. **PlatformIO integration:** `library.json` suggests using this library in an Arduino/ESP32 project. Explore PlatformIO’s documentation for how to include the library and compile for your board.

Overall, the project offers a compact, extensible CLI suitable for embedded environments or simple host applications. The example program (`example/stdin-out.c`) is a good starting point for experimentation and understanding how to hook the library into your own project.
