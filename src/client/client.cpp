enum class client_terrain_names {
    FOG = 0,
    FOG_TOP_GRASS,
    FOG_TOP_LEFT_GRASS,
    FOG_TOP_RIGHT_GRASS,
    FOG_LEFT_GRASS,
    FOG_RIGHT_GRASS,
    FOG_BOTTOM_GRASS,
    FOG_BOTTOM_LEFT_GRASS,
    FOG_BOTTOM_RIGHT_GRASS,
    FOG_TOP_WATER,
    FOG_TOP_LEFT_WATER,
    FOG_TOP_RIGHT_WATER,
    FOG_LEFT_WATER,
    FOG_RIGHT_WATER,
    FOG_BOTTOM_WATER,
    FOG_BOTTOM_LEFT_WATER,
    FOG_BOTTOM_RIGHT_WATER,
    FOG_TOP_DESERT,
    FOG_TOP_LEFT_DESERT,
    FOG_TOP_RIGHT_DESERT,
    FOG_LEFT_DESERT,
    FOG_RIGHT_DESERT,
    FOG_BOTTOM_DESERT,
    FOG_BOTTOM_LEFT_DESERT,
    FOG_BOTTOM_RIGHT_DESERT,
    GRASS,
    WATER,
    DESERT
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

    Music background_music;

    Texture2D terrain_tex;
    Texture2D entities_tex;

    u32 my_server_id;
    bool my_turn;

    struct {
        u32 width, height;
        terrain_names *terrain;
        client_terrain_names *client_terrain;
        doubly_linked_list<entity *> entities;
    } map;

    struct {
        Color *colors;
        Rectangle *town_srcs;
        Rectangle *soldier_srcs;
        Rectangle *caravan_srcs;
        u32 max, used;
    } clients;

    struct {
        u32 x, y;
    } camera;

    entity *selected_entity;

    struct {
        struct {
            Rectangle rect;
            char *build_names[3];
            s32 build_active;
        } town;
        struct {
            Rectangle rect;
            s32 player;
        } admin;
        struct {
            Rectangle rect;
        } end_turn;
    } gui;

    struct {
        doubly_linked_list<v2<u32>> highlighted_tiles;
        doubly_linked_list<u32> highlighted_priorities;
    } debug;
};

bool is_pt_in_gui(client_context *ctx, Vector2 pt) {
    if (ctx->selected_entity && ctx->selected_entity->type == entity_types::STRUCTURE)
        if (CheckCollisionPointRec(pt, ctx->gui.town.rect))
            return true;

    if (CheckCollisionPointRec(pt, ctx->gui.admin.rect))
        return true;

    if (CheckCollisionPointRec(pt, ctx->gui.end_turn.rect))
        return true;

    return false;
}

bool is_passable_for_unit(unit *u, terrain_names name) {
    if (u->name == unit_names::SOLDIER) {
        if (name == terrain_names::DESERT) return false;
        if (name == terrain_names::WATER) return false;
    } else if (u->name == unit_names::CARAVAN) {
        if (name == terrain_names::WATER) return false;
    }

    return true;
}

u32 get_neighbors_for_unit(client_context *ctx, unit *u, v2<u32> pt, memory_arena *mem, v2<u32> **neighbors) {
    u32 rv = 0;
    v2<s32> d;
    d.x = -1;
    d.y = -1;
    *neighbors = (v2<u32> *)(mem->base + mem->used);
    for (u32 y = 0; y < 3; ++y) {
        for (u32 x = 0; x < 3; ++x) {
            s32 X = (s32)pt.x + d.x + x;
            s32 Y = (s32)pt.y + d.y + y;
            if (X < 0 || X > (s32)ctx->map.width - 1 ||
                Y < 0 || Y > (s32)ctx->map.height - 1) {
                continue;
            }
            if ((s32)pt.x == X && (s32)pt.y == Y)
                continue;

            u32 idx = Y * ctx->map.width + X;
            terrain_names name = ctx->map.terrain[idx];
            bool passable = is_passable_for_unit(u, name);

            if (passable) {
                v2<u32> *n = (v2<u32> *)memory_arena_use(mem, sizeof(*n));
                n->x = X;
                n->y = Y;
                ++rv;
            }
        }
    }

    return rv;
}

