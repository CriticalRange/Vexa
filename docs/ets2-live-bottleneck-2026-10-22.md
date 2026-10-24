# Açık ETS2 oturumunda canlı darboğaz profili — 22 Ekim 2026

## Sonuç

İncelenen canlı sahnede ana yük shader/program derlemesi veya Android'e kare aktarımı değil; FEX tarafından çevrilmiş oyun kodunun CPU'da yürütülmesi ve çizim komutlarının hazırlanmasıdır. GPU sürücüsünün CPU tarafında da kayda değer iş var. Saf GPU render süresi alınamadığı için GPU maliyetinin tamamı dışlanmış değildir.

Mevcut clone binary cache açılışta gerçekten kullanılmıştır. Ancak bu sahnedeki sürekli düşük FPS'yi tek başına çözecek müdahale olarak görülmemelidir.

## Oturum ve yöntem

- Çalışan oyun: native Linux x86-64 ETS2, OpenGL -> GLES yolu.
- Aynı runtime PID'si `16802` bütün inceleme boyunca korundu. Oyun yeniden başlatılmadı, APK/kütüphane değiştirilmedi, eski sürüm denemesi yapılmadı ve oyun girdisi gönderilmedi.
- Yüklenen GL host SHA256: `96fece8b68d7496e60fb434189b685da52f2fc1f357becef6fb3abacd99fa6c7`.
- 20 saniyelik `simpleperf` CPU profili: `cpu-cycles:u`, 500 Hz, DWARF çağrı zinciri. 19.065 örnek; kayıp örnek yok.
- Ayrı 20 saniyelik Perfetto: scheduler, CPU/GPU frekansları, gfx/view atrace ve SurfaceFlinger veri kaynakları.
- Günlük başlangıç/son snapshot'ı arasında 54,5 saniye. Ekran görüntüsü oyunun gerçek kabin/şehir sahnesinde olduğunu doğruluyor.
- Kaynak/derleme/profil dizini: `logs/live-ets2-profile-20261022/`. İki profil aralığı birebir aynı değildir; CPU yüzdeleriyle duvar saati ölçümleri birbirine dönüştürülmedi.

## Kare süresi ve kritik thread

Perfetto'da 449 `eglSwapBuffers` olayı görüldü: yaklaşık 22,5 kare/saniye ve 44,5 ms/kare. Bu, swap/uygulama kare üretimi ölçümüdür; bağımsız fiziksel ekran FPS'i olarak sunulmuyor.

Ana GL/oyun thread'i `16830`:

| Durum | Yaklaşık 20 saniyelik izde toplam |
|---|---:|
| CPU'da çalışıyor | 16,183 s |
| Uyuyor/bekliyor | 3,343 s |
| Çalışmaya hazır, CPU sırası bekliyor (`R` + `R+`) | 0,375 s |
| Kesilemeyen bekleme (`D`) | 0,002 s |

Ana thread'in CPU çalışması, üretilen kare başına yaklaşık **36,0 ms**. Çalışmasının 14,312 saniyesi CPU 7 üzerinde geçti. Bu thread hem oyun/çizim hazırlığı hem native GL çağrılarını yürütüyor; bütün CPU süresine yalnız fizik/simülasyon veya yalnız FEX ek maliyeti demek doğru değildir.

İkinci yoğun guest thread `17002`, aynı izde 9,362 CPU-saniyesi çalıştı. Paralel thread süreleri toplanıp tek karenin duvar saati süresi olarak kullanılamaz.

## CPU çevrimleri nerede?

| Kod bölgesi | Örneklenen user-space CPU çevrimleri |
|---|---:|
| `[anon:FEXMemJIT]` içindeki çevrilmiş kod | %79,576 |
| Mali GLES sürücüsü | %12,55 |
| Bionic libc | %3,13 |
| `libvexa.so` içindeki native runtime/yardımcı kod | %1,83 |
| VEXA GL host köprüsünün kendi kodu | %1,13 |

`simpleperf` başlangıçta %80,48'i `unknown` diye gösterdi. Ham period değerleri ve süreç maps dosyasıyla adresler ayrıştırıldı: %79,576'sı gerçekten yürütülebilir FEX JIT bölgesinde, yaklaşık %0,90'ı başka çözümlenmemiş adreslerde. JIT bölgesinde çalışmak, yeniden JIT derlemesi yapmakla aynı şey değildir.

