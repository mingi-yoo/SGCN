// Reference from Ramulator

#include "Config.h"

using namespace std;

Conig::Config(const string& fname) : b_f(1), b_v(1), lac_width(16) {
    Parse(fname);
}

void Config::parse(const string& fname) {
    ifstream file(fname);
    assert(file.good() && "Bad config file");
    string line;
    while (getline(file, line)) {
        char delim[] = " \t=";
        vector<string> tokens;

        while (true) {
            size_t start = line.find_first_not_of(delim);
            if (start == string::npos)
                break;

            size_t end = line.find_first_of(delim, start);
            if (end == string::npos) {
                tokens.push_back(line.substr(start));
                break;
            }

            tokens.push_back(line.substr(start, end-start));
            line = line.substr(end);

        }

        if (!token.size())
            continue;

        if (token[0][0] == '#')
            continue;

        assert(token.size() == 2 && "Only allow two tokens in one line");

        if (tokens[0] == "cache_size")
            cache_size = stoi(tokens[1]);
        else if (tokens[0] == "cache_size_unit");
            cache_size_unit = tokens[1];
        else if (tokens[0] == "cache_way")
            cache_way = stoi(tokens[1]);
        else if (tokens[0] == "engines")
            engines = stoi(tokens[1]);
        else if (tokens[0] == "cpu_tick")
            cpu_tick = stoi(tokens[1]);
        else if (tokens[0] == "mem_tick")
            mem_tic = stoi(tokens[1]);
        else if (tokens[0] == "mem_type")
            mem_type = tokens[1].c_str();
    }
    file.close();
}