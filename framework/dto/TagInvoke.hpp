//
// Created by Caesar on 2025/12/7.
//

#ifndef BOOST_TAGINVOKE_HPP
#define BOOST_TAGINVOKE_HPP

#include <boost/json.hpp>
#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <type_traits>

namespace boost::json
{
  namespace desc = boost::describe;
  namespace mp11 = boost::mp11;

  // =================================================================
  // 1. 通用序列化 (Struct -> JSON)
  //    C++17 改进: 使用 std::enable_if_t 简化语法
  // =================================================================
  template <class T>
  auto tag_invoke(value_from_tag, value& jv, T const& t)
    -> std::enable_if_t<desc::has_describe_members<T>::value>
  {
    auto& obj = jv.emplace_object();

    using Md = desc::describe_members<T, desc::mod_public>;

    mp11::mp_for_each<Md>([&](auto D)
    {
      // 使用 emplace 稍微高效一点，直接构造 value
      obj.emplace(D.name, value_from(t.*D.pointer));
    });
  }

  // =================================================================
  // 2. 通用反序列化 (JSON -> Struct)
  //    C++17 改进: 使用 std::enable_if_t 和 std::remove_reference_t
  // =================================================================
  template <class T>
  auto tag_invoke(value_to_tag<T>, value const& jv)
    -> std::enable_if_t<desc::has_describe_members<T>::value, T>
  {
    // 只有 T 是 DefaultConstructible 时才能这样写
    T t{};

    // 如果 JSON 不是 object，as_object() 会抛出异常，这是符合预期的行为
    auto const& obj = jv.as_object();

    using Md = desc::describe_members<T, desc::mod_public>;

    mp11::mp_for_each<Md>([&](auto D)
    {
      // 查找 key 是否存在
      if (auto it = obj.find(D.name); it != obj.end())
      {
        // C++17: 使用 remove_reference_t 简化类型获取
        using MemberT = std::remove_reference_t<decltype(t.*D.pointer)>;

        // 将 json value 转换为具体的成员类型并赋值
        t.*D.pointer = value_to<MemberT>(it->value());
      }
    });

    return t;
  }
} // namespace boost::json

#endif //BOOST_TAGINVOKE_HPP
