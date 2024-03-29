#pragma once

Scriptable_Resource*
Get_Scriptable_Resource(Game_State& state, Scriptable_Resource_ID id) {
    Assert(id - 1 < state.scriptable_resources_count);
    auto exists = id != 0;
    auto ptr_offset = (ptrd)(state.scriptable_resources + id - 1);
    auto result = ptr_offset * exists;
    return rcast<Scriptable_Resource*>(result);
}

Terrain_Tile& Get_Terrain_Tile(Game_Map& game_map, v2i16 pos) {
    Assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_tiles + pos.y * game_map.size.x + pos.x);
}

Terrain_Resource& Get_Terrain_Resource(Game_Map& game_map, v2i16 pos) {
    Assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_resources + pos.y * game_map.size.x + pos.x);
}

void Place_Building(Game_State& state, v2i16 pos, Scriptable_Building* scriptable) {
    auto& game_map = state.game_map;
    auto gsize = game_map.size;
    Assert(Pos_Is_In_Bounds(pos, gsize));

    auto [found_instance, _] = Find_And_Occupy_Empty_Slot(game_map.buildings);
    auto& instance = *found_instance;

    instance.pos = pos;
    instance.scriptable = scriptable;

    auto& tile = *(game_map.element_tiles + gsize.x * pos.y + pos.x);
    Assert(tile.type == Element_Tile_Type::None);
    tile.type = Element_Tile_Type::Building;
    tile.building = found_instance;
}

// TODO(hulvdan): Прикрутить какой-либо аллокатор,
// который позволяет использовать заранее аллоцированные memory spaces.
template <typename T>
void Init_Bucket_Array(Bucket_Array<T>& arr, u32 buckets_count, u32 items_per_bucket) {
    arr.allocator_functions.allocate = _aligned_malloc;
    arr.allocator_functions.free = _aligned_free;
    arr.items_per_bucket = items_per_bucket;
    arr.buckets_count = buckets_count;

    arr.buckets = nullptr;
    arr.unfull_buckets = nullptr;
    arr.count = 0;
    arr.used_buckets_count = 0;
    arr.unfull_buckets_count = 0;
}

template <typename T>
void Deinit_Bucket_Array(Bucket_Array<T>& arr) {
    Assert(arr.allocator_functions.allocate != nullptr);
    Assert(arr.allocator_functions.free != nullptr);

    for (auto bucket_ptr = arr.buckets;  //
         bucket_ptr != arr.buckets + arr.used_buckets_count;  //
         bucket_ptr++  //
    ) {
        auto& bucket = *bucket_ptr;
        arr.allocator_functions.free(bucket.occupied);
        arr.allocator_functions.free(bucket.data);
    }

    arr.allocator_functions.free(arr.buckets);
    arr.allocator_functions.free(arr.unfull_buckets);
}

template <typename T>
void Init_Queue(Queue<T>& queue) {
    queue.count = 0;
    queue.max_count = 0;
    queue.base = nullptr;

    queue.allocator_functions.allocate = _aligned_malloc;
    queue.allocator_functions.free = _aligned_free;
}

template <typename T, typename Allocation_Tag>
void Init_Static_Allocation_Queue(Static_Allocation_Queue<T, Allocation_Tag>& queue) {
    queue.count = 0;
    queue.max_count = 0;
    queue.base = nullptr;

    queue.allocator_functions.allocate = _aligned_malloc;
    queue.allocator_functions.free = _aligned_free;
}

template <typename T>
void Deinit_Queue(Queue<T>& queue) {
    if (queue.base != nullptr) {
        Assert(queue.max_count > 0);
        queue.allocator_functions.free(queue.base);
        queue.base = nullptr;
    }
    queue.count = 0;
    queue.max_count = 0;
}

template <typename T, typename Allocation_Tag>
void Deinit_Queue(Static_Allocation_Queue<T, Allocation_Tag>& queue) {
    if (queue.base != nullptr) {
        Assert(queue.max_count > 0);
        queue.allocator_functions.free(queue.base);
        queue.base = nullptr;
    }
    queue.count = 0;
    queue.max_count = 0;
}

// void Update_Building__Not_Constructed(Building& building, float dt) {
//     if (!building.is_constructed) {
//         building.time_since_item_was_placed += dt;
//     }
//
//     if (building.resources_to_book.Count > 0) {
//         _resourceTransportation.Add_ResourcesToBook(building.resourcesToBook);
//         building.resourcesToBook.Clear();
//     }
// }

