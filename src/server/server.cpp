#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

#define MAP_GRID_WIDTH 80
#define MAP_GRID_HEIGHT 80

enum server_state_names {
    AWAITING_CONNECTIONS = 0,
    INIT_EVERYBODY,
    LOOP
};

struct server_context {
    bool is_init;

    memory_arena temp_buffer, read_buffer;
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

real32 interpolate(real32 a, real32 b, real32 w) {
    return (1.0f - w) * a + w * b;
}

void generate_map(server_context *ctx) {
    real32 *noise_map = (real32 *)memory_arena_use(&ctx->temp_buffer, sizeof(*noise_map)
                                                            * ctx->map.terrain_height
                                                            * ctx->map.terrain_width
                                                    );
    for (u32 y = 0; y < ctx->map.terrain_height; ++y) {
        for (u32 x = 0; x < ctx->map.terrain_width; ++x) {
            u32 idx = y * ctx->map.terrain_height + x;
            real32 nx = (real32)x / ctx->map.terrain_width - 0.5;
            real32 ny = (real32)y / ctx->map.terrain_height - 0.5;
            real32 freq = 9.0f;
            real32 perlin = stb_perlin_noise3(nx * freq, ny * freq, 0, 0, 0, 0) / 2.0 + 0.5;
            noise_map[idx] = perlin;
        }
    }

    // TODO: Find some better way to determine how many islands should be generated
    u32 num_islands = 50;
    real32 min_radius = 4;
    v2<u32> *island_centers = (v2<u32> *)memory_arena_use(&ctx->temp_buffer, sizeof(*island_centers)
                                                            * num_islands);
    for (u32 i = 0; i < num_islands; ++i) {
        bool too_close = false;
        v2<u32> center;
        center.x = rand() % ctx->map.terrain_width;
        center.y = rand() % ctx->map.terrain_height;

        island_centers[i] = center;
    }

    u32 size_of_circles_map = sizeof(*noise_map) * ctx->map.terrain_height * ctx->map.terrain_width;
    real32 *circles_map = (real32 *)memory_arena_use(&ctx->temp_buffer, size_of_circles_map);
    memset(circles_map, 0, size_of_circles_map);

    real32 radius = 4.0f;
    for (u32 i = 0; i < num_islands; ++i) {
        for (u32 y = 0; y < ctx->map.terrain_height; ++y) {
            for (u32 x = 0; x < ctx->map.terrain_height; ++x) {
                u32 idx = y * ctx->map.terrain_height + x;
                v2<u32> pos = {.x = x, .y = y};
                v2<u32> d = (pos - island_centers[i]);
                real32 mag = d.length();
                if (mag <= radius) {
                    if (mag == 0)
                        circles_map[idx] = 1.0f;
                    else
                        circles_map[idx] = 1.0f - mag/radius;
                }
            }
        }
    }

    for (u32 y = 0; y < ctx->map.terrain_height; ++y) {
        for (u32 x = 0; x < ctx->map.terrain_height; ++x) {
            u32 idx = y * ctx->map.terrain_height + x;
            real32 value = interpolate(noise_map[idx], circles_map[idx], 0.0);
            if (value > 0.65) {
                ctx->map.terrain[idx] = terrain_names::GROUND;
            } else {
                ctx->map.terrain[idx] = terrain_names::WATER;
            }
        }
    }
}

void send_entire_map(communication *comm, server_context *ctx) {
    comm_server_header header;
    header.name = comm_server_msg_names::DISCOVER;
    comm_write(comm, &header, sizeof(header));
    
    comm_server_discover_body discover_body;
    discover_body.num = ctx->map.terrain_width * ctx->map.terrain_height;
    comm_write(comm, &discover_body, sizeof(discover_body));

    u32 size_of_tiles = sizeof(comm_server_discover_body_tile)
                        * ctx->map.terrain_width * ctx->map.terrain_height;
    comm_server_discover_body_tile *tiles =
        (comm_server_discover_body_tile *)memory_arena_use(&ctx->temp_buffer,
                                            size_of_tiles);
    u32 X = 0, Y = 0;
    for (u32 i = 0; i < ctx->map.terrain_width * ctx->map.terrain_height; ++i) {
        tiles[i].name = ctx->map.terrain[i];
        tiles[i].position.x = X;
        tiles[i].position.y = Y;

        ++X;
        if (X == ctx->map.terrain_width) {
            ++Y;
            X = 0;
        }
    }
    comm_write(comm, tiles, size_of_tiles);
}

void server_update(memory_arena *mem, communication *comms, u32 num_comms) {
    struct server_context *ctx = (struct server_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));

        ctx->temp_buffer = memory_arena_child(mem, MB(80), "server_memory_temp");
        ctx->read_buffer = memory_arena_child(mem, MB(10), "server_memory_read");

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

        for (u32 i = 0; i < num_comms; ++i) {
            ctx->clients.comms[i] = comms[i];
            ++ctx->clients.used;
        }
        ctx->clients.admins[0] = true;

        ctx->map.terrain_width = MAP_GRID_WIDTH;
        ctx->map.terrain_height = MAP_GRID_HEIGHT;
        ctx->map.terrain = (terrain_names *)memory_arena_use(mem,
                                                sizeof(*ctx->map.terrain)
                                                * ctx->map.terrain_width
                                                * ctx->map.terrain_height
                                                );
        generate_map(ctx);
        
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
						printf("CONNECTED(%d)\n", i);
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
        }

        ctx->current_state = server_state_names::LOOP;
    } else if (ctx->current_state == server_state_names::LOOP) {
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
                    } else if (header->name == comm_client_msg_names::ADMIN_DISCOVER_ENTIRE_MAP) {
                        if (ctx->clients.admins[i]) {
                            send_entire_map(comm, ctx);
                        }
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

    ctx->temp_buffer.used = 0;
}
