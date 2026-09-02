package com.justr.renderer

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    private lateinit var textView: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        SettingsManager.init(this)

        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(48, 48, 48, 48)
        }

        textView = TextView(this).apply {
            text = buildInfoText()
            textSize = 13f
            // 开启滚动，防止内容过长屏幕溢出
            isScrollContainer = true
            setHorizontallyScrolling(false)
        }

        val settingsButton = Button(this).apply {
            text = "渲染器设置"
            setOnClickListener {
                startActivity(Intent(this@MainActivity, SettingsActivity::class.java))
            }
        }

        layout.addView(textView)
        layout.addView(
            settingsButton,
            LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = 32 }
        )

        setContentView(layout)
    }

    /** 构建页面显示的全部信息文本，抽成独立方法方便刷新 */
    private fun buildInfoText(): String {
        return buildString {
            appendLine("JustrRender Renderer Plugin")
            appendLine("============================")
            appendLine()
            appendLine("版本: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
            appendLine("包名: ${BuildConfig.APPLICATION_ID}")
            appendLine()
            appendLine("渲染后端: 双后端自动切换")
            appendLine("  优先: Vulkan (WSI Swapchain)")
            appendLine("  回退: OpenGL ES 3.0")
            appendLine("超分辨率: FSR 1.0 (EASU + RCAS)")
            appendLine("原生库: libjustr_render.so")
            appendLine()
            appendLine("当前设置:")
            appendLine("  后端: ${SettingsManager.backend.displayName}")
            appendLine("  FSR: ${SettingsManager.fsrMode.displayName}")
            appendLine("  VSync: ${if (SettingsManager.vsync) "开启" else "关闭"}")
            appendLine("  MSAA: ${if (SettingsManager.msaa == 0) "关闭" else "${SettingsManager.msaa}x"}")
            appendLine()
            appendLine("使用方法:")
            appendLine("1. 确保已安装 Fold Craft Launcher")
            appendLine("2. 打开 FCL → 版本设置 → 渲染器")
            appendLine("3. 选择 \"JustrRender\"")
            appendLine("4. 启动游戏，自动选择最佳后端")
            appendLine()
            appendLine("注意: 此插件需配合 FCL 使用，单独打开无游戏功能。")
        }
    }

    override fun onResume() {
        super.onResume()
        // 从设置页面返回，只更新文本，不重建Activity
        textView.text = buildInfoText()
    }
                       }
