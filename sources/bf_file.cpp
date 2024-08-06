
struct Debug_Load_File_Result {
    bool success;
    u8*  output;
    u32  size;
};

Debug_Load_File_Result
Debug_Load_File(const char* filename, u8* output, size_t output_max_bytes) {
    char absolute_file_path[512];
    // TODO: Make the path relative to the executable
    const auto pattern = R"PATH(c:\Users\user\dev\home\handmade-cpp-game\%s)PATH";
    sprintf(absolute_file_path, pattern, filename);
#if WIN32
    for (auto& character : absolute_file_path) {
        if (character == '/')
            character = '\\';
    }
#endif  // WIN32

    FILE* file   = nullptr;
    auto  failed = fopen_s(&file, absolute_file_path, "rb");
    Assert(!failed);

    auto read_bytes = fread((void*)output, 1, output_max_bytes, file);

    Debug_Load_File_Result res{};
    if (feof(file)) {
        res.success = true;
        res.output  = output;
        res.size    = read_bytes;
    }

    Assert(fclose(file) == 0);
    return res;
}

Debug_Load_File_Result Debug_Load_File_To_Arena(const char* filename, Arena& arena) {
    auto res
        = Debug_Load_File(filename, arena.base + arena.used, arena.size - arena.used);
    res.output = Allocate_Array(arena, u8, res.size);
    return res;
}
