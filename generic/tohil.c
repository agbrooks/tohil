// tohil C interface library
//
// ...contains all the C code for both Python and TCL
//
// GO TOE HEELS
//
// There is also python support code in pysrc/tohil,
// and TCL support code in tclsrc
//
// https://github.com/flightaware/tohil
//

#include "tohil.h"

// TCL library begins here

// maintain a pointer to the tcl interp - we need it from our stuff python calls where
// we don't get passed an interpreter

Tcl_Interp *tcl_interp = NULL;

// maintain pointers to our exception handler and python function that
// we return as our iterator object
// NB this could be a problem if either of these functions get redefined
PyObject *pTohilHandleException = NULL;
PyObject *pTohilTclErrorClass = NULL;
PyObject *pyTclObjIterator = NULL;

//
// tohil_TclObjToUTF8 - convert a Tcl object (string in WTF-8) to real UTF-8
// for Python.
//
char *
tohil_TclObjToUTF8(Tcl_Obj *obj, Tcl_DString *ds)
{
    static Tcl_Encoding utf8encoding = NULL;
    if (!utf8encoding)
        utf8encoding = Tcl_GetEncoding(tcl_interp, "utf-8");
    int tclStringLen;
    char *tclString = Tcl_GetStringFromObj(obj, &tclStringLen);
    return Tcl_UtfToExternalDString(utf8encoding, tclString, tclStringLen, ds);
}

//
// tohil_UTF8ToTcl - convert a Python utf-8 string to a Tcl "WTF-8" string.
//
char *
tohil_UTF8ToTcl(char *utf8String, int utf8StringLen, Tcl_DString *ds)
{
    static Tcl_Encoding utf8encoding = NULL;
    if (!utf8encoding)
        utf8encoding = Tcl_GetEncoding(tcl_interp, "utf-8");
    // Accepts -1 for string length but try to avoid it.
    if (utf8StringLen == -1) {
        utf8StringLen = strlen(utf8String);
    }
    return Tcl_ExternalToUtfDString(utf8encoding, utf8String, utf8StringLen, ds);
}

//
// turn a tcl list into a python list
//
PyObject *
tclListObjToPyListObject(Tcl_Interp *interp, Tcl_Obj *inputObj)
{
    Tcl_Obj **list;
    int count;

    if (Tcl_ListObjGetElements(interp, inputObj, &count, &list) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(interp)));
        return NULL;
    }

    PyObject *plist = PyList_New(count);

    for (int i = 0; i < count; i++) {
        Tcl_DString ds;
        PyList_SET_ITEM(plist, i, Py_BuildValue("s", tohil_TclObjToUTF8(list[i], &ds)));
        Tcl_DStringFree(&ds);
    }

    return plist;
}

//
// turn a tcl list into a python set
//
PyObject *
tclListObjToPySetObject(Tcl_Interp *interp, Tcl_Obj *inputObj)
{
    Tcl_Obj **list;
    int count;

    if (Tcl_ListObjGetElements(interp, inputObj, &count, &list) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(interp)));
        return NULL;
    }

    PyObject *pset = PySet_New(NULL);

    for (int i = 0; i < count; i++) {
        Tcl_DString ds;
        if (PySet_Add(pset, Py_BuildValue("s", tohil_TclObjToUTF8(list[i], &ds))) < 0) {
            return NULL;
        }
        Tcl_DStringFree(&ds);
    }

    return pset;
}

//
// turn a tcl list into a python tuple
//
PyObject *
tclListObjToPyTupleObject(Tcl_Interp *interp, Tcl_Obj *inputObj)
{
    Tcl_Obj **list;
    int count;

    if (Tcl_ListObjGetElements(interp, inputObj, &count, &list) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(interp)));
        return NULL;
    }

    PyObject *ptuple = PyTuple_New(count);

    for (int i = 0; i < count; i++) {
        Tcl_DString ds;
        PyTuple_SET_ITEM(ptuple, i, Py_BuildValue("s", tohil_TclObjToUTF8(list[i], &ds)));
        Tcl_DStringFree(&ds);
    }

    return ptuple;
}

//
// turn a tcl list of key-value pairs into a python dict
//
PyObject *
tclListObjToPyDictObject(Tcl_Interp *interp, Tcl_Obj *inputObj)
{
    Tcl_Obj **list;
    int count;

    if (Tcl_ListObjGetElements(interp, inputObj, &count, &list) == TCL_ERROR) {
        PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(interp)));
        return NULL;
    }

    if (count % 2 != 0) {
        // list doesn't have an even number of elements
        PyErr_SetString(PyExc_RuntimeError, "list doesn't have an even number of elements");
        return NULL;
    }

    PyObject *pdict = PyDict_New();

    for (int i = 0; i < count; i += 2) {
        Tcl_DString kds, vds;
        char *key = tohil_TclObjToUTF8(list[i], &kds);
        char *val = tohil_TclObjToUTF8(list[i + 1], &vds);
        PyDict_SetItem(pdict, Py_BuildValue("s", key), Py_BuildValue("s", val));
        Tcl_DStringFree(&kds);
        Tcl_DStringFree(&vds);
    }

    return pdict;
}

//
// Convert a Python UTF8 string to a Tcl "WTF-8" string.
//
int
tohil_UTF8toTcl(char *src, int srclen, char **res, int *reslen)
{
    static Tcl_Encoding utf8encoding = NULL;
    int buflen = srclen;
    char *buf = ckalloc(buflen + 4);
    int written;
    int result;
    if (!utf8encoding)
        utf8encoding = Tcl_GetEncoding(tcl_interp, "utf-8");

    while (1) {
        result = Tcl_ExternalToUtf(tcl_interp, utf8encoding, src, srclen, 0, NULL, buf, buflen + 4, NULL, &written, NULL);
        if (result == TCL_OK) {
            *res = buf;
            *reslen = written;
            return TCL_OK;
        }
        if (result == TCL_CONVERT_NOSPACE) {
            ckfree(buf);
            buflen *= 2;
            buf = ckalloc(buflen + 4);
            continue;
        }
        ckfree(buf);
        return result;
    }
}

//
// Convert a Tcl "WTF-8" string to a Python UTF8 string
//
int
tohil_TclToUTF8(char *src, int srclen, char **res, int *reslen)
{
    static Tcl_Encoding utf8encoding = NULL;
    int buflen = srclen;
    char *buf = ckalloc(buflen + 4);
    int written;
    if (!utf8encoding)
        utf8encoding = Tcl_GetEncoding(tcl_interp, "utf-8");
    int result;

    while (1) {
        result = Tcl_UtfToExternal(tcl_interp, utf8encoding, src, srclen, 0, NULL, buf, buflen + 4, NULL, &written, NULL);
        if (result == TCL_OK) {
            *res = buf;
            *reslen = written;
            return TCL_OK;
        }
        if (result == TCL_CONVERT_NOSPACE) {
            ckfree(buf);
            buflen *= 2;
            buf = ckalloc(buflen + 4);
            continue;
        }
        ckfree(buf);
        return result;
    }
}

