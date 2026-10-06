# Seeed XIAO ESP32S3 Sensor Link Demo

Streams MJPEG video on request over the Sensor Link protocol.

PKI enrollment and the cloud connection task are enabled by default. The
`wendy_pki` NVS partition stores the device identity.

When installing this partition layout for the first time, flash the full build
over USB from an ESP-IDF 5.5.4 shell:

```sh
idf.py -p /dev/cu.usbmodem101 build flash
```

Replace the serial port with your board's port. `wendy run` updates the app
binary only; it does not update the partition table. An older partition table
without `wendy_pki` causes the enrollment challenge to fail with `bad state`.
Subsequent app updates can use `wendy run` while the partition layout is unchanged.

CA roots are provisioned over USB by `wendy device enroll`, not embedded in the
app. The CLI discovers the selected PKI instance's roots through its EST
CA-discovery endpoint using the computer's HTTPS trust store. Custom endpoint
layouts can use `--ca-certs-url https://.../cacerts`. Private HTTPS CAs must be
trusted by the computer, or supplied with `--device-roots` and `--https-roots`.
The HTTPS bundle must also cover the broker if it uses a different CA. RFC 3161
time additionally requires `--tsa-roots`; Roughtime does not need a TSA bundle.
The board persists these roots in its configuration alongside its PKI settings.

JPEG frame buffers live in PSRAM. Direct PSRAM DMA is disabled because it
stalled capture during WiFi/PKI operation on the XIAO ESP32-S3; the driver uses
an internal DMA buffer instead. Camera initialization runs before networking to
reserve contiguous DMA memory. If camera initialization fails, device management
continues and the camera is omitted from the SensorLink manifest.

WiFi/LwIP prefer PSRAM, general allocations above 1 KiB prefer PSRAM, and
96 KiB of internal RAM is reserved for task stacks and DMA. Fixed WiFi TX/RX
buffers and the RX block-ack window are limited to four for this camera build.

View over WiFi after signing in to the same PKI organization used for enrollment:

```sh
wendy device camera view --device wendy-lite:<device-hostname>.local
```
