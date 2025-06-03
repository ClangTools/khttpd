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
   >   integrity = "sha256-xNVqLJWKZOaQk5cqht35l/6siz4yUrXvyRRHei0EYmU=",
   >   strip_prefix = "0.1.0",
   >   url = "https://github.com/ClangTools/khttpd/archive/refs/tags/0.1.0.tar.gz",
   > )
   > ```
3. create `BUILD.bazel`,like [BUILD.bazel](example/BUILD.bazel)
4. create you code like [example](example)
5. run `bazel build //:your_target_name` to compile
6. run `bazel run //:your_target_name` to execute
