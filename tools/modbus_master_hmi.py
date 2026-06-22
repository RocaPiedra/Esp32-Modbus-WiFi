#!/usr/bin/env python3
"""
Modbus TCP Master + Web HMI for InputReaderModbus

Usage:
  python modbus_master_hmi.py --web-port 8080

Then open http://localhost:8080 in your browser, select a network interface,
discover or enter the PLC IP, and connect.
"""

import argparse
import threading
import time
import socket
import ipaddress
from concurrent.futures import ThreadPoolExecutor, as_completed
from flask import Flask, render_template_string, jsonify, request
from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusIOException

try:
    import psutil
    HAS_PSUTIL = True
except ImportError:
    HAS_PSUTIL = False

app = Flask(__name__)

# Global state
client = None
client_lock = threading.Lock()
poll_lock = threading.Lock()
state = {
    "connected": False,
    "host": "",
    "port": 502,
    "iface_name": "",
    "iface_ip": "",
    "digital_inputs": [],
    "coils": [],
    "last_error": "",
    "scanning": False,
    "scan_results": [],
    "num_inputs": 13,
    "num_coils": 8,
}

BOARD_PRESETS = {
    "esp32plc_21": {"inputs": 13, "coils": 8},
    "14iosplc_ma": {"inputs": 9, "coils": 4},
}


def get_network_interfaces():
    """Return list of {name, ip, netmask} for IPv4 interfaces."""
    interfaces = []
    if HAS_PSUTIL:
        for name, addrs in psutil.net_if_addrs().items():
            for addr in addrs:
                if addr.family == socket.AF_INET:
                    interfaces.append({
                        "name": name,
                        "ip": addr.address,
                        "netmask": addr.netmask,
                    })
    else:
        try:
            hostname = socket.gethostname()
            ip = socket.getaddrinfo(hostname, None, socket.AF_INET)[0][4][0]
            interfaces.append({"name": "default", "ip": ip, "netmask": "255.255.255.0"})
        except Exception:
            interfaces.append({"name": "default", "ip": "127.0.0.1", "netmask": "255.0.0.0"})
    return interfaces


def get_interface_by_name(name):
    for iface in get_network_interfaces():
        if iface["name"] == name:
            return iface
    return None


def scan_host(ip, port, timeout=1):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((str(ip), port))
        sock.close()
        if result == 0:
            return str(ip)
    except Exception:
        pass
    return None


def scan_subnet(iface_ip, netmask, port=502, max_workers=50):
    try:
        network = ipaddress.IPv4Network(f"{iface_ip}/{netmask}", strict=False)
    except Exception:
        return []
    hosts = list(network.hosts())
    results = []
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {executor.submit(scan_host, ip, port): ip for ip in hosts}
        for future in as_completed(futures):
            res = future.result()
            if res:
                results.append(res)
    return sorted(results)


def create_modbus_client(host, port, source_ip=None):
    kwargs = {"host": host, "port": port, "timeout": 3}
    if source_ip:
        kwargs["source_address"] = (source_ip, 0)
    return ModbusTcpClient(**kwargs)


def is_socket_error(e):
    msg = str(e).lower()
    return any(k in msg for k in ["broken pipe", "connection reset", "connection aborted", "errno 104", "errno 32"])


def poll_plc():
    global client
    while True:
        time.sleep(0.5)
        with client_lock:
            c = client
            if c is None:
                with poll_lock:
                    state["connected"] = False
                continue
            if not c.connected:
                try:
                    c.connect()
                except Exception:
                    pass
            if not c.connected:
                with poll_lock:
                    state["connected"] = False
                    state["last_error"] = "Connection failed"
                continue

            try:
                with poll_lock:
                    di_count = max(1, state["num_inputs"])
                    coil_count = max(1, state["num_coils"])

                di_result = c.read_discrete_inputs(address=0, count=di_count)
                coil_result = c.read_coils(address=0, count=coil_count)

                with poll_lock:
                    state["connected"] = True
                    if di_result and not di_result.isError():
                        state["digital_inputs"] = list(di_result.bits)[:di_count]
                    else:
                        state["last_error"] = "Read discrete inputs failed"

                    if coil_result and not coil_result.isError():
                        state["coils"] = list(coil_result.bits)[:coil_count]
                    else:
                        state["last_error"] = "Read coils failed"
            except Exception as e:
                with poll_lock:
                    state["connected"] = False
                    state["last_error"] = str(e)
                if is_socket_error(e):
                    try:
                        c.close()
                    except Exception:
                        pass
                    client = None


