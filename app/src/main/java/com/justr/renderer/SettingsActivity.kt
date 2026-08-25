package com.justr.renderer

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.theme.ThemeController
import top.yukonga.miuix.kmp.theme.ColorSchemeMode
import top.yukonga.miuix.kmp.preference.SwitchPreference
import top.yukonga.miuix.kmp.preference.CheckboxPreference
import top.yukonga.miuix.kmp.preference.SliderPreference
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Button
import androidx.compose.ui.Alignment

class SettingsActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        SettingsManager.init(this)

        setContent {
            val controller = remember { ThemeController(ColorSchemeMode.System) }
            MiuixTheme(controller = controller) {
                SettingsScreen(
                    onBack = { finish() }
                )
            }
        }
    }
}

@Composable
fun SettingsScreen(onBack: () -> Unit) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = "JustrRender 设置",
                onBack = onBack
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            RenderBackendSection()
            FsrSection()
            QualitySection()
            DisplaySection()
            AdvancedSection()
            ResetButton()
        }
    }
}

@Composable
fun RenderBackendSection() {
    var backend by remember { mutableStateOf(SettingsManager.backend) }
    val backendOptions = SettingsManager.Backend.entries
    val selectedIndex = backendOptions.indexOf(backend)

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = "渲染后端",
                modifier = Modifier.padding(bottom = 8.dp)
            )
            Text(
                text = "选择图形 API，自动模式优先使用 Vulkan",
                modifier = Modifier.padding(bottom = 12.dp)
            )

            backendOptions.forEachIndexed { index, option ->
                CheckboxPreference(
                    title = option.displayName,
                    summary = when (option) {
                        SettingsManager.Backend.AUTO -> "优先 Vulkan，不支持时回退 OpenGL ES"
                        SettingsManager.Backend.VULKAN -> "强制使用 Vulkan（需要设备支持）"
                        SettingsManager.Backend.OPENGLES -> "强制使用 OpenGL ES 3.0"
                    },
                    checked = index == selectedIndex,
                    onCheckedChange = { checked ->
                        if (checked) {
                            backend = option
                            SettingsManager.backend = option
                        }
                    }
                )
            }
        }
    }
}

@Composable
fun FsrSection() {
    var fsrMode by remember { mutableStateOf(SettingsManager.fsrMode) }
    var sharpening by remember { mutableStateOf(SettingsManager.fsrSharpening) }
    val fsrOptions = SettingsManager.FsrMode.entries
    val selectedIndex = fsrOptions.indexOf(fsrMode)

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = "FSR 1.0 超分辨率",
                modifier = Modifier.padding(bottom = 8.dp)
            )
            Text(
                text = "AMD FidelityFX Super Resolution，降低渲染分辨率后放大以提升帧率",
                modifier = Modifier.padding(bottom = 12.dp)
            )

            fsrOptions.forEachIndexed { index, option ->
                CheckboxPreference(
                    title = option.displayName,
                    summary = when (option) {
                        SettingsManager.FsrMode.OFF -> "原生分辨率，不使用超分辨率"
                        SettingsManager.FsrMode.ULTRA_QUALITY -> "1.3x 放大，画质损失极小"
                        SettingsManager.FsrMode.QUALITY -> "1.5x 放大，推荐日常使用"
                        SettingsManager.FsrMode.BALANCED -> "1.7x 放大，画质与性能平衡"
                        SettingsManager.FsrMode.PERFORMANCE -> "2.0x 放大，最大化帧率"
                    },
                    checked = index == selectedIndex,
                    onCheckedChange = { checked ->
                        if (checked) {
                            fsrMode = option
                            SettingsManager.fsrMode = option
                        }
                    }
                )
            }

            if (fsrMode != SettingsManager.FsrMode.OFF) {
                Spacer(modifier = Modifier.height(8.dp))
                SliderPreference(
                    title = "锐化强度",
                    summary = "FSR RCAS 锐化强度：%.0f%%".format(sharpening * 100),
                    value = sharpening,
                    onValueChange = {
                        sharpening = it
                        SettingsManager.fsrSharpening = it
                    },
                    valueRange = 0f..1f,
                    modifier = Modifier.padding(top = 8.dp)
                )
            }
        }
    }
}

