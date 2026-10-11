import java.util.Properties

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}


val thirdPartyDir = rootProject.file("third_party")

val shadercDir = thirdPartyDir.resolve("shaderc")
val rapidJsonDir = rootProject.file("third_party/rapidjson/include/rapidjson")
val spirvCrossDir = thirdPartyDir.resolve("spirv-cross")
val sdl3Prefix = thirdPartyDir.resolve("install-android-arm64/sdl3")
val sdl3ImagePrefix = thirdPartyDir.resolve("install-android-arm64/sdl3_image")
val sdl2Prefix = thirdPartyDir.resolve("SDL2")

val shadercCommit = "1d234d34d43cf5ade135803f7777484eaa48e27f"
val spirvCrossCommit = "bf6bb5ce3c9fb141127a7d3289ebaaef86d3b065"
val sdl3Commit = "48045fae0bb210f1828f1304022d5fb88986d736"
val sdl3ImageCommit = "d78dd65454f442bfdbc373201165f1ef579484ab"
val sdl2Commit = "5d249570393f7a37e037abf22cd6012a4cc56a71"
val isWindowsHost = System.getProperty("os.name").lowercase().contains("windows")

fun localProperty(name: String): String? {
    val f = rootProject.file("local.properties")
    if (!f.exists()) return null
    val props = Properties()
    f.inputStream().use { props.load(it) }
    return props.getProperty(name)?.takeIf { it.isNotBlank() }
}

fun cmakePath(path: String): String = path.replace("\\", "/")

val sdkDirPath: String = localProperty("sdk.dir")
    ?: System.getenv("ANDROID_SDK_ROOT")
    ?: System.getenv("ANDROID_HOME")
    ?: ""

val fexRoot = providers.provider {
    val prop = (findProperty("FEX_ROOT") as? String)?.takeIf { it.isNotBlank() }
    val env = System.getenv("FEX_ROOT")?.takeIf { it.isNotBlank() }
    val linux = "/home/critical/FEX"
    val sibling = rootProject.file("../FEX").absolutePath
    when {
        prop != null -> prop
        env != null -> env
        file(linux).exists() -> linux
        else -> sibling
    }
}
val fexBuildDir = providers.provider {
    val prop = (findProperty("FEX_BUILD") as? String)?.takeIf { it.isNotBlank() }
    val env = System.getenv("FEX_BUILD")?.takeIf { it.isNotBlank() }
    when {
        prop != null -> prop
        env != null -> env
        file("/home/critical/FEX").exists() -> "/home/critical/FEX/build-android-arm64-ninja"
        else -> File(fexRoot.get(), "build-android-arm64-ninja").absolutePath
    }
}
val ndkToolchain = providers.provider {
    // NDK 28 and below have libc++ std::atomic_ref disabled in headers.
    // FEX/LinuxEmulation paths include SHMStats.h, which relies on std::atomic_ref.
    // NDK 29 re-enables this, so it is required.
    val env = System.getenv("NDK_TOOLCHAIN")?.takeIf { it.isNotBlank() }
    val linux = "/home/critical/Android/Sdk/ndk/29.0.14206865/build/cmake/android.toolchain.cmake"
    val sdkNdk = if (sdkDirPath.isNotBlank()) {
        File(sdkDirPath, "ndk/29.0.14206865/build/cmake/android.toolchain.cmake").absolutePath
    } else {
        ""
    }
    when {
        env != null -> env
        file(linux).exists() -> linux
        sdkNdk.isNotBlank() && file(sdkNdk).exists() -> sdkNdk
        else -> linux
    }
}
val hostCmake = providers.provider {
    val name = if (isWindowsHost) "cmake.exe" else "cmake"
    val bundled = if (sdkDirPath.isNotBlank()) File(sdkDirPath, "cmake/3.22.1/bin/$name") else null
    if (bundled != null && bundled.exists()) bundled.absolutePath else "cmake"
}
val hostNinja = providers.provider {
    val name = if (isWindowsHost) "ninja.exe" else "ninja"
    val bundled = if (sdkDirPath.isNotBlank()) File(sdkDirPath, "cmake/3.22.1/bin/$name") else null
    if (bundled != null && bundled.exists()) bundled.absolutePath else "ninja"
}
val pythonExe: String =
    (findProperty("PYTHON") as? String)?.takeIf { it.isNotBlank() }
        ?: System.getenv("PYTHON")?.takeIf { it.isNotBlank() }
        ?: if (isWindowsHost) "python" else "python3"
val buildThunks =
    providers.environmentVariable("BUILD_THUNKS").orElse("ON")
val enableRuntimeShaderTranslator =
    providers.environmentVariable("VEXA_ENABLE_RUNTIME_SHADER_TRANSLATOR").orElse("OFF")

