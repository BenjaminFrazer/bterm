#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Escape sequence - Cursor forward (right) */
static const char *esc_seq_cursor_right = "\x1B[%dC";

/** Escape sequence - Cursor backward (left) */
static const char *esc_seq_cursor_left = "\x1B[%dD";

/** Escape sequence - Horizontal absolute */
static const char *esc_seq_cursor_horizontal_n = "\x1B[%dG";

/** Escape sequence - Cursor insert character (ICH) */
static const char *esc_seq_insert_char = "\x1B[@";

/** Escape sequence - Cursor delete character (DCH) */
static const char *esc_seq_delete_char = "\x1B[P";

/** Escape sequence - Cursor delete full line */
static const char *esc_seq_delete_line = "\x1B[2K";

/** Escape sequence - Clear full screen and move cursor home */
static const char *esc_seq_clear_screen_home = "\x1B[2J\x1B[H";

static int _count_completions(Cli* state, const char* prefix);
static const char* _nth_completion(Cli* state, const char* prefix, int n);
static CLI_ERR _complete_buffer(Cli* state);

#ifdef CLI_ENABLE_HISTORY
static void _add_to_history(Cli* state, const char* line);
static CLI_ERR _replace_line_with_text(Cli* state, const char* text);
static CLI_ERR _exit_history_navigation(Cli* state);
#endif

/* Forward declarations */
Command_Func_t help;
Command_Func_t echo;
Command_Func_t clc;


#define MAX_ERR_MSG 10
#define MAX_ERR_MSG_CHARS 40
#define MAX_FILE_NAME_CHARS 20

struct error_msg {
	enum severity type;
	int line;
	char file[MAX_FILE_NAME_CHARS];
	char msg[MAX_ERR_MSG_CHARS];
};

int _error_head = 0;
int _error_count = 0;
struct error_msg _error_msg_table[MAX_ERR_MSG]; 

void _error_message(const char* msg, const char* file, int line, enum severity type){
	memset(_error_msg_table[_error_head].msg, 0, sizeof(_error_msg_table[_error_head].msg));
	memset(_error_msg_table[_error_head].file, 0, sizeof(_error_msg_table[_error_head].file));
	strncpy(_error_msg_table[_error_head].msg, msg, sizeof(_error_msg_table[_error_head].msg));
	strncpy(_error_msg_table[_error_head].file, file, sizeof(_error_msg_table[_error_head].file));
	_error_msg_table[_error_head].line = line;
	_error_msg_table[_error_head].type = type;
	_error_head = (_error_head+1) % MAX_ERR_MSG;
	if (_error_count < MAX_ERR_MSG){
		_error_count++;
	}
};


#define WARNING(msg) _error_message(msg, __FILE__, __LINE__, CLI_DBG_LVL_WARN)
#define ERROR(msg) _error_message(msg, __FILE__, __LINE__, CLI_DBG_LVL_ERR)
#define CLI_ERR_DESC(sev, ec) {.desc={#ec}, .lvl=sev}

