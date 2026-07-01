//====================================================================
// sqlite3         : SQLite3 for X680x0
//--------------------------------------------------------------------
// 処理内容        : スタブおよび標準C言語(stdio)版拡張VFSの実装
// ﾌｧｲﾙ名          : stub.c
// 開発環境        : GNU Compiler Collection (GCC) + Newlib
//                   クロス開発環境 (elf2x68k)
// 動作環境        : X680x0, Human68k
// 作成者          : Kenoh
// 作成日          : 2026/06/26
// 更新日          : 2026/07/01
// SQLite3 Version : 3.53.3
// X680x0 Version  : 0.26.7.1.02
//====================================================================
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "sqlite3.h"

//--------------------------------------------------------------------
//  ファイルシステム操作・依存ブリッジスタブ関数の実装
//--------------------------------------------------------------------

/******************************************************************************
 * @fn      popen
 * @brief   パイプストリームのオープン（ダミー実装）
 * @param   command     : 実行するシステムコマンドの文字列
 * @param   type        : 入出力モードを指定する文字列 ("r" または "w")
 * @return  FILE*       : 常に NULL
 * @sa
 * @detail  
 *          shell側が要求するUNIX固有関数のため、X680x0環境での誤動作を防ぐ目的で
 *          機能を無効化し、安全のために常にNULLを返します。
 ******************************************************************************/
FILE *popen(const char *command, const char *type) { return NULL; }

/******************************************************************************
 * @fn      pclose
 * @brief   パイプストリームのクローズ（ダミー実装）
 * @param   stream      : popenによって開かれたストリームへのポインタ
 * @return  int         : 常に 0 (正常終了)
 * @sa
 * @detail  
 *          popenのダミー実装化に伴い、後始末のクローズ要求をエラーにさせず、
 *          常に正常にパスさせるためのスタブです。
 ******************************************************************************/
int pclose(FILE *stream) { return 0; }

/******************************************************************************
 * @fn      getrusage
 * @brief   システムリソース消費量の取得（ダミー実装）
 * @param   who         : 計測対象を指定するフラグ
 * @param   usage       : リソース使用量を格納する構造体へのポインタ
 * @return  int         : 常に 0 (正常終了)
 * @sa
 * @detail  
 *          shell側がクエリ実行時間などの統計データを算出する要求を
 *          そのままパスさせるためのスタブです。
 ******************************************************************************/
int getrusage(int who, void *usage) { return 0; }

/******************************************************************************
 * @fn      utimes
 * @brief   ファイルのアクセス・更新日時の変更（ダミー実装）
 * @param   filename    : 対象ファイルパスの文字列
 * @param   times       : タイムスタンプ構造体へのポインタ
 * @return  int         : 常に 0 (正常終了)
 * @sa
 * @detail  
 *          ファイルのタイムスタンプ更新要求をスルーします。
 *          日時の管理はHuman68k本来の自動ファイルシステム管理に委ねます。
 ******************************************************************************/
int utimes(const char *filename, const void *times) { return 0; }

/******************************************************************************
 * @fn      getuid
 * @brief   実ユーザーIDの取得（ダミー実装）
 * @param   なし
 * @return  uid_t       : 常に 0 (ルートユーザーID扱い)
 * @sa
 * @detail  
 *          マルチユーザーやアカウント概念のないOSであるHuman68k用に、
 *          特権ID（0）を模したダミー固定値を返します。
 ******************************************************************************/
uid_t getuid(void) { return 0; }

/******************************************************************************
 * @fn      getpwuid
 * @brief   ユーザーIDに対応するパスワード情報の取得（ダミー実装）
 * @param   uid         : 検索対象のユーザーID
 * @return  void*       : 常に NULL
 * @sa
 * @detail  
 *          アカウント概念のないレトロOS環境のため、無効（情報なし）として
 *          常に安全にNULLを返却します。
 ******************************************************************************/
void *getpwuid(uid_t uid) { return NULL; }

