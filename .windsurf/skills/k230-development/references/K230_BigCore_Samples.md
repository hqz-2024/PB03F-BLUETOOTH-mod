# K230 大核 MPP 例程总览

> 源码路径：`k230_sdk/src/big/mpp/userapps/sample/`

---

## 一、视频输入（VICAP / 摄像头）

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_vicap` | `sample_vicap.elf` | 摄像头采集综合示例。支持多传感器（OV5647/OV9732/OV9286/IMX335等）、多通道输出、裁剪、旋转、镜像、HDR、离线模式、Raw图加载、帧dump保存、ISP寄存器导出。用法：`./sample_vicap.elf -mode 0 -dev 0 -sensor 0 -chn 0 -ow 640 -oh 480 -preview 1` |
| `sample_vicap_transform_bin` | `sample_vicap.elf`(复用) | VICAP标定文件转换工具 |
| `sample_ahd_sensor` | — | AHD模拟高清传感器采集示例 |
| `sample_ahd_mipi_sensor` | — | AHD转MIPI传感器采集示例 |
| `sample_vdss` | `sample_vdss.elf` | VDSS（Video DSS）子系统测试 |
| `sample_mcm` | `sample_mcm.elf` | 多摄像头管理（MCM），同时操作3路VICAP（XS9950传感器），支持实时切换显示通道和视频编码 |

## 二、视频输出（VO / 显示）

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_vo` | `sample_vo.elf` | 视频输出综合测试。支持多种test case：背景色、OSD叠加、图层插帧、VVI绑定、多帧OSD、DSI读ID、图层旋转/缩放、多种Connector（HX8377/LT9611/ST7701/ILI9806）。用法：`./sample_vo.elf [test_case]` |
| `sample_lvgl` | `sample-lvgl.elf` | LVGL GUI框架 + VGLite GPU加速。用法：`./sample-lvgl.elf -d 2`（3.5寸LCD） |

## 三、视频编码（VENC）

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_venc` | `sample_venc.elf` | 硬件视频编码。支持H.265、JPEG、H.264+OSD叠加、H.265+OSD+边框。从摄像头采集后实时编码保存。用法：`./sample_venc.elf [0-3] -sensor [id] -o [file]` |

## 四、视频解码（VDEC）

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_vdec` | `sample_vdec.elf` | 硬件视频解码。支持H.264/H.265/JPEG文件解码并绑定VO上屏显示。用法：`./sample_vdec.elf -i test.265 -type 0` |

## 五、音视频综合

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_av` | `sample_av.elf` | 音视频同步测试。同时进行摄像头采集+视频编码+音频采集+音频编码，检测AV同步时间戳偏差 |

## 六、音频（AI/AO/AENC/ADEC）

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_audio` | `sample_audio.elf` | 音频综合示例（13种模式）：AI采集（I2S/PDM）、AO播放、AI→AO回环、AI绑定AO、音频编码（G.711A）、音频解码、编解码回环。支持设置采样率/位宽/通道数/3A。用法：`./sample_audio.elf -type [0-12] -samplerate 44100` |

## 七、AI / 人脸检测

| 例程 | ELF | 功能 |
|------|-----|------|
| `fastboot_app` | `fastboot_app.elf` | 开机默认程序。初始化LCD+摄像头+VO，运行MobileRetinaface人脸检测并画框。用法：`./fastboot_app.elf test.kmodel` |
| `sample_face_detect` | `sample_face_detect.elf` | KPU人脸检测示例，与fastboot_app类似但可独立配置 |
| `sample_face_ae` | `sample_face_ae.elf` | 人脸检测+自动曝光联动，根据人脸区域调节AE ROI |
| `sample_triple_camera_facedetect` | `sample_triple_camera_facedetect.elf` | 三摄像头同时人脸检测 |

## 八、图像处理

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_dma` | `sample_dma.elf` | DMA数据搬运测试。GDMA支持旋转（90°/180°）、镜像、多种像素格式（YUV400/420/Planar）；SDMA支持1D/2D数据搬运 |
| `sample_dma_bind` | `sample_dma_bind.elf` | DMA绑定模式测试，通过系统绑定机制自动搬运数据 |
| `sample_nonai_2d` | `sample_nonai_2d.elf` | Non-AI 2D硬件加速：OSD叠加、边框绘制、CSC色彩空间转换 |
| `sample_csc` | `sample_csc.elf` | 色彩空间转换（CSC）+ OSD叠加测试 |
| `sample_dw200` | `sample_dw200.elf` | DW200去畸变/透视校正。支持鱼眼镜头畸变矫正，通过JSON配置映射表 |

## 九、深度处理（DPU）

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_dpu` | `sample_dpu.elf` | DPU深度处理单元测试，立体视觉深度图生成 |
| `sample_dpu_vicap` | `sample_dpu_vicap.elf` | DPU + VICAP联合测试，从摄像头实时获取深度图 |
| `sample_dpu_vo` | `sample_dpu_vo.elf` | DPU + VO联合测试，深度图实时显示 |
| `sample_vdd_r` | `sample_vdd_r.elf` | VDD-R（VICAP→DPU→Display）完整流水线测试 |
| `sample_vdv` | `sample_vdv.elf` | VICAP→DPU→VO完整流水线 |

