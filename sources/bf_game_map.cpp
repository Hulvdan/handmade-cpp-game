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

    auto noise_pitch = Ceil_To_Power_Of_2((u32)MAX(gsize.x, gsize.y));
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
    // element_tiles = _initialMapProvider.LoadElementTiles();
    //
    // auto cityHalls = buildings.FindAll(i = > i.scriptable.type ==
    // Building_Type.SpecialCityHall); foreach (auto building in cityHalls) {
    //     auto pos = building.pos;
    //     element_tiles[pos.y][pos.x] = new (Element_Tile_Type::Building, building);
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
    const Updated_Tiles& updated_tiles,
    const Graph_Segment& segment  //
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

// TODO(hulvdan): Прикруть `Graph_v2u operator==`
#define Add_Without_Duplication(max_count_, count_, array_, value_)     \
    {                                                                   \
        assert((max_count_) >= (count_));                               \
                                                                        \
        auto found = false;                                             \
        FOR_RANGE(int, i, (count_)) {                                   \
            auto existing = *((array_) + i);                            \
            if (existing.x == (value_).x && existing.y == (value_).y) { \
                found = true;                                           \
                break;                                                  \
            }                                                           \
        }                                                               \
                                                                        \
        if (!found) {                                                   \
            assert((count_) < (max_count_));                            \
            *((array_) + (count_)) = (value_);                          \
            (count_)++;                                                 \
        }                                                               \
    }

Graph_v2u* Allocate_Segment_Vertices(Game_State& state, int vertices_count) {
    auto& game_map = state.game_map;
    auto size_to_book = sizeof(Graph_v2u) * vertices_count;

    NOT_IMPLEMENTED;

    return nullptr;
}

struct Allocation {
    u8* base;
    size_t size;
    bool active;
    size_t next;
};

//
// []                                 first_index = max
// [(v1,max,t)]                       first_index = 0  (added node to the end)
// [(v1,1,t), (v2,max,t)]             first_index = 0  (added node to the end)
// [(v1,1,t), (v2,2,t), (v3,max,t)]   first_index = 0  (added node to the end)
// [(v1,2,t), (v2,2,F), (v3,max,t)]   first_index = 0  (removed node at index pos 1)
// [(v1,2,F), (v2,2,F), (v3,max,t)]   first_index = 2  (removed node at index pos 0)
// [(v4,max,t), (v2,2,F), (v3,0,t)]   first_index = 2  (added node to the end)
// [(v4,1,t), (v5,max,t), (v3,0,t)]   first_index = 2  (added node to the end)
// [(v4,max,t), (v5,max,F), (v3,0,t)]   first_index = 2  (removed node at index pos 1)
// [(v4,max,F), (v5,max,F), (v3,max,t)]   first_index = 2  (removed node at index pos 0)
// [(v4,max,F), (v5,max,F), (v3,max,F)]   first_index = max  (removed node at index pos 2)

void Linked_List_Add(
    u8* nodes,
    size_t& n,
    const size_t first_node_index,
    const u8* const node,
    const size_t active_offset,
    const size_t next_offset,
    const size_t node_size  //
) {
    size_t new_free_node_index = size_t_max;
    u8* new_free_node = nullptr;
    FOR_RANGE(size_t, i, n + 1) {
        u8* n = nodes + i * node_size;
        bool active = *rcast<bool*>(n + active_offset);
        if (active)
            continue;

        new_free_node = n;
        new_free_node_index = i;

        break;
    }

    assert(new_free_node_index != size_t_max);
    assert(new_free_node != nullptr);

    // 1,2,3,4
    // n = 4
    if (n > 0) {
        u8* last_node = nodes + first_node_index * node_size;
        FOR_RANGE(size_t, i, n - 1) {
            assert(*rcast<bool*>(last_node + active_offset) == true);

            auto index_offset = *rcast<size_t*>(last_node + next_offset);
            last_node = nodes + index_offset * node_size;
        }
        assert(*rcast<bool*>(last_node + active_offset) == true);
        *rcast<size_t*>(last_node + next_offset) = new_free_node_index;
    }

    memcpy(new_free_node, node, node_size);
    *rcast<bool*>(new_free_node + active_offset) = true;
    *rcast<size_t*>(new_free_node + next_offset) = size_t_max;

    n++;
}

void Linked_List_Add(
    Allocation* nodes,
    size_t& n,
    const size_t first_node_index,
    const Allocation* const node  //
) {
    auto active_offset = offsetof(Allocation, active);
    auto next_offset = offsetof(Allocation, next);

    Linked_List_Add(
        (u8*)nodes, n, first_node_index, (u8*)node, active_offset, next_offset, sizeof(Allocation));
}

void Linked_List_Remove(Allocation* nodes, size_t& n, size_t a) {
    // node.active = false;
    n--;
}

struct Allocator {
    // int pages_count;
    // int used_pages_count;
    Page* allocations;
    Page* page;

    Allocation* first_allocation;

    u8* Allocate(size_t size) {
        assert(page != nullptr);
        assert(allocations != nullptr);

        auto allocations_ptr = allocations->base;
        if (first_allocation == nullptr) {
            //
        } else {
            //
        }

        auto current_mem_ptr = page->base;
        auto p = page->base;
        // for (;;) {
        //     //
        // }
    }

    void Free(u8* address) {
        //
    }

    // auto a = Allocate(64 * sizeof(Graph_v2u));
    // Free(a);
    // auto b = Allocate(4 * sizeof(Graph_v2u));
    // Free(b);
};

void Free_Segment_Vertices(Game_State& state, u8* ptr) {
    auto& game_map = state.game_map;
    NOT_IMPLEMENTED;
}

u8* Allocate_Graph_Nodes(Game_State& state, int all_nodes_count) {
    NOT_IMPLEMENTED;
    return nullptr;
}

void Rect_Copy(u8* dest, u8* source, int stride, int rows, int bytes_per_line) {
    FOR_RANGE(int, i, rows) {
        memcpy(dest, source + stride * i, bytes_per_line);
    }
}

