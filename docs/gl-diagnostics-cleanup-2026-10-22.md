# GL tanılama temizliği ve cihaz testi — 22 Ekim 2026

## Değişiklik

`../FEX/ThunkLibs/libGL/libGL_Host.cpp` içinde:

- İlk 48 draw'ın program sorgusu ve FB0 üzerindeki ilk 8 draw'ın yedi state sorgusu yalnız `VexaGLDebugEnabled()` açıkken çalışıyor. Normal çalışmada bu tanılama bloklarından gelen, kare başına en fazla 104 ek sürücü sorgusu kaldırıldı.
- Tanılama için FBO bağlama sırasını `snprintf` ile biriktirme ve FBO tracking overflow mesajları da aynı debug koşuluna alındı.
- Sunum için FBO takibi, sunum fallback'i, gerçek GL bind/draw çağrıları ve grass buffer uyarlaması korunuyor. İhtiyaç duyulduğunda `files/vexa_gldbg.flag` ile mevcut tanılama tekrar kullanılabilir.
- Cihazdaki `files/vexa_fbo_dump.flag` kaldırıldı. Önceki bayrak ve host kütüphaneleri `files/fps-cleanup-backup-20261022/` altında saklandı.

## Derleme, paketleme ve kurulum

`GL-host-64` ve `GL-host-32` başarıyla derlendi. Değişiklik öncesi HEAD kaynağı ayrıca yeniden derlendi: 64 bit çıktı cihazın eski `e4c52bd9...` kütüphanesiyle birebir eşleşti. Böylece yeni host'taki farkın incelenen kaynak değişikliğinden geldiği doğrulandı.

Yeni dosyalar paketleme assets'ine kopyalandı; içerik hash'inden runtime STAMP yenilendi. APK paketleme başarılı, `adb install -r` başarılı. Normal oyun hazırlama yolu yeni runtime'ı kurdu; ardından direct-launch ile kontrollü test yapıldı. Cihazdaki stamp `vexa-graphics-1:f758bef7d3101834`.

Kurulu ve paketlenmiş host hash'leri:

| Host ABI | SHA256 |
|---|---|
| 64 | `33a3b4aa32a4a68809aea2b8ed709271e16a0c41e1129fec78af87a185fea699` |
| 32 | `92e07a713409f8223efcdcda546da4c598ff8db41b73862e8e5da3857f4c581d` |

APK karşılaştırmasında native Android/FEX core kütüphaneleri ve uygulama kodu aynı kaldı. Farklar iki GL host, runtime STAMP ve build manifest; ayrıca önceki APK'den sonra zaten cihazda kullanılan SDL2 guest input düzeltmesi bu APK'ye dahil edildi. SDL2 guest hash'i paket ve cihazda `214b8972...` olarak eşleşiyor; bu testte cihazdaki SDL2 sürümü değiştirilmedi.

**Mevcut tam derleme engeli:** normal `:app:assembleDebug`, `verifyFexCompat` adımında yerel FEX HEAD `de26b0e8...` sabitlenmiş `eedae0c4...` commit'ini içermediği için duruyor. Pin veya FEX geçmişi değiştirilmedi. Bu yalnız GL host güncellemesi için GL hedefleri mevcut yapılandırmada ayrı derlendi; mevcut Android/FEX core binary'leri korunarak native yeniden derleme ve genel runtime staging adımları hariç tutulan APK paketleme çalıştırıldı. Bu, tam native rebuild doğrulaması değildir.

## Ölçüm yöntemi ve ilk sonuçlar

Megabonk GL, aynı ana menü, aynı 2712×1220 başlatma parametreleri. Oyun içi `[FRAME]` kayıtlarındaki 256 örneklik pencereler ve saniyede bir üç CPU kümesi frekansı kaydedildi. Pencereler örtüşüyor; pencerelerin medyanı birleşik benchmark ortalaması veya gerçek ekran FPS'i değildir. 144 Hz ekran yenilemesi ile render döngüsü FPS'i ayrıca ayrılmalıdır.

İlk eski sürüm ölçümünde CPU 7 medyan 2,3 GHz, ilk yeni sürüm ölçümünde 1,7 GHz idi. 5,845 ms ve 7,52 ms sonuçları bu nedenle bir regresyon veya kazanç hesabı olarak kullanılmadı. Isınmış cihazda, FBO dökümü kapalı tutularak eski host tekrar denendi ve yeni host ile tekrar ölçüm yapıldı.

FBO bayrağı açık ilk eski oturumun ilk 128 kare penceresinde 33 ms üzeri 59 kare ve 123 `[DUMP]` satırı vardı. Yeni ilk oturumda aynı pencere 7 ağır kare ve sıfır dump satırı içeriyordu. Başlatma/cache/frekans koşulları tam eşit olmadığı için bu sayılardan tek başına kazanç yüzdesi çıkarılmıyor. Büyük başlatma takılmalarının tamamı çözülmüş değildir; yeni oturumda da tekil uzun yükleme aralığı vardı.

## Son sessiz ölçüm

`warm-candidate-final` adlı ilk kurulum-sonrası ölçümde telefon Ayarlar/USB ekranına geçti. Bu kayıt karşılaştırmadan çıkarıldı. Kullanıcı telefonu serbest bıraktıktan sonra, çalışan yeni sürümde yeniden 20 saniyelik `final-quiet` kaydı alındı; son ekran görüntüsü oyunun aynı ana menüsünü doğruluyor.

| Isınmış cihaz, FBO dump kapalı | Eski host (`warm-baseline`) | Yeni host (`final-quiet`) |
|---|---:|---:|
| Kare pencerelerinin ortalama süre medyanı | 8,92 ms | 7,25 ms |
| Yukarıdaki değerin render döngüsü FPS karşılığı | 112,1 | 137,9 |
| CPU 0 / CPU 4 / CPU 7 örnek medyanları | 0,9 / 1,0 / 1,7 GHz | 0,9 / 1,0 / 1,7 GHz |
| Kaydedilen, örtüşen kare penceresi | 19 | 23 |

Bu tek karşılaştırma olumlu sinyal veriyor: kare süresi yaklaşık %18,7 daha düşük, render döngüsü FPS karşılığı yaklaşık %23 daha yüksek. Ancak eski kayıt için kullanıcı etkileşimsizliği son sessiz kayıt kadar güvenceye alınmadı; arka plan yükü, GPU frekansı, menü animasyonu ve termal durum tam sabitlenmedi. Bu nedenle **kanıtlanmış %23 genel performans kazancı** veya oynanış benchmark'ı olarak sunulmamalı. Son sessiz kaydın pencere p99 değerlerinin medyanı 10,6 ms; bu birleşik ölçümün gerçek p99'u değildir.

Kurulu manifest `installedMismatches=0` ve yüklenen dosya izlemesi `trackedMismatches=0` bildirdi. Son kayıt yeni `33a3b4aa...` GL host hash'ini ve FBO bayrağının kapalı olduğunu doğruladı. Menü görüntüsünde belirgin render bozulması gözlenmedi. 32 bit host derlendi ve kuruldu; ayrı bir i386 oyun testi yapılmadı.

Kanıt klasörü: `logs/fps-cleanup-20261022/`. Baseline/candidate host yedekleri, derleme ve paketleme logları, APK fark listesi, her ölçümün Player.log/summary.json/frekans/sıcaklık/maps ve ekran görüntüleri burada.
