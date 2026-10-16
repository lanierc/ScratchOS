# ScratchOS Sistem Güçlendirme Denetimi (Audit)

Bu dosya, ScratchOS çekirdeğindeki (kernel) hataları, performans darboğazlarını ve eksiklikleri belgelemektedir. Tespit edilen sorunlar önem derecesine göre sıralanmıştır.

## 🔴 Kritik Sorunlar (Sistem Çökmesine/Veri Kaybına Yol Açabilir)

1.  **Bellek ve Disk Erişiminde Yarış Durumu (Race Condition):**
    - `kmalloc`, `kfree`, `lafs_write`, `lafs_read` gibi fonksiyonlarda **kilitleme (locking/mutex)** mekanizması yok.
    - Çoklu görev (multitasking) etkinken iki görev aynı anda bellek ayırmaya çalışırsa heap yapısı bozulur ve sistem çöker (Triple Fault).
    - Aynı durum disk işlemleri için de geçerlidir; ATA kontrolcüsü aynı anda iki isteği işleyemez.

2.  **VFS Katmanında Bellek Sızıntısı (Memory Leak):**
    - `lafs_vfs_finddir` fonksiyonu her çağrıldığında `kmalloc` ile yeni bir `vfs_node_t` oluşturuyor ancak bu düğümler (node) hiçbir zaman serbest bırakılmıyor (`kfree` çağrılmıyor).
    - Uzun süreli kullanımda sistem belleği tükenecektir.

3.  **Statik Değişken Kullanımı (Thread-Safety):**
    - `lafs.c` içindeki `static struct vfs_dirent dirent` yapısı tüm görevler tarafından ortak kullanılıyor. Bir görev dizin okurken diğeri okursa veriler birbirine karışır.

4.  **`create_task` İçinde Yarış Durumu:**
    - Görev listesine (`task_list`) yeni bir görev eklenirken kesmeler (interrupts) kapatılmıyor. Bu sırada bir zamanlayıcı kesmesi gelirse liste yapısı bozulabilir.

## 🟡 Performans ve Verimlilik Sorunları (Yavaşlık)

1.  **Aşırı Disk Yazma (Inefficient Metadata Sync):**
    - `save_metadata` fonksiyonu her dosya oluşturulduğunda veya boyutu değiştiğinde tüm Inode tablosunu (16 sektör) diske yazıyor. Sadece değişen sektörün yazılması gerekir.

2.  **PMM Alokasyon Yavaşlığı:**
    - `alloc_frame` fonksiyonu her seferinde bitmap'i en baştan tarıyor ($O(n)$). Bellek doldukça boş yer bulmak çok daha uzun sürecektir.

3.  **Disk Önbelleği (Disk Cache) Eksikliği:**
    - Her dosya okuma/yazma işleminde doğrudan ATA sürücüsüne gidiliyor. Okunan blokların bellekte tutulması (Buffer Cache) performansı 10-100 kat artıracaktır.

4.  **Gereksiz TLB Flush:**
    - `paging_map_memory` fonksiyonu döngü içindeki her sayfa için değil, sadece sonunda CR3'ü güncelleyerek TLB'yi temizliyor (bu doğru) ancak tek sayfalık güncellemeler için `invlpg` komutu kullanılmıyor.

## 🔵 Tasarım ve Altyapı Eksiklikleri

1.  **Heap Boyutu Sınırı:**
    - Kernel heap alanı 4MB ile sınırlı. Grafik arayüz ve çoklu uygulama kullanımı için bu miktar çok hızlı dolacaktır. Dinamik olarak genişleyebilen bir heap yapısı (paging desteğiyle) gereklidir.
2.  **Hata Kontrolü Eksikliği:**
    - `kmalloc` NULL döndüğünde çoğu fonksiyon bu durumu kontrol etmiyor ve doğrudan NULL üzerinden işlem yapmaya çalışıyor (Kernel Panic).
3.  **Hızlı String Fonksiyonları:**
    - `memcpy` ve `memset` gibi fonksiyonlar çok basit (byte-byte) implemente edilmiş. Bunlar `long` veya SIMD (mevcutsa) kullanılarak hızlandırılabilir.

---
**Öneri:** İlk olarak "Kritik" sınıftaki Yarış Durumlarını (Locking) çözmeliyiz. Ardından bellek sızıntılarını gidermeliyiz.
