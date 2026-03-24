plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}


val thirdPartyDir = rootProject.file("third_party")

val shadercDir = thirdPartyDir.resolve("shaderc")
val rapidJsonDir = rootProject.file("third_party/rapidjson/include/rapidjson")
val spirvCrossDir = thirdPartyDir.resolve("spirv-cross")

val shadercCommit = "1d234d34d43cf5ade135803f7777484eaa48e27f"
val spirvCrossCommit = "bf6bb5ce3c9fb141127a7d3289ebaaef86d3b065"
val fexRoot = providers.environmentVariable("FEX_ROOT").orElse("/home/critical/FEX")
val fexBuildDir = providers.gradleProperty("FEX_BUILD")
    .orElse("/home/critical/FEX/build-android-arm64-ninja")
val ndkToolchain =
    providers.environmentVariable("NDK_TOOLCHAIN")
        // NDK 28 and below have libc++ std::atomic_ref disabled in headers.
        // FEX/LinuxEmulation paths include SHMStats.h, which relies on std::atomic_ref.
        // NDK 29 Re-enables this so it's required.
        .orElse("/home/critical/Android/Sdk/ndk/29.0.14206865/build/cmake/android.toolchain.cmake")
val buildThunks =
    providers.environmentVariable("BUILD_THUNKS").orElse("OFF")

val ensureThirdPartySources by tasks.registering {
    group = "build setup"
    description = "Ensures required third-party sources are present."

    doLast {
        fun run(
            vararg args: String,
            cwd: File = rootProject.rootDir,
            ignoreExit: Boolean = false
        ): Int {
            return providers.exec {
                workingDir = cwd
                commandLine(*args)
                isIgnoreExitValue = ignoreExit
            }.result.get().exitValue
        }

        fun gitStatusPorcelain(dir: File): String {
            val out = providers.exec {
                workingDir = dir
                commandLine(
                    "git",
                    "status",
                    "--porcelain"
                )
            }
            var rc = out.result.get().exitValue

            if (rc != 0) {
                throw GradleException("git status failed in ${dir.absolutePath}")
            }
            return out.standardOutput.asText.get().trim()
        }

        fun isDirtyRepo(dir: File): Boolean {
            val rc = run(
                "bash",
                "-lc",
                "test -z \"$(git -c alias.status= status --porcelain --untracked-files=normal)\"",
                cwd = dir,
                ignoreExit = true
            )
            return rc != 0
        }

        fun ensureRepo(
            name: String,
            url: String,
            commit: String,
            updateSubmodules: Boolean = false
        ) {
            val dir = thirdPartyDir.resolve(name)
            if (!dir.resolve(".git").exists()) {
                run("git", "clone", url, dir.absolutePath)
            }

            if (gitStatusPorcelain(dir).isNotEmpty()) {
                throw GradleException("dirty repo: ${dir.absolutePath}")
            }

            run("git", "fetch", "--force", "origin", commit, cwd = dir, ignoreExit = false)
            run("git", "checkout", "--detach", commit, cwd = dir)

            if (updateSubmodules && dir.resolve(".gitmodules").exists()) {
                run("git", "submodule", "update", "--init", "--recursive", cwd = dir)
            }
        }

        if (!thirdPartyDir.exists()) {
            thirdPartyDir.mkdirs()
        }

        if (isDirtyRepo(rapidJsonDir)) {
            throw GradleException("dirt repo: ${rapidJsonDir.absolutePath}")
        }

        // RapidJSON
        if (!rapidJsonDir.exists()) {
            logger.lifecycle("RapidJSON not found at ${rapidJsonDir.path}, fetching via git submodule...")
            val rc = run(
                "git",
                "submodule",
                "update",
                "--init",
                "--depth",
                "1",
                "third_party/rapidjson"
            )
            if (rc != 0 || !rapidJsonDir.exists()) {
                throw GradleException(
                    "Failed to fetch RapidJSON. Run: git submodule update --init --depth 1 third_party/rapidjson"
                )
            }
        }

        // shaderc + spirv-cross
        if (isDirtyRepo(shadercDir)) {
            throw GradleException("dirt repo: ${shadercDir.absolutePath}")
        }

        if (isDirtyRepo(spirvCrossDir)) {
            throw GradleException("dirt repo: ${spirvCrossDir.absolutePath}")
        }

        ensureRepo(
            "shaderc",
            "https://github.com/google/shaderc.git",
            shadercCommit,
            updateSubmodules = true
        )
        run(
            "python3",
            shadercDir.resolve("utils/git-sync-deps").absolutePath,
            cwd = rootProject.rootDir
        )
        ensureRepo(
            "spirv-cross",
            "https://github.com/KhronosGroup/SPIRV-Cross.git",
            spirvCrossCommit
        )
    }
}

val configureFexAndroid by
tasks.registering(org.gradle.api.tasks.Exec::class) {
    group = "build setup"
    description = "Configures FEX Android build."
    onlyIf { !file("${fexBuildDir.get()}/build.ninja").exists() }
    workingDir = rootProject.rootDir
    commandLine(
        "cmake",
        "-S", fexRoot.get(),
        "-B", fexBuildDir.get(),
        "-G", "Ninja",

        "-DCMAKE_TOOLCHAIN_FILE=${ndkToolchain.get()}",
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
        "-DENABLE_ASSERTIONS=ON",
        "-DANDROID_ABI=arm64-v8a",
        "-DANDROID_PLATFORM=android-30",
        "-DANDROID_STL=c++_shared",
        "-DBUILD_FEXCONFIG=OFF",
        "-DBUILD_FEX_LINUX_TESTS=OFF",
        "-DBUILD_TESTING=OFF",
        "-DBUILD_STEAM_SUPPORT=OFF",
        "-DBUILD_THUNKS=${buildThunks.get()}",
        "-DENABLE_GDB_SYMBOLS=OFF",
        "-DENABLE_LTO=OFF",
        "-DENABLE_CCACHE=OFF",
        "-DENABLE_WERROR=OFF",
        "-DENABLE_JEMALLOC=OFF",
        "-DENABLE_JEMALLOC_GLIBC_ALLOC=OFF"
    )
}

val buildFEXCore = tasks.register<org.gradle.api.tasks.Exec>("buildFEXCore") {
    group = "build setup"
    description = "Builds libFEXCore.so"
    dependsOn(configureFexAndroid)
    workingDir = rootProject.rootDir
    val buildPath = fexBuildDir.get()
    commandLine(
        "cmake",
        "--build", buildPath,
        "--target", "FEXCore",
        "-j"
    )
}

kotlin {
    jvmToolchain(21)
}

android {
    namespace = "com.critical.vexaemulator"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }
    // Must match the toolchain used for FEX configure/build.
    // If AGP resolves NDK 28 here, native compile can fail with:
    // "no member named 'atomic_ref' in namespace 'std'".
    ndkVersion = "29.0.14206865"
    defaultConfig {
        applicationId = "com.critical.vexaemulator"
        minSdk = 30
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters += "arm64-v8a"
        }

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                cppFlags += ""
            }
        }
    }

    packaging {
        jniLibs {
            keepDebugSymbols += "**/*.so"
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
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
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

tasks.named(
    "preBuild",
    org.gradle.api.DefaultTask::class
) {
    dependsOn(ensureThirdPartySources)
    dependsOn(configureFexAndroid)
}

tasks.matching {
    it.name.startsWith("configureCMake")
}.configureEach {
    dependsOn(configureFexAndroid)
}

tasks.matching {
    it.name.startsWith("buildCMake")
}.configureEach {
    dependsOn(buildFEXCore)
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.browser:browser:1.9.0")
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
