#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>

// 迷你 5x8 字库 (仅包含大写字母 A-Z 和空格，为了演示精简代码)
const unsigned char font5x8[27][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 0: 空格
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 1: A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 2: B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 3: C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 4: D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 5: E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 6: F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 7: G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 8: H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 9: I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 10: J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 11: K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 12: L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 13: M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 14: N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 15: O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 16: P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 17: Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 18: R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 19: S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 20: T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 21: U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 22: V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 23: W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 24: X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 25: Y
    {0x61, 0x51, 0x49, 0x45, 0x43}  // 26: Z
};

class OLED {
private:
    int fd;

    // 发送单条命令
    void sendCommand(unsigned char cmd) {
        unsigned char buffer[2] = {0x00, cmd};
        write(fd, buffer, 2);
    }

    // 字符转字库索引
    int getFontIndex(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
        return 0; // 默认空格
    }

public:
    OLED(const char* device, int addr) {
        fd = open(device, O_RDWR);
        ioctl(fd, I2C_SLAVE, addr);
    }

    ~OLED() { close(fd); }

    void init() {
        unsigned char init_cmds[] = {
            0xAE,       // 关显示
            0x20, 0x00, // 设置内存寻址模式为：水平寻址 (写满一行自动换行，写字必备)
            0x21, 0x00, 0x7F, // 设置列地址范围 0-127
            0x22, 0x00, 0x07, // 设置页地址范围 0-7
            0x8D, 0x14, // 开启电荷泵
            0xAF        // 开显示
        };
        for (unsigned char cmd : init_cmds) sendCommand(cmd);
    }

    // 清空屏幕 (写入全 0)
    void clear() {
        unsigned char buffer[1025];
        buffer[0] = 0x40; // 数据标识
        memset(&buffer[1], 0x00, 1024); // 填满 0 (灭掉所有像素点)
        write(fd, buffer, 1025);
    }

    // 打印字符串
    void printString(const char* str) {
        unsigned char buffer[512]; // 缓存区
        buffer[0] = 0x40; // 数据标识
        int index = 1;

        while (*str) {
            int font_idx = getFontIndex(*str);
            for (int i = 0; i < 5; i++) {
                buffer[index++] = font5x8[font_idx][i];
            }
            buffer[index++] = 0x00; // 字符间隙(空一列)
            str++;
        }
        write(fd, buffer, index);
    }
};

int main() {
    OLED screen("/dev/i2c-7", 0x3c);
    
    screen.init();
    screen.clear(); // 这一步会瞬间清除花屏！
    
    // 只能输入大写字母和空格哦！
    screen.printString("HELLO KICKPI");
    screen.printString(" AHIWDUHAIWUDHUI  "); // 加点空格占位
    screen.printString("HELLO JOJO");
    
    std::cout << "✅ 文本已发送！" << std::endl;
    return 0;
}
