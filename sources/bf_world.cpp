#define WORLD_PTR_OFFSET(arr_p, pos) (*((arr_p) + gsize.x * (pos).y + (pos).x))

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
        while (WORLD_PTR_OFFSET(bfs_parents_mtx, dest).has_value()) {
            auto value = WORLD_PTR_OFFSET(bfs_parents_mtx, dest).value();
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
    while (WORLD_PTR_OFFSET(bfs_parents_mtx, destination).has_value()) {
        auto value = WORLD_PTR_OFFSET(bfs_parents_mtx, destination).value();
        Array_Push(path, path_count, path_max_count, value);
        destination = value;
    }

    Array_Reverse(path, path_count);

    return {path, path_count};
}

// NOTE: Обязательно нужно копировать path, если результат успешен.
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
    queue.memory_size = sizeof(v2i16) * tiles_count;
    queue.base        = (v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);
    *queue.Enqueue()  = source;

    bool* visited_mtx = Allocate_Zeros_Array(trash_arena, bool, tiles_count);
    WORLD_PTR_OFFSET(visited_mtx, source) = true;

    auto bfs_parents_mtx
        = Allocate_Zeros_Array(trash_arena, std::optional<v2i16>, tiles_count);

    while (queue.count > 0) {
        auto pos = queue.Dequeue();
        FOR_DIRECTION (dir) {
            auto offset  = As_Offset(dir);
            auto new_pos = pos + offset;
            if (!Pos_Is_In_Bounds(new_pos, gsize))
                continue;

            auto& visited = WORLD_PTR_OFFSET(visited_mtx, new_pos);
            if (visited)
                continue;

            if (avoid_harvestable_resources) {
                auto& terrain_tile = WORLD_PTR_OFFSET(terrain_tiles, new_pos);
                if (terrain_tile.resource_amount > 0)
                    continue;
            }

            auto& element_tile = WORLD_PTR_OFFSET(element_tiles, new_pos);
            visited            = true;

            WORLD_PTR_OFFSET(bfs_parents_mtx, new_pos) = pos;

            if (new_pos == destination) {
                result.success = true;
                auto [path, path_count]
                    = Build_Path(trash_arena, gsize, bfs_parents_mtx, new_pos);
                result.path       = path;
                result.path_count = path_count;
                return result;
            }

            *queue.Enqueue() = new_pos;
        }
    }

    result.success = false;
    return result;
}

// NOTE: Обязательно нужно копировать path, если результат успешен.
Path_Find_Result Find_Path_Inside_Graph(
    Arena& trash_arena,
    Graph& graph,
    v2i16  source,      // NOTE: in the world
    v2i16  destination  // NOTE: in the world
) {
    Assert(graph.data != nullptr);
    Assert(source.x - graph.offset.x >= 0);
    Assert(source.y - graph.offset.y >= 0);
    Assert(source.x - graph.offset.x < graph.size.x);
    Assert(source.y - graph.offset.y < graph.size.y);

    if (source == destination)
        return {true, nullptr, 0};

    TEMP_USAGE(trash_arena);

    auto path = Allocate_Array(trash_arena, v2i16, graph.non_zero_nodes_count);

    *(path + 0)     = destination;
    auto path_count = 1;

    auto& data = *graph.data;

    auto source_node_index      = data.pos_2_node_index[source - graph.offset];
    auto destination_node_index = data.pos_2_node_index[destination - graph.offset];

    auto current_iteration = 0;

    while ((current_iteration++ < graph.non_zero_nodes_count)
           && (source_node_index != destination_node_index))
    {
        auto i
            = *(data.prev + graph.non_zero_nodes_count * source_node_index
                + destination_node_index);
        Assert(i >= 0);
        Assert(i < graph.non_zero_nodes_count);

        destination_node_index = i;

        *(path + path_count)
            = data.node_index_2_pos[destination_node_index] + graph.offset;
        path_count++;
    }

    Assert(current_iteration <= graph.non_zero_nodes_count);

    Array_Reverse(path, path_count);

    // Валидация допустимых значений клеток пути.
    FOR_RANGE (int, i, path_count) {
        auto& a = path[i];
        Assert(a.x >= graph.offset.x);
        Assert(a.y >= graph.offset.y);
        Assert(a.x < graph.size.x + graph.offset.x);
        Assert(a.y < graph.size.y + graph.offset.y);
    }

    // Валидация, что все клетки в пути - это последовательность соседствующих клеток.
    FOR_RANGE (int, i, path_count - 1) {
        auto& a = path[i];
        auto& b = path[i + 1];
        auto  d = b - a;

        auto only_one_tile_difference =  //
            (d == v2i16_right)           //
            || (d == v2i16_up)           //
            || (d == v2i16_left)         //
            || (d == v2i16_down);

        Assert(only_one_tile_difference);
    }

    return {true, path, path_count};
}

Terrain_Tile& Get_Terrain_Tile(World& world, v2i16 pos) {
    Assert(Pos_Is_In_Bounds(pos, world.size));
    return *(world.terrain_tiles + pos.y * world.size.x + pos.x);
}

Terrain_Resource& Get_Terrain_Resource(World& world, v2i16 pos) {
    Assert(Pos_Is_In_Bounds(pos, world.size));
    return *(world.terrain_resources + pos.y * world.size.x + pos.x);
}

