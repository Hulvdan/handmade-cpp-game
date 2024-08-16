struct Debug_Load_File_Result {
    bool success;
    u8*  output;
    u32  size;
};

Debug_Load_File_Result
Debug_Load_File(const char* filename, u8* output, size_t output_max_bytes) {
    char absolute_file_path[512];

    const auto path = filename;

    FILE* file   = nullptr;
    auto  failed = fopen_s(&file, path, "rb");
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
