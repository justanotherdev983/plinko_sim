#include "raylib.h"
#include <string>
#include <vector>
#include <cstdio>
#include <random>
#include <cmath>
#include <algorithm> 

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 1200
#define FONT_SIZE 30

const int NUM_ROWS = 18;
const float PIN_RADIUS = 5.0f;
const float PIN_SPACING_X = 50.0f;
const float PIN_SPACING_Y = 45.0f;
const float START_Y = WINDOW_HEIGHT / 4.0f + 20.0f;

const float BALL_RADIUS = 8.0f;
const float GRAVITY = 0.3f;
const float BOUNCE_DAMPING = 0.7f;
const float FRICTION = 0.995f;

const Color COLOR_BG = { 0x18, 0x18, 0x18, 0xFF };
const Color COLOR_PIN = { 180, 180, 180, 255 };
const Color COLOR_PIN_HIGHLIGHT = { 230, 230, 230, 255 };
const Color COLOR_BALL = { 255, 203, 0, 255 };
const Color COLOR_BALL_SHADOW = { 205, 133, 63, 255 };
const Color COLOR_UI_PRIMARY = { 40, 40, 40, 255 };
const Color COLOR_UI_ACCENT = { 80, 80, 80, 255 };
const Color COLOR_TEXT = { 245, 245, 245, 255 };
const Color COLOR_WIN = { 46, 204, 113, 255 };
const Color COLOR_LOSE = { 231, 76, 60, 255 };

const std::vector<float> PRIZE_MULTIPLIERS = {
    1000.0f, 130.0f, 26.0f, 9.0f, 4.0f, 2.0f, 0.5f, 0.2f, 0.2f, 0.2f,
    0.2f, 
    0.2f, 0.2f, 0.2f, 0.5f, 2.0f, 4.0f, 9.0f, 26.0f, 130.0f, 1000.0f
};

struct Ball {
    Vector2 position;
    Vector2 velocity;
    bool active;
    int finalBin;
    int winAmount;
    int betAmount;

    Ball() : position({0, 0}), velocity({0, 0}), active(false), finalBin(-1), winAmount(0), betAmount(0) {}
};

struct Pin {
    Vector2 position;
    Pin(float x, float y) : position({x, y}) {}
};

struct LastWinInfo {
    int amountWon = 0;
    int betAmount = 0;
    int finalBin = -1;
    float displayTimer = 0.0f;
};
const float WIN_DISPLAY_DURATION = 2.5f;

std::vector<Ball> balls;
std::vector<Pin> pins;
std::vector<Vector2> binCenters;
int balance = 1000;
int currentBet = 10;
LastWinInfo lastWin;
std::mt19937 rng(std::random_device{}());

const std::vector<int> BET_AMOUNTS = {1, 5, 10, 25, 50, 100, 250, 500};
int betIndex = 2;

void InitializeGame() {
    pins.clear();
    binCenters.clear();

    for (int row = 0; row < NUM_ROWS; row++) {
        int pins_in_row = 3 + row;
        float y = START_Y + row * PIN_SPACING_Y;
        float start_x = WINDOW_WIDTH / 2.0f - (pins_in_row - 1) * PIN_SPACING_X / 2.0f;

        for (int pin = 0; pin < pins_in_row; pin++) {
            float x = start_x + pin * PIN_SPACING_X;
            pins.push_back(Pin(x, y));
        }
    }

    const int NUM_BINS = PRIZE_MULTIPLIERS.size();
    const float BIN_WIDTH = PIN_SPACING_X;
    const float total_bins_width = NUM_BINS * BIN_WIDTH;
    const float bins_block_start_x = WINDOW_WIDTH / 2.0f - total_bins_width / 2.0f;

    for (int i = 0; i < NUM_BINS; i++) {
        float bin_center_x = bins_block_start_x + (i * BIN_WIDTH) + (BIN_WIDTH / 2.0f);
        binCenters.push_back({bin_center_x, 0});
    }
}

int PrecalculateOutcome() {
    std::vector<float> weights;
    int numBins = PRIZE_MULTIPLIERS.size();
    float center = (numBins - 1) / 2.0f;

    for (int i = 0; i < numBins; i++) {
        float distance = abs(i - center);
        float weight = exp(-distance * 0.6f);
        weights.push_back(weight);
    }

    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    return distribution(rng);
}

void DropBall() {
    if (balance < currentBet) return;

    balance -= currentBet;

    Ball newBall;
    newBall.active = true;
    newBall.betAmount = currentBet;
    newBall.finalBin = PrecalculateOutcome();
    newBall.winAmount = (int)(currentBet * PRIZE_MULTIPLIERS[newBall.finalBin]);
    newBall.position = {WINDOW_WIDTH / 2.0f + ((float)rand() / RAND_MAX - 0.5f) * 5.0f, 160};
    newBall.velocity = {0, 0};

    balls.push_back(newBall);
}

