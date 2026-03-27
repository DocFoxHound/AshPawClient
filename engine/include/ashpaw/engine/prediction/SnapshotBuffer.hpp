#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "ashpaw/engine/math/Vector2.hpp"

namespace ashpaw::engine::prediction {

struct PositionSnapshot {
    double timestampSeconds {0.0};
    math::Vector2 position {};
};

class SnapshotBuffer {
public:
    void Push(const PositionSnapshot& snapshot) {
        snapshots_.push_back(snapshot);
        std::sort(snapshots_.begin(), snapshots_.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.timestampSeconds < rhs.timestampSeconds;
        });
        if (snapshots_.size() > 32) {
            snapshots_.erase(snapshots_.begin(), snapshots_.begin() + static_cast<std::ptrdiff_t>(snapshots_.size() - 32));
        }
    }

    [[nodiscard]] std::optional<math::Vector2> Sample(double timestampSeconds) const {
        if (snapshots_.empty()) {
            return std::nullopt;
        }
        if (timestampSeconds <= snapshots_.front().timestampSeconds) {
            return snapshots_.front().position;
        }
        if (timestampSeconds >= snapshots_.back().timestampSeconds) {
            return snapshots_.back().position;
        }

        for (std::size_t index = 1; index < snapshots_.size(); ++index) {
            const auto& previous = snapshots_[index - 1];
            const auto& current = snapshots_[index];
            if (timestampSeconds <= current.timestampSeconds) {
                const auto span = current.timestampSeconds - previous.timestampSeconds;
                const auto alpha = static_cast<float>((timestampSeconds - previous.timestampSeconds) / span);
                return math::Vector2 {
                    previous.position.x + ((current.position.x - previous.position.x) * alpha),
                    previous.position.y + ((current.position.y - previous.position.y) * alpha)
                };
            }
        }
        return snapshots_.back().position;
    }

private:
    std::vector<PositionSnapshot> snapshots_;
};

}  // namespace ashpaw::engine::prediction
