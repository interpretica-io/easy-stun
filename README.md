# easy-stun

[![Build & Release](https://github.com/interpretica-io/easy-stun/actions/workflows/release.yml/badge.svg)](https://github.com/interpretica-io/easy-stun/actions/workflows/release.yml)
[![Latest Release](https://img.shields.io/github/v/release/interpretica-io/easy-stun?sort=semver)](https://github.com/interpretica-io/easy-stun/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

`easy-stun` is a small, fast STUN client focused on **discovering your public (NAT-mapped) address/port** by sending a STUN Binding Request and parsing the response. It’s designed to run continuously and keep the mapping alive, while executing optional user scripts on key events.

## What this project is for

- **NAT mapping discovery** (public IP/port as seen by a STUN server)
- **Keepalive** to reduce NAT mapping expiration
- **Event hooks** via external scripts:
  - `bind`: when a mapping is discovered/updated
  - `cr`: when something that looks like a TR-111 connection request is received (currently a placeholder)

This is not a full STUN server implementation. It’s a client intended to be used as a building block in networking setups and automation.

## How it works (high level)

1. Creates a UDP socket (non-blocking).
2. Sends a STUN Binding Request to a configured STUN server.
3. Receives packets in an event-driven loop and processes STUN responses:
   - extracts `MAPPED-ADDRESS` or `XOR-MAPPED-ADDRESS`
4. Periodically sends keepalive pings to keep NAT mappings alive.
5. Spawns an external script on certain events (fire-and-forget; does not block packet processing).

## Requirements

- C toolchain (clang or gcc)
- CMake (>= 3.5)
- Ninja (recommended) or Make
- OpenSSL development headers/libs

### macOS (Homebrew)

- `cmake`, `ninja`, `pkg-config`, `openssl@3`

### Linux (Debian/Ubuntu)

- `build-essential`, `cmake`, `ninja-build`, `pkg-config`, `libssl-dev`

## Build

From the repository root:

### Using CMake + Ninja (recommended)

```
cmake -S easy-stun -B easy-stun/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build easy-stun/build
```

### Using the repo build script

There is a helper script used by the project:

```
cd easy-stun
./scripts/make.sh
```

## Run

`easy-stun` expects a config file. The command line supports:

- `--config <path>`: path to config file
- `--fork`: run as a daemon (fork + detach)

Example:

```
./easy-stun/build/easy-stun --config ./config.txt
./easy-stun/build/easy-stun --config ./config.txt --fork
```

## Configuration

The config file is space-delimited: `<key> <value>`, one per line.

Keys used by `easy-stun`:

- `local-port <port|any>`
  - UDP port to bind locally. Use `any` (or `0`) to let OS choose.
- `remote-addr <host>`
  - STUN server hostname/IP.
- `remote-port <port>`
  - STUN server port (typical STUN: 3478).
- `username <string>`
- `password <string>`
  - Credentials used for MESSAGE-INTEGRITY (project-specific; depends on your server setup).
- `script <path>`
  - Path to an executable script that will be spawned on events.
- `keepalive-interval <seconds>`
  - Keepalive interval in seconds. Set to `0` to disable keepalives.
- `acs-addr <host>`
- `acs-port <port>`
  - Address/port used for additional keepalive ping (useful for some deployments).
- `restart-interval <seconds>`
  - If non-zero, on connection error the client waits N seconds and retries.

### Example config

```
local-port any
remote-addr stun.example.com
remote-port 3478
username myuser
password mypass
script /usr/local/bin/easy-stun-hook.sh
keepalive-interval 30
acs-addr acs.example.com
acs-port 7547
restart-interval 5
```

## Script hooks

The configured `script` is spawned asynchronously. Exit codes are logged when the child process exits.

The script is invoked as:

- On mapping discovery/update:
  - `<script> bind <mapped_ip> <mapped_port>`
- On “connection request” packet (currently a placeholder detector):
  - `<script> cr <mapped_ip> <mapped_port>`

## License

MIT