#include "secd.h"
#include "secd_io.h"
#include "secdops.h"
#include "memory.h"
#include "env.h"

#include <string.h>

/*
 *  Compiled form of control paths
 */

index_t search_opcode_table(cell_t *sym);

cell_t *compile_control_path(secd_t *secd, cell_t *control) {
    assert_cell(control, "control path is invalid");
    cell_t *compiled = SECD_NIL;

    cell_t *cursor = control;
    cell_t *compcursor = compiled;
    while (not_nil(cursor)) {
        cell_t *opcode = list_head(cursor);
        if (atom_type(secd, opcode) != ATOM_SYM) {
            errorf("compile_control: not a symbol in control path\n");
            sexp_print(secd, opcode); printf("\n");
            return new_error(secd, "compile_control_path: symbol expected");
        }

        index_t opind = search_opcode_table(opcode);
        assert(opind >= 0, "Opcode not found: %s", symname(opcode))

        cell_t *new_cmd = new_op(secd, opind);
        cell_t *cc = new_cons(secd, new_cmd, SECD_NIL);
        if (not_nil(compcursor)) {
            compcursor->as.cons.cdr = share_cell(secd, cc);
            compcursor = list_next(secd, compcursor);
        } else {
            compiled = compcursor = cc;
        }
        cursor = list_next(secd, cursor);

        cell_t *new_tail;

        if (new_cmd->as.atom.as.op == SECD_AP) {
            cell_t *next = list_head(cursor);
            if (atom_type(secd, next) == ATOM_INT) {
                new_tail = new_cons(secd, next, SECD_NIL);
                compcursor->as.cons.cdr = share_cell(secd, new_tail);
                compcursor = list_next(secd, compcursor);
                cursor = list_next(secd, cursor);
            }
        }

        if (opcode_table[opind].args > 0) {
            if (new_cmd->as.atom.as.op == SECD_SEL) {
                cell_t *thenb = compile_control_path(secd, list_head(cursor));
                new_tail = new_cons(secd, thenb, SECD_NIL);
                compcursor->as.cons.cdr = share_cell(secd, new_tail);
                compcursor = list_next(secd, compcursor);
                cursor = list_next(secd, cursor);

                cell_t *elseb = compile_control_path(secd, list_head(cursor));
                new_tail = new_cons(secd, elseb, SECD_NIL);
                compcursor->as.cons.cdr = share_cell(secd, new_tail);
                compcursor = list_next(secd, compcursor);
                cursor = list_next(secd, cursor);
            } else {
                new_tail = new_cons(secd, list_head(cursor), SECD_NIL);
                compcursor->as.cons.cdr = share_cell(secd, new_tail);
                compcursor = list_next(secd, compcursor);
                cursor = list_next(secd, cursor);
            }
        }
    }
    return compiled;
}

bool is_control_compiled(secd_t *secd, cell_t *control) {
    return atom_type(secd, list_head(control)) == ATOM_OP;
}

cell_t* compiled_ctrl(secd_t *secd, cell_t *ctrl) {
    if (is_control_compiled(secd, ctrl))
        return SECD_NIL;

    ctrldebugf(" compiling control path\n");
    cell_t *compiled = compile_control_path(secd, ctrl);
    assert(compiled, "compiled_ctrl: NIL");
    assert_cell(compiled, "compiled_ctrl: failed");

    return compiled;
}

bool compile_ctrl(secd_t *secd, cell_t **ctrl) {
    cell_t *compiled = compiled_ctrl(secd, *ctrl);
    if (is_nil(compiled))
        return false;
    *ctrl = compiled;
    return true;
}


/*
 *  SECD built-ins
 */

cell_t *secd_cons(secd_t *secd) {
    ctrldebugf("CONS\n");
    cell_t *a = pop_stack(secd);

    cell_t *b = pop_stack(secd);

    cell_t *cons = new_cons(secd, a, b);
    drop_cell(secd, a); drop_cell(secd, b);

    return push_stack(secd, cons);
}

cell_t *secd_car(secd_t *secd) {
    ctrldebugf("CAR\n");
    cell_t *cons = pop_stack(secd);
    assert(not_nil(cons), "secd_car: cons is NIL");
    assert_cell(cons, "secd_car: pop_stack() failed");
    assert(is_cons(cons), "secd_car: cons expected");

    cell_t *car = push_stack(secd, get_car(cons));
    drop_cell(secd, cons);
    return car;
}

