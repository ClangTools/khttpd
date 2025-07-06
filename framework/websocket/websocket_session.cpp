// framework/websocket/websocket_session.cpp
#include "websocket_session.hpp"
#include "context/websocket_context.hpp"
#include <fmt/core.h>
#include <boost/uuid/uuid_io.hpp>           // to_string

namespace khttpd::framework
{
  std::mutex WebsocketSession::m_sessions_mutex{};
  std::map<std::string, std::shared_ptr<WebsocketSession>> WebsocketSession::m_sessions_id_{};
  std::mutex WebsocketSession::m_gen_mutex{};
  boost::uuids::random_generator WebsocketSession::gen{};

  WebsocketSession::WebsocketSession(tcp::socket&& socket, WebsocketRouter& ws_router,
                                     const std::string& initial_path)
    : ws_(std::move(socket)),
      websocket_router_(ws_router),
      initial_path_(initial_path)
  {
    {
      std::unique_lock<std::mutex> lock(m_gen_mutex);
      id = boost::uuids::to_string(gen());
    }
    ws_.read_message_max(32 * 1024 * 1024);
    ws_.set_option(ws::stream_base::decorator([](ws::response_type& res)
    {
      res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " khttpd-websocket");
    }));
  }


  void WebsocketSession::on_handshake(beast::error_code ec)
  {
    if (ec)
    {
      fmt::print(stderr, "WebSocket handshake error for path '{}': {}\n", initial_path_, ec.message());
      do_close(ec);
      return;
    }
    fmt::print("WebSocket handshake successful for path: {}\n", initial_path_);

    WebsocketContext open_ctx(shared_from_this(), initial_path_);
    {
      std::unique_lock<std::mutex> lock{m_sessions_mutex};
      m_sessions_id_[id] = shared_from_this();
    }
    websocket_router_.dispatch_open(initial_path_, open_ctx);

    do_read();
  }

  void WebsocketSession::do_read()
  {
    ws_.async_read(buffer_,
                   beast::bind_front_handler(&WebsocketSession::on_read, shared_from_this()));
  }

  void WebsocketSession::on_read(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec == ws::error::closed)
    {
      fmt::print("WebSocket connection for path '{}' closed by client.\n", initial_path_);
      do_close(ec);
      return;
    }
    if (ec)
    {
      fmt::print(stderr, "WebSocket read error for path '{}': {}\n", initial_path_, ec.message());
      do_close(ec);
      return;
    }

    std::string received_message = beast::buffers_to_string(buffer_.data());
    bool is_text = ws_.got_text();

    fmt::print("Received WS message on path '{}': {}\n", initial_path_, received_message);

    buffer_.consume(buffer_.size());

    WebsocketContext message_ctx(shared_from_this(), received_message, is_text, initial_path_);
    websocket_router_.dispatch_message(initial_path_, message_ctx);

    do_read();
  }

  void WebsocketSession::send_message(const std::string& msg, bool is_text_msg)
  {
    auto ss = std::make_shared<const std::string>(msg);
    do_write(ss, is_text_msg);
  }

  bool WebsocketSession::send_message(const std::string& id, const std::string& msg, bool is_text)
  {
    return send_message(std::vector<std::string>{id}, msg, is_text) > 0;
  }

  size_t WebsocketSession::send_message(const std::vector<std::string>& ids, const std::string& msg, bool is_text)
  {
    std::unique_lock<std::mutex> lock{m_sessions_mutex};
    size_t count = 0;
    for (const auto& id : ids)
    {
      auto item = m_sessions_id_.find(id);
      if (item == m_sessions_id_.end())
      {
        continue;
      }
      item->second->send_message(msg, is_text);
      count++;
    }
    return count;
  }

  void WebsocketSession::do_write(std::shared_ptr<const std::string> ss, bool is_text_msg)
  {
    // 设置消息是文本还是二进制
    ws_.text(is_text_msg);

    // --- 检查消息大小，决定是否分片 ---
    if (ss->length() < auto_fragment_threshold_)
    {
      // 消息不大，直接发送，无需分片。
      // 这可以避免为小消息创建 vector 和 buffer sequence 的开销。
      ws_.async_write(net::buffer(*ss),
                      beast::bind_front_handler(&WebsocketSession::on_write, shared_from_this()));
    }
    else
    {
      // 消息很大，需要分片发送。
      // 1. 创建一个缓冲区序列（vector of const_buffer）的 shared_ptr。
      //    必须用 shared_ptr 来管理，因为它需要在异步操作期间保持存活。
      auto buffer_sequence_ptr = std::make_shared<std::vector<net::const_buffer>>();

      // 2. 预留空间以提高效率
      buffer_sequence_ptr->reserve(ss->length() / fragment_size_ + 1);

      // 3. 将大字符串切分成多个 buffer，并添加到序列中。
      //    这个过程不会拷贝字符串数据，net::const_buffer 只是一个视图。
      size_t offset = 0;
      while (offset < ss->length())
      {
        size_t current_chunk_size = std::min(fragment_size_, ss->length() - offset);
        buffer_sequence_ptr->emplace_back(ss->data() + offset, current_chunk_size);
        offset += current_chunk_size;
      }

      // 4. 调用 async_write，传入缓冲区序列。
      //    Beast 会自动将序列中的每个 buffer 作为一帧来发送。
      ws_.async_write(
        *buffer_sequence_ptr, // 传入缓冲区序列
        [ss, buffer_sequence_ptr, self = shared_from_this()](beast::error_code ec, std::size_t bytes)
        {
          // 这个 lambda 的作用是确保 ss 和 buffer_sequence_ptr 的生命周期
          // 能够覆盖整个异步写操作。当 on_write 被调用时，它们依然有效。
          self->on_write(ec, bytes);
        }
      );
    }
  }

  void WebsocketSession::on_write(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec)
    {
      fmt::print(stderr, "WebSocket write error for path '{}': {}\n", initial_path_, ec.message());
      do_close(ec);
      return;
    }
  }

  void WebsocketSession::do_close(beast::error_code ec)
  {
    if (ec && ec != ws::error::closed && ec != boost::asio::error::eof)
    {
      WebsocketContext error_ctx(shared_from_this(), initial_path_, ec);
      websocket_router_.dispatch_error(initial_path_, error_ctx);
    }
    else
    {
      WebsocketContext close_ctx(shared_from_this(), initial_path_, ec);
      {
        std::unique_lock<std::mutex> lock{m_sessions_mutex};
        for (auto item = m_sessions_id_.begin(); item != m_sessions_id_.end(); ++item)
        {
          if (item->first == id)
          {
            m_sessions_id_.erase(item);
            break;
          }
        }
      }
      websocket_router_.dispatch_close(initial_path_, close_ctx);
    }
  }
}
