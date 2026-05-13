# ZstdModder

ZstdModder is a powerful, automated, and cross-platform desktop application for modding Nintendo Switch audio files. It provides a seamless, end-to-end pipeline for converting standard audio files (FLAC, MP3, WAV, OGG) into native Switch formats (`.bwav`), injecting them into `.bars` audio archives, compressing them, and deploying them directly to your emulator or Nintendo Switch SD card.

## Features

* **Modern Graphical Interface:** Built with `customtkinter`, featuring a clean, dark-themed UI that manages the complex C++ backend completely invisibly.
* **Batch Processing:** Select multiple files or entire folders to process massive audio mods in one click. The orchestrator runs conversions concurrently for maximum speed.
* **Intelligent Target Routing:** If your input audio filename doesn't perfectly match an internal game file, the UI pauses and provides a real-time, fuzzy-search list of valid internal `.bars` targets to map your audio to.
* **Native BWAV Auto-Detection:** The C++ backend natively parses Nintendo's original `.bwav` dumps to automatically match the exact sample rate and channel count required by the game—no manual configuration needed.
* **Bulletproof Architecture:** Uses a custom "Busybox" C++ backend. It statically links `brstm_converter` and `auto_bars_patcher` and safely isolates them in child processes. This eliminates race conditions and bypasses legacy 1-channel segfault bugs on Windows.
* **1-Click Atmosphere Deployment:** Automatically copies the finished `.bwav` and `.bars.zs` files directly into your `romfs/Sound/Resource` folder.

## How It Works

Under the hood, ZstdModder handles the entire audio modding pipeline:
1. **Decompression:** Uses `zstd` to uncompress game `.bars.zs` files.
2. **Resampling:** Uses `ffmpeg` to resample user audio to perfectly match the game's expected channels and Hz.
3. **Encoding:** Uses `openrevolution` (`brstm_converter`) to encode the audio into Nintendo's DSPADPCM `.bwav` format.
4. **Patching:** Uses `auto_bars_patcher` to update the BARS archive tables with the new BWAV file sizes and offsets.
5. **Compression:** Repacks the BARS archive back into a `.bars.zs` file.

## Installation & Usage

