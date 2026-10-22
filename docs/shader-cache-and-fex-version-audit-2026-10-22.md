# Shader/program önbelleği ve FEX sürüm denetimi — 22 Ekim 2026

## Sonuç

Önbellek altyapısının geliştirilmesi yerinde; ancak ilk iş tüm shader'lar için disk cache eklemek değil, mevcut program/clone kayıtlarının yaşam döngüsünü düzeltmek ve gerçek derleme maliyetlerini ölçmek olmalı. Mevcut clone binary cache'in sınırlı doğruluk testi bağlı Android cihazda yeniden geçti. Oyun FPS/p99 kazancı ölçülmedi.

İncelenen yerel FEX checkout'u `de26b0e8f69383c6120a5bbd4487a1d1cc7abd9f`, dalı `FEX-2601-local`. `git describe --tags --always`: `FEX-2601-33-gde26b0e8f`. Bu checkout'u FEX-2603 olarak adlandırmak doğru değil. Kurulu APK'nin tüm native bileşenlerinin bu HEAD'den derlendiği ayrıca doğrulanmış değildir.

Ürün kodu, FEX geçmişi, pin ve kurulu APK değiştirilmedi. Mevcut izlenmemiş/commit edilmemiş çalışma korundu. Yalnızca bu rapor ve `logs/clone-cache-audit-20261022/` test çıktıları oluşturuldu; cihazda bağımsız bir native probe çalıştırıldı.

## Mevcut önbellekler

| Katman | Mevcut davranış | Sınır |
|---|---|---|
| Runtime GLSL -> SPIR-V -> ESSL çeviri cache'i | Shader türü ve kaynak hash'iyle RAM'de çevrilmiş metin tutuyor | Mevcut CMake cache'inde OFF; Gradle varsayılanı da OFF. Derlenmiş GPU programı değil. Aktif edilirse hash çakışma doğrulaması ve bellek sınırı ayrıca gerekir. |
| Clone program binary cache'i | Dönüştürülmüş vertex kaynağının tamamıyla eşleşen thread-local LRU; 8 MiB/64 kayıt | RAM'de; süreç kapanınca kaybolur. Yalnızca pipeline onarımı için oluşturulan vertex clone'ları kapsar. Her thread'in ayrı bütçesi vardır. |
| Normal GL programları | `glCompileShader`, `glLinkProgram`, `glCreateShaderProgramv` sürücüye gider | Bu yollara otomatik VEXA binary cache eklenmemiş. Guest oyunun veya sürücünün kendi cache'i bulunabilir; bunların bulunmadığı iddia edilmiyor. |
| Vulkan pipeline cache | Guest'in VkPipelineCache nesnesi host çağrılarına aktarılır; pInitialData ve GetPipelineCacheData yolu var | İncelenen yolda uygulamalar arasında kalıcı VEXA cache yöneticisi yok. Guest/driver cache davranışı ayrı incelenmeli. |
| FEX JIT cache | CPU kodu için ayrı altyapı | GL shader/program binary cache ile aynı şey değil. |

Kaynaklar: `../FEX/ThunkLibs/libGL/clone_binary_cache.{h,inc}`, `libGL_Host.cpp:437,1126,3305,5748,7019,7047,7172`; `../FEX/ThunkLibs/libvulkan/Host.cpp:1073,2799`; `vulkan_pass_profile.inc:213`; `app/build.gradle.kts:101`; `../FEX/build-android-arm64-ninja/CMakeCache.txt`.

## Öncelikli bulgular

### 1. Program kaynak kayıtları relink sırasında yenilenmiyor

`libGL_Host.cpp:7060` içindeki `g_vexa_prog_sources[program]` vektörüne kaynaklar ekleniyor, önceki nesil temizlenmiyor. Clone oluşturma kodu bu vektördeki ilk vertex kaynağını seçiyor (`3354`). Aynı program ID'si farklı shader ile yeniden linklenirse eski kaynak seçilebilir.

Üstelik `VexaTakeShaderSource` shader'ın kaydını taşıyıp siliyor; aynı shader birden çok programa bağlanıp linklenirse sonraki program snapshot'ı kaynağı bulamayabilir. Bu kısım gerçek GL nesne yaşam döngüsüne göre yeniden tasarlanmalı; yalnızca vektöre clear eklemek yeterli değil.

Bunlar kaynak koddan görülen eksikliklerdir; mevcut oyunda görüntü hatası ürettikleri bu incelemede gösterilmedi.

### 2. Clone/program/pipeline kayıtlarının silinmesi ve nesil takibi eksik

Program kaynakları, clone listeleri ve uniform-location eşlemeleri global tablolarda saklanıyor. İncelenen GL kodunda `glDeleteProgram`, `glDeleteShader`, `glDeleteProgramPipelines` yollarına bu tabloları temizleyen özel sarmalayıcılar eklenmemiş; ilgili API'ler normal üretilmiş thunk yolunda. `glUseProgramStages` pipeline doğrulama kaydını siliyor fakat bu, program relink/delete ve context/share-group yönetiminin tamamını karşılamıyor.

Sonuç olarak program ID tekrar kullanımı, eski clone kayıtlarına uniform yayılımı ve büyüyen kayıtlar için risk var. Düzeltme GL'nin silinmesi ertelenen nesne kurallarına, paylaşılan context'lere ve program nesillerine uymalı; körlemesine bütün tabloları temizlemek doğru çözüm değildir.

### 3. Cache isabeti bütün ilk-kullanım maliyetini kaldırmıyor

