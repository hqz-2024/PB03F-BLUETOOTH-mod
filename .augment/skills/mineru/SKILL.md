---
name: mineru
description: MinerU 文档解析工具知识库，涵盖安装、环境配置、CUDA加速、CLI使用、批量转换等完整操作指南。适用于所有 MinerU PDF/文档解析任务。
---

# MinerU 知识库

## 基本信息

- **版本**：3.1.x（2026年最新）
- **功能**：将 PDF / DOCX / PPTX / XLSX / 图片转为 Markdown / JSON
- **支持系统**：Windows 10/11、Linux、macOS 11+
- **Python 版本**：3.10–3.13（Windows 推荐 3.10）

---

## 环境要求

| 项目 | 要求 |
|------|------|
| RAM | 最低 16GB，推荐 32GB |
| 磁盘 | 最低 20GB（pipeline模式），SSD 推荐 |
| GPU VRAM | pipeline: 4GB / vlm: 8GB |
| NVIDIA 驱动 | 最新专有驱动 |
| CUDA 版本 | 11.8 / 12.1 / 12.4 / 12.6（任选其一） |

---

## 安装流程

### 1. 安装 Conda（若未安装）

下载 Miniconda：https://docs.conda.io/en/latest/miniconda.html

### 2. 创建 Python 环境

```bash
# 推荐 Python 3.10（Windows 必须用 3.10）
conda create -n mineru python=3.10 -y
conda activate mineru
```

### 3. 安装 MinerU（pip + uv 方式，推荐）

```bash
pip install --upgrade pip
pip install uv
uv pip install -U "mineru[all]"
```

### 4. 从源码安装（可选）

```bash
git clone https://github.com/opendatalab/MinerU.git
cd MinerU
uv pip install -e ".[all]"
```

---

## CUDA GPU 加速配置

### Windows CUDA 安装步骤

**Step 1：安装 CUDA Toolkit**

前往 https://developer.nvidia.com/cuda-downloads 下载并安装，
支持版本：CUDA 11.8 / 12.1 / 12.4 / 12.6

**Step 2：安装 cuDNN**（CUDA 11.8 需要 cuDNN v8.7.0）

前往 https://developer.nvidia.com/cudnn 下载并配置环境变量

**Step 3：安装支持 CUDA 的 PyTorch**

```bash
# CUDA 11.8
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu118

# CUDA 12.1
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu121

# CUDA 12.4
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu124
```

### Linux / macOS

Linux 和 macOS 会自动尝试 CUDA/MPS 加速，无需额外配置 torch。

### 验证 CUDA 可用

```bash
python -c "import torch; print(torch.cuda.is_available())"
```

---

## CLI 使用指令

### 基本转换

```bash
# GPU 加速（自动检测）
mineru -p <输入路径> -o <输出目录>

# 强制 CPU 模式（pipeline 后端）
mineru -p <输入路径> -o <输出目录> -b pipeline

# 指定 vlm 后端（精度更高）
mineru -p <输入路径> -o <输出目录> -b vlm-auto-engine
```

### 完整参数说明

```bash
mineru \
  -p <input_path>          # 输入：文件或目录（PDF/图片/DOCX/PPTX/XLSX）
  -o <output_path>         # 输出目录
  -b <backend>             # 后端：pipeline | hybrid-auto-engine | vlm-auto-engine | vlm-http-client
  -l <lang>                # 语言：ch | en | japan | korean 等（提升OCR精度）
  -s <start_page>          # 起始页（0-based）
  -e <end_page>            # 结束页（0-based）
  -f <true|false>          # 是否解析公式（默认 true）
  -t <true|false>          # 是否解析表格（默认 true）
  --api-url <url>          # 连接已有 mineru-api 服务
```

### 后端选择指南

| 后端 | 特点 | 推荐场景 |
|------|------|---------|
| `pipeline` | 快速稳定，纯CPU可用 | 批量处理，资源受限 |
| `hybrid-auto-engine` | 默认，平衡精度与速度 | 日常使用 |
| `vlm-auto-engine` | 最高精度，需GPU | 复杂版面、公式密集 |
| `vlm-http-client` | 连接远程推理服务 | 分布式部署 |

---

## 批量转换

### 批量转换整个目录

```bash
# 转换目录下所有支持的文件
mineru -p ./pdf_folder/ -o ./output/

# 批量 + pipeline 后端 + 指定批大小
mineru -p ./pdf_folder/ -o ./output/ -b pipeline

# 批量 + 中文文档优化
mineru -p ./pdf_folder/ -o ./output/ -b pipeline -l ch
```

### PowerShell 批量脚本示例

```powershell
# 遍历指定目录下所有 PDF 逐一转换
Get-ChildItem -Path "D:\documents" -Filter "*.pdf" | ForEach-Object {
    mineru -p $_.FullName -o "D:\output\$($_.BaseName)" -b pipeline
}
```

### Bash 批量脚本示例

```bash
# 批量转换并记录日志
for f in ./pdfs/*.pdf; do
    mineru -p "$f" -o "./output/$(basename "$f" .pdf)" -b pipeline
done
```

---

## 模型源配置

```bash
# 默认使用 HuggingFace，国内网络切换为 ModelScope
export MINERU_MODEL_SOURCE=modelscope

# 使用本地模型（需先下载）
export MINERU_MODEL_SOURCE=local
```

---

## 启动 WebUI

```bash
# 启动 Gradio Web 界面
mineru-gradio
```

## 启动 API 服务

```bash
# 启动本地 FastAPI 服务（默认端口 8000）
mineru-api

# 查看 API 文档
# 浏览器访问 http://localhost:8000/docs
```

---

## 常见问题

- **Windows CLI 无法运行**：降级到 Python 3.10，`ray` 组件在 Windows Python 3.13 不支持
- **CUDA 不可用**：检查 `torch.cuda.is_available()`，确认 torch 与 CUDA 版本匹配
- **内存不足**：使用 `pipeline` 后端，或分段处理（`-s` / `-e` 参数）
- **网络限制下载模型**：设置 `MINERU_MODEL_SOURCE=modelscope`
