#include "controller.h"

using namespace std;

extern ArchInfo arch_info;
extern DataInfo data_info;
extern LogInfo log_info;
extern vector<DataIndex> data_index;
extern uint64_t total_write;
extern uint64_t tq_get;
uint64_t xw_get;

extern set<int> davc_list;

Controller::Controller() {
	mem = new Memory(arch_info.mem_type, arch_info.mem_output);
	cah = new Cache();
	for (int i = 0; i < arch_info.n_of_engine; i++){
		gr.push_back(new GraphReader(i, mem));
		si.push_back(new SIMD(i, mem));
	}
	fr = new FeatureReader(mem, cah);
}

Controller::~Controller() {
	for (int i = 0; i < arch_info.n_of_engine; i++) {
		delete gr[i];
		delete si[i];
	}
	delete fr;
	delete cah;
	delete mem;
}

uint64_t Controller::GCNInference(int f_mode) {
	uint64_t cycle = DAVCRead();
	uint64_t f_pass = 0;
	int prop_count = 9;

	uint64_t unit_write = ceil((float)total_write / 10);
	uint64_t ori_write = total_write;

	fr->ModeChange((F_MODE)f_mode);

	while (true) {
		int over_ctrs = 0;
		
		
		for (int i = 0; i < arch_info.n_of_engine; i++) {
			// GraphReader Work
			if (!gr[i]->grf.v_req_over)
				gr[i]->VertexRequest();
			if (!gr[i]->grf.e_req_over)
				gr[i]->EdgeRequest();
			if (!gr[i]->grf.reci_over)
				gr[i]->GraphReceive();

			if (!gr[i]->grf.pass_over) {
				EdgeInfo ei = gr[i]->NextFeature();

				// FeatureReader Work
				if (ei.dst != -1) 
					fr->EnterFtoList(ei);
			}	
			vector<uint64_t> pass;

			fr->ReadNext(i);
			pass = fr->PassFtoSIMD(i);

			if (!pass.empty()) {
				f_pass++;
				si[i]->GetFeature(pass);

				//cout<<"F PASS: "<<f_pass<<endl;
				//cout<<pass[0]<<", "<<pass[1]<<", "<<pass[2]<<", "<<pass[3]<<endl;
			}
		}

		fr->FeatureReceive();
		mem->ConsumeTransaction();
		mem->UpdateCycle();
		cycle++;

		if (total_write <= unit_write * prop_count) {
			cout<<"PROGRESS: "<<ori_write - total_write<<"/"<<ori_write<<endl;
			prop_count--;
		}

		
		if (total_write == 0)
			break;
		
	}
	
	return cycle;
}

uint64_t Controller::Combination() {
	uint64_t read_cycle = 0;
	uint64_t compute_cycle = 0;

	uint64_t start_addr = arch_info.xw_ele_addr_start;

	int cnt = 0;

	uint64_t total_get = data_info.x_h * arch_info.urb * arch_info.bf;
	for (int i = 0; i < data_info.x_h; i++) {
		for (int j = 0; j < arch_info.urb * arch_info.bf; j++) {
			mem->AddTransaction({start_addr, READ});
			mem->ConsumeTransaction();
			cnt++;
			if (cnt % arch_info.n_of_engine == 0) {
				mem->UpdateCycle();
				read_cycle++;
			}
			
		}
	}
	while (xw_get < total_get) {
		mem->ConsumeTransaction();
		mem->UpdateCycle();
		read_cycle++;
	}

	int total_w_repeat = ceil((float)data_info.w_w/(32 * arch_info.n_of_engine));
	int total_x_repeat = ceil((float)data_info.x_h/(32 * arch_info.n_of_engine));

	compute_cycle += data_info.x_w + (32 * arch_info.n_of_engine);
	compute_cycle *= total_w_repeat;
	compute_cycle *= total_x_repeat;

	cout<<"RESULT. mem: "<<read_cycle<<", cal: "<<compute_cycle<<endl;

	return max(read_cycle, compute_cycle);
}

uint64_t Controller::DAVCRead() {
	if (davc_list.empty())
		return 0;
	else {
		uint64_t cycle = 0;
		cout<<"DAVC READ"<<endl;
		uint64_t address = arch_info.xw_ele_addr_start;
		int width = arch_info.urb * arch_info.bf;
		for (int i = 0; i < davc_list.size() * width; i++) {
			mem->AddTransaction({address, READ});
			mem->ConsumeTransaction();
			mem->UpdateCycle();
			mem->GetFeature();
			cycle++;
		}
		while (xw_get < davc_list.size() * width) {
			mem->ConsumeTransaction();
			mem->UpdateCycle();
			mem->GetFeature();
			cycle++;
		}

		cout<<"DAVC READ OVER. TOTAL Cycle: :"<<cycle<<endl;
		xw_get = 0;
		return cycle;
	}
}

void Controller::PrintMemoryStats() {
	cout<<"Total Graph Access: "<<mem->ms.g_access<<endl;
	log_info.g_access = mem->ms.g_access;
	cout<<"Total Feature(Fragmentation) Access: "<<mem->ms.f_frag_access<<endl;
	log_info.f_frag_access = mem->ms.f_frag_access;
	cout<<"Total Feature(Element) Access: "<<mem->ms.f_ele_access<<endl;
	log_info.f_ele_access = mem->ms.f_ele_access;
	cout<<"Total Output Access: "<<mem->ms.o_access<<endl;
	log_info.o_access = mem->ms.o_access;

	cout<<"Total get transaction: "<<tq_get<<endl;

	mem->PrintStats();
}