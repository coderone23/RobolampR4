# "Smart" RoboLamp R4

Custom firmware for the **RoboLamp**: an animated desk lamp with a moving head, an adjustable height, and color and white lighting.

This project starts from CrunchLabs' stock RoboLamp firmware (v1.1.0) and makes three main changes:

1. **Runs on an Arduino Nano R4** (Renesas RA4M1) instead of the original Arduino Nano (due to flash memory being too small).
2. **Moves servo control off the main board.** An I2C servo driver now moves the head, so the main board no longer generates servo PWM itself.
3. **Adds Home Assistant control** through a **Seeed XIAO ESP32-S3** running ESPHome. The lamp appears in Home Assistant as a dimmable RGB + tunable-white light with preset buttons. Changes made on the lamp itself show up in Home Assistant too.

Everything from the stock firmware still works: joystick control, presets, moods, the color menu and the smiley face.

---

## How it fits together

```
Home Assistant
      ▲   │   Wi-Fi (ESPHome native API)
      │   ▼
Seeed XIAO ESP32-S3 ──── runs esp32s3/lamp.yaml (ESPHome)
      │   ▲     └──▶ yaw + pitch servos (via the xiao_servo_bridge component)
      │   │
 UART │   │ I2C: Nano's Qwiic port → XIAO @ 0x08
 text │   │  · servo pulse widths
 cmds │   │  · light status (power, mode, hue, brightness, tone) every 250 ms
      ▼   │
Arduino Nano R4 ──── runs src/main.cpp (PlatformIO)
      ├── NeoPixel ring (12 px) + warm/cool white LEDs
      ├── height motor + height potentiometer
      └── joystick, 4 preset buttons, up/down buttons
```

---

## What changed from the original firmware

| Area | Original | This repo |
|---|---|---|
| Main board | AVR Arduino Nano | **Arduino Nano R4** (`env:nano_r4` in `platformio.ini`) |
| Servo output | `Adafruit_TiCoServo` driving pins D10 (yaw) and D9 (pitch). It is AVR-only and won't build on the R4. | Servo abstraction with two back-ends: **`I2CServo`** (default) sends pulse widths to an I2C device at `0x08` on the Qwiic bus. **`MyServo`** drives a PCA9685 board instead. Yaw = channel 0, pitch = channel 1. |
| Pitch range | 10°–170° | 20°–160° (`Config.h`) |
| Serial control | Long-form commands on USB serial only | Short commands accepted on **both USB serial and `Serial1`** (from the ESP32): `lof`, `bri:`, `hue:`, `tones:`, `p1`–`p4`, `setp1`–`setp4` |
| Smart-home | — | ESPHome config for a Seeed XIAO ESP32-S3 that bridges Home Assistant and the lamp in both directions |
| State reporting | — | The lamp sends power, mode, hue, brightness and white tone over I2C every 250 ms, so Home Assistant shows changes made on the lamp |
| Lights off | — | Brightness can go to 0 (an "off" level was added), and the smiley face stays dark while the lights are off |
| Debug logging | Many `Serial.print` calls commented out | Routed through `DEBUG_PRINT` / `DEBUG_PRINTLN` macros, which `#define __DEBUG__` in `main.cpp` turns on or off |

The animation, mood, preset and lighting logic otherwise matches the stock firmware.

---

## Hardware

- CrunchLabs RoboLamp kit
- **Arduino Nano R4** (replaces the stock controller)
- **Seeed XIAO ESP32-S3**: the Home Assistant bridge. In the default build it's also the servo driver (an I2C peripheral at `0x08`; see the [I2C protocol](#i2c-protocol)).

### Nano R4 pin map

| Function | Pin |
|---|---|
| Preset buttons 1 / 2 / 3 / 4 | D8 / D7 / D2 / D4 |
| Height **down** / **up** buttons | A1 / A2 |
| Joystick button | A3 |
| Joystick yaw / pitch axes | A6 / A7 |
| Height sensor (potentiometer) | A0 |
| NeoPixel ring data (12 px) | D3 |
| Warm / cool white LED (PWM) | D5 / D6 |
| Height motor speed (PWM) / direction | D11 / D12 |
| ESP32 link (`Serial1`) | D0 (RX) / D1 (TX) |
| I2C servo driver (`Wire1`) | Qwiic connector | 

