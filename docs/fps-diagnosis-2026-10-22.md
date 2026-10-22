# Bağlı cihazda VEXA FPS araştırması — 22 Ekim 2026

## Sonuç ve güven sınırı

En ciddi yeni bulgu, yük devam ederken cihazın CPU frekanslarının belirgin şekilde düşmesi ve bir CPU kümesinin üst frekans sınırının sistem tarafından azaltılmasıdır. FEX üzerinden çalışan motor kodu CPU işinin büyük bölümünü oluşturduğu için bu durum performans açısından önemlidir. İkinci somut bulgu, GL katmanında açık tanılama bayrağı gerektirmeyen, her kare tekrarlanan sürücü sorgularıdır.

Bütün oyunların düşük FPS'ini açıklayan tek bir neden veya ölçülmüş bir düzeltme kazancı henüz kanıtlanmış değildir. Frekans sınırını koyan üretici politikası, aynı sahnede kısıt kalkmış karşılaştırma ve Vulkan oyun profili eksiktir. Menü ve oynanış sayıları aynı iş yükü değildir; aralarındaki FPS farkını yalnızca frekansa atfetmiyorum.

Kaynak kod, APK ve kurulu thunk kütüphaneleri değiştirilmedi. Cihazda oyunlar başlatıldı, kısa bir Megabonk oynanışı yapıldı, geçici JIT sembol kaydı ve CPU/Perfetto profilleri alındı. Profil amaçlı eklenen JIT bayrağı kaldırıldı ve değiştirilen profiler sistem özellikleri başlangıç değerlerine geri döndürüldü.

## Cihaz ve binary kimliği

- ADB seri: `TSORX4OB45L779WO`; model `2407FPN8EG`; SoC `MT6989`.
- Uygulama: `com.critical.vexaemulator`, kurulum zamanı 21 Ekim 2026 23:02.
- VEXA başlangıç commit'i: `7455614`.
- Yerel FEX commit'i: `de26b0e8f69383c6120a5bbd4487a1d1cc7abd9f`; kaynak ağacında izlenen değişiklik yoktu.
- Cihazdaki GL host SHA256: `e4c52bd9c2b4c6b579c8f9cf160740bf87e189d6af47c2421889c8bd213e238b`.
- Cihazdaki GL guest SHA256: `5c4d59508f204afed0b909268e734b72b3770386d876f0b3196da9c7f3be628f`.
- İki GL dosyası da mevcut paketleme assets dosyalarıyla eşleşti; aktif süreç maps ve simpleperf kaydı GL host'un yüklenmesini doğruladı. Bu, bütün yerel FEX kaynaklarının bu binary'ye derlendiği anlamına gelmez.
- Başlangıçta oyun çalışmıyordu. Bu nedenle önceki raporlar güncel ölçüm sayılmadı.

Kanıtlar: `logs/fps-audit-20261022/`.

## 1. Doğrulanan frekans düşüşü ve sistem sınırı

Perfetto CPU frekans izleri:

| CPU kümesi | Menü izi: görülen en yüksek | Sonraki oynanış/upgrade izi: görülen en yüksek |
|---|---:|---:|
| CPU 0–3 | 1,6 GHz | 0,9 GHz |
| CPU 4–6 | 2,1 GHz | 1,3 GHz |
| CPU 7 | 3,2 GHz | 1,7 GHz |

Zaman ağırlıklı ortalama CPU 7 frekansı ilk izde yaklaşık 2,86 GHz, sonraki izde 1,65 GHz idi. CPU 4–6 ortalaması yaklaşık 1,64 GHz'den 1,21 GHz'e düştü. Bunlar farklı sahneler ve farklı zamanlar olduğu için FPS kazanç/kayıp yüzdesi hesabında kullanılamaz.

Sonraki sabit upgrade ekranındaki `touch.pftrace`, **`Cpu 4 Max Freq Limit = 1000000 kHz`** olayını kaydetti. Bu CPU 4–6 kümesinin sınır değişimini gösterir. CPU 7 için aynı izde bir maksimum sınır olayı yoktur; 1,7 GHz yalnızca o ölçümlerde görülen frekanstır, kanıtlanmış policy7 üst sınırı değildir.

Aynı upgrade ekranında önce dokunmadan, sonra boş alana Android dokunuşları göndererek, sonra tekrar dokunmadan örnekleme yapıldı. CPU 7 bütün 24 örnekte 1,7 GHz kaldı. CPU 4 kümesi 1,1 GHz'den 1,0 GHz'e indi. Dokunma eksikliğinin tek başına açıkladığı bir geçici boost eksikliği doğrulanmadı. Görüntüde sayaç yaklaşık 77–79 FPS idi.

