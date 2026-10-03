# Puffin

Header-only C++20 libraries, one module per folder under `puffin/<module>/` (`include/`, `tests/`, `README.md`).
Namespaces follow the folder: `puffin::<module>`. Code, comments, commits and PR text are in English.

## Build and test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DBUILD_SAMPLES=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The asio, TLS and Qt adapters are only built when `libasio-dev`, `libssl-dev` and `qt6-base-dev` are installed.

## Conventions

- CMake minimum is 3.24 in every `CMakeLists.txt`.
- Adapters for a third-party runtime live in `puffin/<module>/include/puffin/<module>/adapter/`.
- Internal helpers live in `detail/`, never as static members of the class that happens to use them first.
- Module docs live in `puffin/<module>/README.md`; `docs/roadmap.md` has one short checkbox list per module,
  keep it updated when finishing or deferring work.
- PRs target `master`.

## Code quality

Passing tests is not enough. Before committing, check the diff against these rules:

- One function, one responsibility. Around 40 lines is the limit: past that, split it. A coroutine that reads,
  parses, applies protocol rules and writes is four functions.
- No duplicated logic. Before writing a loop or a helper, grep for an existing one; if the same code is needed
  in two places (client and server for instance), extract it first.
- No string surgery to work around a missing option (truncating serialized output, searching for separators in
  something we just produced). Add the option to the API that produced it.
- Repeated workarounds (`optional` + `exception_ptr` + flag to escape a `catch` in a coroutine) get a named
  helper, written once.
- Keep classes as thin facades over smaller, testable pieces; pure logic (no I/O) gets its own unit tests.
- `.clang-tidy` (function size and complexity) is enforced in CI on async and webkit: run `tools/clang-tidy.sh`
  before pushing. A `NOLINT` needs a comment saying why, and a roadmap entry if it is debt.

Before every push, run `/simplify` on the diff and apply what it finds. Bugs are covered by the Claude reviewer
on the PR.
