---
name: k230-development
description: K230 嵌入式 AI 开发专家知识库，涵盖 SDK 架构、编译系统、MPP API、AI 推理、跨核通信、音频、WiFi、调试指令等完整开发经验。适用于所有 K230/CanMV 01Studio 开发任务。
---

## [K230-LINUX-CRITICAL-SKILL]

以下约束为本 Skills 的最高优先级约束，用于规范后续 AI 生成代码。

### 1. Skills 定义

- 用途：约束 K230 官方 SDK 场景下的 Linux 用户态工程开发，确保代码基于真实 API、可编译、可运行、可集成。
- 支持范围：AI 推理、摄像头采集、显示输出、音频采集播放、GPIO/外设控制、网络与系统服务集成。
- 适用对象：K230 Linux 用户态 C 语言工程（非 K210、非裸机、非伪代码 Demo）。

### 2. 强制规则（CRITICAL）

- 禁止使用 K210 API（如 `kpu_*`、`sensor_*` 等 K210 生态接口）。
- 禁止编造 SDK 函数、结构体、头文件、设备节点、ioctl 命令。
- 必须基于 Linux 标准接口与官方驱动链路：`V4L2` / `ALSA` / `DRM` 或 `framebuffer` / `media controller`。
- 所有硬件访问必须通过驱动接口完成，禁止直接写寄存器替代驱动。
- AI 推理必须使用 `nncase / KPU runtime` 真实接口与模型加载流程。
- 提交的代码必须是可编译工程代码，禁止只给伪代码。

### 3. 开发环境约束

- 操作系统：K230 Linux / Buildroot 用户态环境。
- 编译方式：本地 `gcc` 或交叉编译 `riscv64-unknown-linux-gnu-gcc`。
- 依赖库：`pthread`、`m`、`rt`、`dl`，以及按模块启用的 `alsa-lib`、`v4l2` 相关头库、`nncase runtime`。
- 构建结果必须与 K230 SDK 工程目录和发布方式一致（可集成到现有 SDK/Buildroot）。

### 4. 项目结构（必须工程化）

- 必须使用工程目录组织，至少包含：`src/`、`include/`、`Makefile`（或 `CMakeLists.txt`）。
- 功能按模块拆分：摄像头、显示、AI、音频、设备管理、错误与日志。
- 禁止把所有逻辑堆在单文件 `main.c`，必须保留清晰模块边界与头文件声明。

### 5. 核心模块模板（只描述，不写完整代码）

- 摄像头采集流程（V4L2）：打开设备 → 查询能力 → 设置格式/帧率 → 申请缓冲区（MMAP/USERPTR）→ 入队 → `VIDIOC_STREAMON` → `DQBUF/QBUF` 循环取帧。
- 显示输出流程：优先 DRM/KMS，其次 framebuffer；完成显示设备初始化、缓冲管理、格式匹配、刷新/翻页。
- AI 推理流程：加载 nncase/kmodel → 准备输入 tensor → 预处理（尺寸/色彩/归一化）→ 推理执行 → 后处理输出。
- 音频采集播放流程（ALSA）：打开 PCM 设备 → 设置 HW/SW 参数（采样率/位宽/通道）→ 循环读写帧 → 欠载/过载恢复。

### 6. API 使用规范（非常重要）

- 摄像头必须使用 V4L2 `ioctl` 链路（`VIDIOC_*`），禁止虚构采集 API。
- 音频必须使用 ALSA PCM 接口，禁止使用与当前平台不匹配的音频框架。
- 显示必须使用 DRM/KMS 或 framebuffer，按实际驱动节点实现。
- 多媒体链路涉及子设备时，必须遵循 media controller 拓扑配置。
- AI 模型必须通过 nncase runtime 加载与执行，模型格式、输入输出维度必须与模型文件一致。

### 7. 代码风格

- 使用 C 语言与 POSIX 风格接口。
- 模块化组织：接口声明在 `include/`，实现放在 `src/`。
- 错误处理必须完整：每一步都检查返回值并给出错误路径回收。
- 资源管理必须成对：`open/close`、`mmap/munmap`、`malloc/free`、`init/deinit`。

### 8. AI 使用说明（关键）

- 后续 AI 生成代码时必须复用本 Skills 定义的工程结构与接口约束。
- 不允许输出“演示型 Demo 风格”代码，必须按可落地工程组织。
- 不允许简化为伪代码、占位函数、假设 API。
- 若接口不确定，必须先在 K230 SDK 现有头文件/示例中检索再实现。

## 官方参考资料索引

以下官方文档存放于 `.augment/skills/k230-development/references/`。
**当用户询问具体 API、模块配置或驱动细节时，必须优先通过 codebase-retrieval 在对应文件中检索准确信息，不得凭记忆猜测。**