const struct error_code_lut_row error_code_lut[] = {
	[CLI_ERR_OK] = CLI_ERR_DESC(CLI_DBG_LVL_INFO, OK),
	[CLI_ERR_NOT_IMPLEMENTED] = CLI_ERR_DESC(CLI_DBG_LVL_ERR, NOT_IMPLEMENTED),
	[CLI_ERR_INVALID_COMMAND] = CLI_ERR_DESC(CLI_DBG_LVL_WARN, INVALID_COMMAND),
	[CLI_ERR_USER_CMD_FAILED] = CLI_ERR_DESC(CLI_DBG_LVL_ERR, USER_CMD_FAILED),
	[CLI_ERR_READ] = CLI_ERR_DESC(CLI_DBG_LVL_ERR, READ),
	[CLI_ERR_UNKNOWN_ESCAPED_CHAR] = CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKNOWN_ESCAPED_CHAR),
	[CLI_ERR_UNKNOWN_CSI_CHAR] = CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKNOWN_CSI_CHAR),
	[CLI_ERR_WRITE] = CLI_ERR_DESC(CLI_DBG_LVL_ERR, WRITE),
	[CLI_ERR_BUFFER_OVERFLOW] = CLI_ERR_DESC(CLI_DBG_LVL_WARN, BUFFER_OVERFLOW),
	[CLI_ERR_DEV_NOT_INITIALISED] = CLI_ERR_DESC(CLI_DBG_LVL_WARN, DEV_NOT_INITIALISED),
	[CLI_ERR_CURSOR_EXCEEDS_BOUNDS] = CLI_ERR_DESC(CLI_DBG_LVL_INFO, CURSOR_EXCEEDS_BOUNDS),
	[CLI_ERR_CANNOT_DELETE] = CLI_ERR_DESC(CLI_DBG_LVL_INFO, CANNOT_DELETE),
	[CLI_ERR_UNKNOWN_CTL_CHAR] = CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKNOWN_CTL_CHAR),
	[CLI_ERR_UNKNOWN_SEQ_STATE] = CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKNOWN_SEQ_STATE),
	[CLI_ERR_HANDLE_KEYCODE] = CLI_ERR_DESC(CLI_DBG_LVL_ERR, HANDLE_KEYCODE),
	[CLI_ERR_UNKNOWN_KEYCODE] = CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKNOWN_KEYCODE),
	[CLI_ERR_ESC_SEQ_BUFF_OVERFLOW] = CLI_ERR_DESC(CLI_DBG_LVL_ERR, ESC_SEQ_BUFF_OVERFLOW),
	[CLI_ERR_MAX_ERRORCODE] = CLI_ERR_DESC(CLI_DBG_LVL_ERR, MAX_ERRORCODE),
};


void _print_esc_seq(Cli* state, char* msg, size_t msg_size){
	snprintf(msg, msg_size, "seq: %d-%d-%d-%d",
		 state->escape_buff[0],
		 state->escape_buff[1],
		 state->escape_buff[2],
		 state->escape_buff[3]);
};

CLI_ERR cli_print(Cli* state, const char* msg){
	const char newline[] = "\n";
	const char carriage_return[] = "\r";
	char buff_horizontal[8];
	snprintf(buff_horizontal, sizeof(buff_horizontal), esc_seq_cursor_horizontal_n, state->hcursor+1); 
	const char* writes[] = {
		esc_seq_delete_line,
		&carriage_return[0],
		msg,
		&carriage_return[0],
		&newline[0],
		&state->linebuff[0],
		&buff_horizontal[0],
	};
	for (int i = 0; i< sizeof(writes)/sizeof(char*); i++){
		if (state->write_data(writes[i])!=0){
			char msgbuff[MAX_ERR_MSG_CHARS];
			snprintf(msgbuff, MAX_ERR_MSG_CHARS, "Failed to write sequence: %s to device", writes[i]); 
			ERROR(msgbuff);
			return CLI_ERR_WRITE;
		};
	}
	return CLI_ERR_OK;
};

int echo(Cli* state, int argn, char* argv[]){
	char buff[100];
	for (int i=0; i<argn; i++){
		snprintf(buff, sizeof(buff),"%s\r\n" , argv[i]);
		if (state->write_data(buff) !=0){
			return -1;
		}
	}
	return 0;
}

int clc(Cli* state, int argn, char* argv[])
{
	(void)argn;
	(void)argv;

	if (state == NULL || state->write_data == NULL){
		return -1;
	}

	if (state->write_data(esc_seq_clear_screen_home) != 0){
		return -1;
	}

	return 0;
}

Command_t _builtin_commands[] = {
	{.name="clc", .desc="Clear terminal screen", .f=&clc},
	{.name="clear", .desc="Alias for clc", .f=&clc},
	{.name="help", .desc="Display available commands and usage info", .f=&help},
	{.name="echo", .desc="Print arguments to output", .f=&echo}
};

