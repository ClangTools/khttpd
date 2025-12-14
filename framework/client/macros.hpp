#ifndef KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP
#define KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP

#include <string>
#include <tuple>
#include <boost/beast/http/verb.hpp>

// Suppress warnings for variadic macro extensions (standard in C++20/GNU but we are on C++17 pedantic)
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
// Argument Tags and Tuples
// =========================================================================
#define QUERY(Type, Name, Key) (QUERY_TAG, Type, Name, Key)
#define PATH(Type, Name)       (PATH_TAG, Type, Name)
#define BODY(Type, Name)       (BODY_TAG, Type, Name)
#define HEADER(Type, Name, Key) (HEADER_TAG, Type, Name, Key)

// =========================================================================
// Tuple Unpacking
// =========================================================================

#define EXPAND(x) x

#define GET_TAG(Tuple) GET_TAG_I Tuple
#define GET_TAG_I(Tag, ...) Tag

#define POP_TAG(Tuple) POP_TAG_I Tuple
#define POP_TAG_I(Tag, ...) __VA_ARGS__

// =========================================================================
// Dispatchers
// =========================================================================

#define INVOKE(MACRO, ...) MACRO(__VA_ARGS__)

// SIG_DISPATCH(Tuple) -> SIG_TAG(...)
// Indirection to ensure Tag is expanded before concatenation
#define SIG_DISPATCH(Tuple) SIG_DISPATCH_I(GET_TAG(Tuple), Tuple)
#define SIG_DISPATCH_I(Tag, Tuple) SIG_DISPATCH_II(Tag, Tuple)
#define SIG_DISPATCH_II(Tag, Tuple) INVOKE(SIG_##Tag, POP_TAG(Tuple))

// PROC_DISPATCH(Tuple) -> PROC_TAG(...)
#define PROC_DISPATCH(Tuple) PROC_DISPATCH_I(GET_TAG(Tuple), Tuple)
#define PROC_DISPATCH_I(Tag, Tuple) PROC_DISPATCH_II(Tag, Tuple)
#define PROC_DISPATCH_II(Tag, Tuple) INVOKE(PROC_##Tag, POP_TAG(Tuple))

// =========================================================================
// Implementation of SIG_... (Signature Generation)
// =========================================================================
#define SIG_QUERY_TAG(Type, Name, Key) Type Name
#define SIG_PATH_TAG(Type, Name) Type Name
#define SIG_BODY_TAG(Type, Name) Type Name
#define SIG_HEADER_TAG(Type, Name, Key) Type Name

// =========================================================================
// Implementation of PROC_... (Process Logic Generation)
// =========================================================================
#define PROC_QUERY_TAG(Type, Name, Key) query_params.emplace(Key, khttpd::framework::client::to_string(Name))
#define PROC_PATH_TAG(Type, Name) path_str = khttpd::framework::client::replace_all(path_str, ":" #Name, khttpd::framework::client::to_string(Name))
#define PROC_BODY_TAG(Type, Name) body_str = khttpd::framework::client::serialize_body(Name)
#define PROC_HEADER_TAG(Type, Name, Key) header_map.emplace(Key, khttpd::framework::client::to_string(Name))

// =========================================================================
// API_CALL_N Implementations
// =========================================================================

#define API_CALL_0(METHOD, PATH_TEMPLATE, NAME) \
    void NAME(khttpd::framework::client::HttpClient::ResponseCallback callback) \
    { \
        std::string path_str = PATH_TEMPLATE; \
        std::map<std::string, std::string> query_params; \
        std::map<std::string, std::string> header_map; \
        std::string body_str; \
        this->request(METHOD, path_str, query_params, body_str, header_map, std::move(callback)); \
    } \
    boost::beast::http::response<boost::beast::http::string_body> NAME##_sync() \
    { \
        std::string path_str = PATH_TEMPLATE; \
        std::map<std::string, std::string> query_params; \
        std::map<std::string, std::string> header_map; \
        std::string body_str; \
        return this->request_sync(METHOD, path_str, query_params, body_str, header_map); \
    }