`low_power=0`; Android Thermal Status `0`; HAL CPU sıcaklığı yaklaşık 61 °C, batarya/skin yaklaşık 38–39 °C bildirdi. Mali cooling-device değeri `28` idi. Bu değerler tek başına termal kısıtı dışlamaz veya hangi limitin hangi sıcaklıkta uygulandığını göstermez. `mQosLevel=-1` de belirli bir termal profile kanıt değildir. Üretici thermal config dosyaları okunabilir ama ikili biçimdedir; içerikleri çözülmedi. Root/su yok; sysfs maksimum frekans ve bazı GPU kullanım dosyaları erişime kapalı.

**Yorum:** Sistem frekans sınırlaması gerçektir. Bunun termal, şarj, üretici güç bütçesi veya uygulama sınıflandırma politikasından hangisi tarafından uygulandığı henüz kesin değildir. Frekans sınırlamasını kaldıran aynı-sahne A/B olmadan kaybedilen FPS miktarı söylenemez.

Kanıt: `gl-full-stable.pftrace`, `gl-game.pftrace`, `touch.pftrace`, `trace-summary.json`, `touch-clocks.json`, `power-thermal.txt`.

## 2. CPU işinin büyük kısmı çevrilmiş motor kodu

Megabonk GL oturumunda `simpleperf` ile 10 saniyelik CPU örneklemesi alındı. FEX perf map ve süreç maps kullanılarak anonim JIT adresleri guest modüllerine eşlendi.

- Menü profili: tutulan örneklerin yaklaşık %55,2'si `UnityPlayer.so`, %8,1'i `GameAssembly.so`.
- Oynanışın erken bölümündeki profil: yaklaşık %69,5 `UnityPlayer.so`, %4,4 `GameAssembly.so`.
- Oynanış CPU-cycle ağırlıklı raporunda Mali sürücüsü render thread üzerinde yaklaşık %10,5, VEXA GL host'un kendi kodu yaklaşık %0,37. Bu son oran GL host çağrılarının sürücüde veya beklemede geçirdiği toplam süre değildir.
- Örnek sayısı oranları ile CPU-cycle ağırlıkları farklı ölçülerdir. Hiçbiri GPU kullanım yüzdesi, doğrudan FEX ek maliyeti veya FPS kaybı yüzdesi değildir.
- Unity guest kodunun kendi içindeki işlemler ile x86/ARM çeviri ek maliyetini ayıracak native ARM karşılaştırması yoktur. Bu nedenle "FEX %69 performans kaybettiriyor" sonucu çıkmaz.

Sonraki yaklaşık 10 saniyelik scheduler izinde ana guest thread 6,93 CPU-saniyesi, render thread 6,30 CPU-saniyesi çalıştı. Ana thread'in 6,13 saniyesi CPU 7 üzerinde geçti. Worker thread'lerde de önemli çalışma ve runnable bekleme süreleri var; tek bir mutex veya tek bir fonksiyon baskın kök neden olarak teşhis edilmedi.

Kanıt: `jit-symbols.txt`, `jit-dso.txt`, `game-symbols.txt`, `game-dso.txt`, `jit-perf.map`, `game-perf.map`, `game-maps.txt`, `trace-summary.json`. Yeniden analiz komutu:

```powershell
python tools/jit_profile.py logs/fps-audit-20261022/game-symbols.txt logs/fps-audit-20261022/game-perf.map logs/fps-audit-20261022/game-maps.txt --top 20
```

## 3. Kaynakta doğrulanan gereksiz GL tanılama işi

`../FEX/ThunkLibs/libGL/libGL_Host.cpp`, `VexaDiagDraw`:

- İlk 48 çizim için `glGetIntegerv(GL_CURRENT_PROGRAM)` her kare çağrılıyor.
- FB0 üzerindeki ilk 8 çizim için viewport/program/texture/blend/color-mask dahil 7 ek sorgu yapılıyor.
- Bu iki blok açık tracing veya debug bayrağına bağlı değil. Uygun karede 48 + 8×7 = **104 ek sürücü sorgusu/kare** yapılabilir. Bu bir üst sınırdır; bütün karelerde 104 ölçülmedi.
- Bu kodu çağıran GL draw wrapper'ları farklı oyunlar tarafından kullanılabilir. Vulkan yolu için genellenemez.
- `VexaPresentFallback` içindeki overflow logu da `overflow > 0` koşulunda debug bayrağından bağımsız yazılıyor. İlk menü oturumunda Player.log içinde tekrarlandığı görüldü.
- `glBindFramebuffer` her FBO bind'inde tanılama için `snprintf` ile bağlama sırası biriktiriyor.

