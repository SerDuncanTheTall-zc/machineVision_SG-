#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define PWM_PATH "/sys/class/pwm/pwmchip2/pwm0"
#define PWM_EXPORT "/sys/class/pwm/pwmchip2/export"

#define PERIOD_NS 20000000
#define ANGLE_60  1166666  
#define ANGLE_120 1833333  

// 改进后的写入函数，带详细日志
void write_sysfs(const char* file, const char* value) {
    FILE* fp = fopen(file, "w");
    if (!fp) {
        if (errno != EBUSY) {
            fprintf(stderr, "[错误] 无法打开文件 %s: %s\n", file, strerror(errno));
        }
        return;
    }
    fprintf(fp, "%s", value);
    fclose(fp);
    // printf("[LOG] 已写入 %s -> %s\n", value, file); // 调试时可以打开
}

int main() {
    printf("🚀 舵机自动摆动程序启动 (Pin 9)...\n");
    printf("🔔 范围：60° <-> 120° | 按 Ctrl+C 停止\n");

    // 1. 初始化 PWM
    write_sysfs(PWM_EXPORT, "0");
    
    // 给内核多一点时间创建 sysfs 节点 (200毫秒)
    usleep(200000); 

    // ==========================================
    // 🔥 关键修复区：严格遵守 Linux PWM 初始化顺序
    // ==========================================
    
    // 2. 设定周期
    write_sysfs(PWM_PATH "/period", "20000000");

    // 3. 设定极性 (原代码遗漏了这里！)
    write_sysfs(PWM_PATH "/polarity", "normal");

    // 4. 设定初始占空比 (必须在 enable 之前给一个合法值！)
    char val_str[32];
    snprintf(val_str, sizeof(val_str), "%d", ANGLE_60);
    write_sysfs(PWM_PATH "/duty_cycle", val_str);

    // 5. 最后再开启使能
    write_sysfs(PWM_PATH "/enable", "1");
    // ==========================================

    int current_duty = ANGLE_60;
    int step = 10000; 
    int direction = 1; 

    while (1) {
        snprintf(val_str, sizeof(val_str), "%d", current_duty);
        write_sysfs(PWM_PATH "/duty_cycle", val_str);

        if (current_duty >= ANGLE_120) direction = -1;
        if (current_duty <= ANGLE_60)  direction = 1;

        current_duty += (step * direction);
        usleep(20000); 
    }

    return 0;
}