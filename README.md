# News Server

A C++20 low-latency TCP news server for Linux/WSL.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Style

The project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
Use the repository's `.clang-format` file when formatting C++ files.