u32 get_path_for_unit(client_context *ctx, unit *u, v2<u32> goal, memory_arena *mem, v2<u32> **paths) {
    auto frontier = priority_queue<v2<u32>>();
    frontier.push(u->position, 0);
    auto came_from = dictionary<v2<u32>, v2<u32>>();
    came_from.push(u->position, u->position);
    auto heuristic = [](v2<u32> a, v2<u32> b) {
        s32 dx = abs((s32)a.x - (s32)b.x);
        s32 dy = abs((s32)a.y - (s32)b.y);
        return (dx + dy) + MIN(dx, dy);
    };

    ctx->debug.highlighted_tiles.clear();
    ctx->debug.highlighted_priorities.clear();
    while (!frontier.empty()) {
        ctx->debug.highlighted_priorities.push_back(frontier.first->priority);
        auto current = frontier.pop();
        ctx->debug.highlighted_tiles.push_back(current);

        if (current == goal) {
            break;
        }

        v2<u32> *neighbors;
        u32 num_neighbors = get_neighbors_for_unit(ctx, u, current, mem, &neighbors);
        for (u32 i = 0; i < num_neighbors; ++i) {
            v2<u32> *from = came_from.get(neighbors[i]);
            if (!from) {
                v2<s32> s, e, d;
                s.x = (s32)u->position.x;
                s.y = (s32)u->position.y;
                e.x = (s32)neighbors[i].x;
                e.y = (s32)neighbors[i].y;
                d = e - s;
                if (d.length() <= 50) {
                    u32 priority = heuristic(goal, neighbors[i]);
                    frontier.push(neighbors[i], priority);
                    came_from.push(neighbors[i], current);
                }
            }
        }
    }

    *paths = (v2<u32> *)(mem->base + mem->used);
    u32 paths_used = 0;
    v2<u32> current = goal;
    while (current != u->position) {
        (*paths)[paths_used++] = current;
        memory_arena_use(mem, sizeof(current));

        v2<u32> *from = came_from.get(current);
        if (from) {
            current = *from;
        } else
            return 0;
    }

    for (u32 i = 0; i < paths_used / 2; ++i) {
        v2<u32> temp = (*paths)[i];
        (*paths)[i] = (*paths)[(paths_used - 1) - i];
        (*paths)[(paths_used - 1) - i] = temp;
    }

    return paths_used;
}

template <class T>
Vector2 v2_to_Vector2(v2<T> v) {
    Vector2 rv;
    rv.x = (real32)v.x;
    rv.y = (real32)v.y;
    return rv;
}

void initialize_map(client_context *ctx, memory_arena *mem, u32 width, u32 height) {
    ctx->map.width = width;
    ctx->map.height = height;

    ctx->map.terrain = (terrain_names *)memory_arena_use(mem, sizeof(*ctx->map.terrain)
                                                                    * ctx->map.width
                                                                    * ctx->map.height
                                                                );
    ctx->map.client_terrain = (client_terrain_names *)memory_arena_use(mem, sizeof(*ctx->map.client_terrain)
                                                                    * ctx->map.width
                                                                    * ctx->map.height
                                                                );
}

void update_client_map(client_context *ctx) {
    for (u32 y = 0; y < ctx->map.height; ++y) {
        for (u32 x = 0; x < ctx->map.width; ++x) {
            bool fog_top = false;
            bool fog_left = false;
            bool fog_right = false;
            bool fog_bottom = false;
            if (y == 0) {
                fog_top = true;
            } else {
                u32 idx_top = ctx->map.width * (y - 1) + x;
                terrain_names name_top = ctx->map.terrain[idx_top];
                if (name_top == terrain_names::FOG) {
                    fog_top = true;
                }
            }

            if (y == ctx->map.height - 1) {
                fog_bottom = true;
            } else {
                u32 idx_bottom = ctx->map.width * (y + 1) + x;
                terrain_names name_bottom = ctx->map.terrain[idx_bottom];
                if (name_bottom == terrain_names::FOG) {
                    fog_bottom = true;
                }
            }

            if (x == 0) {
                fog_left = true;
            } else {
                u32 idx_left = ctx->map.width * y + (x - 1);
                terrain_names name_left = ctx->map.terrain[idx_left];
                if (name_left == terrain_names::FOG) {
                    fog_left = true;
                }
            }

            if (x == ctx->map.width - 1) {
                fog_right = true;
            } else {
                u32 idx_right = ctx->map.width * y + (x + 1);
                terrain_names name_right = ctx->map.terrain[idx_right];
                if (name_right == terrain_names::FOG) {
                    fog_right = true;
                }
            }

            client_terrain_names name = client_terrain_names::FOG;
            u32 idx = ctx->map.width * y + x;
            if (ctx->map.terrain[idx] == terrain_names::GRASS) {
                if (fog_top) {
                    if (fog_left) {
                        name = client_terrain_names::FOG_TOP_LEFT_GRASS;
                    } else if (fog_right) {
                        name = client_terrain_names::FOG_TOP_RIGHT_GRASS;
                    } else {
                        name = client_terrain_names::FOG_TOP_GRASS;
                    }
                } else if (fog_bottom) {
                    if (fog_left) {
                        name = client_terrain_names::FOG_BOTTOM_LEFT_GRASS;
                    } else if (fog_right) {
                        name = client_terrain_names::FOG_BOTTOM_RIGHT_GRASS;
                    } else {
                        name = client_terrain_names::FOG_BOTTOM_GRASS;
                    }
                } else if (fog_left) {
                    name = client_terrain_names::FOG_LEFT_GRASS;
                } else if (fog_right) {
                    name = client_terrain_names::FOG_RIGHT_GRASS;
                } else {
                    name = client_terrain_names::GRASS;
                }

            } else if (ctx->map.terrain[idx] == terrain_names::WATER) {
                if (fog_top) {
                    if (fog_left) {
                        name = client_terrain_names::FOG_TOP_LEFT_WATER;
                    } else if (fog_right) {
                        name = client_terrain_names::FOG_TOP_RIGHT_WATER;
                    } else {
                        name = client_terrain_names::FOG_TOP_WATER;
                    }
                } else if (fog_bottom) {
                    if (fog_left) {
                        name = client_terrain_names::FOG_BOTTOM_LEFT_WATER;
                    } else if (fog_right) {
                        name = client_terrain_names::FOG_BOTTOM_RIGHT_WATER;
                    } else {
                        name = client_terrain_names::FOG_BOTTOM_WATER;
                    }
                } else if (fog_left) {
                    name = client_terrain_names::FOG_LEFT_WATER;
                } else if (fog_right) {
                    name = client_terrain_names::FOG_RIGHT_WATER;
                } else {
                    name = client_terrain_names::WATER;
                }
            } else if (ctx->map.terrain[idx] == terrain_names::DESERT) {
                if (fog_top) {
                    if (fog_left) {
                        name = client_terrain_names::FOG_TOP_LEFT_DESERT;
                    } else if (fog_right) {
                        name = client_terrain_names::FOG_TOP_RIGHT_DESERT;
                    } else {
                        name = client_terrain_names::FOG_TOP_DESERT;
                    }
                } else if (fog_bottom) {
                    if (fog_left) {
                        name = client_terrain_names::FOG_BOTTOM_LEFT_DESERT;
                    } else if (fog_right) {
                        name = client_terrain_names::FOG_BOTTOM_RIGHT_DESERT;
                    } else {
                        name = client_terrain_names::FOG_BOTTOM_DESERT;
                    }
                } else if (fog_left) {
                    name = client_terrain_names::FOG_LEFT_DESERT;
                } else if (fog_right) {
                    name = client_terrain_names::FOG_RIGHT_DESERT;
                } else {
                    name = client_terrain_names::DESERT;
                }
            }

            ctx->map.client_terrain[idx] = name;
        }
    }
}

