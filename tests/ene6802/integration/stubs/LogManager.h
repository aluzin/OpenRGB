// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
inline unsigned int test_log_errors = 0;
#define LOG_ERROR(...) (++test_log_errors)
