#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "sys.h"

static int g_argc;
static char** g_argv;

void init_sys_module(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
    // Debug pour voir ce qu'on reçoit
    // printf("[SYS DEBUG] Argc: %d\n", argc);
    // for(int i=0; i<argc; i++) printf("[SYS DEBUG] Arg[%d]: %s\n", i, argv[i]);
}

char* sys_get_argv(int index) {
    // Logique intelligente pour trouver le premier argument après le script .swf
    int script_idx = -1;
    for (int i = 0; i < g_argc; i++) {
        if (strstr(g_argv[i], ".swf")) {
            script_idx = i;
            break;
        }
    }

    if (script_idx == -1) return NULL;

    // index 0 = l'argument APRES le script
    int target_idx = script_idx + 1 + index;

    if (target_idx < g_argc) {
        return g_argv[target_idx];
    }
    return NULL;
}

int sys_exec_int(const char* cmd) {
    if (!cmd) {
        printf("%s[SYS ERROR]%s Command is NULL\n", COLOR_RED, COLOR_RESET);
        return -1;
    }
    // printf("%s[SYS EXEC]%s %s\n", COLOR_YELLOW, COLOR_RESET, cmd);
    int res = system(cmd);
    if (res == -1) return -1;
    return (res >> 8) & 0xFF; 
}
