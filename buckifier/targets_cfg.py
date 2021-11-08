# Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved.
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

rocksdb_target_header_template = \
    """# This file \100generated by:
#$ python3 buckifier/buckify_rocksdb.py{extra_argv}
# --> DO NOT EDIT MANUALLY <--
# This file is a Facebook-specific integration for buck builds, so can
# only be validated by Facebook employees.
#
load("@fbcode_macros//build_defs:auto_headers.bzl", "AutoHeaders")
load("@fbcode_macros//build_defs:cpp_library.bzl", "cpp_library")
load(":defs.bzl", "test_binary")

REPO_PATH = package_name() + "/"

ROCKSDB_COMPILER_FLAGS_0 = [
    "-fno-builtin-memcmp",
    # Needed to compile in fbcode
    "-Wno-expansion-to-defined",
    # Added missing flags from output of build_detect_platform
    "-Wnarrowing",
    "-DROCKSDB_NO_DYNAMIC_EXTENSION",
]

ROCKSDB_EXTERNAL_DEPS = [
    ("bzip2", None, "bz2"),
    ("snappy", None, "snappy"),
    ("zlib", None, "z"),
    ("gflags", None, "gflags"),
    ("lz4", None, "lz4"),
    ("zstd", None, "zstd"),
]

ROCKSDB_OS_DEPS_0 = [
    (
        "linux",
        [
            "third-party//numa:numa",
            "third-party//liburing:uring",
            "third-party//tbb:tbb",
        ],
    ),
    (
        "macos",
        ["third-party//tbb:tbb"],
    ),
]

ROCKSDB_OS_PREPROCESSOR_FLAGS_0 = [
    (
        "linux",
        [
            "-DOS_LINUX",
            "-DROCKSDB_FALLOCATE_PRESENT",
            "-DROCKSDB_MALLOC_USABLE_SIZE",
            "-DROCKSDB_PTHREAD_ADAPTIVE_MUTEX",
            "-DROCKSDB_RANGESYNC_PRESENT",
            "-DROCKSDB_SCHED_GETCPU_PRESENT",
            "-DROCKSDB_IOURING_PRESENT",
            "-DHAVE_SSE42",
            "-DLIBURING",
            "-DNUMA",
            "-DROCKSDB_PLATFORM_POSIX",
            "-DROCKSDB_LIB_IO_POSIX",
            "-DTBB",
        ],
    ),
    (
        "macos",
        [
            "-DOS_MACOSX",
            "-DROCKSDB_PLATFORM_POSIX",
            "-DROCKSDB_LIB_IO_POSIX",
            "-DTBB",
        ],
    ),
    (
        "windows",
        [
            "-DOS_WIN",
            "-DWIN32",
            "-D_MBCS",
            "-DWIN64",
            "-DNOMINMAX",
        ],
    ),
]

ROCKSDB_PREPROCESSOR_FLAGS = [
    "-DROCKSDB_SUPPORT_THREAD_LOCAL",

    # Flags to enable libs we include
    "-DSNAPPY",
    "-DZLIB",
    "-DBZIP2",
    "-DLZ4",
    "-DZSTD",
    "-DZSTD_STATIC_LINKING_ONLY",
    "-DGFLAGS=gflags",

    # Added missing flags from output of build_detect_platform
    "-DROCKSDB_BACKTRACE",
]

# Directories with files for #include
ROCKSDB_INCLUDE_PATHS = [
    "",
    "include",
]

ROCKSDB_ARCH_PREPROCESSOR_FLAGS = {{
    "x86_64": [
        "-DHAVE_PCLMUL",
    ],
}}

build_mode = read_config("fbcode", "build_mode")

is_opt_mode = build_mode.startswith("opt")

# -DNDEBUG is added by default in opt mode in fbcode. But adding it twice
# doesn't harm and avoid forgetting to add it.
ROCKSDB_COMPILER_FLAGS = ROCKSDB_COMPILER_FLAGS_0 + (["-DNDEBUG"] if is_opt_mode else [])

sanitizer = read_config("fbcode", "sanitizer")

# Do not enable jemalloc if sanitizer presents. RocksDB will further detect
# whether the binary is linked with jemalloc at runtime.
ROCKSDB_OS_PREPROCESSOR_FLAGS = ROCKSDB_OS_PREPROCESSOR_FLAGS_0 + ([(
    "linux",
    ["-DROCKSDB_JEMALLOC"],
)] if sanitizer == "" else [])

ROCKSDB_OS_DEPS = ROCKSDB_OS_DEPS_0 + ([(
    "linux",
    ["third-party//jemalloc:headers"],
)] if sanitizer == "" else [])

ROCKSDB_LIB_DEPS = [
    ":rocksdb_lib",
    ":rocksdb_test_lib",
] if not is_opt_mode else [":rocksdb_lib"]
"""


