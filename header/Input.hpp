#include <iostream>
#include <string>
#include <vector>
#include <sstream>


std::vector<std::string> getInput() {
    std::string line, word;
    std::vector<std::string> buffer;
    
    std::cout << "root:\\>";
    if (std::getline(std::cin, line)) {
        std::stringstream ss(line);
        while (ss >> word) {
            buffer.push_back(word);
        }
    }

    if (buffer.size() == 0)
        buffer.push_back("");

    return buffer;
}