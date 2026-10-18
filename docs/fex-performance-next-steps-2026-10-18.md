# VEXA performans incelemesi: optimizasyonlardan sonra kalan işler

18 Ekim 2026. Araştırma; uygulama/FEX kodu, ayarlar ve cihaz kütüphaneleri değiştirilmedi. Yeni oyun oturumu veya benchmark başlatılmadı. Sıralama ölçülmüş FPS kazancı değil, mevcut kodda görülen iş miktarı, kullanım kanıtı ve geliştirme maliyeti üzerinden önceliklendirmedir.

## Önceki değişikliklerin doğrulanması ve sınırları

- `libGL_Host.cpp:3696`: map access maskesi kaldırılmış. Unmap sonrası 16-byte ve TexBuffer sonrası tam tampon READ map kaldırılmış.
- 64-bit proc-resolution tanılama yönlendirmeleri azaltılmış. Ancak `libGL_interface.cpp` içinde bind/data/map/unmap için custom host tanımları ve doğrudan sembol yolları hâlâ var. “Bütün çağrılar her giriş yolunda wrappersız” demek doğru değil. i386 map/unmap wrapper'ları pointer genişliği için zaten gerekli.
- `libGL_Host.cpp:3205`: program 0 kısa yolu ve linked-program cache var; `:6565` relink'te cache kaydını siliyor.
- Uniform değişikliğinin açıklamasına düzeltme: önceki turda okunan sürümde de `if (!candidate) return false` vardı; o sürümde her uniform çağrısının iki `glGetProgramiv(0)` hatası ürettiği kanıtlanmıyor. Başka bir ara sürüm kastedilmiş olabilir. Yeni cache olumlu; “en etkili değişiklik” iddiası ölçülmüş değil.
- `libGL_Host.cpp:3988`: yalnız `0x83F0` opaque BC1 RGB565'e çevriliyor. RGBA8'e göre çıktı byte hacmi yarıya iniyor; toplam bellek trafiğinin veya FPS maliyetinin yarıya indiği sonucu çıkmaz.
- `:4038`: `std::vector<uint8_t>::resize()` büyüyen bölümü değer-ilklendirir. Thread-local reuse aynı/küçük boyutlarda faydalı; “hiç sıfırlama kalmadı” doğru değil. Ayrıca BC1 önce RGBA'ya decode ediliyor, ardından her piksel tekrar 565'e paketleniyor.
- BC1 uç renklerinin 565 olması, interpolasyonla üretilen ara renklerin 565'e kayıpsız sığdığı anlamına gelmez. RGB565 çıkış ikinci kuantizasyon yapar; bantlaşma/görsel kabul testi gerekir. Alpha/sRGB/normal veri formatlarına otomatik genellenmemeli.
- Host `ScopedGLThunkTrace` saatleri trace arkasına alınmış. Guest `ScopedGLGuestTrace` (`libGL_Guest.cpp:211`) hâlâ trace kontrolünden ÖNCE atomic fetch_add yapıyor. Host `ScopedGLCallTimer` (`:335`) da sync/read/flush yollarında koşulsuz zamanlama yapıyor.
- Base-instance sabit limit sorgusu cache'lenmiş; aşağıdaki çok daha büyük döngü maliyeti kalmış.

## Öncelik sırası