Bu, grafik uyumluluğu için gerekli state takibiyle aynı şey değildir. Önerilen dar değişiklik yalnız tanılama kaydı/sorgularını debug koşuluna almak, sunum fallback ve grass buffer uyarlamasını korumaktır. Bu araştırmada değiştirilmedi ve FPS etkisi A/B ölçülmedi.

Önceki notlardaki bir aday artık geçerli değil: güncel `ScopedGLGuestTrace` trace kapalıyken atomic artırmadan dönüyor. Eski atomic önerisi tekrar edilmemeli. Base-instance ve multi-draw'ın host'a taşınmış yolları da mevcut; eski guest-döngüsü önerileri güncel kaynak için doğrudan geçerli kabul edilmedi.

## 4. Cihazda kalan FBO döküm bayrağı

Başlangıçta `files/vexa_fbo_dump.flag`, `files/fex_gdb.flag`, `files/vexa_tbo_dump_disable.flag` vardı. Son ETS2 kaydı yoğun FBO transition tanılaması içeriyordu.

FBO bayrağı açıkken kod ilk 60 uygun geçişte `glReadPixels`, piksel tamponu ayırma ve PPM dosyasına piksel başına yazma yapıyor. Bu başlatma/ilk karelerde gerçek ek iştir. 16 elemanlık seen tablosu dolduğunda yeni FBO'lar kaydedilemediği için transition satırları ilerleyen karelerde de tekrarlanabilir.

İlk 60 dump sınırı nedeniyle sürekli oynanış FPS'inin tamamını bu bayrağa yüklemek yanlış olur. Bayraklar başlangıç durumunda bırakıldı. GDB bayrağının tek başına düşük FPS nedeni olduğu gösterilmedi.

## 5. Dışlanan veya doğrulanamayan açıklamalar

- **"Sadece küçük çekirdekler kullanılabiliyor":** ölçülen aktif süreç/thread'lerde izin maskesi 0–7 ve ana/render iş parçacıkları CPU 7'de çalıştı. `/foreground` ismi tek başına büyük çekirdek yasağı anlamına gelmiyor.
- **"Bütün oyunlar 60/72 FPS kilidine takılıyor":** Megabonk menüsünde oyun içi 151–183 FPS ve GL swap aralıkları yaklaşık 6 ms görüldü. Ekran 144 Hz; oyun sayacı, swap ve ekrana sunulan kareler ayrı ölçüler. Bu oturum için evrensel 60/72 sınırı yok.
- Mevcut `frame_bench.py` queueBuffer filtresi bazı kayıtlarda Activity yüzeyini de eşleştiriyor. Android 16 `RequestedLayerState{...}` biçimi nedeniyle SF latency satırları da alınamadı. `summary.json` içindeki 144,3 queueBuffer FPS, güvenilir bağımsız oyun sunum FPS'i olarak kullanılmadı.
- **"Bütün sorun GPU doluluğu":** güvenilir GPU busy/pass süresi alınmadı. GPU frekansı tek başına kullanım ölçüsü değildir; bu açıklama doğrulanmadı ve tamamen dışlanmadı.
- ETS2 normal açılışı bu turda oynanışa ulaşmadı. Warband direct-launch denemesi `libGLEW.so.2.1` bulunamadığı için çıktı; ikinci oyun benchmark'ı sayılmadı. Bu açılış problemleri FPS kök nedeni diye sunulmuyor.
- Unity/GL ölçümü Vulkan ve bütün oyunlar için evrensel sonuç değildir.

## Sonraki doğrulamanın önceliği

1. Aynı oyun/sahne/ayar ve aynı runtime ile frekans sınırının yüksek ve düşük olduğu koşulları karşılaştır; başlangıç sıcaklığı, güç/şarj durumu, policy limitleri ve kare sürelerini beraber kaydet. Menü→oynanış farkını A/B sayma.
2. GL tanılama sorgularını tek başına kapatan aday host derlemesini aynı sahnede karşılaştır; geometri, renk ve sunum doğruluğunu koruduğunu doğrula.
3. Frekanslar eşitlendikten sonra guest motor/GL driver payını ve Vulkan oyununu ayrı profille. TSO/split-lock doğruluk ayarlarını körlemesine kapatma.

Bu araştırma bir FPS düzeltmesi veya kazanç yüzdesi iddiasıyla sonuçlanmadı; sistem kısıtı, CPU iş dağılımı ve kaynakta kaldırılabilir tanılama işi somutlaştırıldı.
