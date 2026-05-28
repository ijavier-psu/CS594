
#include "database.h"

std::mutex db_mutex;

//FIXME: Figure out which structures are actually needed

/*enum opcodes {
    ERR,
    CONN_INIT,
    CONN_ACCEPT,
    KEEPALIVE,
    LIST_ROOMS,
    LIST_ROOMS_RESP,
    JOIN_ROOM,
    LEAVE_ROOM,
    SEND_MSG,
    RELAY_MSG
}; */

/*
std::unordered_map<std::string, Command> cmd_map {
    {"conn_init", CONN_INIT},
    {"list_rooms", LIST_ROOMS},
    {"join_room", JOIN_ROOM},
    {"leave_room", LEAVE_ROOM},
    {"send_msg", SEND_MSG}
}
*/


std::unordered_map<std::string, uint32_t> opcode_map{
    {"ERR", 0x10000001U},
    {"CONN_INIT", 0x10000002U},
    {"CONN_ACCEPT", 0x10000003},
    {"KEEPALIVE", 0x10000004},
    {"LIST_ROOMS", 0x10000005},
    {"LIST_ROOMS_RESP", 0x10000006},
    {"JOIN_ROOM", 0x10000007},
    {"LEAVE_ROOM", 0x10000008},
    {"SEND_MSG", 0x10000009},
    {"RELAY_MSG",0x1000000A}
};

std::unordered_map<uint32_t,std::string> opcode_map_server{
    {0x10000001U,"ERR"},
    {0x10000002U,"CONN_INIT"},
    {0x10000003,"CONN_ACCEPT"},
    {0x10000004,"KEEPALIVE"},
    {0x10000005,"LIST_ROOMS"},
    {0x10000006,"LIST_ROOMS_RESP"},
    {0x10000007,"JOIN_ROOM"},
    {0x10000008,"LEAVE_ROOM"},
    {0x10000009,"SEND_MSG"},
    {0x1000000A,"RELAY_MSG"}
};


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

void insert_data(sqlite3* db, std::string sql)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    if (execute_sql(db, sql))
    {
        std::cout << "[Writer] Inserted row "<< std::endl;
    }
}

void read_data(sqlite3* db, int thread_id)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    std::string sql = "SELECT user_id, last_msg FROM users;";

    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(
        db,
        sql.c_str(),
        -1,
        &stmt,
        nullptr
    );

    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare statement: "
                  << sqlite3_errmsg(db)
                  << std::endl;
        return;
    }

    std::cout << "\n[Reader " << thread_id << "] Reading database:\n";

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        int user_id = sqlite3_column_int(stmt, 0);

        const int username =
            sqlite3_column_int(stmt, 1);


        std::cout << "ID: " << user_id
                  << ", Name: " << username
                  << std::endl;
    }

    sqlite3_finalize(stmt);
}

int init_db(sqlite3* db){
    std::string create_table_sql =
    "CREATE TABLE IF NOT EXISTS users ("
    "user_id INTEGER PRIMARY KEY AUTOINCREMENT, last_msg INTEGER"
    ");";

    if (!execute_sql(db, create_table_sql))
    {
        sqlite3_close(db);
        return 0;
    }

    std::string create_table_rooms =
    "CREATE TABLE IF NOT EXISTS rooms ("
    "room_name TEXT PRIMARY KEY"
    ");";

    if (!execute_sql(db, create_table_rooms))
    {
        sqlite3_close(db);
        return 0;
    }

    std::string create_table_2 = 
    "CREATE TABLE IF NOT EXISTS room_users ("
    "room_name TEXT NOT NULL, user_id INTEGER NOT NULL, primary key (room_name,user_id)"
    "FOREIGN KEY (room_name) references rooms(room_name) ON DELETE CASCADE,"
    "FOREIGN KEY (user_id) references users(user_id) ON DELETE CASCADE);";
    
    if (!execute_sql(db, create_table_2))
    {
        sqlite3_close(db);
        return 0;
    }

    return 1;

}