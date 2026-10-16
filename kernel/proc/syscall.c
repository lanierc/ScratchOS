#include "syscall.h"
#include "timer.h"
#include "vfs.h"
#include "task.h"
#include "paging.h"
#include "pmm.h"
#include "socket.h"

static void* syscall_table[20];

extern unsigned char keyboard_get();
extern void task_exit();

void sys_getchar(struct registers *regs) {
    regs->eax = keyboard_get();
}

void sys_exit(struct registers *regs) {
    task_exit();
}

void sys_open(struct registers *regs) {
    char *path = (char *)regs->ebx;
    int flags = regs->ecx;
    
    vfs_node_t *node = vfs_get_node_by_path(vfs_root, path);
    if (!node) {
        if (flags & 1) { // 1 = O_CREAT
            extern int lafs_write(const char *name, const char *data, int len);
            lafs_write(path, "", 0);
            node = vfs_get_node_by_path(vfs_root, path);
            if (!node) {
                regs->eax = -1;
                return;
            }
        } else {
            regs->eax = -1;
            return;
        }
    }
    task_t *current = get_current_task();
    for (int i = 0; i < 16; i++) {
        if (current->fd_table[i] == 0) {
            current->fd_table[i] = node;
            current->fd_offset[i] = 0;
            regs->eax = i;
            return;
        }
    }
    vfs_close(node);
    regs->eax = -1;
}

void sys_read(struct registers *regs) {
    int fd = regs->ebx;
    char *buf = (char *)regs->ecx;
    int size = regs->edx;
    
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || current->fd_table[fd] == 0) {
        regs->eax = -1;
        return;
    }
    int read_bytes = vfs_read(current->fd_table[fd], current->fd_offset[fd], size, (unsigned char*)buf);
    if (read_bytes > 0 && current->fd_table[fd]->flags != VFS_SOCKET) {
        current->fd_offset[fd] += read_bytes;
    }
    regs->eax = read_bytes;
}

void sys_write_fd(struct registers *regs) {
    int fd = regs->ebx;
    char *buf = (char *)regs->ecx;
    int size = regs->edx;
    
    if (fd == 1 || fd == 2) {
        for (int i = 0; i < size; i++) put_char(buf[i]);
        regs->eax = size;
        return;
    }
    
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || current->fd_table[fd] == 0) {
        regs->eax = -1;
        return;
    }
    int written = vfs_write(current->fd_table[fd], current->fd_offset[fd], size, (unsigned char*)buf);
    if (written > 0 && current->fd_table[fd]->flags != VFS_SOCKET) {
        current->fd_offset[fd] += written;
    }
    regs->eax = written;
}

void sys_close(struct registers *regs) {
    int fd = regs->ebx;
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || current->fd_table[fd] == 0) {
        regs->eax = -1;
        return;
    }
    vfs_close(current->fd_table[fd]);
    current->fd_table[fd] = 0;
    current->fd_offset[fd] = 0;
    regs->eax = 0;
}

void sys_sbrk(struct registers *regs) {
    int increment = regs->ebx;
    task_t *current = get_current_task();
    
    unsigned int old_end = current->heap_end;
    if (increment == 0) {
        regs->eax = old_end;
        return;
    }
    
    unsigned int new_end = old_end + increment;
    
    unsigned int old_page = (old_end + 4095) / 4096;
    unsigned int new_page = (new_end + 4095) / 4096;
    
    for (unsigned int i = old_page; i < new_page; i++) {
        unsigned int frame = alloc_frame();
        paging_map_memory(frame, i * 4096, 4096);
    }
    
    current->heap_end = new_end;
    regs->eax = old_end;
}

extern void clear_scr();
void sys_clear(struct registers *regs) {
    clear_scr();
}

void sys_uptime(struct registers *regs) {
    regs->eax = timer_get_ticks();
}

void sys_write(struct registers *regs) {
    char* str = (char*)regs->ebx;
    put_str(str);
}

void sys_socket(struct registers *regs) {
    int domain = regs->ebx;
    int type = regs->ecx;
    int protocol = regs->edx;
    
    vfs_node_t *node = socket_create(domain, type, protocol);
    if (!node) {
        regs->eax = -1;
        return;
    }
    
    task_t *current = get_current_task();
    for (int i = 0; i < 16; i++) {
        if (current->fd_table[i] == 0) {
            current->fd_table[i] = node;
            regs->eax = i;
            return;
        }
    }
    regs->eax = -1;
}

