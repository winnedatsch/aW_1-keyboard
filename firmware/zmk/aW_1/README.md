# Building ZMK for the aW_1

## Standard Build

```
west build -p -d build/aW_1 --board aW_1
```

## Flashing

`west` can be used to flash the board directly. Press the reset button once, and run:

```
west flash -d build/aW_1
```
