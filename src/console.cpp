#include "console.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace baker_godot {

Console::~Console() {
    for (Client& c : clients_) c.peer->disconnect_from_host();
    if (server_.is_valid()) server_->stop();
}

bool Console::listen(uint16_t port) {
    server_.instantiate();
    return server_->listen(port, "127.0.0.1") == godot::OK;
}

uint16_t Console::port() const {
    return server_.is_valid() ? uint16_t(server_->get_local_port()) : 0;
}

void Console::poll() {
    if (server_.is_null()) return;
    while (server_->is_connection_available()) clients_.push_back({server_->take_connection(), {}});
    for (size_t i = 0; i < clients_.size();) {
        if (serve(clients_[i])) {
            ++i;
        } else {
            clients_[i].peer->disconnect_from_host();
            clients_.erase(clients_.begin() + std::ptrdiff_t(i));
        }
    }
}

bool Console::serve(Client& c) {
    c.peer->poll();
    if (c.peer->get_status() != godot::StreamPeerTCP::STATUS_CONNECTED) return false;
    const int32_t available = c.peer->get_available_bytes();
    if (available > 0) {
        const godot::Array got = c.peer->get_partial_data(available);
        if (int(got[0]) != godot::OK) return false;
        const godot::PackedByteArray bytes = got[1];
        c.pending.append(reinterpret_cast<const char*>(bytes.ptr()), size_t(bytes.size()));
    }
    for (size_t end; (end = c.pending.find('\n')) != std::string::npos;) {
        std::string line = c.pending.substr(0, end);
        c.pending.erase(0, end + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // An answer is one or more lines, then an empty line.
        const std::string reply = answer_(line) + "\n\n";
        godot::PackedByteArray out;
        out.resize(int64_t(reply.size()));
        for (size_t k = 0; k < reply.size(); ++k) out.set(int64_t(k), uint8_t(reply[k]));
        if (c.peer->put_data(out) != godot::OK) return false;
    }
    return true;
}

}  // namespace baker_godot
