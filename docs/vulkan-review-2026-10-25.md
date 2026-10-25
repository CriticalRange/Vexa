# Vulkan incelemesi — 25 Ekim 2026

Güncelleme: X11 WSI bulguları daha sonra düzeltildi. Uygulama, doğrulama sınırları ve pre-rotation önerisi için [düzeltme raporuna](vulkan-wsi-fixes-2026-10-25.md) bakın. Aşağıdaki kod satırları inceleme anındaki sürüme aittir.

Kapsam: AI'ın güncel X11 WSI değişikliğinin kaynak incelemesi, FEX'in Android Vulkan yolu ve bağlı Mali-G720 cihazda Megabonk denemesi. Ürün kaynak kodu, APK ve thunk ikilileri değiştirilmedi. Bu rapor, yeni worker'ın cihazda doğrulandığı anlamına gelmez: Megabonk doğrudan Android yüzeyini kullanıyor.

## Cihazda ölçülen durum

- Cihaz: TSORX4OB45L779WO, Xiaomi 2407FPN8EG, Mali-G720-Immortalis MC12.
- Depodaki ve cihazdaki `libvulkan-host.so` SHA-256: `27e047c7de8c68407595d2647f4539d68c3af18224704ded9ad5928a68c47fae`. Çalışan PID 24071'in maps dosyası aynı app-private kütüphane yolunu gösteriyor.
- Unity günlüğü `Forcing GfxDevice: Vulkan`, Vulkan 1.1 ve Android `vkCreate*Surface` yolunu doğruluyor. `pre-rotation mode=0 guest 2712x1220 -> swapchain 1220x2712` etkin; başlangıçta sürücüden 5, sonra 6 swapchain imajı geliyor.
- Önceden mevcut `files/vk_test_images` içeriği **4**. Bu tanı ayarı değiştirilmedi. İstenen minimum sayı, sürücünün döndürdüğü gerçek imaj sayısı değildir.
- Mevcut oyun ayarları: vsync=0, fps_limit=1000, bloom=1, AO=0, grass=0, shadows=0, AA=0. Değerler test için düşürülmedi.
- Normal açılış Steam Cloud kayıt çakışmasına takıldı. Cloud/device seçimi yapılmadı; mevcut yerel HOME ile görünür GameActivity üzerinden doğrudan ELF açıldı. Bu, normal Steam oturum akışının eşdeğerlik testi değildir.

| Örnek | Ölçüm | Sonuç | Sınır |
|---|---|---|---|
| Isınmış ana menü | SurfaceView BufferQueueProducer queueBuffer, 17 adet yaklaşık 1 sn pencere | ort. **143.60**, aralık **142.68–144.73 kare/sn** | İlk 3 başlangıç penceresi dışarıda. Ekrana sunum zaman damgası veya p99 değildir. |
| Forest Tier 1, Sir Oofie; oyuncu sabit, düşmanlar/otomatik saldırılar etkin | 10 sn boyunca 9 queueBuffer penceresi | ort. **115.24**, aralık **112.75–117.76 kare/sn** | Ekran görüntülerinde oyun saati ilerliyor. Aynı seed/kamera/oyun anıyla GL karşılaştırması yapılmadı. |
| Önceki 25 sn ilk oyun örneği | 25 queueBuffer penceresi | ort. 114.29 kare/sn | Son kısım level-up ekranını içeriyor; temiz oyun benchmark'ı olarak kullanılmamalı. |

Sıcak menü eski ~68 FPS durumunu tekrar üretmedi. Oyun sahnesindeki ~115 FPS, tek başına bir Vulkan köprü kaybını veya erişilebilir üst sınırı kanıtlamıyor. Menü ve oynanışın yükleri farklı. GPU yürütme süresi ile Acquire/Present beklemeleri aynı sahnede ayrıştırılmadan, bekleme süreleri GPU süresine eklenerek bir FPS bütçesi çıkarılamaz.

