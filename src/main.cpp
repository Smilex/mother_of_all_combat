#include "raylib.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {

    InitWindow(800, 600, "Hello, world");
    SetTargetFPS(60);

    while(!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(WHITE);
        EndDrawing();
    }

    CloseWindow();

    return EXIT_SUCCESS;
}
