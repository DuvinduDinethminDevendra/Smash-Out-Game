#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"



int main(void) {
    // Initialize window
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Smash Out!");

    // Main game loop
    while (!WindowShouldClose()) {
        // Update game state

        // Draw
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Welcome to Smash Out!", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }

    // De-initialize
    CloseWindow();
    return 0;
}