8 sn simpleperf örneği 5.119 örnek, sıfır kayıp üretti; **level-up ekranında** alındı. CPU örneklerinin %71.32'si sembolsüz/unknown, %15.86'sı libvexa, %4.73'ü Mali kütüphanesinde. Bu örnekten GPU kullanımı veya aktif oynanışın ana CPU darboğazı çıkarılamaz. VEXA_VK_PROFILE etkinleştirilmedi; GPU timestamp ölçümü yapılmadı. Son thermalservice genel durumu 0; bu tek başına frekans kısılmadığını kanıtlamaz.

## Güncel AI değişikliğinde doğru olanlar

- Caller release komutlarını kaydedip **tek vkQueueSubmit** yapıyor; çoklu swapchain için binary wait semaforlarını tekrar tüketme sorunu düzeltilmiş.
- Worker GPU fence ve X11 round-trip bekleyişini global mutex dışında yapıyor.
- pResults yalnızca çağrı sırasında yazılıyor; caller pointer'ı saklanmıyor.
- İmaj başına fence ve pending durumu var; başarılı X teslimi sonrası busy/IdleNotify korunuyor.
- DestroySwapchain'in condition_variable bekleyişi mutex'i bırakıyor. Worker'ın connection mutex'i, global mutex alınmadan önce bırakılıyor.

**Önceki incelememin düzeltmesi:** Bu kilit sırası kendi başına ABBA deadlock kanıtı değildir. Kuyruk da aktif swapchain'lerin toplam imaj sayısıyla sınırlıdır; çalışan işi de dahil ederek bu sınır geçerlidir. Bunları doğrulanmış hata olarak saymıyorum. Buna karşılık aşağıdaki uzun kilit tutma ve timeout sorunları gerçek.

## Bulgular

### 1. [P1] X11 Acquire pending imajlar için timeout sözleşmesini bozuyor

`app/src/main/cpp/x11/vulkan_wsi.cpp:496` pending imajları atlıyor; iki tarama bittikten sonra satır 527 civarında hemen VK_TIMEOUT dönüyor. Worker henüz X'e teslim etmediyse, **UINT64_MAX ile çağrı da anında TIMEOUT dönebilir**. Bu, iki imajlı yoğun present döngüsünde normal çalışma koşuludur; yalnızca nadir bir kenar durum değildir. İstemcide tekrar deneme döngüsü, gereksiz CPU yükü veya hatalı hata kurtarma tetikleyebilir.

Çözüm yönü: pending durum değişiminde haber veren condition variable, tek bir mutlak deadline, uyanınca tüm imajları yeniden tarama. Timeout=0 yolu bloklanmamalı; sonsuz timeout imaj veya hata beklemeli. X11 WaitIdle'ın 2 saniyelik I/O üst sınırı da sonsuz Acquire bekleyişinin sonu olarak yorumlanmamalı. [Vulkan timeout sözleşmesi](https://docs.vulkan.org/refpages/latest/refpages/source/vkAcquireNextImageKHR.html).

### 2. [P1] X11 Acquire uygulamanın kuyruğuna gizli submit yapıyor

`vulkan_wsi.cpp:518` Acquire içinde `s.queue` üzerine ownership submission gönderiyor. Uygulama başka thread'den aynı VkQueue'ya vkQueueSubmit yaparken Acquire çağırabilir; normal Acquire için uygulamanın bu kuyruğu kilitlemesi gerekmiyor. WSI'nin global mutex'i, sürücüye doğrudan giden guest submit yolunu korumuyor. Böylece driver queue host erişimi eşzamanlı hale gelebilir.

Çözüm yönü: uygulama tarafından kullanılan tüm ilgili queue işlemlerini kapsayan ortak senkronizasyon veya WSI'ye ait uygun bir kuyruk ve açık semaphore bağımlılıkları. Yalnızca worker'a lock eklemek çözmez. [Queue host synchronization](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit.html).

### 3. [P2] X11 Acquire global mutex altında X11 bekliyor

