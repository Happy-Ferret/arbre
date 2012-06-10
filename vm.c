/*
 * arbre
 *
 * (c) 2011-2012, Alexis Sellier
 *
 * vm.c
 *
 *   arbre virtual-machine
 *
 * +  vm
 * -  vm_free
 *
 * -> vm_open, vm_run
 *
 * TODO: Document functions
 * TODO: Abstract module/path retrieval
 *
 */
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <assert.h>

#include "hash.h"
#include "value.h"
#include "op.h"
#include "runtime.h"
#include "vm.h"
#include "error.h"
#include "bin.h"

#define INDENT "    "

#define DEBUG

#ifdef DEBUG
#define debug(...) (fprintf(stderr, __VA_ARGS__))
#else
#define debug(...) ;
#endif

Module *vm_module  (VM *vm, const char *name);
TValue *vm_execute (VM *vm, Process *proc);

int match (TValue *locals, TValue *pattern, TValue *v, TValue *local);

VM *vm(void)
{
    size_t msize = sizeof(ModuleList*) * 512;
    VM    *vm    = malloc(sizeof(*vm) + msize);

    vm->clause = NULL;
    vm->nprocs = 0;
    vm->procs  = malloc(sizeof(Process*) * 1024);
    vm->proc   = NULL;

    memset(vm->modules, 0, msize);

    return vm;
}

void vm_free(VM *vm)
{
    // TODO: Implement
}

uint8_t *vm_readk(VM *vm, uint8_t *b, TValue *k);

uint8_t *vm_readlist(VM *vm, uint8_t *b, Value *v)
{
    size_t length = *(size_t *)b;

    b += sizeof(length);

    List *l = list_cons(NULL, NULL);

    for (size_t i = 0; i < length; i++) {
        TValue *val = malloc(sizeof(*val));

        b = vm_readk(vm, b, val);
        l = list_cons(l, val);

        if (i < length - 1)
            debug(", ");
    }
    (*v).list = l;

    return b;
}

uint8_t *vm_readk(VM *vm, uint8_t *b, TValue *k)
{
    TYPE t = *b ++;
    Value v;

    switch (t & TYPE_MASK) {
        case TYPE_PATHID:
            v.pathid = malloc(sizeof(*v.pathid));
            v.pathid->module = (char *)b;
            b += strlen(v.pathid->module) + 1;
            v.pathid->path = (char *)b;
            b += strlen(v.pathid->path) + 1;
            break;
        case TYPE_LIST: {
            debug("[");
            b = vm_readlist(vm, b, &v);
            debug("]");
            break;
        }
        case TYPE_ATOM:
            v.atom = (char *)b;
            b += strlen(v.atom) + 1;
            debug("%s", v.atom);
            break;
        case TYPE_STRING:
            assert(0);
            break;
        case TYPE_NUMBER:
            v.number = *(int *)b;
            b += sizeof(int);
            debug("%d", v.number);
            break;
        case TYPE_ANY:
            v.ident = *b ++;
            debug("<any>");
            if (t & Q_RANGE) {
                debug("..");
            }
            break;
        case TYPE_VAR:
            v.ident = *b ++;
            debug("r%d", v.ident);
            break;
        case TYPE_TUPLE: {
            uint8_t arity = *b ++;

            debug("(");

            // TODO: Use `tuple` function
            v.tuple = malloc(sizeof(*v.tuple) + sizeof(TValue) * arity);
            v.tuple->arity = arity;

            for (int i = 0; i < arity; i++) {
                b = vm_readk(vm, b, v.tuple->members + i);
            }
            debug(")");
            break;
        }
        default:
            assert(0);
            break;
    }
    k->t = t;
    k->v = v;

    return b;
}

/* TODO: Paths should be per-module */
uint8_t *vm_readclause(VM *vm, Path *p, int index, uint8_t *b)
{
    /* Pattern */
    TValue pattern = bin_readnode(&b);

    /* Number of locals */
    uint8_t nlocals = *b ++;

    /* Size of constants table */
    uint8_t ksize = *b ++;

    debug("reading clause with %d constant(s) and %d local(s)..\n", ksize, nlocals);

    Clause *c = clause(pattern, nlocals, ksize);
    c->path = p;

    for (int i = 0; i < ksize; i++) {
        debug("\tk%d = ", i);
        b = vm_readk(vm, b, &c->constants[i]);
        debug("\n");
    }
    debug("\n");

    c->codelen = *((unsigned long*)b);
    b += sizeof(c->codelen);

    c->code = (Instruction *)b;

    /* Skip all code */
    b += c->codelen * sizeof(Instruction);

    p->clauses[index] = c;

    return b;
}

