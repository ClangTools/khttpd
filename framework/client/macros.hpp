#ifndef KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP
#define KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP

#include <string>
#include <tuple>
#include <boost/beast/http/verb.hpp>

// =========================================================================
// Compiler Warning Suppression
// =========================================================================
// 虽然我们修复了调度器的警告，但 API_CALL_0 传递空参数给具体实现宏时，
// 仍可能触发 GNU 扩展警告，保留这些 pragma 以确保兼容性。
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

#define INVOKE(MACRO, ...) MACRO(__VA_ARGS__)

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
// Dispatcher Logic (Corrected)
// =========================================================================

// 宏选择器：
// 我们定义 _1 到 _7 为被“消耗”的参数位。
// NAME 是我们真正想要选中的宏。
// ... 是剩余参数。
// 这里的关键是：必须保证调用 GET_MACRO 时，提供的参数数量使得 NAME 之后永远还有至少一个参数进入 ...
#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME

// 外部调用宏：
// 我们在参数列表末尾显式追加一个 DUMMY。
//
// 场景 1: API_CALL(M, P, N)  -> 3个参数
// 传入 GET_MACRO: M, P, N, CALL_4, CALL_3, CALL_2, CALL_1, CALL_0, DUMMY
// _1.._7 消耗了前7个 (M..CALL_1)
// NAME 命中了 CALL_0
// ... 捕获了 DUMMY (不为空，消除了警告)
//
// 场景 2: API_CALL(M, P, N, A) -> 4个参数
// 传入 GET_MACRO: M, P, N, A, CALL_4, CALL_3, CALL_2, CALL_1, CALL_0, DUMMY
// _1.._7 消耗了前7个 (M..CALL_2)
// NAME 命中了 CALL_1
// ... 捕获了 CALL_0, DUMMY (不为空)
#define API_CALL(...) GET_MACRO(__VA_ARGS__, API_CALL_4, API_CALL_3, API_CALL_2, API_CALL_1, API_CALL_0, DUMMY)(__VA_ARGS__)

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP