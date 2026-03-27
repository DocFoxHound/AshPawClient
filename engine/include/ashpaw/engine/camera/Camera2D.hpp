#pragma once

#include "ashpaw/engine/math/Vector2.hpp"

namespace ashpaw::engine::camera {

class Camera2D {
public:
    void SetViewport(float width, float height);
    void SetWorldBounds(float width, float height);
    void Follow(const math::Vector2& target);
    [[nodiscard]] math::Vector2 WorldToScreen(const math::Vector2& worldPoint) const;
    [[nodiscard]] math::Vector2 Position() const;

private:
    math::Vector2 position_ {};
    math::Vector2 viewport_ {1280.0F, 720.0F};
    math::Vector2 worldBounds_ {1280.0F, 720.0F};
};

}  // namespace ashpaw::engine::camera