//
// turn a tcl object into a python object by trying to convert it as a boolean,
// then a long, then a double and finally a string
//
// NB not currently used, and could be better by peeking first at the tcl
// object's internal rep.  i have concerns about using this because the python
// dev may be surprised to get an int or float back instead of a string.
// not sure but not ready to get rid of this; it still seems promising
//
PyObject *
tclObjToPy(Tcl_Obj *tObj)
{
    int intValue;
    long longValue;
    double doubleValue;

    if (Tcl_GetBooleanFromObj(NULL, tObj, &intValue) == TCL_OK) {
        PyObject *p = (intValue ? Py_True : Py_False);
        Py_INCREF(p);
        return p;
    }

    if (Tcl_GetLongFromObj(NULL, tObj, &longValue) == TCL_OK) {
        return PyLong_FromLong(longValue);
    }

    if (Tcl_GetDoubleFromObj(NULL, tObj, &doubleValue) == TCL_OK) {
        return PyFloat_FromDouble(doubleValue);
    }

    int tclStringSize;
    char *tclString;
    tclString = Tcl_GetStringFromObj(tObj, &tclStringSize);

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
// convert a python object to a tcl object - amazing code by aidan
//
Tcl_Obj *
_pyObjToTcl(Tcl_Interp *interp, PyObject *pObj)
{
    Tcl_Obj *tObj;
    PyObject *pBytesObj;
    PyObject *pStrObj;

    Py_ssize_t i, len;
    PyObject *pVal = NULL;
    Tcl_Obj *tVal;

    PyObject *pItems = NULL;
    PyObject *pItem = NULL;
    PyObject *pKey = NULL;
    Tcl_Obj *tKey;

    char *utf8string;
    int utf8len;

    /*
     * The ordering must always be more 'specific' types first. E.g. a
     * string also obeys the sequence protocol...but we probably want it
     * to be a string rather than a list. Suggested order below:
     * - None -> {}
     * - True -> 1, False -> 0
     * - tclobj -> tclobj
     * - bytes -> tcl byte string
     * - unicode -> tcl unicode string
     * - number protocol -> tcl number
     * - sequence protocol -> tcl list
     * - mapping protocol -> tcl dict
     * - other -> error (currently converts to string)
     *
     * Note that the sequence and mapping protocol are both determined by __getitem__,
     * the only difference is that dict subclasses are excluded from sequence.
     */

    if (pObj == Py_None) {
        tObj = Tcl_NewObj();
    } else if (pObj == Py_True || pObj == Py_False) {
        tObj = Tcl_NewBooleanObj(pObj == Py_True);
    } else if (PyTclObj_Check(pObj)) {
        PyTclObj *pyTclObj = (PyTclObj *)pObj;
        tObj = pyTclObj->tclobj;
    } else if (PyBytes_Check(pObj)) {
        tObj = Tcl_NewByteArrayObj((const unsigned char *)PyBytes_AS_STRING(pObj), PyBytes_GET_SIZE(pObj));
    } else if (PyUnicode_Check(pObj)) {
        pBytesObj = PyUnicode_AsUTF8String(pObj);
        if (pBytesObj == NULL)
            return NULL;
        if (tohil_UTF8toTcl(PyBytes_AS_STRING(pBytesObj), PyBytes_GET_SIZE(pBytesObj), &utf8string, &utf8len) != TCL_OK) {
            Py_DECREF(pBytesObj);
            return NULL;
        }
        tObj = Tcl_NewStringObj(utf8string, utf8len);
        ckfree(utf8string);
        Py_DECREF(pBytesObj);
    } else if (PyNumber_Check(pObj)) {
        /* We go via string to support arbitrary length numbers */
        if (PyLong_Check(pObj)) {
            pStrObj = PyNumber_ToBase(pObj, 10);
        } else {
            assert(PyComplex_Check(pObj) || PyFloat_Check(pObj));
            pStrObj = PyObject_Str(pObj);
        }
        if (pStrObj == NULL)
            return NULL;
        pBytesObj = PyUnicode_AsUTF8String(pStrObj);
        Py_DECREF(pStrObj);
        if (pBytesObj == NULL)
            return NULL;
        if (tohil_UTF8toTcl(PyBytes_AS_STRING(pBytesObj), PyBytes_GET_SIZE(pBytesObj), &utf8string, &utf8len) != TCL_OK) {
            Py_DECREF(pBytesObj);
            return NULL;
        }
        tObj = Tcl_NewStringObj(utf8string, utf8len);
        ckfree(utf8string);
        Py_DECREF(pBytesObj);
    } else if (PySequence_Check(pObj)) {
        tObj = Tcl_NewListObj(0, NULL);
        len = PySequence_Length(pObj);
        if (len == -1)
            return NULL;

        for (i = 0; i < len; i++) {
            pVal = PySequence_GetItem(pObj, i);
            if (pVal == NULL)
                return NULL;
            tVal = _pyObjToTcl(interp, pVal);
            Py_DECREF(pVal);
            if (tVal == NULL)
                return NULL;
            Tcl_ListObjAppendElement(interp, tObj, tVal);
        }
    } else if (PyMapping_Check(pObj)) {
        tObj = Tcl_NewDictObj();
        len = PyMapping_Length(pObj);
        if (len == -1)
            return NULL;
        pItems = PyMapping_Items(pObj);
        if (pItems == NULL)
            return NULL;
#define ONERR(VAR)         \
    if (VAR == NULL) {     \
        Py_DECREF(pItems); \
        return NULL;       \
    }
        for (i = 0; i < len; i++) {
            pItem = PySequence_GetItem(pItems, i);
            ONERR(pItem)
            pKey = PySequence_GetItem(pItem, 0);
            ONERR(pKey)
            pVal = PySequence_GetItem(pItem, 1);
            ONERR(pVal)
            tKey = _pyObjToTcl(interp, pKey);
            Py_DECREF(pKey);
            ONERR(tKey);
            tVal = _pyObjToTcl(interp, pVal);
            Py_DECREF(pVal);
            ONERR(tVal);
            Tcl_DictObjPut(interp, tObj, tKey, tVal);
        }
#undef ONERR
        Py_DECREF(pItems);
        /* Broke out of loop because of error */
        if (i != len) {
            Py_XDECREF(pItem);
            return NULL;
        }
    } else {
        /* Get python string representation of other objects */
        pStrObj = PyObject_Str(pObj);
        if (pStrObj == NULL)
            return NULL;
        pBytesObj = PyUnicode_AsUTF8String(pStrObj);
        Py_DECREF(pStrObj);
        if (pBytesObj == NULL)
            return NULL;
        if (tohil_UTF8toTcl(PyBytes_AS_STRING(pBytesObj), PyBytes_GET_SIZE(pBytesObj), &utf8string, &utf8len) != TCL_OK) {
            Py_DECREF(pBytesObj);
            return NULL;
        }
        tObj = Tcl_NewStringObj(utf8string, utf8len);
        ckfree(utf8string);
        Py_DECREF(pBytesObj);
    }

    return tObj;
}

Tcl_Obj *
pyObjToTcl(Tcl_Interp *interp, PyObject *pObj)
{
    Tcl_Obj *ret = _pyObjToTcl(interp, pObj);
    // NB tcl is not prepared to accept null pointers to set interpreter results
    // and probably even variables and stuff yet pyObjToTcl can return them.
    // here we look to detect if it does.  later we might choose to cause an
    // error return or some kind, or, if necessary, do checks
    // everywhere in the code that currently assumes pyObjToTcl can't fail
    assert(ret != NULL);
    return ret;
}

//
// PyReturnTclError - return a tcl error to the tcl interpreter
//   with the specified string as an error message
//
int
PyReturnTclError(Tcl_Interp *interp, char *string)
{
    Tcl_SetObjResult(interp, Tcl_NewStringObj(string, -1));
    return TCL_ERROR;
}

//
// PyReturnException - return a python exception to tcl as a tcl error
//
int
PyReturnException(Tcl_Interp *interp, char *description)
{
    // Shouldn't call this function unless Python has excepted
    if (PyErr_Occurred() == NULL) {
        return PyReturnTclError(interp, "bug in tohil - PyReturnException called without a python error having occurred");
    }

    // break out the exception
    PyObject *pType = NULL, *pVal = NULL, *pTrace = NULL;

    PyErr_Fetch(&pType, &pVal, &pTrace); /* Clears exception */
    PyErr_NormalizeException(&pType, &pVal, &pTrace);

    // set tcl interpreter result
    Tcl_SetObjResult(interp, pyObjToTcl(interp, pVal));

    // invoke python tohil.handle_exception(type, val, tracebackObject)
    // it returns a tuple consisting of the error code and error info (traceback)
    PyObject *pExceptionResult = PyObject_CallFunctionObjArgs(pTohilHandleException, pType, pVal, pTrace, NULL);

    // call tohil python exception handler function
    // return to me a tuple containing the error string, error code, and traceback
    if (pExceptionResult == NULL) {
        // NB debug break out the exception
        PyObject *pType = NULL, *pVal = NULL, *pTrace = NULL;
        PyErr_Fetch(&pType, &pVal, &pTrace); /* Clears exception */
        PyErr_NormalizeException(&pType, &pVal, &pTrace);
        PyObject_Print(pType, stdout, 0);
        PyObject_Print(pVal, stdout, 0);
        return PyReturnTclError(interp, "some problem running the tohil python exception handler");
    }

    if (!PyTuple_Check(pExceptionResult) || PyTuple_GET_SIZE(pExceptionResult) != 2) {
        return PyReturnTclError(interp, "malfunction in tohil python exception handler, did not return tuple or tuple did not contain 2 elements");
    }

    Tcl_SetObjErrorCode(interp, pyObjToTcl(interp, PyTuple_GET_ITEM(pExceptionResult, 0)));
    Tcl_AppendObjToErrorInfo(interp, pyObjToTcl(interp, PyTuple_GET_ITEM(pExceptionResult, 1)));
    Py_DECREF(pExceptionResult);
    return TCL_ERROR;
}

//
// call python from tcl with very explicit arguments versus
//   slamming stuff through eval
//
int
TohilCall_Cmd(ClientData clientData, /* Not used. */
              Tcl_Interp *interp,    /* Current interpreter */
              int objc,              /* Number of arguments */
              Tcl_Obj *const objv[]  /* Argument strings */
)
{
    if (objc < 2) {
    wrongargs:
        Tcl_WrongNumArgs(interp, 1, objv, "?-kwlist list? func ?arg ...?");
        return TCL_ERROR;
    }

    PyObject *kwObj = NULL;
    Tcl_DString ds;
    const char *objandfn = tohil_TclObjToUTF8(objv[1], &ds);
    int objStart = 2;

    if (*objandfn == '-' && strcmp(objandfn, "-kwlist") == 0) {
        if (objc < 4)
            goto wrongargs;
        kwObj = tclListObjToPyDictObject(interp, objv[2]);
        Tcl_DStringFree(&ds);
        objandfn = tohil_TclObjToUTF8(objv[3], &ds);
        objStart = 4;
        if (kwObj == NULL) {
            return TCL_ERROR;
        }
    }

    /* Borrowed ref, do not decrement */
    PyObject *pMainModule = PyImport_AddModule("__main__");
    if (pMainModule == NULL) {
        Tcl_DStringFree(&ds);
        return PyReturnException(interp, "unable to add module __main__ to python interpreter");
    }

    /* So we don't have to special case the decref in the following loop */
    Py_INCREF(pMainModule);
    PyObject *pObjParent = NULL;
    PyObject *pObj = pMainModule;
    PyObject *pObjStr = NULL;
    char *dot = index(objandfn, '.');
    while (dot != NULL) {
        pObjParent = pObj;

        pObjStr = PyUnicode_FromStringAndSize(objandfn, dot - objandfn);
        if (pObjStr == NULL) {
            Py_DECREF(pObjParent);
            Tcl_DStringFree(&ds);
            return PyReturnException(interp, "failed unicode translation of call function in python interpreter");
        }

        pObj = PyObject_GetAttr(pObjParent, pObjStr);
        Py_DECREF(pObjStr);
        Py_DECREF(pObjParent);
        if (pObj == NULL) {
            Tcl_DStringFree(&ds);
            return PyReturnException(interp, "failed to find dotted attribute in python interpreter");
        }

        objandfn = dot + 1;
        dot = index(objandfn, '.');
    }

    PyObject *pFn = PyObject_GetAttrString(pObj, objandfn);
    Py_DECREF(pObj);
    Tcl_DStringFree(&ds);
    if (pFn == NULL)
        return PyReturnException(interp, "failed to find object/function in python interpreter");

    if (!PyCallable_Check(pFn)) {
        Py_DECREF(pFn);
        return PyReturnException(interp, "function is not callable");
    }

    // if there are no positional arguments, we will
    // call PyTuple_New with a 0 argument, producing
    // a 0-length tuple.  whil PyObject_Call's kwargs
    // argument can be NULL, args must not; an empty
    // tuple should be used
    int i;
    PyObject *pArgs = PyTuple_New(objc - objStart);
    PyObject *curarg = NULL;
    for (i = objStart; i < objc; i++) {
        curarg = PyUnicode_FromString(tohil_TclObjToUTF8(objv[i], &ds));
        Tcl_DStringFree(&ds);
        if (curarg == NULL) {
            Py_DECREF(pArgs);
            Py_DECREF(pFn);
            return PyReturnException(interp, "unicode string conversion failed");
        }
        /* Steals a reference */
        PyTuple_SET_ITEM(pArgs, i - objStart, curarg);
    }

    PyObject *pRet = PyObject_Call(pFn, pArgs, kwObj);
    Py_DECREF(pFn);
    Py_DECREF(pArgs);
    if (kwObj != NULL)
        Py_DECREF(kwObj);
    if (pRet == NULL)
        return PyReturnException(interp, "error in python object call");

    Tcl_Obj *tRet = pyObjToTcl(interp, pRet);
    Py_DECREF(pRet);
    if (tRet == NULL)
        return PyReturnException(interp, "error converting python object to tcl object");

    Tcl_SetObjResult(interp, tRet);
    return TCL_OK;
}

//
// implements tcl command tohil::import, to import a python module
//   into the python interpreter.
//
int
TohilImport_Cmd(ClientData clientData, /* Not used. */
                Tcl_Interp *interp,    /* Current interpreter */
                int objc,              /* Number of arguments */
                Tcl_Obj *const objv[]  /* Argument strings */
)
{
    const char *modname, *topmodname;
    PyObject *pMainModule, *pTopModule;
    int ret = -1;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "module");
        return TCL_ERROR;
    }

    modname = Tcl_GetString(objv[1]);

    /* Borrowed ref, do not decrement */
    pMainModule = PyImport_AddModule("__main__");
    if (pMainModule == NULL)
        return PyReturnException(interp, "add module __main__ failed");

    // We don't use PyImport_ImportModule so mod.submod works
    pTopModule = PyImport_ImportModuleEx(modname, NULL, NULL, NULL);
    if (pTopModule == NULL)
        return PyReturnException(interp, "import module failed");

    topmodname = PyModule_GetName(pTopModule);
    if (topmodname != NULL) {
        ret = PyObject_SetAttrString(pMainModule, topmodname, pTopModule);
    }
    Py_DECREF(pTopModule);

    if (ret < 0)
        return PyReturnException(interp, "while trying to import a module");

    return TCL_OK;
}

