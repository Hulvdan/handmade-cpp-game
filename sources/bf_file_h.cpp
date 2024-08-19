struct Debug_Load_File_Result {
    bool success;
    u8*  output;
    u32  size;
};

Debug_Load_File_Result
Debug_Load_File(const char* filename, u8* output, size_t output_max_bytes);

Debug_Load_File_Result Debug_Load_File_To_Arena(const char* filename, Arena& arena, MCTX);
