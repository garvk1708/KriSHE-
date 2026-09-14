#include "dashboard.h"
#include "config.h"
#include "data_model.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "DASHBOARD";
static httpd_handle_t s_server = NULL;

/* Embedded Single Page HTML / CSS / JS */
static const char s_dashboard_html[] = 
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>KriSHE Carbon - Kiln dMRV Node</title>"
"<style>"
":root {"
"  --bg: #090d16; --card: #131b2e; --border: #1e293b; --text: #f8fafc;"
"  --text-dim: #94a3b8; --accent-cyan: #38bdf8; --accent-amber: #f59e0b;"
"  --accent-emerald: #10b981; --accent-rose: #f43f5e; --accent-purple: #a855f7;"
"}"
"* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }"
"body { background: var(--bg); color: var(--text); padding: 1.25rem; min-height: 100vh; }"
".container { max-width: 1200px; margin: 0 auto; display: flex; flex-direction: column; gap: 1.25rem; }"
"header { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 1rem; border-bottom: 1px solid var(--border); padding-bottom: 1rem; }"
".brand h1 { font-size: 1.5rem; font-weight: 700; letter-spacing: 0.05em; color: #fff; display: flex; align-items: center; gap: 0.5rem; }"
".brand h1 span { color: var(--accent-cyan); font-weight: 800; }"
".brand p { font-size: 0.85rem; color: var(--text-dim); margin-top: 0.2rem; }"
".system-pills { display: flex; gap: 0.6rem; flex-wrap: wrap; }"
".pill { font-size: 0.75rem; font-weight: 600; padding: 0.35rem 0.75rem; border-radius: 9999px; background: rgba(30, 41, 59, 0.8); border: 1px solid var(--border); display: flex; align-items: center; gap: 0.4rem; }"
".dot { width: 8px; height: 8px; border-radius: 50%; background: var(--accent-emerald); }"
".grid-3 { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 1.25rem; }"
".grid-2 { display: grid; grid-template-columns: 2fr 1fr; gap: 1.25rem; }"
"@media (max-width: 900px) { .grid-2 { grid-template-columns: 1fr; } }"
".card { background: var(--card); border: 1px solid var(--border); border-radius: 1rem; padding: 1.25rem; display: flex; flex-direction: column; gap: 0.75rem; position: relative; overflow: hidden; box-shadow: 0 4px 20px rgba(0,0,0,0.3); }"
".card-header { display: flex; justify-content: space-between; align-items: center; }"
".card-title { font-size: 0.8rem; font-weight: 700; text-transform: uppercase; letter-spacing: 0.08em; color: var(--text-dim); }"
".badge { font-size: 0.7rem; font-weight: 700; padding: 0.2rem 0.6rem; border-radius: 0.375rem; text-transform: uppercase; }"
".badge-idle { background: #334155; color: #cbd5e1; }"
".badge-resting { background: #1e293b; color: #94a3b8; border: 1px solid #334155; }"
".badge-preheating { background: #78350f; color: #fde68a; }"
".badge-active { background: #064e3b; color: #6ee7b7; box-shadow: 0 0 12px rgba(16, 185, 129, 0.4); }"
".badge-cooldown { background: #1e3a5f; color: #93c5fd; }"
".badge-complete { background: #581c87; color: #e9d5ff; }"
".badge-error { background: #881337; color: #fecdd3; }"
".temp-val { font-size: 2.25rem; font-weight: 800; letter-spacing: -0.02em; }"
".temp-sub { font-size: 0.8rem; color: var(--text-dim); display: flex; justify-content: space-between; align-items: center; }"
".metric-row { display: flex; justify-content: space-between; align-items: center; border-top: 1px solid rgba(255,255,255,0.05); padding-top: 0.5rem; font-size: 0.85rem; }"
".metric-row span:first-child { color: var(--text-dim); }"
".metric-row span:last-child { font-weight: 600; font-family: monospace; }"
"canvas { width: 100%; height: 260px; display: block; border-radius: 0.5rem; background: rgba(10, 15, 26, 0.6); border: 1px solid rgba(255,255,255,0.05); }"
".legend { display: flex; gap: 1rem; justify-content: center; font-size: 0.75rem; font-weight: 600; }"
".legend-item { display: flex; align-items: center; gap: 0.4rem; }"
".legend-color { width: 12px; height: 4px; border-radius: 2px; }"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"  <header>"
"    <div class=\"brand\">"
"      <h1><span>KriSHE</span> CARBON</h1>"
"      <p>Kiln dMRV Real Hardware Sensor Node</p>"
"    </div>"
"    <div class=\"system-pills\">"
"      <div class=\"pill\"><div class=\"dot\"></div> ONLINE</div>"
"      <div class=\"pill\">Wi-Fi AP: KriSHE_Carbon_AP</div>"
"      <div class=\"pill\" id=\"pill-uptime\">Uptime: 0s</div>"
"      <div class=\"pill\" id=\"pill-fw\">FW: v1.0.0</div>"
"    </div>"
"  </header>"
"  <div class=\"grid-3\">"
"    <div class=\"card\" id=\"card-top\">"
"      <div class=\"card-header\">"
"        <span class=\"card-title\">TOP Thermocouple</span>"
"        <span class=\"badge\" id=\"badge-top\">INITIALIZING</span>"
"      </div>"
"      <div class=\"temp-val\" id=\"val-top\">--.-- °C</div>"
"      <div class=\"temp-sub\">"
"        <span>Rate of Change</span>"
"        <span id=\"rate-top\">-- °C/s</span>"
"      </div>"
"    </div>"
"    <div class=\"card\" id=\"card-mid\">"
"      <div class=\"card-header\">"
"        <span class=\"card-title\">MIDDLE Thermocouple</span>"
"        <span class=\"badge\" id=\"badge-mid\">INITIALIZING</span>"
"      </div>"
"      <div class=\"temp-val\" id=\"val-mid\">--.-- °C</div>"
"      <div class=\"temp-sub\">"
"        <span>Rate of Change</span>"
"        <span id=\"rate-mid\">-- °C/s</span>"
"      </div>"
"    </div>"
"    <div class=\"card\" id=\"card-bot\">"
"      <div class=\"card-header\">"
"        <span class=\"card-title\">BOTTOM Thermocouple</span>"
"        <span class=\"badge\" id=\"badge-bot\">INITIALIZING</span>"
"      </div>"
"      <div class=\"temp-val\" id=\"val-bot\">--.-- °C</div>"
"      <div class=\"temp-sub\">"
"        <span>Rate of Change</span>"
"        <span id=\"rate-bot\">-- °C/s</span>"
"      </div>"
"    </div>"
"  </div>"
"  <div class=\"grid-2\">"
"    <div class=\"card\">"
"      <div class=\"card-header\">"
"        <span class=\"card-title\">Real-time Temperature Profile</span>"
"        <div class=\"legend\">"
"          <div class=\"legend-item\"><div class=\"legend-color\" style=\"background: var(--accent-cyan);\"></div> TOP</div>"
"          <div class=\"legend-item\"><div class=\"legend-color\" style=\"background: var(--accent-amber);\"></div> MID</div>"
"          <div class=\"legend-item\"><div class=\"legend-color\" style=\"background: var(--accent-emerald);\"></div> BOT</div>"
"        </div>"
"      </div>"
"      <canvas id=\"chart\"></canvas>"
"    </div>"
"    <div style=\"display: flex; flex-direction: column; gap: 1.25rem;\">"
"      <div class=\"card\">"
"        <div class=\"card-header\">"
"          <span class=\"card-title\">Kiln State Machine</span>"
"          <span class=\"badge badge-idle\" id=\"badge-state\">IDLE</span>"
"        </div>"
"        <div class=\"metric-row\"><span>Batch ID</span><span id=\"m-batch\">NONE</span></div>"
"        <div class=\"metric-row\"><span>Batch Duration</span><span id=\"m-duration\">0 s</span></div>"
"        <div class=\"metric-row\"><span>Activation Target</span><span>60.00 °C</span></div>"
"      </div>"
"      <div class=\"card\">"
"        <div class=\"card-header\">"
"          <span class=\"card-title\">L89HA GNSS Positioning</span>"
"          <span class=\"badge badge-idle\" id=\"badge-gps\">SEARCHING</span>"
"        </div>"
"        <div class=\"metric-row\"><span>Latitude</span><span id=\"m-lat\">0.000000</span></div>"
"        <div class=\"metric-row\"><span>Longitude</span><span id=\"m-lon\">0.000000</span></div>"
"        <div class=\"metric-row\"><span>Satellites</span><span id=\"m-sat\">0</span></div>"
"        <div class=\"metric-row\"><span>Authoritative UTC</span><span id=\"m-utc\">UNAVAILABLE</span></div>"
"      </div>"
"    </div>"
"  </div>"
"</div>"
"<script>"
"const history = { top: [], mid: [], bot: [], maxPoints: 60 };"
"const canvas = document.getElementById('chart');"
"const ctx = canvas.getContext('2d');"
"function resizeCanvas() {"
"  canvas.width = canvas.parentElement.clientWidth - 40;"
"  canvas.height = 260;"
"}"
"window.addEventListener('resize', resizeCanvas);"
"resizeCanvas();"
"function drawChart() {"
"  ctx.clearRect(0, 0, canvas.width, canvas.height);"
"  const w = canvas.width, h = canvas.height;"
"  const pad = { top: 20, right: 20, bottom: 30, left: 45 };"
"  const pw = w - pad.left - pad.right;"
"  const ph = h - pad.top - pad.bottom;"
"  let min = 20, max = 80;"
"  const allVals = [...history.top, ...history.mid, ...history.bot].filter(v => v !== null);"
"  if (allVals.length > 0) {"
"    min = Math.floor(Math.min(...allVals) / 10) * 10 - 5;"
"    max = Math.ceil(Math.max(...allVals) / 10) * 10 + 5;"
"    if (max - min < 20) { max = min + 20; }"
"  }"
"  ctx.strokeStyle = '#1e293b'; ctx.lineWidth = 1;"
"  ctx.fillStyle = '#64748b'; ctx.font = '10px monospace'; ctx.textAlign = 'right';"
"  for (let i = 0; i <= 4; i++) {"
"    const y = pad.top + (ph / 4) * i;"
"    const val = (max - ((max - min) / 4) * i).toFixed(0);"
"    ctx.beginPath(); ctx.moveTo(pad.left, y); ctx.lineTo(w - pad.right, y); ctx.stroke();"
"    ctx.fillText(val + '°C', pad.left - 8, y + 3);"
"  }"
"  function plot(data, color) {"
"    if (data.length < 2) return;"
"    ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath();"
"    let started = false;"
"    data.forEach((val, idx) => {"
"      if (val === null) { started = false; return; }"
"      const x = pad.left + (pw / (history.maxPoints - 1)) * (idx + (history.maxPoints - data.length));"
"      const y = pad.top + ph - ((val - min) / (max - min)) * ph;"
"      if (!started) { ctx.moveTo(x, y); started = true; } else { ctx.lineTo(x, y); }"
"    });"
"    ctx.stroke();"
"  }"
"  plot(history.top, '#38bdf8');"
"  plot(history.mid, '#f59e0b');"
"  plot(history.bot, '#10b981');"
"}"
"async function poll() {"
"  try {"
"    const res = await fetch('/api/status');"
"    if (!res.ok) return;"
"    const d = await res.json();"
"    document.getElementById('pill-uptime').textContent = `Uptime: ${d.device.uptime_s}s`;"
"    document.getElementById('pill-fw').textContent = `FW: v${d.device.firmware}`;"
"    function updateZone(prefix, temp, valid, open, rate) {"
"      const valEl = document.getElementById(`val-${prefix}`);"
"      const badgeEl = document.getElementById(`badge-${prefix}`);"
"      const rateEl = document.getElementById(`rate-${prefix}`);"
"      if (open) {"
"        valEl.textContent = 'OPEN'; valEl.style.color = 'var(--accent-rose)';"
"        badgeEl.textContent = 'DISCONNECTED'; badgeEl.className = 'badge badge-error';"
"        rateEl.textContent = '-- °C/s';"
"      } else if (!valid) {"
"        valEl.textContent = 'FAULT'; valEl.style.color = 'var(--accent-rose)';"
"        badgeEl.textContent = 'ERROR'; badgeEl.className = 'badge badge-error';"
"        rateEl.textContent = '-- °C/s';"
"      } else {"
"        valEl.textContent = `${temp.toFixed(2)} °C`; valEl.style.color = 'var(--text)';"
"        badgeEl.textContent = 'HEALTHY'; badgeEl.className = 'badge badge-active';"
"        rateEl.textContent = `${rate >= 0 ? '+' : ''}${rate.toFixed(2)} °C/s`;"
"      }"
"    }"
"    updateZone('top', d.temperature.top_c, d.temperature.top_valid, d.temperature.top_open, d.temperature.top_rate);"
"    updateZone('mid', d.temperature.middle_c, d.temperature.middle_valid, d.temperature.middle_open, d.temperature.middle_rate);"
"    updateZone('bot', d.temperature.bottom_c, d.temperature.bottom_valid, d.temperature.bottom_open, d.temperature.bottom_rate);"
"    history.top.push(d.temperature.top_valid ? d.temperature.top_c : null);"
"    history.mid.push(d.temperature.middle_valid ? d.temperature.middle_c : null);"
"    history.bot.push(d.temperature.bottom_valid ? d.temperature.bottom_c : null);"
"    if (history.top.length > history.maxPoints) { history.top.shift(); history.mid.shift(); history.bot.shift(); }"
"    drawChart();"
"    const badgeState = document.getElementById('badge-state');"
"    badgeState.textContent = d.kiln.state;"
"    badgeState.className = `badge badge-${d.kiln.state.toLowerCase()}`;"
"    document.getElementById('m-batch').textContent = d.kiln.batch_id || 'NONE';"
"    document.getElementById('m-duration').textContent = `${d.kiln.duration_s} s`;"
"    const badgeGps = document.getElementById('badge-gps');"
"    if (d.gnss.location_valid) {"
"      badgeGps.textContent = 'FIXED'; badgeGps.className = 'badge badge-active';"
"    } else {"
"      badgeGps.textContent = 'SEARCHING'; badgeGps.className = 'badge badge-idle';"
"    }"
"    document.getElementById('m-lat').textContent = d.gnss.location_valid ? d.gnss.latitude.toFixed(6) : '0.000000';"
"    document.getElementById('m-lon').textContent = d.gnss.location_valid ? d.gnss.longitude.toFixed(6) : '0.000000';"
"    document.getElementById('m-sat').textContent = d.gnss.satellites;"
"    document.getElementById('m-utc').textContent = d.gnss.time_valid ? d.gnss.utc_time : 'UNAVAILABLE';"
"  } catch (err) {"
"    console.error('API poll failed', err);"
"  }"
"}"
"setInterval(poll, 1000);"
"poll();"
"</script>"
"</body>"
"</html>";

