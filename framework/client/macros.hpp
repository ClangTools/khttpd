#ifndef KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP
#define KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP

#include <string>
#include <tuple>
#include <map>
#include <boost/beast/http/verb.hpp>
#include <dto/TagInvoke.hpp>

// =========================================================================
// Compiler Warning Suppression
// =========================================================================
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wpedantic"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

// =========================================================================
// MSVC Compatibility Helper (关键修复 1)
// =========================================================================
// 用于强制 MSVC 展开 __VA_ARGS__
#define EXPAND(x) x

// =========================================================================
// Argument Tags
// =========================================================================
#define QUERY(Type, Name, Key) (QUERY_TAG, Type, Name, Key)
#define PATH(Type, Name)       (PATH_TAG, Type, Name)
#define BODY(Type, Name)       (BODY_TAG, Type, Name)
#define HEADER(Type, Name, Key) (HEADER_TAG, Type, Name, Key)

// =========================================================================
// Dispatching Logic (关键修复 2：简化解包逻辑)
// =========================================================================

// 之前的 POP_TAG 方式在 MSVC 上容易出错。
// 我们改为直接展开 Tuple：
// SIG_DISPATCH((TAG, Type, Name)) -> SIG_DISPATCH_I(TAG, Type, Name) -> SIG_TAG(Type, Name)

#define SIG_DISPATCH(Tuple) EXPAND(SIG_DISPATCH_I Tuple)
#define SIG_DISPATCH_I(Tag, ...) EXPAND(SIG_##Tag(__VA_ARGS__))

#define PROC_DISPATCH(Tuple) EXPAND(PROC_DISPATCH_I Tuple)
#define PROC_DISPATCH_I(Tag, ...) EXPAND(PROC_##Tag(__VA_ARGS__))

// =========================================================================
// Implementation Logic
// =========================================================================
// Signature Generation
#define SIG_QUERY_TAG(Type, Name, Key) Type Name
#define SIG_PATH_TAG(Type, Name) Type Name
#define SIG_BODY_TAG(Type, Name) Type Name
#define SIG_HEADER_TAG(Type, Name, Key) Type Name

// Process Logic
#define PROC_QUERY_TAG(Type, Name, Key) query_params.emplace(Key, khttpd::framework::client::to_string(Name))
#define PROC_PATH_TAG(Type, Name) path_str = khttpd::framework::client::replace_all(path_str, ":" #Name, khttpd::framework::client::to_string(Name))
#define PROC_BODY_TAG(Type, Name) body_str = khttpd::framework::client::serialize_body(Name)
#define PROC_HEADER_TAG(Type, Name, Key) header_map.emplace(Key, khttpd::framework::client::to_string(Name))

// =========================================================================
// API Function Body Generators
// =========================================================================

#define API_FUNC_BODY(METHOD, PATH_TEMPLATE, ...) \
    std::string path_str = PATH_TEMPLATE; \
    std::map<std::string, std::string> query_params; \
    std::map<std::string, std::string> header_map; \
    std::string body_str; \
    __VA_ARGS__ \
    this->request(METHOD, path_str, query_params, body_str, header_map, std::move(callback));

#define API_FUNC_BODY_SYNC(METHOD, PATH_TEMPLATE, ...) \
    std::string path_str = PATH_TEMPLATE; \
    std::map<std::string, std::string> query_params; \
    std::map<std::string, std::string> header_map; \
    std::string body_str; \
    __VA_ARGS__ \
    return this->request_sync(METHOD, path_str, query_params, body_str, header_map);

// =========================================================================
// N-Argument Macro Implementations
// =========================================================================

#define API_CALL_0(METHOD, PT, NAME) \
    void NAME(khttpd::framework::client::HttpClient::ResponseCallback callback) { \
        API_FUNC_BODY(METHOD, PT, ) \
    } \
    boost::beast::http::response<boost::beast::http::string_body> NAME##_sync() { \
        API_FUNC_BODY_SYNC(METHOD, PT, ) \
    }

#define API_CALL_1(METHOD, PT, NAME, A) \
    void NAME(SIG_DISPATCH(A), khttpd::framework::client::HttpClient::ResponseCallback callback) { \
        API_FUNC_BODY(METHOD, PT, PROC_DISPATCH(A);) \
    } \
    auto NAME##_sync(SIG_DISPATCH(A)) { \
        API_FUNC_BODY_SYNC(METHOD, PT, PROC_DISPATCH(A);) \
    }

#define API_CALL_2(METHOD, PT, NAME, A, B) \
    void NAME(SIG_DISPATCH(A), SIG_DISPATCH(B), khttpd::framework::client::HttpClient::ResponseCallback callback) { \
        API_FUNC_BODY(METHOD, PT, PROC_DISPATCH(A); PROC_DISPATCH(B);) \
    } \
    auto NAME##_sync(SIG_DISPATCH(A), SIG_DISPATCH(B)) { \
        API_FUNC_BODY_SYNC(METHOD, PT, PROC_DISPATCH(A); PROC_DISPATCH(B);) \
    }

#define API_CALL_3(METHOD, PT, NAME, A, B, C) \
    void NAME(SIG_DISPATCH(A), SIG_DISPATCH(B), SIG_DISPATCH(C), khttpd::framework::client::HttpClient::ResponseCallback callback) { \
        API_FUNC_BODY(METHOD, PT, PROC_DISPATCH(A); PROC_DISPATCH(B); PROC_DISPATCH(C);) \
    } \
    auto NAME##_sync(SIG_DISPATCH(A), SIG_DISPATCH(B), SIG_DISPATCH(C)) { \
        API_FUNC_BODY_SYNC(METHOD, PT, PROC_DISPATCH(A); PROC_DISPATCH(B); PROC_DISPATCH(C);) \
    }

#define API_CALL_4(METHOD, PT, NAME, A, B, C, D) \
    void NAME(SIG_DISPATCH(A), SIG_DISPATCH(B), SIG_DISPATCH(C), SIG_DISPATCH(D), khttpd::framework::client::HttpClient::ResponseCallback callback) { \
        API_FUNC_BODY(METHOD, PT, PROC_DISPATCH(A); PROC_DISPATCH(B); PROC_DISPATCH(C); PROC_DISPATCH(D);) \
    } \
    auto NAME##_sync(SIG_DISPATCH(A), SIG_DISPATCH(B), SIG_DISPATCH(C), SIG_DISPATCH(D)) { \
        API_FUNC_BODY_SYNC(METHOD, PT, PROC_DISPATCH(A); PROC_DISPATCH(B); PROC_DISPATCH(C); PROC_DISPATCH(D);) \
    }

// =========================================================================
// Dispatcher Logic (关键修复 3：修正宏选择计数)
// =========================================================================

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME

// 在这里使用 EXPAND 包裹整个 GET_MACRO 调用。
// 这解决了 "not enough arguments" 警告，并确保正确选择 API_CALL_x 宏。
#define API_CALL(...) EXPAND(GET_MACRO(__VA_ARGS__, API_CALL_4, API_CALL_3, API_CALL_2, API_CALL_1, API_CALL_0, DUMMY)(__VA_ARGS__))

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP
