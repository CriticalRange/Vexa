# GL hot-path düzeltmeleri — 2026-10-26

## Güncel durum kontrolü

İnceleme başlangıcında kullanıcının GL BCn GPU decode çalışması, Vulkan SPIR-V/feature değişiklikleri, runtime launch değişiklikleri ve runtime asset'leri zaten dirty idi. GL'de önerilen sürekli upload telemetrisi, modern draw erken çıkışı ve compatibility uniform tekrarlarını azaltma işleri henüz yapılmamıştı.

Bu çalışma doğrulanmış tekrarların kaldırılmasını uyguladı. Genel shadow-state manager, shader variant altyapısı veya buffer backing renaming'i uygulamadı. Aşağıdaki durum tablosu önerilerin tamamının bitirildiği anlamına gelmez.

| Önceki öneri | Bu çalışmadaki durum |
|---|---|
| Upload telemetrisini opt-in yapmak | Uygulandı; normal upload'da kapasite query/clock/sample araması yok |
| Modern draw için ucuz compatibility fast path | Uygulandı; hiç BGRA kullanılmamış share group'ta legacy hazırlığı atlanıyor; normal pipeline'sız, alpha/clip'siz pre-draw kilidi atlanıyor |
| Genel VAO/buffer/base-instance/pixel-store state cache | Genel cache yapılmadı; internal/raw GL yolları henüz tümüyle izlenmiyor. Texture dönüşüm scope'undaki aynı-değer pixel-store yazıları kaldırıldı; gerçek state query'leri korunuyor |
| Değişmeyen alpha/clip/ARB uniform değerleri | Uygulandı; clip inverse/MVP hesapları da içerik değişmedikçe tekrar yapılmıyor |
| Clone'lara lazy uniform fan-out | Bu değişiklikte uygulanmadı; mevcut eager mirror yolu korunuyor |
| Alpha/clip/BGRA shader varyantları | Uygulanmadı; mevcut görünür shader davranışı korunuyor |
| Selective vertex/index CPU shadow ve index-range cache | Uygulanmadı; GPU yazmaları ve map/copy invalidation kapsamı kurulmadan readback kaldırılmadı. Kullanıcının yeni BCn texture decode yolu ayrı bir özellik ve korundu |
| Buffer backing renaming/ring | Uygulanmadı; küçük partial write'larda orphan yapılmıyor |

## Uygulananlar

### Upload

`FEX/ThunkLibs/libGL/libGL_Host.cpp`:

- Profil ve orphan deneyi ayrı opt-in. İkisi de kapalıyken native `glBufferSubData` dışında ek kapasite sorgusu, saat okuması, örnek tablosu araması yok. Mevcut opt-in TBO debug kontrolü değişmedi.
- Profil örnek tablosu en fazla 256 farklı anahtar tutuyor; taşan örnekler `dropped` sayacına yazılıyor. Raporlama kapalıyken sort/print yapılmıyor.
- Orphan yalnız mevcut güvenli tam-buffer koşulunda; mapped/immutable kontrolleri korunuyor.
- Ayarlar süreçte bir kez okunuyor. Önceki orphan bayrağının 128 swap'ta bir canlı yoklanması kaldırıldı; açıp kapatmak için runtime sürecini yeniden başlatmak gerekiyor.
- Profil: `files/vexa_upload_profile.flag` veya gerçek host process environment'ında `VEXA_GL_UPLOAD_PROFILE=1`.
- Orphan: `files/vexa_buffer_orphan.flag` veya gerçek host process environment'ında `VEXA_GL_BUFFER_ORPHAN=1`.
- Yeni environment isimleri runtime forward allowlist'ine eklenmedi; cihazda app-private dosya bayrakları kullanılabilir. Vulkan ile ilgili launch.cpp değişikliklerine dokunulmadı.

### Modern draw ve BGRA

`legacy_client_runtime.inc`, `legacy_client_host.inc`, `program_cache_state.inc`:

- `!LegacyClient.Used` kontrolü artık VAO query ve BGRA reflection/mutex işinden önce ucuz bir çıkış sağlayabiliyor.
- Share-group içinde bir BGRA setter kullanıldığında monotonic atomic `BGRATouched` işaretleniyor. O andan sonra başka bir context legacy kullanmıyor olsa bile BGRA maskesi sıfırlama yolunu çalıştırıyor. Böylece paylaşılan programdaki önceki context'in nonzero maskesi sızmıyor.
- Bu işaretin monotonic tutulması delete, client stack restore veya program-name reuse sonrasında yanlış erken çıkış yapılmamasını sağlıyor. Karışık legacy share group'larda daha az optimizasyon, daha güvenli davranış tercih edildi.
- Pipeline yok ve alpha/clip desteği kapalıysa `VexaPreDraw` share-group kilidine girmeden dönüyor. Pipeline draw'unda generation refresh ve validation yolu korunuyor.

### Compatibility uniform'ları ve matrisler

`alpha_test.inc`, `legacy_clip.inc`, ilgili state header'ları, `arb_assembly_runtime.inc`:

