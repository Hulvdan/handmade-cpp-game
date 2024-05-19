
struct Game_Library {};

void Load_Game_Library(Arena& trash_arena) {
    TEMP_USAGE(trash_arena);

    auto result = Debug_Load_File_To_Arena("gamelib.jsonc", trash_arena);
    Assert(result.success);
}
