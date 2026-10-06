# Waveshare 2.13" V4 — XIAO ESP32-C3

Lekki driver monochromatycznego panelu 250 × 122, SSD1680Z, Arduino / PlatformIO.

## Połączenie

Seeed ePaper Breakout Board for XIAO V2: RST D0, CS D1, BUSY D2, DC D3, SCK D8, MOSI D10. Piny są zebrane w `src/Epaper213.h`. Uwaga: dokumentacja **starszej** płytki Seeed podaje inne BUSY (D5). Seeed nie wymienia panelu Waveshare 250 × 122 na liście oficjalnie wspieranych paneli tej płytki: sprawdź zgodność taśmy FPC przed podłączeniem.

## Uruchomienie

`pio run -e seeed_xiao_esp32c3`; `pio run -e seeed_xiao_esp32c3 -t upload`. Logi Serial: 115200 baud. Bez Seeed GFX i bez dynamicznej alokacji.

Firmware jest zegarem HH:MM z nieruchomym dwukropkiem, pozycja (5,13), komórki 240 × 96 px. Konfiguracja: `src/ClockConfig.h`; przed wgraniem wpisz WIFI_SSID i WIFI_PASSWORD (domyślnie puste). Nie publikuj pliku z hasłem. `src/Clock.cpp` zawiera cykl pracy, `src/ClockLogic.h` testowalne decyzje, a `src/main.cpp` tylko wywołanie setupClock().

## Cykl zegara

Diagnostyka: `DEEP_SLEEP_ENABLED = false` jest obecnie ustawieniem domyślnym. ESP32 pozostaje aktywny, `loopClock()` wykonuje cykle bez ponownej inicjalizacji stanu zegara, a USB Serial pozostaje dostępny. Monitor: 115200 baud. Po Serial.begin() firmware czeka maksymalnie 5 s na monitor; co 10 s loguje `Clock alive`. Przy nieudanym NTP i niewiarygodnym czasie ponawia próbę po 60 s tylko w trybie bez deep sleep. Po poprawnej synchronizacji nadal obowiązuje 6 godzin. Panel jest usypiany między aktualizacjami także w diagnostyce. Ustaw `DEEP_SLEEP_ENABLED = true` przed testami bateryjnymi; diagnostyka nie jest energooszczędnym trybem produkcyjnym. `CLOCK_TEST_MODE` jest niezależny i pozostaje wyłączony: diagnozowane są rzeczywiste Wi-Fi i NTP.

- Zimny start/reset: Wi-Fi, NTP, pełne odświeżenie, deep sleep. Timeout Wi-Fi 15 s, NTP 10 s. Sukces oznacza nową odpowiedź SNTP, nie tylko poprawną datę z RTC.
- Timer wake: odczyt czasu systemowego, aktualizacja zmienionych cyfr, sen do następnej granicy minuty według gettimeofday(). Wi-Fi pozostaje wyłączone, poza terminami synchronizacji.
- Synchronizacja co 6 godzin według czasu absolutnego. Nieudana próba także wyznacza następny termin za 6 godzin: brak prób co minutę przy niedostępnej sieci. Poprawny dotychczasowy czas nie jest kasowany. Bez wiarygodnej daty (próg 2024-01-01 UTC) ekran pokazuje --:--.
- Polska: CET-1CEST,M3.5.0/2,M10.5.0/3, ustawiane po każdym wybudzeniu. Produkcja nie prowadzi własnego licznika minut; RTC utrzymuje czas systemowy. Korekta NTP zmieniająca widoczne HH:MM wymusza full refresh.
- Full refresh: reset, błąd bazy, zmiana godziny, zmiana poprawności czasu, upływ godziny, cofnięcie czasu lub limit 30 operacji partial. Liczniki i poprzedni obraz aktualizowane dopiero po sukcesie drivera.
- RAM RTC przechowuje terminy, poprzednią godzinę/minutę, licznik, flagi i znacznik wersji. Obiekt drivera, Wi-Fi i framebuffer są zwykłą RAM: framebuffer odtwarzany jest w całości z zachowanego opisu obrazu.
- Panel usypiany jest przed ESP32. Po wybudzeniu begin() resetuje kontroler; restoreBaseImage() zapisuje odtworzony poprzedni obraz do obu banków bez odświeżenia. Następnie framebuffer jest przerysowany na aktualny czas i odświeżane są zmienione komórki. Zapis obu banków nie jest full refresh.