int help(Cli* state, int argn, char* argv[]){
	char buff[100];

	if (argn > 1){
		/* Show help for specific command */
		const char* cmdname = argv[1];
		Command_t* found = NULL;

		/* Search builtin commands */
		for (int i=0; i<sizeof(_builtin_commands)/sizeof(Command_t); i++){
			if (strcmp(cmdname, _builtin_commands[i].name)==0){
				found = &_builtin_commands[i];
				break;
			}
		}

		/* Search user commands */
		if (found == NULL){
			for (int i=0; i<MAX_COMMANDS; i++){
				if (state->commands[i].name[0] == '\0'){
					break;
				}
				if (strcmp(cmdname, state->commands[i].name)==0){
					found = &state->commands[i];
					break;
				}
			}
		}

		if (found != NULL){
			snprintf(buff, sizeof(buff), "%s: %s\r\n", found->name, found->desc);
			if (state->write_data(buff) !=0){
				return -1;
			}
		} else {
			snprintf(buff, sizeof(buff), "Command '%s' not found\r\n", cmdname);
			if (state->write_data(buff) !=0){
				return -1;
			}
		}
		return 0;
	}

	/* List all commands */
	if (state->write_data("Available commands:\r\n") !=0){
		return -1;
	}

	/* List builtin commands */
	for (int i=0; i<sizeof(_builtin_commands)/sizeof(Command_t); i++){
		snprintf(buff, sizeof(buff), "  %-15s %s\r\n",
			_builtin_commands[i].name, _builtin_commands[i].desc);
		if (state->write_data(buff) !=0){
			return -1;
		}
	}

	/* List user commands */
	for (int i=0; i<MAX_COMMANDS; i++){
		if (state->commands[i].name[0] == '\0'){
			break;
		}
		snprintf(buff, sizeof(buff), "  %-15s %s\r\n",
			state->commands[i].name, state->commands[i].desc);
		if (state->write_data(buff) !=0){
			return -1;
		}
	}

	if (state->write_data("\r\nUse 'help <command>' for more info\r\n") !=0){
		return -1;
	}

	return 0;
}

CLI_ERR cli_init(Cli *state){
        if (state == NULL) {
                return CLI_ERR_DEV_NOT_INITIALISED;
        }
        if (state->read_data == NULL || state->write_data == NULL) {
                return CLI_ERR_DEV_NOT_INITIALISED;
        }

        state->s = NORMAL;
        state->completing = 0;
        state->completion_head = 0;
        state->completion_idx = 0;
        state->head = 0;
        state->hcursor = 0;
        state->escbuff_head = 0;
        memset(state->completion_buff, 0, sizeof(state->completion_buff));
        memset(state->linebuff, 0, sizeof(state->linebuff));
        memset(state->escape_buff, 0, sizeof(state->escape_buff));
        memset(state->inputbuff, 0, sizeof(state->inputbuff));

#ifdef CLI_ENABLE_HISTORY
        state->history_write_head = 0;
        state->history_count = 0;
        state->history_nav_pos = -1;
        state->history_temp_cursor = 0;
        state->history_temp_head = 0;
        memset(state->history_buffer, 0, sizeof(state->history_buffer));
        memset(state->history_temp_line, 0, sizeof(state->history_temp_line));
#endif

        return CLI_ERR_OK;
};

CLI_ERR cli_register_command(Cli* state, const char* name, const char* desc, Command_Func_t* func) {
        if (state == NULL || name == NULL || func == NULL) {
                return CLI_ERR_INVALID_COMMAND;
        }

        /* Find empty slot */
        for (int i = 0; i < MAX_COMMANDS; i++) {
                if (state->commands[i].name[0] == '\0') {
                        /* Found empty slot - populate it */
                        strncpy(state->commands[i].name, name, sizeof(state->commands[i].name) - 1);
                        state->commands[i].name[sizeof(state->commands[i].name) - 1] = '\0';
                        if (desc != NULL) {
                                strncpy(state->commands[i].desc, desc, sizeof(state->commands[i].desc) - 1);
                                state->commands[i].desc[sizeof(state->commands[i].desc) - 1] = '\0';
                        } else {
                                state->commands[i].desc[0] = '\0';
                        }
                        state->commands[i].f = func;
                        return CLI_ERR_OK;
                }
        }

        /* No empty slot found */
        return CLI_ERR_BUFFER_OVERFLOW;
}

CLI_ERR _cursor_bounds_check(Cli* state, int pos){
	if (pos > state->head){
		return CLI_ERR_CURSOR_EXCEEDS_BOUNDS;
	}
	if (pos < 0){
		return CLI_ERR_CURSOR_EXCEEDS_BOUNDS;
	}
	return CLI_ERR_OK;
};

