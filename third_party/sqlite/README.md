# SQLite (vendored amalgamation)

## Version
**3.45.1** — `SQLITE_VERSION "3.45.1"`, `SQLITE_VERSION_NUMBER 3045001`
(see `SQLITE_VERSION` in `sqlite3.c`).

## Download source
Official amalgamation snapshot from the SQLite website:
https://sqlite.org/2024/sqlite-amalgamation-3450100.zip

For future upgrades, fetch the current amalgamation from
https://sqlite.org/download.html and replace `sqlite3.c` / `sqlite3.h`.

## License
Public Domain. SQLite is released under the blessing of the SQLite
copyright holders (https://sqlite.org/copyright.html); no license fee or
attribution is required.

## Build options
The project builds the single-file amalgamation `sqlite3.c` as a static
library, `sqlite3`, with the standard CMake target in `CMakeLists.txt`:

```cmake
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_SOURCE_DIR}/third_party/sqlite)
```

No custom `SQLITE_*` compile definitions are set — the amalgamation is
compiled with the default feature set plus the compiler's standard flags.
Because the amalgamation is ~9 MB, it is compiled **once** into the `sqlite3`
static library (built once per configuration, per remediation plan P2-2)
rather than recompiled into every executable/test target. Executables and
tests link against `sqlite3` instead of compiling `sqlite3.c` themselves.