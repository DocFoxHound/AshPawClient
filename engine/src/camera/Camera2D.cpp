#include "ashpaw/engine/camera/Camera2D.hpp"

#include <algorithm>

namespace ashpaw::engine::camera {

void Camera2D::SetViewport(float width, float height) {
    viewport_ = {width, height};
}

void Camera2D::SetWorldBounds(float width, float height) {
    worldBounds_ = {width, height};
}

void Camera2D::Follow(const math::Vector2& target) {
    const auto halfWidth = viewport_.x * 0.5F;
    const auto halfHeight = viewport_.y * 0.5F;
    position_.x = std::clamp(target.x - halfWidth, 0.0F, std::max(worldBounds_.x - viewport_.x, 0.0F));
    position_.y = std::clamp(target.y - halfHeight, 0.0F, std::max(worldBounds_.y - viewport_.y, 0.0F));
}

math::Vector2 Camera2D::WorldToScreen(const math::Vector2& worldPoint) const {
    return {worldPoint.x - position_.x, worldPoint.y - position_.y};
}

math::Vector2 Camera2D::Position() const {
    return position_;
}

}  // namespace ashpaw::engine::camera
