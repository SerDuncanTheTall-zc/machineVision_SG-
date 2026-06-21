#ifndef SERVO_SERVER_H
#define SERVO_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include <string>

// --- PWM 常量 (与原测试代码一致) ---
#define PERIOD_NS   20000000
#define ANGLE_60_NS 1166666
#define ANGLE_120_NS 1833333
#define ANGLE_90_NS 1500000

// --- 协议常量 (与 QT 客户端 ServoProtocol 一致) ---
#define FRAME_SYNC1  0xAA
#define FRAME_SYNC2  0x55
#define FRAME_HEADER 4

// 命令字
enum CmdType {
    CMD_SERVO_MOVE    = 0x01,  // servo_id(u8) | dir(u8) | speed(u8)
    CMD_SERVO_ABS     = 0x02,  // servo_id(u8) | pan(i16) | tilt(i16)
    CMD_TRACKING_CTRL = 0x03,  // enable(u8)
    CMD_STREAM_CTRL   = 0x04,  // enable(u8) | udp_data_port(u16) | udp_video_port(u16)
    CMD_PING          = 0x05,  // (无载荷)
};

// 方向
enum ServoDir {
    DIR_STOP  = 0,
    DIR_UP    = 1,
    DIR_DOWN  = 2,
    DIR_LEFT  = 3,
    DIR_RIGHT = 4,
};

// --- 单个舵机状态 ---
struct ServoState {
    int    chip_num;        // PWM 控制器编号 (0,1,2)
    int    current_duty;    // 当前占空比 (ns)
    int    target_dir;      // 当前运动方向
    int    speed;           // 每次步进量 (ns)
};

// --- 全局状态 ---
extern ServoState g_servos[3];
extern int        g_tracking_enabled;

// --- 函数声明 ---
void write_sysfs(const char* file, const char* value);
void pwm_init(int chip_num, int period, int initial_duty);
void pwm_set_duty(int chip_num, int duty);

int  servo_server_init(int port);
void servo_server_run();
void servo_server_stop();

// 协议处理
int  parse_frame(const uint8_t* data, int len);
void handle_servo_move(uint8_t servo_id, uint8_t dir, uint8_t speed);
void handle_servo_abs(uint8_t servo_id, int16_t pan, int16_t tilt);
void handle_tracking_ctrl(uint8_t enable);
void handle_stream_ctrl(uint8_t enable, uint16_t udp_data, uint16_t udp_video, const std::string& client_ip);

// CRC-8-ATM
uint8_t crc8(const uint8_t* data, int len);

#endif // SERVO_SERVER_H
