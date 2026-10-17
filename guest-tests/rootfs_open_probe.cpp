// Standalone Linux/Android regression probe; no VEXA runtime or guest needed.
// Compile with -I<FEX>/Source/Tools/LinuxEmulation and run in a writable TMPDIR.
#include "LinuxSyscalls/AndroidRootFSOpen.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>

static void require(bool ok, const char* name) {
  if (!ok) { std::fprintf(stderr, "ROOTFS_OPEN_FAIL %s errno=%d\n", name, errno); std::exit(1); }
}
int main() {
  auto tmp = std::filesystem::temp_directory_path() / "vexa-rootfs-open-XXXXXX";
  auto text = tmp.string();
  require(mkdtemp(text.data()) != nullptr, "mkdtemp");
  const std::filesystem::path root(text);
  std::filesystem::create_directories(root / "lib");
  std::filesystem::create_directories(root / "lib64");
  FILE* file = std::fopen((root / "lib/loader").c_str(), "w");
  require(file != nullptr, "fixture"); std::fputs("guest-loader", file); std::fclose(file);
  require(symlink("/lib/loader", (root / "lib64/loader").c_str()) == 0, "absolute fixture");
  require(symlink("/lib64", (root / "alias").c_str()) == 0, "intermediate fixture");
  require(symlink("../../lib/loader", (root / "lib64/relative").c_str()) == 0, "relative fixture");
  require(symlink("/loop", (root / "loop").c_str()) == 0, "loop fixture");
  require(symlink("/lib/loader/", (root / "trailing").c_str()) == 0, "trailing fixture");
  const int dir = open(root.c_str(), O_PATH | O_DIRECTORY);
  require(dir >= 0, "root fd");
  for (const char* path : {"lib64/loader", "/alias/loader", "lib64/relative", "/../../lib/loader"}) {
    const int fd = FEX::HLE::AndroidOpenInRoot(dir, path, O_RDONLY, 0);
    require(fd >= 0, path);
    char data[16] {}; require(read(fd, data, sizeof(data)) == 12, "read loader");
    require(std::string(data) == "guest-loader", "loader contents"); close(fd);
  }
  require(FEX::HLE::AndroidOpenInRoot(dir, "loop", O_RDONLY, 0) == -1 && errno == ELOOP, "symlink limit");
  require(FEX::HLE::AndroidOpenInRoot(dir, "lib64/loader", O_RDONLY | O_NOFOLLOW, 0) == -1 && errno == ELOOP, "nofollow");
  require(FEX::HLE::AndroidOpenInRoot(dir, "lib64/loader", O_WRONLY | O_CREAT | O_EXCL, 0600) == -1 && errno == EEXIST, "exclusive");
  require(FEX::HLE::AndroidOpenInRoot(dir, "trailing", O_RDONLY, 0) == -1 && errno == ENOTDIR, "link trailing slash");
  int fd = FEX::HLE::AndroidOpenInRoot(dir, "lib64/loader", O_PATH | O_NOFOLLOW, 0);
  struct stat st {}; require(fd >= 0 && fstat(fd, &st) == 0 && S_ISLNK(st.st_mode), "O_PATH nofollow"); close(fd);
  fd = FEX::HLE::AndroidOpenInRoot(dir, "new", O_WRONLY | O_CREAT | O_EXCL, 0600);
  require(fd >= 0 && std::filesystem::exists(root / "new"), "create within root"); close(fd);
  close(dir); std::filesystem::remove_all(root);
  std::puts("ROOTFS_OPEN_PASS absolute-link intermediate-link relative-link root-clamp ELOOP O_NOFOLLOW O_EXCL trailing-slash create");
}
