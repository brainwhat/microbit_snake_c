/**
 * Snake Game for micro:bit v2
 *
 * A conversion of the Rust snake game to C using CODAL
 * Originally based on snake-game by Alan Bunbury
 */

#include "MicroBit.h"
#include "CodalDmesg.h"

// Create a global instance of the MicroBit class
MicroBit uBit;

// Game constants
#define MAX_SNAKE_LENGTH 24
#define GRID_SIZE 5
#define HEAD_BRIGHTNESS 255
#define TAIL_BRIGHTNESS 128
#define FOOD_BRIGHTNESS 255

// Direction definitions
typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

// Turn definitions
typedef enum {
    TURN_NONE,
    TURN_LEFT,
    TURN_RIGHT
} Turn;

// Game status
typedef enum {
    ONGOING,
    WON,
    LOST
} GameStatus;

// Step outcome
typedef enum {
    MOVE,
    EAT,
    COLLISION,
    FULL
} StepOutcome;

// Coordinates on the grid
typedef struct {
    int8_t row;
    int8_t col;
} Coords;

// Snake structure
typedef struct {
    Coords head;
    Coords tail[MAX_SNAKE_LENGTH];
    int tailLength;
    Direction direction;
} Snake;

// Game state
typedef struct {
    Snake snake;
    Coords foodCoords;
    uint8_t speed;
    GameStatus status;
    uint8_t score;
} Game;

// Global variables for game state
Game game;
Turn currentTurn = TURN_NONE;

// Function declarations
void initGame(void);
void resetGame(void);
void placeFood(void);
bool coordsEqual(Coords a, Coords b);
bool coordsInSnake(Coords coords);
Coords getRandomCoords(void);
Coords wraparound(Coords coords);
Coords getNextMove(void);
StepOutcome getStepOutcome(void);
void moveSnake(Coords coords, bool extend);
void handleStepOutcome(StepOutcome outcome);
void turnSnake(Turn turn);
void stepGame(void);
uint32_t getStepLengthMs(void);
void displayGameState(void);
void displayScore(void);
void handleButtonA(MicroBitEvent e);
void handleButtonB(MicroBitEvent e);

// Main function
int main() {
    // Initialize the micro:bit runtime
    uBit.init();

    // Register button handlers
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_A, MICROBIT_BUTTON_EVT_CLICK, handleButtonA);
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_B, MICROBIT_BUTTON_EVT_CLICK, handleButtonB);

    // Seed the random number generator with current time
    srand(system_timer_current_time());

    // Initialize game state
    initGame();

    // Main game loop
    while (1) {
        // Game loop
        while (game.status == ONGOING) {
            displayGameState();
            uBit.sleep(getStepLengthMs());
            stepGame();
        }

        // Game over - flash the final state
        for (int i = 0; i < 3; i++) {
            uBit.display.clear();
            uBit.sleep(200);
            displayGameState();
            uBit.sleep(200);
        }

        // Show the score
        uBit.display.clear();
        displayScore();
        uBit.sleep(2000);

        // Reset game for next round
        resetGame();
    }

    // We should never reach here
    return 0;
}

// Initialize the game state
void initGame(void) {
    // Initialize snake
    game.snake.head.row = 2;
    game.snake.head.col = 2;
    game.snake.tail[0].row = 2;
    game.snake.tail[0].col = 1;
    game.snake.tailLength = 1;
    game.snake.direction = RIGHT;

    // Initialize game state
    game.speed = 1;
    game.status = ONGOING;
    game.score = 0;

    // Place initial food
    placeFood();
}

// Reset the game state to start a new game
void resetGame(void) {
    initGame();
    currentTurn = TURN_NONE;
}

// Check if two coordinates are equal
bool coordsEqual(Coords a, Coords b) {
    return (a.row == b.row && a.col == b.col);
}

// Check if coordinates are in the snake's body
bool coordsInSnake(Coords coords) {
    if (coordsEqual(coords, game.snake.head)) {
        return true;
    }

    for (int i = 0; i < game.snake.tailLength; i++) {
        if (coordsEqual(coords, game.snake.tail[i])) {
            return true;
        }
    }

    return false;
}

// Get random coordinates not occupied by the snake
Coords getRandomCoords(void) {
    Coords coords;
    do {
        coords.row = rand() % GRID_SIZE;
        coords.col = rand() % GRID_SIZE;
    } while (coordsInSnake(coords));

    return coords;
}

// Place food at a random location on the grid
void placeFood(void) {
    game.foodCoords = getRandomCoords();
}

// Wrap around out of bounds coordinates
Coords wraparound(Coords coords) {
    Coords result = coords;

    if (result.row < 0) {
        result.row = GRID_SIZE - 1;
    } else if (result.row >= GRID_SIZE) {
        result.row = 0;
    }

    if (result.col < 0) {
        result.col = GRID_SIZE - 1;
    } else if (result.col >= GRID_SIZE) {
        result.col = 0;
    }

    return result;
}

// Check if coordinates are out of bounds
bool isOutOfBounds(Coords coords) {
    return (coords.row < 0 || coords.row >= GRID_SIZE ||
            coords.col < 0 || coords.col >= GRID_SIZE);
}

