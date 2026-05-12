# --- Compiler and Flags ---
CXX = g++
CXXFLAGS_MAIN = -std=c++17 -O2 -Wall
CXXFLAGS_REV = -std=c++11 -O2 -Wall
CXXFLAGS_PATCH = -O2 -pipe -Wall -Wextra

# --- Cross-Platform OS Detection & Dependencies ---
ifeq ($(OS),Windows_NT)
	TARGET = ZstdModder.exe
	CONVERTER = brstm_converter.exe
	PATCHER = auto_bars_patcher.exe
	EXTERNAL_BINS = ffmpeg.exe zstd.exe vgmstream-cli.exe
	
	# PowerShell command to patch out the MinGW dirent.h incompatibility automatically
	PATCH_CMD = powershell -Command "(Get-Content automatic-bars-patcher/bars-patcher-core/bars-patcher.h) -replace 'mod_dir_entry->d_type != DT_REG', 'mod_dir_entry->d_name[0] == ''.''' | Set-Content automatic-bars-patcher/bars-patcher-core/bars-patcher.h"
else
	TARGET = ZstdModder
	CONVERTER = brstm_converter
	PATCHER = auto_bars_patcher
	EXTERNAL_BINS = linux_check
	PATCH_CMD = @echo "No patching needed on Linux."
endif

# Universal MSYS2 / Linux cleanup commands
CLEAN_CMD = -rm -f $(TARGET) $(CONVERTER) $(PATCHER) ffmpeg.exe zstd.exe vgmstream-cli.exe *.dll *.zip
CLEAN_DIR_CMD = -rm -rf

.PHONY: all clean dependencies linux_check

# --- Build Targets ---

all: $(TARGET) dependencies

$(TARGET): ZstdModder.cpp
	@echo "Building ZstdModder..."
	$(CXX) $(CXXFLAGS_MAIN) ZstdModder.cpp -o $(TARGET) -static-libgcc -static-libstdc++ -static

dependencies: $(CONVERTER) $(PATCHER) $(EXTERNAL_BINS)

$(CONVERTER):
	@echo "Fetching and building brstm_converter..."
	git clone https://github.com/ic-scm/openrevolution.git
	$(CXX) $(CXXFLAGS_REV) openrevolution/src/converter.cpp -o $(CONVERTER) -static
	@echo "Waiting for file locks to release..."
	sleep 1
	$(CLEAN_DIR_CMD) openrevolution

$(PATCHER):
	@echo "Fetching and building auto_bars_patcher..."
	git clone https://github.com/ic-scm/automatic-bars-patcher.git
	$(PATCH_CMD)
	$(CXX) $(CXXFLAGS_PATCH) automatic-bars-patcher/pc/main.cpp -o $(PATCHER) -static
	@echo "Waiting for file locks to release..."
	sleep 1
	$(CLEAN_DIR_CMD) automatic-bars-patcher

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

vgmstream-cli.exe:
	@echo "Downloading official vgmstream build..."
	curl -L -o vgmstream.zip https://github.com/vgmstream/vgmstream/releases/latest/download/vgmstream-win.zip
	unzip -j vgmstream.zip "vgmstream-cli.exe" "*.dll" -d .
	@rm -f vgmstream.zip

linux_check:
	@echo "[Info] Running on Linux. Please ensure 'ffmpeg', 'vgmstream', and 'zstd' are installed via your package manager."

clean:
	@echo "Cleaning up all build files and downloaded binaries..."
	$(CLEAN_CMD)