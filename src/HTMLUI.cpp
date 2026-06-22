#include <Arduino.h>
#include "HTMLUI.h"

String createDashboardPage(const String& boardName, const String& interfaceName, const String& ip, uint16_t modbusPort, bool* digitalInputs, int numDigitalInputs, bool* digitalOutputs, int numDigitalOutputs) {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>InputReaderModbus</title>"
    "<style>"
    ":root{--bg:#0b0c10;--card:#1f2833;--text:#c5c6c7;--accent:#ffa500;--on:#00e676;--off:#ff1744;--border:#45a29e;}"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);padding:12px;}"
    "header{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;padding-bottom:8px;border-bottom:2px solid var(--border);}"
    "header h1{font-size:1.3rem;color:var(--accent);letter-spacing:1px;text-transform:uppercase;}"
    ".badge{background:var(--card);border:1px solid var(--border);padding:4px 10px;border-radius:4px;font-size:0.75rem;font-family:monospace;color:var(--accent);}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px;}"
    ".card{background:var(--card);border:1px solid #2a3b45;border-radius:6px;padding:12px;box-shadow:0 4px 12px rgba(0,0,0,0.5);}"
    ".card h2{font-size:0.9rem;color:var(--accent);text-transform:uppercase;letter-spacing:1px;margin-bottom:10px;display:flex;align-items:center;gap:6px;}"
    ".card h2::before{content:'';display:inline-block;width:6px;height:6px;background:var(--accent);border-radius:50%;}"
    "table{width:100%;border-collapse:collapse;font-size:0.85rem;}"
    "th,td{padding:6px 4px;text-align:left;border-bottom:1px solid #2a3b45;}"
    "th{color:var(--accent);font-weight:600;font-size:0.75rem;text-transform:uppercase;}"
    ".led{width:10px;height:10px;border-radius:50%;display:inline-block;margin-right:6px;box-shadow:0 0 4px currentColor;}"
    ".led.on{background:var(--on);color:var(--on);}"
    ".led.off{background:var(--off);color:var(--off);}"
    ".val{font-family:'Courier New',monospace;font-weight:bold;font-size:0.9rem;}"
    ".row{display:flex;align-items:center;justify-content:space-between;padding:6px 0;border-bottom:1px solid #2a3b45;}"
    ".row:last-child{border-bottom:none;}"
    ".btn{border:none;padding:6px 14px;border-radius:4px;cursor:pointer;font-weight:bold;font-size:0.8rem;color:#fff;transition:filter 0.15s;}"
    ".btn:hover{filter:brightness(1.2);}"
    ".btn-on{background:#b71c1c;} .btn-off{background:#1b5e20;}"
    ".btn:disabled{opacity:0.5;cursor:wait;}"
    ".nav{display:flex;gap:10px;margin-top:12px;}"
    ".nav a{background:var(--card);border:1px solid var(--border);color:var(--accent);padding:8px 14px;border-radius:4px;text-decoration:none;font-size:0.8rem;font-weight:bold;text-transform:uppercase;transition:background 0.15s;}"
    ".nav a:hover{background:#2a3b45;}"
    ".status-line{display:flex;gap:16px;flex-wrap:wrap;margin-bottom:12px;font-size:0.8rem;font-family:monospace;color:#8d99ae;}"
    ".status-line span{display:flex;align-items:center;gap:4px;}"
    ".error{color:var(--off);font-weight:bold;font-size:0.85rem;margin-bottom:8px;}"
    "</style></head><body>"
  );

  html += "<header><h1>InputReaderModbus</h1><div style='display:flex;gap:8px;'><span class='badge'>" + boardName + "</span><span class='badge'>" + interfaceName + "</span></div></header>";

  html += "<div class='status-line'>";
  html += "<span>IP: " + ip + "</span>";
  html += "<span>Port: " + String(modbusPort) + "</span>";
  html += "</div>";
  html += "<div id='err' class='error'></div>";

  html += "<div class='grid'>";

  html += "<div class='card'><h2>Digital Inputs</h2><table>";
  html += "<thead><tr><th>Pin</th><th>State</th></tr></thead><tbody id='inputs-body'>";
  for (int i = 0; i < numDigitalInputs; i++) {
    bool on = digitalInputs[i];
    html += "<tr data-idx='" + String(i) + "'><td>I0_" + String(i) + "</td><td><span class='led " + String(on ? "on" : "off") + "'></span><span class='val'>" + String(on ? "ON" : "OFF") + "</span></td></tr>";
  }
  html += "</tbody></table></div>";

  if (numDigitalOutputs > 0) {
    html += "<div class='card'><h2>Digital Outputs</h2><div id='outputs-body'>";
    for (int i = 0; i < numDigitalOutputs; i++) {
      bool isOn = digitalOutputs[i];
      String btnClass = isOn ? "btn-on" : "btn-off";
      String btnText = isOn ? "TURN OFF" : "TURN ON";
      int targetVal = isOn ? 0 : 1;
      html += "<div class='row' data-idx='" + String(i) + "'>";
      html += "<span><span class='led " + String(isOn ? "on" : "off") + "'></span>Q0_" + String(i) + " <span class='val'>" + String(isOn ? "ON" : "OFF") + "</span></span>";
      html += "<button class='btn " + btnClass + "' onclick=\"setCoil(this," + String(i) + "," + String(targetVal) + ")\">" + btnText + "</button>";
      html += "</div>";
    }
    html += "</div></div>";
  }

  html += "</div>"; // close grid

  html += "<div class='nav'><a href='/config'>Configuration</a><a href='/update'>OTA Update</a></div>";

  html += "<script>";
  html += "function el(id){return document.getElementById(id);}";
  html += "function setLed(el,on){el.className='led '+(on?'on':'off');}";
  html += "async function setCoil(btn,idx,val){";
  html += "btn.disabled=true;";
  html += "try{const r=await fetch('/coil?idx='+idx+'&val='+val);const j=await r.json();if(!j.ok)throw new Error(j.error||'Write failed');updateRow(idx,j.state);}";
  html += "catch(e){el('err').innerText='Coil write error: '+e.message;}";
  html += "finally{btn.disabled=false;}";
  html += "}";
  html += "function updateRow(idx,state){";
  html += "const row=document.querySelector(\"#outputs-body .row[data-idx='\"+idx+\"']\");if(!row)return;";
  html += "const led=row.querySelector('.led');const val=row.querySelector('.val');const btn=row.querySelector('.btn');";
  html += "setLed(led,state);val.innerText=state?'ON':'OFF';";
  html += "btn.className='btn '+(state?'btn-on':'btn-off');btn.innerText=state?'TURN OFF':'TURN ON';";
  html += "btn.setAttribute('onclick','setCoil(this,'+idx+','+(state?0:1)+')');";
  html += "}";
  html += "async function refreshState(){";
  html += "try{const r=await fetch('/api/state');const j=await r.json();el('err').innerText='';";
  html += "j.inputs.forEach((v,i)=>{const row=document.querySelector(\"#inputs-body tr[data-idx='\"+i+\"']\");if(row)setLed(row.querySelector('.led'),v);});";
  html += "j.outputs.forEach((v,i)=>updateRow(i,v));}";
  html += "catch(e){el('err').innerText='Refresh error: '+e.message;}";
  html += "}";
  html += "setInterval(refreshState,500);";
  html += "</script></body></html>";
  return html;
}