cell_t *secd_cdr(secd_t *secd) {
    ctrldebugf("CDR\n");
    cell_t *cons = pop_stack(secd);
    assert(not_nil(cons), "secd_cdr: cons is NIL");
    assert_cell(cons, "secd_cdr: pop_stack() failed");
    assert(is_cons(cons), "secd_cdr: cons expected");

    cell_t *cdr = push_stack(secd, get_cdr(cons));
    drop_cell(secd, cons);
    return cdr;
}

cell_t *secd_ldc(secd_t *secd) {
    ctrldebugf("LDC\n");

    cell_t *arg = pop_control(secd);
    assert_cell(arg, "secd_ldc: pop_control failed");

    push_stack(secd, arg);
    drop_cell(secd, arg);
    return arg;
}

cell_t *secd_ld(secd_t *secd) {
    ctrldebugf("LD\n");

    cell_t *arg = pop_control(secd);
    assert_cell(arg, "secd_ld: stack empty");
    assert(atom_type(secd, arg) == ATOM_SYM,
           "secd_ld: not a symbol [%ld]", cell_index(secd, arg));

    const char *sym = symname(arg);
    cell_t *val = lookup_env(secd, sym);
    drop_cell(secd, arg);
    assert_cellf(val, "lookup failed for %s", sym);
    return push_stack(secd, val);
}

static inline cell_t *to_bool(secd_t *secd, bool cond) {
    return ((cond)? lookup_env(secd, "#t") : SECD_NIL);
}

bool atom_eq(secd_t *secd, const cell_t *a1, const cell_t *a2) {
    enum atom_type atype1 = atom_type(secd, a1);
    if (a1 == a2)
        return true;
    if (atype1 != atom_type(secd, a2))
        return false;
    switch (atype1) {
      case ATOM_INT: return (a1->as.atom.as.num == a2->as.atom.as.num);
      case ATOM_SYM: return (str_eq(symname(a1), symname(a2)));
      case ATOM_OP: return (a1->as.atom.as.op == a2->as.atom.as.op);
      case ATOM_FUNC: return (a1->as.atom.as.ptr == a2->as.atom.as.ptr);
      default: errorf("atom_eq(secd, [%ld], [%ld]): don't know how to handle type %d\n",
                       cell_index(secd, a1), cell_index(secd, a2), atype1);
    }
    return false;
}

bool list_eq(secd_t *secd, const cell_t *xs, const cell_t *ys) {
    asserti(is_cons(xs), "list_eq: [%ld] is not a cons", cell_index(secd, xs));

    if (xs == ys)
        return true;
    while (not_nil(xs)) {
        if (!is_cons(xs))
            return is_equal(secd, xs, ys);
        if (cell_type(xs) != cell_type(ys))
            return false;
        if (is_nil(ys))
            return false;

        const cell_t *x = get_car(xs);
        const cell_t *y = get_car(ys);
        if (not_nil(x) ? is_nil(y) : not_nil(y))
            return false;
        if (!is_equal(secd, x, y))
            return false;

        xs = list_next(secd, xs);
        ys = list_next(secd, ys);
    }
    return is_nil(ys);
}

bool array_eq(secd_t *secd, const cell_t *a, const cell_t *b) {
    if (a == b)
        return true;

    size_t len = arr_size(secd, a);
    if (len != arr_size(secd, b))
        return false;

    size_t i;
    for (i = 0; i < len; ++i) {
        if (!is_equal(secd, arr_val(a, i), arr_val(b, i)))
            return false;
    }

    return true;
}

bool is_equal(secd_t *secd, const cell_t *a, const cell_t *b) {
    if (a == b)
        return true;
    if (is_nil(a))
        return is_nil(b);
    if (cell_type(a) != cell_type(b))
        return false;

    switch (cell_type(a)) {
      case CELL_CONS: return list_eq(secd, a, b);
      case CELL_ATOM: return atom_eq(secd, a, b);
      case CELL_STR:  return !strcmp(strval(a), strval(b));
      case CELL_ARRAY: return array_eq(secd, a, b);
      case CELL_UNDEF: return true;

      case CELL_ARRMETA: case CELL_ERROR:
      case CELL_FREE: case CELL_FRAME:
      case CELL_REF:  case CELL_PORT:
           errorf("is_equal: comparing internal data");
    }
    return false;
}

