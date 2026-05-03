
# OSCAR P01 Flasher / Loader

![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-STM32-green.svg)
![Toolchain](https://img.shields.io/badge/toolchain-STM32CubeIDE-orange.svg)
![Status](https://img.shields.io/badge/status-stable-brightgreen.svg)

Boot program for the **OSCAR platform (P01 processor board)** combining a bootloader and a flasher client.

It allows:
- Storing into onboard QSPI flash memory:
  - ELF files of effect programs  
  - Associated dependencies (fonts, images, etc.)  
- Selecting which effect to launch at startup

At power-up, the bootloader automatically runs the selected effect.


## 🚀 Getting Started

### Clone the repository

```bash
git clone --recurse-submodules https://github.com/DADDesign-Projects/OSCAR_P01_FLasher_Loader
```

## 🛠️ Build

- Toolchain: **STM32CubeIDE**
- Target: **STM32 (P01 board)**

## 🎛️ Usage

### Normal Mode
- The system boots directly into the selected effect.

### Flasher / Loader Mode

To enter Flasher mode:

1. **Press and hold Footswitch 0**
2. **Power ON the pedal**

The system will switch to **Flasher/Loader mode**.

### Uploading Effects

1. Connect the pedal to your PC using a **USB cable**
2. Launch:

```
OSCAR_FlasherServer.exe
```

(located in the `@Flasher server` directory)

3. Upload:
   - ELF effect files  
   - Dependencies (fonts, images, etc.)

### Selecting the Boot Effect

1. Use the **encoder M** to browse available ELF files  
2. Press the **encoder M** to confirm selection  

The selected effect will be automatically launched at next power-up.

## 🧩 Flasher Server Source Code

The Flasher Server source code is available here:

https://github.com/DADDesign-Projects/OSCAR_FLasherServer

## 📄 License

This project is licensed under the **Apache License 2.0**.

## 📌 Notes

- Ensure all dependencies are uploaded along with the ELF file  
- Only valid ELF files will be listed for selection  
