#pragma once
#include <cstdint>
#include <functional>

namespace lsa::runtime {

using NativeHash = uint64_t;

// Context structure mirroring rage::scrNativeCallContext
struct NativeContext {
    void* return_value;
    uint32_t arg_count;
    void* args;
    uint32_t vector_count;
    void* vector_results[4];
    uint8_t inline_buffer[256];
};

void Initialize(void* module_handle);
bool IsReady();
void Invoke(NativeHash hash, NativeContext* ctx);
void SetTick(std::function<void()> callback);
void Shutdown();

} // namespace lsa::runtime
