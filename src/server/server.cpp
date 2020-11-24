#define _USE_MATH_DEFINES
#include <math.h>
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
    s32 current_turn_id;

    u32 ent_id_counter;

    struct {
        terrain_names *terrain;
        u32 terrain_width,
            terrain_height;
        doubly_linked_list<entity*> entities;
    } map;

    struct {
        communication *comms;
        bool *admins;
        bool *connecteds;
        u32 max, used;
    } clients;
};

void add_unit(communication *comm, server_context *ctx, v2<u32> pos, unit_names name, s32 owner, memory_arena *mem) {
    comm_server_header header;
    header.name = comm_server_msg_names::ADD_UNIT;
    comm_write(comm, &header, sizeof(header));

    unit *u = (unit *)memory_arena_use(mem, sizeof(*u));
    u->server_id = ctx->ent_id_counter++;
    u->position = pos;
    u->name = name;
    u->owner = owner;
    u->slot = NULL;
    u->type = entity_types::UNIT;

    if (name == unit_names::SOLDIER) {
        u->action_points = 1;
    }
    else if (name == unit_names::CARAVAN) {
        u->action_points = 5;
    }

    ctx->map.entities.push_front(u);

    comm_server_add_unit_body add_unit_body;
    add_unit_body.unit_id = u->server_id;
    add_unit_body.action_points = u->action_points;
    add_unit_body.position = u->position;
    add_unit_body.unit_name = u->name;
    add_unit_body.owner = u->owner;
    comm_write(comm, &add_unit_body, sizeof(add_unit_body));
}