#define API_CALL_1(METHOD, PATH_TEMPLATE, NAME, ARG1) \
    void NAME(SIG_DISPATCH(ARG1), khttpd::framework::client::HttpClient::ResponseCallback callback) \
    { \
        std::string path_str = PATH_TEMPLATE; \
        std::map<std::string, std::string> query_params; \
        std::map<std::string, std::string> header_map; \
        std::string body_str; \
        PROC_DISPATCH(ARG1); \
        this->request(METHOD, path_str, query_params, body_str, header_map, std::move(callback)); \
    } \
    boost::beast::http::response<boost::beast::http::string_body> NAME##_sync(SIG_DISPATCH(ARG1)) \
    { \
        std::string path_str = PATH_TEMPLATE; \
        std::map<std::string, std::string> query_params; \
        std::map<std::string, std::string> header_map; \
        std::string body_str; \
        PROC_DISPATCH(ARG1); \
        return this->request_sync(METHOD, path_str, query_params, body_str, header_map); \
    }

#define API_CALL_2(METHOD, PATH_TEMPLATE, NAME, ARG1, ARG2) \
    void NAME(SIG_DISPATCH(ARG1), SIG_DISPATCH(ARG2), khttpd::framework::client::HttpClient::ResponseCallback callback) \
    { \
        std::string path_str = PATH_TEMPLATE; \
        std::map<std::string, std::string> query_params; \
        std::map<std::string, std::string> header_map; \
        std::string body_str; \
        PROC_DISPATCH(ARG1); \
        PROC_DISPATCH(ARG2); \
        this->request(METHOD, path_str, query_params, body_str, header_map, std::move(callback)); \
    } \
    boost::beast::http::response<boost::beast::http::string_body> NAME##_sync(SIG_DISPATCH(ARG1), SIG_DISPATCH(ARG2)) \
    { \
        std::string path_str = PATH_TEMPLATE; \
        std::map<std::string, std::string> query_params; \
        std::map<std::string, std::string> header_map; \
        std::string body_str; \
        PROC_DISPATCH(ARG1); \
        PROC_DISPATCH(ARG2); \
        return this->request_sync(METHOD, path_str, query_params, body_str, header_map); \
    }

#define API_CALL_3(METHOD, PATH_TEMPLATE, NAME, ARG1, ARG2, ARG3) \
    void NAME(SIG_DISPATCH(ARG1), SIG_DISPATCH(ARG2), SIG_DISPATCH(ARG3), khttpd::framework::client::HttpClient::ResponseCallback callback) \
    { \
        std::string path_str = PATH_TEMPLATE; \
        std::map<std::string, std::string> query_params; \
        std::map<std::string, std::string> header_map; \
        std::string body_str; \
        PROC_DISPATCH(ARG1); \
        PROC_DISPATCH(ARG2); \
        PROC_DISPATCH(ARG3); \
        this->request(METHOD, path_str, query_params, body_str, header_map, std::move(callback)); \
    } \
    boost::beast::http::response<boost::beast::http::string_body> NAME##_sync(SIG_DISPATCH(ARG1), SIG_DISPATCH(ARG2), SIG_DISPATCH(ARG3)) \
    { \
        std::string path_str = PATH_TEMPLATE; \
        std::map<std::string, std::string> query_params; \
        std::map<std::string, std::string> header_map; \
        std::string body_str; \
        PROC_DISPATCH(ARG1); \
        PROC_DISPATCH(ARG2); \
        PROC_DISPATCH(ARG3); \
        return this->request_sync(METHOD, path_str, query_params, body_str, header_map); \
    }

#define GET_API_MACRO(_0, _1, _2, _3, NAME, ...) NAME
#define API_CALL(METHOD, PATH, NAME, ...) GET_API_MACRO(_0, ##__VA_ARGS__, API_CALL_3, API_CALL_2, API_CALL_1, API_CALL_0)(METHOD, PATH, NAME, ##__VA_ARGS__)

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // KHTTPD_FRAMEWORK_CLIENT_MACROS_HPP
