/* -*- mode: C; c-basic-offset: 4 -*- */

#include "pygobject-private.h"

/* -------------- __gtype__ objects ---------------------------- */

typedef struct {
    PyObject_HEAD
    GType type;
} PyGTypeWrapper;

static int
pyg_type_wrapper_compare(PyGTypeWrapper *self, PyGTypeWrapper *v)
{
    if (self->type == v->type) return 0;
    if (self->type > v->type) return -1;
    return 1;
}

static long
pyg_type_wrapper_hash(PyGTypeWrapper *self)
{
    return (long)self->type;
}

static PyObject *
pyg_type_wrapper_repr(PyGTypeWrapper *self)
{
    char buf[80];
    const gchar *name = g_type_name(self->type);

    g_snprintf(buf, sizeof(buf), "<GType %s (%lu)>",
	       name?name:"invalid", self->type);
    return PyString_FromString(buf);
}

static void
pyg_type_wrapper_dealloc(PyGTypeWrapper *self)
{
    PyObject_DEL(self);
}

PyTypeObject PyGTypeWrapper_Type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "gobject.GType",
    sizeof(PyGTypeWrapper),
    0,
    (destructor)pyg_type_wrapper_dealloc,
    (printfunc)0,
    (getattrfunc)0,
    (setattrfunc)0,
    (cmpfunc)pyg_type_wrapper_compare,
    (reprfunc)pyg_type_wrapper_repr,
    0,
    0,
    0,
    (hashfunc)pyg_type_wrapper_hash,
    (ternaryfunc)0,
    (reprfunc)0,
    0L,0L,0L,0L,
    NULL
};

PyObject *
pyg_type_wrapper_new(GType type)
{
    PyGTypeWrapper *self;

    self = (PyGTypeWrapper *)PyObject_NEW(PyGTypeWrapper,
					  &PyGTypeWrapper_Type);
    if (self == NULL)
	return NULL;

    self->type = type;
    return (PyObject *)self;
}

GType
pyg_type_from_object(PyObject *obj)
{
    PyObject *gtype;
    GType type;

    /* NULL check */
    if (!obj) {
	PyErr_SetString(PyExc_TypeError, "can't get type from NULL object");
	return 0;
    }

    /* map some standard types to primitive GTypes ... */
    if (obj == Py_None)
	return G_TYPE_NONE;
    if (PyType_Check(obj)) {
	PyTypeObject *tp = (PyTypeObject *)obj;

	if (tp == &PyInt_Type)
	    return G_TYPE_INT;
	else if (tp == &PyLong_Type)
	    return G_TYPE_LONG;
	else if (tp == &PyFloat_Type)
	    return G_TYPE_DOUBLE;
	else if (tp == &PyString_Type)
	    return G_TYPE_STRING;
	else if (tp == &PyBaseObject_Type)
	    return PY_TYPE_OBJECT;
    }

    if (obj->ob_type == &PyGTypeWrapper_Type) {
	return ((PyGTypeWrapper *)obj)->type;
    }

    /* handle strings */
    if (PyString_Check(obj)) {
	type = g_type_from_name(PyString_AsString(obj));
	if (type == 0)
	    PyErr_SetString(PyExc_TypeError, "could not find named typecode");
	return type;
    }

    /* finally, look for a __gtype__ attribute on the object */
    gtype = PyObject_GetAttrString(obj, "__gtype__");
    if (gtype) {
	if (gtype->ob_type == &PyGTypeWrapper_Type) {
	    type = ((PyGTypeWrapper *)gtype)->type;
	    Py_DECREF(gtype);
	    return type;
	}
	Py_DECREF(gtype);
    }

    PyErr_Clear();
    PyErr_SetString(PyExc_TypeError, "could not get typecode from object");
    return 0;
}

/* -------------- GValue marshalling ------------------ */

