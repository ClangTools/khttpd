#include "framework/context/http_context.hpp"
#include "framework/server.hpp"
#include "framework/tests/concurrency_benchmark.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace fs = boost::filesystem;
namespace khttpd_fw = khttpd::framework;

namespace
{
  struct Options
  {
    int concurrency = 32;
    int requests = 2000;
    int server_threads = 4;
    int warmup = 100;
  };

  struct TempWebRoot
  {
    fs::path path;

    TempWebRoot()
      : path(fs::temp_directory_path() / fs::unique_path("khttpd-concurrency-benchmark-%%%%-%%%%-%%%%"))
    {
      fs::create_directories(path);
      std::ofstream((path / "index.html").string()) << "ok";
    }

    ~TempWebRoot()
    {
      boost::system::error_code ignored;
      fs::remove_all(path, ignored);
    }
  };

  int parse_positive_value(const char* name, const char* value)
  {
    try
    {
      const int parsed = std::stoi(value);
      if (parsed <= 0) throw std::invalid_argument("not positive");
      return parsed;
    }
    catch (const std::exception&)
    {
      throw std::runtime_error(std::string(name) + " must be a positive integer");
    }
  }

  Options parse_options(int argc, char** argv)
  {
    Options options;
    for (int i = 1; i < argc; ++i)
    {
      const std::string argument = argv[i];
      if (argument == "--help")
      {
        std::cout << "Usage: concurrency_benchmark [--concurrency N] [--requests N] [--server-threads N] [--warmup N]\n";
        std::exit(0);
      }
      if (i + 1 >= argc) throw std::runtime_error("missing value for " + argument);
      const int value = parse_positive_value(argument.c_str(), argv[++i]);
      if (argument == "--concurrency") options.concurrency = value;
      else if (argument == "--requests") options.requests = value;
      else if (argument == "--server-threads") options.server_threads = value;
      else if (argument == "--warmup") options.warmup = value;
      else throw std::runtime_error("unknown argument: " + argument);
    }
    return options;
  }

  bool request_once(unsigned short port, int request_id, std::uint64_t& latency_us)
  {
    const auto started_at = std::chrono::steady_clock::now();
    try
    {
      net::io_context ioc;
      beast::tcp_stream stream(ioc);
      stream.expires_after(std::chrono::seconds(10));
      stream.connect(tcp::endpoint(net::ip::address_v4::loopback(), port));

      http::request<http::empty_body> request{http::verb::get, "/benchmark?id=" + std::to_string(request_id), 11};
      request.set(http::field::host, "127.0.0.1");
      request.set(http::field::user_agent, "khttpd-concurrency-benchmark");
      request.keep_alive(false);
      http::write(stream, request);

      beast::flat_buffer buffer;
      http::response<http::string_body> response;
      http::read(stream, buffer, response);
      beast::error_code ignored;
      stream.socket().shutdown(tcp::socket::shutdown_both, ignored);

      latency_us = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started_at).count());
      return response.result() == http::status::ok && response.body() == "pong";
    }
    catch (...)
    {
      latency_us = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started_at).count());
      return false;
    }
  }

  void run_requests(unsigned short port, int requests, int concurrency, std::vector<std::uint64_t>& latencies,
                    std::atomic<std::size_t>& successes, std::atomic<std::size_t>& failures)
  {
    std::atomic<int> next_request{0};
    std::vector<std::vector<std::uint64_t>> worker_latencies(static_cast<std::size_t>(concurrency));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(concurrency));
    for (int worker = 0; worker < concurrency; ++worker)
    {
      workers.emplace_back([&, worker]()
      {
        auto& worker_samples = worker_latencies[static_cast<std::size_t>(worker)];
        while (true)
        {
          const int request_id = next_request.fetch_add(1, std::memory_order_relaxed);
          if (request_id >= requests) return;
          std::uint64_t latency_us = 0;
          if (request_once(port, request_id, latency_us)) successes.fetch_add(1, std::memory_order_relaxed);
          else failures.fetch_add(1, std::memory_order_relaxed);
          worker_samples.push_back(latency_us);
        }
      });
    }
    for (auto& worker : workers) worker.join();
    for (auto& samples : worker_latencies)
    {
      latencies.insert(latencies.end(), samples.begin(), samples.end());
    }
  }
}

int main(int argc, char** argv)
{
  try
  {
    const Options options = parse_options(argc, argv);
    TempWebRoot web_root;
    auto server = std::make_shared<khttpd_fw::Server>(tcp::endpoint(tcp::v4(), 0), web_root.path.string(), options.server_threads);
    server->get_http_router().get("/benchmark", [](khttpd_fw::HttpContext& context)
    {
      context.set_status(http::status::ok);
      context.set_content_type("text/plain");
      context.set_body("pong");
    });
    const auto port = server->local_endpoint().port();
    std::thread server_thread([server]() { server->run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<std::uint64_t> warmup_latencies;
    std::atomic<std::size_t> warmup_successes{0};
    std::atomic<std::size_t> warmup_failures{0};
    run_requests(port, options.warmup, options.concurrency, warmup_latencies, warmup_successes, warmup_failures);

    std::vector<std::uint64_t> latencies;
    latencies.reserve(static_cast<std::size_t>(options.requests));
    std::atomic<std::size_t> successes{0};
    std::atomic<std::size_t> failures{0};
    const auto started_at = std::chrono::steady_clock::now();
    run_requests(port, options.requests, options.concurrency, latencies, successes, failures);
    const auto elapsed_us = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - started_at).count());

    server->stop();
    server_thread.join();

    const khttpd_fw::benchmark::Stats stats(std::move(latencies), elapsed_us, successes.load(), failures.load());
    std::cout << khttpd_fw::benchmark::format_report(stats, options.server_threads, options.concurrency, options.requests);
    return stats.failures() == 0 ? 0 : 1;
  }
  catch (const std::exception& error)
  {
    std::cerr << "concurrency benchmark failed: " << error.what() << "\n";
    return 2;
  }
}