`VexaPreDraw()` ilk kullanımda pipeline doğruluyor. Onarım gerekirse cache isabetinde bile yeni GL programı oluşturuluyor, binary yükleniyor, link durumu kontrol ediliyor; kaynak arayüzü okunuyor ve uniform/block eşlemeleri yeniden kuruluyor. Bu maliyetler derlemeden ayrı ölçülmeli. Binary dışa aktarma da miss yolunda senkron yapılıyor.

Cache boyutunu körlemesine artırmak yerine hit/miss/reject/eviction sayıları, compile/link/binary load/export süreleri ve ilk draw zamanı toplanmalı. Mevcut `binary_hit` satırı tek başına yeterli telemetri değil.

### 4. Kalıcı clone cache mantıklı ikinci aşama

Oturumlar arasında aynı clone kaynakları tekrar derleniyorsa disk cache yararlı olabilir. Güvenli tasarım: son dönüştürülmüş kaynak + dönüşüm şema sürümü + GLES sürücü/GPU kimliği + binary formatı; sınırlı dosya boyutu, atomik yazma, kota, sürücü reddinde kaynak derlemesine dönüş. GL nesneleri/uniform değerleri paylaşılmamalı.

Genel program cache'i daha kapsamlı iş: bütün shader aşamaları ve link öncesi attribute/transform-feedback/separable ayarları anahtara dahil edilmeli. Yalnızca `glLinkProgram` sarmalayıcısına cache eklemek, daha önce çalışmış `glCompileShader` maliyetini ortadan kaldırmaz.

Khronos, ProgramBinary yüklemesinin uniform ve block binding durumunu başlangıç değerlerine döndürdüğünü belirtir. Yaşam döngüsü ve durumun yeniden kurulması bu nedenle zorunludur: https://registry.khronos.org/OpenGL/specs/es/3.1/es_spec_3.1.pdf

## Bu turdaki cihaz testi

Mevcut `guest-tests/graphics/test_clone_binary_cache.py` kaynakları değiştirilmeden yeni çıktı diziniyle çalıştırıldı. Probe gerçek clone kodundan derlendi ve `adb` ile bağımsız çalıştırıldı.

Sonuç:

```text
PASS cache hit, independent rendered uniforms, changed source, corrupt fallback, LRU/budget;
driver microbenchmark binary=0.644ms source=0.679ms
```

Bu test temel binary yükleme/fallback yolunu doğrular. Relink, deletion/ID reuse, bir shader'ın birden çok programa bağlanması, çoklu context ve gerçek oyun ilk-kullanım takılmalarını kapsamaz. Mikro ölçüm oyun FPS kazancı değildir; sürücünün kaynak derleme cache'i de bu süreleri etkileyebilir.

## FEX tabanı ve güncelleme yönetimi

- FEX-2601 commit'i: `1188c90c1`; FEX-2603 commit'i: `9eb639e89` (annotated tag nesneleri değil, commit'ler).
- HEAD, yerel FEX-2601 etiketi üzerine 33 commit içeriyor. Listelenen commit'ler VEXA/Android değişiklikleri.
- ARM64 emitter dosyası HEAD ile FEX-2601 arasında birebir aynı. FEX-2603 ile farklı.
- FEX-2603 ağacında `External/rpmalloc` var; HEAD ağacında yok. HEAD allocator kodu jemalloc yolunu içeriyor.
- Üretilmiş mevcut build sürümü `FEX-2601-32-g5baa836`; bu HEAD'den eski bir build kimliği.
- Repo shallow. Gelecekte upstream entegrasyonundan önce ortak geçmiş tamamlanmalı; yalnızca başarısız merge-base üzerinden eksik commit sonucu çıkarılmamalı.
- `python tools/apply_fex_compat.py --fex-root ../FEX --check` başarısız: HEAD, `tools/fex.lock.json` içindeki `eedae0c48af4a8e8f2fa2d31f34313962cc6f37a` pin'ini içermiyor. Eksik geçmiş veya ayrışmış kaynak olasılığı çözülmeden pin kontrolü atlanmamalı/pin körlemesine değiştirilmemeli.

Önerilen politika:

1. Upstream resmi etiketlerini değiştirmeden tut; VEXA fork release'lerini `vexa-fex-2603-r1` gibi ayrı adlandır.
2. Paylaşılan fork dalı için resmi release tag'ini ayrı entegrasyon dalında merge et; mevcut Android değişikliklerini koru. Rebase, özel/taslak patch stack için alternatif; yayınlanmış geçmişi zorla yeniden yazma.
3. Cherry-pick'i seçilmiş backport'lar için kullan, bütün release güncellemesini otomatik commit seçimiyle taklit etme. `cherry-pick -x` ile kaynağı kaydet; bağımlılıklarını ve sonraki düzeltmelerini incele.
4. Yalnızca birkaç 2603 yaması alınmış 2601 tabanını 2603 diye sunma. Taban, backport listesi ve fork commit'ini ayrı kaydet.
5. Otomasyon upstream tag'ini algılayabilir, entegrasyon dalı açabilir, build/test raporu hazırlayabilir. Üretim pin'ini yalnızca başarılı doğrulama sonrası güncelle.
6. Test: temiz tam build; eşleşen 32/64 guest-host thunks; ELF ve Proton yollarında ayrı smoke; sinyal/atomik/süreç doğruluğu; aynı sahnede cold/warm cache, FPS/frame-time ve termal A/B. Her raporda kaynak commit'iyle yüklenen binary hash'lerini birlikte kaydet.

Referanslar: https://git-scm.com/docs/git-merge , https://git-scm.com/docs/git-cherry-pick , https://fex-emu.com/FEX-2603/
