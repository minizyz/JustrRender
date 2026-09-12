package com.justr.renderer

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * JustrRender 渲染器插件主 Activity
 *
 * 标签页式 UI：主页 + 设置
 * 实际渲染功能由原生库 libjustr_render.so 提供
 * 双后端架构：优先使用 Vulkan，设备不支持时自动回退 OpenGL ES 3.0
 * 支持 FSR 1.0 超分辨率
 */
class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        SettingsManager.init(this)
        setContent {
            MaterialTheme {
                MainScreen()
            }
        }
    }
}

/**
 * 主屏幕：顶部标题栏 + 标签页切换
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen() {
    var selectedTab by remember { mutableIntStateOf(0) }
    val tabs = listOf("主页", "设置")

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("JustrRender") }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
        ) {
            TabRow(
                selectedTabIndex = selectedTab
            ) {
                tabs.forEachIndexed { index, title ->
                    Tab(
                        selected = selectedTab == index,
                        onClick = { selectedTab = index },
                        text = { Text(title) }
                    )
                }
            }

            when (selectedTab) {
                0 -> HomeTab()
                1 -> SettingsTab()
            }
        }
    }
}

// ==================== 主页标签页 ====================

@Composable
fun HomeTab() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // 插件信息卡片
        Card(
            modifier = Modifier.fillMaxWidth()
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = "JustrRender 渲染器插件",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
                HorizontalDivider()
                InfoRow("版本", "${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
                InfoRow("包名", BuildConfig.APPLICATION_ID)
                InfoRow("原生库", "libjustr_render.so")
            }
        }

        // 渲染后端卡片
        Card(
            modifier = Modifier.fillMaxWidth()
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = "渲染后端",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )
                HorizontalDivider()
                InfoRow("优先", "Vulkan (WSI Swapchain)")
                InfoRow("回退", "OpenGL ES 3.0")
                InfoRow("超分辨率", "FSR 1.0 (EASU + RCAS)")
            }
        }

        // 当前设置卡片
        Card(
            modifier = Modifier.fillMaxWidth()
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = "当前设置",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )
                HorizontalDivider()
                InfoRow("后端", SettingsManager.backend.displayName)
                InfoRow("FSR", SettingsManager.fsrMode.displayName)
                InfoRow("VSync", if (SettingsManager.vsync) "开启" else "关闭")
                InfoRow("MSAA", if (SettingsManager.msaa == 0) "关闭" else "${SettingsManager.msaa}x")
            }
        }

        // 使用方法卡片
        Card(
            modifier = Modifier.fillMaxWidth()
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = "使用方法",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )
                HorizontalDivider()
                Text("1. 确保已安装 Fold Craft Launcher")
                Text("2. 打开 FCL → 版本设置 → 渲染器")
                Text("3. 选择 \"JustrRender\"")
                Text("4. 启动游戏，自动选择最佳后端")
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "注意：此插件需配合 FCL 使用，单独打开无游戏功能。",
                    fontSize = 12.sp
                )
            }
        }
    }
}

@Composable
fun InfoRow(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(text = label, fontSize = 14.sp)
        Text(text = value, fontSize = 14.sp, fontWeight = FontWeight.Medium)
    }
}

// ==================== 设置标签页 ====================

@Composable
fun SettingsTab() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
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

// ===== 渲染后端设置 =====
@Composable
fun RenderBackendSection() {
    var backend by remember { mutableStateOf(SettingsManager.backend) }
    val backendOptions = SettingsManager.Backend.entries
    val selectedIndex = backendOptions.indexOf(backend)

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(
                text = "渲染后端",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold
            )
            Text(
                text = "选择图形 API，自动模式优先使用 Vulkan",
                fontSize = 13.sp
            )
            HorizontalDivider()
            backendOptions.forEachIndexed { index, option ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    RadioButton(
                        selected = index == selectedIndex,
                        onClick = {
                            backend = option
                            SettingsManager.backend = option
                        }
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text(
                            text = option.displayName,
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Medium
                        )
                        Text(
                            text = when (option) {
                                SettingsManager.Backend.AUTO -> "优先 Vulkan，不支持时回退 OpenGL ES"
                                SettingsManager.Backend.VULKAN -> "强制使用 Vulkan（需要设备支持）"
                                SettingsManager.Backend.OPENGLES -> "强制使用 OpenGL ES 3.0"
                            },
                            fontSize = 12.sp
                        )
                    }
                }
            }
        }
    }
}

// ===== FSR1 超分辨率设置 =====
@Composable
fun FsrSection() {
    var fsrMode by remember { mutableStateOf(SettingsManager.fsrMode) }
    var sharpening by remember { mutableStateOf(SettingsManager.fsrSharpening) }
    val fsrOptions = SettingsManager.FsrMode.entries
    val selectedIndex = fsrOptions.indexOf(fsrMode)

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(
                text = "FSR 1.0 超分辨率",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold
            )
            Text(
                text = "AMD FidelityFX Super Resolution，降低渲染分辨率后放大以提升帧率",
                fontSize = 13.sp
            )
            HorizontalDivider()
            fsrOptions.forEachIndexed { index, option ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    RadioButton(
                        selected = index == selectedIndex,
                        onClick = {
                            fsrMode = option
                            SettingsManager.fsrMode = option
                        }
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text(
                            text = option.displayName,
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Medium
                        )
                        Text(
                            text = when (option) {
                                SettingsManager.FsrMode.OFF -> "原生分辨率，不使用超分辨率"
                                SettingsManager.FsrMode.ULTRA_QUALITY -> "1.3x 放大，画质损失极小"
                                SettingsManager.FsrMode.QUALITY -> "1.5x 放大，推荐日常使用"
                                SettingsManager.FsrMode.BALANCED -> "1.7x 放大，画质与性能平衡"
                                SettingsManager.FsrMode.PERFORMANCE -> "2.0x 放大，最大化帧率"
                            },
                            fontSize = 12.sp
                        )
                    }
                }
            }
            if (fsrMode != SettingsManager.FsrMode.OFF) {
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = "锐化强度：%.0f%%".format(sharpening * 100),
                    fontSize = 14.sp
                )
                Slider(
                    value = sharpening,
                    onValueChange = { v ->
                        sharpening = v
                        SettingsManager.fsrSharpening = v
                    },
                    valueRange = 0f..1f
                )
            }
        }
    }
}

// ===== 画质设置 =====
@Composable
fun QualitySection() {
    var msaa by remember { mutableStateOf(SettingsManager.msaa) }
    val msaaOptions = SettingsManager.MSAA_OPTIONS
    val selectedIndex = msaaOptions.indexOf(msaa).coerceAtLeast(0)

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(
                text = "画质设置",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold
            )
            Text(
                text = "MSAA 多重采样抗锯齿",
                fontSize = 13.sp
            )
            Text(
                text = "更高的采样数带来更平滑的边缘，但会增加性能开销",
                fontSize = 12.sp
            )
            HorizontalDivider()
            msaaOptions.forEachIndexed { index, samples ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    RadioButton(
                        selected = index == selectedIndex,
                        onClick = {
                            msaa = samples
                            SettingsManager.msaa = samples
                        }
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text(
                            text = if (samples == 0) "关闭" else "${samples}x MSAA",
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Medium
                        )
                        Text(
                            text = when (samples) {
                                0 -> "不使用抗锯齿，最高性能"
                                2 -> "轻度抗锯齿，性能影响小"
                                4 -> "标准抗锯齿，推荐"
                                8 -> "高质量抗锯齿，性能开销大"
                                else -> ""
                            },
                            fontSize = 12.sp
                        )
                    }
                }
            }
        }
    }
}

// ===== 显示设置 =====
@Composable
fun DisplaySection() {
    var vsync by remember { mutableStateOf(SettingsManager.vsync) }
    var forceHighRefresh by remember { mutableStateOf(SettingsManager.forceHighRefresh) }
    var keepAwake by remember { mutableStateOf(SettingsManager.keepAwake) }

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(
                text = "显示设置",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold
            )
            HorizontalDivider()
            SwitchSettingItem(
                title = "垂直同步 (VSync)",
                summary = "锁定帧率到屏幕刷新率，减少画面撕裂",
                checked = vsync,
                onCheckedChange = {
                    vsync = it
                    SettingsManager.vsync = it
                }
            )
            SwitchSettingItem(
                title = "强制高刷新率",
                summary = "尝试使用设备支持的最高刷新率（需要设备支持）",
                checked = forceHighRefresh,
                onCheckedChange = {
                    forceHighRefresh = it
                    SettingsManager.forceHighRefresh = it
                }
            )
            SwitchSettingItem(
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
fun SwitchSettingItem(
    title: String,
    summary: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(
            modifier = Modifier.weight(1f)
        ) {
            Text(
                text = title,
                fontSize = 14.sp,
                fontWeight = FontWeight.Medium
            )
            Text(
                text = summary,
                fontSize = 12.sp
            )
        }
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange
        )
    }
}

// ===== 高级设置 =====
@Composable
fun AdvancedSection() {
    var customScale by remember { mutableStateOf(SettingsManager.customScale) }
    var debugLog by remember { mutableStateOf(SettingsManager.debugLog) }

    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(
                text = "高级设置",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold
            )
            HorizontalDivider()
            Text(
                text = "自定义渲染缩放：%.0f%%（FSR 关闭时生效）".format(customScale * 100),
                fontSize = 14.sp
            )
            Slider(
                value = customScale,
                onValueChange = { v ->
                    customScale = v
                    SettingsManager.customScale = v
                },
                valueRange = 0.5f..1.5f
            )
            Spacer(modifier = Modifier.height(8.dp))
            SwitchSettingItem(
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

// ===== 重置按钮 =====
@Composable
fun ResetButton() {
    var showConfirm by remember { mutableStateOf(false) }

    Button(
        onClick = { showConfirm = true },
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp)
    ) {
        Text("恢复默认设置")
    }

    if (showConfirm) {
        AlertDialog(
            onDismissRequest = { showConfirm = false },
            title = { Text("确认重置") },
            text = { Text("所有设置将恢复为默认值，此操作不可撤销。") },
            confirmButton = {
                TextButton(onClick = {
                    SettingsManager.resetAll()
                    showConfirm = false
                }) {
                    Text("确认")
                }
            },
            dismissButton = {
                TextButton(onClick = { showConfirm = false }) {
                    Text("取消")
                }
            }
        )
    }
}
