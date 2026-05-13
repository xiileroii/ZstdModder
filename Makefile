# --- Compiler and Flags ---
APP_VERSION = $(shell cat VERSION 2>/dev/null || type VERSION 2>nul || echo 1.0.0)
CXXFLAGS_MAIN = -std=c++17 -O2 -Wall -DAPP_VERSION=\"$(APP_VERSION)\"

# --- Cross-Platform OS Detection & Dependencies ---
ifeq ($(OS),Windows_NT)
	TARGET = ZstdModder.exe
	EXTERNAL_BINS = ffmpeg.exe zstd.exe
	
	# PowerShell command to patch out the MinGW dirent.h incompatibility automatically
	PATCH_CMD = powershell -Command "(Get-Content bars-patcher-core/bars-patcher.h) -replace 'mod_dir_entry->d_type != DT_REG', 'mod_dir_entry->d_name[0] == ''.''' | Set-Content bars-patcher-core/bars-patcher.h"
	PATCH_CMD = powershell -Command "(Get-Content src/bars-patcher-core/bars-patcher.h) -replace 'mod_dir_entry->d_name[0] == ''.''', 'mod_dir_entry->d_name[0] == ''.''' | Set-Content src/bars-patcher-core/bars-patcher.h"
else
	TARGET = ZstdModder
	EXTERNAL_BINS = linux_check
	PATCH_CMD = @echo "No patching needed on Linux."
endif

# Universal MSYS2 / Linux cleanup commands
CLEAN_CMD = -rm -f $(TARGET) ffmpeg.exe zstd.exe *.zip
CLEAN_DIR_CMD = -rm -rf

.PHONY: all clean dependencies linux_check fix_patcher

# --- Build Targets ---

all: fix_patcher $(TARGET) dependencies

fix_patcher:
	$(PATCH_CMD)

$(TARGET): src/ZstdModder.cpp src/openrevolution-2.9.0/src/converter.cpp src/pc/main.cpp
	@echo "Building ZstdModder v$(APP_VERSION)..."
	$(CXX) $(CXXFLAGS_MAIN) -I src -I src/openrevolution-2.9.0/src/lib -I src/bars-patcher-core src/ZstdModder.cpp src/openrevolution-2.9.0/src/converter.cpp src/pc/main.cpp -o $(TARGET) -static-libgcc -static-libstdc++ -static

dependencies: $(EXTERNAL_BINS)

# --- Pre-Compiled Windows Binaries ---

ffmpeg.exe:
	@echo "Downloading official FFmpeg build..."
	curl -L -o ffmpeg.zip https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip
	unzip -j ffmpeg.zip "*/bin/ffmpeg.exe" -d .
	@rm -f ffmpeg.zip

zstd.exe:
	@echo "Downloading official Zstandard build..."
	curl -L -o zstd.zip https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-v1.5.6-win64.zip
	unzip -j zstd.zip "zstd-v1.5.6-win64/zstd.exe" -d .
	@rm -f zstd.zip

linux_check:
	@echo "[Info] Running on Linux. Please ensure tools are installed via package manager."

clean:
	@echo "Cleaning up all build files and downloaded binaries..."
	$(CLEAN_CMD)