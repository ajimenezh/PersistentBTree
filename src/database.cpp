#include "database.h"

std::string Database::query(std::string & q) {

    std::string res;

    StringParser parser(q);

    if (parser.hasNext()) {
        std::string type = to_upper(parser.next());

        if (type == "CREATE") {

            std::string name = parser.next();

            std::string keyStr = parser.next();
            std::string dataStr = parser.next();

            if (name != "" && keyStr != "" && dataStr != dataStr) {

                StringParser keyParser(keyStr);
                StringParser dataParser(dataStr);

                PersistentBTree tree;
                DataStructure keySt(keyParser.tokenize());
                DataStructure dataSt(dataParser.tokenize());
                tree.create(name, keySt, dataSt);

            }

        }
    }

    return res;

}