## Testy i ograniczenia sprzętowe

`CLOCK_TEST_MODE = true`: symulowany czas startuje od 12:59 czasu polskiego 1 stycznia 2026, przesuwa się o minutę po każdym wybudzeniu; sen trwa 2 s, synchronizacja jest symulowana co 2 minuty i nie włącza Wi-Fi. Rzeczywisty czas cyklu obejmuje dodatkowo rozruch i waveform panelu. Produkcja domyślnie ma ten tryb wyłączony.

Test hostowy (Linux/WSL, g++): `g++ -std=c++11 test/clock_logic.cpp -o /tmp/clock_logic_test && /tmp/clock_logic_test`. Sprawdza harmonogram, próg daty, sen, full refresh i granice CET/CEST w 2026. Test hostowy używa libc hosta; nie zastępuje sprawdzenia newlib na ESP32.

Po wgraniu sprawdź: zimny start i NTP, brak Wi-Fi bez zawieszania, partial po uśpieniu/resetowaniu panelu, przejścia 12:39→12:40 i 12:59→13:00, limit partial oraz odzyskanie poprawnego czasu. Odtwarzanie bazowej RAM po sleep wymaga walidacji na konkretnym panelu V4. Jeśli panel jest fizycznie resetowany/odłączany niezależnie od ESP32, wykonaj reset ESP32, aby wymusić pełny obraz. Parametr 30 należy dobrać do panelu i temperatury; nie gwarantuje braku ghostingu. Czas pracy LiPo 1200 mAh wymaga pomiaru prądu całej płytki, regulatora i panelu. Wi-Fi i wewnętrzne komponenty frameworka mogą alokować pamięć; logika aplikacji nie dodaje własnych alokacji dynamicznych.

## API i pamięć

`begin()` inicjuje SPI i panel; `clear(true)` zmienia bufor na biały, `setPixel(x,y,false)` rysuje czarny piksel, `fillRect(x,y,w,h,false)` rysuje prostokąt. `fullRefresh()` oraz `partialRefresh(x,y,w,h)` zwracają `false` przy braku inicjalizacji, obrazu bazowego lub timeout BUSY. `sleep()` wymaga ponownego `begin()` przed kolejnym użyciem. Rysowanie zmienia wyłącznie lokalny bufor, a nie ekran.

## Tekst, polskie litery i liczby

`drawText(x, y, tekst, EpaperFont::Medium, white=false)` rysuje w buforze od lewego górnego rogu komórki. Fonty tekstowe obsługują UTF-8: ASCII, `°` oraz polskie `ĄĆĘŁŃÓŚŹŻąćęłńóśźż`; nieobsługiwane znaki są zastępowane `?`. ClockBold ma ograniczony zestaw opisany poniżej. Kod źródłowy zapisuj w UTF-8. `drawNumber()` ma analogiczny wybór fontu i obsługuje int32_t ze znakiem (ClockBold nie zawiera minusa). `white=true` rysuje białe litery, ale nie czyści tła ani odstępów. Tekst za krawędzią jest przycinany, bez automatycznego zawijania; `\n` przesuwa o wysokość fontu + 2 px.

| Font | Komórka | Odstęp kolejnych znaków |
|---|---|---|
| `EpaperFont::Small` | 6 × 12 px | 6 px |
| `EpaperFont::Medium` | 8 × 16 px | 8 px |
| `EpaperFont::Large` | 12 × 24 px | 12 px |
| `EpaperFont::MediumBold` | 8 × 16 px | 8 px |
| `EpaperFont::LargeBold` | 12 × 24 px | 12 px |
| `EpaperFont::ExtraLargeBold` | 16 × 32 px | 16 px |
| `EpaperFont::ClockBold` | 48 × 96 px | 48 px |

Każdy rozmiar ma osobną bitmapę, wspólną linię bazową i miejsce na polskie akcenty/ogonki. Cyfry są jednakowej szerokości. `fontHeight(font)` zwraca wysokość komórki. `textWidth(tekst, font)` zwraca szerokość układu (liczba znaków × odstęp, z końcowym odstępem); dla wielu wierszy zwraca najdłuższy, dla nullptr 0, przy przepełnieniu 65535. Liczy znaki UTF-8, nie bajty. Dotychczasowe wywołania bez fontu lub z liczbową skalą nadal działają, korzystając z nowego Small; wymiary różnią się od starego 8 × 12.

