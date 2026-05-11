# --- Compiler and Flags ---
CXX = g++
CXXFLAGS_MAIN = -std=c++17 -O2 -Wall
CXXFLAGS_REV = -std=c++11 -O2 -Wall
CXXFLAGS_PATCH = -O2 -pipe -Wall -Wextra

# --- Cross-Platform OS Detection ---
ifeq ($(OS),Windows_NT)
	TARGET = ZstdModder.exe
	CONVERTER = brstm_converter.exe
	PATCHER = auto_bars_patcher.exe
	# PowerShell command to patch out the MinGW dirent.h incompatibility automatically
	PATCH_CMD = powershell -Command "(Get-Content automatic-bars-patcher/bars-patcher-core/bars-patcher.h) -replace 'mod_dir_entry->d_type != DT_REG', 'mod_dir_entry->d_name[0] == ''.''' | Set-Content automatic-bars-patcher/bars-patcher-core/bars-patcher.h"
else
	TARGET = ZstdModder
	CONVERTER = brstm_converter
	PATCHER = auto_bars_patcher
	PATCH_CMD = @echo "No patching needed on Linux."
endif

# Universal MSYS2 / Linux cleanup commands
CLEAN_CMD = -rm -f $(TARGET) $(CONVERTER) $(PATCHER)
CLEAN_DIR_CMD = -rm -rf

.PHONY: all clean dependencies

# --- Build Targets ---

all: $(TARGET) dependencies

$(TARGET): ZstdModder.cpp
	@echo "Building ZstdModder..."
	$(CXX) $(CXXFLAGS_MAIN) ZstdModder.cpp -o $(TARGET)

dependencies: $(CONVERTER) $(PATCHER)

$(CONVERTER):
	@echo "Fetching and building brstm_converter..."
	git clone https://github.com/ic-scm/openrevolution.git
	$(CXX) $(CXXFLAGS_REV) openrevolution/src/converter.cpp -o $(CONVERTER) -static
	@echo "Waiting for Windows locks to release..."
	sleep 1
	$(CLEAN_DIR_CMD) openrevolution

$(PATCHER):
	@echo "Fetching and building auto_bars_patcher..."
	git clone https://github.com/ic-scm/automatic-bars-patcher.git
	$(PATCH_CMD)
	$(CXX) $(CXXFLAGS_PATCH) automatic-bars-patcher/pc/main.cpp -o $(PATCHER) -static
	@echo "Waiting for Windows locks to release..."
	sleep 1
	$(CLEAN_DIR_CMD) automatic-bars-patcher

clean:
	@echo "Cleaning up build files..."
	$(CLEAN_CMD)