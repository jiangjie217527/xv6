syscall(write,defined in user/user.h)

-> usys.pl(generate usys.S to put num in a7 in trapframe)

-> trap.c(through ecall)

-> syscall.c (syscall and get a7 as num)

-> specific function according to the type

-> usertrap return 