void UpdateBalls() {
    for (auto& ball : balls) {
        if (!ball.active) continue;

        ball.velocity.y += GRAVITY;

        if (ball.finalBin >= 0 && ball.finalBin < binCenters.size()) {
            float targetX = binCenters[ball.finalBin].x;
            float distanceToTarget = targetX - ball.position.x;

            const float dropTopY = 160.0f;
            const float dropBottomY = START_Y + (NUM_ROWS - 1) * PIN_SPACING_Y;
            float totalDropHeight = dropBottomY - dropTopY;

            float progress = fmax(0.0f, fmin(1.0f, (ball.position.y - dropTopY) / totalDropHeight));

            float guidanceStrength = 0.025f * pow(progress, 3.0f);
            float guidanceForce = distanceToTarget * guidanceStrength;

            ball.velocity.x += guidanceForce;
        }
        ball.velocity.x *= FRICTION;

        ball.position.x += ball.velocity.x;
        ball.position.y += ball.velocity.y;

        for (const Pin& pin : pins) {
            float dx = ball.position.x - pin.position.x;
            float dy = ball.position.y - pin.position.y;
            float distance = sqrt(dx * dx + dy * dy);

            if (distance < BALL_RADIUS + PIN_RADIUS) {
                float angle = atan2(dy, dx);
                float overlap = BALL_RADIUS + PIN_RADIUS - distance;
                ball.position.x += cos(angle) * overlap;
                ball.position.y += sin(angle) * overlap;

                float dot = ball.velocity.x * cos(angle) + ball.velocity.y * sin(angle);
                ball.velocity.x -= 2 * dot * cos(angle);
                ball.velocity.y -= 2 * dot * sin(angle);
                ball.velocity.x *= BOUNCE_DAMPING;
                ball.velocity.y *= BOUNCE_DAMPING;
            }
        }

        if (ball.position.y > START_Y + NUM_ROWS * PIN_SPACING_Y + 50) {
            ball.active = false;
            balance += ball.winAmount;
            
            lastWin.amountWon = ball.winAmount;
            lastWin.betAmount = ball.betAmount;
            lastWin.finalBin = ball.finalBin;
            lastWin.displayTimer = WIN_DISPLAY_DURATION;
        }

        if (ball.position.x < BALL_RADIUS) {
            ball.position.x = BALL_RADIUS;
            ball.velocity.x = -ball.velocity.x * BOUNCE_DAMPING;
        }
        if (ball.position.x > WINDOW_WIDTH - BALL_RADIUS) {
            ball.position.x = WINDOW_WIDTH - BALL_RADIUS;
            ball.velocity.x = -ball.velocity.x * BOUNCE_DAMPING;
        }
    }
    
    balls.erase(std::remove_if(balls.begin(), balls.end(), [](const Ball& b) {
        return !b.active;
    }), balls.end());
}

void UpdateTimers() {
    if (lastWin.displayTimer > 0) {
        lastWin.displayTimer -= GetFrameTime();
    } else {
        lastWin.finalBin = -1;
    }
}

void HandleInput() {
    if ((IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) && balance >= currentBet) {
        DropBall();
    }

    if (IsKeyPressed(KEY_UP) && betIndex < BET_AMOUNTS.size() - 1) {
        betIndex++;
        currentBet = BET_AMOUNTS[betIndex];
    }
    if (IsKeyPressed(KEY_DOWN) && betIndex > 0) {
        betIndex--;
        currentBet = BET_AMOUNTS[betIndex];
    }
}

void DrawPlinkoBoard() {
    for (const Pin& pin : pins) {
        DrawCircle(pin.position.x, pin.position.y, PIN_RADIUS, COLOR_PIN);
        DrawCircle(pin.position.x - 1, pin.position.y - 1, PIN_RADIUS - 2.5f, COLOR_PIN_HIGHLIGHT);
    }
}

void DrawPrizeBins() {
    const int NUM_BINS = PRIZE_MULTIPLIERS.size();
    const float BIN_WIDTH = PIN_SPACING_X;
    const float BIN_HEIGHT = 40.0f;
    const float BIN_DIVIDER_HEIGHT = BIN_HEIGHT + 20.0f;
    const float total_bins_width = NUM_BINS * BIN_WIDTH;
    const float bins_block_start_x = WINDOW_WIDTH / 2.0f - total_bins_width / 2.0f;
    const float last_row_y = START_Y + (NUM_ROWS - 1) * PIN_SPACING_Y;
    const float bin_y = last_row_y + PIN_SPACING_Y;

    if (lastWin.finalBin >= 0) {
        float bin_start_x = bins_block_start_x + (lastWin.finalBin * BIN_WIDTH);
        Rectangle highlight = {bin_start_x, bin_y, BIN_WIDTH, BIN_HEIGHT + 20};

        float alpha = (lastWin.displayTimer / WIN_DISPLAY_DURATION) * 150;
        Color highlightColor = (lastWin.amountWon > lastWin.betAmount) ? COLOR_WIN : COLOR_LOSE;
        highlightColor.a = (unsigned char)fmax(0.0f, alpha);
        DrawRectangleRec(highlight, highlightColor);
    }

    for (int i = 0; i < NUM_BINS; ++i) {
        float bin_center_x = bins_block_start_x + (i * BIN_WIDTH) + (BIN_WIDTH / 2.0f);

        std::string text;
        float prize = PRIZE_MULTIPLIERS[i];
        if (prize >= 10.0f) {
            text = "x" + std::to_string((int)prize);
        } else {
            char buffer[16];
            snprintf(buffer, sizeof(buffer), "x%.1f", prize);
            text = buffer;
        }

        int text_width = MeasureText(text.c_str(), 20);
        DrawText(text.c_str(), bin_center_x - text_width / 2, bin_y + BIN_HEIGHT / 2 - 10, 20, COLOR_TEXT);
    }

    for (int i = 0; i < NUM_BINS + 1; i++) {
        float divider_x = bins_block_start_x + (i * BIN_WIDTH);
        DrawLine(divider_x, bin_y, divider_x, bin_y + BIN_DIVIDER_HEIGHT, COLOR_UI_ACCENT);
    }
}

