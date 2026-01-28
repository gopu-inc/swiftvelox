#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include "common.h"
#include "stdlib.h"

// --- MATH ---
double std_math_calc(int op_type, double val1, double val2) {
    switch(op_type) {
        case TK_MATH_SIN: return sin(val1);
        case TK_MATH_COS: return cos(val1);
        case TK_MATH_TAN: return tan(val1);
        case TK_MATH_SQRT: return sqrt(val1);
        case TK_MATH_ABS: return fabs(val1);
        case TK_MATH_FLOOR: return floor(val1);
        case TK_MATH_CEIL: return ceil(val1);
        case TK_MATH_ROUND: return round(val1);
        case TK_MATH_POW: return pow(val1, val2);
        case TK_MATH_RANDOM: return (double)rand() / (double)RAND_MAX;
        default: return 0.0;
    }
}

// --- STRING ---
char* std_str_upper(const char* s) {
    if(!s) return NULL;
    char* res = strdup(s);
    for(int i=0; res[i]; i++) res[i] = toupper(res[i]);
    return res;
}

char* std_str_lower(const char* s) {
    if(!s) return NULL;
    char* res = strdup(s);
    for(int i=0; res[i]; i++) res[i] = tolower(res[i]);
    return res;
}

char* std_str_sub(const char* s, int start, int len) {
    if(!s || start < 0 || len <= 0) return strdup("");
    int slen = strlen(s);
    if(start >= slen) return strdup("");
    if(start + len > slen) len = slen - start;
    
    char* res = malloc(len + 1);
    strncpy(res, s + start, len);
    res[len] = '\0';
    return res;
}

// Replace simple (première occurrence ou toutes, ici simplifié)
char* std_str_replace(const char* orig, const char* rep, const char* with) {
    if(!orig || !rep) return NULL;
    // Implémentation basique, retourne copie si pas trouvé
    char *result; 
    char *ins;    
    char *tmp;    
    int len_rep;  
    int len_with; 
    int len_front; 
    int count;    

    len_rep = strlen(rep);
    if (len_rep == 0) return strdup(orig);
    len_with = strlen(with);

    ins = (char*)orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; 
    }
    strcpy(tmp, orig);
    return result;
}

// --- TIME ---
double std_time_now(void) {
    return (double)time(NULL);
}

void std_time_sleep(double seconds) {
    usleep((useconds_t)(seconds * 1000000));
}

// --- ENCODING (Base64 minimal) ---
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* std_b64_encode(const char* data) {
    // Implémentation simplifiée ou utilisation de libcurl/openssl si dispo
    // Pour l'instant, on peut renvoyer une chaîne fictive ou utiliser une lib externe
    // Si tu as -lcurl, curl a des fonctions internes, mais mieux vaut une implém pure C
    return strdup(data); // TODO: Vraie implémentation Base64
}
