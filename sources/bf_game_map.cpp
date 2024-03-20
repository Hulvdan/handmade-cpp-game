#pragma once

Scriptable_Resource* Get_Scriptable_Resource(
    Game_State& state,
    Scriptable_Resource_ID id) {
    Assert(id - 1 < state.scriptable_resources_count);
    auto exists = id != 0;
    auto ptr_offset = (ptrd)(state.scriptable_resources + id - 1);
    auto result = ptr_offset * exists;
    return rcast<Scriptable_Resource*>(result);
}

Terrain_Tile& Get_Terrain_Tile(Game_Map& game_map, v2i pos) {
    Assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_tiles + pos.y * game_map.size.x + pos.x);
}

Terrain_Resource& Get_Terrain_Resource(Game_Map& game_map, v2i pos) {
    Assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_resources + pos.y * game_map.size.x + pos.x);
}

void Initialize_Game_Map(Game_State& state, Arena& arena) {
    auto& game_map = state.game_map;

    {
        size_t toc_size = 1024;
        size_t data_size = 4096;

        auto buf = Allocate_Zeros_For(arena, Allocator);
        new (buf) Allocator(
            toc_size, Allocate_Zeros_Array(arena, u8, toc_size),  //
            data_size, Allocate_Array(arena, u8, data_size));

        game_map.segment_vertices_allocator = buf;
        Set_Allocator_Name_If_Profiling(
            arena, *buf, "segment_vertices_allocator_%d", state.dll_reloads_count);
    }
    {
        size_t toc_size = 1024;
        size_t data_size = 4096;

        auto buf = Allocate_Zeros_For(arena, Allocator);
        new (buf) Allocator(
            toc_size, Allocate_Zeros_Array(arena, u8, toc_size),  //
            data_size, Allocate_Array(arena, u8, data_size));

        game_map.graph_nodes_allocator = buf;
        Set_Allocator_Name_If_Profiling(
            arena, *buf, "graph_nodes_allocator_%d", state.dll_reloads_count);
    }
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
        terrain_perlin, sizeof(u16) * output_size, trash_arena, data.terrain_perlin,
        noise_pitch, noise_pitch);

    auto forest_perlin = Allocate_Array(trash_arena, u16, output_size);
    DEFER(Deallocate_Array(trash_arena, u16, output_size));
    Fill_Perlin_2D(
        forest_perlin, sizeof(u16) * output_size, trash_arena, data.forest_perlin,
        noise_pitch, noise_pitch);

    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = Get_Terrain_Tile(game_map, {x, y});
            tile.terrain = Terrain::Grass;
            auto noise = *(terrain_perlin + noise_pitch * y + x) / (f32)u16_max;
            tile.height = int((data.terrain_max_height + 1) * noise);

            Assert(tile.height >= 0);
            Assert(tile.height <= data.terrain_max_height);
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

                auto should_change =
                    tile.height > height_below && tile.height > height_above;
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

            tile.is_cliff =
                y == 0 || tile.height > Get_Terrain_Tile(game_map, {x, y - 1}).height;
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
        Assert(tile.building == nullptr);
    }

    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            Element_Tile& tile = *(game_map.element_tiles + y * gsize.x + x);
            Validate_Element_Tile(tile);
        }
    }
}

Building_Page_Meta& Get_Building_Page_Meta(size_t page_size, Page& page) {
    return *rcast<Building_Page_Meta*>(
        page.base + page_size - sizeof(Building_Page_Meta));
}

Graph_Segment_Page_Meta& Get_Graph_Segment_Page_Meta(
    size_t page_size,
    Page& page  //
) {
    return *rcast<Graph_Segment_Page_Meta*>(
        page.base + page_size - sizeof(Graph_Segment_Page_Meta));
}

