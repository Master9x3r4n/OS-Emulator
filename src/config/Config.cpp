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

    Config parsed;
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

            if (!parsed.parseEntry(key, value, lineNum))
                return false;
        }
        ++lineNum;
    }

    if (!parsed.validateCrossFields())
        return false;

    *this = parsed;
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

Config::NumResult Config::parseUnsigned(const std::string& text, uint64_t min,
                                        uint64_t max, uint64_t& out)
{
    if (text.empty())
        return NumResult::NotANumber;

    uint64_t v = 0;
    bool overflow = false;
    for (char c : text)
    {
        if (c < '0' || c > '9')
            return NumResult::NotANumber; 

        const uint64_t digit = static_cast<uint64_t>(c - '0');
        if (v > (UINT64_MAX - digit) / 10)
            overflow = true;
        else
            v = v * 10 + digit;
    }

    if (overflow || v < min || v > max)
        return NumResult::OutOfRange;

    out = v;
    return NumResult::Ok;
}

bool Config::readNumber(const std::string& key, const std::string& value,
                        int lineNum, uint64_t min, uint64_t max,
                        uint64_t& out) const
{
    switch (parseUnsigned(value, min, max, out))
    {
    case NumResult::Ok:
        return true;
    case NumResult::NotANumber:
        std::cerr << "Error: " << key << " must be a whole number, got '"
                  << value << "' (line " << lineNum << ")" << std::endl;
        return false;
    case NumResult::OutOfRange:
        std::cerr << "Error: " << key << " must be between " << min << " and "
                  << max << " (line " << lineNum << ")" << std::endl;
        return false;
    }
    return false;
}

bool Config::parseEntry(const std::string& key, const std::string& value, int lineNum)
{
    uint64_t v = 0;

    if (key == "num-cpu")
    {
        if (!readNumber(key, value, lineNum, 1, 128, v)) return false;
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
        if (!readNumber(key, value, lineNum, 1, kMaxValue, v)) return false;
        quantumCycles = v;
    }
    else if (key == "batch-process-freq")
    {
        if (!readNumber(key, value, lineNum, 1, kMaxValue, v)) return false;
        batchProcessFreq = v;
    }
    else if (key == "min-ins")
    {
        if (!readNumber(key, value, lineNum, 1, kMaxValue, v)) return false;
        minIns = v;
    }
    else if (key == "max-ins")
    {
        if (!readNumber(key, value, lineNum, 1, kMaxValue, v)) return false;
        maxIns = v;
    }
    else if (key == "delay-per-exec")
    {
        if (!readNumber(key, value, lineNum, 0, kMaxValue, v)) return false;
        delayPerExec = v;
    }
    else
    {
        std::cerr << "Warning: unknown key '" << key << "' on line "
                  << lineNum << " (ignored)" << std::endl;
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