### For Windows Users (Pre-Compiled)
1. Download the latest release `.zip` from the **Releases** page.
2. Extract the folder to your PC.
3. **CRITICAL REQUIREMENT:** The pre-compiled Windows binary requires FFmpeg and Zstandard. You must ensure that `ffmpeg.exe` and `zstd.exe` are downloaded and placed in the exact same folder as the GUI executable.
   * [https://github.com/facebook/zstd/releases/latest](https://github.com/facebook/zstd/releases/latest)
   * [https://www.ffmpeg.org/download.html](https://www.ffmpeg.org/download.html)
4. Run `ZstdModder_GUI.exe`. (No Python installation or terminal required!)

### For Linux Users (Pre-Compiled)
1. Download the latest Linux release from the **Releases** page and extract it.
2. **CRITICAL REQUIREMENT:** You must ensure that the `ffmpeg` and `zstd` packages are already installed on your system before using the pre-compiled executable. 
   * **Ubuntu/Debian:** `sudo apt install ffmpeg zstd`
   * **Arch Linux:** `sudo pacman -S ffmpeg zstd`
   * **Fedora:** `sudo dnf install ffmpeg zstd`
3. Run the executable (e.g., `./ZstdModder_GUI`).

## How to Make a Mod

1. **Set your Paths:** Open ZstdModder and browse for your **Atmosphere Path** (where the mod will go) and your **Game 'Stream' Folder** (the original, unmodified audio dump from the game).
2. **Add Files:** Click **Add Files** or **Add Folder** to select the songs/sound effects you want to inject into the game.
3. **Build:** Click **Build Mod**.
4. **Resolve Names (If Needed):** If the tool doesn't recognize an audio file's name, a pop-up will appear. Simply type the name of the in-game track you want to replace, select it from the filtered list, and hit **Enter**.
5. **Play:** Boot up your emulator or Switch! The mod is already installed.

### For Mac / Linux / Source Developers
To run the tool from source, ensure you have **Python 3**, `make`, and a C++ compiler (`g++` or `clang`) installed.

1. **Install System Dependencies:**
   Make sure `ffmpeg` and `zstd` are installed on your system.
   * **Ubuntu/Debian:** `sudo apt install ffmpeg zstd`
   * **macOS:** `brew install ffmpeg zstd`
2. **Clone the Repository:**
   ```bash
   git clone --recursive [https://github.com/yourusername/zstdmodder.git](https://github.com/yourusername/zstdmodder.git)
   cd zstdmodder
   ```
3. **Build the C++ Engine:**
   ```bash
   make
   ```
   *This compiles the `ZstdModder` busybox backend, statically linking the BARS patcher and OpenRevolution encoder.*
4. **Install Python UI Dependencies:**
   ```bash
   pip install -r requirements.txt
   ```
5. **Run the Application:**
   ```bash
   python src/app.py
   ```

## Building the Windows Executable (For Maintainers)
To package the Python script into a standalone `.exe` for releases:
```cmd
python -m pip install pyinstaller
python -m PyInstaller --noconsole --onefile --name "ZstdModder_GUI" src/app.py
```
Place the resulting `ZstdModder_GUI.exe` from the `dist/` folder next to your compiled `ZstdModder.exe`, `ffmpeg.exe`, and `zstd.exe`.

## Acknowledgements

This project relies on several incredible open-source tools:
* [CustomTkinter](https://github.com/TomSchimansky/CustomTkinter) for the UI framework.
* [openrevolution](https://github.com/ic-scm/openrevolution) for BRSTM/BWAV encoding.
* [auto-bars-patcher](https://github.com/YourPatcherLinkHere) for BARS archive manipulation.
* # ZstdModder

ZstdModder is a powerful, automated, and cross-platform desktop application for modding Nintendo Switch audio files. It provides a seamless, end-to-end pipeline for converting standard audio files (FLAC, MP3, WAV, OGG) into native Switch formats (`.bwav`), injecting them into `.bars` audio archives, compressing them, and deploying them directly to your emulator or Nintendo Switch SD card.

## Features

* **Modern Graphical Interface:** Built with `customtkinter`, featuring a clean, dark-themed UI that manages the complex C++ backend completely invisibly.
* **Batch Processing:** Select multiple files or entire folders to process massive audio mods in one click. The orchestrator runs conversions concurrently for maximum speed.
* **Intelligent Target Routing:** If your input audio filename doesn't perfectly match an internal game file, the UI pauses and provides a real-time, fuzzy-search list of valid internal `.bars` targets to map your audio to.
* **Native BWAV Auto-Detection:** The C++ backend natively parses Nintendo's original `.bwav` dumps to automatically match the exact sample rate and channel count required by the game—no manual configuration needed.
* **Bulletproof Architecture:** Uses a custom "Busybox" C++ backend. It statically links `brstm_converter` and `auto_bars_patcher` and safely isolates them in child processes. This eliminates race conditions and bypasses legacy 1-channel segfault bugs on Windows.
* **1-Click Atmosphere Deployment:** Automatically copies the finished `.bwav` and `.bars.zs` files directly into your `romfs/Sound/Resource` folder.

## How It Works

Under the hood, ZstdModder handles the entire audio modding pipeline:
1. **Decompression:** Uses `zstd` to uncompress game `.bars.zs` files.
2. **Resampling:** Uses `ffmpeg` to resample user audio to perfectly match the game's expected channels and Hz.
3. **Encoding:** Uses `openrevolution` (`brstm_converter`) to encode the audio into Nintendo's DSPADPCM `.bwav` format.
4. **Patching:** Uses `auto_bars_patcher` to update the BARS archive tables with the new BWAV file sizes and offsets.
5. **Compression:** Repacks the BARS archive back into a `.bars.zs` file.

## Installation & Usage

### For Windows Users (Pre-Compiled)
1. Download the latest release `.zip` from the **Releases** page.
2. Extract the folder to your PC.
3. **CRITICAL REQUIREMENT:** The pre-compiled Windows binary requires FFmpeg and Zstandard. You must ensure that `ffmpeg.exe` and `zstd.exe` are downloaded and placed in the exact same folder as the GUI executable.
[https://github.com/facebook/zstd/releases/latest](https://github.com/facebook/zstd/releases/latest)
[https://www.ffmpeg.org/download.html](https://www.ffmpeg.org/download.html)
4. Run `ZstdModder_GUI.exe`. (No Python installation or terminal required!)

### For Linux Users (Pre-Compiled)
1. Download the latest Linux release from the **Releases** page and extract it.
2. **CRITICAL REQUIREMENT:** You must ensure that the `ffmpeg` and `zstd` packages are already installed on your system before using the pre-compiled executable. 
   * **Ubuntu/Debian:** `sudo apt install ffmpeg zstd`
   * **Arch Linux:** `sudo pacman -S ffmpeg zstd`
   * **Fedora:** `sudo dnf install ffmpeg zstd`
3. Run the executable (e.g., `./ZstdModder_GUI`).

## How to Make a Mod

1. **Preparation:** Add the bars.zs file that you wish to modify to the same folder as the ZstdModder executable.
2. **Set your Paths:** Open ZstdModder and browse for your **Atmosphere Path** (where the mod will go) and your **Game 'Stream' Folder** (the original, unmodified audio dump from the game).
3. **Add Files:** Click **Add Files** or **Add Folder** to select the songs/sound effects you want to inject into the game.
4. **Build:** Click **Build Mod**.
5. **Resolve Names (If Needed):** If the tool doesn't recognize an audio file's name, a pop-up will appear. Simply type the name of the in-game track you want to replace, select it from the filtered list, and hit **Enter**.
6. **Play:** Boot up your emulator or Switch! The mod is already installed.

### For Mac / Linux / Source Developers
To run the tool from source, ensure you have **Python 3**, `make`, and a C++ compiler (`g++` or `clang`) installed.

1. **Install System Dependencies:**
   Make sure `ffmpeg` and `zstd` are installed on your system.
   * **Ubuntu/Debian:** `sudo apt install ffmpeg zstd`
   * **macOS:** `brew install ffmpeg zstd`
2. **Clone the Repository:**
   ```bash
   git clone --recursive [https://github.com/yourusername/zstdmodder.git](https://github.com/yourusername/zstdmodder.git)
   cd zstdmodder
   ```
3. **Build the C++ Engine:**
   ```bash
   make
   ```
   *This compiles the `ZstdModder` busybox backend, statically linking the BARS patcher and OpenRevolution encoder.*
4. **Install Python UI Dependencies:**
   ```bash
   pip install -r requirements.txt
   ```
5. **Run the Application:**
   ```bash
   python src/app.py
   ```

## Building the UI Executable
To package the Python GUI executable:
```cmd
python -m pip install pyinstaller
python -m PyInstaller --noconsole --onefile --name "ZstdModder_GUI" src/app.py
```
Place the resulting `ZstdModder_GUI.exe` from the `dist/` folder next to your compiled `ZstdModder.exe`, `ffmpeg.exe`, and `zstd.exe`.

## Acknowledgements

This project relies on several incredible open-source tools:
* [CustomTkinter](https://github.com/TomSchimansky/CustomTkinter) for the UI framework.
* [openrevolution](https://github.com/ic-scm/openrevolution) for BRSTM/BWAV encoding.
* [auto-bars-patcher](https://github.com/ic-scm/automatic-bars-patcher) for BARS archive manipulation.
* [FFmpeg](https://github.com/ffmpeg/ffmpeg) for audio processing.
* [Zstandard](https://github.com/facebook/zstd) for compression.