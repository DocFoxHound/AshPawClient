# Network Protocol

This document summarizes the binary server contract as the client currently implements it.

The authoritative source remains:

- [server-side docs/protocol.md](/home/martinb/Documents/AshPawClient/server-side%20docs/protocol.md)
- [server-side docs/client_server_contract.md](/home/martinb/Documents/AshPawClient/server-side%20docs/client_server_contract.md)

## Wire Basics

- protocol version: `1`
- packet size limit mirrored by client: `512` bytes
- strings are encoded as one-byte length-prefixed UTF-8
- multi-byte integers are little-endian

## ENet Channels

- channel `0`: reliable traffic and gameplay events
- channel `1`: movement input

Client send routing:

- `client_hello` on channel `0`
- `interaction_request` on channel `0`
- `chat_send` on channel `0`
- `movement_input` on channel `1`

## Implemented Packet Types

Client outbound:

- `client_hello`
- `movement_input`
- `interaction_request`
- `chat_send`

Client inbound:

- `server_hello`
- `join_accepted`
- `join_rejected`
- `player_spawn`
- `player_despawn`
- `transform_snapshot`
- `interaction_result`
- `object_state_update`
- `chat_broadcast`
- `identity_update`

## Validation Mirrored Client-Side

The client mirrors these server-facing rules before send:

- display names are sanitized to ASCII letters, digits, spaces, `-`, and `_`
- display names are trimmed and truncated to `24`
- empty chat messages are not sent
- chat messages longer than `120` are not sent
- strings that would overflow one-byte length encoding are rejected
- packets larger than `512` bytes are suppressed

## Handshake Flow

Expected handshake:

1. ENet peer connects
2. client sends `client_hello(protocol_version=1, display_name)`
3. server sends `server_hello(protocol_version=1, tick_rate)`
4. server sends `join_accepted` or `join_rejected`

Client behavior:

- stores `server_tick_rate`
- stores `session_id`
- stores authoritative controlled `entity_id`
- treats immediate `join_rejected` before `server_hello` as valid rejection

## Message Semantics In Client Code

- `player_spawn` is an upsert
- `player_despawn` is the only entity removal signal
- `transform_snapshot` is delta-only, never full-world replacement
- `identity_update` is authoritative for displayed names
- `object_state_update` is authoritative for interactable state
- `occupant_entity_id == 0` means empty occupant
- `chat_broadcast` is the only authoritative chat message event

## Packet Logging

The developer panel packet log shows:

- direction
- ENet channel
- packet type name
- packet byte size

It is intended for debugging routing and handshake flow, not for protocol-authoritative inspection.
