#include "ashpaw/engine/input/InputSystem.hpp"

namespace ashpaw::engine::input {

InputSystem::InputSystem() {
    bindings_[SDLK_w] = InputAction::MoveUp;
    bindings_[SDLK_s] = InputAction::MoveDown;
    bindings_[SDLK_a] = InputAction::MoveLeft;
    bindings_[SDLK_d] = InputAction::MoveRight;
    bindings_[SDLK_e] = InputAction::Interact;
    bindings_[SDLK_RETURN] = InputAction::OpenChat;
    bindings_[SDLK_ESCAPE] = InputAction::DismissUi;
    bindings_[SDLK_F1] = InputAction::ToggleDebug;
    bindings_[SDLK_F5] = InputAction::Reconnect;
}

void InputSystem::BeginFrame() {
    interactPressed_ = false;
    openChatPressed_ = false;
    dismissUiPressed_ = false;
    toggleDebugPressed_ = false;
    reconnectPressed_ = false;
    quitRequested_ = false;
}

void InputSystem::HandleEvent(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        quitRequested_ = true;
        return;
    }

    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) {
        return;
    }

    const auto iterator = bindings_.find(event.key.keysym.sym);
    if (iterator == bindings_.end()) {
        return;
    }

    const auto action = iterator->second;
    const auto pressed = event.type == SDL_KEYDOWN;
    heldActions_[action] = pressed;

    if (pressed && action == InputAction::ToggleDebug && event.key.repeat == 0) {
        toggleDebugPressed_ = true;
    }
    if (pressed && action == InputAction::Interact && event.key.repeat == 0) {
        interactPressed_ = true;
    }
    if (pressed && action == InputAction::OpenChat && event.key.repeat == 0) {
        openChatPressed_ = true;
    }
    if (pressed && action == InputAction::DismissUi && event.key.repeat == 0) {
        dismissUiPressed_ = true;
    }
    if (pressed && action == InputAction::Reconnect && event.key.repeat == 0) {
        reconnectPressed_ = true;
    }
    if (pressed && action == InputAction::Quit) {
        quitRequested_ = true;
    }
}

InputSnapshot InputSystem::Snapshot(bool uiCapturesKeyboard) const {
    InputSnapshot snapshot;
    snapshot.openChatPressed = openChatPressed_;
    snapshot.dismissUiPressed = dismissUiPressed_;
    snapshot.toggleDebugPressed = toggleDebugPressed_;
    snapshot.reconnectPressed = reconnectPressed_;
    snapshot.quitRequested = quitRequested_ || (!uiCapturesKeyboard && dismissUiPressed_);
    if (uiCapturesKeyboard) {
        return snapshot;
    }

    snapshot.interactPressed = interactPressed_;
    snapshot.moveUp = heldActions_.contains(InputAction::MoveUp) && heldActions_.at(InputAction::MoveUp);
    snapshot.moveDown = heldActions_.contains(InputAction::MoveDown) && heldActions_.at(InputAction::MoveDown);
    snapshot.moveLeft = heldActions_.contains(InputAction::MoveLeft) && heldActions_.at(InputAction::MoveLeft);
    snapshot.moveRight = heldActions_.contains(InputAction::MoveRight) && heldActions_.at(InputAction::MoveRight);
    return snapshot;
}

}  // namespace ashpaw::engine::input