void Place_Building(Game_State& state, v2i pos, Scriptable_Building* scriptable) {
    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    auto& os_data = *state.os_data;

    const auto page_size = os_data.page_size;
    Assert(Pos_Is_In_Bounds(pos, gsize));

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
        Assert(game_map.building_pages_used < game_map.building_pages_total);
        page = game_map.building_pages + game_map.building_pages_used;

        page->base = Book_Single_Page(state.pages, *state.os_data);
        game_map.building_pages_used++;

        found_instance = rcast<Building*>(page->base);
        Assert(found_instance != nullptr);
    }

    Get_Building_Page_Meta(page_size, *page).count++;
    auto& instance = *found_instance;

    instance.pos = pos;
    instance.active = true;
    instance.scriptable = scriptable;

    auto& tile = *(game_map.element_tiles + gsize.x * pos.y + pos.x);
    Assert(tile.type == Element_Tile_Type::None);
    tile.type = Element_Tile_Type::Building;
    tile.building = found_instance;
}

Graph_Segment&
New_Graph_Segment(Segment_Manager& manager, OS_Data& os_data, Pages& pages) {
    const auto page_size = os_data.page_size;

    Page* page = nullptr;
    Graph_Segment* found_instance = nullptr;
    FOR_RANGE(size_t, page_index, manager.segment_pages_used) {
        page = manager.segment_pages + page_index;
        auto& meta = Get_Graph_Segment_Page_Meta(page_size, *page);

        if (meta.count >= manager.max_segments_per_page)
            continue;

        FOR_RANGE(size_t, segment_index, manager.max_segments_per_page) {
            auto instance = rcast<Graph_Segment*>(page->base) + segment_index;
            if (!instance->active) {
                found_instance = instance;
                break;
            }
        }

        if (found_instance != nullptr)
            break;
    }

    if (found_instance == nullptr) {
        Assert(manager.segment_pages_used < manager.segment_pages_total);
        page = manager.segment_pages + manager.segment_pages_used;

        page->base = Book_Single_Page(pages, os_data);
        manager.segment_pages_used++;

        found_instance = rcast<Graph_Segment*>(page->base);
        Assert(found_instance != nullptr);
    }

    Get_Graph_Segment_Page_Meta(page_size, *page).count++;
    auto& instance = *found_instance;

    Assert(!instance.active);
    instance.active = true;

    return instance;
}

struct Updated_Tiles {
    u16 count;
    v2i* pos;
    Tile_Updated_Type* type;
};

bool Should_Segment_Be_Deleted(
    v2i gsize,
    Element_Tile* element_tiles,
    const Updated_Tiles& updated_tiles,
    const Graph_Segment& segment  //
) {
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

                auto& tile = *(element_tiles + gsize.x * pos.y + pos.x);
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
        Assert((max_count_) >= (count_));                               \
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
            Assert((count_) < (max_count_));                            \
            *((array_) + (count_)) = (value_);                          \
            (count_)++;                                                 \
        }                                                               \
    }

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

size_t Linked_List_Push_Back(
    u8* const nodes,
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
    Assert(new_free_node_index != size_t_max);
    Assert(new_free_node != nullptr);

    if (n > 0) {
        u8* last_node = nodes + first_node_index * node_size;
        FOR_RANGE(size_t, i, n - 1) {
            Assert(*rcast<bool*>(last_node + active_offset) == true);
            auto index_offset = *rcast<size_t*>(last_node + next_offset);
            last_node = nodes + index_offset * node_size;
        }

        Assert(*rcast<bool*>(last_node + active_offset) == true);
        *rcast<size_t*>(last_node + next_offset) = new_free_node_index;
    }

    memcpy(new_free_node, node, node_size);
    *rcast<bool*>(new_free_node + active_offset) = true;
    *rcast<size_t*>(new_free_node + next_offset) = size_t_max;

    n++;
    return new_free_node_index;
}