Bunlar CPU çevrimi paylarıdır; GPU kullanım yüzdesi veya kare süresi yüzdesi değildir. Guest JIT isimleri bu oturum başlatılırken etkin olmadığından, oyunu yeniden başlatmadan bu ölçümle belirli bir guest fonksiyon adı/kaynağı kesinleştirilmedi. En sıcak işin motor içindeki hangi fonksiyon olduğu henüz bilinmiyor.

## Sunum ve GPU beklemeleri

Ana thread üzerindeki ölçümler:

| İşlem | Ortalama | En uzun |
|---|---:|---:|
| `eglSwapBuffers` | 1,013 ms | 3,149 ms |
| `queueBuffer` | 0,718 ms | 2,755 ms |

`queueBuffer`, swap yolunun içindedir; süreleri toplanmaz. Yaklaşık 44,5 ms'lik kareyi açıklayan ana maliyet bu 1 ms'lik sunum çağrısı değildir.

GPU completion yardımcı thread'inde 447 bekleme ortalama 3,034 ms, en uzun 11,982 ms sürdü. GED yardımcı thread'indeki `Wait 3D Fence` ortalaması 3,695 ms. Bu olaylar **ana thread'in her kare bu kadar bloklandığı** veya **GPU'nun saf render süresinin bu olduğu** anlamına gelmez. İşler asenkron ve örtüşebilir.

Bu cihazda istenen bazı kernel GPU izleri kullanılamadı (`ftrace_setup_errors=37`). GPU donanım pass/timestamp ölçümü yok. SurfaceFlinger frame timeline içindeki bazı layer adları da boş geldi; başka katmanların olaylarından oyun sunum FPS'i çıkarılmadı. Ana thread çalışma süresi ve doğrudan uygulama swap dilimleri kullanılabilir kanıtlardır.

## Cache bu sahnede ne yapıyor?

- Mevcut oturumun başlangıcında 24 clone kurulum olayı var: **23 disk isabeti, 1 RAM isabeti**.
- Bu 24 olayın toplam `build_ms` süresi yaklaşık **108,5 ms**. Bu toplam yalnız binary okumasını değil, clone kurulumunu ve uniform eşlemelerini de kapsar.
- Sonraki **54,5 saniyelik canlı gözlemde sıfır yeni clone kurulum olayı** var.
- CPU profilinde `glCompileShader`, `glLinkProgram`, clone oluşturma veya shader çeviri fonksiyonlarında örnek yakalanmadı.
- Kapalı sürücünün içeride draw sırasında yaptığı olası özel derlemeleri, sembolsüz sürücü örneklerinden tamamen dışlamak mümkün değil. Buna rağmen VEXA clone cache'inin bu aralıkta tekrar tekrar derleme yaptığına dair kanıt yok.

Yaptığımız sistem bütün oyunun shader'larını yükleme sırasında önceden hazırlayan genel bir precache sistemi değildir. Onarım için gereken vertex clone'larının driver binary'lerini tekrar kullanır; ilk kullanımda yine program yükleme ve durum kurma işi vardır. Normal oyun shader/programları ve sürücünün kendi cache'i ayrı yollardır.

**Beklenen yarar:** İlk karşılaşma ve sonraki açılışlarda clone derleme takılmalarını azaltmak. **Bu oturumdaki sınır:** Sürekli çalışan çevrilmiş oyun kodu, draw hazırlığı veya GPU'da shader yürütme maliyetini kaldırmaz. Bu nedenle mevcut yaklaşık 22 FPS'nin ana çözümü olarak daha fazla clone cache genişletmesini seçmek için kanıt yok.

Önceki oturumlarda görülen 21 -> 29 FPS farkı tek başına cache kazancı olarak kabul edilmiyor. Güncel canlı profilde araştırmanın sonraki hedefi, ana thread'in çevrilmiş kodundaki sıcak fonksiyonları ve GL komut hazırlığını birbirinden ayırmak olmalı. FEX sürüm güncellemesi yapılmadı.

## Ek inceleme: CPU yükünün nedenleri ve sonraki müdahaleler

