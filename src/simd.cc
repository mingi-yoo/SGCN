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

	ss.cur_row_idx = 0;
	ss.cur_col_idx = 0;
	ss.cur_v_fold = 0;

	ss.cur_get_f = 0;

	ss.simd_end = false;
}

SIMD::~SIMD() {}

void SIMD::GetFeature(vector<uint64_t> f) {
	if (f.back() == 1) {
		// f.back() = is_last
		uint64_t address = GetAddress();
		//cout<<hex<<address<<dec<<endl;
		//total_write--;
		mem->AddTransaction({address, WRITE});
	}

	ss.cur_get_f++;

	if (ss.cur_get_f == arch_info.urb || (ss.cur_v_fold * arch_info.urb + ss.cur_col_idx) >= total_write_blk) {
		ss.cur_col_idx++;
		if (f.back() == 1) {
			ss.cur_row_idx++;
			while (ss.cur_row_idx < row_ptr[id].size() - 1) {
				if (row_ptr[id][ss.cur_row_idx+1] != row_ptr[id][ss.cur_row_idx])
					break;
				ss.cur_row_idx++;
			}
			if (ss.cur_row_idx == row_ptr[id].size() - 1) {
				ss.cur_row_idx = 0;
				ss.cur_col_idx = 0;
				ss.cur_v_fold++;
				if (ss.cur_v_fold == arch_info.bf)
					ss.simd_end = true;
			}
		}
		ss.cur_get_f = 0;
	}

}

uint64_t SIMD::GetAddress() {
	uint64_t address = arch_info.axw_addr_start;

	address += cnt * CACHE_LINE_BYTE;
	cnt++;
	
	return address;


}