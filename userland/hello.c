// Yeni POSIX Syscall Testi
int _write(int file, char *ptr, int len);
void *_sbrk(int incr);
int _open(const char *name, int flags, int mode);
int _close(int file);
void _exit(int status);

// Basit strlen
int strlen(const char *s) {
    int i = 0;
    while(s[i]) i++;
    return i;
}

void main() {
    char *msg = "Merhaba ScratchOS (POSIX Syscall ile)!\n";
    _write(1, msg, strlen(msg));
    
    char *mem = (char *)_sbrk(4096);
    if ((int)mem != -1) {
        char *msg2 = "sbrk basarili, 4KB alindi!\n";
        _write(1, msg2, strlen(msg2));
    }
    
    int fd = _open("guess", 0, 0);
    if (fd >= 0) {
        char *msg3 = "Dosya basariyla acildi!\n";
        _write(1, msg3, strlen(msg3));
        _close(fd);
    } else {
        char *msg3 = "Dosya acilamadi!\n";
        _write(1, msg3, strlen(msg3));
    }
    
    _exit(0);
}
