//====================================================================
// sqlite3         : SQLite3 for X680x0
//--------------------------------------------------------------------
// 処理内容        : コマンドラインシェル（軽量版）
// ﾌｧｲﾙ名          : minshell.c
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
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

int main(int argc, char **argv) {
    sqlite3 *db;
    char *zErrMsg = 0;
    char query[256];
    const char *db_name = ":memory:";

    // 引数があれば、それをファイル名として使用
    if (argc > 1) {
        db_name = argv[1];
    }

    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("SQLite version 3.53.3  ");
    printf("(X680x0 version 0.26.6.27)\n");
    printf("Opened database: %s\n", db_name);
    printf("Type '.quit' to quit.\n\n");

    while (1) {
        printf("sqlite> ");
        if (!fgets(query, sizeof(query), stdin)) break;
        query[strcspn(query, "\n")] = 0;
        
        if (strcmp(query, ".quit") == 0) break;
        if (strlen(query) == 0) continue;

        if (sqlite3_exec(db, query, callback, 0, &zErrMsg) != SQLITE_OK) {
            fprintf(stderr, "SQL Error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }

    sqlite3_close(db);
    return 0;
}
