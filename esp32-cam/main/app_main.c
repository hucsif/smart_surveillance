/* ESPRESSIF MIT License
 * 
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 * 
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "app_camera.h"
#include "app_wifi.h"
#include "app_httpd.h"
#include "app_mdns.h"

/*
步骤1：用户输入网址
    用户 → 浏览器 → 输入：http://esp32-camera.local/

步骤2：DNS解析（mDNS作用）
    浏览器 → 发送mDNS查询：esp32-camera.local在哪里？
    ESP32 → 响应mDNS：我是192.168.1.100

步骤3：HTTP请求
    浏览器 → 发送HTTP请求到192.168.1.100:80

步骤4：服务器处理（app_httpd_main的作用）
    HTTP服务器 → 调用index_handler
                → 返回HTML控制页面

步骤5：控制摄像头
    用户点击"拍照"按钮 → 浏览器发送请求到/control
                     → cmd_handler处理
                     → 调用摄像头API拍照
                     → 返回图像数据

步骤6：实时视频流
    用户点击"实时视频" → 浏览器连接到81端口的stream_uri
                     → stream_handler启动MJPEG流
                     → 持续发送摄像头帧
*/

void app_main()
{
    app_wifi_main();     // 第一步：连接网络
    app_camera_main();   // 第二步：初始化摄像头硬件
    app_httpd_main();    // 第三步：启动Web服务器
    app_mdns_main();     // 第四步：注册网络服务
}
