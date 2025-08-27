
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "../input_manager/manager.h"
#include "process_manager.h"

int main(int argc, char const *argv[]) {
    set_buffer();
    signal(SIGCHLD, sigchld_handler); // Para manejar hijos terminados
    signal(SIGUSR1, sigusr1_handler); // Para abort
    signal(SIGUSR2, sigusr2_handler); // Para shutdown
    int time_max = (argc > 1) ? atoi(argv[1]) : -1; // -1 si no hay límite

    while (1) {
        char **input = read_user_input();

        if (strcmp(input[0], "launch") == 0) {
            handle_launch(input, time_max);

        } else if (strcmp(input[0], "status") == 0) {
            handle_status();

        } else if (strcmp(input[0], "abort") == 0) {
            handle_abort(atoi(input[1]));

        } else if (strcmp(input[0], "shutdown") == 0) {
            handle_shutdown();

        } else if (strcmp(input[0], "emergency") == 0) {
            // handle_emergency();
            printf("[DEBUG] Emergency command received\n");
            break;
        }

        free_user_input(input);
    }

    return 0;
}


