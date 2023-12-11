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
  Expect<void> dump(std::string filename) const noexcept {
    // Open file
    filename = filename + "global.img";
    std::ofstream ofs(filename, std::ios::trunc | std::ios::binary);
    if (!ofs) {
      return Unexpect(ErrCode::Value::IllegalPath);
    }

    // TODO: uint32_t型以外の場合どうなるのか(int32_tとかは同じ値が出力された)
    ValType T = GlobType.getValType();
    if (T == ValType::I32 || T == ValType::F32) {
      ofs.write(reinterpret_cast<char*>(Value.get<uint32_t>()), sizeof(uint32_t));
    }
    else if (T == ValType::I64 || T == ValType::F64) {
      ofs.write(reinterpret_cast<char*>(Value.get<uint64_t>()), sizeof(uint64_t));
    }
    else {
      ofs.write(reinterpret_cast<char*>(Value.get<uint128_t>()), sizeof(uint128_t));
    }

    // Close file
    ofs.close();
    return {};
  }

  Expect<void> restore(std::string filename)  noexcept {
    // Open file
    filename = filename + "global.img";
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
      return Unexpect(ErrCode::Value::IllegalPath);
    }

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
