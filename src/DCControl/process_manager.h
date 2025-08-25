#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>
#include <time.h>

typedef enum {
    RUNNING,
    FINISHED
} ProcessState;

typedef struct {
    pid_t pid;            // PID del proceso
    char name[256];       // nombre del ejecutable
    time_t start_time;    // tiempo de inicio
    time_t end_time;    // tiempo de termino congelado al terminar
    int exit_code;        // código de salida (si terminó), -1 si no
    int signal_value;     // señal recibida, -1 si ninguna
    ProcessState state;         // 0 = corriendo, 1 = terminado
} ProcessInfo;

void handle_launch(char **input, int time_max);
void handle_status();
void handle_abort(int time);
void handle_shutdown();
void handle_emergency();
void sigchld_handler(int sig);
void sigusr1_handler(int sig);
#endif
