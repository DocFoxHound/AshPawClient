# Client Configuration

The client reads configuration from `assets/config/client.json`.

## Fields

### Display And Window

- `window_width`: startup window width
- `window_height`: startup window height
- `fullscreen`: startup fullscreen flag
- `vsync`: startup VSync flag

These are local presentation preferences.

### Audio And Assets

- `master_volume_percent`: stored local volume preference
- `asset_root`: base asset directory
- `map_path`: local map visual file to load

### Networking

- `server_host`: target hostname or IP
- `server_port`: target UDP port
- `player_name`: requested display name preference
- `auto_connect`: connect automatically after startup
- `connect_timeout_ms`: ENet connect timeout
- `handshake_timeout_ms`: handshake timeout covering `server_hello` and `join_accepted`

### Logging And UI

- `log_level`: logger verbosity
- `show_debug_overlay`: startup debug overlay state
- `show_onboarding_hints`: startup help-panel visibility state

### Cached Authoritative Data

- `cached_authoritative_name`: last server-confirmed display name
- `cached_last_map`: last known map label shown in the UI

These are display conveniences only. They are not authority.

## Persistence Behavior

When settings are saved, the client:

- clamps minimum window size
- clamps volume to `0..100`
- sanitizes the requested player name using server-compatible rules
- writes the full config back to disk

The requested player name is stored locally, but the server still decides the effective authoritative name.
