# MicroDrive

# 🧰 Required Tools

To build and flash the project to the Raspberry Pi Pico, you’ll need the following tools and dependencies installed on your system.

## 🧩 1. Core build tools
These are needed for compiling C/C++ code and using CMake:
```bashv
sudo pacman -S --needed base-devel cmake ninja arm-none-eabi-gcc arm-none-eabi-newlib git
```
💡 On Debian/Ubuntu:
```bash
sudo apt install build-essential cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi git
```

## 🧠 2. Raspberry Pi Pico SDK
Clone the official SDK and its submodules:
```bash
mkdir -p ~/pico
cd ~/pico
git clone -b master https://github.com/raspberrypi pico-sdk.git
```
Then export its path to your environment:

For Fish shell:

```fish

echo "set -Ux PICO_SDK_PATH ~/pico/pico-sdk" >> ~/.config/fish/config.fish
exec fish
```

💡 If you use Bash or Zsh:
```bash
echo "export PICO_SDK_PATH=~/pico/pico-sdk" >> ~/.bashrc
source ~/.bashrc
```


# ⚙️ Building and Flashing the Code

Once all required tools are installed and the Raspberry Pi Pico SDK is configured, you can build and flash the firmware onto your Pico board.

## 🧩 1. Configure the build directory

CMake keeps build artifacts separate from source files.
Inside your project folder, create a dedicated build directory:

```bash
mkdir build
cd build
```

Then configure the project with CMake:

```bash
cmake ..
```

If the SDK path is not detected automatically, you can specify it manually:

```bash
cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH
```

You should see CMake detect your toolchain and Pico SDK successfully.

## 🛠️ 2. Build the project

Once configured, build your firmware using:

```bash
make -j$(nproc)
```


This will compile all source files and generate a firmware file in the build/ directory — typically named something like:

```bash
build/microdrive.uf2
```

## 🔌 3. Flashing the Raspberry Pi Pico

To flash the firmware onto your Pico:

Hold the BOOTSEL button on the Pico.

Plug in the USB cable while keeping the button pressed.

The board will appear as a USB drive (e.g., /media/omar/RPI-RP2).

Then simply copy your compiled UF2 file to it:

```bash
cp build/*.uf2 /run/media/$USER/RPI-RP2
```

Once copied, the Pico will automatically reboot and run your program.