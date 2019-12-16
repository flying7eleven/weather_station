#!/bin/bash
find ./include \( -name "*.cpp" -o -name "*.hpp" -o -name "*.cxx.in" -o -name "*.cxx" -o -name "*.hxx" -o -name "*.c" -o -name "*.h" -o -name "*.hxx.in" -o -name "*.ino" \) -exec clang-format -style=file -i {} \;
find ./src \( -name "*.cpp" -o -name "*.hpp" -o -name "*.cxx.in" -o -name "*.cxx" -o -name "*.hxx" -o -name "*.c" -o -name "*.h" -o -name "*.hxx.in" -o -name "*.ino" \) -exec clang-format -style=file -i {} \;
