import sqlite3_x68k

print("Connecting to test.db...")
db = sqlite3_x68k.connect("test.db")

print("Executing LEFT JOIN query...")
# ご提示いただいた実際のテーブル構造に基づいたリレーショナル結合
sql = "SELECT e.emp_id, e.emp_name, d.dept_name FROM employees AS e LEFT JOIN departments AS d ON e.dept_id = d.dept_id;"
rows = db.execute(sql)

print("\n--- MicroPython SQLite3 Results ---")
for row in rows:
    print("-----------------------------------")
    print("社員ID   :", row.get("emp_id"))
    print("氏名     :", row.get("emp_name"))
    print("所属部署 :", row.get("dept_name"))
print("-----------------------------------")
