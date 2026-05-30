#pragma once

#ifndef SQLITE_DB_HPP
#define SQLITE_DB_HPP

#include <sqlite3.h>

#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <sqlite3.h>

extern std::mutex db_mutex;

// Database helpers
bool execute_sql(sqlite3* db, const std::string& sql);

// Thread worker functions
void insert_data(sqlite3* db, std::string sql);

uint16_t insert_user_data(sqlite3* db, std::string sql);

std::vector<std::string> read_data(sqlite3* db);

int init_db(sqlite3* db);

enum opcodes : uint32_t {
    CONN_INIT =  0x10000002U,
    CONN_ACCEPT = 0x10000003U,
    KEEP_ALIVE =  0x10000004U,
    LIST_ROOMS =  0x10000005U,
    LIST_ROOMS_RESP = 0x10000006U,
    JOIN_ROOM = 0x10000007U,
    LEAVE_ROOM = 0x10000008U,
    SEND_MSG = 0x10000009U,
    RELAY_MSG = 0x1000000AU
};

extern std::unordered_map<std::string, opcodes> cmd_map;
extern std::unordered_map<std::string, uint32_t> opcode_map;
extern std::unordered_map<uint32_t,std::string> opcode_map_server;

constexpr uint32_t MAX_PAYLOAD_SIZE = 4096;
constexpr size_t MAX_ROOMS = 100;

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

struct irc_pkt_conn_init {
    irc_pkt_header header;
};

struct irc_pkt_conn_accept {
    irc_pkt_header header;
    uint16_t userid;
};

struct irc_pkt_list_rooms_resp {
	irc_pkt_header header;
	char rooms[MAX_ROOMS][20];
};


#endif