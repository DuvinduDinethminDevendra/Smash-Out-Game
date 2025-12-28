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

// Combo system
int brickCombo = 0;
float comboMultiplier = 1.0f;
float comboDisplayTimer = 0.0f;

// Floating combo text
typedef struct FloatingCombo {
    Vector2 position;
    float lifetime;
    float multiplier;
} FloatingCombo;

#define MAX_FLOATING_COMBOS 10
FloatingCombo floatingCombos[MAX_FLOATING_COMBOS];

// High score persistence
int highScore = 0;
#define HIGHSCORE_FILE "highscore.txt"

// Time bonus
float levelTimer = 0.0f;
#define TIME_LIMIT 60.0f  // 60 seconds to get time bonus
#define MAX_TIME_BONUS 500  // Maximum bonus points

// Screen shake
float shakeIntensity = 0.0f;
float shakeTimer = 0.0f;
#define MAX_SHAKE_TIME 0.15f

// Particle effects for brick destruction
typedef struct Particle {
    Vector2 position;
    Vector2 velocity;
    float lifetime;
    Color color;
} Particle;

#define MAX_BRICK_PARTICLES 100
Particle brickParticles[MAX_BRICK_PARTICLES];

// Ball trail
typedef struct BallTrailPoint {
    Vector2 position;
    float alpha;
} BallTrailPoint;

#define MAX_TRAIL_POINTS 8

// Paddle squash/stretch
float paddleSquashTimer = 0.0f;
#define PADDLE_SQUASH_DURATION 0.1f

// Level statistics tracking
int bricksSmashed = 0;
float levelCompletionTime = 0.0f;
float levelSummaryTimer = 0.0f;
#define LEVEL_SUMMARY_DURATION 3.0f

// Ball death particles
#define MAX_DEATH_PARTICLES 50
Particle deathParticles[MAX_DEATH_PARTICLES];

typedef enum PowerUpType {
    MULTIBALL,
    WIDE_PADDLE,
    SCREEN_WIDE,
    EXTRA_LIFE
} PowerUpType;

typedef enum BrickType {
    BRICK_NORMAL,
    BRICK_TOUGH,
    BRICK_EXPLOSIVE,
    BRICK_SPEED,
    BRICK_INVISIBLE
} BrickType;

typedef struct PowerUp {
    Rectangle rect;
    PowerUpType type;
    bool active;
    Color color;
} PowerUp;

