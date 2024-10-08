# Generated by devtools/yamaker.

PROGRAM()

VERSION(16.0.0)

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/clang16
    contrib/libs/clang16/lib/Basic
    contrib/libs/clang16/lib/Driver
    contrib/libs/llvm16
    contrib/libs/llvm16/lib/Support
)

ADDINCL(
    contrib/libs/clang16/tools/clang-offload-bundler
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    ClangOffloadBundler.cpp
)

END()
