#include <iostream>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

vector<string> get_input() {
    string line, word;
    vector<string> buffer;
    
    cout << "root:\\>";
    if (getline(cin, line)) {
        stringstream ss(line);
        while (ss >> word) {
            buffer.push_back(word);
        }
    }

    if (buffer.size() == 0)
        buffer.push_back("");

    return buffer;
}