fun hostToolPath(): String {
    val extras = mutableListOf<String>()
    val hostTools = File(fexRoot.get(), ".hosttools")
    if (hostTools.exists()) extras += hostTools.absolutePath
    val gitUsr = File("C:/Program Files/Git/usr/bin")
    if (gitUsr.exists()) extras += gitUsr.absolutePath
    val current = System.getenv("Path") ?: System.getenv("PATH") ?: ""
    return (extras + current).filter { it.isNotBlank() }.joinToString(File.pathSeparator)
}

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
            if (!dir.exists()) return false
            return try {
                gitStatusPorcelain(dir).isNotEmpty()
            } catch (e: Exception) {
                false
            }
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

        // shaderc is only required when the GL host translates desktop GLSL at runtime.
        if (buildThunks.get() == "ON" && enableRuntimeShaderTranslator.get() == "ON") {
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
                pythonExe,
                shadercDir.resolve("utils/git-sync-deps").absolutePath,
                cwd = rootProject.rootDir
            )
            ensureRepo(
                "spirv-cross",
                "https://github.com/KhronosGroup/SPIRV-Cross.git",
                spirvCrossCommit
            )
        }

        if (buildThunks.get() == "ON") {
            ensureRepo(
                "SDL",
                "https://github.com/libsdl-org/SDL.git",
                sdl3Commit
            )
            ensureRepo(
                "SDL2",
                "https://github.com/libsdl-org/SDL.git",
                sdl2Commit
            )
            ensureRepo(
                "SDL_image",
                "https://github.com/libsdl-org/SDL_image.git",
                sdl3ImageCommit
            )
        }
    }
}

fun hostBuildPath(): String {
    val cmakeDir = File(hostCmake.get()).parentFile?.absolutePath.orEmpty()
    return listOf(cmakeDir, hostToolPath())
        .filter { it.isNotBlank() }
        .joinToString(File.pathSeparator)
}

val buildAndroidSDL3 by tasks.registering(org.gradle.api.tasks.Exec::class) {
    group = "build setup"
    description = "Builds the Android SDL3 runtime used by the SDL3 host thunk."
    onlyIf { buildThunks.get() == "ON" }
    dependsOn(ensureThirdPartySources)
    workingDir = rootProject.rootDir
    environment("Path", hostBuildPath())
    environment("PATH", hostBuildPath())
    environment("NDK_TOOLCHAIN", cmakePath(ndkToolchain.get()))
    commandLine(
        if (isWindowsHost) "C:/Program Files/Git/bin/bash.exe" else "bash",
        rootProject.file("scripts/build_android_sdl3.sh").absolutePath
    )
}

val buildAndroidSDL3Image by tasks.registering(org.gradle.api.tasks.Exec::class) {
    group = "build setup"
    description = "Builds SDL3_image for the Android SDL3 host thunk."
    onlyIf { buildThunks.get() == "ON" }
    dependsOn(buildAndroidSDL3)
    workingDir = rootProject.rootDir
    environment("Path", hostBuildPath())
    environment("PATH", hostBuildPath())
    environment("NDK_TOOLCHAIN", cmakePath(ndkToolchain.get()))
    environment("SDL3_PREFIX", cmakePath(sdl3Prefix.absolutePath))
    commandLine(
        if (isWindowsHost) "C:/Program Files/Git/bin/bash.exe" else "bash",
        rootProject.file("scripts/build_android_sdl3_image.sh").absolutePath
    )
}

