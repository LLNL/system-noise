# System Noise Utilities and Benchmarks for HPC 

This project includes system-noise related software including benchmarks to assess the presence of noise on supercomputers. System noise is any activity that interferes with the execution of high-performance computing applications.

Currently, the project consists of the Fixed Work Quanta (FWQ) benchmarks, which includes serial, threaded, and MPI variations. 


## FWQ 

### Building FWQ
```
pushd fwq
make 
popd
```
This will build `fwq`, `fwq-th`, and `fwq-mpi`, the single, threaded, and MPI versions of FWQ, respectively.  

### Running FWQ-MPI

Run the benchmark on all the *user* cores of a node. For example: 
```
flux run -N1 -n84 -x fwq/fwq-mpi -n50000 -w16384 -o fwq-mpi-n50k-w14.dat
```

### Calculate basic statistics

The program `utils/fwq-stats.py` calculates and reports basic statistics for two processes: the one with the min standard deviation (std) and the one with the max standard deviation. 

For example: 
```
python utils/fwq-stats.py fwq-mpi-n50k-w14.dat 
```
This command results in the following: 
```
fwq-mpi-n50k-w14.dat
                 33            75
count  49999.000000  49999.000000
mean       8.856883      8.859093
std        0.007211      0.280071
min        8.850074      8.850074
25%        8.850074      8.850074
50%        8.860074      8.860074
75%        8.860074      8.860074
max        9.950083     70.080587
```
Process 33 has the lowest std (0.007) while process 75 has the highest std (0.280).


## Authors

This project was created by Edgar A. León. 

FWQ was initially written by Mark Seager and subsequently modified and extended by Edgar León and Adam Moody.  



## License 

This project is distributed under the terms of the MIT license. All new contributions must be made under this license.

See [LICENSE-MIT](LICENSE-MIT), [fwq/LICENSE-GPL](fwq/LICENSE-GPL), [COPYRIGHT](COPYRIGHT), and [NOTICE](NOTICE) for details.

SPDX-License-Identifier: MIT.

LLNL-CODE-2007931.
