struct client_context {
    bool is_init;
};

CLIENT_UPDATE_AND_RENDER(client_update_and_render) {
    client_context *ctx = (client_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));

        ctx->is_init = true;
    }

    BeginDrawing();
        ClearBackground(WHITE);

        real32 scrWidth = GetScreenWidth();
        real32 scrHeight = GetScreenHeight();
        if (GuiButton((Rectangle){scrWidth / 2 - 150 / 2, scrHeight / 2 - 15, 150, 30}, "Connect")) {
            comm_client_header header;
            header.name = comm_client_msg_names::CONNECT;
            header.size = 0;

            comm->send(*comm, &header, sizeof(header));
        }
    EndDrawing();
}
