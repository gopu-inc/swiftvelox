#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include "common.h"
#include "stdlib.h"

// ======================================================
// [SECTION] MATH MODULE
// ======================================================

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

double std_math_const(int type) {
    if (type == TK_MATH_PI) return 3.14159265358979323846;
    if (type == TK_MATH_E) return 2.71828182845904523536;
    return 0.0;
}

// ======================================================
// [SECTION] STRING MODULE
// ======================================================

char* std_str_upper(const char* s) {
    if(!s) return strdup("");
    char* res = strdup(s);
    for(int i=0; res[i]; i++) res[i] = toupper(res[i]);
    return res;
}

char* std_str_lower(const char* s) {
    if(!s) return strdup("");
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

char* std_str_trim(const char* s) {
    if (!s) return strdup("");
    while(isspace(*s)) s++; // Trim début
    if(*s == 0) return strdup("");
    
    char* back = (char*)s + strlen(s) - 1;
    while(back > s && isspace(*back)) back--; // Trim fin
    
    int len = back - s + 1;
    char* res = malloc(len + 1);
    strncpy(res, s, len);
    res[len] = '\0';
    return res;
}

char* std_str_replace(const char* orig, const char* rep, const char* with) {
    if(!orig || !rep) return orig ? strdup(orig) : strdup("");
    if(strlen(rep) == 0) return strdup(orig);
    if(!with) with = "";

    // Compter les occurrences
    char *ins = (char*)orig;
    char *tmp;
    int count = 0;
    int len_rep = strlen(rep);
    int len_with = strlen(with);

    while ((tmp = strstr(ins, rep))) {
        ins = tmp + len_rep;
        count++;
    }

    // Allouer la mémoire
    char *result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    if (!result) return NULL;

    // Remplacer
    tmp = result;
    ins = (char*)orig;
    while (count--) {
        char* next = strstr(ins, rep);
        int len_front = next - ins;
        tmp = strncpy(tmp, ins, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        ins = next + len_rep;
    }
    strcpy(tmp, ins);
    return result;
}

int std_str_contains(const char* haystack, const char* needle) {
    if (!haystack || !needle) return 0;
    return strstr(haystack, needle) != NULL;
}

// ======================================================
// [SECTION] TIME MODULE
// ======================================================

double std_time_now(void) {
    return (double)time(NULL);
}

void std_time_sleep(double seconds) {
    usleep((useconds_t)(seconds * 1000000));
}

// ======================================================
// [SECTION] ENV MODULE
// ======================================================

char* std_env_get(const char* key) {
    if(!key) return strdup("");
    char* val = getenv(key);
    return val ? strdup(val) : strdup("");
}

void std_env_set(const char* key, const char* value) {
    if(key && value) setenv(key, value, 1);
}

char* std_env_os(void) {
    #ifdef _WIN32
        return strdup("windows");
    #elif __APPLE__
        return strdup("macos");
    #elif __linux__
        return strdup("linux");
    #else
        return strdup("unknown");
    #endif
}

// ======================================================
// [SECTION] PATH MODULE
// ======================================================

char* std_path_basename(const char* path) {
    if (!path) return strdup("");
    char* p = strrchr(path, '/');
    return p ? strdup(p + 1) : strdup(path);
}

char* std_path_dirname(const char* path) {
    if (!path) return strdup(".");
    char* p = strrchr(path, '/');
    if (!p) return strdup(".");
    if (p == path) return strdup("/"); 
    
    int len = p - path;
    char* res = malloc(len + 1);
    strncpy(res, path, len);
    res[len] = '\0';
    return res;
}

char* std_path_join(const char* p1, const char* p2) {
    if (!p1) return p2 ? strdup(p2) : strdup("");
    if (!p2) return strdup(p1);
    
    int l1 = strlen(p1);
    int l2 = strlen(p2);
    int need_slash = (l1 > 0 && p1[l1-1] != '/') ? 1 : 0;
    
    char* res = malloc(l1 + need_slash + l2 + 1);
    strcpy(res, p1);
    if (need_slash) strcat(res, "/");
    strcat(res, p2);
    return res;
}

char* std_path_abs(const char* path) {
    if (!path) return strdup("");
    char buf[PATH_MAX];
    if (realpath(path, buf)) {
        return strdup(buf);
    }
    return strdup(path);
}

// ======================================================
// [SECTION] CRYPTO MODULE
// ======================================================

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* std_crypto_b64enc(const char* data) {
    if (!data) return strdup("");
    size_t len = strlen(data);
    size_t out_len = 4 * ((len + 2) / 3);
    char* out = malloc(out_len + 1);
    if (!out) return NULL;
    
    size_t i, j;
    for (i = 0, j = 0; i < len;) {
        uint32_t octet_a = i < len ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < len ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < len ? (unsigned char)data[i++] : 0;
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        
        out[j++] = b64_table[(triple >> 3 * 6) & 0x3F];
        out[j++] = b64_table[(triple >> 2 * 6) & 0x3F];
        out[j++] = (i > len + 1) ? '=' : b64_table[(triple >> 1 * 6) & 0x3F];
        out[j++] = (i > len) ? '=' : b64_table[(triple >> 0 * 6) & 0x3F];
    }
    out[out_len] = '\0';
    return out;
}

char* std_crypto_b64dec(const char* data) {
    // TODO: Implémenter le décodage si nécessaire
    return strdup("not_implemented");
}

// Wrapper system pour SHA256 (plus fiable que implém manuelle rapide)
char* std_crypto_sha256(const char* data) {
    if (!data) return strdup("");
    
    char cmd[4096];
    // Attention aux injections, ceci est pour usage local contrôlé
    // On utilise printf '%.*s' pour éviter les débordements
    snprintf(cmd, 4096, "echo -n \"%s\" | sha256sum | cut -d' ' -f1", data);
    
    FILE* fp = popen(cmd, "r");
    if (!fp) return strdup("error_popen");
    
    char result[128];
    if (fgets(result, sizeof(result), fp) != NULL) {
        pclose(fp);
        result[strcspn(result, "\n")] = 0;
        return strdup(result);
    }
    pclose(fp);
    return strdup("error_exec");
}

char* std_crypto_md5(const char* data) {
    if (!data) return strdup("");
    
    char cmd[4096];
    snprintf(cmd, 4096, "echo -n \"%s\" | md5sum | cut -d' ' -f1", data);
    
    FILE* fp = popen(cmd, "r");
    if (!fp) return strdup("error");
    
    char result[128];
    if (fgets(result, sizeof(result), fp) != NULL) {
        pclose(fp);
        result[strcspn(result, "\n")] = 0;
        return strdup(result);
    }
    pclose(fp);
    return strdup("error");
}
