#include <config/Config.hpp>

bool Config::initialize(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Error: config file not found / problem opening file"
                  << std::endl;
        return false;
    }

    std::string line;
    int lineNum = 1;
    while (std::getline(file, line))
    {
        std::string trimmed = trim(line);
        if (!trimmed.empty())
        {
            std::string key, value;
            if (!splitKV(trimmed, key, value))
            {
                std::cerr << "Error: malformed line " << lineNum
                          << ": " << trimmed << std::endl;
                return false;
            }

            if (!parseEntry(key, value, lineNum))
                return false;
        }
        ++lineNum;
    }

    if (!validateCrossFields())
        return false;

    std::cout << "Program successfully initialized\n";
    return true;
}

std::string Config::trim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool Config::splitKV(const std::string& line, std::string& key, std::string& value)
{
    auto pos = line.find_first_of(" \t");
    if (pos == std::string::npos) return false;
    key   = trim(line.substr(0, pos));
    value = trim(line.substr(pos + 1));
    return !key.empty() && !value.empty();
}

std::string Config::stripQuotes(const std::string& s)
{
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

bool Config::parseEntry(const std::string& key, const std::string& value, int lineNum)
{
    try
    {
        if (key == "num-cpu")
        {
            unsigned long v = std::stoul(value);
            if (v < 1 || v > 128)
            {
                std::cerr << "Error: num-cpu must be 1..128 (line "
                          << lineNum << ")" << std::endl;
                return false;
            }
            numCpu = static_cast<uint8_t>(v);
        }
        else if (key == "scheduler")
        {
            std::string s = stripQuotes(value);
            if (s != "fcfs" && s != "rr")
            {
                std::cerr << "Error: scheduler must be \"fcfs\" or \"rr\" (line "
                          << lineNum << ")" << std::endl;
                return false;
            }
            scheduler = s;
        }
        else if (key == "quantum-cycles")
        {
            unsigned long v = std::stoul(value);
            if (v < 1)
            {
                std::cerr << "Error: quantum-cycles must be >= 1 (line "
                          << lineNum << ")" << std::endl;
                return false;
            }
            quantumCycles = static_cast<uint32_t>(v);
        }
        else if (key == "batch-process-freq")
        {
            batchProcessFreq = static_cast<uint32_t>(std::stoul(value));
        }
        else if (key == "min-ins")
        {
            minIns = static_cast<uint32_t>(std::stoul(value));
        }
        else if (key == "max-ins")
        {
            maxIns = static_cast<uint32_t>(std::stoul(value));
        }
        else if (key == "delay-per-exec")
        {
            delayPerExec = static_cast<uint32_t>(std::stoul(value));
        }
        else
        {
            std::cerr << "Warning: unknown key '" << key << "' on line "
                      << lineNum << " (ignored)" << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: invalid value on line " << lineNum
                  << " (" << key << " = " << value << "): " << e.what()
                  << std::endl;
        return false;
    }
    return true;
}

bool Config::validateCrossFields() const
{
    if (minIns > maxIns)
    {
        std::cerr << "Error: min-ins (" << minIns
                  << ") cannot be greater than max-ins (" << maxIns << ")"
                  << std::endl;
        return false;
    }
    return true;
}