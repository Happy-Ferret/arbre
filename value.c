/*
 * arbre
 *
 * (c) 2011-2012, Alexis Sellier
 *
 * value.c
 *
 */
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "value.h"

const char *TYPE_STRINGS[] = {
    [TYPE_ANY] = "any",
    [TYPE_ATOM] = "atom",
    [TYPE_BIN] = "bin",
    [TYPE_TUPLE] = "tuple",
    [TYPE_STRING] = "string",
    [TYPE_NUMBER] = "number",
    [TYPE_LIST] = "list",
    [TYPE_PATH] = "path"
};

TValue *tvalue(TYPE type, Value val)
{
    TValue *tval = malloc(sizeof(*tval));

    tval->v = val;
    tval->t = type;

    return tval;
}

void tvalue_pp(TValue *tval)
{
    if (tval == NULL) {
        printf("(null)");
        return;
    }

    TYPE t = tval->t;
    Value v = tval->v;

    switch (t) {
        case TYPE_BIN:
        case TYPE_TUPLE:
        case TYPE_ATOM:
            printf("%s", v.atom);
            break;
        case TYPE_STRING:
            break;
        case TYPE_NUMBER:
            printf("%d", v.number);
            break;
        default:
            printf("<%s>", TYPE_STRINGS[t]);
            break;
    }
}

void tvalues_pp(TValue *tval, int size)
{
    putchar('[');
    for (int i = 0; i < size; i++) {
        tvalue_pp(tval + i);
        if (i < size - 1)
            putchar(',');
    }
    putchar(']');
}

TValue *tuple(int arity)
{
    assert(arity <= 255);

    Tuple *t = malloc(sizeof(*t) + sizeof(TValue) * arity);
           t->arity = arity;

    return tvalue(TYPE_TUPLE, (Value){ .tuple = t });
}

TValue *atom(const char *name)
{
    assert(name);

    return tvalue(TYPE_ATOM, (Value){ .atom = name });
}

TValue *number(const char *src)
{
    int number = atoi(src);

    Value v = (Value){ .number = number };

    return tvalue(TYPE_NUMBER, v);
}
