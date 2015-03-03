#include "secd.h"
#include "secd_io.h"
#include "secdops.h"
#include "memory.h"
#include "env.h"

#include <string.h>  /* strcmp(), memcmp() */

/*
 *  Compiled form of control paths
 */

inline static void tail_append(secd_t *secd, cell_t **tail, cell_t *to) {
    if (is_nil(to)) return;
    (*tail)->as.cons.cdr = share_cell(secd, to);
    *tail = list_next(secd, *tail);
}

static void tail_append_or_init(secd_t *secd, cell_t **list, cell_t **tail, cell_t *to) {
    if (not_nil(*tail))
        tail_append(secd, tail, to);
    else
        *list = *tail = to;
}

static void tail_append_and_move(secd_t *secd, cell_t **lst, cell_t **tail, cell_t *val) {
    tail_append_or_init(secd, lst, tail, val);
    while (not_nil(list_next(secd, *tail)))
        *tail = list_next(secd, *tail);
}

cell_t *compile_control_path(secd_t *secd, cell_t *control) {
    assert_cell(control, "control path is invalid");
    cell_t *compiled = SECD_NIL;

    cell_t *cursor = control;
    cell_t *compcursor = compiled;

    while (not_nil(cursor)) {
        cell_t *opcode = list_head(cursor);
        cursor = list_next(secd, cursor);

        if (!is_symbol(opcode)) {
            errorf("compile_control: not a symbol in control path\n");
            sexp_print(secd, opcode); secd_printf(secd, "\n");
            return new_error(secd, SECD_NIL, "compile_control_path: symbol expected");
        }

        int opind = secdop_by_name( symname(opcode) );
        assert(opind >= 0, "Opcode not found: %s", symname(opcode))

        cell_t *new_cmd = new_op(secd, opind);
        tail_append_and_move(secd, &compiled, &compcursor,
                             new_cons(secd, new_cmd, SECD_NIL));

        if (new_cmd->as.op == SECD_AP) {
            /* look ahead for possible number of arguments after AP */
            cell_t *next = list_head(cursor);
            if (is_number(next)) {
                tail_append(secd, &compcursor,
                            new_cons(secd, next, SECD_NIL));
                cursor = list_next(secd, cursor);
            }
        }

        if (opcode_table[opind].args > 0) {
            switch (new_cmd->as.op) {
              case SECD_SEL: {
                    cell_t *thenb = compile_control_path(secd, list_head(cursor));
                    assert_cell(thenb, "compile_control_path: failed at thenb");
                    tail_append(secd, &compcursor, new_cons(secd, thenb, SECD_NIL));
                    cursor = list_next(secd, cursor);

                    cell_t *elseb = compile_control_path(secd, list_head(cursor));
                    assert_cell(thenb, "compile_control_path: failed at elseb");
                    tail_append(secd, &compcursor, new_cons(secd, elseb, SECD_NIL));
                    cursor = list_next(secd, cursor);
                } break;

              case SECD_LDF: {
                    cell_t *func = list_head(cursor);
                    cursor = list_next(secd, cursor);

                    cell_t *args = get_car(func);
                    cell_t *body = get_car(get_cdr(func));
                    cell_t *other = get_cdr(get_cdr(func));
                    cell_t *compiled = compile_control_path(secd, body);
                    assert_cell(compiled, "compile_control_path: failed at LDF");
                    cell_t *newfunc = new_cons(secd, args, new_cons(secd, compiled, other));
                    tail_append(secd, &compcursor, new_cons(secd, newfunc, SECD_NIL));
                } break;

              case SECD_LD:
                assert(is_symbol(list_head(cursor)),
                       "compile_ctrl: not a symbol after LD");
                // fall through

              default:
                tail_append(secd, &compcursor,
                            new_cons(secd, list_head(cursor), SECD_NIL));
                cursor = list_next(secd, cursor);
            }
        }
    }
    return compiled;
}

bool is_control_compiled(cell_t *control) {
    return cell_type(list_head(control)) == CELL_OP;
}

cell_t* compiled_ctrl(secd_t *secd, cell_t *ctrl) {
    if (is_control_compiled(ctrl))
        return SECD_NIL;

    ctrldebugf(" compiling control path\n");
    cell_t *compiled = compile_control_path(secd, ctrl);
    assert(compiled, "compiled_ctrl: NIL");
    assert_cell(compiled, "compiled_ctrl: failed");

    return compiled;
}