uint8_t *vm_readpath(VM *vm, Module *m, int index, uint8_t *b)
{
    char *name;

    /* Path attributes */
    uint8_t attrs   = *b ++;
    uint8_t namelen = *b ++;

    assert(attrs);

    /* TODO: check attributes instead */
    /* TODO: clean this up.. maybe with strdup */
    if (namelen > 0) {
        name = malloc(namelen + 1);
        memcpy(name, b, namelen);
        name[namelen] = '\0';
        b += namelen;
    }

    uint8_t nclauses = *b ++;

    debug("reading path '%s'..\n", name);
    debug("reading %d clause(s)..\n", nclauses);

    Path *p = path(name, nclauses);
    p->module = m;

    m->paths[index] = p;

    for (int i = 0; i < nclauses; i++) {
        b = vm_readclause(vm, p, i, b);
    }

    return b;
}

void vm_open(VM *vm, const char *name, uint8_t *buffer)
{
    unsigned char magic = *buffer;

    if (magic != 167) { /* TODO: Make this segment 2 bytes */
        error(1, 0, "file is not an arbre bin file");
    }
    buffer ++;

    struct version v = *((struct version *)buffer);
    buffer += sizeof(struct version); /* XXX: How portable is this? */

    debug("version %d.%d.%d\n", v.major, v.minor, v.patch);

    int pathsn = *((int*)buffer);
    buffer += sizeof(int);

    debug("reading %d paths..\n", pathsn);

    Module *m = module(name, pathsn);

    for (int i = 0; i < pathsn; i++) {
        buffer = vm_readpath(vm, m, i, buffer);
    }

    uint32_t key = hash(name, strlen(name)) % 512;

    if (vm->modules[key]) {
        module_prepend(vm->modules[key], m);
    } else {
        vm->modules[key] = modulelist(m);
    }
}

int match_atom(Value pattern, Value v, TValue *local)
{
    // TODO: If we keep a global store of atoms, we only
    // need a pointer comparison here.
    if (! strcmp(pattern.atom, v.atom))
        return 0;

    return -1;
}

int match_tuple(TValue *locals, Value pattern, Value v, TValue *local)
{
    int m = 0, nmatches = 0;

    if (pattern.tuple->arity != v.tuple->arity)
        return -1;

    for (int i = 0; i < v.tuple->arity; i++) {
        m = match(locals, pattern.tuple->members + i, v.tuple->members + i, local + nmatches);

        if (m == -1)
            return -1;

        nmatches += m;
    }
    return nmatches;
}

int match_list(TValue *locals, Value pattern, Value v, TValue *local)
{
    int m = 0, nmatches = 0;

    List *pat = pattern.list;
    List *val = v.list;

    while (val) {
        if (!val->head && !pat->head) { /* Reached end of both lists (match) */
            break;
        }

        if (val->head && !pat->head) { /* pattern is shorter than value */
            return -1;
        }

        if (pat->head->t & Q_RANGE) {
            TValue *t = tvalue(TYPE_LIST, (Value){ .list = val });
            m = match(locals, pat->head, t, local + nmatches);
            return nmatches + m;
        } else {
            m = match(locals, pat->head, val->head, local + nmatches);
        }

        if (!val->head && pat->head) { /* value is shorter than pattern */
            return -1;
        }

        if (m == -1)
            return -1;

        nmatches += m;

        pat = pat->tail;
        val = val->tail;
    }
    return nmatches;
}

int match(TValue *locals, TValue *pattern, TValue *v, TValue *local)
{
    assert(pattern);

    if (v == NULL) {
        if ((pattern->t & TYPE_MASK) == TYPE_TUPLE &&
            pattern->v.tuple->arity == 0) {
            return 0;
        } else {
            return -1;
        }
    }

    if ((pattern->t & TYPE_MASK) == TYPE_ANY) {
        *local = *v;
        return 1;
    }

    if ((pattern->t & TYPE_MASK) == TYPE_VAR) {
        return match(locals, &locals[pattern->v.ident], v, local);
    }

    if ((pattern->t & TYPE_MASK) != (v->t & TYPE_MASK)) {
        return -1;
    }

    switch (pattern->t) {
        case TYPE_TUPLE:
            return match_tuple(locals, pattern->v, v->v, local);
        case TYPE_LIST:
            return match_list(locals, pattern->v, v->v, local);
        case TYPE_ATOM:
            return match_atom(pattern->v, v->v, local);
        case TYPE_NUMBER:
            if (pattern->v.number == v->v.number)
                return 0;
            break;
        default:
            assert(0);
    }
    return -1;
}

