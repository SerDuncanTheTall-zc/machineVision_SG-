#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// 根据你的硬件文档：Pin 11 对应 PWM2_CH6_M2 -> pwmchip1
#define PWM_BASE_PATH "/sys/class/pwm/pwmchip1"
#define PWM_PERIOD    20000000  // 20ms (50Hz)

// 辅助函数：向文件写入字符串并打印日志
int write_pwm_sysfs(const char* node, const char* value) {
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", PWM_BASE_PATH, node);
    
    printf("[LOG] 尝试写入文件: %s 内容: %s\n", full_path, value);
    
    FILE* fp = fopen(full_path, "w");
    if (!fp) {
        // 如果是 export 节点报错 busy，通常意味着已经导出，可以忽略
        if (strstr(node, "export") && errno == EBUSY) {
            printf("[INFO] 节点已导出，无需再次操作。\n");
            return 0;
        }
        fprintf(stderr, "[ERROR] 无法打开文件 %s: %s\n", full_path, strerror(errno));
        return -1;
    }
    
    fprintf(fp, "%s", value);
    fclose(fp);
    printf("[SUCCESS] 写入成功。\n");
    return 0;
}

int main() {
    printf("========== SG90 舵机调试程序 (Pin 11 / pwmchip1) ==========\n");

    // 1. 尝试导出 pwm0 通道
    if (write_pwm_sysfs("export", "0") < 0) {
        printf("[WARN] 导出失败，如果后续操作正常则忽略此项。\n");
    }

    // 2. 设置极性 (必须在 enable 之前)
    write_pwm_sysfs("pwm0/polarity", "normal");

    // 3. 设置周期 (20ms)
    write_pwm_sysfs("pwm0/period", "20000000");

    // 4. 开启 PWM
    write_pwm_sysfs("pwm0/enable", "1");

    // 5. 开始阶梯式测试，确保舵机移动
    long test_points[] = {500000, 1000000, 1500000, 2000000, 2500000};
    char val_str[32];

    for (int i = 0; i < 5; i++) {
        printf("\n--- 正在移动到测试点 %d: %ld ns ---\n", i + 1, test_points[i]);
        snprintf(val_str, sizeof(val_str), "%ld", test_points[i]);
        write_pwm_sysfs("pwm0/duty_cycle", val_str);
        
        printf("[DEBUG] 请观察舵机是否转动... (等待 2 秒)\n");
        sleep(2);
    }

    printf("\n测试完成。如仍未转动，请检查硬件 5V/GND 连线。\n");
    return 0;
}