gint
pyg_enum_get_value(GType enum_type, PyObject *obj, gint *val)
{
    GEnumClass *eclass = NULL;
    gint res = -1;

    g_return_val_if_fail(val != NULL, -1);
    if (!obj) {
	*val = 0;
	res = 0;
    } else if (PyInt_Check(obj)) {
	*val = PyInt_AsLong(obj);
	res = 0;
    } else if (PyString_Check(obj)) {
	GEnumValue *info;
	char *str = PyString_AsString(obj);
	
	if (enum_type != G_TYPE_NONE)
	    eclass = G_ENUM_CLASS(g_type_class_ref(enum_type));
	else {
	    PyErr_SetString(PyExc_TypeError, "could not convert string to enum because there is no GType associated to look up the value");
	    res = -1;
	}
	info = g_enum_get_value_by_name(eclass, str);
	g_type_class_unref(eclass);

	if (!info)
	    info = g_enum_get_value_by_nick(eclass, str);
	if (info) {
	    *val = info->value;
	    res = 0;
	} else {
	    PyErr_SetString(PyExc_TypeError, "could not convert string");
	    res = -1;
	}
    } else {
	PyErr_SetString(PyExc_TypeError,"enum values must be strings or ints");
	res = -1;
    }
    return res;
}

gint
pyg_flags_get_value(GType flag_type, PyObject *obj, gint *val)
{
    GFlagsClass *fclass = NULL;
    gint res = -1;

    g_return_val_if_fail(val != NULL, -1);
    if (!obj) {
	*val = 0;
	res = 0;
    } else if (PyInt_Check(obj)) {
	*val = PyInt_AsLong(obj);
	res = 0;
    } else if (PyString_Check(obj)) {
	GFlagsValue *info;
	char *str = PyString_AsString(obj);

	if (flag_type != G_TYPE_NONE)
	    fclass = G_FLAGS_CLASS(g_type_class_ref(flag_type));
	else {
	    PyErr_SetString(PyExc_TypeError, "could not convert string to flag because there is no GType associated to look up the value");
	    res = -1;
	}
	info = g_flags_get_value_by_name(fclass, str);
	g_type_class_unref(fclass);
	
	if (!info)
	    info = g_flags_get_value_by_nick(fclass, str);
	if (info) {
	    *val = info->value;
	    res = 0;
	} else {
	    PyErr_SetString(PyExc_TypeError, "could not convert string");
	    res = -1;
	}
    } else if (PyTuple_Check(obj)) {
	int i, len;

	len = PyTuple_Size(obj);
	*val = 0;
	res = 0;

	if (flag_type != G_TYPE_NONE)
	    fclass = G_FLAGS_CLASS(g_type_class_ref(flag_type));
	else {
	    PyErr_SetString(PyExc_TypeError, "could not convert string to flag because there is no GType associated to look up the value");
	    res = -1;
	}

	for (i = 0; i < len; i++) {
	    PyObject *item = PyTuple_GetItem(obj, i);
	    char *str = PyString_AsString(item);
	    GFlagsValue *info = g_flags_get_value_by_name(fclass, str);

	    if (!info)
		info = g_flags_get_value_by_nick(fclass, str);
	    if (info) {
		*val |= info->value;
	    } else {
		PyErr_SetString(PyExc_TypeError, "could not convert string");
		res = -1;
		break;
	    }
	}
	g_type_class_unref(fclass);
    } else {
	PyErr_SetString(PyExc_TypeError,
			"flag values must be strings, ints or tuples");
	res = -1;
    }
    return res;
}

typedef struct {
    fromvaluefunc fromvalue;
    tovaluefunc tovalue;
} PyGBoxedMarshal;
static GQuark pyg_boxed_marshal_key = 0;

#define pyg_boxed_lookup(boxed_type) \
  ((PyGBoxedMarshal *)g_type_get_qdata((boxed_type), pyg_boxed_marshal_key))

