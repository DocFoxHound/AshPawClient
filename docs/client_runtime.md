# Client Runtime

This document explains the current client lifecycle from startup through gameplay.

## Startup

At launch, the client:

1. loads `assets/config/client.json`
2. initializes logging
3. creates the SDL window and OpenGL renderer
4. initializes ImGui UI integration
5. initializes ENet through `NetworkClient`
6. loads the local visual map from `config.mapPath`
7. creates a placeholder local entity while disconnected
8. auto-connects if `auto_connect` is enabled

## Connection State Machine

The runtime uses these connection states:

- `Disconnected`
- `Connecting`
- `WaitingForServerHello`
- `WaitingForJoinAccepted`
- `Active`
- `Disconnecting`

State behavior:

- on ENet connect, the client sends `client_hello`
- on `server_hello`, it records `tick_rate`
- on `join_accepted`, it records `session_id`, `entity_id`, and spawn position
- on `join_rejected`, it surfaces the server message and returns to `Disconnected`
- on disconnect during handshake, it treats that as a handshake failure

## Gameplay Loop

Every frame, the client:

1. processes SDL input and UI events
2. ticks the network client
3. consumes authoritative server messages
4. applies interpolation for remote entities
5. updates the current interaction target
6. sends movement input while active
7. renders map layers, markers, entities, labels, overlays, and UI panels

## Session Activation

When a session becomes active:

- the client clears prior replicated entities
- creates the locally controlled entity from `join_accepted`
- waits for follow-up `player_spawn`, `identity_update`, and `object_state_update`
- resets chat and interaction UI state
- keeps the local map visuals already loaded

## UI Surfaces

Current player-facing UI includes:

- session status panel
- options and identity panel
- developer panel
- onboarding/help panel
- interaction prompt
- interaction result banner
- chat panel and chat focus hint
- debug overlay

Important behavior:

- the requested name is only a preference
- the authoritative name comes from server packets
- chat is only displayed from `chat_broadcast`
- interaction success is only shown after `interaction_result`
