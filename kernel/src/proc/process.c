#include <proc.h>
#include <mm.h>
#include <types.h>

static ProcessManager process_manager;

void proc_init(void) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_manager.processes[i].pid = 0;
        process_manager.processes[i].state = PROCESS_UNUSED;
        process_manager.processes[i].thread_count = 0;
    }
    
    process_manager.process_count = 0;
    process_manager.current_process = NULL;
    process_manager.ready_queue = NULL;
    process_manager.running_thread = NULL;
}

Process* create_process(const char* name, u64 entry_point, u64* page_table) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (process_manager.processes[i].state == PROCESS_UNUSED) {
            Process* proc = &process_manager.processes[i];
            
            proc->pid = i + 1;
            proc->ppid = process_manager.current_process ? 
                         process_manager.current_process->pid : 0;
            proc->state = PROCESS_READY;
            proc->page_table = page_table;
            proc->thread_count = 0;
            proc->current_thread = NULL;
            
            for (size_t j = 0; j < 256 && name[j]; j++) {
                proc->name[j] = name[j];
            }
            proc->name[255] = '\0';
            
            process_manager.process_count++;
            
            create_thread(proc, entry_point);
            
            return proc;
        }
    }
    
    return NULL;
}

Thread* create_thread(Process* proc, u64 entry_point) {
    if (!proc || proc->thread_count >= MAX_THREADS_PER_PROCESS) {
        return NULL;
    }
    
    Thread* thread = &proc->threads[proc->thread_count];
    
    thread->context.rip = entry_point;
    thread->context.rsp = (u64)pmm_alloc_page() + PAGE_SIZE;
    thread->context.cr3 = (u64)proc->page_table;
    thread->context.rflags = 0x202;
    
    thread->state = THREAD_READY;
    thread->priority = 1;
    thread->time_slice = 0;
    
    thread->next = process_manager.ready_queue;
    thread->prev = NULL;
    
    if (process_manager.ready_queue) {
        process_manager.ready_queue->prev = thread;
    }
    process_manager.ready_queue = thread;
    
    proc->thread_count++;
    proc->current_thread = thread;
    
    return thread;
}

void destroy_process(u32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (process_manager.processes[i].pid == pid) {
            Process* proc = &process_manager.processes[i];
            
            for (u32 j = 0; j < proc->thread_count; j++) {
                Thread* thread = &proc->threads[j];
                
                if (thread->prev) {
                    thread->prev->next = thread->next;
                } else {
                    process_manager.ready_queue = thread->next;
                }
                
                if (thread->next) {
                    thread->next->prev = thread->prev;
                }
                
                if (thread->state == THREAD_RUNNING) {
                    process_manager.running_thread = NULL;
                }
            }
            
            if (proc->page_table) {
                pmm_free_page((void*)proc->page_table);
            }
            
            proc->state = PROCESS_UNUSED;
            proc->pid = 0;
            proc->thread_count = 0;
            
            process_manager.process_count--;
            
            break;
        }
    }
}

void schedule(void) {
    if (!process_manager.ready_queue) {
        return;
    }
    
    Thread* next_thread = process_manager.ready_queue;
    
    if (next_thread) {
        process_manager.ready_queue = next_thread->next;
        
        if (process_manager.ready_queue) {
            process_manager.ready_queue->prev = NULL;
        }
        
        next_thread->next = NULL;
        next_thread->prev = NULL;
        next_thread->state = THREAD_RUNNING;
        
        Thread* old_thread = process_manager.running_thread;
        
        if (old_thread && old_thread != next_thread) {
            old_thread->state = THREAD_READY;
            old_thread->next = process_manager.ready_queue;
            
            if (process_manager.ready_queue) {
                process_manager.ready_queue->prev = old_thread;
            }
            process_manager.ready_queue = old_thread;
        }
        
        process_manager.running_thread = next_thread;
        
        if (next_thread->context.cr3 != ((Thread*)0)->context.cr3) {
            asm volatile("mov %0, %%cr3" :: "r"(next_thread->context.cr3));
        }
    }
}

void yield(void) {
    schedule();
}