void Linked_List_Remove_At(
    u8* const nodes,
    size_t& n,
    size_t& first_node_index,
    const size_t node_index,
    const size_t active_offset,
    const size_t next_offset,
    const size_t node_size  //
) {
    Assert(n > 0);

    if (node_index == first_node_index) {
        auto node = nodes + node_size * first_node_index;
        *rcast<bool*>(node + active_offset) = false;

        auto next = *rcast<size_t*>(node + next_offset);
        first_node_index = next * (next != size_t_max);

        n--;
        return;
    }

    FOR_RANGE(size_t, i, n) {
        auto node = nodes + node_size * i;
        auto next_index = *rcast<size_t*>(node + next_offset);

        if (next_index == node_index) {
            u8* node_to_delete = nodes + next_index * node_size;
            auto node_to_delete_next_index =
                *rcast<size_t*>(node_to_delete + next_offset);

            auto next_exists = node_to_delete_next_index != size_t_max;
            if (next_exists)
                *rcast<size_t*>(node + next_offset) = node_to_delete_next_index;
            else
                *rcast<size_t*>(node + next_offset) = size_t_max;
        }

        if (i == node_index) {
            *rcast<bool*>(node + active_offset) = false;
            n--;
            return;
        }
    }

    Assert(false);
}

tuple<size_t, Graph_v2u*> Allocate_Segment_Vertices(
    Allocator& allocator,
    int vertices_count) {
    auto [key, buffer] = allocator.Allocate(vertices_count, 1);
    return {key, (Graph_v2u*)buffer};
}

tuple<size_t, u8*> Allocate_Graph_Nodes(Allocator& allocator, int nodes_count) {
    auto [key, buffer] = allocator.Allocate(nodes_count, 1);
    return {key, buffer};
}

void Rect_Copy(u8* dest, u8* source, int stride, int rows, int bytes_per_line) {
    FOR_RANGE(int, i, rows) {
        memcpy(dest + i * bytes_per_line, source + i * stride, bytes_per_line);
    }
}

class Graph_Segment_Iterator : public Iterator_Facade<Graph_Segment_Iterator> {
public:
    Graph_Segment_Iterator() = delete;

    Graph_Segment_Iterator(Segment_Manager* manager)
        : Graph_Segment_Iterator(manager, 0, 0) {}
    Graph_Segment_Iterator(Segment_Manager* manager, u32 current, u32 current_page)
        : _current(current),
          _current_page(current_page),
          _manager(manager)  //
    {
        Assert(manager->max_segments_per_page > 0);
    }

    Graph_Segment_Iterator begin() const { return {_manager, _current, _current_page}; }
    Graph_Segment_Iterator end() const {
        return {_manager, 0, _manager->segment_pages_used};
    }

    Graph_Segment* dereference() const {
        auto page_base = (_manager->segment_pages + _current_page)->base;
        auto result = rcast<Graph_Segment*>(page_base) + _current;
        return result;
    }

    void increment() {
        FOR_RANGE(int, _GUARD_, 256) {
            _current++;
            if (_current >= _manager->max_segments_per_page) {
                _current = 0;
                _current_page++;

                if (_current_page == _manager->segment_pages_used)
                    return;
            }

            if (dereference()->active)
                return;
        }
        Assert(false);
    }

    bool equal_to(const Graph_Segment_Iterator& o) const {
        return _current == o._current && _current_page == o._current_page;
    }

private:
    Segment_Manager* _manager;
    u16 _current = 0;
    u16 _current_page = 0;
};

auto Iter(Segment_Manager& manager) {
    return Graph_Segment_Iterator(&manager);
}

// struct Update_Tiles_Operations_To_Apply {
//     size_t trash_arena_allocation;
//
//     u32 segments_to_be_deleted_count;
//     Graph_Segment** segments_to_be_deleted;
//
//     u32 added_segments_count;
//     Graph_Segment* added_segments;
// };
//
// Update_Tiles_Operations_To_Apply Update_Tiles_Gather_Operations(
//     Game_State& state,
//     Arena& non_pesistent_arena,
//     Arena& trash_arena,
//     const Updated_Tiles& updated_tiles  //
// ) {
//     //
// }