| Sıra | Geliştirme | Potansiyel / hedef | İş boyutu | Güven sınırı |
|---|---|---|---|---|
| 1 | Base-instance emülasyonunu tek host çağrısına taşı; VAO state cache ekle | Yüksek CPU kazancı adayı; ETS2 bitki/geometri | Orta | Kullanıldığı eski ETS2 logunda doğrulandı, süre payı bilinmiyor |
| 2 | Clone uniform güncellemelerini lazy/dirty yap; clone tekrarlarını birleştir | Yüksek CPU kazancı adayı; materyal/pipeline ağırlıklı sahneler | Orta–yüksek | Fan-out kodda kesin, etkin clone sayısı ölçülmeli |
| 3 | Multi-draw/multi-bind'i guest döngüsü yerine tek host batch yap | Yüksek potansiyel; ilgili API'yi yoğun kullanan oyunlar | Orta | ETS2'de çağrı sıklığı henüz bilinmiyor |
| 4 | Bağlam bazlı GL state cache: VAO, unpack, buffer/texture ve grass state | Orta–yüksek; tekrarlanan sorgu ve state değişimleri | Orta–yüksek | Doğru invalidasyon ve bütün giriş yollarının kapsamı şart |
| 5 | BC decode/upload hattı: SIMD, bounded cache, sonra offline ASTC/ETC | Streaming takılmaları ve bellek baskısında yüksek potansiyel | Aşamalı; orta–büyük | GPU/bellek sınırlıysa steady FPS'e etkisi büyür |
| 6 | Shader/clone içerik ve binary cache + kaynak ömrü yönetimi | Özellikle p99, ilk karşılaşma ve tekrar açılış | Orta–yüksek | Sürücünün mevcut cache'iyle ek kazanç ölçülmeli |
| 7 | X11 dosya polling yerine kalıcı fd/binary shared queue | CPU/syscall ve input gecikmesi | Küçük–orta, sonra orta | Poll frekansına bağlı |
| 8 | Debug callback ve kalan ölçüm işlerini gerçekten opt-in yap | Düşük–orta; hata fırtınasında daha büyük olabilir | Küçük | En kolay izolasyon, ana darboğaz olduğu kanıtlanmadı |
| 9 | i386 düşük adresli map staging havuzu | 32-bit streaming oyunlarında bellek kopyası/allocation yükü | Orta | ETS2 x64 için kazanç yok |
| 10 | Daha genel GL komut kuyruğu / başka backend | Teorik kazanç tavanı yüksek | Çok büyük | İlk tercih değil; profil sonrası mimari karar |

1–4 aynı maliyetlerin bir bölümünü hedefler; beklenen kazançlar toplanamaz. Aktif oyunda büyük çekirdek erişiminin kısıtlı olduğu ortaya çıkarsa Android scheduling düzeltmesi bu listenin önüne geçebilir. Şu anda yeni oynanış verisi olmadan bunu doğrulanmış neden saymıyorum.

## 1. En güçlü yeni sıcak nokta: guest base-instance döngüsü

`../FEX/ThunkLibs/libGL/libGL_Guest.cpp:575–638` her baseinstance!=0 çizimde:

1. Bütün attribute'larda enabled sorgular.
2. Enabled attribute için binding sorgular.
3. Her farklı binding için buffer/offset/stride/divisor sorgular.
4. Instanced binding offset'lerini değiştirir, draw yapar ve geri yükler.

Hepsi x86 guest kodunda; GL çağrıları host sınırını tekrar tekrar geçer. N attribute, E enabled attribute, B farklı binding, I instanced binding için yaklaşık **N+E+4B+2I+1** GL çağrısı vardır. Örnek N=16,E=8,B=8,I=4 ise **65 çağrı / tek çizim**. Bu bir örnek hesabıdır; oyunda ölçülmüş attribute sayısı değildir. MAX_VERTEX_ATTRIBS'i cache'lemek bu toplamdan yalnız bir çağrıyı çıkarır.

`logs/fex-performance-audit-20261018/ets2-stderr.log:718–725` nonzero baseinstance fallback kullanımını gösteriyor. Dolayısıyla bu yalnız teorik bir API değil; eski oturumda gerçekten kullanılan yol.

**Aşama A:** VEXA custom host fonksiyonu ekle; guest sadece bir kez pack/thunk yapsın. Döngü ARM tarafında çalışsın. Sürücü sorguları hâlâ var fakat emülasyon/geçiş çoğalması azalır.

**Aşama B:** Context+VAO bazlı binding/format/divisor/enabled cache. VertexAttribPointer/Format/Binding/Divisor, BindVertexBuffer(s), Enable/DisableVertexAttribArray, VAO bind/delete dahil değişikliklerden güncelle. Default VAO ve host iç uyarlamalarını da izle. Draw sırasında sadece ilgili instanced bindings değişsin.

**Aşama C:** Doğrulanmış native EXT desteği varsa doğrudan kullan; yalnız eglGetProcAddress'in non-null olması destek kanıtı değildir. Guest resolver `:750` civarı desktop ve EXT isimlerini zaten fallback'e yönlendiriyor; host'ta native fonksiyon aramak tek başına bu guest yolunu düzeltmez.

