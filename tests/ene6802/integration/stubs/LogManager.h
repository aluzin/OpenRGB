// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
inline unsigned int test_log_errors = 0;
template<typename... Args> inline void TestLogError(const char*, Args...) { ++test_log_errors; }
#define LOG_ERROR(...) TestLogError(__VA_ARGS__)
