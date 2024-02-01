#pragma once

Terrain_Tile& Get_Terrain_Tile(Game_Map& game_map, v2i pos)
{
    assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_tiles + pos.y * game_map.size.x + pos.x);
}

void Regenerate_Terrain_Tiles(
    Game_Map& game_map,
    u8* temp_storage,
    size_t free_temp_storage_space,
    uint seed)
{
    Terrain_Generation_Data data;
    data.max_height = 6;

    // TODO(hulvdan): There is a very good code for eliminating grass on the top
    // auto stride = state.game_map.size.x;
    // for (int y = 0; y < state.game_map.size.y; y++) {
    //     for (int x = 0; x < state.game_map.size.x; x++) {
    //         auto is_grass = (Terrain)(*terrain_tile) == Terrain::GRASS;
    //         auto above_is_grass = false;
    //         auto below_is_grass = false;
    //         if (y < state.game_map.size.y - 1)
    //             above_is_grass = (Terrain)(*(terrain_tile + stride)) == Terrain::GRASS;
    //         if (y > 0)
    //             below_is_grass = (Terrain)(*(terrain_tile - stride)) == Terrain::GRASS;
    //
    //         if (is_grass && !above_is_grass && !below_is_grass)
    //             *terrain_tile = (Tile_ID)Terrain::NONE;
    //
    //         terrain_tile++;
    //     }
    // }

    auto size = game_map.size;

    auto max_pow2size = MAX(size.x, size.y);
    auto pitch = u16_max;
    while (pitch > max_pow2size)
        pitch >>= 1;
    pitch++;
    if (pitch < max_pow2size)
        pitch <<= 1;
    assert(pitch != 0);

    auto perlin = (u16*)temp_storage;
    auto output_size = sizeof(u16) * pitch * pitch;
    Fill_Perlin_2D(
        perlin, output_size,  //
        (u8*)perlin + output_size, free_temp_storage_space - output_size,  //
        9, 2.0f, seed, pitch, pitch);

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

    // for (int y : range(size.y)) {
    //     for (int x : range(size.x)) {
    //         auto& tile = Get_Terrain_Tile(game_map, {x, y});
    //         tile.terrain = Terrain::GRASS;
    //
    //         tile.height = int((data.max_height + 1) * frand());
    //     }
    // }

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
