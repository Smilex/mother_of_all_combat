enum class client_terrain_names {
    FOG = 0,
    GROUND,
    WATER,
    HILLS
};

enum client_screen_names {
    MAIN_MENU = 0,
    INITIALIZE_GAME,
    GAME
};

struct client_context {
    bool is_init;
    
    client_screen_names current_screen;
    memory_arena temp_mem, read_buffer;

    Music background_music;

    u32 my_server_id;
    bool my_turn;

    struct {
        u32 width, height;
        client_terrain_names *terrain;

        struct {
            v2<u32> *positions;
            u32 *server_ids;
            unit_names *constructions;
            s32 *owners;
            u32 used, max;
        } towns;

        struct {
            v2<u32> *positions;
            unit_names *names;
            u32 *server_ids;
            s32 *owners;
            u32 *action_points;
            u32 used, max;
        } units;
    } map;

    struct {
        Color *colors;
        u32 max, used;
    } clients;

    struct {
        u32 x, y;
    } camera;

    s32 selected_town_id,
        selected_unit_id;

    struct {
        struct {
            Rectangle rect;
            char *build_names[3];
            s32 build_active;
        } town;
        struct {
            Rectangle rect;
        } admin;
        struct {
            Rectangle rect;
        } end_turn;
    } gui;

    struct {
        doubly_linked_list<v2<u32>> highlighted_tiles;
        doubly_linked_list<u32> highlighted_priorities;
    } debug;
};

bool is_pt_in_gui(client_context *ctx, Vector2 pt) {
    if (ctx->selected_town_id != -1)
        if (CheckCollisionPointRec(pt, ctx->gui.town.rect))
            return true;

    if (CheckCollisionPointRec(pt, ctx->gui.admin.rect))
        return true;

    if (CheckCollisionPointRec(pt, ctx->gui.end_turn.rect))
        return true;

    return false;
}

u32 get_neighbors_for_caravan(client_context *ctx, v2<u32> pt, memory_arena *mem, v2<u32> **neighbors) {
    u32 rv = 0;
    v2<s32> d;
    d.x = -1;
    d.y = -1;
    *neighbors = (v2<u32> *)(mem->base + mem->used);
    for (u32 y = 0; y < 3; ++y) {
        for (u32 x = 0; x < 3; ++x) {
            s32 X = (s32)pt.x + d.x + x;
            s32 Y = (s32)pt.y + d.y + y;
            if (X < 0 || X > (s32)ctx->map.width - 1 ||
                Y < 0 || Y > (s32)ctx->map.height - 1) {
                continue;
            }
            if ((s32)pt.x == X && (s32)pt.y == Y)
                continue;

            bool passable = true;
            u32 idx = Y * ctx->map.width + X;
            if (ctx->map.terrain[idx] == client_terrain_names::HILLS) {
                passable = false;
            }

            if (passable) {
                v2<u32> *n = (v2<u32> *)memory_arena_use(mem, sizeof(*n));
                n->x = X;
                n->y = Y;
                ++rv;
            }
        }
    }

    return rv;
}

u32 get_path_for_caravan(client_context *ctx, v2<u32> start, v2<u32> goal, memory_arena *mem, v2<u32> **paths) {
    auto frontier = priority_queue<v2<u32>>();
    frontier.push(start, 0);
    auto came_from = dictionary<v2<u32>, v2<u32>>();
    came_from.push(start, start);
    auto heuristic = [](v2<u32> a, v2<u32> b) {
        s32 dx = abs((s32)a.x - (s32)b.x);
        s32 dy = abs((s32)a.y - (s32)b.y);
        return (dx + dy) + MIN(dx, dy);
    };

    ctx->debug.highlighted_tiles.clear();
    ctx->debug.highlighted_priorities.clear();
    while (!frontier.empty()) {
        ctx->debug.highlighted_priorities.push_back(frontier.first->priority);
        auto current = frontier.pop();
        ctx->debug.highlighted_tiles.push_back(current);

        if (current == goal) {
            break;
        }

        v2<u32> *neighbors;
        u32 num_neighbors = get_neighbors_for_caravan(ctx, current, mem, &neighbors);
        for (u32 i = 0; i < num_neighbors; ++i) {
            v2<u32> *from = came_from.get(neighbors[i]);
            if (!from) {
                v2<s32> s, e, d;
                s.x = (s32)start.x;
                s.y = (s32)start.y;
                e.x = (s32)neighbors[i].x;
                e.y = (s32)neighbors[i].y;
                d = e - s;
                if (d.length() <= 5) {
                    u32 priority = heuristic(goal, neighbors[i]);
                    frontier.push(neighbors[i], priority);
                    came_from.push(neighbors[i], current);
                }
            }
        }
    }

    *paths = (v2<u32> *)(mem->base + mem->used);
    u32 paths_used = 0;
    v2<u32> current = goal;
    while (current != start) {
        (*paths)[paths_used++] = current;

        v2<u32> *from = came_from.get(current);
        if (from) {
            current = *from;
        } else
            return 0;
    }

    for (u32 i = 0; i < paths_used / 2; ++i) {
        v2<u32> temp = (*paths)[i];
        (*paths)[i] = (*paths)[(paths_used - 1) - i];
        (*paths)[(paths_used - 1) - i] = temp;
    }

    return paths_used;
}

