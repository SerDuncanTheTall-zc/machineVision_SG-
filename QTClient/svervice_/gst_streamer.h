#ifndef GST_STREAMER_H
#define GST_STREAMER_H

#include <string>
#include <cstdint>
#include <gst/gst.h>

/**
 * @brief 板端视频推流器 (原生 GStreamer API)
 *
 * 在后台线程构建并运行 V4L2 → MPP 硬解 JPEG → MPP 硬编 H.264 → RTP/UDP 管线。
 * 适配 RK3576 的硬件编解码器。
 */
class GstStreamer {
public:
    struct Config {
        std::string device = "/dev/video73";
        int         width  = 1920;
        int         height = 1080;
        int         fps    = 30;
    };

    GstStreamer();
    ~GstStreamer();

    /// 设置视频源参数 (必须在 start 前调用)
    void setConfig(const Config& cfg);
    const Config& config() const { return m_config; }

    /// 启动推流 (非阻塞，在后台线程运行 GLib 主循环)
    bool start(const std::string& target_ip, uint16_t port);
    /// 停止推流
    void stop();
    /// 是否正在推流
    bool isRunning() const;

private:
    /// 清理残留进程并释放视频设备
    void resetHardware();
    /// 构建 GStreamer 管线
    bool buildPipeline(const std::string& target_ip, uint16_t port);
    /// 销毁管线
    void destroyPipeline();
    /// 后台线程入口 (运行 GLib 主循环)
    static gpointer threadFunc(gpointer data);

    /// GLib 主循环的 bus 回调
    static gboolean onBusMessage(GstBus* bus, GstMessage* msg, gpointer data);

    Config      m_config;
    GstElement* m_pipeline;
    GMainLoop*  m_loop;
    GThread*    m_thread;
    guint       m_busWatchId;
    bool        m_running;
};

#endif // GST_STREAMER_H
