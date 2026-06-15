#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ws_server.h"
#include "ap_wifi.h"
#include "cJSON.h"
#include "get_image.h"
#include <inttypes.h>      // 用于PRI宏

static const char *TAG = "WebSocket Server";
//http服务器句柄
httpd_handle_t http_ws_server = NULL;
//连接的客户端fds
static int client_sockfd = -1;

//传感器数据发送任务句柄
QueueHandle_t sensor_queue = NULL;

// ==================== 硬编码 HTML 页面 ====================
const char* INDEX_HTML = 
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "    <title>ESP32 智能监控系统</title>"
    "    <meta charset='UTF-8'>"
    "    <meta name='viewport' content='width=device-width, initial-scale=1'>"
    "    <style>"
    "        body { font-family: Arial; text-align: center; margin: 0; padding: 20px; background: #f0f2f5; }"
    "        .container { max-width: 800px; margin: 0 auto; }"
    "        h1 { color: #1a73e8; }"
    "        .video-container {"
    "            background: black;"
    "            border-radius: 10px;"
    "            overflow: hidden;"
    "            margin: 20px 0 0 0;"
    "            box-shadow: 0 4px 15px rgba(0,0,0,0.2);"
    "        }"
    "        #video-stream {"
    "            width: 100%;"
    "            height: auto;"
    "            display: block;"
    "            min-height: 240px;"
    "            background: #1a1a1a;"
    "        }"
    "        .fire-card {"
    "            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);"
    "            border-radius: 10px;"
    "            padding: 20px;"
    "            margin: 10px 0 20px 0;"
    "            box-shadow: 0 2px 10px rgba(0,0,0,0.1);"
    "            color: white;"
    "        }"
    "        .fire-card .fire-label { font-size: 18px; color: #ffaa00; margin-bottom: 10px; font-weight: bold; }"
    "        .fire-status-value { font-size: 32px; font-weight: bold; margin: 10px 0; }"
    "        .fire-confidence { font-size: 14px; color: #aaa; }"
    "        .fire-safe { color: #44ff44 !important; }"
    "        .fire-alert { color: #ff4444 !important; animation: blink 0.8s infinite; }"
    "        @keyframes blink {"
    "            0% { opacity: 1; text-shadow: 0 0 0px #ff4444; }"
    "            50% { opacity: 0.7; text-shadow: 0 0 10px #ff4444; }"
    "            100% { opacity: 1; text-shadow: 0 0 0px #ff4444; }"
    "        }"
    "        .sensor-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 20px; margin: 20px 0; }"
    "        .sensor-card { background: white; border-radius: 10px; padding: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
    "        .sensor-value { font-size: 48px; font-weight: bold; color: #1a73e8; margin: 10px 0; }"
    "        .sensor-label { font-size: 16px; color: #666; }"
    "        .debug-container {"
    "            background: #1e1e1e;"
    "            border-radius: 10px;"
    "            margin: 20px 0;"
    "            overflow: hidden;"
    "            box-shadow: 0 2px 10px rgba(0,0,0,0.1);"
    "        }"
    "        .debug-header {"
    "            background: #2d2d2d;"
    "            padding: 12px 15px;"
    "            color: #d4d4d4;"
    "            font-weight: bold;"
    "            text-align: left;"
    "            border-bottom: 1px solid #3d3d3d;"
    "            display: flex;"
    "            justify-content: space-between;"
    "            align-items: center;"
    "        }"
    "        .debug-header span { color: #4ec9b0; }"
    "        .clear-btn {"
    "            background: #0e639c;"
    "            border: none;"
    "            color: white;"
    "            padding: 4px 12px;"
    "            border-radius: 4px;"
    "            cursor: pointer;"
    "            font-size: 12px;"
    "        }"
    "        .clear-btn:hover { background: #1177bb; }"
    "        .debug-log {"
    "            height: 200px;"
    "            overflow-y: auto;"
    "            background: #1e1e1e;"
    "            text-align: left;"
    "            font-family: 'Consolas', 'Monaco', monospace;"
    "            font-size: 12px;"
    "        }"
    "        .debug-entry { padding: 6px 12px; border-bottom: 1px solid #2d2d2d; word-wrap: break-word; }"
    "        .debug-entry.info { color: #9cdcfe; }"
    "        .debug-entry.sensor { color: #ce9178; }"
    "        .debug-entry.fire { color: #f48771; font-weight: bold; background: #3d1a1a; }"
    "        .debug-time { color: #6a9955; margin-right: 8px; }"
    "        .status { padding: 10px; border-radius: 5px; margin: 20px 0; }"
    "        .connected { background: #d4edda; color: #155724; }"
    "        .disconnected { background: #f8d7da; color: #721c24; }"
    "        .footer { margin-top: 30px; color: #666; font-size: 12px; }"
    "    </style>"
    "</head>"
    "<body>"
    "    <div class='container'>"
    "        <h1>📊 ESP32 智能监控系统</h1>"
    "        <div id='status' class='status disconnected'>🔴 连接断开</div>"
    ""
    "        <div class='video-container'>"
    "            <img id='video-stream' src='http://192.168.87.56:81/stream' alt='Camera Stream'>"
    "        </div>"
    ""
    "        <div class='fire-card'>"
    "            <div class='fire-label'>🔥 火焰检测结果</div>"
    "            <div class='fire-status-value' id='fire-status'>--</div>"
    "            <div class='fire-confidence' id='fire-confidence'>置信度: --</div>"
    "        </div>"
    ""
    "        <div class='sensor-grid'>"
    "            <div class='sensor-card'>"
    "                <div class='sensor-label'>🌡️ 温度</div>"
    "                <div class='sensor-value' id='temp'>--</div>"
    "                <div>℃</div>"
    "            </div>"
    "            <div class='sensor-card'>"
    "                <div class='sensor-label'>💧 湿度</div>"
    "                <div class='sensor-value' id='humi'>--</div>"
    "                <div>%</div>"
    "            </div>"
    "            <div class='sensor-card'>"
    "                <div class='sensor-label'>☀️ 光照</div>"
    "                <div class='sensor-value' id='light'>--</div>"
    "                <div>lux</div>"
    "            </div>"
    "            <div class='sensor-card'>"
    "                <div class='sensor-label'>🔊 蜂鸣器状态</div>"
    "                <div class='sensor-value' id='beep-status' style='font-size: 36px;'>--</div>"
    "                <div id='beep-text' style='font-size: 14px;'></div>"
    "           </div>"
    "        </div>"
    ""
    "        <div class='debug-container'>"
    "            <div class='debug-header'>"
    "                <span>📡 WebSocket 调试信息</span>"
    "                <button class='clear-btn' id='clear-debug'>清空</button>"
    "            </div>"
    "            <div class='debug-log' id='debug-log'>"
    "                <div class='debug-entry info'>等待连接...</div>"
    "            </div>"
    "        </div>"
    ""
    "        <div class='footer'>ESP32-S3 | 传感器数据实时更新 | 视频监控 | 火焰智能检测</div>"
    "    </div>"
    ""
    "    <script>"
    "        var ws = new WebSocket('ws://' + window.location.host + '/ws');"
    "        var statusDiv = document.getElementById('status');"
    "        var videoImg = document.getElementById('video-stream');"
    "        var debugLog = document.getElementById('debug-log');"
    ""
    "        function addDebugMessage(type, message, rawData) {"
    "            var entry = document.createElement('div');"
    "            entry.className = 'debug-entry ' + type;"
    "            var time = new Date().toLocaleTimeString();"
    "            var displayMsg = message;"
    "            if (rawData) {"
    "                displayMsg = message + ' ' + JSON.stringify(rawData);"
    "            }"
    "            entry.innerHTML = '<span class=\"debug-time\">[' + time + ']</span> ' + displayMsg;"
    "            debugLog.appendChild(entry);"
    "        }"
    ""
    "        document.getElementById('clear-debug').onclick = function() {"
    "            debugLog.innerHTML = '';"
    "            addDebugMessage('info', '调试日志已清空');"
    "        };"
    ""
    "        function updateSensorData(data) {"
    "            if (data.temperature !== undefined) {"
    "                document.getElementById('temp').innerText = data.temperature;"
    "            }"
    "            if (data.humidity !== undefined) {"
    "                document.getElementById('humi').innerText = data.humidity;"
    "            }"
    "            if (data.light_intensity !== undefined) {"
    "                document.getElementById('light').innerText = data.light_intensity;"
    "            }"
    "            if (data.beep_status !== undefined) {"
    "                var beepDiv = document.getElementById('beep-status');"
    "                var beepTextDiv = document.getElementById('beep-text');"
    "                if (data.beep_status === 1) {"
    "                    beepDiv.innerHTML = '🔴 开启';"
    "                    beepDiv.style.color = '#ff4444';"
    "                    beepTextDiv.innerHTML = '蜂鸣器正在鸣响';"
    "                } else {"
    "                    beepDiv.innerHTML = '🟢 关闭';"
    "                    beepDiv.style.color = '#44ff44';"
    "                    beepTextDiv.innerHTML = '蜂鸣器已关闭';"
    "                }"
    "            }"
    "       }"
    ""       
    "        function updateFireStatus(detected, confidence) {"
    "            var fireStatusDiv = document.getElementById('fire-status');"
    "            var fireConfidenceDiv = document.getElementById('fire-confidence');"
    "            if (detected) {"
    "                fireStatusDiv.innerHTML = '🔥 检测到火焰！';"
    "                fireStatusDiv.className = 'fire-status-value fire-alert';"
    "                fireConfidenceDiv.innerHTML = '置信度: ' + (confidence * 100).toFixed(1) + '%';"
    "                document.title = '🔥 火焰告警！ - ESP32监控';"
    "            } else {"
    "                fireStatusDiv.innerHTML = '✅ 安全';"
    "                fireStatusDiv.className = 'fire-status-value fire-safe';"
    "                fireConfidenceDiv.innerHTML = '置信度: ' + (confidence * 100).toFixed(1) + '%';"
    "                document.title = '📊 ESP32 智能监控系统';"
    "            }"
    "        }"
    ""
    "        ws.onopen = function() {"
    "            console.log('WebSocket connected');"
    "            statusDiv.className = 'status connected';"
    "            statusDiv.innerHTML = '🟢 已连接 - 等待数据...';"
    "            addDebugMessage('info', '🔌 WebSocket 连接已建立');"
    "        };"
    ""
    "        ws.onclose = function() {"
    "            console.log('WebSocket disconnected');"
    "            statusDiv.className = 'status disconnected';"
    "            statusDiv.innerHTML = '🔴 连接断开';"
    "            addDebugMessage('info', '⚠️ WebSocket 连接已断开');"
    "        };"
    ""
    "        ws.onerror = function(error) {"
    "            console.log('WebSocket error: ' + error);"
    "            statusDiv.innerHTML = '🔴 连接错误';"
    "            addDebugMessage('info', '❌ WebSocket 连接错误');"
    "        };"
    ""
    "        ws.onmessage = function(event) {"
    "            console.log('Received: ' + event.data);"
    "            var data = JSON.parse(event.data);"
    "            "
    "            addDebugMessage('info', '📩 收到消息', data);"
    "            "
    "            if (data.detected !== undefined && data.confidence !== undefined) {  "
    "                addDebugMessage('fire', '🔥 火焰检测结果: detected=' + data.detected + ', confidence=' + data.confidence);  "
    "                updateFireStatus(data.detected, data.confidence);  "
    "            }"
    "            "
    "            updateSensorData(data);"
    "            "
    "            statusDiv.innerHTML = '🟢 已连接 - 数据更新中...';"
    "        };"
    ""
    "        videoImg.onload = function() { console.log('Video loaded'); };"
    "        videoImg.onerror = function() {"
    "            console.log('Video error - 摄像头可能未开启');"
    "            addDebugMessage('info', '⚠️ 视频流加载失败，5秒后重试');"
    "            setTimeout(function() {"
    "                videoImg.src = 'http://192.168.127.56:81/stream?' + new Date().getTime();"
    "            }, 5000);"
    "        };"
    "    </script>"
    "</body>"
    "</html>";