template <class T>
Vector2 v2_to_Vector2(v2<T> v) {
    Vector2 rv;
    rv.x = (real32)v.x;
    rv.y = (real32)v.y;
    return rv;
}

client_terrain_names terrain_names_to_client_terrain_names(terrain_names name) {
    if (name == terrain_names::FOG)
        return client_terrain_names::FOG;
    else if (name == terrain_names::GROUND)
        return client_terrain_names::GROUND;
    else if (name == terrain_names::WATER)
        return client_terrain_names::WATER;
    else if (name == terrain_names::HILLS)
        return client_terrain_names::HILLS;
}

void initialize_map(client_context *ctx, memory_arena *mem, u32 width, u32 height) {
    ctx->map.width = width;
    ctx->map.height = height;

    ctx->map.terrain = (client_terrain_names *)memory_arena_use(mem, sizeof(*ctx->map.terrain)
                                                                    * ctx->map.width
                                                                    * ctx->map.height
                                                                );

    ctx->map.towns.used = 0;
    ctx->map.towns.max = 100;
    ctx->map.towns.positions = (v2<u32> *)memory_arena_use(mem,
                                            sizeof(*ctx->map.towns.positions)
                                            * ctx->map.towns.max
                                            );
    ctx->map.towns.server_ids = (u32 *)memory_arena_use(mem,
                                            sizeof(*ctx->map.towns.server_ids)
                                            * ctx->map.towns.max
                                            );
    ctx->map.towns.owners = (s32 *)memory_arena_use(mem,
                                            sizeof(*ctx->map.towns.owners)
                                            * ctx->map.towns.max
                                            );
    ctx->map.towns.constructions = (unit_names *)memory_arena_use(mem,
                                            sizeof(*ctx->map.towns.constructions)
                                            * ctx->map.towns.max
                                            );

    ctx->map.units.used = 0;
    ctx->map.units.max = 1000;
    ctx->map.units.positions = (v2<u32> *)memory_arena_use(mem,
                                            sizeof(*ctx->map.units.positions)
                                            * ctx->map.units.max
                                            );
    ctx->map.units.names = (unit_names *)memory_arena_use(mem,
                                            sizeof(*ctx->map.units.names)
                                            * ctx->map.units.max
                                            );
    ctx->map.units.owners = (s32 *)memory_arena_use(mem,
                                            sizeof(*ctx->map.units.owners)
                                            * ctx->map.units.max
                                            );
    ctx->map.units.server_ids = (u32 *)memory_arena_use(mem,
                                            sizeof(*ctx->map.units.server_ids)
                                            * ctx->map.units.max
                                            );
    ctx->map.units.action_points = (u32 *)memory_arena_use(mem,
                                            sizeof(*ctx->map.units.action_points)
                                            * ctx->map.units.max
                                            );
}

