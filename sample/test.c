//====================================================================
// sqlite3_test    : SQLite3 Library Test for X680x0
//--------------------------------------------------------------------
// 処理内容        : スタティックライブラリのリンクテスト
// ﾌｧｲﾙ名          : test.c
// 開発環境        : GNU Compiler Collection (GCC) + Newlib
//                   クロス開発環境 (elf2x68k)
// 動作環境        : X680x0, Human68k
// 作成者          : Kenoh
// 作成日          : 2026/07/02
// 更新日          : 2026/07/02
// SQLite3 Version : 3.53.3
// X680x0 Version  : 0.26.7.2.03
//====================================================================
#include <stdio.h>
#include "../sqlite3.h"

// libsqlite3.a (stub.o) 内に内蔵されている
// 型自動判別・Shift-JIS文字コード自動変換・2パス等幅出力関数の外部参照宣言
extern int x68k_sqlite3_exec(sqlite3 *db, const char *zSqlSjis);

int main(int argc, char **argv) {
    sqlite3 *db;
    const char *db_name = "test.db";    // データベースファイル名

    printf("Connecting to %s (C Native Library)...\n", db_name);

    // 1. データベースをネイティブオープン
    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("Executing LEFT JOIN query with Auto-Width & Shift-JIS Convert...\n\n");
    printf("--- C Native SQLite3 Results (Static Link) ---\n");

    // 2. ライブラリ内蔵の最強関数を一発呼び出し
    const char *sql = "SELECT e.emp_id, e.emp_name, d.dept_name FROM employees AS e LEFT JOIN departments AS d ON e.dept_id = d.dept_id;";
    x68k_sqlite3_exec(db, sql);

    printf("----------------------------------------------\n");

    // 3. データベースをクローズ
    sqlite3_close(db);
    return 0;
}
