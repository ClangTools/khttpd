# khttpd - a simple http server,power by boost

## usage

1. install `bazel` or `bazelisk`
2. mkdir workspace and add following content to `MODULE.bazel`
   > ```MODULE.bazel
   > http_archive = use_repo_rule("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
   > bazel_dep(name = "platforms", version = "0.0.11")
   > bazel_dep(name = "rules_cc", version = "0.1.1")
   > http_archive(
   >   name = "khttpd",
   >   integrity = "sha256-Mi8LRIL8DZ+gu0aBNIQfCNjFVMVP9aopoTp6JL9+HrU=",
   >   strip_prefix = "arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu",
   >   type = "tar.xz",
   >   url = "https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz",
   > )
   > ```