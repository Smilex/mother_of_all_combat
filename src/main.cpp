#include "raylib.h"
#include <stdlib.h>
#include <string.h>

#include "shared.cpp"
#include "communication/protocol.cpp"
#include "communication/server/memory.cpp"
#include "server/server.cpp"

memory_arena total_memory, server_memory, comm_memory;
communication server_comm;

int main(int argc, char *argv[]) {

    total_memory.used = 0;
    total_memory.max = MB(20);
    total_memory.base = (u8 *)malloc(total_memory.max);
    assert(total_memory.base);
    memset(total_memory.base, 0, total_memory.max);

    server_memory = memory_arena_child(&total_memory, MB(5));
    comm_memory = memory_arena_child(&total_memory, MB(5));

    comm_server_memory_init(&server_comm, &comm_memory);

    InitWindow(1280, 720, "Hello, world");
    SetTargetFPS(60);

    while(!WindowShouldClose()) {
        server_update(&server_memory, &server_comm);

        BeginDrawing();
            ClearBackground(WHITE);
        EndDrawing();
    }

    CloseWindow();

    return EXIT_SUCCESS;
}
