# Obegransad

## Get the proper C-Code for static analysis

https://www.perplexity.ai/search/how-to-strip-the-c-c-code-by-r-JLu7mmGjRi.iywX2IPxX9Q

## Develop

See [[DEVELOPER.md]]

### Simulator

Run the firmware without the board, with the display and button replaced by
simulator backends:

```sh
tools/sim/run-qemu.sh    # real firmware on emulated ESP32-C3 (QEMU)
tools/sim/run-host.sh    # native Linux build (fast iteration, gdb)
```

See [[tools/sim/README.md]] for options, the renderer, networking and
limitations, and [[docs/known-issues.md]] for caveats around OTA, QEMU
networking and the partition layout. The host build needs the `libbsd`
development headers.

### OTA updates

The device has two OTA slots (`partitions.csv`: `otadata` + `ota_0` + `ota_1`,
1984 KiB each) and accepts a push update on the config server:

```sh
curl -X POST -H 'Content-Type: application/octet-stream' -H 'Expect:' \
     --data-binary @build/obegransad.bin http://<device-ip>:8080/api/ota
```

The device writes the inactive slot, validates the image, switches the boot
partition and restarts. Until that point the running image keeps booting, so an
interrupted upload is harmless. `GET /api/ota` reports the running slot.

One-time migration (and every wired reflash) — use the wrapper, which also
clears `otadata` so the freshly flashed image boots:

```sh
tools/flash.sh --port /dev/ttyACM0
```

It runs `idf.py flash` (bootloader, partition table, app → `ota_0`) and then
erases the `otadata` partition; NVS (Wi-Fi credentials, config) is kept.
`--erase-nvs` additionally wipes NVS. Plain `idf.py flash` writes `ota_0` but
leaves `otadata` alone, so a device that was last updated via OTA would keep
booting `ota_1`.

For testing the flow in QEMU: `tools/sim/ota-test.sh` (push + slot check), and
`tools/sim/run-qemu.sh --persist-flash` if the OTA'd slot should survive
simulator restarts. The full API is in [[docs/openapi.yaml]].



## TIPS

### Wont flash

Open arduino IDE and flash a minimal program, then come back here and try again.

### Little FS errors

Try deleting sdkconfig.

## CLion

There are three resources for running ESP-IDF in CLion:
- Unofficial plugin: https://yunyizhi-github-io.translate.goog/?_x_tr_sl=en&_x_tr_tl=de&_x_tr_hl=de&_x_tr_pto=wapp&_x_tr_hist=true#target
- Guide: https://www.jetbrains.com/help/clion/esp-idf.html
- Blog: https://developer.espressif.com/blog/clion/
