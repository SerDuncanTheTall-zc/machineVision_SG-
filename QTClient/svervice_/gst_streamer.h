#ifndef GST_STREAMER_H
#define GST_STREAMER_H

#include <string>
#include <cstdint>

/**
 * @brief 板端视频推流器
 *
 * 管理 gst-launch-1.0 子进程，实现硬件加速 H.264 编码 + RTP/UDP 推流。
 * 适配 RK3576 的 V4L2 + MPP 硬件编解码管线。
 */
class GstStreamer {
public:
    struct Config {
        std::string device   = "/dev/video73";
        int         width    = 1920;
        int         height   = 1080;
        int         fps      = 30;
    };

    GstStreamer();
    ~GstStreamer();

    /// 设置视频源参数
    void setConfig(const Config& cfg);
    const Config& config() const { return m_config; }

    /// 启动推流 (非阻塞，内部 fork+exec)
    bool start(const std::string& target_ip, uint16_t port);
    /// 停止推流
    void stop();
    /// 是否正在推流
    bool isRunning() const;

private:
    /// 清理残留 gst 进程并释放视频设备
    void resetHardware();
    /// 实际构建并执行 gst-launch-1.0
    bool launchPipeline(const std::string& target_ip, uint16_t port);

    Config m_config;
    int    m_pid;     // 子进程 PID，-1 表示未运行
};

#endif // GST_STREAMER_H
