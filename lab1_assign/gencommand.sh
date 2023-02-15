for i in `seq 21 200`; do
    ./labgen1.py > input-${i}
    ./linker input-${i} > out-${i}
done