/* Handler for GET / */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Connection", "close");
    return httpd_resp_send(req, s_dashboard_html, HTTPD_RESP_USE_STRLEN);
}

/* Handler for GET /api/status */
static esp_err_t api_status_get_handler(httpd_req_t *req)
{
    device_state_t st = data_model_get_snapshot();

    char resp_buf[1024];
    snprintf(resp_buf, sizeof(resp_buf),
        "{\n"
        "  \"device\": {\n"
        "    \"name\": \"%s\",\n"
        "    \"uptime_s\": %lu,\n"
        "    \"firmware\": \"%s\"\n"
        "  },\n"
        "  \"kiln\": {\n"
        "    \"state\": \"%s\",\n"
        "    \"batch_id\": %s%s%s,\n"
        "    \"duration_s\": %lu\n"
        "  },\n"
        "  \"temperature\": {\n"
        "    \"top_c\": %.2f,\n"
        "    \"middle_c\": %.2f,\n"
        "    \"bottom_c\": %.2f,\n"
        "    \"top_valid\": %s,\n"
        "    \"middle_valid\": %s,\n"
        "    \"bottom_valid\": %s,\n"
        "    \"top_open\": %s,\n"
        "    \"middle_open\": %s,\n"
        "    \"bottom_open\": %s,\n"
        "    \"top_rate\": %.2f,\n"
        "    \"middle_rate\": %.2f,\n"
        "    \"bottom_rate\": %.2f\n"
        "  },\n"
        "  \"gnss\": {\n"
        "    \"latitude\": %.6f,\n"
        "    \"longitude\": %.6f,\n"
        "    \"location_valid\": %s,\n"
        "    \"time_valid\": %s,\n"
        "    \"satellites\": %u,\n"
        "    \"utc_epoch\": %lld,\n"
        "    \"utc_time\": \"%s\"\n"
        "  },\n"
        "  \"system\": {\n"
        "    \"wifi\": %s\n"
        "  }\n"
        "}",
        st.name,
        (unsigned long)st.uptime_s,
        st.firmware,
        kiln_state_to_str(st.kiln_state),
        strcmp(st.batch_id, "NONE") == 0 ? "null" : "\"",
        strcmp(st.batch_id, "NONE") == 0 ? "" : st.batch_id,
        strcmp(st.batch_id, "NONE") == 0 ? "" : "\"",
        (unsigned long)st.session_duration_s,
        st.top_c,
        st.middle_c,
        st.bottom_c,
        st.top_valid ? "true" : "false",
        st.middle_valid ? "true" : "false",
        st.bottom_valid ? "true" : "false",
        st.top_open ? "true" : "false",
        st.middle_open ? "true" : "false",
        st.bottom_open ? "true" : "false",
        st.top_rate,
        st.middle_rate,
        st.bottom_rate,
        st.latitude,
        st.longitude,
        st.location_valid ? "true" : "false",
        st.time_valid ? "true" : "false",
        (unsigned int)st.satellites,
        (long long)st.utc_epoch,
        st.utc_str,
        st.wifi_active ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
    return httpd_resp_send(req, resp_buf, HTTPD_RESP_USE_STRLEN);
}

static const httpd_uri_t uri_get_index = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_get_status = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = api_status_get_handler,
    .user_ctx  = NULL
};

