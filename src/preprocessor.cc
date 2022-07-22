#include "preprocessor.h"

using namespace std;

vector<vector<int>> row_ptr;
vector<vector<int>> col_idx;

vector<vector<uint64_t>> x_to_addr;

ArchInfo arch_info;
DataInfo data_info;
LogInfo log_info;
vector<DataIndex> data_index;

uint64_t total_write;
int total_write_blk;

set<int> davc_list;

vector<string> split(const string& str, const string& delim);

Preprocessor::Preprocessor(F_PATH &f_path) {
	ReadIni(f_path.ini);
	ParseIni(f_path.csv_path);
	ReadData(f_path.a_data, f_path.xw_data);
	ReadDAVC(f_path.davc_path);

	for (int i = 0; i < arch_info.n_of_engine; i++)
		data_index.push_back(DataIndex());

	// A preprocessing
	LAC();
	Tiling();


	for (int i = 0; i < data_info.x_h; i++)
		x_to_addr.push_back(vector<uint64_t> ());

	// X preprocessing
	TransXW();
	AddressMapping();

	for (int i = 0; i < data_index.size(); i++)
		total_write += data_index[i].total_write;

}

Preprocessor::~Preprocessor() {}

void Preprocessor::ReadIni(string ini) {
	ifstream ini_file(ini);
	if (ini_file.is_open()) {
		string line;
		while (getline(ini_file, line)) {
			string delimiter = " = ";
			if (string::npos == line.find(delimiter))
				delimiter = "=";
			string token1 = line.substr(0, line.find(delimiter));
			string token2 = line.substr(line.find(delimiter) + delimiter.length(), line.length());
			m_table[token1] = token2; 
		}
		ini_file.close();
	}
	else
		throw invalid_argument("Cannot open inifile");
}

bool Preprocessor::Contain(string name) {
	if (m_table.find(name) == m_table.end())
		return false;
	return true;
}

string Preprocessor::GetString(string name) {
	if (Contain(name)) {
		if (m_table[name].find("\"") == string::npos)
			return m_table[name];
		else
			return m_table[name].substr(1, m_table[name].length() - 2);
	}
	else
		throw invalid_argument(name + " Not exist.");
}

int Preprocessor::GetInt(string name) {
	if (Contain(name))
		return stoi(m_table[name]);
	else
		throw invalid_argument(name + "Not exist.");
}

uint64_t Preprocessor::GetUint64(string name) {
	if (Contain(name))
		return stoull(m_table[name], NULL, 0);
	else
		throw invalid_argument(name + " Not exist.");
}

void Preprocessor::ParseIni(string csv_path) {
	arch_info.cache_size = GetInt("CacheSize");
	if (GetString("UnitSize") == "KB")
		arch_info.cache_size *= 1024;
	else
		arch_info.cache_size *= 1024 * 1024;

	arch_info.cache_way = GetInt("CacheWay");
	arch_info.cache_set = arch_info.cache_size / arch_info.cache_way / CACHE_LINE_BYTE;

	arch_info.n_of_engine = GetInt("NumberOfEngine");
	arch_info.bf = GetInt("BF");

	arch_info.d_value_addr_start = 0;

	arch_info.mem_type = GetString("MemoryType");

	/*
	 * Log information
	 */
	cout << "------------Save basic information into logger------------" << endl;
	log_info.engine_n = arch_info.n_of_engine; // engine num
	vector<string> mem_vec = split(arch_info.mem_type, "/");
	string ini_name = (string)mem_vec.back();
	vector<string> ini_vec = split(ini_name, "_");
	log_info.mem_type = (string)ini_vec.front();
	log_info.cache_size = (int)(arch_info.cache_size/(1024*1024)); // print cache size as MB
	log_info.bf = arch_info.bf;
}