`vulkan_wsi.cpp:471` lock alıyor; satır 507'de WaitIdle aynı kilitle çalışıyor. Bu sırada worker bitmiş başka imajların pending durumunu temizleyemiyor; diğer swapchain Acquire/Present çağrıları da bekliyor. Connection mutex'i de bağlantının diğer işlemlerini sıraya sokuyor. İki saniyelik I/O sınırı bu etkinin kare bütçesinden çok daha uzun olmasına izin veriyor.

Çözüm yönü: swapchain ömrünü sabitleyip blocking I/O sırasında global kilidi bırakmak; event geldikçe uygun herhangi bir imajı yeniden taramak. Worker'ın global kilidi beklemesi tek başına kalıcı deadlock kanıtı değil; burada doğrulanan şey ilerlemeyi gereksiz seri hale getirmek.

### 4. [P2] Worker hâlâ her karede X11 round-trip yapıyor; tek FIFO diğer işleri de bekletiyor

`x11_present.cpp:396`: PresentPixmap + GetInputFocus round-trip. `vulkan_wsi.cpp:96` ve 108: tek worker önce GPU fence, sonra bağlantı round-trip bekliyor. Caller serbest kalsa da köprünün toplam servis hızı sınırlı. Bir queue/swapchain'in yavaş fence'i, bağımsız diğerlerinin işlerini de durdurabiliyor.

Çözüm yönü: X protokol hatalarını/event'lerini sürekli işleyen bağlantı döngüsüyle present'leri paketlemek; bağımsız queue'ların ilerlemesini korumak. GPU fence beklemesini kanıtsız kaldırmak güvenli değil. Bu bulgu Megabonk'un native Android yolunda etkin değil.

### 5. [P2] Swapchain destruction, condition-variable bekleyişi boyunca geçersizleşebilen iterator saklıyor

`vulkan_wsi.cpp:448–459`: chains.find sonucundaki iterator, drainCv.wait sırasında korunuyor. Wait global kilidi bıraktığında başka bir swapchain oluşturulup unordered_map rehash olursa iterator geçersizleşir; ardından chains.erase(it) tanımsız davranıştır. unique_ptr'ın hedefindeki Swapchain referansı rehash ile bozulmaz; problem erase için saklanan iterator'dır.

Çözüm yönü: bekleme sonrası handle ile tekrar bulmak veya anahtarla silmek. Bu bir steady-state FPS kusuru değil, eşzamanlı swapchain oluşturma/yok etme sırasında çökme riski.

### 6. [P2] Kalıcı present/connection hatası yalnızca bir çağrıda raporlanıp unutuluyor

Worker başarısız Present sonrasında pending=false yapıyor; busy yalnızca başarıda true oluyor (`vulkan_wsi.cpp:120–124`). Acquire/Present deferredError'ı okuyup temizliyor (483, 600). Bağlantı/surface hâlâ bozukken sonraki Acquire bir imajı yeniden kullanılabilir sayabilir. Ayrıca PresentConnection, isteği göndermeden önce kendi pixmap busy durumunu true yapıyor; hata halinde iki katmanın durumları ayrışabiliyor.

Çözüm yönü: surface/device kaybı için kalıcı swapchain hata durumu ve güvenli drain/cleanup. Hataları bir defa loglamak veya bir defa döndürmek yeterli değil.

### 7. [P2] Swapchain cleanup bütün device'ı global kilit altında boşaltıyor

`vulkan_wsi.cpp:239` vkDeviceWaitIdle, DestroySwapchain tarafından global mutex tutulurken çağrılıyor. Başka swapchain ve kuyrukların GPU işlerini de bekletir. Normal her kare yolunda çalışmaz; resize, alt-tab ve swapchain rebuild anlarında takılma kaynağıdır.

Çözüm yönü: bu swapchain'in bütün render/present kaynaklarının tamamlandığını kanıtlayan kapsamlı ömür takibiyle sınırlı bekleme. Yalnız release fence'ini beklemek, tüm presentation kaynaklarının ömrünü tek başına kanıtlamaz.

### 8. Native Android: pre-rotation ek tam ekran pass ve submit getiriyor; maliyeti ayrıştırılmadı