void draw_entity(client_context *ctx, entity *ent) {
    real32 X = 0, Y = 0;
    u32 tile_width = 32;
    u32 tile_height = 32;

    v2<u32> pos = ent->position;
    X = pos.x * tile_width;
    Y = pos.y * tile_height;
    X -= ctx->camera.x * tile_width;
    Y -= ctx->camera.y * tile_height;

    if (ent->type == entity_types::STRUCTURE) {
        Rectangle src = ctx->clients.town_srcs[ent->owner];
        DrawTextureRec(ctx->entities_tex, src, CLITERAL(Vector2){X, Y}, WHITE);
    } else if (ent->type == entity_types::UNIT) {
        auto u = (unit *)ent;
        if (u->name == unit_names::SOLDIER) {
            Rectangle src = ctx->clients.soldier_srcs[ent->owner];
            DrawTextureRec(ctx->entities_tex, src, CLITERAL(Vector2){X, Y}, WHITE);
        } else if (u->name == unit_names::CARAVAN) {
            Rectangle src = ctx->clients.caravan_srcs[ent->owner];
            DrawTextureRec(ctx->entities_tex, src, CLITERAL(Vector2){X, Y}, WHITE);
        }
    }
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
            Vector2 position;
            position.x = X;
            position.y = Y;
            Color tint = WHITE;
            Rectangle rect;
            if (ctx->map.client_terrain[it] == client_terrain_names::FOG)
                rect = CLITERAL(Rectangle){0, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_GRASS)
                rect = CLITERAL(Rectangle){32 * 4, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_LEFT_GRASS)
                rect = CLITERAL(Rectangle){32 * 8, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_RIGHT_GRASS)
                rect = CLITERAL(Rectangle){32 * 9, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_LEFT_GRASS)
                rect = CLITERAL(Rectangle){32 * 5, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_RIGHT_GRASS)
                rect = CLITERAL(Rectangle){32 * 6, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_GRASS)
                rect = CLITERAL(Rectangle){32 * 7, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_LEFT_GRASS)
                rect = CLITERAL(Rectangle){32 * 10, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_RIGHT_GRASS)
                rect = CLITERAL(Rectangle){32 * 11, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::GRASS)
                rect = CLITERAL(Rectangle){32 * 2, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_WATER)
                rect = CLITERAL(Rectangle){32 * 0, 32, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_LEFT_WATER)
                rect = CLITERAL(Rectangle){32 * 4, 32, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_RIGHT_WATER)
                rect = CLITERAL(Rectangle){32 * 5, 32, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_LEFT_WATER)
                rect = CLITERAL(Rectangle){32 * 1, 32, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_RIGHT_WATER)
                rect = CLITERAL(Rectangle){32 * 2, 32, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_WATER)
                rect = CLITERAL(Rectangle){32 * 3, 32, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_LEFT_WATER)
                rect = CLITERAL(Rectangle){32 * 6, 32, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_RIGHT_WATER)
                rect = CLITERAL(Rectangle){32 * 7, 32, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::WATER)
                rect = CLITERAL(Rectangle){32 * 3, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::DESERT)
                rect = CLITERAL(Rectangle){32 * 1, 0, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_DESERT)
                rect = CLITERAL(Rectangle){32 * 0, 64, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_LEFT_DESERT)
                rect = CLITERAL(Rectangle){32 * 4, 64, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_TOP_RIGHT_DESERT)
                rect = CLITERAL(Rectangle){32 * 5, 64, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_LEFT_DESERT)
                rect = CLITERAL(Rectangle){32 * 1, 64, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_RIGHT_DESERT)
                rect = CLITERAL(Rectangle){32 * 2, 64, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_DESERT)
                rect = CLITERAL(Rectangle){32 * 3, 64, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_LEFT_DESERT)
                rect = CLITERAL(Rectangle){32 * 6, 64, 32, 32};
            else if (ctx->map.client_terrain[it] == client_terrain_names::FOG_BOTTOM_RIGHT_DESERT)
                rect = CLITERAL(Rectangle){32 * 7, 64, 32, 32};

            DrawTextureRec(ctx->terrain_tex, rect, position, tint);
            X = X + tile_width;
        }
        X = 0;
        Y = Y + tile_height;
    }

    u32 len = ctx->debug.highlighted_tiles.length();
    for (u32 i = 0; i < len; ++i) {
        v2<u32> *pos_world = ctx->debug.highlighted_tiles.get(i);
        v2<real32> pos_screen;
        pos_screen.x = (pos_world->x - ctx->camera.x) * tile_width;
        pos_screen.y = (pos_world->y - ctx->camera.y) * tile_height;

        DrawRectangleLines(pos_screen.x, pos_screen.y, tile_width, tile_height, PURPLE);
        u32 *priority = ctx->debug.highlighted_priorities.get(i);
        if (priority) {
            char buf[15];
            snprintf(buf, 15, "%u", *priority);
            DrawText(buf, pos_screen.x, pos_screen.y, 16, WHITE);
        }
    }

    auto ent_iter = ctx->map.entities.first;
    auto prev_ent_iter = ent_iter;
    while (ent_iter) {
        prev_ent_iter = ent_iter;
        ent_iter = ent_iter->next;
    }
    ent_iter = prev_ent_iter;
    while (ent_iter) {
        auto ent = ent_iter->payload;
        if (ctx->selected_entity == ent) {
            ent_iter = ent_iter->prev;
            continue;
        }

        draw_entity(ctx, ent);

        ent_iter = ent_iter->prev;
    }

    if (ctx->selected_entity) {
        draw_entity(ctx, ctx->selected_entity);
    }
}