// template <typename Derived>
// struct Human_Top_Controller {
// protected:
//     auto Self_Type = Derived;
//
// public:
//     void On_Enter(Human& human, Human_Data& data) { Self_Type::On_Enter(human, data); }
//
//     void On_Exit(Human& human, Human_Data& data) { Self_Type::On_Exit(human, data); }
//
//     void Update(Human& human, Human_Data& data, f32 dt) {
//         Self_Type::Update(human, data, dt);
//     }
//
//     void On_Human_Current_Segment_Changed(
//         Human& human,
//         Human_Data& data,
//         Graph_Segment* old_segment  //
//     ) {
//         Self_Type::On_Human_Current_Segment_Changed(human, datta, old_segment);
//     }
//
//     void On_Human_Moved_To_The_Next_Tile(Human& human, Human_Data& data) {}
//
//     void Update_States(
//         Human& human,
//         Human_Data& data,
//         Graph_Segment* old_segment,
//         Building* old_building) {}
// };
//
struct Human_Data {
    Player_ID max_player_id;
    Building* city_halls;
    Game_Map* game_map;
};

void Set_Human_State(Human& human, Human_Main_State new_state);

struct Human_Moving_In_The_World_Controller {
    static void On_Enter(Human& human, Human_Data& data) {
        // if (human.segment != nullptr) {
        //     TRACELOG(
        //         "human.segment.resourcesToTransport.Count = "
        //         "{human.segment.resourcesToTransport.Count}");
        // }

        human.moving.path.count = 0;
        Update_States(human, data, nullptr, nullptr);
    }

    static void On_Exit(Human& human, Human_Data& data) {
        // TRACELOG_SCOPE;

        human.state_moving_in_the_world = Moving_In_The_World_State::None;
        human.moving.to.reset();
        human.moving.path.count = 0;

        if (human.type == Human_Type::Employee) {
            Assert(human.building != nullptr);
            human.building->employee_is_inside = true;
            // TODO: Somehow remove this human
        }
    }

    static void Update(Human& human, Human_Data& data, f32 dt) {
        Update_States(human, data, human.segment, human.building);
    }

    static void On_Human_Current_Segment_Changed(
        Human& human,
        Human_Data& data,
        Graph_Segment* old_segment
    ) {
        // TRACELOG_SCOPE;
        Assert(human.type == Human_Type::Transporter);
        Update_States(human, data, old_segment, nullptr);
    }

    static void On_Human_Moved_To_The_Next_Tile(Human& human, Human_Data data) {
        if (human.type == Human_Type::Constructor  //
            && human.building != nullptr  //
            && human.moving.pos == human.building->pos  //
        ) {
            Set_Human_State(human, Human_Main_State::Building);
        }

        if (human.type == Human_Type::Employee) {
            Assert(human.building != nullptr);
            auto& building = *human.building;

            if (human.moving.pos == building.pos) {
                Assert_False(human.moving.to.has_value());

                // TODO: data.game_map->Employee_Reached_Building_Callback(human);
                building.employee_is_inside = true;
            }
        }
    }

