//====================================================================
// sqlite3         : SQLite3 for X680x0
//--------------------------------------------------------------------
// 処理内容        : スタブ
// ﾌｧｲﾙ名          : stub.c
// 開発環境        : GNU Compiler Collection (GCC) + Newlib
//                   クロス開発環境 (elf2x68k)
// 動作環境        : X680x0, Human68k
// 作成者          : Kenoh
// 作成日          : 2026/06/26
// 更新日          : 2026/06/27
// SQLite3 Version : 3.53.3
// X680x0 Version  : 0.26.6.27
//====================================================================
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "sqlite3.h"

// 基本的なシステムスタブ
FILE *popen(const char *command, const char *type) { return NULL; }
int pclose(FILE *stream) { return 0; }
int getrusage(int who, void *usage) { return 0; }
int utimes(const char *filename, const void *times) { return 0; }
uid_t getuid(void) { return 0; }
void *getpwuid(uid_t uid) { return NULL; }

// 標準C言語（stdio）だけで動く SQLite 拡張VFSの実装
typedef struct StdioFile {
  sqlite3_file base;
  FILE *f;
} StdioFile;

static int stdioClose(sqlite3_file *id) {
  StdioFile *p = (StdioFile*)id;
  if(p->f) { fclose(p->f); p->f = NULL; }
  return SQLITE_OK;
}
static int stdioRead(sqlite3_file *id, void *pBuf, int amt, sqlite3_int64 iOfst) {
  StdioFile *p = (StdioFile*)id;
  fseek(p->f, (long)iOfst, SEEK_SET);
  int got = fread(pBuf, 1, amt, p->f);
  if(got == amt) return SQLITE_OK;
  if(got < 0) return SQLITE_IOERR_SHORT_READ;
  memset(&((char*)pBuf)[got], 0, amt - got);
  return SQLITE_OK;
}
static int stdioWrite(sqlite3_file *id, const void *pBuf, int amt, sqlite3_int64 iOfst) {
  StdioFile *p = (StdioFile*)id;
  fseek(p->f, (long)iOfst, SEEK_SET);
  int put = fwrite(pBuf, 1, amt, p->f);
  if(put == amt) return SQLITE_OK;
  return SQLITE_IOERR_WRITE;
}
static int stdioTruncate(sqlite3_file *id, sqlite3_int64 size) { return SQLITE_OK; }
static int stdioSync(sqlite3_file *id, int flags) { StdioFile *p = (StdioFile*)id; fflush(p->f); return SQLITE_OK; }
static int stdioFileSize(sqlite3_file *id, sqlite3_int64 *pSize) {
  StdioFile *p = (StdioFile*)id;
  long cur = ftell(p->f);
  fseek(p->f, 0, SEEK_END);
  *pSize = ftell(p->f);
  fseek(p->f, cur, SEEK_SET);
  return SQLITE_OK;
}
static int stdioLock(sqlite3_file *id, int lockType) { return SQLITE_OK; }
static int stdioUnlock(sqlite3_file *id, int lockType) { return SQLITE_OK; }
static int stdioCheckReservedLock(sqlite3_file *id, int *pResOut) { *pResOut = 0; return SQLITE_OK; }
static int stdioFileControl(sqlite3_file *id, int op, void *pArg) { return SQLITE_NOTFOUND; }
static int stdioSectorSize(sqlite3_file *id) { return 512; }
static int stdioDeviceCharacteristics(sqlite3_file *id) { return 0; }

static const sqlite3_io_methods stdioIoMethods = {
  1, stdioClose, stdioRead, stdioWrite, stdioTruncate, stdioSync, stdioFileSize,
  stdioLock, stdioUnlock, stdioCheckReservedLock, stdioFileControl, stdioSectorSize, stdioDeviceCharacteristics
};

static int stdioOpen(sqlite3_vfs* vfs, const char *zName, sqlite3_file* file, int flags, int *pOutFlags) {
  StdioFile *p = (StdioFile*)file;
  if(zName == NULL) return SQLITE_CANTOPEN;
  FILE *f = fopen(zName, "r+b");
  if(f == NULL) {
    f = fopen(zName, "w+b");
  }
  if(f == NULL) return SQLITE_CANTOPEN;
  p->f = f;
  p->base.pMethods = &stdioIoMethods;
  if(pOutFlags) *pOutFlags = flags;
  return SQLITE_OK;
}
static int stdioDelete(sqlite3_vfs* vfs, const char *zName, int syncDir) { remove(zName); return SQLITE_OK; }

// 【★修正】未定義だった access 関数を、標準Cの fopen で完璧にエミュレート
int stdioAccess(sqlite3_vfs* vfs, const char *zName, int flags, int *pResOut) {
  FILE *f = fopen(zName, "rb");
  if (f) {
    *pResOut = 1; // ファイルが存在
    fclose(f);
  } else {
    *pResOut = 0; // ファイルが存在しない
  }
  return SQLITE_OK;
}

static int stdioFullPathname(sqlite3_vfs* vfs, const char *zName, int nOut, char *zOut) {
  sqlite3_snprintf(nOut, zOut, "%s", zName);
  return SQLITE_OK;
}
static int stdioRandomness(sqlite3_vfs* vfs, int nByte, char *zOut) { memset(zOut, 0, nByte); return SQLITE_OK; }
static int stdioSleep(sqlite3_vfs* vfs, int microseconds) { return microseconds; }
static int stdioCurrentTime(sqlite3_vfs* vfs, double* pTime) { *pTime = 2440587.5; return SQLITE_OK; }

static sqlite3_vfs stdioVfs = {
  1, sizeof(StdioFile), 1024, 0, "stdio", 0,
  stdioOpen, stdioDelete, stdioAccess, stdioFullPathname, 0, 0, 0, 0,
  stdioRandomness, stdioSleep, stdioCurrentTime, 0
};

int sqlite3_os_init(void) {
  return sqlite3_vfs_register(&stdioVfs, 1); 
}
int sqlite3_os_end(void) {
  return SQLITE_OK;
}

void sqlite3Analyze(void){}
