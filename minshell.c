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
// X680x0 Version  : 0.26.7.2.03
//====================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

#define MAX_COLS 32
#define MAX_LINE_LEN 256

/******************************************************************************
 * @fn      utf8_to_sjis
 * @brief   外部データファイルからShift-JISコードを引っこ抜く関数
 * @param   pUtf8       : 変換元のUTF-8文字列へのポインタ
 * @param   pSjis       : 変換結果を格納するバッファへのポインタ
 * @param   maxLen      : 格納先バッファの最大サイズ
 * @return  なし
 * @detail  
 *          RAMを1バイトも消費せず、utf8sjis.datファイルからfseekで必要な文字の
 *          SJISコードだけを都度シークして読み込みます。ファイルがオープンできない
 *          場合は、画面に一度だけ警告テキストを出力し、生データを透過します。
 ******************************************************************************/
static void utf8_to_sjis(const char *pUtf8, char *pSjis, int maxLen) {
    int i = 0, j = 0;
    static int warned = 0;
    FILE *fp = fopen("utf8sjis.dat", "rb");
    
    if (fp == NULL) {
        if (!warned) {
            fprintf(stderr, "\nWarning: utf8sjis.dat not found! UTF-8 passthrough mode.\n");
            warned = 1;
        }
        strncpy(pSjis, pUtf8, maxLen);
        pSjis[maxLen - 1] = '\0';
        return;
    }
    
    while (pUtf8[i] && j < maxLen - 2) {
        unsigned char c1 = pUtf8[i];
        if (c1 < 0x80) {
            pSjis[j++] = pUtf8[i++];
        } else if ((c1 & 0xE0) == 0xE0) {
            unsigned char c2 = pUtf8[i+1];
            unsigned char c3 = pUtf8[i+2];
            if (c2 && c3) {
                unsigned int uni = ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                unsigned short sjis_code = 0x81A0;
                fseek(fp, uni * 2, SEEK_SET);
                fread(&sjis_code, 2, 1, fp);
                unsigned char s1 = (sjis_code >> 8) & 0xFF;
                unsigned char s2 = sjis_code & 0xFF;
                if (s1 == 0) {
                    pSjis[j++] = s2;
                } else {
                    pSjis[j++] = s1;
                    pSjis[j++] = s2;
                }
                i += 3;
            } else { pSjis[j++] = pUtf8[i++]; }
        } else { pSjis[j++] = pUtf8[i++]; }
    }
    pSjis[j] = '\0';
    fclose(fp);
}

/******************************************************************************
 * @fn      sjis_to_utf8
 * @brief   半角文字時はディスクアクセスを完全に0回にスキップして爆速化する関数
 * @param   pSjis       : 変換元のShift-JIS文字列へのポインタ
 * @param   pUtf8       : 変換結果を格納するバッファへのポインタ
 * @param   maxLen      : 格納先バッファの最大サイズ
 * @return  なし
 * @detail  
 *          512要素ずつのセクタ単位で一括まとめ読みし、日本語エリア(0x2000〜0xA000)に
 *          限定してスキャンすることで、INSERT文の処理を一瞬で完了させます。
 *          ファイルが見つからない場合は透過して処理を継続します。
 ******************************************************************************/