HTML_PAGE = """
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Modbus TCP HMI</title>
    <style>
        :root{--bg:#0b0c10;--card:#1f2833;--text:#c5c6c7;--accent:#ffa500;--on:#00e676;--off:#ff1744;--border:#45a29e;}
        *{box-sizing:border-box;margin:0;padding:0;}
        body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);padding:12px;}
        header{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;padding-bottom:8px;border-bottom:2px solid var(--border);}
        header h1{font-size:1.3rem;color:var(--accent);letter-spacing:1px;text-transform:uppercase;}
        .badge{background:var(--card);border:1px solid var(--border);padding:4px 10px;border-radius:4px;font-size:0.75rem;font-family:monospace;color:var(--accent);}
        .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:12px;}
        .card{background:var(--card);border:1px solid #2a3b45;border-radius:6px;padding:12px;box-shadow:0 4px 12px rgba(0,0,0,0.5);}
        .card h3{font-size:0.9rem;color:var(--accent);text-transform:uppercase;letter-spacing:1px;margin-bottom:10px;display:flex;align-items:center;gap:6px;border-bottom:1px solid #2a3b45;padding-bottom:8px;}
        .card h3::before{content:'';display:inline-block;width:6px;height:6px;background:var(--accent);border-radius:50%;}
        .status-line{display:flex;gap:16px;flex-wrap:wrap;margin-bottom:12px;font-size:0.8rem;font-family:monospace;color:#8d99ae;}
        .status-line span{display:flex;align-items:center;gap:4px;}
        .status{padding:6px 12px;border-radius:4px;font-weight:bold;display:inline-block;font-size:0.8rem;margin-bottom:12px;}
        .online{background:#1b5e20;color:var(--on);border:1px solid var(--on);}
        .offline{background:#b71c1c;color:var(--off);border:1px solid var(--off);}
        .error{color:var(--off);margin-top:8px;font-weight:bold;font-size:0.85rem;}
        table{width:100%;border-collapse:collapse;font-size:0.85rem;}
        th,td{padding:6px 4px;text-align:left;border-bottom:1px solid #2a3b45;}
        th{color:var(--accent);font-weight:600;font-size:0.75rem;text-transform:uppercase;}
        .led{width:10px;height:10px;border-radius:50%;display:inline-block;margin-right:6px;box-shadow:0 0 4px currentColor;}
        .led.on{background:var(--on);color:var(--on);}
        .led.off{background:var(--off);color:var(--off);}
        .val{font-family:'Courier New',monospace;font-weight:bold;font-size:0.9rem;}
        .row{display:flex;align-items:center;justify-content:space-between;padding:6px 0;border-bottom:1px solid #2a3b45;}
        .row:last-child{border-bottom:none;}
        .btn{border:none;padding:6px 14px;border-radius:4px;cursor:pointer;font-weight:bold;font-size:0.8rem;color:#fff;transition:filter 0.15s;}
        .btn:hover{filter:brightness(1.2);}
        .btn-on{background:#b71c1c;} .btn-off{background:#1b5e20;}
        .config-row{margin:10px 0;}
        .config-row label{display:block;font-size:0.75rem;text-transform:uppercase;color:#8d99ae;margin-bottom:4px;}
        .config-row select,.config-row input{width:100%;background:#0b0c10;border:1px solid #2a3b45;color:var(--text);padding:8px;margin-top:4px;border-radius:4px;box-sizing:border-box;font-family:monospace;}
        .config-row select:focus,.config-row input:focus{outline:none;border-color:var(--accent);}
        .btn-primary{background:#2a3b45;border:1px solid var(--border);color:var(--accent);padding:8px 16px;border-radius:4px;cursor:pointer;font-weight:bold;font-size:0.8rem;text-transform:uppercase;letter-spacing:1px;transition:background 0.15s;margin-right:6px;}
        .btn-secondary{background:#0b0c10;border:1px solid #2a3b45;color:#8d99ae;padding:8px 16px;border-radius:4px;cursor:pointer;font-weight:bold;font-size:0.8rem;text-transform:uppercase;letter-spacing:1px;transition:background 0.15s;margin-right:6px;}
        .btn-primary:hover{background:var(--border);color:var(--bg);}
        .btn-secondary:hover{background:#2a3b45;color:var(--text);}
        .btn-primary:disabled,.btn-secondary:disabled{background:#1a1a1a;color:#555;cursor:not-allowed;border-color:#333;}
        .scan-results{margin-top:8px;max-height:140px;overflow-y:auto;background:#0b0c10;border:1px solid #2a3b45;border-radius:4px;padding:8px;}
        .scan-result-item{padding:4px 0;cursor:pointer;color:var(--accent);font-family:monospace;font-size:0.85rem;}
        .scan-result-item:hover{text-decoration:underline;}
        .spinner{display:inline-block;width:14px;height:14px;border:2px solid #2a3b45;border-top-color:var(--accent);border-radius:50%;animation:spin 0.8s linear infinite;vertical-align:middle;margin-left:8px;}
        @keyframes spin{to{transform:rotate(360deg);}}
        .preset-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:6px;}
        .preset-chip{background:#0b0c10;border:1px solid var(--border);color:var(--accent);padding:4px 10px;border-radius:4px;font-size:0.75rem;cursor:pointer;font-family:monospace;transition:background 0.15s;}
        .preset-chip:hover{background:#2a3b45;}
    </style>
</head>
<body>
    <header><h1>Modbus TCP HMI</h1><div style="display:flex;gap:8px;"><span class="badge">Test Tool</span><span class="badge">InputReaderModbus</span></div></header>

    <div class="status-line">
        <span id="conn-status" class="status offline">Disconnected</span>
    </div>
    <div id="error-msg" class="error"></div>

    <div class="grid">
        <div class="card">
            <h3>Configuration</h3>
            <div class="config-row">
                <label>Network Interface</label>
                <select id="iface-select" onchange="onIfaceChange()">
                    <option value="">Loading interfaces...</option>
                </select>
            </div>
            <div class="config-row">
                <label>Target IP</label>
                <input type="text" id="target-ip" placeholder="e.g. 10.92.92.96" value="">
            </div>
            <div class="config-row">
                <label>Target Port</label>
                <input type="number" id="target-port" value="502">
            </div>
            <div class="config-row">
                <label>Board Preset</label>
                <div class="preset-row">
                    <span class="preset-chip" onclick="applyPreset('esp32plc_21')">ESP32 PLC 21 IO+</span>
                    <span class="preset-chip" onclick="applyPreset('14iosplc_ma')">PLC 14 IOS</span>
                    <span class="preset-chip" onclick="applyPreset('custom')">Custom</span>
                </div>
            </div>
            <div class="config-row">
                <label>Digital Inputs</label>
                <input type="number" id="num-inputs" value="13" min="1" max="32">
            </div>
            <div class="config-row">
                <label>Coils</label>
                <input type="number" id="num-coils" value="8" min="1" max="32">
            </div>
            <div class="config-row">
                <button class="btn-secondary" id="btn-scan" onclick="startScan()">Scan Subnet</button>
                <button class="btn-primary" id="btn-connect" onclick="doConnect()">Connect</button>
                <button class="btn-secondary" id="btn-disconnect" onclick="doDisconnect()">Disconnect</button>
                <span id="scan-spinner" class="spinner" style="display:none;"></span>
            </div>
            <div class="scan-results" id="scan-results" style="display:none;"></div>
        </div>

        <div class="card">
            <h3>Digital Inputs</h3>
            <table><thead><tr><th>Pin</th><th>State</th></tr></thead><tbody id="inputs-list"></tbody></table>
        </div>
        <div class="card">
            <h3>Coils</h3>
            <div id="coils-list"></div>
        </div>
    </div>

    <script>
        const PRESETS = {
            esp32plc_21: { inputs: 13, coils: 8 },
            '14iosplc_ma': { inputs: 9, coils: 4 },
            custom: null
        };

        let currentState = {};

        async function fetchData() {
            try {
                const res = await fetch('/api/state');
                const data = await res.json();
                currentState = data;
                updateUI(data);
            } catch (e) {
                const st = document.getElementById('conn-status');
                st.className = 'status offline';
                st.innerText = 'Disconnected';
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
                const row = document.createElement('tr');
                row.innerHTML = `<td>I0_${i}</td><td><span class="led ${val ? 'on' : 'off'}"></span><span class="val">${val ? 'ON' : 'OFF'}</span></td>`;
                inList.appendChild(row);
            });

            const coilList = document.getElementById('coils-list');
            coilList.innerHTML = '';
            data.coils.forEach((val, i) => {
                const row = document.createElement('div');
                row.className = 'row';
                const btnClass = val ? 'btn-on' : 'btn-off';
                const btnText = val ? 'TURN OFF' : 'TURN ON';
                const targetVal = val ? 0 : 1;
                row.innerHTML = `<span><span class="led ${val ? 'on' : 'off'}"></span>Q0_${i} <span class="val">${val ? 'ON' : 'OFF'}</span></span><button class="btn ${btnClass}" onclick="toggleCoil(${i},${targetVal})">${btnText}</button>`;
                coilList.appendChild(row);
            });

            if (data.scanning) {
                document.getElementById('scan-spinner').style.display = 'inline-block';
                document.getElementById('btn-scan').disabled = true;
            } else {
                document.getElementById('scan-spinner').style.display = 'none';
                document.getElementById('btn-scan').disabled = false;
            }

            const scanContainer = document.getElementById('scan-results');
            if (data.scan_results && data.scan_results.length > 0) {
                scanContainer.style.display = 'block';
                scanContainer.innerHTML = '<strong style="color:var(--accent);font-size:0.8rem;">Discovered devices:</strong><br>' +
                    data.scan_results.map(ip => `<div class="scan-result-item" onclick="pickIp('${ip}')">${ip}</div>`).join('');
            } else if (!data.scanning) {
                scanContainer.style.display = 'none';
            }
        }

        async function loadInterfaces() {
            try {
                const res = await fetch('/api/interfaces');
                const data = await res.json();
                const sel = document.getElementById('iface-select');
                sel.innerHTML = '<option value="">-- Select interface --</option>';
                data.interfaces.forEach(iface => {
                    const opt = document.createElement('option');
                    opt.value = iface.name;
                    opt.textContent = `${iface.name} (${iface.ip})`;
                    opt.dataset.ip = iface.ip;
                    sel.appendChild(opt);
                });
            } catch (e) {
                console.error('Failed to load interfaces', e);
            }
        }

        function onIfaceChange() {}
        function pickIp(ip) { document.getElementById('target-ip').value = ip; }

        function applyPreset(key) {
            const p = PRESETS[key];
            if (p) {
                document.getElementById('num-inputs').value = p.inputs;
                document.getElementById('num-coils').value = p.coils;
            }
        }

        async function startScan() {
            const iface = document.getElementById('iface-select').value;
            if (!iface) { alert('Please select a network interface first.'); return; }
            try {
                await fetch('/api/scan?iface=' + encodeURIComponent(iface), {method: 'POST'});
            } catch (e) { console.error('Scan failed', e); }
        }

        async function doConnect() {
            const host = document.getElementById('target-ip').value.trim();
            const port = parseInt(document.getElementById('target-port').value, 10);
            const iface = document.getElementById('iface-select').value;
            const numInputs = parseInt(document.getElementById('num-inputs').value, 10);
            const numCoils = parseInt(document.getElementById('num-coils').value, 10);
            if (!host) { alert('Please enter a target IP.'); return; }
            try {
                const res = await fetch('/api/connect', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ host, port, iface, num_inputs: numInputs, num_coils: numCoils })
                });
                const data = await res.json();
                if (!data.ok) alert(data.error || 'Connection failed');
            } catch (e) { alert('Connection error: ' + e); }
        }

        async function doDisconnect() {
            try { await fetch('/api/disconnect', {method: 'POST'}); }
            catch (e) { console.error('Disconnect failed', e); }
        }

        async function toggleCoil(index, value) {
            // Optimistic update
            if (currentState.coils && index < currentState.coils.length) {
                currentState.coils[index] = Boolean(value);
                updateUI(currentState);
            }
            try {
                const res = await fetch('/api/coil', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ index: index, value: value })
                });
                const data = await res.json();
                if (!data.ok) {
                    document.getElementById('error-msg').innerText = 'Write failed: ' + (data.error || 'Unknown error');
                    // Revert optimistic update on failure by refreshing from PLC
                    fetchData();
                }
            } catch (e) {
                document.getElementById('error-msg').innerText = 'Write error: ' + e;
                fetchData();
            }
        }

        setInterval(fetchData, 500);
        loadInterfaces();
        fetchData();
    </script>
</body>
</html>
"""