Aynı PID korunarak CPU 7 için `cpuinfo_max_freq=3400000` okundu; canlı `scaling_cur_freq` örnekleri 1,6–1,7 GHz idi. Önceki Perfetto izinde de CPU 7 en çok 1,7 GHz gördü. Bu, CPU'ya bağımlı iş için önemli bir performans koşuludur. Ancak 3,4 GHz donanım üst sınırı sürekli garanti edilen çalışma frekansı değildir; frekans farkından doğrudan iki kat FPS sonucu çıkarılamaz. `scaling_max_freq` okuması izin nedeniyle başarısız oldu; mevcut sınırlamanın termal/güç/üretici politikasından hangisi olduğu bu turda çözülmedi. Android'in Thermal Status 0 bildirmesi üretici sınırını dışlamaz.

Mevcut gdb socket'inden yalnız duraklatmayan `qVexaThreads` ve `m` paketleri kullanıldı. Açılan geçici adb port yönlendirmesi kaldırıldı; oyun yeniden başlatılmadı. Guest RIP snapshot'ları CPU zamanına göre ağırlıklı profil değildir ve host thunk içindeyken eski kalabilir; bunlardan yüzde/fonksiyon darboğazı çıkarılmadı.

Önceki CPU profilindeki sıcak host JIT adreslerinin ikisinden canlı kod okunup AArch64 disassembly alındı:

- `0x751c571438` ve `0x751c571450` çevresi: `fmul`, `fadd`, `faddp`, `trn`, `zip`, `tbl` gibi yoğun SIMD hesaplama/vektör yerleşimi işlemleri. Bunlar önbelleğe alınmış shader derlemesi değil, CPU üzerinde her kare tekrar yürütülen çevrilmiş koddur. Motor içindeki tam fonksiyon henüz adlandırılmadı.
- `0x751c04e558` çevresinde `ldapr` ve aynı blokta SVE `ld1d`/`sel` komutları var. Dolayısıyla bu oturum için “FEX LRCPC veya SVE kullanmıyor” iddiası doğru değildir. Hotspot'taki maliyetin esasen TSO veya split-lock olduğuna dair henüz kanıt yok.

Ham kod ve guest snapshot'ları: `logs/live-ets2-profile-20261022/jit-*.asm`, `guest-samples.json`. Guest executable dosya offset'i ile ELF sanal adresi ayrıdır: bu ETS2 binary'sinde executable PT_LOAD sanal adresi dosya offset'inden `0x400000` fazladır.

Ek `simpleperf stat` denemesinde bazı sayaçlar ve app-uid uyarıları tutarsız çıktı (branch sayısı branch-miss sayısından düşük). Bu denemeden dal tahmini/IPC/bellek darboğazı sonucu çıkarılmadı.

Önerilen sıra:

1. CPU frekansının 1,6–1,7 GHz'de kalmasının kaynağını ayır: üretici güç politikası, şarj/batarya/sıcaklık ve GL ana thread'i için performans ipuçlarının gerçek host durumunu incele. Güvenlik/termal korumaları kapatmadan ölç.
2. Sonraki kontrollü profil açılışında JIT isimlerini etkinleştirip en sıcak ARM bloklarını guest fonksiyonlara bağla. Vektör aritmetiği, gather/shuffle, bellek sıralaması ve guest döngülerini ayrı değerlendir; yalnız kanıtlanan emisyon/uyumluluk maliyetine küçük bir değişiklik uygula.
3. Yaklaşık 1.960–1.980 draw/kare görülen canlı sahnede draw hazırlığı ve geçiş maliyetini ölç. Mevcut multi-draw/base-instance optimizasyonlarını tekrar “eksik” sayma; kalan state sorgusu, uniform yayılımı ve paketleme işini hedefle.
4. Daha fazla shader cache veya FEX ana sürüm güncellemesini mevcut CPU darboğazının kanıtlanmış çözümü olarak sunma. Shader cache ilk-kullanım maliyetini azaltır; her kare yürütülen oyun matematiğini azaltmaz.

Bellek modeli için upstream açıklaması: https://fex-emu.com/Scourge-of-emulation/ . Genel mekanizma VEXA'da ölçülmüş darboğazla aynı şey değildir.

## 23 Ekim: JIT bloklarini guest modullere esleme (salt okunur)