typedef tuple<Direction, v2i> Dir_v2i;

void Update_Graphs(
    const v2i gsize,
    const Element_Tile* const element_tiles,
    Graph_Segment* const added_segments,
    u32& added_segments_count,
    Fixed_Size_Queue<Dir_v2i>& big_queue,
    Fixed_Size_Queue<Dir_v2i>& queue,
    Arena& trash_arena,
    u8* visited,
    Allocator& segment_vertices_allocator,
    Allocator& graph_nodes_allocator,
    const bool full_graph_build  //
) {
    auto tiles_count = gsize.x * gsize.y;

    while (big_queue.count > 0) {
        auto p = Dequeue(big_queue);
        Enqueue(queue, p);

        int vertices_count = 0;
        Graph_v2u* vertices = Allocate_Zeros_Array(trash_arena, Graph_v2u, tiles_count);
        DEFER(Deallocate_Array(trash_arena, Graph_v2u, tiles_count));

        int segment_tiles_count = 1;
        Graph_v2u* segment_tiles =
            Allocate_Zeros_Array(trash_arena, Graph_v2u, tiles_count);
        DEFER(Deallocate_Array(trash_arena, Graph_v2u, tiles_count));

        auto [_, p_pos] = p;
        *(segment_tiles + 0) = To_Graph_v2u(p_pos);

        Graph temp_graph = {};
        temp_graph.nodes = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
        temp_graph.size.x = gsize.x;
        temp_graph.size.y = gsize.y;
        DEFER(Deallocate_Array(trash_arena, u8, tiles_count));

        while (queue.count > 0) {
            auto [dir, pos] = Dequeue(queue);
            auto& tile = *(element_tiles + pos.y * gsize.x + pos.x);

            auto is_flag = tile.type == Element_Tile_Type::Flag;
            auto is_building = tile.type == Element_Tile_Type::Building;
            auto is_city_hall = is_building &&
                tile.building->scriptable->type == Building_Type::City_Hall;

            if (is_flag || is_building) {
                auto converted = To_Graph_v2u(pos);
                Add_Without_Duplication(tiles_count, vertices_count, vertices, converted);
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

                if (full_graph_build && new_is_flag) {
                    Enqueue(big_queue, {Direction::Right, new_pos});
                    Enqueue(big_queue, {Direction::Up, new_pos});
                    Enqueue(big_queue, {Direction::Left, new_pos});
                    Enqueue(big_queue, {Direction::Down, new_pos});
                }

                visited_value = Graph_Node_Mark(visited_value, dir_index, true);
                new_visited_value =
                    Graph_Node_Mark(new_visited_value, opposite_dir_index, true);
                Graph_Update(temp_graph, pos.x, pos.y, dir_index, true);
                Graph_Update(temp_graph, new_pos.x, new_pos.y, opposite_dir_index, true);

                auto converted = To_Graph_v2u(new_pos);
                Add_Without_Duplication(
                    tiles_count, segment_tiles_count, segment_tiles, converted);

                if (new_is_building || new_is_flag) {
                    Add_Without_Duplication(
                        tiles_count, vertices_count, vertices, converted);
                } else {
                    Enqueue(queue, {(Direction)0, new_pos});
                }
            }
        }

        if (vertices_count <= 1)
            continue;

        // NOTE(hulvdan): Adding a new segment
        Assert(temp_graph.nodes_count > 0);
        // Assert(temp_height > 0);
        // Assert(width > 0);

        auto& segment = *(added_segments + added_segments_count);
        Assert(!segment.active);
        segment.active = true;
        segment.vertices_count = vertices_count;
        added_segments_count++;

        auto [vertices_key, verticesss] =
            Allocate_Segment_Vertices(segment_vertices_allocator, vertices_count);
        segment.vertices = verticesss;
        segment.vertices_key = vertices_key;
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
                    offset.x = MIN(offset.x, x);
                    offset.y = MIN(offset.y, y);
                }
            }
        }
        gr_size.x -= offset.x;
        gr_size.y -= offset.y;
        gr_size.x += 1;
        gr_size.y += 1;

        Assert(gr_size.x > 0);
        Assert(gr_size.y > 0);
        Assert(offset.x >= 0);
        Assert(offset.y >= 0);
        Assert(offset.x < gsize.x);
        Assert(offset.y < gsize.y);

        // NOTE(hulvdan): Копирование нод из временного графа
        // с небольшой оптимизацией по требуемой памяти
        auto all_nodes_count = gr_size.x * gr_size.y;
        auto [nodes_key, nodesss] =
            Allocate_Graph_Nodes(graph_nodes_allocator, all_nodes_count);
        segment.graph.nodes = nodesss;
        segment.graph.nodes_key = nodes_key;

        auto rows = gr_size.y;
        auto stride = gsize.x;
        auto starting_node = temp_graph.nodes + offset.y * gsize.x + offset.x;
        Rect_Copy(segment.graph.nodes, starting_node, stride, rows, gr_size.x);
    }
}

