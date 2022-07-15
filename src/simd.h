#ifndef __SIMD_H__
#define __SIMD_H__

#include "common.h"
#include "memory.h"

using namespace std;

struct SIMD_STAT {
	uint64_t axw_addr_start;
	int cur_row_idx;
	int cur_v_fold;

	int cur_f;

	int past_f;

	bool simd_end;
};

class SIMD {
public:
	SIMD_STAT ss;
	SIMD(int id, Memory* mem);
	~SIMD();

	void GetFeature(int f);
private:
	int id;
	Memory* mem;
	queue<uint64_t> wq;
	
	uint64_t GetAddress();
};

#endif