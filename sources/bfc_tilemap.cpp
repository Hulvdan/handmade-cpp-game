#pragma once

#ifndef BF_CLIENT
#error "This code should run on a client! BF_CLIENT must be defined!"
#endif  // BF_CLIENT

Tile_State_Check Parse_Tile_State_Check(u8 data)
{
    switch (data) {
    case '@':
        return Tile_State_Check::INCLUDED;

    case ' ':
        return Tile_State_Check::SKIP;

    case '*':
        return Tile_State_Check::EXCLUDED;

    default:
        assert(false);
    }

    return Tile_State_Check::SKIP;
}

Load_Smart_Tile_Result Load_Smart_Tile_Rules(
    Smart_Tile& tile,
    u8* rules_output,
    const size_t output_max_bytes,
    const u8* filedata,
    u64 filesize)
{
    // --- ASSERTING THAT THERE IS NO `0` BYTES IN THE LOADED FILE
    auto c = filedata;
    auto f = filesize;
    while (f--)
        assert(*c != '\0');
    // --- ASSERTING THAT THERE IS NO `0` BYTES IN THE LOADED FILE END

    // grass_7 -- fallback texture name
    // grass_1 -- rule #1
    // | * |
    // |*@@|
    // | @ |
    // grass_2 -- rule #2. There could be more (or less) rules
    // | * |
    // |@@@|
    // | @ |

    Load_Smart_Tile_Result res = {};
    tile.rules_count = 0;
    tile.rules = nullptr;

    // 1. Parsing fallback_texture_name
    {
        auto offset = Find_Newline(filedata, filesize);
        assert(offset > 0);

        tile.fallback_texture_id = scast<BF_Texture_ID>(Hash32(filedata, offset));

        filedata += offset;
        filesize -= offset;
    }

    {
        auto offset = Find_Not_Newline(filedata, filesize);
        assert(offset > 0);
        filedata += offset;
        filesize -= offset;
    }

    // 2. Parsing rules
    //     Parsing texture_name
    //     Parsing state
    //     Parsing state while ignoring the middle cell because of the always-present `@`
    //     Parsing state
    int rules_count = 0;
    while (true) {
        if (sizeof(Tile_Rule) * (rules_count + 1) > output_max_bytes) {
            assert(false);
            return res;
        }

        Tile_Rule& rule = *(Tile_Rule*)(rules_output + sizeof(Tile_Rule) * rules_count);
        {
            auto offset = Find_Newline(filedata, filesize);
            if (offset < 0)
                break;

            rule.texture_id = scast<BF_Texture_ID>(Hash32(filedata, offset));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = Find_Not_Newline(filedata, filesize);
            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }

        assert(*filedata == '|');
        {
            auto offset = Find_Newline(filedata, filesize);
            assert(offset == 5);

            rule.states[0] = Parse_Tile_State_Check(*(filedata + 1));
            rule.states[1] = Parse_Tile_State_Check(*(filedata + 2));
            rule.states[2] = Parse_Tile_State_Check(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = Find_Not_Newline(filedata, filesize);
            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = Find_Newline(filedata, filesize);
            assert(offset == 5);

            rule.states[3] = Parse_Tile_State_Check(*(filedata + 1));
            rule.states[4] = Parse_Tile_State_Check(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = Find_Not_Newline(filedata, filesize);
            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }
        {
            auto offset = Find_Newline_Or_EOF(filedata, filesize);
            assert(offset == 5);

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

            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }
    }

    res.success = true;
    tile.rules_count = rules_count;
    tile.rules = (Tile_Rule*)rules_output;
    res.size = tile.rules_count * sizeof(Tile_Rule);
    return res;
}

void Set_Tile(Tilemap& tilemap, v2i pos, Tile_ID tile_id)
{
    assert(Pos_Is_In_Bounds(pos, tilemap.size));
    *(tilemap.tiles + tilemap.size.x * pos.y + pos.x) = tile_id;
}

// NOTE(hulvdan): `tile_ids_distance` is the distance
// (in bytes) between two adjacent Tile_IDs in `tile_ids`
BF_Texture_ID Test_Smart_Tile(Tilemap& tilemap, v2i size, v2i pos, Smart_Tile& tile)
{
    v2i offsets[] = {{-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};

    for (int r = 0; r < tile.rules_count; r++) {
        auto& rule = *(tile.rules + r);

        b32 found = true;
        for (int i = 0; (i < 8) && found; i++) {
            Tile_State_Check state = *(rule.states + i);
            if (state == Tile_State_Check::SKIP)
                continue;

            auto& offset = offsets[i];
            auto new_pos = pos + offset;

            if (!Pos_Is_In_Bounds(new_pos, size)) {
                found &= state != Tile_State_Check::INCLUDED;
                continue;
            }

            Tile_ID tile_id = *(tilemap.tiles + size.x * new_pos.y + new_pos.x);

            if (state == Tile_State_Check::INCLUDED)
                found &= tile_id == (Tile_ID)tile.id;
            else if (state == Tile_State_Check::EXCLUDED)
                found &= tile_id != (Tile_ID)tile.id;
        }

        if (found)
            return rule.texture_id;
    }

    return tile.fallback_texture_id;
}
