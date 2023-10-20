// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

#include "executor/executor.h"

#include <cstdint>

namespace WasmEdge {
namespace Executor {

Expect<void> Executor::runLocalGetOp(Runtime::StackManager &StackMgr,
                                     uint32_t StackOffset) const noexcept {
  StackMgr.push(StackMgr.getTopN(StackOffset));

  uint8_t T = StackMgr.getTypeTopN(StackOffset);
  if (T == 0) StackMgr.pushType(4);
  else if (T == 1) StackMgr.pushType(8);
  else std::cerr << "[DEBUG]TypeStack Offset(" << StackOffset << ") is " << +T << std::endl;

  // std::cout << "[DEBUG]push stack: type kind: " << +StackMgr.getTypeTop() << std::endl;
  return {};
}

Expect<void> Executor::runLocalSetOp(Runtime::StackManager &StackMgr,
                                     uint32_t StackOffset) const noexcept {
  // StackMgr.getTypeTopN(StackOffset) = StackMgr.getTypeTop();
  StackMgr.getTopN(StackOffset - 1) = StackMgr.pop();
  return {};
}

Expect<void> Executor::runLocalTeeOp(Runtime::StackManager &StackMgr,
                                     uint32_t StackOffset) const noexcept {
  const ValVariant &Val = StackMgr.getTop();
  StackMgr.getTopN(StackOffset) = Val;
  // StackMgr.getTypeTopN(StackOffset) = StackMgr.getTypeTop();
  return {};
}

Expect<void> Executor::runGlobalGetOp(Runtime::StackManager &StackMgr,
                                      uint32_t Idx) const noexcept {
  auto *GlobInst = getGlobInstByIdx(StackMgr, Idx);
  assuming(GlobInst);
  ValType T = GlobInst->getGlobalType().getValType();
  if (T == ValType::I32 || T == ValType::F32) {
    StackMgr.push(GlobInst->getValue());
    StackMgr.pushType(sizeof(uint32_t));
  }
  else if (T == ValType::I64 || T == ValType::F64) {
    StackMgr.push(GlobInst->getValue());
    StackMgr.pushType(sizeof(uint64_t));
  }
  else {
    StackMgr.push(GlobInst->getValue());
    std::cerr << "[DEBUG]runGlobalGetOp::valtype is not 32bit or 64bit" << std::endl;
    StackMgr.pushType(sizeof(uint64_t));
  }
  return {};
}

Expect<void> Executor::runGlobalSetOp(Runtime::StackManager &StackMgr,
                                      uint32_t Idx) const noexcept {
  auto *GlobInst = getGlobInstByIdx(StackMgr, Idx);
  assuming(GlobInst);
  GlobInst->getValue() = StackMgr.pop();
  return {};
}

} // namespace Executor
} // namespace WasmEdge
