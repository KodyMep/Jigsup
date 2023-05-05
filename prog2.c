#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>




typedef struct Process {
    int PID;
    int PR;
    int numCPUBurst;
    int numIOBurst;
    int *CPUBurst;
    int *IOBurst;
    int cpuindex;
    int ioindex;
    int remaining_burst_time;
    struct Process *prev;
    struct Process *next;
	int arrival_time;
	int completion_time;
	int waiting_time;

} Process;

typedef struct Queue {
    Process *head;
    Process *tail;
} Queue;



// Global variables

sem_t sem_cpu, sem_io;
int cpu_busy = 0;
int io_busy = 0;
int file_read_done = 0;
int cpu_sch_done = 0;
int io_sys_done = 0;
int QUANTUM;
char* ALG;
char* InputFile;
Queue ready_queue;
Queue io_queue;
int total_processes = 0;
int total_simulation_time = 0;
int total_cpu_busy_time = 0;
int total_turnaround_time = 0;
int total_waiting_time = 0;
int num_processes = 0;

// Function prototypes
void *cpu_scheduler(void *arg);
void *io_system(void *arg);
void *file_read(void *arg);
bool queue_is_empty(Queue *queue);
void queue_remove(Queue *queue, Process *process);
Process *FIFO(Queue *queue);
Process *SJF(Queue *queue);
Process *PR(Queue *queue);
Process *RR(Queue *queue, int quantum);
void terminate_process(Process *process);
Process *create_process(int PID, int PR, int numCPUBurst, int numIOBurst, int *CPUBurst, int *IOBurst);
Process **proc_list;
void queue_enqueue(Queue *queue, Process *process);
Process* queue_dequeue(Queue *queue);
pthread_mutex_t cpu_mutex;



int main(int argc, char *argv[]) {
    // Process command line args and get the simulation parameters

	/*if (argc != 4)
	 {
    		printf("Usage: %s ALG QUANTUM InputFile\n", argv[0]);
   	 	return 1;
	 }*/


      int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-alg") == 0 && i+1 < argc) {
            ALG = argv[i+1];
            i++;
            printf("Algorithm: %s\n", ALG); // Add this line for debugging
        } else if (strcmp(argv[i], "-quantum") == 0 && i+1 < argc) {
            QUANTUM = atoi(argv[i+1]);
            i++;
            printf("Quantum: %d\n", QUANTUM); // Add this line for debugging
        } else if (strcmp(argv[i], "-input") == 0 && i+1 < argc) {
            InputFile = argv[i+1];
            i++;
            printf("Input File: %s\n", InputFile); // Add this line for debugging
        } else {
            printf("Invalid arguments\n");
            printf("Usage: %s -alg [FIFO|SJF|PR|RR] [-quantum [integer(ms)]] -input [file name]\n", argv[0]);
            return 1;
        }
    } 

    if (InputFile == NULL) {
        printf("Input file not provided\n");
        printf("Usage: %s -alg [FIFO|SJF|PR|RR] [-quantum [integer(ms)]] -input [file name]\n", argv[0]);
        return 1;
    }


// Initialize mutex
pthread_mutex_t mutex;
pthread_mutex_init(&mutex, NULL);
pthread_mutex_init(&cpu_mutex, NULL);


