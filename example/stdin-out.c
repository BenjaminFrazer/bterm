#include <signal.h>
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include "unistd.h"
#include <termios.h>

struct termios saved_attributes;

void reset_input_mode (void)
{
  tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}

void set_input_mode (void)
{
  struct termios tattr;

  /* Make sure stdin is a terminal. */
  if (!isatty (STDIN_FILENO))
    {
      fprintf (stderr, "Not a terminal.\n");
      exit (EXIT_FAILURE);
    }

  /* Save the terminal attributes so we can restore them later. */
  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit (reset_input_mode);

  /* Set the funny terminal modes. */
  tcgetattr (STDIN_FILENO, &tattr);
  tattr.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
  tattr.c_cc[VMIN] = 1;
  tattr.c_cc[VTIME] = 0;
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &tattr);
}

/* sig interup handler */
volatile sig_atomic_t flag = 0;
void
sighandler (int val)
{
	flag = 1;
};

Write_data_t write_to_stdout;
int write_to_stdout(const char* data){
	printf("%s", data);
	return CLI_ERR_OK;
};

Read_data_t read_from_stdin;
int read_from_stdin(char* data, int n){
	int cnt=1;
	data[0] = getchar();
	if (data[0] == EOF){
		return -1;
	}
	return cnt;
//	for (int i =0; i<n; i++){
//		c = getchar();
//		if (c == EOF){
//			return -1;
//		}
//		if ((c<255)){
//			data[cnt]=c;
//			cnt++;
//		}
//	}
//	return cnt;
};

Command_Func_t print_hello_cmd;
int print_hello_cmd(int argc, char* argv[]){
	printf("Hello, world!\n");
	return CLI_ERR_OK;
};

Cli cli = {
	.debug = INFO,
	.read_data = &read_from_stdin,
	.write_data= &write_to_stdout,
	.commands = {
		//{.name="ECHO_TEST", .f=&echo_command},
		{.name="PRINT_HELLO", .f=&print_hello_cmd}
	}
};


int main(int argc, char* argv[]){
	char errmsg[100];
	printf("Starting CLI Demo...\n");
  signal(SIGINT, sighandler);
	set_input_mode();
	CLI_ERR err = cli_init(&cli);
	if (err != CLI_ERR_OK){
		printf("cli_init returned %d\n", (int)err);
	}
	while(1){
		if (flag){
			printf("Sigterm detected cleaning up\n");
			return EXIT_SUCCESS;
		}

		err = cli_handle_input(&cli);
		if (err != CLI_ERR_OK){
			snprintf(errmsg, sizeof(errmsg), "cli_handle_input returned %s\n", error_code_lut[err].desc);
			cli_print(&cli, errmsg);
			err = CLI_ERR_OK; // reset error
		}
	}
};