cell_t * compile_ctrl(secd_t *secd, cell_t **ctrl) {
    cell_t *compiled = compiled_ctrl(secd, *ctrl);
    if (is_nil(compiled))
        return SECD_NIL;
    assert_cell(compiled, "compile_ctrl: failed");

    return assign_cell(secd, ctrl, compiled);
}

/*
 *  Run a SECD function from native code
 */
cell_t *secd_execute(secd_t *secd, cell_t *clos, cell_t *argv) {
    if (cell_type(clos) == CELL_FUNC) {
        secd_nativefunc_t func = (secd_nativefunc_t)clos->as.ptr;
        return func(secd, argv);
    }
    assert(is_cons(clos), "secd_execute: closure is not a cons");
    assert(not_nil(clos), "secd_execute: nil argument");

    cell_t *func = get_car(clos);
    cell_t *env = get_cdr(clos);

    cell_t *args = get_car(func);
    cell_t *ctrl = get_car(get_cdr(func));

    cell_t *kont = new_current_continuation(secd);

    cell_t *frame = setup_frame(secd, args, argv, env);
    assert_cell(frame, "secd_execute: failed to setup frame");

    /* jump into the closure */
    assign_cell(secd, &secd->stack, SECD_NIL);
    assign_cell(secd, &secd->env, new_cons(secd, frame, env));
    push_dump(secd, SECD_NIL); /* signals to run_secd() to return */

    cell_t *result = run_secd(secd, ctrl);
    share_cell(secd, result);

    assign_cell(secd, &secd->stack, kont->as.kont.stack);
    assign_cell(secd, &secd->env, kont->as.kont.env);
    assign_cell(secd, &secd->control, kont->as.kont.ctrl);
    //dbg_printc(secd, secd->stack);
    //dbg_printc(secd, secd->control);

    free_cell(secd, kont);
    return result;
}

cell_t *secd_raise(secd_t *secd, cell_t *exc) {
    cell_t *symc = NULL;
    cell_t *exchandlers = lookup_env(secd, SECD_EXC_HANDLERS, &symc);
    assert(symc, "raise: no exception handlers");
    assert(not_nil(exchandlers), "raise: no handlers");

    assert(is_cons(exchandlers), "raise: not a cons");
    cell_t *handler = get_car(exchandlers);

    symc = NULL;
    cell_t *ret = lookup_env(secd, SECD_EXC_BACKTRACE, &symc);
    if (symc) {
        if (secd_bool(secd, ret))
            secd_print_env(secd);
    }

    cell_t *func = get_car(handler);
    cell_t *eargs = get_car(func);
    cell_t *ectrl = get_car(get_cdr(func));
    assert(cell_type(get_car(ectrl)) == CELL_OP, "raise: exchandler is not a closure");

    cell_t *eargv = new_cons(secd, exc, SECD_NIL);

    cell_t *frame = setup_frame(secd, eargs, eargv, secd->env);
    assert_cell(frame, "raise: setup_frame() failed");

    /* set new SECD state */
    assign_cell(secd, &secd->stack, SECD_NIL);
    assign_cell(secd, &secd->env, new_cons(secd, frame, secd->env));
    set_control(secd, &ectrl);
    assign_cell(secd, &secd->dump, SECD_NIL);

    return SECD_NIL;
}

/*
 *  Cell type encoding with ASCII
 */
const char secd_type_chars[] = {
    [CELL_UNDEF] = 'u',
    [CELL_CONS]  = ':',
    [CELL_ARRAY] = 'v',
    [CELL_STR]   = 'S',
    [CELL_BYTES] = 'b',
    [CELL_FRAME] = 'F',
    [CELL_KONT]  = 'k',
    [CELL_ARRMETA] = 'M',
    [CELL_FREE]  = '0',
    [CELL_REF]   = 'r',

    [CELL_SYM]   = 's',
    [CELL_INT]   = 'i',
    [CELL_CHAR]  = 'c',
    [CELL_OP]    = 'o',
    [CELL_FUNC]  = 'n',
    [CELL_PORT]  = 'p',

    [CELL_ERROR] = 'E',
};


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
    assert_cell(cons, "secd_car: pop_stack() failed");
    assert(not_nil(cons), "secd_car: cons is NIL");

    cell_t *car = push_stack(secd, secd_first(secd, cons));
    drop_cell(secd, cons);
    return car;
}

