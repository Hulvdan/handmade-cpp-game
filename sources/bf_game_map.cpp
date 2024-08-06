#define GRID_PTR_VALUE(arr_ptr, pos) \
    (*((arr_ptr) + (ptrdiff_t)(gsize.x * (pos).y + (pos).x)))

#define Array_Push(array, array_count, array_max_count, value) \
    STATEMENT({                                                \
        *((array) + (array_count)) = value;                    \
        (array_count)++;                                       \
        Assert((array_count) <= (array_max_count));            \
    })

template <typename T>
BF_FORCE_INLINE T Array_Pop(T* array, auto& array_count) {
    Assert(array_count > 0);
    T result = *(array + array_count - 1);
    array_count--;
    return result;
}

#define Array_Reverse(array, count)          \
    STATEMENT({                              \
        Assert((count) >= 0);                \
        FOR_RANGE (i32, l, (count) / 2) {    \
            auto r         = (count)-l - 1;  \
            auto t         = *((array) + l); \
            *((array) + l) = *((array) + r); \
            *((array) + r) = t;              \
        }                                    \
    })

bool Have_Some_Of_The_Same_Vertices(const Graph_Segment& s1, const Graph_Segment& s2) {
    FOR_RANGE (i32, i1, s1.vertices_count) {
        auto& v1 = *(s1.vertices + i1);

        FOR_RANGE (i32, i2, s2.vertices_count) {
            auto& v2 = *(s2.vertices + i2);

            if (v1 == v2)
                return true;
        }
    }

    return false;
}

global_var int global_last_segments_to_add_count    = 0;
global_var int global_last_segments_to_delete_count = 0;

struct Path_Find_Result {
    bool   success;
    v2i16* path;
    i32    path_count;
};

std::tuple<v2i16*, i32> Build_Path(
    Arena&                trash_arena,
    v2i16                 gsize,
    std::optional<v2i16>* bfs_parents_mtx,
    v2i16                 destination
) {
#if 0
    // NOTE: Двойной проход, но без RAM overhead-а на `trash_arena`.
    auto path_max_count = 1;
    {
        v2i16 dest = destination;
        while (GRID_PTR_VALUE(bfs_parents_mtx, dest).has_value()) {
            auto value = GRID_PTR_VALUE(bfs_parents_mtx, dest).value();
            path_max_count++;
            dest = value;
        }
    }
#else
    // NOTE: Одинарный проход. С RAM overhead-ом.
    auto path_max_count = Longest_Meaningful_Path(gsize);
#endif

    i32  path_count = 0;
    auto path       = Allocate_Zeros_Array(trash_arena, v2i16, path_max_count);

    Array_Push(path, path_count, path_max_count, destination);
    while (GRID_PTR_VALUE(bfs_parents_mtx, destination).has_value()) {
        auto value = GRID_PTR_VALUE(bfs_parents_mtx, destination).value();
        Array_Push(path, path_count, path_max_count, value);
        destination = value;
    }

    Array_Reverse(path, path_count);

    return {path, path_count};
}

Path_Find_Result Find_Path(
    Arena&        trash_arena,
    v2i16         gsize,
    Terrain_Tile* terrain_tiles,
    Element_Tile* element_tiles,
    v2i16         source,
    v2i16         destination,
    bool          avoid_harvestable_resources
) {
    if (source == destination)
        return {true, {}, 0};

    TEMP_USAGE(trash_arena);
    i32 tiles_count = gsize.x * gsize.y;

    Path_Find_Result        result{};
    Fixed_Size_Queue<v2i16> queue{};
    queue.memory_size       = sizeof(v2i16) * tiles_count;
    queue.base              = (v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);
    *queue.Enqueue_Unsafe() = source;

    bool* visited_mtx = Allocate_Zeros_Array(trash_arena, bool, tiles_count);
    GRID_PTR_VALUE(visited_mtx, source) = true;

    auto bfs_parents_mtx = Allocate_Array(trash_arena, std::optional<v2i16>, tiles_count);

    FOR_RANGE (int, i, tiles_count) {
        std::construct_at(bfs_parents_mtx + i);
    }

    while (queue.count > 0) {
        auto pos = queue.Dequeue();
        FOR_DIRECTION (dir) {
            auto offset  = As_Offset(dir);
            auto new_pos = pos + offset;
            if (!Pos_Is_In_Bounds(new_pos, gsize))
                continue;

            auto& visited = GRID_PTR_VALUE(visited_mtx, new_pos);
            if (visited)
                continue;

            if (avoid_harvestable_resources) {
                auto& terrain_tile = GRID_PTR_VALUE(terrain_tiles, new_pos);
                if (terrain_tile.resource_amount > 0)
                    continue;
            }

            auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
            visited            = true;

            GRID_PTR_VALUE(bfs_parents_mtx, new_pos) = pos;

            if (new_pos == destination) {
                result.success = true;
                auto [path, path_count]
                    = Build_Path(trash_arena, gsize, bfs_parents_mtx, new_pos);
                result.path       = path;
                result.path_count = path_count;
                return result;
            }

            *queue.Enqueue_Unsafe() = new_pos;
        }
    }

    result.success = false;
    return result;
}

Terrain_Tile& Get_Terrain_Tile(Game_Map& game_map, v2i16 pos) {
    Assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_tiles + (ptrdiff_t)(pos.y * game_map.size.x + pos.x));
}

Terrain_Resource& Get_Terrain_Resource(Game_Map& game_map, v2i16 pos) {
    Assert(Pos_Is_In_Bounds(pos, game_map.size));
    return *(game_map.terrain_resources + (ptrdiff_t)(pos.y * game_map.size.x + pos.x));
}

template <typename T>
void Set_Container_Allocator_Context(T& container, MCTX) {
    container.allocator_      = ctx->allocator;
    container.allocator_data_ = ctx->allocator_data;
}

template <typename T>
void Init_Queue(Queue<T>& container, MCTX) {
    container.count     = 0;
    container.max_count = 0;
    container.base      = nullptr;

    container.allocator_      = ctx->allocator;
    container.allocator_data_ = ctx->allocator_data;
}

template <typename T>
void Init_Vector(Vector<T>& container, MCTX) {
    container.count     = 0;
    container.max_count = 0;
    container.base      = nullptr;

    container.allocator_      = ctx->allocator;
    container.allocator_data_ = ctx->allocator_data;
}

template <typename T>
void Deinit_Queue(Queue<T>& container, MCTX) {
    CONTAINER_ALLOCATOR;

    if (container.base != nullptr) {
        Assert(container.max_count > 0);
        FREE(container.base, sizeof(T) * container.max_count);
        container.base = nullptr;
    }

    container.count     = 0;
    container.max_count = 0;
}

template <typename T>
void Deinit_Sparse_Array(Sparse_Array_Of_Ids<T>& container, MCTX) {
    CTX_ALLOCATOR;

    if (container.ids != nullptr) {
        Assert(container.max_count != 0);
        FREE(container.ids, sizeof(T) * container.max_count);
    }

    container.count     = 0;
    container.max_count = 0;
}

template <typename T, typename U>
void Deinit_Sparse_Array(Sparse_Array<T, U>& container, MCTX) {
    CTX_ALLOCATOR;

    if (container.ids != nullptr) {
        Assert(container.base != nullptr);
    }

    if (container.base != nullptr) {
        Assert(container.max_count != 0);
        Assert(container.ids != nullptr);

        FREE(container.ids, sizeof(T) * container.max_count);
        FREE(container.base, sizeof(U) * container.max_count);
    }
    container.count     = 0;
    container.max_count = 0;
}

template <typename T>
void Deinit_Vector(Vector<T>& container, MCTX) {
    CONTAINER_ALLOCATOR;

    if (container.base != nullptr) {
        Assert(container.max_count > 0);
        FREE(container.base, sizeof(T) * container.max_count);
        container.base = nullptr;
    }
    container.count     = 0;
    container.max_count = 0;
}

void Assert_No_Collision(Entity_ID id, Entity_ID mask) {
    Assert((id & mask) == 0);
}

Human_ID Next_Human_ID(Entity_ID& last_entity_id) {
    auto ent = ++last_entity_id;
    Assert_No_Collision(ent, Human::component_mask);
    return ent | Human::component_mask;
}

Building_ID Next_Building_ID(Entity_ID& last_entity_id) {
    auto ent = ++last_entity_id;
    Assert_No_Collision(ent, Building::component_mask);
    return ent | Building::component_mask;
}

Graph_Segment_ID Next_Graph_Segment_ID(Entity_ID& last_entity_id) {
    auto ent = ++last_entity_id;
    Assert_No_Collision(ent, Graph_Segment::component_mask);
    return ent | Graph_Segment::component_mask;
}

Map_Resource_ID Next_Map_Resource_ID(Entity_ID& last_entity_id) {
    auto ent = ++last_entity_id;
    Assert_No_Collision(ent, Map_Resource::component_mask);
    return ent | Map_Resource::component_mask;
}