CLI_ERR _move_cursor_horizontal(Cli* state, int n){
	char buff[10];
	CLI_ERR err = _cursor_bounds_check(state, state->hcursor+n);
	if (err!=CLI_ERR_OK){
		return err;
	}
	/* Truncate cursor position */
	if (n>0){ // move right
		snprintf(buff, sizeof(buff), esc_seq_cursor_right, n);

	}
	else { // move left
		snprintf(buff, sizeof(buff), esc_seq_cursor_left, -n);
		}
	if (state->write_data(buff)!=0){
			return CLI_ERR_WRITE;
	}
	state->hcursor = state->hcursor+n;
	return CLI_ERR_OK;
};

CLI_ERR _delete_char_leftof_cursor(Cli* state){
	CLI_ERR err = CLI_ERR_OK;
	if(state->hcursor == 0){
		WARNING("Cannot delete when cursor is at [0]");
		return CLI_ERR_CANNOT_DELETE; // do nothing
	}
	int tail = state->head - state->hcursor + 1;
	memmove(&state->linebuff[state->hcursor-1], &state->linebuff[state->hcursor], tail);

	err = _move_cursor_horizontal(state, -1);
	if (err != CLI_ERR_OK){
		return err;
	}
	if (state->write_data(esc_seq_delete_char)!=0){
			return CLI_ERR_WRITE;
	}
	state->head--;
	return CLI_ERR_OK;
};

CLI_ERR _insert_char_under_cursor(Cli* state, char c){
	char buff[2] = {0};
	buff[0] = c;
	if ((state->head+1)>=sizeof(state->linebuff)){
		return CLI_ERR_BUFFER_OVERFLOW;
	}
	int tail = state->head - state->hcursor + 1;
	memmove(&state->linebuff[state->hcursor+1], &state->linebuff[state->hcursor], tail);
	state->linebuff[state->hcursor] = c;
	if(state->head != state->hcursor){
		if (state->write_data(esc_seq_insert_char)!=0){
			return CLI_ERR_WRITE;
		}
	}
	if (state->write_data(buff)!=0){
			return CLI_ERR_WRITE;
	}
	state->head++;
	state->hcursor++;
	return CLI_ERR_OK;
};



Command_Func_t* _match_cmd(Cli * state, const char* cmdname){
	/* Match against built-in commands */
	if (cmdname == NULL){
		return NULL;
	}
	if (cmdname[0] == '\0'){
		return NULL;
	}
	for (int i=0; i<sizeof(_builtin_commands)/sizeof(Command_t); i++){
		if (strcmp(cmdname, _builtin_commands[i].name)==0){
			return _builtin_commands[i].f;
		}
	};
	/* Match against user commands */
	for (int i=0; i<MAX_COMMANDS; i++){
		/* Check for empty command slot (end of command list) */
		if (state->commands[i].name[0] == '\0'){
			break;
		}
		if (strcmp(cmdname, state->commands[i].name)==0){
			return state->commands[i].f;
		}
	}
	char buff[MAX_ERR_MSG_CHARS];
	snprintf(buff, MAX_ERR_MSG_CHARS, "Cannot find command %s", cmdname);
	WARNING(buff);
	return NULL;
};

int _check_tokens(char* tok){
	return 1;
};

CLI_ERR _execute_command_buff(Cli* state){
	static char* argv[MAX_ARGS];
	static char cmdbuff[BUFF_MAX_CHARS];  /* Local copy for parsing */
	const char delim[] = " ";
	int arg_next_idx =0;

	/* Copy command to local buffer and clear linebuff BEFORE execution.
	 * This prevents cli_print() from re-drawing old command text during
	 * command execution (cli_print re-draws linebuff after each message). */
	memcpy(cmdbuff, state->linebuff, sizeof(cmdbuff));
	memset(state->linebuff, 0, sizeof(state->linebuff));
	state->head = 0;
	state->hcursor = 0;

	char* ptr = cmdbuff;
	for (int i = 0; i<MAX_TOKENISATION_ATTEMPTS; i++){
		char* tok = strtok(ptr, delim);
		if (tok == NULL){
			break;
		}
		if (_check_tokens(tok)){
			argv[arg_next_idx] = tok;
			arg_next_idx++;
		}
		ptr = NULL; // this is done for strtok
	}
	Command_Func_t* f = _match_cmd(state, argv[0]);
	if (f == NULL){
		return CLI_ERR_INVALID_COMMAND;
	}
	if (f(state, arg_next_idx, argv) !=0){
		return CLI_ERR_USER_CMD_FAILED;
	}
	return CLI_ERR_OK;
};

