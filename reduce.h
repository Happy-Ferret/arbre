/*
 * arbre
 *
 * (c) 2011-2012, Alexis Sellier
 *
 * reduce.h
 *
 */
typedef struct {
    int _;
} Reducer;

static NodeList *reduce_nodelist(Reducer *r, NodeList *ns);
static Node *reduce_path(Reducer *r, Node *n);
static Node *reduce_list(Reducer *r, Node *n);
static Node *reduce_block(Reducer *r, Node *n);
static Node *reduce_bind(Reducer *r, Node *n);
static Node *reduce_apply(Reducer *r, Node *n);
static Node *reduce_select(Reducer *r, Node *n);

Node *(*REDUCERS[])(Reducer*, Node*) = {
    [OBLOCK]    =  reduce_block,  [ODECL]     =  NULL,
    [OMATCH]    =  NULL,          [OBIND]     =  reduce_bind,
    [OMODULE]   =  NULL,          [OSELECT]   =  reduce_select,
    [OWAIT]     =  NULL,          [OIDENT]    =  NULL,
    [OTYPE]     =  NULL,          [OADD]      =  NULL,
    [OPATH]     =  reduce_path,   [OMPATH]    =  NULL,
    [OSTRING]   =  NULL,          [OATOM]     =  NULL,
    [OCHAR]     =  NULL,          [ONUMBER]   =  NULL,
    [OTUPLE]    =  NULL,          [OLIST]     =  reduce_list,
    [OACCESS]   =  NULL,          [OAPPLY]    =  reduce_apply,
    [OSEND]     =  NULL,          [ORANGE]    =  NULL,
    [OCLAUSE]   =  NULL,          [OPIPE]     =  NULL,
    [OSUB]      =  NULL,          [OLT]       =  NULL,
    [OGT]       =  NULL
};

static Node *anode(OP op)
{
    Node *n   = calloc(1, sizeof(*n));
    n->op     = op;
    return n;
}

static Node *cons(Node *lval, Node *rval)
{
    Node *n = anode(OCONS);
    n->o.cons.lval = lval;
    n->o.cons.rval = rval;
    return n;
}

static Node *reduce_list(Reducer *r, Node *n)
{
    if (n->o.list.length == 0)
        return cons(NULL, NULL);

    NodeList *ns = n->o.list.items;
    NodeList *end = ns->end;

    Node *new = NULL;

    while (end) {
        new = cons(end->head, new);
        end = end->prev;
    }

    return new;
}

static void reduce_node(Reducer *r, Node **n)
{
    Node *new;

    if (*n == NULL)
        return;

    if (REDUCERS[(*n)->op]) {
        new = REDUCERS[(*n)->op](r, *n);
        if (new != *n) {
            // TODO: Free *n
        }
        *n = new;
    }
}

static Node *reduce_bind(Reducer *r, Node *n)
{
    reduce_node(r, &n->o.bind.lval);
    reduce_node(r, &n->o.bind.rval);
    return n;
}

static Node *reduce_apply(Reducer *r, Node *n)
{
    reduce_node(r, &n->o.apply.lval);
    reduce_node(r, &n->o.apply.rval);
    return n;
}

static Node *reduce_select(Reducer *r, Node *n)
{
    reduce_node(r, &n->o.select.arg);
    reduce_nodelist(r, n->o.select.clauses);
    return n;
}

static Node *reduce_clause(Reducer *r, Node *n)
{
    reduce_block(r, n->o.clause.rval);
    return n;
}

static Node *reduce_path(Reducer *r, Node *n)
{
    reduce_clause(r, n->o.path.clause);
    return n;
}

static Node *reduce_block(Reducer *r, Node *n)
{
    NodeList *ns = n->o.block.body;
    reduce_nodelist(r, ns);
    return n;
}

static NodeList *reduce_nodelist(Reducer *r, NodeList *ns)
{
    while (ns) {
        reduce_node(r, &ns->head);
        ns = ns->tail;
    }
    return ns;
}

void reduce(Tree *tree)
{
    Reducer *r = malloc(sizeof(*r));

    reduce_block(r, tree->root);

    free(r);
}