void Preprocessor::ReadData(string a_data, string xw_data) {
	ifstream a_file(a_data);
	ifstream xw_file(xw_data);
	string line, temp;

	vector<int> row_ptr_temp;
	vector<int> col_idx_temp;

	// first, read graph
	// store in row_ptr[0] & col_idx[0]
	if (a_file.is_open()) {
		getline(a_file, line);
		stringstream ss(line);
		while (getline(ss, temp, ' '))
			row_ptr_temp.push_back(stoi(temp));

		ss.clear();
		getline(a_file, line);
		ss.str(line);
		while (getline(ss, temp, ' '))
			col_idx_temp.push_back(stoi(temp));

		a_file.close();
	}
	else
		throw invalid_argument("Cannot open A file");

	data_info.num_v = row_ptr_temp.size() - 1;
	data_info.num_e = col_idx_temp.size();

	row_ptr.push_back(row_ptr_temp);
	col_idx.push_back(col_idx_temp);

	data_info.a_h = row_ptr_temp.size() - 1;
	data_info.a_w = data_info.a_h;
	data_info.x_h = data_info.a_w;

	// second, read xw. it should be matrix
	// it stores in xw vector that is class private array. 
	// xw vector is only used for xw compression.
	if (xw_file.is_open()) {
		while (getline(xw_file, line)) {
			vector<int> xw_row;
			stringstream ss(line);
			while (getline(ss, temp, ' '))
				xw_row.push_back(stoi(temp));
			xw.push_back(xw_row);
		}

		xw_file.close();
	}
	else
		throw invalid_argument("Cannot open XW file");

	data_info.w_w = xw[0].size();
	data_info.w_h = data_info.w_w;
	data_info.x_w = data_info.w_h;

	// urb unit is cache line. (1 urb = read C(16) elements)
	data_info.total_urb = ceil((float)data_info.x_w / CACHE_LINE_COUNT);
	arch_info.urb = ceil((float)data_info.x_w / arch_info.bf / CACHE_LINE_COUNT);
	// log feature width and urb
	log_info.feature_length = data_info.w_w;
	log_info.urb = arch_info.urb;	
}

void Preprocessor::ReadDAVC(string davc_path) {
	if (davc_path == "")
		return;

	ifstream davc_file(davc_path);

	if (davc_file.is_open()) {
		string line, temp;
		getline(davc_file, line);
		stringstream ss(line);
		while (getline(ss, temp, ' '))
			davc_list.insert(stoi(temp));
		
		davc_file.close();
	} 
}

void Preprocessor::LAC() {
	int n = arch_info.n_of_engine;

	if (arch_info.lac_width == -1)
		arch_info.lac_width = ceil((float)data_info.a_h / n);
	if (arch_info.lac_width == 0)
		arch_info.lac_width = ceil((float)data_info.a_h / data_info.n_tiles / n);

	cout<<"LAC WIDTH : "<<arch_info.lac_width<<endl;

	// this process is next step of ReadData()
	// so, the original(non-distributed) graph is stored in 0th index.
	vector<int> row_ptr_origin = row_ptr[0];
	row_ptr.pop_back();
	vector<int> col_idx_origin = col_idx[0];
	col_idx.pop_back();


	// divide graph using lac.
	// each of engines has a csr of subgraph.
	vector<vector<int>> row_ptr_lac;
	vector<vector<int>> col_idx_lac;

	// initialize
	int *edge_acm = new int[n];
	for (int i = 0; i < n; i++) {
		row_ptr_lac.push_back(vector<int>(1));
		col_idx_lac.push_back(vector<int>());
		edge_acm[i] = 0;
	}

	// distribute data in lac_width chunks.
	// if row is 1000, width is 32 and the number of engines is 8, this vertex should in (1000/32)%8 = 7th engine. 
	for (int i = 0; i < row_ptr_origin.size() - 1; i++) {
		int lac_idx = (i / arch_info.lac_width) % n;
		edge_acm[lac_idx] += row_ptr_origin[i+1] - row_ptr_origin[i];
		row_ptr_lac[lac_idx].push_back(edge_acm[lac_idx]);
		for (int j = row_ptr_origin[i]; j < row_ptr_origin[i+1]; j++)
			col_idx_lac[lac_idx].push_back(col_idx_origin[j]);
	}

	// Calculate zero row

	for (int i = 0; i < n; i++) {
		int zero = 0;
		for (int j = 0; j < row_ptr_lac[i].size() - 1; j++) {
			// if there is vertex that has no edge, count up the zero row value.
			if (row_ptr_lac[i][j+1] == row_ptr_lac[i][j])
				zero++;
		}
		data_index[i].row = row_ptr_lac[i].size() - 1;
		data_index[i].zero_row = zero;
		data_index[i].total_write = (row_ptr_lac[i].size() - 1 - zero) * data_info.total_urb;
		
	}

	row_ptr = row_ptr_lac;
	col_idx = col_idx_lac;

	delete [] edge_acm;
}

