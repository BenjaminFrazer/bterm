# bterm - Lightweight Terminal/CLI Library for C

A minimal, portable command-line interface (CLI) library written in C, designed for embedded systems and lightweight applications. Particularly suitable for ESP32 and Arduino platforms via PlatformIO.

## Features

- **Lightweight and Portable**: Written in pure C with minimal dependencies
- **Command Registration System**: Easy registration of custom commands with function callbacks
- **Line Editing Support**: Full cursor movement, character insertion/deletion via escape sequences
- **Tab Completion**: Built-in support for command name completion
- **Error Handling**: Comprehensive error codes with severity levels (INFO, WARN, ERR)
- **Customizable I/O**: Flexible read/write function pointers for various I/O backends (UART, USB, stdio)
- **Hook System**: Pre/post command hooks and initialization callbacks
- **Buffer Management**: Configurable buffer sizes for commands, arguments, and input
- **Escape Sequence Handling**: Proper handling of terminal control sequences (arrow keys, delete, etc.)

## Installation

### Standard C Project

Clone the repository and include the headers in your project:

```bash
git clone https://github.com/BenjaminFrazer/bterm.git
```

Add to your build system:
- Include path: `-Iheaders`
- Source file: `src/cli.c`

### PlatformIO (ESP32/Arduino)

Add to your `platformio.ini`:

```ini
lib_deps = 
    https://github.com/BenjaminFrazer/bterm.git
```

## Quick Start

```c
#include "cli.h"
#include <stdio.h>

// Define I/O functions
int my_read(char* data, int n) {
    // Read up to n bytes into data
    // Return number of bytes read, or -1 on error
}

int my_write(const char* data) {
    // Write null-terminated string
    // Return 0 on success, non-zero on error
}

// Define a custom command
int hello_cmd(Cli* state, int argc, char* argv[]) {
    cli_print(state, "Hello, World!");
    return CLI_ERR_OK;
}

// Initialize CLI
Cli cli = {
    .debug = CLI_DBG_LVL_INFO,
    .read_data = &my_read,
    .write_data = &my_write,
    .commands = {
        {.name = "hello", .f = &hello_cmd},
        {.name = "", .f = NULL}  // Terminator
    }
};

int main() {
    cli_init(&cli);
    
    while(1) {
        CLI_ERR err = cli_handle_input(&cli);
        if (err != CLI_ERR_OK) {
            // Handle error
        }
    }
}
```

## API Reference

### Core Functions

- `CLI_ERR cli_init(Cli* state)` - Initialize the CLI state
- `CLI_ERR cli_handle_input(Cli* state)` - Process input characters (call in main loop)
- `CLI_ERR cli_print(Cli* state, const char* msg)` - Print message without disrupting current line

### State Structure

The `Cli` structure contains:
- `read_data` - Function pointer for reading input
- `write_data` - Function pointer for writing output
- `commands[]` - Array of registered commands (max 100)
- `debug` - Debug level (INFO/WARN/ERR)
- Optional hooks for customization

### Error Codes

All functions return `CLI_ERR` enum values:
- `CLI_ERR_OK` - Success
- `CLI_ERR_INVALID_COMMAND` - Unknown command
- `CLI_ERR_BUFFER_OVERFLOW` - Input buffer full
- And more... (see `cli.h` for complete list)

## Examples

See the `example/` directory for a complete working example using standard I/O:

```bash
make examples
./example/stdin-out.out
```

The example demonstrates:
- Terminal raw mode setup
- Signal handling (Ctrl+C)
- Custom command registration
- Error handling

## Building

### Make Targets

- `make all` - Clean and run tests
- `make test` - Build and run unit tests
- `make examples` - Build example programs
- `make clean` - Remove built files

### Requirements

- C11 compatible compiler
- Make build system
- Unity test framework (for tests, optional)

## Configuration

Key configuration defines in `cli.h`:

```c
#define MAX_COMMANDS 100        // Maximum registered commands
#define MAX_ARGS 10            // Maximum arguments per command
#define BUFF_MAX_CHARS 100     // Line buffer size
```

## Platform Support

- **Linux/Unix**: Full support with example
- **Windows**: Should work with appropriate I/O functions
- **ESP32**: Via PlatformIO/Arduino framework
- **STM32**: Portable C code, provide UART I/O functions
- **AVR/Arduino**: Memory constraints may require buffer size adjustment

## License

MIT License - See [LICENSE](LICENSE) file for details

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Author

Benjamin Frazer - [GitHub](https://github.com/BenjaminFrazer)

## Acknowledgments

- Unity test framework for unit testing support
- PlatformIO for embedded platform integration