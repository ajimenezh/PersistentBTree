
#ifndef SRC_DATABASE_H_
#define SRC_DATABASE_H_

#include "string_utils.h"
#include "persistentbtree.h"

class Database {
public:

    static Database & GET_DATABASE() {
        static Database database;
        return database;
    }

private:
    Database() {}

public:
    Database(Database const&) = delete;
    void operator=(Database const&) = delete;

    std::string query(std::string q);

};


#endif /* SRC_DATABASE_H_ */
