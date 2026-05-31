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

int userid = 0;

bool recv_all(int sock, void* buffer, size_t length) {
    uint8_t* ptr = static_cast<uint8_t*>(buffer);

    while (length > 0) {
        ssize_t bytes = recv(sock, ptr, length, 0);

        if (bytes <= 0) {
            return false;
        }

        ptr += bytes;
        length -= bytes;
    }

    return true;
}

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

bool send_all(int sock, const void* buffer, size_t length) {
    const uint8_t* ptr = static_cast<const uint8_t*>(buffer);

    while (length > 0) {
        ssize_t bytes = send(sock, ptr, length, 0);

        if (bytes <= 0) {
            return false;
        }

        ptr += bytes;
        length -= bytes;
    }

    return true;
}

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
}


bool handle_packet(int sock)
{
    irc_pkt_header header;

    if (!recv_all(sock,&header,sizeof(header))){
        return false;
    }

    header.opcode = ntohl(header.opcode);
 
    header.length = ntohl(header.length);
    //std::cout<<"Length 1: "<<header.length<<std::endl;

    switch (header.opcode){
        case CONN_ACCEPT:
        {
            irc_pkt_conn_accept packet;

            packet.header = header;

            if (!recv_all(sock, &packet.userid, sizeof(packet.userid))){
                return false;
            }

            packet.userid = ntohs(packet.userid);

            std::cout
                << "Client recieved CONN_ACCEPT\n";

            std::cout
                << "userid="
                << packet.userid
                << std::endl;

            userid = packet.userid;

            break;
        }

        case LIST_ROOMS_RESP:
        {
            std::cout << "Client received LIST_ROOMS_RESP" << std::endl;

            irc_pkt_list_rooms_resp packet;
            packet.header = header;
            //packet.header.length = ntohl(packet.header.length);
            int num_rooms = header.length / 20;
            if (!recv_all(sock, &packet.rooms, header.length)){
                return false;
            }

            std::cout<< "size: "<<header.length<<" num rooms: "<<num_rooms<<std::endl;

            for (uint32_t i = 0; i < num_rooms; i++)
            {
                std::cout << packet.rooms[i] << std::endl;
            }
            break;
        }

        default:
        {
            std::cout
                << "Client Received Unknown opcode: "
                << header.opcode
                << std::endl;

            // discard unknown payload
            if (header.length > 0)
            {
                std::vector<uint8_t>
                    discard( header.length);

                recv_all(sock, discard.data(), discard.size());
            }

            break;
        }
    }

    return true;
}

void receive_thread(int sock) {
    while (true) {
        
        if (!handle_packet(sock)){
            break; 
        }

    /// FIXME DELETE
        /*
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
        

            std::cout << "> ";
            std::cout.flush();

            switch(packet.header.opcode){
                case CONN_ACCEPT: {
                    userid = std::stoi(text);
                    std::cout << "user id: "<< text << std::endl;
                    break;
                }

                default: {
                    std::cout << "Unrecognized opcode" << std::endl;
                    break;
                }

            }
        } */
    }
}

bool init_connection(int sock) {
    uint32_t opcode = opcode_map["CONN_INIT"];

    irc_pkt_conn_init packet;

    packet.header.opcode =  htonl(opcode);
    packet.header.length = htonl(0);

    return send_all(sock, &packet, sizeof(packet));

    //packet.payload.assign(message.begin(), message.end());

    //if (!send_packet(sock,packet)) {
     //   std::cout << "Send failed\n";
    //    return false;
    //}
    //return true;
}

//FIXME add all req checks
bool check_room_req(std::string str){
    if ( str.length() > 20 ){
        std::cout<<"Maximum room name length: 20 characters"<<std::endl;
        return false;
    }

    return true;
}

