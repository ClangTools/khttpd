#ifndef KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP
#define KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP

#include <string>
#include <tuple>
#include <boost/beast/http/verb.hpp>

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
// MSVC Compatibility Helper (关键修复)
// =========================================================================
// MSVC 默认预处理器会将 __VA_ARGS__ 视为单个标记。
// 使用 EXPAND 宏可以强制其展开为多个参数。
#define EXPAND(x) x

// =========================================================================
// Argument Tags
// =========================================================================
#define QUERY(Type, Name, Key) (QUERY_TAG, Type, Name, Key)
#define PATH(Type, Name)       (PATH_TAG, Type, Name)
#define BODY(Type, Name)       (BODY_TAG, Type, Name)
#define HEADER(Type, Name, Key) (HEADER_TAG, Type, Name, Key)

// =========================================================================
// Tuple Unpacking & Dispatching
// =========================================================================
#define GET_TAG(Tuple) GET_TAG_I Tuple
#define GET_TAG_I(Tag, ...) Tag

#define POP_TAG(Tuple) POP_TAG_I Tuple
#define POP_TAG_I(Tag, ...) __VA_ARGS__

// 修复: 在 INVOKE 中使用 EXPAND，确保 POP_TAG 返回的参数在传递给具体宏之前被正确拆分
#define INVOKE(MACRO, ...) EXPAND(MACRO(__VA_ARGS__))

#define SIG_DISPATCH(Tuple) SIG_DISPATCH_I(GET_TAG(Tuple), Tuple)
#define SIG_DISPATCH_I(Tag, Tuple) SIG_DISPATCH_II(Tag, Tuple)
#define SIG_DISPATCH_II(Tag, Tuple) INVOKE(SIG_##Tag, POP_TAG(Tuple))

#define PROC_DISPATCH(Tuple) PROC_DISPATCH_I(GET_TAG(Tuple), Tuple)
#define PROC_DISPATCH_I(Tag, Tuple) PROC_DISPATCH_II(Tag, Tuple)
#define PROC_DISPATCH_II(Tag, Tuple) INVOKE(PROC_##Tag, POP_TAG(Tuple))

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
// Dispatcher Logic
// =========================================================================

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME

// 修复: 在最外层使用 EXPAND 包裹 GET_MACRO 调用
// 这样 MSVC 在传递参数给 GET_MACRO 之前，会先展开 __VA_ARGS__，
// 从而确保参数计数（_1 到 _7）正确，选中正确的 API_CALL_x 宏。
#define API_CALL(...) EXPAND(GET_MACRO(__VA_ARGS__, API_CALL_4, API_CALL_3, API_CALL_2, API_CALL_1, API_CALL_0, DUMMY)(__VA_ARGS__))

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP
