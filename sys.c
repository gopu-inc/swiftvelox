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
    printf("%s[SYS MODULE]%s Initialized\n", COLOR_CYAN, COLOR_RESET);
}

// sys.exec("cmd") -> int (code de retour)
void sys_exec(ASTNode* node) {
    if (!node->left) return;
    
    // Extraction manuelle car swf.c n'est pas inclus ici
    char* cmd = NULL;
    if (node->left->type == NODE_STRING) cmd = node->left->data.str_val;
    
    // Note: Dans une version finale, utilisez une fonction helper partagée pour évaluer les variables
    if (cmd) {
        int res = system(cmd);
        node->data.int_val = res; // Retourne le code de sortie
    }
}

// sys.argv(index) -> string
void sys_argv(ASTNode* node) {
    if (!node->left) return;
    int idx = 0;
    if (node->left->type == NODE_INT) idx = (int)node->left->data.int_val;
    
    // On décale de 1 pour ignorer le nom du programme swift lui-même si besoin,
    // ou on garde tel quel. Ici on accède aux arguments passés au script.
    // Hack: on suppose que argv[2] est le premier arg du script si lancé via ./swift script.swf arg1
    
    // Ajustement selon comment tu lances le script
    // ./swift zarch.swf install math
    // argv[0]=./swift, argv[1]=zarch.swf, argv[2]=install, argv[3]=math
    
    int real_idx = idx + 2; 
    
    if (real_idx < g_argc) {
        node->data.str_val = strdup(g_argv[real_idx]); // Hack temporaire de retour
    } else {
        node->data.str_val = NULL;
    }
}

void sys_exit(ASTNode* node) {
    int code = 0;
    if (node->left) code = (int)node->left->data.int_val;
    exit(code);
}