cell_t *secd_type(secd_t *secd) {
    ctrldebugf("TYPE\n");
    cell_t *val = pop_stack(secd);
    assert_cell(val, "secd_type: pop_stack() failed");

    const char *type = "unknown";
    switch (cell_type(val)) {
      case CELL_CONS:  type = "cons"; break;
      case CELL_STR:   type = "str";  break;
      case CELL_ARRAY: type = "vect"; break;
      case CELL_PORT:  type = "port"; break;
      case CELL_FRAME: type = "frame"; break;
      case CELL_ATOM:
        switch (atom_type(secd, val)) {
          case NOT_AN_ATOM: return new_error(secd, "not an atom");
          case ATOM_INT: type = "int"; break;
          case ATOM_SYM: type = "sym"; break;
          case ATOM_FUNC: type = "func"; break;
          case ATOM_OP:  type = "op"; break;
        } break;
      case CELL_UNDEF: type = "void"; break;
      case CELL_ARRMETA: type = "meta"; break;
      case CELL_ERROR: type = "err"; break;
      case CELL_REF:   type = "ref"; break;
      case CELL_FREE:  type = "free"; break;
    }

    drop_cell(secd, val);
    return push_stack(secd, new_symbol(secd, type));
}

cell_t *secd_eq(secd_t *secd) {
    ctrldebugf("EQ\n");
    cell_t *a = pop_stack(secd);
    assert_cell(a, "secd_eq: pop_stack(a) failed");

    cell_t *b = pop_stack(secd);
    assert_cell(b, "secd_eq: pop_stack(b) failed");

    cell_t *val = to_bool(secd, is_equal(secd, a, b));
    drop_cell(secd, a); drop_cell(secd, b);
    return push_stack(secd, val);
}

static cell_t *arithm_op(secd_t *secd, int op(int, int)) {
    cell_t *a = pop_stack(secd);
    assert_cell(a, "secd_arithm: pop_stack(a) failed")
    assert(atom_type(secd, a) == ATOM_INT, "secd_add: a is not int");

    cell_t *b = pop_stack(secd);
    assert_cell(b, "secd_arithm: pop_stack(b) failed");
    assert(atom_type(secd, b) == ATOM_INT, "secd_add: b is not int");

    int res = op(a->as.atom.as.num, b->as.atom.as.num);
    drop_cell(secd, a); drop_cell(secd, b);
    return push_stack(secd, new_number(secd, res));
}

inline static int iplus(int x, int y) {
    return x + y;
}
inline static int iminus(int x, int y) {
    return x - y;
}
inline static int imult(int x, int y) {
    return x * y;
}
inline static int idiv(int x, int y) {
    return x / y;
}
inline static int irem(int x, int y) {
    return x % y;
}

cell_t *secd_add(secd_t *secd) {
    ctrldebugf("ADD\n");
    return arithm_op(secd, iplus);
}
cell_t *secd_sub(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, iminus);
}
cell_t *secd_mul(secd_t *secd) {
    ctrldebugf("MUL\n");
    return arithm_op(secd, imult);
}
cell_t *secd_div(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, idiv);
}
cell_t *secd_rem(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, irem);
}

cell_t *secd_leq(secd_t *secd) {
    ctrldebugf("LEQ\n");

    cell_t *opnd1 = pop_stack(secd);
    cell_t *opnd2 = pop_stack(secd);

    assert(atom_type(secd, opnd1) == ATOM_INT, "secd_leq: int expected as opnd1");
    assert(atom_type(secd, opnd2) == ATOM_INT, "secd_leq: int expected as opnd2");

    cell_t *result = to_bool(secd, opnd1->as.atom.as.num <= opnd2->as.atom.as.num);
    drop_cell(secd, opnd1); drop_cell(secd, opnd2);
    return push_stack(secd, result);
}

cell_t *secd_sel(secd_t *secd) {
    ctrldebugf("SEL\n");

    cell_t *condcell = pop_stack(secd);

    bool cond = not_nil(condcell) ? true : false;
    drop_cell(secd, condcell);

    cell_t *thenb = pop_control(secd);
    cell_t *elseb = pop_control(secd);
    assert(is_cons(thenb) && is_cons(elseb), "secd_sel: both branches must be conses");

    cell_t *joinb = secd->control;
    secd->control = share_cell(secd, cond ? thenb : elseb);

    push_dump(secd, joinb);

    drop_cell(secd, thenb); drop_cell(secd, elseb); drop_cell(secd, joinb);
    return secd->control;
}

