# System Noise Utilities and Benchmarks for HPC 

This project includes system-noise related software including benchmarks to assess the presence of noise on supercomputers. System noise is any activity that interferes with the execution of high-performance computing applications.

Currently, the project consists of the Fixed Work Quanta (FWQ) benchmarks, which include serial, threaded, and MPI variations. 


## FWQ 

### Building FWQ
```
pushd fwq
make 
popd
```
This builds `fwq`, `fwq-th`, and `fwq-mpi`, the serial, threaded, and MPI versions of FWQ, respectively.  


### Running FWQ-MPI

Run the benchmark on all the *user* Cores of a node. For example:
```
# Slurm
$ srun -N1 -n84 fwq-mpi -n50000 -w16384 -o fwq-mpi-n50k-w14.dat
```
```
# Flux
$ flux run -N1 -n84 -x fwq-mpi -n50000 -w16384 -o fwq-mpi-n50k-w14.dat
```

One can use the resource manager to place the processes on the desired CPUs, see [MAP tutorials](https://github.com/LLNL/mpibind/blob/master/doc/README.md).

### Running FWQ-THREADED

```
$ fwq-th -h
Usage: ./fwq-th [OPTIONS]
Options:
  -t, --threads NUM  Number of threads
  -c, --cpus RANGE   Bind threads to these CPUs
  -n, --samples NUM  Number of samples per thread
  -w, --work NUM     Number of work bits
  -o, --output FILE  Output file
  -s, --stdout       Output results to STDOUT
  -h, --help         Show this help message
```

Since this program uses a single process, it provides a parameter to place the threads on specific CPUs: `-c`


For example, on a compute node with 48 cores: 
```
$ fwq-th -t 48 -c 0-47
Number of threads: 48
Work per thread: 1048576
Number of samples per thread: 10000
Output file: fwq-th-times.dat
Thread 0 running on CPUs 0
Thread 2 running on CPUs 2
Thread 1 running on CPUs 1
Thread 4 running on CPUs 4
Thread 3 running on CPUs 3
[...]
Thread 43 running on CPUs 43
Thread 44 running on CPUs 44
Thread 45 running on CPUs 45
Thread 46 running on CPUs 46
Thread 47 running on CPUs 47
```

### Calculate basic statistics

The program `utils/fwq-stats.py` calculates and reports basic statistics for two workers: the one with the min standard deviation (std) and the one with the max standard deviation. 

For example: 
```
$ python fwq-stats.py fwq-mpi-n50k-w14.dat 
fwq-mpi-n50k-w14.dat
Worker  2 on CPUs 3 with std 0.0059
Worker 76 on CPUs 87 with std 0.0438
                  2            76
count  49999.000000  49999.000000
mean       8.853355      8.854145
std        0.005862      0.043767
min        8.840091      8.840091
25%        8.850091      8.850091
50%        8.850091      8.850091
75%        8.860091      8.860091
max        9.640099     12.970133
```

Another example:
```
$ python fwq-stats.py fwq-th-times.dat 
fwq-th-times.dat
Worker 11 on CPUs 11 with std 2.1737
Worker  5 on CPUs 5 with std 26.7153
                11            5
count  9999.000000  9999.000000
mean   1131.527761  1132.815608
std       2.173676    26.715265
min    1128.835890  1129.035887
25%    1130.085874  1130.325871
50%    1130.755866  1131.085861
75%    1132.145848  1132.615842
max    1148.785640  2140.353246
```


## Authors

This project was created by Edgar A. León. 

FWQ was initially written by Mark Seager and subsequently modified and extended by Edgar León and Adam Moody.  



## License 

This project is distributed under the terms of the MIT license. All new contributions must be made under this license.

See [LICENSE-MIT](LICENSE-MIT), [fwq/LICENSE-GPL](fwq/LICENSE-GPL), [COPYRIGHT](COPYRIGHT), and [NOTICE](NOTICE) for details.

SPDX-License-Identifier: MIT.

LLNL-CODE-2007931.
