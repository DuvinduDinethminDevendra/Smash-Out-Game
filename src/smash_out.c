#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <time.h>
#include <math.h>

#define BRICKS_WIDE 10
#define BRICKS_HIGH 5
#define TOTAL_BRICKS (BRICKS_WIDE * BRICKS_HIGH)
#define MAX_BALLS 5
#define MAX_POWERUPS 50
#define POWERUP_SPAWN_CHANCE 20  // 20% chance to spawn on brick break
#define POWERUP_SPEED 3.0f
#define MAX_PARTICLES 50

// Global variables
int currentLevel = 1;
float levelNotificationTimer = 0.0f;
float masterVolume = 0.5f;

typedef enum PowerUpType {
    MULTIBALL,
    WIDE_PADDLE,
    SCREEN_WIDE,
    EXTRA_LIFE
} PowerUpType;

typedef struct PowerUp {
    Rectangle rect;
    PowerUpType type;
    bool active;
    Color color;
} PowerUp;

typedef struct Brick {
    Rectangle rect;
    bool active;
} Brick;

typedef struct Ball {
    Vector2 position;
    Vector2 speed;
    float radius;
    bool active;
} Ball;

typedef enum GameState {
    MENU,
    PLAYING,
    GAME_OVER,
    WIN,
    SETTINGS
} GameState;

void ResetGame(Brick bricks[], Ball balls[], Vector2 *ballPosition, Vector2 *ballSpeed, Rectangle *paddle) {
    // Reset paddle
    paddle->x = 800 / 2 - 50;
    paddle->width = 100;
    
    // Reset balls
    for (int i = 0; i < MAX_BALLS; i++) {
        balls[i].active = false;
    }
    balls[0].active = true;
    balls[0].position = (Vector2){ 800 / 2, 400 };
    balls[0].speed = (Vector2){ 4.0f, -4.0f };
    balls[0].radius = 8.0f;
    
    // Reset bricks
    int brickWidth = 70;
    int brickHeight = 20;
    int padding = 10;
    
    for (int i = 0; i < TOTAL_BRICKS; i++) {
        int row = i / BRICKS_WIDE;
        int col = i % BRICKS_WIDE;
        
        bricks[i].rect = (Rectangle){ 
            col * (brickWidth + padding) + 40, 
            row * (brickHeight + padding) + 50, 
            brickWidth, 
            brickHeight 
        };
        bricks[i].active = true;
    }
}

int CountActiveBricks(Brick bricks[]) {
    int count = 0;
    for (int i = 0; i < TOTAL_BRICKS; i++) {
        if (bricks[i].active) count++;
    }
    return count;
}

Color GetBrickColor(int index) {
    int row = index / BRICKS_WIDE;
    switch (row) {
        case 0: return RED;
        case 1: return ORANGE;
        case 2: return YELLOW;
        case 3: return GREEN;
        case 4: return BLUE;
        default: return RED;
    }
}

Color GetPowerUpColor(PowerUpType type) {
    switch (type) {
        case MULTIBALL: return SKYBLUE;
        case WIDE_PADDLE: return LIME;
        case SCREEN_WIDE: return MAGENTA;
        case EXTRA_LIFE: return RED;
        default: return WHITE;
    }
}

void SpawnPowerUp(PowerUp powerups[], int x, int y) {
    // Find first inactive power-up slot
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerups[i].active) {
            int randomType = rand() % 4;
            powerups[i].type = (PowerUpType)randomType;
            powerups[i].rect = (Rectangle){ x, y, 20, 20 };
            powerups[i].active = true;
            powerups[i].color = GetPowerUpColor(powerups[i].type);
            break;
        }
    }
}

void SpawnBall(Ball balls[], Vector2 position, Vector2 baseSpeed) {
    for (int i = 0; i < MAX_BALLS; i++) {
        if (!balls[i].active) {
            balls[i].active = true;
            balls[i].position = position;
            // Random horizontal velocity variation
            float speedVariation = (rand() % 100 - 50) / 100.0f * 4.0f;
            balls[i].speed = (Vector2){ baseSpeed.x + speedVariation, baseSpeed.y };
            balls[i].radius = 8.0f;
            break;
        }
    }
}