cell_t *secd_cdr(secd_t *secd) {
    ctrldebugf("CDR\n");
    cell_t *cons = pop_stack(secd);
    assert(not_nil(cons), "secd_cdr: cons is NIL");
    assert_cell(cons, "secd_cdr: pop_stack() failed");

    cell_t *cdr = push_stack(secd, secd_rest(secd, cons));
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
    assert(is_symbol(arg), "secd_ld: not a symbol [%ld]", cell_index(secd, arg));

    const char *sym = symname(arg);

    cell_t *symc = SECD_NIL;
    cell_t *val = lookup_env(secd, sym, &symc);
    assert(symc, "lookup failed for %s", sym);

    drop_cell(secd, arg);
    push_stack(secd, val);
    return SECD_NIL;
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
      case CELL_CONS:  return list_eq(secd, a, b);
      case CELL_ARRAY: return array_eq(secd, a, b);
      case CELL_STR:   return !strcmp(strval(a), strval(b));
      case CELL_SYM:   return a->as.sym.data == b->as.sym.data;
      case CELL_INT: case CELL_CHAR:
                       return (a->as.num == b->as.num);
      case CELL_OP:    return (a->as.op == b->as.op);
      case CELL_FUNC:  return (a->as.ptr == b->as.ptr);
      case CELL_BYTES: {
        const size_t len = mem_size(a);
        if (len != mem_size(b))
            return false;
        return !memcmp(strval(a), strval(b), len);
      }
      case CELL_UNDEF: return true;

      case CELL_ARRMETA: case CELL_ERROR:
      case CELL_FREE: case CELL_FRAME:
      case CELL_REF:  case CELL_PORT:
      case CELL_KONT:
           errorf(";; error: comparing internal data in is_equal()\n");
    }
    return false;
}

cell_t *secd_type(secd_t *secd) {
    ctrldebugf("TYPE\n");
    cell_t *val = pop_stack(secd);
    assert_cell(val, "secd_type: pop_stack() failed");

    cell_t *typec = secd_type_sym(secd, val);
    drop_cell(secd, val);
    return push_stack(secd, typec);
}

cell_t *secd_eq(secd_t *secd) {
    ctrldebugf("EQ\n");
    cell_t *a = pop_stack(secd);
    assert_cell(a, "secd_eq: pop_stack(a) failed");

    cell_t *b = pop_stack(secd);
    assert_cell(b, "secd_eq: pop_stack(b) failed");

    cell_t *val = to_bool(secd, is_equal(secd, a, b));
    push_stack(secd, val);

    drop_cell(secd, a); drop_cell(secd, b);
    return SECD_NIL;
}

static cell_t *arithm_op(secd_t *secd, int (*op)(int, int)) {
    cell_t *a = pop_stack(secd);
    assert_cell(a, "secd_arithm: pop_stack(a) failed")
    assert(is_number(a), "secd_arithm: a is not int");

    cell_t *b = pop_stack(secd);
    assert_cell(b, "secd_arithm: pop_stack(b) failed");
    assert(is_number(b), "secd_arithm: b is not int");

    int res = op(numval(a), numval(b));
    push_stack(secd, new_number(secd, res));

    drop_cell(secd, a); drop_cell(secd, b);
    return SECD_NIL;
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
    ctrldebugf("DIV\n");
    return arithm_op(secd, idiv);
}
cell_t *secd_rem(secd_t *secd) {
    ctrldebugf("REM\n");
    return arithm_op(secd, irem);
}

cell_t *secd_leq(secd_t *secd) {
    ctrldebugf("LEQ\n");

    cell_t *opnd1 = pop_stack(secd);
    cell_t *opnd2 = pop_stack(secd);

    assert(is_number(opnd1) || cell_type(opnd1) == CELL_CHAR,
            "secd_leq: int/char expected as opnd1");
    assert(is_number(opnd2) || cell_type(opnd2) == CELL_CHAR,
            "secd_leq: int/char expected as opnd2");

    cell_t *result = to_bool(secd, numval(opnd1) <= numval(opnd2));
    push_stack(secd, result);

    drop_cell(secd, opnd1); drop_cell(secd, opnd2);
    return SECD_NIL;
}