String createConfigPage(const String& eth_ip, const String& eth_gw, const String& eth_sn, const String& eth_mac,
                        const String& wifi_ip, const String& wifi_gw, const String& wifi_sn, const String& wifi_mac,
                        const String& wifi_ssid, const String& wifi_pass,
                        uint16_t modbus_port, const String& hostname) {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Configuration</title>"
    "<style>"
    ":root{--bg:#0b0c10;--card:#1f2833;--text:#c5c6c7;--accent:#ffa500;--border:#45a29e;}"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);padding:12px;}"
    "h1{font-size:1.2rem;color:var(--accent);text-transform:uppercase;letter-spacing:1px;margin-bottom:12px;padding-bottom:8px;border-bottom:2px solid var(--border);}"
    "form{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px;}"
    ".card{background:var(--card);border:1px solid #2a3b45;border-radius:6px;padding:12px;box-shadow:0 4px 12px rgba(0,0,0,0.5);}"
    ".card h2{font-size:0.85rem;color:var(--accent);text-transform:uppercase;letter-spacing:1px;margin-bottom:10px;}"
    "label{display:block;margin-top:8px;font-size:0.75rem;text-transform:uppercase;color:#8d99ae;}"
    "input{width:100%;background:#0b0c10;border:1px solid #2a3b45;color:var(--text);padding:8px;margin-top:4px;border-radius:4px;box-sizing:border-box;font-family:monospace;}"
    "input:focus{outline:none;border-color:var(--accent);}"
    "input[type=submit]{background:#2a3b45;border:1px solid var(--border);color:var(--accent);font-weight:bold;cursor:pointer;text-transform:uppercase;letter-spacing:1px;transition:background 0.15s;}"
    "input[type=submit]:hover{background:var(--border);color:var(--bg);}"
    ".nav{display:flex;gap:10px;margin-top:12px;}"
    ".nav a{background:var(--card);border:1px solid var(--border);color:var(--accent);padding:8px 14px;border-radius:4px;text-decoration:none;font-size:0.8rem;font-weight:bold;text-transform:uppercase;}"
    "</style></head><body>"
  );
  html += "<h1>Configuration</h1>";
  html += "<form method='POST' action='/config'>";
  html += "<div class='card'><h2>Ethernet</h2>";
  html += "<label>IP</label><input name='eth_ip' value='" + eth_ip + "'>";
  html += "<label>Gateway</label><input name='eth_gw' value='" + eth_gw + "'>";
  html += "<label>Subnet</label><input name='eth_sn' value='" + eth_sn + "'>";
  html += "<label>MAC</label><input name='eth_mac' value='" + eth_mac + "'>";
  html += "</div>";
  html += "<div class='card'><h2>WiFi</h2>";
  html += "<label>IP</label><input name='wifi_ip' value='" + wifi_ip + "'>";
  html += "<label>Gateway</label><input name='wifi_gw' value='" + wifi_gw + "'>";
  html += "<label>Subnet</label><input name='wifi_sn' value='" + wifi_sn + "'>";
  html += "<label>MAC</label><input name='wifi_mac' value='" + wifi_mac + "'>";
  html += "<label>SSID</label><input name='wifi_ssid' value='" + wifi_ssid + "'>";
  html += "<label>Password</label><input type='password' name='wifi_pass' value='" + wifi_pass + "'>";
  html += "</div>";
  html += "<div class='card'><h2>Modbus / General</h2>";
  html += "<label>Modbus Port</label><input name='modbus_port' value='" + String(modbus_port) + "'>";
  html += "<label>Hostname</label><input name='hostname' value='" + hostname + "'>";
  html += "<input type='submit' value='Save & Reboot'></div>";
  html += "</form>";
  html += "<div class='nav'><a href='/'>Back to Dashboard</a></div>";
  html += "</body></html>";
  return html;
}

