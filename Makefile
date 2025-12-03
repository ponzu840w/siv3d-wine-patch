CC32 = i686-w64-mingw32-gcc
CC64 = x86_64-w64-mingw32-gcc

BIN_DIR = bin
SRC_DIR = .
MH_INC_DIR = lib/minhook/include
MH_SRC_DIR = lib/minhook/src

CFLAGS = -O2 -Wall
LDFLAGS_DLL = -shared -static -lole32
LDFLAGS_EXE = -static

MH_COMMON_SRCS = $(MH_SRC_DIR)/buffer.c \
                 $(MH_SRC_DIR)/hook.c \
                 $(MH_SRC_DIR)/trampoline.c

all: directories launcher32 launcher64 patch32 patch64

directories:
	@mkdir -p $(BIN_DIR)

# --- 32-bit Targets ---
launcher32: $(SRC_DIR)/launcher.c
	$(CC32) $(CFLAGS) -DDEFAULT_DLL_NAME=\"patch32.dll\" -o $(BIN_DIR)/launcher32.exe $< $(LDFLAGS_EXE)
	@echo "Built 32-bit launcher: $(BIN_DIR)/launcher32.exe (Default DLL: patch32.dll)"

patch32: $(SRC_DIR)/patch.c
	$(CC32) $(CFLAGS) -I$(MH_INC_DIR) -o $(BIN_DIR)/patch32.dll \
		$< \
		$(MH_COMMON_SRCS) \
		$(MH_SRC_DIR)/hde/hde32.c \
		$(LDFLAGS_DLL)
	@echo "Built 32-bit DLL: $(BIN_DIR)/patch32.dll"

# --- 64-bit Targets ---
launcher64: $(SRC_DIR)/launcher.c
	$(CC64) $(CFLAGS) -DDEFAULT_DLL_NAME=\"patch64.dll\" -o $(BIN_DIR)/launcher64.exe $< $(LDFLAGS_EXE)
	@echo "Built 64-bit launcher: $(BIN_DIR)/launcher64.exe (Default DLL: patch64.dll)"

patch64: $(SRC_DIR)/patch.c
	$(CC64) $(CFLAGS) -I$(MH_INC_DIR) -o $(BIN_DIR)/patch64.dll \
		$< \
		$(MH_COMMON_SRCS) \
		$(MH_SRC_DIR)/hde/hde64.c \
		$(LDFLAGS_DLL)
	@echo "Built 64-bit DLL: $(BIN_DIR)/patch64.dll"

# --- Clean ---
clean:
	rm -rf $(BIN_DIR)
	@echo "Cleaned build artifacts."

.PHONY: all directories clean launcher32 launcher64 patch32 patch64