### XIAO ESP32-S3 ↔ Nano R4 wiring

| XIAO ESP32-S3 | Nano R4 | Used for |
|---|---|---|
| GPIO9 (TX) | D0 (RX) | UART commands to the Nano |
| GPIO44 (RX) | D1 (TX) | UART from the Nano (serial proxy) |
| I2C SDA / SCL (set in the `xiao_servo_bridge` component) | Qwiic SDA / SCL | Servo positions + light status |
| GND | GND | |

> ⚠️ The XIAO uses 3.3 V logic. The R4 Nano's Qwiic port is 3.3 V, so everything should be fine, just make sure you are not sending 5V to the XIAO through TX, because that could fry the chip (the servos work fine on 3.3V)
---

## Repository layout

```
├── platformio.ini            PlatformIO build config for the Nano R4
├── src/
│   ├── main.cpp              Main lamp firmware (motion, lighting, moods, presets, serial, I2C)
│   ├── I2CServo.cpp          Servo back-end: sends pulse widths to the I2C device at 0x08
│   ├── MyServo.cpp           Servo back-end: drives a PCA9685
│   └── LIGHT_anim_planner_injector_02112026.js   CrunchLabs browser tool
├── include/
│   ├── Config.h              User-tunable settings (startup mood, colors, speeds, limits…)
│   ├── Animations.h          Keyframe animations stored in PROGMEM
│   ├── Types.h               Shared enums and the Keyframe struct
│   ├── I2CServo.h
│   └── MyServo.h
└── esp32s3/                  ESPHome files
    ├── lamp.yaml             ESPHome config: Home Assistant bridge + servo driver
    └── i2c_servo_slave.h     Earlier standalone prototype of the I2C servo driver (not used by lamp.yaml)
```

---

## Building and flashing the Nano R4

Requires [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

```bash
git clone https://github.com/coderone23/RobolampR4.git
cd RobolampR4

# Build and upload
pio run -e nano_r4 -t upload

# Open the serial monitor
pio device monitor -b 115200
```

The first build downloads the Renesas RA platform from GitHub and a pinned ARM toolchain (`toolchain-gccarmnoneeabi @ 1.140201.0`). PlatformIO installs the library dependencies automatically:

- Adafruit NeoPixel
- Adafruit PWM Servo Driver Library
- FastLED
- OneButton

### Choosing the servo back-end

The default build uses `I2CServo` and talks to the device at `0x08` on the Qwiic port.

To drive a **PCA9685** instead, add the flag to `build_flags` in `platformio.ini`:

```ini
build_flags = -w -Iinclude -Wno-unknown-pragmas -D__PWM_DRIVER__
```

### Debug output

- `Config::useSerial` in `include/Config.h` turns serial on or off. It starts both USB `Serial` and `Serial1`.
- `#define __DEBUG__` near the top of `src/main.cpp` turns the verbose `DEBUG_PRINT` logging on or off.
- Every command the lamp receives is echoed to USB serial as `Read: <command>`.

---

## Home Assistant setup (XIAO ESP32-S3 + ESPHome)

`esp32s3/lamp.yaml` creates an ESPHome device called **`robolamp`** (friendly name "Robolamp") with:

- an **RGBWW light** called "Robolamp": color, plus color temperature from 153 to 500 mireds, with a 1 s default fade
- **Preset 1–4** buttons that send `p1`–`p4`, plus a **Lamp Preset** dropdown with a **Send Preset** button
- a serial proxy named **"Arduino"** that exposes the Nano's UART

Sync works both ways:

- **Home Assistant → lamp:** every 40 ms the XIAO reads the light's *target* state and sends UART commands to the Nano, only when something has changed. It reads the target, not the value partway through a fade, so a 1 s fade isn't forwarded as a string of brightness changes.
- **Lamp → Home Assistant:** every 500 ms the XIAO reads the light status the Nano sends over I2C and updates the light in Home Assistant. Changes made with the joystick or buttons therefore show up there. For 5 s after sending a command, the XIAO ignores this status, so an outdated reading can't undo a change you just made.

### Setup steps

1. Fill in your own values in `esp32s3/lamp.yaml`, wherever the file has the placeholder `xxxx`:

   | Setting in `lamp.yaml` | Replace `xxxx` with |
   |---|---|
   | `api:` → `encryption:` → `key:` | A 32-byte base64 API key. Generate one in ESPHome |
   | `wifi:` → `password:` | Your Wi-Fi password (again setup in ESPHome) |
   | `wifi:` → `ap:` → `password:` | A password for the fallback hotspot (ESPHome) |

   The Wi-Fi network name is still read with `ssid: !secret wifi_ssid`. Either replace that with your network name (ESPHome auto does this for you when first setting it up), or create `esp32s3/secrets.yaml` next to `lamp.yaml` containing:

   ```yaml
   wifi_ssid: "YourNetwork"
   ```

2. Put the `xiao_servo_bridge` external component in `esp32s3/components/xiao_servo_bridge/`. `lamp.yaml` loads it from that local folder. *(This component is not in the repo yet.)*

3. Flash the XIAO in ESPHome

4. Home Assistant should discover the device. Add it with your API key.

5. Connect the lamp from Home Assistant to your favorite smart home manager (Apple Home, Google Home, SmartThings), then control it from there!

### How Home Assistant settings map to the lamp

| Home Assistant | Lamp |
|---|---|
| Brightness 0–100 % | `bri:0`–`bri:255` |
| RGB color| Snapped to the closest of the lamp's 9 palette colors (red, orange, yellow, lime, green, cyan, blue, violet, pink), then sent as `hue:<value>` |
| Color temp ≤ 260 mireds | `tones:cool` |
| Color temp ≥ 390 mireds | `tones:warm` |
| Color temp in between | `tones:both` (soft white: warm and cool LEDs together) |
| Turn on / off | `lon` / `lof` |
| Preset 1–4 buttons, or Lamp Preset + Send Preset | `p1`–`p4` |

In the other direction, the lamp's palette hue becomes the matching RGB color in Home Assistant, and its white tones map to 500 mireds (warm), 326 (soft) and 153 (cool).

---

## Serial command reference

The Nano accepts these on **USB serial** or on **`Serial1`** (the ESP32 link), at 115200 baud. Each command ends with a newline and is case-insensitive. You can type them straight into the PlatformIO serial monitor to test the lamp without the ESP32.

| Command | Effect |
|---|---|
| `lof` | Turn all lights off |
| `bri:<0-255>` | Set brightness. Any value above 0 turns the lights back on |
| `hue:<0-65535>` | Switch to color mode with this NeoPixel hue |
| `tones:warm` / `tones:cool` / `tones:both` | Switch to white light: warm, cool or soft (both) |
| `p1` … `p4` | Recall preset 1–4 (head position, height and lighting) |
| `setp1` … `setp4` | Save the lamp's current state to preset 1–4 |

> The XIAO also sends `lon` when the light turns on. The Nano currently ignores it (that handler is commented out). The lamp turns back on through the `bri:` command that follows.

---

## I2C protocol

The Nano R4 is the I2C **controller** on `Wire1` (the Qwiic port). It sends every message to **address `0x08`**. Each message is 5 bytes:

```
[ channel (1 byte) ][ value (4 bytes, uint32, big-endian) ]
```

| Channel | Meaning | Value |
|---|---|---|
| `0x00` | Yaw servo | pulse width in µs (544–2400) |
| `0x01` | Pitch servo | pulse width in µs (544–2400) |
| `0x10` | Light power | `1` = on, `0` = off |
| `0x11` | Hue | 0–65535 |
| `0x12` | Brightness | 0–255 |
| `0x13` | White tone | `1` = warm, `2` = soft, `3` = cool |
| `0x14` | Light mode | `1` = color (NeoPixels), `0` = white LEDs |

Servo messages go out whenever the head moves. The light-status messages (`0x10`–`0x14`) go out every 250 ms, so the device at `0x08` always knows the lamp's current lighting state.

In this build, the device at `0x08` is the XIAO ESP32-S3, running the `xiao_servo_bridge` external component. It drives the servos and passes the light status to Home Assistant. Any other microcontroller that acts as an I2C peripheral at `0x08` and handles these messages could do the same job.

### Earlier prototype: `esp32s3/i2c_servo_slave.h`

`i2c_servo_slave.h` is an earlier standalone ESPHome C++ component for the same job. `lamp.yaml` doesn't use it. It:

- listens as an I2C peripheral on **GPIO23 (SDA)** and **GPIO24 (SCL)**
- converts each pulse width to a duty cycle for a 50 Hz PWM output (`µs / 20000`)
- drives two ESPHome outputs: `servo_1_pwm` (yaw) and `servo_2_pwm` (pitch)
- answers a 2-byte read request with whether that servo has been commanded yet

It uses the earlier test protocol, so it doesn't match the current Nano firmware:

| | Nano R4 firmware (`src/I2CServo.cpp`) | `esp32s3/i2c_servo_slave.h` |
|---|---|---|
| I2C address | `0x08` | `0x42` |
| Servo message | 5 bytes: `channel` + uint32 **big-endian** | 6 bytes: `message_id` + `motor_id` + uint32 **little-endian** |
| Light status (`0x10`–`0x14`) | Sent every 250 ms | Not handled |

The `dualr4` branch has another example peripheral, `servoCode/servoCode.ino`, which runs on a second Nano R4. It also uses an older message format.

---

## Using the lamp

| Control | Action |
|---|---|
| **Joystick** | Move the head (yaw / pitch) |
| **Joystick click** | Open the color menu. Joystick left/right picks one of 9 colors or 3 whites, and up/down changes brightness. Click again to confirm. |
| **Joystick hold (3 s)** | Restore the factory presets |
| **Preset button, click** | Recall that preset |
| **Preset button, hold (~1 s)** | Save the current head position, height and lighting to that preset |
| **Preset button, double-click** | Mood: 1 = distracted, 2 = excited, 3 = moody, 4 = focused |
| **Button 4, triple-click** | Toggle the lights off and back on |
| **Up / Down buttons (hold)** | Raise or lower the lamp |

Presets are stored in EEPROM, so they survive power cycles.

---

## Configuration

Most settings live in **`include/Config.h`**:

- **Startup behavior:** `startupMood`, `useSmiley`, `useStartupAnim` (off in this build, though all customizable!), startup color and hue mode, brightness and height
- **Motion feel:** PID gains (`Kp`, `Ki`, `Kd`) and `manualControlInterval`. Lower values make the lamp more energetic; higher values make it calmer.
- **Safety limits:** yaw and pitch servo constraints
- **Mood tuning:** timings, ranges and colors for the distracted, excited and moody modes

Keyframe animations are in **`include/Animations.h`**. Each keyframe is `{ yaw, pitch, duration_ms, easing, height, controlYaw, controlPitch, useBezier }`.

---

## Branches

| Branch | Contents |
|---|---|
| `main` | First working port of the stock firmware to the Nano R4 |
| `dualr4` | Experiment with two Nano R4s, where the second runs `servoCode/servoCode.ino` as an I2C servo driver |
| `motordriver` | **Current version:** servo back-ends (I2C or PCA9685) plus the XIAO ESP32-S3 / Home Assistant integration |

---

## Credits and license

Based on the RoboLamp firmware by **CrunchLabs LLC** (© 2024), released under the **MIT License**. The original license notice is kept at the top of `src/main.cpp`, and this project is shared under the same license.

This is an unofficial community project. It is not affiliated with or endorsed by CrunchLabs.
