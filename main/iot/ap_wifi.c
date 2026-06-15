#include "ap_wifi.h"
#include "ws_server.h"
#include "cJSON.h"
// #include "esp_spiffs.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define TAG     "apcfg"
extern const char* INDEX_HTML;

//html网页在spiffs文件系统中的路径
// #define INDEX_HTML_PATH "/spiffs/apcfg.html"

//html网页缓存
char* index_html = NULL;

#define APCFG_BIT   (BIT0)


/** 从spiffs中加载html页面到内存
 * @param 无
 * @return 无 
*/
/*
static char* load_html_page(void)
{
    //定义挂载点
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",            //挂载点
        .partition_label = "html",         //分区名称
        .max_files = 5,                    //最大打开的文件数
        .format_if_mount_failed = false    //挂载失败是否执行格式化
        };
    //挂载spiffs
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    //查找文件是否存在
    struct stat st;
    if (stat(INDEX_HTML_PATH, &st))
    {
        ESP_LOGE(TAG, "apcfg.html not found");
        return NULL;
    }
    //打开html文件并且读取到内存中
    char* page = (char*)malloc(st.st_size + 1);
    if(!page)
    {
        return NULL;
    }
    memset(page,0,st.st_size + 1);
    FILE *fp = fopen(INDEX_HTML_PATH, "r");
    if (fread(page, st.st_size, 1, fp) == 0)
    {
        free(page);
        page = NULL;
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);
    return page;
}
*/

/** wifi功能和ap配网功能初始化
 * @param f wifi连接状态回调函数
 * @return 无 
*/
void ap_wifi_init(p_wifi_state_callback f)
{
    // 步骤1: 初始化网页缓冲区
    index_html = (char*)INDEX_HTML;  // 使用硬编码
    
    // 步骤2: 初始化WiFi管理器
    wifi_init_sta(f);                  // 初始化WiFi协议栈

}

/** 连接某个热点
 * @param ssid
 * @param password
 * @return 无 
*/
void ap_wifi_set(const char* ssid,const char* password)
{
    wifi_manager_connect(ssid,password);
}