void Preprocessor::Tiling() {
	// it will process only tile file exist.
	log_info.tiling_info = to_string(data_info.n_tiles);
	if (data_info.n_tiles > 1) {
		// this process is next step of LAC()
		vector<vector<int>> row_ptr_tiled;
		vector<vector<int>> col_idx_tiled;

		int n = arch_info.n_of_engine;

		for (int i = 0; i < n; i++) {
			row_ptr_tiled.push_back(vector<int> ());
			col_idx_tiled.push_back(vector<int> ());
		}

		// change tile data to tile accumulated data
		// ex) if tile data is '1 1 2 1 1 2'.
		//     tile accumulated data is '1 2 4 5 6 8'. 
		int t_acm = 0;
		vector<int> t_unit;

		for (int i = 0; i < data_info.n_tiles; i++) {
			t_acm += 1;
			t_unit.push_back(t_acm);
		}

		// for checking tile index, calculate tile unit(unit_v).
		int unit_v = ceil((float)data_info.a_w / t_acm);

		// do tiling to each of engines
		for (int i = 0; i < n; i++) {
			vector<vector<int>> v_list;
			vector<vector<int>> e_list;

			// v_list has the number of in-edges of each vertices
			// e_list has column indices of each vertices
			for (int j = 0; j < t_unit.size(); j++) {
				v_list.push_back(vector<int> ());
				e_list.push_back(vector<int> ());
			}

			// distribute columns to each tile
			// if t_unit[i] <= (col / unit_v) < t_unit[i+1], this column is in i-th tile.
			// edge_acm[i] is accumulation of edges. it is need for making row_ptr
			for (int j = 0; j < row_ptr[i].size() - 1; j++) {
				int *edge_acm = new int[t_unit.size()];
				for (int k = 0; k < t_unit.size(); k++)
					edge_acm[k] = 0;
				for (int k = row_ptr[i][j]; k < row_ptr[i][j+1]; k++) {
					int t_idx = col_idx[i][k] / unit_v;
					for (int l = 0; l < t_unit.size(); l++) {
						if (t_idx < t_unit[l]) {
							edge_acm[l]++;
							e_list[l].push_back(col_idx[i][k]);
							break;
						}
					}
				}
				for (int k = 0; k < t_unit.size(); k++)
					v_list[k].push_back(edge_acm[k]);

				delete [] edge_acm;
			}

			// Trans to CSR
			int v_acm = 0;
			row_ptr_tiled[i].push_back(0);

			for (int j = 0; j < t_unit.size(); j++) {
				for (int k = 0; k < v_list[j].size(); k++) {
					v_acm += v_list[j][k];
					row_ptr_tiled[i].push_back(v_acm);
				}
				for (int k = 0; k < e_list[j].size(); k++) {
					col_idx_tiled[i].push_back(e_list[j][k]);
				}
			}
		}

		// Calculate address & zero row
		// Address will be recalculated.
		uint64_t val_addr_start = 0;
		uint64_t col_addr_start = 0;
		uint64_t row_addr_start = 0; 

		for (int i = 0; i < n; i++) {
			int zero = 0;
			for (int j = 0; j < row_ptr_tiled[i].size() - 1; j++) {
				if (row_ptr_tiled[i][j+1] == row_ptr_tiled[i][j])
					// if there is vertex that has no edge, count up the zero row value.
					zero++;
			}
			data_index[i].row = row_ptr_tiled[i].size() - 1;
			data_index[i].zero_row = zero;
			data_index[i].total_write = (row_ptr_tiled[i].size() - 1 - zero) * data_info.total_urb;
		}

		row_ptr.clear();
		row_ptr = row_ptr_tiled;
		col_idx.clear();
		col_idx = col_idx_tiled;
	}
	// ofstream o("graph_tiled.txt");
	// o << 0 << " ";
	// int edge_acm = 0;
	// for (int i = 0; i < arch_info.n_of_engine; i++) {
	// 	for (int j = 1; j < row_ptr[i].size(); j++) {
	// 		o << edge_acm + row_ptr[i][j] <<" ";
	// 	}
	// 	edge_acm += row_ptr[i].back();
	// }
	// o<<endl;
	// for (int i = 0; i < arch_info.n_of_engine; i++) {
	// 	for (int j = 0; j < col_idx[i].size(); j++) {
	// 		o << col_idx[i][j] <<" ";
	// 	}
	// }
	// o<<endl;
}

