#include "gst_streamer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

GstStreamer::GstStreamer()
    : m_pid(-1)
{
}

GstStreamer::~GstStreamer()
{
    stop();
}

void GstStreamer::setConfig(const Config& cfg)
{
    m_config = cfg;
}

bool GstStreamer::isRunning() const
{
    if (m_pid < 0) return false;
    int status;
    pid_t r = waitpid(m_pid, &status, WNOHANG);
    if (r == 0) return true;
    return false;
}

void GstStreamer::resetHardware()
{
    // 清理残留 gst 进程
    system("pkill -9 gst-launch-1.0 2>/dev/null");
    // 释放摄像头设备
    std::string cmd = "fuser -k " + m_config.device + " 2>/dev/null";
    system(cmd.c_str());
    // 尝试清理内核缓存 (需要 root)
    sync();
    FILE* fp = fopen("/proc/sys/vm/drop_caches", "w");
    if (fp) {
        fprintf(fp, "3");
        fclose(fp);
    }
    usleep(500000);
}

bool GstStreamer::launchPipeline(const std::string& target_ip, uint16_t port)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return false;
    }

    if (pid == 0) {
        // --- 子进程 ---
        // 脱离父进程会话，避免 Ctrl+C 影响子进程
        setsid();

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%u", port);

        printf("[STREAM] starting pipeline -> %s:%u\n", target_ip.c_str(), port);
        fflush(stdout);

        // 等效 gst-launch-1.0 命令行
        execlp("gst-launch-1.0", "gst-launch-1.0",
               "v4l2src",
                   "device="   + (char*)m_config.device.c_str(),
                   "io-mode=2",
               "!",
               "image/jpeg",
                   "width="    + std::to_string(m_config.width),
                   "height="   + std::to_string(m_config.height),
                   "framerate=" + std::to_string(m_config.fps) + "/1",
               "!",
               "jpegparse",
               "!",
               "mppjpegdec",
               "!",
               "queue",
               "!",
               "mpph264enc",
               "!",
               "rtph264pay",
                   "config-interval=1",
               "!",
               "udpsink",
                   "host="     + target_ip,
                   "port="     + std::string(port_str),
               (char*)nullptr);

        // execlp 失败才走到这里
        perror("execlp gst-launch-1.0");
        _exit(1);
    }

    // --- 父进程 ---
    m_pid = pid;
    printf("[STREAM] gst-launch pid=%d\n", m_pid);
    return true;
}

bool GstStreamer::start(const std::string& target_ip, uint16_t port)
{
    stop();
    resetHardware();
    return launchPipeline(target_ip, port);
}

void GstStreamer::stop()
{
    if (m_pid < 0) return;

    // SIGTERM 通知子进程组
    kill(-m_pid, SIGTERM);
    usleep(200000);

    // 如果还没退出，强杀
    int status;
    pid_t r = waitpid(m_pid, &status, WNOHANG);
    if (r == 0) {
        kill(-m_pid, SIGKILL);
        waitpid(m_pid, &status, 0);
    }

    printf("[STREAM] stopped pid=%d\n", m_pid);
    m_pid = -1;

    // 清理残留
    system("pkill -9 gst-launch-1.0 2>/dev/null");
}
