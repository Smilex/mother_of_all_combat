#define MAP_GRID_WIDTH 1000
#define MAP_GRID_HEIGHT 1000

enum server_state_names {
    AWAITING_CONNECTIONS = 0,
    INIT_EVERYBODY,
    TEMP
};

struct server_context {
    bool is_init;

    memory_arena read_buffer;
    server_state_names current_state;

    struct {
        terrain_names *terrain;
        u32 terrain_width,
            terrain_height;
    } map;
    struct {
        communication *comms;
        bool *admins;
        bool *connecteds;
        u32 max, used;
    } clients;
};

void server_update(memory_arena *mem, communication *comm) {
    struct server_context *ctx = (struct server_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));

        ctx->read_buffer = memory_arena_child(mem, MB(10));

        ctx->clients.used = 0;
        ctx->clients.max = 32;
        ctx->clients.comms = (communication *)memory_arena_use(mem,
                                                sizeof(*ctx->clients.comms)
                                                * ctx->clients.max
                                                );
        ctx->clients.admins = (bool *)memory_arena_use(mem,
                                                sizeof(*ctx->clients.admins)
                                                * ctx->clients.max
                                                );
        ctx->clients.connecteds = (bool *)memory_arena_use(mem,
                                                sizeof(*ctx->clients.connecteds)
                                                * ctx->clients.max
                                                );

        if (comm) {
            ctx->clients.comms[0] = *comm;
            ctx->clients.admins[0] = true;
            ++ctx->clients.used;
        }

        ctx->map.terrain_width = MAP_GRID_WIDTH;
        ctx->map.terrain_height = MAP_GRID_HEIGHT;
        ctx->map.terrain = (terrain_names *)memory_arena_use(mem,
                                                sizeof(*ctx->map.terrain)
                                                * ctx->map.terrain_width
                                                * ctx->map.terrain_height
                                                );
        
        ctx->is_init = true;
    }

    if (ctx->current_state == server_state_names::AWAITING_CONNECTIONS) {
        for (u32 i = 0; i < ctx->clients.used; ++i) {
            comm_client_header *header;
            communication *comm = &ctx->clients.comms[i];
            s32 len = comm_read(comm, ctx->read_buffer.base, ctx->read_buffer.max);
			u32 read_it = sizeof(comm_shared_header);
            
			while (read_it < len) {
				if (len - read_it >= sizeof(*header)) {
					header = (comm_client_header *)(ctx->read_buffer.base + read_it);
					if (header->name == comm_client_msg_names::CONNECT) {
						ctx->clients.connecteds[i] = true;
						printf("CONNECTED\n");
					} else if(header->name == comm_client_msg_names::START) {
						if (ctx->clients.admins[i]) {
							ctx->current_state = server_state_names::INIT_EVERYBODY;
							printf("STARTING\n");
						}
					}
					read_it += sizeof(*header);
				} else
					break;
            }
        }
    } else if (ctx->current_state == server_state_names::INIT_EVERYBODY) {
        for (u32 i = 0; i < ctx->clients.used; ++i) {
            comm_server_header header;
            communication *comm = &ctx->clients.comms[i];
            header.name = comm_server_msg_names::INIT_MAP;
            comm_write(comm, &header, sizeof(header));
            
            comm_server_init_map_body init_map_body;
            init_map_body.width = ctx->map.terrain_width;
            init_map_body.height = ctx->map.terrain_height;
            comm_write(comm, &init_map_body, sizeof(init_map_body));

            comm_flush(&ctx->clients.comms[i]);

            header.name = comm_server_msg_names::DISCOVER;
            comm_write(comm, &header, sizeof(header));
            
            comm_server_discover_body discover_body;
            discover_body.x = 10;
            discover_body.y = 7;
            comm_write(comm, &discover_body, sizeof(discover_body));

            header.name = comm_server_msg_names::DISCOVER;
            comm_write(comm, &header, sizeof(header));

            discover_body.x = 9;
            discover_body.y = 6;
            comm_write(comm, &discover_body, sizeof(discover_body));

            comm_flush(&ctx->clients.comms[i]);
        }

        ctx->current_state = server_state_names::TEMP;
    } else if (ctx->current_state == server_state_names::TEMP) {
        for (u32 i = 0; i < ctx->clients.used; ++i) {
            comm_client_header *header;
            communication *comm = &ctx->clients.comms[i];
            s32 len = comm_read(comm, ctx->read_buffer.base, ctx->read_buffer.max);
            u32 read_it = sizeof(comm_shared_header);

            while (read_it < len) {
				if (len - read_it >= sizeof(*header)) {
                    header = (comm_client_header *)(ctx->read_buffer.base + read_it);
                    read_it += sizeof(*header);
                    if (header->name == comm_client_msg_names::PONG) {
                        printf("CLIENT(%d) PONGED\n", i);
                    }
                }
            }
        }
    }

    for (u32 i = 0; i < ctx->clients.used; ++i) {
        if (ctx->clients.connecteds[i]) {
            communication *comm = &ctx->clients.comms[i];
            if (time_get_now_in_ms() - comm->last_sent_time > 300) {
                comm_server_header header;
                header.name = comm_server_msg_names::PING;
                comm_write(comm, &header, sizeof(header));
            }
            
            bool success = comm_flush(comm);
            if (!success) {
                ctx->clients.connecteds[i] = false;
                printf("DISCONNECT\n");
            }
        }
    }

}
