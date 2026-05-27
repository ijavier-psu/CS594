#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "database.h"

constexpr uint32_t MAX_PAYLOAD_SIZE = 4096;

#pragma pack(push, 1)
struct irc_pkt_header {
    uint32_t opcode;
    uint32_t length;
};
#pragma pack(pop)

struct irc_packet {
    irc_pkt_header header;
    std::vector<uint8_t> payload;
};

bool recv_all(int sock, void* buffer, size_t length) {
    uint8_t* ptr =
        static_cast<uint8_t*>(buffer);

    while (length > 0) {
        ssize_t bytes =
            recv(sock, ptr, length, 0);

        if (bytes <= 0) {
            return false;
        }

        ptr += bytes;
        length -= bytes;
    }

    return true;
}

bool send_all(int sock,
              const void* buffer,
              size_t length) {
    const uint8_t* ptr =
        static_cast<const uint8_t*>(buffer);

    while (length > 0) {
        ssize_t bytes =
            send(sock, ptr, length, 0);

        if (bytes <= 0) {
            return false;
        }

        ptr += bytes;
        length -= bytes;
    }

    return true;
}

bool send_packet(int sock,
                 const irc_packet& packet) {
    size_t total_size =
        sizeof(irc_pkt_header) +
        packet.payload.size();

    std::vector<uint8_t> buffer(
        total_size
    );

    irc_pkt_header net_header;

    net_header.opcode =
        htonl(packet.header.opcode);

    net_header.length =
        htonl(packet.header.length);

    memcpy(buffer.data(),
           &net_header,
           sizeof(net_header));

    if (!packet.payload.empty()) {
        memcpy(buffer.data()
                   + sizeof(net_header),
               packet.payload.data(),
               packet.payload.size());
    }

    return send_all(sock,
                    buffer.data(),
                    buffer.size());
}

bool recv_packet(int sock,
                 irc_packet& packet) {
    irc_pkt_header net_header;

    if (!recv_all(sock,
                  &net_header,
                  sizeof(net_header))) {
        return false;
    }

    packet.header.opcode =
        ntohl(net_header.opcode);

    packet.header.length =
        ntohl(net_header.length);

    if (packet.header.length >
        MAX_PAYLOAD_SIZE) {
        return false;
    }

    packet.payload.resize(
        packet.header.length
    );

    if (packet.header.length > 0) {
        if (!recv_all(sock,
                      packet.payload.data(),
                      packet.payload.size())) {
            return false;
        }
    }

    return true;
}

void receive_thread(int sock) {
    while (true) {
        irc_packet packet;

        if (!recv_packet(sock, packet)) {
            std::cout
                << "\nDisconnected\n";

            exit(0);
        }

        std::cout << "\nReceived packet\n";
        std::cout << "Opcode: "
                  << packet.header.opcode
                  << "\n";

        std::cout << "Length: "
                  << packet.header.length
                  << "\n";

        if (!packet.payload.empty()) {
            std::string text(
                packet.payload.begin(),
                packet.payload.end()
            );

            std::cout << "Payload: "
                      << text
                      << "\n";
        }

        std::cout << "> ";
        std::cout.flush();
    }
}

int main() {
    int port = 1356;
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock,
                (sockaddr*)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    //setup listening thread
    std::thread receiver(receive_thread, sock);

    //Client UI loop
    //FIXME
    while (true) {
        std::cout << "> ";

        std::string line;
        std::getline(std::cin, line);

        if (line == "quit") {
            break;
        }

        size_t pos = line.find(' ');

        if (pos == std::string::npos) {
            std::cout
                << "Format: "
                << "<opcode> <message>\n";
            continue;
        }

        uint32_t opcode =
            opcode_map["CONN_INIT"];
            //std::stoul(
            //    line.substr(0, pos)
            //);

        std::string message =
            line.substr(pos + 1);

        irc_packet packet;

        packet.header.opcode =
            opcode;

        packet.header.length =
            message.size();

        packet.payload.assign(
            message.begin(),
            message.end()
        );

        if (!send_packet(sock,
                         packet)) {
            std::cout
                << "Send failed\n";
            break;
        }
    }

    close(sock);

    receiver.join();

    return 0;
}