| 文件名 | 内容 |
|---|---|
| `K230_VICAP_API.html` | VICAP（视频输入采集）完整 API 参考，含 kd_mpi_vicap_* 所有函数签名和参数说明 |
| `K230_VICAP_SENSOR参数分区.html` | Sensor 参数分区配置，ISP 数据库解析模式 |
| `K230多媒体中间件API.html` | VB/VO/AI/AO/VENC/VDEC 全套 MPP 中间件 API |
| `K230系统控制MAPI.html` | 系统初始化、MMZ 内存管理、sys mmap/mmz API |
| `k230核间通讯api.html` | IPCM 跨核通信完整 API，kd_ipcmsg_* 函数说明 |
| `K230_DMA_API.html` | DMA 旋转/镜像/搬运 API |
| `K230_DPU_API.html` | DPU 结构光深度 API |
| `K230_FFT_API.html` | 硬件 FFT API（64~4096 点） |
| `K230_GPU_API.html` | VGLite GPU API |
| `K230_SHA256_API.html` | SHA256 硬件加速 API |
| `K230_ISP初始化配置指南.html` | ISP 初始化流程与配置参数 |
| `K230_ISP图像调优指南.html` | ISP 图像质量调节方法 |
| `K230_ISP_Tuning_Tool使用指南.html` | ISP Tuning Tool 工具使用 |
| `K230_GUI实战_LVGL移植教程.html` | LVGL 移植与 VGLite GPU 加速 GUI |
| `K230_LCD适配指南.html` | LCD 屏适配与 Connector 配置 |
| `K230_nncase开发指南.html` | nncase 模型转换与 KPU 推理开发 |
| `K230_SDK更新nncase运行时库指南.html` | nncase runtime 版本更新方法 |
| `K230_SDK_Dewarp使用指南.html` | 镜头畸变矫正（Dewarp）配置 |
| `K230_SDK_Burntool使用指南.html` | 烧录工具使用指南 |
| `K230_SDK_IoT_WiFi_AiW4211LV10使用指南.html` | WiFi 模块使用指南 |
| `K230_SDK_IoT_WiFi_AiW4211LV10驱动开发指南.html` | WiFi 驱动开发 |
| `K230_PMU使用指南.html` | 电源管理 PMU 使用 |
| `K230_debian_ubuntu说明.html` | K230 运行 Debian/Ubuntu 说明 |
| `K230_lpddr3_lpddr4驱动适配指南.html` | LPDDR 驱动适配 |
| `K230_BigCore_Samples.md` | 大核 MPP Sample 示例汇总 |

**检索规则：**
- 涉及 VICAP/VO/AI/AO/VENC/VDEC API → 查 `K230多媒体中间件API.html`
- 涉及 kd_mpi_vicap_* 具体参数 → 查 `K230_VICAP_API.html`
- 涉及 IPCM/kd_ipcmsg_* → 查 `k230核间通讯api.html`
- 涉及内存/sys/MMZ → 查 `K230系统控制MAPI.html`
- 涉及 KPU/kmodel 推理 → 查 `K230_nncase开发指南.html`
- 涉及 LVGL/GUI → 查 `K230_GUI实战_LVGL移植教程.html`


# K230 开发 Skills Package

你是一位 K230 嵌入式 AI 开发专家，熟悉 K230 SDK 全套架构与实战开发经验。
开发者使用 **WSL2 Ubuntu + Windows** 环境，目标板为 **01Studio K230 CANMV 开发板**（1GB DDR，HDMI输出，GC2093摄像头）。
当前主要项目：**AI Camera**（大核YOLOv8姿态检测 + 小核ESP-AI语音对话一体机）。

开发原则：
- 严格遵循 K230 SDK 原有架构和 Makefile 设计思路，不绕过现有构建系统
- 大核代码（RT-Smart）用 CMake 编译，小核代码（Linux）用 Makefile 编译
- 优先参考 `src/reference/ai_poc/` 中的官方 Demo 模式
- 内存、IPCM、VB 配置修改需全量重编

## [SKILL-1] SoC 芯片架构

K230 是嘉楠科技双核异构 RISC-V AISoC：

| 核心 | ISA | OS | 职责 |
|---|---|---|---|
| 大核 Core1 | RV64GCVB | RT-Smart RTOS | MPP多媒体管线、KPU推理、音频采集/播放 |
| 小核 Core0 | RV64GC | Linux 5.10 | 网络、USB、WebSocket、应用逻辑 |

**硬件加速器：** KPU（nncase v2.x）、ai2d（图像预处理）、DPU（结构光深度）、DMA、FFT、VGLite GPU
**视频输入：** 最多3路 VICAP + ISP，MIPI CSI/DVP
**视频输出：** VO + Connector（HDMI LT9611、MIPI DSI LCD）
**音频：** PDM麦克风（dev=1）、I2S双向（dev=0）、内置Codec
**存储启动：** SD卡、eMMC、SPI NOR/NAND Flash

## [SKILL-2] SDK 目录结构

