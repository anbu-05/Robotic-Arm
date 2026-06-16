"""
robarm_driver - Python driver for the robotic arm firmware
Hosts a local webserver on localhost:5000
All persistent data (motor names, constants) stored in config.json
"""

from flask import Flask, request, jsonify, send_from_directory
import serial
import serial.tools.list_ports
import threading
import json
import os
import time
import re

def clean_reply(s):
    """Strip microrl terminal escape sequences from firmware reply."""
    s = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', s)
    s = re.sub(r'[\x00-\x08\x0b-\x0c\x0e-\x1f\x7f]', '', s)
    return s.strip()

# ─── config ───────────────────────────────────────────────────────────────────

HOST = "127.0.0.1"
PORT = 5000
CONFIG_FILE = "config.json"
STATIC_DIR = os.path.join(os.path.dirname(__file__), "static")

# ─── default motor config ─────────────────────────────────────────────────────

MOTOR_IDS = ["M0A", "M0B", "M1A", "M1B", "M2A", "M2B"]

DEFAULT_MOTOR = {
    "name": "",        # display name, editable in UI
    "flipdir": 0,
    "pos_start": 0,
    "pos_end": 4095,
    "speed": 128
}

# ─── state ────────────────────────────────────────────────────────────────────

ser = None                  # serial.Serial object when connected
ser_lock = threading.Lock() # protect serial reads/writes

motor_positions = {m: 0 for m in MOTOR_IDS}   # live positions from getpos
pos_lock = threading.Lock()

poll_interval_ms = 100      # how often getpos is sent (ms)
poll_running = False
poll_thread = None

# last successfully connected port/baud — used for autoreconnect
last_port = None
last_baud = 115200

# ─── config load/save ─────────────────────────────────────────────────────────

def load_config():
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, "r") as f:
            return json.load(f)
    # build defaults
    cfg = {}
    for m in MOTOR_IDS:
        cfg[m] = dict(DEFAULT_MOTOR)
        cfg[m]["name"] = m   # default display name = motor id
    return cfg

def save_config(cfg):
    with open(CONFIG_FILE, "w") as f:
        json.dump(cfg, f, indent=2)

motor_config = load_config()

# ─── serial helpers ───────────────────────────────────────────────────────────

def send_command(cmd):
    """Send a command string over serial. Returns reply string or error."""
    global ser
    if ser is None or not ser.is_open:
        return "ERROR: not connected"
    try:
        with ser_lock:
            ser.write((cmd.strip() + "\r\n").encode())
            time.sleep(0.05)
            reply = b""
            while ser.in_waiting:
                reply += ser.read(ser.in_waiting)
                time.sleep(0.01)
            return reply.decode(errors="replace").strip()
    except Exception as e:
        return f"ERROR: {e}"

def parse_positions(reply):
    """
    Extract M0A=val,M0B=val,... pairs from anywhere in the reply string.
    Firmware echoes the command back and appends 'IRin >' so we can't
    rely on line structure — just grep for MxY=<digits> anywhere in the blob.
    """
    result = {}
    # finds e.g. M0A=2007, M1B=673 anywhere in the string
    for match in re.finditer(r'(M[012][AB])=(\d+)', reply):
        key = match.group(1)
        val = int(match.group(2))
        if key in MOTOR_IDS:
            result[key] = val
    return result

# ─── polling thread ───────────────────────────────────────────────────────────

def poll_loop():
    global poll_running, motor_positions
    while poll_running:
        if ser and ser.is_open:
            reply = send_command("getpos")
            parsed = parse_positions(reply)
            with pos_lock:
                motor_positions.update(parsed)
        time.sleep(poll_interval_ms / 1000.0)

def start_poll():
    global poll_running, poll_thread
    if poll_running:
        return
    poll_running = True
    poll_thread = threading.Thread(target=poll_loop, daemon=True)
    poll_thread.start()

def stop_poll():
    global poll_running
    poll_running = False

# ─── apply motor params ────────────────────────────────────────────────────────

def apply_all_motor_params():
    """Send all motor parameters from config to the firmware."""
    replies = []
    for motor_id in MOTOR_IDS:
        config = motor_config.get(motor_id, {})
        for param in ["flipdir", "pos_start", "pos_end"]:
            if param in config:
                value = config[param]
                reply = send_command(f"setmotorparam {motor_id} {param} {value}")
                replies.append((motor_id, param, reply))
    return replies

# ─── flask app ────────────────────────────────────────────────────────────────

app = Flask(__name__, static_folder=STATIC_DIR)

@app.route("/")
def index():
    return send_from_directory(STATIC_DIR, "index.html")

# --- serial ---

@app.route("/api/ports")
def list_ports():
    ports = [p.device for p in serial.tools.list_ports.comports()]
    ports.reverse()
    return jsonify(ports)