String createOTAFormPage() {
  return F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>OTA Update</title>"
    "<style>"
    ":root{--bg:#0b0c10;--card:#1f2833;--text:#c5c6c7;--accent:#ffa500;--on:#00e676;--off:#ff1744;--border:#45a29e;}"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);padding:12px;}"
    "h1{font-size:1.2rem;color:var(--accent);text-transform:uppercase;letter-spacing:1px;margin-bottom:12px;padding-bottom:8px;border-bottom:2px solid var(--border);}"
    ".card{background:var(--card);border:1px solid #2a3b45;border-radius:6px;padding:16px;box-shadow:0 4px 12px rgba(0,0,0,0.5);max-width:420px;}"
    "input[type=file]{margin-top:8px;background:#0b0c10;border:1px solid #2a3b45;color:var(--text);padding:8px;border-radius:4px;width:100%;}"
    "button{background:#2a3b45;border:1px solid var(--border);color:var(--accent);padding:10px 18px;border-radius:4px;cursor:pointer;font-weight:bold;text-transform:uppercase;letter-spacing:1px;margin-top:12px;transition:background 0.15s;}"
    "button:hover{background:var(--border);color:var(--bg);}"
    "button:disabled{opacity:0.5;cursor:not-allowed;}"
    "#status{margin-top:12px;font-family:monospace;color:var(--accent);min-height:1.2em;}"
    "#progress-wrap{margin-top:10px;display:none;background:#0b0c10;border:1px solid #2a3b45;border-radius:4px;height:18px;overflow:hidden;}"
    "#progress-bar{height:100%;width:0%;background:var(--accent);transition:width 0.1s linear;}"
    ".nav{display:flex;gap:10px;margin-top:12px;}"
    ".nav a{background:var(--card);border:1px solid var(--border);color:var(--accent);padding:8px 14px;border-radius:4px;text-decoration:none;font-size:0.8rem;font-weight:bold;text-transform:uppercase;}"
    "</style></head><body>"
    "<h1>OTA Update</h1><div class='card'>"
    "<input type='file' id='file' accept='.bin'><br>"
    "<button id='btn-up' onclick='upload()'>Update</button>"
    "<div id='progress-wrap'><div id='progress-bar'></div></div>"
    "<p id='status'></p>"
    "</div><div class='nav'><a href='/'>Back to Dashboard</a></div><script>"
    "function setStatus(t,c){const s=document.getElementById('status');s.innerText=t;s.style.color=c||'var(--accent)';}"
    "function setProgress(p){document.getElementById('progress-bar').style.width=p+'%';document.getElementById('progress-wrap').style.display=p>0?'block':'none';}"
    "function upload(){"
    "const f=document.getElementById('file').files[0];if(!f){setStatus('Please select a .bin file','var(--off)');return;}"
    "const btn=document.getElementById('btn-up');btn.disabled=true;setStatus('Uploading '+f.name+' ...');setProgress(0);"
    "const fd=new FormData();fd.append('update',f);"
    "const xhr=new XMLHttpRequest();"
    "xhr.upload.addEventListener('progress',e=>{if(e.lengthComputable){const p=Math.round(100*e.loaded/e.total);setProgress(p);setStatus('Uploading '+p+'% ('+e.loaded+' / '+e.total+' bytes)');}});"
    "xhr.upload.addEventListener('load',()=>{setStatus('Upload complete. Flashing firmware... Do not power off.');setProgress(100);});"
    "xhr.addEventListener('load',()=>{"
    "if(xhr.status===200){setStatus('Update successful. Rebooting... Please wait ~15 seconds, then refresh the page.','var(--on)');let n=15;const iv=setInterval(()=>{n--;setStatus('Update successful. Rebooting... please wait '+n+'s.','var(--on)');if(n<=0)clearInterval(iv);},1000);}"
    "else{setStatus('Update failed: HTTP '+xhr.status+' - '+xhr.responseText,'var(--off)');btn.disabled=false;}"
    "});"
    "xhr.addEventListener('error',()=>{setStatus('Upload failed: network error','var(--off)');btn.disabled=false;});"
    "xhr.open('POST','/update');xhr.send(fd);"
    "}"
    "</script></body></html>"
  );
}