CLIENT_UPDATE(client_update) {
    client_context *ctx = (client_context *)mem->base;

    if (!ctx->is_init) {
        memory_arena_use(mem, sizeof(*ctx));
        ctx->read_buffer = memory_arena_child(mem, MB(50), "client_memory_read");
        ctx->temp_mem = memory_arena_child(mem, MB(20), "client_memory_temp");

        ctx->clients.used = 1;
        ctx->clients.max = 32;
        ctx->clients.colors = (Color *)memory_arena_use(mem, sizeof(*ctx->clients.colors)
                                                        * ctx->clients.max
                                                        );
        ctx->clients.town_srcs = (Rectangle *)memory_arena_use(mem, sizeof(*ctx->clients.town_srcs)
                                                        * ctx->clients.max
                                                        );
        ctx->clients.soldier_srcs = (Rectangle *)memory_arena_use(mem, sizeof(*ctx->clients.soldier_srcs)
                                                        * ctx->clients.max
                                                        );
        ctx->clients.caravan_srcs = (Rectangle *)memory_arena_use(mem, sizeof(*ctx->clients.caravan_srcs)
                                                            * ctx->clients.max
                                                            );

        {
            u32 num_entities = 3;
            real32 X = 0, Y = 0;
            real32 client_tex_width = 32 * ctx->clients.max;
            real32 client_tex_height = 32 * num_entities;
            Image full_img = GenImageColor(client_tex_width, client_tex_height, PINK);
            Image entities_img = LoadImage("assets/entities.png");

            Image entities_copy = ImageCopy(entities_img);
            Color replace_color = CLITERAL(Color){0,0,0,0};
            Color with_color = GRAY;
            ImageColorReplace(&entities_copy, replace_color, with_color);

            Rectangle src = CLITERAL(Rectangle){0, 0, 32, 32};
            Rectangle dst;
            dst.x = X;
            dst.y = Y;
            dst.width = 32;
            dst.height = 32 * num_entities;
            ImageDraw(&full_img, entities_copy, src, dst, WHITE);
            UnloadImage(entities_copy);

            ctx->clients.town_srcs[0] = CLITERAL(Rectangle){X, 0, 32, 32};
            ctx->clients.soldier_srcs[0] = CLITERAL(Rectangle){X, 32, 32, 32};
            ctx->clients.caravan_srcs[0] = CLITERAL(Rectangle){X, 64, 32, 32};

            X += 32;
            for (u32 i = 1; i < ctx->clients.max; ++i) {
                u8 red = rand() % 255;
                u8 green = rand() % 255;
                u8 blue = rand() % 255;
                ctx->clients.colors[i] = CLITERAL(Color){red, green, blue, 255};

                entities_copy = ImageCopy(entities_img);
                replace_color = CLITERAL(Color){0,0,0,0};
                with_color = ctx->clients.colors[i];
                ImageColorReplace(&entities_copy, replace_color, with_color);

                src = CLITERAL(Rectangle){0, 0, 32, 32};
                dst;
                dst.x = X;
                dst.y = Y;
                dst.width = 32;
                dst.height = 32 * num_entities;
                ImageDraw(&full_img, entities_copy, src, dst, WHITE);
                UnloadImage(entities_copy);

                ctx->clients.town_srcs[i] = CLITERAL(Rectangle){X, 0, 32, 32};
                ctx->clients.soldier_srcs[i] = CLITERAL(Rectangle){X, 32, 32, 32};
                ctx->clients.caravan_srcs[i] = CLITERAL(Rectangle){X, 64, 32, 32};

                X += 32;
            }
            UnloadImage(entities_img);

            ctx->entities_tex = LoadTextureFromImage(full_img);
            UnloadImage(full_img);
        }

        ctx->selected_entity = NULL;

        real32 scr_width = GetScreenWidth();
        real32 scr_height = GetScreenHeight();

        ctx->gui.town.rect = CLITERAL(Rectangle){scr_width - 200, 0.0f, 200.0f, scr_height / 2};
        ctx->gui.town.build_names[0] = "None";
        ctx->gui.town.build_names[1] = "Soldier";
        ctx->gui.town.build_names[2] = "Caravan";

        ctx->gui.admin.rect = CLITERAL(Rectangle){scr_width - 200, scr_height / 2, 200, scr_height / 2};

        ctx->gui.end_turn.rect = CLITERAL(Rectangle){scr_width / 2 - 60, scr_height - 30, 120, 30};

        ctx->background_music = LoadMusicStream("assets/trouble_with_tribals.mp3");
        SetMusicVolume(ctx->background_music, 0.03);

        ctx->terrain_tex = LoadTexture("assets/terrain.png");
        {
            
        }

        ctx->current_screen = client_screen_names::MAIN_MENU;

        ctx->is_init = true;
    }

    if (ctx->current_screen == client_screen_names::MAIN_MENU) {
        comm_server_header *header;
        s32 len = comm_read(comm, ctx->read_buffer.base, ctx->read_buffer.max); 
        u32 buf_it = sizeof(comm_shared_header);
        if (len > 0) {
            while (len - buf_it >= sizeof(*header)) {
                header = (comm_server_header *)(ctx->read_buffer.base + buf_it);
                buf_it += sizeof(*header);
                if (header->name == comm_server_msg_names::PING) {
                    comm_client_header client_header;
                    client_header.name = comm_client_msg_names::PONG;
                    comm_write(comm, &client_header, sizeof(client_header));
                }
            }
        }
    } else if (ctx->current_screen == client_screen_names::INITIALIZE_GAME) {
        comm_server_header *header;
        s32 len = comm_read(comm, ctx->read_buffer.base, ctx->read_buffer.max); 
        u32 buf_it = sizeof(comm_shared_header);
        if (len > 0) {
            while (len - buf_it >= sizeof(*header)) {
                header = (comm_server_header *)(ctx->read_buffer.base + buf_it);
                buf_it += sizeof(*header);
                if (header->name == comm_server_msg_names::INIT_MAP) {
                    comm_server_init_map_body *init_map_body;
                    if (len - buf_it >= sizeof(*init_map_body)) {
                        init_map_body = (comm_server_init_map_body *)(ctx->read_buffer.base + buf_it);
                        buf_it += sizeof(*init_map_body);
                        ctx->my_server_id = init_map_body->your_id;
                        ctx->clients.used = init_map_body->num_clients + 1;
                        initialize_map(ctx, mem, init_map_body->width, init_map_body->height);
                        ctx->current_screen = client_screen_names::GAME;
                        sitrep(SITREP_DEBUG, "INIT_EVERYBODY");
                        PlayMusicStream(ctx->background_music);
                    }
                } else if (header->name == comm_server_msg_names::PING) {
                    comm_client_header client_header;
                    client_header.name = comm_client_msg_names::PONG;
                    comm_write(comm, &client_header, sizeof(client_header));
                } else if (header->name == comm_server_msg_names::YOUR_TURN) {
                    sitrep(SITREP_DEBUG, "WHOOP");
                    ctx->my_turn = true;
                }
            }
        }
    } else if (ctx->current_screen == client_screen_names::GAME) {
        UpdateMusicStream(ctx->background_music);

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

        if (!is_pt_in_gui(ctx, GetMousePosition())) {
            Vector2 mouse_pos = GetMousePosition();
            v2<u32> mouse_tile_pos;
            real32 tile_width = 32.0f;
            real32 tile_height = 32.0f;
            mouse_tile_pos.x = (u32)floor(mouse_pos.x / tile_width);
            mouse_tile_pos.y = (u32)floor(mouse_pos.y / tile_height);
            mouse_tile_pos.x += ctx->camera.x;
            mouse_tile_pos.y += ctx->camera.y;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (ctx->selected_entity != NULL) {
                    if (ctx->selected_entity->type == entity_types::UNIT) {
                        auto u = (unit *)ctx->selected_entity;
                        v2<u32> prev_path = u->position;
                        v2<u32> *paths;
                        u32 num_paths = get_path_for_unit(ctx, u, mouse_tile_pos, &ctx->temp_mem, &paths);

                        for (u32 i = 0; i < num_paths; ++i) {
                            u32 num_entities;
                            entity **entities = find_entities_at_position(ctx->map.entities, paths[i], &ctx->temp_mem, &num_entities);
                            v2<s32> d;
                            d.x = (s32)paths[i].x - (s32)prev_path.x;
                            d.y = (s32)paths[i].y - (s32)prev_path.y;

                            comm_client_header header;
                            if (u->loaded_by != NULL) {
                                header.name = comm_client_msg_names::UNLOAD_UNIT;
                                comm_write(comm, &header, sizeof(header));
                                comm_client_unload_unit_body body;
                                body.unit_id = u->server_id;
                                body.delta = d;
                                comm_write(comm, &body, sizeof(body));
                            } else {
                                unit *u_that_loads = NULL;
                                if (u->name == unit_names::SOLDIER) {
                                    for (u32 j = 0; j < num_entities; ++j) {
                                        if (entities[j]->type == entity_types::UNIT) {
                                            auto u = (unit *)(entities[j]);
                                            if (u->name == unit_names::CARAVAN) {
                                                u_that_loads = u;
                                                break;
                                            }
                                        }
                                    }
                                }

                                if (u_that_loads != NULL) {
                                    header.name = comm_client_msg_names::LOAD_UNIT;
                                    comm_write(comm, &header, sizeof(header));
                                    comm_client_load_unit_body body;
                                    body.unit_that_loads = u_that_loads->server_id; 
                                    body.unit_to_load = u->server_id;
                                    comm_write(comm, &body, sizeof(body));
                                } else {
                                    header.name = comm_client_msg_names::MOVE_UNIT;
                                    comm_write(comm, &header, sizeof(header));
                                    comm_client_move_unit_body body;
                                    body.unit_id = u->server_id;
                                    body.delta = d;
                                    comm_write(comm, &body, sizeof(body));
                                }
                            }

                            prev_path = paths[i];
                        }
                    }
                }
            } else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                u32 num_entities;
                entity **entities = find_entities_at_position(ctx->map.entities, mouse_tile_pos, &ctx->temp_mem, &num_entities);
                if (num_entities > 0) {
                    bool found = false;
                    for (u32 i = 0; i < num_entities; ++i) {
                        if (entities[i] == ctx->selected_entity) {
                            found = true;
                            if (i == num_entities - 1) {
                                ctx->selected_entity = entities[0];
                            } else {
                                ctx->selected_entity = entities[i + 1];
                            }
                            break;
                        }
                    }
                    if (!found) {
                        ctx->selected_entity = entities[0];
                    }
                } else {
                    ctx->selected_entity = NULL;
                }
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
                            ctx->map.terrain[tile->position.y * ctx->map.width + tile->position.x] = tile->name;
                        }
                    }
                    update_client_map(ctx);
                } else if (header->name == comm_server_msg_names::DISCOVER_TOWN) {
                    comm_server_discover_town_body *discover_town_body;
                    if (len - buf_it >= sizeof(*discover_town_body)) {
                        discover_town_body = (comm_server_discover_town_body *)(ctx->read_buffer.base + buf_it);
                        buf_it += sizeof(*discover_town_body);

                        bool found = false;
                        auto ent_iter = ctx->map.entities.first;
                        while (ent_iter) {
                            auto ent = ent_iter->payload;
                            if (ent->server_id == discover_town_body->id) {
                                found = true;
                            }
                            ent_iter = ent_iter->next;
                        }

                        if (!found) {
                            structure *town = (structure *)memory_arena_use(mem, sizeof(*town));
                            ctx->map.entities.push_front(town);
                            town->type = entity_types::STRUCTURE;
                            town->position = discover_town_body->position;
                            town->owner = discover_town_body->owner;
                            town->server_id = discover_town_body->id;

                            if (discover_town_body->owner == ctx->my_server_id) {
                                s32 camera_x = (s32)discover_town_body->position.x;
                                s32 camera_y = (s32)discover_town_body->position.y;
                                u32 num_tiles_in_scr_width = scr_width / 32;
                                u32 num_tiles_in_scr_height = scr_height / 32;
                                u32 half_scr_width = num_tiles_in_scr_width >> 1;
                                u32 half_scr_height = num_tiles_in_scr_height >> 1;
                                camera_x -= half_scr_width;
                                if (camera_x < 0)
                                    camera_x = 0;
                                camera_y -= half_scr_height;
                                if (camera_y < 0)
                                    camera_y = 0;
                                ctx->camera.x = camera_x;
                                ctx->camera.y = camera_y;
                            }
                        }
                    }
                } else if (header->name == comm_server_msg_names::YOUR_TURN) {
                    ctx->my_turn = true;
                } else if (header->name == comm_server_msg_names::SET_UNIT_ACTION_POINTS) {
                    comm_server_set_unit_action_points_body *body;
                    if (len - buf_it >= sizeof(*body)) {
                        body = (comm_server_set_unit_action_points_body *)(ctx->read_buffer.base + buf_it);
                        buf_it += sizeof(*body);
                        auto ent_iter = ctx->map.entities.first;
                        while (ent_iter) {
                            auto ent = ent_iter->payload;
                            if (ent->type == entity_types::UNIT) {
                                auto u = (unit *)ent;
                                if (u->server_id == body->unit_id) {
                                    u->action_points = body->new_action_points;
                                }
                            }
                            ent_iter = ent_iter->next;
                        }
                    }
                } else if (header->name == comm_server_msg_names::CONSTRUCTION_SET) {
                    comm_server_construction_set_body *construction_set_body;
                    if (len - buf_it >= sizeof(*construction_set_body)) {
                        construction_set_body = (comm_server_construction_set_body *)(ctx->read_buffer.base + buf_it);
                        buf_it += sizeof(*construction_set_body);

                        auto ent_iter = ctx->map.entities.first;
                        while (ent_iter) {
                            auto ent = ent_iter->payload;
                            if (ent->type == entity_types::STRUCTURE) {
                                auto town = (structure *)ent;
                                if (town->server_id == construction_set_body->town_id) {
                                    town->construction = construction_set_body->unit_name;
                                }
                            }
                            ent_iter = ent_iter->next;
                        }
                        if (ctx->selected_entity != NULL && ctx->selected_entity->server_id == construction_set_body->town_id) {
                            ctx->gui.town.build_active = (s32)construction_set_body->unit_name;
                        }
                    }
                } else if (header->name == comm_server_msg_names::ADD_UNIT) {
                    comm_server_add_unit_body *add_unit_body;
                    if (len - buf_it >= sizeof(*add_unit_body)) {
                        add_unit_body = (comm_server_add_unit_body *)(ctx->read_buffer.base + buf_it);
                        buf_it += sizeof(*add_unit_body);

                        unit *u = (unit *)memory_arena_use(mem, sizeof(*u));
                        ctx->map.entities.push_front(u);
                        ctx->selected_entity = u;
                        u->type = entity_types::UNIT;
                        u->server_id = add_unit_body->unit_id;
                        u->position = add_unit_body->position;
                        u->name = add_unit_body->unit_name;
                        u->action_points = add_unit_body->action_points;
                        u->owner = add_unit_body->owner;
                    }
                } else if (header->name == comm_server_msg_names::MOVE_UNIT) {
                    comm_server_move_unit_body *move_unit_body;
                    if (len - buf_it >= sizeof(*move_unit_body)) {
                        move_unit_body = (comm_server_move_unit_body *)(ctx->read_buffer.base + buf_it);
                        buf_it += sizeof(*move_unit_body);

                        auto ent_iter = ctx->map.entities.first;
                        while (ent_iter) {
                            auto ent = ent_iter->payload;
                            if (ent->type == entity_types::UNIT) {
                                auto u = (unit *)ent;
                                if (u->server_id == move_unit_body->unit_id) {
                                    u->position = move_unit_body->new_position;
                                    u->action_points = move_unit_body->action_points_left;
                                    break;
                                }
                            }

                            ent_iter = ent_iter->next;
                        }
                    }
                } else if (header->name == comm_server_msg_names::LOAD_UNIT) {
                    comm_server_load_unit_body *load_unit_body;
                    if (len - buf_it >= sizeof(*load_unit_body)) {
                        load_unit_body = (comm_server_load_unit_body *)(ctx->read_buffer.base + buf_it);
                        buf_it += sizeof(*load_unit_body);

                        auto ent_that_loads = find_entity_by_server_id(ctx->map.entities, load_unit_body->unit_that_loads);
                        auto ent_to_load = find_entity_by_server_id(ctx->map.entities, load_unit_body->unit_to_load);
                        if (ent_that_loads && ent_to_load) {
                            if (ent_that_loads->type == entity_types::UNIT &&
                                ent_to_load->type == entity_types::UNIT) {
                                auto u_to_load = (unit *)ent_to_load;
                                auto u_that_loads = (unit *)ent_that_loads;
                                u_to_load->action_points = load_unit_body->action_points_left;
                                u_to_load->position = load_unit_body->new_position;
                                u_that_loads->slot = u_to_load;
                                u_to_load->loaded_by = u_that_loads;
                            }
                        }
                    }
                } else if (header->name == comm_server_msg_names::UNLOAD_UNIT) {
                    comm_server_unload_unit_body *unload_unit_body;
                    if (len - buf_it >= sizeof(*unload_unit_body)) {
                        unload_unit_body = (comm_server_unload_unit_body *)(ctx->read_buffer.base + buf_it);
                        buf_it += sizeof(*unload_unit_body);

                        auto ent = find_entity_by_server_id(ctx->map.entities, unload_unit_body->unit_id);
                        if (ent) {
                            if (ent->type == entity_types::UNIT) {
                                auto u = (unit *)ent;
                                u->action_points = unload_unit_body->action_points_left;
                                u->position = unload_unit_body->new_position;
                                if (u->loaded_by) {
                                    u->loaded_by->slot = NULL;
                                    u->loaded_by = NULL;
                                }
                            }
                        }
                    }
                } else if (header->name == comm_server_msg_names::PING) {
                    comm_client_header client_header;
                    client_header.name = comm_client_msg_names::PONG;
                    comm_write(comm, &client_header, sizeof(client_header));
                } else {
                    sitrep(SITREP_WARNING, "Unhandled server message (%u)", header->name);
                }
            }
        }
    }

    comm_flush(comm);
}

