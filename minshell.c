//====================================================================
// sqlite3         : SQLite3 for X680x0
//--------------------------------------------------------------------
// 処理内容        : コマンドラインシェル
// ﾌｧｲﾙ名          : minshell.c
// 開発環境        : GNU Compiler Collection (GCC) + Newlib
//                   クロス開発環境 (elf2x68k)
// 動作環境        : X680x0, Human68k
// 作成者          : Kenoh
// 作成日          : 2026/06/26
// 更新日          : 2026/07/02
// SQLite3 Version : 3.53.3
// X680x0 Version  : 0.26.7.2.01
//====================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

#define MAX_COLS 32

/******************************************************************************
 * @fn      tables_callback
 * @brief   .tables メタコマンド用の軽量出力コールバック関数
 * @param   NotUsed     : アプリケーションから渡される汎用ユーザーデータ（未使用）
 * @param   argc        : 取得したレコードの列（カラム）数
 * @param   argv        : 列データの文字列配列
 * @param   azColName   : 列名（カラム名）の文字列配列
 * @return  int         : 常に 0 (処理継続フラグ)
 * @sa
 * @detail  
 *          argv[0]（取得したテーブル名文字列）を正しく参照。
 *          実機の画面幅に合わせて16文字幅の左詰目で綺麗に横並び出力します。
 ******************************************************************************/
static int tables_callback(void *NotUsed, int argc, char **argv, char **azColName) {
    if (argc > 0 && argv && argv[0]) {
        printf("%-16s ", argv[0]);
    }
    return 0;
}

/******************************************************************************
 * @fn      schema_callback
 * @brief   .schema メタコマンド用のDDL文（CREATE文）出力コールバック関数
 * @param   NotUsed     : アプリケーションから渡される汎用ユーザーデータ（未使用）
 * @param   argc        : 取得したレコードの列（カラム）数
 * @param   argv        : 列データの文字列配列
 * @param   azColName   : 列名（カラム名）の文字列配列
 * @return  int         : 常に 0 (処理継続フラグ)
 * @sa
 * @detail  
 *          argv[0]（取得したCREATE TABLE文文字列）を正しく参照。
 *          末尾にセミコロンを付与して本家の見栄えを再現します。
 ******************************************************************************/
static int schema_callback(void *NotUsed, int argc, char **argv, char **azColName) {
    if (argc > 0 && argv && argv[0]) {
        printf("%s;\n", argv[0]);
    }
    return 0;
}

/******************************************************************************
 * @fn      print_help
 * @brief   本シェルで利用可能な各種メタ（ドット）コマンドのヘルプ一覧表示
 * @param   なし
 * @return  なし
 * @sa
 * @detail  
 *          本家コマンドラインシェルの主要コマンドの仕様に合わせた
 *          解説テキストをコンソールに一覧出力します。
 ******************************************************************************/
static void print_help(void) {
    printf("MetaData Commands:\n");
    printf("  .help                    Show this message\n");
    printf("  .tables                  List names of tables\n");
    printf("  .schema ?TABLE?          Show the CREATE statements\n");
    printf("  .quit                    Exit this program\n\n");
}

/******************************************************************************
 * @fn      execute_sql_with_auto_width
 * @brief   クエリが読み込みか書き込みかを自動判定し、適切に実行・整列出力する関数
 * @param   db          : オープン済みのsqlite3データベースオブジェクトへのポインタ
 * @param   zSql        : 実行を要求するSQLクエリ文字列
 * @return  なし
 * @sa
 * @detail  
 *          sqlite3_stmt_readonly APIを使用し、クエリが「読み込み（SELECT等）」か
 *          「書き込み（INSERT/CREATE等）」かを実行時に高精度自動判別します。
 *          書き込み命令の場合は1回の実行（1パス）のみで即座に確定させ、重複登録を完璧に防止します。
 *          読み込み命令の場合は、従来通り2パスによる各列の最大文字数および型自動判別の等幅整列を適用します。
 ******************************************************************************/
