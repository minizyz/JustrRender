package com.justr.renderer

import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

/**
 * JustrRender 渲染器插件主 Activity
 *
 * 此 Activity 仅用于展示插件信息，实际渲染功能由原生库 libjustr_render.so 提供。
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
            appendLine("渲染器 ID: opengles3")
            appendLine("原生库: libjustr_render.so")
            appendLine("OpenGL ES: 3.0")
            appendLine()
            appendLine("使用方法:")
            appendLine("1. 确保已安装 Fold Craft Launcher")
            appendLine("2. 打开 FCL → 版本设置 → 渲染器")
            appendLine("3. 选择 \"JustrRender\"")
            appendLine("4. 启动游戏即可")
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
