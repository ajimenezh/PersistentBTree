
#ifndef SRC_DATABASE_H_
#define SRC_DATABASE_H_


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

};


#endif /* SRC_DATABASE_H_ */