@app.route("/")
def index():
    return render_template_string(HTML_PAGE)


@app.route("/api/interfaces")
def api_interfaces():
    return jsonify({"interfaces": get_network_interfaces()})


@app.route("/api/scan", methods=["POST"])
def api_scan():
    iface_name = request.args.get("iface", "")
    iface = get_interface_by_name(iface_name)
    if not iface:
        return jsonify({"ok": False, "error": "Interface not found"}), 400

    def do_scan():
        with poll_lock:
            state["scanning"] = True
            state["scan_results"] = []
        try:
            results = scan_subnet(iface["ip"], iface["netmask"])
            with poll_lock:
                state["scan_results"] = results
        finally:
            with poll_lock:
                state["scanning"] = False

    t = threading.Thread(target=do_scan, daemon=True)
    t.start()
    return jsonify({"ok": True})


@app.route("/api/connect", methods=["POST"])
def api_connect():
    global client
    data = request.get_json(force=True)
    host = data.get("host", "").strip()
    port = int(data.get("port", 502))
    iface_name = data.get("iface", "")
    num_inputs = int(data.get("num_inputs", 13))
    num_coils = int(data.get("num_coils", 8))

    if not host:
        return jsonify({"ok": False, "error": "Host required"}), 400

    iface = get_interface_by_name(iface_name) if iface_name else None
    source_ip = iface["ip"] if iface else None

    with client_lock:
        if client:
            try:
                client.close()
            except Exception:
                pass
        client = create_modbus_client(host, port, source_ip)
        ok = client.connect()

    with poll_lock:
        state["host"] = host
        state["port"] = port
        state["iface_name"] = iface_name
        state["iface_ip"] = source_ip or ""
        state["connected"] = ok
        state["last_error"] = "" if ok else "Connection failed"
        state["scan_results"] = []
        state["num_inputs"] = max(1, min(32, num_inputs))
        state["num_coils"] = max(1, min(32, num_coils))
        state["digital_inputs"] = [False] * state["num_inputs"]
        state["coils"] = [False] * state["num_coils"]

    return jsonify({"ok": ok, "error": "" if ok else "Connection failed"})


