plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

val rapidJsonDir = rootProject.file("third_party/rapidjson/include/rapidjson")

val ensureThirdPartySources by tasks.registering {
    group = "build setup"
    description = "Ensures required third-party sources are present."

    doLast {
        if (rapidJsonDir.exists()) return@doLast

        logger.lifecycle("RapidJSON not found at ${rapidJsonDir.path}, fetching via git submodule...")

        val gitProcess = ProcessBuilder(
            "git",
            "submodule",
            "update",
            "--init",
            "--depth",
            "1",
            "third_party/rapidjson"
        )
            .directory(rootProject.rootDir)
            .inheritIO()
            .start()
        val gitExitCode = gitProcess.waitFor()

        if (gitExitCode != 0 || !rapidJsonDir.exists()) {
            throw GradleException(
                "Failed to fetch RapidJSON. Run: git submodule update --init --depth 1 third_party/rapidjson"
            )
        }
    }
}

android {
    namespace = "com.critical.vexaemulator"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }

    defaultConfig {
        applicationId = "com.critical.vexaemulator"
        minSdk = 30
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                cppFlags += ""
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    buildFeatures {
        compose = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

tasks.named("preBuild") {
    dependsOn(ensureThirdPartySources)
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.graphics)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.compose.ui.test.junit4)
    debugImplementation(libs.androidx.compose.ui.tooling)
    debugImplementation(libs.androidx.compose.ui.test.manifest)
}