    static void Update_States(
        Human& human,
        Human_Data& data,
        Graph_Segment* old_segment,
        Building* old_building
    ) {
        // TRACELOG_SCOPE;

        if (human.segment != nullptr) {
            auto& segment = *human.segment;
            Assert(human.type == Human_Type::Transporter);

            // NOTE(hulvdan): Human stepped on a tile of the segment.
            // We don't need to keep the path anymore
            if (human.moving.to.has_value()
                && Graph_Contains(segment.graph, human.moving.to.value())
                && Graph_Node(segment.graph, human.moving.to.value()) != 0  //
            ) {
                human.moving.path.count = 0;
                return;
            }

            // NOTE(hulvdan): Human stepped on a tile of the segment
            // and finished moving. Starting Moving_Inside_Segment
            if (!(human.moving.to.has_value())  //
                && Graph_Contains(segment.graph, human.moving.pos)  //
                && Graph_Node(segment.graph, human.moving.pos) != 0  //
            ) {
                // TRACELOG("_controller.SetState(human,
                // HumanState.MovingInsideSegment)");
                // TODO: _controller.Set_State(human, MainState.MovingInsideSegment);
                return;
            }

            if (old_segment != human.segment  //
                || human.state_moving_in_the_world
                    != Moving_In_The_World_State::Moving_To_Destination  //
            ) {
                // TRACELOG("Setting human.stateMovingInTheWorld =
                // State.MovingToSegment");
                human.state_moving_in_the_world
                    = Moving_In_The_World_State::Moving_To_Destination;

                // TODO:
                // var center = segment.graph.Get_Centers()[0];
                // var path = data.map.Find_Path(human.moving.to ?? human.moving.pos,
                // center, true);
                //
                // Assert(path.success);
                // human.moving.AddPath(path.value);
            }
        }
        else if (human.building != nullptr) {
            auto& building = *human.building;

            auto is_constructor_or_employee = human.type == Human_Type::Constructor  //
                || human.type == Human_Type::Employee;
            Assert(is_constructor_or_employee);

            if (human.type == Human_Type::Constructor) {
                // TODO:
                // Assert_False(building.is_constructed);
            }
            else if (human.type == Human_Type::Employee) {
                // TODO:
                // Assert(building.is_constructed);
            }

            if (old_building != human.building) {
                human.moving.path.count = 0;

                // TODO:
                // var path = data.map.Find_Path(
                //     human.moving.to ?? human.moving.pos, building.pos, true
                // );
                //
                // Assert(path.success);
                // human.moving.Add_Path(path.value);
            }
        }
        else if (
            human.state_moving_in_the_world
            != Moving_In_The_World_State::Moving_To_The_City_Hall  //
        ) {
            // TRACELOG("human.stateMovingInTheWorld = State.MovingToTheCityHall");
            human.state_moving_in_the_world
                = Moving_In_The_World_State::Moving_To_The_City_Hall;

            auto& city_hall = Assert_Deref(data.city_halls + human.player_id);
            // TODO:
            // auto path = data.game_map->Find_Path(
            //     human.moving.to.value_or(human.moving.pos), city_hall.pos, true);
            //
            // Assert(path.success);
            // human.moving.Add_Path(path.value);
        }
    }
};

#define TOP_HUMAN_CONTROLLERS_FUNCTIONS(_generate_case_, code) \
    _generate_case_(Moving_In_The_World, Human_Moving_In_The_World_Controller::code);

#define GENERATE_CASE(_human_main_state_, _called_function_) \
    case (Human_Main_State::_human_main_state_): {           \
        _called_function_;                                   \
    } break;

void Set_Human_State(Human& human, Human_Main_State new_state, Human_Data& data) {
    // TRACELOG_SCOPE;
    // TRACELOG("Set_Human_State");
    auto old_state = human.state;
    human.state = new_state;

    if (old_state != Human_Main_State::None) {
        switch (old_state) {
            TOP_HUMAN_CONTROLLERS_FUNCTIONS(GENERATE_CASE, On_Exit(human, data))
        default:
            INVALID_PATH;
        }
    }

    human.state_moving_in_the_world = Moving_In_The_World_State::Moving_To_Destination;

    switch (new_state) {
        TOP_HUMAN_CONTROLLERS_FUNCTIONS(GENERATE_CASE, On_Enter(human, data))
        // NOTE(hulvdan): There is another way of specifying Employee's behaviour
    default:
        INVALID_PATH;
    }
}

Human* Create_Human_Transporter(
    Game_Map& game_map,
    Building& building,
    Graph_Segment& segment,
    Human_Data& data
) {
    auto [human_ptr, locator] = Find_And_Occupy_Empty_Slot(game_map.humans);
    auto& human = *human_ptr;
    // TODO:
    // var human = Human.Transporter(Guid.NewGuid(), building.pos, segment);
    // human.aboba = 1;

    Set_Human_State(human, Human_Main_State::Moving_In_The_World, data);

    // onHumanCreated.OnNext(new() { human = human });
    //
    // if (building.scriptable.type == BuildingType.SpecialCityHall) {
    //     onCityHallCreatedHuman.OnNext(new() { cityHall = building });
    //     DomainEvents<E_CityHallCreatedHuman>.Publish(new() { cityHall = building });
    // }

    segment.assigned_human = human_ptr;
    return human_ptr;
}

void Update_Building__Constructed(
    Game_Map& game_map,
    Building& building,
    f32 dt,
    Human_Data& data
) {
    auto& scriptable = *building.scriptable;

    auto delay = scriptable.human_spawning_delay;

    if (scriptable.type == Building_Type::City_Hall) {
        auto& since_created = building.time_since_human_was_created;
        since_created += dt;
        if (since_created > delay)
            since_created = delay;

        if (game_map.segments_that_need_humans.count > 0) {
            if (since_created >= delay) {
                since_created -= delay;
                auto segment = Dequeue(game_map.segments_that_need_humans);
                Create_Human_Transporter(game_map, building, *segment, data);
            }
        }
    }

    // _building_controller.Update(building, dt);
}