esp_err_t start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    ESP_LOGI(TAG, "Starting HTTP server on port: %d", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret == ESP_OK) {
        httpd_register_uri_handler(s_server, &uri_get_index);
        httpd_register_uri_handler(s_server, &uri_get_status);
        ESP_LOGI(TAG, "HTTP server handlers registered: / and /api/status");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Error starting HTTP server: %s", esp_err_to_name(ret));
    return ret;
}

void stop_web_server(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

static bool s_wifi_running = false;

esp_err_t wifi_init_softap(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi SoftAP (SSID: %s)", WIFI_AP_SSID);

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    if (strlen(WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Reduce Wi-Fi TX power from 20 dBm (80) to 13 dBm (52) to cut heat dissipation by >50% */
    esp_wifi_set_max_tx_power(52);

    ESP_LOGI(TAG, "Wi-Fi SoftAP started successfully (TX Power: 13 dBm). IP: 192.168.4.1");
    s_wifi_running = true;
    data_model_update_system(true);
    return ESP_OK;
}

void wifi_stop_softap(void)
{
    if (s_wifi_running) {
        stop_web_server();
        esp_wifi_stop();
        s_wifi_running = false;
        data_model_update_system(false);
        ESP_LOGI(TAG, "Wi-Fi SoftAP & Web Server stopped (BLE connected - thermal power-save active)");
    }
}

void wifi_resume_softap(void)
{
    if (!s_wifi_running) {
        esp_err_t ret = esp_wifi_start();
        if (ret == ESP_OK) {
            esp_wifi_set_max_tx_power(52);
            start_web_server();
            s_wifi_running = true;
            data_model_update_system(true);
            ESP_LOGI(TAG, "Wi-Fi SoftAP & Web Server resumed (BLE disconnected)");
        } else {
            ESP_LOGE(TAG, "Failed to restart Wi-Fi: %s", esp_err_to_name(ret));
        }
    }
}
