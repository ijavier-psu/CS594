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

void read_data(sqlite3* db, int thread_id);

enum opcodes : uint32_t {
    CONN_INIT =  0x10000002U
};

extern std::unordered_map<std::string, uint32_t> opcode_map;
extern std::unordered_map<uint32_t,std::string> opcode_map_server;

#endif