void Update_Buildings(Game_State& state, f32 dt, Human_Data& data) {
    for (auto building_ptr : Iter(&state.game_map.buildings)) {
        auto& building = *building_ptr;
        // if (building.constructionProgress < 1) {
        //     Update_Building__Not_Constructed(building, dt);
        // } else {
        Update_Building__Constructed(state.game_map, building, dt, data);
        // }
    }
}

void Initialize_Game_Map(Game_State& state, Arena& arena) {
    auto& game_map = state.game_map;

    {
        size_t toc_size = 1024;
        size_t data_size = 4096;

        auto buf = Allocate_Zeros_For(arena, Allocator);
        new (buf) Allocator(
            toc_size, Allocate_Zeros_Array(arena, u8, toc_size),  //
            data_size, Allocate_Array(arena, u8, data_size)
        );

        game_map.segment_vertices_allocator = buf;
        Set_Allocator_Name_If_Profiling(
            arena, *buf, "segment_vertices_allocator_%d", state.dll_reloads_count
        );
    }
    {
        size_t toc_size = 1024;
        size_t data_size = 4096;

        auto buf = Allocate_Zeros_For(arena, Allocator);
        new (buf) Allocator(
            toc_size, Allocate_Zeros_Array(arena, u8, toc_size),  //
            data_size, Allocate_Array(arena, u8, data_size)
        );

        game_map.graph_nodes_allocator = buf;
        Set_Allocator_Name_If_Profiling(
            arena, *buf, "graph_nodes_allocator_%d", state.dll_reloads_count
        );
    }

    auto tiles_count = game_map.size.x * game_map.size.y;

    Init_Bucket_Array(game_map.buildings, 32, 128);
    Init_Bucket_Array(game_map.segments, 32, 128);
    Init_Bucket_Array(game_map.humans, 32, 128);
    Init_Static_Allocation_Queue(game_map.segments_that_need_humans);

    Place_Building(state, {2, 2}, state.scriptable_building_city_hall);
}

void Deinitialize_Game_Map(Game_State& state) {
    auto& game_map = state.game_map;

    Deinit_Bucket_Array(game_map.buildings);
    Deinit_Bucket_Array(game_map.segments);
    Deinit_Bucket_Array(game_map.humans);
    Deinit_Queue(game_map.segments_that_need_humans);
}