void DrawUI() {
    const char* title = "P L I N K O";
    int title_width = MeasureText(title, FONT_SIZE);
    DrawText(title, WINDOW_WIDTH / 2 - title_width / 2, 20, FONT_SIZE, COLOR_TEXT);

    int box_width = 150;
    int box_height = 50;
    int box_x = WINDOW_WIDTH / 2 - 200;
    int box_y = 70;

    Rectangle balance_rect = {(float)box_x, (float)box_y, (float)box_width, (float)box_height};
    DrawRectangleRounded(balance_rect, 0.2f, 10, COLOR_UI_PRIMARY);
    DrawRectangleRoundedLines(balance_rect, 0.2f, 10, COLOR_UI_ACCENT);

    std::string balance_text = "$" + std::to_string(balance);
    int balance_text_width = MeasureText(balance_text.c_str(), 20);
    DrawText(balance_text.c_str(), box_x + (box_width / 2) - (balance_text_width / 2),
             box_y + (box_height / 2) - 10, 20, COLOR_TEXT);

    box_x = WINDOW_WIDTH / 2 + 50;
    Rectangle bet_rect = {(float)box_x, (float)box_y, (float)box_width, (float)box_height};
    DrawRectangleRounded(bet_rect, 0.2f, 10, COLOR_UI_PRIMARY);
    
    Color bet_line_color = (balance >= currentBet) ? COLOR_UI_ACCENT : COLOR_LOSE;
    DrawRectangleRoundedLines(bet_rect, 0.2f, 10, bet_line_color);

    std::string bet_text = "Bet: $" + std::to_string(currentBet);
    int bet_text_width = MeasureText(bet_text.c_str(), 20);
    DrawText(bet_text.c_str(), box_x + (box_width / 2) - (bet_text_width / 2),
             box_y + (box_height / 2) - 10, 20, COLOR_TEXT);

    const char* instruction = "SPACE/CLICK to drop ball • UP/DOWN to change bet";
    int inst_width = MeasureText(instruction, 16);
    DrawText(instruction, WINDOW_WIDTH / 2 - inst_width / 2, 130, 16, COLOR_TEXT);

    if (lastWin.displayTimer > 0) {
        std::string result_text;
        Color result_color;
        
        if (lastWin.amountWon > lastWin.betAmount) {
            result_text = "WIN! +$" + std::to_string(lastWin.amountWon);
            result_color = COLOR_WIN;
        } else if (lastWin.amountWon == lastWin.betAmount) {
            result_text = "PUSH";
            result_color = COLOR_TEXT;
        } else {
            result_text = "WIN +$" + std::to_string(lastWin.amountWon);
            result_color = COLOR_LOSE;
        }
        
        result_color.a = (unsigned char)fmax(0.0f, (lastWin.displayTimer / WIN_DISPLAY_DURATION) * 255);
        
        int result_width = MeasureText(result_text.c_str(), 24);
        float y_offset = (WIN_DISPLAY_DURATION - lastWin.displayTimer) * 20.0f;
        DrawText(result_text.c_str(), WINDOW_WIDTH / 2 - result_width / 2, 160 - y_offset, 24, result_color);
    }
}

void DrawBalls() {
    DrawCircleGradient(WINDOW_WIDTH / 2, 160, BALL_RADIUS, COLOR_BALL, COLOR_BALL_SHADOW);
    
    for (const auto& ball : balls) {
        if (ball.active) {
            DrawCircleGradient(ball.position.x, ball.position.y, BALL_RADIUS, COLOR_BALL, COLOR_BALL_SHADOW);
        }
    }
}

int main(void) {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Plinko Game Simulation");
    SetTargetFPS(60);

    InitializeGame();

    while (!WindowShouldClose()) {
        HandleInput();
        UpdateTimers();
        UpdateBalls();

        BeginDrawing();
            ClearBackground(COLOR_BG);

            DrawUI();
            DrawPlinkoBoard();
            DrawPrizeBins();
            DrawBalls();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}