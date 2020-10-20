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

    struct {
        u32 width, height;
        client_terrain_names *terrain;

        struct {
            v2<u32> *positions;
            u32 *server_ids;
            u32 used, max;
        } towns;
    } map;

    struct {
        u32 x, y;
    } camera;
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
        DrawCircle(X, Y, tile_width / 2, RED);
    }
}

CLIENT_UPDATE_AND_RENDER(client_update_and_render) {
    client_context *ctx = (client_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));
        ctx->read_buffer = memory_arena_child(mem, MB(50), "client_memory_read");

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
                            initialize_map(ctx, mem, init_map_body->width, init_map_body->height);
                            ctx->current_screen = client_screen_names::GAME;
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
                            ctx->map.towns.server_ids[id] = discover_town_body->id;
                        }
                    }
                }
            }
            draw_map(ctx);

            if (GuiButton((Rectangle){scr_width / 2 - 150 / 2, scr_height / 2 - 15, 150, 30}, "DISCOVER")) {
                comm_client_header header;
                header.name = comm_client_msg_names::ADMIN_DISCOVER_ENTIRE_MAP;

                comm_write(comm, &header, sizeof(header));
            }
        }
    EndDrawing();

    comm_flush(comm);
}
