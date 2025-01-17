

## Tohil 3 Release Notes

Welcome to Tohil 3.

Tohil 3 brings forward all the slick stuff from Tohil 2, plus it provides the
means of accessing Tcl functions from python in such a way that they very much
look and behave like native python functions.  Not only that, but for tcl procs
made available to python by tohil, every parameter can be specified by
position or by name, something few native python functions or python C
functions can do.

### TclProcs

Any Tcl proc or C command can be defined as a python function simply
by creating a TclProc object and then calling it.

```
>>> import tohil
>>> tohil.package_require("Tclx")
'8.6'
>>> intersect = tohil.TclProc("intersect")
>>> intersect([1, 2, 3, 4, 5, 6], [4, 5, 6, 7, 8, 9], to=list)
['4', '5', '6']
```

It's pretty fun to play with them this way from the command line.

While TclProcs are directly callable, as seen above, they are fully
fledged python object and have a number of interesting and potentially
useful attributes and method, including the python function name,
tcl proc name, whether the tcl function being shadowed is a proc or
not (if not, it's a command written in C), a python dictionary
specifying any default arguments and their values, and the proc's arguments.

### TclNamespaces

But wait, there's more.  tohil.import_namespace(my_namespace) will create
a TclNamespace object and import all the procs and C commands as methods
of that namespace, recursively importing any subordinate namespaces and
their procs and C commands as well.  Namespaces and function calls can
be chained, so you get the hierarchy of tcl namespaces and procs and
C commands created after loading all of your packages, chainable
from python.

It's a convenient way to leverage TclProcs across all of your tcl procs
and commands.

```
>>> import tohil
>>> tohil.package_require("clock::rfc2822")
'0.1'
>>> tcl = tohil.import_tcl()
>>> tcl.clock.rfc2822.parse_date('Wed, 14 Apr 2021 12:04:48 -0500', to=int)
1618419888
```

### TclError Exception Class

Tohil 3 also adds a sweet TclError exception class, and any tcl errors that
bubble back all the way to python without any tcl code having caught the
error will be thrown in python as TclError exceptions.  The TclError object
can be examined to find out all the stuff Tcl knows about the error...
the result, the error code, code level, error stack, traceback, and error line.

### New helpers Functions

tohil.package_require is real useful.  The others ones tohil needs for itself
and they're not as useful, but maybe for some people for some purposes.

* tohil.package_require(package_name, version=version)
* tohil.info_procs() - return a list of procs.  pattern arg optional.
* tohil.info_commands() - return a list of commands, includes procs and C commands.
* tohil.info_body() - return the body of a proc.
* tohil.info_default() - return the default value for an argument of a proc
* tohil.info_args(proc) - return a list of the names of the arguments for a proc
* tohil.namespace_children(namespace) - return a list of all the child namespaces of a namespace

### Tests

* Dozens of new tests.
* "make test" runs both the python ones and the tcl ones