void _reset_prompt(Cli* state){

        memset(state->linebuff, 0, sizeof(state->linebuff));
        state->hcursor = 0;
        state->head = 0;
        state->completing = 0;
        state->completion_idx = 0;
};

CLI_ERR _handle_ctrl_character(Cli* state, unsigned char c){
	CLI_ERR err;
	char msg[MAX_ERR_MSG_CHARS];
	switch (c){
		case 0: // Null
			err = CLI_ERR_NOT_IMPLEMENTED;
			break;
                case 8: // backspace
                        state->completing = 0;
                        state->completion_idx = 0;
#ifdef CLI_ENABLE_HISTORY
                        _exit_history_navigation(state);
#endif
                        err = _delete_char_leftof_cursor(state);
                        break;
                case 9: // tab
                        if(state->hcursor != state->head){
                                err = CLI_ERR_OK;
                                break;
                        }
#ifdef CLI_ENABLE_HISTORY
                        _exit_history_navigation(state);
#endif
                        if(!state->completing){
                                state->completing = 1;
                                state->completion_head = state->head;
                                state->completion_idx = 0;
                                memcpy(state->completion_buff, state->linebuff,
                                       sizeof(state->linebuff));
                        }
                        err = _complete_buffer(state);
                        break;
                case 10: // line feed (new line)
                        state->completing = 0;
                        state->completion_idx = 0;
                        if (state->write_data("\n\r")!=0){
                                err = CLI_ERR_WRITE;
                                break;
                        }
#ifdef CLI_ENABLE_HISTORY
                        _add_to_history(state, state->linebuff);
#endif
                        err = _execute_command_buff(state);
#ifdef CLI_ENABLE_HISTORY
                        _exit_history_navigation(state);
#endif
                        _reset_prompt(state);
                        break;
                case 13: // carriage return
                        state->completing = 0;
                        state->completion_idx = 0;
                        if (state->write_data("\n\r")!=0){
                                err = CLI_ERR_WRITE;
                                break;
                        }
#ifdef CLI_ENABLE_HISTORY
                        _add_to_history(state, state->linebuff);
#endif
                        err = _execute_command_buff(state);
#ifdef CLI_ENABLE_HISTORY
                        _exit_history_navigation(state);
#endif
                        _reset_prompt(state);
                        break;
                case 27: // escape
                        if(state->completing){
                                while(state->head > state->completion_head){
                                        CLI_ERR e = _delete_char_leftof_cursor(state);
                                        if(e!=CLI_ERR_OK){
                                                err = e;
                                                break;
                                        }
                                }
                                memcpy(state->linebuff, state->completion_buff,
                                       sizeof(state->linebuff));
                                state->head = state->completion_head;
                                state->hcursor = state->head;
                                state->completing = 0;
                                state->completion_idx = 0;
                                err = CLI_ERR_OK;
                        } else {
                                state->completing = 0;
                                state->completion_idx = 0;
                                state->s = ESC;
                                err = CLI_ERR_OK;
                        }
                        break;
                case 127: // DEL
                        state->completing = 0;
                        state->completion_idx = 0;
#ifdef CLI_ENABLE_HISTORY
                        _exit_history_navigation(state);
#endif
                        err = _delete_char_leftof_cursor(state);
                        break;
                default: // most control characters won't be handled
                        state->completing = 0;
                        state->completion_idx = 0;
                        snprintf(msg, MAX_ERR_MSG_CHARS, "Unknown Control Character: %d", (int)c);
                        WARNING(msg);
                        err = CLI_ERR_UNKNOWN_CTL_CHAR;
                        break;
	}
	return err;
};

CLI_ERR _handle_printable_character(Cli* state, char c){
        state->completing = 0;
        state->completion_idx = 0;
#ifdef CLI_ENABLE_HISTORY
        _exit_history_navigation(state);
#endif
        return _insert_char_under_cursor(state, c);
};