void Preprocessor::TransXW() {
	if (arch_info.mode == X_CMP) {
		arch_info.urb = ceil((float)arch_info.x_unit / CACHE_LINE_COUNT);
		arch_info.bf = ceil((float)data_info.x_w / arch_info.x_unit);
		data_info.total_urb = arch_info.bf * arch_info.urb;
		
		for (int i = 0; i < data_info.x_h; i++) {
			int count = ceil((float)arch_info.x_unit / 32);
			for (int j = 1; j <= data_info.x_w; j++) {
				if (j % arch_info.x_unit == 0 || j == data_info.x_w) {
					int cur_urb;
					if (j == data_info.x_w)
						cur_urb = data_info.total_urb - ((arch_info.bf-1) * arch_info.urb);
					else
						cur_urb = arch_info.urb;
					for (int k = 0; k < cur_urb; k++) {
						if (count > 0)
							x_to_addr[i].push_back(1);
						else
							x_to_addr[i].push_back(0);
						
						count -= CACHE_LINE_COUNT;
					}
					count = ceil((float)arch_info.x_unit / 32);
				}
				if (xw[i][j] != 0)
					count++;
			}
		}
		for (int i = 0; i < arch_info.n_of_engine; i++)
			data_index[i].total_write = (row_ptr[i].size() - 1 - data_index[i].zero_row) * data_info.total_urb;
	}
	else if (arch_info.mode == MAT) {
		int unit = ceil((float)data_info.x_w / CACHE_LINE_COUNT);
		for (int i = 0; i < data_info.x_h; i++) {
			for (int j = 0; j < unit; j++) {
				x_to_addr[i].push_back(1);
			}
		}
	}
	else if (arch_info.mode == CSR) {
		for (int i = 0; i < data_info.x_h; i++) {
			x_to_addr[i].push_back(1);
			int x_acm = 0;
			for (int j = 0; j < data_info.x_w; j++) {
				if (xw[i][j] != 0)
					x_acm++;
			}
			int block = ceil((float)x_acm / CACHE_LINE_COUNT);
			for (int j = 0; j < block; j++) {
				x_to_addr[i].push_back(1);
				x_to_addr[i].push_back(1);
			}
			x_to_addr[i].push_back(x_acm);
		}
	}
	else if (arch_info.mode == X_FULL_CMP) {
		for (int i = 0; i < data_info.x_h; i++) {
			x_to_addr[i].push_back(1);
			int count = ceil((float)data_info.x_w / 32);
			for (int j = 0; j < data_info.x_w; j++) {
				if (xw[i][j] != 0)
					count++;
			}
			int block = ceil((float)count / CACHE_LINE_COUNT);
			for (int j = 0; j < block; j++)
				x_to_addr[i].push_back(1);
		}
	}

	// clear xw_mat vector
	vector<vector<int>>().swap(xw);
}