void Place_Building(
    Game_State&          state,
    v2i16                pos,
    Scriptable_Building* scriptable_building,
    bool                 built,
    MCTX
) {
    auto& game_map = state.game_map;
    auto  gsize    = game_map.size;
    Assert(Pos_Is_In_Bounds(pos, gsize));

    auto     bid = Next_Building_ID(game_map.last_entity_id);
    Building b{};
    b.pos        = pos;
    b.scriptable = scriptable_building;

    {
        auto [id, value] = game_map.buildings.Add(ctx);
        *id              = bid;
        *value           = b;
    }

    C_Sprite sprite{};
    sprite.anchor = {0.5f, 0};
    sprite.pos    = pos;

    if (built) {
        Assert(scriptable_building->type == Building_Type::City_Hall);

        City_Hall c{};
        c.time_since_human_was_created = f32_inf;
        {
            auto [id, value] = game_map.city_halls.Add(ctx);
            *id              = bid;
            *value           = c;
        }

        sprite.texture = scriptable_building->texture;
    }
    else {
        for (auto pair_ptr : Iter(&scriptable_building->construction_resources)) {
            auto& [resource, count] = *pair_ptr;

            *game_map.resources_booking_queue.Vector_Occupy_Slot(ctx)
                = Map_Resource_To_Book(resource, count, bid);
        }

        Not_Constructed_Building c{};
        c.constructor         = Human_Constructor_ID_Missing;
        c.construction_points = 0;

        {
            auto [id, value] = game_map.not_constructed_buildings.Add(ctx);
            *id              = bid;
            *value           = c;
        }

        sprite.texture = state.renderer_state->building_in_progress_texture;
    }

    // state.renderer_state->sprites.Add(bid, sprite, ctx);

    auto& tile = *(game_map.element_tiles + (ptrdiff_t)(gsize.x * pos.y + pos.x));
    Assert(tile.type == Element_Tile_Type::None);
    tile.type        = Element_Tile_Type::Building;
    tile.building_id = bid;
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

// TODO: rename to Human_Controller_Dependencies
struct Human_Data {
    Game_State* state;
    Game_Map*   game_map;
    Arena*      trash_arena;
};

void Root_Set_Human_State(
    Human&            human,
    Human_Main_State  new_state,
    const Human_Data& data,
    MCTX
);

void Advance_Moving_To(Human_Moving_Component& moving) {
    if (moving.path.count == 0) {
        moving.elapsed  = 0;
        moving.progress = 0;
        moving.to.reset();
    }
    else {
        moving.to = moving.path.Dequeue();
    }
}

void Human_Moving_Component_Add_Path(
    Human_Moving_Component& moving,
    v2i16*                  path,
    i32                     path_count,
    MCTX
) {
    moving.path.Reset();

    if (path_count != 0) {
        Assert(path != nullptr);

        if (moving.to.value_or(moving.pos) == path[0]) {
            path++;
            path_count--;
        }

        memcpy(  //
            moving.path.Bulk_Enqueue(path_count, ctx),
            path,
            sizeof(*path) * path_count
        );
    }

    if (!moving.to.has_value())
        Advance_Moving_To(moving);
}

Building* Query_Building(Game_Map& game_map, Building_ID id) {
    Assert(id != Building_ID_Missing);

    for (auto [instance_id, instance] : Iter(&game_map.buildings)) {
        if (instance_id == id)
            return instance;
    }

    INVALID_PATH;
    return nullptr;
}

Human* Query_Human(Game_Map& game_map, Human_ID id) {
    Assert(id != Human_ID_Missing);

    for (auto [instance_id, instance] : Iter(&game_map.humans)) {
        if (instance_id == id)
            return instance;
    }

    INVALID_PATH;
    return nullptr;
}

Graph_Segment* Query_Graph_Segment(Game_Map& game_map, Graph_Segment_ID id) {
    return game_map.segments.Find(id);
}

struct Human_Moving_In_The_World_Controller {
    static void On_Enter(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        if (human.segment_id != Graph_Segment_ID_Missing) {
            // TODO: After implementing resources.
            // LOG_DEBUG(
            //     "human.segment.resources_to_transport.size() = {}",
            //     human.segment.resources_to_transport.size()
            // );
        }

        human.moving.path.Reset();

        Update_States(
            human, data, Graph_Segment_ID_Missing, Building_ID_Missing, nullptr, ctx
        );
    }

    static void On_Exit(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        human.state_moving_in_the_world = Moving_In_The_World_State::None;
        human.moving.to.reset();
        human.moving.path.Reset();

        if (human.type == Human_Type::Employee) {
            Assert(human.building_id != Building_ID_Missing);
            auto& building = *Get_Building(*data.game_map, human.building_id);
            // TODO: building.employee_is_inside = true;
            // C# TODO: Somehow remove this human.
        }
    }

    static void Update(Human& human, const Human_Data& data, f32 /* dt */, MCTX) {
        Update_States(
            human,
            data,
            human.segment_id,
            human.building_id,
            Get_Building(*data.game_map, human.building_id),
            ctx
        );
    }

    static void On_Human_Current_Segment_Changed(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID  old_segment_id,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        Assert(human.type == Human_Type::Transporter);
        Update_States(human, data, old_segment_id, Building_ID_Missing, nullptr, ctx);
    }

    static void
    On_Human_Moved_To_The_Next_Tile(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;
        LOG_DEBUG("human.moving {}.{}", human.moving.pos.x, human.moving.pos.y);

        if (human.type == Human_Type::Constructor        //
            && human.building_id != Building_ID_Missing  //
            && human.moving.pos
                   == Get_Building(*data.game_map, human.building_id)->pos  //
        )
        {
            Root_Set_Human_State(human, Human_Main_State::Building, data, ctx);
        }

        if (human.type == Human_Type::Employee) {
            Assert(human.building_id != Building_ID_Missing);
            auto& building = *Get_Building(*data.game_map, human.building_id);

            if (human.moving.pos == building.pos) {
                Assert_False(human.moving.to.has_value());

                // TODO: data.game_map->Employee_Reached_Building_Callback(human);
                // building.employee_is_inside = true;
            }
        }
    }

    static void Update_States(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID  old_segment_id,
        Building_ID       old_building_id,
        Building* /* old_building */,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        auto& game_map = *data.game_map;

        if (human.segment_id != Graph_Segment_ID_Missing) {
            Assert(human.type == Human_Type::Transporter);

            auto& segment = *Query_Graph_Segment(game_map, human.segment_id);

            // NOTE: Следующая клетка, на которую перейдёт (или уже находится) чувак,
            // - это клетка его сегмента. Нам уже не нужно помнить его путь.
            if (human.moving.to.has_value()
                && Graph_Contains(segment.graph, human.moving.to.value())
                && Graph_Node(segment.graph, human.moving.to.value()) != 0)
            {
                human.moving.path.Reset();
                return;
            }

            // NOTE: Чувак перешёл на клетку сегмента. Переходим на Moving_Inside_Segment.
            if (!(human.moving.to.has_value())
                && Graph_Contains(segment.graph, human.moving.pos)
                && Graph_Node(segment.graph, human.moving.pos) != 0)
            {
                LOG_DEBUG(
                    "Root_Set_Human_State(human, "
                    "Human_Main_State::Moving_Inside_Segment, data, ctx)"
                );
                Root_Set_Human_State(
                    human, Human_Main_State::Moving_Inside_Segment, data, ctx
                );
                return;
            }

            auto moving_to_destination = Moving_In_The_World_State::Moving_To_Destination;
            if (old_segment_id != human.segment_id
                || human.state_moving_in_the_world != moving_to_destination)
            {
                LOG_DEBUG(
                    "Setting human.state_moving_in_the_world = "
                    "Moving_In_The_World_State::Moving_To_Destination"
                );
                human.state_moving_in_the_world
                    = Moving_In_The_World_State::Moving_To_Destination;

                Assert(data.trash_arena != nullptr);

                auto [success, path, path_count] = Find_Path(
                    *data.trash_arena,
                    game_map.size,
                    game_map.terrain_tiles,
                    game_map.element_tiles,
                    human.moving.to.value_or(human.moving.pos),
                    Assert_Deref(segment.graph.data).center,
                    true
                );

                Assert(success);
                Assert(path_count > 0);

                Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
            }
        }
        else if (human.building_id != Building_ID_Missing) {
            auto& building = *Get_Building(*data.game_map, human.building_id);

            auto is_constructor_or_employee = human.type == Human_Type::Constructor
                                              || human.type == Human_Type::Employee;
            Assert(is_constructor_or_employee);

            if (human.type == Human_Type::Constructor) {
                // TODO: Assert_False(building.is_constructed);
            }
            else if (human.type == Human_Type::Employee) {
                // TODO: Assert(building.is_constructed);
            }

            if (old_building_id != human.building_id) {
                Assert(data.trash_arena != nullptr);

                TEMP_USAGE(*data.trash_arena);
                auto [success, path, path_count] = Find_Path(
                    *data.trash_arena,
                    game_map.size,
                    game_map.terrain_tiles,
                    game_map.element_tiles,
                    human.moving.to.value_or(human.moving.pos),
                    building.pos,
                    true
                );

                Assert(success);
                Assert(path_count > 0);

                Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
            }
        }
        else if (human.state_moving_in_the_world != Moving_In_The_World_State::Moving_To_The_City_Hall)
        {
            LOG_DEBUG(
                "human.state_moving_in_the_world = "
                "Moving_In_The_World_State::Moving_To_The_City_Hall"
            );
            human.state_moving_in_the_world
                = Moving_In_The_World_State::Moving_To_The_City_Hall;

            Building& city_hall = Assert_Deref(Query_Building(game_map, human.player_id));

            TEMP_USAGE(*data.trash_arena);
            auto [success, path, path_count] = Find_Path(
                *data.trash_arena,
                game_map.size,
                game_map.terrain_tiles,
                game_map.element_tiles,
                human.moving.to.value_or(human.moving.pos),
                city_hall.pos,
                true
            );

            Assert(success);
            Assert(path_count > 0);

            Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
        }
    }
};

struct Human_Moving_Inside_Segment {
    static void On_Enter(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        Assert(human.segment_id != Graph_Segment_ID_Missing);
        Assert(!human.moving.to.has_value());
        Assert(human.moving.path.count == 0);

        auto& game_map = *data.game_map;

        auto& segment = *Query_Graph_Segment(game_map, human.segment_id);

        if (segment.resources_to_transport.count == 0) {
            TEMP_USAGE(*data.trash_arena);
            LOG_DEBUG("Calculating path to the center of the segment");

            auto [success, path, path_count] = Find_Path(
                *data.trash_arena,
                game_map.size,
                game_map.terrain_tiles,
                game_map.element_tiles,
                human.moving.to.value_or(human.moving.pos),
                Assert_Deref(segment.graph.data).center,
                true
            );

            Assert(success);

            Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
        }
    }

    static void On_Exit(Human& human, const Human_Data& /* data */, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        human.moving.path.Reset();
    }

    static void Update(Human& human, const Human_Data& data, f32 /* dt */, MCTX) {
        Update_States(
            human, data, Graph_Segment_ID_Missing, Building_ID_Missing, nullptr, ctx
        );
    }

    static void On_Human_Current_Segment_Changed(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID /* old_segment_id */,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        // Tracing.Log("_controller.SetState(human, HumanState.MovingInTheWorld)");
        Root_Set_Human_State(human, Human_Main_State::Moving_In_The_World, data, ctx);
    }

    static void On_Human_Moved_To_The_Next_Tile(
        Human& /* human */,
        const Human_Data& /* data */,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        // NOTE: Intentionally left blank.
    }

    static void Update_States(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID /* old_segment_id */,
        Building_ID /* old_building_id */,
        Building* /* old_building */,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        if (human.segment_id == Graph_Segment_ID_Missing) {
            human.moving.path.Reset();
            Root_Set_Human_State(human, Human_Main_State::Moving_In_The_World, data, ctx);
            return;
        }

        auto& segment = *Query_Graph_Segment(*data.game_map, human.segment_id);

        if (segment.resources_to_transport.count > 0) {
            if (!human.moving.to.has_value()) {
                // TODO:
                // Tracing.Log("_controller.SetState(human, HumanState.MovingItem)");
                Root_Set_Human_State(human, Human_Main_State::Moving_Resource, data, ctx);
                return;
            }

            human.moving.path.Reset();
        }
    }
};

struct Human_Moving_Resources {
    static void On_Enter(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void On_Exit(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void Update(Human& human, const Human_Data& data, f32 dt, MCTX) {
        // TODO:
    }

    static void On_Human_Current_Segment_Changed(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID  old_segment_id,
        MCTX
    ) {
        // TODO:
    }

    static void
    On_Human_Moved_To_The_Next_Tile(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void Update_States(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID  old_segment_id,
        Building_ID       old_building_id,
        Building*         old_building,
        MCTX
    ) {
        // TODO:
    }
};

struct Human_Construction_Controller {
    static void On_Enter(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void On_Exit(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void Update(Human& human, const Human_Data& data, f32 dt, MCTX) {
        // TODO:
    }

    static void On_Human_Current_Segment_Changed(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID  old_segment_id,
        MCTX
    ) {
        // TODO:
    }

    static void
    On_Human_Moved_To_The_Next_Tile(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void Update_States(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID  old_segment_id,
        Building_ID       old_building_id,
        Building*         old_building,
        MCTX
    ) {
        // TODO:
    }
};

struct Human_Employee_Controller {
    static void On_Enter(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void On_Exit(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void Update(Human& human, const Human_Data& data, f32 dt, MCTX) {
        // TODO:
    }

    static void On_Human_Current_Segment_Changed(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID  old_segment_id,
        MCTX
    ) {
        // TODO:
    }

    static void
    On_Human_Moved_To_The_Next_Tile(Human& human, const Human_Data& data, MCTX) {
        // TODO:
    }

    static void Update_States(
        Human&            human,
        const Human_Data& data,
        Graph_Segment_ID  old_segment_id,
        Building_ID       old_building_id,
        Building*         old_building,
        MCTX
    ) {
        // TODO:
    }
};

void Human_Root_Update(Human& human, const Human_Data& data, f32 dt, MCTX) {
    switch (human.state) {
    case Human_Main_State::Moving_In_The_World:
        Human_Moving_In_The_World_Controller::Update(human, data, dt, ctx);
        break;

    case Human_Main_State::Moving_Inside_Segment:
        Human_Moving_Inside_Segment::Update(human, data, dt, ctx);
        break;

    case Human_Main_State::Moving_Resource:
        Human_Moving_Resources::Update(human, data, dt, ctx);
        break;

    case Human_Main_State::Building:
        Human_Construction_Controller::Update(human, data, dt, ctx);
        break;

    case Human_Main_State::Employee:
        Human_Employee_Controller::Update(human, data, dt, ctx);
        break;

    default:
        INVALID_PATH;
    }
}

void Root_On_Human_Current_Segment_Changed(
    Human_ID /* id */,
    Human&            human,
    const Human_Data& data,
    Graph_Segment_ID  old_segment_id,
    MCTX
) {
    switch (human.state) {
    case Human_Main_State::Moving_In_The_World:
        Human_Moving_In_The_World_Controller::On_Human_Current_Segment_Changed(
            human, data, old_segment_id, ctx
        );
        break;

    case Human_Main_State::Moving_Inside_Segment:
        Human_Moving_Inside_Segment::On_Human_Current_Segment_Changed(
            human, data, old_segment_id, ctx
        );
        break;

    case Human_Main_State::Moving_Resource:
        Human_Moving_Resources::On_Human_Current_Segment_Changed(
            human, data, old_segment_id, ctx
        );
        break;

    case Human_Main_State::Building:
    default:
        INVALID_PATH;
    }
}

void Root_On_Human_Moved_To_The_Next_Tile(Human& human, const Human_Data& data, MCTX) {
    switch (human.state) {
    case Human_Main_State::Moving_In_The_World:
        Human_Moving_In_The_World_Controller::On_Human_Moved_To_The_Next_Tile(
            human, data, ctx
        );
        break;

    case Human_Main_State::Moving_Inside_Segment:
        Human_Moving_Inside_Segment::On_Human_Moved_To_The_Next_Tile(human, data, ctx);
        break;

    case Human_Main_State::Moving_Resource:
        Human_Moving_Resources::On_Human_Moved_To_The_Next_Tile(human, data, ctx);
        break;

    case Human_Main_State::Employee:
        Human_Employee_Controller::On_Human_Moved_To_The_Next_Tile(human, data, ctx);
        break;

    case Human_Main_State::Building:
    default:
        INVALID_PATH;
    }
}

void Root_Set_Human_State(
    Human&            human,
    Human_Main_State  new_state,
    const Human_Data& data,
    MCTX
) {
    CTX_LOGGER;
    LOG_TRACING_SCOPE;
    auto old_state = human.state;
    human.state    = new_state;

    if (old_state != Human_Main_State::None) {
        switch (old_state) {
        case Human_Main_State::Moving_In_The_World:
            Human_Moving_In_The_World_Controller::On_Exit(human, data, ctx);
            break;

        case Human_Main_State::Moving_Inside_Segment:
            Human_Moving_Inside_Segment::On_Exit(human, data, ctx);
            break;

        case Human_Main_State::Moving_Resource:
            Human_Moving_Resources::On_Exit(human, data, ctx);
            break;

        case Human_Main_State::Building:
            Human_Construction_Controller::On_Exit(human, data, ctx);
            break;

        case Human_Main_State::Employee:
            break;

        default:
            INVALID_PATH;
        }
    }

    human.state_moving_in_the_world = Moving_In_The_World_State::Moving_To_Destination;

    switch (new_state) {
    case Human_Main_State::Moving_In_The_World:
        Human_Moving_In_The_World_Controller::On_Enter(human, data, ctx);
        break;

    case Human_Main_State::Moving_Inside_Segment:
        Human_Moving_Inside_Segment::On_Enter(human, data, ctx);
        break;

    case Human_Main_State::Moving_Resource:
        Human_Moving_Resources::On_Enter(human, data, ctx);
        break;

    case Human_Main_State::Building:
        Human_Construction_Controller::On_Enter(human, data, ctx);
        break;

    case Human_Main_State::Employee:
        // TODO: Human_Employee_Controller::Switch_To_The_Next_Behaviour(human);
        break;

    default:
        INVALID_PATH;
    }
}

// NOTE: Создание чувачка-грузчика.
// Он добавляется в game_map.humans_to_add, после чего перекидывается в gama_map.humans.
// Привязка к сегменту происходит в этот момент.
std::tuple<Human_ID, Human*> Create_Human_Transporter(
    Game_Map&         game_map,
    v2i16             pos,
    Graph_Segment_ID  segment_id,
    Player_ID         player_id,
    const Human_Data& data,
    MCTX
) {
    CTX_LOGGER;
    LOG_TRACING_SCOPE;
    LOG_DEBUG("Creating a new human...");

    Human human{};
    human.player_id       = player_id;
    human.moving.pos      = pos;
    human.moving.elapsed  = 0;
    human.moving.progress = 0;
    human.moving.from     = pos;
    human.moving.to.reset();
    human.moving.path.count         = 0;
    human.moving.path.max_count     = 0;
    human.moving.path.base          = nullptr;
    human.segment_id                = segment_id;
    human.type                      = Human_Type::Transporter;
    human.state                     = Human_Main_State::None;
    human.state_moving_in_the_world = Moving_In_The_World_State::None;
    human.building_id               = Building_ID_Missing;

    auto [human_id, human_ptr] = game_map.humans_to_add.Add(ctx);

    *human_id  = Next_Human_ID(game_map.last_entity_id);
    *human_ptr = human;

    Root_Set_Human_State(*human_ptr, Human_Main_State::Moving_In_The_World, data, ctx);

    // TODO:
    // onHumanCreated.OnNext(new() { human = human });
    //
    // if (building.scriptable.type == BuildingType.SpecialCityHall) {
    //     onCityHallCreatedHuman.OnNext(new() { cityHall = building });
    //     DomainEvents<E_CityHallCreatedHuman>.Publish(new() { cityHall = building });
    // }

    // NOTE: Привязка будет при переносе из humans_to_add в humans.
    auto& segment             = *Query_Graph_Segment(game_map, segment_id);
    segment.assigned_human_id = Human_ID_Missing;

    return {*human_id, human_ptr};
}

// void Update_Building__Constructed(
//     Game_State&       state,
//     Game_Map&         game_map,
//     Building*       building,
//     f32               dt,
//     const Human_Data& data,
//     MCTX
// ) {
//     auto& scriptable = *Get_Scriptable_Building(state, building->scriptable);
//
//     auto delay = scriptable.human_spawning_delay;
//
//     if (scriptable.type == Building_Type::City_Hall) {
//         auto& since_created = building->time_since_human_was_created;
//         since_created += dt;
//         if (since_created > delay)
//             since_created = delay;
//
//         if (game_map.segments_wo_humans.count > 0) {
//             if (since_created >= delay) {
//                 since_created -= delay;
//                 Graph_Segment* segment = Dequeue(game_map.segments_wo_humans);
//                 Create_Human_Transporter(game_map, building, segment, data, ctx);
//             }
//         }
//     }
//
//     // TODO: _building_controller.Update(building, dt);
// }

void Map_Buildings_With_City_Halls(
    Game_Map&                                                 game_map,
    std::invocable<Building_ID, Building*, City_Hall*> auto&& func
) {
    for (auto [building_id, building] : Iter(&game_map.buildings)) {
        for (auto [city_hall_id, city_hall] : Iter(&game_map.city_halls)) {
            if (building_id != city_hall_id)
                continue;

            func(building_id, building, city_hall);
        }
    }
}

void Process_City_Halls(Game_State& state, f32 dt, const Human_Data& data, MCTX) {
    CTX_ALLOCATOR;
    CTX_LOGGER;

    auto& game_map = state.game_map;

    Map_Buildings_With_City_Halls(
        game_map,
        [&](Building_ID /* id */, Building* building, City_Hall* city_hall) {
            auto delay = building->scriptable->human_spawning_delay;

            auto& since_created = city_hall->time_since_human_was_created;
            since_created += dt;
            if (since_created > delay)
                since_created = delay;

            if (game_map.segments_wo_humans.count > 0) {
                if (since_created >= delay) {
                    since_created -= delay;
                    Create_Human_Transporter(
                        game_map,
                        building->pos,
                        game_map.segments_wo_humans.Dequeue(),
                        building->player_id,
                        data,
                        ctx
                    );
                }
            }
        }
    );
}

void Process_Humans_Moving_To_City_Halls() {
    // TODO:
}

void Map_Humans_To_Remove(
    Game_Map&                                                     game_map,
    std::invocable<Human_ID, Human&, Human_Removal_Reason> auto&& func
) {
    for (auto [id, reason] : Iter(&game_map.humans_to_remove))
        func(id, *Query_Human(game_map, id), *reason);
}

void Remove_Humans(Game_Map& game_map) {
    Map_Humans_To_Remove(
        game_map,
        [&](Human_ID id, Human& human, Human_Removal_Reason reason) {
            if (reason == Human_Removal_Reason::Transporter_Returned_To_City_Hall) {
                // TODO: on_Human_Reached_City_Hall.On_Next(new (){human = human});
            }
            else if (reason == Human_Removal_Reason::Employee_Reached_Building) {
                // TODO: on_Employee_Reached_Building.On_Next(new (){human = human});
                // TODO: human.building->employee = nullptr;
                Assert(human.building_id != Building_ID_Missing);
            }

            game_map.humans.Unstable_Remove(id);
        }
    );

    game_map.humans_to_remove.Reset();
}

void Update_Human_Moving_Component(
    Game_Map&         game_map,
    Human&            human,
    float             dt,
    const Human_Data& data,
    MCTX
) {
    CTX_LOGGER;
    CTX_ALLOCATOR;

    auto& game_map_data = Assert_Deref(game_map.data);

    auto&      moving   = human.moving;
    const auto duration = game_map_data.human_moving_one_tile_duration;
    moving.elapsed += dt;

    constexpr int GUARD_MAX_MOVING_TILES_PER_FRAME = 4;

    auto iteration = 0;
    while (iteration < 10 * GUARD_MAX_MOVING_TILES_PER_FRAME  //
           && moving.to.has_value()                           //
           && moving.elapsed > duration)
    {
        LOG_TRACING_SCOPE;

        iteration++;

        moving.elapsed -= duration;

        moving.pos  = moving.to.value();
        moving.from = moving.pos;
        Advance_Moving_To(moving);

        Root_On_Human_Moved_To_The_Next_Tile(human, data, ctx);
        // TODO: on_Human_Moved_To_The_Next_Tile.On_Next(new (){ human = human });
    }

    Assert(iteration < 10 * GUARD_MAX_MOVING_TILES_PER_FRAME);
    if (iteration >= GUARD_MAX_MOVING_TILES_PER_FRAME) {
        LOG_TRACING_SCOPE;
        LOG_WARN("WTF? iteration >= GUARD_MAX_MOVING_TILES_PER_FRAME");
    }

    moving.progress = MIN(1.0f, moving.elapsed / duration);
    SANITIZE;
}

void Update_Human(
    Game_Map&         game_map,
    Human_ID          id,
    Human*            human_ptr,
    float             dt,
    const Human_Data& data,
    MCTX
) {
    CTX_ALLOCATOR;

    auto& human            = *human_ptr;
    auto& humans_to_remove = game_map.humans_to_remove;

    if (human.moving.to.has_value())
        Update_Human_Moving_Component(game_map, human, dt, data, ctx);

    if (humans_to_remove.count > 0 && humans_to_remove.Contains(id))
        return;

    Human_Root_Update(human, data, dt, ctx);

    auto state = Moving_In_The_World_State::Moving_To_The_City_Hall;
    if (human.state_moving_in_the_world == state && !human.moving.to.has_value()
        && human.moving.pos == Query_Building(game_map, human.building_id)->pos)
    {
        // TODO: auto rem = Game_Map::Human_To_Remove{
        //     Human_Removal_Reason::Transporter_Returned_To_City_Hall, human_ptr, locator
        // };
        // Vector_Add(humans_to_remove, std::move(rem));

        auto [r_id, r_value] = humans_to_remove.Add(ctx);

        *r_id    = id;
        *r_value = Human_Removal_Reason::Transporter_Returned_To_City_Hall;
    }

    SANITIZE;
}

void Update_Humans(Game_State& state, f32 dt, const Human_Data& data, MCTX) {
    CTX_LOGGER;

    auto& game_map = state.game_map;

    for (auto [id, reason_ptr] : Iter(&game_map.humans_to_remove))
        Assert(*reason_ptr != Human_Removal_Reason::Transporter_Returned_To_City_Hall);

    Remove_Humans(game_map);

    for (auto [id, human_ptr] : Iter(&game_map.humans))
        Update_Human(game_map, id, human_ptr, dt, data, ctx);

    auto prev_count = game_map.humans_to_add.count;
    for (auto [id, human_to_move] : Iter(&game_map.humans_to_add)) {
        LOG_DEBUG("Update_Humans: moving human from humans_to_add to humans");
        auto [r_id, human_ptr] = game_map.humans.Add(ctx);

        *r_id      = id;
        *human_ptr = *human_to_move;

        auto& human = *human_ptr;

        if (human.segment_id != Graph_Segment_ID_Missing) {
            auto& segment             = *Query_Graph_Segment(game_map, human.segment_id);
            segment.assigned_human_id = id;
        }

        Update_Human(game_map, id, human_ptr, dt, data, ctx);
    }

    Assert(prev_count == game_map.humans_to_add.count);
    game_map.humans_to_add.Reset();

    Remove_Humans(game_map);

    {  // NOTE: Debug shiet.
        int humans                         = 0;
        int humans_moving_to_the_city_hall = 0;
        int humans_moving_to_destination   = 0;
        int humans_moving_inside_segment   = 0;
        for (auto [id, human_ptr] : Iter(&game_map.humans)) {
            humans++;
            auto& human = *human_ptr;

            if (human.state == Human_Main_State::Moving_In_The_World) {
                if (human.state_moving_in_the_world
                    == Moving_In_The_World_State::Moving_To_The_City_Hall)
                {
                    humans_moving_to_the_city_hall++;
                }
                else if (human.state_moving_in_the_world  //
                         == Moving_In_The_World_State::Moving_To_Destination)
                {
                    humans_moving_to_destination++;
                }
            }
            else if (human.state == Human_Main_State::Moving_Inside_Segment) {
                humans_moving_inside_segment++;
            }
        }

        ImGui::Text("humans %d", humans);
        ImGui::Text("humans_moving_to_the_city_hall %d", humans_moving_to_the_city_hall);
        ImGui::Text("humans_moving_to_destination %d", humans_moving_to_destination);
        ImGui::Text("humans_moving_inside_segment %d", humans_moving_inside_segment);
    }
}

void Update_Game_Map(Game_State& state, float dt, MCTX) {
    auto& game_map    = state.game_map;
    auto& trash_arena = state.trash_arena;

    ImGui::Text("last_segments_to_add_count %d", global_last_segments_to_add_count);
    ImGui::Text("last_segments_to_delete_count %d", global_last_segments_to_delete_count);
    ImGui::Text("game_map.segments.count %d", game_map.segments.count);

    Process_City_Halls(state, dt, Assert_Deref(state.game_map.human_data), ctx);
    Update_Humans(state, dt, Assert_Deref(state.game_map.human_data), ctx);
}

void Add_Map_Resource(
    Game_State&          state,
    Scriptable_Resource* scriptable,
    const v2i16          pos,
    MCTX
) {
    CTX_ALLOCATOR;
    auto& game_map    = state.game_map;
    auto  gsize       = game_map.size;
    auto& trash_arena = state.trash_arena;

    Map_Resource resource{};
    resource.scriptable = scriptable;
    resource.pos        = pos;
    resource.booking    = Map_Resource_Booking_ID_Missing;
    Init_Vector(resource.transportation_segments, ctx);
    Init_Vector(resource.transportation_vertices, ctx);
    resource.targeted_human = Human_ID_Missing;
    resource.carrying_human = Human_ID_Missing;

    {
        auto [rid, rvalue] = game_map.resources.Add(ctx);
        *rid               = Next_Map_Resource_ID(game_map.last_entity_id);
        *rvalue            = resource;
    }
}

void Init_Game_Map(
    bool /* first_time_initializing */,
    bool /* hot_reloaded */,
    Game_State& state,
    Arena&      arena,
    MCTX
) {
    auto& game_map = state.game_map;

    game_map.last_entity_id = 0;

    game_map.data = std::construct_at(Allocate_For(arena, Game_Map_Data), 0.3f);
    {
        auto human_data         = Allocate_For(arena, Human_Data);
        human_data->game_map    = &state.game_map;
        human_data->trash_arena = &state.trash_arena;
        human_data->state       = &state;

        game_map.human_data = human_data;
    }

    std::construct_at(&game_map.segments);
    std::construct_at(&game_map.buildings);
    std::construct_at(&game_map.not_constructed_buildings);
    std::construct_at(&game_map.city_halls);
    std::construct_at(&game_map.humans);
    std::construct_at(&game_map.humans_going_to_the_city_hall);
    std::construct_at(&game_map.humans_to_add);
    std::construct_at(&game_map.humans_to_remove);
    std::construct_at(&game_map.resources);

    Init_Queue(game_map.segments_wo_humans, ctx);
    Init_Vector(game_map.resources_booking_queue, ctx);
}

void Post_Init_Game_Map(
    bool /* first_time_initializing */,
    bool /* hot_reloaded */,
    Game_State& state,
    Arena& /* arena */,
    MCTX
) {
    bool built = true;
    Place_Building(state, {2, 2}, state.scriptable_buildings + 0, built, ctx);

    // TODO: !!!
    Assert(state.scriptable_resources_count > 0);
    Add_Map_Resource(state, state.scriptable_resources + 0, {0, 0}, ctx);
}

void Deinit_Game_Map(Game_State& state, MCTX) {
    CTX_ALLOCATOR;
    auto& game_map = state.game_map;

    for (auto [id, segment_ptr] : Iter(&game_map.segments)) {
        auto& segment = *segment_ptr;

        FREE(segment.vertices, sizeof(v2i16) * segment.vertices_count);
        FREE(segment.graph.nodes, sizeof(u8) * segment.graph.nodes_allocation_count);

        segment.linked_segments.Reset();
        Deinit_Queue(segment.resources_to_transport, ctx);

        Assert(segment.graph.nodes != nullptr);
        Assert(segment.graph.data != nullptr);

        auto& data = *segment.graph.data;
        Assert(data.dist != nullptr);
        Assert(data.prev != nullptr);

        data.node_index_2_pos.clear();
        data.pos_2_node_index.clear();

        auto n = segment.graph.nodes_count;
        FREE(data.dist, sizeof(i16) * n * n);
        FREE(data.prev, sizeof(i16) * n * n);
    }

    Deinit_Sparse_Array(game_map.segments, ctx);
    Deinit_Queue(game_map.segments_wo_humans, ctx);

    Deinit_Sparse_Array(game_map.buildings, ctx);
    Deinit_Sparse_Array(game_map.not_constructed_buildings, ctx);
    Deinit_Sparse_Array(game_map.city_halls, ctx);

    for (auto [_, human] : Iter(&game_map.humans))
        Deinit_Queue(human->moving.path, ctx);
    Deinit_Sparse_Array(game_map.humans, ctx);

    Deinit_Sparse_Array(game_map.humans_going_to_the_city_hall, ctx);
    Deinit_Sparse_Array(game_map.humans_to_add, ctx);
    Deinit_Sparse_Array(game_map.humans_to_remove, ctx);
    Deinit_Sparse_Array(game_map.resources, ctx);

    Deinit_Vector(game_map.resources_booking_queue, ctx);
}

void Regenerate_Terrain_Tiles(
    Game_State& /* state */,
    Game_Map& game_map,
    Arena& /* arena */,
    Arena& trash_arena,
    uint /* seed */,
    Editor_Data& data,
    MCTX_
) {
    auto gsize = game_map.size;

    auto noise_pitch = Ceil_To_Power_Of_2((u32)MAX(gsize.x, gsize.y));
    auto output_size = noise_pitch * noise_pitch;
    TEMP_USAGE(trash_arena);

    auto terrain_perlin = Allocate_Array(trash_arena, u16, output_size);
    Fill_Perlin_2D(
        terrain_perlin,
        sizeof(u16) * output_size,
        trash_arena,
        data.terrain_perlin,
        noise_pitch,
        noise_pitch
    );

    auto forest_perlin = Allocate_Array(trash_arena, u16, output_size);
    Fill_Perlin_2D(
        forest_perlin,
        sizeof(u16) * output_size,
        trash_arena,
        data.forest_perlin,
        noise_pitch,
        noise_pitch
    );

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile   = Get_Terrain_Tile(game_map, {x, y});
            tile.terrain = Terrain::Grass;
            auto noise   = (f32)(*(terrain_perlin + noise_pitch * y + x)) / (f32)u16_max;
            tile.height  = int((data.terrain_max_height + 1) * noise);

            Assert(tile.height >= 0);
            Assert(tile.height <= data.terrain_max_height);
        }
    }

    // NOTE: Removing one-tile-high grass blocks because they'd look ugly.
    while (true) {
        bool changed = false;
        FOR_RANGE (int, y, gsize.y) {
            FOR_RANGE (int, x, gsize.x) {
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

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile = Get_Terrain_Tile(game_map, {x, y});
            if (tile.is_cliff)
                continue;

            tile.is_cliff
                = y == 0 || tile.height > Get_Terrain_Tile(game_map, {x, y - 1}).height;
        }
    }

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile     = Get_Terrain_Tile(game_map, {x, y});
            auto& resource = Get_Terrain_Resource(game_map, {x, y});

            auto noise    = *(forest_perlin + noise_pitch * y + x) / (f32)u16_max;
            bool generate = (!tile.is_cliff) && (noise > data.forest_threshold);

            // TODO: прикрутить terrain-ресурс леса
            // resource.scriptable = global_forest_resource_id * generate;
            resource.amount = data.forest_max_amount * generate;
        }
    }

    // TODO: Element Tiles
    // element_tiles = _initialMapProvider.LoadElementTiles();
    //
    // auto cityHalls = buildings.FindAll(i = > i.scriptable.type ==
    // Building_Type.SpecialCityHall);
    // foreach (auto building in cityHalls) {
    //     auto pos = building.pos;
    //     element_tiles[pos.y][pos.x] = new (Element_Tile_Type::Building, building);
    // }
}

void Regenerate_Element_Tiles(
    Game_State& /* state */,
    Game_Map& game_map,
    Arena& /* arena */,
    Arena& /* trash_arena */,
    uint /* seed */,
    Editor_Data& /* data */,
    MCTX_
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

    v2i16 flag_tiles[] = {
        {2, 2},
        {3, 3},
    };

    auto base_offset = v2i16(1, 1);

    for (auto offset : road_tiles) {
        auto          off  = offset + base_offset;
        Element_Tile& tile = game_map.element_tiles[off.y * gsize.x + off.x];
        tile.type          = Element_Tile_Type::Road;
    }

    for (auto offset : flag_tiles) {
        auto          off  = offset + base_offset;
        Element_Tile& tile = game_map.element_tiles[off.y * gsize.x + off.x];
        tile.type          = Element_Tile_Type::Flag;
    }

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            Element_Tile& tile = game_map.element_tiles[y * gsize.x + x];
            tile.building_id   = Building_ID_Missing;
            Validate_Element_Tile(tile);
        }
    }
}

struct Updated_Tiles {
    u16                count;
    v2i16*             pos;
    Tile_Updated_Type* type;
};

bool Should_Segment_Be_Deleted(
    v2i16                gsize,
    Element_Tile*        element_tiles,
    const Updated_Tiles& updated_tiles,
    const Graph_Segment& segment
) {
    FOR_RANGE (u16, i, updated_tiles.count) {
        auto& tile_pos     = *(updated_tiles.pos + i);
        auto& updated_type = *(updated_tiles.type + i);

        // TODO: Логика работы была изменена после C# репы.
        // Нужно перепроверить, что тут всё работает стабильно.
        switch (updated_type) {
        case Tile_Updated_Type::Road_Placed:
        case Tile_Updated_Type::Building_Placed: {
            for (auto& offset : v2i16_adjacent_offsets) {
                auto pos = tile_pos + offset;
                if (!Pos_Is_In_Bounds(pos, gsize))
                    continue;

                auto& graph     = segment.graph;
                auto  graph_pos = pos;
                graph_pos.x -= graph.offset.x;
                graph_pos.y -= graph.offset.y;

                if (!Pos_Is_In_Bounds(graph_pos, graph.size))
                    continue;

                auto node
                    = *(graph.nodes
                        + (ptrdiff_t)(graph.size.x * graph_pos.y + graph_pos.x));
                if (node == 0)
                    continue;

                auto& tile = *(element_tiles + (ptrdiff_t)(gsize.x * pos.y + pos.x));
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

            auto& graph     = segment.graph;
            auto  graph_pos = pos;
            graph_pos.x -= graph.offset.x;
            graph_pos.y -= graph.offset.y;

            if (!Pos_Is_In_Bounds(graph_pos, graph.size))
                break;

            u8 node
                = *(graph.nodes + (ptrdiff_t)(graph.size.x * graph_pos.y + graph_pos.x));
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
    STATEMENT({                                                     \
        Assert((max_count_) >= (count_));                           \
                                                                    \
        auto found = false;                                         \
        FOR_RANGE (int, i, (count_)) {                              \
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
    })

// v2i16* Allocate_Segment_Vertices(Allocator& allocator, int vertices_count) {
//     return (v2i16*)allocator.Allocate(sizeof(v2i16) * vertices_count, 1);
// }
//
// u8* Allocate_Graph_Nodes(Allocator& allocator, int nodes_count) {
//     return allocator.Allocate(nodes_count, 1);
// }

void Rect_Copy(u8* dest, u8* source, int stride, int rows, int bytes_per_line) {
    FOR_RANGE (int, i, rows) {
        memcpy(
            dest + (ptrdiff_t)(i * bytes_per_line),
            source + (ptrdiff_t)(i * stride),
            bytes_per_line
        );
    }
}

bool Adjacent_Tiles_Are_Connected(Graph& graph, i16 x, i16 y) {
    const auto gx = graph.size.x;

    u8 node = *(graph.nodes + (ptrdiff_t)(y * gx + x));

    FOR_DIRECTION (dir) {
        if (!Graph_Node_Has(node, dir))
            continue;

        auto new_pos = v2i16(x, y) + As_Offset(dir);
        if (!Pos_Is_In_Bounds(new_pos, graph.size))
            return false;

        u8 adjacent_node = *(graph.nodes + gx * new_pos.y + new_pos.x);
        if (!Graph_Node_Has(adjacent_node, Opposite(dir)))
            return false;
    }

    return true;
}

void Assert_Is_Undirected(Graph& graph) {
    Assert(graph.size.x > 0);
    Assert(graph.size.y > 0);

    FOR_RANGE (i16, y, graph.size.y) {
        FOR_RANGE (i16, x, graph.size.x) {
            Assert(Adjacent_Tiles_Are_Connected(graph, x, y));
        }
    }
}

#define _Anon_Variable(name, counter) name##counter
#define Anon_Variable(name, counter) _Anon_Variable(name, counter)

void Calculate_Graph_Data(Graph& graph, Arena& trash_arena, MCTX) {
    TEMP_USAGE(trash_arena);

    CTX_ALLOCATOR;

    auto n      = graph.nodes_count;
    auto nodes  = graph.nodes;
    auto height = graph.size.y;
    auto width  = graph.size.x;

    graph.data = (Calculated_Graph_Data*)ALLOC(sizeof(Calculated_Graph_Data));
    auto& data = *graph.data;

    auto& node_index_2_pos = *std::construct_at(&data.node_index_2_pos);
    auto& pos_2_node_index = *std::construct_at(&data.pos_2_node_index);

    {
        int node_index = 0;
        FOR_RANGE (int, y, height) {
            FOR_RANGE (int, x, width) {
                auto node = nodes[y * width + x];
                if (node == 0)
                    continue;

                data.node_index_2_pos.insert({node_index, v2i16(x, y)});
                data.pos_2_node_index.insert({v2i16(x, y), node_index});

                node_index += 1;
            }
        }
    }

    // NOTE: |V| = _nodes_count
    // > let dist be a |V| × |V| array of minimum distances initialized to ∞ (infinity)
    // > let prev be a |V| × |V| array of minimum distances initialized to null
    auto& dist = data.dist;
    auto& prev = data.prev;
    dist       = (i16*)ALLOC(sizeof(i16) * n * n);
    prev       = (i16*)ALLOC(sizeof(i16) * n * n);
    FOR_RANGE (int, y, n) {
        FOR_RANGE (int, x, n) {
            dist[y * n + x] = i16_max;
            prev[y * n + x] = i16_min;
        }
    }

    // NOTE: edge (u, v) = (node_index, new_node_index)
    // > for each edge (u, v) do
    // >     dist[u][v] ← w(u, v)  // The weight of the edge (u, v)
    // >     prev[u][v] ← u
    {
        int node_index = 0;
        FOR_RANGE (int, y, height) {
            FOR_RANGE (int, x, width) {
                u8 node = nodes[y * width + x];
                if (!node)
                    continue;

                FOR_DIRECTION (dir) {
                    if (!Graph_Node_Has(node, dir))
                        continue;

                    auto new_pos = v2i16(x, y) + As_Offset(dir);
                    Assert(pos_2_node_index.contains(new_pos));
                    auto new_node_index = pos_2_node_index[new_pos];

                    dist[node_index * n + new_node_index] = 1;
                    prev[node_index * n + new_node_index] = node_index;
                }

                node_index += 1;
            }
        }
    }

    // NOTE: vertex v = nodeIndex
    // > for each vertex v do
    // >     dist[v][v] ← 0
    // >     prev[v][v] ← v
    {
        FOR_RANGE (int, node_index, n) {
            dist[node_index * n + node_index] = 0;
            prev[node_index * n + node_index] = node_index;
        }
    }

    // Standard Floyd-Warshall
    // for k from 1 to |V|
    //     for i from 1 to |V|
    //         for j from 1 to |V|
    //             if dist[i][j] > dist[i][k] + dist[k][j] then
    //                 dist[i][j] ← dist[i][k] + dist[k][j]
    //                 prev[i][j] ← prev[k][j]
    //
    FOR_RANGE (int, k, n) {
        FOR_RANGE (int, j, n) {
            FOR_RANGE (int, i, n) {
                auto ij = dist[n * i + j];
                auto ik = dist[n * i + k];
                auto kj = dist[n * k + j];

                if ((ik != i16_max)     //
                    && (kj != i16_max)  //
                    && (ij > ik + kj))
                {
                    dist[i * n + j] = ik + kj;
                    prev[i * n + j] = prev[k * n + j];
                }
            }
        }
    }

#ifdef ASSERT_SLOW
    Assert_Is_Undirected(graph);
#endif  // ASSERT_SLOW

    // NOTE: Вычисление центра графа.
    i16* node_eccentricities = Allocate_Zeros_Array(trash_arena, i16, n);
    FOR_RANGE (i16, i, n) {
        FOR_RANGE (i16, j, n) {
            node_eccentricities[i] = MAX(node_eccentricities[i], dist[n * i + j]);
        }
    }

    i16 rad  = i16_max;
    i16 diam = 0;
    FOR_RANGE (i16, i, n) {
        rad  = MIN(rad, node_eccentricities[i]);
        diam = MAX(diam, node_eccentricities[i]);
    }

    FOR_RANGE (i16, i, n) {
        Assert(node_index_2_pos.contains(i));
        if (node_eccentricities[i] == rad) {
            data.center = node_index_2_pos[i] + graph.offset;
            break;
        }
    }

    SANITIZE;
}

std::tuple<Graph_Segment_ID, Graph_Segment*> Add_And_Link_Segment(
    Entity_ID&                                     last_entity_id,
    Sparse_Array<Graph_Segment_ID, Graph_Segment>& segments,
    Graph_Segment&                                 added_segment,
    Arena&                                         trash_arena,
    MCTX
) {
    CTX_ALLOCATOR;

    // NOTE: Создание финального Graph_Segment,
    // который будет использоваться в игровой логике.
    Graph_Segment segment = added_segment;
    {
        Calculate_Graph_Data(segment.graph, trash_arena, ctx);
        segment.assigned_human_id = Human_ID_Missing;
        Init_Vector(segment.linked_segments, ctx);
    }

    auto [pid, segment1_ptr] = segments.Add(ctx);
    *pid                     = Next_Graph_Segment_ID(last_entity_id);
    *segment1_ptr            = segment;

    auto& id = *pid;

    for (auto [segment2_id, segment2_ptr] : Iter(&segments)) {
        if (segment2_id == id)
            continue;

        // PERF: Мб AABB / QuadTree стоит заюзать.
        auto& segment1 = *segment1_ptr;
        auto& segment2 = *segment2_ptr;
        if (Have_Some_Of_The_Same_Vertices(segment1, segment2)) {
            if (segment2.linked_segments.Index_Of(id) == -1)
                *segment2.linked_segments.Vector_Occupy_Slot(ctx) = id;

            if (segment1.linked_segments.Index_Of(segment2_id) == -1)
                *segment1.linked_segments.Vector_Occupy_Slot(ctx) = segment2_id;
        }
    }

    SANITIZE;

    return {id, segment1_ptr};
}

using Graph_Segments_To_Add = Fixed_Size_Slice<Graph_Segment>;

using Segment_To_Delete        = std::tuple<Graph_Segment_ID, Graph_Segment*>;
using Graph_Segments_To_Delete = Fixed_Size_Slice<Segment_To_Delete>;

BF_FORCE_INLINE void Update_Segments(
    Arena&                    trash_arena,
    Game_State&               state,
    Game_Map&                 game_map,
    Graph_Segments_To_Add&    segments_to_add,
    Graph_Segments_To_Delete& segments_to_delete,
    MCTX
) {
    CTX_ALLOCATOR;
    CTX_LOGGER;
    LOG_TRACING_SCOPE;
    LOG_DEBUG("game_map.segments.count = {}", game_map.segments.count);
    LOG_DEBUG("segments_to_add.count = {}", segments_to_add.count);
    LOG_DEBUG("segments_to_delete.count = {}", segments_to_delete.count);

    TEMP_USAGE(trash_arena);

    auto& segments = game_map.segments;

    global_last_segments_to_add_count    = segments_to_add.count;
    global_last_segments_to_delete_count = segments_to_delete.count;

    // NOTE: Подсчёт максимального количества чувачков, которые были бы без сегментов
    // (с учётом тех, которые уже не имеют сегмент).
    // PERF: можем кешировать количество чувачков,
    // которые уже не имеют сегмента и идут в ратушу.
    auto humans_moving_to_city_hall = 0;
    for (auto [_, human_ptr] : Iter(&game_map.humans)) {
        auto state = Moving_In_The_World_State::Moving_To_The_City_Hall;
        if (human_ptr->state_moving_in_the_world == state)
            humans_moving_to_city_hall++;
    }

    // PERF: Мб стоит переделать так, чтобы мы лишнего не аллоцировали заранее.
    i32 humans_wo_segment_count = 0;
    // NOTE: `Graph_Segment*` is nullable, `Human*` is not.
    using tttt
        = std::tuple<Graph_Segment_ID, Graph_Segment*, std::tuple<Human_ID, Human*>>;
    const i32 humans_wo_segment_max_count
        = segments_to_delete.count + humans_moving_to_city_hall;

    // NOTE: Настекиваем чувачков без сегментов (которые идут в ратушу).
    tttt* humans_wo_segment = nullptr;
    if (humans_wo_segment_max_count > 0) {
        humans_wo_segment
            = Allocate_Array(trash_arena, tttt, humans_wo_segment_max_count);

        for (auto [id, human_ptr] : Iter(&game_map.humans)) {
            auto state = Moving_In_The_World_State::Moving_To_The_City_Hall;
            if (human_ptr->state_moving_in_the_world == state) {
                Array_Push(
                    humans_wo_segment,
                    humans_wo_segment_count,
                    humans_wo_segment_max_count,
                    tttt(Graph_Segment_ID_Missing, nullptr, {id, human_ptr})
                );
            }
        }
    }

    // NOTE: Удаление сегментов (отвязка от чувачков,
    // от других сегментов и высвобождение памяти).
    FOR_RANGE (u32, i, segments_to_delete.count) {
        auto [id, segment_ptr] = segments_to_delete.items[i];
        auto& segment          = *segment_ptr;

        // NOTE: Настекиваем чувачков без сегментов (сегменты которых удалили только что).
        if (segment.assigned_human_id != Human_ID_Missing) {
            auto human_ptr            = Query_Human(game_map, segment.assigned_human_id);
            human_ptr->segment_id     = Graph_Segment_ID_Missing;
            segment.assigned_human_id = Human_ID_Missing;
            Array_Push(
                humans_wo_segment,
                humans_wo_segment_count,
                humans_wo_segment_max_count,
                tttt(id, segment_ptr, {segment.assigned_human_id, human_ptr})
            );
        }

        // NOTE: Отвязываем сегмент от других сегментов.
        for (auto linked_segment_id_ptr : Iter(&segment.linked_segments)) {
            auto linked_segment_id = *linked_segment_id_ptr;

            Graph_Segment& linked_segment
                = *Query_Graph_Segment(game_map, linked_segment_id);

            auto found_index = linked_segment.linked_segments.Index_Of(id);
            if (found_index != -1)
                linked_segment.linked_segments.Remove_At(found_index);
        }

        // TODO: _resource_transportation.OnSegmentDeleted(segment);

        // NOTE: Удаляем сегмент из очереди сегментов на добавление чувачков,
        // если этот сегмент ранее был в неё добавлен.
        // PERF: Много memmove происходит.
        auto& queue = game_map.segments_wo_humans;
        auto  index = queue.Index_Of(id);
        if (index != -1)
            queue.Remove_At(index);

        // NOTE: Уничтожаем сегмент.
        segments.Unstable_Remove(id);

        FREE(segment.vertices, sizeof(v2i16) * segment.vertices_count);
        FREE(segment.graph.nodes, sizeof(u8) * segment.graph.nodes_allocation_count);
    }

    // NOTE: Вносим созданные сегменты. Если будут свободные чувачки - назначим им.
    using Added_Calculated_Segment_Type = std::tuple<Graph_Segment_ID, Graph_Segment*>;
    Added_Calculated_Segment_Type* added_calculated_segments = nullptr;

    if (segments_to_add.count > 0) {
        added_calculated_segments = Allocate_Array(
            trash_arena, Added_Calculated_Segment_Type, segments_to_add.count
        );
    }

    FOR_RANGE (u32, i, segments_to_add.count) {
        added_calculated_segments[i] = Add_And_Link_Segment(
            game_map.last_entity_id,
            game_map.segments,
            segments_to_add.items[i],
            trash_arena,
            ctx
        );
    }

    // TODO: _resourceTransportation.PathfindItemsInQueue();
    // Tracing.Log("_itemTransportationSystem.PathfindItemsInQueue()");

    Assert(game_map.human_data != nullptr);

    // NOTE: По возможности назначаем чувачков на старые сегменты без них.
    while (humans_wo_segment_count > 0 && game_map.segments_wo_humans.count > 0) {
        auto  segment_id = game_map.segments_wo_humans.Dequeue();
        auto& segment    = *Query_Graph_Segment(game_map, segment_id);
        auto [old_segment_id, old_segment, human_p]
            = Array_Pop(humans_wo_segment, humans_wo_segment_count);

        auto [hid, human_ptr] = human_p;
        auto& human           = *human_ptr;

        human.segment_id          = segment_id;
        segment.assigned_human_id = hid;

        Root_On_Human_Current_Segment_Changed(
            hid, human, *game_map.human_data, old_segment_id, ctx
        );
    }

    // NOTE: По возможности назначаем чувачков на новые сегменты.
    // Если нет чувачков - сохраняем сегменты как те, которым нужны чувачки.
    FOR_RANGE (int, i, segments_to_add.count) {
        auto [id, segment_ptr] = added_calculated_segments[i];

        if (humans_wo_segment_count == 0) {
            *game_map.segments_wo_humans.Enqueue(ctx) = id;
            continue;
        }

        auto [old_segment_id, old_segment, human_p]
            = Array_Pop(humans_wo_segment, humans_wo_segment_count);
        auto [hid, human_ptr] = human_p;

        human_ptr->segment_id          = id;
        segment_ptr->assigned_human_id = hid;

        Root_On_Human_Current_Segment_Changed(
            hid, *human_ptr, *game_map.human_data, old_segment_id, ctx
        );
    }
}

typedef std::tuple<Direction, v2i16> Dir_v2i16;

#define QUEUES_SCALE 4

void Update_Graphs(
    const v2i16                  gsize,
    const Element_Tile* const    element_tiles,
    Graph_Segments_To_Add&       added_segments,
    Fixed_Size_Queue<Dir_v2i16>& big_queue,
    Fixed_Size_Queue<Dir_v2i16>& queue,
    Arena&                       trash_arena,
    u8* const                    visited,
    const bool                   full_graph_build,
    MCTX
) {
    CTX_ALLOCATOR;

    TEMP_USAGE(trash_arena);

    auto tiles_count = gsize.x * gsize.y;

    bool* vis = nullptr;
    if (full_graph_build)
        vis = Allocate_Zeros_Array(trash_arena, bool, tiles_count);

    while (big_queue.count) {
        TEMP_USAGE(trash_arena);

        auto p                  = big_queue.Dequeue();
        *queue.Enqueue_Unsafe() = p;

        auto [_, p_pos] = p;
        if (full_graph_build) {
            GRID_PTR_VALUE(vis, p_pos) = true;
        }

        v2i16* vertices      = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);
        v2i16* segment_tiles = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);

        int vertices_count      = 0;
        int segment_tiles_count = 1;
        *(segment_tiles + 0)    = p_pos;

        Graph temp_graph{};
        temp_graph.nodes  = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
        temp_graph.size.x = gsize.x;
        temp_graph.size.y = gsize.y;

        while (queue.count) {
            auto [dir, pos] = queue.Dequeue();
            if (full_graph_build)
                GRID_PTR_VALUE(vis, pos) = true;

            auto& tile = GRID_PTR_VALUE(element_tiles, pos);

            bool is_flag     = tile.type == Element_Tile_Type::Flag;
            bool is_building = tile.type == Element_Tile_Type::Building;
            bool is_vertex   = is_building || is_flag;

            if (is_vertex)
                Add_Without_Duplication(tiles_count, vertices_count, vertices, pos);

            FOR_DIRECTION (dir_index) {
                if (is_vertex && dir_index != dir)
                    continue;

                u8& visited_value = GRID_PTR_VALUE(visited, pos);
                if (Graph_Node_Has(visited_value, dir_index))
                    continue;

                v2i16 new_pos = pos + As_Offset(dir_index);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                Direction opposite_dir_index = Opposite(dir_index);
                u8&       new_visited_value  = GRID_PTR_VALUE(visited, new_pos);
                if (Graph_Node_Has(new_visited_value, opposite_dir_index))
                    continue;

                auto& new_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (new_tile.type == Element_Tile_Type::None)
                    continue;

                bool new_is_building = new_tile.type == Element_Tile_Type::Building;
                bool new_is_flag     = new_tile.type == Element_Tile_Type::Flag;
                bool new_is_vertex   = new_is_building || new_is_flag;

                if (is_vertex && new_is_vertex) {
                    if (tile.building_id != new_tile.building_id)
                        continue;

                    if (!Graph_Node_Has(new_visited_value, opposite_dir_index)) {
                        new_visited_value = Graph_Node_Mark(
                            new_visited_value, opposite_dir_index, true
                        );
                        FOR_DIRECTION (new_dir_index) {
                            if (!Graph_Node_Has(new_visited_value, new_dir_index))
                                *big_queue.Enqueue_Unsafe() = {new_dir_index, new_pos};
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
                    FOR_DIRECTION (new_dir_index) {
                        if (!Graph_Node_Has(new_visited_value, new_dir_index))
                            *big_queue.Enqueue_Unsafe() = {new_dir_index, new_pos};
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
                    *queue.Enqueue_Unsafe() = {(Direction)0, new_pos};
                }
            }

            SANITIZE;
        }

        // NOTE: Поиск островов графа.
        if (full_graph_build && !big_queue.count) {
            FOR_RANGE (int, y, gsize.y) {
                FOR_RANGE (int, x, gsize.x) {
                    auto  pos  = v2i16(x, y);
                    auto& tile = GRID_PTR_VALUE(element_tiles, pos);
                    u8&   v1   = GRID_PTR_VALUE(visited, pos);
                    bool& v2   = GRID_PTR_VALUE(vis, pos);

                    bool is_building = tile.type == Element_Tile_Type::Building;
                    bool is_flag     = tile.type == Element_Tile_Type::Flag;
                    bool is_vertex   = is_building || is_flag;

                    if (is_vertex && !v1 && !v2) {
                        FOR_DIRECTION (dir_index) {
                            *big_queue.Enqueue_Unsafe() = {dir_index, pos};
                        }
                    }
                }
            }
        }

        if (vertices_count <= 1)
            continue;

        // NOTE: Adding a new segment.
        Assert(temp_graph.nodes_count > 0);

        auto& segment          = added_segments.items[added_segments.count];
        segment.vertices_count = vertices_count;
        added_segments.count += 1;

        segment.vertices = (v2i16*)ALLOC(sizeof(v2i16) * vertices_count);
        memcpy(segment.vertices, vertices, sizeof(v2i16) * vertices_count);

        segment.graph.nodes_count = temp_graph.nodes_count;

        // NOTE: Вычисление size и offset графа.
        auto& gr_size = segment.graph.size;
        auto& offset  = segment.graph.offset;
        offset.x      = gsize.x - 1;
        offset.y      = gsize.y - 1;

        FOR_RANGE (int, y, gsize.y) {
            FOR_RANGE (int, x, gsize.x) {
                auto& node = *(temp_graph.nodes + y * gsize.x + x);
                if (node) {
                    gr_size.x = MAX(gr_size.x, x);
                    gr_size.y = MAX(gr_size.y, y);
                    offset.x  = MIN(offset.x, x);
                    offset.y  = MIN(offset.y, y);
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

        // NOTE: Копирование нод из временного графа
        // с небольшой оптимизацией по требуемой памяти.
        auto nodes_allocation_count          = gr_size.x * gr_size.y;
        segment.graph.nodes_allocation_count = nodes_allocation_count;
        segment.graph.nodes
            = (u8*)ALLOC(sizeof(*segment.graph.nodes) * nodes_allocation_count);

        auto rows          = gr_size.y;
        auto stride        = gsize.x;
        auto starting_node = temp_graph.nodes + offset.y * gsize.x + offset.x;
        Rect_Copy(segment.graph.nodes, starting_node, stride, rows, gr_size.x);

        SANITIZE;
    }
}

void Build_Graph_Segments(
    Entity_ID&                                     last_entity_id,
    v2i16                                          gsize,
    Element_Tile*                                  element_tiles,
    Sparse_Array<Graph_Segment_ID, Graph_Segment>& segments,
    Arena&                                         trash_arena,
    std::invocable<Graph_Segments_To_Add&, Graph_Segments_To_Delete&, Context*> auto&&
        Update_Segments_Lambda,
    MCTX
) {
    CTX_ALLOCATOR;

    Assert(segments.count == 0);
    TEMP_USAGE(trash_arena);

    auto tiles_count = gsize.x * gsize.y;

    // NOTE: Создание новых сегментов.
    auto                  segments_to_add_allocate = tiles_count * 4;
    Graph_Segments_To_Add segments_to_add{};
    segments_to_add.max_count = segments_to_add_allocate;
    segments_to_add.items
        = Allocate_Zeros_Array(trash_arena, Graph_Segment, segments_to_add_allocate);

    v2i16 pos   = -v2i16_one;
    bool  found = false;
    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile = *(element_tiles + y * gsize.x + x);

            if (tile.type == Element_Tile_Type::Flag
                || tile.type == Element_Tile_Type::Building)
            {
                pos   = {x, y};
                found = true;
                break;
            }
        }

        if (found)
            break;
    }

    SANITIZE;

    if (!found)
        return;

    Fixed_Size_Queue<Dir_v2i16> big_queue{};
    big_queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    big_queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, big_queue.memory_size);

    FOR_DIRECTION (dir) {
        *big_queue.Enqueue_Unsafe() = {dir, pos};
    }

    Fixed_Size_Queue<Dir_v2i16> queue{};
    queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    queue.base        = (Dir_v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);

    u8* visited = Allocate_Zeros_Array(trash_arena, u8, tiles_count);

    bool full_graph_build = true;

    Update_Graphs(
        gsize,
        element_tiles,
        segments_to_add,
        big_queue,
        queue,
        trash_arena,
        visited,
        full_graph_build,
        ctx
    );

    Graph_Segments_To_Delete no_segments_to_delete{};
    Update_Segments_Lambda(segments_to_add, no_segments_to_delete, ctx);

    FOR_RANGE (u32, i, segments_to_add.count) {
        Add_And_Link_Segment(
            last_entity_id, segments, segments_to_add.items[i], trash_arena, ctx
        );
    }

    SANITIZE;
}

std::tuple<int, int> Update_Tiles(
    v2i16                                          gsize,
    Element_Tile*                                  element_tiles,
    Sparse_Array<Graph_Segment_ID, Graph_Segment>* segments,
    Arena&                                         trash_arena,
    const Updated_Tiles&                           updated_tiles,
    std::invocable<Graph_Segments_To_Add&, Graph_Segments_To_Delete&, Context*> auto&&
        Update_Segments_Lambda,
    MCTX
) {
    CTX_ALLOCATOR;

    Assert(updated_tiles.count > 0);
    if (!updated_tiles.count)
        return {0, 0};

    TEMP_USAGE(trash_arena);

    auto tiles_count = gsize.x * gsize.y;

    // NOTE: Ищем сегменты для удаления.
    auto segments_to_delete_allocate = updated_tiles.count * 4;
    u32  segments_to_delete_count    = 0;

    Graph_Segments_To_Delete segments_to_delete{};
    segments_to_delete.max_count = segments_to_delete_allocate;
    segments_to_delete.items     = Allocate_Zeros_Array(
        trash_arena, Segment_To_Delete, segments_to_delete_allocate
    );

    for (auto [id, segment_ptr] : Iter(segments)) {
        if (Should_Segment_Be_Deleted(gsize, element_tiles, updated_tiles, *segment_ptr))
            *segments_to_delete.Add_Unsafe() = Segment_To_Delete(id, segment_ptr);
    };

    // NOTE: Создание новых сегментов.
    auto                  segments_to_add_allocate = updated_tiles.count * 4;
    Graph_Segments_To_Add segments_to_add{};
    segments_to_add.max_count = segments_to_add_allocate;
    segments_to_add.items
        = Allocate_Zeros_Array(trash_arena, Graph_Segment, segments_to_add_allocate);

    Fixed_Size_Queue<Dir_v2i16> big_queue{};
    big_queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    big_queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, big_queue.memory_size);

    FOR_RANGE (auto, i, updated_tiles.count) {
        const auto& updated_type = *(updated_tiles.type + i);
        const auto& pos          = *(updated_tiles.pos + i);

        switch (updated_type) {
        case Tile_Updated_Type::Road_Placed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                *big_queue.Enqueue_Unsafe() = {dir, pos};
            }
        } break;

        case Tile_Updated_Type::Flag_Placed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::Road)
                    *big_queue.Enqueue_Unsafe() = {dir, pos};
            }
        } break;

        case Tile_Updated_Type::Flag_Removed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type != Element_Tile_Type::None)
                    *big_queue.Enqueue_Unsafe() = {dir, pos};
            }
        } break;

        case Tile_Updated_Type::Road_Removed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                FOR_DIRECTION (dir) {
                    *big_queue.Enqueue_Unsafe() = {dir, new_pos};
                }
            }
        } break;

        case Tile_Updated_Type::Building_Placed: {
            FOR_DIRECTION (dir) {
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

                FOR_DIRECTION (dir) {
                    *big_queue.Enqueue_Unsafe() = {dir, new_pos};
                }
            }
        } break;

        case Tile_Updated_Type::Building_Removed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::Road) {
                    FOR_DIRECTION (new_dir) {
                        *big_queue.Enqueue_Unsafe() = {new_dir, new_pos};
                    }
                }
            }
        } break;

        default:
            INVALID_PATH;
        }
    }

    // NOTE: Each byte here contains differently bit-shifted values of `Direction`.
    u8* visited = Allocate_Zeros_Array(trash_arena, u8, tiles_count);

    Fixed_Size_Queue<Dir_v2i16> queue{};
    queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    queue.base        = (Dir_v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);

    bool full_graph_build = false;
    Update_Graphs(
        gsize,
        element_tiles,
        segments_to_add,
        big_queue,
        queue,
        trash_arena,
        visited,
        full_graph_build,
        ctx
    );

    Update_Segments_Lambda(segments_to_add, segments_to_delete, ctx);

    SANITIZE;

