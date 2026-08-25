# JustrRender

**Fold Craft Launcher (FCL) 自定义渲染器插件 — 双后端架构**

基于 **Vulkan 优先 + OpenGL ES 自动回退** 的轻量级渲染器插件，可被 Fold Craft Launcher 调用，为 Android 平台上的 Minecraft: Java Edition 提供硬件加速渲染支持。

## 特性

- **双后端自动切换**：优先 Vulkan，设备不支持时自动回退 OpenGL ES 3.0
- **Vulkan WSI**：完整的 Instance/Device/Swapchain 管理，支持 Android Surface
- **OpenGL ES 3.0**：EGL 上下文管理，ES 2.0 降级兼容
- **VSync 控制**：可开关的垂直同步（FIFO / MAILBOX / IMMEDIATE）
- **多架构支持**：arm64-v8a、armeabi-v7a、x86_64
- **FCL 插件兼容**：通过 `fclPlugin` meta-data 自动识别
- **运行时探测**：启动时自动检测 Vulkan 可用性并缓存结果

## 后端选择策略

```
启动 → 读取 JUSTR_BACKEND 环境变量
         │
    ┌────┴────┐
    │  auto   │ → 探测 Vulkan → 可用则用 Vulkan，不可用则用 GLES
    │ vulkan  │ → 强制 Vulkan，失败则报错
    │ opengles│ → 强制 OpenGL ES
    └─────────┘
```

通过环境变量 `JUSTR_BACKEND` 控制：
- `auto`（默认）— 优先 Vulkan，自动回退
- `vulkan` — 强制 Vulkan
- `opengles` — 强制 OpenGL ES

## 项目结构

```
JustrRender/
├── app/
│   ├── build.gradle.kts
│   └── src/main/
│       ├── AndroidManifest.xml           # 插件声明
│       ├── java/com/justr/renderer/
│       │   └── MainActivity.kt
│       ├── jni/                          # 原生渲染器源码
│       │   ├── Android.mk                # NDK 构建（链接 -lvulkan）
│       │   ├── Application.mk
│       │   ├── justr_render.h            # 双后端统一接口
│       │   ├── justr_render.c            # 核心调度 + GLES 后端
│       │   ├── vulkan_render.h           # Vulkan 后端头文件
│       │   ├── vulkan_render.c           # Vulkan WSI 实现
│       │   └── egl_bridge.c              # FCL 桥接层（双后端调度）
│       ├── jniLibs/                      # 编译后的 .so 库
│       └── res/
├── build.gradle.kts
└── .github/workflows/build.yml           # CI 自动构建
```

## 构建

### 前置要求

- Android Studio Hedgehog (2023.1.1)+
- Android SDK 34
- Android NDK r26+（需包含 Vulkan headers）
- JDK 17

### 1. 编译原生库

```bash
cd app/src/main/jni
ndk-build NDK_PROJECT_PATH=. \
          APP_BUILD_SCRIPT=./Android.mk \
          NDK_APPLICATION_MK=./Application.mk
cp -r ../libs/* ../jniLibs/
```

### 2. 构建 APK

```bash
./gradlew assembleDebug    # Debug
./gradlew assembleRelease  # Release
```

### CI 自动构建

推送代码后 GitHub Actions 自动编译原生库 + APK，打 `v*` 标签自动发布 Release。

## 安装与使用

1. 安装 `JustrRender.apk`
2. 打开 **Fold Craft Launcher**
3. 进入 **版本设置** → **渲染器**
4. 选择 **JustrRender (Vulkan+GLES)**
5. 启动游戏 — 自动选择最佳后端

## 渲染器配置

在 `app/build.gradle.kts` 中调整：

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `des` | 启动器显示名称 | `JustrRender (Vulkan+GLES)` |
| `renderer` | 库定义 `名称:GL库:EGL库` | `JustrRender:libjustr_render.so:...` |
| `pojavEnv` | 环境变量 | 含 `JUSTR_BACKEND=auto` |
| `minMCVer` / `maxMCVer` | MC 版本限制 | 无限制 |

### 环境变量

| 变量 | 值 | 说明 |
|------|-----|------|
| `JUSTR_BACKEND` | `auto` / `vulkan` / `opengles` | 后端选择 |
| `POJAV_RENDERER` | `opengles3` | FCL 渲染器类型 |
| `LIBGL_ES` | `3` | OpenGL ES 版本 |

## Vulkan 后端架构

```
vkCreateInstance
    ↓
vkEnumeratePhysicalDevices → 选择最佳 GPU（优先独显）
    ↓
vkCreateAndroidSurfaceKHR → 绑定 ANativeWindow
    ↓
vkCreateDevice + 队列（graphics + present）
    ↓
vkCreateSwapchainKHR → 图像视图
    ↓
渲染循环：vkAcquireNextImageKHR → 渲染 → vkQueuePresentKHR
```

Vulkan 后端导出的关键接口：
- `justr_vk_probe()` — 设备可用性探测
- `justr_vk_init()` — 完整初始化
- `justr_vk_acquire_next_image()` — 获取下一帧图像
- `justr_vk_present()` — 呈现到屏幕
- `justr_vk_set_vsync()` — 切换呈现模式（重建 swapchain）

## FCL 插件接口

通过 AndroidManifest meta-data 注册：

```xml
<meta-data android:name="fclPlugin" android:value="true" />
<meta-data android:name="des" android:value="JustrRender (Vulkan+GLES)" />
<meta-data android:name="renderer" android:value="JustrRender:libjustr_render.so:libjustr_render.so" />
<meta-data android:name="pojavEnv" android:value="POJAV_RENDERER=opengles3:JUSTR_BACKEND=auto" />
```

FCL 启动时加载 `libjustr_render.so`，调用 `pojav_egl_*` 桥接函数，桥接层根据当前活动后端分发到 Vulkan 或 GLES 实现。

## 许可证

MIT License — 详见 [LICENSE](LICENSE)

## 致谢

- [Fold Craft Launcher](https://github.com/FCL-Team/FoldCraftLauncher)
- [PojavLauncher](https://github.com/PojavLauncherTeam/PojavLauncher)
- [FCLRendererPlugin](https://github.com/ShirosakiMio/FCLRendererPlugin)
- [RendererPlugin-v2](https://github.com/ZalithLauncher/RendererPlugin-v2)