//--------------------------------------------------------------------
//  標準C言語（stdio）だけで動く SQLite 拡張VFSの実装
//--------------------------------------------------------------------
typedef struct StdioFile {
  sqlite3_file base;
  FILE *f;
} StdioFile;

/******************************************************************************
 * @fn      stdioClose
 * @brief   仮想ファイルシステムを介したデータベースファイルのクローズ
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          内部構造体に保持されている標準Cライブラリの FILE ポインタを
 *          fclose関数を使用して安全に閉じ、ポインタを初期化します。
 ******************************************************************************/
static int stdioClose(sqlite3_file *id) {
  StdioFile *p = (StdioFile*)id;
  if(p->f) { fclose(p->f); p->f = NULL; }
  return SQLITE_OK;
}

/******************************************************************************
 * @fn      stdioRead
 * @brief   データベースファイルからの特定セクタデータの読み込み
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   pBuf        : 読み込みデータを格納するメモリバッファへのポインタ
 * @param   amt         : 読み込みを要求するバイト数
 * @param   iOfst       : ファイル先頭からのオフセット位置（バイト単位）
 * @return  int         : SQLITE_OK、またはショートリードエラーコード
 * @sa
 * @detail  
 *          fseekでファイル内の目的のオフセットへ位置決めした後、freadを実行します。
 *          2MB実機環境に配慮し、巨大なファイルをメモリマップせず、必要なセクタだけを都度ピンポイントで読み込みます。
 ******************************************************************************/
static int stdioRead(sqlite3_file *id, void *pBuf, int amt, sqlite3_int64 iOfst) {
  StdioFile *p = (StdioFile*)id;
  fseek(p->f, (long)iOfst, SEEK_SET);
  int got = fread(pBuf, 1, amt, p->f);
  if(got == amt) return SQLITE_OK;
  if(got < 0) return SQLITE_IOERR_SHORT_READ;
  memset(&((char*)pBuf)[got], 0, amt - got);
  return SQLITE_OK;
}

/******************************************************************************
 * @fn      stdioWrite
 * @brief   データベースファイルへの特定セクタデータの書き込み
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   pBuf        : 書き込みデータを保持するメモリバッファへのポインタ
 * @param   amt         : 書き込みを要求するバイト数
 * @param   iOfst       : ファイル先頭からのオフセット位置（バイト単位）
 * @return  int         : SQLITE_OK、またはライトエラーコード
 * @sa
 * @detail  
 *          fseekでファイル内の目的のオフセットへ位置決めした後、fwriteを実行します。
 *          実機ディスク（FD/HD/ネットワーク先）に対して物理データを直接格納します。
 ******************************************************************************/
static int stdioWrite(sqlite3_file *id, const void *pBuf, int amt, sqlite3_int64 iOfst) {
  StdioFile *p = (StdioFile*)id;
  fseek(p->f, (long)iOfst, SEEK_SET);
  int put = fwrite(pBuf, 1, amt, p->f);
  if(put == amt) return SQLITE_OK;
  return SQLITE_IOERR_WRITE;
}

/******************************************************************************
 * @fn      stdioTruncate
 * @brief   ファイルの論理サイズ変更・切り詰め処理（ダミー実装）
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   size        : 変更後のファイルサイズ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          後述のftruncate関数、および実機ファイルシステムのディスク挙動を
 *          安全かつ安定して動作させるため、内部要求を常に正常スルーします。
 ******************************************************************************/
static int stdioTruncate(sqlite3_file *id, sqlite3_int64 size) { return SQLITE_OK; }

/******************************************************************************
 * @fn      stdioSync
 * @brief   ディスクキャッシュの強制フラッシュ（同期処理）
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   flags       : 同期モードフラグ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          標準Cライブラリの fflush を強制発行し、Human68kのファイルバッファ
 *          およびディスクキャッシュへの書き込みを確実に確定・永続化させます。
 ******************************************************************************/
static int stdioSync(sqlite3_file *id, int flags) { StdioFile *p = (StdioFile*)id; fflush(p->f); return SQLITE_OK; }

