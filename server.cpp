
#include "server.h"
#include "database.h"

//std::mutex db_mutex;

std::vector<int> clients;
std::mutex clients_mutex;

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
    /*return send_all(sock,
                    buffer.data(),
                    buffer.size());
    */
}

void broadcast_packet(const irc_packet& packet, int sender_socket) {
    std::lock_guard<std::mutex> lock(
        clients_mutex
    );

    for (int client : clients) {
        if (client != sender_socket) {
            send_packet(client, packet);
        }
    }
}

int handle_packet(int sock, sqlite3* db,uint16_t userid,std::vector<std::string> &user_rooms){
    irc_pkt_header header;

    if (!recv_all(sock,&header,sizeof(header))){
        return false;
    }

    header.opcode = ntohl(header.opcode);
 
    header.length = ntohl(header.length);

    switch (header.opcode){
        case CONN_INIT:
        {
            std::cout << "SERVER recieved CONN INIT" << std::endl;
            int recv_id;
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

            std::string sql_str = "INSERT INTO users (socket) VALUES (" + std::to_string(sock) + ") RETURNING userid;";

            //FIXME: handle errors
            uint16_t userid = insert_user_data(db,sql_str);

            irc_pkt_conn_accept packet;
            packet.header.opcode = htonl(CONN_ACCEPT);
            packet.header.length = htonl(sizeof(uint16_t));
            packet.userid = htons(userid);

            send_all(sock, &packet, sizeof(packet));

            return userid;

            break;
        }
        case LIST_ROOMS:
        {
            std::vector<std::string> rooms = read_data(db);
            std::cout << "Rooms: "<<std::endl;
            for (const auto& element : rooms) {
                std::cout << element << " "<<std::endl;
            }
            std::cout << rooms.size() << " "<<std::endl;
            int num_rooms = rooms.size();

            irc_pkt_list_rooms_resp packet;

            packet.header.opcode = htonl(LIST_ROOMS_RESP);
            packet.header.length = htonl(num_rooms * 20);

            std::cout<<"size: "<<num_rooms * 20<<std::endl;

            //char* room_list = new char[num_rooms *20];
            for(int i=0; i<num_rooms;i++){
                strncpy(packet.rooms[i], rooms[i].c_str(),19);
                //std::cout<<"arr size: "<<sizeof(packet.rooms[i]) <<std::endl;
            }

            return send_all(sock, &packet, sizeof(irc_pkt_header)+ num_rooms * 20);
            break;
        }

        case JOIN_ROOM:
        {
            std::cout<<"Server received JOIN ROOM"<<std::endl;
            irc_pkt_join_room packet;
            if (!recv_all(sock, &packet.room_name, sizeof(packet.room_name))){
                return false;
            }

            std::cout<<"room name: "<<packet.room_name <<std::endl;

            //read existing rooms
            std::vector<std::string> rooms = read_data(db);
            std::cout << "Rooms: "<<std::endl;
            int room_found = 0;
            for (const auto& element : rooms) {
                std::cout << element << " "<<std::endl;
                if (element == std::string(packet.room_name)){
                    std::cout<<"Room exists!"<<std::endl;
                    room_found = 1;
                    break;
                }
            }
            if(!room_found){
                std::string sql = "INSERT INTO rooms (room_name) VALUES (\""+ std::string(packet.room_name) +"\");";
                insert_data(db,sql);
            }

            //add to foreign key link table
            std::string link = "INSERT INTO room_users (userid, room_name) VALUES ("+std::to_string(userid) +",\"" +packet.room_name +"\");";
            insert_data(db,link);

            user_rooms.push_back(packet.room_name);

            //DEBUG std::cout<<"New room list1: "<<std::endl;
            //for (const auto& element : user_rooms) {
            //    std::cout << element << " "<<std::endl;
            //}

            break;
        }

        case LEAVE_ROOM:
        {
            std::cout<<"Server received LEAVE ROOM"<<std::endl;
            irc_pkt_join_room packet;
            if (!recv_all(sock, &packet.room_name, sizeof(packet.room_name))){
                return false;
            }

            std::cout<<"room name: "<<packet.room_name <<std::endl;

            //check if room name is in user's room cache
            auto it = std::find(user_rooms.begin(), user_rooms.end(), std::string(packet.room_name));
            if (it != user_rooms.end()) {
                int index = std::distance(user_rooms.begin(), it);
                std::cout << "Found key at index " << index << "\n";

                //remove from database
                //FIXME: need lock to persist through all transactions, move to database.cpp (check if room is empty)
                std::string remove_room = "DELETE FROM room_users where userid = "+std::to_string(userid)+" AND room_name =\""+std::string(packet.room_name)+"\";";
                execute_sql(db,remove_room);

                user_rooms.erase(user_rooms.begin() + index);
            } else {
                std::cout << "Key not found\n";
                //FIXME: return error
            }

            break;
        }

        case SEND_MSG:
        {
            std::cout<<"Server received SEND_MSG"<<std::endl;
            size_t msg_len = header.length - 20;

            size_t packet_size = sizeof(irc_pkt_send_msg) + msg_len;

            auto* packet =reinterpret_cast<irc_pkt_send_msg*>(malloc(packet_size));

            packet->header = header;

            //FIXME: error code
            if (!recv_all(sock,packet->room_name,header.length)){
                free(packet);
                break;
            }

            std::string room_name(packet->room_name,strnlen(packet->room_name, 20));

            std::string msg(packet->msg,msg_len);

            std::cout
                << "[" << room_name << "] "
                << msg
                << std::endl;

            //check that user is not subscribed to room
            auto it = std::find(user_rooms.begin(), user_rooms.end(), std::string(packet->room_name));
            if (it == user_rooms.end()){
                //FIXME: send error code
                std::cout<<"User not subscribed to room"<<std::endl;

                irc_pkt_err_msg err_packet;

                err_packet.header.opcode = htonl(ERR);
                err_packet.header.length = htonl(sizeof(uint32_t)+30);
                err_packet.err_code = htonl(0x20000004U);
                strncpy(err_packet.msg,"User not subscribed to room",29);

                send_all(sock, &err_packet, sizeof(err_packet));
            }

            //prepare relay msg
            //irc_pkt_relay_msg r_packet;

            //relay packet
            std::vector<int> socks = read_users(db,room_name);
            std::cout << "sockets: "<<std::endl;
            for (const auto& sock : socks) {
                std::cout << sock << " "<<std::endl;
            }
            


            free(packet);
            break;
        }


        default:
        {
            std::cout
                << "Server recieved Unknown opcode: "
                << header.opcode
                << std::endl;

            // discard unknown payload
            if (header.length > 0)
            {
                std::vector<uint8_t>
                    discard(
                        header.length
                    );

                recv_all(sock,
                         discard.data(),
                         discard.size());
            }

            break;
        }
    }

    return true;
}