```
k230_sdk/
├── Makefile              ← 顶层编译入口（核心）
├── parse.mak             ← 解析 .config，导出工具链/路径变量
├── repo.mak              ← 子仓库版本与克隆路径
├── Kconfig / Kconfig.board / Kconfig.memory / Kconfig.storage / Kconfig.wifi
├── configs/              ← 各板型 defconfig
│   └── k230_canmv_01studio_defconfig  ← 当前使用板型
├── src/
│   ├── big/
│   │   ├── rt-smart/          ← 大核 RT-Smart 内核源码
│   │   ├── mpp/               ← 多媒体处理平台
│   │   │   ├── include/comm/  ← k_vicap_comm.h / k_ai_comm.h / k_vo_comm.h ...
│   │   │   ├── userapps/api/  ← mpi_xxx_api.h（用户态API头文件）
│   │   │   ├── userapps/lib/  ← 预编译 .a 库
│   │   │   └── userapps/sample/ ← 官方sample + 自定义App（ai_camera在此）
│   │   └── nncase/            ← nncase runtime（make prepare_sourcecode下载）
│   ├── little/
│   │   ├── linux/             ← Linux 内核
│   │   ├── uboot/             ← U-Boot
│   │   └── buildroot-ext/     ← Buildroot扩展（小核App在 app/ 子目录）
│   ├── common/
│   │   ├── cdk/               ← IPCM跨核通信框架（内核驱动+用户态库）
│   │   └── opensbi/           ← OpenSBI固件
│   └── reference/
│       ├── ai_poc/            ← 50+ 官方AI Demo（AIBase基类模式）
│       ├── business_poc/      ← 商业应用参考（doorlock等）
│       └── fancy_poc/         ← 趣味Demo
├── board/common/gen_image_script/ ← 镜像打包脚本
└── output/<CONF>/images/      ← 编译产物（不入库）
```

**AI Camera 项目文件位置：**

| 文件 | 路径 |
|---|---|
| 大核主程序 | `src/big/mpp/userapps/sample/ai_camera/main.cc` |
| VI/VO 初始化 | `src/big/mpp/userapps/sample/ai_camera/vi_vo_setup.h` |
| 音频硬件抽象 | `src/big/mpp/userapps/sample/ai_camera/audio_hw.h` |
| IPCM 协议定义 | `src/big/mpp/userapps/sample/ai_camera/ipcm_comm.h` |
| 大核 CMakeLists | `src/big/mpp/userapps/sample/ai_camera/CMakeLists.txt` |
| 一键编译脚本 | `src/big/mpp/userapps/sample/ai_camera/build.sh` |
| 小核主程序 | `src/little/buildroot-ext/app/ai_camera_linux/main.c` |
| ESP-AI 客户端 | `src/little/buildroot-ext/app/ai_camera_linux/esp_ai_client.h/c` |
| 小核音频管理 | `src/little/buildroot-ext/app/ai_camera_linux/audio_manager.h/c` |

## [SKILL-3] 编译系统

### Kconfig 配置

```bash
make CONF=k230_canmv_01studio_defconfig   # 选配置（必须最先执行）
make menuconfig                            # 图形化配置
make savedefconfig                         # 保存当前配置
make show_current_config                   # 查看关键路径变量
```

生成 `k_autoconf_comm.h`（大核）和 `sdk_autoconf.h`（小核），包含板型宏和内存地址。

### Makefile 编译目标（基于实际 Makefile 逻辑）

```bash
# 首次准备
make prepare_sourcecode    # 下载 nncase/kmodel/buildroot dl缓存（依赖网络）
make prepare_toolchain     # 下载解压工具链到 toolchain/

# 全量编译
make                       # 全量：linux + mpp + cdk + rt-smart-apps + opensbi + buildroot + uboot + build-image

# 大核相关
make mpp-apps              # 大核 MPP sample（含 ai_camera）→ 编译 userapps/src + userapps/sample
make rt-smart-apps         # 大核用户态 scons 编译 + mkromfs.py 打包 romfs.c
make rt-smart-kernel       # 大核 RT-Smart 内核（scons）
make rt-smart              # = mpp + rt-smart-apps + big-core-opensbi
make big-core-opensbi      # OpenSBI 包装大核镜像

# 小核相关
make linux                 # 小核 Linux 内核（defconfig + build）
make linux-rebuild         # 仅重建（不重跑 defconfig）
make buildroot             # 小核 Buildroot 根文件系统（全量）
make buildroot-rebuild     # 小核 Buildroot 增量重编（推荐日常使用）
make uboot                 # U-Boot（defconfig + build）
make uboot-rebuild         # 仅重建 U-Boot

# 打包
make build-image           # 仅打包烧录镜像（调用 CONFIG_GEN_IMG_SCRIPT）
make clean                 # 删除 output/ + defconfig + prepare_memory
```

### 两套工具链

| 工具链前缀 | 用途 | 路径 |
|---|---|---|
| `riscv64-unknown-linux-musl-` | 大核 RT-Smart (musl libc) | `toolchain/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin` |
| `riscv64-unknown-linux-gnu-` | 小核 Linux (glibc V2.6.0) | `toolchain/Xuantie-900-gcc-linux-5.10.4-glibc-x86_64-V2.6.0/bin` |

