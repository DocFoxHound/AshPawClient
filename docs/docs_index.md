# Client Docs Index

## Overview

- [client-contract.md](/home/martinb/Documents/AshPawClient/docs/client-contract.md): what the client owns, what it expects from the server, and current repo-level assumptions
- [client_runtime.md](/home/martinb/Documents/AshPawClient/docs/client_runtime.md): startup, connection flow, gameplay loop, UI flow, and authoritative state updates

## Runtime Contracts

- [network_protocol.md](/home/martinb/Documents/AshPawClient/docs/network_protocol.md): binary packet contract as implemented by the client, channel routing, validation, and state-machine behavior
- [world_and_rendering.md](/home/martinb/Documents/AshPawClient/docs/world_and_rendering.md): client-side world stores, interpolation, identity handling, interactables, and rendering assumptions
- [configuration.md](/home/martinb/Documents/AshPawClient/docs/configuration.md): client config fields, meanings, and persistence behavior

## Verification

- [testing.md](/home/martinb/Documents/AshPawClient/docs/testing.md): current automated coverage, in-process protocol server tests, and manual verification notes

## Related Server Docs

- [server-side docs/client_server_contract.md](/home/martinb/Documents/AshPawClient/server-side%20docs/client_server_contract.md): authoritative server-to-client behavior contract
- [server-side docs/client_implementation_playbook.md](/home/martinb/Documents/AshPawClient/server-side%20docs/client_implementation_playbook.md): implementation guidance that this client now follows
- [server-side docs/protocol.md](/home/martinb/Documents/AshPawClient/server-side%20docs/protocol.md): authoritative packet format and session flow
