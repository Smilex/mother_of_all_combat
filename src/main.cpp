#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_RICONS
#include "raygui.h"
#include <stdlib.h>
#include <string.h>

#include "shared.cpp"
#include "communication/protocol.cpp"
#include "communication/server/memory.cpp"
#include "communication/client/memory.cpp"
#include "server/server.cpp"

#define CLIENT_UPDATE_AND_RENDER(_n) void _n(memory_arena *mem, communication *comm)
typedef CLIENT_UPDATE_AND_RENDER(client_update_and_render_t);

#include "client/client.cpp"

memory_arena total_memory, server_memory, client_memory;
communication server_comm, client_comm;

client_update_and_render_t *client_update_and_render_ptr = &client_update_and_render;

int main(int argc, char *argv[]) {

    total_memory.used = 0;
    total_memory.max = MB(20);
    total_memory.base = (u8 *)malloc(total_memory.max);
    assert(total_memory.base);
    memset(total_memory.base, 0, total_memory.max);

    server_memory = memory_arena_child(&total_memory, MB(5));
    ring_buffer<u8> comm_memory = ring_buffer<u8>(&total_memory, sizeof(comm_client_header));
    client_memory = memory_arena_child(&total_memory, MB(5));

    comm_server_memory_init(&server_comm, &comm_memory);
    comm_client_memory_init(&client_comm, &comm_memory);

    InitWindow(1280, 720, "Hello, world");
    SetTargetFPS(60);

    while(!WindowShouldClose()) {
        server_update(&server_memory, &server_comm);

        client_update_and_render_ptr(&client_memory, &client_comm);
    }

    CloseWindow();

    return EXIT_SUCCESS;
}
