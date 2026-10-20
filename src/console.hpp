#pragma once

// A developer console for tools and agents, over TCP on 127.0.0.1 only: a console runs any
// command it is sent. Each line a client sends is answered, then an empty line ends the
// answer. A line in request form, (ID {command}), gets one line of JSON (baker.reply/1.1),
// for a tool; any other line, text for a person. Polled once a frame on the main thread, so
// commands run between frames like any other script. Debug builds only.

#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/classes/tcp_server.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace baker_godot {

class Console {
public:
    using Answer = std::function<std::string(const std::string& line)>;

    explicit Console(Answer answer) : answer_(std::move(answer)) {}
    ~Console();
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    // Port 0 picks a free port; port() says which.
    [[nodiscard]] bool listen(uint16_t port);
    [[nodiscard]] uint16_t port() const;
    // Accepts clients and answers every complete line they have sent. Never blocks.
    void poll();

private:
    struct Client {
        godot::Ref<godot::StreamPeerTCP> peer;
        std::string pending;  // received, up to an incomplete line
    };
    bool serve(Client& c);  // false once the client has gone

    Answer answer_;
    godot::Ref<godot::TCPServer> server_;
    std::vector<Client> clients_;
};

}  // namespace baker_godot
