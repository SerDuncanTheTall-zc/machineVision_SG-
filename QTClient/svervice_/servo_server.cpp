#include "servo_server.h"
#include "gst_streamer.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

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

ServoState g_servos[3] = {
    {0, ANGLE_90_NS, DIR_STOP, 8000},
    {1, ANGLE_90_NS, DIR_STOP, 8000},
    {2, ANGLE_90_NS, DIR_STOP, 8000}
};
int g_tracking_enabled = 0;

static int g_server_fd = -1;
static int g_client_fd = -1;
static int g_running = 1;
static pthread_t g_motion_thread;
static pthread_mutex_t g_servo_mutex = PTHREAD_MUTEX_INITIALIZER;
static GstStreamer g_streamer;
static std::string g_client_ip;

static int clamp_duty(int duty)
{
    if (duty < ANGLE_60_NS)  return ANGLE_60_NS;
    if (duty > ANGLE_120_NS) return ANGLE_120_NS;
    return duty;
}

static int servo_index_from_id(uint8_t servo_id)
{
    if (servo_id < 1 || servo_id > 3) return -1;
    return (int)servo_id - 1;
}

static int angle_to_duty_ns(int angle_001deg)
{
    if (angle_001deg < 6000)  angle_001deg = 6000;
    if (angle_001deg > 12000) angle_001deg = 12000;

    const int duty_span = ANGLE_120_NS - ANGLE_60_NS;
    const int angle_span = 12000 - 6000;
    return ANGLE_60_NS + (duty_span * (angle_001deg - 6000)) / angle_span;
}

void write_sysfs(const char* file, const char* value)
{
    FILE* fp = fopen(file, "w");
    if (!fp) {
        if (errno != EBUSY) {
            fprintf(stderr, "[ERROR] open %s failed: %s\n", file, strerror(errno));
        }
        return;
    }
    fprintf(fp, "%s", value);
    fclose(fp);
}

void pwm_init(int chip_num, int period, int initial_duty)
{
    char path[128];
    char val_str[32];

    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/export", chip_num);
    write_sysfs(path, "0");
    usleep(200000);

    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/period", chip_num);
    snprintf(val_str, sizeof(val_str), "%d", period);
    write_sysfs(path, val_str);

    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/polarity", chip_num);
    write_sysfs(path, "normal");

    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/duty_cycle", chip_num);
    snprintf(val_str, sizeof(val_str), "%d", initial_duty);
    write_sysfs(path, val_str);

    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/enable", chip_num);
    write_sysfs(path, "1");

    printf("[INFO] pwmchip%d init ok\n", chip_num);
}

void pwm_set_duty(int chip_num, int duty)
{
    char path[128];
    char val_str[32];
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/pwm0/duty_cycle", chip_num);
    snprintf(val_str, sizeof(val_str), "%d", duty);
    write_sysfs(path, val_str);
}

uint8_t crc8(const uint8_t* data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; ++i) {
        crc = s_crc8Table[crc ^ data[i]];
    }
    return crc;
}

void handle_servo_move(uint8_t servo_id, uint8_t dir, uint8_t speed)
{
    int idx = servo_index_from_id(servo_id);
    if (idx < 0) {
        fprintf(stderr, "[WARN] invalid servo_id=%u\n", servo_id);
        return;
    }

    pthread_mutex_lock(&g_servo_mutex);
    g_servos[idx].target_dir = dir;
    g_servos[idx].speed = 1000 + ((int)speed * 300);
    pthread_mutex_unlock(&g_servo_mutex);

    printf("[CMD] MOVE servo=%u dir=%u speed=%u\n", servo_id, dir, speed);
}