val configureFexAndroid by
tasks.registering(org.gradle.api.tasks.Exec::class) {
    group = "build setup"
    description = "Configures FEX Android build."
    dependsOn(buildAndroidSDL3Image)
    workingDir = rootProject.rootDir
    val pkgConfig = File(fexRoot.get(), ".hosttools/pkg-config.cmd")
    val hostPath = hostToolPath()
    environment("Path", hostPath)
    environment("PATH", hostPath)
    val configureArgs = mutableListOf(
        hostCmake.get(),
        "-S", fexRoot.get(),
        "-B", fexBuildDir.get(),
        "-G", "Ninja",
        "-DCMAKE_MAKE_PROGRAM=${hostNinja.get()}",
        "-DCMAKE_TOOLCHAIN_FILE=${ndkToolchain.get()}",
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
        "-DTUNE_CPU=none",
        "-DPKG_CONFIG_EXECUTABLE=${pkgConfig.absolutePath}",
        "-DPython_EXECUTABLE=$pythonExe",
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
    if (buildThunks.get() == "ON") {
        val ndkPrebuilt = File(sdkDirPath, "ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64")
        val guestRootfs = rootProject.file("guest-tests/graphics/rootfs").absolutePath
        configureArgs += listOf(
            "-DTHUNKGEN_EXECUTABLE=${cmakePath(File(fexRoot.get(), "build-thunkgen-host/thunkgen.exe").absolutePath)}",
            "-DX86_DEV_ROOTFS=${cmakePath(guestRootfs)}",
            "-DENABLE_CLANG_THUNKS=ON",
            "-DENABLE_DESKTOP_GL_THUNKS=ON",
            "-DENABLE_EGL_THUNKS=ON",
            "-DENABLE_VULKAN_THUNKS=ON",
            "-DENABLE_WAYLAND_THUNKS=OFF",
            "-DENABLE_VDSO_THUNKS=OFF",
            "-DVEXA_ENABLE_RUNTIME_SHADER_TRANSLATOR=${enableRuntimeShaderTranslator.get()}",
            "-DVEXA_GUEST_CC=${cmakePath(File(ndkPrebuilt, "bin/clang.exe").absolutePath)}",
            "-DVEXA_GUEST_CXX=${cmakePath(File(ndkPrebuilt, "bin/clang++.exe").absolutePath)}",
            "-DANDROID_NDK_SYSROOT=${cmakePath(File(ndkPrebuilt, "sysroot").absolutePath)}",
            "-DVEXA_SDL3_PREFIX=${cmakePath(sdl3Prefix.absolutePath)}",
            "-DVEXA_SDL2_PREFIX=${cmakePath(sdl2Prefix.absolutePath)}",
            "-DVEXA_SDL3_IMAGE_PREFIX=${cmakePath(sdl3ImagePrefix.absolutePath)}",
            "-DVEXA_OPENAL_PREFIX=${cmakePath(rootProject.file("third_party/openal-soft-android").absolutePath)}"
        )
    }
    commandLine(configureArgs)
}

val buildFEXCore = tasks.register<org.gradle.api.tasks.Exec>("buildFEXCore") {
    group = "build setup"
    description = "Builds libFEXCore.so"
    dependsOn(configureFexAndroid)
    workingDir = rootProject.rootDir
    val hostPath = hostToolPath()
    environment("Path", hostPath)
    environment("PATH", hostPath)
    val buildPath = fexBuildDir.get()
    val buildArgs = mutableListOf(
        hostCmake.get(),
        "--build", buildPath,
        "--target", "FEXCore_shared",
        "--target", "FEXCore",
        "--target", "FEXCore_Base",
        "--target", "JemallocLibs",
        "--target", "Common",
        "--target", "CommonTools",
        "--target", "LinuxEmulation",
        "--target", "fmt",
        "--target", "xxhash",
        "--target", "cephes_128bit",
        "--target", "softfloat_3e",
        "--target", "tiny-json"
    )
    if (buildThunks.get() == "ON") {
        buildArgs += listOf(
            "--target", "GL-host-64",
            "--target", "EGL-host-64",
            "--target", "GLESv2-host-64",
            "--target", "vulkan-host-64",
            "--target", "openal-host-64",
            "--target", "SDL3-host-64",
            "--target", "SDL3_image-host-64",
            "--target", "SDL2-host-64",
            "--target", "guest-libs"
        )
    }
    buildArgs += "-j"
    commandLine(buildArgs)
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
                arguments += listOf(
                    // Host thunks and FEX are built against c++_shared; this also packages libc++_shared.so.
                    "-DANDROID_STL=c++_shared",
                    "-DFEX_ROOT=${cmakePath(fexRoot.get())}",
                    "-DFEX_BUILD=${cmakePath(fexBuildDir.get())}",
                )
            }
        }
    }

    packaging {
        jniLibs {
            keepDebugSymbols += "**/*.so"
            /* wrap.sh is only honored for a debuggable APK, and the preload
             * .so has to be a real file next to it. Debug builds extract libs. */
            useLegacyPackaging = true
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

val stageGuestRuntime by tasks.registering(Exec::class) {
    group = "build setup"
    description = "Re-stages the full guest rootfs, graphics programs, and thunk libraries."
    dependsOn(buildFEXCore)
    environment("REQUIRE_THUNKS", if (buildThunks.get() == "ON") "1" else "0")
    commandLine(
        pythonExe,
        rootProject.file("guest-tests/graphics/stage_runtime_assets.py").absolutePath,
    )
}

val stageSDL3Thunks by tasks.registering(Exec::class) {
    group = "build setup"
    description = "Adds SDL3 thunk payloads without replacing existing runtime assets."
    dependsOn(buildFEXCore)
    onlyIf { buildThunks.get() == "ON" }
    commandLine(
        pythonExe,
        rootProject.file("guest-tests/graphics/stage_sdl3_thunks.py").absolutePath,
    )
}

tasks.matching {
    it.name == "mergeDebugAssets" || it.name == "mergeReleaseAssets"
}.configureEach {
    dependsOn(stageSDL3Thunks)
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation("com.google.android.material:material:1.12.0")
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.graphics)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)
    // Java port of SteamKit. The Android RSA path needs BouncyCastle by name;
    // coroutines-jdk8 is what JavaSteam uses to bridge its callback futures.
    implementation("in.dragonbra:javasteam:1.8.0")
    implementation("com.google.protobuf:protobuf-java:4.31.1")
    implementation("org.bouncycastle:bcprov-jdk18on:1.78.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-jdk8:1.10.2")
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.compose.ui.test.junit4)
    debugImplementation(libs.androidx.compose.ui.tooling)
    debugImplementation(libs.androidx.compose.ui.test.manifest)
}
