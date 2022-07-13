#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cassert>

using namespace std;

class Config {

public:
    Config() : b_f(1), b_v(1), lac_width(16) {}
    Config(const string& fname);
    void parse(const string& fname);
    string operator [] (const string& name) const {
        if (options.find(name) != options.end())
            return (options.find(name))->second;
        else
            return "";
    }

    bool contains(const std::string& name) const {
        if (options.find(name) != options.end())
            return true;
        else
            return false;
    }

    void add (const string& name, const string& value) {
        if (!contains(name))
            options.insert(make_pair(name, value));
        else
            cout<<"option "<<name<<" is already set."<<endl;
    }

    int get_cache_size() const {return cache_size;}
    string get_cache_size_unit() const {return cache_size_unit;}
    int get_cache_way() const {return cache_way;}
    int get_engines() const {return engines;}
    int get_cpu_tick() const {return cpu_tick;}
    int get_mem_tic() const {return mem_tick;}
    string get_mem_type() const {return mem_type;}

    int set_b_f(const string& value) {
        add ("b_f", value);
        b_f = stoi(value);
    }

    int set_b_v(const string& value) {
        add ("b_v", value);
        b_v = stoi(value);
    }

    int set_lac_width(const string& value) {
        add ("lac_width", value);
        lac_width = stoi(value);
    }

    int get_b_f() const {return b_f;}
    int get_b_v() const {return b_v;}
    int get_lac_width() const {return lac_width};

private:
    map<string, string> options;

    // hardware configuration
    int cache_size;
    string cache_size_unit;
    int cache_way;
    int engines;
    int cpu_tick;
    int mem_tick;
    string mem_type;

    // process configuration
    int b_f;
    int b_v;
    int lac_width;
};

#endif