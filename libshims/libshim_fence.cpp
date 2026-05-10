// fence_shim.cpp

namespace android {
    extern "C" void _ZN7android5FenceD1Ev(void* thisptr) {
        // android::Fence::~Fence() shim
        // No-op destructor
    }
}