### 大核 App CMakeLists.txt 模板

```cmake
cmake_minimum_required(VERSION 3.2)
include(../../../../nncase/examples/cmake/Riscv64.cmake)
project(my_app C CXX)

set(nncase_sdk_root "${PROJECT_SOURCE_DIR}/../../../../nncase")
set(k230_sdk        "${nncase_sdk_root}/../../../")
set(CMAKE_EXE_LINKER_FLAGS "-T ${nncase_sdk_root}/examples/cmake/link.lds --static")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -O2 -fopenmp -march=rv64imafdcv -mabi=lp64d -mcmodel=medany")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fopenmp -march=rv64imafdcv -mabi=lp64d -mcmodel=medany")

include_directories(
    ${k230_sdk}/src/big/mpp/userapps/api/
    ${k230_sdk}/src/big/mpp/include
    ${k230_sdk}/src/big/mpp/include/comm
    ${nncase_sdk_root}/riscv64
    ${nncase_sdk_root}/riscv64/nncase/include
)
link_directories(
    ${k230_sdk}/src/big/mpp/userapps/lib
    ${nncase_sdk_root}/riscv64/rvvlib/
    ${nncase_sdk_root}/riscv64/nncase/lib/
)

add_executable(my_app.elf main.cc)
target_link_libraries(my_app.elf -Wl,--start-group
    rvv Nncase.Runtime.Native nncase.rt_modules.k230
    functional_k230 sys vicap vb cam_device cam_engine
    hal oslayer ebase fpga isp_drv binder auto_ctrol common
    cam_caldb isi 3a buffer_management cameric_drv video_in
    virtual_hal start_engine cmd_buffer switch cameric_reg_drv
    t_database_c t_mxml_c t_json_c t_common_c
    vo connector sensor atomic dma
    ai ao ipcmsg
    -Wl,--end-group)
install(TARGETS my_app.elf DESTINATION my_app)
```

### 小核 App Makefile 模板

```makefile
CROSS_COMPILE ?= riscv64-unknown-linux-gnu-
CC = $(CROSS_COMPILE)gcc
STAGING_DIR ?= $(K230_SDK_ROOT)/output/k230_canmv_01studio_defconfig/little/buildroot-ext/staging

CFLAGS  = -O2 -Wall -I$(STAGING_DIR)/usr/include
LDFLAGS = -lpthread -L$(STAGING_DIR)/usr/lib

my_app: main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
```

## [SKILL-4] 内存布局（01Studio 1GB）

```
物理地址        大小      用途
0x00000000     1MB       保留
0x00100000     1MB       IPCM 共享内存（大小核通信，kd_ipcmsg 独占）
0x00200000     126MB     RT-Smart 系统内存（大核）
0x08000000     128MB     Linux 系统内存（小核）
0x10000000     ~768MB    MMZ（Media Memory Zone，VB帧缓冲/AI推理/编解码）
```

**规则：** 修改任意内存布局必须同步修改 defconfig 中的 `CONFIG_MEM_*` 并**全量重编**。

## [SKILL-5] 大核 MPP 开发

### 标准初始化流程

```cpp
// 1. VB 内存池（blk_size 必须 4K 对齐）
k_vb_config vb_config = {};
vb_config.max_pool_cnt = 64;
vb_config.comm_pool[0].blk_cnt  = 5;
vb_config.comm_pool[0].blk_size = VICAP_ALIGN_UP(1920*1080*3/2, VICAP_ALIGN_1K);
vb_config.comm_pool[0].mode     = VB_REMAP_MODE_NOCACHE;
kd_mpi_vb_set_config(&vb_config);
kd_mpi_vb_init();

// 2. Connector（HDMI LT9611）
k_connector_info ci;
kd_mpi_get_connector_info(LT9611_MIPI_4LAN_1920X1080_30FPS, &ci);
int fd = kd_mpi_connector_open(ci.connector_name);
kd_mpi_connector_power_set(fd, K_TRUE);
kd_mpi_connector_init(fd, ci);

// 3. VICAP（CH0 YUV→VO，CH1 RGB→AI）
kd_mpi_vicap_get_sensor_info(GC2093_MIPI_CSI2_1920X1080_60FPS_10BIT_LINEAR, &sensor_info);
kd_mpi_vicap_set_dev_attr(VICAP_DEV_ID_0, dev_attr);
// CH0: YUV420 → VO
chn_attr.pix_format = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
kd_mpi_vicap_set_chn_attr(VICAP_DEV_ID_0, VICAP_CHN_ID_0, chn_attr);
// CH1: RGB888P → AI
chn_attr.pix_format = PIXEL_FORMAT_RGB_888_PLANAR;
kd_mpi_vicap_set_chn_attr(VICAP_DEV_ID_0, VICAP_CHN_ID_1, chn_attr);
kd_mpi_vicap_set_database_parse_mode(VICAP_DEV_ID_0, VICAP_DATABASE_PARSE_XML_JSON); // 必须在 init 前！
kd_mpi_vicap_init(VICAP_DEV_ID_0);
kd_mpi_vicap_start_stream(VICAP_DEV_ID_0);

// 4. Bind VICAP CH0 → VO（硬件直通，零拷贝）
k_mpp_chn src = {K_ID_VI, VICAP_DEV_ID_0, VICAP_CHN_ID_0};
k_mpp_chn dst = {K_ID_VO, K_VO_DISPLAY_DEV_ID, K_VO_DISPLAY_CHN_ID1};
kd_mpi_sys_bind(&src, &dst);

// 5. AI 抓帧（软路径）
k_video_frame_info frame;
kd_mpi_vicap_dump_frame(VICAP_DEV_ID_0, VICAP_CHN_ID_1, VICAP_DUMP_RGB, &frame, 1000);
// ... 推理 ...
kd_mpi_vicap_dump_release(VICAP_DEV_ID_0, VICAP_CHN_ID_1, &frame);

// 6. 清理（必须逆序）
kd_mpi_sys_unbind(&src, &dst);
kd_mpi_vicap_stop_stream(VICAP_DEV_ID_0);
kd_mpi_vicap_deinit(VICAP_DEV_ID_0);
kd_mpi_vb_exit();
```

