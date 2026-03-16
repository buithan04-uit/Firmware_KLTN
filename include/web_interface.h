#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>

const char html_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Health Device Setup</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --primary: #00BFA5; /* Medical Teal */
            --primary-dark: #00897B;
            --accent: #FF5252;
            --bg-grad: linear-gradient(135deg, #E0F2F1 0%, #B2DFDB 100%);
            --surface: #FFFFFF;
            --text-main: #263238;
            --text-sec: #546E7A;
            --border: #ECEFF1;
            --shadow: 0 20px 60px rgba(0, 191, 165, 0.15);
            --radius: 20px;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Inter', sans-serif; -webkit-tap-highlight-color: transparent; }

        body {
            background: var(--bg-grad);
            min-height: 100vh;
            display: flex; justify-content: center; align-items: center;
            padding: 20px; color: var(--text-main);
        }

        .card {
            background: var(--surface);
            width: 100%; max-width: 420px;
            border-radius: var(--radius);
            box-shadow: var(--shadow);
            padding: 40px 30px;
            position: relative;
            overflow: hidden;
        }

        /* Header Design */
        .header { text-align: center; margin-bottom: 35px; }
        .logo-box {
            width: 64px; height: 64px;
            background: rgba(0, 191, 165, 0.1);
            border-radius: 50%;
            display: flex; align-items: center; justify-content: center;
            margin: 0 auto 15px;
            position: relative;
        }
        .logo-box svg { width: 32px; height: 32px; fill: var(--primary); animation: pulse 2s infinite; }
        .logo-box::after {
            content: ''; position: absolute; width: 100%; height: 100%;
            border-radius: 50%; border: 2px solid var(--primary);
            opacity: 0.2; animation: ripple 2s infinite;
        }
        h2 { font-weight: 700; font-size: 22px; color: var(--text-main); margin-bottom: 5px; }
        p.subtitle { font-size: 13px; color: var(--text-sec); }

        /* Form Elements */
        .section-label {
            font-size: 11px; font-weight: 700; text-transform: uppercase;
            color: var(--text-sec); letter-spacing: 1px;
            margin: 25px 0 10px; display: block;
        }

        .input-wrapper { position: relative; margin-bottom: 15px; }
        .input-icon {
            position: absolute; left: 16px; top: 50%; transform: translateY(-50%);
            width: 20px; height: 20px; fill: #B0BEC5; transition: 0.3s;
        }
        
        input, select {
            width: 100%; padding: 14px 45px 14px 48px;
            border: 2px solid var(--border);
            border-radius: 12px;
            font-size: 14px; color: var(--text-main);
            background: #FAFAFA;
            outline: none; transition: 0.3s;
            appearance: none;
        }
        
        input:focus, select:focus {
            background: #FFF; border-color: var(--primary);
            box-shadow: 0 0 0 4px rgba(0, 191, 165, 0.1);
        }
        input:focus ~ .input-icon, select:focus ~ .input-icon { fill: var(--primary); }

        input.error { border-color: var(--accent); background: #FFEBEE; }
        input.error ~ .input-icon { fill: var(--accent); }

        .pass-toggle {
            position: absolute; right: 16px; top: 50%; transform: translateY(-50%);
            width: 20px; height: 20px; fill: #B0BEC5; cursor: pointer;
        }

        /* Buttons */
        .btn-group { display: flex; gap: 12px; margin-top: 35px; }
        button {
            flex: 1; padding: 15px; border-radius: 12px; border: none;
            font-weight: 600; font-size: 14px; cursor: pointer;
            transition: 0.2s; display: flex; align-items: center; justify-content: center; gap: 8px;
        }
        .btn-main {
            background: var(--primary); color: white;
            box-shadow: 0 8px 20px rgba(0, 191, 165, 0.3);
        }
        .btn-main:active { transform: translateY(2px); }
        .btn-sec { background: #F5F5F5; color: var(--text-sec); }
        
        /* Toast Notification */
        .toast {
            position: fixed; top: 20px; left: 50%; transform: translateX(-50%) translateY(-100px);
            background: #323232; color: white; padding: 12px 24px;
            border-radius: 50px; font-size: 13px; font-weight: 500;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            transition: 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            display: flex; align-items: center; gap: 10px; opacity: 0; z-index: 1000;
        }
        .toast.show { transform: translateX(-50%) translateY(0); opacity: 1; }
        .toast.success { background: #2E7D32; }
        .toast.error { background: #C62828; }

        @keyframes pulse { 0% { transform: scale(1); } 50% { transform: scale(1.1); } 100% { transform: scale(1); } }
        @keyframes ripple { 0% { transform: scale(1); opacity: 0.6; } 100% { transform: scale(2); opacity: 0; } }
        
        .spinner {
            width: 18px; height: 18px; border: 2px solid rgba(255,255,255,0.3);
            border-top-color: white; border-radius: 50%; animation: spin 0.8s linear infinite; display: none;
        }
        @keyframes spin { to { transform: rotate(360deg); } }

    </style>
</head>
<body>

    <div id="toast" class="toast">
        <span id="toast-icon"></span>
        <span id="toast-msg"></span>
    </div>

    <div class="card">
        <div class="header">
            <div class="logo-box">
                <svg viewBox="0 0 24 24"><path d="M12 21.35l-1.45-1.32C5.4 15.36 2 12.28 2 8.5 2 5.42 4.42 3 7.5 3c1.74 0 3.41.81 4.5 2.09C13.09 3.81 14.76 3 16.5 3 19.58 3 22 5.42 22 8.5c0 3.78-3.4 6.86-8.55 11.54L12 21.35z"/></svg>
            </div>
            <h2>Thiết Lập Y Tế</h2>
            <p class="subtitle">Kết nối thiết bị với hệ thống dữ liệu</p>
        </div>

        <span class="section-label">Thông tin người dùng</span>
        <div class="input-wrapper">
            <svg class="input-icon" viewBox="0 0 24 24"><path d="M20 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V6c0-1.1-.9-2-2-2zm0 4l-8 5-8-5V6l8 5 8-5v2z"/></svg>
            <input type="email" id="email" placeholder="Nhập Email (user@example.com)" oninput="validateEmailLive()">
        </div>

        <span class="section-label">Kết nối Mạng</span>
        <div class="input-wrapper">
            <svg class="input-icon" viewBox="0 0 24 24"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4l2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg>
            <select id="ssid"><option>Đang quét...</option></select>
        </div>
        <div class="input-wrapper">
            <svg class="input-icon" viewBox="0 0 24 24"><path d="M18 8h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6v2H6c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zm-6 9c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2zm3.1-9H8.9V6c0-1.71 1.39-3.1 3.1-3.1 1.71 0 3.1 1.39 3.1 3.1v2z"/></svg>
            <input type="password" id="pass" placeholder="Mật khẩu WiFi">
            <svg class="pass-toggle" onclick="togglePass('pass')" viewBox="0 0 24 24"><path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z"/></svg>
        </div>

        <span class="section-label">Máy chủ MQTT</span>
        <div class="input-wrapper">
            <svg class="input-icon" viewBox="0 0 24 24"><path d="M12 12c2.21 0 4-1.79 4-4s-1.79-4-4-4-4 1.79-4 4 1.79 4 4 4zm0 2c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z"/></svg>
            <input type="text" id="mqtt_user" placeholder="MQTT Username">
        </div>
        <div class="input-wrapper">
            <svg class="input-icon" viewBox="0 0 24 24"><path d="M12.65 10C11.83 7.67 9.61 6 7 6c-3.31 0-6 2.69-6 6s2.69 6 6 6c2.61 0 4.83-1.67 5.65-4H17v4h4v-4h2v-4H12.65zM7 14c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2z"/></svg>
            <input type="password" id="mqtt_pass" placeholder="MQTT Password">
            <svg class="pass-toggle" onclick="togglePass('mqtt_pass')" viewBox="0 0 24 24"><path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z"/></svg>
        </div>

        <div class="btn-group">
            <button class="btn-sec" onclick="scanWifi()">Quét Lại</button>
            <button class="btn-main" onclick="saveConfig()">
                <div class="spinner" id="loader"></div>
                <span id="btn-text">Lưu Cấu Hình</span>
            </button>
        </div>
    </div>

    <script>
        window.onload = function() { setTimeout(() => { scanWifi(); loadConfig(); }, 500); };

        function showToast(msg, type) {
            var t = document.getElementById('toast');
            var icon = type === 'success' ? '✅' : '⚠️';
            document.getElementById('toast-icon').innerText = icon;
            document.getElementById('toast-msg').innerText = msg;
            t.className = 'toast show ' + type;
            setTimeout(() => { t.className = t.className.replace('show', ''); }, 3000);
        }

        function togglePass(id) {
            var x = document.getElementById(id);
            x.type = x.type === "password" ? "text" : "password";
        }

        function isValidEmail(email) {
            return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
        }

        function validateEmailLive() {
            var el = document.getElementById('email');
            if(el.value.length > 0 && !isValidEmail(el.value)) el.classList.add('error');
            else el.classList.remove('error');
        }

        function scanWifi() {
            var s = document.getElementById('ssid'); s.disabled=true; s.innerHTML="<option>Đang quét...</option>";
            fetch('/scan').then(r=>r.json()).then(d=>{
                s.innerHTML=""; 
                if(d.length === 0) s.add(new Option("Không tìm thấy mạng", ""));
                else d.forEach(i=>{ s.add(new Option(i.ssid + (i.rssi>-60?" 📶":""), i.ssid)); });
                s.disabled=false;
            }).catch(e=>s.innerHTML="<option>Lỗi quét</option>");
        }

        function loadConfig() {
            fetch('/config').then(r=>r.json()).then(d=>{
                if(d.email) document.getElementById('email').value = d.email;
                if(d.mqtt_user) document.getElementById('mqtt_user').value = d.mqtt_user;
                if(d.mqtt_pass) document.getElementById('mqtt_pass').value = d.mqtt_pass;
            });
        }

        function saveConfig() {
            var email = document.getElementById('email').value.trim();
            var ssid = document.getElementById('ssid').value;
            var btnText = document.getElementById('btn-text');
            var loader = document.getElementById('loader');

            if(!email) return showToast("Vui lòng nhập Email!", "error");
            if(!isValidEmail(email)) return showToast("Email không hợp lệ!", "error");
            if(!ssid || ssid.includes("Đang quét")) return showToast("Chưa chọn WiFi!", "error");

            btnText.style.display = 'none'; loader.style.display = 'block';
            
            var data = {
                ssid: ssid,
                pass: document.getElementById('pass').value,
                email: email,
                mqtt_user: document.getElementById('mqtt_user').value,
                mqtt_pass: document.getElementById('mqtt_pass').value
            };

            var p = new URLSearchParams(data).toString();
            fetch('/save?'+p).then(r=>r.text()).then(t=>{
                showToast("Đã lưu! Đang khởi động...", "success");
                setTimeout(() => location.reload(), 5000);
            }).catch(e=> {
                showToast("Đã gửi lệnh (Device Reboot)", "success"); 
                btnText.style.display = 'block'; loader.style.display = 'none';
            });
        }
    </script>
</body>
</html>
)=====";
#endif