int vm_tailcall(VM *vm, Process *proc, Clause *c, TValue *arg)
{
    Module *m = proc->module;

    Frame *frame = *(proc->stack->frame);

    size_t size = sizeof(TValue) * c->nlocals;
    TValue *local  = frame->locals;

    memset(frame->locals, 0, size);

    int nlocals = match(NULL, &c->pattern, arg, local);

    if (nlocals == -1)
        return -1;

    #ifdef DEBUG
        for (int i = 0; i < proc->stack->size; i++)
            debug(INDENT);

        printf("%s/%s ", m->name, c->path->name);
        tvalue_pp(arg);
        printf("\n");
    #endif

    if (! c)
        return -1;

    frame->clause = c;
    frame->pc     = 0;
    frame->prev   = NULL;

    return nlocals;
}

int vm_call(VM *vm, Process *proc, Clause *c, TValue *arg)
{
    /* TODO: Perform pattern-match */

    vm->clause = c;

    assert(c);

    size_t size = sizeof(TValue) * c->nlocals;
    TValue *locals = malloc(size),
           *local  = locals;

    memset(locals, 0, size);

    int nlocals = match(NULL, &c->pattern, arg, local);

    if (nlocals == -1) {
        return -1;
    }

    for (int i = 0; i < proc->stack->size; i++)
        debug(INDENT);

#ifdef DEBUG
    printf("%s/%s ", m->name, p->name);
    tvalue_pp(arg);
    printf("\n");
#endif

    Frame *f = frame(locals, c->nlocals);

    f->clause = c;
    f->pc     = 0;
    f->prev   = NULL;

    stack_push(proc->stack, f);

    return nlocals;
}

Process *vm_spawn(VM *vm, Module *m, Path *p)
{
    Process *proc = process(m, p);

    proc->flags |= PROC_READY;

    vm->procs[vm->nprocs++] = proc;

    return proc;
}

Process *vm_select(VM *vm)
{
    Process *proc;

    /* TODO: Don't start from 0, start from last proc */

    for (;;) {
        for (int i = 0; i < vm->nprocs; i++) {
            if ((proc = vm->procs[i])) {
                if ((proc->credits > 0) && (proc->flags & PROC_READY)) {
                    return proc;
                }
            }
        }
        /* All proc credits exhausted, reset */
        /* TODO: We don't need this step if we start from the last proc,
         * and reset as we go */
        for (int i = 0; i < vm->nprocs; i++) {
            if ((proc = vm->procs[i])) {
                proc->credits = 16;
            }
        }
    }
    assert(0);
    return NULL;
}

