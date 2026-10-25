# Vulkan WSI düzeltmeleri ve pre-rotation araştırması

25 Ekim 2026. X11/Proton WSI düzeltildi; native Android pre-rotation kodu bu çalışmada değiştirilmedi.

## Altı bulgunun karşılığı

1. **Acquire timeout:** pending ve busy imajlar için swapchain condition variable'ı bekleniyor. Sonsuz timeout erken TIMEOUT üretmiyor; sonlu timeout tek mutlak deadline kullanıyor; timeout=0 bloklanmıyor. Hazır olan herhangi bir imaj yeniden taranıyor. Deadline hesabında taşma sınırlandırıldı.
2. **Gizli submit ile guest kuyruğu arasındaki yarış:** WSI ownership submit'leri ve FEX üzerinden gelen QueueSubmit, QueueSubmit2/core+KHR, QueueBindSparse, QueueWaitIdle aynı kuyruk kapısından geçiyor. DeviceWaitIdle o device'ın kayıtlı kuyruklarını tutarlı sırayla kilitliyor; normal QueueWaitIdle yalnız kendi kuyruğunu tutuyor. Böylece başka kuyruktan gerekli semaphore sinyalinin gönderilmesi engellenmiyor. Native Android yolu, VEXA_REAL_X11 kapalıyken değiştirilmedi.
3. **Global kilit altında bekleme:** Acquire artık yalnız kendi swapchain durumunu kilitliyor; condition variable beklerken bu kilidi bırakıyor. X olayları worker'da işleniyor. Swapchain import, cleanup ve worker join de global registry kilidi dışında.
4. **Present round-trip ve global worker kuyruğu:** WSI artık QueuePresent ile yalnız PresentPixmap gönderiyor. Aynı bağlantıyı yöneten worker, IdleNotify ve gecikmeli protokol hatalarını sürekli okuyor. Swapchain başına worker var; aynı chain'in sırası korunurken bağımsız chain'ler birbirinin GPU fence'ini beklemiyor. eventfd, yeni iş/yok etme isteğinde bağlantı poll'unu uyandırıyor. GPU tamamlanma güvencesi korunuyor; bekleme silinmedi. Eski senkron Present API'si diğer kullanıcılar için korunuyor.
5. **Destroy iterator ömrü:** swapchain registry'den kilit altında çıkarılıp sahipliği yerel shared_ptr'a alınıyor; worker sonra join ediliyor. Bekleyiş boyunca unordered_map iterator'ı saklanmıyor.
6. **Kalıcı hata ve cleanup:** surface/device hatası latch ediliyor ve sonraki Acquire/Present çağrılarında tekrar dönüyor. Bozuk bağlantının imajları yeniden dağıtılmıyor. Cleanup, device'ın bütün GPU işlerini durdurmak yerine worker'ın release işlerini ve WSI'ye ait acquire completion fence'lerini bekliyor. Caller'ın fence pointer/handle ömrü cleanup'a taşınmıyor.

Caller hem semaphore hem fence verirse, özel ownership completion fence'i ile caller primitive'leri iki ardışık submit kullanıyor. İlk ownership işi caller primitive'lerini sinyallemiyor; ikinci submit semaphore ve caller fence'i sinyalliyor. İkinci submit hata verirse caller semaforu değişmeden kalıyor. Kuyruk kilidi iki submit boyunca korunuyor. Fence reset sonrası submit başarısız olursa pending GPU işi varmış gibi cleanup bekleyişi bırakılmıyor.

Çoklu swapchain present, wait semaphore'larını tek submission'da tüketiyor. Bu submission'ın completion fence'i ilgili worker'lar arasında shared ownership ile tutuluyor. İş sayısı aktif swapchain imajlarıyla sınırlı.

Değişen ürün kaynakları:

- `app/src/main/cpp/x11/vulkan_wsi.cpp`
- `app/src/main/cpp/x11/x11_present.cpp` ve `.h`
- Komşu FEX reposu: `ThunkLibs/libvulkan/Host.cpp` — generated/custom queue entry point'lerini aynı kapılara yönlendirme.
- Eşleşen `libvulkan-host.so` runtime asset'i ve STAMP.

## Doğrulama

