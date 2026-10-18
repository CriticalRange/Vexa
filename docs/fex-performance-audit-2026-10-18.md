# VEXA / FEX performans incelemesi — 18 Ekim 2026

Bu incelemede uygulama, FEX kaynakları, oyun ayarları veya telefondaki çalışma durumu değiştirilmedi; build/install/oyun başlatma yapılmadı. Yalnızca araştırma raporu ve telefondaki mevcut logların yerel kopyaları oluşturuldu.

**Sonuç:** ETS2'nin düşük FPS'ini yalnızca FEX JIT'e yüklemek için yeterli veri yok. En güçlü somut adaylar özel GL katmanındaki senkron tampon işlemleri, yazılım BC doku açma, yüksek çizim çağrısı sayısı ve shader/pipeline uyumluluk işlemleri. Bunların kodda varlığı doğrulandı; her birinin kaç ms kaybettirdiği henüz ayrı ölçülmüş değil. Bu çalışma kaynak ve mevcut kayıt incelemesidir, kontrollü performans benchmark'ı değildir; bütün olası nedenlerin tüketildiği iddiasını taşımaz.

## Kanıt ve ölçüm sınırları

- ETS2 native Linux x86-64 ELF: `app/src/main/java/com/critical/vexaemulator/guest/GuestProgram.kt:197`. Akış: oyun → FEX x86-64/ARM64 çevirisi → GL guest/host thunk → GLES 3.2 → Mali. Bu oturum Wine/Proton/DXVK kullanmıyor.
- Cihaz SoC bilgisi `MT6989`; oyun logunda Mali-G720-Immortalis MC12. GL sürümü olarak görülen 4.3.0 uyumluluk katmanının bildirdiği değerdir, eksiksiz native desktop GL 4.3 desteği anlamına gelmez.
- Telefonda `files/artifacts/ets2/fex_stderr.log` son değişiklik zamanı 2026-10-18 12:19. Alınan kopyalar: `logs/fex-performance-audit-20261018/`.
- Telefonda kurulu GL host/guest dosyalarının SHA256 değerleri mevcut VEXA assets dosyalarıyla aynı: host `f8b81f89d90105b9e667c9927093328483fef56ae9bf6b22a369edeb22c9a607`, guest `e812245cac00be0b9a5fcfbe7c061d7182dcb6f50817aa3ef0912312f8d600b1`. Bu, asset eşleşmesini kanıtlar; değiştirilmiş tüm FEX kaynaklarının bu binary'ye derlendiğinin tek başına kanıtı değildir.
- Log son bölümünde 25.21–43.85 ms ortalama kare aralıkları var: yaklaşık 39.7–22.8 FPS. Örnek ağır pencere: ortalama 43.85 ms, p99 82.04 ms; 256 örneğin 232'si 33 ms üstünde. Son pencere: ortalama 28.24 ms, p99 77.44 ms, maksimum 107.24 ms. Pencereler örtüşüyor; bunlar birleştirilmiş benchmark veya 1% low FPS değildir.
- Çizim sayıları ağır bölümde yaklaşık 590–1594/kare. Sahne, kamera, mod ve çözünürlük değişmiş olabilir. Logda oyun dünyası yükleniyor; tüm pencerelerin aynı oynanış sahnesine ait olduğu kanıtlanmadı.
- İnceleme anındaki runtime PID 935'in maps verisinde ETS2/FEX/GL host eşleşmesi bulunmadı; 118 MB civarı RSS ve 19 thread vardı. Dolayısıyla onun `/background`, CPU 0–3 durumunu ETS2 oynanışındaki CPU kısıtı olarak sunmuyorum. Aktif ETS2 CPU/GPU profili toplanmadı.

## Öncelikli bulgular

### 1. Tampon eşleme hızlı yolları genel olarak kapatılıyor — yüksek öncelik