CLIENT_RENDER(client_render) {
    client_context *ctx = (client_context *)mem->base;

    BeginDrawing();
        ClearBackground(WHITE);

        if (ctx->current_screen == client_screen_names::MAIN_MENU) {
            real32 scrWidth = GetScreenWidth();
            real32 scrHeight = GetScreenHeight();
            if (GuiButton(CLITERAL(Rectangle){scrWidth / 2 - 150 / 2, scrHeight / 2 - 15, 150, 30}, "Start")) {
                comm_client_header header;
                header.name = comm_client_msg_names::START;

                comm_write(comm, &header, sizeof(header));
                ctx->current_screen = client_screen_names::INITIALIZE_GAME;
            }
        } else if (ctx->current_screen == client_screen_names::INITIALIZE_GAME) {
        } else if (ctx->current_screen == client_screen_names::GAME) {
            real32 scr_width = GetScreenWidth();
            real32 scr_height = GetScreenHeight();


            draw_map(ctx);

            if (ctx->selected_entity != NULL) {
                v2<real32> unit_world_pos;
                v2<real32> unit_screen_pos;
                v2<real32> unit_screen_pos_centered;
                unit_world_pos.x = ctx->selected_entity->position.x * 32;
                unit_world_pos.y = ctx->selected_entity->position.y * 32;
                unit_screen_pos.x = unit_world_pos.x - (ctx->camera.x * 32);
                unit_screen_pos.y = unit_world_pos.y - (ctx->camera.y * 32);
                unit_screen_pos_centered.x = unit_screen_pos.x + 16;
                unit_screen_pos_centered.y = unit_screen_pos.y + 16;

                DrawLineV(v2_to_Vector2(unit_screen_pos_centered), GetMousePosition(), BLACK);
            }

            if (GuiButton(ctx->gui.end_turn.rect, "END TURN")) {
                comm_client_header header;
                header.name = comm_client_msg_names::END_TURN;

                comm_write(comm, &header, sizeof(header));

                ctx->my_turn = false;
            }



            if (ctx->selected_entity != NULL && ctx->selected_entity->type == entity_types::STRUCTURE) {
                Rectangle town_window = ctx->gui.town.rect;
                bool close = GuiWindowBox(town_window, "Town");
                GuiLabel(CLITERAL(Rectangle){town_window.x + 10, town_window.y + 30, town_window.width - 20, 20}, "Build:");
                s32 prev_active = ctx->gui.town.build_active;
                s32 curr_active = GuiToggleGroup(CLITERAL(Rectangle){town_window.x + 20, town_window.y + 50, town_window.width - 40, 30}, TextJoin((const char**)ctx->gui.town.build_names, 3, "\n"), ctx->gui.town.build_active);
                if (prev_active != curr_active) {
                    comm_client_header header;
                    header.name = comm_client_msg_names::SET_CONSTRUCTION;

                    comm_write(comm, &header, sizeof(header));

                    comm_client_set_construction_body body;
                    body.town_id = ctx->selected_entity->server_id;
                    body.unit_name = (unit_names)curr_active;

                    comm_write(comm, &body, sizeof(body));
                }

                if (close) {
                    ctx->selected_entity = NULL;
                }
            }

            Rectangle admin_window = ctx->gui.admin.rect;
            GuiWindowBox(admin_window, "Admin");
            {
                real32 Y = 30;
                if (GuiButton(CLITERAL(Rectangle){admin_window.x + 10, admin_window.y + Y, admin_window.width - 20, 20}, "DISCOVER")) {
                    comm_client_header header;
                    header.name = comm_client_msg_names::ADMIN_DISCOVER_ENTIRE_MAP;

                    comm_write(comm, &header, sizeof(header));
                }
                Y += 20 + 10;
                char buf[256];
                snprintf(buf, 256, "Your server id is: %d", ctx->my_server_id);
                GuiLabel(CLITERAL(Rectangle){admin_window.x + 10, admin_window.y + Y, admin_window.width - 20, 20}, buf);
                Y += 20 + 10;
                GuiSpinner(CLITERAL(Rectangle){admin_window.x + 50, admin_window.y + Y, admin_window.width - 10 - 50, 20}, "PLAYER",
                            &ctx->gui.admin.player, 0, ctx->clients.used - 1, false);
                Y += 20 + 10;
                if (GuiButton(CLITERAL(Rectangle){admin_window.x + 10, admin_window.y + Y, admin_window.width - 20, 20}, "ADD SOLDIER")) {
                    comm_client_header header;
                    header.name = comm_client_msg_names::ADMIN_ADD_UNIT;

                    comm_write(comm, &header, sizeof(header));

                    comm_client_admin_add_unit_body body;
                    body.name = unit_names::SOLDIER;
                    body.owner_id = ctx->gui.admin.player;
                    body.position.x = ctx->camera.x;
                    body.position.y = ctx->camera.y;

                    comm_write(comm, &body, sizeof(body));
                } 
            }
            if (!ctx->my_turn) {
                Rectangle window = CLITERAL(Rectangle){scr_width / 2 - 150, scr_height / 2 - 30, 300, 60};
                GuiWindowBox(window, "Status");
                    GuiLabel(CLITERAL(Rectangle){window.x + 100, window.y + 25, window.width - 200, window.height - 25}, "Waiting on your turn");
            }
        }

    EndDrawing();

    comm_flush(comm);
}
