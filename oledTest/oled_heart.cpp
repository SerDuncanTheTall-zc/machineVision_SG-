#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>
#include <cmath>

// --- OLED 驱动类 (带显存) ---
class OLEDDisplay {
private:
    int fd;
    unsigned char buffer[1025]; // 1024字节显存 + 1字节控制字

    void sendCommand(unsigned char cmd) {
        unsigned char buf[2] = {0x00, cmd};
        write(fd, buf, 2);
    }

public:
    OLEDDisplay(const char* device, int addr) {
        fd = open(device, O_RDWR);
        ioctl(fd, I2C_SLAVE, addr);
        buffer[0] = 0x40; // 数据标识
    }
    ~OLEDDisplay() { close(fd); }

    void init() {
        unsigned char init_cmds[] = {
            0xAE, 0x20, 0x00, 0x21, 0x00, 0x7F, 0x22, 0x00, 0x07, 0x8D, 0x14, 0xAF
        };
        for (unsigned char cmd : init_cmds) sendCommand(cmd);
    }

    void clearBuffer() {
        memset(&buffer[1], 0x00, 1024);
    }

    void drawPixel(int x, int y) {
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return; // 越界保护
        int page = y / 8;
        int bit = y % 8;
        int index = 1 + x + (page * 128);
        buffer[index] |= (1 << bit);
    }

    void display() {
        sendCommand(0x21); sendCommand(0x00); sendCommand(0x7F);
        sendCommand(0x22); sendCommand(0x00); sendCommand(0x07);
        write(fd, buffer, 1025);
    }
};

int main() {
    OLEDDisplay oled("/dev/i2c-7", 0x3c);
    oled.init();

    double theta = 0.0;       // 旋转角度
    double scale = 1.0;       // 缩放比例 (用来实现心跳)
    double scale_step = 0.05; // 每次跳动的幅度

    std::cout << "💖 数学魔法启动！按 Ctrl+C 退出。" << std::endl;

    while (true) {
        oled.clearBuffer();

        // 1. 更新心跳缩放比例 (在 1.0 到 1.5 之间来回变化)
        scale += scale_step;
        if (scale > 1.5 || scale < 1.0) {
            scale_step = -scale_step; // 反转缩放方向，形成“噗通”跳动感
        }

        // 2. 更新旋转角度 (每次转一点点)
        theta += 0.1;

        // 3. 用数学公式画出爱心的每一个点
        for (double t = 0; t <= 2 * M_PI; t += 0.05) {
            
            // a. 基础爱心参数方程
            double hx = 16 * pow(sin(t), 3);
            // 屏幕的 Y 轴是向下的，所以加个负号反转过来
            double hy = -(13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t)); 

            // b. 矩阵旋转计算
            double rx = hx * cos(theta) - hy * sin(theta);
            double ry = hx * sin(theta) + hy * cos(theta);

            // c. 加上缩放，并平移到屏幕正中央 (X: 64, Y: 32)
            // 乘以 1.2 是为了微调基础大小，让它占据屏幕的主体
            int px = 64 + (int)(rx * scale * 1.2);
            int py = 32 + (int)(ry * scale * 1.2);

            // d. 点亮这个计算出的像素！
            oled.drawPixel(px, py);
        }

        // 4. 一次性将 1024 字节刷入屏幕
        oled.display();

        // 5. 控制帧率 (大约 20 帧/秒)
        usleep(50000); 
    }

    return 0;
}