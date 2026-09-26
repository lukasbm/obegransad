# Obegransad

## Get the proper C-Code for static analysis

https://www.perplexity.ai/search/how-to-strip-the-c-c-code-by-r-JLu7mmGjRi.iywX2IPxX9Q

## Develop

See [[DEVELOPER.md]]

### Simulator (QEMU)

Run the firmware on an emulated ESP32-C3 with the display and button replaced
by simulator backends (QEMU does not emulate SPI/RMT/GPIO or Wi-Fi):

```sh
tools/sim/run-qemu.sh
```

See [[tools/sim/README.md]] for options, the renderer, networking (emulated
Ethernet + host port forwarding) and limitations.



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