// Button drawing helper - returns true when clicked
bool DrawButton(Rectangle bounds, const char* text, int fontSize, Color normalColor, Color hoverColor) {
    Vector2 mousePos = GetMousePosition();
    bool isHovered = CheckCollisionPointRec(mousePos, bounds);
    
    Color buttonColor = isHovered ? hoverColor : normalColor;
    DrawRectangleRec(bounds, buttonColor);
    DrawRectangleLines((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, WHITE);
    
    // Draw text centered
    int textWidth = MeasureText(text, fontSize);
    int textX = (int)(bounds.x + bounds.width / 2 - textWidth / 2);
    int textY = (int)(bounds.y + bounds.height / 2 - fontSize / 2);
    DrawText(text, textX, textY, fontSize, isHovered ? BLACK : WHITE);
    
    // Return true if clicked
    return isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Initialize menu particles
void InitializeParticles(Vector2 particles[], int count) {
    for (int i = 0; i < count; i++) {
        particles[i].x = (float)(rand() % 800);
        particles[i].y = (float)(rand() % 600);
    }
}

// Update particles
void UpdateParticles(Vector2 particles[], int count, float speed) {
    for (int i = 0; i < count; i++) {
        particles[i].y += speed;
        if (particles[i].y > 600) {
            particles[i].y = -10.0f;
            particles[i].x = (float)(rand() % 800);
        }
    }
}

// Draw particles
void DrawParticles(Vector2 particles[], int count, Color color) {
    for (int i = 0; i < count; i++) {
        DrawCircle((int)particles[i].x, (int)particles[i].y, 2, color);
    }
}

// Helper function to draw text with shadow
void DrawTextWithShadow(const char* text, int posX, int posY, int fontSize, Color color) {
    DrawText(text, posX + 2, posY + 2, fontSize, BLACK);
    DrawText(text, posX, posY, fontSize, color);
}

// Load level with procedural layout
void LoadLevel(int level, Brick bricks[], Ball balls[]) {
    // Calculate number of rows based on level (capped at 8)
    int rows = 2 + level;
    if (rows > 8) rows = 8;
    
    int brickWidth = 70;
    int brickHeight = 20;
    int padding = 10;
    int topMargin = 100;  // Move bricks down to create HUD space
    
    // Deactivate all bricks first
    for (int i = 0; i < TOTAL_BRICKS; i++) {
        bricks[i].active = false;
    }
    
    // Create pattern based on level
    for (int i = 0; i < TOTAL_BRICKS; i++) {
        int row = i / BRICKS_WIDE;
        int col = i % BRICKS_WIDE;
        
        // Only fill up to the calculated number of rows
        if (row >= rows) {
            bricks[i].active = false;
            continue;
        }
        
        bricks[i].rect = (Rectangle){ 
            col * (brickWidth + padding) + 40, 
            row * (brickHeight + padding) + topMargin,  // Apply top margin
            brickWidth, 
            brickHeight 
        };
        
        // Apply pattern based on level
        if (level % 2 == 0) {
            // Checkerboard pattern for even levels
            bricks[i].active = ((row + col) % 2 == 0);
        } else if (level % 3 == 0) {
            // V-Shape/Pyramid pattern for levels divisible by 3
            int distFromCenter = col - (BRICKS_WIDE / 2);
            bricks[i].active = (row >= abs(distFromCenter) - 1);
        } else {
            // Default: fill all bricks
            bricks[i].active = true;
        }
    }
    
    // Reset balls with increased speed per level
    for (int i = 0; i < MAX_BALLS; i++) {
        balls[i].active = false;
    }
    balls[0].active = true;
    balls[0].position = (Vector2){ 400, 500 };
    balls[0].speed.x = 4.0f;
    balls[0].speed.y = -(4.0f + (level - 1) * 0.5f);  // Increase speed per level
    balls[0].radius = 8.0f;
    
    // Show level notification
    levelNotificationTimer = 3.0f;  // Changed to 3 seconds
}

// Get the number of active bricks in current level
int CountActiveBricksInLevel(Brick bricks[]) {
    int count = 0;
    for (int i = 0; i < TOTAL_BRICKS; i++) {
        if (bricks[i].active) count++;
    }
    return count;
}

int main(void) {
    // 1. Initialization
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Smash Out! - Play to Win!");
    InitAudioDevice();

    // Game state
    GameState gameState = MENU;
    int score = 0;
    int lives = 3;

    // Load menu music
    Music menuMusic = LoadMusicStream("resources/Wav/Goblin_Tinker_Soldier_Spy.mp3");
    menuMusic.looping = true;
    bool menuMusicPlaying = false;

    // Load brick hit sound
    Sound brickHitSound = LoadSound("resources/Wav/mixkit-retro-game-notification-212.wav");

    // Load wall hit sound
    Sound wallHitSound = LoadSound("resources/Wav/mixkit-gym-ball-hitting-the-ground-2079.wav");

    // Load paddle hit sound
    Sound paddleHitSound = LoadSound("resources/Wav/mixkit-catching-a-basketball-ball-2081.wav");

    // Load game over sound
    Sound gameOverSound = LoadSound("resources/Wav/mixkit-piano-game-over-1941.wav");

    // Load lose heart sound
    Sound loseHeartSound = LoadSound("resources/Wav/mixkit-failure-arcade-alert-notification-240.wav");

    // Paddle setup
    Rectangle paddle = { screenWidth / 2 - 50, screenHeight - 40, 100, 20 };
    float paddleSpeed = 8.0f;
    float paddleBuffTimer = 0.0f;
    float paddleOriginalWidth = 100.0f;

    // Ball setup
    Ball balls[MAX_BALLS];
    for (int i = 0; i < MAX_BALLS; i++) {
        balls[i].active = false;
        balls[i].radius = 8.0f;
    }
    balls[0].active = true;
    balls[0].position = (Vector2){ screenWidth / 2, screenHeight / 2 };
    balls[0].speed = (Vector2){ 4.0f, -4.0f };
    balls[0].radius = 8.0f;

    // Power-ups setup
    PowerUp powerups[MAX_POWERUPS];
    for (int i = 0; i < MAX_POWERUPS; i++) {
        powerups[i].active = false;
    }

    // Menu particles setup
    Vector2 menuParticles[MAX_PARTICLES];
    InitializeParticles(menuParticles, MAX_PARTICLES);

    // Bricks setup
    Brick bricks[TOTAL_BRICKS];
    int brickWidth = 70;
    int brickHeight = 20;
    int padding = 10;

    for (int i = 0; i < TOTAL_BRICKS; i++) {
        int row = i / BRICKS_WIDE;
        int col = i % BRICKS_WIDE;
        
        bricks[i].rect = (Rectangle){ 
            col * (brickWidth + padding) + 40, 
            row * (brickHeight + padding) + 50, 
            brickWidth, 
            brickHeight 
        };
        bricks[i].active = true;
    }

    SetTargetFPS(60);

    // Heart pixel pattern
    static const int heartPattern[6][7] = {
        {0, 1, 1, 0, 1, 1, 0}, // Row 0:  XX   XX
        {1, 1, 1, 1, 1, 1, 1}, // Row 1: XXXXXXX
        {1, 1, 1, 1, 1, 1, 1}, // Row 2: XXXXXXX
        {0, 1, 1, 1, 1, 1, 0}, // Row 3:  XXXXX
        {0, 0, 1, 1, 1, 0, 0}, // Row 4:   XXX
        {0, 0, 0, 1, 0, 0, 0}  // Row 5:    X
    };
    int pixelScale = 4; // Size multiplier: How big each "pixel" square is on screen

    // 2. Main game loop
    while (!WindowShouldClose()) {
        // --- MENU STATE ---
        if (gameState == MENU) {
            // Play menu music
            if (!menuMusicPlaying) {
                PlayMusicStream(menuMusic);
                menuMusicPlaying = true;
            }
            UpdateMusicStream(menuMusic);
            
            // Update particles
            UpdateParticles(menuParticles, MAX_PARTICLES, 0.5f);
            
            // Button handling
            // Start button
            Rectangle startButton = { 300, 300, 200, 60 };
            if (DrawButton(startButton, "START", 30, BLUE, SKYBLUE)) {
                StopMusicStream(menuMusic);
                menuMusicPlaying = false;
                gameState = PLAYING;
                currentLevel = 1;
                levelNotificationTimer = 0.0f;
                LoadLevel(currentLevel, bricks, balls);
                score = 0;
                lives = 3;
                paddleBuffTimer = 0.0f;
                paddle.x = screenWidth / 2 - 50;
                paddle.width = paddleOriginalWidth;
            }
            
            // Settings button
            Rectangle settingsButton = { 300, 380, 200, 60 };
            if (DrawButton(settingsButton, "SETTINGS", 20, ORANGE, YELLOW)) {
                gameState = SETTINGS;
            }
            
            // Exit button
            Rectangle exitButton = { 300, 460, 200, 60 };
            if (DrawButton(exitButton, "EXIT", 30, RED, ORANGE)) {
                break; // Exit game loop
            }
        }
        // --- PLAYING STATE ---
        else if (gameState == PLAYING) {
            // Update level notification timer
            if (levelNotificationTimer > 0.0f) {
                levelNotificationTimer -= GetFrameTime();
            }

            // Update paddle buff timer
            if (paddleBuffTimer > 0.0f) {
                paddleBuffTimer -= GetFrameTime();
                if (paddleBuffTimer <= 0.0f) {
                    paddle.width = paddleOriginalWidth;
                    paddleBuffTimer = 0.0f;
                }
            }

            // Move Paddle
            if (IsKeyDown(KEY_LEFT) && paddle.x > 0) paddle.x -= paddleSpeed;
            if (IsKeyDown(KEY_RIGHT) && paddle.x < screenWidth - paddle.width) paddle.x += paddleSpeed;

            // Update and draw balls
            for (int b = 0; b < MAX_BALLS; b++) {
                if (!balls[b].active) continue;

                // Move Ball
                balls[b].position.x += balls[b].speed.x;
                balls[b].position.y += balls[b].speed.y;

                // Ball Collision: Walls
                if (balls[b].position.x >= screenWidth - balls[b].radius || balls[b].position.x <= balls[b].radius) {
                    balls[b].speed.x *= -1.0f;
                    PlaySound(wallHitSound);
                }
                if (balls[b].position.y <= balls[b].radius) {
                    balls[b].speed.y *= -1.0f;
                    PlaySound(wallHitSound);
                }
                
                // Ball Collision: Paddle
                if (CheckCollisionCircleRec(balls[b].position, balls[b].radius, paddle)) {
                    balls[b].speed.y *= -1.0f;
                    balls[b].position.y = paddle.y - balls[b].radius;
                    
                    // Add horizontal velocity based on where ball hits paddle
                    float hitPos = (balls[b].position.x - paddle.x) / paddle.width;
                    balls[b].speed.x = (hitPos - 0.5f) * 8.0f;
                    PlaySound(paddleHitSound);
                }

                // Ball Collision: Bricks
                for (int i = 0; i < TOTAL_BRICKS; i++) {
                    if (bricks[i].active) {
                        if (CheckCollisionCircleRec(balls[b].position, balls[b].radius, bricks[i].rect)) {
                            bricks[i].active = false;
                            balls[b].speed.y *= -1.0f;
                            score += 10;
                            PlaySound(brickHitSound);
                            
                            // 20% chance to spawn power-up
                            if ((rand() % 100) < POWERUP_SPAWN_CHANCE) {
                                SpawnPowerUp(powerups, (int)(bricks[i].rect.x + bricks[i].rect.width / 2), (int)bricks[i].rect.y);
                            }
                            break;
                        }
                    }
                }

                // Reset ball if it falls off screen
                if (balls[b].position.y > screenHeight) {
                    balls[b].active = false;
                }
            }

            // Check if all balls are gone
            bool anyBallActive = false;
            for (int i = 0; i < MAX_BALLS; i++) {
                if (balls[i].active) {
                    anyBallActive = true;
                    break;
                }
            }
            
            if (!anyBallActive) {
                lives--;
                if (lives <= 0) {
                    PlaySound(gameOverSound);
                    gameState = GAME_OVER;
                } else {
                    PlaySound(loseHeartSound);
                    balls[0].active = true;
                    balls[0].position = (Vector2){ screenWidth / 2, screenHeight / 2 };
                    balls[0].speed = (Vector2){ 4.0f, -4.0f };
                }
            }

            // Update power-ups
            for (int i = 0; i < MAX_POWERUPS; i++) {
                if (!powerups[i].active) continue;

                // Fall downward
                powerups[i].rect.y += POWERUP_SPEED;

                // Check collision with paddle
                if (CheckCollisionRecs(powerups[i].rect, paddle)) {
                    powerups[i].active = false;

                    // Apply power-up effect
                    if (powerups[i].type == MULTIBALL) {
                        // Spawn a second ball
                        SpawnBall(balls, balls[0].position, balls[0].speed);
                    } else if (powerups[i].type == WIDE_PADDLE) {
                        paddle.width = paddleOriginalWidth * 2.0f;
                        paddleBuffTimer = 10.0f;
                    } else if (powerups[i].type == SCREEN_WIDE) {
                        paddle.width = screenWidth;
                        paddle.x = 0;
                        paddleBuffTimer = 5.0f;
                    } else if (powerups[i].type == EXTRA_LIFE) {
                        lives++;
                        PlaySound(loseHeartSound);  // Reusing sound; can use different if needed
                    }
                } else if (powerups[i].rect.y > screenHeight) {
                    // Power-up fell off screen
                    powerups[i].active = false;
                }
            }

            // Check win condition
            if (CountActiveBricksInLevel(bricks) == 0) {
                // All bricks destroyed - go to next level
                currentLevel++;
                levelNotificationTimer = 2.0f;
                LoadLevel(currentLevel, bricks, balls);
                paddle.x = screenWidth / 2 - 50;
                paddle.width = paddleOriginalWidth;
                paddleBuffTimer = 0.0f;
                
                // Clear power-ups for new level
                for (int i = 0; i < MAX_POWERUPS; i++) {
                    powerups[i].active = false;
                }
            }
        }
        // --- GAME OVER STATE ---
        else if (gameState == GAME_OVER) {
            if (IsKeyPressed(KEY_SPACE)) {
                gameState = MENU;
            }
        }
        // --- WIN STATE ---
        else if (gameState == WIN) {
            if (IsKeyPressed(KEY_SPACE)) {
                gameState = MENU;
            }
        }
        // --- SETTINGS STATE ---
        else if (gameState == SETTINGS) {
            // Volume slider interaction
            Rectangle volumeSlider = { 250, 250, 300, 20 };
            
            // Handle mouse click on slider
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 mousePos = GetMousePosition();
                if (CheckCollisionPointRec(mousePos, volumeSlider)) {
                    // Update volume based on click position
                    masterVolume = (mousePos.x - volumeSlider.x) / volumeSlider.width;
                    if (masterVolume < 0.0f) masterVolume = 0.0f;
                    if (masterVolume > 1.0f) masterVolume = 1.0f;
                    SetMasterVolume(masterVolume);
                }
            }
            
            // Back to menu
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE)) {
                gameState = MENU;
            }
        }

        // --- DRAW ---
        BeginDrawing();
        ClearBackground(BLACK);

        if (gameState == MENU) {
            // Draw background particles
            DrawParticles(menuParticles, MAX_PARTICLES, DARKBLUE);
            
            // Draw animated title with bob effect and shadow
            float bobOffset = sinf((float)GetTime() * 3.0f) * 15.0f;
            int titleY = 80 + (int)bobOffset;
            DrawTextWithShadow("SMASH OUT!", screenWidth / 2 - 150, titleY, 60, YELLOW);
            
            // Draw subtitle
            DrawText("Click to Play or Press SPACE", screenWidth / 2 - 160, 180, 20, LIGHTGRAY);
        }
        else if (gameState == PLAYING) {
            // Draw HUD Bar Background
            DrawRectangle(0, 0, screenWidth, 70, Fade(DARKGRAY, 0.4f));
            DrawLine(0, 70, screenWidth, 70, GRAY);
            
            // Draw Bricks
            for (int i = 0; i < TOTAL_BRICKS; i++) {
                if (bricks[i].active) {
                    DrawRectangleRec(bricks[i].rect, GetBrickColor(i));
                    DrawRectangleLines((int)bricks[i].rect.x, (int)bricks[i].rect.y, 
                                     (int)bricks[i].rect.width, (int)bricks[i].rect.height, BLACK);
                }
            }

            // Draw Balls
            for (int i = 0; i < MAX_BALLS; i++) {
                if (balls[i].active) {
                    DrawCircleV(balls[i].position, balls[i].radius, WHITE);
                }
            }

            // Draw Power-Ups
            for (int i = 0; i < MAX_POWERUPS; i++) {
                if (powerups[i].active) {
                    DrawRectangleRec(powerups[i].rect, powerups[i].color);
                    DrawRectangleLines((int)powerups[i].rect.x, (int)powerups[i].rect.y, 
                                     (int)powerups[i].rect.width, (int)powerups[i].rect.height, WHITE);
                }
            }

            // Draw Paddle
            DrawRectangleRec(paddle, BLUE);
            DrawRectangleLines((int)paddle.x, (int)paddle.y, (int)paddle.width, (int)paddle.height, SKYBLUE);

            // === HUD LAYOUT ===
            
            // 1. Score (Top-Left)
            DrawTextWithShadow(TextFormat("SCORE: %04d", score), 20, 18, 20, YELLOW);
            
            // 2. Level (Top-Center)
            int levelTextWidth = MeasureText(TextFormat("LEVEL %d", currentLevel), 20);
            DrawTextWithShadow(TextFormat("LEVEL %d", currentLevel), screenWidth / 2 - levelTextWidth / 2, 18, 20, GREEN);
            
            // 3. Bricks Remaining (Top-Right)
            char bricksText[30];
            sprintf_s(bricksText, sizeof(bricksText), "BRICKS: %d", CountActiveBricksInLevel(bricks));
            int bricksTextWidth = MeasureText(bricksText, 20);
            DrawTextWithShadow(bricksText, screenWidth - bricksTextWidth - 20, 18, 20, LIGHTGRAY);

            // 4. Draw hearts for lives (Top-Right below bricks count)
            for (int i = 0; i < lives; i++) {
                int baseX = screenWidth - 120 + i * 30;
                int baseY = 45;

                for (int row = 0; row < 6; row++) {
                    for (int col = 0; col < 7; col++) {
                        if (heartPattern[row][col] == 1) {
                            DrawRectangle(
                                baseX + (col * pixelScale),
                                baseY + (row * pixelScale),
                                pixelScale,
                                pixelScale,
                                RED
                            );
                        }
                    }
                }
            }

            // 5. Paddle buff timer (if active)
            if (paddleBuffTimer > 0.0f) {
                char timerText[20];
                sprintf_s(timerText, sizeof(timerText), "BUFF: %.1f", paddleBuffTimer);
                int timerTextWidth = MeasureText(timerText, 16);
                DrawTextWithShadow(timerText, screenWidth / 2 - timerTextWidth / 2, 45, 16, LIME);
            }

            // 6. Level notification (center screen, appears for 3 seconds)
            if (levelNotificationTimer > 0.0f) {
                float fadeFactor = levelNotificationTimer / 3.0f;  // Fade out over 3 seconds
                unsigned char alphaValue = (unsigned char)(200 * fadeFactor);
                
                // Semi-transparent background box
                DrawRectangle(screenWidth / 2 - 150, screenHeight / 2 - 60, 300, 120, 
                            (Color){0, 0, 0, alphaValue});
                DrawRectangleLines(screenWidth / 2 - 150, screenHeight / 2 - 60, 300, 120, 
                                 (Color){255, 255, 255, alphaValue});
                
                // Large level text with fade
                char levelNotificationText[30];
                sprintf_s(levelNotificationText, sizeof(levelNotificationText), "LEVEL %d", currentLevel);
                int notifTextWidth = MeasureText(levelNotificationText, 60);
                DrawText(levelNotificationText, screenWidth / 2 + 2 - notifTextWidth / 2, screenHeight / 2 - 40, 60, (Color){0, 0, 0, alphaValue});
                DrawText(levelNotificationText, screenWidth / 2 - notifTextWidth / 2, screenHeight / 2 - 42, 60, (Color){255, 255, 0, (unsigned char)(255 * fadeFactor)});
            }
        }
        else if (gameState == GAME_OVER) {
            DrawText("GAME OVER!", screenWidth / 2 - 150, 200, 60, RED);
            char finalScore[30];
            sprintf_s(finalScore, sizeof(finalScore), "Final Score: %d", score);
            DrawText(finalScore, screenWidth / 2 - 150, 320, 40, YELLOW);
            DrawText("Press SPACE to return to menu", screenWidth / 2 - 200, 420, 25, LIGHTGRAY);
        }
        else if (gameState == WIN) {
            DrawText("YOU WIN!", screenWidth / 2 - 150, 200, 60, GREEN);
            char finalScore[30];
            sprintf_s(finalScore, sizeof(finalScore), "Final Score: %d", score);
            DrawText(finalScore, screenWidth / 2 - 150, 320, 40, YELLOW);
            DrawText("Press SPACE to return to menu", screenWidth / 2 - 200, 420, 25, LIGHTGRAY);
        }
        else if (gameState == SETTINGS) {
            // Draw title
            DrawTextWithShadow("SETTINGS", screenWidth / 2 - 90, 80, 50, YELLOW);
            
            // Draw Master Volume label
            DrawTextWithShadow("Master Volume", 250, 200, 25, LIGHTGRAY);
            
            // Draw volume slider background
            Rectangle volumeSlider = { 250, 250, 300, 20 };
            DrawRectangleRec(volumeSlider, DARKGRAY);
            DrawRectangleLines((int)volumeSlider.x, (int)volumeSlider.y, (int)volumeSlider.width, (int)volumeSlider.height, LIGHTGRAY);
            
            // Draw volume slider fill (progress)
            Rectangle volumeFill = { volumeSlider.x, volumeSlider.y, volumeSlider.width * masterVolume, volumeSlider.height };
            DrawRectangleRec(volumeFill, LIME);
            
            // Draw volume percentage
            char volumeText[10];
            sprintf_s(volumeText, sizeof(volumeText), "%d%%", (int)(masterVolume * 100));
            int volumeTextWidth = MeasureText(volumeText, 20);
            DrawTextWithShadow(volumeText, screenWidth / 2 - volumeTextWidth / 2, 310, 20, YELLOW);
            
            // Draw Back button
            Rectangle backButton = { screenWidth / 2 - 100, 420, 200, 60 };
            if (DrawButton(backButton, "BACK", 30, ORANGE, YELLOW)) {
                gameState = MENU;
            }
            
            // Draw hint text
            DrawText("Click slider to adjust volume or press ESC", screenWidth / 2 - 230, 520, 18, LIGHTGRAY);
        }

        EndDrawing();
    }

    // 3. De-initialization
    UnloadSound(loseHeartSound);
    UnloadSound(gameOverSound);
    UnloadSound(paddleHitSound);
    UnloadSound(wallHitSound);
    UnloadSound(brickHitSound);
    UnloadMusicStream(menuMusic);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}