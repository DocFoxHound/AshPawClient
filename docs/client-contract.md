# Client Contract

This document describes what the AshPaw client currently does, what it expects from the server, and where authority lives.

## Repository Role

This repo owns:

- window and platform bootstrap
- renderer and camera
- local asset loading for visual presentation
- player input collection
- network transport and protocol encode/decode
- client-side world presentation state
- interpolation of remote movement
- UI for connection state, options, chat, interaction prompts, and debug overlays

This repo does not own:

- authoritative movement outcomes
- interaction approval or rejection
- authoritative object state
- authoritative display names
- authoritative persistence results
- final chat delivery

Core rule:

- client presents
- server decides

## Current Authority Boundary

The client treats these as server-owned:

- session acceptance and rejection
- authoritative player entity id
- authoritative transforms
- identity/display names
- interaction outcomes
- replicated object state for doors, seats, and containers
- chat delivery

The client treats these as local presentation concerns:

- map visuals loaded from `assets/maps/test_map.json`
- camera follow and screen-space transforms
- interpolation of remote entity movement
- local UI state and cached preferences
- interaction prompt placement using local visual anchors

## Current Hybrid Visual Assumption

The current client intentionally keeps a hybrid model:

- local maps are still used for world visuals and marker positions
- server packets are the source of truth for interactable state and interaction success
- the client uses local markers as positional anchors for prompts because the current wire contract does not transmit interactable positions

This means the map art can still be local, while gameplay authority remains server-driven.

## Required Server Expectations

The client expects a server that matches the docs in `server-side docs/`:

- protocol version `1`
- ENet with two channels
- reliable/event traffic on channel `0`
- movement input on channel `1`
- the handshake `client_hello -> server_hello -> join_accepted|join_rejected`
- authoritative `identity_update`, `object_state_update`, `chat_broadcast`, `player_spawn`, `player_despawn`, and `transform_snapshot`

If the server contract changes, the client should be updated to match the server docs instead of guessing around them.