void Build_Graph_Segments(
    v2i gsize,
    Element_Tile* element_tiles,
    Segment_Manager& segment_manager,
    Arena& trash_arena,
    Allocator& segment_vertices_allocator,
    Allocator& graph_nodes_allocator,
    Pages& pages,
    OS_Data& os_data  //
) {
    Assert(segment_manager.segment_pages_used == 0);

    auto tiles_count = gsize.x * gsize.y;

    // NOTE(hulvdan): Создание новых сегментов
    auto added_segments_allocate = tiles_count * 4;
    u32 added_segments_count = 0;
    Graph_Segment* added_segments =
        Allocate_Zeros_Array(trash_arena, Graph_Segment, added_segments_allocate);
    DEFER(Deallocate_Array(trash_arena, Graph_Segment, added_segments_allocate));

    v2i pos = -v2i_one;
    bool found = false;
    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = *(element_tiles + y * gsize.x + x);

            if (tile.type == Element_Tile_Type::Flag ||
                tile.type == Element_Tile_Type::Building) {
                pos = {x, y};
                found = true;
                break;
            }
        }

        if (found)
            break;
    }

    if (!found)
        return;

    Fixed_Size_Queue<Dir_v2i> big_queue = {};
    big_queue.memory_size = sizeof(Dir_v2i) * tiles_count;
    big_queue.base = Allocate_Array(trash_arena, Dir_v2i, tiles_count);
    DEFER(Deallocate_Array(trash_arena, Dir_v2i, tiles_count));

    Enqueue(big_queue, {Direction::Right, pos});
    Enqueue(big_queue, {Direction::Up, pos});
    Enqueue(big_queue, {Direction::Left, pos});
    Enqueue(big_queue, {Direction::Down, pos});

    Fixed_Size_Queue<Dir_v2i> queue = {};
    queue.base = Allocate_Array(trash_arena, Dir_v2i, tiles_count);
    queue.memory_size = sizeof(Dir_v2i) * tiles_count;
    DEFER(Deallocate_Array(trash_arena, Dir_v2i, tiles_count));

    u8* visited = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
    DEFER(Deallocate_Array(trash_arena, u8, tiles_count));

    bool full_graph_build = true;
    Update_Graphs(
        gsize, element_tiles, added_segments, added_segments_count, big_queue, queue,
        trash_arena, visited, segment_vertices_allocator, graph_nodes_allocator,
        full_graph_build);

    {
        FOR_RANGE(int, i, added_segments_count) {
            auto& segment = New_Graph_Segment(segment_manager, os_data, pages);
            auto& added_segment = *(added_segments + i);

            // TODO(hulvdan): use move semantics?
            segment.vertices_count = added_segment.vertices_count;
            segment.vertices_key = added_segment.vertices_key;
            segment.vertices = added_segment.vertices;
            segment.graph.nodes_count = added_segment.graph.nodes_count;
            segment.graph.nodes_key = added_segment.graph.nodes_key;
            segment.graph.nodes = added_segment.graph.nodes;
            segment.graph.size = added_segment.graph.size;
            segment.graph.offset = added_segment.graph.offset;

            // SHIT(hulvdan): Do it later
            // FROM C# REPO
            // foreach (auto segmentToLink in segments) {
            //     // Mb there Graph.CollidesWith(other.Graph) is needed for optimization
            //     if (segmentToLink.HasSomeOfTheSameVertices(segment)) {
            //         segment.Link(segmentToLink);
            //         segmentToLink.Link(segment);
            //     }
            // }
        }
    }
}