`../FEX/ThunkLibs/libGL/libGL_Host.cpp:3685` içindeki 64-bit `glMapBufferRange` wrapper'ı `0x04 | 0x08 | 0x20` bitlerini kaldırıyor: INVALIDATE_RANGE, INVALIDATE_BUFFER ve UNSYNCHRONIZED. Oyun bu bayraklarla eski veriyi beklemeden yeni veri üretmek istese de sürücüye bu bilgi ulaşmıyor. GPU'nun kullandığı tampon yeniden eşlenirken bekleme ve gereksiz veri koruma/kopyalama oluşabilir.

**Etki alanı:** Bu bir ETS2 koşulu değil; aynı 64-bit GL wrapper'ını kullanan diğer oyunlara da uygulanıyor. Normal GL streaming, geometri ve dinamik tampon güncellemelerinde güçlü adaydır. Her map çağrısının mutlaka beklediği veya toplam kaybın belli bir yüzde olduğu söylenemez.

### 2. TBO işlemlerinde zorunlu CPU geri okuması — yüksek öncelik

`libGL_Host.cpp:3694` içindeki `glUnmapBuffer`, takip edilen texture buffer nesnesini yeniden READ olarak eşleyip ilk byte'ı CPU'dan okuyor. `glTexBuffer` (`:3715` civarı) ayrıca bağlanan tamponun tamamını READ map/unmap yapıyor. Bunlar tanılama bayrağından bağımsız uyumluluk işlemleri; `vexa_tbo_dump.flag` olmaması bunları kapatmıyor.

Kod açıklamasına göre ETS2'nin bozulmuş geometrisini önlemek için eklenmişler. CPU okuması tamamlanmış veri istediği için sürücüye GPU/CPU senkronizasyonu dayatabilir; 16 byte okunması beklenen GPU işinin küçük olduğu anlamına gelmez. Tam tampon map de tüm tamponun CPU tarafından kopyalandığı anlamına gelmez, fakat senkronizasyon riski taşır.

**Etki alanı:** TBO kullanan diğer GL oyunları. Tampon kayıt listesi ilk 16 nesneyle sınırlı; davranış ve kapsam ayrıca ölçülmeli. Görsel doğruluk sağlayan bu işlemleri doğrudan silmek önerilmiyor. Doğru alternatif tampon ömrü/fence/streaming stratejisiyle doğrulanmalı.

### 3. BC/DXT dokuları CPU'da açılıyor ve sıkıştırılmamış saklanıyor — yüksek öncelik

`libGL_Host.cpp:3990–4368`: BC1–BC7 format eşlemesi, `VexaBCDecode`, PBO READ map ve piksel upload yolları var. `:6081` sonrası proc routing bu wrapper'lara gidiyor. Son ETS2 logundaki `[VEXA][BCN] csub` satırları bu yolun kullanıldığını doğruluyor.

- Her ilgili upload'da CPU decode, geçici vektör ayırma/sıfırlama ve blok kopyalama.
- PBO kaynağında CPU READ map üzerinden ek bekleme olasılığı.
- BC1'in RGBA8'e açılması aynı büyük doku için yaklaşık **8 kat**, BC3/BC7'nin RGBA8'e açılması **4 kat** piksel depolama hacmi yaratır. Bu oran toplam oyun RAM'i veya FPS kaybı değildir.
- Sıkıştırılmamış doku örnekleme daha fazla bellek/bant genişliği baskısı yaratabilir; yeni alan yüklenirken decode ve upload kare takılmalarına katkı verebilir.

**Etki alanı:** BC texture kullanan bütün bu GL yolu. Kodun extension listesi civarında “Mali natively supports” diyen eski bir yorum ile yazılım fallback uygulaması çelişiyor. Performans sonucu yorumdan değil çalışan routing ve logdan çıkarıldı. Alternatif format/natif destek ayrıca cihazda doğrulanmalı.

### 4. Çok sayıda çizim ve x86/ARM thunk geçişi — yüksek öncelikli ölçüm adayı

Loglarda 600–1600 civarı çizim/kare görülüyor. Her çizime ek olarak state, uniform, buffer ve texture çağrıları var. Guest tarafındaki çağrı hazırlığı FEX üzerinden geçiyor; host sürücüye ulaşınca iş native. Bu nedenle “GPU sürücüsü de bütünüyle emüle ediliyor” doğru değil, fakat çok sayıda küçük çağrı CPU bütçesini tüketebilir.

