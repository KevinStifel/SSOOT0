#include "process_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

ProcessInfo *processes = NULL; // arreglo dinámico de procesos
int process_count = 0;
int shutdown_pending = 0;

void cleanup_all_processes(void) {
    free(processes);
    processes = NULL;
    process_count = 0;
}

void add_process(pid_t pid, char *name) {
    int new_count = process_count + 1;
    ProcessInfo *tmp = realloc(processes, (size_t)new_count * sizeof(ProcessInfo));
    if (!tmp) {
        perror("realloc");
        exit(1);
    }
    processes = tmp;
    process_count = new_count;
    processes[process_count - 1].pid = pid;

    size_t cap = sizeof(processes[process_count - 1].name);
    if (cap > 0) {
        strncpy(processes[process_count - 1].name, name ? name : "", cap - 1);
        processes[process_count - 1].name[cap - 1] = '\0';
    }

    processes[process_count - 1].start_time = time(NULL);
    processes[process_count - 1].end_time = 0;
    processes[process_count - 1].exit_code = -1;
    processes[process_count - 1].signal_value = -1;
    processes[process_count - 1].state = RUNNING;
}

void handle_launch(char **input, int time_max) {
    pid_t pid = fork();

    if (pid == 0) {
        setpgid(0, 0);
        printf("Launched process '%s' with PID %d\n", input[1], (int)getpid());
        execvp(input[1], &input[1]);
        perror("execvp");
        _exit(127);
    } else if (pid > 0) {
        printf("Launched process '%s' with PID %d\n", input[1], (int)pid);
        setpgid(pid, pid);

        add_process(pid, input[1]);
        if (time_max > 0) {
            pid_t w = fork();
            if (w == 0) { 
                sleep((unsigned)time_max);

                if (kill(pid, 0) == 0) {
                    kill(-pid, SIGTERM);           
                    sleep(5);                      
                    if (kill(pid, 0) == 0) {
                        kill(-pid, SIGKILL);       
                    }
                }
                _exit(0);
            } else if (w < 0) {
                perror("fork watcher");
            }
        }
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
            ? (long)difftime(processes[i].end_time, processes[i].start_time)
            : (long)difftime(time(NULL), processes[i].start_time);

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
    if (shutdown_pending == 0) {
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
            sleep((unsigned)seconds);
            kill(getppid(), SIGUSR1);
            _exit(0);
        } else if (pid < 0) {
            perror("fork");
        }
    }
}

void handle_shutdown() {
    int running_count = 0;
    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == RUNNING) {
            running_count++;
        }
    }

    if (running_count == 0) {
        printf("DCControl finalizado.\n");
        for (int i = 0; i < process_count; i++) {
            long elapsed = (long)difftime(
                (processes[i].state == FINISHED ? processes[i].end_time : time(NULL)),
                processes[i].start_time
            );
            printf("%d %s %ld %d %d\n",
                   processes[i].pid,
                   processes[i].name,
                   elapsed,
                   processes[i].exit_code,
                   processes[i].signal_value);
        }
        cleanup_all_processes();
        exit(0);
    }

    shutdown_pending = 1; 

    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == RUNNING) {
            if (kill(-processes[i].pid, 0) == 0) {  
                kill(-processes[i].pid, SIGINT);     
            }
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        sleep(10);

        for (int i = 0; i < process_count; i++) {
            if (processes[i].state == RUNNING) {
                if (kill(-processes[i].pid, 0) == 0) { 
                    kill(-processes[i].pid, SIGKILL);  
                }
            }
        }

        sleep(1);  
        kill(getppid(), SIGUSR2);
        _exit(0);
    }
}

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < process_count; i++) {
            if (processes[i].pid == pid) {
                processes[i].state = FINISHED;
                processes[i].end_time = time(NULL);

                if (WIFEXITED(status)) {
                    processes[i].exit_code = WEXITSTATUS(status);
                    processes[i].signal_value = 0;  
                }
                if (WIFSIGNALED(status)) {
                    processes[i].exit_code = -1;
                    processes[i].signal_value = WTERMSIG(status); 
                }
            }
        }
    }
}

void sigusr1_handler(int sig) {
    (void)sig;
    printf("Abort cumplido.\n");
    printf("%-8s %-12s %-6s %-6s %-6s\n",
           "PID", "NAME", "TIME", "EXIT", "SIGNAL");

    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == RUNNING) {
            long elapsed = (long)difftime(time(NULL), processes[i].start_time);
            printf("%-8d %-12s %-6ld %-6d %-6d\n",
                   processes[i].pid,
                   processes[i].name,
                   elapsed,
                   processes[i].exit_code,
                   processes[i].signal_value);

            kill(processes[i].pid, SIGTERM);
        }
    }
}

void sigusr2_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < process_count; i++) {
            if (processes[i].pid == pid) {
                processes[i].state = FINISHED;
                processes[i].end_time = time(NULL);

                if (WIFEXITED(status)) {
                    processes[i].exit_code = WEXITSTATUS(status);
                    processes[i].signal_value = 0; 
                }
                if (WIFSIGNALED(status)) {
                    processes[i].exit_code = -1;
                    processes[i].signal_value = WTERMSIG(status); 
                }
            }
        }
    }

    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == RUNNING) {
            processes[i].state = FINISHED;
            processes[i].end_time = time(NULL);
            processes[i].exit_code = -1;
            processes[i].signal_value = 9;  
        }
    }

    printf("DCControl finalizado.\n");
    for (int i = 0; i < process_count; i++) {
        long elapsed = (long)difftime(
            (processes[i].state == FINISHED ? processes[i].end_time : time(NULL)),
            processes[i].start_time
        );
        printf("%d %s %ld %d %d\n",
               processes[i].pid,
               processes[i].name,
               elapsed,
               processes[i].exit_code,
               processes[i].signal_value);
    }
    cleanup_all_processes();
    exit(0);
}



