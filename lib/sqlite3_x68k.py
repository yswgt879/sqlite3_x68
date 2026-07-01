#====================================================================
# sqlite3_x68k    : SQLite3 Library for X680x0 MicroPython
#--------------------------------------------------------------------
# 処理内容        : SQLite3制御用軽量ラッパーライブラリ
# ﾌｧｲﾙ名          : sqlite3_x68k.py
# 開発環境        : MicroPython for X68k
# 動作環境        : X680x0, Human68k
# 作成者          : Kenoh
# 作成日          : 2026/07/01
# 更新日          : 2026/07/01
# SQLite3 Version : 3.53.3
# X680x0 Version  : 0.26.7.1.02
#====================================================================
import os

class Connection:
    def __init__(self, db_name):
        """
        @fn      __init__
        @brief   Connectionクラスのコンストラクタ（初期化メソッド）
        @param   db_name     : 接続対象となるデータベースファイル名の文字列
        @return  なし
        @sa
        @detail  
                 インスタンス生成時に呼び出され、指定されたデータベースファイル名を
                 メンバ変数（内部プロパティ）へ安全に保持します。
        """
        self.db_name = db_name

    def execute(self, sql):
        """
        @fn      execute
        @brief   SQL文をファイル中継リダイレクト経由で実行し結果をパースするメソッド
        @param   sql         : 実行を要求するSQLクエリ文字列
        @return  list        : 抽出レコードを格納した辞書型（ディクショナリ）の配列、
                               または実行ログの文字列配列
        @sa
        @detail  
                 入力されたSQLの末尾にセミコロンが無い場合は自動補完を施します。
                 2MB実機環境のRAM枯渇を防ぐため、一度一時入力ファイル（_in.tmp）へ
                 クエリと終了コマンドを書き出した後、Human68kの入出力リダイレクト（< および >）
                 を使用してバックグラウンドでsqlite3.xを完全サイレント実行させます。
                 出力された一時結果（_res.tmp）からプロンプトやシステムログ等の不要な
                 ゴミデータを完璧に除外し、パイプ「|」を基準に動的に文字列を分解して
                 Python側で扱いやすい綺麗なキー・値マッピングオブジェクトへと復元します。
        """
        sql_clean = sql.strip()
        if not sql_clean.endswith(';'):
            sql_clean += ';'

        try:
            with open("_in.tmp", "w") as fin:
                fin.write(sql_clean + "\n")
                fin.write(".quit\n")
        except:
            return []

        cmd = "sqlite3.x " + self.db_name + " < _in.tmp > _res.tmp"
        os.system(cmd)

        results = []
        try:
            with open("_res.tmp", "r") as f:
                lines = f.readlines()
                
                valid_lines = []
                for line in lines:
                    s_line = line.strip()
                    
                    if not s_line: continue
                    if s_line.startswith("SQLite version"): continue
                    if s_line.startswith("(X680x0 version"): continue
                    if s_line.startswith("Opened database"): continue
                    if s_line.startswith("Type '.help'"): continue
                    if s_line.startswith("Type '.quit'"): continue
                    if s_line == "sqlite>" or s_line == "...>": continue
                    
                    if s_line.startswith("sqlite> "): s_line = s_line[8:]
                    elif s_line.startswith("...>\t"): s_line = s_line[8:]
                    elif s_line.startswith("...>") : s_line = s_line[4:]
                    
                    valid_lines.append(s_line)
                
                # 【★重要修正】valid_lines[0] から正確にカラム名のヘッダを取得
                if len(valid_lines) >= 2 and "-" in valid_lines[1]:
                    headers = []
                    for h in valid_lines[0].split('|'):
                        headers.append(h.strip())
                    
                    for line in valid_lines[2:]:
                        values = []
                        for v in line.split('|'):
                            values.append(v.strip())
                        
                        row = {}
                        for i in range(min(len(headers), len(values))):
                            row[headers[i]] = values[i]
                        results.append(row)
                else:
                    for line in valid_lines:
                        results.append(line)
        except:
            pass
        finally:
            try: os.remove("_in.tmp")
            except: pass
            try: os.remove("_res.tmp")
            except: pass

        return results

def connect(db_name):
    """
    @fn      connect
    @brief   データベースファイルへの接続管理オブジェクトを生成するエントリ関数
    @param   db_name     : 接続対象となるデータベースファイル名の文字列
    @return  Connection  : 初期化が行われたConnectionクラスのインスタンス
    @sa
    @detail  
             外部のPythonスクリプトから最初の一歩として呼び出され、
             指定されたデータベースを内包したConnectionインスタンスを安全に返却します。
    """
    return Connection(db_name)
