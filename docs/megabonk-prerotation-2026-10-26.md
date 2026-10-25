# Megabonk native Vulkan pre-rotation ölçümü — 2026-10-26

## Sonuç

Bu cihazdaki ölçümlerde pre-rotation'ın önceki kullanım fence'ini beklemek ana darboğaz çıkmadı. Normal fence yolunda batch p50 değerleri yaklaşık 0,018–0,031 ms; ilave fullscreen dönüşüm geçişinin GPU timestamp aralığı yaklaşık 0,30–0,47 ms. Timeline deneyi bekleme/reset çağrılarını kaldırdı ama tutarlı FPS kazancı göstermedi. Varsayılan fence yolu korundu; timeline yalnız özel test bayrağıyla açılıyor.

Bu sonuç bütün Vulkan maliyetini açıklamıyor. Ek GPU geçişi sıfır maliyetli değil, ancak burada görülen yaklaşık 0,35–0,4 ms, çok büyük FPS açığını tek başına açıklamıyor. Son composite pass'e fusion ayrı, daha kapsamlı bir çalışma olarak kalıyor.

## Ortam ve yöntem

- Cihaz TSORX4OB45L779WO, Xiaomi 2407FPN8EG / Mali-G720; Android 16.
- Megabonk native Android Vulkan yolu, FEX x86_64; `VEXA_REAL_X11` kullanılmadı. Önceki X11 WSI worker düzeltmeleriyle karıştırılmamalı.
- Mantıksal görüntü 2712×1220, gerçek swapchain 1220×2712, rotation mode 0.
- Mevcut kayıt/ayarlar teste başlamadan `logs/megabonk-rotation-20261026/saves-before.tar` içine byte olarak yedeklendi. Cloud-conflict seçimleri kullanılmadan direct launch yapıldı.
- Rotation ham ICD çağrıları mevcut guest profiler'ını atladığından özel opt-in profil eklendi. Her 300 örnekte wait/submit/GPU/üretim aralığı p50/p95/p99 raporlanıyor. Son sürüm ayrıca wait thread CPU süresini ve native present wall/thread CPU ortalamasını raporluyor.
- GPU zaman damgaları BOTTOM_OF_PIPE noktaları arasında rotation komutlarını kapsıyor. Query sonuçları tamamlanmış önceki imaj kullanımından, WAIT_BIT olmadan okunuyor. Timestamp geçişlerinin GPU zamanlamasını etkileyebileceği için bunlar profilesiz üst FPS sınırı sayılmamalı.
- FPS değerleri SurfaceView `queueBuffer` üretici hızının yaklaşık bir saniyelik pencerelerinin ortalaması. Ekranda gerçekten gösterilen FPS veya bu pencerelerden türetilmiş p99 değildir. `summary.json` içindeki percentile ortalamaları batch percentile'larının ortalamasıdır; bütün koşunun birleşik percentile'ı değildir.

## Bulgular

İlk normal fence host'uyla sıcak menüde p50 wait yaklaşık 0,018–0,025 ms, raw submit 0,025–0,030 ms ve GPU geçişi 0,31–0,38 ms görüldü. Forest çalıştırıldı; `gameplay-profile.png` bir level-up ekranını gösteriyor. Bu bölümün son örneklerinde wait yaklaşık 0,024–0,025 ms, GPU yaklaşık 0,37–0,39 ms. Bu ekran aktif oynanış performansı olarak sunulmuyor.

Timeline host'unda aynı süreçte, aynı harita seçim ekranından ayrılmadan fence/timeline değiştirildi. Önceki input trace bayrağı bulundu; ilk A/B/A'da açık, ikinci seride geçici olarak kapalıydı. İlk seri 110,89 → 105,77 → 100,71 FPS ile belirgin zamansal drift gösterdi; tek başına hızlanma/yavaşlama kanıtı değil.

Input trace kapalı ikinci seri, 4 saniye mod geçiş payı ve her biri 12 saniyelik örnekler:

| Yol | SurfaceView producer FPS | Wait batch-p50 ortalaması | GPU batch-p50 ortalaması |
|---|---:|---:|---:|
| A — fence | 103,05 | 0,03098 ms | 0,3367 ms |
| B — timeline | 103,72 | 0,00010 ms | 0,3353 ms |
| A — fence tekrar | 102,86 | 0,03113 ms | 0,3399 ms |

Yaklaşık %0,7 farktan kazanım iddiası çıkarılmadı. İki modda da aynı özel timeline retirement sinyali var; A ayrıca önceki fence'i bekleyip reset ediyor. Bu karşılaştırma CPU fence beklemesini kaldırmayı izole ediyor; orijinal binary-fence implementation ile timeline feature/ek signal maliyetinin tamamını karşılaştırmıyor. Normal ilk host'ta daha düşük raw submit p50 görülmesi de farklı zaman/termal koşulları nedeniyle tek başına timeline regresyon kanıtı değil.

