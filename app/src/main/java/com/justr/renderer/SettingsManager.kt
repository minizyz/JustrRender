package com.justr.renderer

import android.content.Context
import android.content.SharedPreferences

/**
 * JustrRender 设置管理器
 *
 * 管理渲染器的所有可配置选项，包括：
 * - 渲染后端选择（自动/Vulkan/OpenGL ES）
 * - FSR1 超分辨率模式
 * - VSync 开关
 * - MSAA 抗锯齿
 * - 其他渲染优化选项
 *
 * 设置通过 SharedPreferences 持久化，并在渲染器初始化时通过
 * 环境变量或 JNI 接口传递给原生层。
 */
object SettingsManager {

    private const val PREFS_NAME = "justr_render_settings"

    // ===== 设置键名 =====
    const val KEY_BACKEND = "backend"
    const val KEY_FSR_MODE = "fsr_mode"
    const val KEY_FSR_SHARPENING = "fsr_sharpening"
    const val KEY_VSYNC = "vsync"
    const val KEY_MSAA = "msaa"
    const val KEY_FORCE_HIGH_REFRESH = "force_high_refresh"
    const val KEY_KEEP_AWAKE = "keep_awake"
    const val KEY_DEBUG_LOG = "debug_log"
    const val KEY_CUSTOM_SCALE = "custom_scale"

    // ===== 默认值 =====
    const val DEFAULT_BACKEND = "opengles"
    const val DEFAULT_FSR_MODE = "off"
    const val DEFAULT_FSR_SHARPENING = 0.5f
    const val DEFAULT_VSYNC = true
    const val DEFAULT_MSAA = 0
    const val DEFAULT_FORCE_HIGH_REFRESH = false
    const val DEFAULT_KEEP_AWAKE = true
    const val DEFAULT_DEBUG_LOG = false
    const val DEFAULT_CUSTOM_SCALE = 1.0f

    // ===== FSR 模式定义 =====
    enum class FsrMode(val key: String, val displayName: String, val scaleFactor: Float) {
        OFF("off", "关闭", 1.0f),
        ULTRA_QUALITY("ultra_quality", "极致画质", 1.3f),
        QUALITY("quality", "质量", 1.5f),
        BALANCED("balanced", "平衡", 1.7f),
        PERFORMANCE("performance", "性能", 2.0f);

        companion object {
            fun fromKey(key: String): FsrMode {
                return entries.find { it.key == key } ?: OFF
            }
        }
    }

    // ===== 后端定义 =====
    enum class Backend(val key: String, val displayName: String) {
        AUTO("auto", "自动"),
        VULKAN("vulkan", "Vulkan（实验性）"),
        OPENGLES("opengles", "OpenGL ES");

        companion object {
            fun fromKey(key: String): Backend {
                return entries.find { it.key == key } ?: OPENGLES
            }
        }
    }

    // ===== MSAA 选项 =====
    val MSAA_OPTIONS = listOf(0, 2, 4, 8)

    private lateinit var prefs: SharedPreferences

    fun init(context: Context) {
        prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    }

    private fun ensureInit() {
        if (!::prefs.isInitialized) {
            throw IllegalStateException("SettingsManager not initialized. Call init(context) first.")
        }
    }

    // ===== 通用读写 =====
    fun getString(key: String, default: String): String {
        ensureInit()
        return prefs.getString(key, default) ?: default
    }

    fun putString(key: String, value: String) {
        ensureInit()
        prefs.edit().putString(key, value).apply()
    }

    fun getBoolean(key: String, default: Boolean): Boolean {
        ensureInit()
        return prefs.getBoolean(key, default)
    }

    fun putBoolean(key: String, value: Boolean) {
        ensureInit()
        prefs.edit().putBoolean(key, value).apply()
    }

    fun getInt(key: String, default: Int): Int {
        ensureInit()
        return prefs.getInt(key, default)
    }

    fun putInt(key: String, value: Int) {
        ensureInit()
        prefs.edit().putInt(key, value).apply()
    }

    fun getFloat(key: String, default: Float): Float {
        ensureInit()
        return prefs.getFloat(key, default)
    }

    fun putFloat(key: String, value: Float) {
        ensureInit()
        prefs.edit().putFloat(key, value).apply()
    }

    // ===== 类型安全的便捷方法 =====

    var backend: Backend
        get() = Backend.fromKey(getString(KEY_BACKEND, DEFAULT_BACKEND))
        set(value) = putString(KEY_BACKEND, value.key)

    var fsrMode: FsrMode
        get() = FsrMode.fromKey(getString(KEY_FSR_MODE, DEFAULT_FSR_MODE))
        set(value) = putString(KEY_FSR_MODE, value.key)

    var fsrSharpening: Float
        get() = getFloat(KEY_FSR_SHARPENING, DEFAULT_FSR_SHARPENING)
        set(value) = putFloat(KEY_FSR_SHARPENING, value.coerceIn(0f, 1f))

    var vsync: Boolean
        get() = getBoolean(KEY_VSYNC, DEFAULT_VSYNC)
        set(value) = putBoolean(KEY_VSYNC, value)

    var msaa: Int
        get() = getInt(KEY_MSAA, DEFAULT_MSAA)
        set(value) = putInt(KEY_MSAA, value)

    var forceHighRefresh: Boolean
        get() = getBoolean(KEY_FORCE_HIGH_REFRESH, DEFAULT_FORCE_HIGH_REFRESH)
        set(value) = putBoolean(KEY_FORCE_HIGH_REFRESH, value)

    var keepAwake: Boolean
        get() = getBoolean(KEY_KEEP_AWAKE, DEFAULT_KEEP_AWAKE)
        set(value) = putBoolean(KEY_KEEP_AWAKE, value)

    var debugLog: Boolean
        get() = getBoolean(KEY_DEBUG_LOG, DEFAULT_DEBUG_LOG)
        set(value) = putBoolean(KEY_DEBUG_LOG, value)

    var customScale: Float
        get() = getFloat(KEY_CUSTOM_SCALE, DEFAULT_CUSTOM_SCALE)
        set(value) = putFloat(KEY_CUSTOM_SCALE, value.coerceIn(0.5f, 1.5f))

    /**
     * 生成传递给原生渲染器的环境变量映射
     */
    fun toEnvMap(): Map<String, String> {
        return mapOf(
            "JUSTR_BACKEND" to backend.key,
            "JUSTR_FSR_MODE" to fsrMode.key,
            "JUSTR_FSR_SHARPENING" to "%.3f".format(fsrSharpening),
            "JUSTR_VSYNC" to if (vsync) "1" else "0",
            "JUSTR_MSAA" to msaa.toString(),
            "JUSTR_DEBUG" to if (debugLog) "1" else "0",
            "JUSTR_CUSTOM_SCALE" to "%.3f".format(customScale)
        )
    }

    /**
     * 重置所有设置为默认值
     */
    fun resetAll() {
        ensureInit()
        prefs.edit().clear().apply()
    }
}