template <typename T>
void Set_Container_Allocator_Context(T& container, MCTX) {
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
void Deinit_Sparse_Array_Of_Ids(Sparse_Array_Of_Ids<T>& container, MCTX) {
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

World_Resource_ID Next_World_Resource_ID(Entity_ID& last_entity_id) {
    auto ent = ++last_entity_id;
    Assert_No_Collision(ent, World_Resource::component_mask);
    return ent | World_Resource::component_mask;
}

World_Resource_Booking_ID Next_World_Resource_Booking_ID(Entity_ID& last_entity_id) {
    auto ent = ++last_entity_id;
    Assert_No_Collision(ent, World_Resource_Booking::component_mask);
    return ent | World_Resource_Booking::component_mask;
}

void Place_Building(
    Game&                game,
    v2i16                pos,
    Scriptable_Building* scriptable,
    bool                 built,
    MCTX
) {
    auto& world = game.world;
    auto  gsize = world.size;
    Assert(Pos_Is_In_Bounds(pos, gsize));

    auto     id = Next_Building_ID(world.last_entity_id);
    Building b{};
    b.pos        = pos;
    b.scriptable = scriptable;
    if (!built)
        b.remaining_construction_points = scriptable->construction_points;

    {
        auto [pid, pvalue] = world.buildings.Add(ctx);
        *pid               = id;
        *pvalue            = b;
    }

    if (built) {
        if (scriptable->type == Building_Type::City_Hall) {
            City_Hall c{};
            c.time_since_human_was_created = f32_inf;
            {
                auto [pid, pvalue] = world.city_halls.Add(ctx);
                *pid               = id;
                *pvalue            = c;
            }
        }
    }
    else {
        for (auto pair_p : Iter(&scriptable->construction_resources)) {
            auto& [resource, count] = *pair_p;

            World_Resource_To_Book to_book{
                .scriptable  = resource,
                .count       = (u8)count,
                .building_id = id,
            };

            *world.resources_to_book.Vector_Occupy_Slot(ctx) = to_book;
        }

        *world.not_constructed_building_ids.Add(ctx) = id;
    }

    auto& tile = *(world.element_tiles + gsize.x * pos.y + pos.x);
    Assert(tile.type == Element_Tile_Type::None);
    tile.type        = Element_Tile_Type::Building;
    tile.building_id = id;
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
    Game*  game;
    World* world;
    Arena* trash_arena;
};

void Root_Set_Human_State(
    Human_ID          human_id,
    Human&            human,
    Human_States      new_state,
    const Human_Data& data,
    MCTX
);

void Advance_Moving_To(Human_Moving_Component& moving) {
    if (moving.path.count == 0) {
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

    if (moving.elapsed == 0)
        moving.to.reset();

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

Building* Strict_Query_Building(World& world, Building_ID id) {
    Assert(id != Building_ID_Missing);

    for (auto [instance_id, instance] : Iter(&world.buildings)) {
        if (instance_id == id)
            return instance;
    }

    INVALID_PATH;
    return nullptr;
}

Human* Strict_Query_Human(World& world, Human_ID human_id) {
    Assert(human_id != Human_ID_Missing);

    for (auto [instance_id, instance] : Iter(&world.humans)) {
        if (instance_id == human_id)
            return instance;
    }

    INVALID_PATH;
    return nullptr;
}

Graph_Segment* Query_Graph_Segment(World& world, Graph_Segment_ID graph_segment_id) {
    auto result = world.segments.Find(graph_segment_id);
    return result;
}

Graph_Segment*
Strict_Query_Graph_Segment(World& world, Graph_Segment_ID graph_segment_id) {
    auto result = Query_Graph_Segment(world, graph_segment_id);
    Assert(result != nullptr);
    return result;
}

World_Resource* Query_World_Resource(World& world, World_Resource_ID world_resource_id) {
    auto result = world.resources.Find(world_resource_id);
    return result;
}

World_Resource*
Strict_Query_World_Resource(World& world, World_Resource_ID world_resource_id) {
    auto result = Query_World_Resource(world, world_resource_id);
    Assert(result != nullptr);
    return result;
}

World_Resource_Booking* Query_World_Resource_Booking(
    World&                    world,
    World_Resource_Booking_ID world_resource_booking_id
) {
    auto result = world.resource_bookings.Find(world_resource_booking_id);
    return result;
}

World_Resource_Booking* String_Query_World_Resource_Booking(
    World&                    world,
    World_Resource_Booking_ID world_resource_booking_id
) {
    auto result = Query_World_Resource_Booking(world, world_resource_booking_id);
    Assert(result != nullptr);
    return result;
}

//----------------------------------------------------------------------------------
// MovingInTheWorld State Functions.
//----------------------------------------------------------------------------------
HumanState_OnEnter_function(HumanState_MovingInTheWorld_OnEnter);
HumanState_OnExit_function(HumanState_MovingInTheWorld_OnExit);
HumanState_Update_function(HumanState_MovingInTheWorld_Update);
HumanState_OnCurrentSegmentChanged_function(
    HumanState_MovingInTheWorld_OnCurrentSegmentChanged
);
HumanState_OnMovedToTheNextTile_function(HumanState_MovingInTheWorld_OnMovedToTheNextTile
);
HumanState_UpdateStates_function(HumanState_MovingInTheWorld_UpdateStates);

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_MovingInTheWorld_OnEnter) {
    CTX_LOGGER;
    LOG_SCOPE;

    if (human.segment_id != Graph_Segment_ID_Missing) {
        // TODO: After implementing resources.
        // LOG_DEBUG(
        //     "human.segment.resource_ids_to_transport.size() = %d",
        //     human.segment.resource_ids_to_transport.size()
        // );
    }

    human.moving.path.Reset();

    HumanState_MovingInTheWorld_UpdateStates(
        state, human_id, human, data, Building_ID_Missing, nullptr, ctx
    );
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_MovingInTheWorld_OnExit) {
    CTX_LOGGER;
    LOG_SCOPE;

    human.state_moving_in_the_world = Moving_In_The_World_State::None;
    human.moving.path.Reset();

    if (human.type == Human_Type::Employee) {
        Assert(human.building_id != Building_ID_Missing);
        auto& building = *Get_Building(*data.world, human.building_id);
        // TODO: building.employee_is_inside = true;
        // C# TODO: Somehow remove this human.
    }
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_MovingInTheWorld_Update) {
    HumanState_MovingInTheWorld_UpdateStates(
        state,
        human_id,
        human,
        data,
        human.building_id,
        Get_Building(*data.world, human.building_id),
        ctx
    );
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(
    HumanState_MovingInTheWorld_OnCurrentSegmentChanged
) {
    CTX_LOGGER;
    LOG_SCOPE;

    Assert(human.type == Human_Type::Transporter);
    HumanState_MovingInTheWorld_UpdateStates(
        state, human_id, human, data, Building_ID_Missing, nullptr, ctx
    );
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(HumanState_MovingInTheWorld_OnMovedToTheNextTile
) {
    CTX_LOGGER;
    LOG_SCOPE;
    LOG_DEBUG("human moved to %d.%d", human.moving.pos.x, human.moving.pos.y);

    if (human.type == Human_Type::Constructor                                     //
        && human.building_id != Building_ID_Missing                               //
        && human.moving.pos == Get_Building(*data.world, human.building_id)->pos  //
    )
    {
        // TODO:
        // Root_Set_Human_State(human, Human_States::Building, data, ctx);
    }

    if (human.type == Human_Type::Employee) {
        Assert(human.building_id != Building_ID_Missing);
        auto& building = *Get_Building(*data.world, human.building_id);

        if (human.moving.pos == building.pos) {
            Assert_False(human.moving.to.has_value());

            // TODO: data.world->Employee_Reached_Building_Callback(human);
            // building.employee_is_inside = true;
        }
    }
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_MovingInTheWorld_UpdateStates) {
    ZoneScoped;

    CTX_LOGGER;
    LOG_SCOPE;

    auto& world          = *data.world;
    bool  is_transporter = human.type == Human_Type::Transporter;
    bool  is_constructor_or_employee
        = (human.type == Human_Type::Constructor) || (human.type == Human_Type::Employee);

    if (human.segment_id != Graph_Segment_ID_Missing) {
        Assert(is_transporter);
        human.building_id = Building_ID_Missing;

        auto& segment = *Strict_Query_Graph_Segment(world, human.segment_id);

        auto moving_from = human.moving.to.value_or(human.moving.pos);
        if (human.moving.elapsed == 0)
            moving_from = human.moving.pos;

        // NOTE: Чувачок уже в сегменте и идёт до его центра.
        if (Graph_Contains(segment.graph, human.moving.pos)
            && (Graph_Node(segment.graph, human.moving.pos) != 0))
        {
            human.moving.path.Reset();
            Root_Set_Human_State(
                human_id, human, Human_States::MovingInsideSegment, data, ctx
            );
            return;
        }

        auto moving_to_destination = Moving_In_The_World_State::Moving_To_Destination;
        if (human.state_moving_in_the_world != moving_to_destination) {
            LOG_DEBUG(
                "Setting human.state_moving_in_the_world = "
                "Moving_In_The_World_State::Moving_To_Destination"
            );
            human.state_moving_in_the_world
                = Moving_In_The_World_State::Moving_To_Destination;

            Assert(data.trash_arena != nullptr);

            if (human.moving.elapsed == 0)
                human.moving.to.reset();
            auto moving_from = human.moving.to.value_or(human.moving.pos);

            auto segment_center = Assert_Deref(segment.graph.data).center;
            if (segment_center != moving_from) {
                LOG_DEBUG("Calculating path to the segment");
                auto [success, path, path_count] = Find_Path(
                    *data.trash_arena,
                    world.size,
                    world.terrain_tiles,
                    world.element_tiles,
                    moving_from,
                    segment_center,
                    true
                );

                Assert(success);
                Assert(path_count > 0);

                Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
            }
        }
    }
    else if (is_constructor_or_employee && (human.building_id != Building_ID_Missing)) {
        auto& building = *Get_Building(*data.world, human.building_id);

        // TODO:
        // if (human.type == Human_Type::Constructor) {
        //     // TODO: Assert_False(building.is_constructed);
        // }
        // else if (human.type == Human_Type::Employee) {
        //     // TODO: Assert(building.is_constructed);
        // }

        if (old_building_id != human.building_id) {
            Assert(data.trash_arena != nullptr);

            TEMP_USAGE(*data.trash_arena);

            if (human.moving.elapsed == 0)
                human.moving.to.reset();
            auto moving_from = human.moving.to.value_or(human.moving.pos);

            auto [success, path, path_count] = Find_Path(
                *data.trash_arena,
                world.size,
                world.terrain_tiles,
                world.element_tiles,
                moving_from,
                building.pos,
                true
            );

            Assert(success);
            Assert(path_count > 0);

            Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
        }
    }
    else if ( //
        human.state_moving_in_the_world
        != Moving_In_The_World_State::Moving_To_The_City_Hall)
    {
        LOG_DEBUG(
            "human.state_moving_in_the_world = "
            "Moving_In_The_World_State::Moving_To_The_City_Hall"
        );
        human.state_moving_in_the_world
            = Moving_In_The_World_State::Moving_To_The_City_Hall;

        Assert(world.city_halls.count > 0);
        auto      city_hall_id = world.city_halls.ids[0];
        Building& city_hall    = *Strict_Query_Building(world, city_hall_id);

        human.building_id = city_hall_id;

        TEMP_USAGE(*data.trash_arena);

        if (human.moving.elapsed == 0)
            human.moving.to.reset();

        human.moving.path.Reset();

        if (human.moving.to.value_or(human.moving.pos) != city_hall.pos) {
            LOG_DEBUG("Calculating path to the city hall");
            auto [success, path, path_count] = Find_Path(
                *data.trash_arena,
                world.size,
                world.terrain_tiles,
                world.element_tiles,
                human.moving.to.value_or(human.moving.pos),
                city_hall.pos,
                true
            );

            Assert(success);
            Assert(path_count > 0);

            Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
        }
    }
}

//----------------------------------------------------------------------------------
// MovingInsideSegment State Functions.
//----------------------------------------------------------------------------------
HumanState_OnEnter_function(HumanState_MovingInsideSegment_OnEnter);
HumanState_OnExit_function(HumanState_MovingInsideSegment_OnExit);
HumanState_Update_function(HumanState_MovingInsideSegment_Update);
HumanState_OnCurrentSegmentChanged_function(
    HumanState_MovingInsideSegment_OnCurrentSegmentChanged
);
HumanState_OnMovedToTheNextTile_function(
    HumanState_MovingInsideSegment_OnMovedToTheNextTile
);
HumanState_UpdateStates_function(HumanState_MovingInsideSegment_UpdateStates);

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_MovingInsideSegment_OnEnter) {
    CTX_LOGGER;
    LOG_SCOPE;

    Assert(human.segment_id != Graph_Segment_ID_Missing);

    auto& world = *data.world;

    auto& segment = *Query_Graph_Segment(world, human.segment_id);

    if (segment.resource_ids_to_transport.count == 0) {
        TEMP_USAGE(*data.trash_arena);
        LOG_DEBUG("Calculating path to the center of the segment");

        if (human.moving.elapsed == 0)
            human.moving.to.reset();
        auto moving_from = human.moving.to.value_or(human.moving.pos);

        auto [success, path, path_count] = Find_Path(
            *data.trash_arena,
            world.size,
            world.terrain_tiles,
            world.element_tiles,
            moving_from,
            Assert_Deref(segment.graph.data).center,
            true
        );

        Assert(success);

        Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
    }
    else {
        // TODO: moving.path.Reset(), moving.to.reset() и идём до вертекса с ресурсом
    }
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_MovingInsideSegment_OnExit) {
    CTX_LOGGER;
    LOG_SCOPE;

    human.moving.path.Reset();
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_MovingInsideSegment_Update) {
    HumanState_MovingInsideSegment_UpdateStates(
        state, human_id, human, data, Building_ID_Missing, nullptr, ctx
    );
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(
    HumanState_MovingInsideSegment_OnCurrentSegmentChanged
) {
    CTX_LOGGER;
    LOG_SCOPE;

    Root_Set_Human_State(human_id, human, Human_States::MovingInTheWorld, data, ctx);
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(
    HumanState_MovingInsideSegment_OnMovedToTheNextTile
) {
    CTX_LOGGER;
    LOG_SCOPE;

    // NOTE: Специально оставлено пустым.
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_MovingInsideSegment_UpdateStates) {
    ZoneScoped;

    CTX_LOGGER;
    LOG_SCOPE;

    if (human.segment_id == Graph_Segment_ID_Missing) {
        human.moving.path.Reset();
        Root_Set_Human_State(human_id, human, Human_States::MovingInTheWorld, data, ctx);
        return;
    }

    auto& segment = *Strict_Query_Graph_Segment(*data.world, human.segment_id);

    if (segment.resource_ids_to_transport.count > 0) {
        if (!human.moving.to.has_value()) {
            // TODO:
            // Tracing.Log("_controller.SetState(human, HumanState.MovingItem)");
            Root_Set_Human_State(
                human_id, human, Human_States::MovingResources, data, ctx
            );
            return;
        }

        human.moving.path.Reset();
    }
}

//----------------------------------------------------------------------------------
// MovingResources Substates.
//----------------------------------------------------------------------------------

void HumanState_MovingResources_SetSubstate(
    Human_State&              state,
    Human_ID                  human_id,
    Human&                    human,
    Moving_Resources_Substate new_state_value,
    const Human_Data&         data,
    MCTX
);

void HumanState_MovingResources_Exit(
    Human_State&      state,
    Human_ID          human_id,
    Human&            human,
    const Human_Data& data,
    MCTX
);

// MovingToResource Substate.
//----------------------------------------------------------------------------------

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_MovingToResource_OnEnter) {
    CTX_LOGGER;
    LOG_SCOPE;

    auto& segment = *Strict_Query_Graph_Segment(*data.world, human.segment_id);

    human.resource_id = *segment.resource_ids_to_transport.First();

    auto& res             = *Strict_Query_World_Resource(*data.world, human.resource_id);
    res.targeted_human_id = human_id;

    if (human.moving.elapsed == 0)
        human.moving.to.reset();

    if ((res.pos == human.moving.pos) && (human.moving.elapsed == 0)) {
        HumanState_MovingResources_SetSubstate(
            state,
            human_id,
            human,
            Moving_Resources_Substate::PickingUpResource,
            data,
            ctx
        );
        return;
    }

    auto graph_contains   = Graph_Contains(segment.graph, human.moving.pos);
    auto node_is_walkable = Graph_Node(segment.graph, human.moving.pos) != 0;

    auto  will_move_to = human.moving.to.value_or(human.moving.pos);
    auto& world        = *data.world;

    if ((res.pos != will_move_to) && graph_contains && node_is_walkable) {
        auto [success, path, path_count] = Find_Path_Inside_Graph(
            *data.trash_arena, segment.graph, will_move_to, res.pos
        );

        Assert(success);
        Assert(path_count > 0);
        Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
    }
    else if ((!graph_contains) || (!node_is_walkable))
        HumanState_MovingResources_Exit(state, human_id, human, data, ctx);
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_MovingToResource_OnExit) {
    CTX_LOGGER;
    LOG_SCOPE;

    human.moving.path.Reset();
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_MovingToResource_Update) {
    CTX_LOGGER;

    // NOTE: Специально оставлено пустым.
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(
    HumanState_MovingToResource_OnCurrentSegmentChanged
) {
    CTX_LOGGER;
    LOG_SCOPE;

    HumanState_MovingResources_Exit(state, human_id, human, data, ctx);
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(HumanState_MovingToResource_OnMovedToTheNextTile
) {
    CTX_LOGGER;
    LOG_SCOPE;

    if ((human.resource_id == World_Resource_ID_Missing) || (human.resource_id == 0)) {
        HumanState_MovingResources_Exit(state, human_id, human, data, ctx);
        return;
    }

    if (human.moving.elapsed == 0)
        human.moving.to.reset();

    auto& res = *Strict_Query_World_Resource(*data.world, human.resource_id);
    if (human.moving.pos == res.pos)
        HumanState_MovingResources_SetSubstate(
            state,
            human_id,
            human,
            Moving_Resources_Substate::PickingUpResource,
            data,
            ctx
        );
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_MovingToResource_UpdateStates) {
    CTX_LOGGER;
    // TODO:
}

// PickingUpResource Substate.
//----------------------------------------------------------------------------------

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_PickingUpResource_OnEnter) {
    CTX_LOGGER;
    LOG_SCOPE;

    Assert(!human.moving.to.has_value());
    Assert(human.moving.path.count == 0);
    Assert(human.action_progress == 0);

    human.moving.elapsed    = 0;
    human.action_started_at = data.game->time;

    On_Human_Started_Picking_Up_Resource(
        *data.game,
        human_id,
        human,
        human.resource_id,
        *Strict_Query_World_Resource(*data.world, human.resource_id),
        ctx
    );
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_PickingUpResource_OnExit) {
    CTX_LOGGER;
    LOG_SCOPE;

    human.action_started_at = -f64_inf;
    human.action_progress   = 0;
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_PickingUpResource_Update) {
    CTX_LOGGER;

    human.action_progress = (f32)(data.game->time - human.action_started_at)
                            / data.world->data.humans_moving_one_tile_duration;

    if (human.action_progress >= 1) {
        human.action_progress = 1;

        On_Human_Finished_Picking_Up_Resource(
            *data.game,
            human_id,
            human,
            human.resource_id,
            *Strict_Query_World_Resource(*data.world, human.resource_id),
            ctx
        );

        HumanState_MovingResources_SetSubstate(
            state, human_id, human, Moving_Resources_Substate::MovingResource, data, ctx
        );
    }
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(
    HumanState_PickingUpResource_OnCurrentSegmentChanged
) {
    CTX_LOGGER;
    LOG_SCOPE;

    // NOTE: Специально оставлено пустым.
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(HumanState_PickingUpResource_OnMovedToTheNextTile
) {
    CTX_LOGGER;
    LOG_SCOPE;

    // NOTE: Специально оставлено пустым.
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_PickingUpResource_UpdateStates) {
    CTX_LOGGER;
    // TODO:
}

// MovingResource Substate.
//----------------------------------------------------------------------------------

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_MovingResource_OnEnter) {
    CTX_LOGGER;
    LOG_SCOPE;

    auto& segment = *Strict_Query_Graph_Segment(*data.world, human.segment_id);

    Assert(!human.moving.to.has_value());

    auto& res = *Strict_Query_World_Resource(*data.world, human.resource_id);

    Assert(res.transportation_vertices.count > 0);
    auto to = *(res.transportation_vertices.base + 0);

    auto [success, path, path_count]
        = Find_Path_Inside_Graph(*data.trash_arena, segment.graph, human.moving.pos, to);

    Assert(success);
    Assert(path_count > 0);
    Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_MovingResource_OnExit) {
    CTX_LOGGER;
    LOG_SCOPE;
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_MovingResource_Update) {
    CTX_LOGGER;
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(
    HumanState_MovingResource_OnCurrentSegmentChanged
) {
    CTX_LOGGER;
    LOG_SCOPE;
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(HumanState_MovingResource_OnMovedToTheNextTile) {
    CTX_LOGGER;
    LOG_SCOPE;

    if (!human.moving.to.has_value())
        HumanState_MovingResources_SetSubstate(
            state, human_id, human, Moving_Resources_Substate::PlacingResource, data, ctx
        );
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_MovingResource_UpdateStates) {
    CTX_LOGGER;
    // TODO:
}

// PlacingResource Substate.
//----------------------------------------------------------------------------------

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_PlacingResource_OnEnter) {
    CTX_LOGGER;
    LOG_SCOPE;

    Assert(!human.moving.to.has_value());
    Assert(human.moving.path.count == 0);
    Assert(human.moving.progress == 0);
    Assert(human.action_progress == 0);

    human.moving.elapsed    = 0;
    human.action_started_at = data.game->time;
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_PlacingResource_OnExit) {
    CTX_LOGGER;
    LOG_SCOPE;

    human.action_started_at = -f64_inf;
    human.action_progress   = 0;
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_PlacingResource_Update) {
    CTX_LOGGER;

    human.action_progress = (f32)(data.game->time - human.action_started_at)
                            / data.world->data.humans_moving_one_tile_duration;

    if (human.action_progress >= 1) {
        human.action_progress = 1;

        auto& resource = *Strict_Query_World_Resource(*data.world, human.resource_id);
        resource.targeted_human_id = Human_ID_Missing;

        On_Human_Finished_Placing_Resource(
            *data.game, human_id, human, human.resource_id, resource, ctx
        );

        HumanState_MovingResources_Exit(state, human_id, human, data, ctx);
    }
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(
    HumanState_PlacingResource_OnCurrentSegmentChanged
) {
    CTX_LOGGER;
    LOG_SCOPE;
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(HumanState_PlacingResource_OnMovedToTheNextTile
) {
    CTX_LOGGER;
    LOG_SCOPE;
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_PlacingResource_UpdateStates) {
    CTX_LOGGER;
    // TODO:
}

//----------------------------------------------------------------------------------
// MovingResources State Functions.
//----------------------------------------------------------------------------------

void HumanState_MovingResources_SetSubstate(
    Human_State&              state,
    Human_ID                  human_id,
    Human&                    human,
    Moving_Resources_Substate new_state_value,
    const Human_Data&         data,
    MCTX
) {
    auto old_state = human.substate_moving_resources;
    if (old_state != Moving_Resources_Substate::None) {
        switch (human.substate_moving_resources) {
#define X(name)                                                        \
    case Moving_Resources_Substate::name: {                            \
        HumanState_##name##_OnExit(state, human_id, human, data, ctx); \
    } break;
            Human_MovingResources_Substates_Table;
#undef X

        default:
            INVALID_PATH;
        }
    }
    human.substate_moving_resources = new_state_value;

    switch (new_state_value) {
#define X(name)                                                         \
    case Moving_Resources_Substate::name: {                             \
        HumanState_##name##_OnEnter(state, human_id, human, data, ctx); \
    } break;
        Human_MovingResources_Substates_Table;
#undef X

    default:
        INVALID_PATH;
    }
}

void HumanState_MovingResources_Exit(
    Human_State&      state,
    Human_ID          human_id,
    Human&            human,
    const Human_Data& data,
    MCTX
) {
    switch (human.substate_moving_resources) {
#define X(name)                                                        \
    case Moving_Resources_Substate::name: {                            \
        HumanState_##name##_OnExit(state, human_id, human, data, ctx); \
    } break;
        Human_MovingResources_Substates_Table;
#undef X

    default:
        INVALID_PATH;
    }

    Root_Set_Human_State(human_id, human, Human_States::MovingInTheWorld, data, ctx);
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_MovingResources_OnEnter) {
    CTX_LOGGER;
    LOG_SCOPE;
    Assert(human.segment_id != 0);
    Assert(human.segment_id != Graph_Segment_ID_Missing);

    auto& segment = *Strict_Query_Graph_Segment(*data.world, human.segment_id);
    Assert(segment.resource_ids_to_transport.count > 0);

    HumanState_MovingResources_SetSubstate(
        state, human_id, human, Moving_Resources_Substate::MovingToResource, data, ctx
    );
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_MovingResources_OnExit) {
    CTX_LOGGER;
    LOG_SCOPE;
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_MovingResources_Update) {
    CTX_LOGGER;

    switch (human.substate_moving_resources) {
#define X(name)                                                            \
    case Moving_Resources_Substate::name: {                                \
        HumanState_##name##_Update(state, human_id, human, data, dt, ctx); \
    } break;
        Human_MovingResources_Substates_Table;
#undef X

    default:
        INVALID_PATH;
    }
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(
    HumanState_MovingResources_OnCurrentSegmentChanged
) {
    CTX_LOGGER;
    LOG_SCOPE;

    switch (human.substate_moving_resources) {
#define X(name)                                                                         \
    case Moving_Resources_Substate::name: {                                             \
        HumanState_##name##_OnCurrentSegmentChanged(state, human_id, human, data, ctx); \
    } break;
        Human_MovingResources_Substates_Table;
#undef X

    default:
        INVALID_PATH;
    }
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(HumanState_MovingResources_OnMovedToTheNextTile
) {
    CTX_LOGGER;
    LOG_SCOPE;

    switch (human.substate_moving_resources) {
#define X(name)                                                                      \
    case Moving_Resources_Substate::name: {                                          \
        HumanState_##name##_OnMovedToTheNextTile(state, human_id, human, data, ctx); \
    } break;
        Human_MovingResources_Substates_Table;
#undef X

    default:
        INVALID_PATH;
    }
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_MovingResources_UpdateStates) {
    CTX_LOGGER;
    // TODO:
}

//----------------------------------------------------------------------------------
// Construction State Functions.
//----------------------------------------------------------------------------------

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_Construction_OnEnter) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_Construction_OnExit) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_Construction_Update) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(
    HumanState_Construction_OnCurrentSegmentChanged
) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(HumanState_Construction_OnMovedToTheNextTile) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_Construction_UpdateStates) {
    // TODO:
}

//----------------------------------------------------------------------------------
// Employee State Functions.
//----------------------------------------------------------------------------------

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnEnter_function(HumanState_Employee_OnEnter) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnExit_function(HumanState_Employee_OnExit) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// f32               dt
// MCTX
HumanState_Update_function(HumanState_Employee_Update) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnCurrentSegmentChanged_function(HumanState_Employee_OnCurrentSegmentChanged) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// MCTX
HumanState_OnMovedToTheNextTile_function(HumanState_Employee_OnMovedToTheNextTile) {
    // TODO:
}

// Human_State&      state
// Human_ID          human_id
// Human&            human
// const Human_Data& data
// Building_ID       old_building_id
// Building*         old_building
// MCTX
HumanState_UpdateStates_function(HumanState_Employee_UpdateStates) {
    // TODO:
}

//----------------------------------------------------------------------------------
// Root State Functions.
//----------------------------------------------------------------------------------
void Human_Root_Update(
    Human_ID          human_id,
    Human&            human,
    const Human_Data& data,
    f32               dt,
    MCTX
) {
    auto index = human.state;
    Assert((int)index >= 0);
    Assert((int)index < (int)Human_States::COUNT);

    auto state = human_states[(int)index];
    state.Update(state, human_id, human, data, dt, ctx);
}

void Root_OnCurrentSegmentChanged(
    Human_ID          human_id,
    Human&            human,
    const Human_Data& data,
    MCTX
) {
    auto index = (int)human.state;
    Assert(index >= 0);
    Assert(index < (int)Human_States::COUNT);

    auto state = human_states[index];
    state.OnCurrentSegmentChanged(state, human_id, human, data, ctx);
}

void Root_OnMovedToTheNextTile(
    Human_ID          human_id,
    Human&            human,
    const Human_Data& data,
    MCTX
) {
    auto index = (int)human.state;
    Assert(index >= 0);
    Assert(index < (int)Human_States::COUNT);

    auto state = human_states[index];
    state.OnMovedToTheNextTile(state, human_id, human, data, ctx);
}

void Root_Set_Human_State(
    Human_ID          human_id,
    Human&            human,
    Human_States      new_state_value,
    const Human_Data& data,
    MCTX
) {
    CTX_LOGGER;
    LOG_SCOPE;
    auto old_state_value = human.state;
    human.state          = new_state_value;

    auto new_state_index = (int)human.state;
    Assert(new_state_index >= 0);
    Assert(new_state_index < (int)Human_States::COUNT);

    auto old_state_index = (int)old_state_value;
    Assert(old_state_index >= (int)Human_States::None);
    Assert(old_state_index < (int)Human_States::COUNT);

    if (old_state_value != Human_States::None) {
        auto old_state = human_states[old_state_index];
        old_state.OnExit(old_state, human_id, human, data, ctx);
    }

    auto new_state = human_states[new_state_index];
    new_state.OnEnter(new_state, human_id, human, data, ctx);
}

// NOTE: Создание чувачка-грузчика.
// Он добавляется в world.humans_to_add, после чего перекидывается в gama_map.humans.
// Привязка к сегменту происходит в этот момент.
std::tuple<Human_ID, Human*> Create_Human_Transporter(
    Game&             game,
    v2i16             pos,
    Graph_Segment_ID  segment_id,
    const Human_Data& data,
    MCTX
) {
    auto& world = game.world;

    CTX_LOGGER;
    LOG_SCOPE;

    Human human{
        .moving = {
            .pos = pos,
        },
        .type                      = Human_Type::Transporter,
        .state                     = Human_States::None,
        .state_moving_in_the_world = Moving_In_The_World_State::None,
        .segment_id                = segment_id,
        .building_id               = Building_ID_Missing,
        .resource_id               = World_Resource_ID_Missing,
        .action_started_at         = -f64_inf,
    };

    auto [human_id, human_p] = world.humans_to_add.Add(ctx);

    *human_id = Next_Human_ID(world.last_entity_id);
    *human_p  = human;

    Root_Set_Human_State(*human_id, *human_p, Human_States::MovingInTheWorld, data, ctx);

    On_Human_Created(game, *human_id, *human_p, ctx);

    // TODO:
    // onHumanCreated.OnNext(new() { human = human });
    //
    // if (building.scriptable.type == BuildingType.SpecialCityHall) {
    //     onCityHallCreatedHuman.OnNext(new() { cityHall = building });
    //     DomainEvents<E_CityHallCreatedHuman>.Publish(new() { cityHall = building });
    // }

    // NOTE: Привязка будет при переносе из humans_to_add в humans.
    auto& segment             = *Query_Graph_Segment(world, segment_id);
    segment.assigned_human_id = Human_ID_Missing;

    return {*human_id, human_p};
}

// void Update_Building__Constructed(
//     Game&       game,
//     World&         world,
//     Building*       building,
//     f32               dt,
//     const Human_Data& data,
//     MCTX
// ) {
//     auto& scriptable = *Get_Scriptable_Building(game, building->scriptable);
//
//     auto delay = scriptable.human_spawning_delay;
//
//     if (scriptable.type == Building_Type::City_Hall) {
//         auto& since_created = building->time_since_human_was_created;
//         since_created += dt;
//         if (since_created > delay)
//             since_created = delay;
//
//         if (world.segment_ids_wo_humans.count > 0) {
//             if (since_created >= delay) {
//                 since_created -= delay;
//                 Graph_Segment* segment = Dequeue(world.segment_ids_wo_humans);
//                 Create_Human_Transporter(world, building, segment, data, ctx);
//             }
//         }
//     }
//
//     // TODO: _building_controller.Update(building, dt);
// }

void Process_City_Halls(Game& game, f32 dt, const Human_Data& data, MCTX) {
    CTX_ALLOCATOR;
    CTX_LOGGER;

    auto& world = game.world;

    for (auto [building_id, building] : Iter(&world.buildings)) {
        for (auto [city_hall_id, city_hall] : Iter(&world.city_halls)) {
            if (building_id != city_hall_id)
                continue;

            auto delay = building->scriptable->human_spawning_delay;

            auto& since_created = city_hall->time_since_human_was_created;
            since_created += dt;
            if (since_created > delay)
                since_created = delay;

            if (world.segment_ids_wo_humans.count > 0) {
                if (since_created >= delay) {
                    since_created -= delay;
                    Create_Human_Transporter(
                        game,
                        building->pos,
                        world.segment_ids_wo_humans.Dequeue(),
                        data,
                        ctx
                    );
                }
            }
        }
    }
}

void Remove_Humans(Game& game, MCTX) {
    auto& world = game.world;

    for (auto [human_id, reason_p] : Iter(&world.humans_to_remove)) {
        auto& reason = *reason_p;
        auto& human  = *Strict_Query_Human(world, human_id);

        if (reason == Human_Removal_Reason::Transporter_Returned_To_City_Hall) {
            // TODO: on_Human_Reached_City_Hall.On_Next(new (){human = human});
            world.human_ids_going_to_city_hall.Unstable_Remove(human_id);
        }
        else if (reason == Human_Removal_Reason::Employee_Reached_Building) {
            // TODO: on_Employee_Reached_Building.On_Next(new (){human = human});
            // TODO: human.building->employee = nullptr;
            Assert(human.building_id != Building_ID_Missing);
        }

        Deinit_Queue(human.moving.path, ctx);

        world.humans.Unstable_Remove(human_id);
        On_Human_Removed(game, human_id, human, reason, ctx);
    }

    world.humans_to_remove.Reset();
}

void Update_Human_Moving_Component(
    World&            world,
    Human_ID          human_id,
    Human&            human,
    float             dt,
    const Human_Data& data,
    MCTX
) {
    CTX_LOGGER;
    CTX_ALLOCATOR;

    auto& moving = human.moving;
    Assert(moving.to.has_value());

    const auto duration = world.data.humans_moving_one_tile_duration;

    moving.elapsed += dt;

    if (moving.elapsed > duration) {
        LOG_SCOPE;

        moving.elapsed -= duration;
        Assert(moving.elapsed < duration);

        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        moving.pos  = moving.to.value();
        moving.from = moving.pos;
        Advance_Moving_To(moving);

        Root_OnMovedToTheNextTile(human_id, human, data, ctx);
        // TODO: on_Human_Moved_To_The_Next_Tile.On_Next(new (){ human = human });
    }

    if (!moving.to.has_value())
        moving.elapsed = 0;

    moving.progress = MIN(1.0f, moving.elapsed / duration);

    SANITIZE_HUMAN;
}

void Update_Human(
    World&            world,
    Human_ID          human_id,
    Human*            human_p,
    float             dt,
    const Human_Data& data,
    MCTX
) {
    ZoneScoped;

    CTX_ALLOCATOR;

    auto& human            = *human_p;
    auto& humans_to_remove = world.humans_to_remove;

    if (human.moving.to.has_value()) {
        ZoneScopedN("Update_Human_Moving_Component");

        Update_Human_Moving_Component(world, human_id, human, dt, data, ctx);
    }

    if ((humans_to_remove.count > 0) && humans_to_remove.Contains(human_id))
        return;

    {
        ZoneScopedN("Human_Root_Update");

        Human_Root_Update(human_id, human, data, dt, ctx);
    }

    {
        ZoneScopedN("Checking if needs removal");

        auto state = Moving_In_The_World_State::Moving_To_The_City_Hall;
        if ((human.state_moving_in_the_world == state)  //
            && (!human.moving.to.has_value())           //
            && (human.moving.pos == Strict_Query_Building(world, human.building_id)->pos))
        {
            auto [id_p, r_value] = humans_to_remove.Add(ctx);

            *id_p    = human_id;
            *r_value = Human_Removal_Reason::Transporter_Returned_To_City_Hall;
        }
    }

    SANITIZE_HUMAN;
}

void Update_Humans(Game& game, f32 dt, const Human_Data& data, MCTX) {
    ZoneScoped;

    CTX_LOGGER;

    auto& world = game.world;

    for (auto [human_id, reason_p] : Iter(&world.humans_to_remove))
        Assert(*reason_p != Human_Removal_Reason::Transporter_Returned_To_City_Hall);

    Remove_Humans(game, ctx);

    for (auto [human_id, human_p] : Iter(&world.humans))
        Update_Human(world, human_id, human_p, dt, data, ctx);

    auto prev_count = world.humans_to_add.count;
    for (auto [human_id, human_to_move] : Iter(&world.humans_to_add)) {
        LOG_DEBUG("Update_Humans: moving human from humans_to_add to humans");
        auto [id_p, human_p] = world.humans.Add(ctx);

        *id_p    = human_id;
        *human_p = *human_to_move;

        auto& human = *human_p;

        if (human.segment_id != Graph_Segment_ID_Missing) {
            auto& segment             = *Query_Graph_Segment(world, human.segment_id);
            segment.assigned_human_id = human_id;
        }

        Update_Human(world, human_id, human_p, dt, data, ctx);
    }

    Assert(prev_count == world.humans_to_add.count);
    world.humans_to_add.Reset();

    Remove_Humans(game, ctx);

    {  // NOTE: Debug shiet.
        int humans_moving_to_destination = 0;
        int humans_moving_inside_segment = 0;
        for (auto [human_id, human_p] : Iter(&world.humans)) {
            auto& human = *human_p;

            if (human.state == Human_States::MovingInTheWorld  //
                && human.state_moving_in_the_world
                       == Moving_In_The_World_State::Moving_To_Destination)
            {
                humans_moving_to_destination++;
            }
            else if (human.state == Human_States::MovingInsideSegment) {
                humans_moving_inside_segment++;
            }
        }

        ImGui::Text("world.humans.count %d", world.humans.count);
        ImGui::Text(
            "world.human_ids_going_to_city_hall %d",
            world.human_ids_going_to_city_hall.count
        );
        ImGui::Text("humans_moving_to_destination %d", humans_moving_to_destination);
        ImGui::Text("humans_moving_inside_segment %d", humans_moving_inside_segment);
    }
}

void Update_World(Game& game, float dt, MCTX) {
    ZoneScoped;

    CTX_LOGGER;
    CTX_ALLOCATOR;

    auto& world       = game.world;
    auto& trash_arena = game.trash_arena;

    ImGui::Text("world.segments.count %d", world.segments.count);

    Process_City_Halls(game, dt, Assert_Deref(game.world.human_data), ctx);
    Update_Humans(game, dt, Assert_Deref(game.world.human_data), ctx);

    {  // Pathfind_Resources_In_Queue();
        auto& trash_arena = game.trash_arena;

        TEMP_USAGE(trash_arena);

        //--------------------------------------------------------------------------
        // `FindPairs`.
        //--------------------------------------------------------------------------
        Queue<std::tuple<Direction, v2i16>> bfqueue{};

        const auto gsize       = world.size;
        const auto tiles_count = gsize.x * gsize.y;

        auto visited     = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
        auto bfs_parents = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);
        FOR_RANGE (int, i, tiles_count) {
            *(bfs_parents + i) = -v2i16_one;
        }

        bool iteration_warning_emitted  = false;
        bool iteration2_warning_emitted = false;

        using Found_Resource_t = std::tuple<
            World_Resource_To_Book,
            World_Resource_ID,
            World_Resource*,
            Vector<v2i16>>;
        Vector<Found_Resource_t> found_pairs{};

        Vector<World_Resource_ID> booked_resources{};

        FOR_RANGE (int, i, world.resources_to_book.count) {
            auto& resource_to_book = *(world.resources_to_book.base + i);

            auto& building = *Strict_Query_Building(world, resource_to_book.building_id);
            auto  destination_pos = building.pos;

            auto min_x = destination_pos.x;
            auto max_x = destination_pos.x;
            auto min_y = destination_pos.y;
            auto max_y = destination_pos.y;

            FOR_RANGE (int, lll, resource_to_book.count) {
                FOR_DIRECTION (d) {
                    *bfqueue.Enqueue(ctx) = {d, destination_pos};
                }

                auto node_with_all_directions_marked = 0b00001111;
                WORLD_PTR_OFFSET(visited, destination_pos)
                    = node_with_all_directions_marked;

                World_Resource_ID found_resource_id = World_Resource_ID_Missing;
                World_Resource*   found_resource    = nullptr;

                // TODO(Hulvdan): Investigate ways of refactoring pathfinding algorithms
                // using some kind of sets of rules that differ depending on use cases.
                // Preferably without a performance hit.

                const int MAX_ITERATIONS = 256;

                int iteration = 0;
                while ((iteration++ < 10 * MAX_ITERATIONS)  //
                       && (found_resource == nullptr)       //
                       && (bfqueue.count > 0))
                {
                    auto [dir, pos] = bfqueue.Dequeue();

                    auto new_pos = pos + As_Offset(dir);

                    if (!Pos_Is_In_Bounds(new_pos, gsize))
                        continue;

                    if (Graph_Node_Has(WORLD_PTR_OFFSET(visited, new_pos), Opposite(dir)))
                        continue;

                    min_x = MIN(min_x, new_pos.x);
                    max_x = MAX(max_x, new_pos.x);
                    min_y = MIN(min_y, new_pos.y);
                    max_y = MAX(max_y, new_pos.y);

                    WORLD_PTR_OFFSET(visited, new_pos) = Graph_Node_Mark(
                        WORLD_PTR_OFFSET(visited, new_pos), Opposite(dir), true
                    );
                    WORLD_PTR_OFFSET(visited, pos)
                        = Graph_Node_Mark(WORLD_PTR_OFFSET(visited, pos), dir, true);

                    auto& tile = WORLD_PTR_OFFSET(world.element_tiles, pos);

                    auto& new_tile = WORLD_PTR_OFFSET(world.element_tiles, new_pos);

                    if ((tile.type == Element_Tile_Type::Building)
                        && (new_tile.type == Element_Tile_Type::Building))
                        continue;

                    if ((tile.type == Element_Tile_Type::None)
                        || (new_tile.type == Element_Tile_Type::None))
                        continue;

                    Building* new_tile_building = nullptr;
                    if (new_tile.type == Element_Tile_Type::Building)
                        new_tile_building
                            = Strict_Query_Building(world, new_tile.building_id);

                    if ((new_tile.type == Element_Tile_Type::Building)
                        && (new_tile_building->scriptable->type
                            != Building_Type::City_Hall))
                        continue;

                    WORLD_PTR_OFFSET(bfs_parents, new_pos) = pos;

                    FOR_RANGE (int, k, world.resources.count) {
                        const auto& res_id  = *(world.resources.ids + k);
                        const auto  res_ptr = world.resources.base + k;
                        auto&       res     = *res_ptr;

                        // На этой клетке должны быть сегменты.
                        auto should_skip = true;
                        FOR_RANGE (int, kk, world.segments.count) {
                            auto& ssss = *(world.segments.base + kk);
                            if (Graph_Contains(ssss.graph, new_pos)
                                && Graph_Node(ssss.graph, new_pos))
                            {
                                should_skip = false;
                                break;
                            }
                        }
                        if (should_skip)
                            continue;

                        // Ресурс уже был забронен.
                        if (res.booking_id != World_Resource_Booking_ID_Missing)
                            continue;

                        if (booked_resources.Contains(res_id))
                            continue;

                        // PERF: Накинуть индексы.
                        if ((res.pos == new_pos)
                            && (resource_to_book.scriptable == res.scriptable))
                        {
                            found_resource                            = res_ptr;
                            found_resource_id                         = res_id;
                            *booked_resources.Vector_Occupy_Slot(ctx) = res_id;
                            break;
                        }
                    }

                    FOR_DIRECTION (queue_dir) {
                        if (queue_dir == Opposite(dir))
                            continue;

                        if (Graph_Node_Has(WORLD_PTR_OFFSET(visited, new_pos), queue_dir))
                            continue;

                        *bfqueue.Enqueue(ctx) = {queue_dir, new_pos};
                    }
                }

                Assert(iteration < 10 * MAX_ITERATIONS);
                if ((iteration >= MAX_ITERATIONS) && (!iteration_warning_emitted)) {
                    iteration_warning_emitted = true;
                    LOG_WARN("WTF?");
                }

                if (found_resource != nullptr) {
                    Vector<v2i16> path{};
                    *path.Vector_Occupy_Slot(ctx) = found_resource->pos;

                    auto destination = found_resource->pos;

                    int iteration2 = 0;
                    while ((iteration2++ < (10 * MAX_ITERATIONS))
                           && (WORLD_PTR_OFFSET(bfs_parents, destination) != -v2i16_one))
                    {
                        *path.Vector_Occupy_Slot(ctx)
                            = WORLD_PTR_OFFSET(bfs_parents, destination);

                        auto new_destination = WORLD_PTR_OFFSET(bfs_parents, destination);
                        Assert(destination != new_destination);

                        destination = new_destination;
                    }

                    Assert(iteration2 < (10 * MAX_ITERATIONS));
                    if ((iteration2 >= MAX_ITERATIONS) && (!iteration2_warning_emitted)) {
                        LOG_WARN("WTF?");
                        iteration2_warning_emitted = true;
                    }

                    if (WORLD_PTR_OFFSET(bfs_parents, destination) == -v2i16_one)
                        *found_pairs.Vector_Occupy_Slot(ctx)
                            = {resource_to_book, found_resource_id, found_resource, path};
                }
            }

            for (int y = min_y; y <= max_y; y++) {
                for (int x = min_x; x <= max_x; x++)
                    WORLD_PTR_OFFSET(bfs_parents, v2i16(x, y)) = -v2i16_one;
            }

            bfqueue.Reset();
        }

        FOR_RANGE (int, i, found_pairs.count) {
            //-------------------------------------------------------------------
            // `BookResource`.
            //-------------------------------------------------------------------
            // using var _ = Tracing.Scope();
            auto& [resource_to_book, res_id, res_p, path] = *(found_pairs.base + i);

            auto& res = *res_p;

            Assert(res.transportation_segment_ids.count == 0);
            Assert(res.transportation_vertices.count == 0);

            FOR_RANGE (int, path_section_idx, path.count - 1) {
                auto a = *(path.base + path_section_idx);
                auto b = *(path.base + path_section_idx + 1);

                auto dir = v2i16_To_Direction(b - a);
                FOR_RANGE (int, segment_idx, world.segments.count) {
                    auto& seg_id = *(world.segments.ids + segment_idx);
                    auto& seg    = *(world.segments.base + segment_idx);

                    if (!Graph_Contains(seg.graph, a))
                        continue;

                    auto node = Graph_Node(seg.graph, a);
                    if (!Graph_Node_Has(node, dir))
                        continue;

                    // Skipping vertices as in this example when moving from C to B:
                    //     CrrFrrB
                    //       rrr
                    //
                    // We don't wanna see the F (flag) in our vertices list.
                    // The ending vertex is the only one we need per segment.
                    // In this example there should only be B (building) added in the
                    // list of vertices
                    //
                    // TODO: !!!
                    // Check the following:
                    //     CrrFFrrB
                    //       rrrr
                    //
                    if (path_section_idx + 2 <= path.count - 1) {
                        auto c = *(path.base + path_section_idx + 2);

                        if (Graph_Contains(seg.graph, c)) {
                            auto node_c = Graph_Node(seg.graph, c);
                            // Checking if b is an intermediate vertex
                            // that should be skipped.
                            auto dir = v2i16_To_Direction(b - c);
                            if (Graph_Node_Has(node_c, dir))
                                continue;
                        }
                    }

                    FOR_RANGE (int, vertex_idx, seg.vertices_count) {
                        auto& vertex = *(seg.vertices + vertex_idx);

                        if (vertex == b) {
                            // Hulvdan: transportation_segment_ids can contain duplicates.
                            // Consider the case:
                            //     CrFFrB
                            //      rrrr
                            *res.transportation_segment_ids.Vector_Occupy_Slot(ctx)
                                = seg_id;
                            *res.transportation_vertices.Vector_Occupy_Slot(ctx) = b;
                            SANITIZE;
                            break;
                        }
                        SANITIZE;
                    }
                }
            }
            SANITIZE;

            Assert(
                res.transportation_vertices.count == res.transportation_segment_ids.count
            );

            FOR_RANGE (int, i, res.transportation_segment_ids.count) {
                const auto  seg_id = *(res.transportation_segment_ids.base + i);
                const auto& seg    = *Strict_Query_Graph_Segment(world, seg_id);

                Assert_False(seg.resource_ids_to_transport.Contains(res_id));
                Assert_False(seg.linked_resource_ids.Contains(res_id));
            }

            World_Resource_Booking booking{
                .type        = World_Resource_Booking_Type::Construction,
                .building_id = resource_to_book.building_id,
            };

            auto booking_id = Next_World_Resource_Booking_ID(world.last_entity_id);
            {
                auto [booking_id_p, booking_p] = world.resource_bookings.Add(ctx);

                *booking_id_p = booking_id;
                *booking_p    = booking;
            }
            res.booking_id = booking_id;

            Assert(res.transportation_segment_ids.count > 0);

            auto& starting_segment_id = *(res.transportation_segment_ids.base + 0);
            auto& starting_segment
                = *Strict_Query_Graph_Segment(world, starting_segment_id);
            *starting_segment.resource_ids_to_transport.Enqueue(ctx) = res_id;

            FOR_RANGE (int, transportation_seg_idx, res.transportation_segment_ids.count)
            {
                auto seg_id
                    = *(res.transportation_segment_ids.base + transportation_seg_idx);
                auto& seg = *Strict_Query_Graph_Segment(world, seg_id);

                if (!seg.linked_resource_ids.Contains(res_id))
                    *seg.linked_resource_ids.Vector_Occupy_Slot(ctx) = res_id;
            }
        }

        FOR_RANGE (int, i, found_pairs.count) {
            auto& [resource_to_book, res_id, res_p, path] = *(found_pairs.base + i);
            Assert(resource_to_book.count > 0);

            resource_to_book.count--;

            if (resource_to_book.count == 0)
                world.resources_to_book.Unstable_Remove_At(
                    world.resources_to_book.Index_Of(resource_to_book)
                );
        }
    }

    SANITIZE;
}

void Add_World_Resource(
    Game&                game,
    Scriptable_Resource* scriptable,
    const v2i16          pos,
    MCTX
) {
    CTX_ALLOCATOR;
    auto& world       = game.world;
    auto  gsize       = world.size;
    auto& trash_arena = game.trash_arena;

    World_Resource resource{
        .scriptable        = scriptable,
        .pos               = pos,
        .booking_id        = World_Resource_Booking_ID_Missing,
        .targeted_human_id = Human_ID_Missing,
        .carrying_human_id = Human_ID_Missing,
    };

    {
        auto [id_p, resource_p] = world.resources.Add(ctx);
        *id_p                   = Next_World_Resource_ID(world.last_entity_id);
        *resource_p             = resource;
    }
}

void Init_World(
    bool /* first_time_initializing */,
    bool /* hot_reloaded */,
    Game&  game,
    Arena& arena,
    MCTX
) {
    CTX_LOGGER;
    SCOPED_LOG_INIT("Init_World");

#define X(state_name)                                      \
    human_states[(int)Human_States::state_name] = {        \
        HumanState_##state_name##_OnEnter,                 \
        HumanState_##state_name##_OnExit,                  \
        HumanState_##state_name##_Update,                  \
        HumanState_##state_name##_OnCurrentSegmentChanged, \
        HumanState_##state_name##_OnMovedToTheNextTile,    \
        HumanState_##state_name##_UpdateStates,            \
    };
    Human_States_Table;
#undef X

    auto& world = game.world;

    world.last_entity_id = 0;
    world.data.humans_moving_one_tile_duration
        = game.gamelib->humans()->moving_one_tile_duration();
    world.data.humans_picking_up_duration = game.gamelib->humans()->picking_up_duration();
    world.data.humans_placing_duration    = game.gamelib->humans()->placing_duration();

    {
        auto human_data         = Allocate_For(arena, Human_Data);
        human_data->world       = &game.world;
        human_data->trash_arena = &game.trash_arena;
        human_data->game        = &game;

        world.human_data = human_data;
    }
}

bool Try_Build(Game& game, v2i16 pos, const Item_To_Build& item, MCTX);

void Post_Init_World(
    bool first_time_initializing,
    bool /* hot_reloaded */,
    Game& game,
    Arena& /* arena */,
    MCTX
) {
    // Ставим здание на след. кадр после начального.
    static bool next_frame_actions_executed = false;
    if (!first_time_initializing && !next_frame_actions_executed) {
        next_frame_actions_executed = true;

        Item_To_Build flag{
            .type = Item_To_Build_Type::Flag,
        };
        Try_Build(game, {8, 1}, flag, ctx);

        Item_To_Build lumberjacks_hut{
            .type                = Item_To_Build_Type::Building,
            .scriptable_building = game.scriptable_buildings + 1,
        };
        Try_Build(game, {0, 3}, lumberjacks_hut, ctx);
        Try_Build(game, {10, 2}, lumberjacks_hut, ctx);

        Assert(game.scriptable_resources_count > 0);
    }

    if (!first_time_initializing)
        return;

    CTX_LOGGER;
    SCOPED_LOG_INIT("Post_Init_World");

    Place_Building(game, {4, 1}, game.scriptable_buildings + 0, true, ctx);  // built=true

    Add_World_Resource(game, game.scriptable_resources + 0, {1, 0}, ctx);
    Add_World_Resource(game, game.scriptable_resources + 0, {1, 0}, ctx);

    Add_World_Resource(game, game.scriptable_resources + 0, {5, 1}, ctx);
    Add_World_Resource(game, game.scriptable_resources + 0, {6, 1}, ctx);
}

void Deinit_World(Game& game, MCTX) {
    CTX_ALLOCATOR;
    auto& world = game.world;

    for (auto [segment_id, segment_p] : Iter(&world.segments)) {
        auto& segment    = *segment_p;
        auto  graph_size = segment.graph.size;

        FREE(segment.vertices, sizeof(v2i16) * segment.vertices_count);
        FREE(segment.graph.nodes, sizeof(u8) * graph_size.x * graph_size.y);

        segment.linked_segment_ids.Reset();
        Deinit_Queue(segment.resource_ids_to_transport, ctx);

        Assert(segment.graph.nodes != nullptr);
        Assert(segment.graph.data != nullptr);

        auto& data = *segment.graph.data;
        Assert(data.dist != nullptr);
        Assert(data.prev != nullptr);

        std::destroy_at(&data.node_index_2_pos);
        std::destroy_at(&data.pos_2_node_index);

        auto n = segment.graph.non_zero_nodes_count;
        FREE(data.dist, sizeof(i16) * n);
        FREE(data.prev, sizeof(i16) * n);
    }

    Deinit_Sparse_Array(world.segments, ctx);
    Deinit_Queue(world.segment_ids_wo_humans, ctx);

    Deinit_Sparse_Array(world.buildings, ctx);
    Deinit_Sparse_Array_Of_Ids(world.not_constructed_building_ids, ctx);
    Deinit_Sparse_Array(world.city_halls, ctx);

    for (auto [_, human] : Iter(&world.humans))
        Deinit_Queue(human->moving.path, ctx);
    Deinit_Sparse_Array(world.humans, ctx);

    Deinit_Sparse_Array_Of_Ids(world.human_ids_going_to_city_hall, ctx);
    Deinit_Sparse_Array(world.humans_to_add, ctx);
    Deinit_Sparse_Array(world.humans_to_remove, ctx);
    Deinit_Sparse_Array(world.resources, ctx);

    Deinit_Vector(world.resources_to_book, ctx);
}

void Regenerate_Terrain_Tiles(
    Game& /* game */,
    World& world,
    Arena& /* arena */,
    Arena& trash_arena,
    uint /* seed */,
    Editor_Data& data,
    MCTX
) {
    CTX_LOGGER;
    SCOPED_LOG_INIT("Regenerate_Terrain_Tiles");

    auto gsize = world.size;

    auto noise_pitch = (size_t)Ceil_To_Power_Of_2((u32)MAX(gsize.x, gsize.y));
    auto output_size = noise_pitch * noise_pitch * 2;
    TEMP_USAGE(trash_arena);

    auto terrain_perlin = Allocate_Array(trash_arena, u16, output_size);
    Cycled_Perlin_2D(
        terrain_perlin,
        sizeof(u16) * output_size,
        trash_arena,
        data.terrain_perlin,
        noise_pitch,
        noise_pitch
    );

    auto forest_perlin = Allocate_Array(trash_arena, u16, output_size);
    Cycled_Perlin_2D(
        forest_perlin,
        sizeof(u16) * output_size,
        trash_arena,
        data.forest_perlin,
        noise_pitch,
        noise_pitch
    );

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile   = Get_Terrain_Tile(world, {x, y});
            tile.terrain = Terrain::Grass;
            auto noise   = (f32)(*(terrain_perlin + noise_pitch * y + x)) / (f32)u16_max;
            tile.height  = int((f32)(data.terrain_max_height + 1) * noise);

            Assert(tile.height >= 0);
            Assert(tile.height <= data.terrain_max_height);
        }
    }

    // NOTE: Removing one-tile-high grass blocks because they'd look ugly.
    while (true) {
        bool changed = false;
        FOR_RANGE (int, y, gsize.y) {
            FOR_RANGE (int, x, gsize.x) {
                auto& tile = Get_Terrain_Tile(world, {x, y});

                int height_above = 0;
                if (y < gsize.y - 1)
                    height_above = Get_Terrain_Tile(world, {x, y + 1}).height;

                int height_below = 0;
                if (y > 0)
                    height_below = Get_Terrain_Tile(world, {x, y - 1}).height;

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
            auto& tile = Get_Terrain_Tile(world, {x, y});
            if (tile.is_cliff)
                continue;

            tile.is_cliff
                = y == 0 || tile.height > Get_Terrain_Tile(world, {x, y - 1}).height;
        }
    }

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile     = Get_Terrain_Tile(world, {x, y});
            auto& resource = Get_Terrain_Resource(world, {x, y});

            auto noise    = (f32)(*(forest_perlin + noise_pitch * y + x)) / (f32)u16_max;
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
    Game& /* game */,
    World& world,
    Arena& /* arena */,
    Arena& /* trash_arena */,
    uint /* seed */,
    Editor_Data& /* data */,
    MCTX
) {
    CTX_LOGGER;
    SCOPED_LOG_INIT("Regenerate_Terrain_Tiles");

    auto gsize = world.size;

    v2i16 road_tiles[] = {
        {0, 0},
        {1, 0},
        {2, 0},

        {0, 1},
        {2, 1},

        {0, 2},
        {1, 2},
        {2, 2},

        // {3, 1},

        {5, 1},
        {6, 1},
        {7, 1},
        {8, 1},
        {9, 1},
        {10, 1},
    };

    auto base_offset = v2i16(0, 0);

    for (auto offset : road_tiles) {
        auto          off  = offset + base_offset;
        Element_Tile& tile = world.element_tiles[off.y * gsize.x + off.x];
        tile.type          = Element_Tile_Type::Road;
    }

    FOR_RANGE (int, y, 5) {
        FOR_RANGE (int, x, 9) {
            const v2i16 base_offset{5, 5};

            auto          off  = v2i16(x, y) + base_offset;
            Element_Tile& tile = world.element_tiles[off.y * gsize.x + off.x];
            tile.type          = Element_Tile_Type::Road;
        }
    }

    for (int y = 0; y < 5; y += 2) {
        for (int x = 0; x < 9; x += 2) {
            const v2i16 base_offset{5, 5};

            auto          off  = v2i16(x, y) + base_offset;
            Element_Tile& tile = world.element_tiles[off.y * gsize.x + off.x];
            tile.type          = Element_Tile_Type::Flag;
        }
    }

#if 1
    v2i16 flag_tiles[] = {
        {11, 1},
        // {0, 2},
        // {2, 0},
    };

    for (auto offset : flag_tiles) {
        auto          off  = offset + base_offset;
        Element_Tile& tile = world.element_tiles[off.y * gsize.x + off.x];
        tile.type          = Element_Tile_Type::Flag;
    }
#endif

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            Element_Tile& tile = world.element_tiles[y * gsize.x + x];
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
                auto  graph_pos = pos - graph.offset;

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

            auto& graph     = segment.graph;
            auto  graph_pos = pos - graph.offset;

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

bool Adjacent_Tiles_Are_Connected(Graph& graph, i16 x, i16 y) {
    const auto gx = graph.size.x;

    u8 node = *(graph.nodes + y * gx + x);

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

// Проверка на то, что граф является неорграфом.
void Assert_Is_Undirected(Graph& graph) {
    Assert(graph.size.x > 0);
    Assert(graph.size.y > 0);

    FOR_RANGE (i16, y, graph.size.y) {
        FOR_RANGE (i16, x, graph.size.x) {
            Assert(Adjacent_Tiles_Are_Connected(graph, x, y));
        }
    }
}

void Calculate_Graph_Data(Graph& graph, Arena& trash_arena, MCTX) {
    Assert(graph.nodes != nullptr);
    Assert(graph.non_zero_nodes_count > 0);

    CTX_ALLOCATOR;

    TEMP_USAGE(trash_arena);

    auto n     = graph.non_zero_nodes_count;
    auto nodes = graph.nodes;
    auto sy    = graph.size.y;
    auto sx    = graph.size.x;

    graph.data = (Calculated_Graph_Data*)ALLOC(sizeof(Calculated_Graph_Data));
    auto& data = *graph.data;

    auto& node_index_2_pos = *std::construct_at(&data.node_index_2_pos);
    auto& pos_2_node_index = *std::construct_at(&data.pos_2_node_index);

    {
        int node_index = 0;
        FOR_RANGE (int, y, sy) {
            FOR_RANGE (int, x, sx) {
                auto node = nodes[sx * y + x];
                if (node == 0)
                    continue;

                data.node_index_2_pos.insert({node_index, v2i16(x, y)});
                data.pos_2_node_index.insert({v2i16(x, y), node_index});

                node_index += 1;
            }
        }
    }

    // NOTE: |V| = _nodes_count
    // > let dist be a |V| × |V| array of minimum distances initialized to ∞
    // (infinity) > let prev be a |V| × |V| array of minimum distances initialized to
    // null
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
        i16 node_index = 0;
        FOR_RANGE (int, y, sy) {
            FOR_RANGE (int, x, sx) {
                u8 node = nodes[sx * y + x];
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
        Assert(n < (u16)i16_max);
        FOR_RANGE (u16, node_index, n) {
            dist[node_index * n + node_index] = 0;
            prev[node_index * n + node_index] = (i16)node_index;
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
    FOR_RANGE (u16, k, n) {
        FOR_RANGE (u16, j, n) {
            FOR_RANGE (u16, i, n) {
                auto ij = dist[n * i + j];
                auto ik = dist[n * i + k];
                auto kj = dist[n * k + j];

                if ((ik != i16_max)     //
                    && (kj != i16_max)  //
                    && (ij > ik + kj))
                {
                    dist[i * n + j] = (i16)(ik + kj);
                    prev[i * n + j] = prev[k * n + j];
                }
            }
        }
    }

#if ASSERT_SLOW
    Assert_Is_Undirected(graph);
#endif

    // NOTE: Вычисление центра графа.
    i16* node_eccentricities = Allocate_Zeros_Array(trash_arena, i16, n);
    FOR_RANGE (u16, i, n) {
        FOR_RANGE (u16, j, n) {
            node_eccentricities[i] = MAX(node_eccentricities[i], dist[n * i + j]);
        }
    }

    i16 rad  = i16_max;
    i16 diam = 0;
    FOR_RANGE (u16, i, n) {
        rad  = MIN(rad, node_eccentricities[i]);
        diam = MAX(diam, node_eccentricities[i]);
    }

    FOR_RANGE (u16, i, n) {
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
    Calculate_Graph_Data(segment.graph, trash_arena, ctx);
    segment.assigned_human_id = Human_ID_Missing;

    auto [id_p, segment1_p] = segments.Add(ctx);
    *id_p                   = Next_Graph_Segment_ID(last_entity_id);
    *segment1_p             = segment;

    auto& id = *id_p;

    for (auto [segment2_id, segment2_p] : Iter(&segments)) {
        if (segment2_id == id)
            continue;

        // PERF: Мб AABB / QuadTree стоит заюзать.
        auto& segment1 = *segment1_p;
        auto& segment2 = *segment2_p;
        if (Have_Some_Of_The_Same_Vertices(segment1, segment2)) {
            if (segment2.linked_segment_ids.Index_Of(id) == -1)
                *segment2.linked_segment_ids.Vector_Occupy_Slot(ctx) = id;

            if (segment1.linked_segment_ids.Index_Of(segment2_id) == -1)
                *segment1.linked_segment_ids.Vector_Occupy_Slot(ctx) = segment2_id;
        }
    }

    SANITIZE;

    return {id, segment1_p};
}

using Graph_Segments_To_Add = Fixed_Size_Slice<Graph_Segment>;

using Segment_To_Delete        = std::tuple<Graph_Segment_ID, Graph_Segment*>;
using Graph_Segments_To_Delete = Fixed_Size_Slice<Segment_To_Delete>;

BF_FORCE_INLINE void Update_Segments(
    Arena& trash_arena,
    Game& /* game */,
    World&                    world,
    Graph_Segments_To_Add&    segments_to_add,
    Graph_Segments_To_Delete& segments_to_delete,
    MCTX
) {
    CTX_ALLOCATOR;
    CTX_LOGGER;
    LOG_SCOPE;
    LOG_DEBUG("world.segments.count = %d", world.segments.count);
    LOG_DEBUG("segments_to_add.count = %d", segments_to_add.count);
    LOG_DEBUG("segments_to_delete.count = %d", segments_to_delete.count);

    TEMP_USAGE(trash_arena);

    // NOTE: `Graph_Segment*` is nullable, `Human*` is not.
    using tttt = std::tuple<Graph_Segment_ID, Graph_Segment*, Human_ID, Human*>;

    SANITIZE;

    // Удаление сегментов (отвязка от чувачков,
    // от других сегментов и высвобождение памяти).
    FOR_RANGE (u32, i, segments_to_delete.count) {
        auto [segment_id, segment_p] = segments_to_delete.items[i];
        auto& segment                = *segment_p;

        LOG_DEBUG("Update_Segments: deleting segment %d", segment_id);
        LOG_DEBUG("segment.assigned_human_id %d", segment.assigned_human_id);

        // Если у сегмента был чувак, отвязываем его от него и ставим,
        // что он идёт в ратушу.
        //
        // Возможно, что ему далее по коду будет назначен новый сегмент.
        if (segment.assigned_human_id != Human_ID_Missing) {
            auto  human_id = segment.assigned_human_id;
            auto& human    = *Strict_Query_Human(world, segment.assigned_human_id);

            human.segment_id = Graph_Segment_ID_Missing;
            bool moving_in_the_world_or_inside_segment
                = (human.state == Human_States::MovingInTheWorld)
                  || (human.state == Human_States::MovingInsideSegment);
            Assert(moving_in_the_world_or_inside_segment);

            Root_Set_Human_State(
                human_id, human, Human_States::MovingInTheWorld, *world.human_data, ctx
            );
            Assert(
                human.state_moving_in_the_world
                == Moving_In_The_World_State::Moving_To_The_City_Hall
            );

            *world.human_ids_going_to_city_hall.Add(ctx) = segment.assigned_human_id;

            segment.assigned_human_id = Human_ID_Missing;
        }

        SANITIZE;

        // Отвязываем сегмент от других сегментов.
        for (auto linked_segment_id_p : Iter(&segment.linked_segment_ids)) {
            auto linked_segment_id = *linked_segment_id_p;
            LOG_DEBUG(
                "Update_Segments: Unlinking %d from %d", linked_segment_id, segment_id
            );

            Graph_Segment& linked_segment
                = *Strict_Query_Graph_Segment(world, linked_segment_id);

            auto found_index = linked_segment.linked_segment_ids.Index_Of(segment_id);
            if (found_index != -1)
                linked_segment.linked_segment_ids.Remove_At(found_index);
        }

        SANITIZE;

        // TODO: _resource_transportation.OnSegmentDeleted(segment);

        // Удаляем сегмент из очереди сегментов на добавление чувачков,
        // если этот сегмент ранее был в неё добавлен.
        // PERF: Много memmove происходит.
        auto& queue = world.segment_ids_wo_humans;
        auto  index = queue.Index_Of(segment_id);
        if (index != -1)
            queue.Remove_At(index);

        SANITIZE;

        // NOTE: Уничтожаем сегмент.
        FREE(segment.vertices, sizeof(v2i16) * segment.vertices_count);
        FREE(segment.graph.nodes, segment.graph.non_zero_nodes_count);
        Assert(segment.graph.data != nullptr);
        std::destroy_at(&segment.graph.data->node_index_2_pos);
        std::destroy_at(&segment.graph.data->pos_2_node_index);

        SANITIZE;
    }

    FOR_RANGE (i32, i, segments_to_delete.count) {
        auto& [segment_id, _] = *(segments_to_delete.items + i);
        world.segments.Unstable_Remove(segment_id);
    }

    // NOTE: Вносим созданные сегменты. Если будут свободные чувачки - назначим им.
    using Added_Segment_Type           = std::tuple<Graph_Segment_ID, Graph_Segment*>;
    Added_Segment_Type* added_segments = nullptr;

    if (segments_to_add.count > 0) {
        added_segments
            = Allocate_Array(trash_arena, Added_Segment_Type, segments_to_add.count);
    }

    FOR_RANGE (u32, i, segments_to_add.count) {
        added_segments[i] = Add_And_Link_Segment(
            world.last_entity_id,
            world.segments,
            segments_to_add.items[i],
            trash_arena,
            ctx
        );
    }

    // TODO: _resourceTransportation.PathfindItemsInQueue();
    // Tracing.Log("_itemTransportationSystem.PathfindItemsInQueue()");

    Assert(world.human_data != nullptr);

    // По возможности назначаем чувачков на сегменты без них.
    while ((world.segment_ids_wo_humans.count > 0)  //
           && (world.human_ids_going_to_city_hall.count > 0))
    {
        auto  segment_id = world.segment_ids_wo_humans.Dequeue();
        auto& segment    = *Strict_Query_Graph_Segment(world, segment_id);

        auto  human_id = world.human_ids_going_to_city_hall.Pop();
        auto& human    = *Strict_Query_Human(world, human_id);

        segment.assigned_human_id = human_id;
        human.segment_id          = segment_id;

        Root_OnCurrentSegmentChanged(human_id, human, *world.human_data, ctx);
    }

    // По возможности назначаем чувачков на новые сегменты.
    // Если нет чувачков - сохраняем сегменты как те, которым нужны чувачки.
    auto added_segments_count = segments_to_add.count;
    while ((added_segments_count > 0)  //
           && (world.human_ids_going_to_city_hall.count > 0))
    {
        auto [segment_id, segment_p] = added_segments[--added_segments_count];
        auto& segment                = *segment_p;

        auto  human_id = world.human_ids_going_to_city_hall.Pop();
        auto& human    = *Strict_Query_Human(world, human_id);

        segment.assigned_human_id = human_id;
        human.segment_id          = segment_id;

        Root_OnCurrentSegmentChanged(human_id, human, *world.human_data, ctx);
    }

    while (added_segments_count > 0) {
        added_segments_count--;
        auto [id, _]                              = added_segments[added_segments_count];
        *world.segment_ids_wo_humans.Enqueue(ctx) = id;
    }
}

using Dir_v2i16 = std::tuple<Direction, v2i16>;

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

        auto p           = big_queue.Dequeue();
        *queue.Enqueue() = p;

        auto [_, p_pos] = p;
        if (full_graph_build)
            WORLD_PTR_OFFSET(vis, p_pos) = true;

        auto vertices      = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);
        auto segment_tiles = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);

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
                WORLD_PTR_OFFSET(vis, pos) = true;

            auto& tile = WORLD_PTR_OFFSET(element_tiles, pos);

            bool is_flag     = tile.type == Element_Tile_Type::Flag;
            bool is_building = tile.type == Element_Tile_Type::Building;
            bool is_vertex   = is_building || is_flag;

            if (is_vertex)
                Add_Without_Duplication(tiles_count, vertices_count, vertices, pos);

            FOR_DIRECTION (dir_index) {
                if (is_vertex && dir_index != dir)
                    continue;

                u8& visited_value = WORLD_PTR_OFFSET(visited, pos);
                if (Graph_Node_Has(visited_value, dir_index))
                    continue;

                v2i16 new_pos = pos + As_Offset(dir_index);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                Direction opposite_dir_index = Opposite(dir_index);
                u8&       new_visited_value  = WORLD_PTR_OFFSET(visited, new_pos);
                if (Graph_Node_Has(new_visited_value, opposite_dir_index))
                    continue;

                auto& new_tile = WORLD_PTR_OFFSET(element_tiles, new_pos);
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
                                *big_queue.Enqueue() = {new_dir_index, new_pos};
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
                            *big_queue.Enqueue() = {new_dir_index, new_pos};
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
                    *queue.Enqueue() = {(Direction)0, new_pos};
                }
            }

            SANITIZE;
        }

        // NOTE: Поиск островов графа.
        if (full_graph_build && !big_queue.count) {
            FOR_RANGE (int, y, gsize.y) {
                FOR_RANGE (int, x, gsize.x) {
                    auto  pos  = v2i16(x, y);
                    auto& tile = WORLD_PTR_OFFSET(element_tiles, pos);
                    u8&   v1   = WORLD_PTR_OFFSET(visited, pos);
                    bool& v2   = WORLD_PTR_OFFSET(vis, pos);

                    bool is_building = tile.type == Element_Tile_Type::Building;
                    bool is_flag     = tile.type == Element_Tile_Type::Flag;
                    bool is_vertex   = is_building || is_flag;

                    if (is_vertex && !v1 && !v2) {
                        FOR_DIRECTION (dir_index) {
                            *big_queue.Enqueue() = {dir_index, pos};
                        }
                    }
                }
            }
        }

        if (vertices_count <= 1)
            continue;

        // NOTE: Adding a new segment.
        Assert(temp_graph.non_zero_nodes_count > 0);

        // TODO: Add_Unsafe()?
        auto& segment          = added_segments.items[added_segments.count];
        segment.vertices_count = vertices_count;
        added_segments.count += 1;

        segment.vertices = (v2i16*)ALLOC(sizeof(v2i16) * vertices_count);
        memcpy(segment.vertices, vertices, sizeof(v2i16) * vertices_count);

        segment.graph.non_zero_nodes_count = temp_graph.non_zero_nodes_count;

        // NOTE: Вычисление size и offset графа.
        auto& gr_size = segment.graph.size;
        auto& offset  = segment.graph.offset;

        offset = gsize - v2i16_one;

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
        gr_size -= offset;
        gr_size += v2i16_one;

        Assert(gr_size.x > 0);
        Assert(gr_size.y > 0);
        Assert(offset.x >= 0);
        Assert(offset.y >= 0);
        Assert(offset.x < gsize.x);
        Assert(offset.y < gsize.y);

        // NOTE: Копирование нод из временного графа
        // с небольшой оптимизацией по требуемой памяти.
        segment.graph.non_zero_nodes_count = gr_size.x * gr_size.y;
        segment.graph.nodes = (u8*)ALLOC(segment.graph.non_zero_nodes_count);

        auto rows   = gr_size.y;
        auto stride = gsize.x;
        // auto stride        = gr_size.x;
        auto starting_node = temp_graph.nodes + offset.y * gsize.x + offset.x;
        Rect_Copy(segment.graph.nodes, starting_node, stride, rows, gr_size.x);

        SANITIZE;
    }
}

// Создание сетки графа.
// Вызывается на старте игры (предполагается, что при её загрузке).
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
        *big_queue.Enqueue() = {dir, pos};
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

// Обновление сетки графа после строения чего-либо.
// Сразу несколько тайлов могут быть построены или удалены.
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

    Graph_Segments_To_Delete segments_to_delete{};
    segments_to_delete.max_count = segments_to_delete_allocate;
    segments_to_delete.items     = Allocate_Zeros_Array(
        trash_arena, Segment_To_Delete, segments_to_delete_allocate
    );

    for (auto [id, segment_p] : Iter(segments)) {
        if (Should_Segment_Be_Deleted(gsize, element_tiles, updated_tiles, *segment_p))
            *segments_to_delete.Add_Unsafe() = Segment_To_Delete(id, segment_p);
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

                auto& element_tile = WORLD_PTR_OFFSET(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                *big_queue.Enqueue() = {dir, pos};
            }
        } break;

        case Tile_Updated_Type::Flag_Placed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = WORLD_PTR_OFFSET(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::Road)
                    *big_queue.Enqueue() = {dir, pos};
            }
        } break;

        case Tile_Updated_Type::Flag_Removed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = WORLD_PTR_OFFSET(element_tiles, new_pos);
                if (element_tile.type != Element_Tile_Type::None)
                    *big_queue.Enqueue() = {dir, pos};
            }
        } break;

        case Tile_Updated_Type::Road_Removed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = WORLD_PTR_OFFSET(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                FOR_DIRECTION (dir) {
                    *big_queue.Enqueue() = {dir, new_pos};
                }
            }
        } break;

        case Tile_Updated_Type::Building_Placed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = WORLD_PTR_OFFSET(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::None)
                    continue;

                if (element_tile.type == Element_Tile_Type::Building)
                    continue;

                if (element_tile.type == Element_Tile_Type::Flag)
                    continue;

                FOR_DIRECTION (dir) {
                    *big_queue.Enqueue() = {dir, new_pos};
                }
            }
        } break;

        case Tile_Updated_Type::Building_Removed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = WORLD_PTR_OFFSET(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::Road) {
                    FOR_DIRECTION (new_dir) {
                        *big_queue.Enqueue() = {new_dir, new_pos};
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

#if ASSERT_SLOW
    for (auto [segment1_id, segment1_p] : Iter(segments)) {
        auto& segment1  = *segment1_p;
        auto& g1        = segment1.graph;
        v2i16 g1_offset = {g1.offset.x, g1.offset.y};

        for (auto [segment2_id, segment2_p] : Iter(segments)) {
            if (segment1_id == segment2_id)
                continue;

            auto& segment2 = *segment2_p;

            auto& g2 = segment2.graph;

            v2i16 g2_offset = {g2.offset.x, g2.offset.y};

            FOR_RANGE (int, y, g1.size.y) {
                FOR_RANGE (int, x, g1.size.x) {
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

    return {segments_to_add.count, segments_to_delete.count};
}

#define Declare_Updated_Tiles(variable_name_, pos_, type_) \
    Updated_Tiles(variable_name_){};                       \
    (variable_name_).count = 1;                            \
    (variable_name_).pos   = &(pos_);                      \
    auto type__            = (type_);                      \
    (variable_name_).type  = &type__;

#define INVOKE_UPDATE_TILES                                                            \
    STATEMENT({                                                                        \
        Update_Tiles(                                                                  \
            game.world.size,                                                           \
            game.world.element_tiles,                                                  \
            &game.world.segments,                                                      \
            trash_arena,                                                               \
            updated_tiles,                                                             \
            [&world, &trash_arena, &game](                                             \
                Graph_Segments_To_Add&    segments_to_add,                             \
                Graph_Segments_To_Delete& segments_to_delete,                          \
                MCTX                                                                   \
            ) {                                                                        \
                Update_Segments(                                                       \
                    trash_arena, game, world, segments_to_add, segments_to_delete, ctx \
                );                                                                     \
            },                                                                         \
            ctx                                                                        \
        );                                                                             \
    })

bool Try_Build(Game& game, v2i16 pos, const Item_To_Build& item, MCTX) {
    CTX_LOGGER;
    auto& arena                = game.arena;
    auto& non_persistent_arena = game.non_persistent_arena;
    auto& trash_arena          = game.trash_arena;

    auto& world = game.world;
    auto  gsize = world.size;
    Assert(Pos_Is_In_Bounds(pos, gsize));

    auto& tile = *(world.element_tiles + pos.y * gsize.x + pos.x);

    switch (item.type) {
    case Item_To_Build_Type::Flag: {
        Assert(item.scriptable_building == nullptr);

        if (tile.type == Element_Tile_Type::Flag) {
            tile.type = Element_Tile_Type::Road;
            Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Flag_Removed);
            LOG_DEBUG(
                "player builds `%s` at %d.%d",
                Item_To_Build_Type_Names[(int)item.type],
                pos.x,
                pos.y
            );
            INVOKE_UPDATE_TILES;
        }
        else if (tile.type == Element_Tile_Type::Road) {
            tile.type = Element_Tile_Type::Flag;
            Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Flag_Placed);
            LOG_DEBUG(
                "player builds `%s` at %d.%d",
                Item_To_Build_Type_Names[(int)item.type],
                pos.x,
                pos.y
            );
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

        LOG_DEBUG(
            "player builds `%s` at %d.%d",
            Item_To_Build_Type_Names[(int)item.type],
            pos.x,
            pos.y
        );
        INVOKE_UPDATE_TILES;
    } break;

    case Item_To_Build_Type::Building: {
        Assert(item.scriptable_building != nullptr);

        if (tile.type != Element_Tile_Type::None)
            return false;

        bool built = false;
        Place_Building(game, pos, item.scriptable_building, built, ctx);

        Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Building_Placed);

        LOG_DEBUG(
            "player builds `%s` at %d.%d",
            Item_To_Build_Type_Names[(int)item.type],
            pos.x,
            pos.y
        );
        INVOKE_UPDATE_TILES;
    } break;

    default:
        INVALID_PATH;
    }

    On_Item_Built(game, pos, item, ctx);

    return true;
}

// Game&                    game
// const Human_ID&          human_id
// Human&                   human
// const World_Resource_ID& resource_id
// World_Resource&          resource
// MCTX
On_Human_Finished_Picking_Up_Resource_function(World_OnHumanFinishedPickingUpResource) {
    //
}

// Game&                    game
// const Human_ID&          human_id
// Human&                   human
// const World_Resource_ID& resource_id
// World_Resource&          resource
// MCTX
On_Human_Finished_Placing_Resource_function(World_OnHumanFinishedPlacingResource) {
    UNUSED(human_id);

    CTX_LOGGER;

    auto& world  = game.world;
    auto& res_id = resource_id;
    auto& res    = resource;

    auto placed_inside_building = false;

    res.pos = human.moving.pos;

    if (res.booking_id != World_Resource_Booking_ID_Missing) {
        auto& booking  = *String_Query_World_Resource_Booking(world, res.booking_id);
        auto& building = *Strict_Query_Building(world, booking.building_id);

        placed_inside_building = (building.pos == res.pos);
    }

    auto moved_to_the_next_segment_in_path = false;
    if (res.transportation_vertices.count > 0) {
        auto vertex = *(res.transportation_vertices.base + 0);
        res.transportation_segment_ids.Remove_At(0);
        res.transportation_vertices.Remove_At(0);

        moved_to_the_next_segment_in_path
            = (res.pos == vertex) && (res.transportation_segment_ids.count > 0);
    }

    if (human.segment_id != Graph_Segment_ID_Missing) {
        auto& seg = *Strict_Query_Graph_Segment(game.world, human.segment_id);
        seg.linked_resource_ids.Unstable_Remove_At(seg.linked_resource_ids.Index_Of(res_id
        ));

        if (seg.resource_ids_to_transport.Contains(res_id)) {
            seg.resource_ids_to_transport.Remove_At(
                seg.resource_ids_to_transport.Index_Of(res_id)
            );
            Assert(!seg.resource_ids_to_transport.Contains(res_id));
        }
    }

    if (placed_inside_building) {
        LOG_INFO("placed_inside_building");

        Assert(res.booking_id != World_Resource_Booking_ID_Missing);
        auto& booking  = *String_Query_World_Resource_Booking(world, res.booking_id);
        auto& building = *Strict_Query_Building(world, booking.building_id);

        // TODO:
        // building.placedResourcesForConstruction.Add(res);
        // _map.OnResourcePlacedInsideBuilding(res, building);
    }
    else if (moved_to_the_next_segment_in_path) {
        LOG_INFO("moved_to_the_next_segment_in_path");

        Assert(res.booking_id != World_Resource_Booking_ID_Missing);

        auto  seg_id = *(res.transportation_segment_ids.base + 0);
        auto& seg    = *Strict_Query_Graph_Segment(world, seg_id);
        Assert(!seg.resource_ids_to_transport.Contains(res_id));

        *seg.resource_ids_to_transport.Enqueue(ctx) = res_id;
    }
    else {
        LOG_INFO("Resource was placed on the map");

        if (res.booking_id != World_Resource_Booking_ID_Missing) {
            game.world.resource_bookings.Unstable_Remove(res.booking_id);
            res.booking_id = World_Resource_Booking_ID_Missing;
        }

        res.targeted_human_id = Human_ID_Missing;
    }
}
