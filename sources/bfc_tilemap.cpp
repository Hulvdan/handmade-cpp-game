
#ifndef BF_CLIENT
#    error "This code should run on a client! BF_CLIENT must be defined!"
#endif  // BF_CLIENT

Tile_State_Check Parse_Tile_State_Check(u8 data) {
    switch (data) {
    case '@':
        return Tile_State_Check::Included;

    case ' ':
        return Tile_State_Check::Skip;

    case '*':
        return Tile_State_Check::Excluded;

    default:
        INVALID_PATH;
    }

    return Tile_State_Check::Skip;
}

// NOTE: Сюда не смотреть.
// В момент написания я забыл про то, что существуют парсеры / лексеры)
Load_Smart_Tile_Result
Load_Smart_Tile_Rules(Smart_Tile& tile, Arena& arena, const u8* filedata, u64 filesize_) {
    u32 filesize = Assert_Truncate_To_u32(filesize_);

    // --- ASSERTING THAT THERE IS NO `0` BYTES IN THE LOADED FILE
    auto c = filedata;
    u32  f = filesize;
    while (f--)
        Assert(*c != '\0');
    // --- ASSERTING THAT THERE IS NO `0` BYTES IN THE LOADED FILE END

    auto rules_output     = arena.base + arena.used;
    auto output_max_bytes = arena.size - arena.used;

    // NOTE: Example of file's contents
    // grass_7 -- fallback texture name
    // grass_1 -- rule #1
    // | * |
    // |*@@|
    // | @ |
    // grass_2 -- rule #2. There could be more (or less) rules
    // | * |
    // |@@@|
    // | @ |

    Load_Smart_Tile_Result res{};
    tile.rules_count = 0;
    tile.rules       = nullptr;

    // 1. Parsing fallback_texture_name
    {
        auto offset = Find_Newline(filedata, filesize);
        Assert(offset > 0);

        tile.fallback_texture = scast<BF_Texture_ID>(Hash32(filedata, offset));

        filedata += offset;
        filesize -= offset;
    }

    {
        auto offset = Find_Not_Newline(filedata, filesize);
        Assert(offset > 0);
        filedata += offset;
        filesize -= offset;
    }

    // 2. Parsing rules
    //     Parsing texture_name
    //     Parsing state
    //     Parsing state while ignoring the middle cell because of the always-present `@`
    //     Parsing state
    u32 rules_count = 0;
    while (true) {
        if (sizeof(Tile_Rule) * (rules_count + 1) > output_max_bytes) {
            INVALID_PATH;
            return res;
        }

        Tile_Rule& rule = *(Tile_Rule*)(rules_output + sizeof(Tile_Rule) * rules_count);
        {
            auto offset = Find_Newline(filedata, filesize);
            if (offset < 0)
                break;

            rule.texture = scast<BF_Texture_ID>(Hash32(filedata, offset));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = Find_Not_Newline(filedata, filesize);
            Assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }

        Assert(*filedata == '|');
        {
            auto offset = Find_Newline(filedata, filesize);
            Assert(offset == 5);

            rule.states[0] = Parse_Tile_State_Check(*(filedata + 1));
            rule.states[1] = Parse_Tile_State_Check(*(filedata + 2));
            rule.states[2] = Parse_Tile_State_Check(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = Find_Not_Newline(filedata, filesize);
            Assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = Find_Newline(filedata, filesize);
            Assert(offset == 5);

            rule.states[3] = Parse_Tile_State_Check(*(filedata + 1));
            rule.states[4] = Parse_Tile_State_Check(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = Find_Not_Newline(filedata, filesize);
            Assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }
        {
            auto offset = Find_Newline_Or_EOF(filedata, filesize);
            Assert(offset == 5);

            rule.states[5] = Parse_Tile_State_Check(*(filedata + 1));
            rule.states[6] = Parse_Tile_State_Check(*(filedata + 2));
            rule.states[7] = Parse_Tile_State_Check(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
            rules_count++;
        }

        {
            if (filesize == 0)
                break;

            auto offset = Find_Not_Newline(filedata, filesize);
            if (offset < 0)
                break;

            Assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }
    }

    res.success      = true;
    tile.rules_count = rules_count;
    tile.rules       = Allocate_Array(arena, Tile_Rule, rules_count);
    return res;
}

// NOTE: `tile_ids_distance` is the distance
// (in bytes) between two adjacent Tile_IDs in `tile_ids`
Texture_ID Test_Smart_Tile(Tilemap& tilemap, v2i16 size, v2i16 pos, Smart_Tile& tile) {
    v2i16 offsets[]
        = {{-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};

    for (int r = 0; r < tile.rules_count; r++) {
        auto& rule = *(tile.rules + r);

        b32 found = true;
        for (int i = 0; (i < 8) && found; i++) {
            Tile_State_Check state = *(rule.states + i);
            if (state == Tile_State_Check::Skip)
                continue;

            auto new_pos = pos + offsets[i];
            if (!Pos_Is_In_Bounds(new_pos, size)) {
                found &= state != Tile_State_Check::Included;
                continue;
            }

            Tile_ID tile_id = tilemap.tiles[size.x * new_pos.y + new_pos.x];

            if (state == Tile_State_Check::Included)
                found &= tile_id == tile.id;
            else if (state == Tile_State_Check::Excluded)
                found &= tile_id != tile.id;
        }

        if (found)
            return rule.texture;
    }

    return tile.fallback_texture;
}

//
// NOTE:
// Подсчёт самого длинного пути без циклов,
// который может быть выдан при расчёте кратчайшего пути.
//
// В геометрическом смысле его длина
// равна количеству клеток с буквой R - 20.
//
//     R.RRR.RRR
//     R.R.R.R.R
//     R.R.R.R.R
//     RRR.RRR.R
//
u32 Longest_Meaningful_Path(v2i16 size) {
    i32 a = MIN(size.x, size.y);
    i32 b = MAX(size.x, size.y);

    i32 v1 = a / 2 + Ceiled_Division(a, 2) * b;
    i32 v2 = b / 2 + Ceiled_Division(b, 2) * a;

    i32 result = MAX(v1, v2);
    return result;
}
