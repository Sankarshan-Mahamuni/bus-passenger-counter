# bus_server.py
from flask import Flask, request, jsonify, render_template_string
from flask_cors import CORS
import sqlite3, time, threading, requests, os

app = Flask(__name__)
CORS(app)

DB = 'bus_attendance.db'
THINGSPEAK_KEY = 'ZX8YMA8LKYAZ01M6'  # <-- Put your ThingSpeak Write API key here if you want TS updates

def init_db():
    if not os.path.exists(DB):
        conn = sqlite3.connect(DB)
        c = conn.cursor()
        c.execute('''CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ts INTEGER,
            count INTEGER,
            capacity INTEGER
        )''')
        conn.commit()
        conn.close()

init_db()

@app.route('/update_count', methods=['POST'])
def update_count():
    data = request.get_json() or {}
    try:
        count = int(data.get('count', 0))
        capacity = int(data.get('capacity', 0))
    except Exception:
        return jsonify({'status':'error','message':'bad payload'}), 400

    ts = int(time.time())
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute('INSERT INTO logs (ts, count, capacity) VALUES (?, ?, ?)', (ts, count, capacity))
    conn.commit()
    conn.close()

    # push to ThingSpeak async (if key provided)
    if THINGSPEAK_KEY:
        threading.Thread(target=push_thingspeak, args=(count, capacity)).start()

    return jsonify({'status':'ok'})

@app.route('/dashboard')
def dashboard():
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute('SELECT ts, count, capacity FROM logs ORDER BY ts DESC LIMIT 100')
    rows = c.fetchall()
    conn.close()
    html = "<h2>Bus Occupancy Dashboard</h2>"
    html += "<p>Latest readings (time, count, capacity)</p>"
    html += "<table border='1' cellpadding='6'><tr><th>Time</th><th>Count</th><th>Capacity</th></tr>"
    for r in rows:
        html += "<tr><td>{}</td><td>{}</td><td>{}</td></tr>".format(
            time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(r[0])), r[1], r[2]
        )
    html += "</table>"
    html += "<p>To simulate external posts, use the simulate_posts.py script or press buttons in the Wokwi ESP32 simulation.</p>"
    return render_template_string(html)

def push_thingspeak(count, capacity):
    try:
        requests.post('https://api.thingspeak.com/update', params={
            'api_key':THINGSPEAK_KEY,
            'field1':count,
            'field2':capacity
        }, timeout=5)
    except Exception as e:
        print("ThingSpeak push failed:", e)

if __name__ == '__main__':
    print("Starting bus_server on http://0.0.0.0:5000")
    app.run(host='0.0.0.0', port=5000, debug=True)