void Update_Tiles(
    v2i gsize,
    Element_Tile* element_tiles,
    Segment_Manager& segment_manager,
    Arena& trash_arena,
    Allocator& segment_vertices_allocator,
    Allocator& graph_nodes_allocator,
    Pages& pages,
    OS_Data& os_data,
    const Updated_Tiles& updated_tiles  //
) {
    auto tiles_count = gsize.x * gsize.y;

    // NOTE(hulvdan): Ищем сегменты для удаления
    auto segments_to_be_deleted_allocate = updated_tiles.count * 4;
    u32 segments_to_be_deleted_count = 0;
    Graph_Segment** segments_to_be_deleted = Allocate_Zeros_Array(
        trash_arena, Graph_Segment*, segments_to_be_deleted_allocate);
    DEFER(Deallocate_Array(trash_arena, Graph_Segment*, segments_to_be_deleted_allocate));

    for (auto segment_ptr : Iter(segment_manager)) {
        auto& segment = *segment_ptr;
        Assert(segment.active);

        if (!Should_Segment_Be_Deleted(gsize, element_tiles, updated_tiles, segment))
            continue;

        Assert(segments_to_be_deleted_count < updated_tiles.count * 4);
        *(segments_to_be_deleted + segments_to_be_deleted_count) = segment_ptr;
        segments_to_be_deleted_count++;
    };

    // NOTE(hulvdan): Создание новых сегментов
    auto added_segments_allocate = updated_tiles.count * 4;
    u32 added_segments_count = 0;
    Graph_Segment* added_segments =
        Allocate_Zeros_Array(trash_arena, Graph_Segment, added_segments_allocate);
    DEFER(Deallocate_Array(trash_arena, Graph_Segment, added_segments_allocate));

    Fixed_Size_Queue<Dir_v2i> big_queue = {};
    big_queue.memory_size = sizeof(Dir_v2i) * tiles_count;
    big_queue.base = Allocate_Array(trash_arena, Dir_v2i, tiles_count);
    DEFER(Deallocate_Array(trash_arena, Dir_v2i, tiles_count));

    FOR_RANGE(auto, i, updated_tiles.count) {
        const auto& updated_type = *(updated_tiles.type + i);
        const auto& pos = *(updated_tiles.pos + i);

        switch (updated_type) {
        case Tile_Updated_Type::Road_Placed:
        case Tile_Updated_Type::Flag_Placed:
        case Tile_Updated_Type::Flag_Removed: {
            Enqueue(big_queue, {Direction::Right, pos});
            Enqueue(big_queue, {Direction::Up, pos});
            Enqueue(big_queue, {Direction::Left, pos});
            Enqueue(big_queue, {Direction::Down, pos});
        } break;

        case Tile_Updated_Type::Road_Removed: {
            for (int i = 0; i < 4; i++) {
                auto new_pos = pos + v2i_adjacent_offsets[i];
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = *(element_tiles + gsize.x * new_pos.y + new_pos.x);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                Enqueue(big_queue, {Direction::Right, new_pos});
                Enqueue(big_queue, {Direction::Up, new_pos});
                Enqueue(big_queue, {Direction::Left, new_pos});
                Enqueue(big_queue, {Direction::Down, new_pos});
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

                if (element_tile.type == Element_Tile_Type::Flag) {
                    Enqueue(big_queue, {Opposite(dir), new_pos});
                } else {
                    Enqueue(big_queue, {Direction::Right, new_pos});
                    Enqueue(big_queue, {Direction::Up, new_pos});
                    Enqueue(big_queue, {Direction::Left, new_pos});
                    Enqueue(big_queue, {Direction::Down, new_pos});
                }
            }
        } break;

        case Tile_Updated_Type::Building_Removed: {
            NOT_IMPLEMENTED;
        } break;

        default:
            INVALID_PATH;
        }
    }

    // NOTE(hulvdan): Each byte here contains differently bit-shifted values of
    // `Direction`
    u8* visited = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
    DEFER(Deallocate_Array(trash_arena, u8, tiles_count));

    Fixed_Size_Queue<Dir_v2i> queue = {};
    queue.base = Allocate_Array(trash_arena, Dir_v2i, tiles_count);
    queue.memory_size = sizeof(Dir_v2i) * tiles_count;
    DEFER(Deallocate_Array(trash_arena, Dir_v2i, tiles_count));

    bool full_graph_build = false;
    Update_Graphs(
        gsize, element_tiles, added_segments, added_segments_count, big_queue, queue,
        trash_arena, visited, segment_vertices_allocator, graph_nodes_allocator,
        full_graph_build);

    // ====================================================================================
    // FROM C# REPO void UpdateSegments(ItemTransportationGraph.OnTilesUpdatedResult res)

    // SHIT(hulvdan): Do it later
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

    {
        FOR_RANGE(u32, i, segments_to_be_deleted_count) {
            Graph_Segment& segment = **(segments_to_be_deleted + i);
            segment_vertices_allocator.Free(segment.vertices_key);
            graph_nodes_allocator.Free(segment.graph.nodes_key);
            segment.active = false;

            // SHIT(hulvdan): Do it later
            // FROM C# REPO
            // auto human = segment.assignedHuman;
            // if (human != null) {
            //     human.segment = null;
            //     segment.assignedHuman = null;
            //     humansThatNeedNewSegment.Push(new(segment, human));
            // }
            //
            // foreach (auto linkedSegment in segment.linkedSegments) {
            //     linkedSegment.Unlink(segment);
            // }
            //
            // _resourceTransportation.OnSegmentDeleted(segment);
            //
            // if (segmentsThatNeedHumans.Contains(segment)) {
            //     segmentsThatNeedHumans.Remove(segment);
            //     Assert.IsFalse(segmentsThatNeedHumans.Contains(segment));
            // }
        }
    }

    {
        FOR_RANGE(int, i, added_segments_count) {
            auto& segment = New_Graph_Segment(segment_manager, os_data, pages);
            auto& added_segment = *(added_segments + i);

            // TODO(hulvdan): use move semantics?
            segment.vertices_count = added_segment.vertices_count;
            segment.vertices_key = added_segment.vertices_key;
            segment.vertices = added_segment.vertices;
            segment.graph.nodes_count = added_segment.graph.nodes_count;
            segment.graph.nodes_key = added_segment.graph.nodes_key;
            segment.graph.nodes = added_segment.graph.nodes;
            segment.graph.size = added_segment.graph.size;
            segment.graph.offset = added_segment.graph.offset;

            // SHIT(hulvdan): Do it later
            // FROM C# REPO
            // foreach (auto segmentToLink in segments) {
            //     // Mb there Graph.CollidesWith(other.Graph) is needed for optimization
            //     if (segmentToLink.HasSomeOfTheSameVertices(segment)) {
            //         segment.Link(segmentToLink);
            //         segmentToLink.Link(segment);
            //     }
            // }
        }
    }

    // SHIT(hulvdan): Do it later
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

#ifdef ASSERT_SLOW
    for (auto segment1_ptr : Iter(segment_manager)) {
        auto& segment1 = *segment1_ptr;
        Assert(segment1.active);

        auto& g1 = segment1.graph;
        v2i g1_offset = {g1.offset.x, g1.offset.y};

        for (auto segment2_ptr : Iter(segment_manager)) {
            auto& segment2 = *segment2_ptr;
            Assert(segment2.active);

            if (segment1_ptr == segment2_ptr)
                continue;

            auto& g2 = segment2.graph;
            v2i g2_offset = {g2.offset.x, g2.offset.y};

            for (auto y = 0; y < g1.size.y; y++) {
                for (auto x = 0; x < g1.size.x; x++) {
                    v2i g1p_world = v2i(x, y) + g1_offset;
                    v2i g2p_local = g1p_world - g2_offset;
                    if (!Pos_Is_In_Bounds(g2p_local, g2.size))
                        continue;

                    u8 node1 = *(g1.nodes + y * g1.size.x + x);
                    u8 node2 = *(g2.nodes + g2p_local.y * g2.size.x + g2p_local.x);
                    bool no_intersections = (node1 & node2) == 0;
                    Assert(no_intersections);
                }
            }
        }
    }
#endif  // ASSERT_SLOW
}

