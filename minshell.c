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
// 更新日          : 2026/07/01
// SQLite3 Version : 3.53.3
// X680x0 Version  : 0.26.7.1.01
//====================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

/******************************************************************************
 * @fn      callback
 * @brief   本家デフォルトのパイプ(|)区切りおよびヘッダ出力を再現するコールバック関数
 * @param   pFirstRow   : 初回行判定フラグ用のポインタ (1=初回、0=2行目以降)
 * @param   argc        : 取得したレコードの列（カラム）数
 * @param   argv        : 列データの文字列配列
 * @param   azColName   : 列名（カラム名）の文字列配列
 * @return  int         : 常に 0 (処理継続フラグ)
 * @sa
 * @detail  
 *          メモリを一切追加消費しないストリーム表示ハッキング。
 *          最初の1件目が流れてきた時のみ、最上部に列名を「|」区切りで出力（ヘッダ）し、
 *          以降はレコードの各値を本家同様に「値|値|値」のフォーマットで画面に出力します。
 ******************************************************************************/
static int callback(void *pFirstRow, int argc, char **argv, char **azColName) {
    int *pFirst = (int*)pFirstRow;

    // 最初の1件目のデータが流れてきた時のみ、最上部にカラム名（ヘッダ）を印刷
    if (*pFirst) {
        for (int i = 0; i < argc; i++) {
            printf("%s%s", azColName[i], (i == argc - 1) ? "" : "|");
        }
        printf("\n");
        // ヘッダの下に本家風の区切り線を引く
        for (int i = 0; i < argc; i++) {
            printf("----------------%s", (i == argc - 1) ? "" : "+");
        }
        printf("\n");
        *pFirst = 0; // 2件目以降はスルーさせる
    }

    // レコードの値を本家風のパイプ区切りで出力
    for (int i = 0; i < argc; i++) {
        printf("%s%s", argv[i] ? argv[i] : "", (i == argc - 1) ? "" : "|");
    }
    printf("\n");
    return 0;
}

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
    char line[256];  // キーボードから1行読み込むためのテンポラリバッファ
    char query[256]; // 複数行を安全に結合して保持するためのクエリバッファ
    const char *db_name = ":memory:";
    int is_first_line = 1; // 複数行入力の1行目かどうかの判定フラグ

    // 引数があれば、それをファイル名として使用
    if (argc > 1) {
        db_name = argv[1];
    }

    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("SQLite version 3.53.3  ");
    // 【★重要維持】起動メッセージ画面の by Kenoh は誇らしげにそのままキープ！
    printf("(X680x0 version 0.26.7.1.01 by Kenoh)\n");
    printf("Opened database: %s\n", db_name);
    printf("Type '.help' for usage hints.\n");
    printf("Type '.quit' to quit.\n\n");

    // クエリバッファの初期化
    query[0] = '\0';

    while (1) {
        // 複数行入力の状態に合わせてプロンプトを本家風に動的切り替え
        if (is_first_line) {
            printf("sqlite> ");
        } else {
            printf("   ...> ");
        }

        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0; // 改行の除去
        
        // 1行目の入力時のみ、即時終了メタコマンドを判定
        if (is_first_line) {
            if (strcmp(line, ".quit") == 0) break;
            if (strlen(line) == 0) continue;
            
            // ドット（メタ）コマンドの処理
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

        // 入力された行をクエリバッファに安全に結合
        if (strlen(query) + strlen(line) + 2 < sizeof(query)) {
            if (!is_first_line) {
                strcat(query, " "); // 改行の代わりに空白を挟む
            }
            strcat(query, line);
        } else {
            fprintf(stderr, "Error: query buffer overflow\n");
            query[0] = '\0';
            is_first_line = 1;
            continue;
        }

        // 文の末尾が「;」で終わっているかチェック（本家の複数行入力確定ロジック）
        int len = strlen(query);
        if (len > 0 && query[len - 1] == ';') {
            int first_row_flag = 1; // コールバックのヘッダ印刷用フラグ

            // 通常のSQL文を実行（ヘッダ印刷用フラグポインタをユーザーデータに渡す）
            if (sqlite3_exec(db, query, callback, &first_row_flag, &zErrMsg) != SQLITE_OK) {
                fprintf(stderr, "SQL Error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            }
            
            // 実行が完了したのでバッファとステートを初期化
            query[0] = '\0';
            is_first_line = 1;
        } else {
            // 「;」がまだないので、次の行の入力を促すモードへ切り替え
            is_first_line = 0;
        }
    }

    sqlite3_close(db);
    return 0;
}
