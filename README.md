# cpp-Claude-CLI-Agent

A small C++ CLI that sends a chat prompt to an OpenRouter-compatible chat completion endpoint and exposes three executable "tools" (Read, Write, Bash) which the model may call. The program loops until the model returns a final text response.

## Quick start

1. Set your OpenRouter API key and (optional) VCPKG root:

```bash
export OPENROUTER_API_KEY="your_api_key_here"
export VCPKG_ROOT="$HOME/vcpkg"   # if you use vcpkg
```

2. Run the provided script (recommended):

```bash
./run.sh -p "Your prompt here"
```

3. Or build manually and run:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/claude-code -p "Your prompt here"
```

## What it does (short)

- Sends a chat prompt to the configured base URL (default: `https://openrouter.ai/api/v1`) using the model `anthropic/claude-haiku-4.5`.
- Provides three tool descriptors to the model:
  - Read: read and return file contents
  - Write: write content to a file (overwrite)
  - Bash: execute a shell command and return its output
- If the model requests tool calls, the program executes them and appends the results back into the conversation; it repeats until the model returns a message with no tool calls, which is printed to stdout.

## Security warning

This program executes arbitrary shell commands and writes files based on model instructions. Do NOT run it with untrusted prompts or on sensitive systems. Use a sandboxed environment (container or VM) for testing.

## Dependencies

- CMake >= 3.13
- C++23
- cpr (HTTP client)
- nlohmann-json

The project includes a vcpkg configuration; the included `run.sh` uses vcpkg. Example vcpkg steps:

```bash
# one-time
git clone https://github.com/microsoft/vcpkg.git $HOME/vcpkg
$HOME/vcpkg/bootstrap-vcpkg.sh
$HOME/vcpkg/vcpkg install cpr nlohmann-json
export VCPKG_ROOT="$HOME/vcpkg"
```

## Example usage

```bash
export OPENROUTER_API_KEY="sk-..."
./run.sh -p "Summarize the contents of src/main.cpp"
```

## Troubleshooting

- Build errors about missing packages: ensure vcpkg is installed and `VCPKG_ROOT` is set, or install CPR and nlohmann-json via your system package manager.
- HTTP errors: verify `OPENROUTER_API_KEY` and set `OPENROUTER_BASE_URL` if your provider uses a different endpoint.

---

If you want, I can also open a PR that further expands examples or adds a brief safe-example prompt. 
