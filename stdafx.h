// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#pragma warning( push, 3 )
#include <boost/bind.hpp>
#include <boost/date_time.hpp>
#include <boost/function.hpp>
#include <boost/random.hpp>
#include <boost/thread/future.hpp>
#pragma warning( pop )

using std::max;
using std::min;

#if !defined(NO_INIUTIL_USING)
#define NO_INIUTIL_USING
#endif
#include <max.h>

#include <frantic/max3d/maxscript/includes.hpp>

#if MAX_VERSION_MAJOR < 15
#define p_end end
#endif