`../FEX/ThunkLibs/libvulkan/vulkan_prerotate.inc:307–325, 462–484, 594`: oyun özel imajlara çiziyor; köprü bunları shader ile okuyup gerçek swapchain'e döndürerek yazıyor. Etkin 2712x1220 RGBA8 örneğinde bir tam okuma + yazmanın mantıksal veri hacmi yaklaşık **26.5 MB/kare**. Bu fiziksel DRAM bant genişliği ölçümü değildir; cache/sıkıştırma sonucu değiştirir. Son çizime ek bağımlılık ve ek submit de var.

Bu bir doğruluk hatası veya kapatılınca hızlanma kanıtı değil: compositor döndürmesi daha pahalı olabilir. [Android'in önerisi](https://developer.android.com/games/optimize/vulkan-prerotation), mümkünse oyunun görüntüyü baştan döndürülmüş üretmesidir. Köprü burada ek son pass ile uyumluluk sağlıyor. İlk native performans adayı, aynı sahnede bu pass'in GPU süresi ile Android sunum zincirini birlikte ölçmek; körlemesine kapatmak değil.

### 9. [P2] Native pre-rotation fence bekleme/reset hataları kontrol edilmiyor

`vulkan_prerotate.inc:582–583`: her present'te önceki aynı-imaj rotasyon fence'i CPU'da bekleniyor; iki fonksiyonun VkResult'u kullanılmıyor. Bekleme başarısız olduğunda kod submit'e devam edebilir. **Bu fence, yeni kare render'ının sonuna yerleştirilmiş X11 bekleyişiyle aynı şey değildir:** önceki rotasyon command buffer/fence kaynağının güvenli tekrar kullanımını korur. Normal başarılı Acquire sonrasında çoğu zaman hazır olması beklenir; ölçmeden büyük FPS kaybı demek yanlış olur.

Çözüm yönü: hata sonuçlarını işlemek; bekleyişin gerçek süresini, hazır olma sıklığını ve imaj tekrar kullanımını ölçmek. GPU tamamlanma güvencesi olmadan wait/reset kaldırılmamalı.

## Sonraki ölçüm önceliği

1. X11 doğruluk kusurları (#1, #2, #5, #6) worker değişikliğinin kendi kapsamındaki öncelikler. Megabonk bu yolu sınamıyor; Windows/DXVK veya gerçek-X11 Vulkan probe gerekir.
2. Megabonk'ta aynı oyun sahnesi/seed/kamera/çözünürlük/ayarlarla GL ve Vulkan karşılaştırması; GPU zamanı, queue submit, Acquire, rotation pass, Android queueBuffer ve ekrana sunum zamanları birlikte yakalanmalı.
3. Profiler VEXA_VK_PROFILE, command buffer'lara timestamp ve ilave marker submit eklediğinden kendi maliyeti de kontrol edilmeli. Profil açık/kapalı FPS aynı metrikle karşılaştırılmalı.
4. Native path'teki per-frame rotation fence bekleyişi, global WSI metadata lock'ları ve ADPF raporu ayrı ölçülmeli. Mevcut kanıt bunlardan birine kalan FPS açığının tamamını yüklemiyor.

## Deneme sonu

Oyun force-stop ile kapatıldı. Test öncesi Saves ağacı yerel tar'a alınmıştı; arşivdeki mevcut dosyaların özgün baytları test sonunda doğrulandı/geri yüklendi. Cloud kayıt seçimi yapılmadı. `vk_test_images`, boot.config, shader/JIT/profiler flag'leri, APK ve runtime ikilileri değiştirilmedi.

Kanıtlar: `logs/vulkan-review-20261025/` altında warm/active ekran görüntüleri, queueBuffer pencereleri, Player günlüğü, CPU raporları, maps, thermal çıktıları ve geri yükleme kontrolü. Warm menü ortalaması `baseline-summary.json`; aktif oyun örneği `active-summary.json`. Yeni worker kaynak dosyası inceleme sonunda SHA-256: `50b719af617a2275298c22fc8326dbf9633169e128dfb0f3b54acd80676f555b`.
