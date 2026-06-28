//====================================================================
// sqlite3         : SQLite3 for X680x0
//--------------------------------------------------------------------
// 処理内容        : コマンドラインシェル（軽量版・メタコマンド追加）
// ﾌｧｲﾙ名          : minshell.c
// 開発環境        : GNU Compiler Collection (GCC) + Newlib
//                   クロス開発環境 (elf2x68k)
// 動作環境        : X680x0, Human68k
// 作成者          : Kenoh
// 作成日          : 2026/06/26
// 更新日          : 2026/06/28
// SQLite3 Version : 3.53.3
// X680x0 Version  : 0.26.6.28.01
//====================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

// SQLの通常実行結果を表示するコールバック
static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

// .tables コマンド用の軽量コールバック（テーブル名だけを横に並べて表示）
static int tables_callback(void *NotUsed, int argc, char **argv, char **azColName) {
    if (argc > 0 && argv[0]) {
        printf("%-16s ", argv[0]);
    }
    return 0;
}

// .schema コマンド用の軽量コールバック（CREATE文だけをそのまま表示）
static int schema_callback(void *NotUsed, int argc, char **argv, char **azColName) {
    if (argc > 0 && argv[0]) {
        printf("%s;\n", argv[0]);
    }
    return 0;
}

// ヘルプメッセージの表示関数
static void print_help(void) {
    printf("MetaData Commands:\n");
    printf("  .help                    Show this message\n");
    printf("  .tables                  List names of tables\n");
    printf("  .schema ?TABLE?          Show the CREATE statements\n");
    printf("  .quit                    Exit this program\n\n");
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
    printf("(X680x0 version 0.26.6.28.01)\n");
    printf("Opened database: %s\n", db_name);
    printf("Type '.help' for usage hints.\n");
    printf("Type '.quit' to quit.\n\n");

    while (1) {
        printf("sqlite> ");
        if (!fgets(query, sizeof(query), stdin)) break;
        query[strcspn(query, "\n")] = 0;
        
        if (strcmp(query, ".quit") == 0) break;
        if (strlen(query) == 0) continue;

        // --- 【★追加】メタコマンド（ドットコマンド）の判定処理 ---
        if (query[0] == '.') {
            if (strcmp(query, ".help") == 0) {
                print_help();
            } 
            else if (strcmp(query, ".tables") == 0) {
                // SQLiteのシステムテーブル（sqlite_master）からテーブル名だけを取得
                const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;";
                sqlite3_exec(db, sql, tables_callback, 0, &zErrMsg);
                printf("\n\n");
                if (zErrMsg) { sqlite3_free(zErrMsg); zErrMsg = 0; }
            } 
            else if (strncmp(query, ".schema", 7) == 0) {
                char sql[256];
                char *target = query + 7;
                // 空白を飛ばす
                while (*target == ' ') target++;

                if (strlen(target) > 0) {
                    // 特定のテーブルのみ指定された場合
                    sqlite3_snprintf(sizeof(sql), sql, "SELECT sql FROM sqlite_master WHERE type='table' AND name='%q';", target);
                } else {
                    // 全テーブルを対象とする場合
                    sqlite3_snprintf(sizeof(sql), sql, "SELECT sql FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';");
                }
                sqlite3_exec(db, sql, schema_callback, 0, &zErrMsg);
                printf("\n");
                if (zErrMsg) { sqlite3_free(zErrMsg); zErrMsg = 0; }
            } 
            else {
                fprintf(stderr, "Error: unknown command or invalid arguments: \"%s\". Enter \".help\" for help\n", query);
            }
            continue;
        }

        // 通常のSQL文を実行
        if (sqlite3_exec(db, query, callback, 0, &zErrMsg) != SQLITE_OK) {
            fprintf(stderr, "SQL Error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }

    sqlite3_close(db);
    return 0;
}
