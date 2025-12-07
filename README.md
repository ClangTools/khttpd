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

## Exception Handling

khttpd supports robust exception handling mechanisms. You can catch specific exceptions or handle all exceptions using `ExceptionDispatcher`.

### Example

```cpp
#include "framework/server.hpp"
#include "framework/exception/exception_handler.hpp"

// ... inside your main function ...

// Create a dispatcher
auto dispatcher = std::make_shared<khttpd::framework::ExceptionDispatcher>();

// Register handler for integer exceptions (e.g., throw 404;)
dispatcher->on<int>([](const int& e, khttpd::framework::HttpContext& ctx) {
    ctx.set_status(boost::beast::http::status::internal_server_error);
    ctx.set_body("Internal Error Code: " + std::to_string(e));
});

// Register handler for standard exceptions
dispatcher->on<std::runtime_error>([](const std::runtime_error& e, khttpd::framework::HttpContext& ctx) {
    ctx.set_status(boost::beast::http::status::internal_server_error);
    ctx.set_body(std::string("Runtime Error: ") + e.what());
});

// Register handler for string literals
dispatcher->on<const char*>([](const char* const& e, khttpd::framework::HttpContext& ctx) {
    ctx.set_status(boost::beast::http::status::bad_request);
    ctx.set_body(std::string("Error: ") + e);
});

// Add the dispatcher to the router
server.get_http_router().add_exception_handler(dispatcher);
```