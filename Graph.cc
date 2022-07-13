#include "Graph.h"

Graph::Graph(const string& gname, string& fname) {
    ifstream a_file(gname);
    ifstream x_file(fname);
    string line, temp;

    assert(a_file.good() && "Bad graph file");
    assert(x_file.good() && "Bad feature file");

    getline(a_file, line);
    stringstream ss(line);
    while (getline(ss, temp, ' '))
        row_ptr.push_back(stoi(temp));

    ss.clear();
    getline(a_file, line);
    ss.str(line);
    while (getline(ss, temp, ' '))
        col_idx.push_back(stoi(temp));

    a_file.close();

    while (getline(x_file, line)) {
        ss.clear();
        ss.str(line);
        while (getline(ss, temp, ' '))
            features.push_back(stoi(temp));
    }

    x_file.close();

    num_nodes = row_ptr.size() - 1;
    num_edges = col_idx.size();
}

void Graph::tiling(int b_v) {
    int unit_v = ceil((float)num_nodes / b_v);

    vector<vector<int>> row_ptr_tiled;
    vector<vector<int>> col_idx_tiled;

    for (int i = 0; i < b_v; i++) {
        row_ptr_tiled.push_back(vector<int> ());
        col_idx_tiled.push_back(vector<int> ());
    }

    vector<int> edge_acm(b_v);

    for (int i = 1; i < row_ptr.size(); i++) {
        for (int j = row_ptr[i-1]; j < row_ptr[i]; j++) {
            int tid = col_idx[j] / uint_v;
            edge_acm[tid]++;
            col_idx_tiled[tid].push_back(col_idx[j]);
        }
        for (int j = 0; j < b_v; j++) {
            row_ptr_tiled[tid].push_back(edge_acm[j]);
            edge_acm[j] = 0;
        }
    }

    row_ptr.swap(vector<int> ());
    col_idx.swap(vector<int> ());

    row_ptr.push_back(0);

    for (int i = 0; i < b_v; i++) {
        for (int j = 0; j < row_ptr_tiled[i].size(); j++) {
            edge_acm[0] += row_ptr_tiled[i][j];
            row_ptr.push_back(edge_acm[0]);
        }
        for (int j = 0; j < col_idx_tiled[i].size(); j++) {
            col_idx.push_back(col_idx_tiled[i][j]);
        }
    }
}

void Graph::lac(int n, int lac_width) {
    int stride = n * lac_width;

    vector<int> row_ptr_lac;
    vector<int> col_idx_lac;

    row_ptr_lac.push_back(0);

    int edge_acm = 0;

    for (int i = 0; i < n; i++) {
        int start = i * lac_width;
        while (start < num_nodes) {
           for (int j = start; j < start + lac_width; j++) {
                if (j >= num_nodes)
                    break;
                edge_acm += row_ptr[j + 1] - row_ptr[j];
                row_ptr_lac.push_back(edge_acm);
                for (int k = row_ptr[j]; k < row_ptr[j+1]; k++)
                    col_idx_lac.push_back(col_idx[k]);
            }
            start += stride;
        }
    }

    row_ptr.swap(row_ptr_lac);
    col_idx.swap(col_idx_lac);
}

int Graph::distribute(int n, vector<G_STAT*>& g_stat) {
    int unit_v = ceil((float)num_nodes / b_v);
    int start = 0;
    for (int i = 0; i < n; i++) {
        vector<int> row_temp;
        vector<int> col_temp;
        row_temp.push_back(row_ptr[i*unit_v]);

        for (int j = i*unit_v+1; j <= (i+1)*unit_v; j++) {
            if (j > num_nodes)
                break;
            row_temp.push_back(row_ptr[j]);
        }
        for (int j = row_temp.front(); j < row_temp.back(); j++)
            col_temp.push_back(col_idx[j]);

        g_stat[i]->set_subgraph(row_temp, col_temp);
        start = g_stat[i]->set_addr(start);
    }
}