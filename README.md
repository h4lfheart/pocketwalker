<div align="center">

# pocketwalker - An H8/38606F emulator built for the PokéWalker

[![Release](https://img.shields.io/github/release/h4lfheart/pocketwalker?color=CCC)]()
[![Downloads](https://img.shields.io/github/downloads/h4lfheart/pocketwalker/total?color=CCC)]()

<img alt="PocketWalker" src=".github/images/cover.gif" width="400" />

</div>

---

## Features

### H8/300H CPU
- Full instruction set used by the PokéWalker
- Support for interrupts

### H8/38606F
- Memory Bus with 48KB ROM, 2KB RAM, and IO
- Synchronous Serial Communication Unit (SSU)
- Serial Communication Interface 3 (SCI3)
- Timer B1
- Timer W
- Real-Time Clock (RTC)
- A/D Converter (ADC)

### PokéWalker
- Accelerometer (BMA150)
- EEPRom (M95512)
- LCD (NT7508)
- Buzzer
- IR Communications over TCP with [melonDS-IR](https://github.com/DaveuRrr/melonDS-IR)

## Planned Features

### Emulation
- Save States

### Application

#### Emulation Settings
- Emulation Speed Multiplier
- RTC Sync Mode

#### IR Settings
- UDP IR Communication Mode
- Serial IR Communication Mode

## Building From Source

1. Install [MSYS2](https://www.msys2.org/) and open the **MSYS2 MinGW64** terminal from the Start menu
2. Update packages and reopen the terminal if prompted:
```bash
pacman -Syu
```
3. Install git and clone the repository:
```bash
pacman -S git
git clone https://github.com/h4lfheart/pocketwalker --recursive
cd pocketwalker
```

### Dynamic

4. Install dependencies:
```bash
pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,qt6-base,qt6-multimedia}
```
5. Build:
```bash
./build.sh
```

### Static (Standalone)

4. Install build tools:
```bash
pacman -S mingw-w64-x86_64-{gcc,cmake,ninja}
```
5. Install and set up [vcpkg](https://vcpkg.io/) with `VCPKG_ROOT` set in your environment
6. Build:
```bash
./build.sh --static
```

The output binary will be located in `./release/`.