#define Declare_Updated_Tiles(variable_name_, pos_, type_) \
    Updated_Tiles(variable_name_) = {};                    \
    (variable_name_).count = 1;                            \
    (variable_name_).pos = &(pos_);                        \
    auto type__ = (type_);                                 \
    (variable_name_).type = &type__;

#define INVOKE_UPDATE_TILES                                                            \
    Update_Tiles(                                                                      \
        state.game_map.size, state.game_map.element_tiles,                             \
        state.game_map.segment_manager, trash_arena,                                   \
        Safe_Deref(state.game_map.segment_vertices_allocator),                         \
        Safe_Deref(state.game_map.graph_nodes_allocator), state.pages, *state.os_data, \
        updated_tiles);

bool Try_Build(Game_State& state, v2i pos, const Item_To_Build& item) {
    auto& arena = state.arena;
    auto& non_persistent_arena = state.non_persistent_arena;
    auto& trash_arena = state.trash_arena;

    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    Assert(Pos_Is_In_Bounds(pos, gsize));

    auto& tile = *(game_map.element_tiles + pos.y * gsize.x + pos.x);

    switch (item.type) {
    case Item_To_Build_Type::Flag: {
        Assert(item.scriptable_building == nullptr);

        if (tile.type == Element_Tile_Type::Flag) {
            tile.type = Element_Tile_Type::Road;
            Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Flag_Removed);
            INVOKE_UPDATE_TILES;
        }  //
        else if (tile.type == Element_Tile_Type::Road) {
            tile.type = Element_Tile_Type::Flag;
            Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Flag_Placed);
            INVOKE_UPDATE_TILES;
        }  //
        else
            return false;

        Assert(tile.building == nullptr);
    } break;

    case Item_To_Build_Type::Road: {
        Assert(item.scriptable_building == nullptr);

        if (tile.type != Element_Tile_Type::None)
            return false;

        Assert(tile.building == nullptr);
        tile.type = Element_Tile_Type::Road;

        Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Road_Placed);
        INVOKE_UPDATE_TILES;
    } break;

    case Item_To_Build_Type::Building: {
        Assert(item.scriptable_building != nullptr);

        if (tile.type != Element_Tile_Type::None)
            return false;

        Place_Building(state, pos, item.scriptable_building);

        Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Building_Placed);
        INVOKE_UPDATE_TILES;
    } break;

    default:
        INVALID_PATH;
    }

    INVOKE_OBSERVER(state.On_Item_Built, (state, pos, item));

    return true;
}
