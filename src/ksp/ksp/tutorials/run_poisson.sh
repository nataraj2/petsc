mpirun -np 8 ./Poisson2D
cat sol_file*.txt > pressure.txt
rm -rf sol_file*.txt
