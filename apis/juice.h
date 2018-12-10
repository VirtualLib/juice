///////////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2017 The Authors of ANT(http:://ant.sh) . All Rights Reserved. 
// Use of this source code is governed by a BSD-style license that can be 
// found in the LICENSE file. 
//
///////////////////////////////////////////////////////////////////////////////////////////

#ifndef VIRTUALLIB_JUICE_JUICE_INCLUDE_H_
#define VIRTUALLIB_JUICE_JUICE_INCLUDE_H_

#if ((defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(_MSC_VER)) && !defined(JUICE_STATIC))

#ifdef JUICE_EXPORTS
#define JUICE_API __declspec(dllexport)
#else
#define JUICE_API __declspec(dllimport)
#endif // UILIB_EXPORTS

#else
#define JUICE_API 
#endif // ((defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(_MSC_VER)) && !defined(JUICE_STATIC))

#endif // !VIRTUALLIB_JUICE_JUICE_INCLUDE_H_