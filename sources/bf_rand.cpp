// [0; 1]
f32 frand() {
    auto result = (f32)rand() / RAND_MAX;
    Assert(result >= 0);
    Assert(result <= 1);
    return result;
}

struct Perlin_Params {
    int  octaves;
    f32  scaling_bias;
    uint seed;
};

void Cycled_Perlin_2D(
    u16*          output,
    size_t        free_output_size,
    Arena&        trash_arena,
    Perlin_Params params,
    u16           sx_,
    u16           sy_
) {
    auto sx = (u32)sx_;
    auto sy = (u32)sy_;

    Assert(free_output_size >= (size_t)sx * sy);

    u32 sx_power = 0;
    u32 sy_power = 0;

    Assert(sx > 0);
    Assert(sy > 0);
    Assert(sx == sy);
    auto sx_is = Is_Power_Of_2(sx, &sx_power);
    auto sy_is = Is_Power_Of_2(sy, &sy_power);
    Assert(sx_is);
    Assert(sy_is);

    Assert(params.octaves > 0);
    Assert(params.scaling_bias > 0);

    TEMP_USAGE(trash_arena);

    auto octaves = params.octaves;

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

    u32 offset = sx;

    f32 octave_c = 1.0f;
    FOR_RANGE (int, _, octaves) {
        sum_of_division += octave_c;

        u32 x0_index = 0;
        u32 x1_index = offset % sx;
        u32 y0_index = 0;
        u32 y1_index = offset % sy;

        u32 yit = 0;
        u32 xit = 0;
        FOR_RANGE (u32, y, sy) {
            u32 y0s = sx * y0_index;
            u32 y1s = sx * y1_index;

            FOR_RANGE (u32, x, sx) {
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

    FOR_RANGE (u64, y, sy) {
        FOR_RANGE (u64, x, sx) {
            f32 t = *(accumulator + y * sy + x) / sum_of_division;
            Assert(t >= 0);
            Assert(t <= 1.0f);

            u16 value = (u16)((f32)u16_max * t);

            *(output + y * sx + x) = value;
        }
    }
}
