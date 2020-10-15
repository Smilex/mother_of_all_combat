#define MAP_GRID_WIDTH 1000
#define MAP_GRID_HEIGHT 1000

enum server_state_names {
    AWAITING_CONNECTIONS = 0,
    INIT_EVERYBODY,
    TEMP
};

struct server_context {
    bool is_init;

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
            comm_client_header header;
            communication comm = ctx->clients.comms[i];
            u32 len = comm.recv(comm, &header, sizeof(header));
            
            if (len == sizeof(header) && header.name == comm_client_msg_names::CONNECT) {
                ctx->clients.connecteds[i] = true;
                printf("CONNECTED\n");
            } else if(len == sizeof(header) && header.name == comm_client_msg_names::START) {
                if (ctx->clients.admins[i]) {
                    ctx->current_state = server_state_names::INIT_EVERYBODY;
                }
            }
        }
    } else if (ctx->current_state == server_state_names::INIT_EVERYBODY) {
        for (u32 i = 0; i < ctx->clients.used; ++i) {
            comm_server_header header;
            communication comm = ctx->clients.comms[i];
            header.name = comm_server_msg_names::INIT_MAP;
            comm.send(comm, &header, sizeof(header));
        }

        ctx->current_state = server_state_names::TEMP;
    }
}