void Update_Tiles(
    Game_State& state,
    Arena& non_pesistent_arena,
    Arena& trash_arena,
    const Updated_Tiles& updated_tiles  //
) {
    auto& game_map = state.game_map;
    auto& gsize = game_map.size;
    auto& element_tiles = game_map.element_tiles;

    typedef std::tuple<Direction, v2i> Dir_v2i;
    auto tiles_count = gsize.x * gsize.y;

    // NOTE(hulvdan): Ищем сегменты для удаления
    auto segments_to_be_deleted_allocate = updated_tiles.count * 4;
    auto segments_to_be_deleted_count = 0;
    auto segments_to_be_deleted =
        Allocate_Zeros_Array(trash_arena, Graph_Segment*, segments_to_be_deleted_allocate);
    DEFER(Deallocate_Array(trash_arena, Graph_Segment*, segments_to_be_deleted_allocate));

    FOR_RANGE(auto, segment_page_index_, game_map.segment_pages_used) {
        auto page_base = (game_map.segment_pages + segment_page_index_)->base;
        assert(page_base != nullptr);
        auto base_segment_ptr = rcast<Graph_Segment*>(page_base);

        FOR_RANGE(auto, segment_index, game_map.max_segments_per_page) {
            auto segment_ptr = base_segment_ptr + segment_index;
            auto& segment = *segment_ptr;
            if (!segment.active)
                continue;

            if (!Should_Segment_Be_Deleted(state, updated_tiles, segment))
                continue;

            // TODO(hulvdan): Протестить, точно ли тут нужно добавление без дублирования
            // // NOTE(hulvdan): Добавление сегмента без дублирования
            // auto found = false;
            // FOR_RANGE(int, i, segments_to_be_deleted_count) {
            //     auto segment_ptr = *(segments_to_be_deleted + i);
            //     if (segment_ptr == &segment) {
            //         found = true;
            //         break;
            //     }
            // }
            //
            // if (!found) {
            assert(segments_to_be_deleted_count < updated_tiles.count * 4);
            *(segments_to_be_deleted + segments_to_be_deleted_count) = segment_ptr;
            segments_to_be_deleted_count++;
            // }
        }
    }

    // NOTE(hulvdan): Создание новых сегментов
    auto added_segments_allocate = updated_tiles.count * 4;
    auto added_segments_count = 0;
    auto added_segments = Allocate_Zeros_Array(trash_arena, Graph_Segment, added_segments_allocate);
    DEFER(Deallocate_Array(trash_arena, Graph_Segment, added_segments_allocate));

    Fixed_Size_Queue<Dir_v2i> big_fuken_queue = {};
    big_fuken_queue.memory_size = sizeof(Dir_v2i) * tiles_count;
    big_fuken_queue.base = Allocate_Array(trash_arena, Dir_v2i, tiles_count);
    DEFER(Deallocate_Array(trash_arena, Dir_v2i, tiles_count));

    FOR_RANGE(auto, i, updated_tiles.count) {
        const auto& updated_type = *(updated_tiles.type + i);
        const auto& pos = *(updated_tiles.pos + i);

        switch (updated_type) {
        case Tile_Updated_Type::Road_Placed:
        case Tile_Updated_Type::Flag_Placed:
        case Tile_Updated_Type::Flag_Removed: {
            Enqueue(big_fuken_queue, {Direction::Right, pos});
            Enqueue(big_fuken_queue, {Direction::Up, pos});
            Enqueue(big_fuken_queue, {Direction::Left, pos});
            Enqueue(big_fuken_queue, {Direction::Down, pos});
        } break;

        case Tile_Updated_Type::Road_Removed: {
            for (int i = 0; i < 4; i++) {
                auto new_pos = pos + v2i_adjacent_offsets[i];
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = *(element_tiles + gsize.x * new_pos.y + new_pos.x);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                Enqueue(big_fuken_queue, {Direction::Right, new_pos});
                Enqueue(big_fuken_queue, {Direction::Up, new_pos});
                Enqueue(big_fuken_queue, {Direction::Left, new_pos});
                Enqueue(big_fuken_queue, {Direction::Down, new_pos});
            }
        } break;

        case Tile_Updated_Type::Building_Placed: {
            for (int i = 0; i < 4; i++) {
                auto dir = (Direction)i;
                auto new_pos = pos + v2i_adjacent_offsets[i];
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = *(element_tiles + gsize.x * new_pos.y + new_pos.x);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                if (element_tile.type == Element_Tile_Type::Building)
                    continue;

                if (element_tile.type == Element_Tile_Type::Flag)
                    Enqueue(big_fuken_queue, {Opposite(dir), new_pos});
                else
                    Enqueue(big_fuken_queue, {Direction::Right, new_pos});

                Enqueue(big_fuken_queue, {Direction::Up, new_pos});
                Enqueue(big_fuken_queue, {Direction::Left, new_pos});
                Enqueue(big_fuken_queue, {Direction::Down, new_pos});
            }
        } break;

        case Tile_Updated_Type::Building_Removed: {
            NOT_IMPLEMENTED;
        } break;

        default:
            INVALID_PATH;
        }
    }

    u8* visited =  // NOTE(hulvdan): Flags of Direction
        Allocate_Zeros_Array(trash_arena, u8, tiles_count);
    DEFER(Deallocate_Array(trash_arena, u8, tiles_count));

    Fixed_Size_Queue<Dir_v2i> queue = {};
    queue.base = Allocate_Array(trash_arena, Dir_v2i, tiles_count);
    queue.memory_size = sizeof(Dir_v2i) * tiles_count;
    DEFER(Deallocate_Array(trash_arena, Dir_v2i, tiles_count));

    while (big_fuken_queue.count > 0) {
        auto p = Dequeue(big_fuken_queue);
        Enqueue(queue, p);

        int vertices_count = 0;
        Graph_v2u* vertices = Allocate_Zeros_Array(trash_arena, Graph_v2u, tiles_count);
        DEFER(Deallocate_Array(trash_arena, Graph_v2u, tiles_count));

        int segment_tiles_count = 1;
        Graph_v2u* segment_tiles = Allocate_Zeros_Array(trash_arena, Graph_v2u, tiles_count);
        DEFER(Deallocate_Array(trash_arena, Graph_v2u, tiles_count));

        *(segment_tiles + 0) = To_Graph_v2u(std::get<1>(p));

        Graph temp_graph = {};
        temp_graph.nodes = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
        temp_graph.size.x = gsize.x;
        temp_graph.size.y = gsize.y;
        DEFER(Deallocate_Array(trash_arena, u8, tiles_count));

        while (queue.count > 0) {
            auto [dir, pos] = Dequeue(queue);
            auto& tile = *(element_tiles + pos.y * gsize.x + pos.x);

            auto& scriptable = *Get_Scriptable_Building(state, tile.building->scriptable_id);

            auto is_flag = tile.type == Element_Tile_Type::Flag;
            auto is_building = tile.type == Element_Tile_Type::Building;
            auto is_city_hall = is_building && scriptable.type == Building_Type::City_Hall;

            if (is_flag || is_building) {
                auto converted = To_Graph_v2u(pos);
                Add_Without_Duplication(tiles_count, vertices_count, vertices, converted);
                // Add_Without_Duplication(tiles_count, vertices_count, vertices, pos);
            }

            for (int i = 0; i < 4; i++) {
                auto dir_index = (Direction)i;

                if ((is_city_hall || is_flag) && dir_index != dir)
                    continue;

                u8& visited_value = *(visited + gsize.x * pos.y + pos.x);
                if (Graph_Node_Has(visited_value, dir_index))
                    continue;

                v2i new_pos = pos + As_Offset(dir_index);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                Direction opposite_dir_index = Opposite(dir_index);
                u8& new_visited_value = *(visited + gsize.x * new_pos.y + new_pos.x);
                if (Graph_Node_Has(new_visited_value, opposite_dir_index))
                    continue;

                auto& new_tile = *(element_tiles + gsize.x * new_pos.y + new_pos.x);
                if (new_tile.type == Element_Tile_Type::None)
                    continue;

                bool new_is_building = new_tile.type == Element_Tile_Type::Building;
                bool new_is_flag = new_tile.type == Element_Tile_Type::Flag;

                if (is_building && new_is_building)
                    continue;

                if (new_is_flag) {
                    Enqueue(big_fuken_queue, {Direction::Right, new_pos});
                    Enqueue(big_fuken_queue, {Direction::Up, new_pos});
                    Enqueue(big_fuken_queue, {Direction::Left, new_pos});
                    Enqueue(big_fuken_queue, {Direction::Down, new_pos});
                }

                visited_value = Graph_Node_Mark(visited_value, dir_index, true);
                new_visited_value = Graph_Node_Mark(new_visited_value, dir_index, true);
                Graph_Update(temp_graph, pos.x, pos.y, dir_index, true);
                Graph_Update(temp_graph, new_pos.x, new_pos.y, opposite_dir_index, true);

                auto converted = To_Graph_v2u(new_pos);
                Add_Without_Duplication(tiles_count, segment_tiles_count, segment_tiles, converted);

                if (new_is_building || new_is_flag) {
                    Add_Without_Duplication(tiles_count, vertices_count, vertices, converted);
                } else
                    Enqueue(queue, {(Direction)0, new_pos});
            }
        }

        if (vertices_count == 0)
            continue;

        assert(temp_graph.nodes_count > 0);
        // assert(temp_height > 0);
        // assert(width > 0);

        auto& segment = *(added_segments + added_segments_count);
        segment.active = true;
        segment.vertices_count = vertices_count;
        segment.vertices = Allocate_Segment_Vertices(state, vertices_count);
        memcpy(segment.vertices, vertices, sizeof(Graph_v2u) * vertices_count);

        segment.graph.nodes_count = temp_graph.nodes_count;

        // NOTE(hulvdan): Вычисление size и offset графа
        auto& gr_size = segment.graph.size;
        auto& offset = segment.graph.offset;
        offset.x = gsize.x - 1;
        offset.y = gsize.y - 1;

        FOR_RANGE(int, y, gsize.y) {
            FOR_RANGE(int, x, gsize.x) {
                auto& node = *(temp_graph.nodes + y * gsize.x + x);
                if (node) {
                    gr_size.x = MAX(gr_size.x, x);
                    gr_size.y = MAX(gr_size.y, y);
                } else {
                    offset.x = MIN(offset.x, x);
                    offset.y = MIN(offset.y, y);
                }
            }
        }
        gr_size.x -= offset.x;
        gr_size.y -= offset.y;

        assert(gr_size.x > 0);
        assert(gr_size.y > 0);
        assert(offset.x >= 0);
        assert(offset.y >= 0);
        assert(offset.x < gsize.x);
        assert(offset.y < gsize.y);

        // NOTE(hulvdan): Копирование нод из временного графа
        // с небольшой оптимизацией по требуемой памяти
        auto all_nodes_count = gr_size.x * gr_size.y;
        segment.graph.nodes = Allocate_Graph_Nodes(state, all_nodes_count);

        auto rows = gr_size.y;
        auto stride = gr_size.x;
        auto starting_node = temp_graph.nodes + offset.y * gsize.x + offset.x;
        Rect_Copy(segment.graph.nodes, starting_node, stride, rows, gr_size.x);
    }

    // ====================================================================================
    // FROM C# REPO void UpdateSegments(ItemTransportationGraph.OnTilesUpdatedResult res) {
    //
    // using auto _ = Tracing.Scope();
    //
    // if (!_hideEditorLogs) {
    //     Debug.Log($"{res.addedSegments.Count} segments added, {res.deletedSegments}
    //     deleted");
    // }
    //
    // auto humansMovingToCityHall = 0;
    // foreach (auto human in _humans) {
    //     auto state = MovingInTheWorld.State.MovingToTheCityHall;
    //     if (human.stateMovingInTheWorld == state) {
    //         humansMovingToCityHall++;
    //     }
    // }
    //
    // Stack<Tuple<GraphSegment?, Human>> humansThatNeedNewSegment =
    //     new(res.deletedSegments.Count + humansMovingToCityHall);
    // foreach (auto human in _humans) {
    //     auto state = MovingInTheWorld.State.MovingToTheCityHall;
    //     if (human.stateMovingInTheWorld == state) {
    //         humansThatNeedNewSegment.Push(new(null, human));
    //     }
    // }
    //
    // foreach (auto segment in res.deletedSegments) {
    //     segments.Remove(segment);
    //
    //     auto human = segment.assignedHuman;
    //     if (human != null) {
    //         human.segment = null;
    //         segment.assignedHuman = null;
    //         humansThatNeedNewSegment.Push(new(segment, human));
    //     }
    //
    //     foreach (auto linkedSegment in segment.linkedSegments) {
    //         linkedSegment.Unlink(segment);
    //     }
    //
    //     _resourceTransportation.OnSegmentDeleted(segment);
    //
    //     if (segmentsThatNeedHumans.Contains(segment)) {
    //         segmentsThatNeedHumans.Remove(segment);
    //         Assert.IsFalse(segmentsThatNeedHumans.Contains(segment));
    //     }
    // }
    //
    // if (!_hideEditorLogs) {
    //     Debug.Log($"{humansThatNeedNewSegment.Count} Humans need to find new segments");
    // }
    //
    // foreach (auto segment in res.addedSegments) {
    //     foreach (auto segmentToLink in segments) {
    //         // Mb there Graph.CollidesWith(other.Graph) is needed for optimization
    //         if (segmentToLink.HasSomeOfTheSameVertices(segment)) {
    //             segment.Link(segmentToLink);
    //             segmentToLink.Link(segment);
    //         }
    //     }
    //
    //     segments.Add(segment);
    // }
    //
    // _resourceTransportation.PathfindItemsInQueue();
    // Tracing.Log("_itemTransportationSystem.PathfindItemsInQueue()");
    //
    // while (humansThatNeedNewSegment.Count > 0 && segmentsThatNeedHumans.Count > 0) {
    //     auto segment = segmentsThatNeedHumans.Dequeue();
    //
    //     auto (oldSegment, human) = humansThatNeedNewSegment.Pop();
    //     human.segment = segment;
    //     segment.assignedHuman = human;
    //     _humanController.OnHumanCurrentSegmentChanged(human, oldSegment);
    // }
    //
    // foreach (auto segment in res.addedSegments) {
    //     if (humansThatNeedNewSegment.Count == 0) {
    //         segmentsThatNeedHumans.Enqueue(segment, 0);
    //         continue;
    //     }
    //
    //     auto (oldSegment, human) = humansThatNeedNewSegment.Pop();
    //     human.segment = segment;
    //     segment.assignedHuman = human;
    //     _humanController.OnHumanCurrentSegmentChanged(human, oldSegment);
    // }
    //
    // // Assert that segments don't have tiles with identical directions
    // for (auto i = 0; i < segments.Count; i++) {
    //     for (auto j = 0; j < segments.Count; j++) {
    //         if (i == j) {
    //             continue;
    //         }
    //
    //         auto g1 = segments[i].graph;
    //         auto g2 = segments[j].graph;
    //         for (auto y = 0; y < g1.height; y++) {
    //             for (auto x = 0; x < g1.width; x++) {
    //                 auto g1X = x + g1.offset.x;
    //                 auto g1Y = y + g1.offset.y;
    //                 if (!g2.Contains(g1X, g1Y)) {
    //                     continue;
    //                 }
    //
    //                 auto g2Y = g1Y - g2.offset.y;
    //                 auto g2X = g1X - g2.offset.x;
    //                 auto node = g2.nodes[g2Y][g2X];
    //
    //                 Assert.AreEqual(node & g1.nodes[y][x], 0);
    //             }
    //         }
    //     }
    // }
}

