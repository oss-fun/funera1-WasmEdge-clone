// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

//===-- wasmedge/common/version.h - Version information -------------------===//
//
// Part of the WasmEdge Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains version information that passed from configure stage.
///
//===----------------------------------------------------------------------===//
#pragma once

#include <string_view>

namespace WasmEdge {

using namespace std::literals::string_view_literals;

#define CPACK_PACKAGE_VERSION "0.13.1-77-g04062abe"sv
static inline std::string_view kVersionString [[maybe_unused]] =
    CPACK_PACKAGE_VERSION;
#undef CPACK_PACKAGE_VERSION

#define CMAKE_INSTALL_FULL_LIBDIR "/usr/local/lib"
static inline std::string_view kGlobalPluginDir [[maybe_unused]] =
    CMAKE_INSTALL_FULL_LIBDIR "/wasmedge"sv;
#undef CMAKE_INSTALL_FULL_LIBDIR

static inline constexpr const uint32_t kPluginCurrentAPIVersion
    [[maybe_unused]] = 2;

} // namespace WasmEdge
