#pragma once

using u8 = char unsigned;
using u16 = short unsigned;
using u32 = int unsigned;
using u64 = long long unsigned;

using s8 = char;
using s16 = short;
using s32 = int;
using s64 = long long;

using b32 = s32;

using f32 = float;
using f64 = double;

#define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <windows.h>

