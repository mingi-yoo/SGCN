#ifndef __SIMD_H__
#define __SIMD_H__

#include "common.h"
#include "memory.h"

using namespace std;

struct SIMD_STAT {
	int cur_row_idx;
	int cur_col_idx;
	int cur_v_fold;

	int cur_get_f;
	int cur_f;

	bool simd_end;
};

class SIMD {
public:
	SIMD_STAT ss;
	SIMD(int id, Memory* mem);
	~SIMD();

	void GetFeature(vector<uint64_t> f);
private:
	int id;
	Memory* mem;
	
	uint64_t GetAddress();
};

#endif