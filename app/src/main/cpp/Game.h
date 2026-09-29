#ifndef PIXEL_TURBO_RACER_GAME_H
#define PIXEL_TURBO_RACER_GAME_H

#include <vector>
#include <cstdlib>
#include <algorithm>

enum class GameState {
    START,
    PLAYING,
    GAMEOVER
};

struct EnemyCar {
    float x;
    float y;
    float width;
    float height;
    float speed;
    int lane;
    float r, g, b;
};

class Game {
public:
    Game() : state_(GameState::START), score_(0), highScore_(0),
             playerX_(0.0f), playerY_(-1.2f),
             spawnTimer_(0.0f), gameSpeed_(3.0f), roadScroll_(0.0f) {}

    void reset() {
        state_ = GameState::PLAYING;
        score_ = 0;
        playerX_ = 0.0f;
        playerY_ = -1.2f;
        enemies_.clear();
        spawnTimer_ = 0.0f;
        gameSpeed_ = 3.0f;
        roadScroll_ = 0.0f;
    }

    void update(float deltaTime) {
        if (state_ != GameState::PLAYING) return;

        // Road scroll simulates moving forward against oncoming traffic
        roadScroll_ += gameSpeed_ * deltaTime;
        if (roadScroll_ > 2.0f) roadScroll_ -= 2.0f;

        // Increase base speed gradually
        gameSpeed_ += 0.04f * deltaTime;

        // Spawn oncoming traffic cars at the top
        spawnTimer_ += deltaTime;
        if (spawnTimer_ > 0.85f) {
            spawnTimer_ = 0.0f;
            int lane = rand() % 3;
            float ex = (lane - 1) * 0.5f;
            float ey = 2.3f;

            float colors[][3] = {
                    {0.2f, 0.8f, 0.2f}, // Green
                    {0.9f, 0.2f, 0.2f}, // Red
                    {0.2f, 0.5f, 0.9f}, // Blue
                    {0.9f, 0.8f, 0.2f}  // Yellow
            };
            int cIndex = rand() % 4;

            enemies_.push_back({
                    ex, ey, 0.28f, 0.48f,
                    gameSpeed_ * 0.9f + static_cast<float>(rand() % 5) * 0.1f,
                    lane,
                    colors[cIndex][0], colors[cIndex][1], colors[cIndex][2]
            });
        }

        // Update oncoming traffic positions (moving downwards towards player)
        float pWidth = 0.26f;
        float pHeight = 0.46f;

        for (auto it = enemies_.begin(); it != enemies_.end();) {
            // Oncoming traffic moves down towards player
            it->y -= (gameSpeed_ + it->speed) * deltaTime;

            // If passed bottom of screen
            if (it->y < -2.3f) {
                it = enemies_.erase(it);
                score_ += 10;
                if (score_ > highScore_) {
                    highScore_ = score_;
                }
            } else {
                // Collision check (AABB)
                if (std::abs(playerX_ - it->x) < (pWidth + it->width) * 0.45f &&
                    std::abs(playerY_ - it->y) < (pHeight + it->height) * 0.45f) {
                    state_ = GameState::GAMEOVER;
                }
                ++it;
            }
        }
    }

    void handleInput(float steerInput, float moveInput, bool actionTriggered) {
        if (state_ == GameState::START || state_ == GameState::GAMEOVER) {
            if (actionTriggered) {
                reset();
            }
        } else if (state_ == GameState::PLAYING) {
            playerX_ += steerInput * 5.0f * 0.016f;
            playerX_ = std::clamp(playerX_, -0.75f, 0.75f);

            playerY_ += moveInput * 4.0f * 0.016f;
            playerY_ = std::clamp(playerY_, -1.5f, -0.4f);
        }
    }

    void setPosition(float x, float y) {
        if (state_ == GameState::PLAYING) {
            playerX_ = std::clamp(x, -0.75f, 0.75f);
            playerY_ = std::clamp(y, -1.5f, -0.4f);
        }
    }

    GameState getState() const { return state_; }
    int getScore() const { return score_; }
    int getHighScore() const { return highScore_; }
    float getPlayerX() const { return playerX_; }
    float getPlayerY() const { return playerY_; }
    float getPlayerWidth() const { return 0.28f; }
    float getPlayerHeight() const { return 0.48f; }
    const std::vector<EnemyCar>& getEnemies() const { return enemies_; }
    float getRoadScroll() const { return roadScroll_; }

private:
    GameState state_;
    int score_;
    int highScore_;
    float playerX_;
    float playerY_;
    std::vector<EnemyCar> enemies_;
    float spawnTimer_;
    float gameSpeed_;
    float roadScroll_;
};

#endif //PIXEL_TURBO_RACER_GAME_H
