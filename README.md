<div align="center">

# 🧠 8-Bit CPU Visualizer — ESP32-C3 + GC9A01

**A tiny 5-instruction CPU that runs in slow motion on a 1.28" round LCD, drawn seven different ways — block diagram, datapath, FSM, waveform, pipeline, debugger and gauges.**

![MCU](https://img.shields.io/badge/MCU-ESP32--C3-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![Display](https://img.shields.io/badge/Display-GC9A01%20240%C3%97240-00e5ff?style=for-the-badge)
![Lang](https://img.shields.io/badge/Arduino-C%2B%2B-00599C?style=for-the-badge&logo=arduino&logoColor=white)
![Pages](https://img.shields.io/badge/Views-7-0f4c5c?style=for-the-badge)

</div>

---

## 📖 What this is

A teaching toy. It simulates an **8-bit accumulator CPU** at roughly one instruction per second and renders what the machine is doing onto a round display. One button cycles through seven views of the *same* running program, so you can watch a single instruction as a block diagram, then as a datapath, then as a state machine, then as a waveform.

Nothing is pre-recorded — every view is driven live by the same simulated registers.

---

## 🧮 The CPU

Five instructions, one accumulator (`A`), one operand register (`B`), a program counter and an output latch.

| Opcode | Mnemonic | Micro-operation |
|---|---|---|
| `1` | `LDA n` | `A ← n` |
| `2` | `LDB n` | `B ← n` |
| `3` | `ADD` | `A ← A + B` |
| `4` | `OUT` | `OUT ← A` |
| `5` | `JMP n` | `PC ← n` |

**Instruction format** — one byte, split into nibbles:

```
 7   6   5   4   3   2   1   0
┌───────────────┬───────────────┐
│    opcode     │    operand    │
└───────────────┴───────────────┘
```

### The stored program

| Addr | Byte | Instruction | Effect |
|---|---|---|---|
| 0 | `0x15` | `LDA 5` | `A ← 5` |
| 1 | `0x23` | `LDB 3` | `B ← 3` |
| 2 | `0x30` | `ADD` | `A ← A + B` |
| 3 | `0x40` | `OUT` | `OUT ← A` |
| 4 | `0x52` | `JMP 2` | `PC ← 2` |

The jump makes addresses 2–4 an infinite loop, so after the two loads the accumulator climbs by `B` every pass: **OUT shows 8, 11, 14, 17, 20 …**, wrapping at 256 because the registers are 8-bit. It is a counter that counts by three — deliberately boring, so the *visualisation* is the interesting part.

### Timing

Each instruction runs as **three phases of 320 ms** — `FETCH` → `EXECUTE` → `PC+1` — so one instruction takes a little under a second. The simulated clock toggles every 80 ms, giving four clock edges per phase, and the last 36 samples are kept in a history buffer for the waveform view.

---

## 📺 The seven views

Press the button to cycle. The dots at the bottom of the screen show which page you are on.

| # | View | What it shows |
|---|---|---|
| 0 | **Block diagram** | RAM, control unit, PC, ALU, IR and OUT as boxes; address and data buses light up green as they are used, and the clock square wave runs along the top |
| 1 | **Datapath** | A single-cycle RISC-style datapath — PC, `+4`, instruction memory, register file, immediate generator, ALU, output latch — with the live control-signal table (`IMM`, `WE`, `B`, `ALU`) along the bottom |
| 2 | **FSM** | The three phases as nodes on a circle, with arcs between them and a dot travelling along the active arc in real time |
| 3 | **Waveform** | A logic-analyser strip: `CLK`, the three phase signals as square waves, and `PC`, `A`, `OUT` as bus traces with values printed at each change |
| 4 | **Pipeline** | A 5-stage `IF/ID/EX/MEM/WB` diagram, cycle counter, and the flush that a taken `JMP` causes, with a running bubble count |
| 5 | **Debugger** | Memory listing with the current `PC` row highlighted, register file with the register being written highlighted, and a scrolling three-line instruction log |
| 6 | **Gauges** | A large `OUT` readout with three concentric arc gauges for `A`, `B` and `PC` |

> **Note on the pipeline view.** The simulated CPU is a plain three-phase sequencer, not a pipelined machine. Page 4 runs its own independent model over the same program to illustrate what a pipelined version *would* do, including the two-instruction flush on a taken branch. It is a demonstration, not a readout of the executing core.

---

## 🔌 Hardware

- **Seeed XIAO ESP32-C3 / ESP32-C3 Super Mini**
- **GC9A01** 1.28" 240×240 round IPS LCD, SPI
- One momentary push button

| Display pin | ESP32-C3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| RST | GPIO 0 |
| CS | GPIO 1 |
| DC | GPIO 10 |
| SDA (MOSI) | GPIO 3 |
| SCL (SCK) | GPIO 4 |

| Other | ESP32-C3 |
|---|---|
| Page button | GPIO 9 → GND |

Power the display from **3V3, not 5V**. If the module has a `BL` pin, tie it to 3V3. SPI runs at 40 MHz, so keep the wires short and firmly seated.

> ⚠️ **GPIO 9 is the BOOT strapping pin** on the ESP32-C3. Using it for the page button means no extra hardware — the onboard BOOT button works directly — but holding it down while the board resets puts the chip into download mode instead of running the sketch. Move the button to another free pin if that gets in your way.

---

## ▶️ Building

Needs the **[GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX)** by moononournation.

**Arduino IDE**

1. Install the `esp32` board package by Espressif through Boards Manager.
2. Install `GFX Library for Arduino` through Library Manager.
3. Put `cpu_visualizer.ino` in a folder named `cpu_visualizer` (the IDE requires the sketch and its folder to share a name).
4. Board: **ESP32C3 Dev Module**, with **USB CDC On Boot: Enabled**.

**PlatformIO**

```ini
[env:esp32c3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
lib_deps = moononournation/GFX Library for Arduino
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

The current GFX library needs Arduino-ESP32 core 3.x. The stock `espressif32` platform ships core 2.x and fails with `esp32-hal-periman.h` / `esp_private/periph_ctrl.h` not found — use the pioarduino platform above, or pin the library to an older release such as `@1.4.9`.

---

## 🎨 Changing the program

The program lives in four arrays near the top of the sketch:

```cpp
const char *nm[]    = {"LDA", "LDB", "ADD", "OUT", "JMP"};
const char *micro[] = {"A = RAM", "B = RAM", "A = A + B", "OUT = A", "PC = ADDR"};
const char *lf[]    = {"%d: A <- %d", ...};   // debugger log format
const uint8_t arg[] = {5, 3, 0, 0, 2};        // operand per address
```

Edit `arg[]` to change the operands — set `arg[1]` to `7` and the counter steps by seven instead of three. Adding an instruction means extending `nm`, `micro`, `lf` and `arg`, bumping `N`, and adding a case to the `switch` in `loop()`.

Phase length is the `wait(320)` calls in `loop()`; the clock period is the `ms / 80` divisor inside `wait()`.

---

<div align="center">

Built by [**Fahim Islam**](https://github.com/fahim-islam15) · EEE @ AIUB

</div>
