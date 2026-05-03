import subprocess
import os
import sys
import time

# --- 配置参数 ---
# 请确保 IP 是你 PC 的当前地址
TARGET_IP = "192.168.2.103" 
PORT = 5000
DEVICE = "/dev/video73"
WIDTH, HEIGHT = 1920, 1080
FPS = 30

def run_command(cmd, shell=True):
    try:
        subprocess.run(cmd, shell=shell, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError:
        pass

def reset_hardware():
    print("--- [1/3] 正在清理残留进程与硬件锁... ---")
    # 强杀残留的 GStreamer
    run_command("pkill -9 gst-launch-1.0")
    # 释放摄像头节点占用
    run_command(f"fuser -k {DEVICE}")
    
    print("--- [2/3] 正在释放内核连续内存 (CMA)... ---")
    # 解决 "Failed to allocate a buffer" 的核心步骤
    run_command("sync")
    # 需要 root 权限执行
    if os.geteuid() == 0:
        with open("/proc/sys/vm/drop_caches", "w") as f:
            f.write("3")
    else:
        print("警告: 非 root 用户，无法清理内核缓存，可能导致 Buffer 申请失败。")
    
    time.sleep(1)

def start_streaming():
    print(f"--- [3/3] 启动硬件加速推流: {WIDTH}x{HEIGHT}@{FPS}fps ---")
    print(f"目标地址: {TARGET_IP}:{PORT}")

    # 构造 GStreamer 指令
    # 使用 io-mode=2 (MMAP) 提高兼容性
    # config-interval=1 确保客户端随时接入都有画面
    gst_cmd = (
        f"gst-launch-1.0 v4l2src device={DEVICE} io-mode=2 ! "
        f"image/jpeg,width={WIDTH},height={HEIGHT},framerate={FPS}/1 ! "
        f"jpegparse ! mppjpegdec ! queue ! mpph264enc ! "
        f"rtph264pay config-interval=1 ! "
        f"udpsink host={TARGET_IP} port={PORT}"
    )

    try:
        # 使用 Popen 启动，方便后续扩展（比如读取输出或异常重启）
        process = subprocess.Popen(gst_cmd, shell=True)
        print(f"\n推流中... PID: {process.pid}")
        process.wait()
    except KeyboardInterrupt:
        print("\n用户停止推流。")
        process.terminate()
    except Exception as e:
        print(f"推流异常: {e}")

if __name__ == "__main__":
    # 检查设备是否存在
    if not os.path.exists(DEVICE):
        print(f"错误: 找不到设备 {DEVICE}，请检查摄像头连接或执行 ls /dev/video*")
        sys.exit(1)

    reset_hardware()
    start_streaming()