# FerkanOS (ScratchOS)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Architecture: x86-32](https://img.shields.io/badge/Architecture-x86--32-blue.svg)](#temel-mimari-özellikleri)
[![Kernel: Monolithic](https://img.shields.io/badge/Kernel-Monolithic-red.svg)](#temel-mimari-özellikleri)
[![Build: Make](https://img.shields.io/badge/Build-GNU%20Make-brightgreen.svg)](#derleme-ve-kurulum)

**FerkanOS**, x86 (IA-32) mimarisi üzerinde çalışan; donanım seviyesinden kullanıcı modu (Ring 3) uygulamalarına kadar tüm alt bileşenleri sıfırdan geliştirilmiş **monolitik (monolithic) bir işletim sistemi çekirdeğidir**.

Proje; önyükleme mekanizmalarından sanal bellek yönetimine, aygıt sürücülerinden preemptive multitasking zamanlayıcıya, VFS dosya sistemlerinden tam yığın TCP/IP ağ protokolüne ve grafik arayüze (GUI) kadar modern işletim sistemi tasarım prensiplerini pratik ve optimize edilmiş bir mimariyle sunar.

---

## İçindekiler
- [Temel Mimari Özellikleri](#temel-mimari-özellikleri)
- [Çekirdek Alt Sistemleri](#çekirdek-alt-sistemleri)
  - [1. Önyükleme ve Düşük Seviye Çekirdek (Core)](#1-önyükleme-ve-düşük-seviye-çekirdek-core)
  - [2. Bellek Yönetimi (MM)](#2-bellek-yönetimi-mm)
  - [3. Süreç ve Görev Yönetimi (Proc & Multitasking)](#3-süreç-ve-görev-yönetimi-proc--multitasking)
  - [4. Sanal Dosya Sistemi ve Depolama (VFS & FS)](#4-sanal-dosya-sistemi-ve-depolama-vfs--fs)
  - [5. Ağ Protokol Yığını (Networking Stack)](#5-ağ-protokol-yığını-networking-stack)
  - [6. Donanım ve Aygıt Sürücüleri (Drivers)](#6-donanım-ve-aygıt-sürücüleri-drivers)
  - [7. Kullanıcı Arayüzü ve Kabuk (lash & GUI)](#7-kullanıcı-arayüzü-ve-kabuk-lash--gui)
- [Kullanıcı Alanı (Userland) ve ELF Desteği](#kullanıcı-alanı-userland-ve-elf-desteği)
- [Sistem Gereksinimleri](#sistem-gereksinimleri)
- [Derleme ve Çalıştırma](#derleme-ve-çalıştırma)
  - [Sistemi QEMU ile Çalıştırma](#sistemi-qemu-ile-çalıştırma)
  - [Önyüklenebilir Canlı ISO Oluşturma](#önyüklenebilir-canlı-iso-oluşturma)
- [Kabuk (lash) Komut Referansı](#kabuk-lash-komut-referansı)
- [Proje Dizin Yapısı](#proje-dizin-yapısı)
- [Yol Haritası (Roadmap)](#yol-haritası-roadmap)
- [Lisans ve Yazar](#lisans-ve-yazar)

---

## Temel Mimari Özellikleri

```
+--------------------------------------------------------------------------+
|                   KULLANICI MODU (Ring 3 / Userland)                    |
|      hello.elf | guess.elf | vi.elf | tcp_test.elf | udp_test.elf        |
+--------------------------------------------------------------------------+
                                     |
                       [ Sistem Çağrıları - INT 0x80 ]
                                     |
+------------------------------------v-------------------------------------+
|                  ÇEKİRDEK MODU (Ring 0 / Monolithic Kernel)              |
|                                                                          |
|  [ Süreç & Çoklu Görev ]   Round-Robin Scheduler, Context Switching, TSS |
|  [ Bellek Yönetimi ]       PMM (Bitmap), Paging (Higher-Half), KHeap     |
|  [ Sanal Dosya Sistemi ]   VFS Soyutlama Katmanı, LAFS, Ext2 (/mnt)      |
|  [ Ağ Protokol Yığını ]    Ethernet II, ARP, IPv4, ICMP, UDP, TCP, BSD   |
|  [ Grafik & Kabuk ]        VBE Framebuffer Masaüstü, lash CLI            |
|  [ Aygıt Sürücüleri ]      ATA/IDE, PCI, RTL8139, Intel E1000, AC97,     |
|                            PIT Timer, PS/2 Klavye & Fare                 |
+--------------------------------------------------------------------------+
                                     |
+------------------------------------v-------------------------------------+
|                          DONANIM (x86 Bare-Metal)                        |
+--------------------------------------------------------------------------+
```

* **Çekirdek Tipi:** Katı Monolitik (Monolithic Kernel) — Sürücüler, bellek yöneticileri, dosya sistemleri ve ağ protokolleri tek adres alanında (Ring 0) çalışır.
* **Mimari Mod:** 32-bit x86 Korumalı Mod (Protected Mode), Higher-Half Kernel (`0xC0000000` / 3GB sanal bellek ofseti).
* **Önyükleme Standardı:** Multiboot Standardı (v1 uyumlu - GRUB ve QEMU doğrudan boot desteği).
* **Yalıtım:** Ring 0 (Kernel) ve Ring 3 (User) ayrıcalık ayrımı; TSS (Task State Segment) üzerinden güvenli yığıt değişimi.

---

## Çekirdek Alt Sistemleri

### 1. Önyükleme ve Düşük Seviye Çekirdek (Core)
* **Higher-Half Başlatma (`boot/boot.s`):** Sayfalama tablolarını kurarak çekirdek kodunu fiziksel bellekten üst sanal adres alanına (`0xC0000000`) haritalandırır.
* **GDT (Global Descriptor Table):** Çekirdek kod/veri, kullanıcı modu kod/veri ve TSS segmentlerini içeren eksiksiz segmentasyon modeli.
* **IDT & Kesmeler:** Donanım (IRQ 0-15) ve yazılım kesmeleri için ISR yapılandırması; 8259 PIC çipinin modern standartlara uygun olarak yeniden haritalanması (IRQ 0-7 -> INT 32-39, IRQ 8-15 -> INT 40-47).
* **Seri Port Konsolu (`serial.c`):** Başlatma anından itibaren COM1 portu (`0x3F8`) üzerinden düşük seviyeli debug günlükleme.
* **Panic Yönetimi (`panic.c`):** Çözülemeyen donanım ve yazılım hatalarında register dökümü ve sistem kilitleme.

### 2. Bellek Yönetimi (MM)
* **Fiziksel Bellek Yöneticisi (PMM - `pmm.c`):** Multiboot bellek haritasını okuyarak kullanılabilir RAM'i 4 KB boyutunda çerçevelere (frames) ayırır; tahsisat ve serbest bırakma işlemlerini verimli bir bitmap veri yapısı ile yönetir.
* **Sanal Bellek ve Sayfalama (Paging - `paging.c`):** İki seviyeli (Page Directory ve Page Table) sayfa eşleme; çekirdek alanının izole edilmesi ve kullanıcı süreçlerine özel adres alanlarının oluşturulması.
* **Dinamik Çekirdek Belleği (Kernel Heap - `kheap.c`):** Çekirdek içinde dinamik bellek tahsisi sağlayan `kmalloc`, `kzalloc` ve `kfree` işlevleri.

### 3. Süreç ve Görev Yönetimi (Proc & Multitasking)
* **Preemptive Çoklu Görev (Multitasking - `task.c`):** PIT zamanlayıcı kesmesine bağlı çalışan, bağlam değiştirme (context switching) yeteneğine sahip Round-Robin görev zamanlayıcısı.
* **Kullanıcı Modu (Ring 3) İzolasyonu:** TSS (Task State Segment) donanım desteği ile kullanıcı süreçlerinin çekirdek bellek alanını bozmasını önler.
* **Sistem Çağrıları (Syscalls - `syscall.c`):** `int 0x80` yazılım kesmesiyle tetiklenen, POSIX benzeri çekirdek arayüzü (`read`, `write`, `open`, `close`, `exit`, `getchar`, `socket`, `connect`, `send`, `recv` vb.).
* **ELF Yükleyici (`elf.c`):** Standart 32-bit ELF (Executable and Linkable Format) ikili dosyalarını ayrıştırarak belleğe yükleme ve Ring 3'te bağımsız görev olarak başlatma.

### 4. Sanal Dosya Sistemi ve Depolama (VFS & FS)
* **VFS Soyutlama Katmanı (`vfs.c`):** Dosya sistemi bağımsız arayüz (`vfs_node_t`); `read`, `write`, `open`, `close`, `readdir`, `finddir` ve `mount` işlemleri.
* **LAFS (Lanier's Advanced File System - `lafs.c`):** Yerel, optimize edilmiş imaj tabanlı dosya sistemi. Kök (`/`) dosya sistemi olarak bağlanır; dosya oluşturma, silme, okuma, yazma ve dizin hiyerarşisi sunar.
* **Ext2 Sürücüsü (`ext2.c`):** Linux standartlarında Ext2 (Revision 1+) dosya sistemleri için okuma, dizin blokları tarama ve sembolik bağ (Symlink) çözünürleme desteği. İkincil diskleri otomatik olarak `/mnt` yoluna bağlar.
* **ATA IDE Sürücüsü (`ata.c`):** Primary/Secondary kontrolcüler üzerinde Master/Slave sabit diskler için 28-bit LBA PIO modunda sektör okuma/yazma.

### 5. Ağ Protokol Yığını (Networking Stack)
Çekirdek içerisine entegre edilmiş, dış kütüphanelere bağımlı olmayan tam durumlu (stateful) TCP/IP yığını:
* **Donanım Sürücüleri:** RTL8139 ve Intel E1000 Gigabit ağ kartları için PCI tabanlı paket alma/iletme sürücüleri.
* **Bağlantı & Ağ Katmanı:** Ethernet II çerçeveleme, ARP (Adres Çözümleme ve Dinamik Önbellek), IPv4 yönlendirme ve paketleme.
* **İletim Katmanı:** 
  - **ICMP:** Ağ erişilebilirlik ve Echo Request/Reply (Ping) desteği.
  - **UDP:** Durumsuz paket iletimi, port dinleme ve callback mekanizması.
  - **TCP:** Bağlantı açma (3-Way Handshake: SYN, SYN-ACK, ACK), veri segmentasyonu, sıra/onay numarası (SEQ/ACK) takibi, pencere yönetimi ve bağlantı kapatma (FIN/ACK).
* **BSD Soket API (`socket.c`):** Standart `socket()`, `bind()`, `connect()`, `send()`, `recv()` ve `close()` çağrıları.

### 6. Donanım ve Aygıt Sürücüleri (Drivers)
* **PCI Veriyolu (`pci.c`):** Sistemdeki tüm PCI aygıtlarını tarama, Vendor/Device ID tanıma ve BAR (Base Address Register) adreslerini yapılandırma.
* **Giriş Birimleri:** 
  - **Klavye (`keyboard.c`):** PS/2 Scancode Set 1 çözümleyici, Türkçe Q düzen desteği, Shift/CapsLock/Ctrl tuş takibi, dairesel tampon bellek (circular buffer).
  - **Fare (`mouse.c`):** PS/2 veri paketi çözümleme, X/Y delta takibi ve buton durumları.
* **Grafik (VBE - `vbe.c`):** VESA BIOS Extensions (VBE) üzerinden 32bpp Linear Framebuffer doğrudan piksel erişimi, arka plan degrade çizimi ve geometrik ilkel çizimler.
* **Font Motoru (`font.c`):** Bitmap yazı tipi işleyici, metin karakterlerini grafik ekran tamponuna çizme.
* **Ses (AC97 - `ac97.c`):** PCI AC97 ses denetleyicisi başlatma ve Bus Master DMA üzerinden 16-bit PCM ses sinyali çalma (örneğin `beep` testi).
* **Zamanlayıcı (PIT - `timer.c`):** Intel 8254 Programmable Interval Timer çipini 100 Hz (her 10 ms bir kesme) çalışma frekansında sürme; sistem çalışma süresi (uptime) ve zaman aşımı hesaplama.

### 7. Kullanıcı Arayüzü ve Kabuk (lash & GUI)
* **lash (Lanier's Shell - `shell.c`):**
  - Dosya ve dizin yönetimi komutları (`ls`, `cd`, `mkdir`, `touch`, `cat`, `rm`, `rmdir`).
  - Komut geçmişi (Yukarı/Aşağı ok tuşlarıyla önceki 10 komuta erişim).
  - Sistem analiz araçları (`mem`, `pci`, `uptime`, `ticks`).
  - ELF program yürütücü (`exec`).
* **Grafik Arayüz (GUI - `gui.c`):**
  - Masaüstü görünümü ve dinamik fare imleci renderı.
  - Grafik moduna geçiş (`vinit`) ve metin konsoluna geri dönüş (`tui` / `exit_gui`).

---

## Kullanıcı Alanı (Userland) ve ELF Desteği

FerkanOS, bağımsız olarak derlenen C programlarını kullanıcı modunda (Ring 3) çalıştırabilir. `userland/` dizini altında kendi bağımsız `syscalls.c` kütüphanesini ve `user.ld` linker betiğini barındırır.

Örnek Uygulamalar:
* **`hello.elf`:** Standart konsol çıkışı testi.
* **`guess.elf`:** `getchar` ve `write` sistem çağrılarıyla çalışan interaktif sayı tahmin oyunu.
* **`vi.elf`:** Dosya sistemi üzerinde okuma/yazma yapabilen minimalist metin editörü.
* **`udp_test.elf`:** Soket API üzerinden UDP paket alışverişi testi.
* **`tcp_test.elf` & `multi_tcp_test.elf`:** Uzak sunucularla veya yerel loopback ile TCP el sıkışması ve veri transferi yapan ağ test istemcileri.

---

## Sistem Gereksinimleri

İşletim sistemini derlemek ve sanal makinede test etmek için Linux ortamında (Ubuntu/Debian tabanlı) aşağıdaki paketler gereklidir:

```bash
sudo apt update
sudo apt install -y build-essential gcc gcc-multilib nasm qemu-system-x86 python3 xorriso grub-common grub-pc-bin
```

---

## Derleme ve Çalıştırma

### Sistemi QEMU ile Çalıştırma

Tüm çekirdeği, sanal disk imajını ve kullanıcı alanı ELF uygulamalarını tek bir komutla derleyip QEMU üzerinde başlatmak için:

```bash
# 1. Çekirdeği, disk imajını ve userland programlarını derleyip başlatır
make run
```

Eğer parçalı derleme yapmak isterseniz:

```bash
# Sadece çekirdek ve bileşenleri derler
make all

# Sadece kullanıcı modu uygulamalarını derleyip disk imajına yazar
cd userland && bash compile_user.sh && cd ..

# Sistemi temizler
make clean
```

> **QEMU Başlatma Parametreleri:**
> `make run` komutu sistemi şu donanım yapılandırmasıyla çalıştırır:
> * **RAM:** 256 MB (`-m 256`)
> * **Disk 0 (Master):** 10 MB LAFS yerel sistem diski (`output/disk.img`)
> * **Disk 1 (Slave):** VFS `/mnt` için Ext2 dosya sistemi (`output/ext2.img`)
> * **VGA:** Standart VBE/VGA adaptörü (`-vga std`)
> * **Ağ Kartı:** Realtek 8139 PCI NIC + QEMU User-Mode Ağ Köprüsü (`-net nic,model=rtl8139 -net user`)

---

### Önyüklenebilir Canlı ISO Oluşturma

Gerçek donanımda (USB üzerinden) veya VirtualBox/VMware üzerinde test etmek için GRUB tabanlı bir ISO oluşturabilirsiniz:

```bash
# GRUB bootable ISO imajını oluşturur
make iso
```

Oluşturulan `output/scratchos.iso` dosyası; Rufus, BalenaEtcher veya `dd` kullanılarak doğrudan bir USB belleğe yazdırılabilir.

---

## Kabuk (lash) Komut Referansı

Çekirdek başarıyla açıldığında ekrana gelen `lash` kabuğu üzerinden aşağıdaki komutlar kullanılabilir:

| Komut | Açıklama |
| :--- | :--- |
| `help` | Kullanılabilir tüm dahili komutları listeler. |
| `clear` | Terminal ekranını temizler ve imleci başa alır. |
| `version` | Çekirdek ve kabuk sürüm bilgisini görüntüler. |
| `uptime` | Sistemin açık kaldığı süreyi dakika, saniye ve tick cinsinden raporlar. |
| `mem` | PMM bellek kullanımını (toplam, kullanılan ve boş çerçeveleri/MB) listeler. |
| `pci` | PCI veriyolunu tarayarak bağlı aygıtların Vendor/Device ID ve BAR bilgilerini dökümler. |
| `ticks` | PIT zamanlayıcısının başlangıçtan itibaren ürettiği toplam kesme sayısını verir. |
| `sleep <ms>` | Sistemi belirtilen milisaniye kadar bekletir (örn: `sleep 1000`). |
| `echo <metin>` | Verilen metni konsola yazdırır. |
| `ls [yol]` | Mevcut veya belirtilen dizin altındaki dosya ve klasörleri listeler. |
| `cd <yol>` | Çalışma dizinini değiştirir (`..` üst dizin desteği mevcuttur). |
| `mkdir <ad>` | Geçerli dizin altında yeni bir klasör oluşturur. |
| `touch <ad>` | Belirtilen isimde yeni boş bir dosya oluşturur. |
| `cat <dosya>` | Metin dosyasının içeriğini ekrana yazdırır. |
| `rm <dosya>` | Belirtilen dosyayı dosya sisteminden siler. |
| `rmdir <dizin>` | Boş bir dizini siler. |
| `exec <dosya>` | Disk üzerindeki ELF32 ikili dosyasını kullanıcı modunda (Ring 3) çalıştırır. |
| `vinit` | VBE 32bpp yüksek çözünürlüklü grafik modunu (GUI) başlatır. |
| `tui` / `exit_gui` | Grafik modundan metin konsoluna (TUI) geri döner. |
| `beep` | AC97 ses denetleyicisi üzerinden PCM test sesi çalar. |
| `format` | LAFS dosya sistemini sıfırlar ve yeniden biçimlendirir. |
| `reboot` | Klavye kontrolcüsü (PS/2 8042) üzerinden donanımsal yeniden başlatma yapar. |
| `shutdown` | QEMU ACPI/APM portu (`0x604`) üzerinden sistemi kapatır. |

---

## Proje Dizin Yapısı

```
FerkanOS/
├── boot/                      # Önyükleme katmanı
│   └── boot.s                 # Multiboot başlığı, GDT/Paging önyükleme montaj kodu
├── include/                   # Çekirdek ve sürücü başlık (.h) dosyaları
├── kernel/                    # Monolitik çekirdek kaynak kodları
│   ├── core/                  # Çekirdek çekirdeği (kernel.c, gdt, idt, irq, panic, serial)
│   ├── drivers/               # Donanım sürücüleri (ata, keyboard, mouse, pci, timer, vbe, ac97, rtl8139, e1000)
│   ├── fs/                    # Dosya sistemi (vfs, lafs, ext2)
│   ├── mm/                    # Bellek yönetimi (pmm, paging, kheap)
│   ├── net/                   # Ağ protokol yığını (net, socket, tcp)
│   ├── proc/                  # Süreç yönetimi (task, tss, syscall, elf)
│   └── ui/                    # Arayüz bileşenleri (shell 'lash', gui)
├── userland/                  # Kullanıcı alanı uygulamaları ve ELF derleme araçları
│   ├── hello.c, guess.c       # Temel konsol kullanıcı uygulamaları
│   ├── vi.c                   # Kullanıcı modu metin düzenleyici
│   ├── tcp_test.c, udp_test.c # Ağ soketi test programları
│   ├── syscalls.c, socket.h   # Ring 3 sistem çağrısı kütüphanesi
│   ├── user.ld                # Kullanıcı uygulamaları linker betiği
│   └── compile_user.sh        # Uygulamaları derleyip diske yükleyen betik
├── scripts/                   # Yardımcı araçlar, ISO ve LAFS disk biçimlendirme betikleri
│   ├── lafs_format.py         # LAFS disk formatlama aracı
│   ├── lafs_tool.py           # İmaj içerisine dosya enjekte etme aracı
│   └── make_iso.sh            # GRUB tabanlı bootable ISO üretim betiği
├── linker.ld                  # Çekirdek linker betiği (Higher-half 0xC0000000 eşlemesi)
├── Makefile                   # Otomatik derleme, bağlama ve çalıştırma kuralları
└── README.md                  # Proje dokümantasyonu
```

---

## Yol Haritası (Roadmap)

- [x] Temel Donanım Sürücüleri (Klavye, Fare, Disk, PCI)
- [x] Sanal Dosya Sistemi (VFS) ve Ext2 / LAFS Entegrasyonu
- [x] Çekirdek İçi Tam Durumlu TCP/IP Yığını ve BSD Soket API
- [x] ELF32 Uygulama Yükleyici ve Kullanıcı Modu (Ring 3) İzolasyonu
- [x] VBE Framebuffer Grafik Masaüstü & Fare Desteği
- [ ] Gelişmiş GUI Pencere Yöneticisi ve Olay Kuyruğu
- [ ] Uygulama Düzeyi HTTP ve DNS Protokolleri
- [ ] USB Kontrolcü Desteği (xHCI/EHCI)

---

## Lisans ve Yazar

Bu proje [MIT Lisansı](LICENSE) kapsamında açık kaynak olarak yayımlanmaktadır.

**Geliştirici:** Muhammed Yasir PEKER  
*Sistem Mühendisi & İşletim Sistemi Geliştiricisi*  
İletişim & Katkı: Projeye yönelik hata bildirimleri ve geliştirmeler için GitHub Issues sekmesini kullanabilirsiniz.