Son APK'nın normal fence yolunda GPU timestamp komutları kapalı kontrolü de çalıştırıldı (`cpu-only-player.log`): örnek batch'lerde wait wall p50 0,0305–0,0353 ms, wait thread-CPU p50 0,0292–0,0338 ms. Yakın değerler, bu sahnede uzun bir GPU completion uykusundan ziyade kısa driver çağrı maliyetine işaret ediyor. Raw submit p50 yaklaşık 0,032 ms; native present 256 çağrılık pencerelerde ortalama yaklaşık 1,46 ms wall / 0,53 ms thread CPU. Bu değerler de toplam frame süresinin tamamını açıklamıyor. GPU-query açık/kapalı koşular farklı zamanda ve farklı menü durumlarında olduğundan profiler overhead'i için kesin FPS farkı çıkarılmadı.

## Kod

Komşu FEX reposu `ThunkLibs/libvulkan/vulkan_prerotate.inc` ve `Host.cpp`:

- Wait/reset VkResult kontrol ediliyor. Başarısız beklemeden sonra reset/submit yapılmıyor.
- Fence yalnız başarılı submit'ten sonra pending olarak izleniyor. Reset sonrası submit OOM hatası, hiç sinyallenmeyecek bir fence'i cleanup sırasında bekletmiyor. OOM ile device loss ayrılıyor; önceki başarılı işler cleanup tarafından beklenebiliyor.
- Static command buffer ilk present queue'suna bağlı; farklı queue'ya sessizce yanlış family komutları gönderilmek yerine out-of-date/fallback isteniyor.
- Opt-in timeline deneyi guest feature zincirini değiştirmeden feature/extension desteğini kontrol ediyor. Caller açıkça timeline özelliğini kapatmışsa zorlamıyor. Aynı swapchain'in özel completion timeline'ı yalnız kaydedilen queue'dan monoton değerlerle sinyalleniyor. WSI'nin beklediği Done semaforları binary kalıyor.
- Timeline komutları SIMULTANEOUS_USE ile kaydediliyor. Normal karelerde host completion wait/reset kaldırılıyor; resource retirement sırasında son başarılı timeline değeri bekleniyor. Query okuması için tamamlanma ayrıca kontrol ediliyor.
- Deney ve profil normalde kapalı. Mevcut pre-rotation açık kalıyor.

Bayraklar `files/` altında, süreç yeniden başlatılarak:

- `vk_rotation_profile.flag`: dar kapsamlı rotation ve native present metrikleri.
- `vk_rotation_cpu_only.flag`: profil açıkken GPU timestamp komutlarını ekleme.
- `vk_rotation_timeline.flag`: device creation sırasında timeline deneyini etkinleştir.
- `vk_rotation_fence.flag`: timeline device'ında 300 karede bir kontrol edilen karşılaştırma seçeneği; varsa wait/reset/fence submit kullan.
- Önceden var olan `vk_prerotate_off.flag`: pre-rotation'ı tamamen kapatır; bu test serisinde kullanılmadı.

## Doğrulama ve kapsam

`guest-tests/compat/vulkan_rotation_state_test.cpp` üretim submit yolunu mock ICD ile çalıştırıyor. Cihazda altı PASS grubu: guest wait + binary present zinciri, wait hatasında kısa devre, reset/submit OOM sonrası phantom cleanup beklemesi olmaması, timeline'da wait/reset olmaması, kontrollü fence modu, başarısız timeline submit'te değerin ilerlememesi ve farklı queue'nun reddi. Bu mock test gerçek driver feature injection veya tüm destruction yarışlarını doğrulamaz.

Gerçek Mali üzerinde timeline ve fence yollarında görünür oyun/menü çizimi ve mod geçişleri çalıştı. Native host ve debug APK derlendi; cihaza kuruldu. Yalnız Vulkan host asset'i ayrıca stage edildi; önceki GL/SDL ve WSI değişiklikleri korundu. Toplu FEX configure/pin verification/staging görevleri önceki çalışma gibi atlandı. Genel runtime/rootfs temizliği veya bütün oyunlarda performans doğrulaması yapılmadı.

Kanıtlar `logs/megabonk-rotation-20261026/` altında; `summary.json`, `clean-*-samples.log`, `clean-*-logcat.log`, termal raporlar ve ekran görüntüleri. Son kurulum/hash ve kayıt geri yükleme doğrulaması `final-validation.json` içinde.

APK asset'i ile cihazdaki Vulkan host SHA-256 eşleşti: `3c0b6a8b4fbd8754c8a9aed9a3166749bc866094eddddcf8503e0b5929e794b0`. Kayıt yedeğindeki 17 dosya geri yüklendi ve byte eşitliği kontrol edildi. Deney/profil bayrakları kaldırıldı; test öncesi var olan input-trace bayrağı geri getirildi. Normal pre-rotation açık, timeline deneyi kapalı. Sonrasında oyun normal menü için tekrar açıldı.

Teknik dayanak: [Vulkan command-buffer pending/simultaneous-use kuralları](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html), [timeline feature etkinleştirmesi](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceTimelineSemaphoreFeatures.html), [submit değerlerinin semaforlarla eşleştirilmesi](https://docs.vulkan.org/refpages/latest/refpages/source/VkTimelineSemaphoreSubmitInfo.html).