void
pyg_register_boxed_custom(GType boxed_type,
			  fromvaluefunc from_func,
			  tovaluefunc to_func)
{
    PyGBoxedMarshal *bm;

    if (!pyg_boxed_marshal_key)
      pyg_boxed_marshal_key = g_quark_from_static_string("PyGBoxed::marshal");

    bm = g_new(PyGBoxedMarshal, 1);
    bm->fromvalue = from_func;
    bm->tovalue = to_func;
    g_type_set_qdata(boxed_type, pyg_boxed_marshal_key, bm);
}

int
pyg_value_from_pyobject(GValue *value, PyObject *obj)
{
    PyObject *tmp;

    switch (G_TYPE_FUNDAMENTAL(G_VALUE_TYPE(value))) {
    case G_TYPE_INTERFACE:
	/* we only handle interface types that have a GObject prereq */
	if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_OBJECT)) {
	    if (!PyObject_TypeCheck(obj, &PyGObject_Type)) {
		return -1;
	    }
	    if (!G_TYPE_CHECK_INSTANCE_TYPE(pygobject_get(obj),
					    G_VALUE_TYPE(value))) {
		return -1;
	    }
	    g_value_set_object(value, pygobject_get(obj));
	} else {
	    return -1;
	}
	break;
    case G_TYPE_CHAR:
	if ((tmp = PyObject_Str(obj)))
	    g_value_set_char(value, PyString_AsString(tmp)[0]);
	else {
	    PyErr_Clear();
	    return -1;
	}
	Py_DECREF(tmp);
	break;
    case G_TYPE_UCHAR:
	if ((tmp = PyObject_Str(obj)))
	    g_value_set_char(value, PyString_AsString(tmp)[0]);
	else {
	    PyErr_Clear();
	    return -1;
	}
	Py_DECREF(tmp);
	break;
    case G_TYPE_BOOLEAN:
	g_value_set_boolean(value, PyObject_IsTrue(obj));
	break;
    case G_TYPE_INT:
	g_value_set_int(value, PyInt_AsLong(obj));
	if (PyErr_Occurred()) {
	    g_value_unset(value);
	    PyErr_Clear();
	    return -1;
	}
	break;
    case G_TYPE_UINT:
	{
	    if (PyInt_Check(obj)) {
		glong val;

		val = PyInt_AsLong(obj);
		if (val >= 0 && val <= G_MAXUINT)
		    g_value_set_uint(value, (guint)val);
		else
		    return -1;
	    } else {
		g_value_set_uint(value, PyLong_AsUnsignedLong(obj));
		if (PyErr_Occurred()) {
		    g_value_unset(value);
		    PyErr_Clear();
		    return -1;
		}
	    }
	}
	break;
    case G_TYPE_LONG:
	g_value_set_long(value, PyInt_AsLong(obj));
	if (PyErr_Occurred()) {
	    g_value_unset(value);
	    PyErr_Clear();
	    return -1;
	}
	break;
    case G_TYPE_ULONG:
	{
	    if (PyInt_Check(obj)) {
		glong val;

		val = PyInt_AsLong(obj);
		if (val >= 0)
		    g_value_set_ulong(value, (gulong)val);
		else
		    return -1;
	    } else {
		g_value_set_ulong(value, PyLong_AsUnsignedLong(obj));
		if (PyErr_Occurred()) {
		    g_value_unset(value);
		    PyErr_Clear();
		    return -1;
		}
	    }
	}
	break;
    case G_TYPE_INT64:
	g_value_set_int64(value, PyLong_AsLongLong(obj));
	if (PyErr_Occurred()) {
	    g_value_unset(value);
	    PyErr_Clear();
	    return -1;
	}
	break;
    case G_TYPE_UINT64:
	g_value_set_uint64(value, PyLong_AsUnsignedLongLong(obj));
	if (PyErr_Occurred()) {
	    g_value_unset(value);
	    PyErr_Clear();
	    return -1;
	}
	break;
    case G_TYPE_ENUM:
	{
	    gint val = 0;
	    if (pyg_enum_get_value(G_VALUE_TYPE(value), obj, &val) < 0)
		return -1;
	    g_value_set_enum(value, val);
	}
	break;
    case G_TYPE_FLAGS:
	{
	    guint val = 0;
	    if (pyg_flags_get_value(G_VALUE_TYPE(value), obj, &val) < 0)
		return -1;
	    g_value_set_flags(value, val);
	}
	break;
    case G_TYPE_FLOAT:
	g_value_set_float(value, PyFloat_AsDouble(obj));
	if (PyErr_Occurred()) {
	    g_value_unset(value);
	    PyErr_Clear();
	    return -1;
	}
	break;
    case G_TYPE_DOUBLE:
	g_value_set_double(value, PyFloat_AsDouble(obj));
	if (PyErr_Occurred()) {
	    g_value_unset(value);
	    PyErr_Clear();
	    return -1;
	}
	break;
    case G_TYPE_STRING:
	if (obj == Py_None)
	    g_value_set_string(value, NULL);
	else if ((tmp = PyObject_Str(obj))) {
	    g_value_set_string(value, PyString_AsString(tmp));
	    Py_DECREF(tmp);
	} else {
	    PyErr_Clear();
	    return -1;
	}
	break;
    case G_TYPE_POINTER:
	if (obj == Py_None)
	    g_value_set_pointer(value, NULL);
	else if (PyObject_TypeCheck(obj, &PyGPointer_Type) &&
		   G_VALUE_HOLDS(value, ((PyGPointer *)obj)->gtype))
	    g_value_set_pointer(value, pyg_pointer_get(obj, gpointer));
	else if (PyCObject_Check(obj))
	    g_value_set_pointer(value, PyCObject_AsVoidPtr(obj));
	else
	    return -1;
	break;
    case G_TYPE_BOXED: {
	PyGBoxedMarshal *bm;

	if (obj == Py_None)
	    g_value_set_boxed(value, NULL);
	if (G_VALUE_HOLDS(value, PY_TYPE_OBJECT))
	    g_value_set_boxed(value, obj);
	else if (PyObject_TypeCheck(obj, &PyGBoxed_Type) &&
		   G_VALUE_HOLDS(value, ((PyGBoxed *)obj)->gtype))
	    g_value_set_boxed(value, pyg_boxed_get(obj, gpointer));
	else if ((bm = pyg_boxed_lookup(G_VALUE_TYPE(value))) != NULL)
	    return bm->tovalue(value, obj);
	else if (PyCObject_Check(obj))
	    g_value_set_boxed(value, PyCObject_AsVoidPtr(obj));
	else
	    return -1;
	break;
    }
    case G_TYPE_PARAM:
	if (PyGParamSpec_Check(obj))
	    g_value_set_param(value, PyCObject_AsVoidPtr(obj));
	else
	    return -1;
	break;
    case G_TYPE_OBJECT:
	if (obj == Py_None) {
	    g_value_set_object(value, NULL);
	} else if (PyObject_TypeCheck(obj, &PyGObject_Type) &&
		   G_TYPE_CHECK_INSTANCE_TYPE(pygobject_get(obj),
					      G_VALUE_TYPE(value))) {
	    g_value_set_object(value, pygobject_get(obj));
	} else
	    return -1;
	break;
    default:
	break;
    }
    return 0;
}

