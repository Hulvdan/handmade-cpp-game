#pragma once

struct Debug_LoadFileResult {
    bool success;
    u32 size;
};

Debug_LoadFileResult Debug_LoadFile(const char* filename, u8* output, size_t output_max_bytes)
{
    char absolute_file_path[512];
    // TODO(hulvdan): Make the path relative to the executable
    const auto pattern = R"PATH(c:\Users\user\dev\home\handmade-cpp-game\%s)PATH";
    sprintf(absolute_file_path, pattern, filename);
#if WIN32
    for (auto& character : absolute_file_path) {
        if (character == '/')
            character = '\\';
    }
#endif  // WIN32

    FILE* file = 0;
    auto failed = fopen_s(&file, absolute_file_path, "r");
    assert(!failed);

    auto read_bytes = fread((void*)output, 1, output_max_bytes, file);

    Debug_LoadFileResult result = {};
    if (feof(file)) {
        result.success = true;
        result.size = read_bytes;
    }

    assert(fclose(file) == 0);
    return result;
}

Debug_LoadFileResult Debug_LoadFileToArena(const char* filename, Arena& arena)
{
    return Debug_LoadFile(filename, arena.base + arena.used, arena.size - arena.used);
}