int
TohilEval_Cmd(ClientData clientData, /* Not used. */
              Tcl_Interp *interp,    /* Current interpreter */
              int objc,              /* Number of arguments */
              Tcl_Obj *const objv[]  /* Argument strings */
)
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "evalString");
        return TCL_ERROR;
    }
    Tcl_DString ds;
    const char *cmd = tohil_TclObjToUTF8(objv[1], &ds);

    // PyCompilerFlags flags = _PyCompilerFlags_INIT;
    // PyObject *code = Py_CompileStringExFlags(cmd, "tohil", Py_eval_input, &flags, -1);
    PyObject *code = Py_CompileStringExFlags(cmd, "tohil", Py_eval_input, NULL, -1);
    Tcl_DStringFree(&ds);

    if (code == NULL) {
        return PyReturnException(interp, "while compiling python eval code");
    }

    PyObject *main_module = PyImport_AddModule("__main__");
    PyObject *global_dict = PyModule_GetDict(main_module);
    PyObject *pyobj = PyEval_EvalCode(code, global_dict, global_dict);

    Py_XDECREF(code);

    if (pyobj == NULL) {
        return PyReturnException(interp, "while evaluating python code");
    }

    Tcl_SetObjResult(interp, pyObjToTcl(interp, pyobj));
    Py_XDECREF(pyobj);
    return TCL_OK;
}

