//
//
// python tcl object "tclobj"
//
//

#include "tohil.h"

//
// return true if python object is a tclobj type
//
int
PyTclObj_Check(PyObject *pyObj)
{
    return PyObject_TypeCheck(pyObj, &PyTclObjType);
}

//
// create a new python tclobj object from a tclobj
//
PyObject *
PyTclObj_FromTclObj(Tcl_Obj *obj)
{
    PyTclObj *self = (PyTclObj *)PyTclObjType.tp_alloc(&PyTclObjType, 0);
    if (self != NULL) {
        self->tclobj = obj;
        Tcl_IncrRefCount(obj);
    }
    return (PyObject *)self;
}

//
// create a new python tclobj object
//
// currently just creates an empty object but could use args
// and keywords to do other interesting stuff?
//
PyObject *
PyTclObj_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *pSource = NULL;
    static char *kwlist[] = {"from", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", kwlist, &pSource))
        return NULL;

    PyTclObj *self = (PyTclObj *)type->tp_alloc(type, 0);
    if (self != NULL) {
        if (pSource == NULL) {
            self->tclobj = Tcl_NewObj();
        } else {
            self->tclobj = pyObjToTcl(tcl_interp, pSource);
        }
        Tcl_IncrRefCount(self->tclobj);
    }
    return (PyObject *)self;
}

