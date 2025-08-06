#!/usr/bin/env python
'''
Edgar A. Leon
Lawrence Livermore National Laboratory
'''

import pandas as pd
import re
import sys


def parse_fwq_file(fname):
    '''
    Parse the FWQ-MPI output file
    Create a dataframe with the cycle counts per task 
    '''
    cycles = {}
    task = None
    i = 0 
    with open(fname) as f:
        for line in f: 
            #print(line)
            if x:=re.search(r"Speed.+GHz\s+([\d.]+)", line):
                # Get result in us
                cpu_freq = float(x.group(1)) * 1e3
                #print(cpu_freq)
            elif x:=re.search(r"^(Process|Thread)\s+(\d+).+CPUs\s+(\S+)",line):
                task = x.group(2)
                cpus[task] = x.group(3)
                cycles[task] = []
                #print(x.group(1), x.group(2), x.group(3))
            elif task is not None: 
                cycles[task].append(int(line.strip()) / cpu_freq)
    #return cycles
    return pd.DataFrame(cycles)


def calc_stats(file):
    '''
    Identify the tasks with the min and max standard deviation
    Calculate basic statistics of those tasks
    '''
    print(file)

    runtimes = parse_fwq_file(file)
    # Drop the first row due to warmup issues
    runtimes.drop([0], inplace=True)
    #runtimes.info()
    
    stats_all = runtimes.describe()
    std_all = stats_all.loc['std']
    #print(stats_all)
    #print(std_all)
    
    task_min = std_all.idxmin()
    task_max = std_all.idxmax()
    selected = [task_min, task_max]
    for task in selected:
        print("Worker {:>2} on CPUs {} with std {:.4f}"
              .format(task, cpus[task], std_all[task]))
    print(stats_all[selected])


name = sys.argv[0]
if (len(sys.argv) == 1):
    sys.exit("Usage: python " + name + " <file1> <file2> ...")

cpus = {}
for file in sys.argv[1:]:
    #parse_fwq_file(file)
    calc_stats(file)

