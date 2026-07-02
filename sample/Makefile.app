# =====================================================================
#  Makefile.app : SQLite3 Application Builder for X680x0
# =====================================================================

CC      := m68k-xelf-gcc
TARGET  := test.x
ELF     := test.elf

# 【両Mac対応自動判別】親ディレクトリのパスを基準に elf2x68k.py を自動チェック
ifeq ($(wildcard /opt/homebrew/bin/elf2x68k.py),)
    ELF2X68 = python3 /usr/local/bin/elf2x68k.py
else
    ELF2X68 = python3 /opt/homebrew/bin/elf2x68k.py
endif

# 【★修正】ターゲットを main.o から test.o へと変更
OBJS    := test.o
LIBS    := ../lib/libsqlite3.a

CFLAGS  := -O2
LDFLAGS := -lm

all: $(TARGET)

$(TARGET): $(ELF)
	$(ELF2X68) -s -o $(TARGET) $(ELF)
	@echo "======================================================"
	@echo " 成功: アプリケーション $(TARGET) が正常に生成されました！"
	@echo "======================================================"

$(ELF): $(OBJS) $(LIBS)
	@echo "-> 上位ディレクトリの lib/libsqlite3.a と結合中..."
	$(CC) $(OBJS) $(LIBS) $(LDFLAGS) -o $(ELF)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(ELF) $(TARGET)

.PHONY: all clean
