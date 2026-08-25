plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.justr.renderer"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.justr.renderer"
        minSdk = 26
        targetSdk = 34
        versionCode = 2
        versionName = "1.1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        configureEach {
            // 应用名（桌面显示）
            resValue("string", "app_name", "JustrRender")

            // 渲染器在启动器内显示的名称
            manifestPlaceholders["des"] = "JustrRender (Vulkan+GLES)"

            // 渲染器定义：名称:GL库名:EGL库名
            // 双后端：Vulkan 优先，自动回退 GLES
            manifestPlaceholders["renderer"] = "JustrRender:libjustr_render.so:libjustr_render.so"

            // Boat 环境变量（旧版启动器兼容）
            // 格式：KEY=VALUE:KEY=VALUE
            // JUSTR_BACKEND=auto: 优先 Vulkan，失败回退 GLES
            manifestPlaceholders["boatEnv"] = listOf(
                "POJAV_RENDERER=opengles3",
                "LIBGL_ES=3",
                "LIBGL_NOINTOVLHACK=1",
                "JUSTR_BACKEND=auto"
            ).joinToString(":")

            // Pojav 环境变量
            manifestPlaceholders["pojavEnv"] = listOf(
                "POJAV_RENDERER=opengles3",
                "LIBGL_ES=3",
                "LIBGL_NOINTOVLHACK=1",
                "JUSTR_BACKEND=auto"
            ).joinToString(":")

            // 最小支持的 MC 版本（空字符串表示不限制）
            manifestPlaceholders["minMCVer"] = ""

            // 最大支持的 MC 版本（空字符串表示不限制）
            manifestPlaceholders["maxMCVer"] = ""
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    kotlinOptions {
        jvmTarget = "1.8"
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
}
