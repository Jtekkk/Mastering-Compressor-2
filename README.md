# MC-2 — Twin-Tube Vari-Mu Mastering Compressor

A mastering-grade variable-mu (variable-gain tube) compressor/limiter plugin in the
classic twin-tube tradition, built with [JUCE](https://juce.com). Ships as **VST3**
(plus Standalone, and AU when built on macOS).

![MC-2 front panel](docs/mc2-frontpanel.png)

## Feature map

Every line of the hardware spec sheet and where it lives in the plugin:

| Hardware spec | Implementation |
| --- | --- |
| Balanced inputs and outputs | Input/output line-stage trims (±12 dB, 0.5 dB detents), output iron 5 Hz LF behaviour, flat balanced response |
| Variable-gain vacuum tube per channel: 5670 | The gain element *is* the modelled 5670 stage: the control voltage re-biases the triode, and 2nd-harmonic content blooms with gain reduction (`Source/DSP/TubeStage.h`) |
| Hard-wire bypass switch | `HARD-WIRE` paddle (also exposed as the host bypass parameter); the programme never touches the circuit |
| Recovery, 5 steps | 0.2 s / 0.4 s / 0.6 s / 4 s / 8 s rotary switch |
| Variable attack 25–70 ms | Detented attack knob, 5 ms steps |
| Limit or Compress modes | `MODE` paddle. COMPRESS = gentle 1.5:1, 6 dB knee. LIMIT = feedback ratio that stiffens from 4:1 toward 20:1 as you push into it |
| Stereo link switch | `ST LINK` paddle — sums the two control voltages like the hardware; unlinked, each channel rides its own sidechain |
| Front-panel meter calibration | `CAL L` / `CAL R` trims, ±3 dB in 0.25 dB detents |
| Large illuminated Sifam meters | Two vector-drawn, lamp-lit VU meters with true logarithmic dial geometry and 300 ms ballistics; switchable GR / output, 0 VU = −18 dBFS |
| Twin-tube design | Two cascaded triode stages per channel (input triode + 5670 mu stage), each normalised for unity gain so colour and gain stay independent |
| Six rectifier circuits | `RECTIFIER` switch: Tube FW, Tube HW, Germanium, Silicon, Opto, RMS — each with its own detection law and ballistic scaling (`Source/DSP/Rectifiers.h`) |
| Excellent sonic range, low noise | Whole path runs 2× oversampled (linear-phase halfbands, latency reported), double-precision filters, no added noise |
| Sidechain EQ | 4 built-in curves: FLAT, HP 100 Hz, HP 200 Hz + presence, HF lift 5 kHz |
| Sweet passive EQ | Boost-only, broad low-Q shelves: LOW +0…6 dB @ 90 Hz, AIR +0…6 dB @ 12 kHz, with an in/out paddle |
| All controls switches or detented knobs | Every parameter is stepped — settings are exactly repeatable |
| High 120 V operating voltage | +24 dBFS soft rail ceiling: enormous headroom, gentle onset |
| Extremely musical, lively sound | Feedback topology + programme-dependent mu-stage harmonics — the compression curve and the tone are one circuit, as in the hardware |

## Architecture

```
in ──> input trim ──> [ input triode ] ──> [ 5670 mu stage ] ──┬──> passive EQ ──> output trim ──> output iron ──> rails ──> out
                                              ▲                │
                                              │ CV             ▼ (feedback tap)
                                       attack/recovery <── rectifier (×6) <── sidechain EQ (×4)
                                              ▲
                                              └── stereo link (CV sum)
```

* **Feedback detection** — the sidechain listens to the mu-stage *output*. With a
  proportional control law `GR = k · overshoot`, the input-referred ratio is exactly
  `1 + k`, which is how the front-panel ratios are realised (k = 0.5 for 1.5:1;
  k = 3→19 for 4:1→20:1).
* The DSP core (`Source/DSP/`) is dependency-free C++17 — it compiles and runs
  without JUCE, which is how the test harness drives it.
* The GUI is 100 % vector-drawn (no image assets): `Source/GUI/`.

## Building

Requires CMake ≥ 3.22 and a C++17 compiler. JUCE 8 is fetched automatically.

```bash
# Linux: install JUCE's usual dev packages first
sudo apt install libasound2-dev libx11-dev libxext-dev libxinerama-dev \
                 libxrandr-dev libxcursor-dev libfreetype-dev libfontconfig1-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MC2_VST3 -j$(nproc)
```

The VST3 lands in `build/MC2_artefacts/Release/VST3/`. Other targets:
`MC2_Standalone`, and `MC2_AU` on macOS.

### DSP verification

A headless test harness measures the engine against the spec sheet — ratios,
attack/recovery times, all six rectifiers, sidechain curves, link behaviour,
passive EQ and tube harmonics:

```bash
cmake --build build --target dsp_smoke && ./build/dsp_smoke
```

Sample of what it verifies on this build: COMPRESS measures 1.52:1, LIMIT sits at
9.5:1 mid-drive, attack-to-63 % GR is 24 ms / 60 ms at the 25/70 ms settings, and
the 2nd harmonic rises with gain reduction exactly as a re-biased mu stage should.

## Controls

| Control | Range | Notes |
| --- | --- | --- |
| INPUT / OUTPUT | ±12 dB, 0.5 dB steps | Balanced line stage trims; no auto make-up — ride them like hardware |
| THRESHOLD | −40…0 dB, 0.5 dB steps | Detector calibrated so the legend matches sine practice |
| ATTACK | 25–70 ms, 5 ms steps | CV charge time |
| RECOVERY | 0.2 / 0.4 / 0.6 / 4 / 8 s | CV discharge time |
| MODE | COMPRESS / LIMIT | 1.5:1 vs 4:1→20:1 programme-dependent |
| RECTIFIER | FW / HW / GE / SI / OPTO / RMS | Six detection circuits, six personalities |
| SC EQ | FLAT / HP100 / HP200+presence / HF lift | Shapes what the compressor hears |
| ST LINK | LINK / DUAL | CV summing |
| PASSIVE EQ | LOW 90 Hz, AIR 12 kHz, 0…+6 dB + in/out | Boost-only programme polish |
| METER / CAL | GR or OUTPUT, ±3 dB trims | 0 VU = −18 dBFS (sine) |
| HARD-WIRE | BYPASS / OPERATE | True bypass, host-visible |

## Licensing

This project uses JUCE 8 under its open-source license terms, which require that
this source be distributed under **GPLv3** (see `LICENSE`). If you hold a JUCE
commercial license you may relicense your own builds accordingly.
