// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

//===-- wasmedge/common/config.h - Configure information ------------------===//
//
// Part of the WasmEdge Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains configure information that passed from configure stage.
///
//===----------------------------------------------------------------------===//
#pragma once

#include <string_view>

namespace WasmEdge {

using namespace std::literals::string_view_literals;

#define CMAKE_INSTALL_FULL_LOCALSTATEDIR                                  \
    "/usr/local/var"
static inline constexpr std::string_view kCacheRoot [[maybe_unused]] =
    CMAKE_INSTALL_FULL_LOCALSTATEDIR "/cache/wasmedge"sv;
#undef CMAKE_INSTALL_FULL_LOCALSTATEDIR

#define HAVE_MMAP 1
#define HAVE_PWD_H 1

} // namespace WasmEdge