//WebSocket 发送任务
void ws_send_task(void *pvParameters)
{
    sensor_data_t received_data;
    
    ESP_LOGI(TAG, "WebSocket 发送任务启动");
    
    while (1) {
        // 打印套接字
        ESP_LOGI(TAG, "等待队列数据, client_sockfd=%d", client_sockfd);
        // 等待队列中的数据（无限等待）
        if (xQueueReceive(sensor_queue, &received_data, portMAX_DELAY)) {
            
            // 如果有客户端连接，才发送
            if (client_sockfd >= 0 && http_ws_server) {
                // 构建JSON
                cJSON *root = cJSON_CreateObject();
                cJSON_AddNumberToObject(root, "temperature", received_data.temperature);
                cJSON_AddNumberToObject(root, "humidity", received_data.humidity);
                cJSON_AddNumberToObject(root, "light_intensity", received_data.light_intensity);
                cJSON_AddNumberToObject(root, "beep_status", received_data.beep_status);
                
                char *json_str = cJSON_Print(root);
                
                httpd_ws_frame_t ws_pkt = {
                    .payload = (uint8_t*)json_str,
                    .len = strlen(json_str),
                    .type = HTTPD_WS_TYPE_TEXT,
                };
                
                esp_err_t ret = httpd_ws_send_frame_async(http_ws_server, client_sockfd, &ws_pkt);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "发送失败: %d", ret);
                }
                
                cJSON_Delete(root);
                free(json_str);
            } else {
                ESP_LOGW(TAG, "无客户端连接，丢弃数据");
            }
        }
    }
}

