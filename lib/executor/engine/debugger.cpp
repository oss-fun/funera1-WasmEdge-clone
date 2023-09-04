#include "executor/executor.h"


namespace WasmEdge {
namespace Executor {

// struct SourceLoc {
//   SourceLoc() {
//     FuncIdx = 0;
//     Offset = 0;
//   };

//   uint32_t FuncIdx;
//   uint32_t Offset;
// };

std::vector<std::string> Split(std::string &s, char delim) {
  std::vector<std::string> elems;
  std::string item;
  for (char ch : s) {
    if (ch == delim) {
      if (!item.empty()) elems.push_back(item);
      item.clear();
    }
    else {
      item += ch;
    }
  }
  if (!item.empty()) elems.push_back(item);

  return elems;
}

void Help() {
    std::cout << "\033[1m" "break" " -- Making program stop at cerain point" "\x1b[0m" << std::endl;
}

void InteractiveMode(SourceLoc &bp) {
  std::string command;
  while(1) {
    std::cout << "\x1b[31m" << "wdb$ " << "\x1b[0m";

    getline(std::cin, command);
    std::vector<std::string> commands = Split(command, ' ');

    // break funcidx offset
    if (commands[0] == "break") {
      if (commands.size() != 3) {
        Help();
        continue;
      }
      bp.FuncIdx = stoi(commands[1]);
      bp.Offset = stoi(commands[2]);
    }
    else if (commands[0] == "run") {
        return;
    }
    // Info commands
    else if (commands[0] == "info") {
      // printStack();
    }
    else {
        Help();
    }
  }
  return;
}



// void printStack() {

// }

}
}