**Doğruluk açığı:** `offset + (baseinstance/divisor)*stride` divisor>1 için yanlış. Spec indeksi `floor(instance/divisor)+baseinstance`; başlangıç offset'i `baseinstance*stride` olmalı. Divisor=1 bunu gizler. Ayrıca binding offset'i GLint ile sorgulanıyor; 64-bit offset korunmalı. Optimizasyon sırasında bunlar için divisor 1/2/4, shared bindings, büyük offset, multiple VAO/context vakaları gerekir. [Khronos EXT_base_instance](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_base_instance.txt)

## 2. Uniform cache'in sonraki adımı: bütün clone'ları her seferinde güncelleme

`libGL_Host.cpp:3266–3316`, `:4410–4450`: uniform setter orijinali ve `g_vexa_clones_of_prog[program]` listesindeki bütün clone'ları geziyor. Birçok farklı pipeline aynı vertex programından clone ürettiyse çağrı büyütme devam eder.

Öneri: orijinal uniform değerlerini tip/count/transpose ve generation ile gölge state'te tut. Clone başına applied generation izle. Draw öncesi sadece bağlanan clone'un değişen uniform'larını uygula. İlk clone oluşturma ve ilk kullanımı da değerleri almalı. GetUniform sorguları orijinal programın doğru durumunu görmeli; aynı değer filtresi bit düzeyinde ve array alt aralıklarıyla doğru çalışmalı.

Clone cache anahtarı yalnız program ID değil: kaynak/link generation, fragment interface, compile özellikleri ve driver kimliği. Aynı dönüştürülmüş vertex source/interface birden fazla pipeline'da tekrar derlenmemeli. Relink, delete, ID yeniden kullanımı ve share-group ömrü ele alınmalı. Mevcut clone tabloları için tamamlayıcı delete temizliği görülmedi; büyüme riski var, ölçülmüş leak yok.

## 3. Multi-draw hâlâ CPU guest döngüsü

`libGL_Guest.cpp:1601–1661`: indirect readback kaldırılmış, iyi. Fakat “GPU-side loop” yorumu yanıltıcı: C++ döngüsü guest CPU'da N ayrı DrawIndirect çağrısı yapıyor. Komut verisi GPU'da kalıyor; dispatch döngüsü GPU'ya taşınmış değil.

Önce tek custom host batch; host'ta desteklenen gerçek EXT multi-draw, yoksa native ARM döngüsü. Multi-bind için de tek sınır geçişi. 32-bit pointer/offset dizileri marshal edilmeli. `glBindTextures` mevcut fallback'i zero texture'ları atlıyor, sadece 2D bağlıyor ve active texture'ı geri yüklemiyor; standart semantik tamamlanmadan genişletmek yanlış görüntüyü yaygınlaştırır.

Indirect command içindeki nonzero baseInstance desteklenmiyorsa düz GLES DrawIndirect yeterli değildir. EXT capability ve doğru fallback şart; yalnız CPU okumayı kaldırmak semantik doğruluğu kanıtlamaz. [Khronos EXT_multi_draw_indirect](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_multi_draw_indirect.txt)

## 4. Genişletilebilir temel: context/share-group state modeli

Mevcut `g_vexa_stage_program`, current program/pipeline, unpack ve diğer bazı tablolar global. Cache'leri artırmadan bağlam ömrü ayrılmalı: current context pointer thread-local, VAO/current binding context'e ait, program/texture kaynakları share-group'a ait. ID yeniden kullanımı için generation gerekli.

`VexaUnpackScope` (`Host:4122`) her upload'da yedi GetIntegerv, altı state sıfırlama ve altı restore yapıyor; decode/fetch/source yollarında da ek sorgular var. Diagnostic sorguların kalkması bunları kaldırmamış. PixelStore ve PBO binding state'i doğru tutulursa tekrarlı sorgular ve değişmeyen state çağrıları azaltılabilir. Host'un yaptığı geçici değişiklikler cache'i bozmamalı.

`vertex_readonly_buffer.inc:51` grass yolunda her draw'da binding/offset/active texture/max unit sorgusu, texture bind, TexBuffer, iki uniform ve restore var. Program/SSBO binding/offset değişmediğinde bunlar atlanabilir. Bu Unity tarzı uyarlamaya özgüdür; ETS2 vegetation yoluyla aynı şey değildir.

## 5. BC decode'dan sürdürülebilir texture sistemine

Yakın vadede:

- Tekrar kullanılan tampon için initialized logical size ile capacity ayrımı; bütün çıktı yazılacaksa gereksiz büyüme sıfırlamasını önle. Thread-local tamponların her wrapper'da ayrı oluşu en büyük eski allocation'ı tutabilir; bütçeli scratch arena düşün.
- Scalar bcdec blok döngüsünü native ARM SIMD veya blok-paralel decode ile karşılaştır. Küçük mip'lerde worker/queue maliyeti kazancı yiyebilir.
- GPU'ya upload render thread'inde kalırken uygun kaynak kopyası ve sıralama ile CPU decode worker'a taşınabilir. Oyun texture'ı hemen kullanıyorsa bekleme yine gereklidir.
- Content hash+format+boyut+layout+decoder version anahtarlı bounded cache. Subimage ve atlas güncellemeleriyle invalidasyon; cache'in RAM'i patlatmaması gerekir. Disk cache decode'u azaltır, RGBA GPU footprint'ini azaltmaz.

Uzun vadede, statik dokular için offline/ön hazırlık BC→ASTC/ETC2 dönüşümü ve cache. Eski cihaz extension logunda ASTC LDR/HDR var; güncel native context'te doğrulanmalı. ASTC 4×4, 16-byte/16-pixel yani 1 byte/pixel; RGBA8'in dörtte biri ve RGB565'in yarısı teorik blok hacmi. Metadata, mips, fiziksel layout ve diğer kaynaklar ayrıca var. ASTC 4×4 orijinal BC1'den küçük değildir. Alpha, normal map, sRGB ve loss toleransı ayrı politikalar ister.

ASTC kodlamasını render thread'inde her upload'a koymak ters etki yaratabilir. GPU compute BC decode ise CPU'yu boşaltabilir ama RGBA çıktıyla VRAM/bandwidth sorununu çözmez; GPU zaten sınırlıysa kötüleştirebilir. [Arm ASTC encoder](https://github.com/ARM-software/astc-encoder), [Arm texture compression](https://www.arm.com/technologies/graphics-technologies/adaptive-scalable-texture-compression)

## 6. Shader cache ve ömür yönetimi

Mevcut translated source cache ve program kaynak snapshot'ları genişletilebilir. Stripped vertex clone compile/link işi ilk draw yolunda yapılıyor; pipeline stage değişiminde validity kaydı siliniyor. Aynı kaynak/interface için tekrarları birleştir, gerekirse bilinen pipeline'ları yükleme sırasında hazırla. Program binary cache driver/version/capability/translator sürümüyle anahtarlansın; binary restore başarısızsa yeniden compile fallback'i olsun. glGetProgramBinary wrapper'ının varlığı uygulama seviyesinde böyle bir cache kurulduğu anlamına gelmez. Kazanç öncelikle ilk kullanım takılmalarıdır, her kare FPS artışı beklenmemeli.

## 7–9. Diğer somut sıcak yollar

- **Input:** `guest-tests/x11-fake/libX11_fake.c:905` open + iki lseek + read + close; `:1258` sonrası Pending/Next/Peek/Check/Flush girişlerinden tekrar çağrılıyor. Kalıcı fd+toplu okuma ilk adım; uzun vadede paylaşılan binary ring ve eventfd/pipe wake. ABI, reconnect, truncate ve event sırası korunmalı. Frame başına kör bir cache input gecikmesi yaratmamalı.
- **Debug:** Host `:1654` VexaInstallGLDebugOnce, DEBUG_OUTPUT ve DEBUG_OUTPUT_SYNCHRONOUS'ı koşulsuz açıyor. Callback'in ilk 80 mesajda logu kesmesi sürücünün mesaj üretmesini durdurmaz. Üretim koşusunda opt-in ve driver-side message filtering adayı. Synchronous callback, GPU'nun her çağrıda glFinish yaptığı anlamına gelmez. [KHR_debug](https://registry.khronos.org/OpenGL/extensions/KHR/KHR_debug.txt)
- **Kalan sayaçlar:** guest trace atomic'si, ayrı ScopedGLCallTimer, BCLogOnce'ın bütçe bittikten sonra da fetch_add'i, swap flag access'leri ve otomatik 600. kare trace'i. Küçük düzeltmeler; tek başlarına büyük FPS vaat edilmez.
- **i386:** `android_gl_guest32.h` her map'te GuestMalloc, READ'de host→guest, WRITE unmap'te guest→host memcpy ve Free yapıyor; mutex'li map tablosu var. Düşük adresli bounded staging havuzu ve explicit flush/dirtied-range takibi düşünülebilir. Yüksek host pointer'ını 32 bit'e kesmek çözüm değildir. Normal write map'te uygulamanın değiştirdiği byte'lar bilinmeden partial-copy varsayılamaz.