/**
 * pyg_value_as_pyobject:
 * @value: the GValue object.
 * @copy_boxed: true if boxed values should be copied.
 *
 * This function creates/returns a Python wrapper object that
 * represents the GValue passed as an argument.
 *
 * Returns: a PyObject representing the value.
 */
PyObject *
pyg_value_as_pyobject(const GValue *value, gboolean copy_boxed)
{
    gchar buf[128];

    switch (G_TYPE_FUNDAMENTAL(G_VALUE_TYPE(value))) {
    case G_TYPE_INTERFACE:
	if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_OBJECT))
	    return pygobject_new(g_value_get_object(value));
	else
	    break;
    case G_TYPE_CHAR:
	{
	    gint8 val = g_value_get_char(value);
	    return PyString_FromStringAndSize((char *)&val, 1);
	}
    case G_TYPE_UCHAR:
	{
	    guint8 val = g_value_get_uchar(value);
	    return PyString_FromStringAndSize((char *)&val, 1);
	}
    case G_TYPE_BOOLEAN:
	return PyInt_FromLong(g_value_get_boolean(value));
    case G_TYPE_INT:
	return PyInt_FromLong(g_value_get_int(value));
    case G_TYPE_UINT:
	{
	    /* in Python, the Int object is backed by a long.  If a
	       long can hold the whole value of an unsigned int, use
	       an Int.  Otherwise, use a Long object to avoid overflow.
	       This matches the ULongArg behavior in codegen/argtypes.h */
#if (G_MAXUINT <= G_MAXLONG)
	    return PyInt_FromLong((glong) g_value_get_uint(value));
#else
	    return PyLong_FromUnsignedLong((gulong) g_value_get_uint(value));
#endif
	}
    case G_TYPE_LONG:
	return PyInt_FromLong(g_value_get_long(value));
    case G_TYPE_ULONG:
	{
	    gulong val = g_value_get_ulong(value);

	    if (val <= G_MAXLONG)
		return PyInt_FromLong((glong) val);
	    else
		return PyLong_FromUnsignedLong(val);
	}
    case G_TYPE_INT64:
	{
	    gint64 val = g_value_get_int64(value);

	    if (G_MINLONG <= val && val <= G_MAXLONG)
		return PyInt_FromLong((glong) val);
	    else
		return PyLong_FromLongLong(val);
	}
    case G_TYPE_UINT64:
	{
	    guint64 val = g_value_get_uint64(value);

	    if (val <= G_MAXLONG)
		return PyInt_FromLong((glong) val);
	    else
		return PyLong_FromUnsignedLongLong(val);
	}
    case G_TYPE_ENUM:
	return PyInt_FromLong(g_value_get_enum(value));
    case G_TYPE_FLAGS:
	return PyInt_FromLong(g_value_get_flags(value));
    case G_TYPE_FLOAT:
	return PyFloat_FromDouble(g_value_get_float(value));
    case G_TYPE_DOUBLE:
	return PyFloat_FromDouble(g_value_get_double(value));
    case G_TYPE_STRING:
	{
	    const gchar *str = g_value_get_string(value);

	    if (str)
		return PyString_FromString(str);
	    Py_INCREF(Py_None);
	    return Py_None;
	}
    case G_TYPE_POINTER:
	return pyg_pointer_new(G_VALUE_TYPE(value),
			       g_value_get_pointer(value));
    case G_TYPE_BOXED: {
	PyGBoxedMarshal *bm;

	if (G_VALUE_HOLDS(value, PY_TYPE_OBJECT)) {
	    PyObject *ret = (PyObject *)g_value_dup_boxed(value);
	    if (ret == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	    }
	    return ret;
	}
	    
	bm = pyg_boxed_lookup(G_VALUE_TYPE(value));
	if (bm) {
	    return bm->fromvalue(value);
	} else {
	    if (copy_boxed)
		return pyg_boxed_new(G_VALUE_TYPE(value),
				     g_value_get_boxed(value), TRUE, TRUE);
	    else
		return pyg_boxed_new(G_VALUE_TYPE(value),
				     g_value_get_boxed(value),FALSE,FALSE);
	}
    }
    case G_TYPE_PARAM:
	return pyg_param_spec_new(g_value_get_param(value));
    case G_TYPE_OBJECT:
	return pygobject_new(g_value_get_object(value));
    default:
	break;
    }
    g_snprintf(buf, sizeof(buf), "unknown type %s",
	       g_type_name(G_VALUE_TYPE(value)));
    PyErr_SetString(PyExc_TypeError, buf);
    return NULL;
}

