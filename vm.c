#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

void vm_init(VM* vm) {
    vm->chunk = NULL;
    vm->ip = NULL;
    vm->stack_capacity = 256;
    vm->stack = malloc(sizeof(Value) * vm->stack_capacity);
    vm->stack_top = 0;
}

void vm_free(VM* vm) {
    free(vm->stack);
}

void vm_push(VM* vm, Value value) {
    if (vm->stack_top >= vm->stack_capacity) {
        // simple resize later if needed
        fprintf(stderr, "Stack overflow!\n");
        exit(1);
    }
    vm->stack[vm->stack_top++] = value;
}

Value vm_pop(VM* vm) {
    if (vm->stack_top == 0) {
        fprintf(stderr, "Stack underflow!\n");
        exit(1);
    }
    return vm->stack[--vm->stack_top];
}

bool vm_run(VM* vm, Chunk* chunk) {
    vm->chunk = chunk;
    vm->ip = chunk->code;

    while (vm->ip < chunk->code + chunk->count) {
        uint8_t instruction = *vm->ip++;
        switch (instruction) {
            case OP_CONSTANT: {
                uint8_t constant_idx = *vm->ip++;
                double val = chunk->constants[constant_idx];
                vm_push(vm, number_value(val));
                break;
            }
            case OP_ADD: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                vm_push(vm, number_value(a.as.number + b.as.number));
                break;
            }
            case OP_SUB: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                vm_push(vm, number_value(a.as.number - b.as.number));
                break;
            }
            case OP_MUL: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                vm_push(vm, number_value(a.as.number * b.as.number));
                break;
            }
            case OP_DIV: {
                Value b = vm_pop(vm);
                Value a = vm_pop(vm);
                if (b.as.number == 0) {
                    fprintf(stderr, "Division by zero\n");
                    return false;
                }
                vm_push(vm, number_value(a.as.number / b.as.number));
                break;
            }
            case OP_PRINT: {
                Value val = vm_pop(vm);
                if (val.type == VAL_NUMBER) {
                    printf("%g\n", val.as.number);
                } else if (val.type == VAL_STRING) {
                    printf("%s\n", val.as.s);
                }
                break;
            }
            case OP_RETURN:
                return true;
            default:
                fprintf(stderr, "Unknown opcode %d\n", instruction);
                return false;
        }
    }
    return true;
}
