#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

// ---------------- 配置参数 ----------------
#define SERVER_PORT 8080
#define PERIOD_NS 20000000
#define DUTY_MIN  500000    // 0度 (保守范围)
#define DUTY_MAX  2500000   // 180度
#define UPDATE_INTERVAL_MS 20

// CRC-8-ATM 查找表 (与客户端一致)
static const uint8_t s_crc8Table[256] = {
    0x00,0x07,0x0E,0x09,0x1C,0x1B,0x12,0x15,0x38,0x3F,0x36,0x31,0x24,0x23,0x2A,0x2D,
    0x70,0x77,0x7E,0x79,0x6C,0x6B,0x62,0x65,0x48,0x4F,0x46,0x41,0x54,0x53,0x5A,0x5D,
    0xE0,0xE7,0xEE,0xE9,0xFC,0xFB,0xF2,0xF5,0xD8,0xDF,0xD6,0xD1,0xC4,0xC3,0xCA,0xCD,
    0x90,0x97,0x9E,0x99,0x8C,0x8B,0x82,0x85,0xA8,0xAF,0xA6,0xA1,0xB4,0xB3,0xBA,0xBD,
    0xC7,0xC0,0xC9,0xCE,0xDB,0xDC,0xD5,0xD2,0xFF,0xF8,0xF1,0xF6,0xE3,0xE4,0xED,0xEA,
    0xB7,0xB0,0xB9,0xBE,0xAB,0xAC,0xA5,0xA2,0x8F,0x88,0x81,0x86,0x93,0x94,0x9D,0x9A,
    0x27,0x20,0x29,0x2E,0x3B,0x3C,0x35,0x32,0x1F,0x18,0x11,0x16,0x03,0x04,0x0D,0x0A,
    0x57,0x50,0x59,0x5E,0x4B,0x4C,0x45,0x42,0x6F,0x68,0x61,0x66,0x73,0x74,0x7D,0x7A,
    0x89,0x8E,0x87,0x80,0x95,0x92,0x9B,0x9C,0xB1,0xB6,0xBF,0xB8,0xAD,0xAA,0xA3,0xA4,
    0xF9,0xFE,0xF7,0xF0,0xE5,0xE2,0xEB,0xEC,0xC1,0xC6,0xCF,0xC8,0xDD,0xDA,0xD3,0xD4,
    0x69,0x6E,0x67,0x60,0x75,0x72,0x7B,0x7C,0x51,0x56,0x5F,0x58,0x4D,0x4A,0x43,0x44,
    0x19,0x1E,0x17,0x10,0x05,0x02,0x0B,0x0C,0x21,0x26,0x2F,0x28,0x3D,0x3A,0x33,0x34,
    0x4E,0x49,0x40,0x47,0x52,0x55,0x5C,0x5B,0x76,0x71,0x78,0x7F,0x6A,0x6D,0x64,0x63,
    0x3E,0x39,0x30,0x37,0x22,0x25,0x2C,0x2B,0x06,0x01,0x08,0x0F,0x1A,0x1D,0x14,0x13,
    0xAE,0xA9,0xA0,0xA7,0xB2,0xB5,0xBC,0xBB,0x96,0x91,0x98,0x9F,0x8A,0x8D,0x84,0x83,
    0xDE,0xD9,0xD0,0xD7,0xC2,0xC5,0xCC,0xCB,0xE6,0xE1,0xE8,0xEF,0xFA,0xFD,0xF4,0xF3
};

uint8_t calc_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc = s_crc8Table[crc ^ data[i]];
    }
    return crc;
}

// ---------------- 硬件控制模块 ----------------
class PwmServo {
private:
    int chip_num;
    int duty_fd; // 缓存文件描述符以提高性能
    int current_duty;

    void write_sysfs(const char* file, const char* value) {
        int fd = open(file, O_WRONLY);
        if (fd < 0) return;
        write(fd, value, strlen(value));
        close(fd);
    }

public:
    PwmServo(int chip) : chip_num(chip), duty_fd(-1), current_duty(1500000) {}

