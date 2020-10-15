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
    memory_arena temp_mem;

    struct {
        u32 width, height;
        client_terrain_names *terrain;
    } map;

    struct {
        u32 x, y;
    } camera;
};

void initialize_map(client_context *ctx, memory_arena *mem, u32 width, u32 height) {
    ctx->map.width = width;
    ctx->map.height = height;

    ctx->map.terrain = (client_terrain_names *)memory_arena_use(mem, sizeof(*ctx->map.terrain)
                                                                    * ctx->map.width
                                                                    * ctx->map.height
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
                DrawRectangle(X, Y, tile_width, tile_height, GREEN);
            X = X + tile_width;
        }
        X = 0;
        Y = Y + tile_height;
    }
}

CLIENT_UPDATE_AND_RENDER(client_update_and_render) {
    client_context *ctx = (client_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));

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

                comm->send(*comm, &header, sizeof(header));

                header.name = comm_client_msg_names::START;
                comm->send(*comm, &header, sizeof(header));

                ctx->current_screen = client_screen_names::INITIALIZE_GAME;
            }
        } else if (ctx->current_screen == client_screen_names::INITIALIZE_GAME) {
            comm_server_header header;
            s32 len = comm->recv(*comm, &header, sizeof(header)); 
            if (len == sizeof(header)) {
                if (header.name == comm_server_msg_names::INIT_MAP) {
                    comm_server_init_map_body init_map_body;
                    len = comm->recv(*comm, &init_map_body, sizeof(init_map_body));
                    if (len == sizeof(init_map_body)) {
                        initialize_map(ctx, mem, init_map_body.width, init_map_body.height);
                        ctx->current_screen = client_screen_names::GAME;
                    }
                }
            }
        } else if (ctx->current_screen == client_screen_names::GAME) {
            comm_server_header header;
            s32 len = comm->recv(*comm, &header, sizeof(header));
            if (len == sizeof(header)) {
                if (header.name == comm_server_msg_names::DISCOVER) {
                    comm_server_discover_body discover_body;
                    len = comm->recv(*comm, &discover_body, sizeof(discover_body));
                    if (len == sizeof(discover_body)) {
                        ctx->map.terrain[discover_body.y * ctx->map.width + discover_body.x] = client_terrain_names::GROUND;
                    }
                }
            }
            draw_map(ctx);
        }
    EndDrawing();
}
