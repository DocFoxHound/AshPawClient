#pragma once

#include <unordered_map>

#include <SDL.h>

namespace ashpaw::engine::input {

enum class InputAction {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Interact,
    OpenChat,
    DismissUi,
    ToggleDebug,
    Reconnect,
    Quit
};

struct InputSnapshot {
    bool moveUp {false};
    bool moveDown {false};
    bool moveLeft {false};
    bool moveRight {false};
    bool interactPressed {false};
    bool openChatPressed {false};
    bool dismissUiPressed {false};
    bool toggleDebugPressed {false};
    bool reconnectPressed {false};
    bool quitRequested {false};
};

class InputSystem {
public:
    InputSystem();

    void BeginFrame();
    void HandleEvent(const SDL_Event& event);
    [[nodiscard]] InputSnapshot Snapshot(bool uiCapturesKeyboard) const;

private:
    std::unordered_map<SDL_Keycode, InputAction> bindings_;
    std::unordered_map<InputAction, bool> heldActions_;
    bool interactPressed_ {false};
    bool openChatPressed_ {false};
    bool dismissUiPressed_ {false};
    bool toggleDebugPressed_ {false};
    bool reconnectPressed_ {false};
    bool quitRequested_ {false};
};

}  // namespace ashpaw::engine::input
