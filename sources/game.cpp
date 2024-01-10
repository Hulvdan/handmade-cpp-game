#include <cassert>

#include "types.cpp"

struct GameBitmap {
    i32 width;
    i32 height;

    i32 bits_per_pixel;
    void* memory;
};

struct GameState {
    f32 offset_x;
    f32 offset_y;
};

struct GameMemory {
    bool is_initialized;
    GameState state;
};

extern "C" void Game_UpdateAndRender(void* memory_, GameBitmap& bitmap)
{
    auto& memory = *((GameMemory*)memory_);
    auto& state = memory.state;

    if (!memory.is_initialized) {
        state.offset_x = 0;
        state.offset_y = 0;
        memory.is_initialized = true;
    }

    assert(bitmap.bits_per_pixel == 4);
    auto pixel = (u32*)bitmap.memory;

    auto offset_x = (i32)state.offset_x;
    auto offset_y = (i32)state.offset_y;

    for (int y = 0; y < bitmap.height; y++) {
        for (int x = 0; x < bitmap.width; x++) {
            // u32 red  = 0;
            u32 red = (u8)(x + offset_x);
            // u32 red  = (u8)(y + offset_y);
            u32 green = 0;
            // u32 green = (u8)(x + offset_x);
            // u32 green = (u8)(y + offset_y);
            // u32 blue = 0;
            // u32 blue = (u8)(x + offset_x);
            u32 blue = (u8)(y + offset_y);
            (*pixel++) = (blue) | (green << 8) | (red << 16);
        }
    }
}
