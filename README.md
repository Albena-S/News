# News Server

A C++20 low-latency TCP news server for Linux/WSL.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Run the server on the default local address (`127.0.0.1:9000`):

```sh
./build/news_server
```

`127.0.0.1` means "listen only on this machine." To accept connections from
other machines on the network, bind to `0.0.0.0` explicitly.

The port and IPv4 bind address can be overridden:

```sh
./build/news_server 8080 0.0.0.0
```

In another terminal, connect the demo client:

```sh
./build/news_client 127.0.0.1 8080
```

The client authenticates as `demo`, subscribes from its last received news id,
prints incoming titles, and reconnects automatically if the socket is closed.

Press `Ctrl+C` for an orderly shutdown. The current networking milestone accepts
client connections and handles the simplified authentication and subscription
frames. A publisher thread generates one demo title every 20 seconds, wakes the
server event loop with `eventfd`, and the server writes the record to the WAL
before broadcasting it to live clients. The demo client remembers the last
received news id and resubscribes from that id after reconnecting.

The demo client authenticates with the user from `config/users.conf.example`.
Passwords are stored as a small deterministic demo hash so the example does not
keep plaintext passwords in the config file. A production version should use TLS
and a proper slow salted password hash.

## Style

The project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
Use the repository's `.clang-format` file when formatting C++ files.
