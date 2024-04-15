#pragma once

// [0; 1]
f32 frand() { return (f32)rand() / RAND_MAX; }

// Usage:
//     -1 -> false
//     0  -> false
//     1  -> false
//     2  -> true
//     3  -> false
//     4  -> true
bool Is_Multiple_Of_2(int number) {
    if (number < 2)
        return false;

    while (number > 1) {
        if (number & 1)
            return false;

        number >>= 1;
    }

    return true;
}

bool Is_Multiple_Of_2(int number, u8& power) {
    if (number < 2)
        return false;

    power = 0;
    while (number > 1) {
        if (number & 1)
            return false;

        number >>= 1;
        power++;
    }

    return true;
}

void Fill_Perlin_1D(
    u16*   output,
    u8*    temp_storage,
    size_t free_temp_storage_space,
    u8     octaves,
    f32    scaling_bias,
    uint   seed,
    u16    sx
) {
    Assert(free_temp_storage_space >= 2 * sizeof(f32) * sx);

    u8 sx_power;
    u8 sy_power;
    Assert(sx > 0);
    Assert(sx <= u16_max);
    Assert(Is_Multiple_Of_2(sx, sx_power));

    f32* cover       = (f32*)temp_storage;
    f32* accumulator = cover + sx;

    srand(seed);
    FOR_RANGE (size_t, i, sx) {
        *(cover + i)       = frand();
        *(accumulator + i) = 0;
    }

    f32 sum_of_division = 0;
    octaves             = MIN(sx_power, octaves);

    f32 iteration = 1;
    u16 offset    = sx;

    f32 octave_c = 1.0f;
    FOR_RANGE (u8, _, octaves) {
        sum_of_division += octave_c;

        f32 l      = *(cover + 0);
        u16 rindex = offset % sx;
        f32 r      = *(cover + rindex);

        u16 it = 0;
        FOR_RANGE (u16, i, sx) {
            if (it == offset) {
                l      = r;
                rindex = (rindex + offset) % sx;
                r      = *(cover + rindex);
                it     = 0;
            }

            auto value = octave_c * Lerp(l, r, (f32)it / (f32)offset);
            *(accumulator + i) += value;
            it++;
        }

        offset >>= 1;
        octave_c /= scaling_bias;
    }

    FOR_RANGE (u16, x, sx) {
        auto t = (*(accumulator + x)) / sum_of_division;
        Assert(t <= 1.0f);
        Assert(t >= 0);

        u16 value     = u16_max * t;
        *(output + x) = value;
    }
}

#ifdef BF_CLIENT
void Perlin_1D(
    Loaded_Texture& texture,
    u8*             temp_storage,
    size_t          free_temp_storage_space,
    u8              octaves,
    f32             scaling_bias,
    uint            seed  //
) {
    auto sx = texture.size.x;
    auto sy = texture.size.y;
    Assert(free_temp_storage_space >= 2 * sizeof(f32) * sx);

    u8 sx_power;
    u8 sy_power;
    Assert(sx > 0);
    Assert(sx <= u16_max);
    Assert(sy > 0);
    Assert(sy <= u16_max);
    Assert(sx == sy);
    Assert(Is_Multiple_Of_2(sx, sx_power));
    Assert(Is_Multiple_Of_2(sy, sy_power));

    f32* cover       = (f32*)temp_storage;
    f32* accumulator = cover + sx;

    srand(seed);
    FOR_RANGE (size_t, i, sx) {
        *(cover + i)       = frand();
        *(accumulator + i) = 0;
    }

    f32 sum_of_division = 0;
    octaves             = MIN(sx_power, octaves);

    f32 iteration = 1;
    u16 offset    = sx;

    f32 octave_c = 1.0f;
    FOR_RANGE (int, _, octaves) {
        sum_of_division += octave_c;

        f32 l      = *(cover + 0);
        u16 rindex = offset % sx;
        f32 r      = *(cover + rindex);

        u16 it = 0;
        FOR_RANGE (u16, i, sx) {
            if (it == offset) {
                l      = r;
                rindex = (rindex + offset) % sx;
                r      = *(cover + rindex);
                it     = 0;
            }

            auto value = octave_c * Lerp(l, r, (f32)it / (f32)offset);
            *(accumulator + i) += value;
            it++;
        }

        offset >>= 1;
        octave_c /= scaling_bias;
    }

    auto pixel = (u32*)texture.base;
    FOR_RANGE (int, y, sy) {
        FOR_RANGE (int, x, sx) {
            auto t = (*(accumulator + x)) / sum_of_division;
            Assert(t <= 1.0f);
            Assert(t >= 0);
            u8 value = u8_max * t;

            u32 r, g, b;
            b = value;
            g = value;
            r = value;

            *(pixel + y * sx + x) = b + (g << 8) + (r << 16) + (255 << 24);
        }
    }
}
#endif