// awfully similar to TohilEval_Cmd above
// but expecting to do more like capture stdout
int
TohilExec_Cmd(ClientData clientData, /* Not used. */
              Tcl_Interp *interp,    /* Current interpreter */
              int objc,              /* Number of arguments */
              Tcl_Obj *const objv[]  /* Argument strings */
)
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "execString");
        return TCL_ERROR;
    }
    Tcl_DString ds;
    const char *cmd = tohil_TclObjToUTF8(objv[1], &ds);

    PyObject *code = Py_CompileStringExFlags(cmd, "tohil", Py_file_input, NULL, -1);
    Tcl_DStringFree(&ds);

    if (code == NULL) {
        return PyReturnException(interp, "while compiling python exec code");
    }

    PyObject *main_module = PyImport_AddModule("__main__");
    PyObject *global_dict = PyModule_GetDict(main_module);
    PyObject *pyobj = PyEval_EvalCode(code, global_dict, global_dict);

    Py_XDECREF(code);

    if (pyobj == NULL) {
        return PyReturnException(interp, "while evaluating python code");
    }

    Tcl_SetObjResult(interp, pyObjToTcl(interp, pyobj));
    Py_XDECREF(pyobj);
    return TCL_OK;
}

//
// implements tcl-side tohil::interact command to launch the python
//   interpreter's interactive loop
//
int
TohilInteract_Cmd(ClientData clientData, /* Not used. */
                  Tcl_Interp *interp,    /* Current interpreter */
                  int objc,              /* Number of arguments */
                  Tcl_Obj *const objv[]  /* Argument strings */
)
{
    if (objc != 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "");
        return TCL_ERROR;
    }

    int result = PyRun_InteractiveLoop(stdin, "stdin");
    if (result < 0) {
        return PyReturnException(interp, "interactive loop failure");
    }

    return TCL_OK;
}

/* Python library begins here */

//
//
// start of tclobj td_iterator python datatype
//
//

PyObject *
PyTohil_TD_iter(PyTohil_TD_IterObj *self)
{
    Py_INCREF(self);

    self->started = 0;
    self->done = 0;

    return (PyObject *)self;
}