#ifdef CLI_ENABLE_HISTORY
static void _add_to_history(Cli* state, const char* line){
	/* Skip empty lines */
	#if HISTORY_IGNORE_EMPTY
	if (line[0] == '\0'){
		return;
	}
	#endif

	/* Skip consecutive duplicates */
	#if HISTORY_IGNORE_CONSECUTIVE_DUPES
	if (state->history_count > 0){
		int last_idx = (state->history_write_head - 1 + MAX_HISTORY_ENTRIES) % MAX_HISTORY_ENTRIES;
		if (strcmp(state->history_buffer[last_idx], line) == 0){
			return;
		}
	}
	#endif

	/* Add to circular buffer */
	strncpy(state->history_buffer[state->history_write_head], line, BUFF_MAX_CHARS - 1);
	state->history_buffer[state->history_write_head][BUFF_MAX_CHARS - 1] = '\0';

	state->history_write_head = (state->history_write_head + 1) % MAX_HISTORY_ENTRIES;

	if (state->history_count < MAX_HISTORY_ENTRIES){
		state->history_count++;
	}
}

static CLI_ERR _replace_line_with_text(Cli* state, const char* text){
	/* Move cursor to beginning */
	while (state->hcursor > 0){
		CLI_ERR err = _move_cursor_horizontal(state, -1);
		if (err != CLI_ERR_OK){
			return err;
		}
	}

	/* Clear entire line */
	if (state->write_data(esc_seq_delete_line) != 0){
		return CLI_ERR_WRITE;
	}

	/* Reset buffers */
	memset(state->linebuff, 0, sizeof(state->linebuff));
	state->head = 0;
	state->hcursor = 0;

	/* Insert new text character by character */
	for (int i = 0; text[i] != '\0' && i < BUFF_MAX_CHARS - 1; i++){
		CLI_ERR err = _insert_char_under_cursor(state, text[i]);
		if (err != CLI_ERR_OK){
			return err;
		}
	}

	return CLI_ERR_OK;
}

static CLI_ERR _exit_history_navigation(Cli* state){
	if (state->history_nav_pos == -1){
		return CLI_ERR_OK; // not navigating
	}

	state->history_nav_pos = -1;
	return CLI_ERR_OK;
}
#endif

CLI_ERR _navigate_history(Cli* state, int dir){
#ifdef CLI_ENABLE_HISTORY
	/* dir > 0: go back (older), dir < 0: go forward (newer) */

	/* If history is empty, do nothing */
	if (state->history_count == 0){
		return CLI_ERR_OK;
	}

	/* Going forward when not navigating - do nothing */
	if (dir < 0 && state->history_nav_pos == -1){
		return CLI_ERR_OK;
	}

	/* First time navigating - save current line */
	if (state->history_nav_pos == -1){
		strncpy(state->history_temp_line, state->linebuff, BUFF_MAX_CHARS - 1);
		state->history_temp_line[BUFF_MAX_CHARS - 1] = '\0';
		state->history_temp_cursor = state->hcursor;
		state->history_temp_head = state->head;

		/* Start at most recent entry */
		state->history_nav_pos = (state->history_write_head - 1 + MAX_HISTORY_ENTRIES) % MAX_HISTORY_ENTRIES;

		return _replace_line_with_text(state, state->history_buffer[state->history_nav_pos]);
	}

	/* Already navigating */
	if (dir > 0){
		/* Go back (older) */
		int oldest_idx;
		if (state->history_count < MAX_HISTORY_ENTRIES){
			oldest_idx = 0;
		} else {
			oldest_idx = state->history_write_head;
		}

		/* Check if at oldest */
		if (state->history_nav_pos == oldest_idx){
			return CLI_ERR_OK; // stay at oldest
		}

		state->history_nav_pos = (state->history_nav_pos - 1 + MAX_HISTORY_ENTRIES) % MAX_HISTORY_ENTRIES;
		return _replace_line_with_text(state, state->history_buffer[state->history_nav_pos]);
	} else {
		/* Go forward (newer) */
		int next_pos = (state->history_nav_pos + 1) % MAX_HISTORY_ENTRIES;
		int most_recent_idx = (state->history_write_head - 1 + MAX_HISTORY_ENTRIES) % MAX_HISTORY_ENTRIES;

		/* Check if we're at the most recent - restore temp buffer */
		if (state->history_nav_pos == most_recent_idx){
			state->history_nav_pos = -1;
			return _replace_line_with_text(state, state->history_temp_line);
		}

		state->history_nav_pos = next_pos;
		return _replace_line_with_text(state, state->history_buffer[state->history_nav_pos]);
	}
#else
	return CLI_ERR_NOT_IMPLEMENTED;
#endif
};

