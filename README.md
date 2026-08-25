# JustrRender

**Fold Craft Launcher (FCL) 自定义渲染器插件 — 双后端 + FSR1 超分辨率**

基于 **Vulkan 优先 + OpenGL ES 自动回退** 的轻量级渲染器插件，内置 **AMD FSR 1.0** 超分辨率，可被 Fold Craft Launcher 调用。

## 特性

- **双后端自动切换**：优先 Vulkan，设备不支持时自动回退 OpenGL ES 3.0
- **Vulkan WSI**：完整的 Instance/Device/Swapchain 管理
- **OpenGL ES 3.0**：EGL 上下文管理，ES 2.0 降级兼容
- **FSR 1.0 超分辨率**：EASU 空间上采样 + RCAS 对比度自适应锐化
  - 关闭 / 极致画质(1.3x) / 质量(1.5x) / 平衡(1.7x) / 性能(2.0x)
  - 可调节锐化强度
- **Miuix 设置界面**：MIUI 风格的 Compose 设置页，实时修改所有选项
- **VSync 控制**、**MSAA 抗锯齿**(0/2/4/8x)、**自定义渲染缩放**(50%-150%)
- **多架构支持**：arm64-v8a、armeabi-v7a、x86_64

## 设置界面

打开 JustrRender 应用，点击「渲染器设置」进入 Miuix 风格设置页：

| 分类 | 选项 | 说明 |
|------|------|------|
| 渲染后端 | 自动 / Vulkan / OpenGL ES | 选择图形 API |
| FSR 1.0 | 关闭 / 极致画质 / 质量 / 平衡 / 性能 | 超分辨率模式 |
| FSR 1.0 | 锐化强度滑块 | RCAS 锐化 0%-100% |
| 画质 | MSAA 0x/2x/4x/8x | 多重采样抗锯齿 |
| 显示 | VSync / 强制高刷 / 保持常亮 | 显示相关设置 |
| 高级 | 自定义渲染缩放 / 调试日志 | 高级选项 |

设置通过 SharedPreferences 持久化，下次启动游戏时通过环境变量传递给原生渲染器。

## FSR 1.0 模式

| 模式 | 缩放比 | 1080p 渲染分辨率 | 适用场景 |
|------|--------|------------------|----------|
| 关闭 | 1.0x | 1920x1080 | 高性能设备 |
| 极致画质 | 1.3x | 1477x831 | 画质优先 |
| 质量 | 1.5x | 1280x720 | 推荐日常使用 |
| 平衡 | 1.7x | 1129x635 | 画质与性能平衡 |
| 性能 | 2.0x | 960x540 | 最大化帧率 |

> FSR 1.0 当前仅在 OpenGL ES 后端上可用。

## 后端选择

通过环境变量 `JUSTR_BACKEND` 控制：`auto`（默认）/ `vulkan` / `opengles`

## 项目结构

```
JustrRender/
├── app/
│   ├── build.gradle.kts
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/com/justr/renderer/
│       │   ├── MainActivity.kt        # 主界面 + 设置入口
│       │   ├── SettingsActivity.kt    # Miuix 设置界面
│       │   └── SettingsManager.kt     # SharedPreferences 封装
│       ├── jni/
│       │   ├── Android.mk
│       │   ├── justr_render.h/c       # 核心调度 + GLES 后端
│       │   ├── vulkan_render.h/c      # Vulkan WSI 实现
│       │   ├── fsr_render.h/c         # FSR 1.0 (EASU + RCAS)
│       │   └── egl_bridge.c           # FCL 桥接层
│       └── res/
└── .github/workflows/build.yml
```

## 构建

```bash
# 编译原生库
cd app/src/main/jni
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk NDK_APPLICATION_MK=./Application.mk
cp -r ../libs/* ../jniLibs/

# 构建 APK
./gradlew assembleDebug
```

## 环境变量

| 变量 | 值 | 说明 |
|------|-----|------|
| `JUSTR_BACKEND` | auto/vulkan/opengles | 后端选择 |
| `JUSTR_FSR_MODE` | off/ultra_quality/quality/balanced/performance | FSR 模式 |
| `JUSTR_FSR_SHARPENING` | 0.0-1.0 | FSR 锐化强度 |
| `JUSTR_VSYNC` | 0/1 | 垂直同步 |
| `JUSTR_MSAA` | 0/2/4/8 | MSAA 采样数 |
| `JUSTR_CUSTOM_SCALE` | 0.5-1.5 | 自定义渲染缩放 |
| `JUSTR_DEBUG` | 0/1 | 调试日志 |

## FSR 渲染管线

```
游戏渲染 → 低分辨率 FBO (input)
    ↓ EASU Pass: 边缘自适应空间上采样
目标分辨率 FBO (output)
    ↓ RCAS Pass: 对比度自适应锐化
默认帧缓冲 (屏幕) → eglSwapBuffers
```

## 许可证

MIT License

## 致谢

- [Fold Craft Launcher](https://github.com/FCL-Team/FoldCraftLauncher)
- [AMD FidelityFX-FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR)
- [Miuix](https://github.com/compose-miuix-ui/miuix)