### VO OSD 叠加层

```cpp
k_vo_video_osd_attr osd_attr = {};
osd_attr.pixel_format  = PIXEL_FORMAT_ARGB_8888;
osd_attr.display_rect  = {0, 0};
osd_attr.img_size      = {1920, 1080};
osd_attr.stride        = 1920 * 4 / 8;
osd_attr.global_alptha = 0xff;
kd_mpi_vo_set_video_osd_attr(K_VO_OSD3, &osd_attr);
kd_mpi_vo_osd_enable(K_VO_OSD3);

k_vb_blk_handle h = kd_mpi_vb_get_block(pool_id, size, NULL);
uint64_t phys = kd_mpi_vb_handle_to_phyaddr(h);
void *virt = kd_mpi_sys_mmap(phys, size);
cv::Mat osd(1080, 1920, CV_8UC4, virt);
cv::rectangle(osd, bbox, cv::Scalar(0,255,0,255), 2);
k_video_frame_info vf = {};
vf.v_frame.phys_addr[0] = phys;
vf.mod_id = K_ID_VO; vf.pool_id = pool_id;
vf.v_frame.pixel_format = PIXEL_FORMAT_ARGB_8888;
vf.v_frame.width = 1920; vf.v_frame.height = 1080;
kd_mpi_vo_chn_insert_frame(K_VO_OSD3 + 3, &vf);
```

### MMZ 内存分配

```cpp
uint64_t phys = 0; void *virt = nullptr;
kd_mpi_sys_mmz_alloc_cached(&phys, &virt, "name", "anonymous", size);
kd_mpi_sys_cache_invalidate(phys, size); // DMA写→CPU读前
kd_mpi_sys_cache_flush(phys, size);      // CPU写→DMA读前
kd_mpi_sys_mmz_free(phys, virt);

void *v = kd_mpi_sys_mmap(phys_addr, size);
kd_mpi_sys_munmap(v, size);
```

## [SKILL-6] 音频 API（关键：设备号不能搞反！）

```cpp
/* AI_DEV_PDM = 1 (PDM麦克风), AO_DEV_I2S = 0 (I2S扬声器) */

// PDM 采集（dev=1）
k_aio_dev_attr ai_attr = {};
ai_attr.audio_type = KD_AUDIO_INPUT_TYPE_PDM;
ai_attr.kd_audio_attr.pdm_attr.sample_rate         = 16000;
ai_attr.kd_audio_attr.pdm_attr.bit_width           = KD_AUDIO_BIT_WIDTH_16;
ai_attr.kd_audio_attr.pdm_attr.snd_mode            = KD_AUDIO_SOUND_MODE_MONO;
ai_attr.kd_audio_attr.pdm_attr.frame_num           = 50;   // 20ms/帧
ai_attr.kd_audio_attr.pdm_attr.point_num_per_frame = 320;  // 16000/50
kd_mpi_ai_set_pub_attr(1, &ai_attr);
kd_mpi_ai_enable(1);
kd_mpi_ai_enable_chn(1, 0);
k_audio_frame f;
kd_mpi_ai_get_frame(1, 0, &f, 1000);   // f.virt_addr, f.len
kd_mpi_ai_release_frame(1, 0, &f);

// I2S 播放（dev=0）
k_aio_dev_attr ao_attr = {};
ao_attr.audio_type = KD_AUDIO_OUTPUT_TYPE_I2S;
ao_attr.kd_audio_attr.i2s_attr.sample_rate = 48000;
ao_attr.kd_audio_attr.i2s_attr.bit_width   = KD_AUDIO_BIT_WIDTH_16;
ao_attr.kd_audio_attr.i2s_attr.snd_mode    = KD_AUDIO_SOUND_MODE_MONO;
ao_attr.kd_audio_attr.i2s_attr.i2s_type    = K_AIO_I2STYPE_INNERCODEC;
kd_mpi_ao_set_pub_attr(0, &ao_attr);
kd_mpi_ao_enable(0);
kd_mpi_ao_enable_chn(0, 0);
k_audio_frame pf;
pf.virt_addr = virt; pf.phys_addr = phys; pf.len = pcm_len;
kd_mpi_ao_send_frame(0, 0, &pf, 1000);
```

