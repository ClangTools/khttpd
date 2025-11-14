//
// Created by Caesar on 2025/6/4.
//

#ifndef DI_CONTAINER_HPP
#define DI_CONTAINER_HPP
#include <iostream>
#include <memory>        // For std::shared_ptr
#include <typeindex>     // For std::type_index
#include <map>           // For std::map
#include <functional>    // For std::function
#include <stdexcept>     // For std::runtime_error
#include <string>        // For typeid(T).name()

namespace khttpd::framework
{
  // 前向声明 DI_Container
  class DI_Container;

  // 基础组件类
  class ComponentBase
  {
  public:
    virtual ~ComponentBase() = default; // 确保多态析构
  };

  // DI容器类
  class DI_Container
  {
  public:
    // Meyers' Singleton 模式，保证DI_Container是全局唯一的实例
    static DI_Container& instance()
    {
      static DI_Container container; // 线程安全（C++11保证静态局部变量初始化是线程安全的）
      return container;
    }

    // 注册一个组件及其构造函数依赖
    template <typename T, typename... Args>
    void register_component()
    {
      const auto type_idx = std::type_index(typeid(T));

      if (component_factories_.count(type_idx))
      {
        std::cerr << "Warning: Component " << typeid(T).name() << " already registered. Overwriting." << std::endl;
      }

      auto factory = [this](const DI_Container& container) -> std::shared_ptr<void>
      {
        return std::make_shared<T>(container.resolve<Args>()...);
      };

      component_factories_[type_idx] = factory;
      // std::cout << "Registered component: " << typeid(T).name() << std::endl; // For testing, suppress verbose output
    }

    // 解析并获取一个组件的实例
    template <typename T>
    std::shared_ptr<T> resolve() const
    {
      const auto type_idx = std::type_index(typeid(T));

      // 1. 尝试从单例缓存中获取
      if (const auto it_singleton = singletons_.find(type_idx); it_singleton != singletons_.end())
      {
        // std::cout << "Returning cached instance for: " << typeid(T).name() << std::endl; // For testing, suppress verbose output
        return std::static_pointer_cast<T>(it_singleton->second);
      }

      // 2. 如果不在缓存中，查找工厂函数
      const auto it_factory = component_factories_.find(type_idx);
      if (it_factory == component_factories_.end())
      {
        throw std::runtime_error("Component not registered or dependency missing: " + std::string(typeid(T).name()));
      }

      // 3. 调用工厂函数创建新实例
      std::shared_ptr<T> instance = std::static_pointer_cast<T>(it_factory->second(*this));

      // 4. 将新创建的实例缓存为单例（所有注册的组件默认都是单例）
      singletons_[type_idx] = instance;

      // std::cout << "Resolved and cached instance for: " << typeid(T).name() << std::endl; // For testing, suppress verbose output
      return instance;
    }

    // **新增：清除容器状态的方法，用于测试**
    void clear()
    {
      component_factories_.clear();
      singletons_.clear();
    }

    DI_Container(const DI_Container&) = delete;
    DI_Container& operator=(const DI_Container&) = delete;

  private:
    DI_Container() = default;

  protected: // Changed from private for testing purposes (specifically for clear())
    std::map<std::type_index, std::function<std::shared_ptr<void>(const DI_Container&)>> component_factories_;
    mutable std::map<std::type_index, std::shared_ptr<void>> singletons_;
  };
}

#endif //DI_CONTAINER_HPP
