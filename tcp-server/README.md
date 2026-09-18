# TCP/TLS Bandwidth Test Server

Wendy Lite native app that measures network throughput. It listens on two ports
and sends a fixed 10 MiB to any client that connects, then closes:

| Port | Transport | Client authentication |
| ---- | --------- | --------------------- |
| 9000 | plain TCP | none |
| 9001 | TLS       | none — the server presents the self-signed certificate embedded in `wendy_conf`, and asks for nothing in return |

Serving the same payload over both makes the cost of the TLS layer directly
readable: run the two back to back on the same board, same AP, same session.

The payload is the ASCII string `hello-world-` repeated to exactly 10485760
bytes.

Each port serves one client at a time; a second connection waits in the
backlog. The two ports are independent tasks, so a plain and a TLS transfer can
overlap. The RGB LED keeps blinking throughout, as a liveness signal.

### Supported boards

* ESP32-C5-DevKitC-1
* ESP32-C6-DevKitC-1
* ESP32-C6-DevKitM-1
* ESP32-C61-DevKitC-1

## Building

```sh
wendy run
```

or

```sh
source ~/.espressif/tools/activate_idf_v5.5.4.sh
idf.py set-target esp32c6
idf.py gen_project_binary
```

## Measuring

Take the board's address from the `wendy_wifi` line in the boot log. Both
commands must report exactly `10485760` bytes:

```sh
# plain TCP
time nc $IP 9000 | wc -c

# TLS
time openssl s_client -connect $IP:9001 -quiet -verify_quiet 2>/dev/null | wc -c
```

The board reports its own view of each transfer, which should agree with the
host's wall clock:

```
I (12345) tcp_server: client 192.168.1.42 connected
I (23456) tcp_server: sent 10485760 bytes in 8402 ms (9983 kbit/s)
```

The TLS server also logs the negotiated ciphersuite. It is worth reading: a
client that picks ChaCha20-Poly1305 is measuring software crypto, while AES-GCM
uses the chip's accelerator, and the two give very different numbers.

To check the payload arrived intact:

```sh
nc $IP 9000 | head -c 48
# hello-world-hello-world-hello-world-hello-world-
```

## Notes

* `sdkconfig.defaults` is byte-identical to `blink-rgb`'s. That is deliberate:
  the number this firmware reports is the one the stock wendy-lite
  configuration delivers, not the one a tuned build could reach. The main
  levers, if you ever want to explore them, are
  `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` (5760 here) and
  `CONFIG_ESP_WIFI_TX_BA_WIN` (6 here).
* The send buffer is 32760 bytes — see the comment on `PAYLOAD_CHUNK_BYTES` in
  [main/payload.h](main/payload.h) for why that size and not another.
* Ports 9000/9001 avoid 5054, which `wendy_server` holds for the mTLS control
  channel.
* The device closes each connection first, so it holds the TIME_WAIT. Rapidly
  repeated runs accumulate TIME_WAIT control blocks; lwIP recycles the oldest,
  so this degrades gracefully rather than failing.
