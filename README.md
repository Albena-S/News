# News Server

A C++20 low-latency TCP news server for Linux/WSL.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Run the server on the default address (`0.0.0.0:9000`):

```sh
./build/news_server
```

The port and IPv4 bind address can be overridden:

```sh
./build/news_server 8080 127.0.0.1
```

Press `Ctrl+C` for an orderly shutdown. The current networking milestone accepts
and drains client connections; binary protocol handling is the next layer.

## Style

The project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
Use the repository's `.clang-format` file when formatting C++ files.
