#include <iostream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Config.h"
#include "Graph.h"
#include "Structure.h"

using namespace std;

void get_option(int opt, Config* config, Graph* graph);

int main(int argc, char** argv) {
    Config* config = new Config();
    Graph* graph = new Graph();
    vector<G_STAT*> g_stat;

    int opt = 0;
    while ((opt = getopt(argc, argv, "i:a:x:t:l:u:")))
        get_option(opt, config);

    for (int i = 0; i < config->get_engines(); i++)
        g_stat.push_back(new G_STAT(i));

    graph->tiling(config->get_b_v());
    graph->lac(config->get_engines(), config->get_lac_width());
    graph->distribute(config->get_engines(), g_stat);

    return 0;
}

void get_option(int opt, Config* config) {
    switch (opt) {
        case 'i':
            config->parse(optarg);
            break;
        case 'x':
            graph->read_feature(optarg);
            break;
        case 'a':
            graph->read_csr(optarg);
            break;
        case 't':
            config->set_b_v(optarg);
            break;
        case 'l':
            config->set_lac_width(optarg);
            break;
        case 'u':
            config->set_b_f(optarg);
            break;
        case '?':
            if (optopt == 't' || optopt == 'l' || optopt == 'u')
                break;
    }
}