ready_queue.head = NULL;
ready_queue.tail = NULL;
io_queue.head = NULL;
io_queue.tail = NULL;




    // Initialize semaphores
    if (sem_init(&sem_cpu, 0, 0) != 0) {
        perror("Error initializing sem_cpu semaphore");
        return 1;
    }

    if (sem_init(&sem_io, 0, 0) != 0) {
        perror("Error initializing sem_io semaphore");
        sem_destroy(&sem_cpu); // Clean up the previously initialized semaphore
        return 1;
    }

    // Create threads
    pthread_t cpu_thread, io_thread, file_read_thread;

    if (pthread_create(&cpu_thread, NULL, cpu_scheduler, (void *)&mutex) != 0) {
        perror("Error creating cpu_thread");
        sem_destroy(&sem_cpu); // Clean up semaphores
        sem_destroy(&sem_io);
        return 1;
    }

    if (pthread_create(&io_thread, NULL, io_system, (void *)&mutex) != 0) {
        perror("Error creating io_thread");
        pthread_cancel(cpu_thread); // Clean up previously created thread
        sem_destroy(&sem_cpu); // Clean up semaphores
        sem_destroy(&sem_io);
        return 1;
    }

    if (pthread_create(&file_read_thread, NULL, file_read, InputFile) != 0) {
        perror("Error creating file_read_thread");
        pthread_cancel(cpu_thread); // Clean up previously created threads
        pthread_cancel(io_thread);
        sem_destroy(&sem_cpu); // Clean up semaphores
        sem_destroy(&sem_io);
        return 1;
    }

    // Main loop
    while (1) {
        // If all threads have finished their work, break the loop
        pthread_mutex_lock(&mutex); // Protect shared variables with a mutex
        if (file_read_done && cpu_sch_done && io_sys_done) {
            pthread_mutex_unlock(&mutex); // Unlock mutex before breaking the loop
            break;
        }
        pthread_mutex_unlock(&mutex); // Unlock mutex at the end of each iteration
        sleep(1); // Sleep for a while to reduce CPU usage
    }

    // Wait for the threads to finish
    pthread_join(cpu_thread, NULL);
    pthread_join(io_thread, NULL);
    pthread_join(file_read_thread, NULL);

    // Clean up semaphores
    sem_destroy(&sem_cpu);
    sem_destroy(&sem_io);


    // Print performance metrics
    // ...
 //int total_processes = 0;
 //total_simulation_time = 0;
 //total_cpu_busy_time = 0;
 //total_turnaround_time = 0;
 //total_waiting_time = 0;

// ...

// Calculate metrics
float cpu_utilization = ((float)total_cpu_busy_time / total_simulation_time) * 100;
float throughput = ((float)total_processes / total_simulation_time);
float avg_turnaround_time = ((float)total_turnaround_time / total_processes);
float avg_waiting_time = ((float)total_waiting_time / total_processes);

// Print metrics
printf("CPU Utilization: %.2f%%\n", cpu_utilization);
printf("Throughput: %.2f processes per time unit\n", throughput);
printf("Average Turnaround Time: %.2f time units\n", avg_turnaround_time);
printf("Average Waiting Time in Ready Queue: %.2f time units\n", avg_waiting_time);

for (int i = 0; i < num_processes; i++) 
{
    free(proc_list[i]);
}
free(proc_list);


pthread_mutex_destroy(&mutex);


    return 0;
}


void *file_read(void *arg) {
    char *file_name = (char *)arg;
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror("Error opening file test");
        return NULL;
    }

    int currPID = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "proc", 4) == 0) {
            // Create a new process
            Process *new_process = (Process *)malloc(sizeof(Process));
            total_processes++;
            new_process->PID = ++currPID;
            
            // Extract the process priority, CPU burst times and IO burst times from the line
            char *token = strtok(line, " "); // Skip the "proc" token
            token = strtok(NULL, " "); // Get the process priority
            new_process->PR = atoi(token);

            token = strtok(NULL, " "); // Get the number of CPU bursts
            new_process->numCPUBurst = atoi(token);
            new_process->CPUBurst = (int*) malloc(new_process->numCPUBurst * sizeof(int));

            for (int i = 0; i < new_process->numCPUBurst; i++) {
                token = strtok(NULL, " "); // Get the next CPU burst time
                new_process->CPUBurst[i] = atoi(token);
            }

            token = strtok(NULL, " "); // Get the number of IO bursts
            new_process->numIOBurst = atoi(token);
            new_process->IOBurst = (int*) malloc(new_process->numIOBurst * sizeof(int));

            for (int i = 0; i < new_process->numIOBurst; i++) {
                token = strtok(NULL, " "); // Get the next IO burst time
                new_process->IOBurst[i] = atoi(token);
            }
            new_process->remaining_burst_time = new_process->CPUBurst[new_process->cpuindex];
            new_process->arrival_time = total_simulation_time;

            queue_enqueue(&ready_queue, new_process);
            // Initialize other fields
            new_process->cpuindex = 0;
            new_process->ioindex = 0;
            new_process->prev = NULL;
            new_process->next = NULL;

            sem_post(&sem_cpu);
        } else if (strncmp(line, "sleep", 5) == 0) {
            int sleep_time;
            sscanf(line, "sleep %d", &sleep_time);
            sleep(sleep_time);

        } else if (strncmp(line, "stop", 4) == 0) {
            break;
        }
    }

    fclose(file);
    file_read_done = 1;
    printf("File read");
    return NULL;
}