@Composable
fun QualitySection() {
    var msaa by remember { mutableStateOf(SettingsManager.msaa) }
    val msaaOptions = SettingsManager.MSAA_OPTIONS
    val selectedIndex = msaaOptions.indexOf(msaa).coerceAtLeast(0)

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = "画质设置",
                modifier = Modifier.padding(bottom = 8.dp)
            )

            Text(
                text = "MSAA 多重采样抗锯齿",
                modifier = Modifier.padding(bottom = 4.dp)
            )
            Text(
                text = "更高的采样数带来更平滑的边缘，但会增加性能开销",
                modifier = Modifier.padding(bottom = 12.dp)
            )

            msaaOptions.forEachIndexed { index, samples ->
                CheckboxPreference(
                    title = if (samples == 0) "关闭" else "${samples}x MSAA",
                    summary = when (samples) {
                        0 -> "不使用抗锯齿，最高性能"
                        2 -> "轻度抗锯齿，性能影响小"
                        4 -> "标准抗锯齿，推荐"
                        8 -> "高质量抗锯齿，性能开销大"
                        else -> ""
                    },
                    checked = index == selectedIndex,
                    onCheckedChange = { checked ->
                        if (checked) {
                            msaa = samples
                            SettingsManager.msaa = samples
                        }
                    }
                )
            }
        }
    }
}

@Composable
fun DisplaySection() {
    var vsync by remember { mutableStateOf(SettingsManager.vsync) }
    var forceHighRefresh by remember { mutableStateOf(SettingsManager.forceHighRefresh) }
    var keepAwake by remember { mutableStateOf(SettingsManager.keepAwake) }

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = "显示设置",
                modifier = Modifier.padding(bottom = 8.dp)
            )

            SwitchPreference(
                title = "垂直同步 (VSync)",
                summary = "锁定帧率到屏幕刷新率，减少画面撕裂",
                checked = vsync,
                onCheckedChange = {
                    vsync = it
                    SettingsManager.vsync = it
                }
            )

            SwitchPreference(
                title = "强制高刷新率",
                summary = "尝试使用设备支持的最高刷新率（需要设备支持）",
                checked = forceHighRefresh,
                onCheckedChange = {
                    forceHighRefresh = it
                    SettingsManager.forceHighRefresh = it
                }
            )

            SwitchPreference(
                title = "保持屏幕常亮",
                summary = "游戏运行时防止屏幕自动关闭",
                checked = keepAwake,
                onCheckedChange = {
                    keepAwake = it
                    SettingsManager.keepAwake = it
                }
            )
        }
    }
}

@Composable
fun AdvancedSection() {
    var customScale by remember { mutableStateOf(SettingsManager.customScale) }
    var debugLog by remember { mutableStateOf(SettingsManager.debugLog) }

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = "高级设置",
                modifier = Modifier.padding(bottom = 8.dp)
            )

            SliderPreference(
                title = "自定义渲染缩放",
                summary = "渲染分辨率缩放：%.0f%%（FSR 关闭时生效）".format(customScale * 100),
                value = customScale,
                onValueChange = {
                    customScale = it
                    SettingsManager.customScale = it
                },
                valueRange = 0.5f..1.5f,
                modifier = Modifier.padding(vertical = 8.dp)
            )

            SwitchPreference(
                title = "调试日志",
                summary = "输出详细的渲染器调试信息到 logcat",
                checked = debugLog,
                onCheckedChange = {
                    debugLog = it
                    SettingsManager.debugLog = it
                }
            )
        }
    }
}

@Composable
fun ResetButton() {
    var showConfirm by remember { mutableStateOf(false) }

    Button(
        onClick = { showConfirm = true },
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 16.dp)
    ) {
        Text("恢复默认设置")
    }

    if (showConfirm) {
        androidx.compose.ui.window.Dialog(onDismissRequest = { showConfirm = false }) {
            Card(modifier = Modifier.padding(16.dp)) {
                Column(modifier = Modifier.padding(20.dp)) {
                    Text(
                        text = "确认重置",
                        modifier = Modifier.padding(bottom = 8.dp)
                    )
                    Text(
                        text = "所有设置将恢复为默认值，此操作不可撤销。",
                        modifier = Modifier.padding(bottom = 16.dp)
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.End
                    ) {
                        Button(onClick = { showConfirm = false }) {
                            Text("取消")
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        Button(onClick = {
                            SettingsManager.resetAll()
                            showConfirm = false
                        }) {
                            Text("确认")
                        }
                    }
                }
            }
        }
    }
}
