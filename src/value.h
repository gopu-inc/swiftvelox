#ifndef VALUE_H
#define VALUE_H

#include "common.h"
#include <stdint.h>

// Déclarations anticipées
typedef struct ASTNode ASTNode;
typedef struct Environment Environment;
typedef struct Value Value;

// Structure Value - version corrigée
struct Value {
    ValueType type;
    union {
        int boolean;
        int64_t integer;
        double number;
        char* string;
        struct {
            Value* items;  // Pointeur seulement, pas de récursion directe
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            Value* values;  // Pointeur seulement
            int count;
            int capacity;
        } object;
        struct {
            char* name;
            Value (*fn)(Value*, int, Environment*);
        } native;
        struct {
            ASTNode* declaration;
            Environment* closure;
        } function;
        struct {
            int resolved;
            Value* value;  // Change en pointeur pour éviter la récursion
        } promise;
        struct {
            char* message;
            Value* data;  // Change en pointeur
        } error;
    };
};

// Fonctions utilitaires pour Value
Value make_number(double value);
Value make_string(const char* str);
Value make_bool(int b);
Value make_nil();
Value make_array();
Value make_object();
Value make_error(const char* message);

void array_push(Value* array, Value item);
void object_set(Value* obj, const char* key, Value value);

#endif