void
PyTclObj_dealloc(PyTclObj *self)
{
    Tcl_DecrRefCount(self->tclobj);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

int
PyTclObj_init(PyTclObj *self, PyObject *args, PyObject *kwds)
{
    return 0;
}

PyObject *
PyTclObj_str(PyTclObj *self)
{
    int tclStringSize;
    char *tclString = Tcl_GetStringFromObj(self->tclobj, &tclStringSize);

    int utf8len;
    char *utf8string;
    if (tohil_TclToUTF8(tclString, tclStringSize, &utf8string, &utf8len) != TCL_OK) {
        PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }
    PyObject *pObj = Py_BuildValue("s#", utf8string, utf8len);
    ckfree(utf8string);
    return pObj;
}

PyObject *
PyTclObj_repr(PyTclObj *self)
{
    Tcl_DString ds;
    char *utf8string = tohil_TclObjToUTF8(self->tclobj, &ds);

    PyObject *stringRep = PyUnicode_FromFormat("%s", utf8string);
    Tcl_DStringFree(&ds);
    PyObject *repr = PyUnicode_FromFormat("<%s: %R>", Py_TYPE(self)->tp_name, stringRep);
    Py_DECREF(stringRep);
    return repr;
}

PyObject *
PyTclObj_richcompare(PyTclObj *self, PyObject *other, int op)
{

    // NB ugh other isn't necessarily a PyTclObj

    // if you want equal and they point to the exact same object,
    // we are donezo
    if (op == Py_EQ && PyTclObj_Check(other) && self->tclobj == ((PyTclObj *)other)->tclobj) {
        Py_INCREF(Py_True);
        return Py_True;
    }

    char *selfString = Tcl_GetString(self->tclobj);

    char *otherString = NULL;
    Tcl_Obj *otherObj = NULL;
    if (PyTclObj_Check(other)) {
        otherString = Tcl_GetString(((PyTclObj *)other)->tclobj);
    } else {
        otherObj = pyObjToTcl(tcl_interp, other);
        otherString = Tcl_GetString(otherObj);
    }

    int cmp = strcmp(selfString, otherString);
    int res = 0;

    switch (op) {
    case Py_LT:
        res = (cmp < 0);
        break;

    case Py_LE:
        res = (cmp <= 0);
        break;

    case Py_EQ:
        res = (cmp == 0);
        break;

    case Py_NE:
        res = (cmp != 0);
        break;

    case Py_GT:
        res = (cmp > 0);
        break;

    case Py_GE:
        res = (cmp >= 0);
        break;

    default:
        assert(0 == 1);
    }
    if (otherObj != NULL) {
        Tcl_DecrRefCount(otherObj);
    }
    PyObject *p = (res ? Py_True : Py_False);
    Py_INCREF(p);
    return p;
}

//
// tclobj.reset()
//
PyObject *
PyTclObj_reset(PyTclObj *self, PyObject *pyobj)
{
    Tcl_DecrRefCount(self->tclobj);
    self->tclobj = Tcl_NewObj();
    Tcl_IncrRefCount(self->tclobj);
    Py_RETURN_NONE;
}

//
// tclobj.as_int()
//
PyObject *
PyTclObj_as_int(PyTclObj *self, PyObject *pyobj)
{
    long longValue;

    if (Tcl_GetLongFromObj(tcl_interp, self->tclobj, &longValue) == TCL_OK) {
        return PyLong_FromLong(longValue);
    }
    PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
    return NULL;
}

//
// tclobj.as_float()
//
PyObject *
PyTclObj_as_float(PyTclObj *self, PyObject *pyobj)
{
    double doubleValue;

    if (Tcl_GetDoubleFromObj(tcl_interp, self->tclobj, &doubleValue) == TCL_OK) {
        return PyFloat_FromDouble(doubleValue);
    }
    PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
    return NULL;
}

//
// tclobj.as_bool()
//
PyObject *
PyTclObj_as_bool(PyTclObj *self, PyObject *pyobj)
{
    int intValue;
    if (Tcl_GetBooleanFromObj(tcl_interp, self->tclobj, &intValue) == TCL_OK) {
        PyObject *p = (intValue ? Py_True : Py_False);
        Py_INCREF(p);
        return p;
    }
    PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
    return NULL;
}

//
// tclobj.as_string()
//
PyObject *
PyTclObj_as_string(PyTclObj *self, PyObject *pyobj)
{
    int tclStringSize;
    char *tclString = Tcl_GetStringFromObj(self->tclobj, &tclStringSize);

    int utf8len;
    char *utf8string;
    if (tohil_TclToUTF8(tclString, tclStringSize, &utf8string, &utf8len) != TCL_OK) {
        PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }
    PyObject *pObj = Py_BuildValue("s#", utf8string, utf8len);
    ckfree(utf8string);
    return pObj;
}

//
// tclobj.as_list()
//
PyObject *
PyTclObj_as_list(PyTclObj *self, PyObject *pyobj)
{
    return tclListObjToPyListObject(tcl_interp, self->tclobj);
}

//
// tclobj.as_set()
//
PyObject *
PyTclObj_as_set(PyTclObj *self, PyObject *pyobj)
{
    return tclListObjToPySetObject(tcl_interp, self->tclobj);
}

//
// tclobj.as_tuple()
//
PyObject *
PyTclObj_as_tuple(PyTclObj *self, PyObject *pyobj)
{
    return tclListObjToPyTupleObject(tcl_interp, self->tclobj);
}

//
// tclobj.as_dict()
//
PyObject *
PyTclObj_as_dict(PyTclObj *self, PyObject *pyobj)
{
    return tclListObjToPyDictObject(tcl_interp, self->tclobj);
}

//
// tclobj.as_tclobj()
//
PyObject *
PyTclObj_as_tclobj(PyTclObj *self, PyObject *pyobj)
{
    return PyTclObj_FromTclObj(self->tclobj);
}

//
// tclobj.as_byte_array()
//
PyObject *
PyTclObj_as_byte_array(PyTclObj *self, PyObject *pyobj)
{
    int size;
    unsigned char *byteArray = Tcl_GetByteArrayFromObj(self->tclobj, &size);
    return PyByteArray_FromStringAndSize((const char *)byteArray, size);
}

//
// tclobj.incr() - increment a python tclobj object
//
PyObject *
PyTclObj_incr(PyTclObj *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"incr", NULL};
    long longValue = 0;
    long increment = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|l", kwlist, &increment)) {
        return NULL;
    }

    if (Tcl_GetLongFromObj(tcl_interp, self->tclobj, &longValue) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }

    longValue += increment;

    if (Tcl_IsShared(self->tclobj)) {
        self->tclobj = Tcl_DuplicateObj(self->tclobj);
    }
    Tcl_SetLongObj(self->tclobj, longValue);
    return PyLong_FromLong(longValue);
}

//
// td - tcl dict stuff
//