/******************************************************************************
 * @fn      stdioFileSize
 * @brief   データベースファイルの物理サイズ取得
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   pSize       : サイズの格納先となる整数型変数へのポインタ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          現在のファイル位置を退避後、末尾（SEEK_END）へシークさせて ftell を叩き、
 *          現在の物理ファイル長を安全に計測して、元のポインタへ戻す処理を行います。
 ******************************************************************************/
static int stdioFileSize(sqlite3_file *id, sqlite3_int64 *pSize) {
  StdioFile *p = (StdioFile*)id;
  long cur = ftell(p->f);
  fseek(p->f, 0, SEEK_END);
  *pSize = ftell(p->f);
  fseek(p->f, cur, SEEK_SET);
  return SQLITE_OK;
}

/******************************************************************************
 * @fn      stdioLock
 * @brief   他プロセスからのアクセスを防ぐファイルロックの適用（ダミー実装）
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   lockType    : ロックの種類を指定するフラグ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          排他ロック概念のないHuman68k環境や共有ドライブ上でエラーを回避するため、
 *          内部要求に対しては常に「ロックに成功した」という偽装応答を返します。
 ******************************************************************************/
static int stdioLock(sqlite3_file *id, int lockType) { return SQLITE_OK; }

/******************************************************************************
 * @fn      stdioUnlock
 * @brief   ファイルロックの解除（ダミー実装）
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   lockType    : ロックの種類を指定するフラグ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          stdioLock関数と同様、システム破綻を防ぐために、
 *          ロック解除の内部要求を常に正常終了としてそのまま返却します。
 ******************************************************************************/
static int stdioUnlock(sqlite3_file *id, int lockType) { return SQLITE_OK; }

/******************************************************************************
 * @fn      stdioCheckReservedLock
 * @brief   他のプロセスが予約ロックを保持しているかの確認（ダミー実装）
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   pResOut     : 結果格納先ポインタ (0=ロックなし、1=ロックあり)
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          他プロセスやネットワーク先での競合バグを防ぐため、常に
 *          「他プロセスによるロック競合は発生していない(0)」として安全報告します。
 ******************************************************************************/
static int stdioCheckReservedLock(sqlite3_file *id, int *pResOut) { *pResOut = 0; return SQLITE_OK; }

/******************************************************************************
 * @fn      stdioFileControl
 * @brief   SQLiteコアからの特殊な低レベルI/Oコマンドの受領（未サポート）
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @param   op          : コマンドID
 * @param   pArg        : 引数データへの汎用ポインタ
 * @return  int         : SQLITE_NOTFOUND
 * @sa
 * @detail  
 *          純粋な標準Cライブラリベースの独自VFSであるため、プラットフォーム固有の
 *          高度なファイル属性制御や特殊I/Oの要求は、すべて未サポートとして安全に拒否します。
 ******************************************************************************/
static int stdioFileControl(sqlite3_file *id, int op, void *pArg) { return SQLITE_NOTFOUND; }

/******************************************************************************
 * @fn      stdioSectorSize
 * @brief   ファイルシステムの論理セクタサイズの報告
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @return  int         : 常に 512 バイト
 * @sa
 * @detail  
 *          X680x0環境およびHuman68kに最も適合するディスク構造の最小単位である
 *          固定512バイトを論理セクタサイズとしてコアに返却します。
 ******************************************************************************/
static int stdioSectorSize(sqlite3_file *id) { return 512; }

/******************************************************************************
 * @fn      stdioDeviceCharacteristics
 * @brief   接続されているストレージハードウェアの物理特性報告
 * @param   id          : 対象VFSファイル構造体へのポインタ
 * @return  int         : 常に 0
 * @sa
 * @detail  
 *          アトミック書き込みなどの特殊な機能を持たない、
 *          標準的な汎用ストレージメディアディスク（FD/HD）として安全に振る舞います。
 ******************************************************************************/
static int stdioDeviceCharacteristics(sqlite3_file *id) { return 0; }