/* -------------- PyGClosure ----------------- */

static void
pyg_closure_invalidate(gpointer data, GClosure *closure)
{
    PyGClosure *pc = (PyGClosure *)closure;

    pyg_block_threads();
    Py_XDECREF(pc->callback);
    Py_XDECREF(pc->extra_args);
    Py_XDECREF(pc->swap_data);
    pc->callback = NULL;
    pc->extra_args = NULL;
    pc->swap_data = NULL;
    pyg_unblock_threads();
}

/* XXXX - need to handle python thread context stuff */
static void
pyg_closure_marshal(GClosure *closure,
		    GValue *return_value,
		    guint n_param_values,
		    const GValue *param_values,
		    gpointer invocation_hint,
		    gpointer marshal_data)
{
    PyGClosure *pc = (PyGClosure *)closure;
    PyObject *params, *ret;
    guint i;

    pyg_block_threads();
    /* construct Python tuple for the parameter values */
    params = PyTuple_New(n_param_values);
    for (i = 0; i < n_param_values; i++) {
	/* swap in a different initial data for connect_object() */
	if (i == 0 && G_CCLOSURE_SWAP_DATA(closure)) {
	    g_return_if_fail(pc->swap_data != NULL);
	    Py_INCREF(pc->swap_data);
	    PyTuple_SetItem(params, 0, pc->swap_data);
	} else {
	    PyObject *item = pyg_value_as_pyobject(&param_values[i], FALSE);

	    /* error condition */
	    if (!item) {
		Py_DECREF(params);
		pyg_unblock_threads();
		return;
	    }
	    PyTuple_SetItem(params, i, item);
	}
    }
    /* params passed to function may have extra arguments */
    if (pc->extra_args) {
	PyObject *tuple = params;
	params = PySequence_Concat(tuple, pc->extra_args);
	Py_DECREF(tuple);
    }
    ret = PyObject_CallObject(pc->callback, params);
    if (ret == NULL) {
	PyErr_Print();
	PyErr_Clear();
	Py_DECREF(params);
	pyg_unblock_threads();
	return;
    }
    if (return_value)
	pyg_value_from_pyobject(return_value, ret);
    Py_DECREF(params);
    Py_DECREF(ret);
    pyg_unblock_threads();
}