// 将采集的传感器数据发送到队列
void ws_queue_sensor_data(sensor_data_t *data)
{
    if (sensor_queue) {
        // 发送到队列，不阻塞
        if (xQueueSend(sensor_queue, data, 0) != pdTRUE) {
            ESP_LOGW(TAG, "传感器队列满，数据丢失");
        }
    }
}

/** 当其他设备WS访问时触发此回调函数
 * @param req http请求
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t handle_ws_req(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "握手结束，新连接已建立");
        //把套接字描述符保存下来，方便后续发送数据用
        client_sockfd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG,"Save client_fds:%d",client_sockfd);
        return ESP_OK;
    }

     // 处理 WebSocket 数据帧
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "接收帧长度失败: %d", ret);
        return ret;
    }
    
    if (ws_pkt.len) {
        uint8_t *buf = malloc(ws_pkt.len + 1);
        if (!buf) return ESP_ERR_NO_MEM;
        
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        
        if (ret == ESP_OK) {
            buf[ws_pkt.len] = 0;
            ESP_LOGI(TAG, "收到数据: %s", buf);
        }
        free(buf);
    }
    
    return ESP_OK;
}

/** 当其他设备http HTTP_GET 访问时，返回html页面
 * @param req http请求
 * @return ESP_OK or ESP_FAIL
*/
esp_err_t get_req_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "收到 HTTP 请求: %s", req->uri);
    
    // 设置响应头
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    // 发送 HTML 页面
    esp_err_t ret = httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "HTML 页面发送成功 (%d 字节)", strlen(INDEX_HTML));
    } else {
        ESP_LOGE(TAG, "HTML 页面发送失败: %d", ret);
    }
    
    return ret;
}