void *cpu_scheduler(void *arg) {

    char *scheduling_algorithm = (char *) arg;
    pthread_mutex_t *mutex = (pthread_mutex_t *) &cpu_mutex;

    while (1) {
        pthread_mutex_lock(mutex);
        // If Ready_Q is empty and CPU is not busy and IO_Q is empty and IO is not busy and file_read is done, then break!
        if (queue_is_empty(&ready_queue) && !cpu_busy && queue_is_empty(&io_queue) && !io_busy && file_read_done == 1) {
            pthread_mutex_unlock(mutex);
            break;
        }
        pthread_mutex_unlock(mutex);

        int res = sem_wait(&sem_cpu);

        if (res == -1 && errno == ETIMEDOUT) {
            continue;
        }

        pthread_mutex_lock(mutex);
        cpu_busy = 1;
        pthread_mutex_unlock(mutex);

        // Get (remove) the first PCB from Ready_Q based on the scheduling algorithm
        Process *current_pcb = NULL;

        if (strcmp(scheduling_algorithm, "FIFO") == 0) {
            current_pcb = FIFO(&ready_queue);
            queue_remove(&ready_queue, current_pcb);
        } else if (strcmp(scheduling_algorithm, "SJF") == 0) {
            current_pcb = SJF(&ready_queue);
            queue_remove(&ready_queue, current_pcb);
        } else if (strcmp(scheduling_algorithm, "PR") == 0) {
            current_pcb = PR(&ready_queue);
            queue_remove(&ready_queue, current_pcb);
        } else if (strcmp(scheduling_algorithm, "RR") == 0) {
            current_pcb = RR(&ready_queue, QUANTUM);
        } else {
            fprintf(stderr, "Invalid scheduling algorithm: %s\n", scheduling_algorithm);
            return NULL;
        }

        // Remove the selected PCB from Ready_Q
        queue_remove(&ready_queue, current_pcb);
        current_pcb->waiting_time += (total_simulation_time - current_pcb->arrival_time);

        // sleep for PCB->CPUBurst[PCB->cpuindex] (ms) 
        sleep(current_pcb->CPUBurst[current_pcb->cpuindex] / 1000);

        // Update the total_simulation_time and total_cpu_busy_time
        pthread_mutex_lock(mutex);
        total_simulation_time += current_pcb->CPUBurst[current_pcb->cpuindex];
        total_cpu_busy_time += current_pcb->CPUBurst[current_pcb->cpuindex];
        pthread_mutex_unlock(mutex);

        current_pcb->completion_time = total_simulation_time;

        current_pcb->cpuindex++;

        // If this is the last cpu burst
        if (current_pcb->cpuindex >= current_pcb->numCPUBurst) {
            // Terminate this PCB
            terminate_process(current_pcb);
            pthread_mutex_lock(mutex);
            cpu_busy = 0;

            // Update total_turnaround_time and total_waiting_time
            total_turnaround_time += (current_pcb->completion_time - current_pcb->arrival_time);
            total_waiting_time += current_pcb->waiting_time;
            pthread_mutex_unlock(mutex);
        } else {
            // Insert PCB into IO_Q
            queue_enqueue(&io_queue, current_pcb);
            pthread_mutex_lock(mutex);
            cpu_busy = 0;
            pthread_mutex_unlock(mutex);
            sem_post(&sem_io);
        }
    }

   

    // Signal that the CPU scheduler has finished
    cpu_sch_done = 1;

    return NULL;
}

void* io_system(void* arg) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
    pthread_mutex_lock(mutex);
    io_busy = 1;
    pthread_mutex_unlock(mutex);

    while (1) {
        // This is always FIFO
        // If Ready_Q is empty and CPU is not busy and IO_Q is empty and IO is not busy and file_read is done, then break!
        if (queue_is_empty(&ready_queue) && !cpu_busy && queue_is_empty(&io_queue) && !io_busy && file_read_done == 1) {
            break;
        }

        int res = sem_wait(&sem_io);

        if (res == -1 && errno == ETIMEDOUT) {
            continue;
        }

        io_busy = 1;

        // Get (remove) the first PCB from IO_Q
        Process* current_pcb = queue_dequeue(&io_queue);

        // sleep for PCB->IOBurst[PCB->ioindex] (s)
        sleep(current_pcb->IOBurst[current_pcb->ioindex]);

        current_pcb->arrival_time = total_simulation_time;

        current_pcb->ioindex++;

        // Insert PCB into Ready_Q
        queue_enqueue(&ready_queue, current_pcb);

        io_busy = 0;
        sem_post(&sem_cpu);
    }

    // Signal that the I/O system has finished
    io_sys_done = 1;

    return NULL;
}
// Implement scheduling algorithms
Process* FIFO(Queue* queue) {
    // Check if the queue is empty
    if (queue_is_empty(queue)) {
        return NULL;
    }
    
    // The first process in the queue is the one to be scheduled
    Process* next_process = queue->head;

    return next_process;
}