library_template = """
cpp_library(
    name = "{name}",
    srcs = [{srcs}],
    {headers_attr_prefix}headers = {headers},
    arch_preprocessor_flags = ROCKSDB_ARCH_PREPROCESSOR_FLAGS,
    compiler_flags = ROCKSDB_COMPILER_FLAGS,
    include_paths = ROCKSDB_INCLUDE_PATHS,
    link_whole = {link_whole},
    os_deps = ROCKSDB_OS_DEPS,
    os_preprocessor_flags = ROCKSDB_OS_PREPROCESSOR_FLAGS,
    preprocessor_flags = ROCKSDB_PREPROCESSOR_FLAGS,
    unexported_deps_by_default = False,
    deps = [{deps}],
    external_deps = ROCKSDB_EXTERNAL_DEPS{extra_external_deps},
)
"""

rocksdb_library_template = """
cpp_library(
    name = "{name}",
    srcs = [{srcs}],
    {headers_attr_prefix}headers = {headers},
    arch_preprocessor_flags = ROCKSDB_ARCH_PREPROCESSOR_FLAGS,
    compiler_flags = ROCKSDB_COMPILER_FLAGS,
    include_paths = ROCKSDB_INCLUDE_PATHS,
    os_deps = ROCKSDB_OS_DEPS,
    os_preprocessor_flags = ROCKSDB_OS_PREPROCESSOR_FLAGS,
    preprocessor_flags = ROCKSDB_PREPROCESSOR_FLAGS,
    unexported_deps_by_default = False,
    deps = ROCKSDB_LIB_DEPS,
    external_deps = ROCKSDB_EXTERNAL_DEPS,
)
"""

binary_template = """
cpp_binary(
    name = "{name}",
    srcs = [{srcs}],
    arch_preprocessor_flags = ROCKSDB_ARCH_PREPROCESSOR_FLAGS,
    compiler_flags = ROCKSDB_COMPILER_FLAGS,
    preprocessor_flags = ROCKSDB_PREPROCESSOR_FLAGS,
    include_paths = ROCKSDB_INCLUDE_PATHS,
    deps = [{deps}],
    external_deps = ROCKSDB_EXTERNAL_DEPS,
)
"""

test_cfg_template = """    [
        "%s",
        "%s",
        "%s",
        %s,
        %s,
    ],
"""

unittests_template = """
# [test_name, test_src, test_type, extra_deps, extra_compiler_flags]
ROCKS_TESTS = [
{tests}]

# Generate a test rule for each entry in ROCKS_TESTS
# Do not build the tests in opt mode, since SyncPoint and other test code
# will not be included.
[
    cpp_unittest(
        name = test_name,
        srcs = [test_cc],
        arch_preprocessor_flags = ROCKSDB_ARCH_PREPROCESSOR_FLAGS,
        compiler_flags = ROCKSDB_COMPILER_FLAGS + extra_compiler_flags,
        include_paths = ROCKSDB_INCLUDE_PATHS,
        os_preprocessor_flags = ROCKSDB_OS_PREPROCESSOR_FLAGS,
        preprocessor_flags = ROCKSDB_PREPROCESSOR_FLAGS,
        deps = [":rocksdb_test_lib"] + extra_deps,
        external_deps = ROCKSDB_EXTERNAL_DEPS + [
            ("googletest", None, "gtest"),
        ],
    )
    for test_name, test_cc, parallelism, extra_deps, extra_compiler_flags in ROCKS_TESTS
    if not is_opt_mode
]
"""
