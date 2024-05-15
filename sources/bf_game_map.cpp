#pragma once

#include "bf_base.h"
#define GRID_PTR_VALUE(arr_ptr, pos) (*(arr_ptr + gsize.x * pos.y + pos.x))

#define Array_Push(array, array_count, array_max_count, value) \
    {                                                          \
        *(array + (array_count)) = value;                      \
        (array_count)++;                                       \
        Assert((array_count) <= (array_max_count));            \
    }

template <typename T>
BF_FORCE_INLINE T Array_Pop(T* array, auto& array_count) {
    Assert(array_count > 0);
    T result = *(array + array_count - 1);
    array_count--;
    return result;
}

#define Array_Reverse(array, count)                                      \
    {                                                                    \
        Assert((count) >= 0);                                            \
        FOR_RANGE (i32, i, (count) / 2) {                                \
            auto t                       = *((array) + i);               \
            *((array) + i)               = *((array) + ((count)-i - 1)); \
            *((array) + ((count)-i - 1)) = t;                            \
        }                                                                \
    }

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

global_var int global_last_segments_to_delete_count = 0;
global_var int global_last_segments_to_add_count    = 0;

struct Path_Find_Result {
    bool   success;
    v2i16* path;
    i32    path_count;
};

ttuple<v2i16*, i32> Build_Path(
    Arena&            trash_arena,
    v2i16             gsize,
    toptional<v2i16>* bfs_parents_mtx,
    v2i16             destination
) {
#if 0
    // NOTE: Двойной проход, но без RAM overhead-а на `trash_arena`.
    i32 path_max_count = 1;
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
    i32 path_max_count = Longest_Meaningful_Path(gsize);
#endif

    i32    path_count = 0;
    v2i16* path       = Allocate_Array(trash_arena, v2i16, path_max_count);

#ifdef SHIT_MEMORY_DEBUG
    memset(path, SHIT_BYTE_MASK, sizeof(v2i16) * path_max_count);
#endif  // SHIT_MEMORY_DEBUG

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

    Path_Find_Result        result = {};
    Fixed_Size_Queue<v2i16> queue  = {};
    queue.memory_size              = sizeof(v2i16) * tiles_count;
    queue.base = (v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);
    Enqueue(queue, source);

    bool* visited_mtx = Allocate_Zeros_Array(trash_arena, bool, tiles_count);
    GRID_PTR_VALUE(visited_mtx, source) = true;

    toptional<v2i16>* bfs_parents_mtx
        = Allocate_Array(trash_arena, toptional<v2i16>, tiles_count);

    FOR_RANGE (int, i, tiles_count) {
        std::construct_at(bfs_parents_mtx + i);
    }

    while (queue.count > 0) {
        auto pos = Dequeue(queue);
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

            Enqueue(queue, new_pos);
        }
    }

    result.success = false;
    return result;
}

Scriptable_Resource*
Get_Scriptable_Resource(Game_State& state, Scriptable_Resource_ID id) {
    Assert(id - 1 < state.scriptable_resources_count);
    auto exists     = id != 0;
    auto ptr_offset = (ptrd)(state.scriptable_resources + id - 1);
    auto result     = ptr_offset * exists;
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

void Place_Building(Game_State& state, v2i16 pos, Scriptable_Building* scriptable, MCTX) {
    auto& game_map = state.game_map;
    auto  gsize    = game_map.size;
    Assert(Pos_Is_In_Bounds(pos, gsize));

    auto [_, found_instance] = Find_And_Occupy_Empty_Slot(game_map.buildings, ctx);
    auto& instance           = *found_instance;

    instance.pos                          = pos;
    instance.scriptable                   = scriptable;
    instance.time_since_human_was_created = f32_inf;

    auto& tile = *(game_map.element_tiles + gsize.x * pos.y + pos.x);
    Assert(tile.type == Element_Tile_Type::None);
    tile.type     = Element_Tile_Type::Building;
    tile.building = found_instance;
}

template <typename T>
void Init_Bucket_Array(
    Bucket_Array<T>& container,
    u32              buckets_count,
    u32              items_per_bucket,
    MCTX
) {
    container.allocator      = ctx->allocator;
    container.allocator_data = ctx->allocator_data;

    container.items_per_bucket = items_per_bucket;
    container.buckets_count    = buckets_count;

    container.buckets              = nullptr;
    container.unfull_buckets       = nullptr;
    container.count                = 0;
    container.used_buckets_count   = 0;
    container.unfull_buckets_count = 0;
}

template <typename T>
void Set_Container_Allocator_Context(T& container, MCTX) {
    container.allocator      = ctx->allocator;
    container.allocator_data = ctx->allocator_data;
}

template <typename T>
void Deinit_Bucket_Array(Bucket_Array<T>& container, MCTX) {
    CONTAINER_ALLOCATOR;

    for (auto bucket_ptr = container.buckets;
         bucket_ptr != container.buckets + container.used_buckets_count;
         bucket_ptr++  //
    ) {
        auto& bucket = *bucket_ptr;
        FREE((u8*)bucket.occupied, container.items_per_bucket / 8);
        FREE((T*)bucket.data, container.items_per_bucket);
    }

    FREE(container.buckets, container.buckets_count);
    FREE(container.unfull_buckets, container.buckets_count);
}

template <typename T>
void Init_Queue(Queue<T>& container, MCTX) {
    container.count     = 0;
    container.max_count = 0;
    container.base      = nullptr;

    container.allocator      = ctx->allocator;
    container.allocator_data = ctx->allocator_data;
}

template <typename T>
void Init_Grid_Of_Queues(Grid_Of_Queues<T>& container, const v2i16 gsize, MCTX) {
    CTX_ALLOCATOR;

    const auto tiles_count = gsize.x * gsize.y;

    container.counts     = (u32*)ALLOC(sizeof(u32) * tiles_count);
    container.max_counts = (i32*)ALLOC(sizeof(i32) * tiles_count);
    container.bases      = (T*)ALLOC(sizeof(T) * tiles_count);
    memset(container.counts, 0, sizeof(u32) * tiles_count);
    memset(container.max_counts, 0, sizeof(i32) * tiles_count);
    memset(container.bases, 0, sizeof(T) * tiles_count);

    container.allocator      = ctx->allocator;
    container.allocator_data = ctx->allocator_data;
}

template <typename T>
void Init_Vector(Vector<T>& container, MCTX) {
    container.count     = 0;
    container.max_count = 0;
    container.base      = nullptr;

    container.allocator      = ctx->allocator;
    container.allocator_data = ctx->allocator_data;
}

template <typename T>
void Init_Grid_Of_Vectors(Grid_Of_Vectors<T>& container, const v2i16 gsize, MCTX) {
    CTX_ALLOCATOR;

    const auto tiles_count = gsize.x * gsize.y;

    container.counts     = (i32*)ALLOC(sizeof(i32) * tiles_count);
    container.max_counts = (u32*)ALLOC(sizeof(u32) * tiles_count);
    container.bases      = (T**)ALLOC(sizeof(T*) * tiles_count);
    memset(container.counts, 0, sizeof(u32) * tiles_count);
    memset(container.max_counts, 0, sizeof(i32) * tiles_count);
    memset(container.bases, 0, sizeof(T) * tiles_count);

    container.allocator      = ctx->allocator;
    container.allocator_data = ctx->allocator_data;
}

template <typename T>
void Deinit_Queue(Queue<T>& container, MCTX) {
    CONTAINER_ALLOCATOR;

    if (container.base != nullptr) {
        Assert(container.max_count > 0);
        FREE(container.base, container.max_count);
        container.base = nullptr;
    }

    container.count     = 0;
    container.max_count = 0;
}

template <typename T>
void Deinit_Grid_Of_Queues(Grid_Of_Queues<T>& container, const v2i16 gsize, MCTX) {
    CTX_ALLOCATOR;

    const auto tiles_count = gsize.x * gsize.y;

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            T*  base      = *Get_By_Stride(container.bases, {x, y}, gsize.x);
            u32 max_count = *Get_By_Stride(container.max_counts, {x, y}, gsize.x);

            if (max_count != 0) {
                Assert(base != nullptr);
            }

            if (base != nullptr) {
                Assert(max_count > 0);
                FREE(base, sizeof(T) * max_count);
            }
        }
    }

    if (container.counts != nullptr)
        FREE(container.counts, sizeof(i32) * tiles_count);
    if (container.max_counts != nullptr)
        FREE(container.max_counts, sizeof(u32) * tiles_count);
    if (container.bases != nullptr)
        FREE(container.bases, sizeof(T*) * tiles_count);
}