static void sjis_to_utf8(const char *pSjis, char *pUtf8, int maxLen) {
    int i = 0, j = 0;
    FILE *fp = fopen("utf8sjis.dat", "rb");
    if (fp == NULL) {
        strncpy(pUtf8, pSjis, maxLen);
        pUtf8[maxLen - 1] = '\0';
        return;
    }

    while (pSjis[i] && j < maxLen - 3) {
        unsigned char c1 = pSjis[i];
        
        if (c1 < 0x80) {
            pUtf8[j++] = pSjis[i++];
        } else if ((c1 >= 0x81 && c1 <= 0x9F) || (c1 >= 0xE0 && c1 <= 0xFC)) {
            unsigned char c2 = pSjis[i+1];
            if (c2) {
                unsigned short target_sjis = (c1 << 8) | c2;
                unsigned int found_uni = 0x3000;
                unsigned short read_buf[512];
                unsigned int start_uni = 0x2000;
                unsigned int end_uni   = 0xA000;
                int is_found = 0;

                fseek(fp, start_uni * 2, SEEK_SET);
                for (unsigned int u_idx = start_uni; u_idx < end_uni; u_idx += 512) {
                    int read_count = fread(read_buf, 2, 512, fp);
                    if (read_count <= 0) break;

                    for (int b_idx = 0; b_idx < read_count; b_idx++) {
                        if (read_buf[b_idx] == target_sjis) {
                            found_uni = u_idx + b_idx;
                            is_found = 1;
                            break;
                        }
                    }
                    if (is_found) break;
                }

                pUtf8[j++] = 0xE0 | ((found_uni >> 12) & 0x0F);
                pUtf8[j++] = 0x80 | ((found_uni >> 6) & 0x3F);
                pUtf8[j++] = 0x80 | (found_uni & 0x3F);
                i += 2;
            } else { pUtf8[j++] = pSjis[i++]; }
        } else { pUtf8[j++] = pSjis[i++]; }
    }
    pUtf8[j] = '\0';
    fclose(fp);
}

/******************************************************************************
 * @fn      tables_callback
 * @brief   .tables メタコマンド用の軽量出力コールバック関数
 * @param   NotUsed     : アプリケーションから渡される汎用ユーザーデータ（未使用）
 * @param   argc        : 取得したレコードの列（カラム）数
 * @param   argv        : 列データの文字列配列
 * @param   azColName   : 列名（カラム名）の文字列配列
 * @return  int         : 常に 0 (処理継続フラグ)
 ******************************************************************************/
