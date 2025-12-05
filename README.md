# UTS IO

UTS IO is a custom IO DLL designed to map keyboard inputs to the game's controls. It implements the `mai2_io` API, handling system buttons, gameplay buttons, and touch sensors.

## Features

- **Keyboard Mapping**: Map keyboard keys to arcade buttons (Test, Service, Coin, Player 1/2 Buttons).
- **Touch Simulation**: Map keyboard keys to specific touch sensors (A1-E8).
- **Touch Overlay**: Support for touch overlay input when running the game.

## Build Instructions

This project is built using MinGW-w64.

1.  Ensure you have `gcc` installed and available in your PATH.
2.  Run `make` in the project directory.

```bash
make
```

This will generate:
- `utsio.dll`: The main IO DLL.

## Acknowledgements

This project references code and concepts from:
- [MaiTouchSensorEmulator](https://github.com/Leapward-Koex/MaiTouchSensorEmulator) by Leapward-Koex