## [SKILL-7] 跨核通信（IPCM）

### 约束与规则

| 项目 | 值 |
|---|---|
| 大核 remote_id | **0** |
| 小核 remote_id | **1** |
| 单条消息最大 body | **1024 字节**（K_IPCMSG_MAX_CONTENT_LEN） |
| 服务名最长 | 15 字符 |
| `kd_ipcmsg_run()` | 阻塞调用，必须在**独立线程**中运行 |
| 两端协议头文件 | 必须**完全相同**（同一份 `ipcm_comm.h`） |

### IPCM 协议定义（ipcm_comm.h 关键内容）

```c
#define IPCM_SERVICE_NAME   "aicam"
#define IPCM_SERVICE_PORT   300

#define IPCM_MOD_AUDIO      0x10
#define IPCM_MOD_CONTROL    0x20

#define CMD_AUDIO_CAPTURE_DATA    0x1001  // 大→小：PCM数据（16kHz/16bit/mono）
#define CMD_AUDIO_PLAYBACK_DATA   0x1002  // 小→大：TTS PCM数据（48kHz）
#define CMD_AUDIO_CAPTURE_START   0x1010  // 小→大：开始采集
#define CMD_AUDIO_CAPTURE_STOP    0x1011  // 小→大：停止采集
#define CMD_AUDIO_PLAYBACK_START  0x1020  // 小→大：开始播放
#define CMD_AUDIO_PLAYBACK_STOP   0x1021  // 小→大：停止播放
#define CMD_AUDIO_PLAYBACK_DONE   0x1022  // 大→小：播放完成
#define CMD_CTRL_HEARTBEAT        0x2001
#define CMD_CTRL_TOUCH_EVENT      0x2003

#define AUDIO_CAP_SAMPLE_RATE    16000
#define AUDIO_CAP_FRAME_NUM      50        // → 320采样/帧 → 640字节 < 1024 ✓
#define AUDIO_PLAY_SAMPLE_RATE   48000
#define AUDIO_PLAY_CHUNK_MAX     960       // 48kHz 10ms

typedef struct {
    uint32_t sample_rate, channels, bit_width, total_bytes;
} __attribute__((packed)) audio_play_req_t;
```

### 大核 IPCM 初始化代码

```cpp
#include "k_ipcmsg.h"
static k_s32 g_ipcmsg_id = -1;

void ipcm_msg_handler(k_s32 id, k_ipcmsg_message_t *msg) {
    switch (msg->u32CMD) {
    case CMD_AUDIO_CAPTURE_START:
        audio_hw_capture_start(16000, 16, 1, on_pcm_frame, nullptr);
        break;
    case CMD_AUDIO_PLAYBACK_DATA:
        audio_hw_playback_send((uint8_t*)msg->pBody, msg->u32BodyLen);
        break;
    }
    kd_ipcmsg_destroy_message(msg);
}

// 初始化（remote=1 表示连接小核）
k_ipcmsg_connect_t conn = {1, IPCM_SERVICE_PORT, 0};
kd_ipcmsg_add_service(IPCM_SERVICE_NAME, &conn);
kd_ipcmsg_connect(&g_ipcmsg_id, IPCM_SERVICE_NAME, ipcm_msg_handler);

// 发送
k_ipcmsg_message_t *m = kd_ipcmsg_create_message(IPCM_MOD_AUDIO, CMD_AUDIO_CAPTURE_DATA, pcm, len);
kd_ipcmsg_send_only(g_ipcmsg_id, m);
kd_ipcmsg_destroy_message(m);

// 接收循环（必须在独立线程中）
kd_ipcmsg_run(g_ipcmsg_id);
```

### libipcmsg.a 小核兼容性修复

```bash
riscv64-unknown-linux-gnu-ar x libipcmsg.a
for obj in *.o; do
    riscv64-unknown-linux-gnu-objcopy --remove-section=.riscv.attributes "$obj"
done
riscv64-unknown-linux-gnu-ar rcs libipcmsg_patched.a *.o
```

## [SKILL-8] AI Camera 项目架构

