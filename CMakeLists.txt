cmake_minimum_required(VERSION 3.27)
project(xv6_lab C)

set(CMAKE_C_STANDARD 11)

include_directories(kernel)
include_directories(user)

add_executable(xv6_lab
        kernel/bio.c
        kernel/buf.h
        kernel/console.c
        kernel/date.h
        kernel/defs.h
        kernel/elf.h
        kernel/exec.c
        kernel/fcntl.h
        kernel/file.c
        kernel/file.h
        kernel/fs.c
        kernel/fs.h
        kernel/kalloc.c
        kernel/log.c
        kernel/main.c
        kernel/memlayout.h
        kernel/param.h
        kernel/pipe.c
        kernel/plic.c
        kernel/printf.c
        kernel/proc.c
        kernel/proc.h
        kernel/ramdisk.c
        kernel/riscv.h
        kernel/sleeplock.c
        kernel/sleeplock.h
        kernel/spinlock.c
        kernel/spinlock.h
        kernel/sprintf.c
        kernel/start.c
        kernel/stat.h
        kernel/stats.c
        kernel/string.c
        kernel/syscall.c
        kernel/syscall.h
        kernel/sysfile.c
        kernel/sysproc.c
        kernel/trap.c
        kernel/types.h
        kernel/uart.c
        kernel/virtio.h
        kernel/virtio_disk.c
        kernel/vm.c
        kernel/vmcopyin.c
        mkfs/mkfs.c
        user/cat.c
        user/echo.c
        user/forktest.c
        user/grep.c
        user/grind.c
        user/init.c
        user/kill.c
        user/ln.c
        user/ls.c
        user/mkdir.c
        user/printf.c
        user/rm.c
        user/sh.c
        user/statistics.c
        user/stats.c
        user/stressfs.c
        user/ulib.c
        user/umalloc.c
        user/user.h
        user/usertests.c
        user/wc.c
        user/zombie.c)
