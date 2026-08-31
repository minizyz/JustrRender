plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}
android {
    namespace = "com.justr.renderer"
    compileSdk = 36
    defaultConfig {
        applicationId = "com.justr.renderer"
        minSdk = 26
        targetSdk = 35
        compileSdk = 37
        versionCode = 3
        versionName = "1.2.0"
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
            resValue("string", "app_name", "JustrRender")
            manifestPlaceholders["des"] = "JustrRender (Vulkan+GLES+FSR1)"
            manifestPlaceholders["renderer"] = "JustrRender:libjustr_render.so:libjustr_render.so"
            manifestPlaceholders["boatEnv"] = listOf(
                "POJAV_RENDERER=opengles3",
                "LIBGL_ES=3",
                "LIBGL_NOINTOVLHACK=1",
                "JUSTR_BACKEND=auto"
            ).joinToString(":")
            manifestPlaceholders["pojavEnv"] = listOf(
                "POJAV_RENDERER=opengles3",
                "LIBGL_ES=3",
                "LIBGL_NOINTOVLHACK=1",
                "JUSTR_BACKEND=auto"
            ).joinToString(":")
            manifestPlaceholders["minMCVer"] = ""
            manifestPlaceholders["maxMCVer"] = ""
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.15"
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
    implementation("androidx.activity:activity-compose:1.8.2")
    // Compose BOM
    val composeBom = platform("androidx.compose:compose-bom:2024.02.00")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    debugImplementation("androidx.compose.ui:ui-tooling")
    // Miuix - MIUI 风格 Compose UI 库
    implementation("top.yukonga.miuix.kmp:miuix-ui-android:0.9.1")
    implementation("top.yukonga.miuix.kmp:miuix-preference-android:0.9.1")
}