GClosure *
pyg_closure_new(PyObject *callback, PyObject *extra_args, PyObject *swap_data)
{
    GClosure *closure;

    g_return_val_if_fail(callback != NULL, NULL);
    closure = g_closure_new_simple(sizeof(PyGClosure), NULL);
    g_closure_add_invalidate_notifier(closure, NULL, pyg_closure_invalidate);
    g_closure_set_marshal(closure, pyg_closure_marshal);
    Py_INCREF(callback);
    ((PyGClosure *)closure)->callback = callback;
    if (extra_args && extra_args != Py_None) {
	Py_INCREF(extra_args);
	if (!PyTuple_Check(extra_args)) {
	    PyObject *tmp = PyTuple_New(1);
	    PyTuple_SetItem(tmp, 0, extra_args);
	    extra_args = tmp;
	}
	((PyGClosure *)closure)->extra_args = extra_args;
    }
    if (swap_data) {
	Py_INCREF(swap_data);
	((PyGClosure *)closure)->swap_data = swap_data;
	closure->derivative_flag = TRUE;
    }
    return closure;
}

/* -------------- PySignalClassClosure ----------------- */
/* a closure used for the `class closure' of a signal.  As this gets
 * all the info from the first argument to the closure and the
 * invocation hint, we can have a single closure that handles all
 * class closure cases.  We call a method by the name of the signal
 * with "do_" prepended.
 *
 *  We also remove the first argument from the * param list, as it is
 *  the instance object, which is passed * implicitly to the method
 *  object. */