void handle_client(int client_socket,sockaddr_in client_addr, sqlite3* db) {
    char ip[INET_ADDRSTRLEN];
    uint16_t userid = 0;
    std::vector<std::string> rooms;

    inet_ntop(AF_INET,
              &client_addr.sin_addr,
              ip,
              sizeof(ip));

    int port =
        ntohs(client_addr.sin_port);

    std::cout << "Connected: "
              << ip << ":" << port
              << std::endl;

    
    userid = (uint16_t)handle_packet(client_socket,db, userid,rooms);
    

    //Client connection loop
    while (true) {

        if (!handle_packet(client_socket,db, userid,rooms)){
            break; }

        std::cout<<"New room list: "<<std::endl;
        for (const auto& element : rooms) {
            std::cout << element << " "<<std::endl;
        }

            //////FIXME DELETE
        /*
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

        //FIXME: HERE
        //Switch on opcode

        std::string op = opcode_map_server[packet.header.opcode];

        switch(packet.header.opcode){
            case CONN_INIT: {
                std::cout << "CONN INIT" << std::endl;
                auto now = std::chrono::system_clock::now();
                auto duration = now.time_since_epoch();
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

                std::string sql_str = "INSERT INTO users (last_msg) VALUES (" + std::to_string(seconds) + ");";

                insert_data(db,sql_str);

                //FIXME: Test only, remove
                read_data(db,56);

                //Send client their client id
                /*std::string message = line.substr(pos + 1);

                irc_packet packet;

                packet.header.opcode = opcode;
                packet.header.length = message.size();

                packet.payload.assign(message.begin(),message.end());

                if (!send_packet(sock, packet)) {
                    std::cout << "Send failed\n";
                    break;
                } */
                /*
                break;
            }

            default: {
                std::cout << "Unrecognized opcode" << std::endl;
                break;
            }

        }

        broadcast_packet(packet,
                         client_socket);
        */
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

    //FIXME: need lock to persist through all transactions, move to database.cpp
    //delete user from database upon disconnect
    //FIXME: get list of users rooms first
    std::string delete_user = "DELETE FROM users where userid = "+std::to_string(userid)+";";
    execute_sql(db,delete_user);

    //FIXME: Check if any of the rooms now have 0 users, remove

    for (const auto& element : rooms) {
        if (!room_occ(db,element)){
            std::cout << element << " removed "<<std::endl;
            std::string rm_room = "DELETE from rooms where room_name = \""+element+"\"";
            execute_sql(db,rm_room);
        }
        
    }


    std::cout << "Client disconnected\n";
}


int main() {
    // Set up database
    sqlite3* db;
    int port = 1356;

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

    if (! init_db(db) ){
        std::cout << "Failed to initialized database!"<<std::endl;
        return 1;
    }


    //Start Socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

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

    std::cout << "Listening on "<< port <<"\n";

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

        //Add client

        {
            std::lock_guard<std::mutex> lock(
                clients_mutex
            );

            clients.push_back(client_socket);
        }

        std::thread(handle_client,client_socket,client_addr,db).detach();
    }
}