cell_t *secd_join(secd_t *secd) {
    ctrldebugf("JOIN\n");

    cell_t *joinb = pop_dump(secd);
    assert_cell(joinb, "secd_join: pop_dump() failed");

    secd->control = joinb; //share_cell(secd, joinb); drop_cell(secd, joinb);
    return secd->control;
}



cell_t *secd_ldf(secd_t *secd) {
    ctrldebugf("LDF\n");

    cell_t *func = pop_control(secd);
    assert_cell(func, "secd_ldf: failed to get the control path");

    cell_t *body = list_head(list_next(secd, func));
    cell_t *compiled = compiled_ctrl(secd, body);
    if (compiled) {
        drop_cell(secd, body);
        func->as.cons.cdr->as.cons.car = share_cell(secd, compiled);
    }

    cell_t *closure = new_cons(secd, func, secd->env);
    drop_cell(secd, func);
    return push_stack(secd, closure);
}

#if TAILRECURSION
/*
 * returns a new dump for a tail-recursive call
 * or SECD_NIL if no Tail Call Optimization.
 */
static cell_t * new_dump_if_tailrec(secd_t *secd, cell_t *control, cell_t *dump) {
    if (is_nil(control))
        return SECD_NIL;

    cell_t *nextop = list_head(control);
    if (atom_type(secd, nextop) != ATOM_OP)
        return SECD_NIL;

    opindex_t opind = nextop->as.atom.as.op;

    if (opind == SECD_RTN) {
        return dump;
    } else if (opind == SECD_JOIN) {
        cell_t *join = list_head(dump);
        return new_dump_if_tailrec(secd, join, list_next(secd, dump));
    } else if (opind == SECD_CONS) {
        /* a situation of CONS CAR - it is how `begin` implemented */
        cell_t *nextcontrol = list_next(secd, control);
        cell_t *afternext = list_head(nextcontrol);
        if (atom_type(secd, afternext) != ATOM_OP)
            return SECD_NIL;
        if (afternext->as.atom.as.op != SECD_CAR)
            return SECD_NIL;
        return new_dump_if_tailrec(secd, list_next(secd, nextcontrol), dump);
    }

    /* all other commands (except DUM, which must have RAP after it)
     * mess with the stack. TCO's not possible: */
    return SECD_NIL;
}
#endif

static cell_t *extract_argvals(secd_t *secd) {
    if (atom_type(secd, list_head(secd->control)) != ATOM_INT) {
        return pop_stack(secd); // don't forget to drop
    }

    cell_t *argvals = SECD_NIL;
    cell_t *argvcursor = SECD_NIL;
    cell_t *new_stack = secd->stack;

    cell_t *ntop = pop_control(secd);
    int n = numval(ntop);

    while (n-- > 0) {
        argvcursor = new_stack;
        new_stack = list_next(secd, new_stack);
    }
    if (not_nil(argvcursor)) {
        argvals = secd->stack;
        argvcursor->as.cons.cdr = SECD_NIL;
    }
    secd->stack = new_stack; // no share_cell

    drop_cell(secd, ntop);
    // has at least 1 "ref", don't forget to drop
    return argvals;
}

static cell_t *new_frame_io(secd_t *secd, cell_t *frame, cell_t *prevenv) {
    /* check if there are *stdin* or *stdout* in new frame */
    cell_t *frame_io = SECD_NIL;
    cell_t *symlist = get_car(frame);
    cell_t *vallist = get_cdr(frame);
    while (not_nil(symlist)) {
        cell_t *sym = get_car(symlist);
        if (str_eq(symname(sym), "*stdin*")) {
            cell_t *val = get_car(vallist);
            assert(cell_type(val) == CELL_PORT, "*stdin* must bind a port");
            if (is_nil(frame_io))
                frame_io = new_cons(secd, val, SECD_NIL);
            else
                frame_io->as.cons.car = share_cell(secd, val);
        }
        if (str_eq(symname(sym), "*stdout*")) {
            cell_t *val = get_car(vallist);
            assert(cell_type(val) == CELL_PORT, "*stdout* must bind a port");
            if (is_nil(frame_io))
                frame_io = new_cons(secd, SECD_NIL, val);
            else
                frame_io->as.cons.cdr = share_cell(secd, val);
        }

        symlist = list_next(secd, symlist);
        vallist = list_next(secd, vallist);
    }
    cell_t *prev_io = get_car(prevenv)->as.frame.io;
    if (is_nil(frame_io))
        return prev_io;

    if (is_nil(get_car(frame_io)))
        frame_io->as.cons.car = share_cell(secd, get_car(prev_io));
    if (is_nil(get_cdr(frame_io)))
        frame_io->as.cons.cdr = share_cell(secd, get_cdr(prev_io));
    return frame_io;
}