CLI_ERR _handle_esc_character(Cli* state, char c){
	CLI_ERR err;
	if (state->escbuff_head >= sizeof(state->escape_buff)){
		char msg[MAX_ERR_MSG_CHARS];
		_print_esc_seq(state, msg, sizeof(msg));
		ERROR(msg);
		state->s = NORMAL;
		state->escbuff_head= 0;
		return CLI_ERR_ESC_SEQ_BUFF_OVERFLOW;
	}
	state->escape_buff[state->escbuff_head] = c;
	state->escbuff_head++;
	switch (c) {
		case '[':
			state->s = CSI;
			err = CLI_ERR_OK;
			break;
               default:
                       state->s = NORMAL;
                       state->escbuff_head = 0;
                       err = CLI_ERR_UNKNOWN_ESCAPED_CHAR;
	}
	return err;
};

CLI_ERR _handle_keycode_sequence(Cli* state){
	CLI_ERR err;
	state->escape_buff[state->escbuff_head-1] = '\n';
	int num = atoi(&state->escape_buff[1]);
	if (num ==0){
		return CLI_ERR_HANDLE_KEYCODE;
	}
	switch(num){
		case 3: // Delete
			err = _delete_char_leftof_cursor(state);
			break;
               default:
                       err = CLI_ERR_UNKNOWN_KEYCODE;
                       break;
	}
	return err;
};


CLI_ERR _handle_csi_character(Cli* state, char c){
	CLI_ERR err;
	if (state->escbuff_head >= sizeof(state->escape_buff)){
		char msg[MAX_ERR_MSG_CHARS];
		_print_esc_seq(state, msg, sizeof(msg));
		ERROR(msg);
		state->s = NORMAL;
		state->escbuff_head = 0;
		return CLI_ERR_ESC_SEQ_BUFF_OVERFLOW;
	}
	state->escape_buff[state->escbuff_head] = c;
	state->escbuff_head++;

	switch (c) {
		case 'A': // up
			err = _navigate_history(state, 1);
			break;
		case 'B': // down
			err = _navigate_history(state, -1);
			break;
		case 'C': // right
			err = _move_cursor_horizontal(state, 1);
			break;
		case 'D': // left
			err = _move_cursor_horizontal(state, -1);
			break;
		case '~':
			err = _handle_keycode_sequence(state);
			break;	
		case '0' ... '9': // parameter byte
			err = CLI_ERR_OK;
			char buff[2];
			snprintf(buff, sizeof(buff), "%c", c);
			state->escape_char_numeric_val=atoi(buff);
			break;
               default: // reset escape sequence state if this is the case
                        if (((int)c>=0x40) && ((int)c<=0x7E)){
                                char msg[MAX_ERR_MSG_CHARS];
                                _print_esc_seq(state, msg, sizeof(msg));
                                WARNING(msg);
                                err = CLI_ERR_UNKNOWN_CSI_CHAR;
                        }
                        else {
                                err = CLI_ERR_OK;
                        }
                        break;
        }

        /* Reset escape mode if char has Range of values for final byte */
        if (((int)c>=0x40) && ((int)c<=0x7E)){
                state->s = NORMAL;
                state->escbuff_head = 0;
        }
        return err;
};

static int _count_completions(Cli* state, const char* prefix){
        size_t plen = strlen(prefix);
        int count = 0;
        for(int i=0; i<sizeof(_builtin_commands)/sizeof(Command_t); i++){
                if(strncmp(prefix, _builtin_commands[i].name, plen)==0){
                        count++;
                }
        }
        for(int i=0; i<MAX_COMMANDS; i++){
                if(state->commands[i].name[0]=='\0'){
                        continue;
                }
                if(strncmp(prefix, state->commands[i].name, plen)==0){
                        count++;
                }
        }
        return count;
}

