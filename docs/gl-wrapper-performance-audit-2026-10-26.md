# GL wrapper karşılaştırması — 2026-10-26

## Sonuç ve sınır

GL thunk için uygulanabilir, önemli performans adayları var. En güçlü bulgu başka bir kütüphanenin bütünüyle taşınması değil: mevcut thunk'ın draw/upload başına yaptığı gereksiz işleri kaldırmak. Aşağıdaki kaynak davranışları doğrulandı; cihazda bu değişikliklerin FPS kazancı ölçülmedi. FEX/GL koduna, APK'ya veya çalışan oyuna bu araştırmada müdahale edilmedi.

İlk uygulama sırası: **upload telemetrisini gerçekten opt-in yapmak → modern draw için ucuz uyumluluk fast path'i ve state cache → değişmeyen uniform'ları göndermemek → shader özellik varyantları → gerektiği kanıtlanan buffer streaming/readback işleri**.

## Documents altına indirilen kaynaklar

| Proje | Yerel konum | İncelenen commit | Kapsam |
|---|---|---|---|
| gl4es | `C:/Users/zeytu/Documents/gl4es` | `2037865590d410bcd7b8d1f22b841472ed6d39ac` | OpenGL→GLES, FPE, uniform/value cache, buffer shadow, renderlist birleştirme |
| glshim | `C:/Users/zeytu/Documents/glshim` | `615dbe9ca2be8053cfa48f2c42bb96c64cf6512d` | Eski GL compatibility, immediate/display-list render block'ları |
| ANGLE | `C:/Users/zeytu/Documents/angle` | `9addc2983b90152f628cfdaea194983b46b254bd` | GLES backend state yönetimi; GL/Vulkan buffer ve vertex streaming |

Üçü de shallow Git checkout. gl4es `src/` + `include/`, ANGLE `src/libANGLE/` + `src/common/` için sparse checkout kullanıyor; kök belgeler de mevcut. Büyük medya/geçmiş/diğer bağımlılıklar indirilmedi. glshim normal shallow checkout. Bunlar derlenmedi. glshim'in son commit'i 2018; modern GL performans önerilerinde asıl referans olarak alınmadı. gl4es'in katkı yönergeleri okundu; bu çalışma upstream'e katkı hazırlamıyor, kaynakları karşılaştırıyor. Eski `p3/regal` adresi repository-not-found döndürdüğü için Regal incelemeye dahil edilmedi.

Referans permalink'leri:

- [gl4es kaynak anlık görüntüsü](https://github.com/ptitSeb/gl4es/tree/2037865590d410bcd7b8d1f22b841472ed6d39ac)
- [glshim kaynak anlık görüntüsü](https://github.com/lunixbochs/glshim/tree/615dbe9ca2be8053cfa48f2c42bb96c64cf6512d)
- [ANGLE kaynak anlık görüntüsü](https://github.com/google/angle/tree/9addc2983b90152f628cfdaea194983b46b254bd)

## 1. Sürekli çalışan upload telemetrisini kaldırmak — ilk ve en dar iş

**Bizde:** `FEX/ThunkLibs/libGL/libGL_Host.cpp:4026`, `fexfn_impl_libGL_glBufferSubData`.

`VexaOrphanExperiment=false` olsa bile her çağrı iki clock okuması, `glGetBufferParameteri64v(GL_BUFFER_SIZE)`, thread-local örnek vektöründe `find_if` ve sayaç güncellemesi yapıyor. `VexaUploadReport` (satır 197) swap yolundaki rapor penceresinde örnekleri topluyor, sıralıyor, stderr'e yazıyor ve deney bayrağını yokluyor. Mevcut raporlamanın maliyeti profil açık/kapalı ayrımına bağlı değil. Driver state sorgusu genellikle GPU completion beklemek anlamına gelmez; yine de burada gereksiz host/driver işi olduğu kaynakta kesin.

**Karşılaştırma:** gl4es `src/gl/buffers.c:278` buffer boyutunu kendi nesnesinden doğruluyor; normal native VBO alt yolunda doğrudan `gles_glBufferSubData` çağırıyor. ANGLE `src/libANGLE/renderer/gl/BufferGL.cpp:137` de cached binding ve mevcut shadow gereksinimi dışında basit upload yoluna sahip.

**Öneri:** profiling ve orphan deneyini ayrı opt-in ayarlara ayırmak. İkisi de kapalıyken doğrudan normal upload; kapasiteyi sadece gerçekten gerektiğinde sorgulamak veya doğrulanmış buffer metadata'sından okumak. Telemetri örnek tablosu büyümesini sınırlamak. Deney açıldığında mevcut tam-buffer koşulu ve immutable/mapped kontrolleri korunmalı.

**Kazanç alanı:** küçük ve sık buffer update yapan oyunlarda CPU/driver maliyeti. Bu değişiklik, aynı buffer alanını GPU kullanırken güncellemenin gerçek implicit-sync stall'ını tek başına kaldırmaz. Eski Portal ölçümündeki tüm upload süresini bu muhasebeye atfetmek yanlış olur.

## 2. Modern draw'u legacy hazırlığından çıkarmak ve state query'lerini azaltmak — geniş etki adayı

**Bizde:** `legacy_client_runtime.inc:107` içindeki scope önce `VexaLegacyVAO()` ile `glGetIntegerv(GL_VERTEX_ARRAY_BINDING)` yapıyor, sonra `VexaPrepareBGRAVertexInputs()` çağırıyor. `!client.Used` kontrolü bundan sonra geliyor. BGRA hazırlığı (satır 28) share-group mutex'ini alıyor, generation kontrol ediyor ve `GL_CURRENT_PROGRAM`/gerekirse pipeline stage sorguluyor. İlk görülen programda attribute/uniform reflection yapıyor. Bu ön işlemler legacy client array kullanmayan modern draw'ların da yolunda.

`VexaPreDraw` ayrıca share-group kilidine giriyor. `VexaApplyBaseInstance` (`libGL_Host.cpp:5011`), baseinstance sıfır değilken enabled/binding/divisor/buffer/stride/offset için attribute ve indexed state sorgularını her draw'da yineliyor. Mevcut optimizasyon bu sorguları host'a taşımış; sorguları ortadan kaldırmamış.

`VexaUnpackScope` (satır 4485) dönüştürülmüş texture upload'larında yedi driver query'si ve altı pixel-store reset/restore çifti yapıyor; mevcut değer aynı olsa bile ayarlıyor. Bu bütün texture upload'larına değil bu scope'u kullanan dönüşüm yollarına ait.

**Karşılaştırma:** ANGLE `StateManagerGL.cpp:1869/1905` program, VAO ve buffer binding değişmediyse driver çağrısını atlıyor. `:2034` pixel-unpack state'i karşılaştırıyor ve sadece değişen alanları gönderiyor; EBO binding'inin VAO'ya ait olduğunu ayrıca izliyor.

**Öneri:** önce ihtiyaç duymayan draw için küçük bir fast path. Sonra context-local program/VAO/buffer/pixel-store/vertex-binding state takibi; native helper draw'ları da aynı state manager üzerinden geçmeli veya cache'i açıkça invalidate etmeli. BGRA maskesi önce açıkken sıfırlanması gereken programlar atlanmamalı. Delete, object-name reuse, context switch/share-group lifetime, DSA/multi-bind/ARB alias ve ham generated entry point'ler kapsanmadan genel cache açılmamalı. Hatalı API çağrısında cache geçerli native state'in önüne geçmemeli.

**Kazanç alanı:** draw sayısı yüksek GL oyunları; instanced/base-instance kullanan ETS2 yolu; streaming texture dönüşümleri. Beklenen etki CPU/driver tarafında, FPS yüzdesi henüz bilinmiyor.

## 3. Uniform value cache ve sadece kullanılacak clone'a lazy senkronizasyon — somut tekrarlar var

**Bizde:** `VexaApplyAlphaTest` (`alpha_test.inc:162`) uygulandığında aynı function/reference için her draw'da iki program-uniform yazıyor. `VexaApplyClipPlanes` (`legacy_clip.inc:58`) projection inverse ve plane dönüşümlerini tekrar hesaplıyor; ilgili location'lar varsa değerler değişmese de tekrar gönderiyor. Bu iki yol sırasıyla `VEXA_GL_ALPHA_TEST=1` ve `VEXA_GL_CLIP_PLANES=1` ile açılıyor; bütün oyunlarda açık değiller.

`VexaAssemblyDrawScope` (`arb_assembly_runtime.inc:37`) ARB assembly çiftini bind ettiği her draw'da aktif sampler ve local/env uniform dizilerini tekrar yüklüyor. `VexaMirrorUniform` (`libGL_Host.cpp:3643`) ve scalar/vector/matrix wrapper'ları orijinal programın bütün clone'larına anında fan-out yapıyor; o draw'da yalnız bir clone kullanılacak olsa da.

**Karşılaştırma:** gl4es `uniform.c:163/189` ve integer/matrix eşleri cached byte değerleri aynıysa driver uniform çağrısından önce dönüyor. `fpe.c:614` parent/child senkronizasyonu bu helper'ları kullanıyor; kendi TODO'su bulunduğundan bütün döngünün sıfır maliyetli olduğu söylenmiyor. ANGLE Vulkan `ProgramExecutableVk.cpp` default-uniform dirty bit'leriyle yalnız değişen shader bloklarını güncelliyor; bu doğrudan GLES kodu olarak kopyalanacak bir fonksiyon değil, tasarım örneği.

**Öneri:** program executable generation + uniform location/type/range için value cache; alpha/clip matrisi için revision; sampler değerlerini link sonrası ilk kullanımda kurma. Clone'larda last-applied revision tutup bağlanacak clone'a draw öncesi lazy flush. Sadece global dirty bit yetmez: her clone/linked program kendi son uygulanan durumunu izlemeli. Uniform setter'ın guest error/validation davranışı korunmalı. ARB bound object değiştiğinde local parameter'lar değişmiş sayılmalı; başka context aynı shared program uniform'ını yazabiliyorsa revision da paylaşılmalı. `glGetUniform*` görünürlüğü unutulmamalı.

**Kazanç alanı:** Portal/Super Meat Boy benzeri compatibility yolları ve çok clone'lu pipeline oyunları. Shared-program kilidinde geçirilen süreyi de azaltma potansiyeli var.

## 4. Alpha/clip/BGRA shader özelliklerini ihtiyaç duyulmayan varyanttan çıkarmak — GPU tarafındaki en güçlü şüphe

**Bizde:** clip desteği opt-in açıldığında `VexaClipInstrument` (`legacy_clip.inc:26`) bütün uygun vertex/fragment kaynaklarına altı clip-distance varying'i ve runtime enable dalları/discard ekliyor. Fragment kaynağındaki explicit `early_fragment_tests` satırını kaldırıyor (satır 50). O draw'da hiçbir clip plane etkin değilken de instrument edilmiş shader kullanılıyor. Bunun native raster clipping davranışını taklit etme nedeni var; açık plane varken doğrudan kaldırılması doğru değil.

`VexaBGRAInstrument` (`legacy_client_runtime.inc:5`) desteklediği vec4 vertex input'ları için `.bgra` seçimini uniform koşuluna bağlıyor. `libGL_Host.cpp:5822` normal vertex shader source yolunda bunu çağırıyor; BGRA kullanmayan program/VAO için de bu ek kaynak oluşabiliyor. Mevcut GPU BGRA yolu önceki CPU readback'e göre yararlı; bu yol geri alınmamalı.

**Karşılaştırma:** gl4es `fpe_shader.c:1726` custom fragment shader'da plane mask ve alpha enable durumuna göre yalnız gereken ek kodu oluşturuyor; `fpe_cache.c:86` state anahtarına göre varyantı saklıyor.

**Öneri:** en azından düz varyant / compatibility varyantı ayrımı; clip mask, alpha enable/function ve gerçekten gereken BGRA attribute maskesi için kontrollü specialization. Önce clip kapalı varyantta altı varying, discard dalları ve gereksiz matris işini kaldırmayı denemek. Özgün shader'ın kendi discard'ı/side effect'i varsa early-Z garantisi verilemez. Uniform dalının mevcut driver tarafından optimize edilebilmesi de mümkün; FPS etkisi GPU ölçümüyle doğrulanmalı. Shader varyantları attribute/uniform reflection, separable pipeline eşleşmesi, relink ve program binary cache anahtarlarıyla birlikte yönetilmeli; varyant sayısı sınırlandırılmalı.

**Kazanç alanı:** compatibility özelliği açıkken fill-rate/overdraw yükü yüksek sahneler. Yanlış precision düşürme veya clip/alpha davranışını kapatma önerilmiyor.

## 5. Kalan readback'ler için selective CPU shadow + index range cache — koşullu büyük kazanç

**Bizde:** `VexaLegacyArrayScope::ReadBuffer` (`legacy_client_runtime.inc:95`) conversion fallback gerektiğinde EBO/vertex buffer'ı `glMapBufferRange(...GL_MAP_READ_BIT)` ile CPU'ya alıyor. İndekslerin maksimumunu tekrar tarıyor; geçici CPU vector'ları ve staging buffer upload'ları kullanılıyor. Ancak BGRA GPU fast path'i zaten mevcut; eski "her BGRA draw geri okunuyor" teşhisi artık genel olarak doğru değil. Kalan GL_DOUBLE, client-array ve unsupported BGRA/assembly birleşimleri ayrı sayılmalı.

**Karşılaştırma:** gl4es `buffers.c:166/278` CPU shadow tutup normal upload'larla güncelliyor. ANGLE GL `BufferGL.cpp:84/137/174` yalnız gerektiğinde shadow tutuyor; `Buffer.cpp:431` index range cache kullanıp data/subdata/copy/map değişimlerinde uygun aralığı invalidate ediyor. ANGLE `VertexArrayGL.cpp:232` index range işini sadece attribute streaming gerekiyorsa yapıyor.

**Öneri:** sadece CPU tarafında üretilmiş ve conversion yolunda okunan buffer/range'ler için shadow ve write-generation. Indeks aralıkları buffer generation/type/offset/count/primitive-restart anahtarıyla cache'lenmeli. Converted vertex staging'i aynı veri+format+stride generation için yeniden kullanmak. Büyük `last` indeksi nedeniyle sıfırdan o indekse kadar dönüştürme yerine güvenli vertex-window staging düşünmek; baseVertex, restart ve instancing hesaba katılmalı.

**Sınır:** FEX GL4.x yolu SSBO/transform feedback/copy/persistent mapping ile GPU tarafından yazılan buffer'lara sahip olabilir. gl4es'in GL2.x CPU-shadow modelini herkese uygulamak doğru veri üretmez. GPU write/copy ve mapped/coherent erişimlerinin invalidation'ı kapsanmıyorsa native readback fallback zorunlu. Bütün buffer'ları shadow'lamak da mevcut bellek baskısını büyütebilir.

## 6. Gerçek upload stall'ı sürerse buffer renaming/ring — en çok mühendislik isteyen aday

ANGLE Vulkan `BufferVk.cpp:743/864` kullanım takibi, yeni allocation, eski verinin korunması ve retirement ile busy buffer'ı ghost edebiliyor. GLES backend `BufferGL::setSubData` ise normal driver upload'ını kullanıyor. Dolayısıyla "ANGLE/gl4es'in ufak SubData fix'ini alıp bütün Mali stall'larını kaldırırız" sonucu çıkmıyor.

Bizdeki orphan deneyi sadece `offset=0 && size=capacity` halinde çalışıyor. Küçük kısmi güncellemeler için bunun kapsamını körlemesine genişletmek buffer'ın değişmeyen alanlarını kaybettirir.

Mantıksal buffer→birden fazla physical backing, fence ile güvenli slot reuse, VAO/index/UBO/TBO/SSBO/indirect binding güncellemeleri ve read/write hazard takibi gerekir. Güncellemeleri draw sınırlarının ötesine birleştirmek, ara draw'un görmesi gereken veriyi değiştirebilir. Tamamen CPU-written belirli streaming VBO/EBO sınıfında opt-in prototype düşünülebilir. Önce 1. maddedeki profiler maliyeti ayrıldıktan sonra native driver stall'ının gerçekten kaldığı gösterilmeli.

## Yeniden önerilmeyen, bizde zaten olanlar

- Guest immediate-mode/display-list batching: `android_fixed_function.h:3`, `libGL_Guest.cpp:1519`. glshim/gl4es renderlist fikrinin temel karşılığı var; "her vertex ayrı thunk geçiyor" varsayımı yanlış.
- Host base-instance emülasyonu ve tek geçişte multi-draw/multi-bind var. Eksik olan bazı state query'lerinin kaldırılması.
- Clone program binary / disk cache var. gl4es PSA bulgusunu sıfırdan cache yokmuş gibi sunmak doğru değil. Derleme hitch'i ile steady-state frame süresi ayrı ölçülmeli.
- GPU BGRA dönüşümü var; CPU swizzle/readback'e topluca dönüş önerilmiyor.
- Alpha test, clip ve ARB assembly emülasyonu zaten var; öneri özellikleri yeniden yazmak değil, kullanım başına maliyetlerini azaltmak.

## Uygulama ve kabul ölçütü

1. Her adımı ayrı A/B değişikliği yap. Aynı sahne/resolution/API, eşleşen host/guest ve kontrollü termal koşullar kullan. Native Vulkan Megabonk koşusu GL thunk değişikliğini test etmez.
2. Draw başına host query / uniform write / mutex giriş sayısı; upload başına query/clock/sample-table maliyeti; mapped readback byte ve sürelerini say. Profiler'ın kapalı koşusunu da ölç.
3. Gerçek frame p50/p95/p99, guest↔host geçiş sayısı, CPU/driver zamanı ve mümkünse GPU zamanı raporla. Sadece glms/f veya menü FPS'inden oyun geneli kazanım çıkarma.
4. State cache için farklı VAO/EBO, shared context, program relink/delete/reuse, internal helper sonrası restore; lazy uniform için iki clone arasında değer geçişleri; shadow için GPU writes ve map/copy invalidation testleri gerekli.

İlk iki adım doğrulanmış gereksiz işlere odaklanıyor. Shader varyantları GPU tarafında daha büyük potansiyel taşıyor ancak doğruluk ve varyant-cache işi daha geniş. Renaming en sona bırakılmalı. Bu araştırmada hiçbir FPS artışı vaat edilmiyor.