`libGL_Host.cpp:3754` sonrası draw wrapper'ları ayrıca `VexaDiagDraw` ve `VexaPreDraw` çağırıyor. Sonuncusu pipeline durumunu cache'liyor; her çizimde yeniden shader derlediği iddia edilmemeli. `glMultiDrawArrays` fallback'i (`:4650` civarı) çizimleri host döngüsüne açıyor; bu, o API'yi kullanan oyunlar için ek maliyet adayı, ETS2'de yoğun kullanımı kanıtlanmadı.

### 5. Pipeline clone ve uniform çoğaltma — ilk kullanım takılması + devam eden CPU işi

`libGL_Host.cpp:3015–3196` uyumsuz pipeline için vertex shader kaynağını değiştirip compile/link yapıyor. Son logun ağır bölümünde de yeni `[VEXA][PIPE] ... clone=...` kayıtları var. Dolayısıyla bu çalışma yalnızca ilk açılışta bitmiyor.

Uniform çağrıları `:3257–3408` ve `:4400` civarındaki wrapper'larda orijinal programın clone listesine çoğaltılıyor. Clone sayısı arttığında bir guest güncellemesi birden fazla native GL güncellemesine dönüşebilir. Program-stage değişiminde cache geçersizleşiyor. Clone/source tabloları için burada karşılık gelen delete temizliği görülmedi; program yenileyen uzun oturumlarda büyüme/stale kayıt riski ayrıca denetlenmeli, mevcut oturumda bellek sızıntısı ölçülmedi.

**Etki alanı:** Özellikle ETS2'nin separate shader program yolu; aynı mekanizmaya giren diğer GL oyunları.

### 6. Çözünürlük menüsü gerçek GPU yükünün tamamını anlatmıyor

Son kaydedilmiş config `1280×720`, `r_scale_x/y=0.75`; ancak aynı oturum 2712×1220 ile açılıyor ve log sonunda 30 Hz → 144 Hz → 1280×720 değişimleri var. Host `glViewport` (`:2238`) küçük default-framebuffer viewport'unu Android yüzeyine büyütebiliyor. Offscreen FBO viewport'ları ise aynen geçiriliyor.

Bu nedenle 720p seçmek bütün pass'lerin 720p çalıştığını kanıtlamaz; ana FBO, aynalar ve son kompozit ayrı incelenmeli. Aynalar `scale=1`, view distance 80; trafik 1.0. Bunlar ek sahne çizimi/simülasyon yükü adaylarıdır, toplam payları ölçülmedi. Bu kayıtla “çözünürlük düşürmek işe yaramadı, dolayısıyla kesin CPU” sonucu çıkarılamaz.

### 7. Mevcut GL süre sayacı eksik — yanlış teşhis riski

`ScopedGLThunkTrace` (`:365`) yalnızca scope eklenmiş fonksiyonları ölçüyor. Draw/map/unmap/pipeline/uniform wrapper'larının önemli bölümü bu sayaçta yok; `eglSwapBuffers` da toplamına dahil değil. Dolayısıyla `glms/f=0.02` tüm driver+GPU maliyetinin 0.02 ms olduğunu göstermez. GPU timestamp ölçümü de değildir.

Scope bulunan yerlerde trace kapalıyken bile clock okuma, atomik sayaç ve progress işaretleme sürüyor. `:1975` civarında 600. swap'ta otomatik bir kare trace açılıyor; düşük sıklıklı frame özetleri, flag dosyası kontrolleri de var. Bunlar optimize edilebilir ek işlerdir, fakat son oturumda sürekli yoğun GL trace açık olduğunu gösteren kanıt yok. Cihazda gltrace/glverbose/TBO dump/JIT-name/Vulkan profiler bayrakları bulunmadı. Eski 50 bin satırlık `logs/ets2-fix/final.log`, güncel 972 satırlık kaydın yerine kullanılmamalı.