cell_t *secd_sel(secd_t *secd) {
    ctrldebugf("SEL\n");

    cell_t *condcell = pop_stack(secd);
    bool cond = secd_bool(secd, condcell);
    drop_cell(secd, condcell);

    cell_t *thenb = pop_control(secd);
    cell_t *elseb = pop_control(secd);
    assert(is_cons(thenb) && is_cons(elseb), "secd_sel: both branches must be conses");

    cell_t *joinb = secd->control;
    push_dump(secd, joinb);

    secd->control = share_cell(secd, cond ? thenb : elseb);

    drop_cell(secd, thenb); drop_cell(secd, elseb); drop_cell(secd, joinb);
    return SECD_NIL;
}

cell_t *secd_join(secd_t *secd) {
    ctrldebugf("JOIN\n");

    cell_t *joinb = pop_dump(secd);
    assert_cell(joinb, "secd_join: pop_dump() failed");
    assert(cell_type(get_car(joinb)) == CELL_OP, "secd_join: not a CELL_OP");

    secd->control = joinb; //share_cell(secd, joinb); drop_cell(secd, joinb);
    return SECD_NIL;
}



cell_t *secd_ldf(secd_t *secd) {
    ctrldebugf("LDF\n");

    cell_t *func = pop_control(secd);
    assert_cell(func, "secd_ldf: failed to get the control path");

    cell_t *ret = compile_ctrl(secd, &func->as.cons.cdr->as.cons.car);
    assert_cell(ret, "secd_ldf: failed to compile ctrl");

    cell_t *closure = new_cons(secd, func, secd->env);
    drop_cell(secd, func);
    push_stack(secd, closure);
    return SECD_NIL;
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
    if (cell_type(nextop) != CELL_OP)
        return SECD_NIL;

    opindex_t opind = nextop->as.op;

    if (opind == SECD_RTN) {
        return dump;
    } else if (opind == SECD_JOIN) {
        cell_t *join = list_head(dump);
        return new_dump_if_tailrec(secd, join, list_next(secd, dump));
    } else if (opind == SECD_CONS) {
        /* a situation of CONS CAR - it is how `begin` implemented */
        cell_t *nextcontrol = list_next(secd, control);
        cell_t *afternext = list_head(nextcontrol);
        if (cell_type(afternext) != CELL_OP)
            return SECD_NIL;
        if (afternext->as.op != SECD_CAR)
            return SECD_NIL;
        return new_dump_if_tailrec(secd, list_next(secd, nextcontrol), dump);
    }

    /* all other commands (except DUM, which must have RAP after it)
     * mess with the stack. TCO's not possible: */
    return SECD_NIL;
}
#endif

