#include <iostream>
#include <fstream> 
#include <string> 
#include <cstdint>
#include <ranges>
#include <string_view>
#include <vector>

struct Config {
    uint8_t numCpu; // range from 1 to 128
    std::string scheduler; // only either "fcfs" or "rr"
    uint32_t quantumCycles; // range from 1 to 2^32
    uint32_t batchProcessFreq;
    uint32_t minIns;
    uint32_t maxIns;
    uint32_t delayPerExec;
};

static std::string trim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool split_kv(const std::string& line, std::string& key, std::string& value)
{
    auto pos = line.find_first_of(" \t");
    if (pos == std::string::npos) return false;
    key = trim(line.substr(0, pos));
    value = trim(line.substr(pos + 1));
    return !key.empty() && !value.empty();
}

static std::string strip_quotes(const std::string& s)
{
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

bool initializeConfig(Config& config)
{
    // Open and verify file path
    std::ifstream file("config.txt");
    if (!file.is_open())
    {
        std::cerr << "Error: config file not found / problem opening file" << std::endl;
        return false; // return 0 for error (initialized = false)
    }

    // read lines from config.txt and store values into Config
    std::string line;
    int j = 1;
    while (std::getline(file, line))
    {
        std::string trimmed = trim(line);
        if (!trimmed.empty())
        {
            // split line to k-v pair
            std::string key, value;
            if (!split_kv(trimmed, key, value))
            {
                std::cerr << "Error: malformed line " << j 
                    << ": " << trimmed << std::endl;
                return false;
            }

            // I got too lazy to implement check for each command so I generated it :nyea:
            try
            {
                if (key == "num-cpu")
                {
                    unsigned long v = stoul(value);
                    if (v < 1 || v > 128)
                    {
                        std::cerr << "Error: num-cpu must be 1..128 (line " << j 
                            << ")" << std::endl;
                        return false;
                    }
                    config.numCpu = static_cast<uint8_t>(v);
                }
                else if (key == "scheduler")
                {
                    std::string s = strip_quotes(value);
                    if (s != "fcfs" && s != "rr")
                    {
                        std::cerr << "Error: scheduler must be \"fcfs\" or \"rr\" (line "
                            << j << ")" << std::endl;
                        return false;
                    }
                    config.scheduler = s;
                }
                else if (key == "quantum-cycles")
                {
                    unsigned long v = stoul(value);
                    if (v < 1)
                    {
                        std::cerr << "Error: quantum-cycles must be >= 1 (line " << j << ")" << std::endl;
                        return false;
                    }
                    config.quantumCycles = static_cast<uint32_t>(v);
                }
                else if (key == "batch-process-freq")
                {
                    config.batchProcessFreq = static_cast<uint32_t>(stoul(value));
                }
                else if (key == "min-ins")
                {
                    config.minIns = static_cast<uint32_t>(stoul(value));
                }
                else if (key == "max-ins")
                {
                    config.maxIns = static_cast<uint32_t>(stoul(value));
                }
                else if (key == "delay-per-exec")
                {
                    config.delayPerExec = static_cast<uint32_t>(stoul(value));
                }
                else
                {
                    std::cerr << "Warning: unknown key '" << key << "' on line "
                        << j << " (ignored)" << std::endl;
                }
            } 
            catch (const std::exception& e) 
            {
                std::cerr << "Error: invalid value on line " << j
                    << " (" << key << " = " << value << "): " << e.what() << std::endl;
                return false;
            }
        }
    }

    // Cross-field validation
    if (config.minIns > config.maxIns)
    {
        std::cerr << "Error: min-ins (" << config.minIns
             << ") cannot be greater than max-ins (" << config.maxIns << ")" << std::endl;
        return false;
    }

    std::cout << "Program successfully initialized\n";
    return true; // return 1 for success (initialized = true)
}