/*
DMBoot 128 v5 - Exec overlay: run a slot, go 64, exit

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef EXEC_H
#define EXEC_H

#define OVERLAY_EXEC        5

__noinline void runbootfrommenu(char select);
__noinline void exec_go64(void);
__noinline void exec_exit_to_basic(void);
__noinline void exec_browse(void);
__noinline void exec_geos(void);

#pragma compile("exec.c")

#endif // EXEC_H
