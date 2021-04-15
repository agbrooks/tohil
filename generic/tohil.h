//
// tohil C interface library
//

// include Python.h before including any standard header files
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <object.h>

#include <tcl.h>

#include <assert.h>
#include <dlfcn.h>

#include <stdio.h>

// forward definitions

// tclobj python data type that consists of a standard python
// object header and then our sole addition, a pointer to
// a Tcl_Obj.  we dig into tclobj using the tcl C api in our
// methods and functions that implement the type.
typedef struct {
    PyObject_HEAD;
    Tcl_Obj *tclobj;
} PyTclObj;

typedef struct {
    PyObject_HEAD;
    int started;
    int done;
    PyObject *to;
    Tcl_Obj *dictObj;
    Tcl_DictSearch search;
} PyTohil_TD_IterObj;

PyTypeObject PyTclObjType;

PyObject *tohil_python_return(Tcl_Interp *, int tcl_result, PyObject *toType, Tcl_Obj *resultObj);

extern Tcl_Interp *tcl_interp;

extern PyObject *pTohilHandleException;
extern PyObject *pTohilTclErrorClass;
extern PyObject *pyTclObjIterator;

//
// turn a tcl list into a python list
//
PyObject *tclListObjToPyListObject(Tcl_Interp *interp, Tcl_Obj *inputObj);

//
// turn a tcl list into a python set
//
PyObject *tclListObjToPySetObject(Tcl_Interp *interp, Tcl_Obj *inputObj);

PyObject *tclListObjToPyTupleObject(Tcl_Interp *interp, Tcl_Obj *inputObj);

PyObject *tclListObjToPyDictObject(Tcl_Interp *interp, Tcl_Obj *inputObj);

int tohil_UTF8toTcl(char *src, int srclen, char **res, int *reslen);

int tohil_TclToUTF8(char *src, int srclen, char **res, int *reslen);

PyObject *tclObjToPy(Tcl_Obj *tObj);

Tcl_Obj *_pyObjToTcl(Tcl_Interp *interp, PyObject *pObj);

Tcl_Obj *pyObjToTcl(Tcl_Interp *interp, PyObject *pObj);

int PyReturnTclError(Tcl_Interp *interp, char *string);

int PyReturnException(Tcl_Interp *interp, char *description);

int TohilCall_Cmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

int TohilImport_Cmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

int TohilEval_Cmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

int TohilExec_Cmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

int TohilInteract_Cmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

int PyTclObj_Check(PyObject *pyObj);

PyObject *PyTclObj_FromTclObj(Tcl_Obj *obj);

PyObject *PyTclObj_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

void PyTclObj_dealloc(PyTclObj *self);

int PyTclObj_init(PyTclObj *self, PyObject *args, PyObject *kwds);

PyObject *PyTclObj_str(PyTclObj *self);

PyObject *PyTclObj_repr(PyTclObj *self);

PyObject *PyTclObj_richcompare(PyTclObj *self, PyObject *other, int op);

PyObject *PyTclObj_reset(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_int(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_float(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_bool(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_string(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_list(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_set(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_tuple(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_dict(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_tclobj(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_as_byte_array(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_incr(PyTclObj *self, PyObject *args, PyObject *kwargs);

Tcl_Obj *PyTclObj_td_locate(PyTclObj *self, PyObject *keys);

PyObject *PyTclObj_td_get(PyTclObj *self, PyObject *args, PyObject *kwargs);

PyObject *PyTclObj_td_exists(PyTclObj *self, PyObject *args, PyObject *kwargs);

void pyListToTclObjv(PyListObject *pList, int *intPtr, Tcl_Obj ***objvPtr);

void pyListToObjv_teardown(int objc, Tcl_Obj **objv);

PyObject *PyTclObj_td_size(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_td_remove(PyTclObj *self, PyObject *args, PyObject *kwargs);

PyObject *PyTclObj_td_set(PyTclObj *self, PyObject *args, PyObject *kwargs);

PyObject *PyTclObj_llength(PyTclObj *self, PyObject *pyobj);

PyObject *PyTclObj_getvar(PyTclObj *self, PyObject *var);

PyObject *PyTclObj_setvar(PyTclObj *self, PyObject *var);

PyObject *PyTclObj_set(PyTclObj *self, PyObject *pyObject);

PyObject *PyTclObj_lindex(PyTclObj *self, PyObject *args, PyObject *kwargs);

PyObject *PyTclObj_lappend(PyTclObj *self, PyObject *pObject);

PyObject *PyTclObj_lappend_list(PyTclObj *self, PyObject *pObject);

PyObject *PyTclObj_refcount(PyTclObj *self, PyObject *dummy);

PyObject *PyTclObj_type(PyTclObj *self, PyObject *dummy);

PyObject *PyTclObjIter(PyObject *self);

PyObject *PyTclObj_subscript(PyTclObj *, PyObject *);

PyObject *PyTclObj_slice(PyTclObj *self, Py_ssize_t ilow, Py_ssize_t ihigh);

PyObject *PyTclObj_item(PyTclObj *self, Py_ssize_t i);

int PyTclObj_ass_item(PyTclObj *self, Py_ssize_t i, PyObject *v);

Py_ssize_t PyTclObj_length(PyTclObj *self, Py_ssize_t i);

PyObject *PyTclObj_subscript(PyTclObj *self, PyObject *item);

PyObject *PyTohil_TD_iter(PyTohil_TD_IterObj *self);

PyObject *PyTohil_TD_iternext(PyTohil_TD_IterObj *self);

PyObject *PyTohil_TD_td_iter(PyTclObj *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_python_return(Tcl_Interp *interp, int tcl_result, PyObject *toType, Tcl_Obj *resultObj);

PyObject *tohil_eval(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_expr(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_convert(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_getvar(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_exists(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_setvar(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_incr(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_unset(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_subst(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *tohil_call(PyObject *self, PyObject *args, PyObject *kwargs);

int Tohil_Init(Tcl_Interp *interp);

// vim: set ts=4 sw=4 sts=4 et :