static void
pyg_signal_class_closure_marshal(GClosure *closure,
				 GValue *return_value,
				 guint n_param_values,
				 const GValue *param_values,
				 gpointer invocation_hint,
				 gpointer marshal_data)
{
    GObject *object;
    PyObject *object_wrapper;
    GSignalInvocationHint *hint = (GSignalInvocationHint *)invocation_hint;
    gchar *method_name, *tmp;
    PyObject *method;
    PyObject *params, *ret;
    guint i;

    g_return_if_fail(invocation_hint != NULL);

    pyg_block_threads();
    /* get the object passed as the first argument to the closure */
    object = g_value_get_object(&param_values[0]);
    g_return_if_fail(object != NULL && G_IS_OBJECT(object));

    /* get the wrapper for this object */
    object_wrapper = pygobject_new(object);
    g_return_if_fail(object_wrapper != NULL);

    /* construct method name for this class closure */
    method_name = g_strconcat("do_", g_signal_name(hint->signal_id), NULL);

    /* convert dashes to underscores.  For some reason, g_signal_name
     * seems to convert all the underscores in the signal name to
       dashes??? */
    for (tmp = method_name; *tmp != '\0'; tmp++)
	if (*tmp == '-') *tmp = '_';

    method = PyObject_GetAttrString(object_wrapper, method_name);
    g_free(method_name);

    if (!method) {
	PyErr_Clear();
	Py_DECREF(object_wrapper);
	pyg_unblock_threads();
	return;
    }
    Py_DECREF(object_wrapper);

    /* construct Python tuple for the parameter values */
    params = PyTuple_New(n_param_values - 1);
    for (i = 1; i < n_param_values; i++) {
	PyObject *item = pyg_value_as_pyobject(&param_values[i], TRUE);

	/* error condition */
	if (!item) {
	    Py_DECREF(params);
	    pyg_unblock_threads();
	    return;
	}
	PyTuple_SetItem(params, i - 1, item);
    }

    ret = PyObject_CallObject(method, params);
    if (ret == NULL) {
	PyErr_Print();
	PyErr_Clear();
	Py_DECREF(method);
	Py_DECREF(params);
	pyg_unblock_threads();
	return;
    }
    Py_DECREF(method);
    Py_DECREF(params);
    if (return_value)
	pyg_value_from_pyobject(return_value, ret);
    Py_DECREF(ret);
    pyg_unblock_threads();
}

GClosure *
pyg_signal_class_closure_get(void)
{
    static GClosure *closure;

    if (closure == NULL) {
	closure = g_closure_new_simple(sizeof(GClosure), NULL);
	g_closure_set_marshal(closure, pyg_signal_class_closure_marshal);

	g_closure_ref(closure);
	g_closure_sink(closure);
    }
    return closure;
}

/* ----- __doc__ descriptor for GObject and GInterface ----- */

static void
object_doc_dealloc(PyObject *self)
{
    PyObject_FREE(self);
}

/* append information about signals of a particular gtype */
static void
add_signal_docs(GType gtype, GString *string)
{
    GTypeClass *class = NULL;
    guint *signal_ids, n_ids = 0, i;

    if (G_TYPE_IS_CLASSED(gtype))
	class = g_type_class_ref(gtype);
    signal_ids = g_signal_list_ids(gtype, &n_ids);

    if (n_ids > 0) {
	g_string_append_printf(string, "Signals from %s:\n",
			       g_type_name(gtype));

	for (i = 0; i < n_ids; i++) {
	    GSignalQuery query;
	    guint j;

	    g_signal_query(signal_ids[i], &query);

	    g_string_append(string, "  ");
	    g_string_append(string, query.signal_name);
	    g_string_append(string, " (");
	    for (j = 0; j < query.n_params; j++) {
		g_string_append(string, g_type_name(query.param_types[j]));
		if (j != query.n_params - 1)
		    g_string_append(string, ", ");
	    }
	    g_string_append(string, ")");
	    if (query.return_type && query.return_type != G_TYPE_NONE) {
		g_string_append(string, " -> ");
		g_string_append(string, g_type_name(query.return_type));
	    }
	    g_string_append(string, "\n");
	}
	g_free(signal_ids);
	g_string_append(string, "\n");
    }
    if (class)
	g_type_class_unref(class);
}