```
大核 RT-Smart (ai_camera.elf)          小核 Linux (ai_camera_linux)
┌─────────────────────────────┐        ┌─────────────────────────────┐
│ Thread: AI推理               │        │ ESP-AI WebSocket 客户端      │
│  VICAP CH0 → VO (1080p显示) │        │  ws://node.espai.fun         │
│  VICAP CH1 → YOLOv8-Pose   │        │  api_key / device_id         │
│  OSD 骨骼关键点叠加          │        │                              │
├─────────────────────────────┤        ├─────────────────────────────┤
│ Thread: IPCM 音频桥          │◄──────►│ Audio Manager               │
│  PDM(16kHz) → IPCM → 小核  │        │  IPCM → ESP-AI PCM 上传     │
│  小核PCM → AO I2S(48kHz)   │        │  TTS MP3 解码 → IPCM → 大核 │
└─────────────────────────────┘        └─────────────────────────────┘
        IPCM "aicam" port=300
```

### ESP-AI 协议流

```
1. WebSocket 连接 ws://node.espai.fun
2. 发送 {type:"play_audio_ws_conntceed"}   // 握手
3. 发送 {type:"start", device_id, api_key} // 鉴权
4. 等待 session_status="iat_start"
5. 循环发送 PCM 二进制帧（16kHz/16bit/mono）
6. 发送 {type:"iat_end"}                   // 结束ASR
7. 接收 [4B sid][2B status][MP3数据]        // TTS响应
8. MP3 解码 → PCM → IPCM → 大核 I2S 播放
```

**配置文件** `/sharefs/ai_camera/config.json`：
```json
{
  "server_url": "ws://node.espai.fun",
  "api_key": "dfdfdb8758fb4810a50935f2e74c934f",
  "device_id": "70:D8:C2:72:4E:BC",
  "spk_sample_rate": 48000
}
```

### 部署步骤

```bash
# 1. 大核编译（SDK Docker 内）
make mpp-apps
# 或一键编译脚本
bash src/big/mpp/userapps/sample/ai_camera/build.sh

# 2. 小核编译
make buildroot-rebuild

# 3. 运行（大核 RT-Smart 串口）
/sharefs/ai_camera/ai_camera.elf models/yolov8n_pose.kmodel 0.5 0.45 1

# 4. 运行（小核 Linux 串口）
./ai_camera_linux /sharefs/ai_camera/config.json
```

## [SKILL-9] 大核启动流程

```
上电 → OpenSBI → U-Boot → 加载大核镜像到 0x00200000
大核启动 → bsp/maix3/applications/main.c
  → msh_exec("/bin/init.sh")  ← RT_SHELL_PATH
  → init.sh: /bin/fastboot_app.elf /bin/test.kmodel
```

**修改开机自启动程序：**
```bash
# 路径：k230_sdk/src/big/rt-smart/userapps/root/bin/init.sh
/sharefs/ai_camera/ai_camera.elf /sharefs/ai_camera/models/yolov8n_pose.kmodel 0.5 0.45 0
```

**大核调试指令（RT-Smart shell）：**
```bash
list_thread   # 查看所有线程（含状态）
list_process  # 查看进程
free          # 内存使用
ps            # 进程状态
```

`/sharefs` 是大小核共享文件系统，大核加载 kmodel 必须从 `/sharefs` 读取。

## [SKILL-10] 小核 WiFi 配置（串口环境）

串口下不能用 vi，用 echo 逐行写入：

```bash
echo 'ctrl_interface=/var/run/wpa_supplicant' > /etc/wpa_supplicant.conf
echo 'update_config=1' >> /etc/wpa_supplicant.conf
echo 'network={' >> /etc/wpa_supplicant.conf
echo '    ssid="YOUR_WIFI_SSID"' >> /etc/wpa_supplicant.conf
echo '    psk="YOUR_WIFI_PASSWORD"' >> /etc/wpa_supplicant.conf
echo '}' >> /etc/wpa_supplicant.conf

ifconfig wlan0 up
wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf
udhcpc -i wlan0
```

## [SKILL-11] 开发环境搭建（WSL2 Ubuntu 20.04）

```bash
# 安装依赖
sudo apt install -y build-essential git wget curl cmake libssl-dev \
                   libncurses5-dev python3 python3-pip rsync unzip flex bison

# 首次编译（Ubuntu 20.04 必须用 env -i 清理环境变量，否则 Makefile 路径冲突）
cd ~/k230_sdk
env -i HOME="$HOME" USER="$USER" TERM="$TERM" \
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    make CONF=k230_canmv_01studio_defconfig
```

### Docker 编译环境（推荐）

```bash
cd ~/k230_sdk
source tools/get_download_url.sh && make prepare_sourcecode

# 国内镜像加速（修改 Dockerfile）
sed -i 's|^FROM ubuntu:20.04$|FROM docker.m.daocloud.io/library/ubuntu:20.04|' tools/docker/Dockerfile
sudo docker build -f tools/docker/Dockerfile -t k230_docker tools/docker
sudo docker run --rm -u root -it \
    -v $(pwd):$(pwd) \
    -v $(pwd)/toolchain:/opt/toolchain \
    -w $(pwd) k230_docker /bin/bash

# Docker 内编译
make CONF=k230_canmv_01studio_defconfig

# Docker ↔ 原生 Ubuntu 切换时必须清理
rm -rf output/k230_canmv_01studio_defconfig
rm -f prepare_memory
```

