// SPDX-License-Identifier: MIT
#define main legacy_main
#include "test.cpp"
#undef main
// The real native camera already uploads 528 before the old Adapter enqueue.
// A replacement integration must not append two more uploads at late enqueue.
int main() {
    Fixture f; f.start();
    unsigned native_uploads = 0;
    for (unsigned eye = 0; eye < 2; ++eye) {
        ++native_uploads;
        f.fake.obs.arena.cursor += 576;
        assert(f.adapter.enqueue(f.eye(eye)));
    }
    std::fprintf(stderr, "native camera uploads=%u, late Adapter uploads=%u\n",
                 native_uploads, f.fake.uploads);
    assert(native_uploads + f.fake.uploads == 2);
}