void draw_map(client_context *ctx) {
    real32 scr_width = GetScreenWidth();
    real32 scr_height = GetScreenHeight();

    u32 tile_width = 32;
    u32 tile_height = 32;

    u32 num_of_tiles_fit_on_screen_x = (u32)floor(scr_width / (real32)tile_width);
    u32 num_of_tiles_fit_on_screen_y = (u32)floor(scr_height / (real32)tile_height);

    u32 num_of_terrain_tiles_x = ctx->map.width - ctx->camera.x;
    u32 num_of_terrain_tiles_y = ctx->map.height - ctx->camera.y;

    u32 lesser_x = num_of_tiles_fit_on_screen_x;
    u32 lesser_y = num_of_tiles_fit_on_screen_y;

    if (num_of_terrain_tiles_x < lesser_x)
        lesser_x = num_of_terrain_tiles_x;
    if (num_of_terrain_tiles_y < lesser_y)
        lesser_y = num_of_terrain_tiles_y;

    real32 X = 0, Y = 0;

    for (u32 y = ctx->camera.y; y < ctx->camera.y + lesser_y; ++y) {
        for (u32 x = ctx->camera.x; x < ctx->camera.x + lesser_x; ++x) {
            u32 it = y * ctx->map.width + x;
            if (ctx->map.terrain[it] == client_terrain_names::FOG)
                DrawRectangle(X, Y, tile_width, tile_height, BLACK);
            else if (ctx->map.terrain[it] == client_terrain_names::GROUND)
                DrawRectangle(X, Y, tile_width, tile_height, CLITERAL(Color){101, 252, 96, 255});
            else if (ctx->map.terrain[it] == client_terrain_names::WATER)
                DrawRectangle(X, Y, tile_width, tile_height, CLITERAL(Color){255, 241, 171, 255});
            else if (ctx->map.terrain[it] == client_terrain_names::HILLS)
                DrawRectangle(X, Y, tile_width, tile_height, BLUE);
            X = X + tile_width;
        }
        X = 0;
        Y = Y + tile_height;
    }

    u32 len = ctx->debug.highlighted_tiles.length();
    for (u32 i = 0; i < len; ++i) {
        v2<u32> *pos_world = ctx->debug.highlighted_tiles.get(i);
        v2<real32> pos_screen;
        pos_screen.x = (pos_world->x - ctx->camera.x) * tile_width;
        pos_screen.y = (pos_world->y - ctx->camera.y) * tile_height;

        DrawRectangleLines(pos_screen.x, pos_screen.y, tile_width, tile_height, PURPLE);
        u32 *priority = ctx->debug.highlighted_priorities.get(i);
        if (priority) {
            char buf[15];
            snprintf(buf, 15, "%u", *priority);
            DrawText(buf, pos_screen.x, pos_screen.y, 16, WHITE);
        }
    }


    for (u32 i = 0; i < ctx->map.towns.used; ++i) {
        v2<u32> pos = ctx->map.towns.positions[i];
        X = pos.x * tile_width + tile_width / 2;
        Y = pos.y * tile_height + tile_width / 2;
        X -= ctx->camera.x * tile_width;
        Y -= ctx->camera.y * tile_height;
        
        Color color = GRAY;
        if (ctx->map.towns.owners[i] != -1) {
            color = ctx->clients.colors[ctx->map.towns.owners[i]];
        }
        DrawCircle(X, Y, tile_width / 2, color);

        if (ctx->selected_town_id == (s32)i) {
            DrawRectangleLines(X - tile_width / 2, Y - tile_height / 2,
                                tile_width, tile_height, RED);
        }
    }

    for (u32 i = 0; i < ctx->map.units.used; ++i) {
        v2<u32> pos = ctx->map.units.positions[i];
        X = pos.x * tile_width;
        Y = pos.y * tile_height;
        X -= ctx->camera.x * tile_width;
        Y -= ctx->camera.y * tile_height;

        Color color = ctx->clients.colors[ctx->map.units.owners[i]];
        if (ctx->map.units.names[i] == unit_names::SOLDIER) {
            Vector2 bottom_left = (Vector2){.x = X, .y = Y + tile_height};
            Vector2 top_middle = (Vector2){.x = X + tile_width / 2, .y = Y};
            Vector2 bottom_right = (Vector2){.x = X + tile_width, .y = Y + tile_height};
            DrawTriangle(bottom_left, bottom_right, top_middle, color);

            color = BLACK;
            if (ctx->selected_unit_id == (s32)i) {
                color = RED;
            }
            DrawTriangleLines(bottom_left, bottom_right,
                                top_middle, color);
        } else if (ctx->map.units.names[i] == unit_names::CARAVAN) {
            Vector2 mid_left = (Vector2){.x = X, .y = Y + tile_height / 2};
            Vector2 top_right = (Vector2){.x = X + tile_width, .y = Y};
            Vector2 bottom_right = (Vector2){.x = X + tile_width, .y = Y + tile_height};
            DrawTriangle(mid_left, bottom_right, top_right, color);

            color = BLACK;
            if (ctx->selected_unit_id == (s32)i) {
                color = RED;
            }
            DrawTriangleLines(mid_left, bottom_right,
                                top_right, color);
        }
        char num_buf[3 + 1];
        snprintf(num_buf, 3 + 1, "%u", ctx->map.units.action_points[i]);
        DrawText(num_buf, X, Y, 16, WHITE);
    }
}