/** 获取最新缓存的摄像头帧
 * @param req http请求
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t latest_frame_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "收到最新帧请求: %s", req->uri);
    
    size_t frame_len = 0;
    uint8_t *frame_data = http_camera_get_frame(&frame_len);
    
    // 检查是否有可用帧
    if (!frame_data || frame_len == 0) {
        ESP_LOGW(TAG, "没有可用的摄像头帧");
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        esp_err_t ret = httpd_resp_send(req, "No frame available", strlen("No frame available"));
        return ret;
    }
    
    ESP_LOGI(TAG, "准备发送帧: %d 字节", frame_len);
    
    // 设置响应头
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    // 发送图像数据
    esp_err_t ret = httpd_resp_send(req, (const char *)frame_data, frame_len);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "最新帧发送成功 (%d 字节)", frame_len);
    } else {
        ESP_LOGE(TAG, "最新帧发送失败: %d", ret);
    }
    
    return ret;
}

esp_err_t web_ws_send(uint8_t* data, int len)
{
    httpd_ws_frame_t pkt;
    memset(&pkt, 0, sizeof(httpd_ws_frame_t));
    pkt.payload = data;
    pkt.len = len;
    pkt.type = HTTPD_WS_TYPE_TEXT;
    return httpd_ws_send_data(http_ws_server, client_sockfd, &pkt);
}

/** 启动WebSocket服务器
 * @param cfg ws一些配置,请看ws_cfg_t定义
 * @return  ESP_OK or ESP_FAIL
*/
esp_err_t start_webserver(void)
{
    //配置http服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;  
    config.server_port = 80;
    config.max_uri_handlers = 20;
    config.stack_size = 8192;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    //注册URI处理
    httpd_uri_t uri_ws =
    {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handle_ws_req,       //处理WebSocket连接
        .is_websocket = true,            //此成员需要再 menuconfig->component config->http server 中勾选 websocket server support 选项，表明该uri专门用于处理websocket的数据
        .handle_ws_control_frames = true,
    };

    httpd_uri_t uri_get = 
    {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_req_handler,     //处理HTTP GET请求
    };

    httpd_uri_t latest_frame_uri = {
        .uri = "/latest_frame",
        .method = HTTP_GET,
        .handler = latest_frame_handler,
        .user_ctx = NULL
    };

    esp_err_t ret = httpd_start(&http_ws_server, &config);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP 服务器启动成功");
        
        // 先注册 WebSocket
        ret = httpd_register_uri_handler(http_ws_server, &uri_ws);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "WebSocket 路由注册失败: %d", ret);
        } else {
            ESP_LOGI(TAG, "WebSocket 路由注册成功: /ws");
        }

        // 再注册普通页面
        ret = httpd_register_uri_handler(http_ws_server, &uri_get);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "HTTP 路由注册失败: %d", ret);
        } else {
            ESP_LOGI(TAG, "HTTP 路由注册成功: /");
        }

        // 注册最新帧接口
        ret = httpd_register_uri_handler(http_ws_server, &latest_frame_uri);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "最新帧路由注册失败: %d", ret);
        } else {
            ESP_LOGI(TAG, "最新帧路由注册成功: /latest_frame");
        }

        ESP_LOGI(TAG, "Web 服务器启动成功，端口: 80");
    }
    else {
        ESP_LOGE(TAG, "HTTP 服务器启动失败: %d", ret);
    }

    return ret;
}

