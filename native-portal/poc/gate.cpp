// Launch-only Aurora scene gate. No Vulkan exports.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <initializer_list>
#include <link.h>
#include <openssl/evp.h>
#include <sys/mman.h>
#include <unistd.h>
namespace {
alignas(4) uint32_t above_menu = 0;
bool branch(uintptr_t from, uintptr_t to, uint32_t &out) {
  const int64_t delta = int64_t(to) - int64_t(from);
  if ((delta & 3) || delta < -(INT64_C(1) << 27) ||
      delta >= (INT64_C(1) << 27)) return false;
  out = 0x14000000 | (uint32_t(delta / 4) & 0x03ffffff);
  return true;
}
// Only w0 is changed, exactly as by the displaced LDR. No BL, stack change,
// flag change, SIMD instruction, callback or resource access occurs here.
void *order_trampoline(uintptr_t site, size_t page_size, uint32_t &entry) {
  const uintptr_t origin = site & ~(uintptr_t(page_size) - 1);
  for (uintptr_t distance = 0x10000; distance < 0x7000000; distance += 0x10000) {
    for (int direction : {-1, 1}) {
      if (direction < 0 && origin < distance) continue;
      uintptr_t address = direction < 0 ? origin-distance : origin+distance;
      void *memory = mmap(reinterpret_cast<void *>(address), page_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                          -1, 0);
      if (memory == MAP_FAILED) continue;
      auto *code = static_cast<uint32_t *>(memory);
      // Native portal-stencil mode keeps its original selector. Ordinary
      // RoomView selects its existing pre/post-overlay camera paths instead.
      code[0] = 0xb940d3e0; // ldr w0,[sp,#0xd0]
      code[1] = 0x350000a0; // cbnz w0,+20 -> original load at instruction6
      code[2] = 0x580000c0; // ldr x0,+24 -> pointer literal at offset32
      code[3] = 0xb9400000; // ldr w0,[x0]
      code[4] = 0xb9010be0; // str w0,[sp,#0x108]
      code[6] = 0xb9410be0; // displaced ldr w0,[sp,#0x108]
      const uintptr_t pointer = reinterpret_cast<uintptr_t>(&above_menu);
      memcpy(code+8, &pointer, sizeof pointer);
      if (!branch(site, address, entry) ||
          !branch(address+20, site+4, code[5]) ||
          !branch(address+28, site+4, code[7])) {
        munmap(memory,page_size); continue;
      }
      __builtin___clear_cache(reinterpret_cast<char *>(memory),
                              reinterpret_cast<char *>(memory)+40);
      if (mprotect(memory,page_size,PROT_READ|PROT_EXEC)) {
        munmap(memory,page_size); return nullptr;
      }
      return memory;
    }
  }
  return nullptr;
}
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
  auto *order = reinterpret_cast<uint32_t *>(info->dlpi_addr + 0x9fab0);
  bool executable = false;
  for (unsigned i = 0; i < info->dlpi_phnum; i++) {
    const auto &p = info->dlpi_phdr[i];
    if (p.p_type == PT_LOAD && (p.p_flags & PF_X) && p.p_vaddr <= 0x9f404 &&
        p.p_vaddr + p.p_memsz >= 0x9fab4)
      executable = true;
  }
  if (!executable || *word != 0x34003577 || *order != 0xb9410be0 ||
      order[1] != 0x7100001f || order[2] != 0xb940d3e0) {
    fprintf(stderr, "ft-poc: Aurora gate original mismatch\n");
    return 1;
  }
  long ps = sysconf(_SC_PAGESIZE);
  if (ps <= 0 || (ps & (ps-1)))
    return 1;
  auto *page = reinterpret_cast<void *>(uintptr_t(word) & ~(uintptr_t(ps) - 1));
  if ((uintptr_t(order) & ~(uintptr_t(ps)-1)) != uintptr_t(page)) return 1;
  uint32_t order_branch = 0;
  void *trampoline = order_trampoline(uintptr_t(order), size_t(ps), order_branch);
  if (!trampoline) {
    fprintf(stderr,"ft-poc: camera order trampoline unavailable\n");
    return 1;
  }
  if (mprotect(page, size_t(ps), PROT_READ | PROT_WRITE)) {
    munmap(trampoline,size_t(ps));
    perror("ft-poc gate mprotect");
    return 1;
  }
  __atomic_store_n(word, 0xd503201f, __ATOMIC_RELEASE);
  __atomic_store_n(order, order_branch, __ATOMIC_RELEASE);
  __builtin___clear_cache(reinterpret_cast<char *>(word),
                          reinterpret_cast<char *>(order + 1));
  if (mprotect(page, size_t(ps), PROT_READ | PROT_EXEC))
    _exit(190);
  fprintf(stderr, "ft-poc: Aurora gate patched\n");
  fprintf(stderr,"ft-poc: native camera pre/post-overlay selector installed\n");
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
extern "C" __attribute__((visibility("default")))
void ft_poc_set_above_menu(uint32_t enabled) {
  __atomic_store_n(&above_menu, enabled ? 1u : 0u, __ATOMIC_RELAXED);
}
