# Voidclank – Bass Annihilator (VST3)

Wtyczka basowa do metalu: tor równoległy (czysty dół + przesterowana góra), przester w stylu TSE BOD ⇄ Darkglass, korekcja „clank", 7 presetów, miernik, trial 14 dni.

## Tor sygnału

```
IN → Input → Tight (HPF) → crossover LR4 (domyślnie 200 Hz)
  ├─ LOW  : czysty dół + kompresor (auto-makeup) ───────────────┐
  ├─ HIGH : Mid Scoop → drive 4x OS (BOD ⇄ Darkglass)           │
  │         → HPF (usuwa mulenie dołu i DC) → Clank EQ          ├→ Output → limiter
  │         → Cab/Fizz LPF → Grit Level ────────────────────────┤
  └─ DRY  : czysty sygnał (fazowo spójny) ──────────────────────┘
```

## Presety
Init, Djent, Metalcore, Deathcore, Death Metal, Thrash Metal, Extreme Metal (`Source/PluginProcessor.cpp`, tabela `presets`).

## Trial i licencja (offline)
* 14 dni od pierwszego uruchomienia; po tym czasie wyjście jest wyciszone, a okno pokazuje panel aktywacji.
* Data startu jest w 2 plikach z sumą kontrolną; cofnięcie zegara systemu = koniec triala.
* Kod licencji = podpis RSA adresu e-mail kupującego. We wtyczce jest tylko klucz **publiczny** (`Source/LicensePublicKey.h`).

### Przed sprzedażą – WYMIEŃ klucz!
```bash
cmake -B build -DVC_BUILD_TOOLS=ON
cmake --build build --target VoidclankKeygen
./build/VoidclankKeygen_artefacts/Release/VoidclankKeygen gen        # tworzy private.key + public.txt
# wklej zawartość public.txt do Source/LicensePublicKey.h i przebuduj wtyczkę
./VoidclankKeygen sign private.key kupujacy@example.com             # kod licencji dla klienta
```
`private.key` trzymaj w sekrecie (nie w repozytorium!). Klucz w paczce jest tylko deweloperski.

> Zabezpieczenie offline zatrzyma zwykłe kopiowanie, ale nie zdeterminowanego crackera. Mocniejsza ochrona = iLok (PACE) lub aktywacja online – `LicenseManager` można później podmienić.

## Budowanie
Najprościej: wrzuć folder na GitHub – workflow `.github/workflows/build.yml` zbuduje VST3 dla Windows/macOS/Linux (**Actions → Artifacts**).

Lokalnie (CMake ≥ 3.22, C++17):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Wynik: `build/Voidclank_artefacts/Release/VST3/Voidclank.vst3` (+ Standalone).

Testy (DSP, presety, trial, licencja) i zrzut GUI:
```bash
cmake -B build -DVC_BUILD_TOOLS=ON && cmake --build build --target VoidclankTest
./build/VoidclankTest_artefacts/Release/VoidclankTest
./build/VoidclankTest_artefacts/Release/VoidclankTest --snapshot gui.png
```

## Przed wydaniem
Podpis kodu (Windows) i podpis + notaryzacja (macOS), licencja JUCE odpowiednia do przychodu, własny klucz licencyjny, adres sklepu w `License.h` (`purchaseUrl`).


## v1.1 additions

* **Solo** buttons for LOW / GRIT / DRY paths.
* **6 amp models** (Modern Ether, Metalcore Edge, Thrash Steel, Raw Grind, Slam Abyss, Crypt Death) – each with its own
  tone stack voicing and its own skin/artwork. Names are descriptive on purpose; the models are *voiced towards*
  Spiritbox / Architects / Metallica / Slayer / Distant-style tones, they are not emulations of any band's rig.
* **Cabinet**: 4 built-in cabs + **User IR loader** (WAV/AIFF/FLAC, max 4 MB / 6 s, load button or drag & drop,
  stored inside the session/preset). Cab can sit on the GRIT path only or on the full mix.
* **FX page**: noise gate, pitch/retune (Drop D, Drop C, octave down, +-12 st, fine cents; pitch-tracked
  crossfade tuned for mono bass), wah (pedal / auto-LFO / envelope), tempo-synced delay, reverb, **chromatic tuner**
  (with output mute).
* **Latency**: Quality switch - *Live 0 ms* (no oversampling, zero added latency), *Standard 2x* (4 samples),
  *HQ 4x* (60 samples). The pitch shifter adds its own ~40 ms delay only while active.
* Status bar shows sample rate / buffer / latency. In the **Standalone** app it shows the audio device and has an
  AUDIO button (device + buffer size). Inside a DAW the plugin cannot choose the audio device - that is done by the host.
* Fixed: the safety limiter no longer adds gain (the JUCE limiter used before boosted by ~4 dB).

## v1.2 additions

* **Language menu** (top right): Polski, English, Español, Italiano, Deutsch, 日本語, 中文. Remembered per user
  (`Fomardo/Voidclank/lang.txt`). Translated: section titles, knob labels, buttons, status bar, tuner, mic notes,
  activation dialog. Kept in English: amp descriptions, parameter/choice names (cab, mic, preset names).
* **Friends / tester build** (Windows): `cmake -B build -DVC_FRIENDS_BUILD=ON` or run the workflow
  *Build FRIENDS (Windows, full, no trial)*. No trial, never muted, shows a PL/EN "thank you / personal use only,
  please do not copy" notice on every open. **Do not distribute widely – it has no copy protection.**
* **GUI repaint**: the editor no longer repaints the whole window 30x per second; only the meter, tuner, CAB view and
  a thin scan-line strip. Nothing is drawn while the editor is hidden.
* **CPU benchmark**: `VoidclankTest --bench` (build with `-DCMAKE_BUILD_TYPE=Release -DVC_BUILD_TOOLS=ON`).
* **NAM prototype** (optional, off by default): `-DVC_WITH_NAM=ON` adds NeuralAmpModelerCore (MIT, pinned commit; handles
  .nam WaveNet/LSTM incl. A2 models) as an alternative GRIT amp stage: `NAM ON` + `LOAD NAM...` on the MAIN page,
  the DRIVE knob becomes the model input level. Needs Eigen 3.4 (found via `find_package`, else downloaded).
  Models trained at a different rate than the host are only accepted if the model supports arbitrary rates.
  Test: `VoidclankTest --nam model.nam`.
