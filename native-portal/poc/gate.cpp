// Launch-only Aurora scene gate. No Vulkan exports.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <link.h>
#include <openssl/evp.h>
#include <sys/mman.h>
#include <unistd.h>
namespace {
bool pin_executable() {
  FILE *f = fopen("/proc/self/exe", "rb");
  if (!f)
    return false;
  auto *ctx = EVP_MD_CTX_new();
  bool ok = ctx && EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1;
  unsigned char buffer[65536], digest[32];
  size_t n;
  unsigned size = 0;
  while (ok && (n = fread(buffer, 1, sizeof buffer, f)))
    ok = EVP_DigestUpdate(ctx, buffer, n) == 1;
  ok = ok && !ferror(f) && EVP_DigestFinal_ex(ctx, digest, &size) == 1 &&
       size == 32;
  fclose(f);
  EVP_MD_CTX_free(ctx);
  char hex[65]{};
  if (ok)
    for (unsigned i = 0; i < 32; i++)
      snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return ok && !strcmp(hex, "9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca"
                            "4be0784bf6c265");
}
int gate(dl_phdr_info *info, size_t, void *) {
  if (info->dlpi_name && *info->dlpi_name)
    return 0;
  auto *word = reinterpret_cast<uint32_t *>(info->dlpi_addr + 0x9f404);
  bool executable = false;
  for (unsigned i = 0; i < info->dlpi_phnum; i++) {
    const auto &p = info->dlpi_phdr[i];
    if (p.p_type == PT_LOAD && (p.p_flags & PF_X) && p.p_vaddr <= 0x9f404 &&
        p.p_vaddr + p.p_memsz >= 0x9f408)
      executable = true;
  }
  if (!executable || *word != 0x34003577) {
    fprintf(stderr, "ft-poc: Aurora gate original mismatch\n");
    return 1;
  }
  long ps = sysconf(_SC_PAGESIZE);
  if (ps <= 0)
    return 1;
  auto *page = reinterpret_cast<void *>(uintptr_t(word) & ~(uintptr_t(ps) - 1));
  if (mprotect(page, size_t(ps), PROT_READ | PROT_WRITE)) {
    perror("ft-poc gate mprotect");
    return 1;
  }
  __atomic_store_n(word, 0xd503201f, __ATOMIC_RELEASE);
  __builtin___clear_cache(reinterpret_cast<char *>(word),
                          reinterpret_cast<char *>(word + 1));
  if (mprotect(page, size_t(ps), PROT_READ | PROT_EXEC))
    _exit(190);
  fprintf(stderr, "ft-poc: Aurora gate patched\n");
  return 1;
}
__attribute__((constructor)) void launch_gate() {
  const char *value = getenv("FT_POC_AURORA");
  if (!value || strcmp(value, "1"))
    return;
  // LD_PRELOAD is inherited by launch helpers. Ignore those before hashing.
  char executable[4096];
  ssize_t length =
      readlink("/proc/self/exe", executable, sizeof(executable) - 1);
  if (length <= 0 || size_t(length) >= sizeof(executable) - 1)
    return;
  executable[length] = 0;
  const char *base = strrchr(executable, '/');
  if (!base || strcmp(base + 1, "vrcompositor"))
    return;
  DIR *dir = opendir("/proc/self/task");
  if (!dir)
    return;
  unsigned threads = 0;
  while (auto *entry = readdir(dir))
    if (entry->d_name[0] != '.')
      threads++;
  closedir(dir);
  if (threads != 1) {
    fprintf(stderr, "ft-poc: Aurora gate requires single-threaded preload\n");
    return;
  }
  if (!pin_executable()) {
    fprintf(stderr, "ft-poc: Aurora gate executable pin rejected\n");
    return;
  }
  dl_iterate_phdr(gate, nullptr);
}
} // namespace
