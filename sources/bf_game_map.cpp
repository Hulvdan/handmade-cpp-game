#pragma once

Terrain_Tile& Get_Terrain_Tile(Game_Map& game_map, v2i pos) {
    assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_tiles + pos.y * game_map.size.x + pos.x);
}

Terrain_Resource& Get_Terrain_Resource(Game_Map& game_map, v2i pos) {
    assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_resources + pos.y * game_map.size.x + pos.x);
}

void Regenerate_Terrain_Tiles(Game_State& state, Game_Map& game_map, Arena& arena, uint seed) {
    auto size = game_map.size;

    auto pitch = Ceil_To_Power_Of_2(MAX(size.x, size.y));
    auto output_size = pitch * pitch;

    // Terrain data
    PerlinParams terrain_params = {};
    terrain_params.octaves = 9;
    terrain_params.scaling_bias = 2.0f;
    terrain_params.seed = seed;
    auto max_terrain_height = 6;

    // Forest data
    PerlinParams forest_params = {};
    forest_params.octaves = 9;
    forest_params.scaling_bias = 2.0f;
    forest_params.seed = seed + 1;
    auto forest_threshold = 0.20f;
    auto max_forest_amount = 5;

    auto terrain_perlin = Allocate_Array(arena, u16, output_size);
    DEFER(Deallocate_Array(arena, u16, output_size));
    Fill_Perlin_2D(terrain_perlin, sizeof(u16) * output_size, arena, terrain_params, pitch, pitch);

    auto forest_perlin = Allocate_Array(arena, u16, output_size);
    DEFER(Deallocate_Array(arena, u16, output_size));
    Fill_Perlin_2D(forest_perlin, sizeof(u16) * output_size, arena, forest_params, pitch, pitch);

    // auto size = game_map.size;
    // for (var y = 0; y < size.y; y++) {
    //     var row = new List<Terrain_Tile>();
    //
    //     for (var x = 0; x < _mapSizeX; x++) {
    //         var forestK = MakeSomeNoise2D(_randomSeed, x, y, _forestNoiseScale);
    //         var hasForest = forestK > _forestThreshold;
    //         var tile = new Terrain_Tile{
    //             name = "grass",
    //             resource = hasForest ? _logResource : null,
    //             resourceAmount = hasForest ? _maxForestAmount : 0,
    //         };
    //
    //         var heightK = MakeSomeNoise2D(_randomSeed, x, y, _terrainHeightNoiseScale);
    //         var randomH = heightK * (_maxHeight + 1);
    //         tile.height = Mathf.Min(_maxHeight, (int)randomH);
    //
    //         row.Add(tile);
    //     }
    //
    //     terrainTiles.Add(row);
    // }
    FOR_RANGE(int, y, size.y) {
        FOR_RANGE(int, x, size.x) {
            auto& tile = Get_Terrain_Tile(game_map, {x, y});
            tile.terrain = Terrain::GRASS;
            auto noise = *(terrain_perlin + size.x * y + x) / (f32)u16_max;
            tile.height = int((max_terrain_height + 1) * noise);

            assert(tile.height >= 0);
            assert(tile.height <= max_terrain_height);
        }
    }

    auto scriptable_ptr = &state.DEBUG_forest;
    FOR_RANGE(int, y, size.y) {
        FOR_RANGE(int, x, size.x) {
            auto& resource = Get_Terrain_Resource(game_map, {x, y});
            auto noise = *(forest_perlin + size.x * y + x) / (f32)u16_max;

            bool forest = noise > forest_threshold;
            resource.scriptable = (Scriptable_Resource*)((ptrd)scriptable_ptr * forest);
            resource.amount = max_forest_amount * forest;
        }
    }

    // NOTE(hulvdan): Removing one-tile-high grass blocks because they'd look ugly
    while (true) {
        bool changed = false;
        FOR_RANGE(int, y, size.y) {
            FOR_RANGE(int, x, size.x) {
                auto& tile = Get_Terrain_Tile(game_map, {x, y});

                TerrainHeight height_above = 0;
                if (y < size.y - 1)
                    height_above = Get_Terrain_Tile(game_map, {x, y + 1}).height;

                TerrainHeight height_below = 0;
                if (y > 0)
                    height_below = Get_Terrain_Tile(game_map, {x, y - 1}).height;

                auto should_change = tile.height > height_below && tile.height > height_above;
                if (should_change)
                    tile.height = MAX(height_below, height_above);

                changed |= should_change;
            }
        }

        if (!changed)
            break;
    }

    FOR_RANGE(int, y, size.y) {
        FOR_RANGE(int, x, size.x) {
            auto& tile = Get_Terrain_Tile(game_map, {x, y});
            auto should_mark_as_cliff =
                y == 0 || tile.height > Get_Terrain_Tile(game_map, {x, y - 1}).height;

            if (!should_mark_as_cliff)
                continue;

            tile.is_cliff = true;
        }
    }

    // TODO(hulvdan): Element Tiles
    // elementTiles = _initialMapProvider.LoadElementTiles();
    //
    // var cityHalls = buildings.FindAll(i = > i.scriptable.type ==
    // BuildingType.SpecialCityHall); foreach (var building in cityHalls) {
    //     var pos = building.pos;
    //     elementTiles[pos.y][pos.x] = new (ElementTileType.Building, building);
    // }
}
