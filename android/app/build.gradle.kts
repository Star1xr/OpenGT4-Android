plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

// The native side reports the same version (passed to CMake, used for logcat).
val openGt4Version = "0.3.0-alpha.1"

android {
    namespace = "org.opengt4.android"
    compileSdk = 35
    ndkVersion = "27.0.12077973"

    defaultConfig {
        applicationId = "org.opengt4.android"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = openGt4Version
        // 64-bit ARM only.
        ndk {
            abiFilters += "arm64-v8a"
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DOPENGT4_ANDROID_VERSION=$openGt4Version")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        debug {
            isJniDebuggable = true
        }
        release {
            isMinifyEnabled = false
            isShrinkResources = false
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
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.documentfile)
    implementation(libs.kotlinx.coroutines.android)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.ui.tooling.preview)
    implementation(libs.compose.material3)
}
