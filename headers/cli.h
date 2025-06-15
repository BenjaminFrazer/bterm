#ifndef _CLI
#define _CLI

#define MAX_COMMANDS                100
#define MAX_ARGS                    10
#define BUFF_MAX_CHARS              100
#define PROMPT_SIZE                 1
#define BUFF_MAX_ESC_CHARS          10
#define MAX_ERRORCODE_DESC          24
#define MAX_TOKENISATION_ATTEMPTS   (MAX_ARGS + 2)
#define MAX_HISTORY                 10

typedef struct _State Cli;

typedef enum {
    CLI_ERR_OK = 0,
    CLI_ERR_NOT_IMPLEMENTED,
    CLI_ERR_INVALID_COMMAND,
    CLI_ERR_USER_CMD_FAILED,
    CLI_ERR_READ,
    CLI_ERR_UNKNOWN_ESC_CHAR,
    CLI_ERR_UNKNOWN_CSI_CHAR,
    CLI_ERR_WRITE,
    CLI_ERR_BUFFER_OVERFLOW,
    CLI_ERR_DEV_NOT_INITIALISED,
    CLI_ERR_CURSOR_EXCEEDS_BOUNDS,
    CLI_ERR_CANNOT_DELETE,
    CLI_ERR_UNKNOWN_CTL_CHAR,
    CLI_ERR_UNKNOWN_SEQ_STATE,
    CLI_ERR_HANDLE_KEYCODE,
    CLI_ERR_UNKNOWN_KEYCODE,
    CLI_ERR_ESC_SEQ_BUFF_OVERFLOW,
    CLI_ERR_MAX_ERRORCODE,
} CLI_ERR;

enum severity{
    CLI_DBG_LVL_INFO = 0,
    CLI_DBG_LVL_WARN,
    CLI_DBG_LVL_ERR,
};

struct error_code_lut_row{
    const char desc[MAX_ERRORCODE_DESC];
    enum severity lvl;
};

extern const struct error_code_lut_row error_code_lut[];

typedef int (Command_Func_t)(Cli* state, int argc, char* argv[]);
typedef int (Read_data_t)(char* data, int n);
typedef int (Write_data_t)(const char* data);
typedef CLI_ERR (Hook_fcn_t)(Cli* state);

typedef struct Command_t {
    char name[20];
    Command_Func_t* f;
} Command_t;

enum ctl_seq_state {
    NORMAL = 0,
    ESC,
    CSI,
};

struct _State {
    enum ctl_seq_state s;
    int debug;
    int escape_char_numeric_val;
    int head;
    int hcursor;
    int escbuff_head;
    char escape_buff[BUFF_MAX_ESC_CHARS];
    char linebuff[BUFF_MAX_CHARS];
    char inputbuff[20];
    char history[MAX_HISTORY][BUFF_MAX_CHARS];
    int history_count;
    int history_head;
    int history_index;
    Read_data_t* read_data;
    Write_data_t* write_data;
    Command_t commands[MAX_COMMANDS];
    Hook_fcn_t* on_init_cplt_hook;
    Hook_fcn_t* pre_command_hook;
    Hook_fcn_t* post_command_hook;
};

CLI_ERR cli_print(Cli* state, const char* msg);

CLI_ERR cli_init(Cli* state);

CLI_ERR cli_handle_input(Cli* state);

#endif

