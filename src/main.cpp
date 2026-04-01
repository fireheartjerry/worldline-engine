#include "raylib.h"

int main() {
    InitWindow(1280, 720, "Worldline");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground({8, 8, 12, 255});
            DrawText("Worldline", 40, 40, 40, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}