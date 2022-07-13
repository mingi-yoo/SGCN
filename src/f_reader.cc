
#include "f_reader.h"

using namespace std;

extern ArchInfo arch_info;
extern DataInfo data_info;
extern LogInfo log_info;
extern vector<DataIndex> data_index;

extern vector<vector<int>> row_ptr;
extern vector<vector<int>> col_idx;

extern vector<vector<int>> xw_of_idx;

extern set<int> davc_list;

extern int total_write_blk;

int f_pass;

FeatureReader::FeatureReader(Memory* mem, Cache* cah) {
	this->mem = mem;
	this->cah = cah;

	mode = X_CMP;

	// initialize status variables
	for (int i = 0; i < arch_info.n_of_engine; i++) {
		FR_STAT stat;

		stat.cur_f.is_last = false;
		stat.cur_f.dst = -1;
		stat.cur_f.v_fold = 0;
		stat.cur_f.cur_col_idx = 0;

		stat.pq_read_need = true;
		stat.on_off = false;

		frs.push_back(stat);

		// pq is pending queue : they need to request feature
		pq.push_back(queue<EdgeInfo> ());
		// tbp is to be passed queue : they check one more: is really can pass?
		tbp.push_back(vector<vector<uint64_t>> ());
	}

	f_requested = 0;
	f_pass_end = false;

	// they need for debug; if you need, use this
	f_pass_cnt = 0;
	p_get_cnt = 0;
}

FeatureReader::~FeatureReader() {}

void FeatureReader::ModeChange(F_MODE mode) {
	// change gnn process mode
	// 0: compressed
	this->mode = mode;

	if (mode == X_CMP)
		cout<<"DO X Compressed MODE"<<endl;
	else if (mode == MAT)
		cout<<"DO MAT (Original SnF) MODE"<<endl;
	else if (mode == CSR)
		cout<<"DO X NAIVE CSR MODE"<<endl;
	else if (mode == X_FULL_CMP)
		cout<<"DO X Fully Compressed MODE"<<endl;
}

void FeatureReader::EnterFtoList(EdgeInfo ei) {
	pq[ei.id].push(ei);
}

void FeatureReader::ConsumePendingQ(int id) {
	// status update to next edge info
	EdgeInfo ei = pq[id].front();
	pq[id].pop();

	frs[id].cur_f.is_last = ei.is_last;
	frs[id].cur_f.dst = ei.dst;
	frs[id].cur_f.v_fold = ei.v_fold;
	frs[id].cur_f.cur_col_idx = 0;

	frs[id].pq_read_need = false;

	// cout<<"ID: "<<id<<endl;
}

