/*//#include "persistentbtree.h"
//#include <map>
#include <iostream>
#include <algorithm>
#include <utility>
#include <chrono>
using namespace std;
using namespace std::chrono;

#include "MemoryPage.h"

int main() {

	MemoryPageManager mgr();

// 	PersistentBTree<int, int> m;
// 	m.setNodeSize(64, 64);
// 	map<int, int> comp;
// 
// 	for (int i = 0; i < 100000; i+=2) {
// 		m.insert(i, 2 * i);
// 		comp.insert(make_pair(i, 2 * i));
// 	}
// 
// 	cout << "Start check..." << endl;
// 
// 	for (int i = 0; i < 100000; i += 2) {
// 		if (!m.exists(i))
// 		{
// 			cout << "Error: Key " << i << " not found." << endl;
// 		}
// 	}
// 
// 	for (int i = 1; i < 100000; i += 2) {
// 		if (m.exists(i))
// 		{
// 			cout << "Error: Key " << i << " found." << endl;
// 		}
// 	}
// 
// 	cout << "Some deletes..." << endl;
// 
// 	for (int j = 0; j < 100; j++) {
// 		int p = rand() % 100000;
// 
// 		m.erase(p);
// 		comp.erase(p);
// 
// 		for (int i = 0; i < 1000; i++) 
// 		{
// 			if (m.exists(i) != (comp.find(i) != comp.end()))
// 			{
// 				cout << "Error: Key " << i << " not found." << endl;
// 			}
// 		}
// 
// 	}
// 
// 	cout << "Check finished." << endl;
// 
// 	cout << "Measure performance..." << endl;
// 
// 	m.clear();
// 
// 	cout << "B-Tree+" << endl;
// 
// 	high_resolution_clock::time_point t1 = high_resolution_clock::now();
// 
// 	for (int i = 0; i < 100000; i += 2) {
// 		m.insert(i, 2 * i);
// 	}
// 
// 	for (int j = 0; j < 100; j++) {
// 		int p = rand() % 100000;
// 
// 		m.erase(p);
// 
// 	}
// 
// 	high_resolution_clock::time_point t2 = high_resolution_clock::now();
// 
// 	auto duration = duration_cast<microseconds>(t2 - t1).count();
// 
// 	cout << duration << endl;
// 
// 	comp.clear();
// 
// 	cout << "STD map" << endl;
// 
// 	t1 = high_resolution_clock::now();
// 
// 	for (int i = 0; i < 100000; i += 2) {
// 		comp.insert(make_pair(i, 2 * i));
// 	}
// 
// 	for (int j = 0; j < 100; j++) {
// 		int p = rand() % 100000;
// 
// 		comp.erase(p);
// 
// 	}
// 
// 	t2 = high_resolution_clock::now();
// 
// 	duration = duration_cast<microseconds>(t2 - t1).count();
// 
// 	cout << duration << endl;

	system("pause");

	return 0;

}
*/

#include <boost/iostreams/device/mapped_file.hpp>
#include <iostream>
#include <iterator>
#include <algorithm>
#include "MemoryPage.h"
#include "persistentbtree.h"

// void *append(int fd, char const *data, size_t nbytes, void *map, size_t &len)
// {
// 	// TODO: check for errors here!
// 	size_t written = write(fd, data, nbytes);
// 	munmap(map, len);
// 	len += written;
// 	return mmap(NULL, len, PROT_READ, 0, fd, 0);
// }

void basic_test() {

    MemoryPageManager<int,int> pageMgr;

    pageMgr.Open("test");

    MemoryNode<int, int> page = pageMgr.InsertPage();

    page->level = 5;

    page = pageMgr.InsertPage();

    pageMgr.DeletePage(0);

    page = pageMgr.InsertPage();
}

int main() {

    PersistentBTree<int,int> tree;

    //tree.insert(5, 4);

    int p = tree.find(5).data();

    std::cout<<p<<std::endl;

}
