#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdint>
#include <array>
#include <vector>

struct Vec3 { float x, y, z; };

struct Vertex
{
    float x, y, z;
    DWORD color;
};

extern Vec3 g_camAt;
extern Vec3 g_camPos;

extern uintptr_t g_gameBase;
extern uintptr_t g_santaBase;
extern uintptr_t g_camPtr;

namespace offsets {
    constexpr uintptr_t PLAYER_ROOT = 0x0039BC5C;
    constexpr uintptr_t CAM_ROOTPTR = 0x0039BC64;

    constexpr std::array<uintptr_t, 4> PLAYER_CHAIN = { 0x3C, 0x7C, 0x44, 0x0 };
    constexpr std::array<uintptr_t, 1> CAMERA_CHAIN = { 0x1B4 };

    constexpr uintptr_t SANTA_X = 0x1B0;
    constexpr uintptr_t SANTA_Z = 0x1B8;
    constexpr uintptr_t SANTA_Y = 0x1C0;

    constexpr uintptr_t CAM_X = 0x28;
    constexpr uintptr_t CAM_Z = 0x2C;
    constexpr uintptr_t CAM_Y = 0x30;

    constexpr uintptr_t CAMT_X = 0x34;
    constexpr uintptr_t CAMT_Z = 0x38;
    constexpr uintptr_t CAMT_Y = 0x3C;
}

void TryResolveGamePointers();
