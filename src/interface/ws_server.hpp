// src/interface/ws_server.hpp
//
// websocketpp server that accepts dashboard connections on kWsPort and
// broadcasts a JSON state dump every kBroadcastEveryMs.

#pragma once

#include "../common/snapshot.hpp"
#include "../common/ignore_list.hpp"
#include <atomic>
#include <memory>
#include <thread>

namespace autoheal {

class WsServer {
public:
    WsServer(SnapshotBuffer& buffer,
             InterventionLog& log,
             const IgnoreList& ignore,
             int port);
    ~WsServer();

    void start();
    void stop();

private:
    void run_();           // server thread body
    void broadcast_();     // periodic JSON push

    SnapshotBuffer&   buffer_;
    InterventionLog&  log_;
    const IgnoreList& ignore_;
    int               port_;

    std::atomic<bool> running_{false};
    std::thread       server_thread_;
    std::thread       broadcast_thread_;

    // Opaque holder so we don't drag the websocketpp template explosion
    // into this header.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace autoheal