static cell_t *extract_argvals(secd_t *secd) {
    if (!is_number(list_head(secd->control))) {
        return pop_stack(secd); // don't forget to drop
    }

    cell_t *argvals = SECD_NIL;
    cell_t *argvcursor = SECD_NIL;
    cell_t *new_stack = secd->stack;

    cell_t *ntop = pop_control(secd);
    int n = numval(ntop);
    ctrldebugf(" %d args on stack\n", n);

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

static cell_t *secd_ap_native(secd_t *secd, cell_t *clos, cell_t *argv) {
    secd_nativefunc_t native = (secd_nativefunc_t)clos->as.ptr;
    cell_t *result = native(secd, argv);
    assert_cellf(result, "secd_ap: a built-in routine failed: %s", errmsg(result));
    push_stack(secd, result);

    drop_cell(secd, clos); drop_cell(secd, argv);
    return SECD_NIL;
}

static cell_t *
secd_ap_cont(secd_t *secd, cell_t *kont, cell_t *dump, cell_t *argv)
{
    assert(not_nil(argv), "secd_ap: no arguments for continuation call");
    cell_t *kontval = get_car(argv);

    assign_cell(secd, &secd->stack, kont->as.kont.stack);
    push_stack(secd, kontval);

    assign_cell(secd, &secd->env,   kont->as.kont.env);
    set_control(secd, &kont->as.kont.ctrl);

    assign_cell(secd, &secd->dump, dump); /* whoa, yeah */

    return SECD_NIL;
}

cell_t *secd_ap(secd_t *secd) {
    ctrldebugf("AP\n");

    cell_t *closure = pop_stack(secd);
    assert_cell(closure, "secd_ap: pop_stack(closure) failed");

    cell_t *argvals = extract_argvals(secd);
    assert_cell(argvals, "secd_ap: no arguments on stack");
    assert(is_cons(argvals), "secd_ap: a list expected for arguments");

    if (cell_type(closure) == CELL_FUNC)
        return secd_ap_native(secd, closure, argvals);

    assert(is_cons(closure), "secd_ap: closure is not a cons");
    cell_t *func = get_car(closure);

    if (cell_type(func) == CELL_KONT) {
        cell_t *ret =
            secd_ap_cont(secd, func, get_cdr(closure), argvals);
        drop_cell(secd, closure); drop_cell(secd, argvals);
        return ret;
    }
    assert(is_cons(func), "secd_ap: not a cons at func definition");

    /* unpack closure */
    cell_t *newenv = get_cdr(closure);
    assert(is_cons(newenv), "secd_ap: not a cons at env in closure");
    assert(not_nil(newenv), "secd_ap: nil env");
    assert(cell_type(list_head(newenv)) == CELL_FRAME,
            "secd_ap: env holds not a frame\n");

    cell_t *argnames = get_car(func);
    cell_t *frame = setup_frame(secd, argnames, argvals, newenv);
    assert_cell(frame, "secd_ap: setup_frame() failed");

    /* prepare dump */
#if TAILRECURSION
    cell_t *new_dump = new_dump_if_tailrec(secd, secd->control, secd->dump);
    if (new_dump) {
        assign_cell(secd, &secd->dump, new_dump);
        ctrldebugf("secd_ap: tailrec\n");
    } else {
        push_dump(secd, new_current_continuation(secd));
    }
#else
    push_dump(secd, new_current_continuation(secd));
#endif
    assign_cell(secd, &secd->stack, SECD_NIL);
    assign_cell(secd, &secd->env, new_cons(secd, frame, newenv));
    set_control(secd, &func->as.cons.cdr->as.cons.car);

    if (ENVDEBUG) secd_print_env(secd);
    drop_cell(secd, closure); drop_cell(secd, argvals);
    return SECD_NIL;
}

cell_t *secd_rtn(secd_t *secd) {
    ctrldebugf("RTN\n");

    assert(is_nil(secd->control), "secd_rtn: commands after RTN");

    assert(not_nil(secd->stack), "secd_rtn: stack is empty");
    cell_t *result = pop_stack(secd);
    assert(is_nil(secd->stack), "secd_rtn: stack holds more than 1 value");

    /* new SECD state */
    cell_t *kont = pop_dump(secd);
    assert(cell_type(kont) == CELL_KONT, "secd_rtn: not a continuation on dump");

    set_control(secd, &kont->as.kont.ctrl);
    //secd->control = share_cell(secd, kont->as.kont.ctrl);
    assign_cell(secd, &secd->env, kont->as.kont.env);
    assign_cell(secd, &secd->stack, kont->as.kont.stack);
    push_stack(secd, result);

    /* restoring I/O */
    cell_t *frame_io = get_car(secd->env);
    secd->input_port = get_car(frame_io->as.frame.io);
    secd->output_port = get_cdr(frame_io->as.frame.io);

    drop_cell(secd, result);
    drop_cell(secd, kont);
    return result;
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

    /* unpack closure */
    cell_t *newenv = get_cdr(closure);
    cell_t *func = get_car(closure);
    cell_t *argnames = get_car(func);

    /* setup environment */
    cell_t *frame = setup_frame(secd, argnames, argvals, list_next(secd, newenv));
    assert_cell(frame, "secd_rap: setup_frame() failed");

    newenv->as.cons.car = share_cell(secd, frame);

    /* return info */
    cell_t *retkont = new_continuation(secd,
                        secd->stack, get_cdr(secd->env), secd->control);

    /* new SECD state */
    assign_cell(secd, &secd->stack, SECD_NIL);
    assign_cell(secd, &secd->env, newenv);
    set_control(secd, &func->as.cons.cdr->as.cons.car);
    push_dump(secd, retkont);

    drop_cell(secd, closure); drop_cell(secd, argvals);
    return secd->truth_value;
}

cell_t *secd_apcc(secd_t *secd) {
    ctrldebugf("APCC\n");

    cell_t *closure = pop_stack(secd);
    assert_cell(closure, "secd_apcc: pop_stack(closure) failed");
    assert(is_cons(closure), "secd_apcc: closure is not a cons");

    cell_t *func = get_car(closure);
    assert(is_cons(func), "secd_apcc: not a cons at function in closure");

    cell_t *newenv = get_cdr(closure);
    assert(is_cons(newenv), "secd_apcc: not a cons at env in closure");
    assert(not_nil(newenv), "secd_apcc: empty env");
    assert(cell_type(list_head(newenv)) == CELL_FRAME,
           "secd_apcc: not a frame in newenv");

    cell_t *current_cont = new_current_continuation(secd);

    cell_t *args = get_car(func);
    assert(is_cons(args), "secd_apcc: not a cons at args in closure");
    assert(not_nil(args), "secd_apcc: zero args in closure");
    assert(is_nil(list_next(secd, args)),
           "secd_apcc: more than 1 argument in closure");
    cell_t *argv = share_cell(secd,
        new_cons(secd,
                 new_cons(secd, current_cont, secd->dump),
                 SECD_NIL));
    cell_t *frame = setup_frame(secd, args, argv, newenv);
    assert_cell(frame, "secd_apcc: setup_frame() failed");

    /* new SECD state */
    assign_cell(secd, &secd->stack, SECD_NIL);
    assign_cell(secd, &secd->env, new_cons(secd, frame, newenv));
    set_control(secd, &func->as.cons.cdr->as.cons.car);
    push_dump(secd, current_cont);

    drop_cell(secd, closure); drop_cell(secd, argv);
    return secd->truth_value;
}


cell_t *secd_read(secd_t *secd) {
    ctrldebugf("READ\n");

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
    secd_printf(secd, "\n");
    return top;
}

const opcode_t opcode_table[] = {
    // opcodes: for information, not to be called
    // keep symbols sorted properly!
    [SECD_ADD]  = { "ADD",     secd_add,  0, -1},
    [SECD_AP]   = { "AP",      secd_ap,   0, -1},
    [SECD_APCC] = { "APCC",    secd_apcc, 0,  0},
    [SECD_CAR]  = { "CAR",     secd_car,  0,  0},
    [SECD_CDR]  = { "CDR",     secd_cdr,  0,  0},
    [SECD_CONS] = { "CONS",    secd_cons, 0, -1},
    [SECD_DIV]  = { "DIV",     secd_div,  0, -1},
    [SECD_DUM]  = { "DUM",     secd_dum,  0,  0},
    [SECD_EQ]   = { "EQ",      secd_eq,   0, -1},
    [SECD_JOIN] = { "JOIN",    secd_join, 0,  0},
    [SECD_LD]   = { "LD",      secd_ld,   1,  1},
    [SECD_LDC]  = { "LDC",     secd_ldc,  1,  1},
    [SECD_LDF]  = { "LDF",     secd_ldf,  1,  1},
    [SECD_LEQ]  = { "LEQ",     secd_leq,  0, -1},
    [SECD_MUL]  = { "MUL",     secd_mul,  0, -1},
    [SECD_PRN]  = { "PRINT",   secd_print,0,  0},
    [SECD_RAP]  = { "RAP",     secd_rap,  0, -1},
    [SECD_READ] = { "READ",    secd_read, 0,  1},
    [SECD_REM]  = { "REM",     secd_rem,  0, -1},
    [SECD_RTN]  = { "RTN",     secd_rtn,  0,  0},
    [SECD_SEL]  = { "SEL",     secd_sel,  2, -1},
    [SECD_STOP] = { "STOP",    SECD_NIL,  0,  0},
    [SECD_SUB]  = { "SUB",     secd_sub,  0, -1},
    [SECD_TYPE] = { "TYPE",    secd_type, 0,  0},

    [SECD_LAST] = { NULL,         NULL,      0,  0}
};

int optable_len = 0;

inline size_t opcode_count(void) {
    if (optable_len == 0)
        while (opcode_table[optable_len].name) ++optable_len;
    return optable_len;
}

int secdop_by_name(const char *name) {
    int a = 0;
    int b = opcode_count();

    while (a != b) {
        int c = (a + b) / 2;
        int ord = str_cmp( name, opcode_table[c].name);
        if (ord == 0) return c;
        if (ord < 0) b = c;
        else a = c;
    }
    return -1;
}