@app.route("/api/disconnect", methods=["POST"])
def api_disconnect():
    global client
    with client_lock:
        if client:
            try:
                client.close()
            except Exception:
                pass
            client = None
    with poll_lock:
        state["connected"] = False
        state["last_error"] = "Disconnected by user"
    return jsonify({"ok": True})


@app.route("/api/state")
def api_state():
    with poll_lock:
        return jsonify(state)


@app.route("/api/coil", methods=["POST"])
def api_coil():
    global client
    data = request.get_json(force=True)
    idx = int(data.get("index", 0))
    val = bool(data.get("value", 0))

    with client_lock:
        c = client
        if c is None or not c.connected:
            return jsonify({"ok": False, "error": "Not connected"})

        try:
            result = c.write_coil(address=idx, value=val)
            if not result or result.isError():
                with poll_lock:
                    state["last_error"] = "Write coil failed"
                return jsonify({"ok": False, "error": "Write coil failed"})

            with poll_lock:
                if idx >= 0 and idx < len(state["coils"]):
                    state["coils"][idx] = bool(val)
                state["last_error"] = ""
            return jsonify({"ok": True})
        except Exception as e:
            with poll_lock:
                state["last_error"] = str(e)
            if is_socket_error(e):
                try:
                    c.close()
                except Exception:
                    pass
                client = None
            return jsonify({"ok": False, "error": str(e)})


