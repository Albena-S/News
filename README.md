# News Server

A small C++20 TCP news streaming project for Linux/WSL.

The project demonstrates:

- a non-blocking TCP server based on `epoll`;
- a simple binary protocol;
- demo authentication;
- live news publishing;
- WAL-based recovery;
- replay from an in-memory ring or from the WAL;
- a reconnecting demo client.

## Requirements

- Linux or WSL
- CMake 3.20+
- A C++20 compiler, for example `g++` or `clang++`

On Ubuntu/WSL:

```sh
sudo apt update
sudo apt install build-essential cmake
```

## Build

From the repository root:

```sh
cmake -S . -B build
cmake --build build
```

This creates:

- `build/news_server`
- `build/news_client`
- test executables under `build/`

## Run the tests

After building:

```sh
ctest --test-dir build --output-on-failure
```

## Start the server

Default server:

```sh
./build/news_server
```

By default, the server listens on:

```text
127.0.0.1:9000
```

`127.0.0.1` means the server accepts connections only from the same machine.

You can choose a port and bind address:

```sh
./build/news_server 8080 127.0.0.1
```

To listen on all network interfaces:

```sh
./build/news_server 8080 0.0.0.0
```

Server arguments:

```text
news_server [port] [bind_address]
```

Examples:

```sh
./build/news_server
./build/news_server 8080
./build/news_server 8080 127.0.0.1
./build/news_server 8080 0.0.0.0
```

Stop the server with `Ctrl+C`.

## Start a client

Open another terminal and run:

```sh
./build/news_client
```

By default, the client connects to:

```text
127.0.0.1:9000
```

and authenticates with:

```text
username: demo
password: demo
```

Client arguments:

```text
news_client [address] [port] [username] [password] [last_received_id]
```

Examples:

```sh
./build/news_client
./build/news_client 127.0.0.1 9000
./build/news_client 127.0.0.1 9000 demo demo
./build/news_client 127.0.0.1 9000 demo demo 0
```

The final argument, `last_received_id`, controls replay:

- omitted: wait only for new titles;
- `0`: request all available news;
- any other id: request news after that id.

If `last_received_id` is greater than the newest server id, the subscription is still valid and the client waits for the next published title.

## Demo flow

In terminal 1:

```sh
./build/news_server
```

In terminal 2:

```sh
./build/news_client 127.0.0.1 9000 demo demo 0
```

The server publishes one demo title every 20 seconds.

You should see output similar to:

```text
authenticated
subscribed from news id 0; waiting for news
news 1: Breaking: market opens higher
news 2: Weather: rain expected tomorrow
```

You can also run multiple clients at the same time with the same demo credentials.

## Authentication

Demo users are loaded from:

```text
config/users.conf.example
```

The demo credentials are:

```text
username: demo
password: demo
```

The file stores a deterministic demo hash instead of the clear-text password. This is enough for the exercise, but it is not production password storage.

A production version should use TLS and a real password hashing strategy such as bcrypt, scrypt, or Argon2 with salts.

## WAL and replay

The server writes published news to:

```text
news.wal
```

The WAL is append-only. On startup, the server reads it back, rebuilds recent replay state, and continues with the next news id.

Replay works like this:

- recent missed records are served from the in-memory replay ring;
- older missed records are read from the WAL;
- live clients receive newly published records after the server appends them to the WAL.

`news.wal` is ignored by Git because it is runtime data.

## Clean build

To rebuild from scratch:

```sh
rm -rf build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Project layout

```text
include/news/      Public headers for the core library
src/               Server/core implementation
client/            Demo client
tests/             Unit and integration tests
config/            Demo user configuration
```

The main CMake target structure is:

- `news_core`: shared project logic;
- `news_server`: server executable;
- `news_client`: client executable;
- `*_test`: test executables.

## Style

The project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

Use the repository `.clang-format` file when formatting C++ files.