//
// td_locate(key) - do a dict get on the tclobj object using
//   either a list of keys or a singleton key, and return
//   the target value object.
//
//   internal routine used by td_get and td_exists
//
Tcl_Obj *
PyTclObj_td_locate(PyTclObj *self, PyObject *keys)
{
    Tcl_Obj *keyObj = NULL;
    Tcl_Obj *valueObj = NULL;

    if (PyList_Check(keys)) {
        int i;
        Tcl_Obj *dictPtrObj = self->tclobj;
        Py_ssize_t nKeys = PyList_GET_SIZE(keys);

        for (i = 0; i < nKeys; i++) {
            PyObject *keyPyObj = PyList_GET_ITEM(keys, i);
            keyObj = pyObjToTcl(tcl_interp, keyPyObj);

            if (Tcl_DictObjGet(tcl_interp, dictPtrObj, keyObj, &valueObj) == TCL_ERROR) {
                Tcl_DecrRefCount(keyObj);
                PyErr_SetString(PyExc_TypeError, "tclobj contents cannot be converted into a td");
                return NULL;
            }
            if (valueObj == NULL) {
                Tcl_DecrRefCount(keyObj);
                return NULL;
            }
            dictPtrObj = valueObj;
            Tcl_DecrRefCount(keyObj);
        }

        // at this point if there's been no error valueObj has our guy
    } else {
        // it's a singleton
        Tcl_Obj *keyObj = pyObjToTcl(tcl_interp, keys);

        if (Tcl_DictObjGet(NULL, self->tclobj, keyObj, &valueObj) == TCL_ERROR) {
            Tcl_DecrRefCount(keyObj);
            PyErr_SetString(PyExc_TypeError, "tclobj contents cannot be converted into a td");
            return NULL;
        }
        Tcl_DecrRefCount(keyObj);
    }

    return valueObj;
}

//
// td_get(key) - do a dict get on the tcl object
//
PyObject *
PyTclObj_td_get(PyTclObj *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"key", "to", "default", NULL};
    PyObject *keys = NULL;
    PyObject *to = NULL;
    PyObject *pDefault = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$OO", kwlist, &keys, &to, &pDefault)) {
        return NULL;
    }

    Tcl_Obj *valueObj = PyTclObj_td_locate(self, keys);
    if (valueObj == NULL) {
        if (pDefault != NULL) {
            if (to == NULL) {
                // not there but they provided a default,
                // give them their default
                Py_INCREF(pDefault);
                return pDefault;
            } else {
                valueObj = pyObjToTcl(tcl_interp, pDefault);
            }
        } else {
            // not there, no default.  it's an error.
            // this is clean and the way python does it.
            PyErr_SetObject(PyExc_KeyError, keys);
            return NULL;
        }
    }

    return tohil_python_return(tcl_interp, TCL_OK, to, valueObj);
}

//
// td_exists(key) - do a dict get on a key or list of keys
//   against our tclobj object and return true if the key's there
//   and false if it isn't
//
PyObject *
PyTclObj_td_exists(PyTclObj *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"key", NULL};
    PyObject *keys = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &keys)) {
        return NULL;
    }

    Tcl_Obj *valueObj = PyTclObj_td_locate(self, keys);
    if (valueObj == NULL) {
        // see if an error occurred -- there is a difference between
        // not finding something (valueObj == NULL) and not finding
        // something (same) and there having been an error
        if (PyErr_Occurred() == NULL) {
            // an error didn't occur
            Py_RETURN_FALSE;
        }
        return NULL;
    }

    Py_RETURN_TRUE;
}

//
// convert a python list into a tcl c-level objv and objc
//
// pyListToTclObjv(pList, &objc, &objv);
//
// you must call pyListToObjv_teardown when done or you'll
// leak memory
//
void
pyListToTclObjv(PyListObject *pList, int *intPtr, Tcl_Obj ***objvPtr)
{
    int i;

    assert(PyList_Check(pList));
    Py_ssize_t objc = PyList_GET_SIZE(pList);
    // build up a tcl objv of the list elements
    Tcl_Obj **objv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *) * objc);
    for (i = 0; i < objc; i++) {
        objv[i] = pyObjToTcl(tcl_interp, PyList_GET_ITEM(pList, i));
        Tcl_IncrRefCount(objv[i]);
    }
    *objvPtr = objv;
    *intPtr = objc;
}