#ifdef ASSERT_SLOW
    for (auto [segment1_id, segment1_ptr] : Iter(segments)) {
        auto& segment1  = *segment1_ptr;
        auto& g1        = segment1.graph;
        v2i16 g1_offset = {g1.offset.x, g1.offset.y};

        for (auto [segment2_id, segment2_ptr] : Iter(segments)) {
            if (segment1_id == segment2_id)
                continue;

            auto& segment2 = *segment2_ptr;

            auto& g2        = segment2.graph;
            v2i16 g2_offset = {g2.offset.x, g2.offset.y};

            for (auto y = 0; y < g1.size.y; y++) {
                for (auto x = 0; x < g1.size.x; x++) {
                    v2i16 g1p_world = v2i16(x, y) + g1_offset;
                    v2i16 g2p_local = g1p_world - g2_offset;
                    if (!Pos_Is_In_Bounds(g2p_local, g2.size))
                        continue;

                    u8   node1 = *(g1.nodes + y * g1.size.x + x);
                    u8   node2 = *(g2.nodes + g2p_local.y * g2.size.x + g2p_local.x);
                    bool no_intersections = (node1 & node2) == 0;
                    Assert(no_intersections);
                }
            }
        }
    }
#endif  // ASSERT_SLOW

    return {segments_to_add.count, segments_to_delete_count};
}

