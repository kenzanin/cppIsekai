---
type: decision
title: "Build system: CMake + C++23, exe-only"
description: "Single executable target, C++23, cmake floor 3.20; lib/tests menyusul saat modul pertama."
generated: { by: agent/mcp, at: "2026-11-06T02:49:16Z" }
status: stable
---

cppIsekai discaffold sebagai single executable target (src/main.cpp), C++23, -Wall -Wextra, CMAKE_EXPORT_COMPILE_COMMANDS=ON, build/ di-gitignore. Floor cmake_minimum_required 3.20. Toolchain terverifikasi: gcc 16.2.1, clang 23.1.4, cmake 4.4.5, ninja 1.13.2. Belum ada lib target/tests/install sampai modul pertama; Makefile existing (helper OKF) tidak diubah.
