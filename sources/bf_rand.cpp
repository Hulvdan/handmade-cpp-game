// [0; 1]
f32 frand() {
    auto result = (f32)rand() / RAND_MAX;
    Assert(result >= 0);
    Assert(result <= 1);
    return result;
}

void Fill_Perlin_2D(
    u16*          output,
    size_t        free_output_space,
    Arena&        trash_arena,
    Perlin_Params params,
    u16           sx_,
    u16           sy_
) {
    auto sx = (u32)sx_;
    auto sy = (u32)sy_;

    TEMP_USAGE(trash_arena);

    auto octaves = params.octaves;

    u32 sx_power = 0;
    u32 sy_power = 0;
    Assert(sx > 0);
    Assert(sx <= u16_max);
    Assert(sy > 0);
    Assert(sy <= u16_max);
    Assert(sx == sy);
    Assert(Is_Power_Of_2(sx, sx_power));
    Assert(Is_Power_Of_2(sy, sy_power));

    auto total_pixels = (u32)sx * sy;
    f32* cover        = Allocate_Array(trash_arena, f32, total_pixels);
    f32* accumulator  = Allocate_Array(trash_arena, f32, total_pixels);

    srand(params.seed);
    FOR_RANGE (u64, i, (u64)total_pixels) {
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

                *(accumulator + (ptrdiff_t)(sx * y + x)) += value;
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
            auto t = (*(accumulator + (ptrdiff_t)(y * sy + x))) / sum_of_division;
            Assert(t <= 1.0f);
            Assert(t >= 0);

            u16 value = (u16)((f32)u16_max * t);

            *(output + (ptrdiff_t)(y * sx + x)) = value;
        }
    }
}