```cpp
epd.drawText(10, 10, u8"Zażółć gęślą jaźń");
epd.drawText(10, 30, u8"Łódź: 23°C");
epd.drawNumber(10, 55, -123);
epd.fullRefresh();

epd.drawText(8, 30, u8"Łódź: 23°C", EpaperFont::Medium);
const char *time = "12:45";
uint16_t x = (Epaper213::WIDTH - Epaper213::textWidth(time, EpaperFont::Large)) / 2;
epd.drawText(x, 60, time, EpaperFont::Large);

// Przy zmianie liczby czyścimy całe pole (włącznie z pozostałościami cyfr):
epd.fillRect(10, 55, 90, 13, true);
epd.drawNumber(10, 55, 42);
epd.partialRefresh(10, 55, 90, 13);
```

## Źródła i generowanie fontów

Sześć fontów tekstowych zawiera po 114 znaków, w tym wszystkie polskie litery i °. ClockBold zawiera tylko 13 znaków: spację, cyfry, dwukropek i ?. Inne znaki są zastępowane ?. Bitmapy zajmują 30744 B plus metadane w flash; bez dodatkowego framebufferu.

ClockBold powstaje przez dokładne powielenie pikseli 3× natywnego Terminus Bold 16×32, bez wygładzania i progowania TTF. HH:MM zajmuje 240×96 px komórek. Widoczne cyfry są mniejsze od komórek.

- src/fonts/EpaperFonts.h: jedyny nagłówek bitmap firmware.
- tools/font_sources/terminus/: oryginalne BDF i jedna licencja OFL.TXT.
- tools/generate_font.py: regenerowanie lokalnych bitmap.
- tools/font_preview.png: podgląd siedmiu wariantów.

Regenerowanie: python tools/generate_font.py. Internet nie jest potrzebny. Pillow jest opcjonalny, tylko do podglądu. Epaper Bitmap pochodzi z Terminus 4.49.1, copyright Dimitar Toshkov Zhekov, licencja SIL OFL 1.1 w tools/font_sources/terminus/OFL.TXT. Źródło: repozytorium mikebeaton/terminus-font-4.49.1 w GitHub. Usunięto stary Font8x12.h i kopię licencji; cache Pythona jest ignorowany.

```cpp
epd.clear();
epd.drawText(5, 13, "12:45", EpaperFont::ClockBold);
epd.fullRefresh();
```


Bufor: 250 rzędów kontrolera × 16 bajtów = 4000 B. Logiczne `x` odpowiada adresowi RAM Y (0..249), a `y` jest mapowane na piksel RAM X jako `121 - y`. Indeks bajtu to `ramX / 8`, maska `0x80 >> (ramX % 8)`. Odbicie dotyczy 122 widocznych pikseli, nie 128 bitów RAM: sześć niewidocznych bitów pozostaje białych. Okno partial `[y, yEnd)` jest najpierw przekształcane do RAM `[122 - yEnd, 122 - y)`, dopiero potem wyrównywane do bajtów. Wysyłane są **tylko bajty przecinające ten prostokąt** do 0x24, a po odświeżeniu również do 0x26 (poprzedni obraz). Wyzwolenie 0x22/0x20 oraz waveform V4 może mimo ograniczenia transferu do okna pobudzać także pozostałą część panelu. Kod nie używa komendy 0x91/0x90 ani nie twierdzi, że fizyczne skanowanie jest ograniczone do okna. Obraz bazowy jest wymagany przed pierwszą zmianą częściową. Korekta osi odpowiada zgłoszonemu pionowemu odbiciu znaków; sprawdź po wgraniu pozycje obu znaczników i działanie licznika.

Źródła: specyfikacja Waveshare V4 (SSD1680Z), Waveshare `epd2in13_V4.py` / `EPD_2in13_V4.c`, wiki Seeed `xiao_eink_expansion_board_v2`. Driver V4 producenta wysyła całe 4000 B także w `displayPartial`; tutejszy kod ogranicza transfer, lecz zgodność efektu fizycznego trzeba sprawdzić eksperymentalnie.