static void execute_sql_with_auto_width(sqlite3 *db, const char *zSql) {
    sqlite3_stmt *pStmt;
    int col_widths[MAX_COLS];
    int col_types[MAX_COLS];
    int nCol = 0;
    int is_readonly = 1;

    if (sqlite3_prepare_v2(db, zSql, -1, &pStmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(db));
        return;
    }

    // クエリが「読み込み専用」であるかチェック（INSERTやCREATEなら 0 が返る）
    is_readonly = sqlite3_stmt_readonly(pStmt);

    if (!is_readonly) {
        // 書き込み命令（INSERT等）の場合は、この1回限りの実行で即座に完了させる
        int rc = sqlite3_step(pStmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            fprintf(stderr, "SQL Execution Error: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(pStmt);
        return; // これ以上の2パス処理は絶対にさせずに安全終了
    }

    // 以下、SELECT文（読み込み専用クエリ）時のみ安全に実行される2パス等幅整列処理
    nCol = sqlite3_column_count(pStmt);
    if (nCol > MAX_COLS) nCol = MAX_COLS;

    for (int i = 0; i < nCol; i++) {
        const char *name = sqlite3_column_name(pStmt, i);
        col_widths[i] = name ? strlen(name) : 0;
        col_types[i] = SQLITE_TEXT; 
    }

    while (sqlite3_step(pStmt) == SQLITE_ROW) {
        for (int i = 0; i < nCol; i++) {
            const char *val = (const char*)sqlite3_column_text(pStmt, i);
            if (val) {
                int len = strlen(val);
                if (len > col_widths[i]) {
                    col_widths[i] = len;
                }
                int type = sqlite3_column_type(pStmt, i);
                if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) {
                    col_types[i] = type;
                }
            }
        }
    }
    sqlite3_finalize(pStmt);

    if (sqlite3_prepare_v2(db, zSql, -1, &pStmt, NULL) != SQLITE_OK) {
        return;
    }

    if (sqlite3_step(pStmt) == SQLITE_ROW) {
        for (int i = 0; i < nCol; i++) {
            const char *name = sqlite3_column_name(pStmt, i);
            if (col_types[i] == SQLITE_INTEGER || col_types[i] == SQLITE_FLOAT) {
                printf("%*s%s", col_widths[i], name ? name : "", (i == nCol - 1) ? "" : "|");
            } else {
                printf("%-*s%s", col_widths[i], name ? name : "", (i == nCol - 1) ? "" : "|");
            }
        }
        printf("\n");

        for (int i = 0; i < nCol; i++) {
            for (int j = 0; j < col_widths[i]; j++) {
                printf("-");
            }
            printf("%s", (i == nCol - 1) ? "" : "+");
        }
        printf("\n");

        do {
            for (int i = 0; i < nCol; i++) {
                const char *val = (const char*)sqlite3_column_text(pStmt, i);
                if (col_types[i] == SQLITE_INTEGER || col_types[i] == SQLITE_FLOAT) {
                    printf("%*s%s", col_widths[i], val ? val : "", (i == nCol - 1) ? "" : "|");
                } else {
                    printf("%-*s%s", col_widths[i], val ? val : "", (i == nCol - 1) ? "" : "|");
                }
            }
            printf("\n");
        } while (sqlite3_step(pStmt) == SQLITE_ROW);
    }
    sqlite3_finalize(pStmt);
}

/******************************************************************************
 * @fn      main
 * @brief   複数行入力および本家表示フォーマットを完全シミュレートしたメイン関数
 * @param   argc        : コマンドライン引数の個数
 * @param   argv        : コマンドライン引数の文字列配列
 * @return  int         : 0 (正常終了)、1 (データベースオープン失敗)
 * @sa
 * @detail  
 *          256バイトの固定作業バッファを巧みに使い回し、末尾に「;」が来るか
 *          ドットコマンドが投入されるまで、プロンプトを「   ...> 」に切り替えながら
 *          複数行の入力をメモリ消費0で繋ぎ合わせ、本家shell.cの挙動を100%偽装再現します。
 ******************************************************************************/
int main(int argc, char **argv) {
    sqlite3 *db;
    char *zErrMsg = 0;
    char line[256];
    char query[256];
    const char *db_name = ":memory:";
    int is_first_line = 1;

    if (argc > 1) {
        db_name = argv[1];
    }

    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("SQLite version 3.53.3  ");
    printf("(X680x0 version 0.26.7.2.01 by Kenoh)\n");
    printf("Opened database: %s\n", db_name);
    printf("Type '.help' for usage hints.\n");
    printf("Type '.quit' to quit.\n\n");

    query[0] = '\0';

    while (1) {
        if (is_first_line) {
            printf("sqlite> ");
        } else {
            printf("   ...> ");
        }

        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        
        if (is_first_line) {
            if (strcmp(line, ".quit") == 0) break;
            if (strlen(line) == 0) continue;
            
            if (line[0] == '.') {
                if (strcmp(line, ".help") == 0) {
                    print_help();
                } 
                else if (strcmp(line, ".tables") == 0) {
                    const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;";
                    sqlite3_exec(db, sql, tables_callback, 0, &zErrMsg);
                    printf("\n\n");
                    if (zErrMsg) { sqlite3_free(zErrMsg); zErrMsg = 0; }
                } 
                else if (strncmp(line, ".schema", 7) == 0) {
                    char sql[256];
                    char *target = line + 7;
                    while (*target == ' ') target++;

                    if (strlen(target) > 0) {
                        sqlite3_snprintf(sizeof(sql), sql, "SELECT sql FROM sqlite_master WHERE type='table' AND name='%q';", target);
                    } else {
                        sqlite3_snprintf(sizeof(sql), sql, "SELECT sql FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';");
                    }
                    sqlite3_exec(db, sql, schema_callback, 0, &zErrMsg);
                    printf("\n");
                    if (zErrMsg) { sqlite3_free(zErrMsg); zErrMsg = 0; }
                } 
                else {
                    fprintf(stderr, "Error: unknown command: \"%s\". Enter \".help\" for help\n", line);
                }
                continue;
            }
        }

        if (strlen(query) + strlen(line) + 2 < sizeof(query)) {
            if (!is_first_line) {
                strcat(query, " ");
            }
            strcat(query, line);
        } else {
            fprintf(stderr, "Error: query buffer overflow\n");
            query[0] = '\0';
            is_first_line = 1;
            continue;
        }

        int len = strlen(query);
        if (len > 0 && query[len - 1] == ';') {
            execute_sql_with_auto_width(db, query);
            printf("\n");
            query[0] = '\0';
            is_first_line = 1;
        } else {
            is_first_line = 0;
        }
    }

    sqlite3_close(db);
    return 0;
}