//
// teardown an objv created by pyListToObjv
//
void
pyListToObjv_teardown(int objc, Tcl_Obj **objv)
{
    int i;

    // tear down the objv of the keys we created
    for (i = 0; i < objc; i++) {
        Tcl_DecrRefCount(objv[i]);
    }
    ckfree(objv);
}

//
// td_size() - return the dict size of a python tclobj's tcl object
//   exception thrown if tcl object isn't a proper tcl dict
//
PyObject *
PyTclObj_td_size(PyTclObj *self, PyObject *pyobj)
{
    int length;
    if (Tcl_DictObjSize(tcl_interp, self->tclobj, &length) == TCL_OK) {
        return PyLong_FromLong(length);
    }
    PyErr_SetString(PyExc_TypeError, "tclobj contents cannot be converted into a td");
    return NULL;
}

//
// td_remove(key) - if key is a python list, do a dict remove keylist on the tcl object,
//   where the arg is a python list of a hierarchy of names to remove.
//
//   if key is not a python list, does a dict remove on the tcl object for that key
//
PyObject *
PyTclObj_td_remove(PyTclObj *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"key", NULL};
    PyObject *keys = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &keys)) {
        return NULL;
    }

    if (PyList_Check(keys)) {
        int objc = 0;
        Tcl_Obj **objv = NULL;

        // build up a tcl objv of the keys
        pyListToTclObjv((PyListObject *)keys, &objc, &objv);

        // we are about to try to modify the object, so if it's shared we need to copy
        if (Tcl_IsShared(self->tclobj)) {
            self->tclobj = Tcl_DuplicateObj(self->tclobj);
        }

        int status = (Tcl_DictObjRemoveKeyList(tcl_interp, self->tclobj, objc, objv));

        // tear down the objv of the keys we created
        pyListToObjv_teardown(objc, objv);

        if (status == TCL_ERROR) {
            PyErr_SetString(PyExc_KeyError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            return NULL;
        }
    } else {
        Tcl_Obj *keyObj = _pyObjToTcl(tcl_interp, keys);

        if (keyObj == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "unable to fashion argument into a string to be used as a dictionary key");
            return NULL;
        }

        // we are about to try to modify the object, so if it's shared we need to copy
        if (Tcl_IsShared(self->tclobj)) {
            self->tclobj = Tcl_DuplicateObj(self->tclobj);
        }

        if (Tcl_DictObjRemove(NULL, self->tclobj, keyObj) == TCL_ERROR) {
            Tcl_DecrRefCount(keyObj);
            PyErr_SetString(PyExc_TypeError, "tclobj contents cannot be converted into a td");
            return NULL;
        }
        Tcl_DecrRefCount(keyObj);
    }

    Py_RETURN_NONE;
}

//
// tclobj.td_set(key, value) - do a dict set on the tcl object
//   if key is a python list, td_set operates on a nested tree
//   of dictionaries
//
PyObject *
PyTclObj_td_set(PyTclObj *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"key", "value", NULL};
    PyObject *keys = NULL;
    PyObject *pValue = NULL;

    // remember, "O" sets our pointer to the object without incrementing its reference count
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|$", kwlist, &keys, &pValue)) {
        return NULL;
    }

    Tcl_Obj *valueObj = pyObjToTcl(tcl_interp, pValue);
    if (valueObj == NULL) {
        return NULL;
    }

    // we are about to try to modify the object, so if it's shared we need to copy
    if (Tcl_IsShared(self->tclobj)) {
        self->tclobj = Tcl_DuplicateObj(self->tclobj);
    }

    if (PyList_Check(keys)) {
        int objc;
        Tcl_Obj **objv;

        // build up a tcl objv of the keys
        pyListToTclObjv((PyListObject *)keys, &objc, &objv);

        int status = (Tcl_DictObjPutKeyList(tcl_interp, self->tclobj, objc, objv, valueObj));

        // tear down the objv of the keys we created
        pyListToObjv_teardown(objc, objv);

        if (status == TCL_ERROR) {
            Tcl_DecrRefCount(valueObj);
            PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            return NULL;
        }
    } else {
        Tcl_Obj *keyObj = _pyObjToTcl(tcl_interp, keys);

        if (keyObj == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "unable to fashion argument into a string to be used as a dictionary key");
            return NULL;
        }

        if (Tcl_DictObjPut(tcl_interp, self->tclobj, keyObj, valueObj) == TCL_ERROR) {
            Tcl_DecrRefCount(keyObj);
            Tcl_DecrRefCount(valueObj);
            PyErr_SetString(PyExc_TypeError, "tclobj contents cannot be converted into a td");
            return NULL;
        }
        // something about this is a crash
        // Tcl_DecrRefCount(keyObj);
        // Tcl_DecrRefCount(valueObj);
    }

    Py_RETURN_NONE;
}