static void
add_property_docs(GType gtype, GString *string)
{
    GObjectClass *class;
    GParamSpec **props;
    guint n_props = 0, i;
    gboolean has_prop = FALSE;

    class = g_type_class_ref(gtype);
    props = g_object_class_list_properties(class, &n_props);

    for (i = 0; i < n_props; i++) {
	if (props[i]->owner_type != gtype)
	    continue; /* these are from a parent type */

	/* print out the heading first */
	if (!has_prop) {
	    g_string_append_printf(string, "Properties from %s:\n",
				   g_type_name(gtype));
	    has_prop = TRUE;
	}
	g_string_append_printf(string, "  %s -> %s: %s\n",
			       g_param_spec_get_name(props[i]),
			       g_type_name(props[i]->value_type),
			       g_param_spec_get_nick(props[i]));
	g_string_append_printf(string, "    %s\n",
			       g_param_spec_get_blurb(props[i]));
    }
    g_free(props);
    if (has_prop)
	g_string_append(string, "\n");
    g_type_class_unref(class);
}

static PyObject *
object_doc_descr_get(PyObject *self, PyObject *obj, PyObject *type)
{
    GType gtype = 0;
    GString *string;
    PyObject *pystring;

    if (obj && pygobject_check(obj, &PyGObject_Type)) {
	gtype = G_OBJECT_TYPE(pygobject_get(obj));
	if (!gtype)
	    PyErr_SetString(PyExc_RuntimeError, "could not get object type");
    } else {
	gtype = pyg_type_from_object(type);
    }
    if (!gtype)
	return NULL;

    string = g_string_new_len(NULL, 512);

    if (g_type_is_a(gtype, G_TYPE_INTERFACE))
	g_string_append_printf(string, "Interface %s\n\n", g_type_name(gtype));
    else if (g_type_is_a(gtype, G_TYPE_OBJECT))
	g_string_append_printf(string, "Object %s\n\n", g_type_name(gtype));
    else
	g_string_append_printf(string, "%s\n\n", g_type_name(gtype));

    if (g_type_is_a(gtype, G_TYPE_OBJECT)) {
	GType parent = G_TYPE_OBJECT;

	while (parent) {
	    GType *interfaces;
	    guint n_interfaces, i;

	    add_signal_docs(parent, string);

	    /* add docs for implemented interfaces */
	    interfaces = g_type_interfaces(parent, &n_interfaces);
	    for (i = 0; i < n_interfaces; i++)
		add_signal_docs(interfaces[i], string);
	    g_free(interfaces);

	    parent = g_type_next_base(gtype, parent);
	}
	parent = G_TYPE_OBJECT;
	while (parent) {
	    add_property_docs(parent, string);
	    parent = g_type_next_base(gtype, parent);
	}
    } else if (g_type_is_a(gtype, G_TYPE_OBJECT)) {
	add_signal_docs(gtype, string);
    }

    pystring = PyString_FromStringAndSize(string->str, string->len);
    g_string_free(string, TRUE);
    return pystring;
}

static PyTypeObject PyGObjectDoc_Type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "gobject.GObject__doc__",
    sizeof(PyObject),
    0,
    (destructor)object_doc_dealloc,
    (printfunc)0,
    (getattrfunc)0,
    (setattrfunc)0,
    (cmpfunc)0,
    (reprfunc)0,
    0,
    0,
    0,
    (hashfunc)0,
    (ternaryfunc)0,
    (reprfunc)0,
    (getattrofunc)0,
    (setattrofunc)0,
    0,
    Py_TPFLAGS_DEFAULT,
    NULL,
    (traverseproc)0,
    (inquiry)0,
    (richcmpfunc)0,
    0,
    (getiterfunc)0,
    (iternextfunc)0,
    0,
    0,
    0,
    (PyTypeObject *)0,
    (PyObject *)0,
    (descrgetfunc)object_doc_descr_get,
    (descrsetfunc)0
};

PyObject *
pyg_object_descr_doc_get(void)
{
    static PyObject *doc_descr = NULL;

    if (!doc_descr) {
	doc_descr = PyObject_NEW(PyObject, &PyGObjectDoc_Type);
	if (doc_descr == NULL)
	    return NULL;
    }
    return doc_descr;
}