static const char* _nth_completion(Cli* state, const char* prefix, int n){
        size_t plen = strlen(prefix);
        int count = 0;
        for(int i=0; i<sizeof(_builtin_commands)/sizeof(Command_t); i++){
                if(strncmp(prefix, _builtin_commands[i].name, plen)==0){
                        if(count==n){
                                return _builtin_commands[i].name;
                        }
                        count++;
                }
        }
        for(int i=0; i<MAX_COMMANDS; i++){
                if(state->commands[i].name[0]=='\0'){
                        continue;
                }
                if(strncmp(prefix, state->commands[i].name, plen)==0){
                        if(count==n){
                                return state->commands[i].name;
                        }
                        count++;
                }
        }
        return NULL;
}

static CLI_ERR _complete_buffer(Cli* state){
        while(state->head > state->completion_head){
                CLI_ERR e = _delete_char_leftof_cursor(state);
                if(e!=CLI_ERR_OK){
                        return e;
                }
        }
        char prefix[BUFF_MAX_CHARS];
        memcpy(prefix, state->completion_buff, state->completion_head);
        prefix[state->completion_head] = '\0';

        int total = _count_completions(state, prefix);
        if(total == 0){
                return CLI_ERR_OK;
        }
        int idx = state->completion_idx % total;
        const char* match = _nth_completion(state, prefix, idx);
        if(match==NULL){
                return CLI_ERR_OK;
        }

        size_t match_len = strlen(match);
        for(size_t i=state->completion_head; i<match_len; i++){
                CLI_ERR e = _insert_char_under_cursor(state, match[i]);
                if(e!=CLI_ERR_OK){
                        return e;
                }
        }
        state->completion_idx = (idx + 1) % total;
        return CLI_ERR_OK;
}

CLI_ERR _handle_char(Cli* state, unsigned char c){
	CLI_ERR err;
	switch (state->s){
		case NORMAL:
			if ((c < 32) || ((c>126 ) && (c < 160))){
				err = _handle_ctrl_character(state, c);
			}
			else {
				err = _handle_printable_character(state, c);
			}
			break;
		case ESC:
			err = _handle_esc_character(state, c);
			break;
		case CSI:
			err = _handle_csi_character(state, c);
			break;
		default:
			err = CLI_ERR_UNKNOWN_SEQ_STATE;
			break;
	}
	return err;
};

int _print_err(Cli* state, CLI_ERR err){
	return 1;
};

void _print_errors(Cli* state){
        char buff[100];
        char _err_type_lut[][5] = {
                [CLI_DBG_LVL_ERR]="ERR",
                [CLI_DBG_LVL_WARN]="WARN",
                [CLI_DBG_LVL_INFO]="INFO",
        };
        int count = _error_count;
        int start = (_error_head - count + MAX_ERR_MSG) % MAX_ERR_MSG;
        for (int i = 0; i < count; i++){
                int idx = (start + i) % MAX_ERR_MSG;
                struct error_msg e = _error_msg_table[idx];
                snprintf(buff, sizeof(buff), "[%5s] %10s:%4d - %.40s", _err_type_lut[e.type], e.file, e.line, e.msg);
                cli_print(state, buff);
        }
        _error_count = 0;
};

CLI_ERR _handle_input_errors(Cli* state, CLI_ERR err){
	/* There may be some errors we wish to handle gracefully rather than raising 
		* further up the call-stack.
	*/
	
	if ((err < CLI_ERR_MAX_ERRORCODE)){
		if ((error_code_lut[err].lvl >= state->debug) && (err != CLI_ERR_OK)){
			cli_print(state, error_code_lut[err].desc);
		}
		// filter future errors
		if (error_code_lut[err].lvl < CLI_DBG_LVL_ERR){
			err = CLI_ERR_OK;
		}
	}
	else {
		err = CLI_ERR_MAX_ERRORCODE;
	}
	_print_errors(state);
	_error_count = 0;

	return err;
};

CLI_ERR cli_handle_input(Cli * state){
	memset(state->inputbuff, 0, sizeof(state->inputbuff)); // clear input buffer
	int n_read = state->read_data(state->inputbuff, sizeof(state->inputbuff));
	if (n_read>=0){
		for (int i =0; i<n_read; i++){
			CLI_ERR err = _handle_char(state, state->inputbuff[i]);
			err = _handle_input_errors(state, err);
			if (err != CLI_ERR_OK){
				return err;
			}
		}
	}
	else {
		return CLI_ERR_READ;
	}
	return CLI_ERR_OK;
};


