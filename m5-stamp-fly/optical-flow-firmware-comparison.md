# Optical flow (PMW3901) support across StampFly / esp-drone / Crazyflie firmware

Survey of drone firmware codebases, checking whether each actually reads the
PMW3901 optical flow sensor and closes a horizontal position-hold control
loop with it — as opposed to just having the sensor present on the board or
a dead/unused driver.

| Codebase | MCU support | Optical flow support | Official hardware |
|---|---|---|---|
| **Wendy Lite port of `m5-stamp-fly`** (this repo) | ESP32-S3 | **No** — no PMW3901 driver at all, dead SPI pin defines only (`sensor.hpp`) | Yes — M5Stack StampFly |
| **m5stack/M5StampFly** (official, upstream of Wendy Lite port) | ESP32-S3 | **No** | Yes — M5Stack StampFly |
| **M5Fly-kanazawa/StampFly2024June** (predecessor) | ESP32-S3 | **No** | Yes — M5Stack StampFly (early rev) |
| **kouhei1970/stampfly_hal** | ESP32-S3 | **Driver only** — `pmw3901.cpp` exists but `main.cpp` never calls it; no RC, no flight controller, no control loop of any kind | Yes — M5Stack StampFly |
| **Taylor-eOS/m5stamp-fly** | ESP32-S3 | **Partial** — reads `dx`/`dy`/SQUAL every loop, but only pushes them out as telemetry text; never reaches the PID controller | Yes — M5Stack StampFly |
| **espressif/esp-drone** | ESP32 / ESP32-S2 / ESP32-S3 | **Yes** — full EKF (`estimator_kalman.c`) + position-hold PID, native `flowdeck_v1v2.c` support | Yes — Espressif's own boards (`ESP32_S2_Drone_V1_2`, `ESPlane_FC_V1`/`V2_S2`, `ESP32_S2_Drone_Flow_Deck`) |
| **Circuit-Digest/ESP-Drone** | ESP32-S3 (fork of esp-drone) | **Yes** — inherits the full flow pipeline unchanged | Yes — their own board, precursor to LiteWing |
| **jobitjoseph/LiteWing** | ESP32-S3 (fork of esp-drone) | **Yes** — same EKF + position-hold PID, actively documented/tutorialized | Yes — LiteWing board (SemiconLab / CircuitDigest / Robocraze / Elecrow) |
| **Bitcraze crazyflie-firmware** | STM32F405 + nRF51822 (not ESP32 at all) | **Yes** — the original Flow Deck implementation; source that all the above trace back to | Yes — Crazyflie 2.x + Flow Deck |

## Takeaway

Everything in the StampFly lineage (top 5 rows) either has no PMW3901 code,
has a driver nobody calls, or reads the sensor without closing the loop.
Everything descended from Crazyflie via `esp-drone` (bottom 4 rows) has a
real, working position-hold implementation — same sensor (PMW3901), same
fusion algorithm (EKF), same controller (PID), just three different
hardware targets carrying it forward.

The most direct path to adding working position hold to this firmware is
porting `estimator_kalman.c` / `kalman_core.c` / `position_controller_pid.c`
/ `flowdeck_v1v2.c` from `espressif/esp-drone` (upstream, same chip family,
still Espressif-maintained) rather than writing a fusion/control loop from
scratch, or than pulling from Bitcraze's original STM32-targeted firmware
(incompatible MCU architecture).
