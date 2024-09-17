
#include "cli.h" 
#include <stdio.h>

Command_Func_t  string_conversion;
// takes in 5 strings inputted by user, converts them to integers and then converts them back to strings
int string_conversion(Cli* state, int nargs, char* args[]){
    if (nargs != 6);{ //6 arguments as the command takes up 1
        write_data("Invalid number of inputs");
        return -1;
    }

    char *end;//Pointer to be used to error check
    long numbers[5];//converted strings are stored here
    char buffer[5][20];//where the converted strings are stored
    for (int i = 1; i <=5; i++){

        numbers[i-1] = strtol(args[i], &end, 10);//converts the string to an integer (long)
        if (end == args[i] || *end != '\0'){
            snprintf(buffer[0], sizeof(buffer[0]), "ERROR: Input '%s' is not vaild\n", args[i]);
            write_data(buffer[0]);
            return -1; //in case the conversion fails, e.g. not suitable string

        }

        snprintf(buffer[i-1], sizeof(buffer[i-1], "%ld", numbers[i-1] ));
        
    }
    write_data("Converted values:")
    for (int i=  0; i < 5; i++){
        write_data(buffer[i]);//could utilise another snprintf function to output "Value 1 is:" etc
        write_data("\n");
    }
    return 0;
}