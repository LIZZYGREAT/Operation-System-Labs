set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote localhost:1234

printf "\n=== QEMU reset vector ===\n"
p/x $pc
x/5i $pc
si
si
si
si
si
printf "PC after five reset-ROM instructions: "
p/x $pc
x/3i $pc

printf "\n=== Stop at the kernel entry ===\n"
break kern_entry
continue
printf "PC at kern_entry: "
p/x $pc
x/3i $pc
printf "SP before the entry instruction: "
p/x $sp
printf "bootstacktop symbol address: "
p/x &bootstacktop
break kern_init

printf "\n=== Execute stack setup ===\n"
si
printf "PC after auipc: "
p/x $pc
printf "SP after auipc: "
p/x $sp
printf "bootstacktop symbol address: "
p/x &bootstacktop
set $entry_sp = $sp
set $stack_top = (unsigned long)&bootstacktop
if $entry_sp == $stack_top
  printf "STACK ASSERTION: PASS (sp equals bootstacktop)\n"
else
  printf "STACK ASSERTION: FAIL (sp differs from bootstacktop)\n"
  quit 1
end
si
printf "PC after the second stack-setup instruction: "
p/x $pc
printf "SP after stack setup: "
p/x $sp

printf "\n=== Stop in the C initialization function ===\n"
continue
printf "PC at kern_init: "
p/x $pc
x/4i $pc
printf "SP at kern_init: "
p/x $sp
printf "bootstacktop symbol address: "
p/x &bootstacktop
if $pc == (unsigned long)&kern_init
  printf "KERNEL ENTRY ASSERTION: PASS (PC equals kern_init)\n"
else
  printf "KERNEL ENTRY ASSERTION: FAIL (PC differs from kern_init)\n"
  quit 1
end
detach
quit
