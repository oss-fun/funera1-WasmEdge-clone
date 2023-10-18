#pragma once

#include "ast/instruction.h"
#include "runtime/instance/module.h"
#include "runtime/instance/function.h"
#include "runtime/stackmgr.h"
#include "runtime/storemgr.h"
#include "executor/executor.h"

#include <map>
#include <iostream>
#include <vector>
#include <string>
#include <cassert>

namespace WasmEdge {
  
namespace Runtime {
  class StackManager;
}

namespace Executor {
struct SourceLoc {
  bool operator==(const SourceLoc& rhs) const {
    return (FuncIdx == rhs.FuncIdx && Offset == rhs.Offset);
  }

  uint32_t FuncIdx;
  uint32_t Offset;
};

class Migrator {
public:
  /// TODO: ModuleInstanceがnullだったときの名前。重複しないようにする
  const std::string NULL_MOD_NAME = "null";
  using IterMigratorType = std::map<AST::InstrView::iterator, SourceLoc>;

  /// ================
  /// Interface
  /// ================
  // const Runtime::Instance::FunctionInstance& restoreFuncInstance(const Runtime::Instance::FunctionInstance& Func) {
  //   Runtime::Instance::FunctionInstance* newFunc = const_cast<Runtime::Instance::FunctionInstance*>(&Func);

  //   const Runtime::Instance::ModuleInstance* restoredModInst = restoreModInst(Func.getModule());
  //   newFunc->setModule(restoredModInst);

  //   const Runtime::Instance::FunctionInstance* constFunc = const_cast<const Runtime::Instance::FunctionInstance*>(newFunc);
  //   return constFunc;
  // } 
  // 

  /// convert wasmedge iterator to wamr itertor
  uint32_t dispatch(const AST::InstrView::iterator PC) {
    const AST::Instruction &Instr = *PC;
    uint32_t res = 1;

    // encode後のoffsetさえわかればいい
    auto encodeLeb = [](uint64_t val) {
      uint32_t offset = 0;
      do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val > 0) {
          byte |= 0x80;
        }
        offset++;
      } while (val > 0);
      return offset;
    };

    switch (Instr.getOpCode()) {
      case OpCode::Call_indirect:
        res += encodeLeb(Instr.getSourceIndex());
        res += encodeLeb(Instr.getTargetIndex());
        break;
      case OpCode::Br_table:
        // todo
        break;
      case OpCode::Block:
      case OpCode::Loop:
      case OpCode::If:
      case OpCode::Br:
      case OpCode::Br_if:
        break;
      case OpCode::Call:
      case OpCode::Return_call:
        res += encodeLeb(Instr.getTargetIndex());
        break;
      // NOTE: いるかわからんので放置
      // case OpCode::Select_t:
      //   break;
      case OpCode::Table__get:
      case OpCode::Table__set:
        res += encodeLeb(Instr.getTargetIndex());
        break;
      case OpCode::Ref__null:
      case OpCode::Ref__func:
        res += encodeLeb(Instr.getTargetIndex());
        break;
      case OpCode::Local__get:
      case OpCode::Local__set:
      case OpCode::Local__tee:
        res += encodeLeb(Instr.getStackOffset());
        break;
      case OpCode::Global__get:
      case OpCode::Global__set:
        res += encodeLeb(Instr.getTargetIndex());
        break;
      case OpCode::I32__load:
      case OpCode::I64__load:
      case OpCode::F32__load:
      case OpCode::F64__load:
      case OpCode::I32__load8_s:
      case OpCode::I32__load8_u:
      case OpCode::I32__load16_s:
      case OpCode::I32__load16_u:
      case OpCode::I64__load8_s:
      case OpCode::I64__load8_u:
      case OpCode::I64__load16_s:
      case OpCode::I64__load16_u:
      case OpCode::I64__load32_s:
      case OpCode::I64__load32_u:
      case OpCode::I32__store:
      case OpCode::I64__store:
      case OpCode::F32__store:
      case OpCode::F64__store:
      case OpCode::I32__store8:
      case OpCode::I32__store16:
      case OpCode::I64__store8:
      case OpCode::I64__store16:
      case OpCode::I64__store32:
      case OpCode::Memory__grow:
      case OpCode::Memory__size:
        res += encodeLeb(Instr.getTargetIndex());
        res += encodeLeb(Instr.getMemoryOffset());
        break;
      case OpCode::I32__const:
      case OpCode::I64__const:
      case OpCode::F32__const:
      case OpCode::F64__const:
        res += encodeLeb((Instr.getNum()).get<uint64_t>());
        break;

      /// MISC RPEFIX
      case OpCode::Memory__init:
      case OpCode::Data__drop:
      case OpCode::Memory__copy:
      case OpCode::Memory__fill:
      case OpCode::Table__init:
      case OpCode::Elem__drop:
      case OpCode::Table__copy:
      case OpCode::Table__grow:
      case OpCode::Table__size:
      case OpCode::Table__fill:
        res++; // OpCodeが2byte分使う
        res += encodeLeb(Instr.getTargetIndex());
        break;
      default:
        break;
    }
    return res;
  }

  /// Find module by name.
  const Runtime::Instance::ModuleInstance *findModule(std::string_view Name) const {
    auto Iter = NamedMod.find(Name);
    if (likely(Iter != NamedMod.cend())) {
      return Iter->second;
    }
    return nullptr;
  }

  /// ================
  /// Tools
  /// ================
  void setIterMigrator(const Runtime::Instance::ModuleInstance* ModInst) {
    if (IterMigrators.count((std::string)ModInst->getModuleName())) {
      return;
    }

    IterMigratorType IterMigrator;

    for (uint32_t I = 0; I < ModInst->getFuncNum(); ++I) {
      Runtime::Instance::FunctionInstance* FuncInst = ModInst->getFunc(I).value();
      AST::InstrView Instr = FuncInst->getInstrs();
      AST::InstrView::iterator PC = Instr.begin();
      AST::InstrView::iterator PCEnd = Instr.end();
      
      uint32_t Offset = 0;
      while (PC != PCEnd) {
        IterMigrator[PC] = SourceLoc{I, Offset};
        Offset++;
        PC++;
      }
    }

    IterMigrators[(std::string)ModInst->getModuleName()] = IterMigrator;
  }
  
  IterMigratorType getIterMigrator(const Runtime::Instance::ModuleInstance* ModInst) {
    if (IterMigrators.count((std::string)ModInst->getModuleName())) {
      return IterMigrators[(std::string)ModInst->getModuleName()];
    }

    std::string_view ModName = ModInst->getModuleName();
    setIterMigrator(ModInst);
    return IterMigrators[(std::string)ModName];
  }

  IterMigratorType getIterMigratorByName(std::string ModName) {
    if (IterMigrators.count(ModName)) {
      return IterMigrators[ModName];
    } else {
      IterMigratorType I;
      return I;
    }
  }

  SourceLoc getSourceLoc(AST::InstrView::iterator Iter) {
    IterMigratorType IterMigrator = getIterMigratorByName(BaseModName);
    assert(IterMigrator);

    struct SourceLoc Data = IterMigrator[Iter];
    return Data;
  }
  
  uint128_t stou128(std::string s) {
    uint128_t ret = 0;
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] < '0' || '9' < s[i]) {
        // std::cout << "value is number!!" << std::endl;
        assert(-1);
        return -1;
      }
      int n = (int)(s[i] - '0');
      ret = ret * 10 + n;
    }
    return ret;
  }
  
  void debugFuncOpcode(uint32_t FuncIdx, uint32_t Offset, AST::InstrView::iterator it) {
    std::ofstream opcodeLog;
    opcodeLog.open("wasmedge_opcode.log", std::ios::app);
    opcodeLog << "fidx: " << FuncIdx << "\n";
    for (uint32_t i = 0; i < Offset; i++) it--;
    for (uint32_t i = 0; i < Offset; i++) {
      opcodeLog << i+1 << ") opcode: 0x" << std::hex << static_cast<uint16_t>(it->getOpCode()) << "\n";
      opcodeLog << std::dec;
      it++;
    }
  }

  /// ================
  /// Dump functions for WAMR
  /// ================
  void dumpMemory(const Runtime::Instance::ModuleInstance* ModInst) {
    ModInst->dumpMemInst("wamr");
  }
  
  void dumpGlobal(const Runtime::Instance::ModuleInstance* ModInst) {
    ModInst->dumpGlobInst("wamr");
  }
  
  void dumpStack(Runtime::StackManager& StackMgr) {
    std::ofstream fout;
    // fout.open("wamr_stack.img", std::ios::out | std::ios::binary | std::ios::trunc);
    fout.open("wamr_stack.img", std::ios::out | std::ios::trunc);
    
    using Value = ValVariant;
    std::vector<Value> Vals = StackMgr.getValueStack();
    std::vector<uint8_t> Typs = StackMgr.getTypeStack();
    
    if (Vals.size() != Typs.size()) {
      std::cerr << "ValStack size != TypStack size" << std::endl;
      std::cerr << "ValStack size: " << Vals.size() << std::endl;
      std::cerr << "TypStack size: " << Typs.size() << std::endl;
      exit(1);
    }

    std::cout << "[DEBUG]TypeStack: [";
    for (size_t I = 0; I < Vals.size(); ++I) std::cout << +Typs[I];
    std::cout << "]" << std::endl;

    for (size_t I = 0; I < Vals.size(); ++I) {
      Value v = Vals[I];
      int t = Typs[I];
      // 32bitのとき
      if (t == 0) 
        fout << v.get<uint32_t>() << std::endl;
      else if (t == 1) 
        fout << v.get<uint64_t>() << std::endl;
      else {
        std::cerr << "Type size is not 32bit or 64bit" << std::endl;
        exit(1);
      }
    }
    
    fout.close();
  }
  
  struct SourceLoc convertIterForWamr(const AST::InstrView::iterator PCNow) {
    IterMigratorType IterMigrator = getIterMigratorByName(BaseModName);
    assert(IterMigrator);

    // fout.open("wamr_iter.img", std::ios::trunc);

    struct SourceLoc Data = IterMigrator[PCNow];
    uint32_t FuncIdx = Data.FuncIdx;
    uint32_t Offset = 0;
    AST::InstrView::iterator It = PCNow;
    It -= Data.Offset;

    while (It != PCNow) {
      Offset += dispatch(It);
      It++;
    }

    return SourceLoc{FuncIdx, Offset};
  }
  
  struct CtrlInfo {
    AST::InstrView::iterator Iter;
  };

  std::vector<struct CtrlInfo> getCtrlStack(const AST::InstrView::iterator PCNow) {
    std::vector<struct CtrlInfo> CtrlStack;
    
    IterMigratorType IterMigrator = getIterMigratorByName(BaseModName);
    assert(IterMigrator);

    struct SourceLoc Data = IterMigrator[PCNow];
    AST::InstrView::iterator It = PCNow;
    
    // プログラムカウンタを関数の先頭に戻す
    It -= Data.Offset;
    
    auto CtrlPush = [&](const AST::InstrView::iterator It) {
      struct CtrlInfo info = CtrlInfo {It};
      CtrlStack.push_back(info);
    };

    auto CtrlPop = [&]() {
      CtrlStack.pop_back();
    };

    while (It != PCNow) {
      const AST::Instruction &Instr = *It;
      switch (Instr.getOpCode()) {
        // push
        case OpCode::Block:
        case OpCode::Loop:
        case OpCode::If:
        case OpCode::Call:
        case OpCode::Return_call:
          CtrlPush(It);
          break;
        // pop
        case OpCode::End:
        // case OpCode::Br:
        // case OpCode::Br_if:
        // case OpCode::Br_table:
          // TODO: Br系は抜けるブロックの分だけPOPする必要がある
          CtrlPop();
          break;
        default:
          break;
      }

      It++;
    }
    
    return CtrlStack;
  }
  
  /// dumpするもの
  void dumpFrame(Runtime::StackManager &StackMgr) {
    using Value = ValVariant;

    std::vector<Runtime::StackManager::Frame> FrameStack = StackMgr.getFrameStack();
    std::vector<Value> ValueStack = StackMgr.getValueStack();
    std::vector<uint8_t> TypeStack = StackMgr.getTypeStack();
    // TypeStackからWAMRのセルの個数累積和みたいにする
    // 累積和 1-indexed
    std::vector<uint32_t> WamrCellSumStack(TypeStack.size()+1, 0);

    IterMigratorType IterMigrator = getIterMigratorByName(BaseModName);

    // WamrCelSumStackの初期化
    for (uint32_t I = 0; I < TypeStack.size(); I++) {
        WamrCellSumStack[I+1] = WamrCellSumStack[I] + (TypeStack[I] == 0 ? 1 : 2);
    }
    std::ofstream fout;
    fout.open("wamr_frame.img", std::ios::trunc);
    
    // WAMRは4bytesセルが何個でカウントするので、それに合わせてOffsetをdumpする
    // [Start, End]の区間のcell_numを取得
    auto getStackOffset = [&](uint32_t Start, uint32_t End) {
      return WamrCellSumStack.at(End) - WamrCellSumStack.at(Start);
    };

    
    std::vector<struct CtrlInfo> LastCtrlStack;
    uint32_t prevVPos = 0;

    std::cout << "[DEBUG]frame num: " << FrameStack.size() << std::endl;

    fout << "frame num" << std::endl;
    fout << FrameStack.size() << std::endl;
    fout << std::endl;

    // Frame Stackを走査
    for (size_t I = 0; I < FrameStack.size(); ++I) {
      Runtime::StackManager::Frame f = FrameStack[I];
      std::vector<struct CtrlInfo> CtrlStack = getCtrlStack(f.From);
      const Runtime::Instance::ModuleInstance *ModInst = f.Module;

      // Last
      if (I == FrameStack.size() - 1) {
        LastCtrlStack = CtrlStack;
      }

      // dummpy frame
      if (ModInst == nullptr) {
        fout << "func_idx" << std::endl;
        fout << -1 << std::endl;
        // TODO: dummy frameのall_cell_numいる
        // fout << all_cell_num << std::endl;
        fout << "all_cell_num" << std::endl;
        fout << 100 << std::endl;
        fout << std::endl;
      }
      else {
        std::string_view ModName = ModInst->getModuleName();
        fout << ModName << std::endl;
        
        // debug: 関数インデックスの出力
        auto Data = IterMigrator[f.From];
        std::cout << "[DEBUG] Func index: " << Data.FuncIdx << std::endl;
        std::cout << "[DEBUG] iter offset: " << Data.Offset << std::endl;

        // ip_offset
        Data = convertIterForWamr(f.From);
        fout << "func_idx" << std::endl;
        fout << Data.FuncIdx << std::endl;
        fout << "ip_offset" << std::endl;
        fout << Data.Offset << std::endl;
        std::cout << "[DEBUG] " << I+1 << ") ip_offset" << std::endl;

        // sp_offset
        fout << "sp_offset" << std::endl;
        fout << getStackOffset(prevVPos + f.Locals, f.VPos) << std::endl;
        fout << std::endl;
        std::cout << "[DEBUG] " << I+1 << ") sp_offset" << std::endl;

        // csp_offset
        fout << "csp_offset" << std::endl;
        fout << CtrlStack.size() << std::endl;
        fout << std::endl;
        std::cout << "[DEBUG] " << I+1 << ") csp_offset" << std::endl;
        
        // lp (params, locals)
        fout << "lp(params, locals)" << std::endl;
        for (uint32_t I = prevVPos+1; I <= prevVPos + f.Locals; I++) {
          Value V = ValueStack[I];
          uint8_t T = TypeStack[I];

          if (T == 0) 
            fout << std::setw(32) << std::setfill('0') << V.get<uint32_t>() << std::endl;
          else  
            fout << std::setw(64) << std::setfill('0') << V.get<uint64_t>() << std::endl;
        }
        fout << std::endl;
        std::cout << "[DEBUG] " << I+1 << ") lp(params, locals)" << std::endl;

        // stack
        fout << "stack" << std::endl;
        for (uint32_t I = prevVPos + f.Locals + 1; I <= f.VPos; I++) {
          Value V = ValueStack[I];
          uint8_t T = TypeStack[I];

          if (T == 0) 
            fout << std::setw(32) << std::setfill('0') << V.get<uint32_t>() << std::endl;
          else  
            fout << std::setw(64) << std::setfill('0') << V.get<uint64_t>() << std::endl;
        }
        fout << std::endl;
        std::cout << "[DEBUG] " << I+1 << ") stack" << std::endl;
        

        // for csp_num
        //  cps->begin_addr_offset
        //  cps->target_addr_offset
        //  cps->sp_offset
        //  cps->cell_num (= resultのcellの数)
        fout << "csp" << std::endl;
        std::cout << "[DEBUG] CtrlStack.size(): " << CtrlStack.size() << std::endl;
        for (uint32_t I = 0; I < CtrlStack.size(); I++) {
          struct CtrlInfo info = CtrlStack[I];
          const AST::Instruction &Instr = *info.Iter;
          const AST::InstrView::iterator BeginAddr = info.Iter;
          const AST::InstrView::iterator TargetAddr = BeginAddr + Instr.getJump().PCOffset; // TODO: TargetAddrの位置をちゃんと調べる
          uint32_t sp_offset = WamrCellSumStack[Instr.getJump().StackEraseBegin]; 
          uint32_t result_cells = WamrCellSumStack[Instr.getJump().StackEraseEnd];

          // cps->begin_addr_offset
          struct SourceLoc Begin = IterMigrator[BeginAddr];
          fout << Begin.Offset << std::endl;
          std::cout << "[DEBUG] Begin.Offset: " << Begin.Offset << std::endl;

          // cps->target_addr_offset
          struct SourceLoc Target = IterMigrator[TargetAddr];
          fout << Target.Offset << std::endl;
          std::cout << "[DEBUG] Target.Offset: " << Target.Offset << std::endl;
          
          // csp->sp_offset
          fout << sp_offset << std::endl;
          std::cout << "[DEBUG] sp_offset: " << sp_offset << std::endl;
          
          // csp->cell_num
          fout << result_cells << std::endl;
          std::cout << "[DEBUG] result_cells: " << result_cells << std::endl;
        }
        fout << std::endl;
        std::cout << "[DEBUG] " << I+1 << ") csp" << std::endl << std::endl;
      }
    }

    /// Addr.img
    fout << "addr.img" << std::endl;
    struct CtrlInfo CtrlTop = LastCtrlStack.back();
    const AST::Instruction& Instr = *(CtrlTop.Iter);

    // else_addr
    AST::InstrView::iterator ElseAddr = CtrlTop.Iter + Instr.getJumpElse();
    auto Data = IterMigrator[ElseAddr];
    fout << Data.Offset << std::endl;

    // end_addr
    AST::InstrView::iterator EndAddr = CtrlTop.Iter + Instr.getJumpEnd();
    Data = IterMigrator[EndAddr];
    fout << Data.Offset << std::endl;

    // done flag
    fout << 1 << std::endl;

    fout.close();
  }

  /// ================
  /// Dump functions
  /// ================
  /// TODO: 関数名を中身にあったものにrenameする
  void preDumpIter(const Runtime::Instance::ModuleInstance* ModInst) {
    setIterMigrator(ModInst);
    BaseModName = ModInst->getModuleName();
  }
  
  void dumpIter(AST::InstrView::iterator Iter, std::string fname_header = "") {
    IterMigratorType IterMigrator = getIterMigratorByName(BaseModName);
    assert(IterMigrator);

    struct SourceLoc Data = IterMigrator[Iter];
    std::ofstream iterStream;
    iterStream.open(fname_header + "iter.img", std::ios::trunc);

    debugFuncOpcode(Data.FuncIdx, Data.Offset, Iter);
    iterStream << Data.FuncIdx << std::endl;
    iterStream << Data.Offset;
      
    iterStream.close();
  }

  void dumpStackMgrFrame(Runtime::StackManager& StackMgr, std::string fname_header = "") {
    std::vector<Runtime::StackManager::Frame> FrameStack = StackMgr.getFrameStack();
    std::ofstream FrameStream;
    FrameStream.open(fname_header + "stackmgr_frame.img", std::ios::trunc);

    std::map<std::string_view, bool> seenModInst;
    for (size_t I = 0; I < FrameStack.size(); ++I) {
      Runtime::StackManager::Frame f = FrameStack[I];

      // ModuleInstance
      const Runtime::Instance::ModuleInstance* ModInst = f.Module;

      // ModInstがnullの場合は、ModNameだけ出力して、continue
      if (ModInst == nullptr) {
        FrameStream << NULL_MOD_NAME << std::endl;
        FrameStream << std::endl; 
        continue; 
      }

      std::string_view ModName = ModInst->getModuleName();
      FrameStream << ModName << std::endl;

      // まだそのModInstを保存してなければ、dumpする
      if(!seenModInst[ModName]) {
        ModInst->dumpMemInst(fname_header + std::string(ModName));
        ModInst->dumpGlobInst(fname_header + std::string(ModName));
        seenModInst[ModName] = true;
      }
      
      // Iterator
      IterMigratorType IterMigrator = getIterMigrator(ModInst);
      struct SourceLoc Data = IterMigrator[const_cast<AST::InstrView::iterator>(f.From)];
      debugFuncOpcode(Data.FuncIdx, Data.Offset, f.From);

      FrameStream << Data.FuncIdx << std::endl;
      FrameStream << Data.Offset << std::endl;

      // Locals, VPos, Arity
      FrameStream << f.Locals << std::endl;
      FrameStream << f.VPos << std::endl;
      FrameStream << f.Arity << std::endl;
      FrameStream << std::endl; 
    }  
    
    FrameStream.close();
  }
  
  void dumpStackMgrValue(Runtime::StackManager& StackMgr, std::string fname_header = "") {
    std::ofstream ValueStream;
    ValueStream.open(fname_header + "stackmgr_value.img", std::ios::trunc);

    using Value = ValVariant;
    std::vector<Value> ValueStack = StackMgr.getValueStack();
    for (size_t I = 0; I < ValueStack.size(); ++I) {
      Value v = ValueStack[I];
      ValueStream << v.get<uint128_t>() << std::endl;
    }
    
    ValueStream.close();
  }
  
  /// ================
  /// Restore functions
  /// ================
  Expect<AST::InstrView::iterator> _restoreIter(const Runtime::Instance::ModuleInstance* ModInst, uint32_t FuncIdx, uint32_t Offset) {
    assert(ModInst != nullptr);
    
    auto Res = ModInst->getFunc(FuncIdx);
    if (unlikely(!Res)) {
      // spdlog::error(ErrInfo::InfoAST(ASTNodeAttr::Seg_Element));
      std::cout << "\x1b[31m";
      std::cout << "ERROR: _restoreIter" << std::endl;
      std::cout << "\x1b[1m";
      return Unexpect(Res);
    }
    Runtime::Instance::FunctionInstance* FuncInst = Res.value();
    assert(FuncInst != nullptr);

    AST::InstrView::iterator Iter = FuncInst->getInstrs().begin();
    assert(Iter != nullptr);

    for (uint32_t I = 0; I < Offset; ++I) {
      Iter++;
    }
    return Iter;
  }

  Expect<AST::InstrView::iterator> restoreIter(const Runtime::Instance::ModuleInstance* ModInst) {
    std::ifstream iterStream;
    iterStream.open("iter.img");
    
    std::string iterString;
    uint32_t FuncIdx, Offset;
    // FuncIdx
    getline(iterStream, iterString);
    try {
      FuncIdx = static_cast<uint32_t>(std::stoul(iterString));
    } catch (const std::invalid_argument& e) {
      std::cout << "\x1b[31m";
      std::cout << "FuncIdx[" << iterString << "]: invalid argument" << std::endl;
      std::cout << "\x1b[m";
      // return Unexpect(ErrCode::Value::UserDefError);
    } catch (const std::out_of_range& e) {
      std::cout << "\x1b[31m";
      std::cout << "FuncIdx[" << iterString << "]: out of range" << std::endl;
      std::cout << "\x1b[m";
      // return Unexpect(e);
    }
    // Offset
    getline(iterStream, iterString);
    try {
      Offset = static_cast<uint32_t>(std::stoul(iterString));
    } catch (const std::invalid_argument& e) {
      std::cout << "\x1b[31m";
      std::cout << "Offset[" << iterString << "]: invalid argument" << std::endl;
      std::cout << "\x1b[m";
      // return Unexpect(e);
    } catch (const std::out_of_range& e) {
      std::cout << "\x1b[31m";
      std::cout << "Offset[" << iterString << "]: out of range" << std::endl;
      std::cout << "\x1b[m";
      // return Unexpect(e);
    }

    iterStream.close();

    // std::cout << FuncIdx << " " << Offset << std::endl;
    
    // FuncIdxとOffsetからitertorを復元
    auto Res = _restoreIter(ModInst, FuncIdx, Offset);
    if (!Res) {
      return Unexpect(Res);
    }

    auto Iter = Res.value();
    return Iter;
  }
  
  Expect<std::vector<Runtime::StackManager::Frame>> restoreStackMgrFrame() {
    std::ifstream FrameStream;
    FrameStream.open("stackmgr_frame.img");
    Runtime::StackManager StackMgr;

    std::vector<Runtime::StackManager::Frame> FrameStack;
    FrameStack.reserve(16U);
    std::string FrameString;
    /// TODO: ループ条件見直す
    std::map<std::string, const Runtime::Instance::ModuleInstance*> ModCache;
    while(getline(FrameStream, FrameString)) {
      // ModuleInstance
      std::string ModName = FrameString;
      const Runtime::Instance::ModuleInstance* ModInst;
      // std::cout << "restore frame: 1" << std::endl;

      // ModInstがnullの場合
      if (ModName == NULL_MOD_NAME) {
        Runtime::StackManager::Frame f(nullptr, nullptr, 0, 0, 0);
        FrameStack.push_back(f);

        // 次の行がなければ終了
        if(!getline(FrameStream, FrameString)) {
          break;
        }
        continue;
      }

      // ModInstがnullじゃない場合
      ModInst = findModule(ModName);
      // std::cout << "restored ModName is " << ModName << std::endl;
      if (ModInst == nullptr) {
        // std::cout << "ModInst is nullptr" << std::endl;
        assert(-1);
      }
      // std::cout << "restore frame: 2" << std::endl;

      /// TODO: 同じModuleの復元をしないよう、キャッシュを作る
      if (ModCache.count(ModName) == 0) {
        // std::cout << ModInst->getMemoryNum() << std::endl;
        ModInst->restoreMemInst(std::string(ModName));
        // std::cout << "Success restore meminst" << std::endl;
        ModInst->restoreGlobInst(std::string(ModName));
        // std::cout << "Success restore globinst" << std::endl;
        ModCache[ModName] = ModInst;
      }
      else {
        ModInst = ModCache[ModName];
      }
      // std::cout << "restore frame: 3" << std::endl;

      // Iterator
      getline(FrameStream, FrameString);
      uint32_t FuncIdx = static_cast<uint32_t>(std::stoul(FrameString));
      getline(FrameStream, FrameString);
      uint32_t Offset = static_cast<uint32_t>(std::stoul(FrameString));
      auto Res = _restoreIter(ModInst, FuncIdx, Offset);
      if (!Res) {
        return Unexpect(Res);
      }
      AST::InstrView::iterator From = Res.value();
      // AST::InstrView::iterator From = _restoreIter(ModInst, FuncIdx, Offset).value();
      // std::cout << "restore frame: 4" << std::endl;

      // Locals, VPos, Arity
      getline(FrameStream, FrameString);
      uint32_t Locals = static_cast<uint32_t>(std::stoul(FrameString));
      getline(FrameStream, FrameString);
      uint32_t VPos = static_cast<uint32_t>(std::stoul(FrameString));
      getline(FrameStream, FrameString);
      uint32_t Arity = static_cast<uint32_t>(std::stoul(FrameString));
      // std::cout << "restore frame: 5" << std::endl;

      Runtime::StackManager::Frame f(ModInst, From, Locals, Arity, VPos);
      FrameStack.push_back(f);

      // 空の行を読み捨て
      getline(FrameStream, FrameString);
    }

    FrameStream.close();
    return FrameStack;
  }
  
  Expect<std::vector<Runtime::StackManager::Value>> restoreStackMgrValue() {	  // Runtime::StackManager restoreStackMgr() {
    std::ifstream ValueStream;	  // }
    ValueStream.open("stackmgr_value.img");	
    Runtime::StackManager StackMgr;	

    std::vector<Runtime::StackManager::Value> ValueStack;	
    ValueStack.reserve(2048U);
    std::string ValueString;	
    /// TODO: ループ条件見直す	
    // int cnt = 0;
    while(getline(ValueStream, ValueString)) {	
      // ValueStringが空の場合はエラー	
      assert(ValueString.size() > 0);	

      // std::cout << "count " << cnt << std::endl;
      // cnt++;
      // std::cout << "restore value: 1" << std::endl;
      /// TODO: stoullは64bitまでしか受け取らないので、128bitの入力が来たら壊れる
      Runtime::StackManager::Value v = static_cast<uint128_t>(stou128(ValueString));	
      ValueStack.push_back(v);	
      // std::cout << "restore value: 2" << std::endl;
    }	

    ValueStream.close();	
    return ValueStack;    	
  }

  Expect<Runtime::StackManager> restoreStackMgr() {
    // auto Res = restoreStackMgrFrame().value;
    // if (!Res) {
    //   // return Unexpect(Res);
    // }
    std::vector<Runtime::StackManager::Frame> fs = restoreStackMgrFrame().value();
    std::cout << "Success to restore stack frame" << std::endl;

    // Res = restoreStackMgrValue();
    // if (!Res) {
    //   return Unexpect(Res);
    // }
    std::vector<Runtime::StackManager::Value> vs = restoreStackMgrValue().value();
    std::cout << "Success to restore stack value" << std::endl;

    Runtime::StackManager StackMgr;
    StackMgr.setFrameStack(fs);
    StackMgr.setValueStack(vs);
    std::cout << "Success to restore stack manager" << std::endl;

    return StackMgr;
  }

  bool DumpFlag; 
  bool RestoreFlag;
private:
  friend class Executor;

  std::map<std::string, IterMigratorType> IterMigrators;
  std::string BaseModName;
  /// \name Module name mapping.
  std::map<std::string, const Runtime::Instance::ModuleInstance *, std::less<>> NamedMod;
};

} // namespace Runtime
} // namespace WasmEdge
