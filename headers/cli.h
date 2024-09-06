#ifndef _CLI
#define _CLI

#define MAX_COMMANDS 100
#define MAX_ARGS 10
#define BUFF_MAX_CHARS 100
#define PROMPT_SIZE 1
#define BUFF_MAX_ESC_CHARS 10
#define MAX_ERRORCODE_DESC 24
#define MAX_TOKENISATION_ATTEMPTS (MAX_ARGS+2)

/* Forward declaration of state struct. */
typedef struct _State Cli; 

typedef enum {
	CLI_ERR_OK = 0,
	CLI_ERR_NOT_IMPLEMENTED,
	CLI_ERR_INVALID_COMMAND,
	CLI_ERR_USER_CMD_FAILED,
	CLI_ERR_READ,
	CLI_ERR_UNKOWN_ESCAPED_CHAR,
	CLI_ERR_UNKNOWN_CSI_CHAR,
	CLI_ERR_WRITE,
	CLI_ERR_BUFFER_OVERFLOW,
	CLI_ERR_DEV_NOT_INITIALISED,
	CLI_ERR_CURSOR_EXCEEDS_BOUNDS,
	CLI_ERR_CANNOT_DELETE,
	CLI_ERR_UNKNOWN_CTL_CHAR,
	CLI_ERR_UNKNOWN_SEQ_STATE,
	CLI_ERR_HANDLE_KEYCODE,
	CLI_ERR_UNKOWN_KEYCODE,
	CLI_ERR_ESC_SEQ_BUFF_OVERFLOW,
	CLI_ERR_MAX_ERRORCODE, // remains at the end of enum
} CLI_ERR;

//extern const char error_code_lut[][MAX_ERRORCODE_DESC];
//

enum severity {
	CLI_DBG_LVL_INFO = 0,
	CLI_DBG_LVL_WARN,
	CLI_DBG_LVL_ERR,
};

struct error_code_lut_row {
	const char desc[MAX_ERRORCODE_DESC];
	enum severity lvl;
};

extern const struct error_code_lut_row error_code_lut[];

typedef int (Command_Func_t)(Cli* state, int argc, char* argv[]);

typedef int (Read_data_t)(char* data, int n);

typedef int (Write_data_t)(const char* data);

typedef CLI_ERR (Hook_fcn_t)(Cli* state);

typedef struct Command_t {
/* User customisable hook functions. */
	char name[20];
	Command_Func_t* f;
} Command_t;

enum ctl_seq_state {
	NORMAL = 0,
	ESC,
	CSI,
};

/* To be populated by the user before init. */
struct _State {
/* User customisable read/write data functions. Must be defined for cli to work. */
	enum ctl_seq_state s; // current control state
	int debug; // debug mode
	int escape_char_numeric_val;
	int head; // number of chars in buffer
	int hcursor; // position of the horizontal cursor
	int escbuff_head; // position of next slot in the escape sequence buffer
	char escape_buff[BUFF_MAX_ESC_CHARS];
	char linebuff[BUFF_MAX_CHARS];
	char inputbuff[20];
	Read_data_t* read_data;
	Write_data_t* write_data;
/* Array of supported commands. */
	Command_t commands[MAX_COMMANDS];
/* User customisable hook functions. Set to NULL if not in use. */
	Hook_fcn_t* on_init_cplt_hook;
	Hook_fcn_t* pre_command_hook;
	Hook_fcn_t* post_command_hook;
};


/* Print function to avoid mangeling of text currently in line buffer */
CLI_ERR cli_print(Cli* state, const char* msg);

CLI_ERR cli_init(Cli* state);

CLI_ERR cli_handle_input(Cli* state);

#endif