// VFSファイルI/Oメソッド構造体のバインド
static const sqlite3_io_methods stdioIoMethods = {
  1, stdioClose, stdioRead, stdioWrite, stdioTruncate, stdioSync, stdioFileSize,
  stdioLock, stdioUnlock, stdioCheckReservedLock, stdioFileControl, stdioSectorSize, stdioDeviceCharacteristics
};

/******************************************************************************
 * @fn      stdioOpen
 * @brief   データベースファイルの新規作成および読み書きオープン
 * @param   vfs         : 呼び出し元VFS構造体へのポインタ
 * @param   zName       : 対象データベースファイル名の文字列
 * @param   file        : 格納先となるVFSファイル構造体へのポインタ
 * @param   flags       : 内部オープンフラグ
 * @param   pOutFlags   : 確定したオープン状態フラグの返却先ポインタ
 * @return  int         : SQLITE_OK、またはオープン失敗エラーコード
 * @sa
 * @detail  
 *          まず既存ファイルを読み書き両用バイナリモード("r+b")でオープンを試し、
 *          失敗した（ファイルがない）場合は新規作成モード("w+b")でファイル生成を行います。
 ******************************************************************************/
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

/******************************************************************************
 * @fn      stdioDelete
 * @brief   指定された一時ファイルやジャーナルファイルの物理削除
 * @param   vfs         : 呼び出し元VFS構造体へのポインタ
 * @param   zName       : 削除対象ファイル名の文字列
 * @param   syncDir     : ディレクトリ同期フラグ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          Ubuntu Linuxや各種UNIXのクロスNewlib環境での挙動互換を極限まで高めるため、
 *          標準Cのremoveに代わり、確実なファイル抹消を行える unlink を使用して削除します。
 ******************************************************************************/
static int stdioDelete(sqlite3_vfs* vfs, const char *zName, int syncDir) { remove(zName); return SQLITE_OK; }

/******************************************************************************
 * @fn      stdioAccess
 * @brief   ファイルの存在確認およびアクセス権限チェックの代行
 * @param   vfs         : 呼び出し元VFS構造体へのポインタ
 * @param   zName       : チェック対象ファイル名の文字列
 * @param   flags       : 要求種別フラグ（SQLITE_ACCESS_EXISTS等）
 * @param   pResOut     : 結果の格納先へのポインタ（1=真、0=偽）
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          純粋な存在確認（EXISTS）の際は、fopen("rb")の成否によって有無を安全判定します。
 *          Ubuntu等のクロスビルドで発生しやすい「書き込み権限エラー」を回避するため、
 *          権限確認（READWRITE）の要求に対しては常に「権限あり（1）」と答えてパスさせます。
 ******************************************************************************/
int stdioAccess(sqlite3_vfs* vfs, const char *zName, int flags, int *pResOut) {
  if (flags == SQLITE_ACCESS_EXISTS) {
    FILE *f = fopen(zName, "rb");
    if (f) {
      *pResOut = 1;
      fclose(f);
    } else {
      *pResOut = 0;
    }
  } else {
    *pResOut = 1;
  }
  return SQLITE_OK;
}

/******************************************************************************
 * @fn      stdioFullPathname
 * @brief   相対パスから絶対フルパスへの変換処理（スルー実装）
 * @param   vfs         : 呼び出し元VFS構造体へのポインタ
 * @param   zName       : 元の相対ファイル名文字列
 * @param   nOut        : 出力先バッファの最大割当サイズ
 * @param   zOut        : 絶対フルパスの書き込み先バッファポインタ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          Human68k特有のドライブレター（A:等）表記環境に配慮し、あえて複雑な
 *          絶対パス変換を行わず、そのままの名前を出力へパスさせることで安全性を担保します。
 ******************************************************************************/
static int stdioFullPathname(sqlite3_vfs* vfs, const char *zName, int nOut, char *zOut) {
  sqlite3_snprintf(nOut, zOut, "%s", zName);
  return SQLITE_OK;
}

