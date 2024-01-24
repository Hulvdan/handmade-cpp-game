#pragma once

TerrainTile& GetTerrainTile(GameMap& gamemap, v2i pos)
{
    assert(PosIsInBounds(pos, gamemap.size));
    return *(gamemap.terrain_tiles + pos.y * gamemap.size.x + pos.x);
}

void RegenerateTerrainTiles(GameMap& gamemap)
{
    TerrainGenerationData data;
    data.max_height = 6;

    // TODO(hulvdan): There is a very good code for eliminating grass on the top
    // auto stride = state.gamemap.size.x;
    // for (int y = 0; y < state.gamemap.size.y; y++) {
    //     for (int x = 0; x < state.gamemap.size.x; x++) {
    //         auto is_grass = (Terrain)(*terrain_tile) == Terrain::GRASS;
    //         auto above_is_grass = false;
    //         auto below_is_grass = false;
    //         if (y < state.gamemap.size.y - 1)
    //             above_is_grass = (Terrain)(*(terrain_tile + stride)) == Terrain::GRASS;
    //         if (y > 0)
    //             below_is_grass = (Terrain)(*(terrain_tile - stride)) == Terrain::GRASS;
    //
    //         if (is_grass && !above_is_grass && !below_is_grass)
    //             *terrain_tile = (TileID)Terrain::NONE;
    //
    //         terrain_tile++;
    //     }
    // }

    auto size = gamemap.size;
    for (int y : range(size.y)) {
        // auto size = gamemap.size;
        // for (var y = 0; y < size.y; y++) {
        //     var row = new List<TerrainTile>();
        //
        //     for (var x = 0; x < _mapSizeX; x++) {
        //         var forestK = MakeSomeNoise2D(_randomSeed, x, y, _forestNoiseScale);
        //         var hasForest = forestK > _forestThreshold;
        //         var tile = new TerrainTile{
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
        for (int x : range(size.x)) {
            auto& tile = GetTerrainTile(gamemap, {x, y});
            tile.terrain = Terrain::GRASS;
            tile.height = int((data.max_height + 1) * frand());

            assert(tile.height >= 0);
            assert(tile.height <= data.max_height);
        }
    }

    // for (int y : range(size.y)) {
    //     for (int x : range(size.x)) {
    //         auto& tile = GetTerrainTile(gamemap, {x, y});
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

    for (int y : range(size.y)) {
        for (int x : range(size.x)) {
            auto& tile = GetTerrainTile(gamemap, {x, y});
            auto shouldMarkAsCliff =
                y == 0 || tile.height > GetTerrainTile(gamemap, {x, y - 1}).height;

            if (!shouldMarkAsCliff)
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