void Preprocessor::AddressMapping() {
	//Address Mapping for serial address
	//Address order: value -> row -> column -> xw (non-frag) -> xw (frag) -> axw
	uint64_t address = 0;
	vector<int> row_frags;
	vector<int> col_frags;

	for (int i = 0; i < arch_info.n_of_engine; i++) {
		int row_frag = ceil((float)row_ptr[i].size()/CACHE_LINE_COUNT);
		int col_frag = ceil((float)col_idx[i].size()/CACHE_LINE_COUNT);
		row_frags.push_back(row_frag);
		col_frags.push_back(col_frag);
	}

	// value address
	arch_info.d_value_addr_start = address;
	for (int i = 0; i < arch_info.n_of_engine; i++) {
		data_index[i].value_addr_start = address;
		address += col_frags[i] * CACHE_LINE_BYTE;
		data_index[i].value_addr_end = address;
	}

	// row address
	arch_info.a_row_addr_start = address;
	for (int i = 0; i < arch_info.n_of_engine; i++) {
		data_index[i].row_addr_start = address;
		address += row_frags[i] * CACHE_LINE_BYTE;
		data_index[i].row_addr_end = address;
	}

	// col address
	arch_info.a_col_addr_start = address;
	for (int i = 0; i < arch_info.n_of_engine; i++) {
		data_index[i].col_addr_start = address;
		address += col_frags[i] * CACHE_LINE_BYTE;
		data_index[i].col_addr_end = address;
	}

	// xw, axw address
	arch_info.xw_ele_addr_start = address;
	// mapping x address
	address = arch_info.xw_ele_addr_start;
	if (arch_info.mode == X_CMP || arch_info.mode == MAT) {
		for (int i = 0; i < arch_info.bf; i++) {
			for (int j = 0; j < data_info.x_h; j++) {
				for (int k = 0; k < arch_info.urb; k++) {
					int row = j;
					int col = i * arch_info.urb + k;
					if (col == data_info.total_urb)
						break;
					if (x_to_addr[row][col] == 1)
						x_to_addr[row][col]=address;
					address += CACHE_LINE_BYTE;
				}
			}
		}
	}
	else if (arch_info.mode == CSR) {
		// row ptr mapping
		for (int i = 1; i <= data_info.x_h; i++) {
			x_to_addr[i-1][0] = address;
			if (i % CACHE_LINE_COUNT == 0)
				address += CACHE_LINE_BYTE;
		}
		// col_idx mapping
		int x_acm = 0;
		for (int i = 0; i < data_info.x_h; i++) {
			int cnt = (x_to_addr[i].size() - 2) / 2;
			for (int j = 0; j < cnt; j++) {
				x_to_addr[i][j*2 + 1] = address;
				address += CACHE_LINE_BYTE;
			}
			x_acm += x_to_addr[i][x_to_addr[i].size()-1];
			if (x_acm % CACHE_LINE_COUNT != 0)
				address -= CACHE_LINE_BYTE;
		}
		// value mapping
		x_acm = 0;
		for (int i = 0; i < data_info.x_h; i++) {
			int cnt = (x_to_addr[i].size() - 2) / 2;
			for (int j = 0; j < cnt; j++) {
				x_to_addr[i][j*2 + 2] = address;
				address += CACHE_LINE_BYTE;
			}
			x_acm += x_to_addr[i][x_to_addr[i].size()-1];
			x_to_addr[i].pop_back();
			if (x_acm % CACHE_LINE_COUNT != 0)
				address -= CACHE_LINE_BYTE;
		}
	}	
	else if (arch_info.mode == X_FULL_CMP) {
		int x_change = CACHE_LINE_COUNT / ceil((float)data_info.x_w / 32);
		for (int i = 1; i <= data_info.x_h; i++) {
			x_to_addr[i-1][0] = address;
			if (i % x_change == 0)
				address += CACHE_LINE_BYTE;
		}
		for (int i = 0; i < data_info.x_h; i++) {
			for (int j = 0; j < x_to_addr[i].size(); j++) {
				x_to_addr[i][j] = address;
				address += CACHE_LINE_BYTE;
			}
		}
	}

	arch_info.axw_addr_start = address;

	// for (int i = 0; i < data_info.x_h; i++) {
	// 	for (int j = 0; j < x_to_addr[i].size(); j++)
	// 		cout<<hex<<x_to_addr[i][j]<<" ";
	// 	cout<<endl;
	// }
}

