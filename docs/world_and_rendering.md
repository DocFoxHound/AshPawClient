# World And Rendering

This document describes the client-side world model and how authoritative state becomes visible presentation.

## Authoritative Stores

The client currently maintains three replicated stores:

- `entities[entity_id]`
- `identities[entity_id]`
- `interactables[target_id]`

These are held in `ClientWorld`.

## Entity Rules

- entities are keyed only by server `entity_id`
- `join_accepted` establishes the locally controlled entity id
- `player_spawn` creates or refreshes entity presence
- `transform_snapshot` updates only listed entities
- `player_despawn` removes the entity immediately
- identities are stored separately and can update after an entity already exists

## Identity Rules

The client treats `identity_update` and `chat_broadcast.display_name` as authoritative for display names.

Effects:

- world labels update from the identity store
- the options panel’s authoritative name uses server-owned identity
- chat speaker labels use the authoritative display name

## Interactable Rules

The client stores replicated interactable state from `object_state_update`:

- `target_id`
- `is_open`
- `occupant_entity_id`

Current usage:

- prompt targeting is limited to local markers that correspond to authoritative interactables
- interaction success or failure is still decided only by `interaction_result`
- the current wire contract does not provide interactable positions, so the client uses local map markers as visual anchors

## Interpolation

Remote entities use snapshot buffering for smoother presentation:

- incoming authoritative positions are pushed into `SnapshotBuffer`
- rendering samples the buffer with a short delay
- only remote entities are interpolated
- the local controlled entity uses authoritative position updates directly

Absence from a snapshot does not imply despawn.

## Rendering Order

The renderer currently draws:

1. map background layers
2. map midground layers
3. local map markers
4. replicated entities
5. map foreground layers
6. debug overlays and labels
7. UI panels

## Current Limitation

Because map visuals are still local and interactable positions are not on the wire:

- visual marker placement can drift from future server map data if the two repos diverge
- interactable state is authoritative, but interactable placement is still local

That is the main intentional compromise in the current hybrid setup.