- Alpha function/reference son uygulanan değerleri program share-group metadata'sında tutuluyor. Değişmeyen değer native API'ye tekrar gönderilmiyor; farklı context aynı programı farklı değerle kullanırsa tekrar gönderiliyor.
- Clip derived değerleri context-local; projection/model/equation içerikleri karşılaştırılıyor. Stack restore setter'ları atlayabildiği için yalnız setter dirty-bit'ine güvenilmiyor.
- Program başına son eye/projected/enabled/MVP payload'ı karşılaştırılıyor; yalnız değişen native uniform'lar yazılıyor.
- Mevcut relink/program-binary/delete cache invalidation yolları yeni applied-value alanlarını da kapsıyor; eski cache kaydı silinince ilk kullanım tekrar yazıyor.
- ARB linked pair başına sampler bir kez, local/env uniform dizileri yalnız değiştiğinde yazılıyor. Shader revision değişince yeni pair'in cache'i boş başlıyor.

### Unpack

`VexaUnpackScope` gerçek native state'i okumaya devam ediyor. Aynı değeri iki kez set/restore etmek yerine sadece scope'un değiştirdiği alanlar için native yazı yapılıyor. Default unpack state'te 12 pixel-store çağrısı yerine 2; alignment zaten 1 ve diğer alanlar 0 ise 0. Nondefault row/skip/image/PBO restore davranışı korunuyor. Bu, henüz genel pixel-store state cache'i değil.

## Doğrulama

- `GL-host-64` ve `GL-host-32` hedefleri derlendi.
- `test_gl_hotpaths.py` gerçek üretim fonksiyonlarını/ince dosyalarını kullanan Android-native, gerçek Mali GLES piksel probe'unu derliyor. FEX/oyun çalıştırmıyor.
- Default, compatibility, profiling ve orphan olmak üzere dört çalışma modu cihazda geçti.
- Kontroller: normal upload'ın sıfır ek query/clock kullanması; native invalid-target hatası; partial-write komşu baytlarının korunması; 256 örnek sınırı; tam-buffer orphan; immutable storage'a orphan uygulanmaması; unpack/PBO restore; modern pre-draw query sayısının sıfır olması; iki shared context'te BGRA mavi→kırmızı→mavi ve normal pointer'a geçiş; değişmeyen alpha/clip'in 100 tekrarda sıfır uniform yazması; alpha/clip state değişikliklerinin doğru piksel üretmesi; context geçişi; relink sonrası yeniden uygulama; ARB local/env değişikliği.
- Native probe'daki relink testi üretim cache kayıtlarını açıkça invalidate ediyor; test guest thunk relink wrapper'ından geçmiyor. Shared-program yarışlarının tümünü doğrulamıyor.
- `bench_gl_hotpaths.py` HEAD ve mevcut kaynakları ayrı native binary'lerde derliyor. İşlem: tek-bayt BufferSubData + legacy scope + pre-draw; **gerçek draw yok**. Her koşuda 2000 warmup, ardından 9×10000 işlem var. Guest geçişi veya GPU frame işi ölçülmüyor.

Son A/B/A/B koşusu, batch medyanları:

| Koşu | ns/işlem |
|---|---:|
| A1 — önce | 626,27 |
| B1 — sonra | 313,13 |
| A2 — önce | 617,05 |
| B2 — sonra | 311,60 |

Önce her 10000 işlemde 20000 generic state query, 10000 capacity query ve 20000 clock okuması; sonra üçü de sıfır. Bu dar CPU/driver mikrotestinde yaklaşık yarıya düşüş var; **oyun FPS kazanımı veya gerçek frame p99 ölçümü değil**. İlk bağımsız koşu da aynı yöndeydi (yaklaşık 564–590 → 273–283 ns/işlem); son tablo sıcaklık/frekans değişkenliğini gizlememek için tek tamamlanmış A/B/A/B serisini kullanıyor.

## Paket ve kullanıcının değişikliklerinin korunması

- Sadece yeni 64/32 GL host asset'leri ayrıca stage edildi; STAMP güncellendi.
- Vulkan source diff'i başlangıç snapshot'ıyla aynı; Vulkan asset hash'i stage öncesi/sonrası aynı: `bcbec7a36e3aba33a89ed1f7cb82bb621baa295be105eb141ce7d5f9cb011c3f`.
- Kullanıcının BCn decode kaynağı/call site'ları ve runtime launch.cpp değişiklikleri korundu.
- Debug APK assembleDebug başarılı. Mevcut configured FEX build kullanıldı; verifyFexCompat/configureFexAndroid/toplu stageSDL3Thunks görevleri atlandı. Bu işlem temiz bir checkout build'i değil.
- Yeni GL host SHA-256: 64-bit guest yolu `5f1921a14995cdc109ae6c7aca26dcf0d16e1300f6959c869bf06dceaac5823c`, 32-bit guest yolu `d8041894ca41dfe050714cffa595a585573e2da5bc638a37ede5012974aa69ad`.
- Cihazda başka bir runtime oturumu çalışıyordu. Bu çalışma onu durdurmadı ve yeni APK'yı kurmadı; kaynak/asset/APK hazır, oyun içi A/B ve i386 guest üzerinden doğrulama henüz yok.

Kanıtlar: `logs/gl-hotpath-20261026/` içindeki `*-test.txt`, `*-bench.txt`, `bench-summary.json`, `staged-validation.json`, başlangıç source diff snapshot'ları. Yeni test kaynakları `guest-tests/graphics/test_gl_hotpaths.py`, `gl_hotpath_probe.cpp.in`, `bench_gl_hotpaths.py`.
