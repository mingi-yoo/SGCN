#include "simd.h"

using namespace std;

extern ArchInfo arch_info;
extern DataInfo data_info;
extern LogInfo log_info;
extern vector<DataIndex> data_index;

extern vector<vector<int>> row_ptr;
extern vector<vector<int>> col_idx;

extern int total_write_blk;
extern uint64_t total_write;

int cnt;

SIMD::SIMD(int id, Memory* mem) {
	this->id = id;
	this->mem = mem;

	ss.axw_addr_start = arch_info.axw_addr_start;
	ss.axw_addr_start += (uint64_t)ceil((float)data_info.a_h/arch_info.n_of_engine) * id * arch_info.bf * arch_info.urb * CACHE_LINE_BYTE;
	ss.cur_row_idx = 0;
	ss.cur_v_fold = 0;

	ss.past_f = -1;

	ss.simd_end = false;
}

SIMD::~SIMD() {}

void SIMD::GetFeature(int f) {
	if (f == 0) {
		uint64_t address = GetAddress();
		wq.push(address);
		ss.cur_v_fold++;
	}
	else if (f == 1) {
		if (ss.past_f == 0) {
			if (ss.cur_get_f < arch_info.urb) {
				while (ss.cur_v_fold < arch_info.urb) {
					uint64_t address = GetAddress();
					wq.push(address);
					ss.cur_v_fold++;
				}
			}
			ss.cur_v_fold = 0;
			ss.cur_row_idx++;
		}
		
	}
	ss.past_f = f;
}

uint64_t SIMD::GetAddress() {
	uint64_t address = ss.axw_addr_start;
	address += cnt * CACHE_LINE_BYTE;
	cnt++;
	
	return address;
}