#define RK(x) (ISK(x) ? K[INDEXK(x)] : R[x])
TValue *vm_execute(VM *vm, Process *proc)
{
    int A, B, C;

    Module *m;
    Path   *p;
    Clause *c;

    Stack *s;
    Frame *f;

    Instruction i;

    TValue *R;
    TValue *K;

reentry:

    vm->proc = proc;

    s = proc->stack;    /* Current stack */
    f = *(s->frame);    /* Current stack-frame */
    c = f->clause;      /* Current clause */
    p = c->path;        /* Current path */
    m = p->module;      /* Current module */
    R = f->locals;      /* Registry (local vars) */
    K = c->constants;   /* Constants */

    while ((i = c->code[f->pc++])) {
        #ifdef DEBUG
        for (int i = 0; i < proc->stack->size; i++)
            debug(INDENT);
        printf("%3lu:\t", f->pc); op_pp(i); putchar('\n');
        #endif

        proc->credits --;

        OpCode op = OP(i);

        A = A(i);

        switch (op) {
            case OP_MOVE:
                R[A] = R[B(i)];
                break;
            case OP_LOADK:
                B = INDEXK(D(i));
                assert(A < f->nlocals);
                R[A] = K[B];
                break;
            case OP_ADD: {
                assert(RK(B(i)).t == TYPE_NUMBER);
                assert(RK(C(i)).t == TYPE_NUMBER);

                R[A].t        = TYPE_NUMBER;
                R[A].v.number = RK(B(i)).v.number +
                                RK(C(i)).v.number;
                break;
            }
            case OP_SUB: {
                assert(RK(B(i)).t == TYPE_NUMBER);
                assert(RK(C(i)).t == TYPE_NUMBER);

                R[A].t        = TYPE_NUMBER;
                R[A].v.number = RK(B(i)).v.number -
                                RK(C(i)).v.number;
                break;
            }
            case OP_JUMP:
                f->pc += J(i);
                continue;
            case OP_MATCH: {
                TValue b = RK(B(i)),
                       c = RK(C(i));

                if (match(R, &b, &c, &R[A + 1]) >= 0)
                    f->pc ++;

                break;
            }
            case OP_GT: {
                TValue b = RK(B(i)),
                       c = RK(C(i));

                assert(b.t == TYPE_NUMBER);
                assert(c.t == TYPE_NUMBER);

                if (b.v.number > c.v.number)
                    f->pc ++;

                break;
            }
            case OP_TUPLE:
                R[A] = *tuple(B(i));
                break;
            case OP_SETTUPLE:
                B = B(i);

                assert(R[A].t == TYPE_TUPLE);
                assert(B < R[A].v.tuple->arity);

                R[A].v.tuple->members[B] = RK(C(i));
                break;
            case OP_LIST:
                R[A] = *list(0);
                break;
            case OP_CONS: {
                assert(R[B(i)].t == TYPE_LIST);

                TValue *t = ISK(C(i)) ? &K[INDEXK(C(i))] : &R[C(i)];

                List *l = list_cons(R[B(i)].v.list, t);
                R[A] = *tvalue(TYPE_LIST, (Value){ .list = l });
                break;
            }
            case OP_PATH:
                R[A].v.pathid->module = RK(B(i)).v.atom;
                R[A].v.pathid->path   = RK(C(i)).v.atom;
                break;
            case OP_TAILCALL: {
                int      matches = -1;
                TValue   arg     = R[C(i)];

                c = p->clauses[B(i)];

                if ((matches = vm_tailcall(vm, proc, c, &arg)) < 0) /* Replace stack call-frame */
                    error(1, 0, "no matches for %s/%s", m->name, p->name);
                goto reentry;
            }
            case OP_CALL: {
                int matches = -1;

                B = B(i);
                C = C(i);

                TValue callee = RK(B);
                TValue arg = RK(C);

                switch (callee.t) {
                    case TYPE_PATH: {
                        Path *p = callee.v.path;
                        c = p->clauses[0];
                        matches = vm_call(vm, proc, c, &arg); /* Create & push stack call-frame */
                        break;
                    }
                    case 0:
                    case TYPE_PATHID: {
                        Path   *p;
                        Module *m;

                        const char *module = callee.v.pathid->module;
                        const char *path   = callee.v.pathid->path;

                        if (! (m = vm_module(vm, module)))
                            error(1, 0, "module `%s` not found", module);

                        if (! (p = module_path(m, path)))
                            error(1, 0, "path `%s` not found in `%s` module", path, module);

                        K[INDEXK(B)] = *tvalue(TYPE_PATH, (Value){ .path = p });

                        c = p->clauses[0];
                        matches = vm_call(vm, proc, c, &arg); /* Create & push stack call-frame */

                        break;
                    }
                    case TYPE_CLAUSE:
                        c = callee.v.clause;
                        break;
                    default:
                        assert(0);
                }
                if (matches < 0)
                    error(1, 0, "no matches for %s/%s", m->name, p->name);

                (*s->frame)->result = A;                    /* Set return-value register */

                goto reentry;
            }
            case OP_RETURN: {
                Frame *old = stack_pop(s);

                /* We reached the top of the stack,
                 * exit loop & return last register value. */
                if (s->size == 0)
                    return &R[A];

                (*s->frame)->locals[old->result] = RK(A);

                for (int i = 0; i < proc->stack->size; i++)
                    debug(INDENT);
                debug("%s/%s\n", (*s->frame)->module->name,
                                 (*s->frame)->path->name);
                free(old);
                goto reentry;
            }
            default:
                assert(0);
                break;
        }
        if (proc->credits == 0) {
            Process *np;

            if ((np = vm_select(vm))) {
                proc = np;
                goto reentry;
            } else {
                proc->credits = 16;
            }
        }
    }
    return NULL;
}

TValue *vm_run(VM *vm, const char *module, const char *path)
{
    Module *m = vm_module(vm, module);

    if (! m)
        error(2, 0, "couldn't find module '%s'", module);

    Path *p = module_path(m, path);

    if (! p)
        error(2, 0, "couldn't find path '%s/%s'", module, path);

    Process *proc = vm_spawn(vm, m, p);

    vm_call(vm, proc, p->clauses[0], NULL);

    return vm_execute(vm, proc);
}

Module *vm_module(VM *vm, const char *name)
{
    uint32_t key = hash(name, strlen(name)) % 512;
    ModuleList *ms  = vm->modules[key];

    while (ms && ms->head) {
        if (! strcmp(ms->head->name, name)) {
            return ms->head;
        }
        ms = ms->tail;
    }
    return NULL;
}