@app.route("/api/connect", methods=["POST"])
def connect():
    global ser, last_port, last_baud
    data = request.json
    port = data.get("port")
    baud = data.get("baud", 115200)
    try:
        if ser and ser.is_open:
            ser.close()
        ser = serial.Serial(port, baud, timeout=1)
        last_port = port
        last_baud = baud
        start_poll()
        # Apply motor parameters from config to firmware
        apply_all_motor_params()
        return jsonify({"ok": True})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)})

@app.route("/api/last_port")
def get_last_port():
    return jsonify({"port": last_port, "baud": last_baud})

@app.route("/api/disconnect", methods=["POST"])
def disconnect():
    global ser
    stop_poll()
    if ser and ser.is_open:
        ser.close()
    ser = None
    return jsonify({"ok": True})

@app.route("/api/status")
def status():
    connected = ser is not None and ser.is_open
    return jsonify({"connected": connected})

# --- command passthrough ---

@app.route("/api/command", methods=["POST"])
def command():
    data = request.json
    cmd = data.get("cmd", "")
    reply = send_command(cmd)
    return jsonify({"reply": clean_reply(reply)})

# --- positions ---

@app.route("/api/positions")
def positions():
    with pos_lock:
        return jsonify(dict(motor_positions))

# --- poll interval ---

@app.route("/api/poll_interval", methods=["POST"])
def set_poll_interval():
    global poll_interval_ms
    data = request.json
    v = int(data.get("ms", 100))
    if v < 50:
        v = 50   # hard floor
    poll_interval_ms = v
    return jsonify({"ok": True, "ms": poll_interval_ms})

@app.route("/api/poll_interval")
def get_poll_interval():
    return jsonify({"ms": poll_interval_ms})

# --- motor config ---

@app.route("/api/motor_config")
def get_motor_config():
    return jsonify(motor_config)

@app.route("/api/motor_config", methods=["POST"])
def set_motor_config():
    """Save a motor's config locally AND push constants to firmware."""
    global motor_config
    data = request.json
    motor_id = data.get("motor_id")
    if motor_id not in MOTOR_IDS:
        return jsonify({"ok": False, "error": "invalid motor"})

    motor_config[motor_id]["name"]      = data.get("name",      motor_config[motor_id]["name"])
    motor_config[motor_id]["flipdir"]   = int(data.get("flipdir",   motor_config[motor_id]["flipdir"]))
    motor_config[motor_id]["pos_start"] = int(data.get("pos_start", motor_config[motor_id]["pos_start"]))
    motor_config[motor_id]["pos_end"]   = int(data.get("pos_end",   motor_config[motor_id]["pos_end"]))
    motor_config[motor_id]["speed"]     = int(data.get("speed",     motor_config[motor_id]["speed"]))

    save_config(motor_config)

    # push to firmware
    replies = []
    replies.append(send_command(f"setmotorparam {motor_id} flipdir {motor_config[motor_id]['flipdir']}"))
    replies.append(send_command(f"setmotorparam {motor_id} pos_start {motor_config[motor_id]['pos_start']}"))
    replies.append(send_command(f"setmotorparam {motor_id} pos_end {motor_config[motor_id]['pos_end']}"))

    return jsonify({"ok": True, "firmware_replies": replies})

# --- fetch motor params from firmware ---

@app.route("/api/fetch_motor_params")
def fetch_motor_params():
    """Read flipdir, pos_start, pos_end for all motors from firmware."""
    result = {}
    for m in MOTOR_IDS:
        result[m] = {}
        for param in ["flipdir", "pos_start", "pos_end"]:
            reply = send_command(f"getmotorparam {m} {param}")
            reply = clean_reply(reply)
            # grab the LAST integer in the reply — the value is always at the end,
            # before "IRin >". Using first digit picks up motor index digits (M0A=0, M1B=1...)
            matches = re.findall(r'\d+', reply)
            result[m][param] = int(matches[-1]) if matches else None
    return jsonify(result)

# --- setpos / stoppos ---

@app.route("/api/setpos", methods=["POST"])
def setpos():
    data = request.json
    motor_id = data.get("motor_id")
    pos      = int(data.get("pos", 0))
    speed    = int(data.get("speed", 128))
    reply = send_command(f"setpos {motor_id} {pos} {speed}")
    return jsonify({"reply": clean_reply(reply)})

@app.route("/api/stoppos", methods=["POST"])
def stoppos():
    data = request.json
    motor_id = data.get("motor_id")
    reply = send_command(f"stoppos {motor_id}")
    return jsonify({"reply": clean_reply(reply)})

@app.route("/api/motor_speed", methods=["POST"])
def set_motor_speed():
    data = request.json
    motor_id = data.get("motor_id")
    if motor_id not in MOTOR_IDS:
        return jsonify({"ok": False, "error": "invalid motor"})

    speed = int(data.get("speed", motor_config[motor_id].get("speed", 128)))
    speed = max(1, min(255, speed))
    motor_config[motor_id]["speed"] = speed
    save_config(motor_config)
    return jsonify({"ok": True, "speed": speed})

# ─── main ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print(f"robarm driver running at http://{HOST}:{PORT}")
    app.run(host=HOST, port=PORT, debug=False)