static cell_t *secd_ap_native(secd_t *secd, cell_t *clos, cell_t *args) {
    secd_nativefunc_t native = (secd_nativefunc_t)clos->as.atom.as.ptr;
    cell_t *result = native(secd, args);
    assert_cell(result, "secd_ap: a built-in routine failed");
    push_stack(secd, result);

    drop_cell(secd, clos); drop_cell(secd, args);
    return result;
}

cell_t *secd_ap(secd_t *secd) {
    ctrldebugf("AP\n");

    cell_t *closure = pop_stack(secd);
    assert_cell(closure, "secd_ap: pop_stack(closure) failed");

    cell_t *argvals = extract_argvals(secd);
    assert_cell(argvals, "secd_ap: no arguments on stack");
    assert(is_cons(argvals), "secd_ap: a list expected for arguments");

    if (atom_type(secd, closure) == ATOM_FUNC)
        return secd_ap_native(secd, closure, argvals);

    assert(is_cons(closure), "secd_ap: closure is not a cons");
    cell_t *func = get_car(closure);
    assert(is_cons(func), "secd_ap: not a cons at func definition");

    cell_t *newenv = get_cdr(closure);
    assert(is_cons(newenv), "secd_ap: not a cons at env in closure");
    assert(not_nil(newenv), "secd_ap: nil env");
    if (cell_type(list_head(newenv)) != CELL_FRAME) {
        errorf("secd_ap: env holds not a frame\n");
        dbg_printc(secd, newenv);
        return new_error(secd, "not a frame");
    }

    cell_t *argnames = get_car(func);
    cell_t *control = list_head(list_next(secd, func));
    assert(is_cons(control), "secd_ap: control path is not a list");

    compile_ctrl(secd, &control);

#if TAILRECURSION
    cell_t *new_dump = new_dump_if_tailrec(secd, secd->control, secd->dump);
    if (new_dump) {
        assign_cell(secd, &secd->dump, new_dump);
        ctrldebugf("secd_ap: tailrec\n");
    } else {
        push_dump(secd, secd->control);
        push_dump(secd, secd->env);
        push_dump(secd, secd->stack);
    }
#else
    push_dump(secd, secd->control);
    push_dump(secd, secd->env);
    push_dump(secd, secd->stack);
#endif

    drop_cell(secd, secd->stack);
    secd->stack = SECD_NIL;

    cell_t *frame = new_frame(secd, argnames, argvals);
    cell_t *new_io = new_frame_io(secd, frame, newenv);
    assert_cell(new_io, "secd_ap: failed to set new frame I/O\n");
    frame->as.frame.io = share_cell(secd, new_io);
    secd->input_port = get_car(new_io);
    secd->output_port = get_cdr(new_io);

    cell_t *oldenv = secd->env;
    memdebugf("secd_ap: dropping env[%ld]\n", cell_index(secd, oldenv));
    secd->env = share_cell(secd, new_cons(secd, frame, newenv));
    drop_cell(secd, oldenv);

    if (ENVDEBUG) print_env(secd);

    set_control(secd, control);
    /*
    cell_t *oldctrl = secd->control;
    secd->control = share_cell(secd, control);
    drop_cell(secd, oldctrl);
    */

    drop_cell(secd, closure); drop_cell(secd, argvals);
    return control;
}

cell_t *secd_rtn(secd_t *secd) {
    ctrldebugf("RTN\n");

    assert(is_nil(secd->control), "secd_rtn: commands after RTN");

    assert(not_nil(secd->stack), "secd_rtn: stack is empty");
    cell_t *top = pop_stack(secd);
    assert(is_nil(secd->stack), "secd_rtn: stack holds more than 1 value");

    cell_t *prevstack = pop_dump(secd);
    cell_t *prevenv = pop_dump(secd);
    cell_t *prevcontrol = pop_dump(secd);

    drop_cell(secd, secd->env);

    secd->stack = share_cell(secd, new_cons(secd, top, prevstack));
    drop_cell(secd, top); drop_cell(secd, prevstack);

    secd->control = prevcontrol; // share_cell(secd, prevcontrol); drop_cell(secd, prevcontrol);
    secd->env = prevenv; // share_cell(secd, prevenv); drop_cell(secd, prevenv);

    /* restoring I/O */
    cell_t *frame_io = get_car(prevenv);
    secd->input_port = get_car(frame_io->as.frame.io);
    secd->output_port = get_cdr(frame_io->as.frame.io);

    return top;
}


