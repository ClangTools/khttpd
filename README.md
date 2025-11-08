# khttpd - a simple http server,power by boost

## usage

1. install `bazel` or `bazelisk`
2. mkdir workspace and add following content to `MODULE.bazel`
   > ```MODULE.bazel
   > http_archive = use_repo_rule("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
   > bazel_dep(name = "platforms", version = "0.0.11")
   > bazel_dep(name = "rules_cc", version = "0.1.1")
   > 
   > bazel_dep(name = "fmt", version = "11.0.2")
   > bazel_dep(name = "googletest", version = "1.17.0")
   > bazel_dep(name = "sqlite3", version = "3.49.1")
   > bazel_dep(name = "boringssl", version = "0.20250415.0")
   > bazel_dep(name = "boost", version = "1.88.0.bcr.2")
   > bazel_dep(name = "boost.asio", version = "1.88.0.bcr.2")
   > bazel_dep(name = "boost.mysql", version = "1.88.0.bcr.2")
   > http_archive(
   >   name = "khttpd",
   >   integrity = "sha256-xrIHeqD3rtrxomQsHd0iWc4eKvlpjyOjW5dWOi/tEYc=",
   >   strip_prefix = "khttpd-0.0.1",
   >   url = "https://github.com/ClangTools/khttpd/archive/refs/tags/v0.0.1.tar.gz",
   > )
   > ```
3. create `BUILD.bazel`,like [BUILD.bazel](example/BUILD.bazel)
4. create you code like [example](example)
5. run `bazel build //:your_target_name` to compile
6. run `bazel run //:your_target_name` to execute
