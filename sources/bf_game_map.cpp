#pragma once

Scriptable_Resource* Get_Scriptable_Resource(Game_State& state, Scriptable_Resource_ID id) {
    assert(id - 1 < state.scriptable_resources_count);
    auto exists = id != 0;
    auto ptr_offset = (ptrd)(state.scriptable_resources + id - 1);
    auto result = ptr_offset * exists;
    return rcast<Scriptable_Resource*>(result);
}

Scriptable_Building* Get_Scriptable_Building(Game_State& state, Scriptable_Building_ID id) {
    assert(id - 1 < state.scriptable_buildings_count);
    auto exists = id != 0;
    auto ptr_offset = (ptrd)(state.scriptable_buildings + id - 1);
    auto result = ptr_offset * exists;
    return rcast<Scriptable_Building*>(result);
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
    Arena& trash_arena,
    uint seed,
    Editor_Data& data  //
) {
    auto gsize = game_map.size;

    auto noise_pitch = Ceil_To_Power_Of_2(MAX(gsize.x, gsize.y));
    auto output_size = noise_pitch * noise_pitch;

    auto terrain_perlin = Allocate_Array(trash_arena, u16, output_size);
    DEFER(Deallocate_Array(trash_arena, u16, output_size));
    Fill_Perlin_2D(
        terrain_perlin, sizeof(u16) * output_size, trash_arena, data.terrain_perlin, noise_pitch,
        noise_pitch);

    auto forest_perlin = Allocate_Array(trash_arena, u16, output_size);
    DEFER(Deallocate_Array(trash_arena, u16, output_size));
    Fill_Perlin_2D(
        forest_perlin, sizeof(u16) * output_size, trash_arena, data.forest_perlin, noise_pitch,
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
    Arena& trash_arena,
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

Graph_Segment_Page_Meta& Get_Graph_Segment_Page_Meta(
    size_t page_size,
    Page& page  //
) {
    return *rcast<Graph_Segment_Page_Meta*>(
        page.base + page_size - sizeof(Graph_Segment_Page_Meta));
}

void Place_Building(Game_State& state, v2i pos, Scriptable_Building_ID id) {
    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    auto& os_data = *state.os_data;

    const auto page_size = os_data.page_size;
    assert(Pos_Is_In_Bounds(pos, gsize));

    Page* page = nullptr;
    Building* found_instance = nullptr;
    FOR_RANGE(size_t, page_index, game_map.building_pages_used) {
        page = game_map.building_pages + page_index;
        auto& meta = Get_Building_Page_Meta(page_size, *page);

        if (meta.count >= game_map.max_buildings_per_page)
            continue;

        FOR_RANGE(size_t, building_index, game_map.max_buildings_per_page) {
            auto& instance = *(rcast<Building*>(page->base) + building_index);
            if (!instance.active) {
                found_instance = &instance;
                break;
            }
        }

        if (found_instance != nullptr)
            break;
    }

    if (found_instance == nullptr) {
        assert(game_map.building_pages_used < game_map.building_pages_total);
        page = game_map.building_pages + game_map.building_pages_used;

        page->base = Book_Single_Page(state);
        game_map.building_pages_used++;

        found_instance = rcast<Building*>(page->base);
        assert(found_instance != nullptr);
    }

    Get_Building_Page_Meta(page_size, *page).count++;
    auto& instance = *found_instance;

    instance.pos = pos;
    instance.active = true;
    instance.scriptable_id = id;

    auto& tile = *(game_map.element_tiles + gsize.x * pos.y + pos.x);
    assert(tile.type == Element_Tile_Type::None);
    tile.type = Element_Tile_Type::Building;
    tile.building = found_instance;
}

Graph_Segment& New_Graph_Segment(Game_State& state) {
    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    auto& os_data = *state.os_data;

    const auto page_size = os_data.page_size;

    Page* page = nullptr;
    Graph_Segment* found_instance = nullptr;
    FOR_RANGE(size_t, page_index, game_map.segment_pages_used) {
        page = game_map.segment_pages + page_index;
        auto& meta = Get_Graph_Segment_Page_Meta(page_size, *page);

        if (meta.count >= game_map.max_segments_per_page)
            continue;

        FOR_RANGE(size_t, segment_index, game_map.max_segments_per_page) {
            auto& instance = *(rcast<Graph_Segment*>(page->base) + segment_index);
            if (!instance.active) {
                found_instance = &instance;
                break;
            }
        }

        if (found_instance != nullptr)
            break;
    }

    if (found_instance == nullptr) {
        assert(game_map.segment_pages_used < game_map.segment_pages_total);
        page = game_map.segment_pages + game_map.segment_pages_used;

        page->base = Book_Single_Page(state);
        game_map.segment_pages_used++;

        found_instance = rcast<Graph_Segment*>(page->base);
        assert(found_instance != nullptr);
    }

    Get_Graph_Segment_Page_Meta(page_size, *page).count++;
    auto& instance = *found_instance;

    instance.active = true;

    return instance;
}

struct Updated_Tiles {
    u16 count;
    v2i* pos;
    Tile_Updated_Type* type;
};

// bool Graph_AABB(Graph& graph, v2i pos) {
//     return Pos_Is_In_Bounds(pos - graph.offset, graph.size);
// }
//
// u8 Graph_Node(Graph& graph, v2i pos) {
//     assert(Graph_AABB(graph, pos));
//
//     auto new_pos = pos - graph.offset;
//     auto& node = *(graph.nodes + graph.size.x * new_pos.y + new_pos.x);
//     return node;
// }
//
// u8 Graph_Node_Safe(Graph& graph, v2i pos) {
//     if (!Pos_Is_In_Bounds(pos - graph.offset, graph.size))
//         return 0;
//
//     auto new_pos = pos - graph.offset;
//     auto& node = *(graph.nodes + graph.size.x * new_pos.y + new_pos.x);
//     return node;
// }

bool Should_Segment_Be_Deleted(
    Game_State& state,
    Updated_Tiles& updated_tiles,
    Graph_Segment& segment  //
) {
    auto& game_map = state.game_map;
    auto& gsize = game_map.size;

    FOR_RANGE(u16, i, updated_tiles.count) {
        auto& tile_pos = *(updated_tiles.pos + i);
        auto& updated_type = *(updated_tiles.type + i);

        switch (updated_type) {
        case Tile_Updated_Type::Road_Placed:
        case Tile_Updated_Type::Building_Placed: {
            for (auto& offset : v2i_adjacent_offsets) {
                auto pos = tile_pos + offset;
                if (!Pos_Is_In_Bounds(pos, gsize))
                    continue;

                auto& graph = segment.graph;
                auto graph_pos = pos;
                graph_pos.x -= graph.offset.x;
                graph_pos.y -= graph.offset.y;

                if (!Pos_Is_In_Bounds(graph_pos, graph.size))
                    continue;

                auto node = *(graph.nodes + graph.size.x * graph_pos.y + graph_pos.x);
                if (node == 0)
                    continue;

                auto& tile = *(game_map.element_tiles + gsize.x * pos.y + pos.x);
                if (tile.type == Element_Tile_Type::Road)
                    return true;
            }
        } break;

        case Tile_Updated_Type::Flag_Placed:
        case Tile_Updated_Type::Flag_Removed:
        case Tile_Updated_Type::Road_Removed:
        case Tile_Updated_Type::Building_Removed: {
            auto pos = tile_pos;
            if (!Pos_Is_In_Bounds(pos, gsize))
                break;

            auto& graph = segment.graph;
            auto graph_pos = pos;
            graph_pos.x -= graph.offset.x;
            graph_pos.y -= graph.offset.y;

            if (!Pos_Is_In_Bounds(graph_pos, graph.size))
                break;

            u8 node = *(graph.nodes + graph.size.x * graph_pos.y + graph_pos.x);
            if (node == 0)
                break;

            return true;
        } break;

        default:
            INVALID_PATH;
        }
    }

    return false;
}

On_Tiles_Updated_Result On_Tiles_Updated(
    Game_State& state,
    Arena& non_pesistent_arena,
    Arena& trash_arena,
    Updated_Tiles& updated_tiles  //
) {
    On_Tiles_Updated_Result res = {};

    auto& game_map = state.game_map;
    auto& gsize = game_map.size;
    auto& element_tiles = game_map.element_tiles;

    auto segments_to_be_deleted_count = 0;
    auto segments_to_be_deleted =
        Allocate_Zeros_Array(trash_arena, Graph_Segment*, updated_tiles.count * 4);

    FOR_RANGE(auto, segment_page_index_, game_map.segment_pages_used) {
        auto page_base = (game_map.segment_pages + segment_page_index_)->base;
        assert(page_base != nullptr);
        auto base_segment_ptr = rcast<Graph_Segment*>(page_base);

        FOR_RANGE(auto, segment_index, game_map.max_segments_per_page) {
            auto segment_ptr = base_segment_ptr + segment_index;
            auto& segment = *segment_ptr;
            if (!segment.active)
                continue;

            if (Should_Segment_Be_Deleted(state, updated_tiles, segment)) {
                // NOTE(hulvdan): Adding it without duplication
                auto found = false;
                FOR_RANGE(int, i, segments_to_be_deleted_count) {
                    auto segment_ptr = *(segments_to_be_deleted + i);
                    if (segment_ptr == &segment) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    assert(segments_to_be_deleted_count < updated_tiles.count * 4);

                    *(segments_to_be_deleted + segments_to_be_deleted_count) = segment_ptr;
                    segments_to_be_deleted_count++;
                }
            }
        }
    }

    return res;
}

bool Try_Build(Game_State& state, v2i pos, const Item_To_Build& item) {
    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    assert(Pos_Is_In_Bounds(pos, gsize));

    auto& tile = *(game_map.element_tiles + pos.y * gsize.x + pos.x);

    switch (item.type) {
    case Item_To_Build_Type::Flag: {
        if (tile.type == Element_Tile_Type::Flag) {
            tile.type = Element_Tile_Type::Road;
            // TODO(hulvdan): Flag_Removed
        } else if (tile.type == Element_Tile_Type::Road) {
            tile.type = Element_Tile_Type::Flag;
            // TODO(hulvdan): Flag_Placed
        } else
            return false;

        assert(tile.building == nullptr);
    } break;

    case Item_To_Build_Type::Road: {
        if (tile.type != Element_Tile_Type::None)
            return false;

        assert(tile.building == nullptr);
        tile.type = Element_Tile_Type::Road;

        // TODO(hulvdan): Road_Placed
    } break;

    case Item_To_Build_Type::Building: {
        if (tile.type != Element_Tile_Type::None)
            return false;

        assert(item.scriptable_building_id != 0);
        Place_Building(state, pos, item.scriptable_building_id);

        // TODO(hulvdan): Building_Placed
    } break;

    default:
        INVALID_PATH;
    }

    INVOKE_OBSERVER(state.On_Item_Built, (state, pos, item));

    return true;
}
