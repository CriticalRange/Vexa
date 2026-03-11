// Top-level build file where you can add configuration options common to all sub-projects/modules.
plugins {
    alias(libs.plugins.android.application) apply true
    alias(libs.plugins.kotlin.compose) apply true
}

kotlin {
    jvmToolchain(11)
}

android {
    compileSdk = 36
    namespace = "com.critical.vexaemulator"
    defaultConfig {
        applicationId = "com.critical.vexaemulator"
        minSdk = 30
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"
        externalNativeBuild {
            cmake {
                cppFlags += ""
                arguments += listOf(
                    "-DFEX_ROOT=/home/critical/FEX",
                    "-DFEX_BUILD=/home/critical/FEX/build-android-arm64-ninja",
                    "-DANDROID_STL=c++_shared"
                )
            }
        }
    }
    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
}
