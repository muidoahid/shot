# shot

PrintScreen tuşuna bağlı, arkaplanda çalışan minik ekran görüntüsü aracı (~6 KB, C++/Win32, CRT'siz).
Simgesi, penceresi ve menüsü yoktur; kısayol tuşlarını dinler.

| Komut | Açıklama |
|---|---|
| `shot` | Arkaplanda başlatır, kısayol bekler |
| `shot -startup` | Windows ile birlikte başlayacak şekilde kaydeder (`HKCU\...\Run`) ve başlatır |
| `shot -nostartup` | Otomatik başlatma kaydını siler |
| `shot -exit` | Çalışan kopyayı kapatır |
| `shot -f` | Tam ekran (tüm monitörler) — **PrintScreen** |
| `shot -w` | Aktif pencere — **Shift + PrintScreen** |
| `shot -h` / `-help` | Komut listesi |

Her görüntü panoya kopyalanır ve `Resimler\shot_YYYYMMDD_HHMMSS_mmm.jpg` olarak (JPG, kalite %50) kaydedilir.

## Derleme

Visual Studio (MSVC) gerekir:

```
build.bat        :: 32 bit (varsayılan, en küçük)
build.bat x64    :: 64 bit
```

Not: Başka bir program PrintScreen'i tutuyorsa (ör. Ayarlar'da "PrtScn ile ekran alıntısı aç") kısayol çalışmayabilir.
Görev Yöneticisi'nde "Arka plan işlemleri" altında `shot.exe` olarak görünür.
