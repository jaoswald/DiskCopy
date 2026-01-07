# https://bazel.build declarations for building DiskCopy

cc_library(
    name = "endian_lib",
    srcs = [],
    hdrs = ["endian.h"]);

cc_test(
    name = "endian_test",
    srcs = ["endian_test.cc"],
    deps = [":endian_lib",
            "@googletest//:gtest_main"])

cc_library(
    name = "disk_copy_lib",
    srcs = ["disk_copy.cc"],
    hdrs = ["disk_copy.h"],
    deps = [
        ":endian_lib",
        "@abseil-cpp//absl/strings:strings",
        "@abseil-cpp//absl/status:statusor"])

cc_library(
    name = "hfs_basic_lib",
    srcs = ["hfs_basic.cc"],
    hdrs = ["hfs_basic.h"],
    deps = [
        ":endian_lib",
        "@abseil-cpp//absl/strings:strings",
        "@abseil-cpp//absl/status:statusor"])

cc_test(
    name = "disk_copy_test",
    srcs = ["disk_copy_test.cc"],
    deps = [":disk_copy_lib",
            "@googletest//:gtest_main"])

cc_binary(
    name = "disk_copy",
    srcs = ["disk_copy_main.cc"],
    deps = [":disk_copy_lib",
            "@abseil-cpp//absl/flags:flag",
            "@abseil-cpp//absl/flags:parse",
            "@abseil-cpp//absl/flags:usage",
            "@abseil-cpp//absl/strings:strings",
            "@abseil-cpp//absl/status:statusor"])