template <typename T>
void Deinit_Grid_Of_Vectors(Grid_Of_Vectors<T>& container, const v2i16 gsize, MCTX) {
    CTX_ALLOCATOR;

    const auto tiles_count = gsize.x * gsize.y;

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            T*  base      = *Get_By_Stride(container.bases, {x, y}, gsize.x);
            u32 max_count = *Get_By_Stride(container.max_counts, {x, y}, gsize.x);

            if (max_count != 0) {
                Assert(base != nullptr);
            }

            if (base != nullptr) {
                Assert(max_count > 0);
                FREE(base, sizeof(T) * max_count);
            }
        }
    }

    if (container.counts != nullptr)
        FREE(container.counts, sizeof(i32) * tiles_count);
    if (container.max_counts != nullptr)
        FREE(container.max_counts, sizeof(u32) * tiles_count);
    if (container.bases != nullptr)
        FREE(container.bases, sizeof(T*) * tiles_count);
}

template <typename T>
void Deinit_Vector(Vector<T>& container, MCTX) {
    CONTAINER_ALLOCATOR;

    if (container.base != nullptr) {
        Assert(container.max_count > 0);
        FREE(container.base, container.max_count);
        container.base = nullptr;
    }
    container.count     = 0;
    container.max_count = 0;
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

struct Human_Data : public Non_Copyable {
    Player_ID  max_player_id;
    Building** city_halls;
    Game_Map*  game_map;
    Arena*     trash_arena;

    Human_Data(
        Player_ID  a_max_player_id,
        Building** a_city_halls,
        Game_Map*  a_game_map,
        Arena*     a_trash_arena
    )
        : max_player_id(a_max_player_id)
        , city_halls(a_city_halls)
        , game_map(a_game_map)
        , trash_arena(a_trash_arena) {}
};

void Root_Set_Human_State(
    Human&            human,
    Human_Main_State  new_state,
    const Human_Data& data,
    MCTX
);

void Advance_Moving_To(Human_Moving_Component& moving, MCTX) {
    if (moving.path.count == 0) {
        moving.elapsed  = 0;
        moving.progress = 0;
        moving.to.reset();
    }
    else {
        moving.to = Dequeue(moving.path);
    }
}

void Human_Moving_Component_Add_Path(
    Human_Moving_Component& moving,
    v2i16*                  path,
    i32                     path_count,
    MCTX
) {
    Container_Reset(moving.path);

    if (path_count != 0) {
        Assert(path != nullptr);

        if (moving.to.value_or(moving.pos) == path[0]) {
            path++;
            path_count--;
        }
        Bulk_Enqueue(moving.path, path, path_count, ctx);
    }

    if (!moving.to.has_value())
        Advance_Moving_To(moving, ctx);
}

struct Human_Moving_In_The_World_Controller {
    static void On_Enter(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        if (human.segment != nullptr) {
            // TODO: After implementing resources.
            // LOG_DEBUG(
            //     "human.segment.resources_to_transport.size() = {}",
            //     human.segment.resources_to_transport.size()
            // );
        }

        Container_Reset(human.moving.path);

        Update_States(human, data, nullptr, nullptr, ctx);
    }

    static void On_Exit(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        human.state_moving_in_the_world = Moving_In_The_World_State::None;
        human.moving.to.reset();
        Container_Reset(human.moving.path);

        if (human.type == Human_Type::Employee) {
            Assert(human.building != nullptr);
            human.building->employee_is_inside = true;
            // C# TODO: Somehow remove this human.
        }
    }

    static void Update(Human& human, const Human_Data& data, f32 dt, MCTX) {
        Update_States(human, data, human.segment, human.building, ctx);
    }

    static void On_Human_Current_Segment_Changed(
        Human&            human,
        const Human_Data& data,
        Graph_Segment*    old_segment,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        Assert(human.type == Human_Type::Transporter);
        Update_States(human, data, old_segment, nullptr, ctx);
    }

    static void
    On_Human_Moved_To_The_Next_Tile(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;
        LOG_DEBUG("human.moving {}.{}", human.moving.pos.x, human.moving.pos.y);

        if (human.type == Human_Type::Constructor       //
            && human.building != nullptr                //
            && human.moving.pos == human.building->pos  //
        ) {
            Root_Set_Human_State(human, Human_Main_State::Building, data, ctx);
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
        Human&            human,
        const Human_Data& data,
        Graph_Segment*    old_segment,
        Building*         old_building,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        auto& game_map = *data.game_map;

        if (human.segment != nullptr) {
            auto& segment = *human.segment;
            Assert(human.type == Human_Type::Transporter);

            // NOTE: Следующая клетка, на которую перейдёт (или уже находится) чувак,
            // - это клетка его сегмента. Нам уже не нужно помнить его путь.
            if (human.moving.to.has_value()
                && Graph_Contains(segment.graph, human.moving.to.value())
                && Graph_Node(segment.graph, human.moving.to.value()) != 0  //
            ) {
                Container_Reset(human.moving.path);
                return;
            }

            // NOTE: Чувак перешёл на клетку сегмента. Переходим на Moving_Inside_Segment.
            if (!(human.moving.to.has_value())                       //
                && Graph_Contains(segment.graph, human.moving.pos)   //
                && Graph_Node(segment.graph, human.moving.pos) != 0  //
            ) {
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
            if (old_segment != human.segment
                || human.state_moving_in_the_world != moving_to_destination  //
            ) {
                LOG_DEBUG(
                    "Setting human.state_moving_in_the_world = "
                    "Moving_In_The_World_State::Moving_To_Destination"
                );
                human.state_moving_in_the_world
                    = Moving_In_The_World_State::Moving_To_Destination;

                Assert(data.trash_arena != nullptr);

                auto& game_map = *data.game_map;

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
        else if (human.building != nullptr) {
            auto& building = *human.building;

            auto is_constructor_or_employee = human.type == Human_Type::Constructor
                                              || human.type == Human_Type::Employee;
            Assert(is_constructor_or_employee);

            if (human.type == Human_Type::Constructor) {
                // TODO: Assert_False(building.is_constructed);
            }
            else if (human.type == Human_Type::Employee) {
                // TODO: Assert(building.is_constructed);
            }

            if (old_building != human.building) {
                Assert(data.trash_arena != nullptr);
                auto& game_map = *data.game_map;

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
        else if (
            human.state_moving_in_the_world
            != Moving_In_The_World_State::Moving_To_The_City_Hall  //
        ) {
            LOG_DEBUG(
                "human.state_moving_in_the_world = "
                "Moving_In_The_World_State::Moving_To_The_City_Hall"
            );
            human.state_moving_in_the_world
                = Moving_In_The_World_State::Moving_To_The_City_Hall;

            Building& city_hall = Assert_Deref(*(data.city_halls + human.player_id));

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

        Assert(human.segment != nullptr);
        Assert(!human.moving.to.has_value());
        Assert(human.moving.path.count == 0);

        auto& game_map = *data.game_map;

        if (human.segment->resources_to_transport.count == 0) {
            TEMP_USAGE(*data.trash_arena);
            LOG_DEBUG("Calculating path to the center of the segment");

            auto [success, path, path_count] = Find_Path(
                *data.trash_arena,
                game_map.size,
                game_map.terrain_tiles,
                game_map.element_tiles,
                human.moving.to.value_or(human.moving.pos),
                Assert_Deref(Assert_Deref(human.segment).graph.data).center,
                true
            );

            Assert(success);

            Human_Moving_Component_Add_Path(human.moving, path, path_count, ctx);
        }
    }

    static void On_Exit(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        Container_Reset(human.moving.path);
    }

    static void Update(Human& human, const Human_Data& data, f32 dt, MCTX) {
        Update_States(human, data, nullptr, nullptr, ctx);
    }

    static void On_Human_Current_Segment_Changed(
        Human&            human,
        const Human_Data& data,
        Graph_Segment*    old_segment,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        // Tracing.Log("_controller.SetState(human, HumanState.MovingInTheWorld)");
        Root_Set_Human_State(human, Human_Main_State::Moving_In_The_World, data, ctx);
    }

    static void
    On_Human_Moved_To_The_Next_Tile(Human& human, const Human_Data& data, MCTX) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        // NOTE: Intentionally left blank.
    }

    static void Update_States(
        Human&            human,
        const Human_Data& data,
        Graph_Segment*    old_segment,
        Building*         old_building,
        MCTX
    ) {
        CTX_LOGGER;
        LOG_TRACING_SCOPE;

        if (human.segment == nullptr) {
            Container_Reset(human.moving.path);
            Root_Set_Human_State(human, Human_Main_State::Moving_In_The_World, data, ctx);
            return;
        }

        if (human.segment->resources_to_transport.count > 0) {
            if (!human.moving.to.has_value()) {
                // TODO:
                // Tracing.Log("_controller.SetState(human, HumanState.MovingItem)");
                Root_Set_Human_State(human, Human_Main_State::Moving_Resource, data, ctx);
                return;
            }

            Container_Reset(human.moving.path);
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
        Graph_Segment*    old_segment,
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
        Graph_Segment*    old_segment,
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
        Graph_Segment*    old_segment,
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
        Graph_Segment*    old_segment,
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
        Graph_Segment*    old_segment,
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
        Graph_Segment*    old_segment,
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
    Human&            human,
    const Human_Data& data,
    Graph_Segment*    old_segment,
    MCTX
) {
    switch (human.state) {
    case Human_Main_State::Moving_In_The_World:
        Human_Moving_In_The_World_Controller::On_Human_Current_Segment_Changed(
            human, data, old_segment, ctx
        );
        break;

    case Human_Main_State::Moving_Inside_Segment:
        Human_Moving_Inside_Segment::On_Human_Current_Segment_Changed(
            human, data, old_segment, ctx
        );
        break;

    case Human_Main_State::Moving_Resource:
        Human_Moving_Resources::On_Human_Current_Segment_Changed(
            human, data, old_segment, ctx
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
Human* Create_Human_Transporter(
    Game_Map&         game_map,
    Building*         building,
    Graph_Segment*    segment_ptr,
    const Human_Data& data,
    MCTX
) {
    CTX_LOGGER;
    LOG_TRACING_SCOPE;
    LOG_DEBUG("Creating a new human...");

    auto& segment = Assert_Deref(segment_ptr);

    auto [locator, human_ptr] = Find_And_Occupy_Empty_Slot(game_map.humans_to_add, ctx);
    auto& human               = *human_ptr;
    human.player_id           = 0;
    human.moving.pos          = building->pos;
    human.moving.elapsed      = 0;
    human.moving.progress     = 0;
    human.moving.from         = building->pos;
    human.moving.to.reset();
    human.moving.path.count     = 0;
    human.moving.path.max_count = 0;
    human.moving.path.base      = nullptr;
    Set_Container_Allocator_Context(human.moving.path, ctx);

    human.segment                   = segment_ptr;
    human.type                      = Human_Type::Transporter;
    human.state                     = Human_Main_State::None;
    human.state_moving_in_the_world = Moving_In_The_World_State::None;
    human.building                  = nullptr;

    Root_Set_Human_State(human, Human_Main_State::Moving_In_The_World, data, ctx);

    // TODO:
    // onHumanCreated.OnNext(new() { human = human });
    //
    // if (building.scriptable.type == BuildingType.SpecialCityHall) {
    //     onCityHallCreatedHuman.OnNext(new() { cityHall = building });
    //     DomainEvents<E_CityHallCreatedHuman>.Publish(new() { cityHall = building });
    // }

    // NOTE: Привязка будет при переносе из humans_to_add в humans.
    segment.assigned_human = nullptr;

    return human_ptr;
}

void Update_Building__Constructed(
    Game_Map&         game_map,
    Building*         building,
    f32               dt,
    const Human_Data& data,
    MCTX
) {
    auto& scriptable = *building->scriptable;

    auto delay = scriptable.human_spawning_delay;

    if (scriptable.type == Building_Type::City_Hall) {
        auto& since_created = building->time_since_human_was_created;
        since_created += dt;
        if (since_created > delay)
            since_created = delay;

        if (game_map.segments_wo_humans.count > 0) {
            if (since_created >= delay) {
                since_created -= delay;
                Graph_Segment* segment = Dequeue(game_map.segments_wo_humans);
                Create_Human_Transporter(game_map, building, segment, data, ctx);
            }
        }
    }

    // TODO: _building_controller.Update(building, dt);
}

void Update_Buildings(Game_State& state, f32 dt, const Human_Data& data, MCTX) {
    for (auto building_ptr : Iter(&state.game_map.buildings)) {
        // TODO: if (building.constructionProgress < 1) {
        //     Update_Building__Not_Constructed(building, dt);
        // } else {
        Update_Building__Constructed(state.game_map, building_ptr, dt, data, ctx);
        // }
    }
}

void Remove_Humans(Game_State& state) {
    auto& game_map = state.game_map;

    for (auto pptr : Iter(&game_map.humans_to_remove)) {
        auto& [reason, human_ptr, locator_in_humans_array] = *pptr;
        Human& human                                       = Assert_Deref(human_ptr);

        if (reason == Human_Removal_Reason::Transporter_Returned_To_City_Hall) {
            // TODO: on_Human_Reached_City_Hall.On_Next(new (){human = human});
        }
        else if (reason == Human_Removal_Reason::Employee_Reached_Building) {
            // TODO: on_Employee_Reached_Building.On_Next(new (){human = human});
            Assert(human.building != nullptr);
            human.building->employee = nullptr;
        }

        Bucket_Array_Remove(game_map.humans, locator_in_humans_array);
    }

    Container_Reset(game_map.humans_to_remove);
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

    constexpr int _GUARD_MAX_MOVING_TILES_PER_FRAME = 4;

    auto iteration = 0;
    while (iteration < 10 * _GUARD_MAX_MOVING_TILES_PER_FRAME  //
           && moving.to.has_value()                            //
           && moving.elapsed > duration                        //
    ) {
        LOG_TRACING_SCOPE;

        iteration++;

        moving.elapsed -= duration;

        moving.pos  = moving.to.value();
        moving.from = moving.pos;
        Advance_Moving_To(moving, ctx);

        Root_On_Human_Moved_To_The_Next_Tile(human, data, ctx);
        // TODO: on_Human_Moved_To_The_Next_Tile.On_Next(new (){ human = human });
    }

    Assert(iteration < 10 * _GUARD_MAX_MOVING_TILES_PER_FRAME);
    if (iteration >= _GUARD_MAX_MOVING_TILES_PER_FRAME) {
        LOG_TRACING_SCOPE;
        LOG_WARN("WTF? iteration >= _GUARD_MAX_MOVING_TILES_PER_FRAME");
    }

    moving.progress = MIN(1.0f, moving.elapsed / duration);
    SANITIZE;
}

void Update_Human(
    Game_Map&         game_map,
    Human*            human_ptr,
    Bucket_Locator    locator,
    float             dt,
    Building**        city_halls,
    const Human_Data& data,
    MCTX
) {
    CTX_ALLOCATOR;

    auto& human            = *human_ptr;
    auto& humans_to_remove = game_map.humans_to_remove;

    if (human.moving.to.has_value())
        Update_Human_Moving_Component(game_map, human, dt, data, ctx);

    if (humans_to_remove.size() > 0  //
        && humans_to_remove[humans_to_remove.size() - 1].human == human_ptr)
        return;

    Human_Root_Update(human, data, dt, ctx);

    auto state = Moving_In_The_World_State::Moving_To_The_City_Hall;
    if (human.state_moving_in_the_world == state                       //
        && human.moving.pos == (*(city_halls + human.player_id))->pos  //
        && !human.moving.to.has_value()                                //
    ) {
        // TODO: auto rem = Game_Map::Human_To_Remove{
        //     Human_Removal_Reason::Transporter_Returned_To_City_Hall, human_ptr, locator
        // };
        // Vector_Add(humans_to_remove, std::move(rem));
        humans_to_remove.push_back(
            {Human_Removal_Reason::Transporter_Returned_To_City_Hall, human_ptr, locator}
        );
        // Vector_Add(
        //     humans_to_remove,
        //     {Human_Removal_Reason::Transporter_Returned_To_City_Hall, human_ptr,
        //     locator}
        // );
    }

    SANITIZE;
}

void Update_Humans(Game_State& state, f32 dt, const Human_Data& data, MCTX) {
    CTX_LOGGER;

    auto& game_map   = state.game_map;
    auto  city_halls = data.city_halls;

    for (auto abobe : Iter(&game_map.humans_to_remove)) {
        auto [reason, _, _2] = *abobe;
        Assert(reason != Human_Removal_Reason::Transporter_Returned_To_City_Hall);
    }

    Remove_Humans(state);

    for (auto [locator, human_ptr] : Iter_With_Locator(&game_map.humans)) {
        Update_Human(game_map, human_ptr, locator, dt, city_halls, data, ctx);
    }

    auto prev_count = game_map.humans_to_add.count;
    for (auto human_to_move : Iter(&game_map.humans_to_add)) {
        LOG_DEBUG("Update_Humans: moving human from humans_to_add to humans");
        auto [locator, human_ptr] = Find_And_Occupy_Empty_Slot(game_map.humans, ctx);

        *human_ptr = std::move(*human_to_move);
        if (human_ptr->segment != nullptr)
            human_ptr->segment->assigned_human = human_ptr;

        Update_Human(game_map, human_ptr, locator, dt, city_halls, data, ctx);
    }

    Assert(prev_count == game_map.humans_to_add.count);
    Container_Reset(game_map.humans_to_add);

    Remove_Humans(state);

    {  // NOTE: Debug shiet.
        int humans                         = 0;
        int humans_moving_to_the_city_hall = 0;
        int humans_moving_to_destination   = 0;
        int humans_moving_inside_segment   = 0;
        for (auto human_ptr : Iter(&game_map.humans)) {
            humans++;
            auto& human = *human_ptr;

            if (human.state == Human_Main_State::Moving_In_The_World) {
                if (human.state_moving_in_the_world
                    == Moving_In_The_World_State::Moving_To_The_City_Hall) {
                    humans_moving_to_the_city_hall++;
                }
                else if (human.state_moving_in_the_world //
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

    ImGui::Text("last_segments_to_delete_count %d", global_last_segments_to_delete_count);
    ImGui::Text("last_segments_to_add_count %d", global_last_segments_to_add_count);
    ImGui::Text("game_map.segments.count %d", game_map.segments.count);

    Update_Buildings(state, dt, Assert_Deref(state.game_map.human_data), ctx);
    Update_Humans(state, dt, Assert_Deref(state.game_map.human_data), ctx);
}

void Init_Game_Map(Game_State& state, Arena& arena, MCTX) {
    auto& game_map = state.game_map;

    game_map.data = std::construct_at(Allocate_For(arena, Game_Map_Data), 0.3f);

    const auto tiles_count = game_map.size.x * game_map.size.y;

    Init_Bucket_Array(game_map.buildings, 32, 128, ctx);
    Init_Bucket_Array(game_map.segments, 32, 128, ctx);
    Init_Bucket_Array(game_map.humans, 32, 128, ctx);
    Init_Bucket_Array(game_map.humans_to_add, 32, 128, ctx);

    // TODO: переписать это на свой Vector
    std::construct_at(&game_map.humans_to_remove);

    Init_Grid_Of_Vectors(game_map.resources, game_map.size, ctx);

    {
        auto& container     = game_map.segments_wo_humans;
        container.count     = 0;
        container.max_count = 0;
        container.base      = nullptr;
    }

    Place_Building(state, {2, 2}, state.scriptable_building_city_hall, ctx);

    {
        const int  players_count    = 1;
        int        city_halls_count = 0;
        Building** city_halls       = Allocate_Array(arena, Building*, players_count);

        {  // NOTE: Доставание всех City Hall.
            for (auto building_ptr : Iter(&game_map.buildings)) {
                auto& building = *building_ptr;
                if (building.scriptable->type == Building_Type::City_Hall)
                    Array_Push(city_halls, city_halls_count, players_count, building_ptr);

                if (city_halls_count == players_count)
                    break;
            }
            Assert(city_halls_count == players_count);
        }

        game_map.human_data = std::construct_at(
            Allocate_For(arena, Human_Data),
            players_count,
            city_halls,
            &state.game_map,
            &state.trash_arena
        );
    }
}

void Deinit_Game_Map(Game_State& state, MCTX) {
    CTX_ALLOCATOR;
    auto& game_map = state.game_map;

    Deinit_Bucket_Array(game_map.buildings, ctx);

    for (auto segment_ptr : Iter(&game_map.segments)) {
        FREE(segment_ptr->vertices, segment_ptr->vertices_count);
        FREE(segment_ptr->graph.nodes, segment_ptr->graph.nodes_allocation_count);

        segment_ptr->linked_segments.clear();
        Deinit_Queue(segment_ptr->resources_to_transport, ctx);

        auto& segment = *segment_ptr;
        Assert(segment.graph.nodes != nullptr);
        Assert(segment.graph.data != nullptr);

        auto& data = *segment.graph.data;
        Assert(data.dist != nullptr);
        Assert(data.prev != nullptr);

        data.node_index_2_pos.clear();
        data.pos_2_node_index.clear();

        auto n = segment.graph.nodes_count;
        FREE(data.dist, n * n);
        FREE(data.prev, n * n);
    }

    Deinit_Bucket_Array(game_map.segments, ctx);
    Deinit_Bucket_Array(game_map.humans, ctx);
    Deinit_Bucket_Array(game_map.humans_to_add, ctx);

    // TODO: деинициалиация game_map.humans_to_remove

    Deinit_Grid_Of_Vectors(game_map.resources, game_map.size, ctx);

    game_map.humans_to_remove.clear();
    Deinit_Queue(game_map.segments_wo_humans, ctx);

    for (auto human_ptr : Iter(&game_map.humans)) {
        auto& human = *human_ptr;
        Deinit_Queue(human.moving.path, ctx);
    }
}

void Regenerate_Terrain_Tiles(
    Game_State&  state,
    Game_Map&    game_map,
    Arena&       arena,
    Arena&       trash_arena,
    uint         seed,
    Editor_Data& data,
    MCTX
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
            auto noise   = *(terrain_perlin + noise_pitch * y + x) / (f32)u16_max;
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

            resource.scriptable_id = global_forest_resource_id * generate;
            resource.amount        = data.forest_max_amount * generate;
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
    Game_State&  state,
    Game_Map&    game_map,
    Arena&       arena,
    Arena&       trash_arena,
    uint         seed,
    Editor_Data& data,
    MCTX
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
        auto          o    = offset + base_offset;
        Element_Tile& tile = *(game_map.element_tiles + o.y * gsize.x + o.x);

        tile.type = Element_Tile_Type::Road;
        Assert(tile.building == nullptr);
    }

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            Element_Tile& tile = *(game_map.element_tiles + y * gsize.x + x);
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
            auto  graph_pos = pos;
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
    }

// v2i16* Allocate_Segment_Vertices(Allocator& allocator, int vertices_count) {
//     return (v2i16*)allocator.Allocate(sizeof(v2i16) * vertices_count, 1);
// }
//
// u8* Allocate_Graph_Nodes(Allocator& allocator, int nodes_count) {
//     return allocator.Allocate(nodes_count, 1);
// }

void Rect_Copy(u8* dest, u8* source, int stride, int rows, int bytes_per_line) {
    FOR_RANGE (int, i, rows) {
        memcpy(dest + i * bytes_per_line, source + i * stride, bytes_per_line);
    }
}

bool Adjacent_Tiles_Are_Connected(Graph& graph, i16 x, i16 y) {
    auto gx   = graph.size.x;
    auto gy   = graph.size.y;
    u8   node = *(graph.nodes + y * gx + x);

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
                    && (ij > ik + kj)   //
                ) {
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

Graph_Segment* Add_And_Link_Segment(
    Bucket_Array<Graph_Segment>& segments,
    Graph_Segment&               added_segment,
    Arena&                       trash_arena,
    MCTX
) {
    CTX_ALLOCATOR;

    auto [locator, segment1_ptr] = Find_And_Occupy_Empty_Slot(segments, ctx);

    // NOTE: Создание финального Graph_Segment,
    // который будет использоваться в игровой логике.
    {
#ifdef SHIT_MEMORY_DEBUG
        memset(segment1_ptr, SHIT_BYTE_MASK, sizeof(Graph_Segment));
#endif  // SHIT_MEMORY_DEBUG

        auto& segment                        = *segment1_ptr;
        segment.vertices_count               = added_segment.vertices_count;
        segment.vertices                     = added_segment.vertices;
        segment.graph.nodes_count            = added_segment.graph.nodes_count;
        segment.graph.nodes_allocation_count = added_segment.graph.nodes_allocation_count;
        segment.graph.nodes                  = added_segment.graph.nodes;
        segment.graph.size                   = added_segment.graph.size;
        segment.graph.offset                 = added_segment.graph.offset;

        Calculate_Graph_Data(segment.graph, trash_arena, ctx);

        segment.locator        = locator;
        segment.assigned_human = nullptr;

        std::construct_at(&segment.linked_segments);

        segment.resources_to_transport.max_count = 0;
        segment.resources_to_transport.count     = 0;
        segment.resources_to_transport.base      = nullptr;
    }

    for (auto segment2_ptr : Iter(&segments)) {
        if (segment2_ptr == segment1_ptr)
            continue;

        // PERF: Мб тут стоит что-то из разряда
        // AABB(graph1, graph2) для оптимизации заюзать.
        auto& segment1 = *segment1_ptr;
        auto& segment2 = *segment2_ptr;
        if (Have_Some_Of_The_Same_Vertices(segment1, segment2)) {
            if (Vector_Find(segment2.linked_segments, segment1_ptr) == -1)
                segment2.linked_segments.push_back(segment1_ptr);

            if (Vector_Find(segment1.linked_segments, segment2_ptr) == -1)
                segment1.linked_segments.push_back(segment2_ptr);
        }
    }

    SANITIZE;

    return segment1_ptr;
}

BF_FORCE_INLINE void Update_Segments_Function(
    Arena&          trash_arena,
    Game_Map&       game_map,
    u32             segments_to_delete_count,
    Graph_Segment** segments_to_delete,
    u32             segments_to_add_count,
    Graph_Segment*  segments_to_add,
    MCTX
) {
    CTX_ALLOCATOR;
    CTX_LOGGER;
    LOG_TRACING_SCOPE;
    LOG_DEBUG("game_map.segments.count = {}", game_map.segments.count);
    LOG_DEBUG("segments_to_delete_count = {}", segments_to_delete_count);
    LOG_DEBUG("segments_to_add_count = {}", segments_to_add_count);

    TEMP_USAGE(trash_arena);

    auto& segments = game_map.segments;

    global_last_segments_to_delete_count = segments_to_delete_count;
    global_last_segments_to_add_count    = segments_to_add_count;

    // NOTE: Подсчёт максимального количества чувачков, которые были бы без сегментов
    // (с учётом тех, которые уже не имеют сегмент).
    // PERF: можем кешировать количество чувачков,
    // которые уже не имеют сегмента и идут в ратушу.
    auto humans_moving_to_city_hall = 0;
    for (auto human_ptr : Iter(&game_map.humans)) {
        auto state = Moving_In_The_World_State::Moving_To_The_City_Hall;
        if (human_ptr->state_moving_in_the_world == state)
            humans_moving_to_city_hall++;
    }

    // PERF: Мб стоит переделать так, чтобы мы лишнего не аллоцировали заранее.
    i32 humans_wo_segment_count = 0;
    // NOTE: `Graph_Segment*` is nullable, `Human*` is not.
    using tttt = ttuple<Graph_Segment*, Human*>;
    const i32 humans_wo_segment_max_count
        = segments_to_delete_count + humans_moving_to_city_hall;

    // NOTE: Настекиваем чувачков без сегментов (которые идут в ратушу).
    tttt* humans_wo_segment = nullptr;
    if (humans_wo_segment_max_count > 0) {
        humans_wo_segment
            = Allocate_Array(trash_arena, tttt, humans_wo_segment_max_count);

        for (auto human_ptr : Iter(&game_map.humans)) {
            auto state = Moving_In_The_World_State::Moving_To_The_City_Hall;
            if (human_ptr->state_moving_in_the_world == state) {
                Array_Push(
                    humans_wo_segment,
                    humans_wo_segment_count,
                    humans_wo_segment_max_count,
                    tttt(nullptr, human_ptr)
                );
            }
        }
    }

    // NOTE: Удаление сегментов (отвязка от чувачков,
    // от других сегментов и высвобождение памяти).
    FOR_RANGE (u32, i, segments_to_delete_count) {
        Graph_Segment* segment_ptr = segments_to_delete[i];
        auto&          segment     = *segment_ptr;

        // NOTE: Настекиваем чувачков без сегментов (сегменты которых удалили только что).
        auto human_ptr = segment.assigned_human;
        if (human_ptr != nullptr) {
            human_ptr->segment     = nullptr;
            segment.assigned_human = nullptr;
            Array_Push(
                humans_wo_segment,
                humans_wo_segment_count,
                humans_wo_segment_max_count,
                tttt(segment_ptr, human_ptr)
            );
        }

        // NOTE: Отвязываем сегмент от других сегментов.
        for (auto linked_segment_pptr : Iter(&segment.linked_segments)) {
            Graph_Segment& linked_segment
                = Assert_Deref(Assert_Deref(linked_segment_pptr));

            auto found_index = Vector_Find(linked_segment.linked_segments, segment_ptr);
            if (found_index != -1) {
                Vector_Unordered_Remove_At(linked_segment.linked_segments, found_index);
            }
        }

        // TODO: _resource_transportation.OnSegmentDeleted(segment);

        // NOTE: Удаляем сегмент из очереди сегментов на добавление чувачков,
        // если этот сегмент ранее был в неё добавлен.
        // PERF: Много memmove происходит.
        auto& queue = game_map.segments_wo_humans;
        auto  index = Queue_Find(queue, segment_ptr);
        if (index != -1)
            Queue_Remove_At(queue, index);

        // NOTE: Уничтожаем сегмент.
        Bucket_Array_Remove(segments, segment.locator);
        FREE(segment.vertices, segment.vertices_count);
        FREE(segment.graph.nodes, segment.graph.nodes_allocation_count);
    }

    // NOTE: Вносим созданные сегменты. Если будут свободные чувачки - назначим им.
    Graph_Segment** added_calculated_segments = nullptr;
    if (segments_to_add_count > 0) {
        added_calculated_segments
            = Allocate_Array(trash_arena, Graph_Segment*, segments_to_add_count);
    }

    FOR_RANGE (u32, i, segments_to_add_count) {
        *(added_calculated_segments + i) = Add_And_Link_Segment(
            game_map.segments, *(segments_to_add + i), trash_arena, ctx
        );
    }

    // TODO: _resourceTransportation.PathfindItemsInQueue();
    // Tracing.Log("_itemTransportationSystem.PathfindItemsInQueue()");

    Assert(game_map.human_data != nullptr);

    // NOTE: По возможности назначаем чувачков на старые сегменты без них.
    while (humans_wo_segment_count > 0 && game_map.segments_wo_humans.count > 0) {
        auto segment_ptr = Assert_Not_Null(Dequeue(game_map.segments_wo_humans));
        auto [old_segment, human_ptr]
            = Array_Pop(humans_wo_segment, humans_wo_segment_count);

        human_ptr->segment          = segment_ptr;
        segment_ptr->assigned_human = human_ptr;

        Root_On_Human_Current_Segment_Changed(
            *human_ptr, *game_map.human_data, old_segment, ctx
        );
    }

    // NOTE: По возможности назначаем чувачков на новые сегменты.
    // Если нет чувачков - сохраняем сегменты как те, которым нужны чувачки.
    FOR_RANGE (int, i, segments_to_add_count) {
        Graph_Segment* segment_ptr = Assert_Deref(added_calculated_segments + i);

        if (humans_wo_segment_count == 0) {
            Enqueue(game_map.segments_wo_humans, segment_ptr, ctx);
            continue;
        }

        auto [old_segment, human_ptr]
            = Array_Pop(humans_wo_segment, humans_wo_segment_count);

        human_ptr->segment          = segment_ptr;
        segment_ptr->assigned_human = human_ptr;

        Root_On_Human_Current_Segment_Changed(
            *human_ptr, *game_map.human_data, old_segment, ctx
        );
    }
}

typedef ttuple<Direction, v2i16> Dir_v2i16;

#define QUEUES_SCALE 4

void Update_Graphs(
    const v2i16                  gsize,
    const Element_Tile* const    element_tiles,
    Graph_Segment* const         added_segments,
    u32&                         added_segments_count,
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

        auto p = Dequeue(big_queue);
        Enqueue(queue, p);

        auto [_, p_pos] = p;
        if (full_graph_build) {
            GRID_PTR_VALUE(vis, p_pos) = true;
        }

        v2i16* vertices      = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);
        v2i16* segment_tiles = Allocate_Zeros_Array(trash_arena, v2i16, tiles_count);

        int vertices_count      = 0;
        int segment_tiles_count = 1;
        *(segment_tiles + 0)    = p_pos;

        Graph temp_graph  = {};
        temp_graph.nodes  = Allocate_Zeros_Array(trash_arena, u8, tiles_count);
        temp_graph.size.x = gsize.x;
        temp_graph.size.y = gsize.y;

        while (queue.count) {
            auto [dir, pos] = Dequeue(queue);
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
                    if (tile.building != new_tile.building)
                        continue;

                    if (!Graph_Node_Has(new_visited_value, opposite_dir_index)) {
                        new_visited_value = Graph_Node_Mark(
                            new_visited_value, opposite_dir_index, true
                        );
                        FOR_DIRECTION (new_dir_index) {
                            if (!Graph_Node_Has(new_visited_value, new_dir_index)) {
                                Enqueue(big_queue, {new_dir_index, new_pos});
                            }
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
                            Enqueue(big_queue, {dir_index, pos});
                        }
                    }
                }
            }
        }

        if (vertices_count <= 1)
            continue;

        // NOTE: Adding a new segment.
        Assert(temp_graph.nodes_count > 0);

        auto& segment          = *(added_segments + added_segments_count);
        segment.vertices_count = vertices_count;
        added_segments_count++;

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
    v2i16                        gsize,
    Element_Tile*                element_tiles,
    Bucket_Array<Graph_Segment>& segments,
    Arena&                       trash_arena,
    std::invocable<u32, Graph_Segment**, u32, Graph_Segment*, Context*> auto&&
        Update_Segments_Lambda,
    MCTX
) {
    CTX_ALLOCATOR;

    Assert(segments.used_buckets_count == 0);
    TEMP_USAGE(trash_arena);

    auto tiles_count = gsize.x * gsize.y;

    // NOTE: Создание новых сегментов.
    auto           segments_to_add_allocate = tiles_count * 4;
    u32            segments_to_add_count    = 0;
    Graph_Segment* segments_to_add
        = Allocate_Zeros_Array(trash_arena, Graph_Segment, segments_to_add_allocate);

    v2i16 pos   = -v2i16_one;
    bool  found = false;
    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile = *(element_tiles + y * gsize.x + x);

            if (tile.type == Element_Tile_Type::Flag
                || tile.type == Element_Tile_Type::Building) {
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

    Fixed_Size_Queue<Dir_v2i16> big_queue = {};
    big_queue.memory_size = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    big_queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, big_queue.memory_size);

    FOR_DIRECTION (dir) {
        Enqueue(big_queue, {dir, pos});
    }

    Fixed_Size_Queue<Dir_v2i16> queue = {};
    queue.memory_size                 = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);

    u8* visited = Allocate_Zeros_Array(trash_arena, u8, tiles_count);

    bool full_graph_build = true;

    Update_Graphs(
        gsize,
        element_tiles,
        segments_to_add,
        segments_to_add_count,
        big_queue,
        queue,
        trash_arena,
        visited,
        full_graph_build,
        ctx
    );

    Update_Segments_Lambda(0, nullptr, segments_to_add_count, segments_to_add, ctx);

    FOR_RANGE (u32, i, segments_to_add_count) {
        Add_And_Link_Segment(segments, *(segments_to_add + i), trash_arena, ctx);
    }

    SANITIZE;
}

ttuple<int, int> Update_Tiles(
    v2i16                        gsize,
    Element_Tile*                element_tiles,
    Bucket_Array<Graph_Segment>* segments,
    Arena&                       trash_arena,
    const Updated_Tiles&         updated_tiles,
    std::invocable<u32, Graph_Segment**, u32, Graph_Segment*, Context*> auto&&
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
    auto            segments_to_delete_allocate = updated_tiles.count * 4;
    u32             segments_to_delete_count    = 0;
    Graph_Segment** segments_to_delete
        = Allocate_Zeros_Array(trash_arena, Graph_Segment*, segments_to_delete_allocate);

    for (auto segment_ptr : Iter(segments)) {
        auto& segment = *segment_ptr;
        if (!Should_Segment_Be_Deleted(gsize, element_tiles, updated_tiles, segment))
            continue;

        Array_Push(
            segments_to_delete,
            segments_to_delete_count,
            segments_to_delete_allocate,
            segment_ptr
        );
    };

    // NOTE: Создание новых сегментов.
    auto           added_segments_allocate = updated_tiles.count * 4;
    u32            segments_to_add_count   = 0;
    Graph_Segment* segments_to_add
        = Allocate_Zeros_Array(trash_arena, Graph_Segment, added_segments_allocate);

    Fixed_Size_Queue<Dir_v2i16> big_queue = {};
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

                Enqueue(big_queue, {dir, pos});
            }
        } break;

        case Tile_Updated_Type::Flag_Placed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type == Element_Tile_Type::Road)
                    Enqueue(big_queue, {dir, pos});
            }
        } break;

        case Tile_Updated_Type::Flag_Removed: {
            FOR_DIRECTION (dir) {
                auto new_pos = pos + As_Offset(dir);
                if (!Pos_Is_In_Bounds(new_pos, gsize))
                    continue;

                auto& element_tile = GRID_PTR_VALUE(element_tiles, new_pos);
                if (element_tile.type != Element_Tile_Type::None)
                    Enqueue(big_queue, {dir, pos});
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
                    Enqueue(big_queue, {dir, new_pos});
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
                    Enqueue(big_queue, {dir, new_pos});
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
                        Enqueue(big_queue, {new_dir, new_pos});
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

    Fixed_Size_Queue<Dir_v2i16> queue = {};
    queue.memory_size                 = sizeof(Dir_v2i16) * tiles_count * QUEUES_SCALE;
    queue.base = (Dir_v2i16*)Allocate_Array(trash_arena, u8, queue.memory_size);

    bool full_graph_build = false;
    Update_Graphs(
        gsize,
        element_tiles,
        segments_to_add,
        segments_to_add_count,
        big_queue,
        queue,
        trash_arena,
        visited,
        full_graph_build,
        ctx
    );

    Update_Segments_Lambda(
        segments_to_delete_count,
        segments_to_delete,
        segments_to_add_count,
        segments_to_add,
        ctx
    );

    SANITIZE;

#ifdef ASSERT_SLOW
    for (auto segment1_ptr : Iter(segments)) {
        auto& segment1  = *segment1_ptr;
        auto& g1        = segment1.graph;
        v2i16 g1_offset = {g1.offset.x, g1.offset.y};

        for (auto segment2_ptr : Iter(segments)) {
            auto& segment2 = *segment2_ptr;
            if (segment1_ptr == segment2_ptr)
                continue;

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

    return {segments_to_add_count, segments_to_delete_count};
}

#define Declare_Updated_Tiles(variable_name_, pos_, type_) \
    Updated_Tiles(variable_name_) = {};                    \
    (variable_name_).count        = 1;                     \
    (variable_name_).pos          = &(pos_);               \
    auto type__                   = (type_);               \
    (variable_name_).type         = &type__;

#define INVOKE_UPDATE_TILES                           \
    Update_Tiles(                                     \
        state.game_map.size,                          \
        state.game_map.element_tiles,                 \
        &state.game_map.segments,                     \
        trash_arena,                                  \
        updated_tiles,                                \
        [&game_map, &trash_arena, &state](            \
            u32             segments_to_delete_count, \
            Graph_Segment** segments_to_delete,       \
            u32             segments_to_add_count,    \
            Graph_Segment*  segments_to_add,          \
            MCTX                                      \
        ) {                                           \
            Update_Segments_Function(                 \
                trash_arena,                          \
                game_map,                             \
                segments_to_delete_count,             \
                segments_to_delete,                   \
                segments_to_add_count,                \
                segments_to_add,                      \
                ctx                                   \
            );                                        \
        },                                            \
        ctx                                           \
    );

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

        Place_Building(state, pos, item.scriptable_building, ctx);

        Declare_Updated_Tiles(updated_tiles, pos, Tile_Updated_Type::Building_Placed);
        INVOKE_UPDATE_TILES;
    } break;

    default:
        INVALID_PATH;
    }

    INVOKE_OBSERVER(state.On_Item_Built, (state, pos, item));

    return true;
}
