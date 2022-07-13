#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <cmath>

#include "Structure"

using namespace std;

class Graph {
public:
    Graph() {}
    Graph(const string& gname, string& fname);
    void read_csr(const string& gname);
    void read_feature(const string& fname);
    void tiling(int b_v);
    void lac(int n, int lac_width);
    int distribute(int n, vector<G_STAT*>& g_stat);
    void compression(int comp_width, F_STAT& f_stat);
    int get_num_nodes() const {return num_nodes;}
    int get_num_edges() const {return num_edges;}
private:
    int num_nodes;
    int num_edges;
    vector<int> row_ptr;
    vector<int> col_idx;
    vector<vector<int>> features; 
};

#endif