void discover_star(communication *comm, server_context *ctx, v2<u32> center) {
    comm_server_header header;
    comm_server_discover_body discover_body;
    comm_server_discover_body_tile *discover_body_tiles;

    header.name = comm_server_msg_names::DISCOVER;
    comm_write(comm, &header, sizeof(header));

    u32 num = 0;
    v2<u32> pos = center;
    discover_body_tiles = (comm_server_discover_body_tile *)(ctx->temp_buffer.base + ctx->temp_buffer.used);

    u32 idx = pos.y * ctx->map.terrain_width + pos.x;
    discover_body_tiles[num].name = ctx->map.terrain[idx];
    discover_body_tiles[num].position = pos;
    ++num;

    if (pos.x > 0) {
        v2<u32> p = pos;
        p.x -= 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.x < ctx->map.terrain_width - 1) {
        v2<u32> p = pos;
        p.x += 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.y > 0) {
        v2<u32> p = pos;
        p.y -= 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.y < ctx->map.terrain_height - 1) {
        v2<u32> p = pos;
        p.y += 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    discover_body.num = num;
    comm_write(comm, &discover_body, sizeof(discover_body));
    comm_write(comm, discover_body_tiles, sizeof(*discover_body_tiles) * num);
}

void discover_3x3(communication *comm, server_context *ctx, v2<u32> center) {
    comm_server_header header;
    comm_server_discover_body discover_body;
    comm_server_discover_body_tile *discover_body_tiles;

    header.name = comm_server_msg_names::DISCOVER;
    comm_write(comm, &header, sizeof(header));

    u32 num = 0;
    v2<u32> pos = center;
    discover_body_tiles = (comm_server_discover_body_tile *)(ctx->temp_buffer.base + ctx->temp_buffer.used);

    u32 idx = pos.y * ctx->map.terrain_width + pos.x;
    discover_body_tiles[num].name = ctx->map.terrain[idx];
    discover_body_tiles[num].position = pos;
    ++num;

    if (pos.x > 0 && pos.y > 0) {
        v2<u32> p = pos;
        p.x -= 1;
        p.y -= 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.x > 0) {
        v2<u32> p = pos;
        p.x -= 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.x < ctx->map.terrain_width - 1) {
        v2<u32> p = pos;
        p.x += 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.x > 0 && pos.y < ctx->map.terrain_height - 1) {
        v2<u32> p = pos;
        p.x -= 1;
        p.y += 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.y > 0) {
        v2<u32> p = pos;
        p.y -= 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.y < ctx->map.terrain_height - 1) {
        v2<u32> p = pos;
        p.y += 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.x < ctx->map.terrain_width && pos.y > 0) {
        v2<u32> p = pos;
        p.x += 1;
        p.y -= 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    if (pos.x < ctx->map.terrain_width && pos.y < ctx->map.terrain_height) {
        v2<u32> p = pos;
        p.x += 1;
        p.y += 1;
        idx = p.y * ctx->map.terrain_width + p.x; 
        discover_body_tiles[num].name = ctx->map.terrain[idx];
        discover_body_tiles[num].position = p;
        ++num;
    }

    discover_body.num = num;
    comm_write(comm, &discover_body, sizeof(discover_body));
    comm_write(comm, discover_body_tiles, sizeof(*discover_body_tiles) * num);

    auto ent_iter = ctx->map.entities.first;
    while (ent_iter) {
        auto ent = ent_iter->payload;
        if (ent->type == entity_types::STRUCTURE) {
            auto town = (structure *)ent;
            v2<u32> town_pos = town->position;
            v2<s32> d;
            d.x = (s32)town_pos.x - (s32)center.x;
            d.y = (s32)town_pos.y - (s32)center.y;

            if ((d.x >= -1 && d.x <= 1) && (d.y >= -1 && d.y <= 1)) {
                header.name = comm_server_msg_names::DISCOVER_TOWN;
                comm_write(comm, &header, sizeof(header));

                comm_server_discover_town_body discover_town_body;
                discover_town_body.id = town->server_id;
                discover_town_body.owner = town->owner;
                discover_town_body.position = town_pos;
                comm_write(comm, &discover_town_body, sizeof(discover_town_body));
            }
        }

        ent_iter = ent_iter->next;
    }
}

real32 interpolate(real32 a, real32 b, real32 w) {
    return (1.0f - w) * a + w * b;
}

void generate_map(server_context *ctx, memory_arena *mem) {
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

    real32 radius = 3.0f;
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
            real32 value = interpolate(noise_map[idx], circles_map[idx], 1.0);
            if (value > 0.6) {
                ctx->map.terrain[idx] = terrain_names::WATER;
            } else if (value > 0.0) {
                ctx->map.terrain[idx] = terrain_names::GRASS;
            } else {
                ctx->map.terrain[idx] = terrain_names::DESERT;
            }
        }
    }

    u32 prev_num_of_entities = ctx->map.entities.length();

    for (u32 i = 0; i < num_islands; ++i) {
        real32 direction_rad = ((real32)rand() / RAND_MAX) * (M_PI * 2);
        real32 direction_x = cosf(direction_rad);
        real32 direction_y = sinf(direction_rad);
        s32 dir_x = 0;
        s32 dir_y = 0;
        if (direction_x > 0.5)
            dir_x = 1;
        else if (direction_x < -0.5)
            dir_x = -1;

        if (direction_y > 0.5)
            dir_y = 1;
        else if (direction_y < -0.5)
            dir_y = -1;

        u32 iter_x = island_centers[i].x;
        u32 iter_y = island_centers[i].y;
        
        bool stuck = false;
        bool found = false;
        while (!found && !stuck) {
            if (dir_x == 1)
                if(iter_x == ctx->map.terrain_width - 1)
                    stuck = true;
                else
                    iter_x += dir_x;
            if (dir_x == -1)
                if (iter_x == 0)
                    stuck = true;
                else
                    iter_x += dir_x;
            if (dir_y == 1)
                if (iter_y == ctx->map.terrain_height - 1)
                    stuck = true;
                else
                    iter_y += dir_y;
            if (dir_y == -1)
                if (iter_y == 0)
                    stuck = true;
                else
                    iter_y += dir_y;

            u32 idx = iter_y * ctx->map.terrain_width + iter_x;
            if (ctx->map.terrain[idx] == terrain_names::GRASS) {
                found = true;
            }
        }

        if (stuck) {
            --i;
            continue;
        }

        v2<u32> pos = {.x = iter_x, .y = iter_y};
        structure *town = (structure *)memory_arena_use(mem, sizeof(*town));
        ctx->map.entities.push_front(town);
        town->type = entity_types::STRUCTURE;
        town->position = pos;
        town->owner = -1;
        town->construction = unit_names::NONE;
        town->server_id = ctx->ent_id_counter++;
    }

    u32 curr_num_of_entities = ctx->map.entities.length();
    for (u32 i = 0; i < ctx->clients.used; ++i) {
        for (;;) {
            u32 town_id = (rand() % (curr_num_of_entities - prev_num_of_entities));
            entity **ent_iter = ctx->map.entities.get(town_id);
            if (ent_iter) {
                auto ent = *ent_iter;
                if (ent->owner == -1) {
                    ent->owner = i;
                    break;
                }
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

    auto ent_iter = ctx->map.entities.first;
    while (ent_iter) {
        auto ent = ent_iter->payload;
        if (ent->type == entity_types::STRUCTURE) {
            auto town = (structure *)ent;
            header.name = comm_server_msg_names::DISCOVER_TOWN;
            comm_write(comm, &header, sizeof(header));

            comm_server_discover_town_body discover_town_body;
            discover_town_body.id = town->server_id;
            discover_town_body.owner = town->owner;
            discover_town_body.position = town->position;
            comm_write(comm, &discover_town_body, sizeof(discover_town_body));
        }
        ent_iter = ent_iter->next;
    }
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


        generate_map(ctx, mem);

        ctx->current_turn_id = -1;
        
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
						sitrep(SITREP_DEBUG, "CONNECTED(%d)", i);
					} else if(header->name == comm_client_msg_names::START) {
						if (ctx->clients.admins[i]) {
							ctx->current_state = server_state_names::INIT_EVERYBODY;
							sitrep(SITREP_DEBUG, "STARTING");
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
            init_map_body.num_clients = ctx->clients.used;
            init_map_body.your_id = i;
            init_map_body.width = ctx->map.terrain_width;
            init_map_body.height = ctx->map.terrain_height;
            comm_write(comm, &init_map_body, sizeof(init_map_body));

            comm_flush(&ctx->clients.comms[i]);

            auto ent_iter = ctx->map.entities.first;
            while (ent_iter) {
                auto ent = ent_iter->payload;
                if (ent->owner == i && ent->type == entity_types::STRUCTURE) {
                    auto town = (structure *)ent;
                    discover_3x3(comm, ctx, town->position);

                    comm_server_discover_town_body discover_town_body;

                    header.name = comm_server_msg_names::DISCOVER_TOWN;
                    comm_write(comm, &header, sizeof(header));
                    discover_town_body.position = town->position;
                    discover_town_body.id = town->server_id;
                    discover_town_body.owner = town->owner;
                    comm_write(comm, &discover_town_body, sizeof(discover_town_body));

                    add_unit(comm, ctx, town->position, unit_names::SOLDIER, i, mem);

                    comm_flush(comm);
                }
                ent_iter = ent_iter->next;
            }

			if (i == 0) {
                ctx->current_turn_id = i;
                header.name = comm_server_msg_names::YOUR_TURN;
                comm_write(comm, &header, sizeof(header));
			}
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
                    } else if (header->name == comm_client_msg_names::ADMIN_ADD_UNIT) {
                        if (ctx->clients.admins[i]) {
                            comm_client_admin_add_unit_body *body =
                                (comm_client_admin_add_unit_body *)(ctx->read_buffer.base + read_it);
                            read_it += sizeof(*body);

                            add_unit(comm, ctx, body->position, body->name, body->owner_id, mem);
                        }
                    } else if (header->name == comm_client_msg_names::END_TURN) {
                        if ((s32)i == ctx->current_turn_id) {
                            ctx->current_turn_id = ctx->current_turn_id % ctx->clients.used;
                            communication *c = &ctx->clients.comms[ctx->current_turn_id];
                            
                            comm_server_header head;
                            head.name = comm_server_msg_names::YOUR_TURN;
                            comm_write(c, &head, sizeof(head));
                            

                            auto iter = ctx->map.entities.first;
                            while (iter) {
                                auto ent = iter->payload;
                                if (ent->type == entity_types::UNIT) {
                                    unit *u = (unit *)ent;
                                    if (u->owner == ctx->current_turn_id) {
                                        if (u->name == unit_names::SOLDIER) {
                                            u->action_points = 1;
                                        } else if (u->name == unit_names::CARAVAN) {
                                            u->action_points = 5;
                                        }

                                        head.name = comm_server_msg_names::SET_UNIT_ACTION_POINTS;
                                        comm_write(c, &head, sizeof(head));

                                        comm_server_set_unit_action_points_body body;
                                        body.unit_id = u->server_id;
                                        body.new_action_points = u->action_points;
                                        comm_write(c, &body, sizeof(body));
                                    }
                                }
                                iter = iter->next;
                            }

                            iter = ctx->map.entities.first;
                            while (iter) {
                                auto ent = iter->payload;
                                if (ent->type == entity_types::STRUCTURE) {
                                    auto town = (structure *)ent;
                                    if (town->owner == ctx->current_turn_id) {
                                        unit_names name = town->construction;
                                        if (name != unit_names::NONE) {
                                            u32 timer = --town->construction_timer;
                                            if (timer == 0) {
                                                if (name == unit_names::SOLDIER) {
                                                    timer = 3;
                                                } else if (name == unit_names::CARAVAN) {
                                                    timer = 5;
                                                }

                                                add_unit(c, ctx, town->position, name, ctx->current_turn_id, mem);

                                                town->construction_timer = timer;
                                            }
                                        }
                                    }
                                }
                                iter = iter->next;
                            }
                        }
                    } else if (header->name == comm_client_msg_names::SET_CONSTRUCTION) {
                        if (ctx->current_turn_id == i) {
                            comm_client_set_construction_body *body =
                                (comm_client_set_construction_body *)(ctx->read_buffer.base + read_it);
                            read_it += sizeof(*body);
                            auto ent_iter = ctx->map.entities.first;
                            while (ent_iter) {
                                auto ent = ent_iter->payload;
                                s32 owner = ent->owner;
                                if (owner == i && ent->type == entity_types::STRUCTURE) {
                                    auto town = (structure *)ent;
                                    town->construction = body->unit_name;

                                    if (body->unit_name == unit_names::SOLDIER) {
                                        town->construction_timer = 3;
                                    } else if (body->unit_name == unit_names::CARAVAN) {
                                        town->construction_timer = 5;
                                    }

                                    comm_server_header head;
                                    head.name = comm_server_msg_names::CONSTRUCTION_SET;
                                    comm_write(comm, &head, sizeof(head));

                                    comm_server_construction_set_body b;
                                    b.construction_timer = town->construction_timer;
                                    b.town_id = body->town_id;
                                    b.unit_name = body->unit_name;
                                    comm_write(comm, &b, sizeof(b));
                                }
                                ent_iter = ent_iter->next;
                            }
                        }
                    } else if (header->name == comm_client_msg_names::MOVE_UNIT) {
                        comm_client_move_unit_body *body =
                            (comm_client_move_unit_body *)(ctx->read_buffer.base + read_it);
                        read_it += sizeof(*body);
                        auto ent_iter = ctx->map.entities.first;
                        while (ent_iter) {
                            auto ent = ent_iter->payload;
                            s32 owner = ent->owner;
                            if (owner == i && ent->type == entity_types::UNIT &&
                                body->unit_id == ent->server_id) {
                                auto u = (unit *)ent;
                                v2<s32> d = body->delta;

                                u32 action_points = u->action_points;
                                if (action_points > 0) {
                                    action_points--;

                                    v2<u32> pos = u->position;
                                    pos.x += d.x;
                                    pos.y += d.y;

                                    u32 idx = pos.y * ctx->map.terrain_width + pos.x;
                                    bool passable = false;
                                    unit_names name = u->name;
                                    terrain_names terrain = ctx->map.terrain[idx];
                                    if (name == unit_names::SOLDIER) {
                                        if (terrain == terrain_names::GRASS) {
                                            passable = true;
                                        }
                                    } else if (name == unit_names::CARAVAN) {
                                        if (terrain == terrain_names::GRASS ||
                                            terrain == terrain_names::DESERT) {
                                            passable = true;
                                        }
                                    }

                                    if (passable) {
                                        u->action_points = action_points;
                                        u->position = pos;

                                        comm_server_header head;
                                        head.name = comm_server_msg_names::MOVE_UNIT;
                                        comm_server_move_unit_body b;
                                        b.unit_id = u->server_id;
                                        b.action_points_left = u->action_points;
                                        b.new_position = pos;
                                        comm_write(comm, &head, sizeof(head));
                                        comm_write(comm, &b, sizeof(b));

                                        discover_3x3(comm, ctx, pos);

                                        if (u->slot != NULL) {
                                            head.name = comm_server_msg_names::MOVE_UNIT;
                                            b.unit_id = u->slot->server_id;
                                            b.action_points_left = u->slot->action_points;
                                            b.new_position = pos;
                                            comm_write(comm, &head, sizeof(head));
                                            comm_write(comm, &b, sizeof(b));
                                        }
                                    }
                                    break;
                                }
                            }
                            ent_iter = ent_iter->next;
                        }
                    } else if (header->name == comm_client_msg_names::LOAD_UNIT) {
                        comm_client_load_unit_body *body =
                            (comm_client_load_unit_body *)(ctx->read_buffer.base + read_it);
                        read_it += sizeof(*body);
                        auto ent_that_loads = find_entity_by_server_id(ctx->map.entities, body->unit_that_loads);
                        auto ent = find_entity_by_server_id(ctx->map.entities, body->unit_to_load);
                        if (ent && ent->type == entity_types::UNIT && ent_that_loads && ent_that_loads->type == entity_types::UNIT) {
                            auto unit_that_loads = (unit *)ent_that_loads;
                            auto u = (unit *)ent;
                            s32 owner_that_loads = unit_that_loads->owner;
                            s32 owner = ent->owner;
                            if (owner == i && i == owner_that_loads) {
                                u32 action_points = u->action_points;
                                if (action_points > 0) {
                                    --action_points;

                                    u->action_points = action_points;
                                    u->position = unit_that_loads->position;

                                    comm_server_header head;
                                    head.name = comm_server_msg_names::LOAD_UNIT;
                                    comm_write(comm, &head, sizeof(head));
                                    comm_server_load_unit_body b;
                                    b.unit_that_loads = unit_that_loads->server_id;
                                    b.unit_to_load = u->server_id;
                                    b.action_points_left = action_points;
                                    b.new_position = unit_that_loads->position;
                                    comm_write(comm, &b, sizeof(b));

                                    unit_that_loads->slot = u;
                                }
                            }
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
                sitrep(SITREP_INFO, "DISCONNECT");
            }
        }
    }

    ctx->temp_buffer.used = 0;
}