typedef struct Brick {
    Rectangle rect;
    bool active;
    BrickType type;
    int health;  // For TOUGH bricks: 1-3 hits
    bool discovered;  // For INVISIBLE bricks: true = revealed
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
    SETTINGS,
    PAUSED,
    LEVEL_SUMMARY,
    HOW_TO_PLAY
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

Color GetBrickColor(Brick brick) {
    // Determine color based on brick type
    switch (brick.type) {
        case BRICK_NORMAL: {
            // Standard row-based coloring
            int row = 0;
            for (int i = 0; i < TOTAL_BRICKS; i++) {
                if (i / BRICKS_WIDE == row) break;
            }
            switch (row % 5) {
                case 0: return RED;
                case 1: return ORANGE;
                case 2: return YELLOW;
                case 3: return GREEN;
                case 4: return BLUE;
                default: return RED;
            }
        }
        case BRICK_TOUGH:
            // Change color based on health (3=dark red, 2=red, 1=pink)
            if (brick.health == 3) return (Color){80, 0, 0, 255};  // Dark Red
            if (brick.health == 2) return RED;
            return (Color){255, 100, 150, 255};  // Pink
        case BRICK_EXPLOSIVE:
            return PURPLE;  // Purple for explosive
        case BRICK_SPEED:
            return LIME;  // Lime for speed boosts
        case BRICK_INVISIBLE:
            return brick.discovered ? GRAY : (Color){0, 0, 0, 0};  // Invisible until hit
        default:
            return WHITE;
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

// Spawn floating combo text at brick location
void SpawnFloatingCombo(Vector2 brickPos, float multiplier) {
    for (int i = 0; i < MAX_FLOATING_COMBOS; i++) {
        if (floatingCombos[i].lifetime <= 0.0f) {
            floatingCombos[i].position = brickPos;
            floatingCombos[i].lifetime = 1.2f;  // 1.2 second lifetime
            floatingCombos[i].multiplier = multiplier;
            break;
        }
    }
}

// Update floating combo text
void UpdateFloatingCombos(float deltaTime) {
    for (int i = 0; i < MAX_FLOATING_COMBOS; i++) {
        if (floatingCombos[i].lifetime > 0.0f) {
            floatingCombos[i].lifetime -= deltaTime;
            floatingCombos[i].position.y -= 40.0f * deltaTime;  // Rise upward
        }
    }
}

// Draw floating combo text
void DrawFloatingCombos() {
    for (int i = 0; i < MAX_FLOATING_COMBOS; i++) {
        if (floatingCombos[i].lifetime > 0.0f) {
            float alpha = floatingCombos[i].lifetime / 1.2f;  // Fade effect
            
            char comboText[20];
            sprintf_s(comboText, sizeof(comboText), "%.1fx!", floatingCombos[i].multiplier);
            int textWidth = MeasureText(comboText, 28);
            
            Color comboColor = (Color){255, 165, 0, (unsigned char)(255 * alpha)};
            DrawText(comboText, (int)floatingCombos[i].position.x + 1 - textWidth / 2, 
                    (int)floatingCombos[i].position.y + 1, 28, (Color){0, 0, 0, (unsigned char)(150 * alpha)});
            DrawText(comboText, (int)floatingCombos[i].position.x - textWidth / 2, 
                    (int)floatingCombos[i].position.y, 28, comboColor);
        }
    }
}

// Initialize floating combos
void InitializeFloatingCombos() {
    for (int i = 0; i < MAX_FLOATING_COMBOS; i++) {
        floatingCombos[i].lifetime = 0.0f;
    }
}

// Initialize brick particles
void InitializeBrickParticles() {
    for (int i = 0; i < MAX_BRICK_PARTICLES; i++) {
        brickParticles[i].lifetime = 0.0f;
    }
}

// Initialize death particles
void InitializeDeathParticles() {
    for (int i = 0; i < MAX_DEATH_PARTICLES; i++) {
        deathParticles[i].lifetime = 0.0f;
    }
}

// Spawn brick particles on destruction
void SpawnBrickParticles(Vector2 position, int count) {
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < MAX_BRICK_PARTICLES; j++) {
            if (brickParticles[j].lifetime <= 0.0f) {
                brickParticles[j].position = position;
                // Random velocity in all directions
                float angle = (rand() % 360) * 3.14159f / 180.0f;
                float speed = 50.0f + (rand() % 100);
                brickParticles[j].velocity.x = cosf(angle) * speed;
                brickParticles[j].velocity.y = sinf(angle) * speed;
                brickParticles[j].lifetime = 0.5f;  // 0.5 second lifetime
                brickParticles[j].color = RED;
                break;
            }
        }
    }
}

// Update brick particles
void UpdateBrickParticles(float deltaTime) {
    for (int i = 0; i < MAX_BRICK_PARTICLES; i++) {
        if (brickParticles[i].lifetime > 0.0f) {
            brickParticles[i].lifetime -= deltaTime;
            brickParticles[i].position.x += brickParticles[i].velocity.x * deltaTime;
            brickParticles[i].position.y += brickParticles[i].velocity.y * deltaTime;
            // Apply gravity
            brickParticles[i].velocity.y += 200.0f * deltaTime;
        }
    }
}

// Draw brick particles with fade
void DrawBrickParticles() {
    for (int i = 0; i < MAX_BRICK_PARTICLES; i++) {
        if (brickParticles[i].lifetime > 0.0f) {
            float alpha = brickParticles[i].lifetime / 0.5f;  // Fade from full to transparent
            Color particleColor = (Color){
                brickParticles[i].color.r,
                brickParticles[i].color.g,
                brickParticles[i].color.b,
                (unsigned char)(200 * alpha)
            };
            DrawCircleV(brickParticles[i].position, 3.0f, particleColor);
        }
    }
}

// Trigger screen shake
void TriggerScreenShake(float intensity) {
    shakeIntensity = intensity;
    shakeTimer = MAX_SHAKE_TIME;
}

// Spawn death particles when ball falls off screen
void SpawnDeathParticles(Vector2 ballPos) {
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < MAX_DEATH_PARTICLES; j++) {
            if (deathParticles[j].lifetime <= 0.0f) {
                deathParticles[j].position = ballPos;
                float angle = (i / 12.0f) * 2.0f * 3.14159f;  // Spread in circle
                float speed = 100.0f + (rand() % 50);
                deathParticles[j].velocity.x = cosf(angle) * speed;
                deathParticles[j].velocity.y = sinf(angle) * speed;
                deathParticles[j].lifetime = 0.6f;
                deathParticles[j].color = LIGHTGRAY;
                break;
            }
        }
    }
}

// Update death particles
void UpdateDeathParticles(float deltaTime) {
    for (int i = 0; i < MAX_DEATH_PARTICLES; i++) {
        if (deathParticles[i].lifetime > 0.0f) {
            deathParticles[i].lifetime -= deltaTime;
            deathParticles[i].position.x += deathParticles[i].velocity.x * deltaTime;
            deathParticles[i].position.y += deathParticles[i].velocity.y * deltaTime;
            // Apply gravity
            deathParticles[i].velocity.y += 200.0f * deltaTime;
        }
    }
}

// Draw death particles
void DrawDeathParticles() {
    for (int i = 0; i < MAX_DEATH_PARTICLES; i++) {
        if (deathParticles[i].lifetime > 0.0f) {
            float alpha = deathParticles[i].lifetime / 0.6f;
            Color particleColor = (Color){
                deathParticles[i].color.r,
                deathParticles[i].color.g,
                deathParticles[i].color.b,
                (unsigned char)(200 * alpha)
            };
            DrawCircleV(deathParticles[i].position, 4.0f, particleColor);
        }
    }
}

// Load high score from file
void LoadHighScore() {
    FILE *file = fopen(HIGHSCORE_FILE, "r");
    if (file != NULL) {
        fscanf(file, "%d", &highScore);
        fclose(file);
    } else {
        highScore = 0;
    }
}

// Save high score to file
void SaveHighScore() {
    FILE *file = fopen(HIGHSCORE_FILE, "w");
    if (file != NULL) {
        fprintf(file, "%d", highScore);
        fclose(file);
    }
}

// Update high score if current score is higher
void UpdateHighScore(int currentScore) {
    if (currentScore > highScore) {
        highScore = currentScore;
        SaveHighScore();
    }
}

