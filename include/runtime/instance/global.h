// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

//===-- wasmedge/runtime/instance/global.h - Global Instance definition ---===//
//
// Part of the WasmEdge Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the global instance definition in store manager.
///
//===----------------------------------------------------------------------===//
#pragma once

#include "ast/type.h"

#include <fstream>
#include <cstring>
#include <iostream>

namespace WasmEdge {
namespace Runtime {
namespace Instance {

class GlobalInstance {
public:
  GlobalInstance() = delete;
  GlobalInstance(const AST::GlobalType &GType,
                 ValVariant Val = uint32_t(0)) noexcept
      : GlobType(GType), Value(Val) {}

  /// Getter of global type.
  const AST::GlobalType &getGlobalType() const noexcept { return GlobType; }

  /// Getter of value.
  const ValVariant &getValue() const noexcept { return Value; }

  /// Getter of value.
  ValVariant &getValue() noexcept { return Value; }

  // Migration functions
  // filename = globinst_{i}
  void dump(std::string filename) const noexcept {
    std::ofstream valueStream;

    // Open file
    std::string valueFile = filename + "_value.img";
    valueStream.open(valueFile, std::ios::trunc);

    // TODO: uint32_t型以外の場合どうなるのか(int32_tとかは同じ値が出力された)
    ValType T = GlobType.getValType();
    if (T == ValType::I32 || T == ValType::F32) {
      valueStream << std::setw(32) << std::setfill('0') << Value.get<uint32_t>() << std::endl;
    }
    else if (T == ValType::I64 || T == ValType::F64) {
      valueStream << std::setw(64) << std::setfill('0') << Value.get<uint64_t>() << std::endl;
    }
    else {
      valueStream << std::setw(128) << std::setfill('0') << Value.get<uint128_t>() << std::endl;
    }

    // Close file
    valueStream.close();
  }

  void restore(std::string filename)  noexcept {
    // restoreFileをparseする
    std::ifstream valueStream;

    // Open file
    std::string valueFile = filename + "_value.img";
    valueStream.open(valueFile);

    // Restore Value
    std::string valueString;
    getline(valueStream, valueString);
    Value = static_cast<uint128_t>(std::stoul(valueString));

    // Close file
    valueStream.close();
  }
private:
  /// \name Data of global instance.
  /// @{
  AST::GlobalType GlobType;
  ValVariant Value;
  /// @}
};

} // namespace Instance
} // namespace Runtime
} // namespace WasmEdge
