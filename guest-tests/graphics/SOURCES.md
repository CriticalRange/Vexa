# Graphics test guests

Official Ubuntu amd64 packages, downloaded from archive.ubuntu.com. The `.deb` files are in `debs/`. Extracted files are in `tree/`.

These are x86_64 Linux ELF programs. They are dynamically linked. They are not copied into the APK.

| Package | What it draws |
| --- | --- |
| mesa-utils-bin 9.0.0-2 | `glxgears`, `es2gears_x11`, `eglgears_x11`, `glxinfo`, `vkgears` |
| mesa-utils 9.0.0-2 | short names that point at the binaries above |
| glmark2-x11 and glmark2-es2-x11 2023.01 | OpenGL and OpenGL ES 2 benchmarks |
| glmark2-data 2023.01 | scenes those benchmarks load from `tree/usr/share/glmark2` |
| vulkan-tools 1.4.304.0 | `vkcube`, `vkcubepp`, `vulkaninfo` |

Real binaries confirmed with llvm-readelf: ELF64, x86_64. `glxgears` needs `libGL.so.1` and `libX11.so.6`. `es2gears_x11` needs `libGLESv2.so.2`, `libEGL.so.1`, and `libX11.so.6`. `glmark2` also needs `libjpeg.so.8`, `libpng16.so.16`, and `libstdc++.so.6`, and loads GL at runtime. `vkcube` loads Vulkan at runtime. All of them need a Linux `libc.so.6` and the dynamic linker.

Copyright texts shipped inside the packages are under `tree/usr/share/doc/*/copyright`. Mesa demos are MIT. glmark2 is GPL-3+ with Expat and BSD parts in the data. vulkan-tools is Apache-2.0 and MIT.