## 十、GPU

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_gpu_cube` | `sample_gpu_cube.elf` | VGLite GPU渲染3D立方体示例 |
| `tiger` | `tiger.elf` | VGLite GPU矢量图形渲染（老虎SVG） |

## 十一、外设（GPIO / PWM / UART / I2C / ADC / 触摸屏）

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_gpio` | `sample_gpio.elf` | GPIO读写测试。CanMV板上为按键检测（S1/S2），其他板为引脚输入输出回环测试 |
| `sample_gpio_intc` | `sample_gpio_intc.elf` | GPIO中断测试，支持上升沿/下降沿/双边沿/高低电平触发 |
| `sample_pwm` | `sample_pwm.elf` | PWM输出测试。配置通道2/3/4分别输出75%/50%/25%占空比，周期100μs |
| `sample_canaan_uart_tx` | `sample_canaan_uart_tx.elf` | UART发送测试 |
| `sample_canaan_uart_rx` | `sample_canaan_uart_rx.elf` | UART接收测试 |
| `sample_i2c_slave` | `sample_i2c_slave.elf` | I2C从设备模式测试 |
| `sample_touch_ft5406` | `sample_touch_ft5406.elf` | FT5406触摸屏事件读取（I2C接口），输出触摸坐标和事件类型 |
| `sample_eeprom` | `sample_eeprom.elf` | I2C EEPROM读写测试 |
| `sample_sensor_otp` | `sample_sensor_otp.elf` | 摄像头传感器OTP数据读取 |

## 十二、系统 / 内存 / 定时器 / 看门狗

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_mmz` | `sample_mmz.elf` | MMZ内存分配/释放/映射测试（`kd_mpi_sys_mmz_alloc/free`），含cached分配和flush |
| `sample_vb` | `sample_vb.elf` | VB视频缓冲池创建/销毁/获取/释放测试 |
| `sample_hwtimer` | `sample_hwtimer.elf` | 硬件定时器测试 |
| `sample_wdt` | `sample_wdt.elf` | 看门狗测试。设置超时时间，创建喂狗线程定时喂狗。用法：`./sample_wdt.elf [超时秒数] [喂狗间隔秒数]` |
| `sample_adc` | `sample_adc.elf` | ADC模数转换测试，读取6通道ADC电压值（1.8V参考，12位精度） |
| `sample_ts` | `sample_ts.elf` | 芯片温度传感器读取 |
| `sample_log` | `sample_log.elf` | 日志系统测试 |
| `sample_otp` | `sample_otp.elf` | OTP（一次性可编程）熔丝读写测试 |
| `sample_virtual_vio` | `sample_virtual_vio.elf` | 虚拟视频输入输出测试（VVI） |

## 十三、电源管理

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_pm` | `sample_pm.elf` | 电源管理综合测试。支持16种功能：获取/设置频率档位（CPU/KPU/DPU/VPU/Display/Media域）、调频策略（manual/performance/energysaving）、温度保护、热关机阈值、时钟/电源开关。用法：`./sample_pm.elf [func_index] [参数]` |

## 十四、硬件加速计算

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_fft` | `sample_fft.elf` | 硬件FFT/IFFT测试。对64~4096点进行正逆变换并校验精度，输出耗时 |

## 十五、安全 / 加密

| 例程 | ELF | 功能 |
|------|-----|------|
| `sample_cipher/sample_aes` | `sample_aes.elf` | AES硬件加密测试 |
| `sample_cipher/sample_hash` | `sample_hash.elf` | Hash摘要计算测试 |
| `sample_cipher/sample_hwhash` | `sample_hwhash.elf` | 硬件Hash加速测试 |
| `sample_cipher/sample_pufs` | `sample_pufs.elf` | PUF（物理不可克隆函数）安全测试 |

## 十六、其他

| 例程 | ELF | 功能 |
|------|-----|------|
| `opencv_camera_test` | — | OpenCV VideoCapture摄像头测试（C++） |

