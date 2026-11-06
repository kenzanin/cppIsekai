---
type: decision
title: "Build system: CMake + C++23, exe-only"
description: "Single executable target, C++23, cmake floor 3.20; lib/tests follow once the first module lands."
generated: { by: agent/mcp, at: "2026-11-06T08:04:46Z" }
status: stable
---

cppIsekai scaffolded as a single executable target (src/main.cpp), C++23, -Wall -Wextra, CMAKE_EXPORT_COMPILE_COMMANDS=ON, build/ gitignored. cmake_minimum_required floor 3.20. Verified toolchain: gcc 16.2.1, clang 23.1.4, cmake 4.4.5, ninja 1.13.2. No lib target/tests/install until the first module lands; existing Makefile (OKF helper) untouched.
