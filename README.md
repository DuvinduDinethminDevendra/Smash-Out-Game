# Smash Out! - A Raylib Breakout Game

A feature-rich breakout/brick-breaker game built with **Raylib** and **C99**. Bounce a ball off your paddle to destroy bricks, earn points, and progress through dynamically generated levels with increasing difficulty.

## Features

### Core Gameplay
- **Classic Breakout Mechanics**: Bounce a ball off your paddle to destroy bricks and clear levels
- **Progressive Difficulty**: Speed increases and brick patterns change with each level
- **Lives System**: Start with 3 lives; gain extra lives through power-up drops
- **Score Tracking**: Earn points for each brick destroyed with HUD display

### Advanced Systems
- **Procedural Level Generation**: Levels dynamically scale in difficulty with:
  - Scaling brick rows (2 base rows + 1 per level, capped at 8)
  - Pattern variations (standard, checkerboard for even levels, pyramid for divisible by 3)
  - Ball speed increases (0.5f per level)
  
- **Power-Up System**: Random drops with 20% spawn chance per destroyed brick:
  - **MULTIBALL**: Spawns an additional ball for simultaneous gameplay
  - **WIDE_PADDLE**: Doubles paddle width for 10 seconds
  - **SCREEN_WIDE**: Expands paddle to full screen width for 5 seconds
  - **EXTRA_LIFE**: Grants an additional life

- **Professional UI/UX**:
  - Interactive menu with animated title (bobbing effect)
  - Hover-responsive buttons for START, SETTINGS, and EXIT
  - Semi-transparent HUD bar with organized information:
    - Score (top-left, yellow)
    - Current level (top-center, green)
    - Remaining bricks (top-right, gray)
    - Lives display with pixel-art hearts (top-right)
  - Paddle buff duration timer
  - Level notifications with fade effects
  - Professional text shadows for readability

- **Audio System**:
  - Menu background music (looping)
  - Sound effects for: brick hits, wall hits, paddle hits, losing hearts, game over
  - Master volume control in Settings (0-100%)

- **Settings Menu**:
  - Interactive volume slider (click and drag support)
  - Real-time volume adjustment with `SetMasterVolume()`
  - Percentage display
  - Easy navigation back to menu

### Game States
- **MENU**: Main menu with interactive buttons and animated background particles
- **PLAYING**: Core gameplay with HUD, collision detection, and entity management
- **SETTINGS**: Volume control and configuration options
- **GAME_OVER**: Game over screen with final score display
- **WIN**: Victory screen for completing all levels

## Controls

| Action | Key |
|--------|-----|
| Move Paddle Left | `LEFT ARROW` |
| Move Paddle Right | `RIGHT ARROW` |
| Navigate Menu | Click buttons |
| Adjust Volume | Click slider |
| Return to Menu | `ESC` or `SPACE` (in SETTINGS) |
| Exit Game | Click EXIT button |

## Technical Details

### Requirements
- **Raylib**: Game framework for graphics, audio, and input
- **C99 Standard**: Compiled with MinGW gcc
- **Platform**: Windows (w64devkit)

### Project Structure
```
Smash-Out-Game/
├── src/
│   ├── smash_out.c          # Main game source code
│   ├── Makefile             # Build configuration
│   └── resources/
│       └── Wav/             # Audio files (mp3, wav)
├── resources/
│   └── images/              # Game assets
├── README.md                # This file
└── LICENSE                  # Project license
```

### Build & Run

**Compile the game:**
```bash
cd src
mingw32-make clean
mingw32-make
```

**Run the executable:**
```bash
.\smash_out.exe
```

Or use one command:
```bash
mingw32-make clean && mingw32-make && .\smash_out.exe
```

### Code Architecture

**Key Data Structures:**
- `Ball`: Tracks position, velocity, radius, and active state (array of up to 5)
- `Brick`: Rectangle and active state (10×5 grid, ~50 per level)
- `PowerUp`: Type, position, size, and color (up to 50 simultaneous)
- `GameState`: Enum for menu, playing, game over, win, and settings states

**Key Functions:**
- `LoadLevel()`: Procedurally generates levels based on current level number
- `SpawnPowerUp()`: Randomly creates power-ups at brick destruction points
- `SpawnBall()`: Adds new ball with velocity variation (for MULTIBALL)
- `DrawButton()`: Interactive button with hover detection
- `DrawTextWithShadow()`: Text rendering with shadow offset for readability
- `UpdateParticles()` / `DrawParticles()`: Menu background animation

**Global Variables:**
- `currentLevel`: Tracks player progress
- `levelNotificationTimer`: Manages level notification fade effects
- `masterVolume`: Master audio volume (0.0 to 1.0)

## Game Loop Flow

1. **Initialization**: Load audio, textures, and set up game state
2. **Menu State**: Display interactive menu with particle background
3. **Settings State**: Allow volume adjustment via slider
4. **Playing State**:
   - Update paddle position based on arrow keys
   - Move all active balls and handle collisions (walls, paddle, bricks)
   - Check for power-up collection
   - Update power-up buffs with timers
   - Check win condition (all bricks destroyed)
5. **Game Over/Win States**: Display results and allow return to menu
6. **Audio Management**: Stream menu music, play sound effects for events

## Features Implemented

✅ Core breakout gameplay with ball physics and paddle control  
✅ Brick destruction with collision detection  
✅ Lives system with visual hearts display  
✅ Score tracking and HUD organization  
✅ Audio integration (5 sound effects + menu music)  
✅ Power-up system (4 types with equal spawn probability)  
✅ Multi-ball support (up to 5 simultaneous balls)  
✅ Professional interactive menu with animations  
✅ Procedural level system with dynamic difficulty  
✅ Professional HUD with semi-transparent background  
✅ Text shadows for visual depth  
✅ Level notifications with fade effects  
✅ Settings menu with master volume control  

## Future Enhancements

- High score persistence (file I/O)
- Additional power-up types (slow ball, fast ball, laser paddle)
- Difficulty settings (affecting ball speed, power-up spawn rate)
- Pause/resume functionality
- Boss levels at milestones
- Particle effects on brick destruction
- Additional sound effects for power-up collection

## License

See [LICENSE](LICENSE) file for details.

## Credits

Built with **Raylib** - A simple and easy-to-use library to enjoy videogames programming.  
Audio assets from free sound effect libraries.