void Fill_Perlin_2D(
    u16*          output,
    size_t        free_output_space,
    Arena&        trash_arena,
    Perlin_Params params,
    u16           sx,
    u16           sy  //
) {
    TEMPORARY_USAGE(trash_arena);

    auto octaves = params.octaves;

    u8 sx_power;
    u8 sy_power;
    Assert(sx > 0);
    Assert(sx <= u16_max);
    Assert(sy > 0);
    Assert(sy <= u16_max);
    Assert(sx == sy);
    Assert(Is_Multiple_Of_2(sx, sx_power));
    Assert(Is_Multiple_Of_2(sy, sy_power));

    auto total_pixels = (size_t)sx * sy;
    f32* cover        = Allocate_Array(trash_arena, f32, total_pixels);
    f32* accumulator  = Allocate_Array(trash_arena, f32, total_pixels);

    srand(params.seed);
    FOR_RANGE (size_t, i, total_pixels) {
        *(cover + i)       = frand();
        *(accumulator + i) = 0;
    }

    f32 sum_of_division = 0;
    octaves             = MIN(sx_power, octaves);

    u16 offset = sx;

    auto octave_c = 1.0f;
    FOR_RANGE (int, _, octaves) {
        sum_of_division += octave_c;

        u16 x0_index = 0;
        u16 x1_index = offset % sx;
        u16 y0_index = 0;
        u16 y1_index = offset % sy;

        u16 yit = 0;
        u16 xit = 0;
        FOR_RANGE (u16, y, sy) {
            auto y0s = sx * y0_index;
            auto y1s = sx * y1_index;

            FOR_RANGE (u16, x, sx) {
                if (xit == offset) {
                    x0_index = x1_index;
                    x1_index = (x1_index + offset) % sx;
                    xit      = 0;
                }

                auto a0 = *(cover + y0s + x0_index);
                auto a1 = *(cover + y0s + x1_index);
                auto a2 = *(cover + y1s + x0_index);
                auto a3 = *(cover + y1s + x1_index);

                auto xb      = (f32)xit / (f32)offset;
                auto yb      = (f32)yit / (f32)offset;
                auto blend01 = Lerp(a0, a1, xb);
                auto blend23 = Lerp(a2, a3, xb);
                auto value   = octave_c * Lerp(blend01, blend23, yb);

                *(accumulator + sx * y + x) += value;
                xit++;
            }

            yit++;
            if (yit == offset) {
                y0_index = y1_index;
                y1_index = (y1_index + offset) % sy;
                yit      = 0;
            }
        }

        offset >>= 1;
        octave_c /= params.scaling_bias;
    }

    FOR_RANGE (int, y, sy) {
        FOR_RANGE (int, x, sx) {
            auto t = (*(accumulator + y * sy + x)) / sum_of_division;
            Assert(t <= 1.0f);
            Assert(t >= 0);

            u16 value = u16_max * t;

            *(output + y * sx + x) = value;
        }
    }
}

#ifdef BF_CLIENT
void Perlin_2D(
    Loaded_Texture& texture,
    u8*             temp_storage,
    size_t          free_temp_storage_space,
    u8              octaves,
    f32             scaling_bias,
    uint            seed  //
) {
    auto sx = texture.size.x;
    auto sy = texture.size.y;
    Assert(free_temp_storage_space >= 2 * sizeof(f32) * sx * sy);

    u8 sx_power;
    u8 sy_power;
    Assert(sx > 0);
    Assert(sx <= u16_max);
    Assert(sy > 0);
    Assert(sy <= u16_max);
    Assert(sx == sy);
    Assert(Is_Multiple_Of_2(sx, sx_power));
    Assert(Is_Multiple_Of_2(sy, sy_power));

    f32* cover       = (f32*)temp_storage;
    f32* accumulator = cover + sx * sy;

    srand(seed);
    FOR_RANGE (size_t, i, sx * sy) {
        *(cover + i)       = frand();
        *(accumulator + i) = 0;
    }

    f32 sum_of_division = 0;
    octaves             = MIN(sx_power, octaves);

    f32 iteration = 1;
    u16 offset    = sx;

    f32 octave_c = 1.0f;
    FOR_RANGE (int, _, octaves) {
        sum_of_division += octave_c;

        u16 x0_index = 0;
        u16 x1_index = offset % sx;
        u16 y0_index = 0;
        u16 y1_index = offset % sy;

        u16 yit = 0;
        u16 xit = 0;
        FOR_RANGE (u16, y, sy) {
            auto y0s = sx * y0_index;
            auto y1s = sx * y1_index;

            FOR_RANGE (u16, x, sx) {
                if (xit == offset) {
                    x0_index = x1_index;
                    x1_index = (x1_index + offset) % sx;
                    xit      = 0;
                }

                auto a0 = *(cover + y0s + x0_index);
                auto a1 = *(cover + y0s + x1_index);
                auto a2 = *(cover + y1s + x0_index);
                auto a3 = *(cover + y1s + x1_index);

                auto xb      = (f32)xit / (f32)offset;
                auto yb      = (f32)yit / (f32)offset;
                auto blend01 = Lerp(a0, a1, xb);
                auto blend23 = Lerp(a2, a3, xb);
                auto value   = octave_c * Lerp(blend01, blend23, yb);

                *(accumulator + sx * y + x) += value;
                xit++;
            }

            yit++;
            if (yit == offset) {
                y0_index = y1_index;
                y1_index = (y1_index + offset) % sy;
                yit      = 0;
            }
        }

        offset >>= 1;
        octave_c /= scaling_bias;
    }

    auto pixel = (u32*)texture.base;
    FOR_RANGE (int, y, sy) {
        FOR_RANGE (int, x, sx) {
            auto t = (*(accumulator + y * sy + x)) / sum_of_division;
            Assert(t <= 1.0f);
            Assert(t >= 0);
            u8 value = u8_max * t;

            u32 r, g, b;
            b = value;
            g = value;
            r = value;

            *(pixel + y * sx + x) = b + (g << 8) + (r << 16) + (255 << 24);
        }
    }
}
#endif