// 发送火焰告警到所有连接的客户端
void ws_send_fire_alert(bool detected, float confidence)
{
    if (client_sockfd < 0 || !http_ws_server) {
        ESP_LOGW(TAG, "无客户端连接，无法发送告警");
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "fire_alert");
    cJSON_AddBoolToObject(root, "detected", detected);
    cJSON_AddNumberToObject(root, "confidence", confidence);
    
    char *json_str = cJSON_Print(root);
    
    httpd_ws_frame_t ws_pkt = {
        .payload = (uint8_t*)json_str,
        .len = strlen(json_str),
        .type = HTTPD_WS_TYPE_TEXT,
    };
    
    esp_err_t ret = httpd_ws_send_frame_async(http_ws_server, client_sockfd, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "火焰告警发送失败: %d", ret);
    } else {
        ESP_LOGI(TAG, "火焰告警已发送: detected=%d, confidence=%.2f", detected, confidence);
    }
    
    cJSON_Delete(root);
    free(json_str);
}

// 停止WebSocket服务
esp_err_t web_ws_stop(void)
{
    if(http_ws_server)
    {
        esp_err_t ret = httpd_stop(http_ws_server);
        http_ws_server = NULL;
        client_sockfd = -1;
        return ret;
    }
    return ESP_OK;
}