    void init() {
        char path[128], val_str[32];
        
        // 1. Export
        snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/export", chip_num);
        write_sysfs(path, "0");
        usleep(200000); // 等待 udev 创建节点

        // 2. Period
        snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/period", chip_num);
        snprintf(val_str, sizeof(val_str), "%d", PERIOD_NS);
        write_sysfs(path, val_str);

        // 3. Polarity
        snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/polarity", chip_num);
        write_sysfs(path, "normal");

        // 4. Initial Duty
        set_duty(current_duty);

        // 5. Enable
        snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/enable", chip_num);
        write_sysfs(path, "1");

        // 缓存 duty_cycle 的文件描述符，供高频写入使用
        snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/duty_cycle", chip_num);
        duty_fd = open(path, O_WRONLY);
        
        std::cout << "[INFO] PWM " << chip_num << " 初始化完成." << std::endl;
    }

    void set_duty(int duty) {
        if (duty < DUTY_MIN) duty = DUTY_MIN;
        if (duty > DUTY_MAX) duty = DUTY_MAX;
        current_duty = duty;

        char val_str[32];
        int len = snprintf(val_str, sizeof(val_str), "%d", duty);
        
        if (duty_fd >= 0) {
            pwrite(duty_fd, val_str, len, 0); // 使用 pwrite 避免每次重置指针
        } else {
            char path[128];
            snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/duty_cycle", chip_num);
            write_sysfs(path, val_str);
        }
    }

    int get_duty() const { return current_duty; }
};

// ---------------- 伺服管理模块 ----------------
class ServoManager {
private:
    PwmServo pan_servo{0};  // PWM0 控制云台水平
    PwmServo tilt_servo{1}; // PWM1 控制云台垂直

    std::atomic<int> pan_step{0};
    std::atomic<int> tilt_step{0};
    std::atomic<bool> is_running{true};
    std::thread motion_thread;

    void motion_loop() {
        while (is_running) {
            if (pan_step != 0) {
                pan_servo.set_duty(pan_servo.get_duty() + pan_step);
            }
            if (tilt_step != 0) {
                tilt_servo.set_duty(tilt_servo.get_duty() + tilt_step);
            }
            usleep(UPDATE_INTERVAL_MS * 1000); // 50Hz 刷新率
        }
    }

public:
    ServoManager() {
        pan_servo.init();
        tilt_servo.init();
        motion_thread = std::thread(&ServoManager::motion_loop, this);
    }

    ~ServoManager() {
        is_running = false;
        if (motion_thread.joinable()) motion_thread.join();
    }

    // 处理绝对移动 (0.01度，假设 0~180度 对应 0~18000)
    void set_abs(int16_t pan, int16_t tilt) {
        pan_step = 0; tilt_step = 0; // 停止相对运动
        
        // 将角度 (0~18000) 映射到占空比 (DUTY_MIN ~ DUTY_MAX)
        int pan_duty = DUTY_MIN + (int)((pan / 18000.0) * (DUTY_MAX - DUTY_MIN));
        int tilt_duty = DUTY_MIN + (int)((tilt / 18000.0) * (DUTY_MAX - DUTY_MIN));
        
        pan_servo.set_duty(pan_duty);
        tilt_servo.set_duty(tilt_duty);
        std::cout << "[CMD] 绝对定位 -> Pan: " << pan << " Tilt: " << tilt << std::endl;
    }

    // 处理连续相对移动
    void set_move(uint8_t dir, uint8_t speed) {
        // speed (0-255) 映射到每次循环的步进值 ns
        int step = speed * 150; 
        
        switch (dir) {
            case 0: // DIR_STOP
                pan_step = 0; tilt_step = 0; break;
            case 1: // DIR_UP (Tilt增加)
                pan_step = 0; tilt_step = step; break;
            case 2: // DIR_DOWN (Tilt减少)
                pan_step = 0; tilt_step = -step; break;
            case 3: // DIR_LEFT (Pan减少)
                pan_step = -step; tilt_step = 0; break;
            case 4: // DIR_RIGHT (Pan增加)
                pan_step = step; tilt_step = 0; break;
        }
    }
};

// ---------------- TCP 服务器模块 ----------------
class ProtocolServer {
private:
    ServoManager servo_mgr;
    std::vector<uint8_t> rx_buffer;

