#!/bin/bash
find . \( -name "*.cxx.in" -o -name "*.cxx" -o -name "*.hxx" -o -name "*.c" -o -name "*.h" -o -name "*.hxx.in" -o -name "*.ino" \) -exec clang-format -style=file -i {} \;
