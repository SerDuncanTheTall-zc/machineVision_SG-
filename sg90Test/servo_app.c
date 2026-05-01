#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// 硬件配置：Pin 9 对应 pwmchip2
#define PWM_EXPORT "/sys/class/pwm/pwmchip2/export"
#define PWM_PATH   "/sys/class/pwm/pwmchip2/pwm0"

// SG90 参数 (单位: 纳秒 ns)
#define PERIOD_NS     20000000  // 20ms 周期
#define DUTY_MIN      500000    // 0度 (0.5ms)
#define DUTY_CENTER   1500000   // 90度中心 (1.5ms)
#define DUTY_MAX      2500000   // 180度 (2.5ms)
#define NS_PER_DEGREE 11111     // (2500000 - 500000) / 180

// 通用写入函数 (初始化用)
void sysfs_write(const char *path, const char *value) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        if (errno != EBUSY) perror("写入失败");
        return;
    }
    fprintf(fp, "%s", value);
    fclose(fp);
}

// 初始化 PWM 序列
void init_pwm() {
    sysfs_write(PWM_EXPORT, "0");
    usleep(200000); // 等待节点生成

    sysfs_write(PWM_PATH "/period", "20000000");
    sysfs_write(PWM_PATH "/polarity", "normal");
    
    // 初始位置设为中心 90 度
    char duty_str[16];
    snprintf(duty_str, sizeof(duty_str), "%d", DUTY_CENTER);
    sysfs_write(PWM_PATH "/duty_cycle", duty_str);
    
    sysfs_write(PWM_PATH "/enable", "1");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("用法: %s <left|right|up|down> <角度(0-90)>\n", argv[0]);
        printf("例子: %s left 30  (从中心向左偏转30度)\n", argv[0]);
        return -1;
    }

    char *direction = argv[1];
    int offset = atoi(argv[2]);
    int target_angle = 90; // 默认中心位置

    // 1. 判断左右/上下逻辑
    // 水平舵机 (Pan)：假设增加角度是向左
    if (strcmp(direction, "left") == 0) {
        target_angle = 90 + offset;
    } else if (strcmp(direction, "right") == 0) {
        target_angle = 90 - offset;
    } 
    // 如果你将来接了第二个舵机(Pin 11)，可以增加 up/down 的判断
    else if (strcmp(direction, "up") == 0 || strcmp(direction, "down") == 0) {
        printf("提示: 垂直舵机通常接在 Pin 11 (pwmchip1)，请修改 PWM_PATH 测试。\n");
        // 这里逻辑相同，只需修改 target_angle
        target_angle = (strcmp(direction, "up") == 0) ? (90 + offset) : (90 - offset);
    } else {
        printf("错误: 未知的方向 '%s'\n", direction);
        return -1;
    }

    // 2. 角度限幅 (0-180度安全检查)
    if (target_angle < 0) target_angle = 0;
    if (target_angle > 180) target_angle = 180;

    // 3. 计算占空比
    int duty = DUTY_MIN + (target_angle * NS_PER_DEGREE);

    // 4. 执行写入
    init_pwm(); // 确保 PWM 已开启
    
    FILE *fp = fopen(PWM_PATH "/duty_cycle", "w");
    if (fp) {
        fprintf(fp, "%d", duty);
        fclose(fp);
        printf("✅ 动作执行: 方向=%s, 相对偏移=%d°, 绝对目标=%d°, 占空比=%d ns\n", 
                direction, offset, target_angle, duty);
    } else {
        perror("写入占空比失败");
    }

    return 0;
}