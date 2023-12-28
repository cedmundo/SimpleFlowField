#include <raylib.h>

int main() {
    InitWindow(800, 800, "SimpleFlowField");
    SetWindowMonitor(0);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(BLACK);
            DrawText("Hello world!", 0, 30, 18, WHITE);
            DrawFPS(0, 0);
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