//
// llength - return the length of a python tclobj's tcl object
//   as a list.  exception thrown if tcl object isn't a list
//
PyObject *
PyTclObj_llength(PyTclObj *self, PyObject *pyobj)
{
    int length;
    if (Tcl_ListObjLength(tcl_interp, self->tclobj, &length) == TCL_OK) {
        return PyLong_FromLong(length);
    }
    PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
    return NULL;
}

//
// getvar - set python tclobj to contain the value of a tcl var
//
PyObject *
PyTclObj_getvar(PyTclObj *self, PyObject *var)
{
    char *varString = (char *)PyUnicode_1BYTE_DATA(var);
    Tcl_Obj *newObj = Tcl_GetVar2Ex(tcl_interp, varString, NULL, (TCL_LEAVE_ERR_MSG));
    if (newObj == NULL) {
        PyErr_SetString(PyExc_NameError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }
    Tcl_DecrRefCount(self->tclobj);
    self->tclobj = newObj;
    Tcl_IncrRefCount(self->tclobj);
    Py_RETURN_NONE;
}

//
// setvar - set set tcl var to point to the tcl object of a python tclobj
//
PyObject *
PyTclObj_setvar(PyTclObj *self, PyObject *var)
{
    char *varString = (char *)PyUnicode_1BYTE_DATA(var);
    // setvar handles incrementing the reference count
    if (Tcl_SetVar2Ex(tcl_interp, varString, NULL, self->tclobj, (TCL_LEAVE_ERR_MSG)) == NULL) {
        PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }
    Py_RETURN_NONE;
}

// set - tclobj type set method can set an object to a lot
// of possible python stuff -- NB there must be a better way
PyObject *
PyTclObj_set(PyTclObj *self, PyObject *pyObject)
{
    Tcl_Obj *newObj = pyObjToTcl(tcl_interp, pyObject);
    if (newObj == NULL) {
        return NULL;
    }
    Tcl_DecrRefCount(self->tclobj);
    self->tclobj = newObj;
    Tcl_IncrRefCount(self->tclobj);

    Py_RETURN_NONE;
}

PyObject *
PyTclObj_lindex(PyTclObj *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"index", "to", NULL};
    PyObject *to = NULL;
    int index = 0;
    int length = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|$O", kwlist, &index, &to))
        return NULL;

    if (Tcl_ListObjLength(tcl_interp, self->tclobj, &length) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }

    if (index < 0 || index >= length) {
        PyErr_SetString(PyExc_IndexError, "list index out of range");
        return NULL;
    }

    Tcl_Obj *resultObj = NULL;
    if (Tcl_ListObjIndex(tcl_interp, self->tclobj, index, &resultObj) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }

    return tohil_python_return(tcl_interp, TCL_OK, to, resultObj);
}

//
// lappend something to the tclobj
//
PyObject *
PyTclObj_lappend(PyTclObj *self, PyObject *pObject)
{
    Tcl_Obj *newObj = pyObjToTcl(tcl_interp, pObject);
    if (newObj == NULL) {
        return NULL;
    }

    // we are about to modify the object so if it's shared we need to copy
    if (Tcl_IsShared(self->tclobj)) {
        self->tclobj = Tcl_DuplicateObj(self->tclobj);
    }

    if (Tcl_ListObjAppendElement(tcl_interp, self->tclobj, newObj) == TCL_ERROR) {
        PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        Tcl_DecrRefCount(newObj);
        return NULL;
    }
    // it worked
    Py_RETURN_NONE;
}

