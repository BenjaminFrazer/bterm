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
#define CLI_ERR_DESC(sev, ec) [CLI_ERR ## _ ## ec] = {.desc={#ec}, .lvl=sev}

const struct error_code_lut_row error_code_lut[] = {
	CLI_ERR_DESC(CLI_DBG_LVL_INFO, OK),
	CLI_ERR_DESC(CLI_DBG_LVL_ERR, NOT_IMPLEMENTED),
	CLI_ERR_DESC(CLI_DBG_LVL_WARN, INVALID_COMMAND),
	CLI_ERR_DESC(CLI_DBG_LVL_ERR, USER_CMD_FAILED),
	CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKOWN_ESCAPED_CHAR),
	CLI_ERR_DESC(CLI_DBG_LVL_ERR, READ),
	CLI_ERR_DESC(CLI_DBG_LVL_ERR, WRITE),
	CLI_ERR_DESC(CLI_DBG_LVL_WARN, BUFFER_OVERFLOW),
	CLI_ERR_DESC(CLI_DBG_LVL_WARN, DEV_NOT_INITIALISED),
	CLI_ERR_DESC(CLI_DBG_LVL_INFO, CURSOR_EXCEEDS_BOUNDS),
	CLI_ERR_DESC(CLI_DBG_LVL_INFO, CANNOT_DELETE),
	CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKNOWN_CTL_CHAR),
	CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKNOWN_SEQ_STATE),
	CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKNOWN_CSI_CHAR),
	CLI_ERR_DESC(CLI_DBG_LVL_ERR, HANDLE_KEYCODE),
	CLI_ERR_DESC(CLI_DBG_LVL_WARN, UNKOWN_KEYCODE),
	CLI_ERR_DESC(CLI_DBG_LVL_ERR, ESC_SEQ_BUFF_OVERFLOW),
	CLI_ERR_DESC(CLI_DBG_LVL_ERR, MAX_ERRORCODE),
};


char* _print_esc_seq(Cli* state){
	static char msg[MAX_ERR_MSG_CHARS];
				snprintf(msg, MAX_ERR_MSG_CHARS, "seq: %d-%d-%d-%d",
						 state->escape_buff[0],
						 state->escape_buff[1],
						 state->escape_buff[2],
						 state->escape_buff[3]);
	return &msg[0];
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

Command_Func_t echo;
int echo(Cli* state, int argn, char* argv[]){
	char buff[100];
	for (int i=0; i<argn; i++){
		snprintf(buff, sizeof(buff),"%s\r\n" , argv[i]);
		if (state->write_data(buff) !=0){
			return -1;
		}
	}
	return 0;
};

Command_t _builtin_commands[] = {
	{.f = &echo, .name="echo"}
};

CLI_ERR cli_init(Cli *state){
	/* TODO add some checking here */
	state->s = NORMAL;

	/* TODO pruge input buffer */
	return CLI_ERR_OK;
};

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
		sprintf(buff, esc_seq_cursor_right, n); 

	}
	else { // move left
		sprintf(buff, esc_seq_cursor_left, -n); 
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
		state->write_data(esc_seq_insert_char);
	}
	if (state->write_data(buff)!=0){
			return CLI_ERR_WRITE;
	}
	state->head++;
	state->hcursor++;
	//_move_cursor_horizontal(state, state->hcursor+1);
	//state->write_data(escSeqInsertChar);
	return CLI_ERR_OK;
};



Command_Func_t* _match_cmd(Cli * state, char* cmdname){
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
	const char delim[] = " ";
	int arg_next_idx =0;
	char* ptr = state->linebuff;
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
};

CLI_ERR _handle_ctrl_character(Cli* state, unsigned char c){
	CLI_ERR err;
	char msg[MAX_ERR_MSG_CHARS];
	switch (c){
		case 0: // Null
			err = CLI_ERR_NOT_IMPLEMENTED;
			break;
		case 8: // backspace
			err = _delete_char_leftof_cursor(state);
			break;
		case 9: // tab
			// TODO autocompletion
			err = cli_print(state, "Test printing");
			//err = CLI_ERR_NOT_IMPLEMENTED;
			break;
		case 10: // line feed (new line)
			state->write_data("\n\r");
			err = _execute_command_buff(state);
			_reset_prompt(state);
			break;
		case 13: // carriage return
			// do nothing
			state->write_data("\n\r");
			err = _execute_command_buff(state);
			_reset_prompt(state);
			//state->write_data("\nnewline");
			//_execute_command_buff(state);
			break;
		case 27: // escape
			state->s = ESC;
			err = CLI_ERR_OK;
			break;
		case 127: // DEL
			err = _delete_char_leftof_cursor(state);
			break;
		default: // most control characters won't be handled
			snprintf(msg, MAX_ERR_MSG_CHARS, "Unkown Control Character: %d", (int)c); 
			WARNING(msg);
			err = CLI_ERR_UNKNOWN_CTL_CHAR;
			break;
	}
	return err;
};

CLI_ERR _handle_printable_character(Cli* state, char c){
	return _insert_char_under_cursor(state, c);
};

CLI_ERR _navigate_history(Cli* state, int dir){

	return CLI_ERR_NOT_IMPLEMENTED;
};

CLI_ERR _handle_esc_character(Cli* state, char c){
	CLI_ERR err;
	if (state->escbuff_head >= sizeof(state->escape_buff)){
		ERROR(_print_esc_seq(state));
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
			err = CLI_ERR_UNKOWN_ESCAPED_CHAR;
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
			err = CLI_ERR_UNKOWN_KEYCODE;
			break;
	}
	return err;
};


CLI_ERR _handle_csi_character(Cli* state, char c){
	CLI_ERR err;
	if (state->escbuff_head >= sizeof(state->escape_buff)){
		ERROR(_print_esc_seq(state));
		state->s = NORMAL;
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
			sprintf(buff, "%c", c);
			state->escape_char_numeric_val=atoi(buff);
			break;
		default: // reset escape sequence state if this is the case
			if (((int)c>=0x40) && ((int)c<=0x7E)){ 
				// TODO this should be done with a for loop
				WARNING(_print_esc_seq(state));
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
	for (int i = 0; i<_error_count; i++){
		int idx = (_error_head-_error_count + MAX_ERR_MSG) % MAX_ERR_MSG;
		struct error_msg e = _error_msg_table[idx];
		snprintf(buff, sizeof(buff), "[%5s] %10s:%4d - %.40s", _err_type_lut[e.type], e.file, e.line, e.msg);
		cli_print(state, buff);
		_error_count--;
	}
};

CLI_ERR _handle_input_errors(Cli* state, CLI_ERR err){
	/* There may be some errors we wish to handle gracefully rather than raising 
		* further up the callstack.
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
	//int capacity = BUFF_MAX_CHARS - state->head;
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