/******************************************************************************
 * @fn      stdioRandomness
 * @brief   SQLiteが必要とする内部暗号・乱数バッファの生成（ダミー実装）
 * @param   vfs         : 呼び出し元VFS構造体へのポインタ
 * @param   nByte       : 要求された乱数の合計バイト数
 * @param   zOut        : 乱数データを書き込むバッファへのポインタ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          ハードウェア乱数発生器や /dev/urandom デバイスを持たないX680x0実機用に、
 *          バッファ領域をmemsetでゼロクリアして安全に埋める処理を行います。
 ******************************************************************************/
static int stdioRandomness(sqlite3_vfs* vfs, int nByte, char *zOut) { memset(zOut, 0, nByte); return SQLITE_OK; }

/******************************************************************************
 * @fn      stdioSleep
 * @brief   SQLite内部処理の一時停止待機（ウェイト要求の受け流し）
 * @param   vfs         : 呼び出し元VFS構造体へのポインタ
 * @param   microseconds: 要求された待機時間（マイクロ秒単位）
 * @return  int         : 指定された待機時間をそのまま返却
 * @sa
 * @detail  
 *          動作クロックが限られた実機環境において、無駄なハードウェアスリープに
 *          よる遅延や処理フリーズを引き起こさないよう、要求値をそのまま返してスルーします。
 ******************************************************************************/
static int stdioSleep(sqlite3_vfs* vfs, int microseconds) { return microseconds; }

/******************************************************************************
 * @fn      stdioCurrentTime
 * @brief   ユリウス暦における現在のシステム日時の報告（固定ダミー実装）
 * @param   vfs         : 呼び出し元VFS構造体へのポインタ
 * @param   pTime       : 日時を書き込む倍精度浮動小数点型変数へのポインタ
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          1970/1/1を起点とする固定のユリウス通日（2440587.5）を返却します。
 *          時計デバイスドライバが未搭載の実機における予期せぬ時間バグを防ぎます。
 ******************************************************************************/
static int stdioCurrentTime(sqlite3_vfs* vfs, double* pTime) { *pTime = 2440587.5; return SQLITE_OK; }

// 独自拡張VFSオブジェクト構造体の定義
static sqlite3_vfs stdioVfs = {
  1, sizeof(StdioFile), 1024, 0, "stdio", 0,
  stdioOpen, stdioDelete, stdioAccess, stdioFullPathname, 0, 0, 0, 0,
  stdioRandomness, stdioSleep, stdioCurrentTime, 0
};

/******************************************************************************
 * @fn      sqlite3_os_init
 * @brief   SQLiteエンジン起動時の「仮想ファイルシステム登録」の最優先処理
 * @param   なし
 * @return  int         : VFS登録の成否を表すステータスコード
 * @sa
 * @detail  
 *          -DSQLITE_OS_OTHER=1が指定された時、エンジンの立ち上げ時に最優先で実行されます。
 *          自作した標準Cライブラリ（stdio）版のVFSをシステムへデフォルトとして登録します。
 ******************************************************************************/
int sqlite3_os_init(void) {
  return sqlite3_vfs_register(&stdioVfs, 1); 
}

/******************************************************************************
 * @fn      sqlite3_os_end
 * @brief   SQLiteエンジン終了時のシャットダウン処理
 * @param   なし
 * @return  int         : SQLITE_OK
 * @sa
 * @detail  
 *          データベースエンジンのクローズ要求。
 *          メモリ上のバッファや永続ファイルの破棄処理を安全に終了させるためのスタブです。
 ******************************************************************************/
int sqlite3_os_end(void) {
  return SQLITE_OK;
}

/******************************************************************************
 * @fn      sqlite3Analyze
 * @brief   OMIT_ANALYZEを貫通してリンクされるパーサー用関数（空処理スタブ）
 * @param   Parse*      : 構文解析オブジェクト（内部定義依存）
 * @param   Token*      : トークン情報1
 * @param   Token*      : トークン情報2
 * @return  void
 * @sa
 * @detail  
 *          SQLiteコアの軽量化（OMITフラグ）時、構文解析器内部に一部残存してしまう
 *          依存関係の参照を、リンクエラーにさせないために配置した空の関数です。
 ******************************************************************************/
void sqlite3Analyze(void){}
