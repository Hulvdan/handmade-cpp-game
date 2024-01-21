#include "bf_base.h"

// TODO(hulvdan): Rename To TileStateCheck
enum class TileState {
    NONE,  // TODO(hulvdan): Rename To SKIP
    EXCLUDED,
    INCLUDED,
};

TileState ParseTileState(u8 data)
{
    switch (data) {
    case '@':
        return TileState::INCLUDED;

    case ' ':
        return TileState::NONE;

    case '*':
        return TileState::EXCLUDED;

    default:
        assert(false);
    }

    return TileState::NONE;
}

using TileID = u32;

struct TileRule {
    BFTextureID texture_id;
    TileState states[8];
};

struct SmartTile {
    TileID id;
    BFTextureID fallback_texture_id;

    int rules_count;
    TileRule* rules;
};

struct LoadSmartTile_Result {
    bool success;
    // SmartTile tile;
};

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
    // rules_output
    // output_max_bytes
    while (true) {
        TileRule& rule = *(TileRule*)rules_output;
        {
            auto offset = FindNewline(filedata, filesize);
            // TODO(hulvdan): Here is the return;
            assert(offset > 0);

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
            assert(offset > 0);

            rule.states[0] = ParseTileState(*(filedata + 1));
            rule.states[1] = ParseTileState(*(filedata + 2));
            rule.states[2] = ParseTileState(*(filedata + 3));

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
            assert(offset > 0);

            rule.states[3] = ParseTileState(*(filedata + 1));
            rule.states[4] = ParseTileState(*(filedata + 3));

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
            assert(offset > 0);

            rule.states[5] = ParseTileState(*(filedata + 1));
            rule.states[6] = ParseTileState(*(filedata + 2));
            rule.states[7] = ParseTileState(*(filedata + 3));

            filedata += offset;
            filesize -= offset;
        }

        {
            auto offset = FindNotNewline(filedata, filesize);
            assert(offset > 0);
            filedata += offset;
            filesize -= offset;
        }
    }

    res.success = true;
    return res;
}

#define InBounds(pos, bounds) \
    (!((pos).x < 0 || (pos).x >= (bounds).x || (pos).y < 0 || (pos).y >= (bounds).y))

// NOTE(hulvdan): `tile_ids_distance` is the distance
// (in bytes) between two adjacent TileIDs in `tile_ids`
BFTextureID
TestSmartTile(TileID* tile_ids, size_t tile_ids_distance, v2i size, v2i pos, SmartTile& tile)
{
    // TODO(hulvdan): Probably need to reverse these
    v2i offsets[] = {{-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};

    for (int r = 0; r < tile.rules_count; r++) {
        auto& rule = *tile.rules++;

        b32 found = true;
        for (int i = 0; (i < 8) && found; i++) {
            TileState state = *(rule.states + i);
            if (state == TileState::NONE)
                continue;

            auto& offset = offsets[i];
            auto new_pos = pos + offset;

            if (!InBounds(new_pos, size)) {
                found &= state == TileState::INCLUDED;
                continue;
            }

            auto scaled_dist = tile_ids_distance * (offset.y * size.x + offset.x);
            auto tile_id = *(TileID*)((u8*)tile_ids + scaled_dist);

            if (state == TileState::INCLUDED)
                found &= tile_id == tile.id;
            else if (state == TileState::EXCLUDED)
                found &= tile_id != tile.id;
        }

        if (found)
            return rule.texture_id;
    }

    return tile.fallback_texture_id;
}
