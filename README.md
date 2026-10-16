# Scratch İşletim Sistemi

ScratchOS, x86 mimarisi üzerinde çalışan, eğitim ve araştırma odaklı geliştirilmiş monolitik bir işletim sistemi çekirdeğidir. Projenin temel amacı, donanım seviyesinden kullanıcı uygulamalarına kadar modern bir işletim sisteminin tüm bileşenlerini sıfırdan inşa ederek düşük seviyeli sistem mimarilerini analiz etmektir.

## Temel Mimari Bileşenler

### 1. Çekirdek ve Önyükleme Süreci
- **Önyükleyici (Bootloader):** Multiboot 1 standardı ile uyumlu, NASM ile yazılmış düşük seviyeli başlatma kodu.
- **Kesme Yönetimi:** GDT (Global Descriptor Table) ve IDT (Interrupt Descriptor Table) yapılandırması. Donanım ve yazılım kesmeleri için özelleştirilmiş ISR (Interrupt Service Routines) ve IRQ (Interrupt Requests) işleyicileri.
- **Zamanlayıcı:** PIT (Programmable Interval Timer) üzerinden 100Hz frekansında çalışan sistem saati ve görev zamanlayıcı.

### 2. Bellek Yönetimi (Memory Management)
- **Fiziksel Bellek (PMM):** Bitmap tabanlı, 4KB sayfa boyutunda çalışan fiziksel çerçeve (frame) yöneticisi.
- **Sanal Bellek (Paging):** Sayfa dizinleri ve tabloları üzerinden bellek korumalı (Memory Protection) adresleme. Çekirdeğin yüksek adreslere (Higher Half Kernel - 0xC0000000) taşınması.
- **Dinamik Tahsisat:** Çekirdek içinde `kmalloc` ve `kfree` işlevlerini sağlayan heap yöneticisi.

### 3. Sanal Dosya Sistemi (VFS) ve Depolama
- **VFS Soyutlama Katmanı:** Dosya sistemi bağımsız bir arayüz üzerinden mount noktaları, dosya okuma/yazma ve dizin listeleme desteği.
- **PAFS (ScratchOS Advanced File System):** İmaj tabanlı, basit ve performans odaklı yerel dosya sistemi.
- **Ext2 Sürücüsü:** Linux standartlarında Ext2 (Revision 1+) dosya sistemleri için tam okuma ve sembolik bağ (Symlink) çözünürleme desteği.
- **Depolama Sürücüleri:** ATA IDE kontrolcüsü üzerinden Master/Slave disk yönetimi ve PCI veri yolu üzerinden aygıt keşfi.

### 4. Ağ Protokol Yığını (Network Stack)
- **Donanım Katmanı:** RTL8139 ve Intel E1000 ağ kartları için PCI sürücüleri.
- **Alt Seviye Protokoller:** Ethernet II çerçeveleme, ARP (Adres Çözümleme Protokolü) ve IPv4 desteği.
- **İletişim Katmanı:** ICMP (Ping), UDP ve tam durumlu (stateful) TCP protokolü.
- **Soket API:** Standart BSD Soket arayüzü ile uyumlu `socket`, `bind`, `connect`, `send` ve `recv` çağrıları.

### 5. Süreç Yönetimi ve Kullanıcı Modu
- **Çoklu Görev (Multitasking):** Round-robin algoritması kullanan, context switching yeteneğine sahip görev yöneticisi.
- **Kullanıcı Modu (Ring 3):** TSS (Task State Segment) yapılandırması ile çekirdekten izole edilmiş kullanıcı alanı.
- **ELF Yükleyici:** Standart ELF32 dosyalarını diskten yükleyerek kullanıcı modunda çalıştırma yeteneği.

## Geliştirme ve Kullanım

### Proje Dizini Yapısı
- `boot/`: İşlemciyi başlatan ve çekirdeği yükleyen düşük seviyeli kodlar.
- `kernel/`: Çekirdek bileşenleri, bellek yönetimi ve protokol yığınları.
- `include/`: Tüm çekirdek ve sürücü başlık dosyaları.
- `drivers/`: Donanım sürücüleri (Grafik, Ağ, Depolama, Giriş Birimleri).
- `userland/`: C ile yazılmış ve ELF formatında derlenen kullanıcı uygulamaları.
- `scripts/`: Otomatik derleme ve disk hazırlama araçları.

### Derleme Adımları
İşletim sistemini derlemek için i686-elf-gcc veya x86-32 destekli standart bir GCC derleyicisi gereklidir.

```bash
# Tüm çekirdek ve sürücüleri derler
make all

# Kullanıcı modu uygulamalarını derler
cd userland && bash compile_user.sh

# Sistemi QEMU emülatörü üzerinde başlatır
make run
```

## Yol Haritası (Roadmap)
- [x] Temel Donanım Sürücüleri (Klavye, Fare, Disk)
- [x] Sanal Dosya Sistemi ve Ext2 Desteği
- [x] Tam Kapsamlı Ağ Yığını ve Soket API
- [x] ELF Uygulama Yükleyici
- [ ] Gelişmiş GUI ve Pencere Yöneticisi
- [ ] HTTP ve DNS Protokolleri
- [ ] USB Kontrolcü Desteği (xHCI/EHCI)

## Geliştirici Bilgileri
**Muhammed Yasir PEKER**
*Low Level Programmer*

Bu proje, tek kişilik bir ekip tarafından düzenli olmayan bir şekilde geliştirilmektedir. Tüm hakları saklıdır.