void sys_bind(struct registers *regs) {
    int fd = regs->ebx;
    struct sockaddr *addr = (struct sockaddr *)regs->ecx;
    int addrlen = regs->edx;
    
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || !current->fd_table[fd] || current->fd_table[fd]->flags != VFS_SOCKET) {
        regs->eax = -1;
        return;
    }
    regs->eax = socket_bind(current->fd_table[fd], addr, addrlen);
}

void sys_sendto(struct registers *regs) {
    int fd = regs->ebx;
    void *buf = (void *)regs->ecx;
    int len = regs->edx;
    struct sockaddr *dest = (struct sockaddr *)regs->esi;
    int dest_len = regs->edi;
    
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || !current->fd_table[fd] || current->fd_table[fd]->flags != VFS_SOCKET) {
        regs->eax = -1;
        return;
    }
    regs->eax = socket_sendto(current->fd_table[fd], buf, len, 0, dest, dest_len);
}

void sys_recvfrom(struct registers *regs) {
    int fd = regs->ebx;
    void *buf = (void *)regs->ecx;
    int len = regs->edx;
    struct sockaddr *src = (struct sockaddr *)regs->esi;
    uint32_t *src_len = (uint32_t *)regs->edi;
    
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || !current->fd_table[fd]) {
        regs->eax = -1;
        return;
    }
    regs->eax = socket_recvfrom(current->fd_table[fd], buf, len, 0, src, src_len);
}

void sys_connect(struct registers *regs) {
    int fd = regs->ebx;
    struct sockaddr *addr = (struct sockaddr *)regs->ecx;
    int addrlen = regs->edx;
    
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || !current->fd_table[fd] || current->fd_table[fd]->flags != VFS_SOCKET) {
        regs->eax = -1;
        return;
    }
    regs->eax = socket_connect(current->fd_table[fd], addr, addrlen);
}

void sys_listen(struct registers *regs) {
    int fd = regs->ebx;
    int backlog = regs->ecx;
    
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || !current->fd_table[fd] || current->fd_table[fd]->flags != VFS_SOCKET) {
        regs->eax = -1;
        return;
    }
    regs->eax = socket_listen(current->fd_table[fd], backlog);
}

void sys_accept(struct registers *regs) {
    int fd = regs->ebx;
    struct sockaddr *addr = (struct sockaddr *)regs->ecx;
    uint32_t *addrlen = (uint32_t *)regs->edx;
    
    task_t *current = get_current_task();
    if (fd < 0 || fd >= 16 || !current->fd_table[fd] || current->fd_table[fd]->flags != VFS_SOCKET) {
        regs->eax = -1;
        return;
    }
    
    vfs_node_t *new_node = socket_accept(current->fd_table[fd], addr, addrlen);
    if (!new_node) {
        regs->eax = -1;
        return;
    }
    
    for (int i = 0; i < 16; i++) {
        if (current->fd_table[i] == 0) {
            current->fd_table[i] = new_node;
            regs->eax = i;
            return;
        }
    }
    regs->eax = -1;
}

void syscall_handler(struct registers *regs) {
    if (regs->eax >= 20) return;
    void *handler = syscall_table[regs->eax];
    if (!handler) return;
    void (*handler_func)(struct registers *) = handler;
    handler_func(regs);
}

void init_syscalls() {
    syscall_table[SYS_UPTIME] = sys_uptime;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_EXIT] = sys_exit;
    syscall_table[SYS_GETCHAR] = sys_getchar;
    syscall_table[SYS_OPEN] = sys_open;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_WRITE_FD] = sys_write_fd;
    syscall_table[SYS_CLOSE] = sys_close;
    syscall_table[SYS_SBRK] = sys_sbrk;
    syscall_table[SYS_CLEAR] = sys_clear;
    syscall_table[SYS_SOCKET] = sys_socket;
    syscall_table[SYS_BIND] = sys_bind;
    syscall_table[SYS_SENDTO] = sys_sendto;
    syscall_table[SYS_RECVFROM] = sys_recvfrom;
    syscall_table[SYS_CONNECT] = sys_connect;
    syscall_table[SYS_LISTEN] = sys_listen;
    syscall_table[SYS_ACCEPT] = sys_accept;
    
    put_str("[OK] Sistem cagrilari (Syscalls) baslatildi.\n");
}
