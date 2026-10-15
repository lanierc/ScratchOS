#include <stdint.h>
#include "socket.h"

int strlen(const char *s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

uint32_t inet_addr(const char *cp) {
    uint32_t res = 0;
    int parts[4] = {0,0,0,0};
    int part = 0;
    while (*cp) {
        if (*cp == '.') {
            part++;
            if (part > 3) return 0;
        } else if (*cp >= '0' && *cp <= '9') {
            parts[part] = parts[part] * 10 + (*cp - '0');
        }
        cp++;
    }
    res = (parts[3] << 24) | (parts[2] << 16) | (parts[1] << 8) | parts[0];
    return res;
}

// Basit kprintf benzeri (syscalls.c'den gelen write'i kullanir)
void print(const char* s) {
    write(1, s, strlen(s));
}

void print_int(int n) {
    char buf[12];
    int i = 0;
    if (n == 0) { buf[i++] = '0'; }
    else {
        while (n > 0) {
            buf[i++] = (n % 10) + '0';
            n /= 10;
        }
    }
    buf[i] = '\0';
    // Stringi ters cevir
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = t;
    }
    print(buf);
}

int main() {
    print("TCP Test Uygulamasi Baslatiliyor...\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        print("Hata: Soket olusturulamadi!\n");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("10.0.2.2"); // QEMU Gateway / Host

    print("Baglaniliyor: 10.0.2.2:8888...\n");
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        print("Hata: Baglanti kurulamadi!\n");
        return 1;
    }

    print("Baglanti Basarili! Mesaj gonderiliyor...\n");
    char *msg = "Merhaba Host! Ben ScratchOS TCP istemcisi.";
    send(sock, msg, 42, 0);

    print("Cevap bekleniyor...\n");
    char buf[128];
    int len = recv(sock, buf, 127, 0);
    if (len > 0) {
        buf[len] = '\0';
        print("Gelen Cevap: ");
        print(buf);
        print("\n");
    }

    print("Test tamamlandi. Soket kapatiliyor (Teorik).\n");
    // close(sock); // Henuz implemente edilmediyse bile VFS halleder

    return 0;
}
