#ifndef __STRUCTURE_H__
#define __STRUCTURE_H__

#include <vector>
#include <cmath>

using namespace std;

class G_STAT {
public:
    G_STAT() {}
    G_STAT(int n) {int id = n;}
    void set_subgraph(vector<int>& row_ptr, vector<int>& col_idx) {
        this->row_ptr.swap(row_ptr);
        this->col_idx.swap(col_idx);
    }
    int set_addr(int start) {
        int row_size = ceil((float)row_ptr.size() / 16)
        int col_size = ceil((float)col_idx.size() / 16)
        row_start_addr = start * 64;
        col_start_addr = row_start_addr + row_size * 64;

        return (start + row_size + col_size);
    }

private:
    int id;
    vector<int> row_ptr;
    vector<int> col_idx;
    uint64_t row_start_addr;
    uint64_t col_start_addr;
};

class F_STAT {
public:
    F_STAT() {}
    get_bicsr(int row, int fold) {
        if (bicsr[row][fold] == 0)
            return false;
        else 
            return true;
    }
private:
    vector<vector<int>> bicsr;
    uint64_t f_start_addr;
};

#endif  