void FeatureReader::ReadNext(int id) {
	if (tbp[id].size() >= 512)
		return;

	if (frs[id].pq_read_need && !pq[id].empty())
		ConsumePendingQ(id);
	else if (frs[id].pq_read_need && pq[id].empty())
		return;

	// pass vector has 3 elements
	// pass = {current col index, (feature address to get|current col index if feature alread exist), is_last}

	if (mode == X_CMP) {
		// NAIVE : IDEAL (no overflow searching cost)
		if (frs[id].cur_f.cur_col_idx != arch_info.urb && f_requested < REQUEST_LIMIT) {
			// Get address for next feature
			uint64_t f_addr = ReturnAddress(frs[id].cur_f.dst, frs[id].cur_f.cur_col_idx, frs[id].cur_f.v_fold, 0);
			// set pass[0] = col
			frs[id].pass.push_back(frs[id].cur_f.dst);

			// if f_addr has max value, it means there is no fragment - do not need to check cache
			if (f_addr != numeric_limits<uint64_t>::max()) {
				// if cache has feature, then set pass[1] = col
				if (cah->Access(f_addr))
					frs[id].pass.push_back(frs[id].cur_f.dst);
				else {
					// cout << "Maximum set size: " << requested.max_size() << endl;
					// else, request if need and set pass[1] = addr
					if (requested.find(f_addr) == requested.end()) {
						requested.insert(f_addr);
						mem->AddTransaction({f_addr, READ});
						f_requested++;
						if (frs[id].cur_f.cur_col_idx >= arch_info.urb/2) {
							mem->ms.f_frag_access++;
						}
					}
					frs[id].pass.push_back(f_addr);
				}
			}
			else {
				if (!frs[id].cur_f.is_last) {
					frs[id].pq_read_need = true;
					frs[id].pass.clear();
					return;
				}
				else
					frs[id].pass.push_back(frs[id].cur_f.dst);
			}
				

			// update current index and insert data to tbp 
			frs[id].cur_f.cur_col_idx++;
			frs[id].pass.push_back(frs[id].cur_f.is_last);
			tbp[id].push_back(frs[id].pass);
			frs[id].pass.clear();

			// we check two cases
			// first, pass current column urb times
			// second, if the last v_fold, last xw value is passed
			// satisfied with one of them, need to get next edge info
			if (frs[id].cur_f.cur_col_idx == arch_info.urb || (frs[id].cur_f.v_fold * arch_info.urb + frs[id].cur_f.cur_col_idx) >= total_write_blk)
				frs[id].pq_read_need = true;
		}
	}
	else if (mode == MAT) {
		// it is very similar to X_CMP. so skip comments
		if (frs[id].cur_f.cur_col_idx != arch_info.urb && f_requested < REQUEST_LIMIT) {

			frs[id].pass.push_back(frs[id].cur_f.dst);

			if (davc_list.find(frs[id].cur_f.dst) == davc_list.end()) {
				uint64_t f_addr = ReturnAddress(frs[id].cur_f.dst, frs[id].cur_f.cur_col_idx, frs[id].cur_f.v_fold, 0);
			
				if (cah->Access(f_addr))
					frs[id].pass.push_back(frs[id].cur_f.dst);
				else {
					// cout << "Maximum set size: " << requested.max_size() << endl;
					if (requested.find(f_addr) == requested.end()) {
						requested.insert(f_addr);
						mem->AddTransaction({f_addr, READ});
						f_requested++;
					}
					frs[id].pass.push_back(f_addr);
				}
			}
			else
				frs[id].pass.push_back(frs[id].cur_f.dst);

			frs[id].cur_f.cur_col_idx++;
			frs[id].pass.push_back(frs[id].cur_f.is_last);
			tbp[id].push_back(frs[id].pass);
			frs[id].pass.clear();

			if (frs[id].cur_f.cur_col_idx == arch_info.urb)
				frs[id].pq_read_need = true;
		}
	}
	else if (mode == CSR) {
		if (frs[id].cur_f.cur_col_idx != arch_info.urb && f_requested < REQUEST_LIMIT) {
			if (frs[id].cur_f.cur_col_idx == 0 && !frs[id].on_off) {
				uint64_t row_addr = ReturnAddress(frs[id].cur_f.dst, -1, frs[id].cur_f.v_fold, 0);

				if (!cah->Access(row_addr)) {
					if (requested.find(row_addr) == requested.end()) {
						requested.insert(row_addr);
						mem->AddTransaction({row_addr, READ});
						f_requested++;
						//cout<<"ADDR: "<<hex<<row_addr<<dec<<" requested"<<endl;
					}
					return;
				}
			}
		
			if (!frs[id].on_off) {
				frs[id].pass.push_back(frs[id].cur_f.dst);
				//cout<<"ID: "<<id<<", pass 0: "<<frs[id].pass[0]<<endl;
			}
				
			uint64_t f_offset = ReturnAddress(frs[id].cur_f.dst, frs[id].cur_f.cur_col_idx, frs[id].cur_f.v_fold, 0);

			if (f_offset != numeric_limits<uint64_t>::max()) {
				if (!frs[id].on_off) {
					uint64_t val_addr = arch_info.xw_value_addr_start + f_offset;
					if (cah->Access(val_addr))
						frs[id].pass.push_back(frs[id].cur_f.dst);
					else {
						// cout << "Maximum set size: " << requested.max_size() << endl;
						// else, request if need and set pass[1] = addr
						if (requested.find(val_addr) == requested.end()) {
							requested.insert(val_addr);
							mem->AddTransaction({val_addr, READ});
							f_requested++;

							//cout<<"ADDR: "<<hex<<val_addr<<dec<<" requested"<<endl;
						}
						frs[id].pass.push_back(val_addr);
						//cout<<"ID: "<<id<<", pass 1: "<<frs[id].pass[1]<<endl;
					}
				}
				else {
					uint64_t col_addr = arch_info.xw_col_addr_start + f_offset;
					if (cah->Access(col_addr))
						frs[id].pass.push_back(frs[id].cur_f.dst);
					else {
						// cout << "Maximum set size: " << requested.max_size() << endl;
						// else, request if need and set pass[1] = addr
						if (requested.find(col_addr) == requested.end()) {
							requested.insert(col_addr);
							mem->AddTransaction({col_addr, READ});
							f_requested++;

							//cout<<"ADDR: "<<hex<<col_addr<<dec<<" requested"<<endl;
						}
						frs[id].pass.push_back(col_addr);
						//cout<<"ID: "<<id<<", pass 2: "<<frs[id].pass[2]<<endl;
					}
				}
			}
			else 
				frs[id].pass.push_back(frs[id].cur_f.dst);
				

			if (frs[id].on_off) {
				frs[id].cur_f.cur_col_idx++;
				frs[id].pass.push_back(frs[id].cur_f.is_last);
				tbp[id].push_back(frs[id].pass);

				frs[id].pass.clear();

				if (frs[id].cur_f.cur_col_idx == arch_info.urb)
					frs[id].pq_read_need = true;
			}
			
			frs[id].on_off = !frs[id].on_off;

		}
	}
	else if (mode == X_FULL_CMP) {
		if (frs[id].cur_f.cur_col_idx != arch_info.urb && f_requested < REQUEST_LIMIT) {
			uint64_t address = ReturnAddress(frs[id].cur_f.dst, frs[id].cur_f.cur_col_idx, frs[id].cur_f.v_fold, 0);

			if (address != 0) {
				if (cah->Access(address))
					frs[id].pass.push_back(frs[id].cur_f.dst);
				else {
					if (requested.find(address) == requested.end()) {
						requested.insert(address);
						mem->AddTransaction({address, READ});
						f_requested++;
						p_get_cnt++;
						//cout<<"REQ: "<<p_get_cnt<<endl;
					}
					frs[id].pass.push_back(address);
				}
			}
			else
				frs[id].pass.push_back(frs[id].cur_f.dst);
		
			address = ReturnAddress(frs[id].cur_f.dst, frs[id].cur_f.cur_col_idx, frs[id].cur_f.v_fold, 1);

			if (address != 0) {
				if (cah->Access(address))
					frs[id].pass.push_back(frs[id].cur_f.dst);
				else {
					if (requested.find(address) == requested.end()) {
						requested.insert(address);
						mem->AddTransaction({address, READ});
						f_requested++;
						mem->ms.f_frag_access++;
					}
					frs[id].pass.push_back(address);
				}
				
			}
			else
				frs[id].pass.push_back(frs[id].cur_f.dst);

			frs[id].cur_f.cur_col_idx++;
			frs[id].pass.push_back(frs[id].cur_f.is_last);
			tbp[id].push_back(frs[id].pass);
			frs[id].pass.clear();

			if (frs[id].cur_f.cur_col_idx == arch_info.urb)
				frs[id].pq_read_need = true;
			

			
		}
	}
}

void FeatureReader::FeatureReceive() {
	while (true){
		// get feature from memory
		// if there is no return, memory return max value
		uint64_t address = mem->GetFeature();

		if (address == numeric_limits<uint64_t>::max())
			break;

		// insert cache and remove from request list
		cah->Insert(address);
		requested.erase(address);
		f_requested--;
		f_pass_cnt++;
		//cout<<"GET: "<<f_pass_cnt<<endl;
	}
}

vector<uint64_t> FeatureReader::PassFtoSIMD(int id) { 
	// if there is no pass data or cannot pass
	// return empty vector
	if (tbp[id].empty())
		return vector<uint64_t>();

	vector<uint64_t> pass = tbp[id].front();
	bool can_pass = true;

	// check it can be passed.
	if (mode == X_CMP || mode == MAT) {
		if (pass[1] >= arch_info.xw_ele_addr_start) {
			if (cah->Access(pass[1])) {
				pass[1] = pass[0];
			}
			else {
				if (requested.find(pass[1]) == requested.end()) {
					requested.insert(pass[1]);
					mem->AddTransaction({pass[1], READ});
					f_requested++;
				}
				can_pass = false;
			}
		}
	}
	else if (mode == CSR) {
		if (pass[1] >= arch_info.xw_value_addr_start) {
			if (cah->Access(pass[1])) {
				pass[1] = pass[0];
			}
			else {
				if (requested.find(pass[1]) == requested.end()) {
					requested.insert(pass[1]);
					mem->AddTransaction({pass[1], READ});
					f_requested++;
				}
				can_pass = false;
			}
		}

		if (pass[2] >= arch_info.xw_col_addr_start) {
			if (cah->Access(pass[2])) {
				pass[2] = pass[0];
			}
			else {
				if (requested.find(pass[2]) == requested.end()) {
					requested.insert(pass[2]);
					mem->AddTransaction({pass[2], READ});
					f_requested++;
				}
				can_pass = false;
			}
		}
	}
	else if (mode == X_FULL_CMP) {
		
		if (pass[1] >= arch_info.xw_ele_addr_start) {
			if (cah->Access(pass[1])) {
				pass[1] = pass[0];
			}
			else {
				if (requested.find(pass[1]) == requested.end()) {
					requested.insert(pass[1]);
					mem->AddTransaction({pass[1], READ});
					f_requested++;
				}
				can_pass = false;
			}
		}

		if (pass[2] >= arch_info.xw_ele_addr_start) {
			if (cah->Access(pass[2])) {
				pass[2] = pass[0];
			}
			else {
				if (requested.find(pass[2]) == requested.end()) {
					requested.insert(pass[2]);
					mem->AddTransaction({pass[2], READ});
					f_requested++;
				}
				can_pass = false;
			}
		}
		
	}
	

	if (can_pass) {
		tbp[id].erase(tbp[id].begin());
		f_pass++;
		//cout << "ID: " << id << ", VERTEX: " << pass[0] << ",  f_pass_cnt: " << f_pass << endl;
		
		return pass;
	}
	else {
		return vector<uint64_t>();
	}
		
}

