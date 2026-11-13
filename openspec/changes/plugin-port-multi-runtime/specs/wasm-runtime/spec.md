# wasm-runtime delta

## ADDED Requirements

### Requirement: WASM (wasm3) plugin runtime
The host SHALL load WASM plugins via wasm3 under `src/plugin/wasm/`, exposing host tools as imports to the wasm module.

#### Scenario: WASM toolchain
- **WHEN** building a WASM plugin
- **THEN** the toolchain is wasi-sdk or odin (decision recorded in Phase 3 task 3.1)

#### Scenario: WASM plugin entrypoint
- **WHEN** a plugin folder `plugins_dev/wasm/<site>/` contains `main.json` and compiled `main.wasm`
- **THEN** the wasm3 loader instantiates the module and routes JSON-text-in/out ABI calls to plugin exports

## MODIFIED Requirements

### Requirement: Plugin ABI contract
The JSON-text-in/out ABI is runtime-agnostic; WASM plugins implement the same exports as Lua/JS plugins, adapted to the wasm3 calling convention.