void Preprocessor::PrintStatus() {
	// ArchInfo check
	cout<<"---------------ArchInfo Check---------------"<<endl;
	cout<<"Cache size: "<<arch_info.cache_size<<" Byte"<<endl;
	cout<<"Cache way: "<<arch_info.cache_way<<endl;
	cout<<"Cache set: "<<arch_info.cache_set<<endl;
	cout<<"Number of engines: "<<arch_info.n_of_engine<<endl;
	cout<<"Bf: "<<arch_info.bf<<endl;
	cout<<"Unit Read Block: "<<arch_info.urb<<endl;
	cout<<"Total URB: "<<data_info.total_urb<<endl;
	cout<<"Value Start Address: "<<hex<<arch_info.d_value_addr_start<<endl;
	cout<<"Row Start Address: "<<hex<<arch_info.a_row_addr_start<<endl;
	cout<<"Column Start Address: "<<hex<<arch_info.a_col_addr_start<<endl;
	if (arch_info.mode == X_CMP || arch_info.mode == MAT || arch_info.mode == X_FULL_CMP)
		cout<<"XW Element Start Address: "<<hex<<arch_info.xw_ele_addr_start<<endl;
	else if (arch_info.mode == CSR) {
		cout<<"XW Value Start Address: "<<hex<<arch_info.xw_value_addr_start<<endl;
		cout<<"XW Row Start Address: "<<hex<<arch_info.xw_row_addr_start<<endl;
		cout<<"XW Column Start Address: "<<hex<<arch_info.xw_col_addr_start<<endl;
	}
	cout<<"AXW Start Address: "<<hex<<arch_info.axw_addr_start<<dec<<endl;
	cout<<endl;

	// DataInfo check
	cout<<"---------------DataInfo Check---------------"<<endl;
	cout<<"A height: "<<data_info.a_h<<endl;
	cout<<"A width: "<<data_info.a_w<<endl;
	cout<<"X height: "<<data_info.x_h<<endl;
	cout<<"X width: "<<data_info.x_w<<endl;
	cout<<"W height: "<<data_info.w_h<<endl;
	cout<<"W width: "<<data_info.w_h<<endl;
	cout<<endl;

	// Graph LAC & Tiling check
	cout<<"------------Graph Tile&LAC Check------------"<<endl;
	for (int i = 0; i < arch_info.n_of_engine; i++) {
		cout<<"Engine "<<i<<endl;
		cout<<"ROW PTR size: "<<row_ptr[i].size()<<endl;
		cout<<"ROW PTR last value: "<<row_ptr[i].back()<<endl;
		cout<<"COL IDX size: "<<col_idx[i].size()<<endl;
		cout<<"Zero rows: "<<data_index[i].zero_row<<endl;
		cout<<"Value Start Address: "<<hex<<data_index[i].value_addr_start<<endl;
		cout<<"Value End Address: "<<hex<<data_index[i].value_addr_end<<endl;
		cout<<"Row Start Address: "<<hex<<data_index[i].row_addr_start<<endl;
		cout<<"Row End Address: "<<hex<<data_index[i].row_addr_end<<endl;
		cout<<"Column Start Address: "<<hex<<data_index[i].col_addr_start<<endl;
		cout<<"Column End Address: "<<hex<<data_index[i].col_addr_end<<dec<<endl;
		cout<<endl;
	}
}

// String Split Funtion for argparse
vector<string> split(const string& str, const string& delim) {
    vector<string> tokens;
    size_t prev = 0, pos = 0;
    do {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}