// Get the next position the snake will move to
Coords getNextMove(void) {
    Coords nextMove = game.snake.head;

    switch (game.snake.direction) {
        case UP:
            nextMove.row--;
            break;
        case DOWN:
            nextMove.row++;
            break;
        case LEFT:
            nextMove.col--;
            break;
        case RIGHT:
            nextMove.col++;
            break;
    }

    if (isOutOfBounds(nextMove)) {
        return wraparound(nextMove);
    }

    return nextMove;
}

// Determine the outcome of the snake's next move
StepOutcome getStepOutcome(void) {
    Coords nextMove = getNextMove();

    // Check if next move collides with snake body (but not if it's the end of the tail)
    bool isCollision = false;
    if (coordsInSnake(nextMove)) {
        // If it's not the end of the tail, it's a collision
        if (!coordsEqual(nextMove, game.snake.tail[0])) {
            return COLLISION;
        }
    }

    // Check if next move is food
    if (coordsEqual(nextMove, game.foodCoords)) {
        // Check if the grid is full
        if (game.snake.tailLength >= MAX_SNAKE_LENGTH - 1) {
            return FULL;
        }
        return EAT;
    }

    // Otherwise, it's just a regular move
    return MOVE;
}

// Move the snake to new coordinates
void moveSnake(Coords coords, bool extend) {
    // Shift tail
    if (!extend) {
        // Remove end of tail if not extending
        for (int i = 0; i < game.snake.tailLength - 1; i++) {
            game.snake.tail[i] = game.snake.tail[i + 1];
        }
        // Move previous head to tail
        game.snake.tail[game.snake.tailLength - 1] = game.snake.head;
    } else {
        // Add new tail segment and shift
        for (int i = game.snake.tailLength; i > 0; i--) {
            game.snake.tail[i] = game.snake.tail[i - 1];
        }
        // Move previous head to front of tail
        game.snake.tail[0] = game.snake.head;
        // Increase tail length
        game.snake.tailLength++;
    }

    // Move head to new coords
    game.snake.head = coords;
}

// Handle the outcome of a step
void handleStepOutcome(StepOutcome outcome) {
    Coords nextMove = getNextMove();

    switch (outcome) {
        case COLLISION:
            game.status = LOST;
            break;

        case FULL:
            moveSnake(nextMove, true);
            game.status = WON;
            break;

        case EAT:
            moveSnake(nextMove, true);
            placeFood();
            game.score++;
            if (game.score % 5 == 0) {
                game.speed++;
            }
            break;

        case MOVE:
            moveSnake(nextMove, false);
            break;
    }
}

// Turn the snake in a new direction
void turnSnake(Turn turn) {
    switch (turn) {
        case TURN_LEFT:
            switch (game.snake.direction) {
                case UP:
                    game.snake.direction = LEFT;
                    break;
                case DOWN:
                    game.snake.direction = RIGHT;
                    break;
                case LEFT:
                    game.snake.direction = DOWN;
                    break;
                case RIGHT:
                    game.snake.direction = UP;
                    break;
            }
            break;

        case TURN_RIGHT:
            switch (game.snake.direction) {
                case UP:
                    game.snake.direction = RIGHT;
                    break;
                case DOWN:
                    game.snake.direction = LEFT;
                    break;
                case LEFT:
                    game.snake.direction = UP;
                    break;
                case RIGHT:
                    game.snake.direction = DOWN;
                    break;
            }
            break;

        case TURN_NONE:
            // No change
            break;
    }
}

// Perform a single game step
void stepGame(void) {
    // Process any pending turns
    turnSnake(currentTurn);
    currentTurn = TURN_NONE;

    // Get and handle the outcome of the step
    StepOutcome outcome = getStepOutcome();
    handleStepOutcome(outcome);
}

// Calculate step length based on game speed
uint32_t getStepLengthMs(void) {
    int32_t stepLength = 1000 - (200 * ((int32_t)game.speed - 1));
    if (stepLength < 200) {
        stepLength = 200;
    }
    return (uint32_t)stepLength;
}

// Display the current game state on the LED matrix
void displayGameState(void) {
    // Clear the display
    uBit.display.clear();

    // Draw the snake's head
    uBit.display.image.setPixelValue(game.snake.head.col, game.snake.head.row, HEAD_BRIGHTNESS);

    // Draw the snake's tail
    for (int i = 0; i < game.snake.tailLength; i++) {
        uBit.display.image.setPixelValue(game.snake.tail[i].col, game.snake.tail[i].row, TAIL_BRIGHTNESS);
    }

    // Draw the food
    uBit.display.image.setPixelValue(game.foodCoords.col, game.foodCoords.row, FOOD_BRIGHTNESS);
}

// Display the score on the LED matrix
void displayScore(void) {
    int fullRows = game.score / 5;
    int remainingCols = game.score % 5;

    // Light up full rows
    for (int row = 0; row < fullRows; row++) {
        for (int col = 0; col < 5; col++) {
            uBit.display.image.setPixelValue(col, row, 255);
        }
    }

    // Light up remaining columns in the next row
    for (int col = 0; col < remainingCols; col++) {
        uBit.display.image.setPixelValue(col, fullRows, 255);
    }
}

// Button A handler - turn left
void handleButtonA(MicroBitEvent e) {
    currentTurn = TURN_LEFT;
}

// Button B handler - turn right
void handleButtonB(MicroBitEvent e) {
    currentTurn = TURN_RIGHT;
}