void handle_servo_abs(uint8_t servo_id, int16_t pan, int16_t tilt)
{
    int idx = servo_index_from_id(servo_id);
    if (idx < 0) {
        fprintf(stderr, "[WARN] invalid servo_id=%u\n", servo_id);
        return;
    }

    int angle_001deg = pan != 0 ? pan : tilt;
    int duty = clamp_duty(angle_to_duty_ns(angle_001deg));

    pthread_mutex_lock(&g_servo_mutex);
    g_servos[idx].target_dir = DIR_STOP;
    g_servos[idx].current_duty = duty;
    pwm_set_duty(g_servos[idx].chip_num, duty);
    pthread_mutex_unlock(&g_servo_mutex);

    printf("[CMD] ABS servo=%u angle=%d duty=%d\n", servo_id, angle_001deg, duty);
}

void handle_tracking_ctrl(uint8_t enable)
{
    g_tracking_enabled = enable ? 1 : 0;
    printf("[CMD] TRACKING %s\n", g_tracking_enabled ? "ON" : "OFF");
}

void handle_stream_ctrl(uint8_t enable, uint16_t udp_data, uint16_t udp_video, const std::string& client_ip)
{
    printf("[CMD] STREAM %s udp_data=%u udp_video=%u ip=%s\n",
           enable ? "ON" : "OFF", udp_data, udp_video, client_ip.c_str());

    if (enable) {
        g_streamer.start(client_ip, udp_video);
    } else {
        g_streamer.stop();
    }
    // TODO: udp_data 通道的手势/AI 数据发送逻辑
}

static void* motion_worker(void*)
{
    while (g_running) {
        pthread_mutex_lock(&g_servo_mutex);
        for (int i = 0; i < 3; ++i) {
            ServoState* servo = &g_servos[i];
            int next = servo->current_duty;

            switch (servo->target_dir) {
            case DIR_UP:
            case DIR_RIGHT:
                next += servo->speed;
                break;
            case DIR_DOWN:
            case DIR_LEFT:
                next -= servo->speed;
                break;
            case DIR_STOP:
            default:
                break;
            }

            next = clamp_duty(next);
            if (next != servo->current_duty) {
                servo->current_duty = next;
                pwm_set_duty(servo->chip_num, servo->current_duty);
            }
        }
        pthread_mutex_unlock(&g_servo_mutex);
        usleep(20000);
    }
    return NULL;
}

int parse_frame(const uint8_t* frame, int total_len)
{
    if (total_len < 6) return -1;
    if (frame[0] != FRAME_SYNC1 || frame[1] != FRAME_SYNC2) return -1;

    int payload_len = ((int)frame[2] << 8) | frame[3];
    if (payload_len < 1) return -1;
    if (total_len != 4 + payload_len + 1) return -1;

    const uint8_t* data = frame + 4;
    uint8_t got_crc = frame[4 + payload_len];
    uint8_t want_crc = crc8(data, payload_len);
    if (got_crc != want_crc) {
        fprintf(stderr, "[WARN] crc mismatch got=%02X want=%02X\n", got_crc, want_crc);
        return -1;
    }

    uint8_t cmd = data[0];
    const uint8_t* p = data + 1;
    int n = payload_len - 1;

    switch (cmd) {
    case CMD_SERVO_MOVE:
        if (n == 3) {
            handle_servo_move(p[0], p[1], p[2]);
            return 0;
        }
        break;
    case CMD_SERVO_ABS:
        if (n == 5) {
            int16_t pan  = (int16_t)((p[1] << 8) | p[2]);
            int16_t tilt = (int16_t)((p[3] << 8) | p[4]);
            handle_servo_abs(p[0], pan, tilt);
            return 0;
        }
        break;
    case CMD_TRACKING_CTRL:
        if (n == 1) {
            handle_tracking_ctrl(p[0]);
            return 0;
        }
        break;
    case CMD_STREAM_CTRL:
        if (n == 5) {
            uint16_t udp_data = (uint16_t)((p[1] << 8) | p[2]);
            uint16_t udp_video = (uint16_t)((p[3] << 8) | p[4]);
            handle_stream_ctrl(p[0], udp_data, udp_video, g_client_ip);
            return 0;
        }
        break;
    case CMD_PING:
        printf("[CMD] PING\n");
        return 0;
    default:
        fprintf(stderr, "[WARN] unknown cmd=%u\n", cmd);
        break;
    }

    return -1;
}