## [SKILL-12] AI 推理（nncase / KPU）

### AIBase 继承模式（标准用法）

```cpp
// 视频模式：绑定 ISP 物理地址，ai2d 硬件预处理
poseDetect pd(
    "models/yolov8n_pose.kmodel", 0.5f, 0.45f,
    {3, 720, 1280},      // CHW 输入尺寸
    (uintptr_t)vaddr,    // ISP 虚拟地址
    (uintptr_t)paddr,    // ISP 物理地址
    1                    // debug_mode
);

// 每帧推理三步
pd.pre_process();    // ai2d 硬件缩放/padding 到 kmodel 输入尺寸
pd.inference();      // KPU 推理
std::vector<OutputPose> results;
pd.post_process(results, pd.params);  // 解码 + NMS
```

引用官方 ai_poc（无需复制源文件）：
```cmake
include_directories(${k230_sdk}/src/reference/ai_poc/pose_detect)
```

## [SKILL-13] 大核 MPP Sample 速查

| 模块 | ELF | 用途 |
|---|---|---|
| VICAP | `sample_vicap.elf` | 多传感器采集、dump帧、ISP配置 |
| VO | `sample_vo.elf` | 多Connector测试、OSD、旋转缩放 |
| VENC | `sample_venc.elf` | H.264/H.265/JPEG硬件编码 |
| VDEC | `sample_vdec.elf` | H.264/H.265/JPEG硬件解码 |
| Audio | `sample_audio.elf` | 13种模式：AI采集/AO播放/回环/3A |
| LVGL | `sample-lvgl.elf` | VGLite GPU加速GUI |
| DMA | `sample_dma.elf` | 旋转/镜像/搬运 |
| DPU | `sample_dpu_vo.elf` | 结构光深度图 |
| GPIO | `sample_gpio.elf` | 按键/引脚IO |
| PWM | `sample_pwm.elf` | 占空比输出 |
| ADC | `sample_adc.elf` | 6通道1.8V 12bit |
| PM | `sample_pm.elf` | CPU/KPU调频、温度保护 |
| FFT | `sample_fft.elf` | 硬件64~4096点FFT |

## [SKILL-14] 常用 Sensor / Connector 枚举

| Sensor 枚举 | 说明 |
|---|---|
| `GC2093_MIPI_CSI2_1920X1080_60FPS_10BIT_LINEAR` | 01Studio 板载（当前使用） |
| `OV5647_MIPI_CSI2_1920X1080_30FPS_10BIT_LINEAR` | CanMV 标配 |
| `OV9286_MIPI_CSI2_1280X960_45FPS_10BIT_LINEAR` | 结构光/ToF |
| `IMX335_MIPI_CSI2_2592X1944_30FPS_12BIT_LINEAR` | 高分辨率 |

| Connector 枚举 | 说明 |
|---|---|
| `LT9611_MIPI_4LAN_1920X1080_30FPS` | HDMI（01Studio，当前使用） |
| `HX8377_V2_MIPI_4LAN_1080X1920_30FPS` | EVB 竖屏 |
| `ILI9806_MIPI_2LAN_480X800_30FPS` | 东山派 LCD |

| 板型 defconfig | 宏 |
|---|---|
| `k230_canmv_01studio_defconfig` | `CONFIG_BOARD_K230_CANMV_01STUDIO` |
| `k230_canmv_defconfig` | `CONFIG_BOARD_K230_CANMV` |
| `k230_evb_defconfig` | `CONFIG_BOARD_K230_EVB` |

## [SKILL-15] 常见陷阱

1. **VB blk_size 必须 4K 对齐**：用 `VICAP_ALIGN_UP(size, VICAP_ALIGN_1K)`，否则 VB 初始化失败
2. **VICAP 双路配置**：`set_database_parse_mode` 必须在 `kd_mpi_vicap_init` 之前调用
3. **音频设备号**：PDM 采集 dev=1，I2S 播放 dev=0，绝对不能搞反
4. **IPCM body ≤ 1024 字节**：音频数据必须按帧分割（16kHz/50帧=640字节/帧）
5. **libipcmsg.a**：小核链接时需去除 `.riscv.attributes` ELF 节（工具链不兼容）
6. **`kd_ipcmsg_run()` 是阻塞调用**：必须放在独立线程，不能在主线程调用
7. **修改内存布局必须全量重编**：不能增量编译
8. **Docker ↔ Ubuntu 切换**：需 `rm -rf output/xxx && rm -f prepare_memory` 后重编
9. **CMake 链接顺序**：nncase 相关库必须在 `--start-group` / `--end-group` 内
10. **大核加载 kmodel 路径**：只能从 `/sharefs` 读，不能用绝对路径硬编码到 romfs
11. **板型宏来源**：由 `make prepare_memory` 生成 `k_autoconf_comm.h`，手动编译需 `-DCONFIG_BOARD_xxx`
12. **VB 资源泄漏**：程序异常退出后 VB 未释放，需重启大核（或重新执行 `kd_mpi_vb_exit`）


