#if !BF_CLIENT
#    error "This code should run on a client! BF_CLIENT must be defined!"
#endif

void Load_Smart_Tile_Rule(Smart_Tile& tile, Arena& arena, const BFGame::Tile_Rule* rule) {
    tile.fallback_texture = rule->default_texture();

    tile.rules_count = rule->states()->size();
    tile.rules       = Allocate_Array(arena, Tile_Rule, tile.rules_count);

    FOR_RANGE (int, i, tile.rules_count) {
        Tile_Rule*  r     = tile.rules + i;
        const auto& state = rule->states()->Get(i);
        r->texture        = state->texture();

        FOR_RANGE (int, k, 8) {
            auto condition = (i32)state->condition()->Get(k);
            r->states[k]   = (Tile_State_Condition)condition;
        }
    }
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
            Tile_State_Condition cond = *(rule.states + i);
            if (cond == Tile_State_Condition::Skip)
                continue;

            auto new_pos = pos + offsets[i];
            if (!Pos_Is_In_Bounds(new_pos, size)) {
                found &= cond != Tile_State_Condition::Included;
                continue;
            }

            Tile_ID tile_id = tilemap.tiles[size.x * new_pos.y + new_pos.x];

            if (cond == Tile_State_Condition::Included)
                found &= tile_id == tile.id;
            else if (cond == Tile_State_Condition::Excluded)
                found &= tile_id != tile.id;
        }

        if (found)
            return rule.texture;
    }

    return tile.fallback_texture;
}