- FEX `vulkan-host-64` hedefi ve Android native libvexa derlendi.
- Debug APK assembleDebug başarılı; güncel APK cihaza kuruldu. FEX pin doğrulama/configuration ve toplu thunk staging adımları atlandı; mevcut FEX build ağacı kullanıldı ve yalnız yeni Vulkan host asset'i ayrıca stage edildi. Önceden var olan GL/SDL ve lock dosyası değişiklikleri korunuyor.
- Üretim WSI kaynağını kullanan Android-native state harness cihazda geçti: timeout=0, sonlu deadline, sonsuz bekleme, global registry ilerlemesi, kalıcı Acquire/Present hatası, guest submit ile iç Acquire submit'inin karşılıklı dışlanması, bağımsız queue ilerlemesi, bağımsız worker'lar, GPU completion öncesi present göndermeme, IdleNotify sonrası reuse ve destruction sırasında registry rehash.
- İki hata enjeksiyonu geçti: fence reset sonrası başarısız submit'in phantom cleanup wait yaratmaması; caller-fence marker başarısızlığında caller semaforunun değişmemesi.
- Gerçek X11 parser/socketpair testi cihazda geçti: PresentPixmap yanında GetInputFocus round-trip gönderilmemesi, bölünmüş IdleNotify paketinin doğru birleştirilmesi ve gecikmeli protokol hatasının bağlantıyı kapatması.
- **Gerçek Mali ICD + Termux:X11 testi geçti:** üretim WSI/connection kaynaklarıyla derlenen Android-native probe, iki imajlı swapchain'de 120 Acquire(semaphore+fence) → clear/render → async Present çevrimi çalıştırdı. Caller acquire fence'leri swapchain'den önce yok edildi; cleanup başarılı tamamlandı. X11 renderer'dan görüntü alındı.
- Hazır Linux vkcube GLIBC_2.38 eksikliği nedeniyle Vulkan'a ulaşamadı. Bu nedenle son test FEX guest ABI'sinden geçmiyor; FEX host derlemesi, queue yönlendirme kodu ve native WSI testleri ayrı kanıtlardır. Tam Windows/DXVK oyun testi veya 32-bit Vulkan testi yapılmadı.
- Genel run manifest'te önceden mevcut dört rootfs uyumsuzluğu var: libX11, libasound, libudev ve eksik libvexa_evdev. Bunlar değiştirilmedi; Vulkan guest/host payload'ları eşleşiyor. Bu çalışma bütün runtime paketinin temiz olduğunu iddia etmiyor.

Regresyon kaynakları `guest-tests/compat/` altında: `vulkan_wsi_state_test.cpp`, `x11_present_event_test.cpp`, `vulkan_wsi_device_probe.cpp`, `vulkan_rotation_caps.cpp`. Örnekler Android NDK clang++ ile `--target=aarch64-linux-android28 -std=c++20 -static-libstdc++` kullanılarak derlendi. State harness ayrıca `-Wl,--export-dynamic -landroid -ldl`; device probe üretim WSI ve connection .cpp dosyalarıyla `-lvulkan -landroid -ldl` kullanıyor. Wire testi connection .cpp kaynağını kendi translation unit'ine dahil ediyor ve `-landroid` gerektiriyor.

Kanıtlar: `logs/vulkan-review-20261025/wsi-state-results.txt`, `actual-wsi-final-results.txt`, `actual-wsi-final.png`, `baseline-wsi-color.png`, `rotation-capabilities.txt` ve `final-validation.json`.

### Ayrı renk uyumluluğu bulgusu

Clear rengi (0.1, 0.65, 0.25) için beklenen 8-bit RGB yaklaşık (26,166,64); X11 yakalamasında (64,166,26) görülüyor. **Git HEAD'deki eski senkron WSI koduyla aynı test de (64,166,26) üretti.** R/B sırası sorunu yeni async değişiklikten önce de var. Bu çalışmada renk formatı/import politikası değiştirilmedi. Çevrim ve görüntü aktarımının başarılı olması, bütün renklerin/oyunların doğrulandığı anlamına gelmiyor.

## Pre-rotation: güncel kanıt ve öneri

Mali-G720 cihaz sorgusu şunları doğruladı:

- `VK_QCOM_render_pass_transform` **yok**.
- `VK_KHR_timeline_semaphore` var; `timelineSemaphore=1`.
- `VK_EXT_image_compression_control` ve swapchain varyantı var; bunların varlığı tek başına bir hızlanma veya mevcut yüzeyin sıkıştırıldığı kanıtı değil.

### Önce ölçümdeki boşluğu kapatmak

`Host.cpp:288` civarında VexaRotSubmit, present CPU sayacı başlamadan önce çağrılıyor; sayaç yaklaşık satır 314'te başlıyor. VexaRotSubmit'in fence wait ve ek queue submit maliyeti `VK-PERF present` sayısına dahil değil. Ayrıca rotation'ın Fns.vkQueueSubmit/vkCmd* çağrıları ham ICD pointer'larıyla yapıldığından mevcut guest submit/pass profiler'ı rotation draw'unu otomatik kapsamıyor.

Dolayısıyla ilk adım üç şeyi ayrı ölçmek: önceki rotation fence'inin CPU wall/thread-CPU süresi, rotation submission/draw GPU süresi ve Android sunum bekleyişi. Query sonuçları sonraki tamamlanmış frame'lerden bloklamadan okunmalı. Profil açık/kapalı kare süreleri de karşılaştırılmalı. Şu an rotation'ın kaç ms kaybettirdiği veya kaç FPS kazandırılabileceği bilinmiyor.

### Daha dar kapsamlı seçenek: host fence bekleyişini timeline ile kaldırmak

Mevcut pre-rotation static command buffer'ları SIMULTANEOUS_USE olmadan kaydediyor; her imajda önceki kullanım fence'i beklenip reset ediliyor. Wait/reset VkResult'ları da kontrol edilmiyor. Bekleme doğrudan kaldırılamaz: native Acquire, index'i döndürdüğünde GPU kullanımının tamamlanmış olmasını host'a garanti etmez; acquire semaphore/fence sinyali önemlidir.

Uygulanabilir tasarım adayı: uygun device extension/feature etkinleştirmesi, static command buffer için güvenli simultaneous-use, queue başına özel timeline completion sayacı, aynı imajın present→acquire→render→rotation semaphore zincirinin korunması; host completion bekleyişini yalnız destruction/resource retirement'a taşımak. Bu CPU bekleyişini azaltabilir; **tam ekran rotation draw'unu ve onun GPU maliyetini kaldırmaz**. Çoklu queue/family, submit hatası, presentation semaphore reuse ve device loss ayrıca doğrulanmalı. Bu tasarım henüz uygulanmadı veya A/B ölçülmedi.

### En güçlü GPU seçeneği: dönüşümü son composite pass'e birleştirmek

Mevcut yol mantıksal boyutta özel bir imaj üretip bu imajı ek fullscreen draw ile döndürerek gerçek swapchain'e yazıyor. Megabonk'un mevcut son fullscreen/composite pass'i gerçek döndürülmüş swapchain hedefini doğrudan üretebilirse ek imaj okuma/yazma ve ek rotation pass kaldırılabilir.

Bu, yalnız preTransform değerini değiştirmekle olmaz. Swapchain'e giden image/view/framebuffer kimlikleri izlenmeli; ilgili render area, viewport/scissor ve output shader koordinatları birlikte dönüştürülmeli. FragCoord ve türevler gibi ekran koordinatına bağlı davranışlar korunmalı. Aynı shader'ın offscreen kullanımları etkilenmemeli; readback, transfer ve tanınmayan pass'ler mevcut güvenli yola dönmeli. İlk uygulama bu nedenle tek oyun ve tanımlı final-pass biçimi için opt-in olmalı. Mümkünse dönüşüm oyun motorunda yapılmalı; genel FEX katmanında bütün pipeline'ları dönüştürmek çok daha geniş iştir.

Normal Vulkan blit, eksenleri birbirine çeviren genel bir 90° rotation işlemi sunmuyor; copy/blit'e geçmek bu shader draw'unu otomatik ortadan kaldırmaz. QCOM uzantısı sürücü desteği olmadan taklit edilemez. Compositor rotation'a dönmek de ölçülmeden önerilmiyor.

**Önerilen sıra:** rotation ölçümü → fence hatalarını düzeltme ve gerekiyorsa timeline deneyi → GPU pass belirgin maliyetliyse Megabonk final-pass fusion deneyi. Mevcut 115 FPS oynanış örneğinden hedef FPS sözü çıkarılmamalı. Bu WSI düzeltmeleri X11/Proton yoluna ait; Megabonk native Android yolunun performansı ayrı doğrulanmalı.

Kaynaklar: [Android pre-rotation rehberi](https://developer.android.com/games/optimize/vulkan-prerotation), [QCOM render-pass transform kapsamı](https://docs.vulkan.org/refpages/latest/refpages/source/VK_QCOM_render_pass_transform.html), [Vulkan blit koordinat dönüşümü](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html).