cell_t *secd_dum(secd_t *secd) {
    ctrldebugf("DUM\n");

    cell_t *oldenv = secd->env;
    cell_t *newenv = new_cons(secd, SECD_NIL, oldenv);

    secd->env = share_cell(secd, newenv);
    drop_cell(secd, oldenv);

    return newenv;
}

cell_t *secd_rap(secd_t *secd) {
    ctrldebugf("RAP\n");

    cell_t *closure = pop_stack(secd);
    cell_t *argvals = pop_stack(secd);

    cell_t *newenv = get_cdr(closure);
    cell_t *func = get_car(closure);
    cell_t *argnames = get_car(func);
    cell_t *control = get_car(list_next(secd, func));

    compile_ctrl(secd, &control);

    push_dump(secd, secd->control);
    push_dump(secd, get_cdr(secd->env));
    push_dump(secd, secd->stack);

    cell_t *frame = new_frame(secd, argnames, argvals);
    /* list_next(newenv) because it has a dummy frame here */
    cell_t *new_io = new_frame_io(secd, frame, list_next(secd, newenv));
    assert_cell(new_io, "secd_rap: failed to set new frame I/O\n");
    frame->as.frame.io = share_cell(secd, new_io);
    secd->input_port = get_car(new_io);
    secd->output_port = get_cdr(new_io);

#if ENVDEBUG
    printf("new frame: \n"); dbg_printc(secd, frame);
    printf(" argnames: \n"); dbg_printc(secd, argnames);
    printf(" argvals : \n"); dbg_printc(secd, argvals);
#endif
    newenv->as.cons.car = share_cell(secd, frame);

    drop_cell(secd, secd->stack);
    secd->stack = SECD_NIL;

    cell_t *oldenv = secd->env;
    secd->env = share_cell(secd, newenv);

    set_control(secd, control);

    drop_cell(secd, oldenv);
    drop_cell(secd, closure); drop_cell(secd, argvals);
    return secd->control;
}


cell_t *secd_read(secd_t *secd) {
    ctrldebugf("READ\n");

    errorf("\n");
    cell_t *inp = sexp_parse(secd, SECD_NIL);
    assert_cell(inp, "secd_read: failed to read");

    push_stack(secd, inp);
    return inp;
}

cell_t *secd_print(secd_t *secd) {
    ctrldebugf("PRINT\n");

    cell_t *top = get_car(secd->stack);
    assert_cell(top, "secd_print: no stack");

    sexp_print(secd, top);
    printf("\n");
    return top;
}

const cell_t cons_func  = INIT_OP(SECD_CONS);
const cell_t car_func   = INIT_OP(SECD_CAR);
const cell_t cdr_func   = INIT_OP(SECD_CDR);
const cell_t add_func   = INIT_OP(SECD_ADD);
const cell_t sub_func   = INIT_OP(SECD_SUB);
const cell_t mul_func   = INIT_OP(SECD_MUL);
const cell_t div_func   = INIT_OP(SECD_DIV);
const cell_t rem_func   = INIT_OP(SECD_REM);
const cell_t leq_func   = INIT_OP(SECD_LEQ);
const cell_t ldc_func   = INIT_OP(SECD_LDC);
const cell_t ld_func    = INIT_OP(SECD_LD);
const cell_t eq_func    = INIT_OP(SECD_EQ);
const cell_t type_func  = INIT_OP(SECD_TYPE);
const cell_t sel_func   = INIT_OP(SECD_SEL);
const cell_t join_func  = INIT_OP(SECD_JOIN);
const cell_t ldf_func   = INIT_OP(SECD_LDF);
const cell_t ap_func    = INIT_OP(SECD_AP);
const cell_t rtn_func   = INIT_OP(SECD_RTN);
const cell_t dum_func   = INIT_OP(SECD_DUM);
const cell_t rap_func   = INIT_OP(SECD_RAP);
const cell_t read_func  = INIT_OP(SECD_READ);
const cell_t print_func = INIT_OP(SECD_PRN);
const cell_t stop_func  = INIT_OP(SECD_STOP);