uint64_t FeatureReader::ReturnAddress(int row, int col, int fold, bool bi_csr) {
	uint64_t address;

	if (mode == X_CMP) {
		int th = arch_info.urb / 2;
		if (col < th || (col >= th && xw_of_idx[fold][row] != -1)) {
			// TODO : extend to x_unit > 32
			address = arch_info.xw_ele_addr_start;
			uint64_t temp = (uint64_t)fold * arch_info.urb;
			temp *= data_info.x_h * CACHE_LINE_BYTE;
			address += temp;

			temp =(uint64_t)row * arch_info.urb;
			temp *= CACHE_LINE_BYTE;
			address += temp;

			address += (uint64_t)col * CACHE_LINE_BYTE;
		}
		else 
			address = numeric_limits<uint64_t>::max();
	}
	else if (mode == MAT) {
		address = arch_info.xw_ele_addr_start;
		uint64_t temp = (uint64_t)fold * arch_info.urb;
		temp *= data_info.x_h * CACHE_LINE_BYTE;
		address += temp;

		temp =(uint64_t)row * arch_info.urb;
		temp *= CACHE_LINE_BYTE;
		address += temp;

		address += (uint64_t)col * CACHE_LINE_BYTE;
		
	}
	else if (mode == CSR) {
		if (col == -1) {
			address = arch_info.xw_row_addr_start;
			uint64_t temp = (uint64_t)(row / CACHE_LINE_COUNT) * CACHE_LINE_BYTE;

			address += temp;
		}
		else {
			address = 0;
			int start = xw_of_idx[0][row];
			int end = xw_of_idx[0][row+1];

			if (start == end)
				return numeric_limits<uint64_t>::max();

			int start_align = start / CACHE_LINE_COUNT;
			int end_align = end / CACHE_LINE_COUNT;

			if (start_align + col > end_align)
				return numeric_limits<uint64_t>::max();

			uint64_t temp = (start_align + col) * CACHE_LINE_BYTE;

			address += temp;

		}
	}
	else if (mode ==X_FULL_CMP) {
		int start = fold * arch_info.urb * CACHE_LINE_COUNT + col * CACHE_LINE_COUNT;
		int end = start + CACHE_LINE_COUNT;
		int idx_1 = -1;
		int idx_2 = -1;

		if (bi_csr == 0)
		{
			for (int i = 0; i < xw_of_idx[row].size(); i++) {
				if (xw_of_idx[row][i] > start) {
					idx_1 = i;
					break;
				}
			}
			address = arch_info.xw_ele_addr_start;

			uint64_t temp = (uint64_t)row * arch_info.urb;
			temp *= arch_info.bf * CACHE_LINE_BYTE;
			temp += idx_1 * CACHE_LINE_BYTE;
			address += temp;
		}
		else if (bi_csr == 1) {
			for (int i = 0; i < xw_of_idx[row].size(); i++) {
				if (xw_of_idx[row][i] > end) {
					idx_2 = i;
					break;
				}
			}
			address = arch_info.xw_ele_addr_start;

			uint64_t temp = (uint64_t)row * arch_info.urb;
			temp *= arch_info.bf * CACHE_LINE_BYTE;
			temp += idx_2 * CACHE_LINE_BYTE;
			address += temp;
		}
		
	}

	return address;
}