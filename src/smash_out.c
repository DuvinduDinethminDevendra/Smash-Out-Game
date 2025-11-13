#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

int main(void) {
    // Initialize window
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Smash Out!");
    Rectangle Box = { 100, 100, 50, 50 };  // Define a rectangle Box to use in the game
    // Ball
    Rectangle Ball = { 200, 200, 30, 30 };  // Define a rectangle Ball to use in the game


    // Main game loop
    while (!WindowShouldClose()) {
        // Update game state

        // Draw
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Smash Out!", 10, 10, 20, DARKGRAY);
        // loop to draw multiple boxes to brake (10x10 grid)
        for (int i = 0; i < 100; i++) {
            Rectangle brick = {
                (i % 30) * 30 + 10,  // x position (10 columns, 80px wide each)
                (i / 30) * 20 + 50,  // y position (10 rows, 60px tall each)
                20,                   // width
                10                    // height
            };
            DrawRectangleRec(brick, RED);
        }

        DrawRectangleRec(Ball, BLUE);  // Draw the Ball
        EndDrawing();
    }

    // De-initialize
    CloseWindow();
    return 0;
}