    void process_command(const uint8_t* payload, uint16_t len) {
        if (len < 1) return;
        uint8_t cmd = payload[0];
        const uint8_t* data = payload + 1;
        int data_len = len - 1;

        switch (cmd) {
            case 0x01: // SERVO_MOVE
                if (data_len >= 3) {
                    uint8_t id = data[0]; // 这里暂忽略id，默认整体控制
                    uint8_t dir = data[1];
                    uint8_t speed = data[2];
                    servo_mgr.set_move(dir, speed);
                }
                break;
            case 0x02: // SERVO_ABS
                if (data_len >= 5) {
                    uint8_t id = data[0];
                    int16_t pan  = (int16_t)((data[1] << 8) | data[2]);
                    int16_t tilt = (int16_t)((data[3] << 8) | data[4]);
                    servo_mgr.set_abs(pan, tilt);
                }
                break;
            case 0x03: // TRACKING_CTRL
                if (data_len >= 1) std::cout << "[CMD] 追踪开关: " << (int)data[0] << std::endl;
                break;
            case 0x04: // STREAM_CTRL
                if (data_len >= 5) std::cout << "[CMD] 流控配置..." << std::endl;
                break;
            case 0x05: // PING
                std::cout << "[CMD] 心跳包 (Ping)" << std::endl;
                break;
            default:
                std::cerr << "[WARN] 未知指令: 0x" << std::hex << (int)cmd << std::dec << std::endl;
                break;
        }
    }

    void parse_buffer() {
        while (rx_buffer.size() >= 5) { // 最小包头长度：AA 55 LenH LenL Cmd
            // 1. 寻找同步头
            if (rx_buffer[0] != 0xAA || rx_buffer[1] != 0x55) {
                rx_buffer.erase(rx_buffer.begin());
                continue;
            }

            // 2. 解析长度 (大端序)
            uint16_t payload_len = (rx_buffer[2] << 8) | rx_buffer[3];
            uint16_t frame_len = 5 + payload_len; // 2(sync) + 2(len) + payload + 1(crc)

            if (rx_buffer.size() < frame_len) {
                break; // 数据不够一帧，继续等待
            }

            // 3. 校验 CRC8
            uint8_t rx_crc = rx_buffer[frame_len - 1];
            uint8_t calc_crc = calc_crc8(&rx_buffer[4], payload_len);

            if (rx_crc == calc_crc) {
                process_command(&rx_buffer[4], payload_len);
            } else {
                std::cerr << "[ERROR] CRC 校验失败! 包长度:" << payload_len << std::endl;
            }

            // 4. 移除已处理的一帧
            rx_buffer.erase(rx_buffer.begin(), rx_buffer.begin() + frame_len);
        }
    }

public:
    void run() {
        int server_fd, client_fd;
        struct sockaddr_in address;
        int opt = 1;

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("Socket failed"); return;
        }

        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(SERVER_PORT);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("Bind failed"); return;
        }

        if (listen(server_fd, 3) < 0) {
            perror("Listen failed"); return;
        }

        std::cout << "🚀 TCP 服务器已启动，监听端口: " << SERVER_PORT << std::endl;

        while (true) {
            socklen_t addrlen = sizeof(address);
            std::cout << "⏳ 等待客户端连接..." << std::endl;
            client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            
            if (client_fd < 0) {
                perror("Accept failed"); continue;
            }

            std::cout << "✅ 客户端已连接!" << std::endl;
            rx_buffer.clear(); // 清空历史残留数据
            
            uint8_t buf[1024];
            while (true) {
                ssize_t bytes_read = read(client_fd, buf, sizeof(buf));
                if (bytes_read <= 0) {
                    std::cout << "❌ 客户端断开连接." << std::endl;
                    servo_mgr.set_move(0, 0); // 断开时自动停止舵机
                    close(client_fd);
                    break;
                }
                
                // 将读取的数据追加到缓冲区
                rx_buffer.insert(rx_buffer.end(), buf, buf + bytes_read);
                // 拆包并处理
                parse_buffer(); 
            }
        }
    }
};

int main() {
    std::cout << "=== RK3576 二度舵机控制服务器 ===" << std::endl;
    ProtocolServer server;
    server.run();
    return 0;
}