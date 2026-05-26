#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <cstdint>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sqlite3.h>

std::mutex db_mutex;

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

std::vector<int> clients;
std::mutex clients_mutex;


bool recv_packet(int sock, irc_packet& packet) {
    irc_pkt_header net_header;

    uint8_t* ptr = reinterpret_cast<uint8_t*>(&net_header);
    size_t length = sizeof(net_header);

    while (length > 0) {
        ssize_t bytes = recv(sock, ptr, length, 0);

        if (bytes <= 0) {
            return false;
        }

        ptr += bytes;
        length -= bytes;
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
        ptr = reinterpret_cast<uint8_t*>(packet.payload.data());
        length = packet.payload.size();

        while (length > 0) {
            ssize_t bytes = recv(sock, ptr, length, 0);

            if (bytes <= 0) {
                return false;
            }

            ptr += bytes;
            length -= bytes;
        }
    }

    return true;
}

/*bool send_all(int sock, const void* buffer, size_t length) {
    const uint8_t* ptr =
        static_cast<const uint8_t*>(buffer);

    while (length > 0) {
        ssize_t bytes = send(sock, ptr, length, 0);

        if (bytes <= 0) {
            return false;
        }

        ptr += bytes;
        length -= bytes;
    }

    return true;
} */

bool send_packet(int sock, const irc_packet& packet) {
    size_t total_size =
        sizeof(irc_pkt_header) +
        packet.payload.size();

    //FIXME
    std::vector<uint8_t> buffer(total_size);
    
    const uint8_t* ptr = reinterpret_cast<uint8_t*>(buffer.data());
    size_t length = buffer.size();

    irc_pkt_header net_header;

    net_header.opcode =
        htonl(packet.header.opcode);

    net_header.length =
        htonl(packet.header.length);

    memcpy(buffer.data(),
           &net_header,
           sizeof(net_header));

    if (!packet.payload.empty()) {
        memcpy(buffer.data() + sizeof(net_header),
               packet.payload.data(),
               packet.payload.size());
    }

    while (length > 0) {
        ssize_t bytes = send(sock, ptr, length, 0);

        if (bytes <= 0) {
            return false;
        }

        ptr += bytes;
        length -= bytes;
    }

    return true;
    /*return send_all(sock,
                    buffer.data(),
                    buffer.size());
    */
}

void broadcast_packet(
    const irc_packet& packet,
    int sender_socket
) {
    std::lock_guard<std::mutex> lock(
        clients_mutex
    );

    for (int client : clients) {
        if (client != sender_socket) {
            send_packet(client, packet);
        }
    }
}

void handle_client(
    int client_socket,
    sockaddr_in client_addr
) {
    char ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET,
              &client_addr.sin_addr,
              ip,
              sizeof(ip));

    int port =
        ntohs(client_addr.sin_port);

    std::cout << "Connected: "
              << ip << ":" << port
              << std::endl;

    while (true) {
        irc_packet packet;

        if (!recv_packet(client_socket,
                         packet)) {
            break;
        }

        std::cout << "Opcode="
                  << packet.header.opcode
                  << " Length="
                  << packet.header.length
                  << std::endl;

        if (!packet.payload.empty()) {
            std::string text(
                packet.payload.begin(),
                packet.payload.end()
            );

            std::cout << "Payload: "
                      << text
                      << std::endl;
        }

        broadcast_packet(packet,
                         client_socket);
    }

    close(client_socket);

    {
        std::lock_guard<std::mutex> lock(
            clients_mutex
        );

        clients.erase(
            std::remove(clients.begin(),
                        clients.end(),
                        client_socket),
            clients.end()
        );
    }

    std::cout << "Client disconnected\n";
}

bool execute_sql(sqlite3* db, const std::string& sql)
{
    char* errMsg = nullptr;

    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL Error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

int main() {
    // Set up database
    sqlite3* db;

    int rc = sqlite3_open("example.db", &db);

    if (rc)
    {
        std::cerr << "Cannot open database: "
                  << sqlite3_errmsg(db)
                  << std::endl;

        return 1;
    }

    // Enable WAL mode for better concurrency
    execute_sql(db, "PRAGMA journal_mode=WAL;");

    std::string create_table_sql =
    "CREATE TABLE IF NOT EXISTS users ("
    "user_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    ");";

    if (!execute_sql(db, create_table_sql))
    {
        sqlite3_close(db);
        return 1;
    }




    //Start Socket
    int server_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (server_socket < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt(server_socket,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    sockaddr_in server_addr {};

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(8080);

    if (bind(server_socket,
             (sockaddr*)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_socket,
               SOMAXCONN) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Listening on 8080\n";

    while (true) {
        sockaddr_in client_addr {};
        socklen_t client_len =
            sizeof(client_addr);

        int client_socket =
            accept(server_socket,
                   (sockaddr*)&client_addr,
                   &client_len);

        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(
                clients_mutex
            );

            clients.push_back(client_socket);
        }

        std::thread(
            handle_client,
            client_socket,
            client_addr
        ).detach();
    }
}