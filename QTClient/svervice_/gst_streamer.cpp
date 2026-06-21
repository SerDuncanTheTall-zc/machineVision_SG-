#include "gst_streamer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>

GstStreamer::GstStreamer()
    : m_pipeline(nullptr)
    , m_loop(nullptr)
    , m_thread(nullptr)
    , m_busWatchId(0)
    , m_running(false)
{
    gst_init(nullptr, nullptr);
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
    return m_running && m_loop && g_main_loop_is_running(m_loop);
}

void GstStreamer::resetHardware()
{
    // 释放摄像头设备
    std::string cmd = "fuser -k " + m_config.device + " 2>/dev/null";
    system(cmd.c_str());

    // 清理内核缓存 (需要 root)
    sync();
    FILE* fp = fopen("/proc/sys/vm/drop_caches", "w");
    if (fp) {
        fprintf(fp, "3");
        fclose(fp);
    }
    usleep(500000);
}

bool GstStreamer::buildPipeline(const std::string& target_ip, uint16_t port)
{
    // === element 工厂 ===
    auto mk = [](const char* factory, const char* name) -> GstElement* {
        GstElement* e = gst_element_factory_make(factory, name);
        if (!e) {
            fprintf(stderr, "[STREAM ERROR] failed to create %s (%s)\n", name, factory);
        }
        return e;
    };

    // --- src ---
    GstElement* src = mk("v4l2src", "src");
    if (!src) return false;
    g_object_set(src,
        "device",  m_config.device.c_str(),
        "io-mode", 2,    // MMAP
        nullptr);

    // --- caps filter (JPEG input) ---
    GstElement* capsfilter = mk("capsfilter", "caps_in");
    if (!capsfilter) return false;
    GstCaps* caps_in = gst_caps_new_simple("image/jpeg",
        "width",    G_TYPE_INT, m_config.width,
        "height",   G_TYPE_INT, m_config.height,
        "framerate",GST_TYPE_FRACTION, m_config.fps, 1,
        nullptr);
    g_object_set(capsfilter, "caps", caps_in, nullptr);
    gst_caps_unref(caps_in);

    // --- JPEG parse ---
    GstElement* jpegparse = mk("jpegparse", "jpegparse");

    // --- MPP JPEG decoder ---
    GstElement* dec = mk("mppjpegdec", "dec");

    // --- queue ---
    GstElement* queue = mk("queue", "queue");

    // --- MPP H.264 encoder ---
    GstElement* enc = mk("mpph264enc", "enc");

    // --- RTP H.264 payloader ---
    GstElement* pay = mk("rtph264pay", "pay");
    if (pay) {
        g_object_set(pay, "config-interval", 1, nullptr);
    }

    // --- UDP sink ---
    GstElement* sink = mk("udpsink", "sink");
    if (!sink) return false;
    g_object_set(sink,
        "host", target_ip.c_str(),
        "port", (int)port,
        nullptr);

    // --- 组装管线 ---
    m_pipeline = gst_pipeline_new("stream-pipeline");
    if (!m_pipeline) {
        fprintf(stderr, "[STREAM ERROR] failed to create pipeline\n");
        return false;
    }

    gst_bin_add_many(GST_BIN(m_pipeline),
        src, capsfilter,
        jpegparse, dec, queue, enc, pay, sink,
        nullptr);

    // v4l2src ! capsfilter ! jpegparse ! mppjpegdec ! queue ! mpph264enc ! rtph264pay ! udpsink
    if (!gst_element_link_many(src, capsfilter,
                               jpegparse, dec, queue, enc, pay, sink,
                               nullptr)) {
        fprintf(stderr, "[STREAM ERROR] failed to link elements\n");
        destroyPipeline();
        return false;
    }

    // --- bus 监听 ---
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    m_busWatchId = gst_bus_add_watch(bus, onBusMessage, this);
    gst_object_unref(bus);

    printf("[STREAM] pipeline built -> %s:%u\n", target_ip.c_str(), port);
    return true;
}

void GstStreamer::destroyPipeline()
{
    if (m_busWatchId) {
        g_source_remove(m_busWatchId);
        m_busWatchId = 0;
    }
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}

gpointer GstStreamer::threadFunc(gpointer data)
{
    GstStreamer* self = static_cast<GstStreamer*>(data);

    // 启动管线
    GstStateChangeReturn ret = gst_element_set_state(
        self->m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        fprintf(stderr, "[STREAM ERROR] failed to set PLAYING\n");
        self->m_running = false;
        return nullptr;
    }

    printf("[STREAM] pipeline playing\n");

    // 运行 GLib 主循环 (阻塞，直到 g_main_loop_quit)
    self->m_loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(self->m_loop);

    // 清理
    self->destroyPipeline();
    if (self->m_loop) {
        g_main_loop_unref(self->m_loop);
        self->m_loop = nullptr;
    }

    printf("[STREAM] thread exit\n");
    self->m_running = false;
    return nullptr;
}

gboolean GstStreamer::onBusMessage(GstBus*, GstMessage* msg, gpointer data)
{
    GstStreamer* self = static_cast<GstStreamer*>(data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(msg, &err, &debug);
        fprintf(stderr, "[STREAM ERROR] %s\n", err->message);
        g_error_free(err);
        g_free(debug);
        // 出错后退出主循环
        if (self->m_loop) {
            g_main_loop_quit(self->m_loop);
        }
        break;
    }
    case GST_MESSAGE_EOS:
        printf("[STREAM] EOS received\n");
        if (self->m_loop) {
            g_main_loop_quit(self->m_loop);
        }
        break;
    default:
        break;
    }

    return TRUE;
}

bool GstStreamer::start(const std::string& target_ip, uint16_t port)
{
    stop();

    resetHardware();

    if (!buildPipeline(target_ip, port)) {
        return false;
    }

    m_running = true;
    m_thread = g_thread_new("gst-streamer", threadFunc, this);
    if (!m_thread) {
        fprintf(stderr, "[STREAM ERROR] failed to create thread\n");
        destroyPipeline();
        m_running = false;
        return false;
    }

    return true;
}

void GstStreamer::stop()
{
    if (!m_running) return;

    // 退出主循环
    if (m_loop && g_main_loop_is_running(m_loop)) {
        g_main_loop_quit(m_loop);
    }

    // 等待线程退出
    if (m_thread) {
        g_thread_join(m_thread);
        m_thread = nullptr;
    }

    // 确保管线销毁
    destroyPipeline();

    m_running = false;
    printf("[STREAM] stopped\n");
}
