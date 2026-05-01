#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>

// --- CPU 数据读取类 ---
class CPUReader {
private:
    long long prev_idle = 0;
    long long prev_total = 0;

public:
    // 获取当前 CPU 占用百分比 (0.0 ~ 100.0)
    double getUsage() {
        std::ifstream file("/proc/stat");
        std::string line;
        std::getline(file, line);
        std::istringstream iss(line);
        
        std::string cpu_label;
        long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
        iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;

        long long current_idle = idle + iowait;
        long long current_non_idle = user + nice + system + irq + softirq + steal;
        long long current_total = current_idle + current_non_idle;

        // 计算差值
        long long total_diff = current_total - prev_total;
        long long idle_diff = current_idle - prev_idle;

        prev_total = current_total;
        prev_idle = current_idle;

        if (total_diff == 0) return 0.0;
        return (double)(total_diff - idle_diff) / total_diff * 100.0;
    }
};

// --- OLED 驱动类 (带显存) ---
class OLEDScope {
private:
    int fd;
    unsigned char buffer[1025]; // 1024字节显存 + 1字节控制字

    void sendCommand(unsigned char cmd) {
        unsigned char buf[2] = {0x00, cmd};
        write(fd, buf, 2);
    }

public:
    OLEDScope(const char* device, int addr) {
        fd = open(device, O_RDWR);
        ioctl(fd, I2C_SLAVE, addr);
        buffer[0] = 0x40; // 0x40 代表发的是像素数据
    }
    ~OLEDScope() { close(fd); }

    void init() {
        unsigned char init_cmds[] = {
            0xAE, 0x20, 0x00, 0x21, 0x00, 0x7F, 0x22, 0x00, 0x07, 0x8D, 0x14, 0xAF
        };
        for (unsigned char cmd : init_cmds) sendCommand(cmd);
    }

    // 清空显存
    void clearBuffer() {
        memset(&buffer[1], 0x00, 1024);
    }

    // 核心绘图算法：坐标转内存位操作
    void drawPixel(int x, int y) {
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
        // SSD1306按“页”排列，每页8个垂直像素
        int page = y / 8;
        int bit = y % 8;
        int index = 1 + x + (page * 128);
        buffer[index] |= (1 << bit); // 将对应的像素点亮
    }

    // 刷屏：把内存数据推到 I2C
    void display() {
        // 告诉屏幕：准备从头开始接收全屏数据
        sendCommand(0x21); sendCommand(0x00); sendCommand(0x7F);
        sendCommand(0x22); sendCommand(0x00); sendCommand(0x07);
        write(fd, buffer, 1025);
    }
};

int main() {
    // 初始化外设 (注意你的 I2C 总线是 7)
    OLEDScope oled("/dev/i2c-7", 0x3c);
    oled.init();
    
    CPUReader cpu;
    int history[128]; // 记录屏幕上 128 列的 Y 坐标
    
    // 初始化历史数据都在屏幕最底下 (Y=63)
    for (int i = 0; i < 128; i++) history[i] = 63;

    // 先丢弃第一次读取（因为需要计算差值）
    cpu.getUsage();
    usleep(100000); 

    std::cout << "🚀 CPU 示波器启动！按 Ctrl+C 退出。" << std::endl;
    std::cout << "💡 提示：再开一个终端窗口跑个大任务，看看屏幕变化！" << std::endl;

    while (true) {
        // 1. 获取 CPU 占用率 (0~100)
        double usage = cpu.getUsage();

        // 2. 映射到 OLED 屏幕的 Y 轴
        // 屏幕高 64 像素。0% 在最底下(y=63)，100% 在最顶上(y=0)
        int y = 63 - (int)(usage * 63.0 / 100.0);
        if (y < 0) y = 0;
        if (y > 63) y = 63;

        // 3. 数据左移 (滑动窗口)
        for (int i = 0; i < 127; i++) {
            history[i] = history[i + 1];
        }
        history[127] = y; // 把最新的点塞到屏幕最右侧

        // 4. 开始绘图流程
        oled.clearBuffer();
        
        // 遍历历史数组，把点画在显存里
        for (int x = 0; x < 128; x++) {
            oled.drawPixel(x, history[x]);
            // 可选：画一条垂直线让波形变成“实心”柱状图，取消下面这行注释即可
            // for(int fill_y = history[x]; fill_y <= 63; fill_y++) oled.drawPixel(x, fill_y);
        }

        // 5. 刷新屏幕
        oled.display();

        // 6. 控制刷新率 (200 毫秒刷新一次)
        usleep(200000); 
    }

    return 0;
}