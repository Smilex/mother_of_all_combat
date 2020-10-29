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
            char *build_names[2];
            s32 build_active;
        } town;
        struct {
            Rectangle rect;
        } admin;
    } gui;
};

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

        ctx->gui.town.rect = (Rectangle){GetScreenWidth() - 200, 0, 200, GetScreenHeight() / 2};
        ctx->gui.town.build_names[0] = "None";
        ctx->gui.town.build_names[1] = "Soldier";

        ctx->gui.admin.rect = (Rectangle){GetScreenWidth() - 200, GetScreenHeight() / 2, 200, GetScreenHeight() / 2};

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
                        }
                    }
                }
            }
        } else if (ctx->current_screen == client_screen_names::GAME) {
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
                            v2<u32> selected_pos = ctx->map.units.positions[ctx->selected_unit_id];
                            v2<s32> d;
                            d.x = (s32)mouse_tile_pos.x - (s32)selected_pos.x;
                            d.y = (s32)mouse_tile_pos.y - (s32)selected_pos.y;
                            if ((d.x >= -1 && d.x <= 1) && (d.y >= -1 && d.y <= 1)) {
                                u32 idx = ctx->map.width * mouse_tile_pos.y + mouse_tile_pos.x;
                                if (ctx->map.terrain[idx] == client_terrain_names::GROUND) {
                                    comm_client_header header;
                                    header.name = comm_client_msg_names::MOVE_UNIT;
                                    comm_write(comm, &header, sizeof(header));
                                    comm_client_move_unit_body body;
                                    body.unit_id = ctx->map.units.server_ids[ctx->selected_unit_id];
                                    body.delta = d;
                                    comm_write(comm, &body, sizeof(body));
                                }
                            } else {
                                ctx->selected_unit_id = -1;
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
                                ctx->camera.x = discover_town_body->position.x;
                                ctx->camera.y = discover_town_body->position.y;
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

            if (GuiButton((Rectangle){scr_width / 2 - 60, scr_height - 30, 120, 30}, "END TURN")) {
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
                s32 curr_active = GuiToggleGroup((Rectangle){town_window.x + 20, town_window.y + 50, town_window.width - 40, 30}, TextJoin((const char**)ctx->gui.town.build_names, 2, "\n"), ctx->gui.town.build_active);
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
