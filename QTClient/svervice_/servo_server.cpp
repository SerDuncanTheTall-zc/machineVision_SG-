#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define PERIOD_NS 20000000
#define ANGLE_60  1166666
#define ANGLE_120 1833333

// 基础写入函数
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
}

/**
 * @brief 初始化指定的 PWM 通道
 * @param chip_num 控制器编号 (0, 1, 2)
 * @param period 周期 (ns)
 * @param initial_duty 初始占空比 (ns)
 */
void pwm_init(int chip_num, int period, int initial_duty) {
    char path[128];
    char val_str[32];

    // 1. 导出通道
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/export", chip_num);
    write_sysfs(path, "0");

    // 给内核生成 sysfs 节点的时间
    usleep(200000);

    // 2. 设定周期
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/period", chip_num);
    snprintf(val_str, sizeof(val_str), "%d", period);
    write_sysfs(path, val_str);

    // 3. 设定极性
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/polarity", chip_num);
    write_sysfs(path, "normal");

    // 4. 设定初始占空比
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/duty_cycle", chip_num);
    snprintf(val_str, sizeof(val_str), "%d", initial_duty);
    write_sysfs(path, val_str);

    // 5. 开启使能
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/enable", chip_num);
    write_sysfs(path, "1");

    printf("[INFO] pwmchip%d 初始化成功\n", chip_num);
}

/**
 * @brief 设置指定 PWM 通道的占空比
 * @param chip_num 控制器编号 (0, 1, 2)
 * @param duty 占空比 (ns)
 */
void pwm_set_duty(int chip_num, int duty) {
    char path[128];
    char val_str[32];
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/duty_cycle", chip_num);
    snprintf(val_str, sizeof(val_str), "%d", duty);
    write_sysfs(path, val_str);
}

int main() {
    printf("🚀 三舵机独立控制程序启动...\n");
    printf("🔔 按 Ctrl+C 停止程序\n");

    // 1. 初始化 3 个 PWM 控制器
    pwm_init(0, PERIOD_NS, ANGLE_60);
    pwm_init(1, PERIOD_NS, ANGLE_60);
    pwm_init(2, PERIOD_NS, ANGLE_60);

    // 状态结构体，用于让 3 个舵机以不同的步长或方向运动
    int current_duty[3] = { ANGLE_60, ANGLE_60, ANGLE_60 };
    int step[3]         = { 8000,     12000,    15000 };    // 不同的速度
    int direction[3]    = { 1,        1,        1     };

    while (1) {
        // 更新并写入 3 个舵机的状态
        for (int i = 0; i < 3; i++) {
            pwm_set_duty(i, current_duty[i]);

            // 边界检查与反向
            if (current_duty[i] >= ANGLE_120) direction[i] = -1;
            if (current_duty[i] <= ANGLE_60)  direction[i] = 1;

            current_duty[i] += (step[i] * direction[i]);
        }

        // 统一控制扫描频率（约 50Hz 刷新率）
        usleep(20000);
    }

    return 0;
}