//
// td's iternext - we have to do the DictObjFirst here since
//   tcl's version, first gives you also the first result.
//
PyObject *
PyTohil_TD_iternext(PyTohil_TD_IterObj *self)
{
    Tcl_Obj *keyObj = NULL;
    Tcl_Obj *valueObj = NULL;
    int done = 0;

    int utf8len;
    char *utf8string;

    if (self->done) {
    done:
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    if (self->started == 0) {
        self->started = 1;
        // substitute &valueObj for NULL below if you also want the value
        if (Tcl_DictObjFirst(tcl_interp, self->dictObj, &self->search, &keyObj, &valueObj, &done) == TCL_ERROR) {
            PyErr_Format(PyExc_TypeError, "tclobj contents cannot be converted into a td");
            return NULL;
        }
    } else {
        // not the first time though, get the next entry
        Tcl_DictObjNext(&self->search, &keyObj, &valueObj, &done);
    }

    if (done) {
        Tcl_DictObjDone(&self->search);
        self->done = 1;
        Tcl_DecrRefCount(self->dictObj);
        self->dictObj = NULL;
        Py_XDECREF(self->to);
        self->to = NULL;
        goto done;
    }

    if (self->to == NULL) {
        int tclStringSize;
        char *tclString = Tcl_GetStringFromObj(keyObj, &tclStringSize);

        if (tohil_TclToUTF8(tclString, tclStringSize, &utf8string, &utf8len) != TCL_OK) {
            PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            return NULL;
        }
        PyObject *pObj = Py_BuildValue("s#", utf8string, utf8len);
        ckfree(utf8string);
        return pObj;
    }

    // they specified a to, return a tuple
    PyObject *pRetTuple = PyTuple_New(2);

    int tclStringSize;
    char *tclString = Tcl_GetStringFromObj(keyObj, &tclStringSize);

    if (tohil_TclToUTF8(tclString, tclStringSize, &utf8string, &utf8len) != TCL_OK) {
        PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
        return NULL;
    }
    PyTuple_SET_ITEM(pRetTuple, 0, Py_BuildValue("s#", utf8string, utf8len));
    ckfree(utf8string);

    PyTuple_SET_ITEM(pRetTuple, 1, tohil_python_return(tcl_interp, TCL_OK, self->to, valueObj));

    return pRetTuple;
}

PyTypeObject PyTohil_TD_IterType = {
    .tp_name = "tohil._td_iter",
    .tp_basicsize = sizeof(PyTohil_TD_IterObj),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "tohil TD iterator object",
    .tp_iter = (getiterfunc)PyTohil_TD_iter,
    .tp_iternext = (iternextfunc)PyTohil_TD_iternext,
};

//
// t.td_iter()
//
PyObject *
PyTohil_TD_td_iter(PyTclObj *self, PyObject *args, PyObject *kwargs)
{
    PyObject *pTo = NULL;
    static char *kwlist[] = {"to", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|$O", kwlist, &pTo)) {
        return NULL;
    }

    int size = 0;
    if (Tcl_DictObjSize(tcl_interp, self->tclobj, &size) == TCL_ERROR) {
        PyErr_Format(PyExc_TypeError, "tclobj contents cannot be converted into a td");
        return NULL;
    }

    PyTohil_TD_IterObj *pIter = (PyTohil_TD_IterObj *)PyObject_New(PyTohil_TD_IterObj, &PyTohil_TD_IterType);

    pIter->started = 0;
    pIter->done = 0;
    memset((void *)&pIter->search, 0, sizeof(Tcl_DictSearch));
    pIter->to = pTo;

    if (pTo != NULL) {
        Py_INCREF(pTo);
    }
    pIter->dictObj = ((PyTclObj *)self)->tclobj;
    Tcl_IncrRefCount(pIter->dictObj);

    return (PyObject *)pIter;
}

//
//
// end of tclobj td_iterator python datatype
//
//

PyMappingMethods PyTclObj_as_mapping = {(lenfunc)PyTclObj_length, (binaryfunc)PyTclObj_subscript, NULL};

PySequenceMethods PyTclObj_as_sequence = {
    .sq_length = (lenfunc)PyTclObj_length,
    // .sq_concat = (binaryfunc)tclobj_concat,
    // .sq_repeat = (ssizeargfunc)tclobj_repeat,
    .sq_item = (ssizeargfunc)PyTclObj_item,
    .sq_ass_item = (ssizeobjargproc)PyTclObj_ass_item,
    // .sq_contains = (objobjproc)list_contains,
    //.sq_inplace_concat = (binaryfunc)list_inplace_concat,
    //.sq_inplace_repeat = (ssizeargfunc)list_inplace_repeat,
};

PyMethodDef PyTclObj_methods[] = {
    {"__getitem__", (PyCFunction)PyTclObj_subscript, METH_O | METH_COEXIST, "x.__getitem__(y) <==> x[y]"},
    {"reset", (PyCFunction)PyTclObj_reset, METH_NOARGS, "reset the tclobj"},
    {"as_str", (PyCFunction)PyTclObj_as_string, METH_NOARGS, "return tclobj as str"},
    {"as_int", (PyCFunction)PyTclObj_as_int, METH_NOARGS, "return tclobj as int"},
    {"as_float", (PyCFunction)PyTclObj_as_float, METH_NOARGS, "return tclobj as float"},
    {"as_bool", (PyCFunction)PyTclObj_as_bool, METH_NOARGS, "return tclobj as bool"},
    {"as_list", (PyCFunction)PyTclObj_as_list, METH_NOARGS, "return tclobj as list"},
    {"as_set", (PyCFunction)PyTclObj_as_set, METH_NOARGS, "return tclobj as set"},
    {"as_tuple", (PyCFunction)PyTclObj_as_tuple, METH_NOARGS, "return tclobj as tuple"},
    {"as_dict", (PyCFunction)PyTclObj_as_dict, METH_NOARGS, "return tclobj as dict"},
    {"as_tclobj", (PyCFunction)PyTclObj_as_tclobj, METH_NOARGS, "return tclobj as tclobj"},
    {"as_byte_array", (PyCFunction)PyTclObj_as_byte_array, METH_NOARGS, "return tclobj as a byte array"},
    {"incr", (PyCFunction)PyTclObj_incr, METH_VARARGS | METH_KEYWORDS, "increment tclobj as int"},
    {"llength", (PyCFunction)PyTclObj_llength, METH_NOARGS, "length of tclobj tcl list"},
    {"td_get", (PyCFunction)PyTclObj_td_get, METH_VARARGS | METH_KEYWORDS, "get from tcl dict"},
    {"td_exists", (PyCFunction)PyTclObj_td_exists, METH_VARARGS | METH_KEYWORDS, "see if key exists in tcl dict"},
    {"td_remove", (PyCFunction)PyTclObj_td_remove, METH_VARARGS | METH_KEYWORDS, "remove item or list hierarchy from tcl dict"},
    {"td_iter", (PyCFunction)PyTohil_TD_td_iter, METH_VARARGS | METH_KEYWORDS, "iterate on a tclobj containing a tcl dict"},
    {"td_set", (PyCFunction)PyTclObj_td_set, METH_VARARGS | METH_KEYWORDS, "set item in tcl dict"},
    {"td_size", (PyCFunction)PyTclObj_td_size, METH_NOARGS, "get size of tcl dict"},
    {"getvar", (PyCFunction)PyTclObj_getvar, METH_O, "set tclobj to tcl var or array element"},
    {"setvar", (PyCFunction)PyTclObj_setvar, METH_O, "set tcl var or array element to tclobj's tcl object"},
    {"set", (PyCFunction)PyTclObj_set, METH_O, "set tclobj from some python object"},
    {"lindex", (PyCFunction)PyTclObj_lindex, METH_VARARGS | METH_KEYWORDS, "get value from tclobj as tcl list"},
    {"lappend", (PyCFunction)PyTclObj_lappend, METH_O, "lappend (list-append) something to tclobj"},
    {"lappend_list", (PyCFunction)PyTclObj_lappend_list, METH_O, "lappend another tclobj or a python list of stuff to tclobj"},
    {"refcount", (PyCFunction)PyTclObj_refcount, METH_NOARGS, "get tclobj's reference count"},
    {"type", (PyCFunction)PyTclObj_type, METH_NOARGS, "return the tclobj's type from tcl, or None if it doesn't have one"},
    {NULL} // sentinel
};

PyTypeObject PyTclObjType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "tohil.tclobj",
    .tp_doc = "Tcl Object",
    .tp_basicsize = sizeof(PyTclObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PyTclObj_new,
    .tp_init = (initproc)PyTclObj_init,
    .tp_dealloc = (destructor)PyTclObj_dealloc,
    .tp_methods = PyTclObj_methods,
    .tp_str = (reprfunc)PyTclObj_str,
    .tp_iter = (getiterfunc)PyTclObjIter,
    .tp_as_sequence = &PyTclObj_as_sequence,
    .tp_as_mapping = &PyTclObj_as_mapping,
    .tp_repr = (reprfunc)PyTclObj_repr,
    .tp_richcompare = (richcmpfunc)PyTclObj_richcompare,
};

//
// end of tclobj python datatype
//

// say return tohil_python_return(interp, tcl_result, to string, resultObject)
// from any python C function in this library that accepts a to=python_data_type argument,
// and this routine ought to handle it
PyObject *
tohil_python_return(Tcl_Interp *interp, int tcl_result, PyObject *toType, Tcl_Obj *resultObj)
{
    const char *toString = NULL;
    PyTypeObject *pt = NULL;

    if (PyErr_Occurred() != NULL) {
        printf("tohil_python_return invoked with a python error already present\n");
        // return NULL;
    }

    if (tcl_result == TCL_ERROR) {
        Tcl_Obj *returnOptionsObj = Tcl_GetReturnOptions(interp, tcl_result);
        PyObject *pReturnOptionsObj = PyTclObj_FromTclObj(returnOptionsObj);

        PyObject *pRetTuple = PyTuple_New(2);
        int tclStringSize;
        char *tclString;
        tclString = Tcl_GetStringFromObj(resultObj, &tclStringSize);
        PyTuple_SET_ITEM(pRetTuple, 0, Py_BuildValue("s#", tclString, tclStringSize));
        PyTuple_SET_ITEM(pRetTuple, 1, pReturnOptionsObj);

        PyErr_SetObject(pTohilTclErrorClass, pRetTuple);
        return NULL;
    }

    if (toType != NULL) {
        if (!PyType_Check(toType)) {
            PyErr_SetString(PyExc_RuntimeError, "to type is not a valid python data type");
            return NULL;
        }

        // toType/pt is a borrowed reference; do not decrement its reference count
        pt = (PyTypeObject *)toType;
        toString = pt->tp_name;
    }
    // printf("tohil_python_return called: tcl result %d, to=%s, resulObj '%s'\n", tcl_result, toString, Tcl_GetString(resultObj));

    if (toType == NULL || strcmp(toString, "str") == 0) {
        int tclStringSize;
        char *tclString;
        int utf8len;
        char *utf8string;

        tclString = Tcl_GetStringFromObj(resultObj, &tclStringSize);
        if (tohil_TclToUTF8(tclString, tclStringSize, &utf8string, &utf8len) != TCL_OK) {
            PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            return NULL;
        }
        PyObject *pObj = Py_BuildValue("s#", utf8string, utf8len);
        ckfree(utf8string);
        return pObj;
    }

    if (strcmp(toString, "int") == 0) {
        long longValue;

        if (Tcl_GetLongFromObj(interp, resultObj, &longValue) == TCL_OK) {
            return PyLong_FromLong(longValue);
        }
        PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(interp)));
        return NULL;
    }

    if (strcmp(toString, "bool") == 0) {
        int boolValue;

        if (Tcl_GetBooleanFromObj(interp, resultObj, &boolValue) == TCL_OK) {
            PyObject *p = (boolValue ? Py_True : Py_False);
            Py_INCREF(p);
            return p;
        }
        PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(interp)));
        return NULL;
    }

    if (strcmp(toString, "float") == 0) {
        double doubleValue;

        if (Tcl_GetDoubleFromObj(interp, resultObj, &doubleValue) == TCL_OK) {
            return PyFloat_FromDouble(doubleValue);
        }
        PyErr_SetString(PyExc_RuntimeError, Tcl_GetString(Tcl_GetObjResult(interp)));
        return NULL;
    }

    if (strcmp(toString, "tohil.tclobj") == 0) {
        return PyTclObj_FromTclObj(resultObj);
    }

    if (strcmp(toString, "list") == 0) {
        return tclListObjToPyListObject(interp, resultObj);
    }

    if (strcmp(toString, "set") == 0) {
        return tclListObjToPySetObject(interp, resultObj);
    }

    if (strcmp(toString, "dict") == 0) {
        return tclListObjToPyDictObject(interp, resultObj);
    }

    if (strcmp(toString, "tuple") == 0) {
        return tclListObjToPyTupleObject(interp, resultObj);
    }

    PyErr_SetString(PyExc_RuntimeError, "'to' conversion type must be str, int, bool, float, list, set, dict, tuple, or tohil.tclobj");
    return NULL;
}

