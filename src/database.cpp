#include "database.h"

/*
 * query strings:
 *  - Create new table: CREATE table_name (key_types) (data_types)
 *    Types: INT, LONGLONG, DOUBLE, BOOL, STRING[SIZE]
 *
 *  - Insert: INSERT table_name (key) (data)
 */
std::string Database::query(std::string q) {

    std::string res;

    StringParser parser(q);

    if (parser.hasNext()) {
        std::string type = to_upper(parser.next());

        if (type == "CREATE") {

            std::string name = parser.next();

            std::string keyStr = parser.next();
            std::string dataStr = parser.next();

            if (name != "" && keyStr != "" && dataStr != "") {

                StringParser keyParser(keyStr);
                StringParser dataParser(dataStr);

                PersistentBTree tree;
                DataStructure keySt(keyParser.tokenize());
                DataStructure dataSt(dataParser.tokenize());
                tree.create(name, keySt, dataSt);

            }

        }
        else if (type == "INSERT") {

            std::string name = parser.next();

            PersistentBTree tree;
            tree.open(name);

            if (tree.is_open())
            {
                DataType key = DataType(tree.GetKeyStructure(), NULL);
                DataType data = DataType(tree.GetDataStructure(), NULL);

                char * keyBuf = new char[key.GetSize()];
                char * dataBuf = new char[data.GetSize()];

                key.SetData(keyBuf);
                data.SetData(dataBuf);

                std::string keyStr = parser.next();
                std::string dataStr = parser.next();

                if (name != "" && keyStr != "" && dataStr != "") {

                    StringParser keyParser(keyStr.substr(1, keyStr.length()-2));
                    StringParser dataParser(dataStr.substr(1, dataStr.length()-2));

                    int i = 0;
                    while (keyParser.hasNext()) {
                        key.SetData(i, keyParser.next());
                    }

                    i = 0;
                    while (dataParser.hasNext()) {
                        data.SetData(i, dataParser.next());
                    }

                    tree.insert(key, data);

                }

                delete keyBuf;
                delete dataBuf;
            }
        }
        else if (type == "GET") {

            std::string name = parser.next();

            PersistentBTree tree;
            tree.open(name);

            if (tree.is_open())
            {
                DataType key = DataType(tree.GetKeyStructure(), NULL);

                char * keyBuf = new char[key.GetSize()];

                key.SetData(keyBuf);

                std::string keyStr = parser.next();

                if (name != "" && keyStr != "") {

                    StringParser keyParser(keyStr.substr(1, keyStr.length()-2));

                    int i = 0;
                    while (keyParser.hasNext()) {
                        key.SetData(i, keyParser.next());
                    }

                    DataType data = tree.find(key).data();

                }

                delete keyBuf;
            }
        }
    }

    return res;

}


