#include "fire_detector.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"

extern "C" {
    #include "hal/my_uart.h" 
}

#include <cstring>
#include <inttypes.h>
#include <cstdint>
#include <cmath>

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#define TAG "FIRE_DETECTOR"

// 模型参数 - 修正为 224x224
#define MODEL_WIDTH 224
#define MODEL_HEIGHT 224
#define CONFIDENCE_THRESHOLD 0.6             // 火焰检测阈值
#define TENSOR_ARENA_SIZE (3 * 1024 * 1024)  // 分配3MB给中间张量

// 引入模型数据
#include "models/fire_model.h"

// 全局变量
EXT_RAM_BSS_ATTR static struct {
    const tflite::Model* model;
    tflite::MicroInterpreter* interpreter;
    TfLiteTensor* input;
    TfLiteTensor* output;
    uint8_t* tensor_arena;
    bool initialized;
} ctx = {};

#ifdef __cplusplus
extern "C" {
#endif

bool fire_detector_init(void) {
    ESP_LOGI(TAG, "初始化火焰分类器...");

    if (ctx.initialized) {
        ESP_LOGW(TAG, "火焰分类器已初始化");
        return true;
    }

    // 1. 打印模型信息
    ESP_LOGI(TAG, "模型数据地址: %p", mobilenetv2_fire_int8_tflite);
    ESP_LOGI(TAG, "模型数据大小: %u bytes", mobilenetv2_fire_int8_tflite_len);
    
    // 打印前16字节（十六进制）
    const uint8_t* data = mobilenetv2_fire_int8_tflite;
    ESP_LOGI(TAG, "模型数据前16字节:");
    ESP_LOG_BUFFER_HEX(TAG, data, 16);
    
    // 验证模型头
    if (data[0] != 0x1c || data[1] != 0x00 || data[2] != 0x00 || data[3] != 0x00) {
        ESP_LOGE(TAG, "无效的模型文件头!");
        ESP_LOGE(TAG, "实际: %02x %02x %02x %02x", data[0], data[1], data[2], data[3]);
        ESP_LOGE(TAG, "期望: 1c 00 00 00");
        return false;
    }
    
    // 2.加载模型
    ctx.model = tflite::GetModel(mobilenetv2_fire_int8_tflite);
    if (!ctx.model) {
        ESP_LOGE(TAG, "GetModel 返回空指针");
        return false;
    }
    ESP_LOGI(TAG, "模型版本: %" PRIu32, ctx.model->version());

    // 3. 分配 Tensor Arena
    ctx.tensor_arena = (uint8_t*)heap_caps_malloc(
        TENSOR_ARENA_SIZE, 
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (!ctx.tensor_arena) {
        ESP_LOGE(TAG, "Tensor Arena分配失败 (尝试 %d KB)", TENSOR_ARENA_SIZE/1024);
        return false;
    }
    ESP_LOGI(TAG, "Tensor Arena分配成功: %d KB", TENSOR_ARENA_SIZE/1024);

    // 4. 创建算子解析器（添加所有需要的算子）
    static tflite::MicroMutableOpResolver<20> resolver;
    
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddFullyConnected();
    resolver.AddAdd();
    resolver.AddMul();
    resolver.AddLogistic();      // Sigmoid
    resolver.AddMean();
    resolver.AddReshape();
    resolver.AddSoftmax();
    // resolver.AddBatchToSpaceND(); // 添加这个算子
    resolver.AddPad();           // 可能需要
    resolver.AddConcatenation(); // 可能需要
    resolver.AddQuantize(); 
    resolver.AddDequantize();
    resolver.AddHardSwish();

    // 5. 创建解释器
    static tflite::MicroInterpreter static_interpreter(
        ctx.model, 
        resolver, 
        ctx.tensor_arena, 
        TENSOR_ARENA_SIZE
    );
    ctx.interpreter = &static_interpreter;

    // 6. 分配张量
    if (ctx.interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "张量分配失败");
        heap_caps_free(ctx.tensor_arena);
        ctx.tensor_arena = NULL;
        return false;
    }

    // 7. 获取输入输出指针
    ctx.input = ctx.interpreter->input(0);
    ctx.output = ctx.interpreter->output(0);

    // 8. 打印模型信息
    ESP_LOGI(TAG, "火焰分类器初始化成功");
    ESP_LOGI(TAG, "  输入: %d x %d x %d, 类型: %d", 
             ctx.input->dims->data[1], 
             ctx.input->dims->data[2], 
             ctx.input->dims->data[3],
             ctx.input->type);
    ESP_LOGI(TAG, "  输出: %d 个值, 类型: %d", 
             ctx.output->dims->data[1],
             ctx.output->type);

    ctx.initialized = true;
    return true;
}

bool fire_detector_run(uint8_t* rgb_data, int width, int height, fire_detection_result_t* result, int temperature) {
    ESP_LOGI(TAG, "=== fire_detector_run 开始 ===");
    
    // ==============开始计时=================
    int64_t start_time = esp_timer_get_time();  // 微秒级计时

    if (!ctx.initialized || !ctx.interpreter || !ctx.input || !ctx.output) {
        ESP_LOGE(TAG, "未初始化");
        return false;
    }

    ESP_LOGI(TAG, "输入尺寸: %dx%d, 期望: %dx%d", width, height, MODEL_WIDTH, MODEL_HEIGHT);
    
    // 检查输入数据
    uint32_t sum_rgb = 0;
    for (int i = 0; i < 100; i++) {
        sum_rgb += rgb_data[i];
    }
    ESP_LOGI(TAG, "输入数据前100字节和: %" PRIu32, sum_rgb);
    
    // 1. 预处理
    uint8_t* input_data = ctx.input->data.uint8;
    float scale_x = (float)width / MODEL_WIDTH;
    float scale_y = (float)height / MODEL_HEIGHT;
    
    for (int y = 0; y < MODEL_HEIGHT; y++) {
        int src_y = (int)(y * scale_y);
        if (src_y >= height) src_y = height - 1;
        
        for (int x = 0; x < MODEL_WIDTH; x++) {
            int src_x = (int)(x * scale_x);
            if (src_x >= width) src_x = width - 1;
            
            int src_idx = (src_y * width + src_x) * 3;
            int dst_idx = (y * MODEL_WIDTH + x) * 3;
            
            input_data[dst_idx]     = rgb_data[src_idx];
            input_data[dst_idx + 1] = rgb_data[src_idx + 1];
            input_data[dst_idx + 2] = rgb_data[src_idx + 2];
        }
    }
    
    // 检查输入张量
    ESP_LOGI(TAG, "输入张量地址: %p", input_data);
    ESP_LOGI(TAG, "输入张量大小: %d", ctx.input->bytes);
    
    // =================记录预处理耗时====================
    int64_t preprocess_time = esp_timer_get_time() - start_time;
    ESP_LOGI(TAG, "预处理耗时: %lld us (%lld ms)", preprocess_time, preprocess_time / 1000);


    // 2. 推理
    ESP_LOGI(TAG, "开始推理...");
    int64_t invoke_start = esp_timer_get_time();
    TfLiteStatus invoke_status = ctx.interpreter->Invoke();
    int64_t invoke_time = esp_timer_get_time() - invoke_start;
    ESP_LOGI(TAG, "推理完成, 状态: %d, 推理耗时: %lld us (%lld ms)", invoke_status, invoke_time, invoke_time / 1000);
    
    if (invoke_status != kTfLiteOk) {
        ESP_LOGE(TAG, "推理失败");
        return false;
    }
    
    // 3. 解析输出
    ESP_LOGI(TAG, "输出类型: %d", ctx.output->type);
    ESP_LOGI(TAG, "输出张量地址: %p", ctx.output->data.uint8);
    
    if (ctx.output->type == kTfLiteUInt8) {
        uint8_t raw_output = ctx.output->data.uint8[0];
        float scale = ctx.output->params.scale;
        int zero_point = ctx.output->params.zero_point;
        float fire_prob = (raw_output - zero_point) * scale;
        
        ESP_LOGI(TAG, "原始输出: %d, scale: %f, zero: %d, 概率: %f", 
                 raw_output, (double)scale, zero_point, (double)fire_prob);
        
        // ========== 多模态融合：根据温度动态调整阈值 ==========
        float dynamic_threshold = CONFIDENCE_THRESHOLD;  // 默认0.6
        
        if (temperature > 40) {
            dynamic_threshold = 0.5;   // 高温环境，降低阈值，提高灵敏度
            ESP_LOGI(TAG, "高温环境(%d℃)，阈值降低至 %.1f", temperature, dynamic_threshold);
        } else if (temperature < 20) {
            dynamic_threshold = 0.7;   // 低温环境，提高阈值，减少误报
            ESP_LOGI(TAG, "低温环境(%d℃)，阈值提高至 %.1f", temperature, dynamic_threshold);
        } else {
            ESP_LOGI(TAG, "正常环境(%d℃)，阈值 %.1f", temperature, dynamic_threshold);
        }

        result->confidence = fire_prob;
        result->detected = (fire_prob > CONFIDENCE_THRESHOLD);
    } else {
        float fire_prob = ctx.output->data.f[0];
        ESP_LOGI(TAG, "Float输出: %f", (double)fire_prob);
        result->confidence = fire_prob;
        result->detected = (fire_prob > CONFIDENCE_THRESHOLD);
    }
    
    ESP_LOGI(TAG, "=== fire_detector_run 结束 ===");

    // ========== 总耗时 ==========
    int64_t total_time = esp_timer_get_time() - start_time;
    ESP_LOGI(TAG, "📊 总耗时统计:");
    ESP_LOGI(TAG, "   预处理: %lld us (%lld ms)", preprocess_time, preprocess_time / 1000);
    ESP_LOGI(TAG, "   推理: %lld us (%lld ms)", invoke_time, invoke_time / 1000);
    ESP_LOGI(TAG, "   总计: %lld us (%lld ms)", total_time, total_time / 1000);

    return true;
}

int fire_detector_get_input_width(void) {
    return MODEL_WIDTH;
}

int fire_detector_get_input_height(void) {
    return MODEL_HEIGHT;
}

const char* fire_detector_get_input_type(void) {
    return "uint8";
}

void fire_detector_deinit(void) {
    if (ctx.tensor_arena) {
        heap_caps_free(ctx.tensor_arena);
        ctx.tensor_arena = NULL;
    }
    memset(&ctx, 0, sizeof(ctx));
    ESP_LOGI(TAG, "火焰分类器已释放");
}

#ifdef __cplusplus
}
#endif