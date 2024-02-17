#pragma once

Scriptable_Resource* Get_Scriptable_Resource(Game_State& game_state, Scriptable_Resource_ID id) {
    assert(id - 1 < game_state.scriptable_resources_count);

    auto exists = id != 0;
    ptrd offset_resource = (ptrd)(game_state.scriptable_resources + (ptrd)id - 1);

    auto result = offset_resource * exists;
    return (Scriptable_Resource*)result;
}

Scriptable_Building* Get_Scriptable_Building(Game_State& game_state, Scriptable_Building_ID id) {
    assert(id - 1 < game_state.scriptable_buildings_count);

    auto exists = id != 0;
    ptrd offset_building = (ptrd)(game_state.scriptable_buildings + (ptrd)id - 1);

    auto result = offset_building * exists;
    return (Scriptable_Building*)result;
}

Terrain_Tile& Get_Terrain_Tile(Game_Map& game_map, v2i pos) {
    assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_tiles + pos.y * game_map.size.x + pos.x);
}

Terrain_Resource& Get_Terrain_Resource(Game_Map& game_map, v2i pos) {
    assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_resources + pos.y * game_map.size.x + pos.x);
}

void Regenerate_Terrain_Tiles(
    Game_State& state,
    Game_Map& game_map,
    Arena& arena,
    Arena& temp_arena,
    uint seed,
    Editor_Data& data  //
) {
    auto gsize = game_map.size;

    auto noise_pitch = Ceil_To_Power_Of_2(MAX(gsize.x, gsize.y));
    auto output_size = noise_pitch * noise_pitch;

    auto terrain_perlin = Allocate_Array(temp_arena, u16, output_size);
    DEFER(Deallocate_Array(temp_arena, u16, output_size));
    Fill_Perlin_2D(
        terrain_perlin, sizeof(u16) * output_size, temp_arena, data.terrain_perlin, noise_pitch,
        noise_pitch);

    auto forest_perlin = Allocate_Array(temp_arena, u16, output_size);
    DEFER(Deallocate_Array(temp_arena, u16, output_size));
    Fill_Perlin_2D(
        forest_perlin, sizeof(u16) * output_size, temp_arena, data.forest_perlin, noise_pitch,
        noise_pitch);

    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = Get_Terrain_Tile(game_map, {x, y});
            tile.terrain = Terrain::Grass;
            auto noise = *(terrain_perlin + noise_pitch * y + x) / (f32)u16_max;
            tile.height = int((data.terrain_max_height + 1) * noise);

            assert(tile.height >= 0);
            assert(tile.height <= data.terrain_max_height);
        }
    }

    // NOTE(hulvdan): Removing one-tile-high grass blocks because they'd look ugly
    while (true) {
        bool changed = false;
        FOR_RANGE(int, y, gsize.y) {
            FOR_RANGE(int, x, gsize.x) {
                auto& tile = Get_Terrain_Tile(game_map, {x, y});

                int height_above = 0;
                if (y < gsize.y - 1)
                    height_above = Get_Terrain_Tile(game_map, {x, y + 1}).height;

                int height_below = 0;
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

    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = Get_Terrain_Tile(game_map, {x, y});
            if (tile.is_cliff)
                continue;

            tile.is_cliff = y == 0 || tile.height > Get_Terrain_Tile(game_map, {x, y - 1}).height;
        }
    }

    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = Get_Terrain_Tile(game_map, {x, y});
            auto& resource = Get_Terrain_Resource(game_map, {x, y});

            auto noise = *(forest_perlin + noise_pitch * y + x) / (f32)u16_max;
            bool generate = (!tile.is_cliff) && (noise > data.forest_threshold);

            resource.scriptable_id = global_forest_resource_id * generate;
            resource.amount = data.forest_max_amount * generate;
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

void Regenerate_Element_Tiles(
    Game_State& state,
    Game_Map& game_map,
    Arena& arena,
    Arena& temp_arena,
    uint seed,
    Editor_Data& data  //
) {
    auto gsize = game_map.size;

    v2i road_tiles[] = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {2, 1},
        {2, 2},
        {2, 3},
        {3, 2},
        {4, 1},
        {4, 2},
        {4, 3},
        {1, 0},
        {2, 0},
        {3, 0},
        {1, 4},
        {2, 4},
        {3, 4},
        //
        {6, 1},
        {7, 1},
        {8, 1},
        {6, 2},
        {8, 2},
        {6, 3},
        {7, 3},
        {8, 3},
    };

    auto base_offset = v2i(1, 1);
    for (auto offset : road_tiles) {
        auto o = offset + base_offset;
        Element_Tile& tile = *(game_map.element_tiles + o.y * gsize.x + o.x);

        tile.type = Element_Tile_Type::Road;
        assert(tile.building == nullptr);
    }

    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            Element_Tile& tile = *(game_map.element_tiles + y * gsize.x + x);
            Validate_Element_Tile(tile);
        }
    }
}

Building_Page_Meta& Get_Building_Page_Meta(size_t page_size, Page& page) {
    return *rcast<Building_Page_Meta*>(page.base + page_size - sizeof(Building_Page_Meta));
}

void Place_Building(Game_State& state, v2i pos, Scriptable_Building_ID id) {
    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    auto& os_data = *state.os_data;

    const auto page_size = os_data.page_size;
    assert(Pos_Is_In_Bounds(pos, gsize));

    Page* page = nullptr;
    Building* found_building = nullptr;
    FOR_RANGE(size_t, page_index, game_map.building_pages_used) {
        page = game_map.building_pages + page_index;
        auto& meta = Get_Building_Page_Meta(page_size, *page);

        if (meta.count >= game_map.max_buildings_per_page)
            continue;

        FOR_RANGE(size_t, building_index, game_map.max_buildings_per_page) {
            auto& building = *(rcast<Building*>(page->base) + building_index);
            if (!building.active) {
                found_building = &building;
                break;
            }
        }

        if (found_building != nullptr)
            break;
    }

    if (found_building == nullptr) {
        assert(state.game_map.building_pages_used < state.game_map.building_pages_total);
        page = state.game_map.building_pages + state.game_map.building_pages_used;

        page->base = Book_Single_Page(state);
        state.game_map.building_pages_used++;

        found_building = rcast<Building*>(page->base);
        assert(found_building != nullptr);
    }

    Get_Building_Page_Meta(page_size, *page).count++;
    auto& b = *found_building;

    b.pos = pos;
    b.active = true;
    b.scriptable_id = id;
}

bool Try_Build(Game_State& state, v2i pos, Item_To_Build item) {
    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    assert(Pos_Is_In_Bounds(pos, gsize));

    auto& tile = *(game_map.element_tiles + pos.y * gsize.x + pos.x);

    switch (item.type) {
    case Item_To_Build_Type::Flag: {
        if (tile.type == Element_Tile_Type::Flag)
            tile.type = Element_Tile_Type::Road;
        else if (tile.type == Element_Tile_Type::Road)
            tile.type = Element_Tile_Type::Flag;
        else
            return false;

        assert(tile.building == nullptr);
    } break;

    case Item_To_Build_Type::Road: {
        if (tile.type != Element_Tile_Type::None)
            return false;

        assert(tile.building == nullptr);
        tile.type = Element_Tile_Type::Road;
    } break;

    case Item_To_Build_Type::Building: {
        if (tile.type != Element_Tile_Type::None)
            return false;

        assert(item.scriptable_building_id != 0);
        Place_Building(state, pos, item.scriptable_building_id);
    } break;

    default:
        UNREACHABLE;
    }

    INVOKE_OBSERVER(state.On_Item_Built, (state, pos, item));

    return true;
}
