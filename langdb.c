/*
   langdb.c — SQLite module for Lang

   Usage:
     import "db"
     let db = db_open("app.db")
     db_exec(db, "CREATE TABLE IF NOT EXISTS users (id INTEGER, name TEXT)")
     db_exec(db, "INSERT INTO users VALUES (1, 'Aakash')")
     let rows = db_query(db, "SELECT * FROM users")
     print rows[0]["name"]
     db_close(db)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "value.h"

/* Store open DB handles by index */
#define MAX_DBS 16
static sqlite3 *g_dbs[MAX_DBS];
static int      g_db_count = 0;

static Value std_db_open(Value *a, int n) {
    if (n < 1 || a[0].type != VAL_STRING) return val_null();
    if (g_db_count >= MAX_DBS) return val_null();
    sqlite3 *db;
    if (sqlite3_open(a[0].as.s, &db) != SQLITE_OK) {
        fprintf(stderr, "db_open error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return val_null();
    }
    g_dbs[g_db_count] = db;
    return val_int(g_db_count++);
}

static Value std_db_close(Value *a, int n) {
    if (n < 1 || a[0].type != VAL_INT) return val_null();
    int idx = (int)a[0].as.i;
    if (idx < 0 || idx >= g_db_count || !g_dbs[idx]) return val_null();
    sqlite3_close(g_dbs[idx]);
    g_dbs[idx] = NULL;
    return val_null();
}

static Value std_db_exec(Value *a, int n) {
    if (n < 2 || a[0].type != VAL_INT || a[1].type != VAL_STRING) return val_bool(0);
    int idx = (int)a[0].as.i;
    if (idx < 0 || idx >= g_db_count || !g_dbs[idx]) return val_bool(0);
    char *errmsg = NULL;
    int rc = sqlite3_exec(g_dbs[idx], a[1].as.s, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_exec error: %s\n", errmsg);
        sqlite3_free(errmsg);
        return val_bool(0);
    }
    return val_bool(1);
}

static Value std_db_query(Value *a, int n) {
    if (n < 2 || a[0].type != VAL_INT || a[1].type != VAL_STRING) return val_array_empty();
    int idx = (int)a[0].as.i;
    if (idx < 0 || idx >= g_db_count || !g_dbs[idx]) return val_array_empty();

    sqlite3_stmt *stmt;
    Value result = val_array_empty();

    if (sqlite3_prepare_v2(g_dbs[idx], a[1].as.s, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "db_query error: %s\n", sqlite3_errmsg(g_dbs[idx]));
        return result;
    }

    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Value row = val_dict_empty();
        for (int i = 0; i < cols; i++) {
            const char *col_name = sqlite3_column_name(stmt, i);
            Value cell;
            switch (sqlite3_column_type(stmt, i)) {
                case SQLITE_INTEGER:
                    cell = val_int(sqlite3_column_int64(stmt, i)); break;
                case SQLITE_FLOAT:
                    cell = val_float(sqlite3_column_double(stmt, i)); break;
                case SQLITE_NULL:
                    cell = val_null(); break;
                default: {
                    const char *text = (const char*)sqlite3_column_text(stmt, i);
                    cell = val_string(text ? text : ""); break;
                }
            }
            val_dict_set(&row, col_name, cell);
        }
        val_array_push(&result, row);
    }
    sqlite3_finalize(stmt);
    return result;
}

static Value std_db_last_id(Value *a, int n) {
    if (n < 1 || a[0].type != VAL_INT) return val_int(0);
    int idx = (int)a[0].as.i;
    if (idx < 0 || idx >= g_db_count || !g_dbs[idx]) return val_int(0);
    return val_int((long long)sqlite3_last_insert_rowid(g_dbs[idx]));
}

void stdlib_load_db(Env *env) {
    env_set(env, "db_open",    val_native(std_db_open));
    env_set(env, "db_close",   val_native(std_db_close));
    env_set(env, "db_exec",    val_native(std_db_exec));
    env_set(env, "db_query",   val_native(std_db_query));
    env_set(env, "db_last_id", val_native(std_db_last_id));
}