#define Declare_Updated_Tiles(variable_name_, pos_, type_) \
    Updated_Tiles(variable_name_){};                       \
    (variable_name_).count = 1;                            \
    (variable_name_).pos   = &(pos_);                      \
    auto type__            = (type_);                      \
    (variable_name_).type  = &type__;

#define INVOKE_UPDATE_TILES                                   \
    STATEMENT({                                               \
        Update_Tiles(                                         \
            state.game_map.size,                              \
            state.game_map.element_tiles,                     \
            &state.game_map.segments,                         \
            trash_arena,                                      \
            updated_tiles,                                    \
            [&game_map, &trash_arena, &state](                \
                Graph_Segments_To_Add&    segments_to_add,    \
                Graph_Segments_To_Delete& segments_to_delete, \
                MCTX                                          \
            ) {                                               \
                Update_Segments(                              \
                    trash_arena,                              \
                    state,                                    \
                    game_map,                                 \
                    segments_to_add,                          \
                    segments_to_delete,                       \
                    ctx                                       \
                );                                            \
            },                                                \
            ctx                                               \
        );                                                    \
    })

bool Try_Build(Game_State& state, v2i16 pos, const Item_To_Build& item, MCTX) {
    auto& arena                = state.arena;
    auto& non_persistent_arena = state.non_persistent_arena;
    auto& trash_arena          = state.trash_arena;

    auto& game_map = state.game_map;
    auto  gsize    = game_map.size;
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

        Assert(tile.building_id == Building_ID_Missing);
    } break;

    case Item_To_Build_Type::Road: {
        Assert(item.scriptable_building == nullptr);

        if (tile.type != Element_Tile_Type::None)
            return false;

        Assert(tile.building_id == Building_ID_Missing);
        tile.type = Element_Tile_Type::Road;

        Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Road_Placed);
        INVOKE_UPDATE_TILES;
    } break;

    case Item_To_Build_Type::Building: {
        Assert(item.scriptable_building != nullptr);

        if (tile.type != Element_Tile_Type::None)
            return false;

        bool built = false;
        Place_Building(state, pos, item.scriptable_building, built, ctx);

        Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Building_Placed);
        INVOKE_UPDATE_TILES;
    } break;

    default:
        INVALID_PATH;
    }

    INVOKE_OBSERVER(state.On_Item_Built, (state, pos, item, ctx));

    return true;
}
