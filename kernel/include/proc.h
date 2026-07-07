#ifndef PROC_H
#define PROC_H

#include <types.h>
#include <mm.h>

#define MAX_PROCESSES 1024
#define MAX_THREADS_PER_PROCESS 256

typedef enum {
    PROCESS_UNUSED,
    PROCESS_RUNNING,
    PROCESS_READY,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE
} ProcessState;

typedef enum {
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_BLOCKED,
    THREAD_ZOMBIE
} ThreadState;

typedef struct Thread Thread;

typedef struct {
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, rbp, rsp;
    u64 r8, r9, r10, r11;
    u64 r12, r13, r14, r15;
    u64 rip, rflags;
    u64 cr3;
} ThreadContext;

struct Thread {
    ThreadContext context;
    ThreadState state;
    u32 priority;
    u32 time_slice;
    struct Thread* next;
    struct Thread* prev;
};

typedef struct {
    u32 pid;
    u32 ppid;
    ProcessState state;
    u64* page_table;
    Thread threads[MAX_THREADS_PER_PROCESS];
    u32 thread_count;
    Thread* current_thread;
    char name[256];
    struct Process* next;
    struct Process* prev;
} Process;

typedef struct {
    Process processes[MAX_PROCESSES];
    u32 process_count;
    Process* current_process;
    Thread* ready_queue;
    Thread* running_thread;
} ProcessManager;

void proc_init(void);
Process* create_process(const char* name, u64 entry_point, u64* page_table);
Thread* create_thread(Process* proc, u64 entry_point);
void destroy_process(u32 pid);
void schedule(void);
void yield(void);

#endif