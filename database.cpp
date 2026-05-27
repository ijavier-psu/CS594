
#include "database.h"

std::mutex db_mutex;

std::unordered_map<std::string, uint32_t> opcodes{
    {"ERR", 0x10000001U}
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

void insert_data(sqlite3* db, int thread_id)
{
    for (int i = 0; i < 5; ++i)
    {
        std::lock_guard<std::mutex> lock(db_mutex);

        std::string sql =
            "INSERT INTO users (name, age) VALUES ('Thread_" +
            std::to_string(thread_id) +
            "', " +
            std::to_string(20 + thread_id) +
            ");";

        if (execute_sql(db, sql))
        {
            std::cout << "[Writer " << thread_id
                      << "] Inserted row " << i << std::endl;
        }
    }
}

void read_data(sqlite3* db, int thread_id)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    std::string sql = "SELECT user_id, username FROM users;";

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

        const unsigned char* username =
            sqlite3_column_text(stmt, 1);


        std::cout << "ID: " << user_id
                  << ", Name: " << username
                  << std::endl;
    }

    sqlite3_finalize(stmt);
}