## 10. Büyük mimari seçenek: komutları toplu thunk etmek

Asenkron, sonuç döndürmeyen GL setter/draw komutları guest'te paketlenip host'ta toplu yürütülebilir; her küçük çağrının FEX sınır geçişi azalır. Ancak pointer verileri enqueue anında doğru kopyalanmalı; map/query/readback/sync/error/context-switch noktalarında flush/barrier gerekir. Shared context sırası, callbacks ve kaynak silme de çözülmeli. Önce mevcut multi-call fallback'lerini tek host çağrısına taşımak daha küçük ve ölçülebilir bir pilot olur. Genel command queue'nun maliyeti ancak bunun sonuçlarıyla gerekçelendirilmeli.

Zink/Vulkan backend ya da başka bir GLES çeviri katmanı ayrı mimari araştırmadır. Mevcut Mali driver özellikleri, BC fallback, vertex storage ve Android WSI uyumu kanıtlanmadan “daha hızlı olur” denemez. Mevcut uygulamayı başka katmanla değiştirmek bu bulguların hazır çözümü değildir.

## Ölçüm tasarımı

“Frame wall time - host süreleri = guest CPU süresi” yanlış olabilir: farklı thread'ler, iç içe çağrılar, scheduler ve GPU beklemeleri var. CPU sampling'i FEX JIT/guest, thunk, driver, syscall sınıflarına ayır; seçilmiş API family'lerinde thread-local exclusive wall time ölç; swap/wait'leri ayrıca raporla. GPU pass zamanını gecikmeli timer query ile ölç, availability/disjoint kontrol et. Query sonucunu aynı karede bekleyip pipeline'ı durdurma.

Trace modu açılması VexaPostDrawDiag içindeki draw sonrası glGetError'ı da etkinleştiriyor; bu yüzden üretim maliyetini ölçmek için bütün verbose trace'i açmak uygun değil. Ayrı düşük maliyetli profiling modu, thread-local sayaçlar ve örnekleme gerekir.

Temel metrikler: base-instance sayısı ve attribute/binding sayısı; thunk sınır geçişleri; multi-draw batch boyları; uniform başına clone fan-out; BC byte/decode/upload süreleri; shader compile sayısı; input poll/syscall sayısı; CPU çekirdek/frekans, GPU süreleri, sıcaklık ve frame p50/p95/p99. Aynı sahnede ısınma sonrası tekrarlı karşılaştırma; her aşamada ağaçlar, renkler ve sahnedeki çizim sayısı da korunmalı. Kaybolan geometri FPS kazancı sayılmamalı.

## Paketleme bulgusu

Bu turda okunan cihaz hash'leri:

| Dosya | Cihaz SHA256 |
|---|---|
| host/libGL-host.so | e6d11da2271ebab5d9390b481e3edf3cb4cc8ca68d9f4de3036e7e45233e530c |
| guest/libGL-guest.so | 468cb6c4e4a966a26afeb726b89469bd3858698716c74bf3d7a1ca6707eeb066 |
| host_32/libGL-host.so | f9425cb46201a0d0b10fbbfb957d4c7733e7006ed4d73bc779a6869ab3d3878b |
| guest_32/libGL-guest.so | 711c615fc9d0f624a76be17090dfdefe93be1ba8f70b3916de402af5b7d5be88 |

Dördü de VEXA `app/src/main/assets/runtime/thunks` karşılıklarından farklı. Bu, elle cihaz staging'iyle tutarlı olabilir; yüklenmiş prosesin bunları kullandığını veya APK'ye paketlendiklerini kanıtlamaz. GuestInstaller.kt:153–166 içerik stamp'i değiştiğinde assets'i tekrar kopyalıyor. Sonraki dağıtımda doğru dört dosya ve STAMP birlikte doğrulanmalı. Bu incelemede paketleme değiştirilmedi.
