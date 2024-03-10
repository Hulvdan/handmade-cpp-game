#pragma once

constexpr u32 EMPTY_HASH32 = 2166136261;

u32 Hash32(const u8* key, const int len) {
    Assert(len >= 0);

    // Wiki: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    u32 hash = EMPTY_HASH32;
    for (int i = 0; i < len; i++) {
        // hash = (hash * 16777619) ^ (*key);  // FNV-1
        hash = (hash ^ (*key)) * 16777619;  // FNV-1a
        key++;
    }

    return hash;
}

u32 Hash32_String(const char* key) {
    u32 hash = EMPTY_HASH32;
    while (*key) {
        // hash = (hash * 16777619) ^ (*key);  // FNV-1
        hash = (hash ^ (*key)) * 16777619;  // FNV-1a
        key++;
    }

    return hash;
}
