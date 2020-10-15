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
    } map;
};

void initialize_map(struct client_context *ctx, u32 width, u32 height) {
    ctx->map.width = width;
    ctx->map.height = height;
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
                        initialize_map(ctx, init_map_body.width, init_map_body.height);
                        ctx->current_screen = client_screen_names::GAME;
                    }
                }
            }
        } else if (ctx->current_screen == client_screen_names::GAME) {
            real32 scrWidth = GetScreenWidth();
            real32 scrHeight = GetScreenHeight();
            
            char buf[1024];
            snprintf(buf, 1024, "%lu", ctx->map.width);
            GuiLabel((Rectangle){scrWidth / 2 - 150 / 2, scrHeight / 2 - 15, 150, 30}, buf);
            snprintf(buf, 1024, "%lu", ctx->map.height);
            GuiLabel((Rectangle){scrWidth / 2 - 150 / 2, scrHeight / 2 - 15 + 30, 150, 30}, buf);
        }
    EndDrawing();
}