//
// lappend_list, lappend a list of something to the tclobj
//
// if append object is a tclobj and the tclobj contents can be
// treated as a list, it will be appended as the list
//
// if append object is a python list then its contents will be
// appended to the list while being converted to tcl objects
//
PyObject *
PyTclObj_lappend_list(PyTclObj *self, PyObject *pObject)
{

    // if passed python object is a tclobj, use tcl list C stuff
    // to append the list to the list
    if (PyTclObj_Check(pObject)) {
        Tcl_Obj *appendListObj = ((PyTclObj *)pObject)->tclobj;

        if (Tcl_IsShared(self->tclobj)) {
            self->tclobj = Tcl_DuplicateObj(self->tclobj);
        }

        if (Tcl_ListObjAppendList(tcl_interp, self->tclobj, appendListObj) == TCL_ERROR) {
            PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            Tcl_DecrRefCount(appendListObj);
            return NULL;
        }
        // if passed python object is a python list, use that and our
        // python-to-tcl stuff to make a tcl list of it, and append
        // that list to the tclobj's object, which is a list or an error
        // is forthcoming
    } else if (PyList_Check(pObject)) {
        int objc;
        Tcl_Obj **objv = NULL;

        pyListToTclObjv((PyListObject *)pObject, &objc, &objv);
        Tcl_Obj *appendListObj = Tcl_NewListObj(objc, objv);
        pyListToObjv_teardown(objc, objv);

        if (Tcl_IsShared(self->tclobj)) {
            self->tclobj = Tcl_DuplicateObj(self->tclobj);
        }

        if (Tcl_ListObjAppendList(tcl_interp, self->tclobj, appendListObj) == TCL_ERROR) {
            PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            Tcl_DecrRefCount(appendListObj);
            return NULL;
        }
    } else {
        PyErr_Format(PyExc_TypeError, "lappend_list argument must be a tclobj or list, not %.200s", Py_TYPE(pObject)->tp_name);
        return NULL;
    }

    // it worked
    Py_RETURN_NONE;
}

PyObject *
PyTclObj_refcount(PyTclObj *self, PyObject *dummy)
{
    return PyLong_FromLong(self->tclobj->refCount);
}

PyObject *
PyTclObj_type(PyTclObj *self, PyObject *dummy)
{
    const Tcl_ObjType *typePtr = self->tclobj->typePtr;
    if (typePtr == NULL) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("s", self->tclobj->typePtr->name);
}

PyObject *
PyTclObjIter(PyObject *self)
{
    assert(pyTclObjIterator != NULL);
    PyObject *pyRet = PyObject_CallFunction(pyTclObjIterator, "O", self);
    return pyRet;
}

PyObject *PyTclObj_subscript(PyTclObj *, PyObject *);

// slice stuff significantly cribbed from cpython source for listobjects...

PyObject *
PyTclObj_slice(PyTclObj *self, Py_ssize_t ilow, Py_ssize_t ihigh)
{
    PyListObject *np;
    int listObjc;
    Tcl_Obj **listObjv;
    Tcl_Obj **src;
    Py_ssize_t i, len;
    len = ihigh - ilow;

    int size = 0;

    // get the list size and crack the list into a list objc and objv.
    // any failure here probably means the object can't be represented as a list.
    if (Tcl_ListObjLength(tcl_interp, self->tclobj, &size) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }

    if (Tcl_ListObjGetElements(tcl_interp, self->tclobj, &listObjc, &listObjv) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }

    np = (PyListObject *)PyList_New(len);
    if (np == NULL) {
        return NULL;
    }

    // src is a pointer to an array of pointers of obj,
    // adjust it to the starting object and we'll walk it forward
    src = &listObjv[ilow];
    for (i = 0; i < len; i++) {
        // create a new tclobj object and store
        // it into the python list we are making
        PyObject *v = PyTclObj_FromTclObj(src[i]);
        PyList_SET_ITEM(np, i, v);
    }
    return (PyObject *)np;
}