Onceki profildeki host PC adresleri, ayni PID 16802 halen mevcutken yalniz RSP `m` ile okunan JIT pencerelerinde FEX JITCodeHeader/JITCodeTail verisi kullanilarak guest blok girislerine eslendi. Kaynak struct: FEXCore/Source/Interface/Core/CPUBackend.h. Header'in tail offset'i, tail boyutu ve guest RIP kaydi birlikte kontrol edildi. `map_profile_jit.py`, `mapped-hot-jit.json` ve `thread-address-periods.txt` kanit dosyalaridir. 46 pencere grubundan 5868 blok tanindi; tum JIT kodunun eksiksiz sembollendirilmesi degildir. Inceleme sirasinda oyun/config/kaynak kod degistirilmedi, oyun yeniden baslatilmadi. Sonraki kontrolde eski PID artik yoktu; yeni runtime surecleri goruldu. Yeni runtime icin bu yuzdeler guncel olcum sayilmaz.

Onemli duzeltme: en sicak SIMD host blogu 0x751c571438/450, guest 0x7849663540 adresli **libfmod.so.14** bloguna aittir. Diger sicak blok 0x751c04e59c/570, ayni kutuphanedeki guest 0x78496da0b0 blogudur. Dolayisiyla onceki genel 'oyun matematigi' yorumu bu bloklar icin ses DSP/hesaplama kodu olarak daraltilmalidir. Private fonksiyon adi bulunamadi; en yakin exported sembol bu fonksiyonun adi kabul edilmedi.

Toplam CPU cevrimi 34,575,398,364 uzerinden, eslenebilen FMOD ornekleri 8,690,103,057 (~%25,13), ETS2 executable ornekleri 12,374,794,408 (~%35,79). Bunlar tum surec CPU cevrimleridir; kare suresi veya FPS kaybi yuzdesi degildir. Eksik esleme nedeniyle alt sinir olarak okunmalidir.

Thread ayrimi: FMOD'un 7,913,906,007 cevrimi thread 17002'de (~%22,89 toplam). Ana thread 16830'da eslenmis ETS2 kodu 11,648,083,821 cevrim (~%33,69 toplam); eslenmis FMOD ornegi yok. Onceki Perfetto'da ses thread'i 20 saniyede 9,36 saniye CPU kullanirken ana thread 16,18 saniye kullaniyordu. Bu nedenle FMOD'un agir olmasi ana thread'in dogrudan ses bekledigi veya sesi kapatmanin FPS'yi artiracagi anlamina gelmez. Dolayli kaynak/paylasim etkisi ayrica olculmeden iddia edilemez.

Ana thread'deki eslenmis guest blok girisleri daginik: 0x1caaeb0 toplam cevrimlerin %1,43'u, 0x1b9d660 %1,31'i, 0x1e44d80 %1,07'si, 0x25073b0 %1,05'i. Bunlar tam fonksiyon kapsayici zamanlari degil, tek JIT bloguna atfedilen self orneklerdir. Ilk blogun x86 disassembly'si yogun shufps/mulps/addps/subps ve stack-temporary islemleri gosteriyor; kesin motor fonksiyon adi stripped binary'den belirlenemedi. Bu bulgular tek bir kilit/spin veya tek bir yardimci fonksiyonun ana CPU'yu yuttugunu gostermiyor.

GL callgraph: glDrawElementsBaseVertex cagrisi dahil toplam CPU cevrimlerinin %9,82'si; bu dalin %92,53'u Mali surucusune iniyor (~%9,09 toplam). Wrapper'in self payi %0,02. Bu CPU tarafinda draw teslim/driver maliyetidir; GPU shader yurutme suresi degildir. Driver agacinda memcpy var, fakat bundan tek basina vertex kopyasi veya shader derlemesi sonucu cikarilamaz.

Sonuc: mevcut profilin FPS kritik yolu ana thread'in CPU isidir; ses DSP'si ayri buyuk CPU tuketicisidir. 'FEX %80 => %80 FEX bug'i' yorumu yanlistir: JIT bolgesi oyunun ve ses kutuphanesinin faydali isini de icerir. Cache veya FEX surum degisiminin ana cozum oldugu kanitlanmadi. Tek bir duzeltilebilir kok hata henuz kanitlanmis degil; kesinlesen hedefler ana thread'deki cevrilmis oyun is yuku ve CPU draw-driver maliyetidir. Duzeltme/frekans/ses ayari A/B testi yapilmadi.
