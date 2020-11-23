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
#include "ai/ai.cpp"

#define NUM_AI 1

memory_arena total_memory, server_memory, client_memory, ai_memory[NUM_AI];
communication server_to_client_comm, client_to_server_comm, ai_to_server_comm[NUM_AI], server_to_ai_comm[NUM_AI];
comm_memory_pipe server_to_client_pipe, client_to_server_pipe, server_to_ai_pipe[NUM_AI], ai_to_server_pipe[NUM_AI];

client_update_and_render_t *client_update_and_render_ptr = &client_update_and_render;

void sitrep(sitrep_names name, char *fmt, ...) {
    char time_str[64] = {0};
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", time_str);

    switch (name) {
        case SITREP_INFO: printf("[INFO] "); break;
        case SITREP_WARNING: printf("[WARN] "); break;
        case SITREP_ERROR: printf("[ERROR] "); break;
        case SITREP_DEBUG: printf("[DEBUG] "); break;
    }

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

u32 time_get_now_in_ms() {
#ifdef _WIN32
    return 0;
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    u32 rv = 0;
    rv += ts.tv_sec * 1000;
    rv += round(ts.tv_nsec / 1.0e6);

    return rv;
#endif
}

int main(int argc, char *argv[]) {
    total_memory.name = "total_memory";
    total_memory.used = 0;
    total_memory.max = GB(2);
    total_memory.base = (u8 *)malloc(total_memory.max);
    assert(total_memory.base);
    memset(total_memory.base, 0, total_memory.max);

    server_memory = memory_arena_child(&total_memory, MB(100), "server_memory");
    client_memory = memory_arena_child(&total_memory, MB(100), "client_memory");

    ring_buffer<u8> server_to_client_ring_buffer = ring_buffer<u8>(&total_memory, MB(100));
    ring_buffer<u8> client_to_server_ring_buffer = ring_buffer<u8>(&total_memory, MB(100));


    server_to_client_pipe.in = &client_to_server_ring_buffer;
    server_to_client_pipe.out = &server_to_client_ring_buffer;

    client_to_server_pipe.in = &server_to_client_ring_buffer;
    client_to_server_pipe.out = &client_to_server_ring_buffer;

    comm_server_memory_init(&server_to_client_comm, &server_to_client_pipe, memory_arena_child(&total_memory, MB(100), "server_to_client_memory"));
    comm_client_memory_init(&client_to_server_comm, &client_to_server_pipe, memory_arena_child(&total_memory, MB(100), "client_to_server_memory"));

    communication server_comms[1 + NUM_AI];
    server_comms[0] = server_to_client_comm;

    ring_buffer<u8> *server_to_ai_ring_buffer = (ring_buffer<u8> *)malloc(sizeof(*server_to_ai_ring_buffer) * NUM_AI);
    ring_buffer<u8> *ai_to_server_ring_buffer = (ring_buffer<u8> *)malloc(sizeof(*ai_to_server_ring_buffer) * NUM_AI);
    for (u32 i = 0; i < NUM_AI; ++i) {
        char *name = (char *)malloc(50);
        snprintf(name, 50, "ai_memory_%u", i);
        ai_memory[i] = memory_arena_child(&total_memory, MB(20), name);

        server_to_ai_ring_buffer[i] = ring_buffer<u8>(&total_memory, MB(5));
        ai_to_server_ring_buffer[i] = ring_buffer<u8>(&total_memory, MB(5));

        server_to_ai_pipe[i].in = &ai_to_server_ring_buffer[i];
        server_to_ai_pipe[i].out = &server_to_ai_ring_buffer[i];

        ai_to_server_pipe[i].in = &server_to_ai_ring_buffer[i];
        ai_to_server_pipe[i].out = &ai_to_server_ring_buffer[i];

        name = (char *)malloc(50);
        snprintf(name, 50, "server_to_ai_memory_%u", i);
        comm_server_memory_init(&server_to_ai_comm[i], &server_to_ai_pipe[i], memory_arena_child(&total_memory, MB(20), name));
        name = (char *)malloc(50);
        snprintf(name, 50, "ai_to_server_memory_%u", i);
        comm_client_memory_init(&ai_to_server_comm[i], &ai_to_server_pipe[i], memory_arena_child(&total_memory, MB(20), name));

        server_comms[i + 1] = server_to_ai_comm[i];
    }



    InitWindow(1280, 720, "Hello, world");
    InitAudioDevice();
    SetTargetFPS(60);

    while(!WindowShouldClose()) {
        server_update(&server_memory, server_comms, 1 + NUM_AI);

        client_update_and_render_ptr(&client_memory, &client_to_server_comm);
#if 0
        for (u32 i = 0; i < NUM_AI; ++i)
            ai_update(&ai_memory[i], &ai_to_server_comm[i]);
#endif
    }

    CloseWindow();

    return EXIT_SUCCESS;
}
