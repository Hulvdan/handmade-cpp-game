#pragma once

Terrain_Tile& Get_Terrain_Tile(Game_Map& game_map, v2i pos)
{
    assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_tiles + pos.y * game_map.size.x + pos.x);
}

void Regenerate_Terrain_Tiles(Game_Map& game_map, Arena& arena, uint seed)
{
    Terrain_Generation_Data data;
    data.max_height = 6;

    auto size = game_map.size;

    auto max_pow2size = MAX(size.x, size.y);
    auto pitch = u16_max;
    while (pitch > max_pow2size)
        pitch >>= 1;
    pitch++;
    if (pitch < max_pow2size)
        pitch <<= 1;
    assert(pitch != 0);

    auto perlin = (u16*)(arena.base + arena.used);
    auto output_size = pitch * pitch;

    Allocate_Array(arena, u16, output_size);
    DEFER(Deallocate_Array(arena, u16, output_size));
    Fill_Perlin_2D(perlin, sizeof(u16) * output_size, arena, 9, 2.0f, seed, pitch, pitch);

    FOR_RANGE(int, y, size.y)
    {
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
        FOR_RANGE(int, x, size.x)
        {
            auto& tile = Get_Terrain_Tile(game_map, {x, y});
            tile.terrain = Terrain::GRASS;
            auto noise = *(perlin + size.x * y + x) / (f32)u16_max;
            tile.height = int((data.max_height + 1) * noise);

            assert(tile.height >= 0);
            assert(tile.height <= data.max_height);
        }
    }

    // for (var y = 0; y < _mapSizeY; y++) {
    //     for (var x = 0; x < _mapSizeX; x++) {
    //         var shouldMarkAsCliff =
    //             y == 0 || terrainTiles[y][x].height > terrainTiles[y - 1][x].height;
    //         if (!shouldMarkAsCliff) continue;
    //
    //         var tile = terrainTiles[y][x];
    //         tile.name = "cliff";
    //         tile.resource = null;
    //         tile.resourceAmount = 0;
    //         terrainTiles[y][x] = tile;
    //     }
    // }

    // NOTE(hulvdan): Removing one-tile-high grass blocks because they'd look ugly
    while (true) {
        bool changed = false;
        FOR_RANGE(int, y, size.y)
        {
            FOR_RANGE(int, x, size.x)
            {
                auto& tile = Get_Terrain_Tile(game_map, {x, y});

                TerrainHeight height_above = 0;
                if (y < size.y - 1)
                    height_above = Get_Terrain_Tile(game_map, {x, y + 1}).height;

                TerrainHeight height_below = 0;
                if (y > 0)
                    height_below = Get_Terrain_Tile(game_map, {x, y - 1}).height;

                auto should_change = tile.height > height_below && tile.height > height_above;
                if (should_change) {
                    tile.height = MAX(height_below, height_above);
                    changed = true;
                }
            }
        }

        if (!changed)
            break;
    }

    FOR_RANGE(int, y, size.y)
    {
        FOR_RANGE(int, x, size.x)
        {
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