PyObject *
PyTclObj_item(PyTclObj *self, Py_ssize_t i)
{
    int size = 0;

    if (Tcl_ListObjLength(tcl_interp, self->tclobj, &size) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }

    if (i < 0 || i >= size) {
        PyErr_SetString(PyExc_IndexError, "list index out of range");
        return NULL;
    }

    int listObjc;
    Tcl_Obj **listObjv;

    if (Tcl_ListObjGetElements(tcl_interp, self->tclobj, &listObjc, &listObjv) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }

    PyObject *ret = PyTclObj_FromTclObj(listObjv[i]);
    Py_INCREF(ret);
    return ret;
}

int
PyTclObj_ass_item(PyTclObj *self, Py_ssize_t i, PyObject *v)
{
    int size = 0;

    if (Tcl_ListObjLength(tcl_interp, self->tclobj, &size) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return -1;
    }

    if (i < 0 || i >= size) {
        PyErr_SetString(PyExc_IndexError, "list assignment index out of range");
        return -1;
    }

    if (v == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "slice assignment not implemented");
        return -1;
        // return list_ass_slice(self, i, i+1, v);
    }

    Tcl_Obj *obj = pyObjToTcl(tcl_interp, v);
    if (obj == NULL) {
        return -1;
    }

    // we are about to modify the object so if it's shared we need to copy
    if (Tcl_IsShared(self->tclobj)) {
        self->tclobj = Tcl_DuplicateObj(self->tclobj);
    }

    if (Tcl_ListObjReplace(tcl_interp, self->tclobj, i, 1, 1, &obj) == TCL_ERROR) {
        PyErr_SetString(PyExc_IndexError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return -1;
    }
    return 0;
}

Py_ssize_t
PyTclObj_length(PyTclObj *self, Py_ssize_t i)
{
    int size = 0;

    if (Tcl_ListObjLength(tcl_interp, self->tclobj, &size) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return 0;
    }

    return size;
}

PyObject *
PyTclObj_subscript(PyTclObj *self, PyObject *item)
{
    int size = 0;

    if (Tcl_ListObjLength(tcl_interp, self->tclobj, &size) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }

    if (PyIndex_Check(item)) {
        Py_ssize_t i;
        i = PyNumber_AsSsize_t(item, PyExc_IndexError);
        if (i == -1 && PyErr_Occurred())
            return NULL;
        if (i < 0)
            i += size;

        if (i < 0 || i >= size) {
            PyErr_SetString(PyExc_IndexError, "list index out of range");
            return NULL;
        }

        Tcl_Obj *resultObj = NULL;
        if (Tcl_ListObjIndex(tcl_interp, self->tclobj, i, &resultObj) == TCL_ERROR) {
            PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            return NULL;
        }
        return PyTclObj_FromTclObj(resultObj);
    } else if (PySlice_Check(item)) {
        Py_ssize_t start, stop, step, slicelength, i;
        size_t cur;
        PyObject *result;
        PyObject *it;

        if (PySlice_Unpack(item, &start, &stop, &step) < 0) {
            return NULL;
        }

        slicelength = PySlice_AdjustIndices(size, &start, &stop, step);

        if (slicelength <= 0) {
            return PyTclObj_FromTclObj(Tcl_NewObj());
        } else if (step == 1) {
            return PyTclObj_slice(self, start, stop);
        } else {
            result = PyList_New(slicelength);
            if (!result)
                return NULL;

            int listObjc;
            Tcl_Obj **listObjv;

            if (Tcl_ListObjGetElements(tcl_interp, self->tclobj, &listObjc, &listObjv) == TCL_ERROR) {
                PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
                return NULL;
            }

            for (cur = start, i = 0; i < slicelength; cur += (size_t)step, i++) {
                it = PyTclObj_FromTclObj(listObjv[cur]);
                PyList_SET_ITEM(result, i, it);
            }
            return result;
        }
    } else {
        PyErr_Format(PyExc_TypeError, "list indices must be integers or slices, not %.200s", Py_TYPE(item)->tp_name);
        return NULL;
    }
}

//
//
// end of python tcl object "tclobj"
//
//