def main():
    parser = argparse.ArgumentParser(description="Modbus TCP Master + Web HMI")
    parser.add_argument("--host", default="", help="Default PLC Modbus IP")
    parser.add_argument("--port", type=int, default=502, help="Modbus TCP port (default: 502)")
    parser.add_argument("--web-port", type=int, default=8080, help="Web HMI port (default: 8080)")
    parser.add_argument("--inputs", type=int, default=13, help="Number of digital inputs (default: 13)")
    parser.add_argument("--coils", type=int, default=8, help="Number of coils (default: 8)")
    args = parser.parse_args()

    state["digital_inputs"] = [False] * args.inputs
    state["coils"] = [False] * args.coils
    state["num_inputs"] = args.inputs
    state["num_coils"] = args.coils
    if args.host:
        state["host"] = args.host
        state["port"] = args.port

    t = threading.Thread(target=poll_plc, daemon=True)
    t.start()

    print("Starting Modbus HMI...")
    print(f"  Web UI:  http://localhost:{args.web_port}")
    if args.host:
        print(f"  Default PLC: {args.host}:{args.port}")
    print("  Use the web UI to select interface, discover the PLC, and connect.")
    app.run(host="0.0.0.0", port=args.web_port, debug=False, use_reloader=False)


if __name__ == "__main__":
    main()
