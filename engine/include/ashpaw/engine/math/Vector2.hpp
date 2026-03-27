#pragma once

namespace ashpaw::engine::math {

struct Vector2 {
    float x {0.0F};
    float y {0.0F};
};

struct Color {
    float r {1.0F};
    float g {1.0F};
    float b {1.0F};
    float a {1.0F};
};

struct Rect {
    float x {0.0F};
    float y {0.0F};
    float w {0.0F};
    float h {0.0F};
};

inline Vector2 operator+(const Vector2& lhs, const Vector2& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

inline Vector2 operator-(const Vector2& lhs, const Vector2& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

inline Vector2 operator*(const Vector2& value, float scalar) {
    return {value.x * scalar, value.y * scalar};
}

inline bool Intersects(const Rect& lhs, const Rect& rhs) {
    return lhs.x < rhs.x + rhs.w && lhs.x + lhs.w > rhs.x && lhs.y < rhs.y + rhs.h && lhs.y + lhs.h > rhs.y;
}

}  // namespace ashpaw::engine::math
