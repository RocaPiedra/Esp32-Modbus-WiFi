#!/usr/bin/env python3
"""
Modbus TCP Master + Web HMI for InputReaderModbus

Usage:
  python modbus_master_hmi.py --host 10.92.92.96 --port 502

Then open http://localhost:8080 in your browser.
"""

import argparse
import threading
import time
from flask import Flask, render_template_string, jsonify, request
from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusIOException

app = Flask(__name__)

# Global state
client = None
lock = threading.Lock()
state = {
    "connected": False,
    "host": "",
    "port": 502,
    "digital_inputs": [],
    "coils": [],
    "last_error": "",
}

HTML_PAGE = """
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Modbus HMI</title>
    <style>
        body { font-family: Arial, sans-serif; background: #f4f6f8; margin: 0; padding: 20px; }
        h1 { color: #1976d2; }
        .status { padding: 10px 16px; border-radius: 6px; margin-bottom: 20px; font-weight: bold; display: inline-block; }
        .online { background: #c8e6c9; color: #2e7d32; }
        .offline { background: #ffcdd2; color: #c62828; }
        .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); gap: 16px; }
        .card { background: #fff; border-radius: 10px; padding: 16px; box-shadow: 0 2px 6px rgba(0,0,0,0.1); }
        .card h3 { margin-top: 0; color: #333; }
        .indicator { width: 20px; height: 20px; border-radius: 50%; display: inline-block; margin-right: 10px; vertical-align: middle; }
        .on { background: #43a047; }
        .off { background: #e53935; }
        .row { display: flex; align-items: center; justify-content: space-between; margin: 8px 0; }
        .toggle-btn { padding: 8px 14px; border: none; border-radius: 6px; cursor: pointer; font-weight: bold; }
        .toggle-on { background: #e53935; color: #fff; }
        .toggle-off { background: #43a047; color: #fff; }
        .error { color: #c62828; margin-top: 10px; }
    </style>
</head>
<body>
    <h1>Modbus TCP HMI</h1>
    <div id="conn-status" class="status offline">Connecting...</div>
    <div id="error-msg" class="error"></div>

    <div class="grid">
        <div class="card">
            <h3>Digital Inputs (Sensors)</h3>
            <div id="inputs-list"></div>
        </div>
        <div class="card">
            <h3>Coils (Outputs)</h3>
            <div id="coils-list"></div>
        </div>
    </div>

    <script>
        async function fetchData() {
            try {
                const res = await fetch('/api/state');
                const data = await res.json();
                updateUI(data);
            } catch (e) {
                document.getElementById('conn-status').className = 'status offline';
                document.getElementById('conn-status').innerText = 'Disconnected';
            }
        }

        function updateUI(data) {
            const status = document.getElementById('conn-status');
            if (data.connected) {
                status.className = 'status online';
                status.innerText = 'Connected to ' + data.host + ':' + data.port;
            } else {
                status.className = 'status offline';
                status.innerText = 'Disconnected';
            }
            document.getElementById('error-msg').innerText = data.last_error || '';

            const inList = document.getElementById('inputs-list');
            inList.innerHTML = '';
            data.digital_inputs.forEach((val, i) => {
                const row = document.createElement('div');
                row.className = 'row';
                row.innerHTML = `<span><span class="indicator ${val ? 'on' : 'off'}"></span>I0_${i}</span><span>${val ? 'ON' : 'OFF'}</span>`;
                inList.appendChild(row);
            });

            const coilList = document.getElementById('coils-list');
            coilList.innerHTML = '';
            data.coils.forEach((val, i) => {
                const row = document.createElement('div');
                row.className = 'row';
                const btnClass = val ? 'toggle-on' : 'toggle-off';
                const btnText = val ? 'Turn OFF' : 'Turn ON';
                row.innerHTML = `<span>Q0_${i}</span><button class="toggle-btn ${btnClass}" onclick="toggleCoil(${i}, ${val ? 0 : 1})">${btnText}</button>`;
                coilList.appendChild(row);
            });
        }

        async function toggleCoil(index, value) {
            await fetch('/api/coil', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ index: index, value: value })
            });
            fetchData();
        }

        setInterval(fetchData, 1000);
        fetchData();
    </script>
</body>
</html>
"""


def poll_plc():
    global client, state
    while True:
        time.sleep(1)
        with lock:
            if client is None:
                state["connected"] = False
                continue
            if not client.connected:
                client.connect()
            if not client.connected:
                state["connected"] = False
                state["last_error"] = "Connection failed"
                continue

            try:
                # Read discrete inputs (FC 02) - max 2000 per request
                di_result = client.read_discrete_inputs(address=0, count=max(1, len(state["digital_inputs"])))
                if di_result and not di_result.isError():
                    state["digital_inputs"] = list(di_result.bits)
                    state["last_error"] = ""
                else:
                    state["last_error"] = "Read discrete inputs failed"

                # Read coils (FC 01)
                coil_result = client.read_coils(address=0, count=max(1, len(state["coils"])))
                if coil_result and not coil_result.isError():
                    state["coils"] = list(coil_result.bits)
                else:
                    state["last_error"] = "Read coils failed"

                state["connected"] = True
            except Exception as e:
                state["connected"] = False
                state["last_error"] = str(e)


@app.route("/")
def index():
    return render_template_string(HTML_PAGE)


@app.route("/api/state")
def api_state():
    with lock:
        return jsonify(state)


@app.route("/api/coil", methods=["POST"])
def api_coil():
    data = request.get_json(force=True)
    idx = int(data.get("index", 0))
    val = bool(data.get("value", 0))
    with lock:
        if client and client.connected:
            try:
                result = client.write_coil(address=idx, value=val)
                if result and not result.isError():
                    state["last_error"] = ""
                    return jsonify({"ok": True})
                else:
                    state["last_error"] = "Write coil failed"
            except Exception as e:
                state["last_error"] = str(e)
    return jsonify({"ok": False, "error": state.get("last_error", "")})


def main():
    parser = argparse.ArgumentParser(description="Modbus TCP Master + Web HMI")
    parser.add_argument("--host", default="10.92.92.96", help="PLC Modbus IP (default: 10.92.92.96)")
    parser.add_argument("--port", type=int, default=502, help="Modbus TCP port (default: 502)")
    parser.add_argument("--web-port", type=int, default=8080, help="Web HMI port (default: 8080)")
    parser.add_argument("--inputs", type=int, default=7, help="Number of digital inputs (default: 7)")
    parser.add_argument("--coils", type=int, default=5, help="Number of coils (default: 5)")
    args = parser.parse_args()

    global client, state
    state["host"] = args.host
    state["port"] = args.port
    state["digital_inputs"] = [False] * args.inputs
    state["coils"] = [False] * args.coils

    client = ModbusTcpClient(host=args.host, port=args.port, timeout=5)
    client.connect()

    t = threading.Thread(target=poll_plc, daemon=True)
    t.start()

    print(f"Starting Modbus HMI...")
    print(f"  PLC:     {args.host}:{args.port}")
    print(f"  Web UI:  http://localhost:{args.web_port}")
    app.run(host="0.0.0.0", port=args.web_port, debug=False)


if __name__ == "__main__":
    main()
