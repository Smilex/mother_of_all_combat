enum class ai_state_names {
    CONNECT = 0,
    INITIALIZE,
    GAME
};

struct ai_context {
    bool is_init;

    memory_arena read_buffer;
    ai_state_names current_state;

    struct {
        u32 width, height;
        client_terrain_names *terrain;
    } map;
};

void ai_initialize_map(ai_context *ctx, memory_arena *mem, u32 width, u32 height) {
    ctx->map.width = width;
    ctx->map.height = height;

    ctx->map.terrain = (client_terrain_names *)memory_arena_use(mem, sizeof(*ctx->map.terrain)
                                                                    * ctx->map.width
                                                                    * ctx->map.height
                                                                );
}

void ai_update(memory_arena *mem, communication *comm) {
    ai_context *ctx = (ai_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));
        ctx->read_buffer = memory_arena_child(mem, MB(10), "ai_memory_read");

        ctx->current_state = ai_state_names::CONNECT;
        ctx->is_init = true;
    }

    if (ctx->current_state == ai_state_names::CONNECT) {
        comm_client_header header;
        header.name = comm_client_msg_names::CONNECT;

        comm_write(comm, &header, sizeof(header));

        ctx->current_state = ai_state_names::INITIALIZE;
    } else if (ctx->current_state == ai_state_names::INITIALIZE) {
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
                        ai_initialize_map(ctx, mem, init_map_body->width, init_map_body->height);
                        ctx->current_state = ai_state_names::GAME;
                    }
                }
            }
        }
    } else if (ctx->current_state == ai_state_names::GAME) {
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
                            v2<u32> *pos = (v2<u32> *)(ctx->read_buffer.base + buf_it);
                            buf_it += sizeof(*pos);
                            ctx->map.terrain[pos->y * ctx->map.width + pos->x] = client_terrain_names::GROUND;
                        }
                    }
                } else if (header->name == comm_server_msg_names::PING) {
                    comm_client_header client_header;
                    client_header.name = comm_client_msg_names::PONG;
                    comm_write(comm, &client_header, sizeof(client_header));
                }
            }
        }
    }

    comm_flush(comm);
}
