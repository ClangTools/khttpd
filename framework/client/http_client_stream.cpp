#include "http_client_stream.hpp"

#include <boost/url.hpp>
#include <limits>
#include "io_context_pool.hpp"

namespace khttpd::framework::client
{
  namespace beast = boost::beast;
  namespace http = beast::http;
  namespace net = boost::asio;
  using tcp = net::ip::tcp;

  struct HttpClientStream::Impl : std::enable_shared_from_this<HttpClientStream::Impl>
  {
    beast::tcp_stream stream;
    tcp::resolver resolver;
    beast::flat_buffer read_buffer;
    http::request<http::buffer_body> request;
    std::optional<http::request_serializer<http::buffer_body>> request_serializer;
    http::response_parser<http::buffer_body> response_parser;
    std::string host;
    std::string port;
    bool started = false;
    bool request_finished = false;

    explicit Impl(net::io_context& ioc) : stream(net::make_strand(ioc)), resolver(stream.get_executor())
    {
      response_parser.body_limit((std::numeric_limits<std::uint64_t>::max)());
    }

    void start(const std::string& url, RequestHead head, Callback callback)
    {
      net::post(stream.get_executor(), [self = shared_from_this(), url, head = std::move(head), callback = std::move(callback)]() mutable
      { self->start_on_executor(url, std::move(head), std::move(callback)); });
    }

    void start_on_executor(const std::string& url, RequestHead head, Callback callback)
    {
      const auto parsed = boost::urls::parse_uri(url);
      if (!parsed || parsed->scheme() != "http")
        return callback(make_error_code(boost::system::errc::operation_not_supported));
      host = parsed->host();
      port = parsed->port().empty() ? "80" : std::string(parsed->port());
      std::string target(parsed->encoded_target());
      if (target.empty()) target = "/";
      request.method(head.method()); request.target(target); request.version(head.version());
      request.keep_alive(head.keep_alive());
      for (const auto& field : head) request.insert(field.name_string(), field.value());
      request.set(http::field::host, host);
      request.body().more = true;
      request_serializer.emplace(request);
      resolver.async_resolve(host, port, [self = shared_from_this(), callback = std::move(callback)]
        (beast::error_code ec, tcp::resolver::results_type results) mutable
      {
        if (ec) return callback(ec);
        self->stream.async_connect(results, [self, callback = std::move(callback)]
          (beast::error_code connect_ec, const tcp::endpoint&) mutable
        {
          if (connect_ec) return callback(connect_ec);
          http::async_write_header(self->stream, *self->request_serializer,
            [self, callback = std::move(callback)](beast::error_code write_ec, std::size_t) mutable
            { self->started = !write_ec; callback(write_ec); });
        });
      });
    }

    void write(net::const_buffer source, Callback callback)
    {
      net::post(stream.get_executor(), [self = shared_from_this(), source, callback = std::move(callback)]() mutable
      { self->write_on_executor(source, std::move(callback)); });
    }

    void write_on_executor(net::const_buffer source, Callback callback)
    {
      if (!started || request_finished) return callback(net::error::operation_aborted);
      request.body().data = const_cast<void*>(source.data());
      request.body().size = source.size();
      request.body().more = true;
      http::async_write(stream, *request_serializer,
        [callback = std::move(callback)](beast::error_code ec, std::size_t) mutable
        { if (ec == http::error::need_buffer) ec = {}; callback(ec); });
    }

    void finish(Callback callback)
    {
      net::post(stream.get_executor(), [self = shared_from_this(), callback = std::move(callback)]() mutable
      { self->finish_on_executor(std::move(callback)); });
    }

    void finish_on_executor(Callback callback)
    {
      if (!started || request_finished) return callback(net::error::operation_aborted);
      request_finished = true;
      if (request_serializer->is_done()) return callback({});
      request.body().data = nullptr; request.body().size = 0; request.body().more = false;
      http::async_write(stream, *request_serializer,
        [callback = std::move(callback)](beast::error_code ec, std::size_t) mutable { callback(ec); });
    }

    void read_head(ResponseHeadCallback callback)
    {
      net::post(stream.get_executor(), [self = shared_from_this(), callback = std::move(callback)]() mutable
      { self->read_head_on_executor(std::move(callback)); });
    }

    void read_head_on_executor(ResponseHeadCallback callback)
    {
      http::async_read_header(stream, read_buffer, response_parser,
        [self = shared_from_this(), callback = std::move(callback)](beast::error_code ec, std::size_t) mutable
      {
        ResponseHead head;
        if (!ec)
        {
          const auto& source = self->response_parser.get();
          head.result(source.result()); head.version(source.version()); head.keep_alive(source.keep_alive());
          for (const auto& field : source) head.insert(field.name_string(), field.value());
        }
        callback(ec, std::move(head));
      });
    }

    void read(net::mutable_buffer target, ReadCallback callback)
    {
      net::post(stream.get_executor(), [self = shared_from_this(), target, callback = std::move(callback)]() mutable
      { self->read_on_executor(target, std::move(callback)); });
    }

    void read_on_executor(net::mutable_buffer target, ReadCallback callback)
    {
      if (response_parser.is_done()) return callback({}, 0, true);
      auto& body = response_parser.get().body(); body.data = target.data(); body.size = target.size();
      http::async_read_some(stream, read_buffer, response_parser,
        [self = shared_from_this(), capacity = target.size(), callback = std::move(callback)]
          (beast::error_code ec, std::size_t) mutable
      {
        if (ec == http::error::need_buffer) ec = {};
        const auto produced = capacity - self->response_parser.get().body().size;
        callback(ec, produced, self->response_parser.is_done());
      });
    }

    void cancel()
    {
      beast::error_code ignored; resolver.cancel(); stream.cancel(); stream.socket().close(ignored);
    }
  };

  HttpClientStream::HttpClientStream() : HttpClientStream(IoContextPool::instance().get_io_context()) {}
  HttpClientStream::HttpClientStream(net::io_context& ioc) : impl_(std::make_shared<Impl>(ioc)) {}
  HttpClientStream::~HttpClientStream() { if (impl_) impl_->cancel(); }
  void HttpClientStream::async_start(const std::string& url, RequestHead head, Callback cb) { impl_->start(url, std::move(head), std::move(cb)); }
  void HttpClientStream::async_write_some(net::const_buffer b, Callback cb) { impl_->write(b, std::move(cb)); }
  void HttpClientStream::async_finish_request(Callback cb) { impl_->finish(std::move(cb)); }
  void HttpClientStream::async_read_response_head(ResponseHeadCallback cb) { impl_->read_head(std::move(cb)); }
  void HttpClientStream::async_read_some(net::mutable_buffer b, ReadCallback cb) { impl_->read(b, std::move(cb)); }
  void HttpClientStream::cancel() { impl_->cancel(); }
}