## Genel FEX ve Android maliyetleri

### 8. TSO ve hizasız atomikler — gerçek mimari maliyet, ETS2 payı bilinmiyor

Yerel `../FEX/FEXCore/Source/Interface/Config/Config.json.in:414` varsayılanları: TSO açık, VectorTSO ve MemcpySetTSO kapalı. VEXA `config.cpp:425` ile `StrictInProcessSplitLocks=1` uyguluyor. SIGBUS/unaligned atomic yolu `../FEX/FEXCore/Source/Utils/ArchHelpers/Arm64.cpp:1947` sonrası görülebilir.

TSO, x86 bellek sıralamasını ARM üzerinde korumak için ek maliyet getirir. Split atomikler sık oluşursa signal/backpatch ve süreç genelindeki kilit pahalı olabilir. Ancak ETS2 için signal sayımı veya hot instruction profili yok. “Bütün atomikler tek kilide giriyor” demek yanlış: bu özel hizasız/split yoldur. Unity sayaç doğruluğu için getirilen korumayı genel FPS ayarı olarak kapatmak uygun değildir.

Yerel HostFeatures.cpp:560 sonrası Cortex-X4 dahil LDAPUR/LRCPC2 performans erratum'u için workaround zaten bulunuyor. Bu yüzden “FEX'te bu düzeltme yok” da denemez; binary'nin etkin feature set'i ayrıca kaydedilmelidir.

FEX'in resmi açıklaması bellek modeli emülasyonunun önemini ve Vector/Memcpy TSO ayrımını doğruluyor: [FEX 2404](https://fex-emu.com/FEX-2404/). Oradaki genel benchmark oranları bu telefona/ETS2'ye kazanç tahmini olarak taşınmadı.

### 9. İş parçacığı, futex ve ana thread sınırı

FEX daha fazla çekirdeğe tek bir guest ana thread'ini otomatik dağıtmaz. Oyun mantığı/render hazırlığı tek thread'de yoğunlaşıyorsa toplam CPU kullanımının düşük olması CPU darboğazını dışlamaz. Çok fazla worker da futex, wakeup ve scheduler maliyeti yaratabilir.

Projenin önceki Megabonk çalışmasında steady menü CPU örneklerinin yaklaşık %18'i FEX futex yolundaydı (`docs/megabonk-performance-2026-10-14.md:53`). Bu ETS2 ölçümü veya %18 FPS kazancı değildir; genel oyunlarda araştırılması gereken somut tarihsel örnektir. Worker sayısını düşürmenin önceki deneylerinde tutarlı kazanç kanıtlanmamış.

### 10. Android CPU grubu / frekans / sıcaklık

`RootLogCollector.kt:108` root ile worker'ı top-app grubuna taşımayı deniyor; uygulama worker'ının foreground/background gruplarında büyük çekirdeklere erişimi kısıtlanabilir. İnceleme anındaki boş runtime CPU 0–3 ile sınırlıydı. Cihazdan okunan maksimum frekanslar CPU0 2.0 GHz, CPU4 2.85 GHz, CPU7 3.4 GHz. **Bu, ETS2 oynarken de aynı kısıtın olduğunu kanıtlamaz.** Oynanış anında ana thread affinity/cpuset/frekans kaydı gerekir.

İnceleme anında thermal status 0; kayıtlı ve anlık CPU/GPU sıcaklıkları yaklaşık 53–56°C. Aktif yük altında thermal throttling yoktur sonucu çıkarılamaz. Frekansların zaman içindeki değişimi ve sahneyle eşleştirilmiş termal kayıt gerekiyor.

### 11. JIT derleme ve kod invalidasyonu

Yerel default'lar: Multiblock=true, MaxInst=5000, SMC=mtrack, EnableCodeCachingWIP=false. Büyük yeni kod alanları ilk kez yürütülürken JIT derleme takılması olabilir; self-modifying/JIT kullanan Mono benzeri oyunlarda invalidasyon ayrı maliyettir. Mevcut ayar `full` SMC kontrolü değildir; her blokta tam byte kontrolü varmış gibi yorumlanmamalı. Kalıcı kod cache WIP kapalı olması runtime JIT cache yok demek değildir.

### 12. Build, log ve yardımcı kütüphaneler

FEX CMakeCache: RelWithDebInfo, `-O2 -g -DNDEBUG`, assertions açık, LTO kapalı, jemalloc kapalı. Bu **optimizasyonsuz -O0 build değildir**. LTO/assert/allocator farklılıkları ölçülebilir fakat onlarca FPS'in tek açıklaması oldukları gösterilmedi.

VEXA `SilentLog=0`; log handler hem Android loguna hem mutex altında dosyaya yazıyor (`runtime/init/logging.cpp:39`). Çağrı sıklığı yüksekse etkiler. Son ETS2 FEX logu yaklaşık 18 KB; mevcut kanıt log spam'ini başlıca sürekli darboğaz ilan etmiyor.

ETS2 `memcpy@GLIBC_2.14` için eski sembole yönlendirme preload'u kullanıyor (`guest-tests/x11-fake/libvexa_memcpy214.c`). Bu native ARM memcpy thunk'ı değil; gerçek guest implementasyonunun kopyalama hot spot'larına etkisi profillenmeli. Sembol adından tek başına yavaş olduğu çıkarılamaz. Ses/decompression/oyun motorunun thunk edilmeyen işleri de FEX üzerinden yürüyebilir.

### 13. Sunum, FPS limiti ve diğer backend'ler

Son ETS2 config'inde vsync=1 olsa da gerçek host logu `swap interval 0 -> ok`, oyun logu requested 1 için period 0 gösteriyor. Aktif limit 299; sabit 30 FPS sınırı doğrulanmadı. Inactive limit 30 olduğundan yanlış odak tespiti ayrı kontrol adayıdır; mevcut log düşük FPS'in bundan kaynaklandığını kanıtlamıyor.

ETS2 kaydı `headless=0 shadow=0` ve doğrudan EGL window surface. HeadlessRenderSurface'taki CPU_READ_OFTEN/ImageReader maliyeti bu oturuma mal edilemez; headless ve başka backend'lerde ayrı konu. Vulkan'daki FIFO/MAILBOX, acquire/fence beklemeleri ve GPU shader yükü de genel performans konusudur, ETS2 GL oturumunun nedeni diye sunulmamalı. Önceki Megabonk Vulkan araştırmasında shader/present değişimleri tutarlı FPS kazancı göstermemişti (`docs/megabonk-vulkan-investigation-2026-10-17.md`).

## Ölçüm önceliği

1. Aynı save/kamera ve sabit grafik ayarıyla aktif ETS2'nin gerçek CPU thread/cpuset/frekansını ve kare sürelerini birlikte kaydetmek.
2. Map/unmap/TBO readback çağrı adetleri ve toplam/dağılımlı sürelerini; eglSwapBuffers süresini; GPU pass timestamp'lerini ayrı ölçmek. GPU query sonucunu aynı karede zorla bekleyerek ölçümü bozmamak.
3. BC decode/upload süre ve byte miktarı, sıkıştırılmamış doku belleği, shader clone derlemeleri ve uniform çoğaltma sayısını çıkarmak.
4. JIT sembollü CPU örneklemesini sürücü/thunk/FEX syscall/guest oyun kodu olarak ayırmak; futex CPU örneğini doğrudan bekleme süresi saymamak.
5. Yalnız bundan sonra tek değişkenli karşılaştırmalar. TBO ve map düzeltmelerinde geometri/renk doğruluğunu; TSO/split-lock değişimlerinde thread doğruluğunu korumak.

Bu adımlar bu araştırmada çalıştırılmadı. Özellikle “şu değişiklik +20 FPS getirir” veya “GPU boş, kesin FEX yavaş” diyebilecek ölçüm yok. Buna karşılık ilk üç bulgu, mevcut kaynakta doğrudan görülen ve önce hedeflenmesi gereken somut performans maliyetleridir.
