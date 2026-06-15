# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mno-psram-cache")

idf_component_register(
    SRCS 
        "smart_surveillance.c"
        "iot/wifi_manager.c"
        "iot/ws_server.c"
        "iot/ap_wifi.c"
        "hal/my_uart.c"
        "ai_detection/get_image.c"
        "ai_detection/fire_detector.cpp"
        "ai_detection/image_processing.cpp"
        # "../components/esp_driver_jpeg/jpeg_decode.c"
        "ai_detection/models/fire_model.cc"
    INCLUDE_DIRS 
        "."
        "iot"
        "hal"
        "common"
        "ai_detection"
        "ai_detection/models"

        # esp-tflite-micro-master 头文件路径
        #"../components/esp-tflite-micro-master"
        #"../components/esp-tflite-micro-master/third_party/gemmlowp"
        #"../components/esp-tflite-micro-master/third_party/flatbuffers/include"
        #"../components/esp-tflite-micro-master/third_party/ruy"
        #"../components/esp-tflite-micro-master/third_party/kissfft"

        # picojpeg 头文件路径
        #"../components/picojpeg"
        
        # esp_driver_jpeg 头文件路径
        # "../components/esp_driver_jpeg/include"

        # "../components/hal/include"           # 添加通用 hal 路径
        # "../components/hal/esp32s3/include"   # 添加 S3 特定 hal 路径

        
    REQUIRES
        driver
        esp_http_server
        esp_http_client
        json
        esp_wifi
        # spiffs
        nvs_flash
        # esp_driver_jpeg
        esp_timer
        

        esp_common
        esp_rom
        soc
        hal
        esp_system
        freertos
        log
        heap
        newlib
        pthread
)

# 添加编译选项
target_compile_options(${COMPONENT_LIB} PRIVATE
    -mfix-esp32-psram-cache-issue
)

# spiffs_create_partition_image(html ../html FLASH_IN_PROJECT)