#define Declare_Updated_Tiles(variable_name_, pos_, type_) \
    Updated_Tiles(variable_name_) = {};                    \
    (variable_name_).count = 1;                            \
    (variable_name_).pos = &(pos_);                        \
    auto type__ = (type_);                                 \
    (variable_name_).type = &type__;

bool Try_Build(Game_State& state, v2i pos, const Item_To_Build& item) {
    auto& arena = state.arena;
    auto& non_persistent_arena = state.non_persistent_arena;
    auto& trash_arena = state.trash_arena;

    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    assert(Pos_Is_In_Bounds(pos, gsize));

    auto& tile = *(game_map.element_tiles + pos.y * gsize.x + pos.x);

    switch (item.type) {
    case Item_To_Build_Type::Flag: {
        if (tile.type == Element_Tile_Type::Flag) {
            tile.type = Element_Tile_Type::Road;
            Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Flag_Removed);
            Update_Tiles(state, non_persistent_arena, trash_arena, updated_tiles);
        }  //
        else if (tile.type == Element_Tile_Type::Road) {
            tile.type = Element_Tile_Type::Flag;
            Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Flag_Placed);
            Update_Tiles(state, non_persistent_arena, trash_arena, updated_tiles);
        }  //
        else
            return false;

        assert(tile.building == nullptr);
    } break;

    case Item_To_Build_Type::Road: {
        if (tile.type != Element_Tile_Type::None)
            return false;

        assert(tile.building == nullptr);
        tile.type = Element_Tile_Type::Road;

        Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Road_Placed);
        Update_Tiles(state, non_persistent_arena, trash_arena, updated_tiles);
    } break;

    case Item_To_Build_Type::Building: {
        if (tile.type != Element_Tile_Type::None)
            return false;

        assert(item.scriptable_building_id != 0);
        Place_Building(state, pos, item.scriptable_building_id);

        Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Building_Placed);
        Update_Tiles(state, non_persistent_arena, trash_arena, updated_tiles);
    } break;

    default:
        INVALID_PATH;
    }

    INVOKE_OBSERVER(state.On_Item_Built, (state, pos, item));

    return true;
}