// Load level with procedural layout
void LoadLevel(int level, Brick bricks[], Ball balls[]) {
    // Reset brick counter
    bricksSmashed = 0;
    
    // Calculate number of rows based on level (capped at 8)
    int rows = 2 + level;
    if (rows > 8) rows = 8;

    int brickWidth = 70;
    int brickHeight = 20;
    int padding = 10;
    int topMargin = 120;  // Move bricks down to create HUD space (HUD is y:0-75)
    
    // Deactivate all bricks first
    for (int i = 0; i < TOTAL_BRICKS; i++) {
        bricks[i].active = false;
        bricks[i].type = BRICK_NORMAL;
        bricks[i].health = 1;
        bricks[i].discovered = true;
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
        
        // Assign brick types randomly (only for active bricks)
        if (bricks[i].active) {
            int typeRoll = rand() % 100;
            if (typeRoll < 60) {
                // 60% Normal
                bricks[i].type = BRICK_NORMAL;
                bricks[i].health = 1;
            } else if (typeRoll < 75) {
                // 15% Tough (increases with level)
                bricks[i].type = BRICK_TOUGH;
                bricks[i].health = (level >= 3) ? 3 : (level == 2 ? 2 : 1);
            } else if (typeRoll < 85) {
                // 10% Explosive
                bricks[i].type = BRICK_EXPLOSIVE;
                bricks[i].health = 1;
            } else if (typeRoll < 92) {
                // 7% Speed
                bricks[i].type = BRICK_SPEED;
                bricks[i].health = 1;
            } else {
                // 8% Invisible (increases with level)
                bricks[i].type = BRICK_INVISIBLE;
                bricks[i].health = 1;
                bricks[i].discovered = (level == 1) ? true : false;
            }
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

// Destroy adjacent bricks for explosive brick effect
void DestroyAdjacentBricks(Brick bricks[], int brickIndex, int *scoreBonus) {
    if (brickIndex < 0 || brickIndex >= TOTAL_BRICKS) return;
    
    int row = brickIndex / BRICKS_WIDE;
    
    // Define adjacent positions (up, down, left, right)
    int adjacentOffsets[] = { -BRICKS_WIDE, BRICKS_WIDE, -1, 1 };
    
    for (int i = 0; i < 4; i++) {
        int adjacentIndex = brickIndex + adjacentOffsets[i];
        
        // Check bounds
        if (adjacentIndex < 0 || adjacentIndex >= TOTAL_BRICKS) continue;
        
        // Don't destroy bricks on wrong row (for left/right neighbors)
        int adjRow = adjacentIndex / BRICKS_WIDE;
        if ((i == 2 || i == 3) && adjRow != row) continue;  // Left/right check
        
        // Destroy adjacent brick
        if (bricks[adjacentIndex].active) {
            bricks[adjacentIndex].active = false;
            *scoreBonus += 10;
        }
    }
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
    Music menuMusic = LoadMusicStream("src/resources/Wav/Goblin_Tinker_Soldier_Spy.mp3");
    menuMusic.looping = true;
    bool menuMusicPlaying = false;

    // Load brick hit sound
    Sound brickHitSound = LoadSound("src/resources/Wav/mixkit-retro-game-notification-212.wav");

    // Load wall hit sound
    Sound wallHitSound = LoadSound("src/resources/Wav/mixkit-gym-ball-hitting-the-ground-2079.wav");

    // Load paddle hit sound
    Sound paddleHitSound = LoadSound("src/resources/Wav/mixkit-catching-a-basketball-ball-2081.wav");

    // Load game over sound
    Sound gameOverSound = LoadSound("src/resources/Wav/mixkit-piano-game-over-1941.wav");

    // Load lose heart sound
    Sound loseHeartSound = LoadSound("src/resources/Wav/mixkit-failure-arcade-alert-notification-240.wav");

    // Load high score from file
    LoadHighScore();

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
            row * (brickHeight + padding) + 120,  // Start at y=120 for HUD buffer
            brickWidth, 
            brickHeight 
        };
        bricks[i].active = true;
        bricks[i].type = BRICK_NORMAL;
        bricks[i].health = 1;
        bricks[i].discovered = true;
    }

    SetTargetFPS(60);

    // Initialize particle systems
    InitializeBrickParticles();
    InitializeDeathParticles();
    InitializeFloatingCombos();

    // Heart pixel pattern
    static const int heartPattern[6][7] = {
        {0, 1, 1, 0, 1, 1, 0}, // Row 0:  XX   XX
        {1, 1, 1, 1, 1, 1, 1}, // Row 1: XXXXXXX
        {1, 1, 1, 1, 1, 1, 1}, // Row 2: XXXXXXX
        {0, 1, 1, 1, 1, 1, 0}, // Row 3:  XXXXX
        {0, 0, 1, 1, 1, 0, 0}, // Row 4:   XXX
        {0, 0, 0, 1, 0, 0, 0}  // Row 5:    X
    };

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
        }
        // --- PLAYING STATE ---
        else if (gameState == PLAYING) {
            // Update level timer
            levelTimer += GetFrameTime();

            // Update combo display timer
            if (comboDisplayTimer > 0.0f) {
                comboDisplayTimer -= GetFrameTime();
            }

            // Update level notification timer
            if (levelNotificationTimer > 0.0f) {
                levelNotificationTimer -= GetFrameTime();
            }

            // Update screen shake
            if (shakeTimer > 0.0f) {
                shakeTimer -= GetFrameTime();
            }

            // Update brick particles
            UpdateBrickParticles(GetFrameTime());

            // Update paddle buff timer
            if (paddleBuffTimer > 0.0f) {
                paddleBuffTimer -= GetFrameTime();
                if (paddleBuffTimer <= 0.0f) {
                    paddle.width = paddleOriginalWidth;
                    paddleBuffTimer = 0.0f;
                }
            }

            // Update paddle squash timer
            if (paddleSquashTimer > 0.0f) {
                paddleSquashTimer -= GetFrameTime();
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
                    
                    // Add juice effects
                    TriggerScreenShake(1.5f);
                    paddleSquashTimer = PADDLE_SQUASH_DURATION;
                    
                    // Reset combo when ball touches paddle
                    brickCombo = 0;
                    comboMultiplier = 1.0f;
                }

                // Ball Collision: Bricks
                for (int i = 0; i < TOTAL_BRICKS; i++) {
                    if (bricks[i].active) {
                        if (CheckCollisionCircleRec(balls[b].position, balls[b].radius, bricks[i].rect)) {
                            balls[b].speed.y *= -1.0f;
                            PlaySound(brickHitSound);
                            
                            // Juice effects
                            TriggerScreenShake(1.0f);
                            SpawnBrickParticles((Vector2){bricks[i].rect.x + bricks[i].rect.width / 2, 
                                                           bricks[i].rect.y + bricks[i].rect.height / 2}, 8);
                            
                            // Update combo
                            brickCombo++;
                            comboMultiplier = 1.0f + (brickCombo - 1) * 0.5f;  // 1.0x, 1.5x, 2.0x, etc.
                            if (comboMultiplier > 3.0f) comboMultiplier = 3.0f;  // Cap at 3x
                            comboDisplayTimer = 1.5f;  // Display combo for 1.5 seconds
                            
                            // Spawn floating combo text at brick center if combo > 1
                            if (brickCombo > 1) {
                                Vector2 brickCenter = {bricks[i].rect.x + bricks[i].rect.width / 2, 
                                                       bricks[i].rect.y + bricks[i].rect.height / 2};
                                SpawnFloatingCombo(brickCenter, comboMultiplier);
                            }
                            
                            int scoreGain = 10;
                            
                            // Handle brick types
                            switch (bricks[i].type) {
                                case BRICK_NORMAL:
                                    bricks[i].active = false;
                                    bricksSmashed++;
                                    break;
                                    
                                case BRICK_TOUGH:
                                    bricks[i].health--;
                                    if (bricks[i].health <= 0) {
                                        bricks[i].active = false;
                                        bricksSmashed++;
                                        scoreGain = 30;  // More points for tough bricks
                                    } else {
                                        scoreGain = 5;  // Partial points for damage
                                    }
                                    break;
                                    
                                case BRICK_EXPLOSIVE:
                                    bricks[i].active = false;
                                    bricksSmashed++;
                                    DestroyAdjacentBricks(bricks, i, &scoreGain);
                                    scoreGain += 20;  // Base points + adjacent bonuses
                                    break;
                                    
                                case BRICK_SPEED:
                                    bricks[i].active = false;
                                    bricksSmashed++;
                                    // Increase ball speed permanently for this level
                                    balls[b].speed.x *= 1.2f;
                                    balls[b].speed.y *= 1.2f;
                                    scoreGain = 25;
                                    break;
                                    
                                case BRICK_INVISIBLE:
                                    bricks[i].discovered = true;  // Reveal it
                                    if (bricks[i].discovered) {
                                        bricks[i].active = false;  // Actually destroy it next hit
                                        bricksSmashed++;
                                        scoreGain = 15;  // Points for discovery and destruction
                                    } else {
                                        scoreGain = 5;  // Points for discovery
                                    }
                                    break;
                                    
                                default:
                                    bricks[i].active = false;
                                    bricksSmashed++;
                            }
                            
                            // Apply combo multiplier to score
                            score += (int)(scoreGain * comboMultiplier);
                            
                            // 20% chance to spawn power-up (except from tough with health > 0)
                            if ((rand() % 100) < POWERUP_SPAWN_CHANCE) {
                                SpawnPowerUp(powerups, (int)(bricks[i].rect.x + bricks[i].rect.width / 2), (int)bricks[i].rect.y);
                            }
                            break;
                        }
                    }
                }

                // Reset ball if it falls off screen - spawn death particles
                if (balls[b].position.y > screenHeight) {
                    SpawnDeathParticles(balls[b].position);
                    balls[b].active = false;
                }
            }

            // Update death particles
            UpdateDeathParticles(GetFrameTime());
            
            // Update floating combo text
            UpdateFloatingCombos(GetFrameTime());

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
                        if (lives < 5) lives++;  // Cap at 5 lives max
                        PlaySound(loseHeartSound);  // Reusing sound; can use different if needed
                    }
                } else if (powerups[i].rect.y > screenHeight) {
                    // Power-up fell off screen
                    powerups[i].active = false;
                }
            }

            // Check win condition
            if (CountActiveBricksInLevel(bricks) == 0) {
                // All bricks destroyed - show level summary
                levelCompletionTime = levelTimer;
                levelSummaryTimer = LEVEL_SUMMARY_DURATION;
                gameState = LEVEL_SUMMARY;
            }
            
            // Pause on P or ESC
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
                gameState = PAUSED;
            }
        }
        // --- PAUSED STATE ---
        else if (gameState == PAUSED) {
            // Resume on P or ESC
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
                gameState = PLAYING;
            }
            // Quit to menu on Q
            if (IsKeyPressed(KEY_Q)) {
                gameState = MENU;
                brickCombo = 0;
                comboMultiplier = 1.0f;
                levelTimer = 0.0f;
                score = 0;
                lives = 3;
            }
        }
        // --- LEVEL SUMMARY STATE ---
        else if (gameState == LEVEL_SUMMARY) {
            if (levelSummaryTimer > 0.0f) {
                levelSummaryTimer -= GetFrameTime();
            } else if (IsKeyPressed(KEY_SPACE)) {
                // Move to next level
                currentLevel++;
                levelNotificationTimer = 2.0f;
                levelTimer = 0.0f;
                LoadLevel(currentLevel, bricks, balls);
                paddle.x = screenWidth / 2 - 50;
                paddle.width = paddleOriginalWidth;
                paddleBuffTimer = 0.0f;
                brickCombo = 0;
                comboMultiplier = 1.0f;
                bricksSmashed = 0;
                
                // Clear power-ups for new level
                for (int i = 0; i < MAX_POWERUPS; i++) {
                    powerups[i].active = false;
                }
                
                gameState = PLAYING;
            }
        }
        // --- GAME OVER STATE ---
        else if (gameState == GAME_OVER) {
            if (IsKeyPressed(KEY_SPACE)) {
                // Update high score and reset game state
                UpdateHighScore(score);
                gameState = MENU;
                brickCombo = 0;
                comboMultiplier = 1.0f;
                levelTimer = 0.0f;
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
            
            // Draw high score
            char hsDisplay[40];
            sprintf_s(hsDisplay, sizeof(hsDisplay), "High Score: %d", highScore);
            int hsWidth = MeasureText(hsDisplay, 20);
            DrawTextWithShadow(hsDisplay, screenWidth / 2 - hsWidth / 2, 160, 20, ORANGE);
            
            // Draw subtitle
            DrawText("Click to Play or Press SPACE", screenWidth / 2 - 160, 180, 20, LIGHTGRAY);
            
            // Button handling and drawing
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
            
            // How to Play button
            Rectangle howButton = { 300, 450, 200, 60 };
            if (DrawButton(howButton, "HOW TO PLAY", 20, SKYBLUE, LIGHTGRAY)) {
                gameState = HOW_TO_PLAY;
            }
            
            // Exit button
            Rectangle exitButton = { 300, 520, 200, 60 };
            if (DrawButton(exitButton, "EXIT", 30, RED, ORANGE)) {
                break; // Exit game loop
            }
        }
        else if (gameState == PLAYING) {
            // Calculate screen shake offset
            float shakeOffsetX = 0.0f;
            float shakeOffsetY = 0.0f;
            if (shakeTimer > 0.0f) {
                shakeOffsetX = (float)((rand() % 100 - 50) / 50.0f * shakeIntensity);
                shakeOffsetY = (float)((rand() % 100 - 50) / 50.0f * shakeIntensity);
            }
            
            // Draw Bricks
            for (int i = 0; i < TOTAL_BRICKS; i++) {
                if (bricks[i].active) {
                    Rectangle shakeBrick = bricks[i].rect;
                    shakeBrick.x += shakeOffsetX;
                    shakeBrick.y += shakeOffsetY;
                    DrawRectangleRec(shakeBrick, GetBrickColor(bricks[i]));
                    DrawRectangleLines((int)shakeBrick.x, (int)shakeBrick.y, 
                                     (int)shakeBrick.width, (int)shakeBrick.height, BLACK);
                } else if (bricks[i].type == BRICK_INVISIBLE && !bricks[i].discovered && bricks[i].rect.x > 0) {
                    // Draw undiscovered invisible bricks as a faint outline
                    Rectangle shakeBrick = bricks[i].rect;
                    shakeBrick.x += shakeOffsetX;
                    shakeBrick.y += shakeOffsetY;
                    DrawRectangleLines((int)shakeBrick.x, (int)shakeBrick.y, 
                                     (int)shakeBrick.width, (int)shakeBrick.height, (Color){100, 100, 100, 80});
                }
            }

            // Draw Balls with trail
            for (int i = 0; i < MAX_BALLS; i++) {
                if (balls[i].active) {
                    // Draw ball trail (semi-transparent circles behind ball)
                    for (int t = 1; t <= 4; t++) {
                        float trailAlpha = (1.0f - (float)t / 4.0f) * 0.5f;  // Fade effect
                        Vector2 trailPos = {
                            balls[i].position.x - balls[i].speed.x * t * 0.1f + shakeOffsetX,
                            balls[i].position.y - balls[i].speed.y * t * 0.1f + shakeOffsetY
                        };
                        DrawCircleV(trailPos, balls[i].radius * 0.6f, (Color){255, 255, 255, (unsigned char)(150 * trailAlpha)});
                    }
                    
                    // Draw ball
                    DrawCircleV((Vector2){balls[i].position.x + shakeOffsetX, balls[i].position.y + shakeOffsetY}, 
                               balls[i].radius, WHITE);
                    
                    // Draw floating combo message above ball (if active)
                    if (comboDisplayTimer > 0.0f && brickCombo > 1) {
                        float fadeAlpha = comboDisplayTimer / 1.5f;  // Fade out
                        float floatOffset = (1.5f - comboDisplayTimer) * 30.0f;  // Rise upward
                        
                        char comboText[30];
                        sprintf_s(comboText, sizeof(comboText), "COMBO x%.1f!", comboMultiplier);
                        int comboTextWidth = MeasureText(comboText, 24);
                        
                        Vector2 comboPos = {
                            balls[i].position.x + shakeOffsetX - comboTextWidth / 2,
                            balls[i].position.y + shakeOffsetY - 50 - floatOffset
                        };
                        
                        Color comboColor = (Color){255, 165, 0, (unsigned char)(255 * fadeAlpha)};
                        DrawText(comboText, (int)comboPos.x + 1, (int)comboPos.y + 1, 24, (Color){0, 0, 0, (unsigned char)(150 * fadeAlpha)});
                        DrawText(comboText, (int)comboPos.x, (int)comboPos.y, 24, comboColor);
                    }
                }
            }

            // Draw Power-Ups
            for (int i = 0; i < MAX_POWERUPS; i++) {
                if (powerups[i].active) {
                    Rectangle shakePowerUp = powerups[i].rect;
                    shakePowerUp.x += shakeOffsetX;
                    shakePowerUp.y += shakeOffsetY;
                    DrawRectangleRec(shakePowerUp, powerups[i].color);
                    DrawRectangleLines((int)shakePowerUp.x, (int)shakePowerUp.y, 
                                     (int)shakePowerUp.width, (int)shakePowerUp.height, WHITE);
                }
            }

            // Draw Paddle with squash/stretch effect
            Rectangle paddleToDraw = paddle;
            paddleToDraw.x += shakeOffsetX;
            paddleToDraw.y += shakeOffsetY;
            
            if (paddleSquashTimer > 0.0f) {
                float squashAmount = 1.0f - (paddleSquashTimer / PADDLE_SQUASH_DURATION);
                squashAmount *= 0.2f;  // Max 20% squash
                paddleToDraw.height = paddle.height * (1.0f - squashAmount);
                paddleToDraw.width = paddle.width * (1.0f + squashAmount * 0.5f);
                paddleToDraw.y += paddle.height * squashAmount * 0.5f;
            }
            
            DrawRectangleRec(paddleToDraw, BLUE);
            DrawRectangleLines((int)paddleToDraw.x, (int)paddleToDraw.y, (int)paddleToDraw.width, (int)paddleToDraw.height, SKYBLUE);

            // Draw particles (explosions, brick destruction)
            DrawBrickParticles();
            
            // Draw death particles
            DrawDeathParticles();
            
            // Draw floating combo text

            // === CLEAN MINIMALIST HUD (y: 0-80) ===
            
            // Main HUD Bar Background
            DrawRectangle(0, 0, screenWidth, 80, Fade((Color){20, 20, 30, 255}, 0.85f));
            DrawLine(0, 80, screenWidth, 80, (Color){100, 100, 120, 255});
            
            // LEFT SECTION: Score and Level
            DrawTextWithShadow("SCORE", 20, 12, 14, ORANGE);
            char scoreStr[20];
            sprintf_s(scoreStr, sizeof(scoreStr), "%d", score);
            DrawTextWithShadow(scoreStr, 20, 32, 28, YELLOW);
            
            DrawTextWithShadow("LEVEL", 130, 12, 14, ORANGE);
            char levelStr[10];
            sprintf_s(levelStr, sizeof(levelStr), "%d", currentLevel);
            DrawTextWithShadow(levelStr, 130, 32, 28, GREEN);
            
            // CENTER SECTION: Bricks and Combo
            char bricksStr[20];
            sprintf_s(bricksStr, sizeof(bricksStr), "BRICKS: %d", CountActiveBricksInLevel(bricks));
            int bricksWidth = MeasureText(bricksStr, 18);
            DrawTextWithShadow(bricksStr, screenWidth / 2 - bricksWidth / 2, 15, 18, SKYBLUE);
            
            // Combo display in center
            if (brickCombo > 1) {
                char comboStr[20];
                sprintf_s(comboStr, sizeof(comboStr), "COMBO: %.1fx", comboMultiplier);
                int comboWidth = MeasureText(comboStr, 18);
                Color comboColor = (Color){255, 200, 0, 255};  // Golden
                DrawTextWithShadow(comboStr, screenWidth / 2 - comboWidth / 2, 42, 18, comboColor);
            }
            
            // RIGHT SECTION: Time and Lives
            // Hearts (Lives) - positioned before time
            int heartsStartX = screenWidth - 220;
            int heartsStartY = 48;
            
            // Draw hearts first
            int displayLives = lives > 5 ? 5 : lives;  // Cap at 5 hearts
            int numHeartsDisplayed = displayLives;
            
            char timeStr[20];
            sprintf_s(timeStr, sizeof(timeStr), "%.1fs", levelTimer);
            DrawTextWithShadow("TIME", screenWidth - 100, 12, 14, ORANGE);
            DrawTextWithShadow(timeStr, screenWidth - 100, 32, 24, SKYBLUE);
            
            for (int i = 0; i < numHeartsDisplayed; i++) {
                int baseX = heartsStartX + (i * 28);
                int baseY = heartsStartY;
                
                for (int row = 0; row < 6; row++) {
                    for (int col = 0; col < 7; col++) {
                        if (heartPattern[row][col] == 1) {
                            DrawRectangle(
                                baseX + (col * 3),
                                baseY + (row * 3),
                                3,
                                3,
                                RED
                            );
                        }
                    }
                }
            }
            
            // Buff progress bar (if active)
            if (paddleBuffTimer > 0.0f) {
                DrawText("BUFF", heartsStartX - 60, 52, 12, LIME);
                
                Rectangle buffBarBg = {heartsStartX - 60, 68, 50, 6};
                DrawRectangleRec(buffBarBg, DARKGRAY);
                DrawRectangleLines((int)buffBarBg.x, (int)buffBarBg.y, (int)buffBarBg.width, (int)buffBarBg.height, LIME);
                
                float buffProgress = paddleBuffTimer / 10.0f;
                if (buffProgress > 1.0f) buffProgress = 1.0f;
                Rectangle buffBarFill = {buffBarBg.x, buffBarBg.y, buffBarBg.width * buffProgress, buffBarBg.height};
                DrawRectangleRec(buffBarFill, LIME);
            }

            // Level notification (center screen, appears for 3 seconds)
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
            DrawText("GAME OVER!", screenWidth / 2 - 150, 150, 60, RED);
            char finalScore[30];
            sprintf_s(finalScore, sizeof(finalScore), "Final Score: %d", score);
            DrawText(finalScore, screenWidth / 2 - 150, 260, 40, YELLOW);
            
            // Show if new high score
            if (score >= highScore && score > 0) {
                DrawTextWithShadow("NEW HIGH SCORE!", screenWidth / 2 - 140, 320, 30, GOLD);
            } else {
                char hsText[40];
                sprintf_s(hsText, sizeof(hsText), "High Score: %d", highScore);
                DrawText(hsText, screenWidth / 2 - 130, 320, 25, LIGHTGRAY);
            }
            
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
        else if (gameState == HOW_TO_PLAY) {
            // Draw How to Play page with card grid layout
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){15, 15, 25, 255});
            
            // Centered title using MeasureText
            const char* titleText = "HOW TO PLAY";
            int titleWidth = MeasureText(titleText, 50);
            DrawTextWithShadow(titleText, screenWidth / 2 - titleWidth / 2, 15, 50, YELLOW);
            
            // CARD 1: CONTROLS
            int cardX = 20;
            int cardY = 80;
            int cardWidth = 340;
            int cardHeight = 200;
            
            DrawRectangle(cardX, cardY, cardWidth, cardHeight, Fade((Color){40, 40, 60, 255}, 0.7f));
            DrawRectangleLines(cardX, cardY, cardWidth, cardHeight, SKYBLUE);
            
            // Centered card title
            const char* controlsTitle = "CONTROLS";
            int controlsTitleWidth = MeasureText(controlsTitle, 22);
            DrawTextWithShadow(controlsTitle, cardX + (cardWidth - controlsTitleWidth) / 2, cardY + 10, 22, ORANGE);
            DrawLine(cardX + 15, cardY + 40, cardX + cardWidth - 15, cardY + 40, SKYBLUE);
            
            DrawText("↔ LEFT/RIGHT ARROW", cardX + 20, cardY + 55, 16, WHITE);
            DrawText("Move Paddle", cardX + 50, cardY + 75, 14, LIGHTGRAY);
            
            DrawText("P or ESC", cardX + 20, cardY + 100, 16, WHITE);
            DrawText("Pause Game", cardX + 50, cardY + 120, 14, LIGHTGRAY);
            
            DrawText("SPACE", cardX + 20, cardY + 145, 16, WHITE);
            DrawText("Continue / Confirm", cardX + 50, cardY + 165, 14, LIGHTGRAY);
            
            // CARD 2: POWER-UPS
            cardX = 380;
            cardY = 80;
            
            DrawRectangle(cardX, cardY, cardWidth, cardHeight, Fade((Color){40, 40, 60, 255}, 0.7f));
            DrawRectangleLines(cardX, cardY, cardWidth, cardHeight, MAGENTA);
            
            // Centered card title
            const char* powerupsTitle = "POWER-UPS";
            int powerupsTitleWidth = MeasureText(powerupsTitle, 22);
            DrawTextWithShadow(powerupsTitle, cardX + (cardWidth - powerupsTitleWidth) / 2, cardY + 10, 22, ORANGE);
            DrawLine(cardX + 15, cardY + 40, cardX + cardWidth - 15, cardY + 40, MAGENTA);
            
            // Multiball
            DrawRectangle(cardX + 15, cardY + 55, 16, 16, SKYBLUE);
            DrawText("Multiball - Extra Ball", cardX + 40, cardY + 55, 14, WHITE);
            
            // Wide Paddle
            DrawRectangle(cardX + 15, cardY + 80, 16, 16, LIME);
            DrawText("Wide Paddle - 10s", cardX + 40, cardY + 80, 14, WHITE);
            
            // Screen Wide
            DrawRectangle(cardX + 15, cardY + 105, 16, 16, MAGENTA);
            DrawText("Screen Wide - 5s", cardX + 40, cardY + 105, 14, WHITE);
            
            // Extra Life
            DrawRectangle(cardX + 15, cardY + 130, 16, 16, RED);
            DrawText("Extra Life - +1 Life", cardX + 40, cardY + 130, 14, WHITE);
            
            // Centered spawn rate text
            const char* spawnText = "20% spawn rate";
            int spawnTextWidth = MeasureText(spawnText, 12);
            DrawText(spawnText, cardX + (cardWidth - spawnTextWidth) / 2, cardY + 165, 12, SKYBLUE);
            
            // CARD 3: BRICK TYPES (Row 2)
            cardX = 20;
            cardY = 300;
            
            DrawRectangle(cardX, cardY, cardWidth, 220, Fade((Color){40, 40, 60, 255}, 0.7f));
            DrawRectangleLines(cardX, cardY, cardWidth, 220, GREEN);
            
            // Centered card title
            const char* bricksTitle = "BRICK TYPES";
            int bricksTitleWidth = MeasureText(bricksTitle, 22);
            DrawTextWithShadow(bricksTitle, cardX + (cardWidth - bricksTitleWidth) / 2, cardY + 10, 22, ORANGE);
            DrawLine(cardX + 15, cardY + 40, cardX + cardWidth - 15, cardY + 40, GREEN);
            
            // Normal Brick
            DrawRectangle(cardX + 15, cardY + 55, 16, 12, RED);
            DrawText("Normal - 1 hit, 10 pts", cardX + 40, cardY + 54, 13, WHITE);
            
            // Tough Brick
            DrawRectangle(cardX + 15, cardY + 80, 16, 12, (Color){80, 0, 0, 255});
            DrawText("Tough - 2-3 hits, 30 pts", cardX + 40, cardY + 79, 13, WHITE);
            
            // Explosive
            DrawRectangle(cardX + 15, cardY + 105, 16, 12, PURPLE);
            DrawText("Explosive - AOE, 50 pts", cardX + 40, cardY + 104, 13, WHITE);
            
            // Speed
            DrawRectangle(cardX + 15, cardY + 130, 16, 12, LIME);
            DrawText("Speed - Ball +20%", cardX + 40, cardY + 129, 13, WHITE);
            
            // Invisible
            DrawRectangle(cardX + 15, cardY + 155, 16, 12, GRAY);
            DrawRectangleLines(cardX + 15, cardY + 155, 16, 12, WHITE);
            DrawText("Invisible - Hidden, 15 pts", cardX + 40, cardY + 154, 13, WHITE);
            
            // CARD 4: SCORING BONUSES
            cardX = 380;
            cardY = 300;
            
            DrawRectangle(cardX, cardY, cardWidth, 220, Fade((Color){40, 40, 60, 255}, 0.7f));
            DrawRectangleLines(cardX, cardY, cardWidth, 220, ORANGE);
            
            // Centered card title
            const char* scoringTitle = "SCORING";
            int scoringTitleWidth = MeasureText(scoringTitle, 22);
            DrawTextWithShadow(scoringTitle, cardX + (cardWidth - scoringTitleWidth) / 2, cardY + 10, 22, ORANGE);
            DrawLine(cardX + 15, cardY + 40, cardX + cardWidth - 15, cardY + 40, ORANGE);
            
            // Centered COMBO SYSTEM label
            const char* comboLabel = "COMBO SYSTEM";
            int comboLabelWidth = MeasureText(comboLabel, 16);
            DrawText(comboLabel, cardX + (cardWidth - comboLabelWidth) / 2, cardY + 55, 16, YELLOW);
            
            DrawText("Hit bricks without", cardX + 25, cardY + 80, 12, WHITE);
            DrawText("touching paddle", cardX + 25, cardY + 97, 12, WHITE);
            DrawText("Multiplier: 1.0x → 3.0x", cardX + 25, cardY + 114, 12, ORANGE);
            
            // Centered TIME BONUS label
            const char* bonusLabel = "TIME BONUS";
            int bonusLabelWidth = MeasureText(bonusLabel, 16);
            DrawText(bonusLabel, cardX + (cardWidth - bonusLabelWidth) / 2, cardY + 145, 16, YELLOW);
            
            DrawText("Complete < 60s", cardX + 25, cardY + 170, 12, WHITE);
            DrawText("Up to 500 pts", cardX + 25, cardY + 187, 12, ORANGE);
            
            // Back button centered
            Rectangle backButton = { screenWidth / 2 - 100, 540, 200, 50 };
            if (DrawButton(backButton, "BACK", 25, ORANGE, YELLOW)) {
                gameState = MENU;
            }
        }
        else if (gameState == PAUSED) {
            // Draw semi-transparent overlay
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
            
            // Draw pause menu
            DrawTextWithShadow("PAUSED", screenWidth / 2 - 120, 150, 60, YELLOW);
            DrawText("Press P or ESC to Resume", screenWidth / 2 - 180, 250, 25, LIGHTGRAY);
            DrawText("Press Q to Quit to Menu", screenWidth / 2 - 170, 300, 25, LIGHTGRAY);
            
            // Show current stats
            char pauseStats[100];
            sprintf_s(pauseStats, sizeof(pauseStats), "Level: %d | Score: %d | Time: %.1fs", currentLevel, score, levelTimer);
            int statsWidth = MeasureText(pauseStats, 20);
            DrawTextWithShadow(pauseStats, screenWidth / 2 - statsWidth / 2, 380, 20, SKYBLUE);
        }
        else if (gameState == LEVEL_SUMMARY) {
            // Draw level summary screen
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.3f));
            
            // Draw background box
            DrawRectangle(screenWidth / 2 - 200, 100, 400, 400, Fade(DARKBLUE, 0.8f));
            DrawRectangleLines(screenWidth / 2 - 200, 100, 400, 400, SKYBLUE);
            
            // Draw title
            DrawTextWithShadow("LEVEL COMPLETE!", screenWidth / 2 - 160, 130, 50, GOLD);
            
            // Draw stats
            char bricksText[50];
            sprintf_s(bricksText, sizeof(bricksText), "Bricks Smashed: %d", bricksSmashed);
            DrawTextWithShadow(bricksText, screenWidth / 2 - 140, 220, 25, LIME);
            
            char timeText[50];
            sprintf_s(timeText, sizeof(timeText), "Time Taken: %.1f seconds", levelCompletionTime);
            DrawTextWithShadow(timeText, screenWidth / 2 - 140, 270, 25, SKYBLUE);
            
            // Calculate and display time bonus
            int timeBonus = 0;
            if (levelCompletionTime < TIME_LIMIT) {
                timeBonus = (int)(MAX_TIME_BONUS * (1.0f - levelCompletionTime / TIME_LIMIT));
                score += timeBonus;
            }
            
            char bonusText[60];
            sprintf_s(bonusText, sizeof(bonusText), "Time Bonus: +%d pts", timeBonus);
            DrawTextWithShadow(bonusText, screenWidth / 2 - 140, 320, 25, ORANGE);
            
            char totalText[50];
            sprintf_s(totalText, sizeof(totalText), "Total Score: %d", score);
            DrawTextWithShadow(totalText, screenWidth / 2 - 140, 370, 25, YELLOW);
            
            // Auto-advance or press space to continue
            if (levelSummaryTimer <= 0.0f) {
                DrawText("Press SPACE to continue", screenWidth / 2 - 150, 450, 20, LIGHTGRAY);
            } else {
                char autoAdvanceText[50];
                sprintf_s(autoAdvanceText, sizeof(autoAdvanceText), "Next level in %.1f...", levelSummaryTimer);
                DrawText(autoAdvanceText, screenWidth / 2 - 120, 450, 20, LIME);
            }
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