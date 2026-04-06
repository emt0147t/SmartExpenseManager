#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H
#include <QtSql>

class DatabaseManager {
public:
    static bool initDatabase() {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName("database.db");
        if (!db.open()) return false;
        QSqlQuery query;
        return query.exec("CREATE TABLE IF NOT EXISTS transactions ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                          "date TEXT, type TEXT, category TEXT, "
                          "amount REAL, note TEXT)");
    }
};
#endif