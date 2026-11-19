# 44 — Console settings: front LED, fan and in-game reset buttons

OXOS addition (proposal 2026-11-19, built the same day; on a console: not yet). Code: `shared/ini_file/settings_ini.cpp` (`ReadConsoleSettings`, host unit
tests in `tests/unit/test_settings_ini.cpp`), `kernel/modules/kernel_settings.cpp` (`ApplyConsoleSettings`), `kernel/src/in_game_reset/in_game_reset.cpp`.

Three console options that today are fixed in the kernel, made settings in `C:\OXOS\oxos_settings.ini` (spec 39 §3: the one file for the kernel's own
options). Each keeps today's behaviour when its key, the file or the kernel modules (spec 43) are missing, and in safe mode (B held at boot, spec 34 §3.3).

## 1. The file
```ini
[console]
; The front LED once a title (the dashboard or a game) starts: 4 steps of its cycle, each G green, R red, A amber or - off. GGGG: solid green (as shipped).
front_led = GGGG
; The fan: a speed from 20 to 100 (percent), or auto (the console's own control). Missing: OXOS leaves the fan as it is (a dashboard may set it).
fan_speed = auto

[in_game_reset]
; Buttons held together in a game, at least 3, from: UP DOWN LEFT RIGHT START BACK LS RS A B X Y BLACK WHITE LT RT; none: off.
; reset_buttons: a power cycle, the console boots again (as shipped: LT RT BACK BLACK).
reset_buttons = LT RT BACK BLACK
; power_off_buttons: the console turns off. It must not share all its buttons with reset_buttons, nor they with it.
power_off_buttons = none
```
- Keys and button names are read in any case; buttons are separated by blanks or `+` (`LT+RT+BACK+BLACK`). A key given twice: the last one counts.
- A value OXOS cannot use (a 5-character `front_led`, `fan_speed = 5`, a 2-button combination, an unknown button, a power-off combination that overlaps the
  reset's) keeps that option's default and counts as a line not understood (the serial log says how many).

## 2. Front LED
The SMC drives the LED from an 8-bit pattern, one bit per quarter of a repeating cycle: bits 3–0 green, bits 7–4 red, both lit for amber (SMC register 0x08,
then 0x07 ← 1 to use it; `hardware::chipset::SetBootLed`, spec 03 §6). Character i of `front_led` sets bit i (green) and/or bit i + 4 (red).

| `front_led` | Pattern | Looks |
|---|---|---|
| `GGGG` | 0x0F | solid green (the default, as before this spec) |
| `RRRR` | 0xF0 | solid red |
| `AAAA` | 0xFF | solid amber |
| `GRGR` | 0xA5 | flashing green and red |
| `G---` | 0x01 | a short green blink |
| `----` | 0x00 | off |

The kernel's boot status (flashing green, 0x0A) and the error screens' solid green are unchanged; the setting is the pattern written as a title starts
(`titles::TitleThread`), so it is read at every launch, quick reboots included.

## 3. Fan
- A speed: SMC register 0x05 ← 1 (manual), then 0x06 ← speed / 2 (the SMC's scale is 0–50), at every launch. The lowest accepted speed is 20 %: a console
  with its fan near standstill overheats in a game.
- `auto`: 0x05 ← 0, the SMC's own temperature control, at every launch.
- Missing (the shipped file): OXOS writes nothing, so a dashboard's fan setting (EvoX's, XBMC's) keeps working. The SMC keeps a manual speed until it is
  told otherwise or loses power, so going back from a speed to the console's control needs `fan_speed = auto`, not removing the key.
- The writes retry up to five times each, as the in-game reset's (spec 22 §6).

## 4. In-game reset buttons
- The button mask is spec 22 §3's: bits 0–7 the digital buttons (UP 0x01, DOWN 0x02, LEFT 0x04, RIGHT 0x08, START 0x10, BACK 0x20, LS 0x40, RS 0x80), bit
  8 + i analog button i held fully down (A B X Y BLACK WHITE LT RT). A combination fires when every one of its bits is set.
- `reset_buttons`: the power cycle of spec 22 §6. `none` turns the in-game reset off.
- `power_off_buttons`: SMC register 0x02 ← 0x80 (power off), up to five tries; then the kernel waits for the power to go. Off unless set.
- At least 3 buttons: one or two held in normal play would end the game. The two combinations must each have a button the other does not: a player holding
  one passes through the other on the way.
- The kernel keeps the two masks (16 bits each) resident: `HalpIgrCheck` runs inside the title (spec 22 §2). They are set at every launch by the settings
  module, and are back at their defaults (0xD020 and none) when it cannot run.

## 5. Not in this spec
- Which controller port may press the combinations (Cerbios's IGRMASTERPORT): `HalpIgrCheck` sees only the state, not the device; to add.
- The in-game reset as a quick reboot to the dashboard: spec 22 §8, untested on hardware.
- Video options (force 480p, VGA, flicker filter): the EEPROM's video settings and spec 02; OXOS Setup's Video page (spec 39).

## 6. Tests
- Host: `tests/unit/test_settings_ini.cpp` (every key, its defaults, the rejected values, the overlap rule).
- xemu (`make -C kernel test_dash`): the serial log's `settings: console: ...` line with the shipped file; `make test_memory`: no page lost.
- Console: each `front_led` of §2's table; `fan_speed = 100` audible, then `auto`; `power_off_buttons = LT RT START` in a game; `reset_buttons = none` and
  the reset combination does nothing.
