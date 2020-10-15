#define MAP_GRID_WIDTH 1000
#define MAP_GRID_HEIGHT 1000

struct server_context {
    bool is_init;

    struct {
        terrain_names *terrain;
        u32 terrain_width,
            terrain_height;
    } map;
};

void server_update(memory_arena *mem, communication *comm) {
    struct server_context *ctx = (struct server_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));

        ctx->map.terrain_width = MAP_GRID_WIDTH;
        ctx->map.terrain_height = MAP_GRID_HEIGHT;
        ctx->map.terrain = (terrain_names *)memory_arena_use(mem,
                                                sizeof(*ctx->map.terrain)
                                                * ctx->map.terrain_width
                                                * ctx->map.terrain_height
                                                );
        
        comm_server_init_map init_map_msg;
        init_map_msg.name = comm_server_msg_names.INIT_MAP;
        init_map_msg.width = ctx->map.terrain_width;
        init_map_msg.height = ctx->map.terrain_height;
        comm->send(&init_map_msg, sizeof(init_map_msg));

        ctx->is_init = true;
    }
}
