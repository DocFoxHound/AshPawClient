# Testing

This document describes how the client is currently verified.

## Automated Tests

Run the test suite with:

```bash
ctest --test-dir build --output-on-failure
```

Current coverage includes:

- config load/save behavior
- binary protocol codec behavior
- input mapping behavior
- full ENet handshake and gameplay flow against an in-process protocol server
- reconnect persistence behavior
- camera transforms
- snapshot interpolation
- client world store behavior
- map loading

## In-Process Protocol Server Tests

The networking tests no longer target the removed temporary handshake server.

Instead, `tests/EngineTests.cpp` spins up a small in-process ENet server that speaks the documented binary contract and verifies:

- `client_hello -> server_hello -> join_accepted`
- handshake rejection
- movement snapshots
- interaction results
- identity updates
- object state updates
- chat broadcasts
- reconnect restore

## Manual Verification

Useful manual checks:

- connect to a real server matching `server-side docs/`
- verify the session panel moves through the handshake states cleanly
- confirm movement still works if snapshots are sparse
- verify names update from server identity, not from the requested name field
- verify interaction feedback only appears after server response
- verify chat only appears after server broadcast

## Known Testing Boundary

The client test suite currently verifies the implemented protocol and runtime behavior in this repo.

It does not replace end-to-end testing against the real server repository when:

- server docs change
- map/interactable content changes
- deployment/network conditions differ from local ENet tests
