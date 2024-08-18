Debug_Load_File_Result
Debug_Load_File(const char* filename, u8* output, size_t output_max_bytes) {
    auto&       logger_data = global_library_integration_data->logger_data;
    const auto& logger_routine
        = (Logger_function_t)global_library_integration_data->logger_routine;
    const auto& logger_scope_routine
        = (Logger_Scope_function_t)global_library_integration_data->logger_scope_routine;

    LOG_INFO("Debug_Load_File %s...", filename);

    char absolute_file_path[2048];

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
        LOG_INFO("Debug_Load_File %s... Success!", filename);
    }
    else {
        LOG_WARN("Debug_Load_File %s... Failed!", filename);
    }

    auto close_result = fclose(file);
    Assert(close_result == 0);
    return res;
}

Debug_Load_File_Result Debug_Load_File_To_Arena(const char* filename, Arena& arena) {
    auto res
        = Debug_Load_File(filename, arena.base + arena.used, arena.size - arena.used);
    res.output = Allocate_Array(arena, u8, res.size);
    return res;
}