Process* SJF(Queue* queue) {
    if (queue_is_empty(queue)) {
        return NULL;
    }

    Process* shortest_job = queue->head;
    Process* current = queue->head;

    while (current != NULL) {
        if (current->CPUBurst[current->cpuindex] < shortest_job->CPUBurst[shortest_job->cpuindex]) {
            shortest_job = current;
        }
        current = current->next;
    }

    return shortest_job;
}


Process* PR(Queue* queue) {
    if (queue_is_empty(queue)) {
        return NULL;
    }

    Process* highest_priority_process = queue->head;
    Process* current = queue->head;

    while (current != NULL) {
        if (current->PR > highest_priority_process->PR) {
            highest_priority_process = current;
        }
        current = current->next;
    }

    return highest_priority_process;
}


Process* RR(Queue* queue, int quantum) {
    // Check if the queue is empty
    if (queue_is_empty(queue)) {
        return NULL;
    }

    // Get the first process in the queue
    Process* next_process = queue_dequeue(queue);

    // Decrease the remaining burst time of the process by the quantum
    next_process->remaining_burst_time -= quantum;

    // If the burst time of the process is less than or equal to 0, the process is done
    if (next_process->remaining_burst_time <= 0) {
        // The process has completed its current CPU burst
        next_process->cpuindex++;
        next_process->remaining_burst_time = next_process->CPUBurst[next_process->cpuindex];
    } else {
        // The process still has some burst time remaining, so move it to the end of the queue
        queue_enqueue(queue, next_process);
    }

    return next_process;
}


// Check if the queue is empty
bool queue_is_empty(Queue* queue) {
    return queue->head == NULL;
}

// Add a process to the end of the queue
void queue_enqueue(Queue* queue, Process* process) {
    process->next = NULL;

    if (queue_is_empty(queue)) {
        queue->head = process;
        queue->tail = process;
        process->prev = NULL;
    } else {
        process->prev = queue->tail;
        queue->tail->next = process;
        queue->tail = process;
    }
}

// Remove a process from the start of the queue and return it
Process* queue_dequeue(Queue* queue) {
    if (queue_is_empty(queue)) {
        return NULL;
    }

    Process* process = queue->head;
    queue->head = process->next;

    if (queue->head != NULL) {
        queue->head->prev = NULL;
    } else {
        queue->tail = NULL;
    }

    return process;
}

// Remove a specific process from the queue
void queue_remove(Queue* queue, Process* process) {
    if (process->prev != NULL) {
        process->prev->next = process->next;
    } else {
        queue->head = process->next;
    }

    if (process->next != NULL) {
        process->next->prev = process->prev;
    } else {
        queue->tail = process->prev;
    }
}

// Move a specific process to the end of the queue
void queue_move_to_end(Queue* queue, Process* process) {
    queue_remove(queue, process);
    queue_enqueue(queue, process);
}
Process* create_process(int PID, int PR, int numCPUBurst, int numIOBurst, int *CPUBurst, int *IOBurst) {
    Process* new_process = (Process*) malloc(sizeof(Process));

    if (new_process == NULL) {
        fprintf(stderr, "Failed to allocate memory for new process\n");
        return NULL;
    }

    new_process->PID = PID;
    new_process->PR = PR;
    new_process->numCPUBurst = numCPUBurst;
    new_process->numIOBurst = numIOBurst;
    new_process->CPUBurst = CPUBurst;
    new_process->IOBurst = IOBurst;
    new_process->cpuindex = 0;
    new_process->ioindex = 0;
    new_process->prev = NULL;
    new_process->next = NULL;

    // Initialize remaining_burst_time
    new_process->remaining_burst_time = new_process->CPUBurst[new_process->cpuindex];

    return new_process;
}

void terminate_process(Process *process) {
    // Update performance metrics
    total_processes++;
    total_turnaround_time += (process->completion_time - process->arrival_time);
    total_waiting_time += process->waiting_time;

    // Free the memory allocated for the process bursts
    free(process->CPUBurst);
    free(process->IOBurst);

    // Free the memory allocated for the process
    free(process);
}
