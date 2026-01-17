#ifndef VALUE_H
#define VALUE_H

#include "common.h"
#include <stdint.h>

// Déclarations anticipées
struct ASTNode;
struct Environment;
struct Value;

typedef struct Value Value;
typedef struct ASTNode ASTNode;
typedef struct Environment Environment;

// Structure Value
struct Value {
    ValueType type;
    union {
        int boolean;
        int64_t integer;
        double number;
        char* string;
        struct {
            Value* items;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            Value* values;
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
            Value* value;
        } promise;
        struct {
            char* message;
            Value* data;
        } error;
    };
};

// Fonctions utilitaires
Value make_number(double value);
Value make_string(const char* str);
Value make_bool(int b);
Value make_nil();
Value make_array();
Value make_object();

void array_push(Value* array, Value item);
void object_set(Value* obj, const char* key, Value value);

#endif