CLIENT_UPDATE_AND_RENDER(client_update_and_render) {
    client_context *ctx = (client_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));
        ctx->read_buffer = memory_arena_child(mem, MB(50), "client_memory_read");
        ctx->temp_mem = memory_arena_child(mem, MB(20), "client_memory_temp");

        ctx->clients.used = 0;
        ctx->clients.max = 32;
        ctx->clients.colors = (Color *)memory_arena_use(mem, sizeof(*ctx->clients.colors)
                                                        * ctx->clients.max
                                                        );
        for (u32 i = 0; i < ctx->clients.max; ++i) {
            u8 red = rand() % 255;
            u8 green = rand() % 255;
            u8 blue = rand() % 255;
            ctx->clients.colors[i] = (Color){red, green, blue, 255};
        }

        ctx->selected_town_id = -1;
        ctx->selected_unit_id = -1;

        real32 scr_width = GetScreenWidth();
        real32 scr_height = GetScreenHeight();

        ctx->gui.town.rect = (Rectangle){GetScreenWidth() - 200, 0, 200, GetScreenHeight() / 2};
        ctx->gui.town.build_names[0] = "None";
        ctx->gui.town.build_names[1] = "Soldier";
        ctx->gui.town.build_names[2] = "Caravan";

        ctx->gui.admin.rect = (Rectangle){GetScreenWidth() - 200, GetScreenHeight() / 2, 200, GetScreenHeight() / 2};

        ctx->gui.end_turn.rect = (Rectangle){scr_width / 2 - 60, scr_height - 30, 120, 30};

        ctx->background_music = LoadMusicStream("assets/trouble_with_tribals.mp3");
        SetMusicVolume(ctx->background_music, 0.03);

        ctx->is_init = true;
    }

    BeginDrawing();
        ClearBackground(WHITE);

        if (ctx->current_screen == client_screen_names::MAIN_MENU) {
            real32 scrWidth = GetScreenWidth();
            real32 scrHeight = GetScreenHeight();
            if (GuiButton((Rectangle){scrWidth / 2 - 150 / 2, scrHeight / 2 - 15, 150, 30}, "Start")) {
                comm_client_header header;
                header.name = comm_client_msg_names::CONNECT;

                comm_write(comm, &header, sizeof(header));

                header.name = comm_client_msg_names::START;
                comm_write(comm, &header, sizeof(header));

                ctx->current_screen = client_screen_names::INITIALIZE_GAME;
            }
        } else if (ctx->current_screen == client_screen_names::INITIALIZE_GAME) {
            comm_server_header *header;
            s32 len = comm_read(comm, ctx->read_buffer.base, ctx->read_buffer.max); 
            u32 buf_it = sizeof(comm_shared_header);
            if (len > 0) {
                if (len - buf_it >= sizeof(*header)) {
                    header = (comm_server_header *)(ctx->read_buffer.base + buf_it);
                    if (header->name == comm_server_msg_names::INIT_MAP) {
                        comm_server_init_map_body *init_map_body;
                        buf_it += sizeof(*header);
                        if (len - buf_it >= sizeof(*init_map_body)) {
                            init_map_body = (comm_server_init_map_body *)(ctx->read_buffer.base + buf_it);
                            ctx->my_server_id = init_map_body->your_id;
                            initialize_map(ctx, mem, init_map_body->width, init_map_body->height);
                            ctx->current_screen = client_screen_names::GAME;
                            sitrep(SITREP_DEBUG, "INIT_EVERYBODY");
                            //PlayMusicStream(ctx->background_music);
                        }
                    }
                }
            }
        } else if (ctx->current_screen == client_screen_names::GAME) {
            UpdateMusicStream(ctx->background_music);

            real32 scr_width = GetScreenWidth();
            real32 scr_height = GetScreenHeight();

            if (IsKeyPressed(KEY_LEFT)) {
                if (ctx->camera.x > 0) {
                    ctx->camera.x -= 1;
                }
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                if (ctx->camera.x < ctx->map.width) {
                    ctx->camera.x += 1;
                }
            }
            if (IsKeyPressed(KEY_UP)) {
                if (ctx->camera.y > 0) {
                    ctx->camera.y -= 1;
                }
            }
            if (IsKeyPressed(KEY_DOWN)) {
                if (ctx->camera.y < ctx->map.height) {
                    ctx->camera.y += 1;
                }
            }

            if (!is_pt_in_gui(ctx, GetMousePosition())) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mouse_pos = GetMousePosition();
                    v2<u32> mouse_tile_pos;
                    real32 tile_width = 32.0f;
                    real32 tile_height = 32.0f;
                    mouse_tile_pos.x = (u32)floor(mouse_pos.x / tile_width);
                    mouse_tile_pos.y = (u32)floor(mouse_pos.y / tile_height);
                    mouse_tile_pos.x += ctx->camera.x;
                    mouse_tile_pos.y += ctx->camera.y;

                    bool found = false;
                    for (u32 i = 0; i < ctx->map.units.used; ++i) {
                        if (mouse_tile_pos.x == ctx->map.units.positions[i].x &&
                            mouse_tile_pos.y == ctx->map.units.positions[i].y) {
                            if (ctx->selected_unit_id == i) {
                                ctx->selected_unit_id = -1;
                            } else {
                                ctx->selected_town_id = -1;
                                ctx->selected_unit_id = i;
                                found = true;
                            }
                            break;
                        }
                    }
                    if (!found) {
                        if (ctx->selected_unit_id == -1) {
                            for (u32 i = 0; i < ctx->map.towns.used; ++i) {
                                if (mouse_tile_pos.x == ctx->map.towns.positions[i].x &&
                                    mouse_tile_pos.y == ctx->map.towns.positions[i].y) {
                                    ctx->selected_town_id = i;
                                    ctx->gui.town.build_active = (s32)ctx->map.towns.constructions[i];
                                    found = true;
                                    break;
                                }
                            }
                        }

                        if (!found) {
                            if (ctx->selected_unit_id != -1) {
                                if (ctx->map.units.names[ctx->selected_unit_id] == unit_names::SOLDIER) {
                                    v2<u32> selected_pos = ctx->map.units.positions[ctx->selected_unit_id];
                                    v2<s32> d;
                                    d.x = (s32)mouse_tile_pos.x - (s32)selected_pos.x;
                                    d.y = (s32)mouse_tile_pos.y - (s32)selected_pos.y;
                                    if ((d.x >= -1 && d.x <= 1) && (d.y >= -1 && d.y <= 1)) {
                                        u32 idx = ctx->map.width * mouse_tile_pos.y + mouse_tile_pos.x;
                                        comm_client_header header;
                                        header.name = comm_client_msg_names::MOVE_UNIT;
                                        comm_write(comm, &header, sizeof(header));
                                        comm_client_move_unit_body body;
                                        body.unit_id = ctx->map.units.server_ids[ctx->selected_unit_id];
                                        body.delta = d;
                                        comm_write(comm, &body, sizeof(body));
                                    } else {
                                        ctx->selected_unit_id = -1;
                                    }
                                } else if (ctx->map.units.names[ctx->selected_unit_id] == unit_names::CARAVAN) {
                                    v2<u32> prev_path = ctx->map.units.positions[ctx->selected_unit_id];
                                    v2<u32> *paths;
                                    u32 num_paths = get_path_for_caravan(ctx, prev_path, mouse_tile_pos, &ctx->temp_mem, &paths);

                                    for (u32 i = 0; i < num_paths; ++i) {
                                        comm_client_header header;
                                        header.name = comm_client_msg_names::MOVE_UNIT;
                                        comm_write(comm, &header, sizeof(header));
                                        comm_client_move_unit_body body;
                                        body.unit_id = ctx->map.units.server_ids[ctx->selected_unit_id];
                                        v2<s32> d;
                                        d.x = (s32)paths[i].x - (s32)prev_path.x;
                                        d.y = (s32)paths[i].y - (s32)prev_path.y;
                                        body.delta = d;
                                        comm_write(comm, &body, sizeof(body));

                                        prev_path = paths[i];
                                    }
                                }
                            }
                        }
                    }
                }
            }

            comm_server_header *header;
            s32 len = comm_read(comm, ctx->read_buffer.base, ctx->read_buffer.max);
            u32 buf_it = sizeof(comm_shared_header);
            if (len > 0) {
                while (len - buf_it >= sizeof(*header)) {
                    header = (comm_server_header *)(ctx->read_buffer.base + buf_it);
                    buf_it += sizeof(*header);
                    if (header->name == comm_server_msg_names::DISCOVER) {
                        comm_server_discover_body *discover_body;
                        if (len - buf_it >= sizeof(*discover_body)) {
                            discover_body = (comm_server_discover_body *)(ctx->read_buffer.base + buf_it);
                            buf_it += sizeof(*discover_body);
                            for (u32 i = 0; i < discover_body->num; ++i) {
                                comm_server_discover_body_tile *tile = (comm_server_discover_body_tile *)(ctx->read_buffer.base + buf_it);
                                buf_it += sizeof(*tile);
                                ctx->map.terrain[tile->position.y * ctx->map.width + tile->position.x] = terrain_names_to_client_terrain_names(tile->name);
                            }
                        }
                    } else if (header->name == comm_server_msg_names::PING) {
                        comm_client_header client_header;
                        client_header.name = comm_client_msg_names::PONG;
                        comm_write(comm, &client_header, sizeof(client_header));
                    } else if (header->name == comm_server_msg_names::DISCOVER_TOWN) {
                        comm_server_discover_town_body *discover_town_body;
                        if (len - buf_it >= sizeof(*discover_town_body)) {
                            discover_town_body = (comm_server_discover_town_body *)(ctx->read_buffer.base + buf_it);
                            buf_it += sizeof(*discover_town_body);

                            u32 id = ctx->map.towns.used++;
                            ctx->map.towns.positions[id] = discover_town_body->position;
                            ctx->map.towns.owners[id] = discover_town_body->owner;
                            ctx->map.towns.server_ids[id] = discover_town_body->id;

                            if (discover_town_body->owner == ctx->my_server_id) {
                                s32 camera_x = (s32)discover_town_body->position.x;
                                s32 camera_y = (s32)discover_town_body->position.y;
                                u32 num_tiles_in_scr_width = scr_width / 32;
                                u32 num_tiles_in_scr_height = scr_height / 32;
                                u32 half_scr_width = num_tiles_in_scr_width >> 1;
                                u32 half_scr_height = num_tiles_in_scr_height >> 1;
                                camera_x -= half_scr_width;
                                if (camera_x < 0)
                                    camera_x = 0;
                                camera_y -= half_scr_height;
                                if (camera_y < 0)
                                    camera_y = 0;
                                ctx->camera.x = camera_x;
                                ctx->camera.y = camera_y;
                            }
                        }
                    } else if (header->name == comm_server_msg_names::YOUR_TURN) {
                        ctx->my_turn = true;
                    } else if (header->name == comm_server_msg_names::SET_UNIT_ACTION_POINTS) {
                        comm_server_set_unit_action_points_body *body;
                        if (len - buf_it >= sizeof(*body)) {
                            body = (comm_server_set_unit_action_points_body *)(ctx->read_buffer.base + buf_it);
                            buf_it += sizeof(*body);
                            for (u32 i = 0; i < ctx->map.units.used; ++i) {
                                if (ctx->map.units.server_ids[i] == body->unit_id) {
                                    ctx->map.units.action_points[i] = body->new_action_points;
                                }
                            }
                        }
                    } else if (header->name == comm_server_msg_names::CONSTRUCTION_SET) {
                        comm_server_construction_set_body *construction_set_body;
                        if (len - buf_it >= sizeof(*construction_set_body)) {
                            construction_set_body = (comm_server_construction_set_body *)(ctx->read_buffer.base + buf_it);
                            buf_it += sizeof(*construction_set_body);

                            for (u32 i = 0; i < ctx->map.towns.used; ++i) {
                                if (ctx->map.towns.server_ids[i] == construction_set_body->town_id) {
                                    ctx->map.towns.constructions[i] = construction_set_body->unit_name;
                                }
                            }
                            if (ctx->selected_town_id != -1 && ctx->map.towns.server_ids[ctx->selected_town_id] == construction_set_body->town_id) {
                                ctx->gui.town.build_active = (s32)construction_set_body->unit_name;
                            }
                        }
                    } else if (header->name == comm_server_msg_names::ADD_UNIT) {
                        comm_server_add_unit_body *add_unit_body;
                        if (len - buf_it >= sizeof(*add_unit_body)) {
                            add_unit_body = (comm_server_add_unit_body *)(ctx->read_buffer.base + buf_it);
                            buf_it += sizeof(*add_unit_body);

                            u32 id = ctx->map.units.used++;
                            ctx->map.units.server_ids[id] = add_unit_body->unit_id;
                            ctx->map.units.positions[id] = add_unit_body->position;
                            ctx->map.units.names[id] = add_unit_body->unit_name;
                            ctx->map.units.action_points[id] = add_unit_body->action_points;
                            ctx->map.units.owners[id] = ctx->my_server_id;
                        }
                    } else if (header->name == comm_server_msg_names::MOVE_UNIT) {
                        comm_server_move_unit_body *move_unit_body;
                        if (len - buf_it >= sizeof(*move_unit_body)) {
                            move_unit_body = (comm_server_move_unit_body *)(ctx->read_buffer.base + buf_it);
                            buf_it += sizeof(*move_unit_body);
                            for (u32 i = 0; i < ctx->map.units.used; ++i) {
                                if (ctx->map.units.server_ids[i] == move_unit_body->unit_id) {
                                    ctx->map.units.positions[i] = move_unit_body->new_position;
                                    ctx->map.units.action_points[i] = move_unit_body->action_points_left;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            draw_map(ctx);

            if (ctx->selected_unit_id != -1) {
                v2<real32> unit_world_pos;
                v2<real32> unit_screen_pos;
                v2<real32> unit_screen_pos_centered;
                unit_world_pos.x = ctx->map.units.positions[ctx->selected_unit_id].x * 32;
                unit_world_pos.y = ctx->map.units.positions[ctx->selected_unit_id].y * 32;
                unit_screen_pos.x = unit_world_pos.x - (ctx->camera.x * 32);
                unit_screen_pos.y = unit_world_pos.y - (ctx->camera.y * 32);
                unit_screen_pos_centered.x = unit_screen_pos.x + 16;
                unit_screen_pos_centered.y = unit_screen_pos.y + 16;

                DrawLineV(v2_to_Vector2(unit_screen_pos_centered), GetMousePosition(), BLACK);
            }

            if (GuiButton(ctx->gui.end_turn.rect, "END TURN")) {
                comm_client_header header;
                header.name = comm_client_msg_names::END_TURN;

                comm_write(comm, &header, sizeof(header));

                ctx->my_turn = false;
            }



            if (ctx->selected_town_id != -1) {
                Rectangle town_window = ctx->gui.town.rect;
                bool close = GuiWindowBox(town_window, "Town");
                GuiLabel((Rectangle){town_window.x + 10, town_window.y + 30, town_window.width - 20, 20}, "Build:");
                s32 prev_active = ctx->gui.town.build_active;
                s32 curr_active = GuiToggleGroup((Rectangle){town_window.x + 20, town_window.y + 50, town_window.width - 40, 30}, TextJoin((const char**)ctx->gui.town.build_names, 3, "\n"), ctx->gui.town.build_active);
                if (prev_active != curr_active) {
                    comm_client_header header;
                    header.name = comm_client_msg_names::SET_CONSTRUCTION;

                    comm_write(comm, &header, sizeof(header));

                    comm_client_set_construction_body body;
                    body.town_id = ctx->map.towns.server_ids[ctx->selected_town_id];
                    body.unit_name = (unit_names)curr_active;

                    comm_write(comm, &body, sizeof(body));
                }

                if (close) {
                    ctx->selected_town_id = -1;
                }
            }

            Rectangle admin_window = ctx->gui.admin.rect;
            GuiWindowBox(admin_window, "Admin");
                if (GuiButton((Rectangle){admin_window.x + 10, admin_window.y + 30, admin_window.width - 20, 20}, "DISCOVER")) {
                    comm_client_header header;
                    header.name = comm_client_msg_names::ADMIN_DISCOVER_ENTIRE_MAP;

                    comm_write(comm, &header, sizeof(header));
                }

            if (!ctx->my_turn) {
                Rectangle window = (Rectangle){scr_width / 2 - 150, scr_height / 2 - 30, 300, 60};
                GuiWindowBox(window, "Status");
                    GuiLabel((Rectangle){window.x + 100, window.y + 25, window.width - 200, window.height - 25}, "Waiting on your turn");
            }
        }
    EndDrawing();

    comm_flush(comm);
}
