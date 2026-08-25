package com.justr.renderer

import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

/**
 * JustrRender 渲染器插件主 Activity
 *
 * 此 Activity 仅用于展示插件信息，实际渲染功能由原生库 libjustr_render.so 提供。
 * 双后端架构：优先使用 Vulkan，设备不支持时自动回退 OpenGL ES 3.0。
 * 安装此 APK 后，在 Fold Craft Launcher 的版本设置 → 渲染器中选择 "JustrRender" 即可启用。
 */
class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val info = buildString {
            appendLine("JustrRender Renderer Plugin")
            appendLine("============================")
            appendLine()
            appendLine("版本: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
            appendLine("包名: ${BuildConfig.APPLICATION_ID}")
            appendLine()
            appendLine("渲染后端: 双后端自动切换")
            appendLine("  优先: Vulkan (WSI Swapchain)")
            appendLine("  回退: OpenGL ES 3.0")
            appendLine("原生库: libjustr_render.so")
            appendLine()
            appendLine("环境变量:")
            appendLine("  JUSTR_BACKEND=auto (默认)")
            appendLine("  可选: vulkan / opengles")
            appendLine()
            appendLine("使用方法:")
            appendLine("1. 确保已安装 Fold Craft Launcher")
            appendLine("2. 打开 FCL → 版本设置 → 渲染器")
            appendLine("3. 选择 \"JustrRender (Vulkan+GLES)\"")
            appendLine("4. 启动游戏，自动选择最佳后端")
            appendLine()
            appendLine("注意: 此插件需配合 FCL 使用，单独打开无游戏功能。")
        }

        val textView = TextView(this).apply {
            text = info
            setPadding(48, 48, 48, 48)
            textSize = 14f
        }
        setContentView(textView)
    }
}