String createOTAInfoPage(const String& activeInterface) {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>OTA Update</title>"
    "<style>"
    ":root{--bg:#0b0c10;--card:#1f2833;--text:#c5c6c7;--accent:#ffa500;--border:#45a29e;}"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);padding:12px;}"
    "h1{font-size:1.2rem;color:var(--accent);text-transform:uppercase;letter-spacing:1px;margin-bottom:12px;padding-bottom:8px;border-bottom:2px solid var(--border);}"
    ".card{background:var(--card);border:1px solid #2a3b45;border-radius:6px;padding:16px;box-shadow:0 4px 12px rgba(0,0,0,0.5);}"
    ".nav{display:flex;gap:10px;margin-top:12px;}"
    ".nav a{background:var(--card);border:1px solid var(--border);color:var(--accent);padding:8px 14px;border-radius:4px;text-decoration:none;font-size:0.8rem;font-weight:bold;text-transform:uppercase;}"
    "</style></head><body>"
  );
  html += "<h1>OTA Update</h1><div class='card'>";
  html += "<p>OTA firmware upload is currently only supported when connected via <b>WiFi</b>.</p>";
  html += "<p>Active interface: <b>" + activeInterface + "</b></p>";
  html += "<p>If you need to update now, force a WiFi fallback by disconnecting the Ethernet cable and rebooting.</p>";
  html += "</div><div class='nav'><a href='/'>Back to Dashboard</a></div></body></html>";
  return html;
}
