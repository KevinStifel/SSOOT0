#include "process_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

ProcessInfo *processes = NULL; // arreglo dinámico de procesos
int process_count = 0;

void add_process(pid_t pid, char *name) {
    // Aumentar espacio dinámicamente
    process_count++;
    processes = realloc(processes, process_count * sizeof(ProcessInfo));
    if (!processes) {
        perror("realloc");
        exit(1);
    }

    // Inicializar el nuevo proceso en la última posición
    processes[process_count - 1].pid = pid;
    strncpy(processes[process_count - 1].name, name, 255);
    processes[process_count - 1].start_time = time(NULL);
    processes[process_count - 1].end_time = 0;
    processes[process_count - 1].exit_code = -1;
    processes[process_count - 1].signal_value = -1;
    processes[process_count - 1].state = RUNNING;
}


void handle_launch(char **input, int time_max) {
    pid_t pid = fork();

    if (pid == 0) {
        printf("Launched process '%s' with PID %d\n", input[1], pid);
        execvp(input[1], &input[1]);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        printf("Launched process '%s' with PID %d\n", input[1], pid);
        add_process(pid, input[1]);
    } else {
        perror("fork");
    }
}

void handle_status() {
    const char *state_str[] = {"RUNNING", "FINISHED"};

    printf("\n%-8s │ %-15s │ %-8s │ %-10s │ %-8s │ %-8s\n", 
           "PID", "NAME", "TIME(s)", "STATE", "EXIT", "SIGNAL");
    printf("─────────┼─────────────────┼──────────┼────────────┼──────────┼──────────\n");

    for (int i = 0; i < process_count; i++) {
        long elapsed = (processes[i].state == FINISHED)
            ? difftime(processes[i].end_time, processes[i].start_time)
            : difftime(time(NULL), processes[i].start_time);

        printf("%-8d │ %-15s │ %-8ld │ %-10s │ %-8d │ %-8d\n",
               processes[i].pid,
               processes[i].name,
               elapsed,
               state_str[processes[i].state],
               processes[i].exit_code,
               processes[i].signal_value);
    }
    printf("\n");
}

void handle_abort(int seconds) {
    // Revisar si hay procesos en ejecución
    int running_count = 0;
    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == RUNNING) {
            running_count++;
        }
    }

    if (running_count == 0) {
        printf("No hay procesos en ejecución. Abort no se puede ejecutar.\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Hijo → solo espera y notifica al padre
        sleep(seconds);
        kill(getppid(), SIGUSR1);
        exit(0);
    } else if (pid < 0) {
        perror("fork");
    }
    // Padre no se bloquea
}

void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < process_count; i++) {
            if (processes[i].pid == pid && processes[i].state == RUNNING) {
                processes[i].state = FINISHED;
                processes[i].end_time = time(NULL);

                if (WIFEXITED(status)) {
                    processes[i].exit_code = WEXITSTATUS(status);
                }
                if (WIFSIGNALED(status)) {
                    processes[i].signal_value = WTERMSIG(status);
                }
            }
        }
    }
}

void sigusr1_handler(int sig) {
    printf("Abort cumplido.\n");
    printf("%-8s %-12s %-6s %-6s %-6s\n", 
           "PID", "NAME", "TIME", "EXIT", "SIGNAL");

    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == RUNNING) {
            long elapsed = difftime(time(NULL), processes[i].start_time);
            printf("%-8d %-12s %-6ld %-6d %-6d\n",
                   processes[i].pid,
                   processes[i].name,
                   elapsed,
                   processes[i].exit_code,
                   processes[i].signal_value);

            // Enviar señal SIGTERM
            kill(processes[i].pid, SIGTERM);
        }
    }
}
