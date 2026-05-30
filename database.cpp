
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


std::unordered_map<std::string, opcodes> cmd_map {
    {"conn_init", CONN_INIT},
    {"list_rooms", LIST_ROOMS},
    {"join_room", JOIN_ROOM},
    {"leave_room", LEAVE_ROOM},
    {"send_msg", SEND_MSG}
};



std::unordered_map<std::string, uint32_t> opcode_map{
    {"ERR", 0x10000001U},
    {"CONN_INIT", 0x10000002U},
    {"CONN_ACCEPT", 0x10000003U},
    {"KEEPALIVE", 0x10000004U},
    {"LIST_ROOMS", 0x10000005U},
    {"LIST_ROOMS_RESP", 0x10000006U},
    {"JOIN_ROOM", 0x10000007U},
    {"LEAVE_ROOM", 0x10000008U},
    {"SEND_MSG", 0x10000009U},
    {"RELAY_MSG",0x1000000AU}
};

std::unordered_map<uint32_t,std::string> opcode_map_server{
    {0x10000001U,"ERR"},
    {0x10000002U,"CONN_INIT"},
    {0x10000003U,"CONN_ACCEPT"},
    {0x10000004U,"KEEPALIVE"},
    {0x10000005U,"LIST_ROOMS"},
    {0x10000006U,"LIST_ROOMS_RESP"},
    {0x10000007U,"JOIN_ROOM"},
    {0x10000008U,"LEAVE_ROOM"},
    {0x10000009U,"SEND_MSG"},
    {0x1000000AU,"RELAY_MSG"}
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

//Keep lock through write and subsequent read 
uint16_t insert_user_data(sqlite3* db, std::string sql)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    sqlite3_stmt* stmt;
    sqlite3_int64 last_id;
    //const char* sql = "INSERT INTO users (name) VALUES ('Bob') RETURNING id;";

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            last_id = sqlite3_column_int64(stmt, 0);
            std::cout << "Inserted row with USERID: " << last_id << std::endl;
        }
    }
    //FIXME
    else {
        return 0;
    }
    
    //free
    sqlite3_finalize(stmt);
    uint16_t userid = (uint16_t) last_id;
    return userid;
}

std::vector<std::string> read_data(sqlite3* db)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    sqlite3_stmt* stmt;
    std::vector<std::string> rooms;
    std::string sql = "SELECT * FROM rooms;";

    int rc = sqlite3_prepare_v2( db, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK){
        std::cerr << "Failed to prepare statement: "<< sqlite3_errmsg(db)<< std::endl;
        return rooms;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        std::string room_name = (const char*) sqlite3_column_text(stmt, 0);

        std::cout << "Room Name: " << room_name<< std::endl;
        rooms.push_back(room_name);
    }

    sqlite3_finalize(stmt);
    return rooms;
}

int init_db(sqlite3* db){
    std::string create_table_sql =
    "CREATE TABLE IF NOT EXISTS users ("
    "userid INTEGER PRIMARY KEY AUTOINCREMENT, last_msg INTEGER"
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
    "room_name TEXT NOT NULL, userid INTEGER NOT NULL, primary key (room_name,userid)"
    "FOREIGN KEY (room_name) references rooms(room_name) ON DELETE CASCADE,"
    "FOREIGN KEY (userid) references users(userid) ON DELETE CASCADE);";
    
    if (!execute_sql(db, create_table_2))
    {
        sqlite3_close(db);
        return 0;
    }

    return 1;

}