//
// tohil.eval command for python to eval code in the tcl interpreter
//
PyObject *
tohil_eval(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"tcl_code", "to", NULL};
    PyObject *to = NULL;
    char *utf8Code = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|$O", kwlist, &utf8Code, &to))
        return NULL;

    // TODO modify the above PyArg_ParseTupleAndKeywords to return a length?
    Tcl_DString ds;
    char *tclCode = tohil_UTF8ToTcl(utf8Code, -1, &ds);
    int result = Tcl_Eval(tcl_interp, tclCode);
    Tcl_DStringFree(&ds);
    Tcl_Obj *resultObj = Tcl_GetObjResult(tcl_interp);

    return tohil_python_return(tcl_interp, result, to, resultObj);
}

//
// tohil.expr command for python to evaluate expressions using the tcl interpreter
//
PyObject *
tohil_expr(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"expression", "to", NULL};
    char *utf8expression = NULL;
    PyObject *to = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|$O", kwlist, &utf8expression, &to))
        return NULL;

    Tcl_DString ds;
    char *expression = tohil_UTF8ToTcl(utf8expression, -1, &ds);

    Tcl_Obj *resultObj = NULL;
    if (Tcl_ExprObj(tcl_interp, Tcl_NewStringObj(expression, -1), &resultObj) == TCL_ERROR) {
        char *errMsg = Tcl_GetString(Tcl_GetObjResult(tcl_interp));
        PyErr_SetString(PyExc_RuntimeError, errMsg);
        return NULL;
    }

    return tohil_python_return(tcl_interp, TCL_OK, to, resultObj);
}

//
// tohil.convert command for python to pass a python object through
// to a tcl object and then convert it to a to= destination type
//
PyObject *
tohil_convert(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"pyobject", "to", NULL};
    PyObject *pyInputObject = NULL;
    PyObject *to = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$O", kwlist, &pyInputObject, &to))
        return NULL;

    Tcl_Obj *interimObj = pyObjToTcl(tcl_interp, pyInputObject);
    if (interimObj == NULL) {
        return NULL;
    }

    return tohil_python_return(tcl_interp, TCL_OK, to, interimObj);
}

//
// tohil.getvar - from python get the contents of a variable
//
PyObject *
tohil_getvar(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"var", "to", "default", NULL};
    char *var = NULL;
    PyObject *to = NULL;
    PyObject *defaultPyObj = NULL;
    Tcl_Obj *obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|$OO", kwlist, &var, &to, &defaultPyObj)) {
        return NULL;
    }

    if (defaultPyObj == NULL) {
        // a default wasn't specified, it's an error if the var or array
        // element doesn't exist
        obj = Tcl_GetVar2Ex(tcl_interp, var, NULL, (TCL_LEAVE_ERR_MSG));

        if (obj == NULL) {
            PyErr_SetString(PyExc_NameError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            return NULL;
        }
    } else {
        // a default was specified, it's not an error if the var or array
        // element doesn't exist, we simply return the default value
        obj = Tcl_GetVar2Ex(tcl_interp, var, NULL, 0);
        if (obj == NULL) {
            // not there but they provided a default
            if (to == NULL) {
                // they didn't provide a to= conversion,
                // give them their default as is
                Py_INCREF(defaultPyObj);
                return defaultPyObj;
            } else {
                // they provided a to= conversion, run
                // their python through that and return it.
                obj = pyObjToTcl(tcl_interp, defaultPyObj);
            }
        }
    }

    // the var or array element exists in tcl, return the value to python,
    // possibly to a specific datatype
    return tohil_python_return(tcl_interp, TCL_OK, to, obj);
}

//
// tohil.exists - from python see if a variable or array element exists in tcl
//
PyObject *
tohil_exists(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"var", NULL};
    char *var = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|$", kwlist, &var))
        return NULL;

    Tcl_Obj *obj = Tcl_GetVar2Ex(tcl_interp, var, NULL, 0);

    PyObject *p = (obj == NULL ? Py_False : Py_True);
    Py_INCREF(p);
    return p;
}