static void process_rx_buffer(uint8_t* buf, int* len)
{
    int pos = 0;
    while (*len - pos >= 6) {
        if (buf[pos] != FRAME_SYNC1 || buf[pos + 1] != FRAME_SYNC2) {
            ++pos;
            continue;
        }

        int payload_len = ((int)buf[pos + 2] << 8) | buf[pos + 3];
        int frame_len = 4 + payload_len + 1;
        if (*len - pos < frame_len) break;

        parse_frame(buf + pos, frame_len);
        pos += frame_len;
    }

    if (pos > 0) {
        memmove(buf, buf + pos, *len - pos);
        *len -= pos;
    }
}

static void signal_handler(int)
{
    g_running = 0;
}

int servo_server_init(int port)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    for (int i = 0; i < 3; ++i) {
        pwm_init(i, PERIOD_NS, ANGLE_90_NS);
    }

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_server_fd);
        g_server_fd = -1;
        return -1;
    }

    if (listen(g_server_fd, 1) < 0) {
        perror("listen");
        close(g_server_fd);
        g_server_fd = -1;
        return -1;
    }

    if (pthread_create(&g_motion_thread, NULL, motion_worker, NULL) != 0) {
        perror("pthread_create");
        close(g_server_fd);
        g_server_fd = -1;
        return -1;
    }

    printf("[INFO] servo server listen on %d\n", port);
    return 0;
}

void servo_server_run()
{
    uint8_t rxbuf[2048];
    int rxlen = 0;

    while (g_running) {
        if (g_client_fd < 0) {
            printf("[INFO] waiting client...\n");
            struct sockaddr_in cli_addr;
            socklen_t addr_len = sizeof(cli_addr);
            g_client_fd = accept(g_server_fd, (struct sockaddr*)&cli_addr, &addr_len);
            if (g_client_fd < 0) {
                if (errno == EINTR) continue;
                perror("accept");
                break;
            }
            g_client_ip = std::string(inet_ntoa(cli_addr.sin_addr));
            printf("[INFO] client connected from %s\n", g_client_ip.c_str());
            rxlen = 0;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_client_fd, &rfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ret = select(g_client_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            close(g_client_fd);
            g_client_fd = -1;
            g_streamer.stop();
            continue;
        }

        if (ret == 0) continue;

        if (FD_ISSET(g_client_fd, &rfds)) {
            int n = recv(g_client_fd, rxbuf + rxlen, sizeof(rxbuf) - rxlen, 0);
            if (n <= 0) {
                printf("[INFO] client disconnected\n");
                g_streamer.stop();
                close(g_client_fd);
                g_client_fd = -1;
                pthread_mutex_lock(&g_servo_mutex);
                for (int i = 0; i < 3; ++i) g_servos[i].target_dir = DIR_STOP;
                pthread_mutex_unlock(&g_servo_mutex);
                continue;
            }

            rxlen += n;
            process_rx_buffer(rxbuf, &rxlen);
        }
    }
}

void servo_server_stop()
{
    g_running = 0;
    g_streamer.stop();

    if (g_client_fd >= 0) {
        close(g_client_fd);
        g_client_fd = -1;
    }
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }

    pthread_join(g_motion_thread, NULL);

    pthread_mutex_lock(&g_servo_mutex);
    for (int i = 0; i < 3; ++i) {
        g_servos[i].target_dir = DIR_STOP;
        g_servos[i].current_duty = ANGLE_90_NS;
        pwm_set_duty(g_servos[i].chip_num, ANGLE_90_NS);
    }
    pthread_mutex_unlock(&g_servo_mutex);

    printf("[INFO] servo server stopped\n");
}

int main()
{
    printf("🚀 Gimbal servo control server start...\n");
    printf("🔔 Ctrl+C to stop\n");

    if (servo_server_init(9000) != 0) {
        return 1;
    }

    servo_server_run();
    servo_server_stop();
    return 0;
}
