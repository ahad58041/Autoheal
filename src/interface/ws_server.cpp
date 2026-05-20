// src/interface/ws_server.cpp
//
// Minimal websocketpp broadcast server. Two threads:
//   - server_thread_    : runs websocketpp's asio io_service
//   - broadcast_thread_ : every kBroadcastEveryMs, push JSON to all clients

#include "ws_server.hpp"
#include "json_serializer.hpp"
#include "../common/config.hpp"
#include "../common/logger.hpp"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <chrono>
#include <mutex>
#include <set>

namespace autoheal {

using server_t = websocketpp::server<websocketpp::config::asio>;

struct WsServer::Impl {
    server_t srv;
    std::mutex conns_mu;
    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> conns;
};

WsServer::WsServer(SnapshotBuffer& buffer,
                   InterventionLog& log,
                   const IgnoreList& ignore,
                   int port)
    : buffer_(buffer), log_(log), ignore_(ignore), port_(port),
      impl_(std::make_unique<Impl>()) {

    impl_->srv.clear_access_channels(websocketpp::log::alevel::all);
    impl_->srv.clear_error_channels(websocketpp::log::elevel::all);
    impl_->srv.init_asio();
    impl_->srv.set_reuse_addr(true);

    impl_->srv.set_open_handler([this](websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lk(impl_->conns_mu);
        impl_->conns.insert(hdl);
        LOG_INFO("WS client connected (now " + std::to_string(impl_->conns.size()) + ")");
    });
    impl_->srv.set_close_handler([this](websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lk(impl_->conns_mu);
        impl_->conns.erase(hdl);
        LOG_INFO("WS client disconnected (now " + std::to_string(impl_->conns.size()) + ")");
    });
    impl_->srv.set_fail_handler([this](websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lk(impl_->conns_mu);
        impl_->conns.erase(hdl);
    });
}

WsServer::~WsServer() { stop(); }

void WsServer::start() {
    if (running_.exchange(true)) return;
    server_thread_    = std::thread([this] { run_(); });
    broadcast_thread_ = std::thread([this] {
        while (running_) {
            broadcast_();
            std::this_thread::sleep_for(std::chrono::milliseconds(config::kBroadcastEveryMs));
        }
    });
}

void WsServer::stop() {
    if (!running_.exchange(false)) return;
    try {
        impl_->srv.stop_listening();
        impl_->srv.stop();
    } catch (const std::exception& e) {
        LOG_WARN(std::string("WsServer stop: ") + e.what());
    }
    if (server_thread_.joinable())    server_thread_.join();
    if (broadcast_thread_.joinable()) broadcast_thread_.join();
}

void WsServer::run_() {
    try {
        LOG_INFO("WsServer: listening on port " + std::to_string(port_));
        impl_->srv.listen(static_cast<uint16_t>(port_));
        impl_->srv.start_accept();
        impl_->srv.run();
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("WsServer error: ") + e.what());
    }
    LOG_INFO("WsServer: stopped");
}

void WsServer::broadcast_() {
    std::string payload;
    try {
        payload = JsonSerializer::build(buffer_, log_);
    } catch (const std::exception& e) {
        LOG_WARN(std::string("JSON build failed: ") + e.what());
        return;
    }

    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> snapshot;
    {
        std::lock_guard<std::mutex> lk(impl_->conns_mu);
        snapshot = impl_->conns;
    }
    for (auto& hdl : snapshot) {
        try {
            impl_->srv.send(hdl, payload, websocketpp::frame::opcode::text);
        } catch (const std::exception&) {
            // Drop dead client silently — close_handler will clean up.
        }
    }
}

}  // namespace autoheal
