
import argparse
import subprocess
import os
import random
from wsgiref.headers import tspecials

"""
SGCN 전체 데이터셋 queue 넣는 스크립트
m은 구분해서 넣어야함

usage: python3 tsp_runner.py -dataset youtube pokec livejournal -tile_path run_tiles/ -config_path run_configs/ -csv /nfs/home/seonbinara/sgcn_yt_pk_lj.csv -m 0 -u 28 32 60 64 -n 36
"""

if __name__ == "__main__":
    """
    ARGUMENT PARSING
    """
    parser = argparse.ArgumentParser(description="SGCN TSP Script...")
    parser.add_argument('-dataset', default=[], nargs='+', required=True, help="Put Dataset List (e.g. -dataset youtube pokec liverjournal citation2 products orkut) ...")
    parser.add_argument('-tile', required=True, nargs='+', type=int, help="number of tiles")
    parser.add_argument('-config_path', required=True, help="Tile files Path")
    parser.add_argument('-csv', required=True, help="CSV File Saving Path")
    parser.add_argument('-n', required=True, default=36, type=int, help="TSP Runner Limit (default=15)")
    parser.add_argument('-m', required=True, type=int, help='Which mode to run (0:SGCN, 1:SnF)')
    parser.add_argument('-u', nargs='+', required=False, type=int, default=[], help='unit for SGCN')
    args = parser.parse_args()

    # SCGN needs unit ('-u')
    if args.m == 0:
        assert len(args.u) >= 1
    if args.m == 1:
        assert len(args.u) == 0
        args.u.append(28) # default is 28

    """
    DATASET PARSING
    """
    # dataset_path_list
    datasets = []
    datasets_x = []
    # argument parsing of datasets.... which datasets to run
    for element in args.dataset:
        if element == "livejournal":
            datasets.append("/nfs/home_old/gnn_data/livejournal.txt")
            datasets_x.append("/nfs/home_old/gnn_data/livejournal_xw.mat")
        elif element == "youtube":
            datasets.append("/nfs/home_old/gnn_data/youtube.txt")
            datasets_x.append("/nfs/home_old/gnn_data/youtube_xw.mat")
        elif element == "pokec":
            datasets.append("/nfs/home_old/gnn_data/pokec.txt")
            datasets_x.append("/nfs/home_old/gnn_data/pokec_xw.mat")
        elif element == "products":
            datasets.append("/nfs/home_old/gnn_data/products.txt")
            datasets_x.append("/nfs/home_old/gnn_data/products_xw.mat")
        elif element == "citation2":
            datasets.append("/nfs/home_old/gnn_data/citation2.txt")
            datasets_x.append("/nfs/home_old/gnn_data/citation2_xw.mat")
        elif element == "orkut":
            datasets.append("/nfs/home_old/gnn_data/orkut.txt")
            datasets_x.append("/nfs/home_old/gnn_data/orkut_xw.mat")
        elif element == "nell":
            datasets.append("/nfs/home_old/gnn_data/gnn_data/nell.txt")
            datasets_x.append("/nfs/home_old/gnn_data/gnn_data/nell_xw.mat")
        elif element == "cora":
            datasets.append("/nfs/home_old/gnn_data/cora.txt")
            datasets_x.append("/nfs/home_old/gnn_data/cora_xw.mat")
        elif element == "wiki":
            datasets.append("/nfs/home_old/gnn_data/wiki.txt")
            datasets_x.append("/nfs/home_old/gnn_data/wiki_xw.mat")

    """
    CONFIG PARSING
    """
    # Config Files to run list
    config_path = args.config_path
    config_list = os.listdir(config_path)
    configs = []
    for config in config_list:
        configs.append(config_path + config)

    """
    TILE PARSING
    """
    tile_path = args.tile_path
    tile_list = os.listdir(tile_path)
    tiles = []
    for tile in tile_list:
        tiles.append(tile_path + tile)

    """
    CSV PATH PARSING
    """
    csv_path = args.csv

    """
    RUN QUEUE MAKING
    """
    # Queue List
    run_q = []

    for i, dataset in enumerate(datasets):
        for config in configs:
            for tile in tiles:
                for unit in args.u:
                    run = ['tsp']
                    run.append('./sim')
                    run.append('-i')
                    run.append(config)
                    run.append('-a')
                    run.append(dataset)
                    run.append('-x')
                    run.append(datasets_x[i])
                    run.append('-c')
                    run.append(csv_path)
                    run.append('-t')
                    run.append(tile)
                    run.append('-m')
                    run.append(str(args.m))
                    run.append('-u')
                    run.append(str(unit))
                    run.append('-l')
                    run.append(0)
                    run_q.append(run)

    random.shuffle(run_q)

    print("===== TOTAL ", str(len(run_q)), " ELEMENTS WILL BE QUEUED =====")

    # for debugging
    # for element in run_q:
    #     print(element)

    """
    APPEND QUEUES
    """
    print("===== APPEND COMMAND TO TSP QUEUE =====")
    for i, com in enumerate(run_q):
        run_com = subprocess.Popen(" ".join(com), shell=True)
        run_com.wait()

    """
    FINISHED...
    """
    print("===== QUEUEING FINISHED =====")