bool check_msg_req(std::string str){
    if ( str.length() > 7999 ){
        std::cout<<"Maximum message length: 7999 characters"<<std::endl;
        return false;
    }

    return true;
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

    if (!init_connection(sock)){
        std::cout<<"Failed to initialize contact with server"<<std::endl;
        return 1;
    }

    while(userid == 0){
        sleep(1);
    }


    //Client UI loop
    //FIXME
    while (true) {
        std::cout << "> ";

        std::string line;
        std::getline(std::cin, line);

        if (line == "quit") {
            break;
        }
        std::cout<<"Input Line: "<<line<<std::endl;

        size_t pos = line.find(' ');

        uint32_t opcode;
        //    opcode_map["CONN_INIT"];
        
        //std::stoul(line.substr(0, pos));
        //FIXME: handle errors
        auto it = cmd_map.find(line.substr(0, pos));
        if (it != cmd_map.end()) {
            opcode = it->second;
            std::cout<<"Read opcode "<< opcode_map_server[opcode]<<std::endl;
            //opcode = opcode_map[op];
        }
        else {
            std::cout<<"Invalid opcode"<<std::endl;
            continue;
        }

        switch (opcode){
            case LIST_ROOMS: {
                irc_pkt_list_rooms packet;
                packet.header.opcode =  htonl(opcode);
                packet.header.length = htonl(0);
                send_all(sock, &packet, sizeof(packet));

                break;
            }
            case JOIN_ROOM: {
                //FIXME: Check all requirements
                if (pos == std::string::npos) {
                    std::cout<< "Format: " << "join_room room_name\n";
                    continue;
                }
                std::string message = line.substr(pos + 1);

                if (!check_room_req(message)){
                    break;
                }

                irc_pkt_join_room packet;
                std::strncpy(packet.room_name, message.c_str(), sizeof(packet.room_name) - 1);
                packet.header.opcode =  htonl(opcode);
                packet.header.length = htonl(sizeof(packet.room_name));
  
                send_all(sock, &packet, sizeof(packet));

                break;
            }
            case LEAVE_ROOM: 
            {
                //FIXME: Check all requirements
                if (pos == std::string::npos) {
                    std::cout<< "Format: " << "join_room room_name\n";
                    continue;
                }
                std::string message = line.substr(pos + 1);

                if (!check_room_req(message)){
                    break;
                }

                irc_pkt_join_room packet;
                std::strncpy(packet.room_name, message.c_str(), sizeof(packet.room_name) - 1);
                packet.header.opcode =  htonl(opcode);
                packet.header.length = htonl(sizeof(packet.room_name));

                send_all(sock, &packet, sizeof(packet));

                break;
            }
            case SEND_MSG:
            {
                if (pos == std::string::npos) {
                    std::cout<< "Format: " << "send_msg room_name [message to send]\n";
                    continue;
                }
                std::string subs = line.substr(pos + 1);
                size_t pos2 = subs.find(' ');

                if (pos2 == std::string::npos) {
                    std::cout<< "Format: " << "send_msg room_name [message to send]\n";
                    continue;
                }

                std::string room_name = subs.substr(0,pos2);
                std::string message = subs.substr(pos2+1);

                if (!check_room_req(room_name)){
                    break;
                }

                if (!check_msg_req(room_name)){
                    break;
                }

                size_t msg_len = message.size();
                size_t packet_size = sizeof(irc_pkt_send_msg) + msg_len;

                irc_pkt_send_msg* packet = reinterpret_cast<irc_pkt_send_msg*>(malloc(packet_size));

                std::cout<<"Room: "<<room_name<<std::endl;
                std::cout<<"Message: "<<message<<std::endl;

                //irc_pkt_send_msg packet;
                packet->header.opcode =  htonl(opcode);
                packet->header.length = htonl(static_cast<uint32_t>(sizeof(packet->room_name) + msg_len));

                //std::strncpy(packet.room_name, room_name.c_str(), sizeof(packet.room_name) - 1);
                memset(packet->room_name,0,sizeof(packet->room_name));
                std::strncpy(packet->room_name,room_name.c_str(),sizeof(packet->room_name) - 1);

                memcpy(packet->msg,message.data(),msg_len);

                send_all(sock,packet,packet_size);
                //send_all(sock, &packet, sizeof(packet));
                
                free(packet);
                break;
            }
            default: {
                break;
            }

        }

        /*
        irc_packet packet;

        std::cout<<"opcode "<<opcode<<std::endl;
        packet.header.opcode =  htonl(opcode);
        packet.header.length =  htonl(message.size());


        send_all(sock, &packet, sizeof(packet));
        //if (!send_packet(sock, packet)) {
        //    std::cout << "Send failed\n";
        //    break;
        //}
        */
    }

    close(sock);

    receiver.join();

    return 0;
}