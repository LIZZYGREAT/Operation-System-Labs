set pagination off
file bin/kernel
set architecture riscv:rv64
target remote localhost:1234
printf "\n=== Initial reset PC ===\n"
p/x $pc
x/8i $pc
printf "\n=== Single-step reset ROM ===\n"
si
p/x $pc
si
p/x $pc
si
p/x $pc
si
p/x $pc
si
p/x $pc
printf "\n=== Instructions at resulting PC ===\n"
x/8i $pc
detach
