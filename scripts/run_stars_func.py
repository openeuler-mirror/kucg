# ***********************************************************************
# Copyright: (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
# Script for testing stars function
# Version: 1.0.0
# Usage: python run_stars_func.py
# Notice: 
#   (1) before run the script, please enable hmpi env variables
#   (2) before run the script, please set hostfile(e.g., hf_2) in current directory
# 
# ***********************************************************************

import time
import subprocess
import os
import sys
import logging

# prefix
coll_type = ["ibcast", "iallgatherv", "iscatterv", "ibarrier", "ialltoallv"]
algo_list_stars = [[1, 2, 3, 4], [1], [1], [1], [1]]
script_dir = os.getcwd()
log_dir = os.path.join(script_dir, "log")
if not os.path.exists(log_dir):
    os.makedirs(log_dir)
hostfile_path = os.path.join(script_dir, "hf_2")
if not os.path.exists(hostfile_path):
    logging.error("hf_2 does not exist, please set it first!")
    sys.exit()
log_path_1node = os.path.join(log_dir, "planc_stars_data_function_1node")
log_path_2nodes = os.path.join(log_dir, "planc_stars_data_function_2nodes")

# 1node
proc_num_list = [7, 19, 47]
max_rails_list = [3, 5]
for max_rails in max_rails_list:
    for proc_num in proc_num_list:
        if not os.path.exists(log_dir):
            os.mkdir(log_dir)
        with open(log_path_1node, 'a') as file_record:
            sys.stdout = file_record
            for idx in range(0, 5):
                coll = coll_type[idx]
                coll_upper = coll.upper()
                validate_flag = "-c"
                if idx == 3:
                    validate_flag = " "
                for algo_stars in algo_list_stars[idx]:
                    cmd_stars = f"mpirun -np {proc_num}  \
                                --allow-run-as-root \
                                --map-by numa \
                                --mca coll_ucg_max_rcache_size 2  \
                                --mca btl ^openib \
                                -x UCG_PLANC=all \
                                -x PATH -x LD_LIBRARY_PATH \
                                -x UCG_PLANC_STARS_MAX_RAILS={max_rails} \
                                -x UCG_PLANC_STARS_RC_ROCE_LOCAL_SUBNET=y \
                                -x UCX_RC_VERBS_ROCE_LOCAL_SUBNET=y \
                                -x UCX_UD_VERBS_ROCE_LOCAL_SUBNET=y \
                                -x UCG_PLANC_STARS_TLS=all \
                                -x UCX_LOG_LEVEL=error -x UCG_LOG_LEVEL=error \
                                -x UCG_PLANC_STARS_{coll.upper()}_ATTR=I:{algo_stars} \
                                osu_{coll} -i 10 -x 10 {validate_flag}"
                    file_record.write(cmd_stars)
                    file_record.flush()
                    subprocess.run(args=cmd_stars, shell=True, stdout=file_record)
                    time.sleep(10)

# 2nodes
proc_num_list = [15, 39, 95]
max_rails_list = [2, 4]
for max_rails in max_rails_list:
    for proc_num in proc_num_list:
        proc_num_per_node = proc_num // 2
        if not os.path.exists(log_dir):
            os.mkdir(log_dir)
        with open(log_path_2nodes, 'a') as file_record:
            sys.stdout = file_record
            for idx in range(0, 5):
                coll = coll_type[idx]
                coll_upper = coll.upper()
                validate_flag = "-c"
                if idx == 3:
                    validate_flag = " "
                for algo_stars in algo_list_stars[idx]:
                    cmd_stars = f"mpirun -np {proc_num} \
                                -N {proc_num_per_node} \
                                --hostfile {hostfile_path} \
                                --allow-run-as-root \
                                --map-by numa \
                                --mca coll_ucg_max_rcache_size 2  \
                                --mca btl ^openib \
                                -x UCG_PLANC=all \
                                -x PATH -x LD_LIBRARY_PATH \
                                -x UCG_PLANC_STARS_MAX_RAILS={max_rails} \
                                -x UCG_PLANC_STARS_RC_ROCE_LOCAL_SUBNET=y \
                                -x UCX_RC_VERBS_ROCE_LOCAL_SUBNET=y \
                                -x UCX_UD_VERBS_ROCE_LOCAL_SUBNET=y \
                                -x UCG_PLANC_STARS_TLS=rc_acc \
                                -x UCX_LOG_LEVEL=error -x UCG_LOG_LEVEL=error \
                                -x UCG_PLANC_STARS_{coll.upper()}_ATTR=I:{algo_stars} \
                                osu_{coll} -i 10 -x 10 {validate_flag}"
                    file_record.write(cmd_stars)
                    file_record.flush()
                    subprocess.run(args=cmd_stars, shell=True, stdout=file_record)
                    time.sleep(10)