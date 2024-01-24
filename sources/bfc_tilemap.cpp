#pragma once

#ifndef BF_CLIENT
#error "This code should run on a client! BF_CLIENT must be defined!"
#endif  // BF_CLIENT

TileStateCheck ParseTileStateCheck(u8 data)
{
    switch (data) {
    case '@':
        return TileStateCheck::INCLUDED;

    case ' ':
        return TileStateCheck::SKIP;

    case '*':
        return TileStateCheck::EXCLUDED;

    default:
        assert(false);
    }

    return TileStateCheck::SKIP;
}

LoadSmartTile_Result LoadSmartTileRules(
    SmartTile& tile,
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

    LoadSmartTile_Result res = {};
    tile.rules_count = 0;
    tile.rules = nullptr;

    // 1. Parsing fallback_texture_name
    {
        auto offset = FindNewline(filedata, filesize);
        assert(offset > 0);

        tile.fallback_texture_id = static_cast<BFTextureID>(Hash32(filedata, offset));

        filedata += offset;
        filesize -= offset;
    }

    {
        auto offset = FindNotNewline(filedata, filesize);
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
        if (sizeof(TileRule) * (rules_count + 1) > output_max_bytes) {
            assert(false);
            return res;
        }

        TileRule& rule = *(TileRule*)(rules_output + sizeof(TileRule) * rules_count);
        {
            auto offset = FindNewline(filedata, filesize);
            if (offset < 0)
                break;

            rule.texture_id = static_cast<BFTextureID>(Hash32(filedata, offset));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = FindNotNewline(filedata, filesize);
            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }

        assert(*filedata == '|');
        {
            auto offset = FindNewline(filedata, filesize);
            assert(offset == 5);

            rule.states[0] = ParseTileStateCheck(*(filedata + 1));
            rule.states[1] = ParseTileStateCheck(*(filedata + 2));
            rule.states[2] = ParseTileStateCheck(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = FindNotNewline(filedata, filesize);
            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = FindNewline(filedata, filesize);
            assert(offset == 5);

            rule.states[3] = ParseTileStateCheck(*(filedata + 1));
            rule.states[4] = ParseTileStateCheck(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = FindNotNewline(filedata, filesize);
            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }
        {
            auto offset = FindNewlineOrEOF(filedata, filesize);
            assert(offset == 5);

            rule.states[5] = ParseTileStateCheck(*(filedata + 1));
            rule.states[6] = ParseTileStateCheck(*(filedata + 2));
            rule.states[7] = ParseTileStateCheck(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
            rules_count++;
        }

        {
            if (filesize == 0)
                break;

            auto offset = FindNotNewline(filedata, filesize);
            if (offset < 0)
                break;

            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }
    }

    res.success = true;
    tile.rules_count = rules_count;
    tile.rules = (TileRule*)rules_output;
    res.size = tile.rules_count * sizeof(TileRule);
    return res;
}

void SetTile(Tilemap& tilemap, v2i pos, TileID tile_id)
{
    assert(PosIsInBounds(pos, tilemap.size));
    *(tilemap.tiles + tilemap.size.x * pos.y + pos.x) = tile_id;
}

// NOTE(hulvdan): `tile_ids_distance` is the distance
// (in bytes) between two adjacent TileIDs in `tile_ids`
// BFTextureID TestSmartTile(Tilemap& tilemap, v2i pos, SmartTile& tile)
// {
//     v2i offsets[] = {{-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};
//
//     for (int r = 0; r < tile.rules_count; r++) {
//         auto& rule = *(tile.rules + r);
//
//         b32 found = true;
//         for (int i = 0; (i < 8) && found; i++) {
//             TileStateCheck state = *(rule.states + i);
//             if (state == TileStateCheck::SKIP)
//                 continue;
//
//             auto& offset = offsets[i];
//             auto new_pos = pos + offset;
//
//             if (!PosIsInBounds(new_pos, size)) {
//                 found &= state != TileStateCheck::INCLUDED;
//                 continue;
//             }
//
//             auto tile = tilemap.terrain_tiles + size.x * new_pos.y + new_pos.x;
//
//             if (state == TileStateCheck::INCLUDED)
//                 found &= tile_id == (TileID)tile.id;
//             else if (state == TileStateCheck::EXCLUDED)
//                 found &= tile_id != (TileID)tile.id;
//         }
//
//         if (found)
//             return rule.texture_id;
//     }
//
//     return tile.fallback_texture_id;
// }