//
// tohil.setvar - set a variable or array element in tcl from python
//
PyObject *
tohil_setvar(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"var", "value", NULL};
    char *var = NULL;
    PyObject *pyValue = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO", kwlist, &var, &pyValue))
        return NULL;

    Tcl_Obj *tclValue = pyObjToTcl(tcl_interp, pyValue);

    Tcl_Obj *obj = Tcl_SetVar2Ex(tcl_interp, var, NULL, tclValue, (TCL_LEAVE_ERR_MSG));

    if (obj == NULL) {
        char *errMsg = Tcl_GetString(Tcl_GetObjResult(tcl_interp));
        PyErr_SetString(PyExc_RuntimeError, errMsg);
        return NULL;
    }
    Py_RETURN_NONE;
}

//
// tohil.incr - incr a variable or array element in tcl from python
//
PyObject *
tohil_incr(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"var", "incr", NULL};
    char *var = NULL;
    long longValue = 0;
    long increment = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|l", kwlist, &var, &increment))
        return NULL;

    Tcl_Obj *obj = Tcl_GetVar2Ex(tcl_interp, var, NULL, 0);
    if (obj == NULL) {
        longValue = increment;
        obj = Tcl_NewLongObj(longValue);
        if (Tcl_SetVar2Ex(tcl_interp, var, NULL, obj, (TCL_LEAVE_ERR_MSG)) == NULL) {
            goto type_error;
        }
    } else {
        if (Tcl_GetLongFromObj(tcl_interp, obj, &longValue) == TCL_ERROR) {
        type_error:
            PyErr_SetString(PyExc_TypeError, Tcl_GetString(Tcl_GetObjResult(tcl_interp)));
            return NULL;
        }

        longValue += increment;

        if (Tcl_IsShared(obj)) {
            obj = Tcl_DuplicateObj(obj);
            Tcl_SetLongObj(obj, longValue);
            if (Tcl_SetVar2Ex(tcl_interp, var, NULL, obj, (TCL_LEAVE_ERR_MSG)) == NULL) {
                goto type_error;
            }
        } else {
            Tcl_SetLongObj(obj, longValue);
        }
    }
    return PyLong_FromLong(longValue);
}

// tohil.unset - from python unset a variable or array element in the tcl
//   interpreter.  it is not an error if the variable or element doesn't
//   exist.  if passed the name of an array with no subscripted element,
//   the entire array is deleted
PyObject *
tohil_unset(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"var", NULL};
    char *var = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|$", kwlist, &var))
        return NULL;

    Tcl_UnsetVar(tcl_interp, var, 0);
    Py_RETURN_NONE;
}

//
// tohil.subst - perform tcl "subst" substitution on the passed string,
// evaluating square-bracketed stuff and expanding $-prefaced variables,
// without evaluating the ultimate result, like eval would
//
PyObject *
tohil_subst(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"string", "to", NULL};
    char *string = NULL;
    PyObject *to = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|$O", kwlist, &string, &to)) {
        return NULL;
    }
    Tcl_Obj *obj = Tcl_SubstObj(tcl_interp, Tcl_NewStringObj(string, -1), TCL_SUBST_ALL);
    if (obj == NULL) {
        char *errMsg = Tcl_GetString(Tcl_GetObjResult(tcl_interp));
        PyErr_SetString(PyExc_RuntimeError, errMsg);
        return NULL;
    }

    return tohil_python_return(tcl_interp, TCL_OK, to, obj);
}

//
// tohil.call function for python that's like the tohil::call in tcl, something
// that lets you explicitly specify a tcl command and its arguments and lets
// you avoid passing everything through eval.  here it is.
//
PyObject *
tohil_call(PyObject *self, PyObject *args, PyObject *kwargs)
{
    Py_ssize_t objc = PyTuple_GET_SIZE(args);
    int i;
    PyObject *to = NULL;

    //
    // allocate an array of Tcl object pointers the same size
    // as the number of arguments we received
    Tcl_Obj **objv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *) * objc);

    // PyObject_Print(kwargs, stdout, 0);

    // we need to process kwargs to get the to
    if (kwargs != NULL) {
        to = PyDict_GetItemString(kwargs, "to");
    }

    // for each argument convert the python object to a tcl object
    // and store it in the tcl object vector
    for (i = 0; i < objc; i++) {
        objv[i] = pyObjToTcl(tcl_interp, PyTuple_GET_ITEM(args, i));
        Tcl_IncrRefCount(objv[i]);
    }

    // invoke tcl using the objv array we just constructed
    int tcl_result = Tcl_EvalObjv(tcl_interp, objc, objv, 0);

    // cleanup and free the objv
    for (i = 0; i < objc; i++) {
        Tcl_DecrRefCount(objv[i]);
    }
    ckfree(objv);

    return tohil_python_return(tcl_interp, tcl_result, to, Tcl_GetObjResult(tcl_interp));
}

