#include "task.h"
#include "kheap.h"
#include "paging.h"
#include "tss.h"
#include "spinlock.h"

static task_t* current_task = 0;
static task_t* task_list = 0;
static int next_pid = 1;
extern unsigned int stack_top;

// Mevcut görevi getir
task_t* get_current_task() {
    return current_task;
}

// Çoklu görev sistemini başlat
void init_multitasking() {
    // 1. Ana (Kernel) görevi oluştur
    current_task = (task_t*)kmalloc(sizeof(task_t));
    char* k_name = "kernel";
    int i = 0;
    while(k_name[i]) {
        current_task->name[i] = k_name[i];
        i++;
    }
    current_task->name[i] = '\0';

    current_task->state = TASK_RUNNING;
    current_task->id = next_pid++;


    current_task->next = 0;
    
    task_list = current_task;
    current_task->kstack_top = (unsigned int)&stack_top; // Boot stack'i kullan
    
    put_str("[OK] Multitasking sistemi baslatildi (Kernel Task PID 1).\n");
}

// Yeni bir görev oluştur
task_t* create_task(char* name, void (*entry_point)(), int is_user) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    new_task->id = next_pid++;
    
    int i = 0;
    while(name[i] && i < 31) {
        new_task->name[i] = name[i];
        i++;
    }
    new_task->name[i] = '\0';
    
    // Görev için yığın (stack) ayır (4KB)
    unsigned int stack_size = 4096;
    unsigned char* stack = (unsigned char*)kmalloc(stack_size);
    unsigned int* esp = (unsigned int*)(stack + stack_size);
    
    // Sahte bir interrupt stack'i hazırla
    // İşlemci IRET ile döndüğünde bu değerleri kullanacak
    if (is_user) {
        *(--esp) = 0x23;                         // SS (User Data)
        *(--esp) = (unsigned int)(stack + stack_size); // ESP
    }

    *(--esp) = 0x202;         // EFLAGS (Interrupts enabled)
    *(--esp) = is_user ? 0x1B : 0x08;      // CS (User/Kernel Code Selector)
    *(--esp) = (unsigned int)entry_point; // EIP (Giriş noktası)
    
    // Sahte hata kodu ve interrupt numarası (irq_common_stub beklediği için)
    *(--esp) = 0;             // Error code
    *(--esp) = 0;             // Int no
    
    // pusha kayıtçıları
    *(--esp) = 0;             // EAX
    *(--esp) = 0;             // ECX
    *(--esp) = 0;             // EDX
    *(--esp) = 0;             // EBX
    *(--esp) = 0;             // ESP (Gerekli değil ama pusha basıyor)
    *(--esp) = 0;             // EBP
    *(--esp) = 0;             // ESI
    *(--esp) = 0;             // EDI
    
    // Data segment (ds)
    *(--esp) = is_user ? 0x23 : 0x10;
    
    new_task->esp = (unsigned int)esp;
    
    // Her görev için bir çekirdek yığını ayır (Ring 3 -> Ring 0 geçişi için)
    if (is_user) {
        new_task->kstack_top = (unsigned int)kmalloc(4096) + 4096;
    } else {
        new_task->kstack_top = (unsigned int)(stack + stack_size); // Kernel task kendi stack'ini kullanır
    }

    new_task->state = TASK_READY;
    new_task->next = 0;
    
    // LibC Altyapısı İlk Değerleri
    new_task->heap_start = 0x10000000; // 256MB seviyesinden heap başlar
    new_task->heap_end = 0x10000000;
    for (int k = 0; k < 16; k++) {
        new_task->fd_table[k] = 0;
        new_task->fd_offset[k] = 0;
    }
    
    // Görevi listeye ekle (Kritik Bölge)
    unsigned int flags = irq_save();
    task_t* tmp = task_list;
    while(tmp->next) tmp = tmp->next;
    tmp->next = new_task;
    irq_restore(flags);
    
    return new_task;
}

// Basit Round Robin Zamanlayıcı
unsigned int schedule_internal(unsigned int current_esp) {
    if (!current_task) return current_esp;
    
    // Mevcut görevin ESP'sini kaydet
    current_task->esp = current_esp;
    
    // Bir sonraki hazır görevi bul (DEAD olanları atla)
    task_t* next = current_task->next;
    while (1) {
        if (!next) next = task_list; // Başa dön
        if (next->state != TASK_DEAD) break;
        next = next->next;
    }
    
    current_task = next;
    current_task->state = TASK_RUNNING;
    
    // TSS'yi güncelle (Ring 3'ten dönerken bu stack kullanılacak)
    set_kernel_stack(current_task->kstack_top);
    
    return current_task->esp;
}

extern int shell_active;
extern void print_prompt();

void schedule() {
    asm volatile("sti; int $0x20");
}

void task_exit() {
    unsigned int flags = irq_save();
    (void)flags;
    task_t* current = get_current_task();
    if (current && current->id != 1) { // Kernel task (PID 1) kapatılamaz
        current->state = TASK_DEAD;
        shell_active = 1;
        print_prompt();
    }
    // Syscall üzerinden gelindiği için EFLAGS IF=0 olabilir. 
    // Scheduler'ın çalışabilmesi için kesmeleri (interrupts) açmalıyız!
    asm volatile("sti");
    while(1) asm volatile("hlt");
}
