for i in `seq 1 200`; do
    ./linker input-${i} > out-${i}
done
