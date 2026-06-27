# =====================================================================
#  Makefile : SQLite3 for X680x0
# =====================================================================

# 使用するクロスコンパイラとコンバータの設定
CC      = m68k-xelf-gcc
ELF2X68 = python3 /opt/homebrew/bin/elf2x68k.py

# ターゲットファイル名
TARGET_ELF = sqlite3.elf
TARGET_X   = sqlite3.x

# ソースファイルの一覧
SRCS = minshell.c sqlite3.c stub.c

# コンパイルオプション（2MB環境最適化・標準C言語VFS専用フラグ）
CFLAGS = -O0 \
         -DSQLITE_OS_OTHER=1 \
         -DSQLITE_THREADSAFE=0 \
         -DSQLITE_OMIT_LOAD_EXTENSION=1 \
         -DSQLITE_OMIT_WAL=1 \
         -DSQLITE_OMIT_DEPRECATED=1 \
         -DSQLITE_DEFAULT_PAGE_SIZE=512 \
         -DSQLITE_DEFAULT_CACHE_SIZE=10 \
         -DSQLITE_DEFAULT_LOOKASIDE=0,0 \
         -DSQLITE_DEFAULT_MEMSTATUS=0 \
         -DSQLITE_MAX_MEMORY=262144 \
         -DSQLITE_OMIT_ANALYZE=1 \
         -DSQLITE_OMIT_PROGRESS_CALLBACK=1 \
         -DSQLITE_OMIT_VIRTUALtable=1 \
         -DHAVE_SYS_IOCTL_H=0 \
         -DIsatty= \
         -Dlstat=stat

# リンクオプション
LIBS = -lm

# デフォルトターゲット
all: patch $(TARGET_X)

# 【★追加】公式ソースコードに対する自動パッチ（コメントアウト修正）
# macOS (sed -i.bak) と Linux (sed -i) の両方に対応できるよう工夫しています
patch:
	@echo "公式ソースコード（ioctl.h）の自動修正チェック中..."
	@if [ -f sqlite3.c ]; then \
		sed -i.bak 's/#include <sys\/ioctl.h>/\/* #include <sys\/ioctl.h> *\//g' sqlite3.c 2>/dev/null || \
		sed -i 's/#include <sys\/ioctl.h>/\/* #include <sys\/ioctl.h> *\//g' sqlite3.c; \
	fi
	@if [ -f shell.c ]; then \
		sed -i.bak 's/# include <sys\/ioctl.h>/\/* # include <sys\/ioctl.h> *\//g' shell.c 2>/dev/null || \
		sed -i 's/# include <sys\/ioctl.h>/\/* # include <sys\/ioctl.h> *\//g' shell.c; \
	fi

# 1. ソースコードから ELF ファイルをビルド
$(TARGET_ELF): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) $(LIBS) -o $(TARGET_ELF)

# 2. ELF ファイルから X68k 用バイナリ (.x) へ変換
$(TARGET_X): $(TARGET_ELF)
	$(ELF2X68) -s -o $(TARGET_X) $(TARGET_ELF)
	@echo "======================================================"
	@echo " 成功: $(TARGET_X) が正常に生成されました！"
	@echo "======================================================"

# クリーンアップ（パッチ適用時のバックアップファイル .bak も一緒に消去します）
clean:
	rm -f $(TARGET_ELF) $(TARGET_X) *.bak

.PHONY: all clean patch
