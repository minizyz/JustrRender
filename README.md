# JustrRender

**Fold Craft Launcher (FCL) 自定义渲染器插件**

基于 OpenGL ES 3.0 的轻量级渲染器插件，可被 Fold Craft Launcher 调用，为 Android 平台上的 Minecraft: Java Edition 提供硬件加速渲染支持。

## 特性

- **OpenGL ES 3.0** 原生硬件加速渲染
- **EGL 上下文管理**：完整的窗口表面、上下文创建与销毁
- **VSync 支持**：可开关的垂直同步
- **多架构支持**：arm64-v8a、armeabi-v7a、x86_64
- **FCL 插件兼容**：通过 `fclPlugin` meta-data 自动识别
- **自动降级**：不支持 ES 3.0 时自动回退到 ES 2.0

## 项目结构

```
JustrRender/
├── app/
│   ├── build.gradle.kts              # App 模块构建配置
│   ├── proguard-rules.pro
│   └── src/main/
│       ├── AndroidManifest.xml       # 插件声明（fclPlugin meta-data）
│       ├── java/com/justr/renderer/
│       │   └── MainActivity.kt       # 插件信息页面
│       ├── jni/                      # 原生渲染器源码
│       │   ├── Android.mk            # NDK 构建脚本
│       │   ├── Application.mk        # NDK 应用配置
│       │   ├── justr_render.h        # 渲染器头文件
│       │   ├── justr_render.c        # 渲染器核心实现
│       │   └── egl_bridge.c          # FCL EGL 桥接层
│       ├── jniLibs/                  # 编译后的 .so 库存放目录
│       │   ├── arm64-v8a/
│       │   ├── armeabi-v7a/
│       │   └── x86_64/
│       └── res/                      # Android 资源
├── build.gradle.kts                  # 根构建配置
├── settings.gradle.kts
├── gradle.properties
└── .github/workflows/build.yml       # CI 自动构建
```

## 构建

### 前置要求

- Android Studio Hedgehog (2023.1.1) 或更高
- Android SDK 34
- Android NDK r26+
- JDK 17

### 1. 编译原生库

```bash
cd app/src/main/jni
ndk-build NDK_PROJECT_PATH=. \
          APP_BUILD_SCRIPT=./Android.mk \
          NDK_APPLICATION_MK=./Application.mk
```

编译产物会输出到 `app/src/main/libs/`，将各架构的 `libjustr_render.so` 复制到对应的 `jniLibs/` 目录：

```bash
cp -r app/src/main/libs/* app/src/main/jniLibs/
```

### 2. 构建 APK

```bash
# Debug 版本
./gradlew assembleDebug

# Release 版本
./gradlew assembleRelease
```

APK 输出路径：`app/build/outputs/apk/`

### CI 自动构建

推送代码到 GitHub 后，GitHub Actions 会自动：
1. 编译所有架构的原生库
2. 构建 Debug 和 Release APK
3. 上传构建产物

打 `v*` 标签时会自动创建 GitHub Release。

## 安装与使用

1. 安装构建好的 `JustrRender.apk`
2. 打开 **Fold Craft Launcher**
3. 进入 **版本设置** → **渲染器**
4. 在列表中选择 **JustrRender**
5. 启动游戏

## 渲染器配置

在 `app/build.gradle.kts` 中可调整以下参数：

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `des` | 启动器中显示的渲染器名称 | `JustrRender` |
| `renderer` | 渲染器定义 `名称:GL库:EGL库` | `JustrRender:libjustr_render.so:libjustr_render.so` |
| `boatEnv` | Boat 启动器环境变量 | `POJAV_RENDERER=opengles3` |
| `pojavEnv` | Pojav 环境变量 | `POJAV_RENDERER=opengles3` |
| `minMCVer` | 最低支持 MC 版本 | 无限制 |
| `maxMCVer` | 最高支持 MC 版本 | 无限制 |

### 环境变量说明

- `POJAV_RENDERER=opengles3`：指定渲染器类型为 OpenGL ES 3.0
- `LIBGL_ES=3`：使用 OpenGL ES 3.0
- `LIBGL_NOINTOVLHACK=1`：禁用重叠 hack（提高兼容性）

## FCL 插件接口

JustrRender 通过 AndroidManifest 中的 meta-data 向 FCL 注册：

```xml
<meta-data android:name="fclPlugin" android:value="true" />
<meta-data android:name="des" android:value="JustrRender" />
<meta-data android:name="renderer" android:value="JustrRender:libjustr_render.so:libjustr_render.so" />
<meta-data android:name="pojavEnv" android:value="POJAV_RENDERER=opengles3:LIBGL_ES=3" />
```

FCL 启动游戏时会：
1. 读取插件的 `renderer` meta-data 获取库名
2. 加载 `libjustr_render.so`
3. 设置 `pojavEnv` 中的环境变量
4. 通过 EGL 桥接函数管理渲染上下文

## 原生库接口

`libjustr_render.so` 导出以下 FCL 桥接函数：

| 函数 | 说明 |
|------|------|
| `pojav_set_native_window()` | 设置渲染窗口 |
| `pojav_egl_create_context()` | 创建 EGL 上下文 |
| `pojav_egl_create_window_surface()` | 创建窗口表面 |
| `pojav_egl_make_current()` | 绑定当前上下文 |
| `pojav_egl_swap_buffers()` | 交换缓冲区（呈现帧） |
| `pojav_egl_swap_interval()` | 设置交换间隔（VSync） |
| `pojav_egl_destroy_context()` | 销毁上下文 |
| `pojav_egl_destroy_surface()` | 销毁表面 |
| `pojav_get_proc_address()` | 获取 GL 函数指针 |
| `pojav_renderer_start/stop()` | 渲染器生命周期 |

## 许可证

MIT License - 详见 [LICENSE](LICENSE) 文件。

## 致谢

- [Fold Craft Launcher](https://github.com/FCL-Team/FoldCraftLauncher) - Android 平台 Minecraft Java 版启动器
- [PojavLauncher](https://github.com/PojavLauncherTeam/PojavLauncher) - 原生运行时参考
- [FCLRendererPlugin](https://github.com/ShirosakiMio/FCLRendererPlugin) - 渲染器插件模板参考
- [RendererPlugin-v2](https://github.com/ZalithLauncher/RendererPlugin-v2) - V2 插件架构参考