//
// python C extension structure defining functions
//
PyMethodDef TohilMethods[] = {
    {"eval", (PyCFunction)tohil_eval, METH_VARARGS | METH_KEYWORDS, "Evaluate tcl code"},
    {"getvar", (PyCFunction)tohil_getvar, METH_VARARGS | METH_KEYWORDS, "get vars and array elements from the tcl interpreter"},
    {"setvar", (PyCFunction)tohil_setvar, METH_VARARGS | METH_KEYWORDS, "set vars and array elements in the tcl interpreter"},
    {"exists", (PyCFunction)tohil_exists, METH_VARARGS | METH_KEYWORDS, "check whether vars and array elements exist in the tcl interpreter"},
    {"unset", (PyCFunction)tohil_unset, METH_VARARGS | METH_KEYWORDS, "unset variables, array elements, or arrays from the tcl interpreter"},
    {"incr", (PyCFunction)tohil_incr, METH_VARARGS | METH_KEYWORDS, "increment vars and array elements in the tcl interpreter"},
    {"subst", (PyCFunction)tohil_subst, METH_VARARGS | METH_KEYWORDS, "perform Tcl command, variable and backslash substitutions on a string"},
    {"expr", (PyCFunction)tohil_expr, METH_VARARGS | METH_KEYWORDS, "evaluate Tcl expression"},
    {"convert", (PyCFunction)tohil_convert, METH_VARARGS | METH_KEYWORDS,
     "convert python to tcl object then to whatever to= says or string and return"},
    {"call", (PyCFunction)tohil_call, METH_VARARGS | METH_KEYWORDS, "invoke a tcl command with arguments"},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

// TODO: there should probably be some tcl deinit in the clear/free code
struct PyModuleDef TohilModule = {
    PyModuleDef_HEAD_INIT,
    "tohil",
    "A module to permit interop with Tcl",
    -1,
    TohilMethods,
    NULL, // m_slots
    NULL, // m_traverse
    NULL, // m_clear
    NULL, // m_free
};

/* Shared initialisation begins here */

// this is the entry point when tcl loads the tohil shared library
int
Tohil_Init(Tcl_Interp *interp)
{
    /* TODO: all TCL_ERRORs should set an error return */

    if (Tcl_InitStubs(interp, "8.6", 0) == NULL)
        return TCL_ERROR;

    if (Tcl_PkgRequire(interp, "Tcl", "8.6", 0) == NULL)
        return TCL_ERROR;

    if (Tcl_PkgProvide(interp, "tohil", PACKAGE_VERSION) != TCL_OK)
        return TCL_ERROR;

    if (Tcl_CreateNamespace(interp, "::tohil", NULL, NULL) == NULL)
        return TCL_ERROR;

    if (Tcl_CreateObjCommand(interp, "::tohil::eval", (Tcl_ObjCmdProc *)TohilEval_Cmd, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL) == NULL)
        return TCL_ERROR;

    if (Tcl_CreateObjCommand(interp, "::tohil::exec", (Tcl_ObjCmdProc *)TohilExec_Cmd, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL) == NULL)
        return TCL_ERROR;

    if (Tcl_CreateObjCommand(interp, "::tohil::call", (Tcl_ObjCmdProc *)TohilCall_Cmd, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL) == NULL)
        return TCL_ERROR;

    if (Tcl_CreateObjCommand(interp, "::tohil::import", (Tcl_ObjCmdProc *)TohilImport_Cmd, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL) == NULL)
        return TCL_ERROR;

    if (Tcl_CreateObjCommand(interp, "::tohil::interact", (Tcl_ObjCmdProc *)TohilInteract_Cmd, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL) == NULL)
        return TCL_ERROR;

    // if i haven't been told python is up, tcl is the parent,
    // and we need to initialize the python interpreter and
    // our python module
    if (!Py_IsInitialized()) {
        Py_Initialize();
    }

    // stash the Tcl interpreter pointer so the python side can find it later
    PyObject *main_module = PyImport_AddModule("__main__");
    PyObject *pCap = PyCapsule_New(interp, "tohil.interp", NULL);
    if (PyObject_SetAttrString(main_module, "interp", pCap) == -1) {
        return TCL_ERROR;
    }
    Py_DECREF(pCap);

    // import tohil to get at the python parts
    // and grab a reference to tohil's exception handler
    PyObject *pTohilModStr, *pTohilMod;

    pTohilModStr = PyUnicode_FromString("tohil");
    pTohilMod = PyImport_Import(pTohilModStr);
    Py_DECREF(pTohilModStr);
    if (pTohilMod == NULL) {
        // NB debug break out the exception
        PyObject *pType = NULL, *pVal = NULL, *pTrace = NULL;
        PyErr_Fetch(&pType, &pVal, &pTrace); /* Clears exception */
        PyErr_NormalizeException(&pType, &pVal, &pTrace);
        PyObject_Print(pType, stdout, 0);
        PyObject_Print(pVal, stdout, 0);

        return PyReturnTclError(interp, "unable to import tohil module to python interpreter");
    }

    pTohilHandleException = PyObject_GetAttrString(pTohilMod, "handle_exception");
    if (pTohilHandleException == NULL || !PyCallable_Check(pTohilHandleException)) {
        Py_XDECREF(pTohilHandleException);
        Py_DECREF(pTohilMod);
        return PyReturnTclError(interp, "unable to find tohil.handle_exception function in python interpreter");
    }

    pTohilTclErrorClass = PyObject_GetAttrString(pTohilMod, "TclError");
    if (pTohilTclErrorClass == NULL || !PyCallable_Check(pTohilTclErrorClass)) {
        Py_XDECREF(pTohilTclErrorClass);
        Py_DECREF(pTohilMod);
        return PyReturnTclError(interp, "unable to find tohil.TclError class in python interpreter");
    }
    Py_DECREF(pTohilMod);

    return TCL_OK;
}

//
// this is the entrypoint for when python loads us as a shared library
// note the double underscore, we are _tohil, not tohil, actually tohil._tohil.
// that helps us be able to load other tohil python stuff that's needed, like
// the handle_exception function, before we trigger a load of the shared library,
// by importing from it, see pysrc/tohil/__init__.py
//
PyMODINIT_FUNC
PyInit__tohil(void)
{
    Tcl_Interp *interp = NULL;

    // see if the tcl interpreter already exists by looking
    // for an attribute we stashed in __main__
    // NB i'm sure there's a better place to put this, but
    // it is opaque to python -- need expert help from
    // a python C API maven
    PyObject *main_module = PyImport_AddModule("__main__");
    PyObject *pCap = PyObject_GetAttrString(main_module, "interp");
    if (pCap == NULL) {
        // stashed attribute doesn't exist.
        // tcl interp hasn't been set up.
        // python is the parent.
        // create and initialize the tcl interpreter.
        PyErr_Clear();
        interp = Tcl_CreateInterp();

        if (Tcl_Init(interp) != TCL_OK) {
            return NULL;
        }

        // invoke Tohil_Init to load us into the tcl interpreter
        // NB uh this probably isn't enough and we need to do a
        // package require tohil as there is tcl code in files in
        // the package now
        // OTOH you know you've got the right shared library
        if (Tohil_Init(interp) == TCL_ERROR) {
            return NULL;
        }
    } else {
        // python interpreter-containing attribute exists, get the interpreter
        interp = PyCapsule_GetPointer(pCap, "tohil.interp");
        Py_DECREF(pCap);
    }
    tcl_interp = interp;

    // turn up the tclobj python type
    if (PyType_Ready(&PyTclObjType) < 0) {
        return NULL;
    }

    // turn up the tclobj td iterator type
    if (PyType_Ready(&PyTohil_TD_IterType) < 0) {
        return NULL;
    }

    // create the python module
    PyObject *m = PyModule_Create(&TohilModule);
    if (m == NULL) {
        return NULL;
    }

    // import tohil to get at the python parts
    PyObject *pTohilModStr, *pTohilMod;
    pTohilModStr = PyUnicode_FromString("tohil");
    pTohilMod = PyImport_Import(pTohilModStr);
    Py_DECREF(pTohilModStr);
    if (pTohilMod == NULL) {
        return NULL;
    }

    // find the TclObjIterator class and keep a reference to it
    pyTclObjIterator = PyObject_GetAttrString(pTohilMod, "TclObjIterator");
    if (pyTclObjIterator == NULL || !PyCallable_Check(pyTclObjIterator)) {
        Py_DECREF(pTohilMod);
        Py_XDECREF(pyTclObjIterator);
        PyErr_SetString(PyExc_RuntimeError, "unable to find tohil.TclObjIterator class in python interpreter");
        return NULL;
    }
    Py_INCREF(pyTclObjIterator);

    // add our tclobj type to python
    Py_INCREF(&PyTclObjType);
    if (PyModule_AddObject(m, "tclobj", (PyObject *)&PyTclObjType) < 0) {
        Py_DECREF(&PyTclObjType);
        Py_DECREF(m);
        return NULL;
    }

    // ..and stash a pointer to the tcl interpreter in a python
    // capsule so we can find it when we're doing python stuff
    // and need to talk to tcl
    pCap = PyCapsule_New(interp, "tohil.interp", NULL);
    if (PyObject_SetAttrString(m, "interp", pCap) == -1) {
        return NULL;
    }
    Py_DECREF(pCap);

    return m;
}

// vim: set ts=4 sw=4 sts=4 et :
