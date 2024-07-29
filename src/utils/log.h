/*
 *  Copyright (C) 2024 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

namespace utils
{

enum class LOGLevel
{
  DEBUG = 0,
  INFO = 1,
  WARNING = 2,
  ERROR = 3,
  FATAL = 4
};

void LOG(LOGLevel loglevel, const char* format, ...);
void DEBUG_PRINT(const char* format, ...);

#if PRINT_DEBUG == 1
#define DBGLOG(f, ...) { utils::DEBUG_PRINT(f, ##__VA_ARGS__); }
#else
#define DBGLOG
#endif

};