const cell_t ap_sym     = INIT_SYM("AP");
const cell_t add_sym    = INIT_SYM("ADD");
const cell_t car_sym    = INIT_SYM("CAR");
const cell_t cdr_sym    = INIT_SYM("CDR");
const cell_t cons_sym   = INIT_SYM("CONS");
const cell_t div_sym    = INIT_SYM("DIV");
const cell_t dum_sym    = INIT_SYM("DUM");
const cell_t eq_sym     = INIT_SYM("EQ");
const cell_t join_sym   = INIT_SYM("JOIN");
const cell_t ld_sym     = INIT_SYM("LD");
const cell_t ldc_sym    = INIT_SYM("LDC");
const cell_t ldf_sym    = INIT_SYM("LDF");
const cell_t leq_sym    = INIT_SYM("LEQ");
const cell_t mul_sym    = INIT_SYM("MUL");
const cell_t print_sym  = INIT_SYM("PRINT");
const cell_t rap_sym    = INIT_SYM("RAP");
const cell_t read_sym   = INIT_SYM("READ");
const cell_t rem_sym    = INIT_SYM("REM");
const cell_t rtn_sym    = INIT_SYM("RTN");
const cell_t sel_sym    = INIT_SYM("SEL");
const cell_t stop_sym   = INIT_SYM("STOP");
const cell_t sub_sym    = INIT_SYM("SUB");
const cell_t type_sym   = INIT_SYM("TYPE");

const opcode_t opcode_table[] = {
    // opcodes: for information, not to be called
    // keep symbols sorted properly
    [SECD_ADD]  = { &add_sym,     secd_add,  0, -1},
    [SECD_AP]   = { &ap_sym,      secd_ap,   0, -1},
    [SECD_CAR]  = { &car_sym,     secd_car,  0,  0},
    [SECD_CDR]  = { &cdr_sym,     secd_cdr,  0,  0},
    [SECD_CONS] = { &cons_sym,    secd_cons, 0, -1},
    [SECD_DIV]  = { &div_sym,     secd_div,  0, -1},
    [SECD_DUM]  = { &dum_sym,     secd_dum,  0,  0},
    [SECD_EQ]   = { &eq_sym,      secd_eq,   0, -1},
    [SECD_JOIN] = { &join_sym,    secd_join, 0,  0},
    [SECD_LD]   = { &ld_sym,      secd_ld,   1,  1},
    [SECD_LDC]  = { &ldc_sym,     secd_ldc,  1,  1},
    [SECD_LDF]  = { &ldf_sym,     secd_ldf,  1,  1},
    [SECD_LEQ]  = { &leq_sym,     secd_leq,  0, -1},
    [SECD_MUL]  = { &mul_sym,     secd_mul,  0, -1},
    [SECD_PRN]  = { &print_sym,   secd_print,0,  0},
    [SECD_RAP]  = { &rap_sym,     secd_rap,  0, -1},
    [SECD_READ] = { &read_sym,    secd_read, 0,  1},
    [SECD_REM]  = { &rem_sym,     secd_rem,  0, -1},
    [SECD_RTN]  = { &rtn_sym,     secd_rtn,  0,  0},
    [SECD_SEL]  = { &sel_sym,     secd_sel,  2, -1},
    [SECD_STOP] = { &stop_sym,    SECD_NIL,  0,  0},
    [SECD_SUB]  = { &sub_sym,     secd_sub,  0, -1},
    [SECD_TYPE] = { &type_sym,    secd_type, 0,  0},

    [SECD_LAST] = { NULL,         NULL,      0,  0}
};

index_t optable_len = 0;

inline size_t opcode_count(void) {
    if (optable_len == 0)
        while (opcode_table[optable_len].sym) ++optable_len;
    return optable_len;
}

index_t search_opcode_table(cell_t *sym) {
    index_t a = 0;
    index_t b = opcode_count();

    while (a != b) {
        index_t c = (a + b) / 2;
        int ord = str_cmp( symname(sym), symname(opcode_table[c].sym));
        if (ord == 0) return c;
        if (ord < 0) b = c;
        else a = c;
    }
    return -1;
}