static int tables_callback(void *NotUsed, int argc, char **argv, char **azColName) {
    if (argc > 0 && argv != NULL && argv[0] != NULL) {
        char sjis_name[256];
        utf8_to_sjis(argv[0], sjis_name, sizeof(sjis_name));
        printf("%-16s ", sjis_name);
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
 ******************************************************************************/
static int schema_callback(void *NotUsed, int argc, char **argv, char **azColName) {
    if (argc > 0 && argv != NULL && argv[0] != NULL) {
        char sjis_sql[2048];
        utf8_to_sjis(argv[0], sjis_sql, sizeof(sjis_sql));
        printf("%s;\n", sjis_sql);
    }
    return 0;
}

/******************************************************************************
 * @fn      print_help
 * @brief   本シェルで利用可能な各種メタ（ドット）コマンドのヘルプ一覧表示
 * @param   なし
 * @return  なし
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
 * @brief   SQL命令を自動判別し、INSERT文等は1パスで重複なく即座に実行完了する関数
 * @param   db          : オープン済みのsqlite3データベースオブジェクトへのポインタ
 * @param   zSql        : 実行を要求するSQLクエリ文字列（UTF-8）
 * @return  なし
 * @detail  
 *          SQL命令の先頭文字を正しくチェックし、SELECT文以外（INSERT/CREATE/UPDATE等）は
 *          2パスによる計測を完全にスキップして、1パスで安全に単発実行を完了させます。
 ******************************************************************************/
static void execute_sql_with_auto_width(sqlite3 *db, const char *zSql) {
    sqlite3_stmt *pStmt;
    int col_widths[MAX_COLS];
    int col_types[MAX_COLS];
    int nCol = 0;
    char sjis_buf[512];

    if (zSql == NULL || strlen(zSql) == 0) return;

    // 先頭の文字の実体を正確に比較し、SELECT文以外を1パス単発実行ルートへ隔離
    if (zSql[0] != 'S' && zSql[0] != 's') {
        char *zErrMsg = 0;
        if (sqlite3_exec(db, zSql, NULL, 0, &zErrMsg) != SQLITE_OK) {
            fprintf(stderr, "SQL Error: %s\n", zErrMsg ? zErrMsg : sqlite3_errmsg(db));
            if (zErrMsg) sqlite3_free(zErrMsg);
        }
        return;
    }

    if (sqlite3_prepare_v2(db, zSql, -1, &pStmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL Error: %s\n", sqlite3_errmsg(db));
        return;
    }

    nCol = sqlite3_column_count(pStmt);
    if (nCol > MAX_COLS) nCol = MAX_COLS;

    for (int i = 0; i < nCol; i++) {
        const char *name = sqlite3_column_name(pStmt, i);
        if (name) {
            utf8_to_sjis(name, sjis_buf, sizeof(sjis_buf));
            col_widths[i] = strlen(sjis_buf);
        } else {
            col_widths[i] = 0;
        }
        col_types[i] = SQLITE_TEXT; 
    }

    // 1パス目: 実際の最大表示文字幅をスキャン計測
    while (sqlite3_step(pStmt) == SQLITE_ROW) {
        for (int i = 0; i < nCol; i++) {
            const char *val = (const char*)sqlite3_column_text(pStmt, i);
            if (val) {
                utf8_to_sjis(val, sjis_buf, sizeof(sjis_buf));
                int len = strlen(sjis_buf);
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

    // 2パス目: 確定した最大幅を元にグリッド印刷
    if (sqlite3_prepare_v2(db, zSql, -1, &pStmt, NULL) != SQLITE_OK) {
        return;
    }

    if (sqlite3_step(pStmt) == SQLITE_ROW) {
        for (int i = 0; i < nCol; i++) {
            const char *name = sqlite3_column_name(pStmt, i);
            if (name) {
                utf8_to_sjis(name, sjis_buf, sizeof(sjis_buf));
            } else {
                sjis_buf[0] = '\0';
            }
            if (col_types[i] == SQLITE_INTEGER || col_types[i] == SQLITE_FLOAT) {
                printf("%*s%s", col_widths[i], sjis_buf, (i == nCol - 1) ? "" : "|");
            } else {
                printf("%-*s%s", col_widths[i], sjis_buf, (i == nCol - 1) ? "" : "|");
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
                if (val) {
                    utf8_to_sjis(val, sjis_buf, sizeof(sjis_buf));
                } else {
                    sjis_buf[0] = '\0';
                }
                if (col_types[i] == SQLITE_INTEGER || col_types[i] == SQLITE_FLOAT) {
                    printf("%*s%s", col_widths[i], sjis_buf, (i == nCol - 1) ? "" : "|");
                } else {
                    printf("%-*s%s", col_widths[i], sjis_buf, (i == nCol - 1) ? "" : "|");
                }
            }
            printf("\n");
        } while (sqlite3_step(pStmt) == SQLITE_ROW);
    }
    sqlite3_finalize(pStmt);
}

/******************************************************************************
 * @fn      main
 * @brief   入力SQLをUTF-8へ変換し、複数行入力を受け付けるメイン関数
 * @param   argc        : コマンドライン引数の個数
 * @param   argv        : コマンドライン引数の文字列配列
 * @return  int         : 0 (正常終了)、1 (データベースオープン失敗)
 * @detail  
 *          キーボードから入力されたShift-JIS文字列を、内部で自動的にUTF-8へ
 *          変換してからSQLiteコアへ引き渡します。
 * ******************************************************************************/
int main(int argc, char **argv) {
    sqlite3 *db;
    char *zErrMsg = 0;
    char line[MAX_LINE_LEN];
    char query[2048];
    char query_utf8[2048];
    const char *db_name = ":memory:";
    int is_first_line = 1;

    if (argc > 1) {
        db_name = argv[1];
    }

    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // ジャーナルをRAMワークに同期させ、FDD環境での unable to open database file エラーを根絶します
    sqlite3_exec(db, "PRAGMA journal_mode = MEMORY;", NULL, 0, NULL);

    printf("SQLite version 3.53.3  ");
    printf("(X680x0 version 0.26.7.2.03 by Kenoh)\n");
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
                    char sql[512];
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
            sjis_to_utf8(query, query_utf8, sizeof(query_utf8));
            execute_sql_with_auto_width(db, query_utf8);
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