void Regenerate_Terrain_Tiles(
    Game_State& state,
    Game_Map& game_map,
    Arena& arena,
    Arena& trash_arena,
    uint seed,
    Editor_Data& data
) {
    auto gsize = game_map.size;

    auto noise_pitch = Ceil_To_Power_Of_2((u32)MAX(gsize.x, gsize.y));
    auto output_size = noise_pitch * noise_pitch;

    auto terrain_perlin = Allocate_Array(trash_arena, u16, output_size);
    DEFER(Deallocate_Array(trash_arena, u16, output_size));
    Fill_Perlin_2D(
        terrain_perlin, sizeof(u16) * output_size, trash_arena, data.terrain_perlin,
        noise_pitch, noise_pitch
    );

    auto forest_perlin = Allocate_Array(trash_arena, u16, output_size);
    DEFER(Deallocate_Array(trash_arena, u16, output_size));
    Fill_Perlin_2D(
        forest_perlin, sizeof(u16) * output_size, trash_arena, data.forest_perlin,
        noise_pitch, noise_pitch
    );

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

                auto should_change
                    = tile.height > height_below && tile.height > height_above;
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

            tile.is_cliff
                = y == 0 || tile.height > Get_Terrain_Tile(game_map, {x, y - 1}).height;
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
    Editor_Data& data
) {
    auto gsize = game_map.size;

    v2i16 road_tiles[] = {
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

    auto base_offset = v2i16(1, 1);
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

struct Updated_Tiles {
    u16 count;
    v2i16* pos;
    Tile_Updated_Type* type;
};

bool Should_Segment_Be_Deleted(
    v2i16 gsize,
    Element_Tile* element_tiles,
    const Updated_Tiles& updated_tiles,
    const Graph_Segment& segment
) {
    FOR_RANGE(u16, i, updated_tiles.count) {
        auto& tile_pos = *(updated_tiles.pos + i);
        auto& updated_type = *(updated_tiles.type + i);

        // TODO(hulvdan): Логика работы была изменена после C# репы.
        // Нужно перепроверить, что тут всё работает стабильно.
        switch (updated_type) {
        case Tile_Updated_Type::Road_Placed:
        case Tile_Updated_Type::Building_Placed: {
            for (auto& offset : v2i16_adjacent_offsets) {
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

#define Add_Without_Duplication(max_count_, count_, array_, value_) \
    {                                                               \
        Assert((max_count_) >= (count_));                           \
                                                                    \
        auto found = false;                                         \
        FOR_RANGE(int, i, (count_)) {                               \
            auto existing = *((array_) + i);                        \
            if (existing == (value_)) {                             \
                found = true;                                       \
                break;                                              \
            }                                                       \
        }                                                           \
                                                                    \
        if (!found) {                                               \
            Assert((count_) < (max_count_));                        \
            *((array_) + (count_)) = (value_);                      \
            (count_)++;                                             \
        }                                                           \
    }

v2i16* Allocate_Segment_Vertices(Allocator& allocator, int vertices_count) {
    return (v2i16*)allocator.Allocate(sizeof(v2i16) * vertices_count, 1);
}

u8* Allocate_Graph_Nodes(Allocator& allocator, int nodes_count) {
    return allocator.Allocate(nodes_count, 1);
}

void Rect_Copy(u8* dest, u8* source, int stride, int rows, int bytes_per_line) {
    FOR_RANGE(int, i, rows) {
        memcpy(dest + i * bytes_per_line, source + i * stride, bytes_per_line);
    }
}

typedef ttuple<Direction, v2i16> Dir_v2i16;

#define GRID_PTR_VALUE(arr_ptr, pos) (*(arr_ptr + gsize.x * pos.y + pos.x))

#define QUEUES_SCALE 4

void Update_Graphs(
    const v2i16 gsize,
    const Element_Tile* const element_tiles,
    Graph_Segment* const added_segments,
    u32& added_segments_count,
    Fixed_Size_Queue<Dir_v2i16>& big_queue,
    Fixed_Size_Queue<Dir_v2i16>& queue,
    Arena& trash_arena,
    u8* visited,
    Allocator& segment_vertices_allocator,
    Allocator& graph_nodes_allocator,
    const bool full_graph_build
) {
    auto tiles_count = gsize.x * gsize.y;

    size_t deallocation_size = 0;

    bool* vis = nullptr;
    if (full_graph_build) {
        vis = Allocate_Zeros_Array(trash_arena, bool, tiles_count);
        deallocation_size += sizeof(bool) * tiles_count;
    }

    while (big_queue.count) {
        auto p = Dequeue(big_queue);
        Enqueue(queue, p);

        auto [_, p_pos] = p;
        if (full_graph_build)
            GRID_PTR_VALUE(vis, p_pos) = true;

        v2i16* vertices = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);
        DEFER(Deallocate_Array(trash_arena, v2i16, tiles_count));

        v2i16* segment_tiles = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);
        DEFER(Deallocate_Array(trash_arena, v2i16, tiles_count));

        int vertices_count = 0;
        int segment_tiles_count = 1;
        *(segment_tiles + 0) = p_pos;

        Graph temp_graph = {};
        temp_graph.nodes = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
        DEFER(Deallocate_Array(trash_arena, u8, tiles_count));
        temp_graph.size.x = gsize.x;
        temp_graph.size.y = gsize.y;

        while (queue.count) {
            auto [dir, pos] = Dequeue(queue);
            if (full_graph_build)
                GRID_PTR_VALUE(vis, pos) = true;

            auto& tile = GRID_PTR_VALUE(element_tiles, pos);

            bool is_flag = tile.type == Element_Tile_Type::Flag;
            bool is_building = tile.type == Element_Tile_Type::Building;
            bool is_vertex = is_building || is_flag;

            if (is_vertex)
                Add_Without_Duplication(tiles_count, vertices_count, vertices, pos);

            FOR_DIRECTION(dir_index) {
                if (is_vertex && dir_index != dir)
                    continue;

                u8& visited_value = GRID_PTR_VALUE(visited, pos);
                if (Graph_Node_Has(visited_value, dir_index))
                    continue;

                v2i16 new_pos = pos + As_Offset(dir_index);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                Direction opposite_dir_index = Opposite(dir_index);
                u8& new_visited_value = GRID_PTR_VALUE(visited, new_pos);
                if (Graph_Node_Has(new_visited_value, opposite_dir_index))
                    continue;

                auto& new_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (new_tile.type == Element_Tile_Type::None)
                    continue;

                bool new_is_building = new_tile.type == Element_Tile_Type::Building;
                bool new_is_flag = new_tile.type == Element_Tile_Type::Flag;
                bool new_is_vertex = new_is_building || new_is_flag;

                if (is_vertex && new_is_vertex) {
                    if (tile.building != new_tile.building)
                        continue;

                    if (!Graph_Node_Has(new_visited_value, opposite_dir_index)) {
                        new_visited_value = Graph_Node_Mark(
                            new_visited_value, opposite_dir_index, true
                        );
                        FOR_DIRECTION(new_dir_index) {
                            if (!Graph_Node_Has(new_visited_value, new_dir_index))
                                Enqueue(big_queue, {new_dir_index, new_pos});
                        }
                    }
                    continue;
                }

                visited_value = Graph_Node_Mark(visited_value, dir_index, true);
                new_visited_value
                    = Graph_Node_Mark(new_visited_value, opposite_dir_index, true);
                Graph_Update(temp_graph, pos, dir_index, true);
                Graph_Update(temp_graph, new_pos, opposite_dir_index, true);

                if (full_graph_build && new_is_vertex) {
                    FOR_DIRECTION(new_dir_index) {
                        if (!Graph_Node_Has(new_visited_value, new_dir_index))
                            Enqueue(big_queue, {new_dir_index, new_pos});
                    }
                }

                Add_Without_Duplication(
                    tiles_count, segment_tiles_count, segment_tiles, new_pos
                );

                if (new_is_vertex) {
                    Add_Without_Duplication(
                        tiles_count, vertices_count, vertices, new_pos
                    );
                }
                else {
                    Enqueue(queue, {(Direction)0, new_pos});
                }
            }
        }

        // NOTE(hulvdan): Поиск островов графа
        if (full_graph_build && !big_queue.count) {
            FOR_RANGE(int, y, gsize.y) {
                FOR_RANGE(int, x, gsize.x) {
                    auto pos = v2i16(x, y);
                    auto& tile = GRID_PTR_VALUE(element_tiles, pos);
                    u8& v1 = GRID_PTR_VALUE(visited, pos);
                    bool& v2 = GRID_PTR_VALUE(vis, pos);

                    bool is_building = tile.type == Element_Tile_Type::Building;
                    bool is_flag = tile.type == Element_Tile_Type::Flag;
                    bool is_vertex = is_building || is_flag;

                    if (is_vertex && !v1 && !v2) {
                        FOR_DIRECTION(dir_index) {
                            Enqueue(big_queue, {dir_index, pos});
                        }
                    }
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
        segment.vertices_count = vertices_count;
        added_segments_count++;

        auto verticesss
            = Allocate_Segment_Vertices(segment_vertices_allocator, vertices_count);
        segment.vertices = verticesss;
        memcpy(segment.vertices, vertices, sizeof(v2i16) * vertices_count);

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
        auto nodesss = Allocate_Graph_Nodes(graph_nodes_allocator, all_nodes_count);
        segment.graph.nodes = nodesss;

        auto rows = gr_size.y;
        auto stride = gsize.x;
        auto starting_node = temp_graph.nodes + offset.y * gsize.x + offset.x;
        Rect_Copy(segment.graph.nodes, starting_node, stride, rows, gr_size.x);
    }

    if (full_graph_build && deallocation_size)
        Deallocate_Array(trash_arena, u8, deallocation_size);
}

void Build_Graph_Segments(
    v2i16 gsize,
    Element_Tile* element_tiles,
    Bucket_Array<Graph_Segment>& segments,
    Arena& trash_arena,
    Allocator& segment_vertices_allocator,
    Allocator& graph_nodes_allocator,
    Pages& pages
) {
    Assert(segments.used_buckets_count == 0);

    auto tiles_count = gsize.x * gsize.y;

    // NOTE(hulvdan): Создание новых сегментов
    auto added_segments_allocate = tiles_count * 4;
    u32 added_segments_count = 0;
    Graph_Segment* added_segments
        = Allocate_Zeros_Array(trash_arena, Graph_Segment, added_segments_allocate);
    DEFER(Deallocate_Array(trash_arena, Graph_Segment, added_segments_allocate));

    v2i16 pos = -v2i16_one;
    bool found = false;
    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = *(element_tiles + y * gsize.x + x);

            if (tile.type == Element_Tile_Type::Flag
                || tile.type == Element_Tile_Type::Building) {
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

    Fixed_Size_Queue<Dir_v2i16> big_queue = {};
    big_queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    big_queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, big_queue.memory_size);
    DEFER(Deallocate_Array(trash_arena, u8, big_queue.memory_size));

    FOR_DIRECTION(dir) {
        Enqueue(big_queue, {dir, pos});
    }

    Fixed_Size_Queue<Dir_v2i16> queue = {};
    queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);
    DEFER(Deallocate_Array(trash_arena, u8, queue.memory_size));

    u8* visited = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
    DEFER(Deallocate_Array(trash_arena, u8, tiles_count));

    bool full_graph_build = true;
    Update_Graphs(
        gsize, element_tiles, added_segments, added_segments_count, big_queue, queue,
        trash_arena, visited, segment_vertices_allocator, graph_nodes_allocator,
        full_graph_build
    );

    {
        FOR_RANGE(int, i, added_segments_count) {
            auto [segment_ptr, locator] = Find_And_Occupy_Empty_Slot(segments);
            auto& segment = *segment_ptr;
            segment.locator = locator;
            auto& added_segment = *(added_segments + i);

            // TODO(hulvdan): use move semantics?
            segment.vertices_count = added_segment.vertices_count;
            segment.vertices = added_segment.vertices;
            segment.graph.nodes_count = added_segment.graph.nodes_count;
            segment.graph.nodes = added_segment.graph.nodes;
            segment.graph.size = added_segment.graph.size;
            segment.graph.offset = added_segment.graph.offset;

            // SHIT(hulvdan): Do it later
            // FROM C# REPO
            // foreach (auto segmentToLink in segments) {
            //     // Mb there Graph.CollidesWith(other.Graph) is needed for
            //     optimization if (segmentToLink.HasSomeOfTheSameVertices(segment)) {
            //         segment.Link(segmentToLink);
            //         segmentToLink.Link(segment);
            //     }
            // }
        }
    }
}

ttuple<int, int> Update_Tiles(
    v2i16 gsize,
    Element_Tile* element_tiles,
    Bucket_Array<Graph_Segment>& segments,
    Arena& trash_arena,
    Allocator& segment_vertices_allocator,
    Allocator& graph_nodes_allocator,
    Pages& pages,
    const Updated_Tiles& updated_tiles
) {
    Assert(updated_tiles.count > 0);
    if (!updated_tiles.count)
        return {0, 0};

    auto tiles_count = gsize.x * gsize.y;

    // NOTE(hulvdan): Ищем сегменты для удаления
    auto segments_to_be_deleted_allocate = updated_tiles.count * 4;
    u32 segments_to_be_deleted_count = 0;
    Graph_Segment** segments_to_be_deleted = Allocate_Zeros_Array(
        trash_arena, Graph_Segment*, segments_to_be_deleted_allocate
    );
    DEFER(Deallocate_Array(trash_arena, Graph_Segment*, segments_to_be_deleted_allocate));

    for (auto segment_ptr : Iter(&segments)) {
        auto& segment = *segment_ptr;
        if (!Should_Segment_Be_Deleted(gsize, element_tiles, updated_tiles, segment))
            continue;

        Assert(segments_to_be_deleted_count < updated_tiles.count * 4);
        *(segments_to_be_deleted + segments_to_be_deleted_count) = segment_ptr;
        segments_to_be_deleted_count++;
    };

    // NOTE(hulvdan): Создание новых сегментов
    auto added_segments_allocate = updated_tiles.count * 4;
    u32 added_segments_count = 0;
    Graph_Segment* added_segments
        = Allocate_Zeros_Array(trash_arena, Graph_Segment, added_segments_allocate);
    DEFER(Deallocate_Array(trash_arena, Graph_Segment, added_segments_allocate));

    Fixed_Size_Queue<Dir_v2i16> big_queue = {};
    big_queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    big_queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, big_queue.memory_size);
    DEFER(Deallocate_Array(trash_arena, u8, big_queue.memory_size));

    FOR_RANGE(auto, i, updated_tiles.count) {
        const auto& updated_type = *(updated_tiles.type + i);
        const auto& pos = *(updated_tiles.pos + i);

        switch (updated_type) {
        case Tile_Updated_Type::Road_Placed: {
            FOR_DIRECTION(dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                Enqueue(big_queue, {dir, pos});
            }
        } break;

        case Tile_Updated_Type::Flag_Placed: {
            FOR_DIRECTION(dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::Road)
                    Enqueue(big_queue, {dir, pos});
            }
        } break;

        case Tile_Updated_Type::Flag_Removed: {
            FOR_DIRECTION(dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type != Element_Tile_Type::None)
                    Enqueue(big_queue, {dir, pos});
            }
        } break;

        case Tile_Updated_Type::Road_Removed: {
            FOR_DIRECTION(dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                FOR_DIRECTION(dir) {
                    Enqueue(big_queue, {dir, new_pos});
                }
            }
        } break;

        case Tile_Updated_Type::Building_Placed: {
            FOR_DIRECTION(dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                if (element_tile.type == Element_Tile_Type::Building)
                    continue;

                if (element_tile.type == Element_Tile_Type::Flag)
                    continue;

                FOR_DIRECTION(dir) {
                    Enqueue(big_queue, {dir, new_pos});
                }
            }
        } break;

        case Tile_Updated_Type::Building_Removed: {
            FOR_DIRECTION(dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::Road) {
                    FOR_DIRECTION(new_dir) {
                        Enqueue(big_queue, {new_dir, new_pos});
                    }
                }
            }
        } break;

        default:
            INVALID_PATH;
        }
    }

    // NOTE(hulvdan): Each byte here contains differently bit-shifted values of
    // `Direction`
    u8* visited = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
    DEFER(Deallocate_Array(trash_arena, u8, tiles_count));

    Fixed_Size_Queue<Dir_v2i16> queue = {};
    queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);
    DEFER(Deallocate_Array(trash_arena, u8, queue.memory_size));

    bool full_graph_build = false;
    Update_Graphs(
        gsize, element_tiles, added_segments, added_segments_count, big_queue, queue,
        trash_arena, visited, segment_vertices_allocator, graph_nodes_allocator,
        full_graph_build
    );

    // ====================================================================================
    // FROM C# REPO void UpdateSegments(ItemTransportationGraph.OnTilesUpdatedResult
    // res)

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
            Graph_Segment* segment_ptr = *(segments_to_be_deleted + i);
            auto& segment = *segment_ptr;
            segment_vertices_allocator.Free(rcast<u8*>(segment.vertices));
            graph_nodes_allocator.Free(segment.graph.nodes);

            Bucket_Array_Remove(segments, segment.locator);

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
            auto [segment_ptr, locator] = Find_And_Occupy_Empty_Slot(segments);
            auto& segment = *segment_ptr;
            segment.locator = locator;

            auto& added_segment = *(added_segments + i);

            // TODO(hulvdan): use move semantics?
            segment.vertices_count = added_segment.vertices_count;
            segment.vertices = added_segment.vertices;
            segment.graph.nodes_count = added_segment.graph.nodes_count;
            segment.graph.nodes = added_segment.graph.nodes;
            segment.graph.size = added_segment.graph.size;
            segment.graph.offset = added_segment.graph.offset;

            // SHIT(hulvdan): Do it later
            // FROM C# REPO
            // foreach (auto segmentToLink in segments) {
            //     // Mb there Graph.CollidesWith(other.Graph) is needed for
            //     optimization if (segmentToLink.HasSomeOfTheSameVertices(segment)) {
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
    // while (humansThatNeedNewSegment.Count > 0 && segmentsThatNeedHumans.Count > 0)
    // {
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
    for (auto segment1_ptr : Iter(&segments)) {
        auto& segment1 = *segment1_ptr;
        auto& g1 = segment1.graph;
        v2i16 g1_offset = {g1.offset.x, g1.offset.y};

        for (auto segment2_ptr : Iter(&segments)) {
            auto& segment2 = *segment2_ptr;
            if (segment1_ptr == segment2_ptr)
                continue;

            auto& g2 = segment2.graph;
            v2i16 g2_offset = {g2.offset.x, g2.offset.y};

            for (auto y = 0; y < g1.size.y; y++) {
                for (auto x = 0; x < g1.size.x; x++) {
                    v2i16 g1p_world = v2i16(x, y) + g1_offset;
                    v2i16 g2p_local = g1p_world - g2_offset;
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

    return {added_segments_count, segments_to_be_deleted_count};
}

#define Declare_Updated_Tiles(variable_name_, pos_, type_) \
    Updated_Tiles(variable_name_) = {};                    \
    (variable_name_).count = 1;                            \
    (variable_name_).pos = &(pos_);                        \
    auto type__ = (type_);                                 \
    (variable_name_).type = &type__;

#define INVOKE_UPDATE_TILES                                                            \
    Update_Tiles(                                                                      \
        state.game_map.size, state.game_map.element_tiles, state.game_map.segments,    \
        trash_arena, Assert_Deref(state.game_map.segment_vertices_allocator),          \
        Assert_Deref(state.game_map.graph_nodes_allocator), state.pages, updated_tiles \
    );

bool Try_Build(Game_State& state, v2i16 pos, const Item_To_Build& item) {
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
        }
        else if (tile.type == Element_Tile_Type::Road) {
            tile.type = Element_Tile_Type::Flag;
            Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Flag_Placed);
            INVOKE_UPDATE_TILES;
        }
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
