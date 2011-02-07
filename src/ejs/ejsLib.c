#define EJS_DEFINE_OPTABLE 1
#include "ejs.h"

/******************************************************************************/
/* 
 *  This file is an amalgamation of all the individual source code files for
 *  Embedthis Ejscript 1.0.4.
 *
 *  Catenating all the source into a single file makes embedding simpler and
 *  the resulting application faster, as many compilers can do whole file
 *  optimization.
 *
 *  If you want to modify ejs, you can still get the whole source
 *  as individual files if you need.
 */


/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsArray.c"
 */
/************************************************************************/

/**
 *  ejsArray.c - Ejscript Array class
 *
 *  This module implents the standard Array type. It provides the type methods and manages the special "length" property.
 *  The array elements with numeric indicies are stored in EjsArray.data[]. Non-numeric properties are stored in EjsArray.obj
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int  checkSlot(Ejs *ejs, EjsArray *ap, int slotNum);
static bool compareArrayElement(Ejs *ejs, EjsVar *v1, EjsVar *v2);
static int growArray(Ejs *ejs, EjsArray *ap, int len);
static int lookupArrayProperty(Ejs *ejs, EjsArray *ap, EjsName *qname);
static EjsVar *pushArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv);
static EjsVar *spliceArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv);
static EjsVar *arrayToString(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv);

#if BLD_FEATURE_EJS_LANG >= EJS_SPEC_PLUS
static EjsVar *makeIntersection(Ejs *ejs, EjsArray *lhs, EjsArray *rhs);
static EjsVar *makeUnion(Ejs *ejs, EjsArray *lhs, EjsArray *rhs);
static EjsVar *removeArrayElements(Ejs *ejs, EjsArray *lhs, EjsArray *rhs);
#endif

/*
 *  Create a new array
 */
static EjsArray *createArray(Ejs *ejs, EjsType *type, int numSlots)
{
    EjsArray     *ap;

    ap = (EjsArray*) ejsCreateObject(ejs, ejs->arrayType, 0);
    if (ap == 0) {
        return 0;
    }
    ap->length = 0;
    /*
     *  Clear isObject because we must NOT use direct slot access in the VM
     */ 
    ap->obj.var.isObject = 0;
    return ap;
}


/*
 *  Cast the object operand to a primitive type
 */

static EjsVar *castArray(Ejs *ejs, EjsArray *vp, EjsType *type)
{
    switch (type->id) {
    case ES_Boolean:
        return (EjsVar*) ejs->trueValue;

    case ES_Number:
        return (EjsVar*) ejs->zeroValue;

    case ES_String:
        return arrayToString(ejs, vp, 0, 0);

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
}


static EjsArray *cloneArray(Ejs *ejs, EjsArray *ap, bool deep)
{
    EjsArray    *newArray;
    EjsVar      **dest, **src;
    int         i;

    newArray = (EjsArray*) ejsCopyObject(ejs, (EjsObject*) ap, deep);
    if (newArray == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    if (ap->length > 0) {
        if (growArray(ejs, newArray, ap->length) < 0) {
            ejsThrowMemoryError(ejs);
            return 0;
        }
        src = ap->data;
        dest = newArray->data;

        if (deep) {
            for (i = 0; i < ap->length; i++) {
                dest[i] = ejsCloneVar(ejs, src[i], 1);
            }

        } else {
            memcpy(dest, src, ap->length * sizeof(EjsVar*));
        }
    }
    return newArray;
}


static void destroyArray(Ejs *ejs, EjsArray *ap)
{
    mprAssert(ap);

    mprFree(ap->data);
    ap->data = 0;
    ejsFreeVar(ejs, (EjsVar*) ap, -1);
}


/*
 *  Delete a property and update the length
 */
static int deleteArrayProperty(Ejs *ejs, EjsArray *ap, int slot)
{
    if (slot >= ap->length) {
        mprAssert(0);
        return EJS_ERR;
    }
    if (ejsSetProperty(ejs, (EjsVar*) ap, slot, (EjsVar*) ejs->undefinedValue) < 0) {
        return EJS_ERR;
    }
    if ((slot + 1) == ap->length) {
        ap->length--;
    }
    return 0;
}


/*
 *  Delete an element by name.
 */
static int deleteArrayPropertyByName(Ejs *ejs, EjsArray *ap, EjsName *qname)
{
    if (isdigit((int) qname->name[0])) {
        return deleteArrayProperty(ejs, ap, atoi(qname->name));
    }
    return (ejs->objectHelpers->deletePropertyByName)(ejs, (EjsVar*) ap, qname);
}


/*
 *  Return the number of elements in the array
 */
static int getArrayPropertyCount(Ejs *ejs, EjsArray *ap)
{
    return ap->length;
}


/*
 *  Get an array element. Slot numbers correspond to indicies.
 */
static EjsVar *getArrayProperty(Ejs *ejs, EjsArray *ap, int slotNum)
{
    if (slotNum < 0 || slotNum >= ap->length) {
        return ejs->undefinedValue;
    }
    return ap->data[slotNum];
}


static EjsVar *getArrayPropertyByName(Ejs *ejs, EjsArray *ap, EjsName *qname)
{
    int     slotNum;

    if (isdigit((int) qname->name[0])) { 
        slotNum = atoi(qname->name);
        if (slotNum < 0 || slotNum >= ap->length) {
            return 0;
        }
        return getArrayProperty(ejs, ap, slotNum);
    }

    /* The "length" property is a method getter */
    if (strcmp(qname->name, "length") == 0) {
        return 0;
    }
    slotNum = (ejs->objectHelpers->lookupProperty)(ejs, (EjsVar*) ap, qname);
    if (slotNum < 0) {
        return 0;
    }
    return (ejs->objectHelpers->getProperty)(ejs, (EjsVar*) ap, slotNum);
}


/*
 *  Lookup an array index.
 */
static int lookupArrayProperty(Ejs *ejs, EjsArray *ap, EjsName *qname)
{
    int     index;

    if (qname == 0 || !isdigit((int) qname->name[0])) {
        return EJS_ERR;
    }
    index = atoi(qname->name);
    if (index < ap->length) {
        return index;
    }

    return EJS_ERR;
}


/*
 *  Cast operands as required for invokeArrayOperator
 */
static EjsVar *coerceArrayOperands(Ejs *ejs, EjsVar *lhs, int opcode,  EjsVar *rhs)
{
    switch (opcode) {
    /*
     *  Binary operators
     */
    case EJS_OP_ADD:
        return ejsInvokeOperator(ejs, arrayToString(ejs, (EjsArray*) lhs, 0, 0), opcode, rhs);

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejs->zeroValue, opcode, rhs);

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_NE:
        if (ejsIsNull(rhs) || ejsIsUndefined(rhs)) {
            return (EjsVar*) ((opcode == EJS_OP_COMPARE_EQ) ? ejs->falseValue: ejs->trueValue);
        } else if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_STRICTLY_NE:
    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return 0;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }

    return 0;
}


static EjsVar *invokeArrayOperator(Ejs *ejs, EjsVar *lhs, int opcode,  EjsVar *rhs)
{
    EjsVar      *result;

    if (rhs == 0 || lhs->type != rhs->type) {
        if ((result = coerceArrayOperands(ejs, lhs, opcode, rhs)) != 0) {
            return result;
        }
    }

    switch (opcode) {

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_GE:
        return (EjsVar*) ejsCreateBoolean(ejs, (lhs == rhs));

    case EJS_OP_COMPARE_NE: case EJS_OP_COMPARE_STRICTLY_NE:
    case EJS_OP_COMPARE_LT: case EJS_OP_COMPARE_GT:
        return (EjsVar*) ejsCreateBoolean(ejs, !(lhs == rhs));

    /*
     *  Unary operators
     */
    case EJS_OP_COMPARE_NOT_ZERO:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return (EjsVar*) ejs->oneValue;

    /*
     *  Binary operators
     */
    case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_REM:
    case EJS_OP_SHR: case EJS_OP_USHR: case EJS_OP_XOR:
        return (EjsVar*) ejs->zeroValue;

#if BLD_FEATURE_EJS_LANG >= EJS_SPEC_PLUS
    /*
     *  Operator overload
     */
    case EJS_OP_ADD:
        result = (EjsVar*) ejsCreateArray(ejs, 0);
        pushArray(ejs, (EjsArray*) result, 1, (EjsVar**) &lhs);
        pushArray(ejs, (EjsArray*) result, 1, (EjsVar**) &rhs);
        return result;

    case EJS_OP_AND:
        return (EjsVar*) makeIntersection(ejs, (EjsArray*) lhs, (EjsArray*) rhs);

    case EJS_OP_OR:
        return (EjsVar*) makeUnion(ejs, (EjsArray*) lhs, (EjsArray*) rhs);

    case EJS_OP_SHL:
        return pushArray(ejs, (EjsArray*) lhs, 1, &rhs);

    case EJS_OP_SUB:
        return (EjsVar*) removeArrayElements(ejs, (EjsArray*) lhs, (EjsArray*) rhs);
#endif

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->type->qname.name);
        return 0;
    }

    mprAssert(0);
}


static void markArrayVar(Ejs *ejs, EjsVar *parent, EjsArray *ap)
{
    EjsVar          *vp;
    int             i;

    mprAssert(ejsIsArray(ap));

    ejsMarkObject(ejs, parent, (EjsObject*) ap);
    for (i = ap->length - 1; i >= 0; i--) {
        if ((vp = ap->data[i]) != 0) {
            ejsMarkVar(ejs, (EjsVar*) ap, vp);
        }
    }
}


/*
 *  Create or update an array elements. If slotNum is < 0, then create the next free array slot. If slotNum is greater
 *  than the array length, grow the array.
 */
static int setArrayProperty(Ejs *ejs, EjsArray *ap, int slotNum,  EjsVar *value)
{
    if ((slotNum = checkSlot(ejs, ap, slotNum)) < 0) {
        return EJS_ERR;
    }
    ap->data[slotNum] = value;
    return slotNum;
}


static int setArrayPropertyByName(Ejs *ejs, EjsArray *ap, EjsName *qname, EjsVar *value)
{
    int     slotNum;

    if (!isdigit((int) qname->name[0])) { 
        /* The "length" property is a method getter */
        if (strcmp(qname->name, "length") == 0) {
            return EJS_ERR;
        }
        slotNum = (ejs->objectHelpers->lookupProperty)(ejs, (EjsVar*) ap, qname);
        if (slotNum < 0) {
            slotNum = (ejs->objectHelpers->setProperty)(ejs, (EjsVar*) ap, slotNum, value);
            if (slotNum < 0) {
                return EJS_ERR;
            }
            if ((ejs->objectHelpers->setPropertyName)(ejs, (EjsVar*) ap, slotNum, qname) < 0) {
                return EJS_ERR;
            }
            return slotNum;

        } else {
            return (ejs->objectHelpers->setProperty)(ejs, (EjsVar*) ap, slotNum, value);
        }
    }

    if ((slotNum = checkSlot(ejs, ap, atoi(qname->name))) < 0) {
        return EJS_ERR;
    }
    ap->data[slotNum] = value;

    return slotNum;
}


#if BLD_FEATURE_EJS_LANG >= EJS_SPEC_PLUS
static EjsVar *makeIntersection(Ejs *ejs, EjsArray *lhs, EjsArray *rhs)
{
    EjsArray    *result;
    EjsVar      **l, **r, **resultSlots;
    int         i, j, k;

    result = ejsCreateArray(ejs, 0);
    l = lhs->data;
    r = rhs->data;

    for (i = 0; i < lhs->length; i++) {
        for (j = 0; j < rhs->length; j++) {
            if (compareArrayElement(ejs, l[i], r[j])) {
                resultSlots = result->data;
                for (k = 0; k < result->length; k++) {
                    if (compareArrayElement(ejs, l[i], resultSlots[k])) {
                        break;
                    }
                }
                if (result->length == 0 || k == result->length) {
                    setArrayProperty(ejs, result, -1, l[i]);
                }
            }
        }
    }
    return (EjsVar*) result;
}


static int addUnique(Ejs *ejs, EjsArray *ap, EjsVar *element)
{
    int     i;

    for (i = 0; i < ap->length; i++) {
        if (compareArrayElement(ejs, ap->data[i], element)) {
            break;
        }
    }
    if (i == ap->length) {
        if (setArrayProperty(ejs, ap, -1, element) < 0) {
            return EJS_ERR;
        }
    }
    return 0;
}


static EjsVar *makeUnion(Ejs *ejs, EjsArray *lhs, EjsArray *rhs)
{
    EjsArray    *result;
    EjsVar      **l, **r;
    int         i;

    result = ejsCreateArray(ejs, 0);

    l = lhs->data;
    r = rhs->data;

    for (i = 0; i < lhs->length; i++) {
        addUnique(ejs, result, l[i]);
    }
    for (i = 0; i < rhs->length; i++) {
        addUnique(ejs, result, r[i]);
    }
    return (EjsVar*) result;
}


static EjsVar *removeArrayElements(Ejs *ejs, EjsArray *lhs, EjsArray *rhs)
{
    EjsVar  **l, **r;
    int     i, j, k;

    l = lhs->data;
    r = rhs->data;

    for (j = 0; j < rhs->length; j++) {
        for (i = 0; i < lhs->length; i++) {
            if (compareArrayElement(ejs, l[i], r[j])) {
                for (k = i + 1; k < lhs->length; k++) {
                    l[k - 1] = l[k];
                }
                lhs->length--;
            }
        }
    }

    return (EjsVar*) lhs;
}
#endif


static int checkSlot(Ejs *ejs, EjsArray *ap, int slotNum)
{
    if (slotNum < 0) {
        if (!ap->obj.var.dynamic) {
            ejsThrowTypeError(ejs, "Object is not dynamic");
            return EJS_ERR;
        }
        slotNum = ap->length;
        if (growArray(ejs, ap, ap->length + 1) < 0) {
            ejsThrowMemoryError(ejs);
            return EJS_ERR;
        }

    } else if (slotNum >= ap->length) {
        if (growArray(ejs, ap, slotNum + 1) < 0) {
            ejsThrowMemoryError(ejs);
            return EJS_ERR;
        }
    }
    return slotNum;
}


/*
 *  Array constructor.
 *
 *  function Array(...args): Array
 *
 *  Support the forms:
 *
 *      var arr = Array();
 *      var arr = Array(size);
 *      var arr = Array(elt, elt, elt, ...);
 */

static EjsVar *arrayConstructor(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsArray    *args;
    EjsVar      *arg0, **src, **dest;
    int         size, i;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = (EjsArray*) argv[0];
    
    if (args->length == 0) {
        return 0;
    }

    size = 0;
    arg0 = getArrayProperty(ejs, args, 0);

    if (args->length == 1 && ejsIsNumber(arg0)) {
        /*
         *  x = new Array(size);
         */
        size = ejsGetInt(arg0);
        if (size > 0 && growArray(ejs, ap, size) < 0) {
            ejsThrowMemoryError(ejs);
            return 0;
        }

    } else {

        /*
         *  x = new Array(element0, element1, ..., elementN):
         */
        size = args->length;
        if (size > 0 && growArray(ejs, ap, size) < 0) {
            ejsThrowMemoryError(ejs);
            return 0;
        }

        src = args->data;
        dest = ap->data;
        for (i = 0; i < size; i++) {
            dest[i] = src[i];
        }
    }
    ap->length = size;

    return (EjsVar*) ap;
}


/*
 *  Append an item to an array
 *
 *  function append(obj: Object) : Array
 */
static EjsVar *appendArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    if (setArrayProperty(ejs, ap, ap->length, argv[0]) < 0) {
        return 0;
    }
    return (EjsVar*) ap;
}


/*
 *  Clear an array. Remove all elements of the array.
 *
 *  function clear() : void
 */
static EjsVar *clearArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    ap->length = 0;
    return 0;
}


/*
 *  Clone an array.
 *
 *  function clone(deep: Boolean = false) : Array
 */
static EjsArray *cloneArrayMethod(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    bool    deep;

    mprAssert(argc == 0 || ejsIsBoolean(argv[0]));

    deep = (argc == 1) ? ((EjsBoolean*) argv[0])->value : 0;

    return cloneArray(ejs, ap, deep);
}


/*
 *  Compact an array. Remove all null elements.
 *
 *  function compact() : Array
 */
static EjsArray *compactArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsVar      **data, **src, **dest;
    int         i;

    data = ap->data;
    src = dest = &data[0];
    for (i = 0; i < ap->length; i++, src++) {
        if (*src == 0 || *src == ejs->undefinedValue || *src == ejs->nullValue) {
            continue;
        }
        *dest++ = *src;
    }

    ap->length = (int) (dest - &data[0]);
    return ap;
}


/*
 *  Concatenate the supplied elements with the array to create a new array. If any arguments specify an array,
 *  their elements are catenated. This is a one level deep copy.
 *
 *  function concat(...args): Array
 */
static EjsVar *concatArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsArray    *args, *newArray, *vpa;
    EjsVar      *vp, **src, **dest;
    int         i, k, next;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = ((EjsArray*) argv[0]);

    newArray = ejsCreateArray(ejs, ap->length);
    src = ap->data;
    dest = newArray->data;

    /*
     *  Copy the original array
     */
    for (next = 0; next < ap->length; next++) {
        dest[next] = src[next];
    }

    /*
     *  Copy the args. If any element is itself an array, then flatten it and copy its elements.
     */
    for (i = 0; i < args->length; i++) {
        vp = args->data[i];
        if (ejsIsArray(vp)) {
            vpa = (EjsArray*) vp;
            if (growArray(ejs, newArray, next + vpa->length) < 0) {
                ejsThrowMemoryError(ejs);
                return 0;
            }
            dest = newArray->data;
            dest = newArray->data;
            for (k = 0; k < vpa->length; k++) {
                dest[next++] = vpa->data[k];
            }
        } else {
            if (growArray(ejs, newArray, next + vpa->length) < 0) {
                ejsThrowMemoryError(ejs);
                return 0;
            }
            dest[next++] = vp;
        }
    }
    return (EjsVar*) newArray;
}


/*
 *  Function to iterate and return the next element name.
 *  NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextArrayKey(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsArray        *ap;
    EjsVar          *vp, **data;

    ap = (EjsArray*) ip->target;
    if (!ejsIsArray(ap)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }
    data = ap->data;

    for (; ip->index < ap->length; ip->index++) {
        vp = data[ip->index];
        if (vp == 0) {
            continue;
        }
        return (EjsVar*) ejsCreateNumber(ejs, ip->index++);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return the default iterator. This returns the array index names.
 *
 *  iterator native function get(): Iterator
 */
static EjsVar *getArrayIterator(Ejs *ejs, EjsVar *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, ap, (EjsNativeFunction) nextArrayKey, 0, NULL);
}


/*
 *  Function to iterate and return the next element value.
 *  NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextArrayValue(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsArray    *ap;
    EjsVar      *vp, **data;

    ap = (EjsArray*) ip->target;
    if (!ejsIsArray(ap)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    data = ap->data;
    for (; ip->index < ap->length; ip->index++) {
        vp = data[ip->index];
        if (vp == 0) {
            continue;
        }
        ip->index++;
        return vp;
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return an iterator to return the next array element value.
 *
 *  iterator native function getValues(): Iterator
 */
static EjsVar *getArrayValues(Ejs *ejs, EjsVar *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, ap, (EjsNativeFunction) nextArrayValue, 0, NULL);
}


#if KEEP
static EjsVar *find(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    return 0;
}


/**
 *  Iterate over all elements in the object and find all elements for which the matching function is true.
 *  The match is called with the following signature:
 *
 *      function match(arrayElement: Object, elementIndex: Number, arr: Array): Boolean
 *
 *  @param match Matching function
 *  @return Returns a new array containing all matching elements.
 */
static EjsVar *findAll(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsVar      *funArgs[3];
    EjsBoolean  *result;
    EjsArray    *elements;
    int         i;

    mprAssert(argc == 1 && ejsIsFunction(argv[0]));

    elements = ejsCreateArray(ejs, 0);
    if (elements == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    for (i = 0; i < ap->length; i++) {
        funArgs[0] = ap->obj.properties.slots[i];               /* Array element */
        funArgs[1] = (EjsVar*) ejsCreateNumber(ejs, i);             /* element index */
        funArgs[2] = (EjsVar*) ap;                                  /* Array */
        result = (EjsBoolean*) ejsRunFunction(ejs, (EjsFunction*) argv[0], 0, 3, funArgs);
        if (result == 0 || !ejsIsBoolean(result) || !result->value) {
            setArrayProperty(ejs, elements, elements->length, ap->obj.properties.slots[i]);
        }
    }
    return (EjsVar*) elements;
}
#endif


static bool compareArrayElement(Ejs *ejs, EjsVar *v1, EjsVar *v2)
{
    if (v1 == v2) {
        return 1;
    }
    if (v1->type != v2->type) {
        return 0;
    }
    if (ejsIsNumber(v1)) {
        return ((EjsNumber*) v1)->value == ((EjsNumber*) v2)->value;
    }
    if (ejsIsString(v1)) {
        return strcmp(((EjsString*) v1)->value, ((EjsString*) v2)->value) == 0;
    }
    return 0;
}


/*
 *  Search for an item using strict equality "===". This call searches from
 *  the start of the array for the specified element.
 *  @return Returns the items index into the array if found, otherwise -1.
 *
 *  function indexOf(element: Object, startIndex: Number = 0): Number
 */
static EjsVar *indexOfArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsVar          *element;
    int             i, start;

    mprAssert(argc == 1 || argc == 2);

    element = argv[0];
    start = (argc == 2) ? (int) ((EjsNumber*) argv[1])->value : 0;

    if (start < 0) {
        start += ap->length;
    }
    if (start >= ap->length) {
        return (EjsVar*) ejs->minusOneValue;
    }
    if (start < 0) {
        start = 0;
    }
    for (i = start; i < ap->length; i++) {
        if (compareArrayElement(ejs, ap->data[i], element)) {
            return (EjsVar*) ejsCreateNumber(ejs, i);
        }
    }
    return (EjsVar*) ejs->minusOneValue;
}


/*
 *  Insert elements. Insert elements at the specified position. Negative indicies are measured from the end of the array.
 *  @return Returns a the original array.
 *
 *  function insert(pos: Number, ...args): Array
 */
static EjsVar *insertArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsArray    *args;
    EjsVar      **src, **dest;
    int         i, pos, delta, oldLen, endInsert;

    mprAssert(argc == 2 && ejsIsArray(argv[1]));

    pos = ejsGetInt(argv[0]);
    if (pos < 0) {
        pos += ap->length;
    }
    if (pos < 0) {
        pos = 0;
    }
    if (pos >= ap->length) {
        pos = ap->length;
    }
    args = (EjsArray*) argv[1];
    if (args->length <= 0) {
        return (EjsVar*) ap;
    }
    oldLen = ap->length;
    if (growArray(ejs, ap, ap->length + args->length) < 0) {
        return 0;
    }
    delta = args->length;
    dest = ap->data;
    src = args->data;

    endInsert = pos + delta;
    for (i = ap->length - 1; i >= endInsert; i--) {
        dest[i] = dest[i - delta];
    }
    for (i = 0; i < delta; i++) {
        dest[pos++] = src[i];
    }

    return (EjsVar*) ap;
}


/*
 *  Joins the elements in the array into a single string.
 *  @param sep Element separator.
 *  @return Returns a string.
 *
 *  function join(sep: String = undefined): String
 */
static EjsVar *joinArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsString       *result, *sep;
    EjsVar          *vp;
    int             i;

    if (argc == 1) {
        sep = (EjsString*) argv[0];
    } else {
        sep = 0;
    }

    result = ejsCreateString(ejs, "");
    for (i = 0; i < ap->length; i++) {
        vp = ap->data[i];
        if (vp == 0 || ejsIsUndefined(vp) || ejsIsNull(vp)) {
            continue;
        }
        if (i > 0 && sep) {
            ejsStrcat(ejs, result, (EjsVar*) sep);
        }
        ejsStrcat(ejs, result, vp);
    }
    return (EjsVar*) result;
}


/*
 *  Search for an item using strict equality "===". This call searches from
 *  the end of the array for the specified element.
 *  @return Returns the items index into the array if found, otherwise -1.
 *
 *  function lastIndexOf(element: Object, fromIndex: Number = -1): Number
 */
static EjsVar *lastArrayIndexOf(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsVar          *element;
    int             i, start;

    mprAssert(argc == 1 || argc == 2);

    element = argv[0];
    start = ((argc == 2) ? (int) ((EjsNumber*) argv[1])->value : ap->length - 1);

    if (start < 0) {
        start += ap->length;
    }
    if (start >= ap->length) {
        start = ap->length - 1;
    }
    if (start < 0) {
        return (EjsVar*) ejs->minusOneValue;
    }
    for (i = start; i >= 0; i--) {
        if (compareArrayElement(ejs, ap->data[i], element)) {
            return (EjsVar*) ejsCreateNumber(ejs, i);
        }
    }
    return (EjsVar*) ejs->minusOneValue;
}


/*
 *  Get the length of an array.
 *  @return Returns the number of items in the array
 *
 *  intrinsic override function get length(): Number
 */

static EjsVar *getArrayLength(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ap->length);
}


/*
 *  Set the length of an array.
 *
 *  intrinsic override function set length(value: Number): void
 */

static EjsVar *setArrayLength(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsVar      **data, **dest;
    int         length;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));
    mprAssert(ejsIsArray(ap));

    length = (int) ((EjsNumber*) argv[0])->value;
    if (length < 0) {
        length = 0;
    }
    if (length > ap->length) {
        if (growArray(ejs, ap, length) < 0) {
            return 0;
        }
        data = ap->data;
        for (dest = &data[ap->length]; dest < &data[length]; dest++) {
            *dest = 0;
        }
    }
    ap->length = length;
    return 0;
}


/*
 *  Remove and return the last value in the array.
 *  @return Returns the last element in the array.
 *
 *  function pop(): Object
 */
static EjsVar *popArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    if (ap->length == 0) {
        return (EjsVar*) ejs->undefinedValue;
    }
    return ap->data[--ap->length];
}


/*
 *  Append items to the end of the array.
 *  @return Returns the new length of the array.
 *
 *  function push(...items): Number
 */
static EjsVar *pushArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsArray    *args;
    EjsVar      **src, **dest;
    int         i, oldLen;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = (EjsArray*) argv[0];

    oldLen = ap->length;
    if (growArray(ejs, ap, ap->length + args->length) < 0) {
        return 0;
    }
    dest = ap->data;
    src = args->data;
    for (i = 0; i < args->length; i++) {
        dest[i + oldLen] = src[i];
    }
    return (EjsVar*) ejsCreateNumber(ejs, ap->length);
}


/*
 *  Reverse the order of the objects in the array. The elements are reversed in the original array.
 *  @return Returns a reference to the array.
 *
 *  function reverse(): Array
 */
static EjsVar *reverseArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsVar  *tmp, **data;
    int     i, j;

    if (ap->length <= 1) {
        return (EjsVar*) ap;
    }

    data = ap->data;
    i = (ap->length - 2) / 2;
    j = (ap->length + 1) / 2;

    for (; i >= 0; i--, j++) {
        tmp = data[i];
        data[i] = data[j];
        data[j] = tmp;
    }
    return (EjsVar*) ap;
}


/*
 *  Remove and return the first value in the array.
 *  @return Returns the first element in the array.
 *
 *  function shift(): Object
 */
static EjsVar *shiftArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsVar      *result, **data;
    int         i;

    if (ap->length == 0) {
        return ejs->undefinedValue;
    }

    data = ap->data;
    result = data[0];

    for (i = 1; i < ap->length; i++) {
        data[i - 1] = data[i];
    }
    ap->length--;

    return result;
}


/*
 *  Create a new array by taking a slice from an array.
 *
 *  function slice(start: Number, end: Number, step: Number = 1): Array
 */
static EjsVar *sliceArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsArray    *result;
    EjsVar      **src, **dest;
    int         start, end, step, i, j, len, size;

    mprAssert(1 <= argc && argc <= 3);

    start = ejsGetInt(argv[0]);
    if (argc >= 2) {
        end = ejsGetInt(argv[1]);
    } else {
        end = ap->length;
    }
    if (argc == 3) {
        step = ejsGetInt(argv[2]);
    } else {
        step = 1;
    }
    if (step == 0) {
        step = 1;
    }

    if (start < 0) {
        start += ap->length;
    }
    if (start < 0) {
        start = 0;
    } else if (start >= ap->length) {
        start = ap->length;
    }

    if (end < 0) {
        end += ap->length;
    }
    if (end < 0) {
        end = 0;
    } else if (end >= ap->length) {
        end = ap->length;
    }
    size = (start < end) ? end - start : start - end;

    /*
     *  This may allocate too many elements if abs(step) is > 1, but length will still be correct.
     */
    result = ejsCreateArray(ejs, size);
    if (result == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    src = ap->data;
    dest = result->data;

    len = 0;
    if (step > 0) {
        for (i = start, j = 0; i < end; i += step, j++) {
            dest[j] = src[i];
            len++;
        }

    } else {
        for (i = start, j = 0; i > end; i += step, j++) {
            dest[j] = src[i];
            len++;
        }
    }
    result->length = len;
    return (EjsVar*) result;
}


static int partition(Ejs *ejs, EjsVar **data, int p, int r)
{
    EjsVar          *tmp, *x;
    EjsString       *sx, *so;
    int             i, j;

    x = data[r];
    j = p - 1;

    for (i = p; i < r; i++) {

        sx = ejsToString(ejs, x);
        so = ejsToString(ejs, data[i]);
        if (sx == 0 || so == 0) {
            return 0;
        }
        if (strcmp(sx->value, so->value) > 0) {
            j = j + 1;
            tmp = data[j];
            data[j] = data[i];
            data[i] = tmp;
        }
    }
    data[r] = data[j + 1];
    data[j + 1] = x;
    return j + 1;
}


void quickSort(Ejs *ejs, EjsArray *ap, int p, int r)
{
    int     q;

    if (p < r) {
        q = partition(ejs, ap->data, p, r);
        quickSort(ejs, ap, p, q - 1);
        quickSort(ejs, ap, q + 1, r);
    }
}


/**
 *  Sort the array using the supplied compare function
 *  intrinsic native function sort(compare: Function = null, order: Number = 1): Array
 */
static EjsVar *sortArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    if (ap->length <= 1) {
        return (EjsVar*) ap;
    }
    quickSort(ejs, ap, 0, ap->length - 1);
    return (EjsVar*) ap;
}


/*
 *  Insert, remove or replace array elements. Return the removed elements.
 *
 *  function splice(start: Number, deleteCount: Number, ...values): Array
 *
 */
static EjsVar *spliceArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsArray    *result, *values;
    EjsVar      **data, **dest, **items;
    int         start, deleteCount, i, delta, endInsert, oldLen;

    mprAssert(1 <= argc && argc <= 3);
    
    start = ejsGetInt(argv[0]);
    deleteCount = ejsGetInt(argv[1]);
    values = (EjsArray*) argv[2];

    if (ap->length == 0) {
        if (deleteCount <= 0) {
            return (EjsVar*) ap;
        }
        ejsThrowArgError(ejs, "Array is empty");
        return 0;
    }
    if (start < 0) {
        start += ap->length;
    }
    if (start < 0) {
        start = 0;
    }
    if (start >= ap->length) {
        start = ap->length - 1;
    }

    if (deleteCount < 0) {
        deleteCount = ap->length - start + 1;
    }
    if (deleteCount > ap->length) {
        deleteCount = ap->length;
    }

    result = ejsCreateArray(ejs, deleteCount);
    if (result == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    data = ap->data;
    dest = result->data;
    items = values->data;

    /*
     *  Copy removed items to the result
     */
    for (i = 0; i < deleteCount; i++) {
        dest[i] = data[i + start];
    }

    oldLen = ap->length;
    delta = values->length - deleteCount;
    
    if (delta > 0) {
        /*
         *  Make room for items to insert
         */
        if (growArray(ejs, ap, ap->length + delta) < 0) {
            return 0;
        }
        data = ap->data;
        endInsert = start + delta;
        for (i = ap->length - 1; i >= endInsert; i--) {
            data[i] = data[i - delta];
        }
        
    } else {
        ap->length += delta;
    }

    /*
     *  Copy in new values
     */
    for (i = 0; i < values->length; i++) {
        data[start + i] = items[i];
    }

    /*
     *  Remove holes
     */
    if (delta < 0) {
        for (i = start + values->length; i < oldLen; i++) {
            data[i] = data[i - delta];
        }
    }
    return (EjsVar*) result;
}



#if ES_Object_toLocaleString
/*
 *  Convert the array to a single localized string each member of the array
 *  has toString called on it and the resulting strings are concatenated.
 *  Currently just calls toString.
 *
 *  function toLocaleString(): String
 */
static EjsVar *toLocaleString(Ejs *ejs, EjsVar *ap, int argc, EjsVar **argv)
{
    return 0;
}
#endif


/*
 *  Convert the array to a single string each member of the array has toString called on it and the resulting strings 
 *  are concatenated.
 *
 *  override function toString(): String
 */
static EjsVar *arrayToString(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsString       *result;
    EjsVar          *vp;
    int             i, rc;

    result = ejsCreateString(ejs, "");
    if (result == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    for (i = 0; i < ap->length; i++) {
        vp = ap->data[i];
        rc = 0;
        if (i > 0) {
            rc = ejsStrcat(ejs, result, (EjsVar*) ejsCreateString(ejs, ","));
        }
        if (vp != 0 && vp != ejs->undefinedValue && vp != ejs->nullValue) {
            rc = ejsStrcat(ejs, result, vp);
        }
        if (rc < 0) {
            ejsThrowMemoryError(ejs);
            return 0;
        }
    }
    return (EjsVar*) result;
}


/*
 *  Return an array with duplicate elements removed where duplicates are detected by using "==" (ie. content equality, 
 *  not strict equality).
 *
 *  function unique(): Array
 */
static EjsVar *uniqueArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsVar      **data;
    int         i, j, k;

    data = ap->data;

    for (i = 0; i < ap->length; i++) {
        for (j = i + 1; j < ap->length; j++) {
            if (compareArrayElement(ejs, data[i], data[j])) {
                for (k = j + 1; k < ap->length; k++) {
                    data[k - 1] = data[k];
                }
                ap->length--;
                j--;
            }
        }
    }
    return (EjsVar*) ap;
}


/*
 *  function unshift(...args): Array
 */
static EjsVar *unshiftArray(Ejs *ejs, EjsArray *ap, int argc, EjsVar **argv)
{
    EjsArray    *args;
    EjsVar      **src, **dest;
    int         i, delta, oldLen, endInsert;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = (EjsArray*) argv[0];
    if (args->length <= 0) {
        return (EjsVar*) ap;
    }
    oldLen = ap->length;
    if (growArray(ejs, ap, ap->length + args->length) < 0) {
        return 0;
    }
    delta = args->length;
    dest = ap->data;
    src = args->data;

    endInsert = delta;
    for (i = ap->length - 1; i >= endInsert; i--) {
        dest[i] = dest[i - delta];
    }
    for (i = 0; i < delta; i++) {
        dest[i] = src[i];
    }
    return (EjsVar*) ap;
}



static int growArray(Ejs *ejs, EjsArray *ap, int len)
{
    int         size, count, factor;

    mprAssert(ap);

    if (len <= 0) {
        return 0;
    }
    if (len <= ap->length) {
        return 0;
    }

    size = mprGetBlockSize(ap->data) / sizeof(EjsVar*);

    /*
     *  Allocate or grow the data structures.
     */
    if (len > size) {
        if (size > EJS_LOTSA_PROP) {
            /*
             *  Looks like a big object so grow by a bigger chunk
             */
            factor = max(size / 4, EJS_NUM_PROP);
            count = (len + factor) / factor * factor;
        } else {
            count = len;
        }
        count = EJS_PROP_ROUNDUP(count);
        if (ap->data == 0) {
            mprAssert(ap->length == 0);
            mprAssert(count > 0);
            ap->data = (EjsVar**) mprAllocZeroed(ap, sizeof(EjsVar*) * count);
            if (ap->data == 0) {
                return EJS_ERR;
            }

        } else {
            mprAssert(size > 0);
            ap->data = (EjsVar**) mprRealloc(ap, ap->data, sizeof(EjsVar*) * count);
            if (ap->data == 0) {
                return EJS_ERR;
            }
            ejsZeroSlots(ejs, &ap->data[ap->length], (count - ap->length));
        }
    }
    ap->length = len;
    return 0;
}



EjsArray *ejsCreateArray(Ejs *ejs, int size)
{
    EjsArray    *ap;

    /*
     *  No need to invoke constructor
     */
    ap = (EjsArray*) ejsCreateObject(ejs, ejs->arrayType, 0);
    if (ap != 0) {
        ap->length = 0;
        if (size > 0 && growArray(ejs, ap, size) < 0) {
            ejsThrowMemoryError(ejs);
            return 0;
        }
    }
    ejsSetDebugName(ap, "array instance");
    return ap;
}


void ejsCreateArrayType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Array"), ejs->objectType, sizeof(EjsArray),
        ES_Array, ES_Array_NUM_CLASS_PROP, ES_Array_NUM_INSTANCE_PROP,
        EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_DYNAMIC_INSTANCE | EJS_ATTR_HAS_CONSTRUCTOR | 
        EJS_ATTR_OBJECT_HELPERS);
    ejs->arrayType = type;

    /*
     *  Define the helper functions.
     */
    type->helpers->castVar = (EjsCastVarHelper) castArray;
    type->helpers->cloneVar = (EjsCloneVarHelper) cloneArray;
    type->helpers->createVar = (EjsCreateVarHelper) createArray;
    type->helpers->destroyVar = (EjsDestroyVarHelper) destroyArray;
    type->helpers->getProperty = (EjsGetPropertyHelper) getArrayProperty;
    type->helpers->getPropertyCount = (EjsGetPropertyCountHelper) getArrayPropertyCount;
    type->helpers->getPropertyByName = (EjsGetPropertyByNameHelper) getArrayPropertyByName;
    type->helpers->deleteProperty = (EjsDeletePropertyHelper) deleteArrayProperty;
    type->helpers->deletePropertyByName = (EjsDeletePropertyByNameHelper) deleteArrayPropertyByName;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokeArrayOperator;
    type->helpers->lookupProperty = (EjsLookupPropertyHelper) lookupArrayProperty;
    type->helpers->markVar = (EjsMarkVarHelper) markArrayVar;
    type->helpers->setProperty = (EjsSetPropertyHelper) setArrayProperty;
    type->helpers->setPropertyByName = (EjsSetPropertyByNameHelper) setArrayPropertyByName;
    type->numericIndicies = 1;
}


void ejsConfigureArrayType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->arrayType;

    /*
     *  We override some object methods
     */
    ejsBindMethod(ejs, type, ES_Object_get, getArrayIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, getArrayValues);
    ejsBindMethod(ejs, type, ES_Object_clone, (EjsNativeFunction) cloneArrayMethod);
    ejsBindMethod(ejs, type, ES_Object_toString, (EjsNativeFunction) arrayToString);
    ejsBindMethod(ejs, type, ES_Object_length, (EjsNativeFunction) getArrayLength);
    ejsBindMethod(ejs, type, ES_Array_set_length, (EjsNativeFunction) setArrayLength);

    /*
     *  Methods and Operators, including constructor.
     */
    ejsBindMethod(ejs, type, ES_Array_Array, (EjsNativeFunction) arrayConstructor);
    ejsBindMethod(ejs, type, ES_Array_append, (EjsNativeFunction) appendArray);
    ejsBindMethod(ejs, type, ES_Array_clear, (EjsNativeFunction) clearArray);
    ejsBindMethod(ejs, type, ES_Array_compact, (EjsNativeFunction) compactArray);
    ejsBindMethod(ejs, type, ES_Array_concat, (EjsNativeFunction) concatArray);

    ejsBindMethod(ejs, type, ES_Array_indexOf, (EjsNativeFunction) indexOfArray);
    ejsBindMethod(ejs, type, ES_Array_insert, (EjsNativeFunction) insertArray);
    ejsBindMethod(ejs, type, ES_Array_join, (EjsNativeFunction) joinArray);
    ejsBindMethod(ejs, type, ES_Array_lastIndexOf, (EjsNativeFunction) lastArrayIndexOf);
    ejsBindMethod(ejs, type, ES_Array_pop, (EjsNativeFunction) popArray);
    ejsBindMethod(ejs, type, ES_Array_push, (EjsNativeFunction) pushArray);
    ejsBindMethod(ejs, type, ES_Array_reverse, (EjsNativeFunction) reverseArray);
    ejsBindMethod(ejs, type, ES_Array_shift, (EjsNativeFunction) shiftArray);
    ejsBindMethod(ejs, type, ES_Array_slice, (EjsNativeFunction) sliceArray);
    ejsBindMethod(ejs, type, ES_Array_sort, (EjsNativeFunction) sortArray);
    ejsBindMethod(ejs, type, ES_Array_splice, (EjsNativeFunction) spliceArray);
    ejsBindMethod(ejs, type, ES_Array_unique, (EjsNativeFunction) uniqueArray);
    ejsBindMethod(ejs, type, ES_Array_unshift, (EjsNativeFunction) unshiftArray);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsArray.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsBlock.c"
 */
/************************************************************************/

/**
 *  ejsBlock.c - Lexical block
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int growTraits(EjsBlock *block, int numTraits);
static int insertGrowTraits(EjsBlock *block, int count, int offset);


EjsBlock *ejsCreateBlock(Ejs *ejs, int size)
{
    EjsBlock        *block;

    block = (EjsBlock*) ejsCreateObject(ejs, ejs->blockType, size);
    if (block == 0) {
        return 0;
    }
    ejsInitList(&block->namespaces);
    return block;
}


/*
 *  Define a new property and set its name, type, attributes and property value.
 */
static int defineBlockProperty(Ejs *ejs, EjsBlock *block, int slotNum, EjsName *qname, EjsType *propType, int attributes, 
    EjsVar *val)
{
    EjsFunction     *fun;
    EjsType         *type;

    mprAssert(ejs);
    mprAssert(slotNum >= -1);
    mprAssert(ejsIsObject(block));
    mprAssert(qname);

    if (val == 0) {
        val = ejs->nullValue;
    }

    if (slotNum < 0) {
        slotNum = ejsGetPropertyCount(ejs, (EjsVar*) block);
    }
    if (ejsSetProperty(ejs, (EjsVar*) block, slotNum, val) < 0) {
        return EJS_ERR;
    }
    if (ejsSetPropertyName(ejs, (EjsVar*) block, slotNum, qname) < 0) {
        return EJS_ERR;
    }
    if (ejsSetPropertyTrait(ejs, (EjsVar*) block, slotNum, propType, attributes) < 0) {
        return EJS_ERR;
    }
    if (ejsIsFunction(val)) {
        fun = ((EjsFunction*) val);
        if (attributes & EJS_ATTR_CONSTRUCTOR) {
            fun->constructor = 1;
        }
        ejsSetFunctionLocation(fun, (EjsVar*) block, slotNum);
        if (fun->getter || fun->setter) {
             block->obj.var.hasGetterSetter = 1;
        }
        if (!ejsIsNativeFunction(fun)) {
            block->hasScriptFunctions = 1;
        }
        if (fun->staticMethod && ejsIsType(block)) {
            type = (EjsType*) block;
            if (!type->isInterface) {
                /*
                 *  For static methods, find the right base class and set thisObj to speed up later invocations.
                 */
                fun->thisObj = (EjsVar*) block;
            } else {
                mprAssert(0);
            }
        }
    }
    return slotNum;
}


/*
 *  Get the property Trait
 */
static EjsTrait *getBlockPropertyTrait(Ejs *ejs, EjsBlock *block, int slotNum)
{
    return ejsGetTrait(block, slotNum);
}


void ejsMarkBlock(Ejs *ejs, EjsVar *parent, EjsBlock *block)
{
    EjsVar          *item;
    EjsBlock        *b;
    int             next;

    ejsMarkObject(ejs, parent, (EjsObject*) block);
    if (block->prevException) {
        ejsMarkVar(ejs, (EjsVar*) block, (EjsVar*) block->prevException);
    }
    if (block->namespaces.length > 0) {
        for (next = 0; ((item = (EjsVar*) ejsGetNextItem(&block->namespaces, &next)) != 0); ) {
            ejsMarkVar(ejs, (EjsVar*) block, item);
        }
    }
    for (b = block->scopeChain; b; b = b->scopeChain) {
        ejsMarkVar(ejs, (EjsVar*) block, (EjsVar*) b);
    }
    for (b = block->prev; b; b = b->prev) {
        ejsMarkVar(ejs, (EjsVar*) block, (EjsVar*) b);
    }
}


/*
 *  Set the property Trait
 */
static int setBlockPropertyTrait(Ejs *ejs, EjsBlock *block, int slotNum, EjsType *type, int attributes)
{
    return ejsSetTrait(block, slotNum, type, attributes);
}


/*
 *  Grow the block traits, slots and names. This will update numTraits and numProp.
 */
int ejsGrowBlock(Ejs *ejs, EjsBlock *block, int size)
{
    if (size == 0) {
        return 0;
    }
    if (ejsGrowObject(ejs, (EjsObject*) block, size) < 0) {
        return EJS_ERR;
    }
    if (growTraits(block, size) < 0) {
        return EJS_ERR;
    }
    return 0;
}


/*
 *  Grow the block traits, slots and names by inserting before all existing properties. This will update numTraits and 
 *  numProp.
 */
int ejsInsertGrowBlock(Ejs *ejs, EjsBlock *block, int count, int offset)
{
    EjsFunction     *fun;
    int             i;

    if (count <= 0) {
        return 0;
    }
    if (ejsInsertGrowObject(ejs, (EjsObject*) block, count, offset) < 0) {
        return EJS_ERR;
    }
    if (insertGrowTraits(block, count, offset) < 0) {
        return EJS_ERR;
    }

    /*
     *  Fixup the slot numbers of all the methods.
     */
    for (i = offset + count; i < block->numTraits; i++) {
        fun = (EjsFunction*) block->obj.slots[i];
        if (fun == 0 || !ejsIsFunction(fun)) {
            continue;
        }
        fun->slotNum += count;
        if (fun->nextSlot >= 0) {
            fun->nextSlot += count;
        }
        mprAssert(fun->slotNum == i);
        mprAssert(fun->nextSlot < block->numTraits);
    }
    return 0;
}


/*
 *  Allocate space for traits. Caller will initialize the actual traits.
 */
static int growTraits(EjsBlock *block, int numTraits)
{
    int         count;

    mprAssert(block);
    mprAssert(numTraits >= 0);

    if (numTraits == 0) {
        return 0;
    }
    if (numTraits > block->sizeTraits) {
        count = EJS_PROP_ROUNDUP(numTraits);
        block->traits = (EjsTrait*) mprRealloc(block, block->traits, sizeof(EjsTrait) * count);
        if (block->traits == 0) {
            return EJS_ERR;
        }
        memset(&block->traits[block->sizeTraits], 0, (count - block->sizeTraits) * sizeof(EjsTrait));
        block->sizeTraits = count;
    }
    if (numTraits > block->numTraits) {
        block->numTraits = numTraits;
    }
    mprAssert(block->numTraits <= block->sizeTraits);
    return 0;
}


static int insertGrowTraits(EjsBlock *block, int count, int offset)
{
    int         mark, i;

    mprAssert(block);
    mprAssert(count >= 0);

    if (count == 0) {
        return 0;
    }

    /*
     *  This call will change both numTraits and sizeTraits
     */
    growTraits(block, block->numTraits + count);

    mark = offset + count ;
    for (i = block->numTraits - 1; i >= mark; i--) {
        block->traits[i] = block->traits[i - mark];
    }
    for (; i >= offset; i--) {
        block->traits[i].attributes = 0;
        block->traits[i].type = 0;
    }
    mprAssert(block->numTraits <= block->sizeTraits);
    return 0;
}


/*
 *  Add a new trait to the trait array.
 */
int ejsSetTrait(EjsBlock *block, int slotNum, EjsType *type, int attributes)
{
    mprAssert(block);
    mprAssert(slotNum >= 0);

    if (slotNum < 0 || slotNum >= block->obj.capacity) {
        return EJS_ERR;
    }

    mprAssert(block->numTraits <= block->sizeTraits);

    if (block->sizeTraits <= slotNum) {
        growTraits(block, slotNum + 1);
    } else if (block->numTraits <= slotNum) {
        block->numTraits = slotNum + 1;
    }
    
    block->traits[slotNum].type = type;
    block->traits[slotNum].attributes = attributes;
    
    mprAssert(block->numTraits <= block->sizeTraits);
    mprAssert(slotNum < block->sizeTraits);

    return slotNum;
}


void ejsSetTraitType(EjsTrait *trait, EjsType *type)
{
    mprAssert(trait);
    mprAssert(type == 0 || ejsIsType(type));

    trait->type = type;
}


void ejsSetTraitAttributes(EjsTrait *trait, int attributes)
{
    mprAssert(trait);

    trait->attributes = attributes;
}


/*
 *  Remove the designated slot. If compact is true, then copy slots down.
 */
static int removeTrait(EjsBlock *block, int slotNum, int compact)
{
    int         i;

    mprAssert(block);
    mprAssert(block->numTraits <= block->sizeTraits);
    
    if (slotNum < 0 || slotNum >= block->numTraits) {
        return EJS_ERR;
    }

    if (compact) {
        /*
         *  Copy traits down starting at
         */
        for (i = slotNum + 1; i < block->numTraits; i++) {
            block->traits[i - 1] = block->traits[i];
        }
        block->numTraits--;
        i--;

    } else {
        i = slotNum;
    }

    mprAssert(i >= 0);
    mprAssert(i < block->sizeTraits);

    block->traits[i].attributes = 0;
    block->traits[i].type = 0;

    if ((i - 1) == block->numTraits) {
        block->numTraits--;
    }
    mprAssert(slotNum < block->sizeTraits);
    mprAssert(block->numTraits <= block->sizeTraits);
    return 0;
}


/*
 *  Copy inherited type slots and traits. Don't copy overridden properties and clear property names for static properites
 */
int ejsInheritTraits(Ejs *ejs, EjsBlock *block, EjsBlock *baseBlock, int count, int offset, bool implementing)
{
    EjsNames        *names;
    EjsTrait        *trait;
    EjsFunction     *existingFun, *fun;
    EjsVar          *vp;
    EjsObject       *obj;
    int             i, start;

    mprAssert(block);
    
    if (baseBlock == 0 || count <= 0) {
        return 0;
    }
    block->numInherited += count;
    
    mprAssert(block->numInherited <= block->numTraits);
    mprAssert(block->numTraits <= block->sizeTraits);

    obj = &block->obj;
    names = obj->names;
    if (names == 0) {
        ejsGrowObjectNames(obj, obj->numProp);
        names = obj->names;
    }

    start = baseBlock->numTraits - count;
    for (i = start; i < baseBlock->numTraits; i++, offset++) {
        trait = &block->traits[i];

        if (obj->var.isInstanceBlock) {
            /*
             *  Instance properties
             */
            obj->slots[offset] = baseBlock->obj.slots[i];
            block->traits[offset] = baseBlock->traits[i];
            names->entries[offset] = baseBlock->obj.names->entries[i];

        } else {
            existingFun = (EjsFunction*) block->obj.slots[offset];
            if (existingFun && ejsIsFunction(existingFun) && existingFun->override) {
                continue;
            }

            /*
             *  Copy implemented properties (including static and instance functions) and inherited instance functions.
             */
            vp = baseBlock->obj.slots[i];
            if (implementing || (vp && ejsIsFunction(vp) && !((EjsFunction*) vp)->staticMethod)) {
                obj->slots[offset] = vp;
                block->traits[offset] = baseBlock->traits[i];
                names->entries[offset] = baseBlock->obj.names->entries[i];
                if (implementing && ejsIsFunction(vp) && ((EjsFunction*) vp)->staticMethod) {
                    /*
                     *  defineBlockProperty sets this as an optimization. Must unset for static functions that are 
                     *  implemented by other types.
                     */
                    ((EjsFunction*) vp)->thisObj = 0;
                }
            }
            if (vp && ejsIsFunction(vp)) {
                fun = (EjsFunction*) vp;
                if (fun->override) {
                    trait->attributes |= EJS_ATTR_INHERITED;
                }
            }
        }
    }

    if (block->numTraits < block->numInherited) {
        block->numTraits = block->numInherited;
        mprAssert(block->numTraits <= block->sizeTraits);
    }
    ejsRebuildHash(ejs, obj);
    return 0;
}


/*
 *  Get a trait by slot number. (Note: use ejsGetPropertyTrait for access to a property's trait)
 */
EjsTrait *ejsGetTrait(EjsBlock *block, int slotNum)
{
    mprAssert(block);
    mprAssert(slotNum >= 0);

    if (slotNum < 0 || slotNum >= block->numTraits) {
        return 0;
    }
    mprAssert(slotNum < block->numTraits);
    return &block->traits[slotNum];
}


int ejsGetTraitAttributes(EjsBlock *block, int slotNum)
{
    mprAssert(block);
    mprAssert(slotNum >= 0);

    if (slotNum < 0 || slotNum >= block->numTraits) {
        mprAssert(0);
        return 0;
    }
    mprAssert(slotNum < block->numTraits);
    return block->traits[slotNum].attributes;
}


EjsType *ejsGetTraitType(EjsBlock *block, int slotNum)
{
    mprAssert(block);
    mprAssert(slotNum >= 0);

    if (slotNum < 0 || slotNum >= block->numTraits) {
        mprAssert(0);
        return 0;
    }
    mprAssert(slotNum < block->numTraits);
    return block->traits[slotNum].type;
}


int ejsGetNumTraits(EjsBlock *block)
{
    mprAssert(block);

    return block->numTraits;
}


int ejsGetNumInheritedTraits(EjsBlock *block)
{
    mprAssert(block);
    return block->numInherited;
}


/*
 *  Remove a property from a block. This erases the property and its traits.
 */
int ejsRemoveProperty(Ejs *ejs, EjsBlock *block, int slotNum)
{
    EjsFunction     *fun;
    EjsVar          *vp;
    int             i;

    mprAssert(ejs);
    mprAssert(block);
    
    /*
     *  Copy type slots and traits down to remove the slot
     */
    removeTrait(block, slotNum, 1);
    ejsRemoveSlot(ejs, (EjsObject*) block, slotNum, 1);

    mprAssert(block->numTraits <= block->obj.numProp);

    /*
     *  Fixup the slot numbers of all the methods
     */
    for (i = slotNum; i < block->obj.numProp; i++) {
        vp = block->obj.slots[i];
        if (vp == 0) {
            continue;
        }
        if (ejsIsFunction(vp)) {
            fun = (EjsFunction*) vp;
            fun->slotNum--;
            mprAssert(fun->slotNum == i);
            if (fun->nextSlot >= 0) {
                fun->nextSlot--;
                mprAssert(fun->slotNum < block->obj.numProp);
            }
        }
    }

    return 0;
}


EjsBlock *ejsCopyBlock(Ejs *ejs, EjsBlock *src, bool deep)
{
    EjsBlock    *dest;

    dest = (EjsBlock*) ejsCopyObject(ejs, (EjsObject*) src, deep);

    dest->numTraits = src->numTraits;
    dest->sizeTraits = src->sizeTraits;
    dest->traits = src->traits;
    dest->dynamicInstance = src->dynamicInstance;
    dest->numInherited = src->numInherited;
    dest->nobind = src->nobind;
    dest->scopeChain = src->scopeChain;
    mprAssert(dest->numTraits <= dest->sizeTraits);
    dest->namespaces = src->namespaces;
    return dest;
}


void ejsResetBlockNamespaces(Ejs *ejs, EjsBlock *block)
{
    ejsClearList(&block->namespaces);
}


int ejsGetNamespaceCount(EjsBlock *block)
{
    mprAssert(block);

    return ejsGetListCount(&block->namespaces);
}


void ejsPopBlockNamespaces(EjsBlock *block, int count)
{
    mprAssert(block);
    mprAssert(block->namespaces.length >= count);

    block->namespaces.length = count;
}


int ejsAddNamespaceToBlock(Ejs *ejs, EjsBlock *block, EjsNamespace *nsp)
{
    EjsFunction     *fun;
    EjsNamespace    *namespace;
    EjsList         *list;
    int             next;

    mprAssert(block);

    if (nsp == 0) {
        ejsThrowTypeError(ejs, "Not a namespace");
        return EJS_ERR;
    }
    fun = (EjsFunction*) block;
    list = &block->namespaces;

    if (ejsIsFunction(fun)) {
        if (fun->isInitializer && fun->owner) {
            block = block->scopeChain;
            list = &block->namespaces;
            /*
             *  If defining a namespace at the class level (outside functions) use the class itself.
             *  Initializers only run once so this should only happen once.
             */
            for (next = 0; (namespace = ejsGetNextItem(list, &next)) != 0; ) {
                if (strcmp(namespace->name, nsp->name) == 0) {
                    /* Already there */
                    return 0;
                }
            }
            if (block->obj.var.master && ejs->master) {
                nsp = ejsCreateNamespace(ejs->master, mprStrdup(ejs->master, nsp->name), mprStrdup(ejs->master, nsp->uri));
            }
        }
    }
    ejsAddItemToSharedList(block, list, nsp);
    return 0;
}


/*
 *  Inherit namespaces from base types. Only inherit protected.
 */
void ejsInheritBaseClassNamespaces(Ejs *ejs, EjsType *type, EjsType *baseType)
{
    EjsNamespace    *nsp;
    EjsBlock        *block;
    EjsList         *baseNamespaces, oldNamespaces;
    int             next;

    block = &type->block;
    oldNamespaces = block->namespaces;
    ejsInitList(&block->namespaces);
    baseNamespaces = &baseType->block.namespaces;

    if (baseNamespaces) {
        for (next = 0; ((nsp = (EjsNamespace*) ejsGetNextItem(baseNamespaces, &next)) != 0); ) {
            if (strstr(nsp->name, ",protected")) {
                ejsAddItem(block, &block->namespaces, nsp);
            }
        }
    }

    if (oldNamespaces.length > 0) {
        for (next = 0; ((nsp = (EjsNamespace*) ejsGetNextItem(&oldNamespaces, &next)) != 0); ) {
            ejsAddItem(block, &block->namespaces, nsp);
        }
    }
}



void ejsCreateBlockType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Block"), ejs->objectType, 
        sizeof(EjsType), ES_Block, ES_Block_NUM_CLASS_PROP, ES_Block_NUM_INSTANCE_PROP, 
        EJS_ATTR_DYNAMIC_INSTANCE | EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_BLOCK_HELPERS);
    type->skipScope = 1;
    ejs->blockType = type;
}


void ejsConfigureBlockType(Ejs *ejs)
{
}


void ejsInitializeBlockHelpers(EjsTypeHelpers *helpers)
{
    helpers->cloneVar               = (EjsCloneVarHelper) ejsCopyBlock;
    helpers->defineProperty         = (EjsDefinePropertyHelper) defineBlockProperty;
    helpers->getPropertyTrait       = (EjsGetPropertyTraitHelper) getBlockPropertyTrait;
    helpers->markVar                = (EjsMarkVarHelper) ejsMarkBlock;
    helpers->setPropertyTrait       = (EjsSetPropertyTraitHelper) setBlockPropertyTrait;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsBlock.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsBoolean.c"
 */
/************************************************************************/

/**
 *  ejsBoolean.c - Boolean native class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Cast the operand to a primitive type
 *
 *  intrinsic function cast(type: Type) : Object
 */

static EjsVar *castBooleanVar(Ejs *ejs, EjsBoolean *vp, EjsType *type)
{
    mprAssert(ejsIsBoolean(vp));

    switch (type->id) {

    case ES_Number:
        return (EjsVar*) ((vp->value) ? ejs->oneValue: ejs->zeroValue);

    case ES_String:
        return (EjsVar*) ejsCreateString(ejs, (vp->value) ? "true" : "false");

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
}


/*
 *  Coerce operands for invokeOperator
 */
static EjsVar *coerceBooleanOperands(Ejs *ejs, EjsVar *lhs, int opcode, EjsVar *rhs)
{
    switch (opcode) {

    case EJS_OP_ADD:
        if (ejsIsUndefined(rhs)) {
            return (EjsVar*) ejs->nanValue;
        } else if (ejsIsNull(rhs) || ejsIsNumber(rhs) || ejsIsDate(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        } else {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        break;

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_NE:
        if (ejsIsString(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return 0;

    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) (((EjsBoolean*) lhs)->value ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
    case EJS_OP_COMPARE_FALSE:
        return (EjsVar*) (((EjsBoolean*) lhs)->value ? ejs->falseValue : ejs->trueValue);

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->falseValue;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }
}


/*
 *  Run an operator on the operands
 */
static EjsVar *invokeBooleanOperator(Ejs *ejs, EjsBoolean *lhs, int opcode, EjsBoolean *rhs)
{
    EjsVar      *result;

    if (rhs == 0 || lhs->obj.var.type != rhs->obj.var.type) {
        if (!ejsIsA(ejs, (EjsVar*) lhs, ejs->booleanType) || !ejsIsA(ejs, (EjsVar*) rhs, ejs->booleanType)) {
            if ((result = coerceBooleanOperands(ejs, (EjsVar*) lhs, opcode, (EjsVar*) rhs)) != 0) {
                return result;
            }
        }
    }

    /*
     *  Types now match
     */
    switch (opcode) {

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ((lhs->value == rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_NE: case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ((lhs->value != rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_LT:
        return (EjsVar*) ((lhs->value < rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_LE:
        return (EjsVar*) ((lhs->value <= rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_GT:
        return (EjsVar*) ((lhs->value > rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_GE:
        return (EjsVar*) ((lhs->value >= rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_NOT_ZERO:
        return (EjsVar*) ((lhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ((lhs->value == 0) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_COMPARE_FALSE:
        return (EjsVar*) ((lhs->value) ? ejs->falseValue: ejs->trueValue);

    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) ((lhs->value) ? ejs->trueValue: ejs->falseValue);

    /*
     *  Unary operators
     */
    case EJS_OP_NEG:
        return (EjsVar*) ejsCreateNumber(ejs, - lhs->value);

    case EJS_OP_LOGICAL_NOT:
        return (EjsVar*) ejsCreateBoolean(ejs, !lhs->value);

    case EJS_OP_NOT:
        return (EjsVar*) ejsCreateBoolean(ejs, ~lhs->value);

    /*
     *  Binary operations
     */
    case EJS_OP_ADD:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value + rhs->value);

    case EJS_OP_AND:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value & rhs->value);

    case EJS_OP_DIV:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value / rhs->value);

    case EJS_OP_MUL:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value * rhs->value);

    case EJS_OP_OR:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value | rhs->value);

    case EJS_OP_REM:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value % rhs->value);

    case EJS_OP_SUB:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value - rhs->value);

    case EJS_OP_USHR:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value >> rhs->value);

    case EJS_OP_XOR:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value ^ rhs->value);

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->obj.var.type->qname.name);
        return 0;
    }
}


/*
 *  Boolean constructor.
 *
 *      function Boolean(value: Boolean = null)
 *
 *  If the value is omitted or 0, -1, NaN, false, null, undefined or the empty string, then set the boolean value to
 *  to false.
 */

static EjsVar *booleanConstructor(Ejs *ejs, EjsBoolean *bp, int argc, EjsVar **argv)
{
    mprAssert(argc == 0 || argc == 1);

    if (argc >= 1) {
        bp->value = ejsToBoolean(ejs, argv[0])->value;
    }
    return (EjsVar*) bp;
}



EjsBoolean *ejsCreateBoolean(Ejs *ejs, int value)
{
    return (value) ? ejs->trueValue : ejs->falseValue;
}


static void defineBooleanConstants(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->booleanType;

    if (ejs->flags & EJS_FLAG_EMPTY) {
        return;
    }

    ejsSetProperty(ejs, ejs->global, ES_boolean, (EjsVar*) type);
    ejsSetProperty(ejs, ejs->global, ES_true, (EjsVar*) ejs->trueValue);
    ejsSetProperty(ejs, ejs->global, ES_false, (EjsVar*) ejs->falseValue);
}


void ejsCreateBooleanType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Boolean"), ejs->objectType, sizeof(EjsBoolean),
        ES_Boolean, ES_Boolean_NUM_CLASS_PROP, ES_Boolean_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
    ejs->booleanType = type;

    /*
     *  Define the helper functions.
     */
    type->helpers->castVar = (EjsCastVarHelper) castBooleanVar;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokeBooleanOperator;

    /*
     *  Pre-create the only two valid instances for boolean
     */
    ejs->trueValue = (EjsBoolean*) ejsCreateVar(ejs, type, 0);
    ejs->trueValue->value = 1;
    ejs->trueValue->obj.var.primitive = 1;

    ejs->falseValue = (EjsBoolean*) ejsCreateVar(ejs, type, 0);
    ejs->falseValue->value = 0;
    ejs->falseValue->obj.var.primitive = 1;

    ejsSetDebugName(ejs->falseValue, "false");
    ejsSetDebugName(ejs->trueValue, "true");

    defineBooleanConstants(ejs);
}


void ejsConfigureBooleanType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->booleanType;

    defineBooleanConstants(ejs);
    ejsBindMethod(ejs, type, ES_Boolean_Boolean, (EjsNativeFunction) booleanConstructor);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsBoolean.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsByteArray.c"
 */
/************************************************************************/

/*
 *  ejsByteArray.c - Ejscript ByteArray class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int  flushByteArray(Ejs *ejs, EjsByteArray *ap);
static int  getInput(Ejs *ejs, EjsByteArray *ap, int required);
static int  growByteArray(Ejs *ejs, EjsByteArray *ap, int len);
static int  lookupByteArrayProperty(Ejs *ejs, EjsByteArray *ap, EjsName *qname);
 static bool makeRoom(Ejs *ejs, EjsByteArray *ap, int require);
static EjsVar *byteArrayToString(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv);

static MPR_INLINE int swap16(EjsByteArray *ap, int a);
static MPR_INLINE int swap32(EjsByteArray *ap, int a);
static MPR_INLINE int64 swap64(EjsByteArray *ap, int64 a);
static MPR_INLINE double swapDouble(EjsByteArray *ap, double a);
static void putByte(EjsByteArray *ap, int value);
static void putInteger(EjsByteArray *ap, int value);
static void putLong(EjsByteArray *ap, int64 value);
static void putShort(EjsByteArray *ap, int value);
static void putString(EjsByteArray *ap, cchar *value, int len);
static void putNumber(EjsByteArray *ap, MprNumber value);

#if BLD_FEATURE_FLOATING_POINT
static void putDouble(EjsByteArray *ap, double value);
#endif

#define availableBytes(ap)  (((EjsByteArray*) ap)->writePosition - ((EjsByteArray*) ap)->readPosition)
#define room(ap) (ap->length - ap->writePosition)
#define adjustReadPosition(ap, amt) \
    if (1) { \
        ap->readPosition += amt; \
        if (ap->readPosition == ap->writePosition) {    \
            ap->readPosition = ap->writePosition = 0; \
        } \
    } else

/*
 *  Cast the object operand to a primitive type
 */

static EjsVar *castByteArrayVar(Ejs *ejs, EjsByteArray *vp, EjsType *type)
{
    switch (type->id) {
    case ES_Boolean:
        return (EjsVar*) ejs->trueValue;

    case ES_Number:
        return (EjsVar*) ejs->zeroValue;

    case ES_String:
        return byteArrayToString(ejs, vp, 0, 0);

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
}


static EjsByteArray *cloneByteArrayVar(Ejs *ejs, EjsByteArray *ap, bool deep)
{
    EjsByteArray    *newArray;
    int             i;

    newArray = ejsCreateByteArray(ejs, ap->length);
    if (newArray == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    for (i = 0; i < ap->length; i++) {
        newArray->value[i] = ap->value[i];
    }

    return newArray;
}


/*
 *  Delete a property and update the length
 */
static int deleteByteArrayProperty(struct Ejs *ejs, EjsByteArray *ap, int slot)
{
    if (slot >= ap->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad subscript");
        return EJS_ERR;
    }
    if ((slot + 1) == ap->length) {
        ap->length--;
        if (ap->readPosition >= ap->length) {
            ap->readPosition = ap->length - 1;
        }
        if (ap->writePosition >= ap->length) {
            ap->writePosition = ap->length - 1;
        }
    }
    if (ejsSetProperty(ejs, (EjsVar*) ap, slot, (EjsVar*) ejs->undefinedValue) < 0) {
        return EJS_ERR;
    }
    return 0;
}


static void destroyByteArray(Ejs *ejs, EjsByteArray *ap)
{
    mprAssert(ap);

    mprFree(ap->value);
    ap->value = 0;
    ejsFreeVar(ejs, (EjsVar*) ap, -1);
}


/*
 *  Return the number of elements in the array
 */
static int getByteArrayPropertyCount(Ejs *ejs, EjsByteArray *ap)
{
    return ap->length;
}


/*
 *  Get an array element. Slot numbers correspond to indicies.
 */
static EjsVar *getByteArrayProperty(Ejs *ejs, EjsByteArray *ap, int slotNum)
{
    if (slotNum < 0 || slotNum >= ap->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad array subscript");
        return 0;
    }
    return (EjsVar*) ejsCreateNumber(ejs, ap->value[slotNum]);
}


/*
 *  Lookup an array index.
 */
static int lookupByteArrayProperty(struct Ejs *ejs, EjsByteArray *ap, EjsName *qname)
{
    int     index;

    if (qname == 0 || ! isdigit((int) qname->name[0])) {
        return EJS_ERR;
    }
    index = atoi(qname->name);
    if (index < ap->length) {
        return index;
    }
    return EJS_ERR;
}


/*
 *  Cast operands as required for invokeOperator
 */
static EjsVar *coerceByteArrayOperands(Ejs *ejs, EjsVar *lhs, int opcode,  EjsVar *rhs)
{
    switch (opcode) {
    /*
     *  Binary operators
     */
    case EJS_OP_ADD:
        return ejsInvokeOperator(ejs, byteArrayToString(ejs, (EjsByteArray*) lhs, 0, 0), opcode, rhs);

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejs->zeroValue, opcode, rhs);

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_NE:
        if (ejsIsNull(rhs) || ejsIsUndefined(rhs)) {
            return (EjsVar*) ((opcode == EJS_OP_COMPARE_EQ) ? ejs->falseValue: ejs->trueValue);
        } else if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_STRICTLY_NE:
    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return 0;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }

    return 0;
}


static EjsVar *invokeByteArrayOperator(Ejs *ejs, EjsVar *lhs, int opcode,  EjsVar *rhs)
{
    EjsVar      *result;

    if (rhs == 0 || lhs->type != rhs->type) {
        if ((result = coerceByteArrayOperands(ejs, lhs, opcode, rhs)) != 0) {
            return result;
        }
    }

    switch (opcode) {

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_GE:
        return (EjsVar*) ejsCreateBoolean(ejs, (lhs == rhs));

    case EJS_OP_COMPARE_NE: case EJS_OP_COMPARE_STRICTLY_NE:
    case EJS_OP_COMPARE_LT: case EJS_OP_COMPARE_GT:
        return (EjsVar*) ejsCreateBoolean(ejs, !(lhs == rhs));

    /*
     *  Unary operators
     */
    case EJS_OP_COMPARE_NOT_ZERO:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return (EjsVar*) ejs->oneValue;

    /*
     *  Binary operators
     */
    case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_REM:
    case EJS_OP_SHR: case EJS_OP_USHR: case EJS_OP_XOR:
        return (EjsVar*) ejs->zeroValue;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->type->qname.name);
        return 0;
    }

    mprAssert(0);
}


static void markByteArrayVar(Ejs *ejs, EjsVar *parent, EjsByteArray *ap)
{
    mprAssert(ejsIsByteArray(ap));

    if (ap->input) {
        ejsMarkVar(ejs, (EjsVar*) ap, (EjsVar*) ap->input);
    }
    if (ap->output) {
        ejsMarkVar(ejs, (EjsVar*) ap, (EjsVar*) ap->output);
    }
}


/*
 *  Create or update an array elements. If slotNum is < 0, then create the next free array slot. If slotNum is greater
 *  than the array length, grow the array.
 */
static int setByteArrayProperty(struct Ejs *ejs, EjsByteArray *ap, int slotNum,  EjsVar *value)
{
    if (slotNum >= ap->length) {
        if (growByteArray(ejs, ap, slotNum + 1) < 0) {
            return EJS_ERR;
        }
    }
    if (ejsIsNumber(value)) {
        ap->value[slotNum] = ejsGetInt(value);
    } else {
        ap->value[slotNum] = ejsGetInt(ejsToNumber(ejs, value));
    }

    if (slotNum >= ap->length) {
        ap->length = slotNum + 1;
    }
    return slotNum;
}


/*
 *  ByteArray constructor.
 *
 *  function ByteArray(size: Number = -1, growable: Boolean = true): ByteArray
 */
static EjsVar *byteArrayConstructor(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    bool    growable;
    int     size;

    mprAssert(0 <= argc && argc <= 2);

    size = (argc >= 1) ? ejsGetInt(argv[0]) : MPR_BUFSIZE;
    if (size <= 0) {
        size = 1;
    }
    growable = (argc == 2) ? ejsGetBoolean(argv[1]): 1;

    if (growByteArray(ejs, ap, size) < 0) {
        return 0;
    }
    mprAssert(ap->value);
    ap->growable = growable;
    ap->growInc = MPR_BUFSIZE;
    ap->length = size;
    ap->endian = mprGetEndian(ejs);

    return (EjsVar*) ap;
}


/**
 *  Get the number of bytes that are currently available on this stream for reading.
 *
 *  function get available(): Number
 */
static EjsVar *availableProc(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ap->writePosition - ap->readPosition);
}


#if ES_ByteArray_compact
/*
 *  Copy data down and adjust the read/write offset pointers.
 *
 *  function compact(): Void
 */
static EjsVar *compact(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    mprAssert(argc == 0);

    if (ap->writePosition == ap->readPosition) {
        ap->writePosition = ap->readPosition = 0;

    } else if (ap->readPosition > 0) {
        memmove(ap->value, &ap->value[ap->readPosition], ap->writePosition - ap->readPosition);
        ap->writePosition -= ap->readPosition;
        ap->readPosition = 0;
    }
    return 0;
}
#endif


/*
 *  Copy data into the array. Data is written at the $destOffset.
 *
 *  function copyIn(destOffset: Number, src: ByteArray, srcOffset: Number = 0, count: Number = -1): Number
 */
static EjsVar *copyIn(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    EjsByteArray    *src;
    int             i, destOffset, srcOffset, count;

    destOffset = ejsGetInt(argv[0]);
    src = (EjsByteArray*) argv[1];
    srcOffset = (argc > 2) ? ejsGetInt(argv[2]) : 0;
    count = (argc > 3) ? ejsGetInt(argv[3]) : MAXINT;

    if (srcOffset >= src->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad source offset");
        return 0;
    }
    if (count < 0) {
        count = MAXINT;
    }
    count = min(src->length - srcOffset, count);

    makeRoom(ejs, ap, destOffset + count);
    if ((destOffset + count) > src->length) {
        ejsThrowOutOfBoundsError(ejs, "Insufficient room for data");
        return 0;
    }
    for (i = 0; i < count; i++) {
        ap->value[destOffset++] = src->value[srcOffset++];
    }
    return (EjsVar*) ejsCreateNumber(ejs, count);
}


/*
 *  Copy data from the array. Data is copied from the $srcOffset.
 *
 *  function copyOut(srcOffset: Number, dest: ByteArray, destOffset: Number = 0, count: Number = -1): Number
 */
static EjsVar *copyOut(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    EjsByteArray    *dest;
    int             i, srcOffset, destOffset, count;

    srcOffset = ejsGetInt(argv[0]);
    dest = (EjsByteArray*) argv[1];
    destOffset = (argc > 2) ? ejsGetInt(argv[2]) : 0;
    count = (argc > 3) ? ejsGetInt(argv[3]) : MAXINT;

    if (srcOffset >= ap->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad source data offset");
        return 0;
    }
    count = min(ap->length - srcOffset, count);
    makeRoom(ejs, dest, destOffset + count);
    if ((destOffset + count) > dest->length) {
        ejsThrowOutOfBoundsError(ejs, "Insufficient room for data");
        return 0;
    }
    for (i = 0; i < count; i++) {
        dest->value[destOffset++] = ap->value[srcOffset++];
    }
    return (EjsVar*) ejsCreateNumber(ejs, count);
}


/*
 *  Determine if the system is using little endian byte ordering
 *
 *  function get endian(): Number
 */
static EjsVar *endian(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ap->endian);
}


/*
 *  Set the system encoding to little or big endian.
 *
 *  function set endian(value: Number): Void
 */
static EjsVar *setEndian(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    int     endian;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));

    endian = ejsGetInt(argv[0]);
    if (endian != 0 && endian != 1) {
        ejsThrowArgError(ejs, "Bad endian value");
        return 0;
    }
    ap->endian = endian;
    ap->swap = (ap->endian != mprGetEndian(ejs));
    return 0;
}


/*
 *  Function to iterate and return the next element index.
 *  NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextByteArrayKey(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsByteArray    *ap;

    ap = (EjsByteArray*) ip->target;
    if (!ejsIsByteArray(ap)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    if (ip->index < ap->readPosition) {
        ip->index = ap->readPosition;
    }
    if (ip->index < ap->writePosition) {
        return (EjsVar*) ejsCreateNumber(ejs, ip->index++);
    }

    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return the default iterator. This returns the array index names.
 *
 *  iterator native function get(): Iterator
 */
static EjsVar *getByteArrayIterator(Ejs *ejs, EjsVar *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, ap, (EjsNativeFunction) nextByteArrayKey, 0, NULL);
}


/*
 *  Function to iterate and return the next element value.
 *  NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextByteArrayValue(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsByteArray    *ap;

    ap = (EjsByteArray*) ip->target;
    if (!ejsIsByteArray(ap)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }
    if (ip->index < ap->readPosition) {
        ip->index = ap->readPosition;
    }
    if (ip->index < ap->writePosition) {
        return (EjsVar*) ejsCreateNumber(ejs, ap->value[ip->index++]);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return an iterator to return the next array element value.
 *
 *  iterator native function getValues(): Iterator
 */
static EjsVar *getByteArrayValues(Ejs *ejs, EjsVar *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, ap, (EjsNativeFunction) nextByteArrayValue, 0, NULL);
}


/**
 *  Callback function to called when read data is required. Callback signature:
 *      function callback(buffer: ByteArray, offset: Number, count: Number): Number
 *
 *  function set input(value: Function): Void
 */
static EjsVar *inputByteArrayData(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsFunction(argv[0]));

    ap->input = (EjsFunction*) argv[0];
    return 0;
}


/*
 *  Flush the data in the byte array and reset the read and write position pointers
 *
 *  function flush(graceful: Boolean = false): Void
 */
static EjsVar *flushProc(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    if (argc == 0 || argv[0] == (EjsVar*) ejs->trueValue) {
        flushByteArray(ejs, ap);
    }
    ap->writePosition = ap->readPosition = 0;
    return 0;
}


/*
 *  Get the length of an array.
 *  @return Returns the number of items in the array
 *
 *  intrinsic override function get length(): Number
 */
static EjsVar *getByteArrayLength(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ap->length);
}


#if KEEP
/*
 *  Set the length of an array.
 *
 *  intrinsic override function set length(value: Number): void
 */
static EjsVar *setByteArrayLength(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsNumber(argv[0]));
    mprAssert(ejsIsByteArray(ap));

    ap->length = ejsGetInt(argv[0]);
    if (ap->readPosition >= ap->length) {
        ap->readPosition = ap->length - 1;
    }
    if (ap->writePosition >= ap->length) {
        ap->writePosition = ap->length - 1;
    }

    return 0;
}
#endif


/**
 *  Function to call to flush data. Callback signature:
 *      function callback(...data): Number
 *
 *  function set output(value: Function): Void
 */
static EjsVar *setOutput(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsFunction(argv[0]));

    ap->output = (EjsFunction*) argv[0];
    return 0;
}


/*
 *  Read data from the array into another byte array. Data is read from the current read $position pointer.
 *  Data is written to the write position if offset is -1. Othwise at the given offset. If offset is < 0, the 
 *  write position is updated.
 *
 *  function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number
 */
static EjsVar *byteArrayReadProc(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    EjsByteArray    *buffer;
    int             offset, count, i;

    mprAssert(1 <= argc && argc <= 3);

    buffer = (EjsByteArray*) argv[0];
    offset = (argc == 2) ? ejsGetInt(argv[1]) : 0;
    count = (argc == 3) ? ejsGetInt(argv[2]) : buffer->length;

    if (count < 0) {
        count = buffer->length;
    }
    if (offset < 0) {
        offset = buffer->writePosition;
    } else if (offset >= buffer->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad read offset value");
        return 0;
    } else {
        buffer->readPosition = 0;
        buffer->writePosition = 0;
    }
    if (getInput(ejs, ap, 1) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }
    count = min(availableBytes(ap), count);
    for (i = 0; i < count; i++) {
        buffer->value[offset++] = ap->value[ap->readPosition++];
    }
    buffer->writePosition += count;
    return (EjsVar*) ejsCreateNumber(ejs, count);
}


/*
 *  Read a boolean from the array.
 *
 *  function readBoolean(): Boolean
 */
static EjsVar *readBoolean(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    int     result;

    if (getInput(ejs, ap, 1) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }
    result = ap->value[ap->readPosition];
    adjustReadPosition(ap, 1);

    return (EjsVar*) ejsCreateBoolean(ejs, result);
}


/*
 *  Read a byte from the array.
 *
 *  function readByte(): Number
 */
static EjsVar *readByte(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    int     result;

    if (getInput(ejs, ap, 1) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }
    result = ap->value[ap->readPosition];
    adjustReadPosition(ap, 1);

    return (EjsVar*) ejsCreateNumber(ejs, result);
}


/**
 *  Read a date from the array.
 *
 *  function readDate(): Date
 */
static EjsVar *readDate(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    double  value;

    if (getInput(ejs, ap, EJS_SIZE_DOUBLE) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }

    value = * (double*) &ap->value[ap->readPosition];
    value = swapDouble(ap, value);
    adjustReadPosition(ap, sizeof(double));

    return (EjsVar*) ejsCreateDate(ejs, (MprTime) value);
}


#if BLD_FEATURE_FLOATING_POINT && ES_ByteArray_readDouble
/**
 *  Read a double from the array. The data will be decoded according to the encoding property.
 *
 *  function readDouble(): Date
 */
static EjsVar *readDouble(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    double  value;

    if (getInput(ejs, ap, EJS_SIZE_DOUBLE) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }
#if OLD
    value = * (double*) &ap->value[ap->readPosition];
#else
    memcpy(&value, (char*) &ap->value[ap->readPosition], sizeof(double));
#endif
    value = swapDouble(ap, value);
    adjustReadPosition(ap, sizeof(double));
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) value);
}
#endif

/*
 *  Read a 32-bit integer from the array. The data will be decoded according to the encoding property.
 *
 *  function readInteger(): Number
 */
static EjsVar *readInteger(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    int     value;

    if (getInput(ejs, ap, EJS_SIZE_INT) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }

    value = * (int*) &ap->value[ap->readPosition];
    value = swap32(ap, value);
    adjustReadPosition(ap, sizeof(int));

    return (EjsVar*) ejsCreateNumber(ejs, value);
}


/*
 *  Read a 64-bit long from the array.The data will be decoded according to the encoding property.
 *
 *  function readLong(): Number
 */
static EjsVar *readLong(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    int64   value;

    if (getInput(ejs, ap, EJS_SIZE_LONG) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }

    value = * (int64*) &ap->value[ap->readPosition];
    value = swap64(ap, value);
    adjustReadPosition(ap, sizeof(int64));

    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) value);
}


/*
 *  Get the current read position offset
 *
 *  function get readPosition(): Number
 */
static EjsVar *readPosition(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ap->readPosition);
}


/*
 *  Set the current read position offset
 *
 *  function set readPosition(position: Number): Void
 */
static EjsVar *setReadPosition(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    int     pos;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));

    pos = ejsGetInt(argv[0]);
    if (pos < 0 || pos > ap->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad position value");
        return 0;
    }
    if (pos > ap->writePosition) {
        ejsThrowStateError(ejs, "Read position is greater than write position");
    } else {
        ap->readPosition = pos;
    }
    return 0;
}


/*
 *  Read a 16-bit short integer from the array. The data will be decoded according to the encoding property.
 *
 *  function readShort(): Number
 */
static EjsVar *readShort(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    int     value;

    if (getInput(ejs, ap, EJS_SIZE_SHORT) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }
    value = * (short*) &ap->value[ap->readPosition];
    value = swap16(ap, value);
    adjustReadPosition(ap, sizeof(short));
    return (EjsVar*) ejsCreateNumber(ejs, value);
}


/*
 *  Read a UTF-8 string from the array. Read data from the read position up to the write position but not more than count
 *  characters.
 *
 *  function readString(count: Number = -1): String
 */
static EjsVar *baReadString(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    EjsVar  *result;
    int     count;

    count = (argc == 1) ? ejsGetInt(argv[0]) : -1;

    if (count < 0) {
        if (getInput(ejs, ap, 1) < 0) {
            return (EjsVar*) ejs->nullValue;
        }
        count = availableBytes(ap);

    } else if (getInput(ejs, ap, count) < 0) {
        return (EjsVar*) ejs->nullValue;
        return 0;
    }
    count = min(count, availableBytes(ap));
    result = (EjsVar*) ejsCreateStringWithLength(ejs, (cchar*) &ap->value[ap->readPosition], count);
    adjustReadPosition(ap, count);
    return result;
}


#if ES_ByteArray_readXML && BLD_FEATURE_EJS_E4X
/*
 *  Read an XML document from the array.
 *
 *  function readXML(): XML
 */
static EjsVar *readXML(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    EjsXML      *xml;

    if (getInput(ejs, ap, -1) <= 0) {
        return (EjsVar*) ejs->nullValue;
    }

    xml = ejsCreateXML(ejs, 0, 0, 0, 0);
    if (xml == 0) {
        mprAssert(ejs->exception);
        return 0;
    }

    //  BUG - need to make sure that the string is null terminated
    ejsLoadXMLString(ejs, xml, (cchar*) &ap->value[ap->readPosition]);
    adjustReadPosition(ap, 0);

    return (EjsVar*) xml;
}
#endif


/*
 *  Reset the read and write position pointers if there is no available data.
 *
 *  function reset(): Void
 */
static EjsVar *reset(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    mprAssert(argc == 0);

    if (ap->writePosition == ap->readPosition) {
        ap->writePosition = ap->readPosition = 0;
    }
    return 0;
}


/**
 *  Get the number of data bytes that the array can store from the write position till the end of the array.
 *
 *  function get room(): Number
 */
static EjsVar *roomProc(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ap->length - ap->writePosition);
}


/*
 *  Convert the byte array data between the read and write positions into a string.
 *
 *  override function toString(): String
 */
static EjsVar *byteArrayToString(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateStringWithLength(ejs, (cchar*) &ap->value[ap->readPosition], availableBytes(ap));
}


/*
 *  Write data to the array. Data is written to the current write $position pointer.
 *
 *  function write(...data): Number
 */
EjsNumber *ejsWriteToByteArray(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    EjsArray        *args;
    EjsByteArray    *bp;
    EjsString       *sp;
    EjsVar          *vp;
    int             i, start, len;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    /*
     *  Unwrap nested arrays
     */
    args = (EjsArray*) argv[0];
    while (args && ejsIsArray(args) && args->length == 1) {
        vp = ejsGetProperty(ejs, (EjsVar*) args, 0);
        if (!ejsIsArray(vp)) {
            break;
        }
        args = (EjsArray*) vp;
    }

    start = ap->writePosition;

    for (i = 0; i < args->length; i++) {

        vp = ejsGetProperty(ejs, (EjsVar*) args, i);
        if (vp == 0) {
            continue;
        }

        switch (vp->type->id) {

        case ES_Boolean:
            if (!makeRoom(ejs, ap, EJS_SIZE_BOOLEAN)) {
                return 0;
            }
            putByte(ap, ejsGetBoolean(vp));
            break;

        case ES_Date:
            if (!makeRoom(ejs, ap, EJS_SIZE_DOUBLE)) {
                return 0;
            }
            putNumber(ap, (MprNumber) ((EjsDate*) vp)->value);
            break;

        case ES_Number:
            if (!makeRoom(ejs, ap, EJS_SIZE_DOUBLE)) {
                return 0;
            }
            putNumber(ap, ejsGetNumber(vp));
            break;

        case ES_String:
            if (!makeRoom(ejs, ap, ((EjsString*) vp)->length)) {
                return 0;
            }
            sp = (EjsString*) vp;
            putString(ap, sp->value, sp->length);
            break;

        default:
            sp = ejsToString(ejs, vp);
            putString(ap, sp->value, sp->length);
            break;

        case ES_ByteArray:
            bp = (EjsByteArray*) vp;
            len = availableBytes(bp);
            if (!makeRoom(ejs, ap, len)) {
                return 0;
            }
            /*
             *  Note: this only copies between the read/write positions of the source byte array
             */
            ejsCopyToByteArray(ejs, ap, ap->writePosition, (char*) &bp->value[bp->readPosition], len);
            ap->writePosition += len;
            break;
        }
    }
    return ejsCreateNumber(ejs, ap->writePosition - start);
}


/*
 *  Write a byte to the array
 *
 *  function writeByte(value: Number): Void
 */
static EjsVar *writeByte(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    if (!makeRoom(ejs, ap, 1)) {
        return 0;
    }
    putByte(ap, ejsGetInt(argv[0]));
    return 0;
}


/*
 *  Write a short to the array
 *
 *  function writeShort(value: Number): Void
 */
static EjsVar *writeShort(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    if (!makeRoom(ejs, ap, sizeof(short))) {
        return 0;
    }
    putShort(ap, ejsGetInt(argv[0]));
    return 0;
}


#if ES_ByteArray_writeDouble && BLD_FEATURE_FLOATING_POINT
/*
 *  Write a double to the array
 *
 *  function writeDouble(value: Number): Void
 */
static EjsVar *writeDouble(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    if (!makeRoom(ejs, ap, sizeof(double))) {
        return 0;
    }
    putDouble(ap, ejsGetDouble(argv[0]));
    return 0;
}
#endif


/*
 *  Write an integer (32 bits) to the array
 *
 *  function writeInteger(value: Number): Void
 */

static EjsVar *writeInteger(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    if (!makeRoom(ejs, ap, sizeof(int))) {
        return 0;
    }
    putInteger(ap, ejsGetInt(argv[0]));
    return 0;
}


/*
 *  Write a long (64 bit) to the array
 *
 *  function writeLong(value: Number): Void
 */
static EjsVar *writeLong(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    if (!makeRoom(ejs, ap, sizeof(int))) {
        return 0;
    }
    putLong(ap, ejsGetInt(argv[0]));
    return 0;
}


/*
 *  Get the current write position offset
 *
 *  function get writePosition(): Number
 */
static EjsVar *writePosition(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ap->writePosition);
}


/*
 *  Set the current write position offset
 *
 *  function set writePosition(position: Number): Void
 */
static EjsVar *setWritePosition(Ejs *ejs, EjsByteArray *ap, int argc, EjsVar **argv)
{
    int     pos;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));

    pos = ejsGetInt(argv[0]);
    if (pos < 0 || pos > ap->length) {
        ejsThrowStateError(ejs, "Write position is outside bounds of array");
        return 0;
    }
    if (pos < ap->readPosition) {
        ejsThrowStateError(ejs, "Write position is less than read position");
    } else {
        ap->writePosition = pos;
    }
    return 0;
}


/*
 *  Flush the array. If an output function is defined, invoke it to accept the data.
 *  Flushing writes pending data before resetting the array.
 */
static int flushByteArray(Ejs *ejs, EjsByteArray *ap)
{
    EjsVar      *arg, *thisObj;

    if (ap->output == 0) {
        return 0;
    }
    while (availableBytes(ap) && !ejs->exception) {
        arg = (EjsVar*) ap;
        thisObj = (ap->output && ap->output->thisObj) ? ap->output->thisObj : (EjsVar*) ap;
        ejsRunFunction(ejs, ap->output, thisObj, 1, &arg);
    }
    ap->writePosition = ap->readPosition = 0;
    return 0;
}


static int growByteArray(Ejs *ejs, EjsByteArray *ap, int len)
{
    if (len > ap->length) {
        ap->value = mprRealloc(ap, ap->value, len);
        if (ap->value == 0) {
            ejsThrowMemoryError(ejs);
            return EJS_ERR;
        }
        memset(&ap->value[ap->length], 0, len - ap->length);
        ap->growInc = min(ap->growInc * 2, 32 * 1024);
        ap->length = len;
    }
    return 0;
}


/*
 *  Get more input, sufficient to satisfy the rquired number of bytes. The required parameter specifies how many bytes 
 *  MUST be read. Short fills are not permitted. Return the count of bytes available or 0 if the required number of 
 *  bytes can't be read. 
 *  Return -ve on errors.
 */
static int getInput(Ejs *ejs, EjsByteArray *ap, int required)
{
    EjsVar      *argv[3], *thisObj;
    int         lastPos;

    if (availableBytes(ap) == 0) {
        ap->writePosition = ap->readPosition = 0;
    }
    if (ap->input) {
        while (availableBytes(ap) < required && !ejs->exception) {
            lastPos = ap->writePosition;

            /*
             *  Run the input function. Get as much data as possible without blocking.
             *  The input function will write to the byte array and will update the writePosition.
             */
            argv[0] = (EjsVar*) ap;
            thisObj = (ap->input && ap->input->thisObj) ? ap->input->thisObj : (EjsVar*) ap;
            ejsRunFunction(ejs, ap->input, thisObj, 1, argv);
            if (lastPos == ap->writePosition) {
                break;
            }
#if KEEP
            if (!ejsIsNumber(result)) {
                if (ejs->exception == NULL) {
                    ejsThrowIOError(ejs, "input callback did not return a number");
                }
                return EJS_ERR;
            }
            count = ejsGetInt(result);
            if (count < 0) {
                ejsThrowIOError(ejs, "Can't read data");
                return EJS_ERR;

            } else if (count == 0) {
                return 0;
            }
#endif
        }
    }
    if (availableBytes(ap) < required) {
        return 0;
    }
    return availableBytes(ap);
}


static bool makeRoom(Ejs *ejs, EjsByteArray *ap, int require)
{
    int     newLen;

    if (room(ap) < require) {
        if (flushByteArray(ejs, ap) < 0) {
            return 0;
        }
        if (room(ap) < require) {
            newLen = max(ap->length + require, ap->length + ap->growInc);
            if (!ap->growable || growByteArray(ejs, ap, newLen) < 0) {
                if (ejs->exception == NULL) {
                    ejsThrowResourceError(ejs, "Byte array is too small");
                }
                return 0;
            }
        }
    }
    return 1;
}


static MPR_INLINE int swap16(EjsByteArray *ap, int a)
{
    if (!ap->swap) {
        return a;
    }
    return (a & 0xFF) << 8 | (a & 0xFF00 >> 8);
}


static MPR_INLINE int swap32(EjsByteArray *ap, int a)
{
    if (!ap->swap) {
        return a;
    }
    return (a & 0xFF) << 24 | (a & 0xFF00 << 8) | (a & 0xFF0000 >> 8) | (a & 0xFF000000 >> 16);
}


static MPR_INLINE int64 swap64(EjsByteArray *ap, int64 a)
{
    int64   low, high;

    if (!ap->swap) {
        return a;
    }

    low = a & 0xFFFFFFFF;
    high = (a >> 32) & 0xFFFFFFFF;

    return  (low & 0xFF) << 24 | (low & 0xFF00 << 8) | (low & 0xFF0000 >> 8) | (low & 0xFF000000 >> 16) |
            ((high & 0xFF) << 24 | (high & 0xFF00 << 8) | (high & 0xFF0000 >> 8) | (high & 0xFF000000 >> 16)) << 32;
}


static MPR_INLINE double swapDouble(EjsByteArray *ap, double a)
{
    int64   low, high;

    if (!ap->swap) {
        return a;
    }

    low = ((int64) a) & 0xFFFFFFFF;
    high = (((int64) a) >> 32) & 0xFFFFFFFF;

    return  (double) ((low & 0xFF) << 24 | (low & 0xFF00 << 8) | (low & 0xFF0000 >> 8) | (low & 0xFF000000 >> 16) |
            ((high & 0xFF) << 24 | (high & 0xFF00 << 8) | (high & 0xFF0000 >> 8) | (high & 0xFF000000 >> 16)) << 32);
}


static void putByte(EjsByteArray *ap, int value)
{
    ap->value[ap->writePosition++] = (char) value;
}


static void putShort(EjsByteArray *ap, int value)
{
    value = swap16(ap, value);

    *((short*) &ap->value[ap->writePosition]) = (short) value;
    ap->writePosition += sizeof(short);
}


static void putInteger(EjsByteArray *ap, int value)
{
    value = swap32(ap, value);

    *((int*) &ap->value[ap->writePosition]) = (int) value;
    ap->writePosition += sizeof(int);
}


static void putLong(EjsByteArray *ap, int64 value)
{
    value = swap64(ap, value);

    *((int64*) &ap->value[ap->writePosition]) = value;
    ap->writePosition += sizeof(int64);
}


#if BLD_FEATURE_FLOATING_POINT
static void putDouble(EjsByteArray *ap, double value)
{
    value = swapDouble(ap, value);

#if OLD
    *((double*) &ap->value[ap->writePosition]) = value;
#else
    memcpy((char*) &ap->value[ap->writePosition], &value, sizeof(double));
#endif
    ap->writePosition += sizeof(double);
}
#endif


/*
 *  Write a number in the default number encoding
 */
static void putNumber(EjsByteArray *ap, MprNumber value)
{
#if BLD_FEATURE_FLOATING_POINT
    putDouble(ap, value);
#elif MPR_64_BIT
    putLong(ap, value);
#else
    putInteger(ap, value);
#endif
}


static void putString(EjsByteArray *ap, cchar *value, int len)
{
    mprMemcpy(&ap->value[ap->writePosition], room(ap), value, len);
    ap->writePosition += len;
}


void ejsSetByteArrayPositions(Ejs *ejs, EjsByteArray *ap, int readPosition, int writePosition)
{
    if (readPosition >= 0) {
        ap->readPosition = readPosition;
    }
    if (writePosition >= 0) {
        ap->writePosition = writePosition;
    }
}


int ejsCopyToByteArray(Ejs *ejs, EjsByteArray *ap, int offset, char *data, int length)
{
    int     i;

    mprAssert(ap);
    mprAssert(data);

    if (!makeRoom(ejs, ap, offset + length)) {
        return EJS_ERR;
    }
    if (ap->length < (offset + length)) {
        return EJS_ERR;
    }
    for (i = 0; i < length; i++) {
        ap->value[offset++] = data[i];
    }
    return 0;
}


int ejsGetAvailableData(EjsByteArray *ap)
{
    return availableBytes(ap);
}


EjsByteArray *ejsCreateByteArray(Ejs *ejs, int size)
{
    EjsByteArray    *ap;

    /*
     *  No need to invoke constructor
     */
    ap = (EjsByteArray*) ejsCreateVar(ejs, ejs->byteArrayType, 0);
    if (ap == 0) {
        return 0;
    }

    if (size <= 0) {
        size = MPR_BUFSIZE;
    }

    if (growByteArray(ejs, ap, size) < 0) {
        return 0;
    }
    ap->length = size;
    ap->growable = 1;
    ap->growInc = MPR_BUFSIZE;
    ap->endian = mprGetEndian(ejs);
    ejsSetDebugName(ap, "ByteArray instance");
    return ap;
}


void ejsCreateByteArrayType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "ByteArray"), ejs->objectType, sizeof(EjsByteArray),
        ES_ByteArray, ES_ByteArray_NUM_CLASS_PROP, ES_ByteArray_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
    ejs->byteArrayType = type;

    /*
     *  Define the helper functions.
     */
    type->helpers->castVar = (EjsCastVarHelper) castByteArrayVar;
    type->helpers->destroyVar = (EjsDestroyVarHelper) destroyByteArray;
    type->helpers->cloneVar = (EjsCloneVarHelper) cloneByteArrayVar;
    type->helpers->getProperty = (EjsGetPropertyHelper) getByteArrayProperty;
    type->helpers->getPropertyCount = (EjsGetPropertyCountHelper) getByteArrayPropertyCount;
    type->helpers->deleteProperty = (EjsDeletePropertyHelper) deleteByteArrayProperty;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokeByteArrayOperator;
    type->helpers->markVar = (EjsMarkVarHelper) markByteArrayVar;
    type->helpers->lookupProperty = (EjsLookupPropertyHelper) lookupByteArrayProperty;
    type->helpers->setProperty = (EjsSetPropertyHelper) setByteArrayProperty;
}


void ejsConfigureByteArrayType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->byteArrayType;
    
    ejsBindMethod(ejs, type, ES_ByteArray_ByteArray, (EjsNativeFunction) byteArrayConstructor);
    ejsBindMethod(ejs, type, ES_ByteArray_available, (EjsNativeFunction) availableProc);
#if ES_ByteArray_compact
    ejsBindMethod(ejs, type, ES_ByteArray_compact, (EjsNativeFunction) compact);
#endif
    ejsBindMethod(ejs, type, ES_ByteArray_copyIn, (EjsNativeFunction) copyIn);
    ejsBindMethod(ejs, type, ES_ByteArray_copyOut, (EjsNativeFunction) copyOut);
    ejsBindMethod(ejs, type, ES_ByteArray_set_input, (EjsNativeFunction) inputByteArrayData);
    ejsBindMethod(ejs, type, ES_ByteArray_flush, (EjsNativeFunction) flushProc);
    ejsBindMethod(ejs, type, ES_Object_length, (EjsNativeFunction) getByteArrayLength);
    ejsBindMethod(ejs, type, ES_Object_get, (EjsNativeFunction) getByteArrayIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, (EjsNativeFunction) getByteArrayValues);
    ejsBindMethod(ejs, type, ES_ByteArray_endian, (EjsNativeFunction) endian);
    ejsBindMethod(ejs, type, ES_ByteArray_set_endian, (EjsNativeFunction) setEndian);
    ejsBindMethod(ejs, type, ES_ByteArray_set_output, (EjsNativeFunction) setOutput);
    ejsBindMethod(ejs, type, ES_ByteArray_read, (EjsNativeFunction) byteArrayReadProc);
    ejsBindMethod(ejs, type, ES_ByteArray_readBoolean, (EjsNativeFunction) readBoolean);
    ejsBindMethod(ejs, type, ES_ByteArray_readByte, (EjsNativeFunction) readByte);
    ejsBindMethod(ejs, type, ES_ByteArray_readDate, (EjsNativeFunction) readDate);
#if BLD_FEATURE_FLOATING_POINT && ES_ByteArray_readDouble
    ejsBindMethod(ejs, type, ES_ByteArray_readDouble, (EjsNativeFunction) readDouble);
#endif
    ejsBindMethod(ejs, type, ES_ByteArray_readInteger, (EjsNativeFunction) readInteger);
    ejsBindMethod(ejs, type, ES_ByteArray_readLong, (EjsNativeFunction) readLong);
    ejsBindMethod(ejs, type, ES_ByteArray_readPosition, (EjsNativeFunction) readPosition);
    ejsBindMethod(ejs, type, ES_ByteArray_set_readPosition, (EjsNativeFunction) setReadPosition);
    ejsBindMethod(ejs, type, ES_ByteArray_readShort, (EjsNativeFunction) readShort);
    ejsBindMethod(ejs, type, ES_ByteArray_readString, (EjsNativeFunction) baReadString);
#if ES_ByteArray_readXML && BLD_FEATURE_EJS_E4X
    ejsBindMethod(ejs, type, ES_ByteArray_readXML, (EjsNativeFunction) readXML);
#endif
    ejsBindMethod(ejs, type, ES_ByteArray_reset, (EjsNativeFunction) reset);
    ejsBindMethod(ejs, type, ES_ByteArray_room, (EjsNativeFunction) roomProc);
    ejsBindMethod(ejs, type, ES_Object_toString, (EjsNativeFunction) byteArrayToString);
    ejsBindMethod(ejs, type, ES_ByteArray_write, (EjsNativeFunction) ejsWriteToByteArray);
    ejsBindMethod(ejs, type, ES_ByteArray_writeByte, (EjsNativeFunction) writeByte);
    ejsBindMethod(ejs, type, ES_ByteArray_writeShort, (EjsNativeFunction) writeShort);
    ejsBindMethod(ejs, type, ES_ByteArray_writeInteger, (EjsNativeFunction) writeInteger);
    ejsBindMethod(ejs, type, ES_ByteArray_writeLong, (EjsNativeFunction) writeLong);
#if ES_ByteArray_writeDouble && BLD_FEATURE_FLOATING_POINT
    ejsBindMethod(ejs, type, ES_ByteArray_writeDouble, (EjsNativeFunction) writeDouble);
#endif
    ejsBindMethod(ejs, type, ES_ByteArray_writePosition, (EjsNativeFunction) writePosition);
    ejsBindMethod(ejs, type, ES_ByteArray_set_writePosition, (EjsNativeFunction) setWritePosition);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsByteArray.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsDate.c"
 */
/************************************************************************/

/**
    ejsDate.c - Date type class

    Date/time is store internally as milliseconds since 1970/01/01 GMT

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */




#if BLD_FEATURE_FLOATING_POINT
#define fixed(n) ((int64) (floor(n)))
#else
#define fixed(n) (n)
#endif

#if BLD_WIN_LIKE
#pragma warning (disable:4244)
#endif

#define getNumber(ejs, a) ejsGetNumber((EjsVar*) ejsToNumber(ejs, ((EjsVar*) a)))

/*
    Cast the operand to the specified type

    intrinsic function cast(type: Type) : Object
 */

static EjsVar *castDate(Ejs *ejs, EjsDate *dp, EjsType *type)
{
    struct tm   tm;

    switch (type->id) {

    case ES_Boolean:
        return (EjsVar*) ejs->trueValue;

    case ES_Number:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) dp->value);

    case ES_String:
        /*
            Format:  Tue Jul 15 2011 10:53:23 GMT-0700 (PDT)
         */
        mprDecodeLocalTime(ejs, &tm, dp->value);
        return (EjsVar*) ejsCreateStringAndFree(ejs, mprFormatTime(ejs, "%a %b %d %Y %T GMT%z (%Z)", &tm));

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
    return 0;
}


static EjsVar *coerceDateOperands(Ejs *ejs, EjsVar *lhs, int opcode, EjsVar *rhs)
{
    switch (opcode) {
    /*
        Binary operators
     */
    case EJS_OP_ADD:
        if (ejsIsUndefined(rhs)) {
            return (EjsVar*) ejs->nanValue;
        } else if (ejsIsNull(rhs)) {
            rhs = (EjsVar*) ejs->zeroValue;
        } else if (ejsIsBoolean(rhs) || ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        } else {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        break;

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_NE:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        if (ejsIsString(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ejs->falseValue;

    /*
        Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return 0;

    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) (((EjsDate*) lhs)->value ? ejs->trueValue : ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
    case EJS_OP_COMPARE_FALSE:
        return (EjsVar*) (((EjsDate*) lhs)->value ? ejs->falseValue: ejs->trueValue);

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->falseValue;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }
    return 0;
}


static EjsVar *invokeDateOperator(Ejs *ejs, EjsDate *lhs, int opcode, EjsDate *rhs)
{
    EjsVar      *result;

    if (rhs == 0 || lhs->obj.var.type != rhs->obj.var.type) {
        if (!ejsIsA(ejs, (EjsVar*) lhs, ejs->dateType) || !ejsIsA(ejs, (EjsVar*) rhs, ejs->dateType)) {
            if ((result = coerceDateOperands(ejs, (EjsVar*) lhs, opcode, (EjsVar*) rhs)) != 0) {
                return result;
            }
        }
    }

    switch (opcode) {
    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value == rhs->value);

    case EJS_OP_COMPARE_NE: case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejsCreateBoolean(ejs, !(lhs->value == rhs->value));

    case EJS_OP_COMPARE_LT:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value < rhs->value);

    case EJS_OP_COMPARE_LE:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value <= rhs->value);

    case EJS_OP_COMPARE_GT:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value > rhs->value);

    case EJS_OP_COMPARE_GE:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs->value >= rhs->value);

    case EJS_OP_COMPARE_NOT_ZERO:
        return (EjsVar*) ((lhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ((lhs->value == 0) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) ejs->falseValue;

    /*
        Unary operators
     */
    case EJS_OP_NEG:
        return (EjsVar*) ejsCreateNumber(ejs, - (MprNumber) lhs->value);

    case EJS_OP_LOGICAL_NOT:
        return (EjsVar*) ejsCreateBoolean(ejs, (MprNumber) !fixed(lhs->value));

    case EJS_OP_NOT:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (~fixed(lhs->value)));

    /*
        Binary operators
     */
    case EJS_OP_ADD:
        return (EjsVar*) ejsCreateDate(ejs, lhs->value + rhs->value);

    case EJS_OP_AND:
        return (EjsVar*) ejsCreateDate(ejs, (MprNumber) (fixed(lhs->value) & fixed(rhs->value)));

    case EJS_OP_DIV:
#if BLD_FEATURE_FLOATING_POINT
        if (rhs->value == 0) {
            ejsThrowArithmeticError(ejs, "Divisor is zero");
            return 0;
        }
#endif
        return (EjsVar*) ejsCreateDate(ejs, lhs->value / rhs->value);

    case EJS_OP_MUL:
        return (EjsVar*) ejsCreateDate(ejs, lhs->value * rhs->value);

    case EJS_OP_OR:
        return (EjsVar*) ejsCreateDate(ejs, (MprNumber) (fixed(lhs->value) | fixed(rhs->value)));

    case EJS_OP_REM:
#if BLD_FEATURE_FLOATING_POINT
        if (rhs->value == 0) {
            ejsThrowArithmeticError(ejs, "Divisor is zero");
            return 0;
        }
#endif
        return (EjsVar*) ejsCreateDate(ejs, (MprNumber) (fixed(lhs->value) % fixed(rhs->value)));

    case EJS_OP_SHL:
        return (EjsVar*) ejsCreateDate(ejs, (MprNumber) (fixed(lhs->value) << fixed(rhs->value)));

    case EJS_OP_SHR:
        return (EjsVar*) ejsCreateDate(ejs, (MprNumber) (fixed(lhs->value) >> fixed(rhs->value)));

    case EJS_OP_SUB:
        return (EjsVar*) ejsCreateDate(ejs, (MprNumber) (fixed(lhs->value) - fixed(rhs->value)));

    case EJS_OP_USHR:
        return (EjsVar*) ejsCreateDate(ejs, (MprNumber) (fixed(lhs->value) >> fixed(rhs->value)));

    case EJS_OP_XOR:
        return (EjsVar*) ejsCreateDate(ejs, (MprNumber) (fixed(lhs->value) ^ fixed(rhs->value)));

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->obj.var.type->qname.name);
        return 0;
    }
    /* Should never get here */
}


/*
    Serialize using JSON encoding. This uses the ISO date format of UTC time.
 */
static EjsVar *date_toJSON(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;
    char        *base, *str;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    base = mprFormatTime(ejs, "%Y-%m-%dT%H:%M:%S", &tm);
    str = mprAsprintf(ejs, -1, "\"%sZ\"", base);
    mprFree(base);
    return (EjsVar*) ejsCreateStringAndFree(ejs, str);
}


/*
    Date constructor
        Date()
        Date(milliseconds)
        Date(dateString)
        Date(year, month, date, hour, minute, second, msec)
        @param milliseconds Integer representing milliseconds since 1 January 1970 00:00:00 UTC.
        @param dateString String date value in a format recognized by parse().
        @param year Integer value for the year. Should be a Four digit year (e.g. 1998).
        @param month Integer month value (0-11)
        @param date Integer date of the month (1-31)
        @param hour Integer hour value (0-23)
        @param minute Integer minute value (0-59)
        @param second Integer second value (0-59)
        @param msec Integer millisecond value (0-999)
*/
static EjsVar *date_Date(Ejs *ejs, EjsDate *date, int argc, EjsVar **argv)
{
    EjsArray    *args;
    EjsVar      *vp;
    struct tm   tm;
    int         year;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = (EjsArray*) argv[0];

    if (args->length == 0) {
        /* Now */
        date->value = mprGetTime(ejs);

    } else if (args->length == 1) {
        vp = ejsGetProperty(ejs, (EjsVar*) args, 0);
        if (ejsIsNumber(vp)) {
            /* Milliseconds */
            date->value = ejsGetNumber(vp);

        } else if (ejsIsString(vp)) {
            if (mprParseTime(ejs, &date->value, ejsGetString(vp), MPR_LOCAL_TIMEZONE, NULL) < 0) {
                ejsThrowArgError(ejs, "Can't parse date string: %s", ejsGetString(vp));
                return 0;
            }
        } else if (ejsIsDate(vp)) {
            date->value = ((EjsDate*) vp)->value;

        } else {
            ejsThrowArgError(ejs, "Can't construct date from this argument");
        }

    } else {
        /* Date(year, month, date, hour, minute, second, msec) or any portion thereof */
        memset(&tm, 0, sizeof(tm));
        tm.tm_isdst = -1;
        vp = ejsGetProperty(ejs, (EjsVar*) args, 0);
        year = getNumber(ejs, vp);
        if (0 <= year && year < 100) {
            year += 1900;
        }
        tm.tm_year = year - 1900;
        if (args->length > 1) {
            vp = ejsGetProperty(ejs, (EjsVar*) args, 1);
            tm.tm_mon = getNumber(ejs, vp);
        }
        if (args->length > 2) {
            vp = ejsGetProperty(ejs, (EjsVar*) args, 2);
            tm.tm_mday = getNumber(ejs, vp);
        } else {
            tm.tm_mday = 1;
        }
        if (args->length > 3) {
            vp = ejsGetProperty(ejs, (EjsVar*) args, 3);
            tm.tm_hour = getNumber(ejs, vp);
        }
        if (args->length > 4) {
            vp = ejsGetProperty(ejs, (EjsVar*) args, 4);
            tm.tm_min = getNumber(ejs, vp);
        }
        if (args->length > 5) {
            vp = ejsGetProperty(ejs, (EjsVar*) args, 5);
            tm.tm_sec = getNumber(ejs, vp);
        }
        date->value = mprMakeTime(ejs, &tm);
        if (date->value == -1) {
            ejsThrowArgError(ejs, "Can't construct date from this argument");
        } else if (args->length > 6) {
            vp = ejsGetProperty(ejs, (EjsVar*) args, 6);
            date->value += getNumber(ejs, vp);
        }
    }
    return (EjsVar*) date;
}


/*
    function get day(): Number
    Range: 0-6, where 0 is Sunday
 */
static EjsVar *date_day(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_wday);
}


/*
    function set day(day: Number): Void
    Range: 0-6, where 0 is Sunday
*/
static EjsVar *date_set_day(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;
    MprTime     dayDiff, day;

    day = ejsGetNumber(argv[0]);
#if UNUSED
    if (day < 0 || day > 6) {
        ejsThrowArgError(ejs, "Bad day. Range 0-6");
        return 0;
    }
#endif
    mprDecodeLocalTime(ejs, &tm, dp->value);
    dayDiff = day - tm.tm_wday;
    dp->value += dayDiff * 86400 * MPR_TICKS_PER_SEC;
    return 0;
}


/*
    function get dayOfYear(): Number
    Return day of year (0 - 365)
 */
static EjsVar *date_dayOfYear(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_yday);
}


/*
    function set dayOfYear(day: Number): Void
    Set the day of year (0 - 365)
 */
static EjsVar *date_set_dayOfYear(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;
    MprTime     dayDiff, day;

    day = ejsGetNumber(argv[0]);
#if UNUSED
    if (day < 0 || day > 365) {
        ejsThrowArgError(ejs, "Bad day. Range 0-365");
        return 0;
    }
#endif
    mprDecodeLocalTime(ejs, &tm, dp->value);
    dayDiff = day - tm.tm_yday;
    dp->value += dayDiff * 86400 * MPR_TICKS_PER_SEC;
    return 0;
}


/*
    function get date(): Number
    Return day of month (1-31)
 */
static EjsVar *date_date(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_mday);
}


/*
    function set date(date: Number): Void
    Range day of month (1-31)
 */
static EjsVar *date_set_date(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;
    MprTime     dayDiff, day;

    day = ejsGetNumber(argv[0]);
#if UNUSED
    if (day < 1 || day > 31) {
        ejsThrowArgError(ejs, "Bad day. Range 1-31");
        return 0;
    }
#endif
    mprDecodeLocalTime(ejs, &tm, dp->value);
    dayDiff = day - tm.tm_mday;
    dp->value += dayDiff * 86400 * MPR_TICKS_PER_SEC;
    return 0;
}


/*
    function get elapsed(): Number
    Get the elapsed time in milliseconds since the Date object was constructed
 */
static EjsVar *date_elapsed(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, mprGetElapsedTime(ejs, dp->value));
}


/*
    function format(layout: String): String
 */
static EjsVar *date_format(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateStringAndFree(ejs, mprFormatTime(ejs, ejsGetString(argv[0]), &tm));
}


/*
    function formatUTC(layout: String): String
 */
static EjsVar *date_formatUTC(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateStringAndFree(ejs, mprFormatTime(ejs, ejsGetString(argv[0]), &tm));
}


/*
    function get fullYear(): Number
    Return year in 4 digits
 */
static EjsVar *date_fullYear(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_year + 1900);
}


/*
    function set fullYear(year: Number): void
    Update the year component using a 4 digit year
 */
static EjsVar *date_set_fullYear(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    tm.tm_year = ejsGetNumber(argv[0]) - 1900;
    dp->value = mprMakeTime(ejs, &tm);
    return 0;
}


/**
    Return the number of minutes between the local computer time and Coordinated Universal Time.
    @return Integer containing the number of minutes between UTC and local time. The offset is positive if
    local time is behind UTC and negative if it is ahead. E.g. American PST is UTC-8 so 420/480 will be retured
    depending on if daylight savings is in effect.

    function getTimezoneOffset(): Number
*/
static EjsVar *date_getTimezoneOffset(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, -mprGetTimeZoneOffset(ejs, dp->value) / (MPR_TICKS_PER_SEC * 60));
}


/*
    function getUTCDate(): Number
    Range: 0-31
 */
static EjsVar *date_getUTCDate(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_mday);
}


/*
    function getUTCDay(): Number
    Range: 0-6
 */
static EjsVar *date_getUTCDay(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_wday);
}


/*
    function getUTCFullYear(): Number
    Range: 4 digits
 */
static EjsVar *date_getUTCFullYear(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_year + 1900);
}


/*
    function getUTCHours(): Number
    Range: 0-23
 */
static EjsVar *date_getUTCHours(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_hour);
}


/*
    function getUTCMilliseconds(): Number
    Range: 0-999
 */
static EjsVar *date_getUTCMilliseconds(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ((int64) dp->value) % MPR_TICKS_PER_SEC);
}


/*
    function getUTCMinutes(): Number
    Range: 0-31
 */
static EjsVar *date_getUTCMinutes(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_min);
}


/*
    function getUTCMonth(): Number
    Range: 1-12
 */
static EjsVar *date_getUTCMonth(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_mon);
}


/*
    function getUTCSeconds(): Number
    Range: 0-59
 */
static EjsVar *date_getUTCSeconds(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_sec);
}


/*
    function get hours(): Number
    Return hour of day (0-23)
 */
static EjsVar *date_hours(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_hour);
}


/*
    function set hours(hour: Number): void
    Update the hour of the day using a 0-23 hour
 */
static EjsVar *date_set_hours(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    tm.tm_hour = ejsGetNumber(argv[0]);
    dp->value = mprMakeTime(ejs, &tm);
    return 0;
}


/*
    function get milliseconds(): Number
 */
static EjsVar *date_milliseconds(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ((int64) dp->value) % MPR_TICKS_PER_SEC);
}


/*
    function set milliseconds(ms: Number): void
 */
static EjsVar *date_set_milliseconds(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    dp->value = (dp->value / MPR_TICKS_PER_SEC  * MPR_TICKS_PER_SEC) + ejsGetNumber(argv[0]);
    return 0;
}


/*
    function get minutes(): Number
 */
static EjsVar *date_minutes(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_min);
}


/*
    function set minutes(min: Number): void
 */
static EjsVar *date_set_minutes(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    tm.tm_min = ejsGetNumber(argv[0]);
    dp->value = mprMakeTime(ejs, &tm);
    return 0;
}


/*
    function get month(): Number
    Get the month (0-11)
 */
static EjsVar *date_month(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_mon);
}


/*
    function set month(month: Number): void
 */
static EjsVar *date_set_month(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    tm.tm_mon = ejsGetNumber(argv[0]);
    dp->value = mprMakeTime(ejs, &tm);
    return 0;
}


/*
    function nextDay(inc: Number = 1): Date
 */
static EjsVar *date_nextDay(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    MprTime     inc;

    if (argc == 1) {
        inc = ejsGetNumber(argv[0]);
    } else {
        inc = 1;
    }
    return (EjsVar*) ejsCreateDate(ejs, dp->value + (inc * 86400 * 1000));
}


/*
    function now(): Number
 */
static EjsVar *date_now(Ejs *ejs, EjsDate *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, mprGetTime(ejs));
}


/*
    static function parse(arg: String): Date
 */
static EjsVar *date_parse(Ejs *ejs, EjsDate *unused, int argc, EjsVar **argv)
{
    MprTime     when;

    if (mprParseTime(ejs, &when, ejsGetString(argv[0]), MPR_LOCAL_TIMEZONE, NULL) < 0) {
        ejsThrowArgError(ejs, "Can't parse date string: %s", ejsGetString(argv[0]));
        return 0;
    }
    return (EjsVar*) ejsCreateNumber(ejs, when);
}


/*
    static function parseDate(arg: String, defaultDate: Date = null): Date
 */
static EjsVar *date_parseDate(Ejs *ejs, EjsDate *unused, int argc, EjsVar **argv)
{
    struct tm   tm, *defaults;
    MprTime     when;

    if (argc >= 2) {
        mprDecodeLocalTime(ejs, &tm, ((EjsDate*) argv[1])->value);
        defaults = &tm;
    } else {
        defaults = 0;
    }
    if (mprParseTime(ejs, &when, ejsGetString(argv[0]), MPR_LOCAL_TIMEZONE, defaults) < 0) {
        ejsThrowArgError(ejs, "Can't parse date string: %s", ejsGetString(argv[0]));
        return 0;
    }
    return (EjsVar*) ejsCreateDate(ejs, when);
}


/*
    static function parseUTCDate(arg: String, defaultDate: Date = null): Date
 */
static EjsVar *date_parseUTCDate(Ejs *ejs, EjsDate *unused, int argc, EjsVar **argv)
{
    struct tm   tm, *defaults;
    MprTime     when;

    if (argc >= 2) {
        mprDecodeUniversalTime(ejs, &tm, ((EjsDate*) argv[1])->value);
        defaults = &tm;
    } else {
        defaults = 0;
    }
    if (mprParseTime(ejs, &when, ejsGetString(argv[0]), MPR_UTC_TIMEZONE, defaults) < 0) {
        ejsThrowArgError(ejs, "Can't parse date string: %s", ejsGetString(argv[0]));
        return 0;
    }
    return (EjsVar*) ejsCreateDate(ejs, when);
}


/*
    function get seconds(): Number
    Get seconds (0-59)
 */
static EjsVar *date_seconds(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_sec);
}


/*
    function set seconds(sec: Number): void
 */
static EjsVar *date_set_seconds(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    tm.tm_sec = ejsGetNumber(argv[0]);
    dp->value = mprMakeTime(ejs, &tm);
    return 0;
}


/*
    function setUTCDate(date: Number): Void
    Range month (1-31)
 */
static EjsVar *date_setUTCDate(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;
    MprTime     dayDiff, day;

    day = ejsGetNumber(argv[0]);
#if UNUSED
    if (day < 1 || day > 31) {
        ejsThrowArgError(ejs, "Bad day. Range 1-31");
        return 0;
    }
#endif
    mprDecodeUniversalTime(ejs, &tm, dp->value);
    dayDiff = day - tm.tm_mday;
    dp->value += dayDiff * 86400 * MPR_TICKS_PER_SEC;
    return 0;
}


/*
   function setUTCFullYear(y: Number): void
 */
static EjsVar *date_setUTCFullYear(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    tm.tm_year = ejsGetNumber(argv[0]) - 1900;
    dp->value = mprMakeUniversalTime(ejs, &tm);
    return 0;
}


/*
    function setUTCHours(h: Number): void
 */
static EjsVar *date_setUTCHours(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    tm.tm_hour = ejsGetNumber(argv[0]);
    dp->value = mprMakeUniversalTime(ejs, &tm);
    return 0;
}


/*
    function setUTCMilliseconds(ms: Number): void
 */
static EjsVar *date_setUTCMilliseconds(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    /* Same as set_milliseconds */
    dp->value = (dp->value / MPR_TICKS_PER_SEC  * MPR_TICKS_PER_SEC) + ejsGetNumber(argv[0]);
    return 0;
}


/*
    function setUTCMinutes(min: Number): void
 */
static EjsVar *date_setUTCMinutes(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    tm.tm_min = ejsGetNumber(argv[0]);
    dp->value = mprMakeUniversalTime(ejs, &tm);
    return 0;
}


/*
    function setUTCMonth(mon: Number): void
 */
static EjsVar *date_setUTCMonth(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    tm.tm_mon = ejsGetNumber(argv[0]);
    dp->value = mprMakeUniversalTime(ejs, &tm);
    return 0;
}


/*
    function setUTCSeconds(sec: Number, msec: Number = null): void
 */
static EjsVar *date_setUTCSeconds(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    tm.tm_sec = ejsGetNumber(argv[0]);
    dp->value = mprMakeUniversalTime(ejs, &tm);
    if (argc >= 2) {
        dp->value = (dp->value / MPR_TICKS_PER_SEC  * MPR_TICKS_PER_SEC) + ejsGetNumber(argv[1]);
    }
    return 0;
}


/*
    Get the number of millsecs since Jan 1, 1970 UTC.
    function get time(): Number
 */
static EjsVar *date_time(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, dp->value);
}


/*
    function set time(value: Number): Number
 */
static EjsVar *date_set_time(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    dp->value = ejsGetNumber(argv[0]);
    return 0;
}


/**
    Return an ISO formatted date string.
    Sample format: "2006-12-15T23:45:09.33-08:00"
    function toISOString(): String
*/
static EjsVar *date_toISOString(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    EjsVar      *vp;
    struct tm   tm;
    char        *base, *str;

    mprDecodeUniversalTime(ejs, &tm, dp->value);
    base = mprFormatTime(ejs, "%Y-%m-%dT%H:%M:%S", &tm);
    str = mprAsprintf(ejs, -1, "%s.%03dZ", base, dp->value % MPR_TICKS_PER_SEC);
    vp = (EjsVar*) ejsCreateStringAndFree(ejs, str);
    mprFree(base);
    return vp;
}


/*
    override native function toString(): String
 */
static EjsVar *date_toString(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    return castDate(ejs, dp, ejs->stringType);
}


/*
    Construct a date from UTC values
    function UTC(year, month, date, hour = 0, minute = 0, second = 0, msec = 0): Number
 */
static EjsVar *date_UTC(Ejs *ejs, EjsDate *unused, int argc, EjsVar **argv)
{
    EjsDate     *dp;
    struct tm   tm;
    int         year;

    memset(&tm, 0, sizeof(tm));
    year = getNumber(ejs, argv[0]);
    if (year < 100) {
        year += 1900;
    }
    tm.tm_year = year - 1900;
    if (argc > 1) {
        tm.tm_mon = getNumber(ejs, argv[1]);
    }
    if (argc > 2) {
        tm.tm_mday = getNumber(ejs, argv[2]);
    }
    if (argc > 3) {
        tm.tm_hour = getNumber(ejs, argv[3]);
    }
    if (argc > 4) {
        tm.tm_min = getNumber(ejs, argv[4]);
    }
    if (argc > 5) {
        tm.tm_sec = getNumber(ejs, argv[5]);
    }
    dp = ejsCreateDate(ejs, mprMakeUniversalTime(ejs, &tm));
    if (argc > 6) {
        dp->value += getNumber(ejs, argv[6]);
    }
    return (EjsVar*) ejsCreateNumber(ejs, dp->value);
}


/*
    function get year(): Number
 */
static EjsVar *date_year(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    return (EjsVar*) ejsCreateNumber(ejs, tm.tm_year + 1900);
}


/*
    function set year(year: Number): void
 */
static EjsVar *date_set_year(Ejs *ejs, EjsDate *dp, int argc, EjsVar **argv)
{
    struct tm   tm;
    MprNumber   value;

    mprDecodeLocalTime(ejs, &tm, dp->value);
    tm.tm_year = ejsGetNumber(argv[0]) - 1900;
    value = mprMakeTime(ejs, &tm);
    if (value == -1) {
        ejsThrowArgError(ejs, "Invalid year");
    } else {
        dp->value = value;
    }
    return 0;
}

/*
    Create an initialized date object. Set to the current time if value is zero.
 */

EjsDate *ejsCreateDate(Ejs *ejs, MprTime value)
{
    EjsDate *vp;

    vp = (EjsDate*) ejsCreateVar(ejs, ejs->dateType, 0);
    if (vp != 0) {
        vp->value = value;
    }
    return vp;
}


void ejsCreateDateType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Date"), ejs->objectType, sizeof(EjsDate),
        ES_Date, ES_Date_NUM_CLASS_PROP, ES_Date_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
    ejs->dateType = type;

    /*
        Define the helper functions.
     */
    type->helpers->castVar = (EjsCastVarHelper) castDate;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokeDateOperator;
}


void ejsConfigureDateType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->dateType;

    ejsBindMethod(ejs, type, ES_Date_Date, (EjsNativeFunction) date_Date);
    ejsBindMethod(ejs, type, ES_Date_day, (EjsNativeFunction) date_day);
    ejsBindMethod(ejs, type, ES_Date_set_day, (EjsNativeFunction) date_set_day);
    ejsBindMethod(ejs, type, ES_Date_dayOfYear, (EjsNativeFunction) date_dayOfYear);
    ejsBindMethod(ejs, type, ES_Date_set_dayOfYear, (EjsNativeFunction) date_set_dayOfYear);
    ejsBindMethod(ejs, type, ES_Date_date, (EjsNativeFunction) date_date);
    ejsBindMethod(ejs, type, ES_Date_set_date, (EjsNativeFunction) date_set_date);
    ejsBindMethod(ejs, type, ES_Date_elapsed, (EjsNativeFunction) date_elapsed);
    ejsBindMethod(ejs, type, ES_Date_format, (EjsNativeFunction) date_format);
    ejsBindMethod(ejs, type, ES_Date_formatUTC, (EjsNativeFunction) date_formatUTC);
    ejsBindMethod(ejs, type, ES_Date_fullYear, (EjsNativeFunction) date_fullYear);
    ejsBindMethod(ejs, type, ES_Date_set_fullYear, (EjsNativeFunction) date_set_fullYear);
    ejsBindMethod(ejs, type, ES_Date_getTimezoneOffset, (EjsNativeFunction) date_getTimezoneOffset); 
    ejsBindMethod(ejs, type, ES_Date_getUTCDate, (EjsNativeFunction) date_getUTCDate);
    ejsBindMethod(ejs, type, ES_Date_getUTCDay, (EjsNativeFunction) date_getUTCDay);
    ejsBindMethod(ejs, type, ES_Date_getUTCFullYear, (EjsNativeFunction) date_getUTCFullYear);
    ejsBindMethod(ejs, type, ES_Date_getUTCHours, (EjsNativeFunction) date_getUTCHours);
    ejsBindMethod(ejs, type, ES_Date_getUTCMilliseconds, (EjsNativeFunction) date_getUTCMilliseconds);
    ejsBindMethod(ejs, type, ES_Date_getUTCMinutes, (EjsNativeFunction) date_getUTCMinutes);
    ejsBindMethod(ejs, type, ES_Date_getUTCMonth, (EjsNativeFunction) date_getUTCMonth);
    ejsBindMethod(ejs, type, ES_Date_getUTCSeconds, (EjsNativeFunction) date_getUTCSeconds);
    ejsBindMethod(ejs, type, ES_Date_hours, (EjsNativeFunction) date_hours);
    ejsBindMethod(ejs, type, ES_Date_set_hours, (EjsNativeFunction) date_set_hours);
    ejsBindMethod(ejs, type, ES_Date_milliseconds, (EjsNativeFunction) date_milliseconds);
    ejsBindMethod(ejs, type, ES_Date_set_milliseconds, (EjsNativeFunction) date_set_milliseconds);
    ejsBindMethod(ejs, type, ES_Date_minutes, (EjsNativeFunction) date_minutes);
    ejsBindMethod(ejs, type, ES_Date_set_minutes, (EjsNativeFunction) date_set_minutes);
    ejsBindMethod(ejs, type, ES_Date_month, (EjsNativeFunction) date_month);
    ejsBindMethod(ejs, type, ES_Date_set_month, (EjsNativeFunction) date_set_month);
    ejsBindMethod(ejs, type, ES_Date_nextDay, (EjsNativeFunction) date_nextDay);
    ejsBindMethod(ejs, type, ES_Date_now, (EjsNativeFunction) date_now);
    ejsBindMethod(ejs, type, ES_Date_parse, (EjsNativeFunction) date_parse);
    ejsBindMethod(ejs, type, ES_Date_parseDate, (EjsNativeFunction) date_parseDate);
    ejsBindMethod(ejs, type, ES_Date_parseUTCDate, (EjsNativeFunction) date_parseUTCDate);
    ejsBindMethod(ejs, type, ES_Date_seconds, (EjsNativeFunction) date_seconds);
    ejsBindMethod(ejs, type, ES_Date_set_seconds, (EjsNativeFunction) date_set_seconds);
    ejsBindMethod(ejs, type, ES_Date_setUTCDate, (EjsNativeFunction) date_setUTCDate);
    ejsBindMethod(ejs, type, ES_Date_setUTCFullYear, (EjsNativeFunction) date_setUTCFullYear);
    ejsBindMethod(ejs, type, ES_Date_setUTCHours, (EjsNativeFunction) date_setUTCHours);
    ejsBindMethod(ejs, type, ES_Date_setUTCMilliseconds, (EjsNativeFunction) date_setUTCMilliseconds);
    ejsBindMethod(ejs, type, ES_Date_setUTCMinutes, (EjsNativeFunction) date_setUTCMinutes);
    ejsBindMethod(ejs, type, ES_Date_setUTCMonth, (EjsNativeFunction) date_setUTCMonth);
    ejsBindMethod(ejs, type, ES_Date_setUTCSeconds, (EjsNativeFunction) date_setUTCSeconds);
    ejsBindMethod(ejs, type, ES_Date_time, (EjsNativeFunction) date_time);
    ejsBindMethod(ejs, type, ES_Date_set_time, (EjsNativeFunction) date_set_time);
    ejsBindMethod(ejs, type, ES_Date_toISOString, (EjsNativeFunction) date_toISOString);
    ejsBindMethod(ejs, type, ES_Date_UTC, (EjsNativeFunction) date_UTC);
    ejsBindMethod(ejs, type, ES_Date_year, (EjsNativeFunction) date_year);
    ejsBindMethod(ejs, type, ES_Date_set_year, (EjsNativeFunction) date_set_year);

    ejsBindMethod(ejs, type, ES_Object_toString, (EjsNativeFunction) date_toString);
    ejsBindMethod(ejs, type, ES_Object_toJSON, (EjsNativeFunction) date_toJSON);
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://www.embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsDate.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsError.c"
 */
/************************************************************************/

/**
 *  ejsError.c - Error Exception class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Cast the operand to the specified type
 *
 *  intrinsic function cast(type: Type) : Object
 */

static EjsVar *castError(Ejs *ejs, EjsError *vp, EjsType *type)
{
    EjsVar      *sp;
    char        *buf;

    switch (type->id) {

    case ES_Boolean:
        return (EjsVar*) ejsCreateBoolean(ejs, 1);

    case ES_String:
        if ((buf = mprAsprintf(ejs, -1,
                "%s Exception: %s\nStack:\n%s\n", vp->obj.var.type->qname.name, vp->message, vp->stack)) == NULL) {
            ejsThrowMemoryError(ejs);
        }
        sp = (EjsVar*) ejsCreateString(ejs, buf);
        mprFree(buf);
        return sp;

    default:
        ejsThrowTypeError(ejs, "Unknown type");
        return 0;
    }
}


/*
 *  Get a property.
 */
static EjsVar *getErrorProperty(Ejs *ejs, EjsError *error, int slotNum)
{
    switch (slotNum) {
    case ES_Error_stack:
        return (EjsVar*) ejsCreateString(ejs, error->stack);

    case ES_Error_message:
        return (EjsVar*) ejsCreateString(ejs, error->message);
    }
    return (ejs->objectHelpers->getProperty)(ejs, (EjsVar*) error, slotNum);
}


/*
 *  Lookup a property.
 */
static int lookupErrorProperty(Ejs *ejs, EjsError *error, EjsName *qname)
{
    if (strcmp(qname->name, "message") == 0) {
        return ES_Error_message;
    }
    if (strcmp(qname->name, "stack") == 0) {
        return ES_Error_stack;
    }
    return -1;
}


/*
 *  Error Constructor and constructor for all the core error classes.
 *
 *  public function Error(message: String = null)
 */
static EjsVar *errorConstructor(Ejs *ejs, EjsError *error, int argc,  EjsVar **argv)
{
    mprFree(error->message);
    if (argc == 0) {
        error->message = mprStrdup(error, "");
    } else {
        error->message = mprStrdup(error, ejsGetString(argv[0]));
    }
    mprFree(error->stack);
    ejsFormatStack(ejs, error);
    return (EjsVar*) error;
}


static EjsVar *getCode(Ejs *ejs, EjsError *vp, int argc,  EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, vp->code);
}


static EjsVar *setCode(Ejs *ejs, EjsError *vp, int argc,  EjsVar **argv)
{
    vp->code = ejsGetInt(argv[0]);
    return 0;
}




static EjsType *createErrorType(Ejs *ejs, cchar *name, int numClassProp, int numInstanceProp)
{
    EjsType     *type, *baseType;
    EjsName     qname;
    int         flags;

    flags = EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_DYNAMIC_INSTANCE | EJS_ATTR_OBJECT_HELPERS | 
        EJS_ATTR_HAS_CONSTRUCTOR | EJS_ATTR_NO_BIND;
    baseType = (ejs->errorType) ? ejs->errorType: ejs->objectType;
    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, name), baseType, sizeof(EjsError), 
        ES_Error, numClassProp, numInstanceProp, flags);
    type->helpers->castVar = (EjsCastVarHelper) castError;
    type->helpers->getProperty = (EjsGetPropertyHelper) getErrorProperty;
    type->helpers->lookupProperty = (EjsLookupPropertyHelper) lookupErrorProperty;
    return type;
}


static void defineType(Ejs *ejs, int slotNum)
{
    EjsType     *type;

    type = ejsGetType(ejs, slotNum);
    ejsBindMethod(ejs, type, type->block.numInherited, (EjsNativeFunction) errorConstructor);
}


void ejsCreateErrorType(Ejs *ejs)
{
    ejs->errorType = createErrorType(ejs, "Error",  ES_Error_NUM_CLASS_PROP, ES_Error_NUM_INSTANCE_PROP);

    createErrorType(ejs, "ArgError", ES_ArgError_NUM_CLASS_PROP, ES_ArgError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "ArithmeticError", ES_ArithmeticError_NUM_CLASS_PROP, ES_ArithmeticError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "AssertError", ES_AssertError_NUM_CLASS_PROP, ES_AssertError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "InstructionError", ES_InstructionError_NUM_CLASS_PROP, ES_InstructionError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "IOError", ES_IOError_NUM_CLASS_PROP, ES_IOError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "InternalError", ES_InternalError_NUM_CLASS_PROP, ES_InternalError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "MemoryError", ES_MemoryError_NUM_CLASS_PROP, ES_MemoryError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "OutOfBoundsError", ES_OutOfBoundsError_NUM_CLASS_PROP, ES_OutOfBoundsError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "ReferenceError", ES_ReferenceError_NUM_CLASS_PROP, ES_ReferenceError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "ResourceError", ES_ResourceError_NUM_CLASS_PROP, ES_ResourceError_NUM_INSTANCE_PROP);
#if ES_SecurityError
    createErrorType(ejs, "SecurityError", ES_SecurityError_NUM_CLASS_PROP, ES_SecurityError_NUM_INSTANCE_PROP);
#endif
    createErrorType(ejs, "StateError", ES_StateError_NUM_CLASS_PROP, ES_StateError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "SyntaxError", ES_SyntaxError_NUM_CLASS_PROP, ES_SyntaxError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "TypeError", ES_TypeError_NUM_CLASS_PROP, ES_TypeError_NUM_INSTANCE_PROP);
    createErrorType(ejs, "URIError", ES_URIError_NUM_CLASS_PROP, ES_URIError_NUM_INSTANCE_PROP);
}


void ejsConfigureErrorType(Ejs *ejs)
{
    defineType(ejs, ES_Error);

    ejsBindMethod(ejs, ejs->errorType, ES_Error_code, (EjsNativeFunction) getCode);
    ejsBindMethod(ejs, ejs->errorType, ES_Error_set_code, (EjsNativeFunction) setCode);

    defineType(ejs, ES_ArgError);
    defineType(ejs, ES_ArithmeticError);
    defineType(ejs, ES_AssertError);
    defineType(ejs, ES_InstructionError);
    defineType(ejs, ES_IOError);
    defineType(ejs, ES_InternalError);
    defineType(ejs, ES_MemoryError);
    defineType(ejs, ES_OutOfBoundsError);
    defineType(ejs, ES_ReferenceError);
    defineType(ejs, ES_ResourceError);
#if ES_SecurityError
    defineType(ejs, ES_SecurityError);
#endif
    defineType(ejs, ES_StateError);
    defineType(ejs, ES_SyntaxError);
    defineType(ejs, ES_TypeError);
    defineType(ejs, ES_URIError);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsError.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsFunction.c"
 */
/************************************************************************/

/**
 *  ejsFunction.c - Function class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Create a function object.
 */
static EjsFunction *createFunction(Ejs *ejs, EjsType *type, int numSlots)
{
    EjsFunction     *fun;

    /*
     *  Note: Functions are not pooled, frames are.
     */
    fun = (EjsFunction*) ejsCreateObject(ejs, ejs->functionType, 0);
    if (fun == 0) {
        return 0;
    }
    fun->block.obj.var.isFunction = 1;
    fun->slotNum = -1;
    fun->nextSlot = -1;
    return fun;
}


/*
 *  Cast the operand to the specified type
 *
 *  intrinsic function cast(type: Type) : Object
 */
static EjsVar *castFunction(Ejs *ejs, EjsFunction *vp, EjsType *type)
{
    switch (type->id) {
    case ES_String:
        return (EjsVar*) ejsCreateString(ejs, "[function Function]");

    case ES_Number:
        return (EjsVar*) ejs->nanValue;

    case ES_Boolean:
        return (EjsVar*) ejs->trueValue;
            
    default:
        ejsThrowTypeError(ejs, "Can't cast type \"%s\"", type->qname.name);
        return 0;
    }
    return 0;
}


static EjsFunction *cloneFunctionVar(Ejs *ejs, EjsFunction *src, bool deep)
{
    EjsFunction     *dest;

    dest = (EjsFunction*) ejsCopyBlock(ejs, &src->block, deep);
    if (dest == 0) {
        return 0;
    }
    dest->properties = src->properties;
    dest->prototype = src->prototype;

    dest->body.code = src->body.code;
    dest->resultType = src->resultType;
    dest->thisObj = src->thisObj;
    dest->owner = src->owner;
    dest->slotNum = src->slotNum;
    dest->numArgs = src->numArgs;
    dest->numDefault = src->numDefault;
    dest->nextSlot = src->nextSlot;

    /*
     *  OPT
     */
    dest->getter = src->getter;
    dest->setter = src->setter;
    dest->staticMethod = src->staticMethod;
    dest->constructor = src->constructor;
    dest->hasReturn = src->hasReturn;
    dest->isInitializer = src->isInitializer;
    dest->literalGetter = src->literalGetter;
    dest->override = src->override;
    dest->rest = src->rest;
    dest->fullScope = src->fullScope;
    dest->nativeProc = src->nativeProc;
    dest->lang = src->lang;
    dest->isFrame = src->isFrame;
    return dest;
}


static void destroyFunction(Ejs *ejs, EjsFunction *fun)
{
    ejsFreeVar(ejs, (EjsVar*) fun, fun->isFrame ? ES_Frame : ES_Function);
}

void ejsMarkFunction(Ejs *ejs, EjsVar *parent, EjsFunction *fun)
{
    ejsMarkBlock(ejs, parent, (EjsBlock*) fun);

    if (fun->prototype) {
        ejsMarkVar(ejs, parent, (EjsVar*) fun->prototype);
    }
    if (fun->properties) {
        ejsMarkVar(ejs, parent, (EjsVar*) fun->properties);
    }
    if (fun->owner) {
        ejsMarkVar(ejs, parent, fun->owner);
    }
    if (fun->thisObj) {
        ejsMarkVar(ejs, parent, fun->thisObj);
    }
    if (fun->isFrame) {
        ejsMarkVar(ejs, parent, (EjsObj*) ((EjsFrame*) fun)->caller);
    }
}


static EjsVar *applyFunction(Ejs *ejs, EjsFunction *fun, int argc, EjsVar **argv)
{
    EjsArray        *args;
    EjsVar          *save, *result;
    
    mprAssert(argc == 2);
    args = (EjsArray*) argv[1];
    mprAssert(ejsIsArray(args));

    save = fun->thisObj;
    fun->thisObj = 0;

    result =  ejsRunFunction(ejs, fun, argv[0], args->length, args->data);

    fun->thisObj = save;
    return result;
}


static EjsVar *callFunctionMethod(Ejs *ejs, EjsFunction *fun, int argc, EjsVar **argv)
{
    return applyFunction(ejs, fun, argc, argv);
}


static EjsVar *getObj(Ejs *ejs, EjsFunction *fun)
{
    //  OPT. Compiler could set loading?
    if ((ejs->flags & EJS_FLAG_COMPILER) || fun->loading || fun->isFrame) {
        return (EjsVar*) fun;
    }
    if (fun->properties == 0) {
        /* 
         *  On-Demand creation of Function.* property storage 
         */  
        fun->properties = ejsCreateSimpleObject(ejs);
    }
    return (EjsVar*) fun->properties;
}


static EjsVar *getFunctionProperty(Ejs *ejs, EjsFunction *fun, int slotNum)
{
    if ((ejs->flags & EJS_FLAG_COMPILER) || fun->loading || fun->isFrame) {
        return (ejs->objectHelpers->getProperty)(ejs, (EjsVar*) fun, slotNum);
    } else if (fun->properties) {
        return (ejs->objectHelpers->getProperty)(ejs, (EjsVar*) fun->properties, slotNum);
    }
    return 0;
}


/*
 *  Return the number of properties in the object
 */
static int getFunctionPropertyCount(Ejs *ejs, EjsFunction *fun)
{
    if ((ejs->flags & EJS_FLAG_COMPILER) || fun->loading || fun->isFrame) {
        return (ejs->objectHelpers->getPropertyCount)(ejs, (EjsVar*) fun);
    } else if (fun->properties) {
        return (ejs->objectHelpers->getPropertyCount)(ejs, (EjsVar*) fun->properties);
    }
    return 0;
}


static EjsName getFunctionPropertyName(Ejs *ejs, EjsFunction *fun, int slotNum)
{
    EjsName     qname;

    if ((ejs->flags & EJS_FLAG_COMPILER) || fun->loading || fun->isFrame || fun->properties == 0) {
        return (ejs->objectHelpers->getPropertyName)(ejs, (EjsVar*) fun, slotNum);
    } else {
        return (ejs->objectHelpers->getPropertyName)(ejs, (EjsVar*) fun->properties, slotNum);
    }
    qname.name = 0;
    qname.space = 0;
    return qname;
}


/*
 *  Lookup a property with a namespace qualifier in an object and return the slot if found. Return EJS_ERR if not found.
 */
static int lookupFunctionProperty(struct Ejs *ejs, EjsFunction *fun, EjsName *qname)
{
    int     slotNum;

    if ((ejs->flags & EJS_FLAG_COMPILER) || fun->loading || fun->isFrame) {
        slotNum = (ejs->objectHelpers->lookupProperty)(ejs, (EjsVar*) fun, qname);
    } else if (fun->properties) {
        slotNum = (ejs->objectHelpers->lookupProperty)(ejs, (EjsVar*) fun->properties, qname);
    } else {
        slotNum = -1;
    }
    return slotNum;
}


/**
 *  Set the value of a property.
 *  @param slot If slot is -1, then allocate the next free slot
 *  @return Return the property slot if successful. Return < 0 otherwise.
 */
static int setFunctionProperty(Ejs *ejs, EjsFunction *fun, int slotNum, EjsVar *value)
{
    return (ejs->objectHelpers->setProperty)(ejs, getObj(ejs, fun), slotNum, value);
}


/*
 *  Set the name for a property. Objects maintain a hash lookup for property names. This is hash is created on demand 
 *  if there are more than N properties. If an object is not dynamic, it will use the types name hash. If dynamic, 
 *  then the types name hash will be copied when required. Callers must supply persistent names strings in qname. 
 */
static int setFunctionPropertyName(Ejs *ejs, EjsFunction *fun, int slotNum, EjsName *qname)
{
    return (ejs->objectHelpers->setPropertyName)(ejs, getObj(ejs, fun), slotNum, qname);
}


static int deleteFunctionProperty(Ejs *ejs, EjsFunction *fun, int slotNum)
{
    return (ejs->objectHelpers->deleteProperty)(ejs, getObj(ejs, fun), slotNum);
}

/*
 *  Create a script function. This defines the method traits. It does not create a  method slot. ResultType may
 *  be null to indicate untyped. NOTE: untyped functions may return a result at their descretion.
 */

EjsFunction *ejsCreateFunction(Ejs *ejs, cuchar *byteCode, int codeLen, int numArgs, int numExceptions, EjsType *resultType, 
        int attributes, EjsConst *constants, EjsBlock *scopeChain, int lang)
{
    EjsFunction     *fun;
    EjsCode         *code;

    fun = (EjsFunction*) ejsCreateVar(ejs, ejs->functionType, 0);
    if (fun == 0) {
        return 0;
    }

    if (scopeChain) {
        fun->block.scopeChain = scopeChain;
    }
    fun->numArgs = numArgs;
    fun->resultType = resultType;
    fun->numArgs = numArgs;
    fun->lang = lang;

    /*
     *  When functions are in object literals, we dely setting .getter until the object is actually created.
     *  This enables reading the function without running the getter in the VM.
     */
    if (attributes & EJS_ATTR_LITERAL_GETTER) {
        fun->literalGetter = 1;

    } else if (attributes & EJS_ATTR_GETTER) {
        fun->getter = 1;
    }
    if (attributes & EJS_ATTR_SETTER) {
        fun->setter = 1;
    }
    if (attributes & EJS_ATTR_CONSTRUCTOR) {
        fun->constructor = 1;
    }
    if (attributes & EJS_ATTR_REST) {
        fun->rest = 1;
    }
    if (attributes & EJS_ATTR_INITIALIZER) {
        fun->isInitializer = 1;
    }
    if (attributes & EJS_ATTR_STATIC) {
        fun->staticMethod = 1;
    }
    if (attributes & EJS_ATTR_OVERRIDE) {
        fun->override = 1;
    }
    if (attributes & EJS_ATTR_NATIVE) {
        fun->nativeProc = 1;
    }
    if (attributes & EJS_ATTR_FULL_SCOPE) {
        fun->fullScope = 1;
    }
    if (attributes & EJS_ATTR_HAS_RETURN) {
        fun->hasReturn = 1;
    }
    code = &fun->body.code;
    code->codeLen = codeLen;
    code->byteCode = (uchar*) byteCode;
    code->numHandlers = numExceptions;
    code->constants = constants;
    return fun;
}


void ejsSetNextFunction(EjsFunction *fun, int nextSlot)
{
    fun->nextSlot = nextSlot;
}


void ejsSetFunctionLocation(EjsFunction *fun, EjsVar *obj, int slotNum)
{
    mprAssert(fun);
    mprAssert(obj);

    fun->owner = obj;
    fun->slotNum = slotNum;
}


EjsEx *ejsAddException(EjsFunction *fun, uint tryStart, uint tryEnd, EjsType *catchType, uint handlerStart,
        uint handlerEnd, int numBlocks, int numStack, int flags, int preferredIndex)
{
    EjsEx           *exception;
    EjsCode         *code;
    int             size;

    mprAssert(fun);

    code = &fun->body.code;

    exception = mprAllocObjZeroed(fun, EjsEx);
    if (exception == 0) {
        mprAssert(0);
        return 0;
    }

    exception->flags = flags;
    exception->tryStart = tryStart;
    exception->tryEnd = tryEnd;
    exception->catchType = catchType;
    exception->handlerStart = handlerStart;
    exception->handlerEnd = handlerEnd;
    exception->numBlocks = numBlocks;
    exception->numStack = numStack;

    if (preferredIndex < 0) {
        preferredIndex = code->numHandlers++;
    }

    if (preferredIndex >= code->sizeHandlers) {
        size = code->sizeHandlers + EJS_EX_INC;
        code->handlers = (EjsEx**) mprRealloc(fun, code->handlers, size * sizeof(EjsEx));
        if (code->handlers == 0) {
            mprAssert(0);
            return 0;
        }
        memset(&code->handlers[code->sizeHandlers], 0, EJS_EX_INC * sizeof(EjsEx)); 
        code->sizeHandlers = size;
    }
    code->handlers[preferredIndex] = exception;
    return exception;
}


void ejsOffsetExceptions(EjsFunction *fun, int offset)
{
    EjsEx           *ex;
    int             i;

    mprAssert(fun);

    for (i = 0; i < fun->body.code.numHandlers; i++) {
        ex = fun->body.code.handlers[i];
        ex->tryStart += offset;
        ex->tryEnd += offset;
        ex->handlerStart += offset;
        ex->handlerEnd += offset;
    }
}


/*
 *  Set the byte code for a script function
 */
int ejsSetFunctionCode(EjsFunction *fun, uchar *byteCode, int len)
{
    mprAssert(fun);
    mprAssert(byteCode);
    mprAssert(len >= 0);

    byteCode = (uchar*) mprMemdup(fun, byteCode, len);
    if (byteCode == 0) {
        return EJS_ERR;
    }
    fun->body.code.codeLen = len;
    mprFree(fun->body.code.byteCode);
    fun->body.code.byteCode = (uchar*) byteCode;
    return 0;
}


EjsFunction *ejsCopyFunction(Ejs *ejs, EjsFunction *src)
{
    return cloneFunctionVar(ejs, src, 0);
}


/*
 *  Allocate a new variable. Size is set to the extra bytes for properties in addition to the type's instance size.
 */
static EjsFrame *allocFrame(Ejs *ejs, int numSlots)
{
    EjsObject       *obj;
    uint            size;

    mprAssert(ejs);

    size = numSlots * sizeof(EjsVar*) + sizeof(EjsFrame);

    if ((obj = (EjsObject*) mprAllocZeroed(ejsGetAllocCtx(ejs), size)) == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    obj->var.type = ejs->functionType;
    obj->var.master = (ejs->master == 0);
    obj->var.isObject = 1;
    obj->var.dynamic = 1;
    obj->var.isFunction = 1;

    obj->slots = (EjsVar**) &(((char*) obj)[sizeof(EjsFrame)]);
    obj->capacity = numSlots;
    obj->numProp = numSlots;
#if BLD_DEBUG
    ejsAddToGcStats(ejs, (EjsVar*) obj, ES_Frame);
#endif
    return (EjsFrame*) obj;
}


/*
 *  Fast allocation of a function activation frame
 */
EjsFrame *ejsCreateFrame(Ejs *ejs, EjsFunction *src)
{
    EjsFrame    *frame;
    int         numSlots;

    numSlots = max(src->block.obj.numProp, EJS_MIN_FRAME_SLOTS);

    if ((frame = (EjsFrame*) ejsAllocPooledVar(ejs, ES_Frame)) == 0) {
        frame = allocFrame(ejs, numSlots);
    }
    frame->function.block.obj.var.isObject = 1;
    frame->function.block.obj.var.dynamic = 1;
    frame->function.block.obj.var.isFunction = 1;
    frame->function.block.obj.var.type = ejs->functionType;
    frame->function.block.obj.numProp = src->block.obj.numProp;
    frame->function.block.obj.names = src->block.obj.names;
    frame->function.block.namespaces = src->block.namespaces;
    frame->function.block.scopeChain = src->block.scopeChain;
    frame->function.block.prev = src->block.prev;
    frame->function.block.traits = src->block.traits;

    frame->function.block.numTraits = src->block.numTraits;
    frame->function.block.sizeTraits = src->block.sizeTraits;
    frame->function.block.numInherited = src->block.numInherited;
    frame->function.block.hasScriptFunctions = src->block.hasScriptFunctions;
    frame->function.block.referenced = src->block.referenced;
    frame->function.block.breakCatch = src->block.breakCatch;

    frame->function.numArgs = src->numArgs;
    frame->function.numDefault = src->numDefault;
    frame->function.nextSlot = src->nextSlot;
    frame->function.constructor = src->constructor;
    frame->function.getter = src->getter;
    frame->function.setter = src->setter;
    frame->function.staticMethod = src->staticMethod;
    frame->function.hasReturn = src->hasReturn;
    frame->function.isInitializer = src->isInitializer;
    frame->function.literalGetter = src->literalGetter;
    frame->function.override = src->override;
    frame->function.lang = src->lang;
    frame->function.fullScope = src->fullScope;
    frame->function.rest = src->rest;
    frame->function.loading = src->loading;
    frame->function.nativeProc = src->nativeProc;
    frame->function.isFrame = src->isFrame;

    frame->function.isFrame = 1;
    frame->function.prototype = 0;
    frame->function.properties = 0;
    frame->function.resultType = src->resultType;
    frame->function.slotNum = src->slotNum;
    frame->function.owner = src->owner;
    frame->function.body = src->body;
    frame->pc = src->body.code.byteCode;
    frame->argc = 0;

    if (src->block.obj.numProp > 0) {
        if (frame->function.block.obj.numProp > frame->function.block.obj.capacity) {
            ejsGrowObject(ejs, (EjsObject*) frame, numSlots);
        }
        memcpy(frame->function.block.obj.slots, src->block.obj.slots, src->block.obj.numProp * sizeof(EjsVar*));
        frame->function.block.obj.numProp = src->block.obj.numProp;
    }
    ejsSetDebugName(frame, ejsGetDebugName(src));
    mprAssert(frame->argc == 0);
    return frame;
}


void ejsCreateFunctionType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Function"), ejs->objectType, sizeof(EjsFunction),
        ES_Function, ES_Function_NUM_CLASS_PROP, ES_Function_NUM_INSTANCE_PROP, 
        EJS_ATTR_OBJECT | EJS_ATTR_NATIVE | EJS_ATTR_DYNAMIC_INSTANCE | EJS_ATTR_BLOCK_HELPERS);
    if (type) {
        ejs->functionType = type;
        ejsInitializeFunctionHelpers(type->helpers, 1);
    }
}


void ejsInitializeFunctionHelpers(EjsTypeHelpers *helpers, int all)
{
    helpers->createVar  = (EjsCreateVarHelper) createFunction;
    helpers->castVar    = (EjsCastVarHelper) castFunction;
    helpers->cloneVar   = (EjsCloneVarHelper) cloneFunctionVar;
    helpers->destroyVar = (EjsDestroyVarHelper) destroyFunction;
    helpers->markVar    = (EjsMarkVarHelper) ejsMarkFunction;

    /*
     *  These helpers are only used for accessing Function.prototype properties and Function.*
     *  They are not used for function actual parameters and locals.
     */
    if (all) {
        helpers->getProperty          = (EjsGetPropertyHelper) getFunctionProperty;
        helpers->getPropertyName      = (EjsGetPropertyNameHelper) getFunctionPropertyName;
        helpers->getPropertyCount     = (EjsGetPropertyCountHelper) getFunctionPropertyCount;
        helpers->lookupProperty       = (EjsLookupPropertyHelper) lookupFunctionProperty;
        helpers->setProperty          = (EjsSetPropertyHelper) setFunctionProperty;
        helpers->setPropertyName      = (EjsSetPropertyNameHelper) setFunctionPropertyName;
        helpers->deleteProperty       = (EjsDeletePropertyHelper) deleteFunctionProperty;
    }
}


void ejsConfigureFunctionType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->functionType;
    ejsBindMethod(ejs, type, ES_Function_apply, (EjsNativeFunction) applyFunction);
    ejsBindMethod(ejs, type, ES_Function_call, (EjsNativeFunction) callFunctionMethod);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsFunction.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsGlobal.c"
 */
/************************************************************************/

/**
 *  ejsGlobal.c - Global functions and variables
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/**
 *  Assert a condition is true.
 *
 *  static function assert(condition: Boolean): Boolean
 */
static EjsVar *assertMethod(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    EjsFrame        *fp;
    EjsBoolean      *b;

    mprAssert(argc == 1);

    if (! ejsIsBoolean(argv[0])) {
        b = (EjsBoolean*) ejsCastVar(ejs, argv[0], ejs->booleanType);
    } else {
        b = (EjsBoolean*) argv[0];
    }
    mprAssert(b);

    if (b == 0 || !b->value) {
        fp = ejs->state->fp;
        if (fp->currentLine) {
            mprLog(ejs, 0, "Assertion error: %s", fp->currentLine);
            ejsThrowAssertError(ejs, "Assertion error: %s", fp->currentLine);
        } else {
            ejsThrowAssertError(ejs, "Assertion error");
        }
        return 0;
    }
    return vp;
}


/**
 *  Trap to the debugger
 *
 *  static function breakpoint(): Void
 */
static EjsVar *breakpoint(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    mprBreakpoint();
    return 0;
}


/**
 *  Clone the base class. Used by Record.es
 *
 *  static function cloneBase(klass: Type): Void
 */
static EjsVar *cloneBase(Ejs *ejs, EjsVar *ignored, int argc, EjsVar **argv)
{
    EjsType     *type;
    
    mprAssert(argc == 1);
    
    type = (EjsType*) argv[0];
    type->baseType = (EjsType*) ejsCloneVar(ejs, (EjsVar*) type->baseType, 0);
    return 0;
}


/*
 *  Reverse www-urlencoding on a string
 *
 *  function decodeURI(str: String): String
 */
static EjsVar *decodeURI(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsString   *str;

    str = (EjsString*) argv[0];
    return (EjsVar*) ejsCreateStringAndFree(ejs, mprUrlDecode(ejs, str->value));
}


/**
 *  Print the arguments to the standard error with a new line.
 *
 *  static function error(...args): void
 */
static EjsVar *error(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsString   *s;
    EjsVar      *args, *vp;
    int         i, count, junk;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = argv[0];
    count = ejsGetPropertyCount(ejs, args);

    for (i = 0; i < count; i++) {
        if ((vp = ejsGetProperty(ejs, args, i)) != 0) {
            if (!ejsIsString(vp)) {
                vp = ejsSerialize(ejs, vp, -1, 0, 0);
            }
            if (ejs->exception) {
                return 0;
            }
            if (vp) {
                s = (EjsString*) vp;
                junk = write(2, s->value, s->length);
            }
        }
    }
    junk = write(2, "\n", 1);
    return 0;
}


/*
 *  Perform www-urlencoding on a string
 *
 *  function encodeURI(str: String): String
 */
static EjsVar *encodeURI(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsString   *str;

    str = (EjsString*) argv[0];
    return (EjsVar*) ejsCreateStringAndFree(ejs, mprUrlEncode(ejs, str->value));
}


/*
 *  HTML escape a string
 *
 *  function escape(str: String): String
 */
static EjsVar *escape(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsString   *str;

    str = (EjsString*) argv[0];
    return (EjsVar*) ejsCreateStringAndFree(ejs, mprEscapeHtml(ejs, str->value));
}


/*
 *  function eval(script: String): String
 */
static EjsVar *eval(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    cchar       *script;

    script = ejsGetString(argv[0]);
    if (ejs->service->loadScriptLiteral) {
        return (ejs->service->loadScriptLiteral)(ejs, script);
    }
    ejsThrowStateError(ejs, "Ability to compile scripts not available");
    return 0;
}


/*
 *  Format the stack
 *
 *  function formatStack(): String
 */
static EjsVar *formatStackMethod(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, ejsFormatStack(ejs, NULL));
}


#if ES_hashcode
/*
 *  Get the hash code for the object.
 *
 *  intrinsic function hashcode(o: Object): Number
 */
static EjsVar *hashcode(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    mprAssert(argc == 1);
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) PTOL(argv[0]));
}
#endif


/**
 *  Read a line of input
 *
 *  static function input(): String
 */
static EjsVar *input(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprFileSystem   *fs;
    MprBuf          *buf;
    EjsVar          *result;
    int             c;

    fs = mprGetMpr(ejs)->fileSystem;

    buf = mprCreateBuf(ejs, -1, -1);
    while ((c = getchar()) != EOF) {
#if BLD_WIN_LIKE
        if (c == fs->newline[0]) {
            continue;
        } else if (c == fs->newline[1]) {
            break;
        }
#else
        if (c == fs->newline[0]) {
            break;
        }
#endif
        mprPutCharToBuf(buf, c);
    }
    if (c == EOF && mprGetBufLength(buf) == 0) {
        return (EjsVar*) ejs->nullValue;
    }
    mprAddNullToBuf(buf);
    result = (EjsVar*) ejsCreateString(ejs, mprGetBufStart(buf));
    mprFree(buf);
    return result;
}


/**
 *  Load a script or module. Name should have an extension. Name will be located according to the EJSPATH search strategy.
 *
 *  static function load(filename: String): void
 */
static EjsVar *load(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    cchar       *path, *cp;

    path = ejsGetString(argv[0]);

    if ((cp = strrchr(path, '.')) != NULL && strcmp(cp, EJS_MODULE_EXT) != 0) {
        if (ejs->service->loadScriptFile == 0) {
            ejsThrowIOError(ejs, "load: Compiling is not enabled for %s", path);
        } else {
            (ejs->service->loadScriptFile)(ejs, path);
        }
    } else {
        /* This will throw on errors */
        ejsLoadModule(ejs, path, -1, -1, 0, NULL);
    }
    return 0;
}


/**
 *  Compute an MD5 checksum
 *
 *  static function md5(name: String): void
 */
static EjsVar *md5(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsString   *str;

    str = (EjsString*) argv[0];
    return (EjsVar*) ejsCreateStringAndFree(ejs, mprGetMD5Hash(ejs, str->value, str->length, NULL));
}


/**
 *  Parse the input and convert to a primitive type
 *
 *  static function parse(input: String, preferredType: Type = null): void
 */
static EjsVar *parse(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    cchar       *input;
    int         preferred;

    input = ejsGetString(argv[0]);

    if (argc == 2 && !ejsIsType(argv[1])) {
        ejsThrowArgError(ejs, "Argument is not a type");
        return 0;
    }
    preferred = (argc == 2) ? ((EjsType*) argv[1])->id : -1;
    while (isspace((int) *input)) {
        input++;
    }
    return ejsParseVar(ejs, input, preferred);
}


/**
 *  Print the arguments to the standard output with a new line.
 *
 *      static function output(...args): void
 *      static function print(...args): void
 */
static EjsVar *outputData(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsString   *s;
    EjsVar      *args, *vp;
    char        *cp, *tmp;
    int         i, count, junk;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = argv[0];
    count = ejsGetPropertyCount(ejs, args);

    for (i = 0; i < count; i++) {
        if ((vp = ejsGetProperty(ejs, args, i)) != 0) {
            if (ejsIsString(vp)) {
                s = (EjsString*) vp;
            } else {
                s  = (EjsString*) ejsToString(ejs, vp);
            }
            if (ejs->exception) {
                return 0;
            }
            if (vp && s) {
                if (ejsIsObject(vp) && s->length > 0 && s->value[0] == '"') {
                    tmp = mprStrdup(ejs, s->value);
                    cp = mprStrTrim(tmp, "\"");
                    junk = write(1, cp, strlen(cp));
                    mprFree(tmp);
                } else {
                    junk = write(1, s->value, s->length);
                }
            }
        }
    }
    junk = write(1, "\n", 1);
    return 0;
}


#if ES_printv && BLD_DEBUG
/**
 *  Print the named variables for debugging.
 *
 *  static function printv(...args): void
 */
static EjsVar *printv(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsString   *s;
    EjsVar      *args, *vp;
    int         i, count;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = argv[0];
    count = ejsGetPropertyCount(ejs, args);

    for (i = 0; i < count; i++) {
        vp = ejsGetProperty(ejs, args, i);
        if (vp == 0) {
            continue;
        }
        if (vp->seq == 28092) {
            printf("GOT YOU\n");
        }
        
        s = (EjsString*) ejsToJson(ejs, vp);

        if (ejs->exception) {
            return 0;
        }

        mprAssert(s && ejsIsString(s));
        mprPrintf(ejs, "%s = %s\n", mprGetName(vp), s->value);
    }
    mprPrintf(ejs, "\n");
    return 0;
}
#endif


static EjsNamespace *addNamespace(Ejs *ejs, EjsBlock *block, cchar *space)
{
    EjsNamespace    *ns;

    ns = ejsDefineReservedNamespace(ejs, block, 0, space);
    mprAddHash(ejs->standardSpaces, space, ns);
    return ns;
}


void ejsCreateGlobalBlock(Ejs *ejs)
{
    EjsBlock    *block;

    /*
     *  Pre-create extra global slots
     */
    ejs->globalBlock = ejsCreateBlock(ejs, max(ES_global_NUM_CLASS_PROP, 256));
    ejs->global = (EjsVar*) ejs->globalBlock;
    ejsSetDebugName(ejs->global, "global");
    
    if (ejs->flags & EJS_FLAG_EMPTY) {
        ejs->globalBlock->obj.numProp = 0;
    } else {
        ejs->globalBlock->obj.numProp = ES_global_NUM_CLASS_PROP;
    }
    
    block = (EjsBlock*) ejs->global;
    
    /*
     *  Create the standard namespaces. Order matters here. This is the (reverse) order of lookup.
     *  Empty is first intrinsic is last.
     */
    ejs->configSpace =      addNamespace(ejs, block, EJS_CONFIG_NAMESPACE);
    ejs->iteratorSpace =    addNamespace(ejs, block, EJS_ITERATOR_NAMESPACE);
    ejs->intrinsicSpace =   addNamespace(ejs, block, EJS_INTRINSIC_NAMESPACE);
    ejs->eventsSpace =      addNamespace(ejs, block, EJS_EVENTS_NAMESPACE);
    ejs->ioSpace =          addNamespace(ejs, block, EJS_IO_NAMESPACE);
    ejs->sysSpace =         addNamespace(ejs, block, EJS_SYS_NAMESPACE);
    ejs->publicSpace =      addNamespace(ejs, block, EJS_PUBLIC_NAMESPACE);
    ejs->emptySpace =       addNamespace(ejs, block, EJS_EMPTY_NAMESPACE);
}


void ejsConfigureGlobalBlock(Ejs *ejs)
{
    EjsBlock    *block;

    block = ejs->globalBlock;
    mprAssert(block);

#if ES_assert
    ejsBindFunction(ejs, block, ES_assert, assertMethod);
#endif
#if ES_breakpoint
    ejsBindFunction(ejs, block, ES_breakpoint, breakpoint);
#endif
#if ES_cloneBase
    ejsBindFunction(ejs, block, ES_cloneBase, (EjsNativeFunction) cloneBase);
#endif
#if ES_decodeURI
    ejsBindFunction(ejs, block, ES_decodeURI, decodeURI);
#endif
#if ES_error
    ejsBindFunction(ejs, block, ES_error, error);
#endif
#if ES_encodeURI
    ejsBindFunction(ejs, block, ES_encodeURI, encodeURI);
#endif
#if ES_escape
    ejsBindFunction(ejs, block, ES_escape, escape);
#endif
#if ES_eval
    ejsBindFunction(ejs, block, ES_eval, eval);
#endif
#if ES_formatStack
    ejsBindFunction(ejs, block, ES_formatStack, formatStackMethod);
#endif
#if ES_hashcode
    ejsBindFunction(ejs, block, ES_hashcode, hashcode);
#endif
#if ES_input
    ejsBindFunction(ejs, block, ES_input, input);
#endif
#if ES_load
    ejsBindFunction(ejs, block, ES_load, load);
#endif
#if ES_md5
    ejsBindFunction(ejs, block, ES_md5, md5);
#endif
#if ES_output
    ejsBindFunction(ejs, block, ES_output, outputData);
#endif
#if ES_parse
    ejsBindFunction(ejs, block, ES_parse, parse);
#endif
#if ES_print
    /* Just uses output */
    ejsBindFunction(ejs, block, ES_print, outputData);
#endif
#if ES_printv && BLD_DEBUG
    ejsBindFunction(ejs, block, ES_printv, printv);
#endif
    ejsConfigureJSON(ejs);
    /*
     *  Update the global reference
     */
    ejsSetProperty(ejs, ejs->global, ES_global, ejs->global);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsGlobal.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsIterator.c"
 */
/************************************************************************/

/**
 *  ejsIterator.c - Iterator class
 *
 *  This provides a high performance iterator construction for native classes.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static void markIteratorVar(Ejs *ejs, EjsVar *parent, EjsIterator *ip)
{
    if (ip->target) {
        ejsMarkVar(ejs, (EjsVar*) ip, ip->target);
    }
}


/*
 *  Call the supplied next() function to return the next enumerable item
 */
static EjsVar *nextIterator(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    if (ip->nativeNext) {
        return (ip->nativeNext)(ejs, (EjsVar*) ip, argc, argv);
    } else {
        ejsThrowStopIteration(ejs);
        return 0;
    }
}


/*
 *  Throw the StopIteration object
 */
EjsVar *ejsThrowStopIteration(Ejs *ejs)
{
    ejs->exception = (EjsVar*) ejs->stopIterationType;
    ejs->attention = 1;
    return 0;
}


#if KEEP
/*
 *  Constructor to create an iterator using a scripted next().
 *
 *  public function Iterator(obj, f, deep, ...namespaces)
 */
static EjsVar *iteratorConstructor(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    if (argc != 2 || !ejsIsFunction(argv[1])) {
        ejsThrowArgError(ejs, "usage: Iterator(obj, function)");
        return 0;
    }
    ip->target = argv[0];
    ip->next = (EjsFunction*) argv[1];
    mprAssert(ip->nativeNext == 0);

    return (EjsVar*) ip;
}
#endif


/*
 *  Create an iterator.
 */
EjsIterator *ejsCreateIterator(Ejs *ejs, EjsVar *obj, EjsNativeFunction nativeNext, bool deep, EjsArray *namespaces)
{
    EjsIterator     *ip;

    ip = (EjsIterator*) ejsCreateVar(ejs, ejs->iteratorType, 0);
    if (ip) {
        ip->index = 0;
        ip->indexVar = 0;
        ip->nativeNext = nativeNext;
        ip->target = obj;
        ip->deep = deep;
        ip->namespaces = namespaces;
        ejsSetDebugName(ip, "iterator");
    }
    return ip;
}


/*
 *  Create the Iterator and StopIteration types
 */
void ejsCreateIteratorType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_ITERATOR_NAMESPACE, "Iterator"), ejs->objectType, sizeof(EjsIterator),
        ES_Iterator, ES_Iterator_NUM_CLASS_PROP, ES_Iterator_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE);
    ejs->iteratorType = type;

    type->helpers->markVar  = (EjsMarkVarHelper) markIteratorVar;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_ITERATOR_NAMESPACE, "StopIteration"), ejs->objectType, sizeof(EjsVar), 
        ES_StopIteration, ES_StopIteration_NUM_CLASS_PROP,  ES_StopIteration_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE);
    ejs->stopIterationType = type;
}


void ejsConfigureIteratorType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->iteratorType;

    /*
     *  Define the "next" method
     */
    ejsBindMethod(ejs, ejs->iteratorType, ES_Iterator_next, (EjsNativeFunction) nextIterator);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsIterator.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsJSON.c"
 */
/************************************************************************/

/**
 *  ejsJSON.c - JSON encoding and decoding
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




typedef struct JsonState {
    char    *data;
    char    *end;
    char    *next;
    char    *error;
} JsonState;


static EjsVar *parseLiteral(Ejs *ejs, JsonState *js);
static EjsVar *parseLiteralInner(Ejs *ejs, MprBuf *buf, JsonState *js);

/*
 *  Convert a string into an object.
 *
 *  function deserialize(obj: String): Object
 */
EjsVar *deserialize(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsString(argv[0]));
    return ejsDeserialize(ejs, (EjsString*) argv[0]);
}


EjsVar *ejsDeserialize(Ejs *ejs, EjsString *str)
{
    EjsVar      *obj;
    JsonState   js;
    char        *data;

    if (!ejsIsString(str)) {
        return 0;
    }
    data = ejsGetString(str);
    if (data == 0) {
        return 0;
    } else if (*data == '\0') {
        return (EjsVar*) ejs->emptyStringValue;
    }

    js.next = js.data = data;
    js.end = &js.data[str->length];
    js.error = 0;
    if ((obj = parseLiteral(ejs, &js)) == 0) {
        if (js.error) {
            ejsThrowSyntaxError(ejs, 
                "Can't parse object literal. Error at position %d.\n"
                "===========================\n"
                "Offending text: %s\n"
                "===========================\n"
                "In literal %s"
                "\n===========================\n",
                (int) (js.error - js.data), js.error, js.data);
        } else {
            ejsThrowSyntaxError(ejs, "Can't parse object literal. Undefined error");
        }
        return 0;
    }
    return obj;
}


static EjsVar *parseLiteral(Ejs *ejs, JsonState *js)
{
    MprBuf      *buf;
    EjsVar      *vp;

    mprAssert(js);

    buf = mprCreateBuf(ejs, 0, 0);
    vp = parseLiteralInner(ejs, buf, js);
    mprFree(buf);
    return vp;
}


typedef enum Token {
    TOK_ERR,            /* Error */
    TOK_EOF,            /* End of input */
    TOK_LBRACE,         /* { */
    TOK_LBRACKET,       /* [ */
    TOK_RBRACE,         /* } */
    TOK_RBRACKET,       /* ] */
    TOK_COLON,          /* : */
    TOK_COMMA,          /* , */
    TOK_ID,             /* Unquoted ID */
    TOK_QID,            /* Quoted ID */
} Token;


Token getNextJsonToken(MprBuf *buf, char **token, JsonState *js)
{
    uchar   *start, *cp, *end, *next;
    char    *src, *dest;
    int     quote, tid, c;

    next = (uchar*) js->next;
    end = (uchar*) js->end;

    if (buf) {
        mprFlushBuf(buf);
    }
    for (cp = next; cp < end && isspace((int) *cp); cp++) {
        ;
    }
    next = cp + 1;

    if (*cp == '\0') {
        tid = TOK_EOF;

    } else  if (*cp == '{') {
        tid = TOK_LBRACE;

    } else if (*cp == '[') {
        tid = TOK_LBRACKET;

    } else if (*cp == '}' || *cp == ']') {
        tid = *cp == '}' ? TOK_RBRACE: TOK_RBRACKET;
        while (*++cp && isspace((int) *cp)) ;
        if (*cp == ',' || *cp == ':') {
            cp++;
        }
        next = cp;

    } else {
        if (*cp == '"' || *cp == '\'') {
            tid = TOK_QID;
            quote = *cp++;
            for (start = cp; cp < end; cp++) {
                if (*cp == '\\') {
                    if (cp[1] == quote) {
                        cp++;
                    }
                    continue;
                }
                if (*cp == quote) {
                    break;
                }
            }
        } else {
            quote = -1;
            tid = TOK_ID;
            for (start = cp; cp < end; cp++) {
                if (*cp == '\\') {
                    continue;
                }
                /* Not an allowable character outside quotes */
                if (!(isalnum((int) *cp) || *cp == '_' || *cp == ' ' || *cp == '-' || *cp == '+' || *cp == '.')) {
                    break;
                }
            }
        }
        if (buf) {
            mprPutBlockToBuf(buf, (char*) start, cp - start);
            mprAddNullToBuf(buf);
        }
        if (quote > 0) {
            if (*cp == quote) {
                cp++;
            } else {
                js->error = (char*) cp;
                return TOK_ERR;
            }
        }
        if (*cp == ',' || *cp == ':') {
            cp++;
        } else if (*cp != '}' && *cp != ']' && *cp != '\0' && *cp != '\n' && *cp != '\r' && *cp != ' ') {
            js->error = (char*) cp;
            return TOK_ERR;
        }
        next = cp;

        if (buf) {
            for (dest = src = buf->start; src < buf->end; ) {
                c = *src++;
                if (c == '\\') {
                    c = *src++;
                    if (c == 'r') {
                        c = '\r';
                    } else if (c == 'n') {
                        c = '\n';
                    } else if (c == 'b') {
                        c = '\b';
                    }
                }
                *dest++ = c;
            }
            *dest = '\0';
            *token = mprGetBufStart(buf);
        }
    }
    js->next = (char*) next;
    return tid;
}


Token peekNextJsonToken(JsonState *js)
{
    JsonState   discard = *js;
    return getNextJsonToken(NULL, NULL, &discard);
}


/*
 *  Parse an object literal string pointed to by js->next into the given buffer. Update js->next to point
 *  to the next input token in the object literal. Supports nested object literals.
 */
static EjsVar *parseLiteralInner(Ejs *ejs, MprBuf *buf, JsonState *js)
{
    EjsName     qname;
    EjsVar      *obj, *vp;
    MprBuf      *valueBuf;
    char        *token, *key, *value;
    int         tid, isArray;

    isArray = 0;

    tid = getNextJsonToken(buf, &token, js);
    if (tid == TOK_ERR || tid == TOK_EOF) {
        return 0;
    }
    if (tid == TOK_LBRACKET) {
        isArray = 1;
        obj = (EjsVar*) ejsCreateArray(ejs, 0);
    } else if (tid == TOK_LBRACE) {
        obj = (EjsVar*) ejsCreateObject(ejs, ejs->objectType, 0);
    } else {
        return ejsParseVar(ejs, token, ES_String);
    }
    if (obj == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    while (1) {
        vp = 0;
        tid = peekNextJsonToken(js);
        if (tid == TOK_ERR) {
            return 0;
        } else if (tid == TOK_EOF) {
            break;
        } else if (tid == TOK_RBRACE || tid == TOK_RBRACKET) {
            getNextJsonToken(buf, &key, js);
            break;
        }
        if (tid == TOK_LBRACKET) {
            /* For array values */
            vp = parseLiteral(ejs, js);
            mprAssert(vp);
            
        } else if (tid == TOK_LBRACE) {
            /* For object values */
            vp = parseLiteral(ejs, js);
            mprAssert(vp);
            
        } else if (isArray) {
            tid = getNextJsonToken(buf, &value, js);
            vp = ejsParseVar(ejs, value, (tid == TOK_QID) ? ES_String : -1);
            mprAssert(vp);
            
        } else {
            getNextJsonToken(buf, &key, js);
            tid = peekNextJsonToken(js);
            if (tid == TOK_ERR) {
                mprAssert(0);
                return 0;
            } else if (tid == TOK_EOF) {
                break;
            } else if (tid == TOK_LBRACE || tid == TOK_LBRACKET) {
                vp = parseLiteral(ejs, js);
                mprAssert(vp);

            } else if (tid == TOK_ID || tid == TOK_QID) {
                valueBuf = mprCreateBuf(ejs, 0, 0);
                getNextJsonToken(valueBuf, &value, js);
                if (tid == TOK_QID) {
                    vp = (EjsVar*) ejsCreateString(ejs, value);
                } else {
                    if (strcmp(value, "null") == 0) {
                        vp = ejs->nullValue;
                    } else if (strcmp(value, "undefined") == 0) {
                        vp = ejs->undefinedValue;
                    } else {
                        vp = ejsParseVar(ejs, value, -1);
                    }
                }
                mprAssert(vp);
                mprFree(valueBuf);
            } else {
                getNextJsonToken(buf, &value, js);
                mprAssert(0);
                js->error = js->next;
                return 0;
            }
        }
        if (vp == 0) {
            js->error = js->next;
            return 0;
        }
        if (isArray) {
            if (ejsSetProperty(ejs, obj, -1, vp) < 0) {
                ejsThrowMemoryError(ejs);
                return 0;
            }
        } else {
            /*
             *  Must not pool this object otherwise the key allocation will be leak. Need the var to be freed.
             */
            key = mprStrdup(obj, key);
            ejsName(&qname, EJS_EMPTY_NAMESPACE, key);
            obj->noPool = 1;
            if (ejsSetPropertyByName(ejs, obj, &qname, vp) < 0) {
                ejsThrowMemoryError(ejs);
                return 0;
            }
        }
    }
    return obj;
}


EjsVar *ejsSerialize(Ejs *ejs, EjsVar *vp, int maxDepth, bool showAll, bool showBase)
{
    int   flags;

    if (maxDepth <= 0) {
        maxDepth = MAXINT;
    }
    flags = 0;
    if (showAll) {
        flags |= EJS_FLAGS_ENUM_ALL;
    }
    if (showBase) {
        flags |= EJS_FLAGS_ENUM_INHERITED;
    }
    return (EjsVar*) ejsToJson(ejs, vp); 
}


/*
 *  Convert the object to a source code string.
 *
 *  intrinsic function serialize(obj: Object, maxDepth: Number = 0, showAll: Boolean = false, 
 *      showBase: Boolean = false): String
 */
static EjsVar *serialize(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsVar          *vp;
    int             flags, maxDepth;
    bool            showBase, showAll;

    flags = 0;
    maxDepth = MAXINT;

    vp = argv[0];

    if (argc >= 2) {
        maxDepth = ejsGetInt(argv[1]);
    }
    showAll = (argc >= 3 && argv[2] == (EjsVar*) ejs->trueValue);
    showBase = (argc == 4 && argv[3] == (EjsVar*) ejs->trueValue);
    return ejsSerialize(ejs, argv[0], maxDepth, showAll, showBase);
}


void ejsConfigureJSON(Ejs *ejs)
{
    EjsBlock    *block;

    block = ejs->globalBlock;
    mprAssert(block);

#if ES_deserialize
    ejsBindFunction(ejs, block, ES_deserialize, deserialize);
#endif
#if ES_serialize
    ejsBindFunction(ejs, block, ES_serialize, serialize);
#endif
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsJSON.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsMath.c"
 */
/************************************************************************/

/**
    ejsMath.c - Math type class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#include     <math.h>


#if BLD_FEATURE_NUM_TYPE_DOUBLE
#define fixed(n) ((int64) (floor(n)))
#else
#define fixed(n) (n)
#endif

/*
    function abs(value: Number): Number
 */
static EjsVar *math_abs(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) fabs(ejsGetNumber(argv[0])));
}


/*
    function acos(value: Number): Number
 */
static EjsVar *math_acos(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprNumber   value;
    
    value = ejsGetNumber(argv[0]);
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) acos(ejsGetNumber(argv[0])));
}


/*
    function asin(value: Number): Number
 */
static EjsVar *math_asin(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) asin(ejsGetNumber(argv[0])));
}


/*
    function atan(value: Number): Number
 */
static EjsVar *math_atan(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) atan(ejsGetNumber(argv[0])));
}


/*
    function atan2(x: Number, y: Number): Number
 */
static EjsVar *math_atan2(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) atan2(ejsGetNumber(argv[0]), ejsGetNumber(argv[1])));
}


/*
    function ceil(value: Number): Number
 */
static EjsVar *math_ceil(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) ceil(ejsGetNumber(argv[0])));
}


/*
    function cos(value: Number): Number
 */
static EjsVar *math_cos(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) cos(ejsGetNumber(argv[0])));
}


/*
    function exp(value: Number): Number
 */
static EjsVar *math_exp(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) exp(ejsGetNumber(argv[0])));
}


/*
    function floor(value: Number): Number
 */
static EjsVar *math_floor(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) floor(ejsGetNumber(argv[0])));
}


/*
    function log10(value: Number): Number
 */
static EjsVar *math_log10(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) log10(ejsGetNumber(argv[0])));
}


/*
    function log(value: Number): Number
 */
static EjsVar *math_log(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) log(ejsGetNumber(argv[0])));
}


/*
    function max(x: Number, y: Number): Number
 */
static EjsVar *math_max(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprNumber   x, y;
    
    x = ejsGetNumber(argv[0]);
    y = ejsGetNumber(argv[1]);
    if (x > y) {
        return argv[0];
    }
    return argv[1];
}


/*
    function min(value: Number): Number
 */
static EjsVar *math_min(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprNumber   x, y;
    
    x = ejsGetNumber(argv[0]);
    y = ejsGetNumber(argv[1]);
    if (x < y) {
        return argv[0];
    }
    return argv[1];
}


/*
    function pow(x: Number, y: Number): Number
 */
static EjsVar *math_pow(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprNumber   x, y, result;
    
    x = ejsGetNumber(argv[0]);
    y = ejsGetNumber(argv[1]);
    result = pow(x, y);
#if CYGWIN
    /* Cygwin computes (0.0 / -1) == -Infinity */
    if (result < 0 && x == 0.0) {
        result = -result;
    }
#endif
    return (EjsObj*) ejsCreateNumber(ejs, (MprNumber) result);
}


/*
    function random(value: Number): Number
 */
static EjsVar *math_random(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprNumber   value;
    uint        uvalue;
    static int  initialized = 0;
    
    if (!initialized) {
#if WIN
        uint seed = (uint) time(0);
        srand(seed);
#elif !MACOSX && !VXWORKS
        srandom(time(0));
#endif
        initialized = 1;
    }
    
#if WIN
{
    errno_t rand_s(uint *value);
    rand_s(&uvalue);
}
#elif LINUX
    uvalue = random();
#elif MACOSX 
    uvalue = arc4random();
#else
{
    int64   data[16];
    int     i;
    mprGetRandomBytes(ejs, (char*) data, sizeof(data), 0);
    uvalue = 0;
    for (i = 0; i < sizeof(data) / sizeof(int64); i++) {
        uvalue += data[i];
    }
}
#endif
    value = ((MprNumber) (uvalue & 0x7FFFFFFF) / INT_MAX);
    return (EjsVar*) ejsCreateNumber(ejs, value);
}


/*
    function round(value: Number): Number
 */
static EjsVar *math_round(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprNumber   n;

    n = ejsGetNumber(argv[0]);

    if (n < 0 && n >= -0.5) {
        n = -0.0;
    } else {
        n += 0.5;
    }
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) floor(n));
}


/*
    function sin(value: Number): Number
 */
static EjsVar *math_sin(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) sin(ejsGetNumber(argv[0])));
}


/*
    function sqrt(value: Number): Number
 */
static EjsVar *math_sqrt(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) sqrt(ejsGetNumber(argv[0])));
}


/*
    function tan(value: Number): Number
 */
static EjsVar *math_tan(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) tan(ejsGetNumber(argv[0])));
}



void ejsConfigureMathType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->mathType = ejsGetType(ejs, ES_Math);
    ejsBindMethod(ejs, type, ES_Math_abs, (EjsNativeFunction) math_abs);
    ejsBindMethod(ejs, type, ES_Math_acos, (EjsNativeFunction) math_acos);
    ejsBindMethod(ejs, type, ES_Math_asin, (EjsNativeFunction) math_asin);
    ejsBindMethod(ejs, type, ES_Math_atan, (EjsNativeFunction) math_atan);
    ejsBindMethod(ejs, type, ES_Math_atan2, (EjsNativeFunction) math_atan2);
    ejsBindMethod(ejs, type, ES_Math_ceil, (EjsNativeFunction) math_ceil);
    ejsBindMethod(ejs, type, ES_Math_cos, (EjsNativeFunction) math_cos);
    ejsBindMethod(ejs, type, ES_Math_exp, (EjsNativeFunction) math_exp);
    ejsBindMethod(ejs, type, ES_Math_floor, (EjsNativeFunction) math_floor);
    ejsBindMethod(ejs, type, ES_Math_log, (EjsNativeFunction) math_log);
    ejsBindMethod(ejs, type, ES_Math_log10, (EjsNativeFunction) math_log10);
    ejsBindMethod(ejs, type, ES_Math_max, (EjsNativeFunction) math_max);
    ejsBindMethod(ejs, type, ES_Math_min, (EjsNativeFunction) math_min);
    ejsBindMethod(ejs, type, ES_Math_pow, (EjsNativeFunction) math_pow);
    ejsBindMethod(ejs, type, ES_Math_random, (EjsNativeFunction) math_random);
    ejsBindMethod(ejs, type, ES_Math_round, (EjsNativeFunction) math_round);
    ejsBindMethod(ejs, type, ES_Math_sin, (EjsNativeFunction) math_sin);
    ejsBindMethod(ejs, type, ES_Math_sqrt, (EjsNativeFunction) math_sqrt);
    ejsBindMethod(ejs, type, ES_Math_tan, (EjsNativeFunction) math_tan);
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://www.embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsMath.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsNamespace.c"
 */
/************************************************************************/

/**
 *  ejsNamespace.c - Ejscript Namespace class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Cast the operand to the specified type
 */

static EjsVar *castNamespace(Ejs *ejs, EjsNamespace *vp, EjsType *type)
{
    switch (type->id) {
    case ES_Boolean:
        return (EjsVar*) ejsCreateBoolean(ejs, 1);

    case ES_String:
        return (EjsVar*) ejsCreateString(ejs, "[object Namespace]");

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
}


static EjsVar *invokeNamespaceOperator(Ejs *ejs, EjsNamespace *lhs, int opCode, EjsNamespace *rhs)
{
    bool        boolResult;

    switch (opCode) {
    case EJS_OP_COMPARE_EQ:
        if ((EjsVar*) rhs == ejs->nullValue || (EjsVar*) rhs == ejs->undefinedValue) {
            return (EjsVar*) ((opCode == EJS_OP_COMPARE_EQ) ? ejs->falseValue: ejs->trueValue);
        }
        boolResult = (strcmp(lhs->name, rhs->name) == 0 && strcmp(lhs->uri, rhs->uri) == 0);
        break;

    case EJS_OP_COMPARE_STRICTLY_EQ:
        boolResult = lhs == rhs;
        break;

    case EJS_OP_COMPARE_NE:
        if ((EjsVar*) rhs == ejs->nullValue || (EjsVar*) rhs == ejs->undefinedValue) {
            return (EjsVar*) ((opCode == EJS_OP_COMPARE_EQ) ? ejs->falseValue: ejs->trueValue);
        }
        boolResult = ! (strcmp(lhs->name, rhs->name) == 0 && strcmp(lhs->uri, rhs->uri) == 0);
        break;

    case EJS_OP_COMPARE_STRICTLY_NE:
        boolResult = !(lhs == rhs);
        break;

    default:
        ejsThrowTypeError(ejs, "Operation is not valid on this type");
        return 0;
    }
    return (EjsVar*) ejsCreateBoolean(ejs, boolResult);
}


/*
 *  Define a reserved namespace in a block.
 */
EjsNamespace *ejsDefineReservedNamespace(Ejs *ejs, EjsBlock *block, EjsName *typeName, cchar *spaceName)
{
    EjsNamespace    *namespace;

    namespace = ejsCreateReservedNamespace(ejs, typeName, spaceName);
    if (namespace) {
        if (ejsAddNamespaceToBlock(ejs, block, namespace) < 0) {
            return 0;
        }
    }
    return namespace;
}


/*
 *  Format a reserved namespace to create a unique namespace URI. "internal, public, private, protected"
 *
 *  Namespaces are formatted as strings using the following format, where type is optional. Types may be qualified.
 *      [type,space]
 *
 *  Example:
 *      [debug::Shape,public] where Shape was declared as "debug class Shape"
 */
char *ejsFormatReservedNamespace(MprCtx ctx, EjsName *typeName, cchar *spaceName)
{
    cchar   *typeNameSpace;
    char    *namespace, *sp;
    int     len, typeLen, spaceLen, l;

    len = typeLen = spaceLen = 0;
    typeNameSpace = 0;

    if (typeName) {
        if (typeName->name == 0) {
            typeName = 0;
        }
        typeNameSpace = typeName->space ? typeName->space : EJS_PUBLIC_NAMESPACE;
    }

    if (typeName && typeName->name) {
        //  Join the qualified typeName to be "space::name"
        mprAssert(typeName->name);
        typeLen = (int) strlen(typeNameSpace);
        typeLen += 2 + (int) strlen(typeName->name);          //  Allow for the "::" between space::name
        len += typeLen;
    }
    spaceLen = (int) strlen(spaceName);

    /*
     *  Add 4 for [,,]
     *  Add 2 for the trailing "::" and one for the null
     */
    len += 4 + spaceLen + 2 + 1;

    namespace = mprAlloc(ctx, len);
    if (namespace == 0) {
        return 0;
    }

    sp = namespace;
    *sp++ = '[';

    if (typeName) {
        if (strcmp(typeNameSpace, EJS_PUBLIC_NAMESPACE) != 0) {
            l = (int) strlen(typeNameSpace);
            strcpy(sp, typeNameSpace);
            sp += l;
            *sp++ = ':';
            *sp++ = ':';
        }
        l = (int) strlen(typeName->name);
        strcpy(sp, typeName->name);
        sp += l;
    }

    *sp++ = ',';
    strcpy(sp, spaceName);
    sp += spaceLen;

    *sp++ = ']';
    *sp = '\0';

    mprAssert(sp <= &namespace[len]);

    return namespace;
}

/*
 *  Create a namespace with the given URI as its definition qualifying value.
 */
EjsNamespace *ejsCreateNamespace(Ejs *ejs, cchar *name, cchar *uri)
{
    EjsNamespace    *np;

    if (uri == 0) {
        uri = name;
    } else if (name == 0) {
        name = uri;
    }
    np = (EjsNamespace*) ejsCreateVar(ejs, ejs->namespaceType, 0);
    if (np) {
        np->name = (char*) name;
        np->uri = (char*) uri;
    }
    ejsSetDebugName(np, np->uri);
    return np;
}

/*
 *  Create a reserved namespace. Format the package, type and space names to create a unique namespace URI.
 *  packageName, typeName and uri are optional.
 */
EjsNamespace *ejsCreateReservedNamespace(Ejs *ejs, EjsName *typeName, cchar *spaceName)
{
    EjsNamespace    *namespace;
    char            *formattedName;

    mprAssert(spaceName);

    if (typeName) {
        formattedName = (char*) ejsFormatReservedNamespace(ejs, typeName, spaceName);
    } else {
        formattedName = (char*) spaceName;
    }
    namespace = ejsCreateNamespace(ejs, formattedName, formattedName);
    return namespace;
}

void ejsCreateNamespaceType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Namespace"), ejs->objectType, 
        sizeof(EjsNamespace), ES_Namespace, ES_Namespace_NUM_CLASS_PROP, ES_Namespace_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE);
    ejs->namespaceType = type;
    
    /*
     *  Define the helper functions.
     */
    type->helpers->castVar = (EjsCastVarHelper) castNamespace;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokeNamespaceOperator;
}

void ejsConfigureNamespaceType(Ejs *ejs)
{
    ejsSetProperty(ejs, ejs->global, ES_intrinsic, (EjsVar*) ejs->intrinsicSpace);
    ejsSetProperty(ejs, ejs->global, ES_iterator, (EjsVar*) ejs->iteratorSpace);
    ejsSetProperty(ejs, ejs->global, ES_public, (EjsVar*) ejs->publicSpace);
}

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsNamespace.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsNull.c"
 */
/************************************************************************/

/**
 *  ejsNull.c - Ejscript Null class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Cast the null operand to a primitive type
 */

static EjsVar *castNull(Ejs *ejs, EjsVar *vp, EjsType *type)
{
    switch (type->id) {
    case ES_Boolean:
        return (EjsVar*) ejs->falseValue;

    case ES_Number:
        return (EjsVar*) ejs->zeroValue;

    case ES_Object:
    default:
        /*
         *  Cast null to anything else results in a null
         */
        return vp;

    case ES_String:
        return (EjsVar*) ejsCreateString(ejs, "null");
    }
}


static EjsVar *coerceNullOperands(Ejs *ejs, EjsVar *lhs, int opcode, EjsVar *rhs)
{
    switch (opcode) {

    case EJS_OP_ADD:
        if (!ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        /* Fall through */

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejs->zeroValue, opcode, rhs);

    /*
     *  Comparision
     */
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejs->zeroValue, opcode, rhs);
        } else if (ejsIsString(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        break;

    case EJS_OP_COMPARE_NE:
        if (ejsIsUndefined(rhs)) {
            return (EjsVar*) ejs->falseValue;
        }
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_EQ:
        if (ejsIsUndefined(rhs)) {
            return (EjsVar*) ejs->trueValue;
        }
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return 0;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }
    return 0;
}


static EjsVar *invokeNullOperator(Ejs *ejs, EjsVar *lhs, int opcode, EjsVar *rhs)
{
    EjsVar      *result;

    if (rhs == 0 || lhs->type != rhs->type) {
        if ((result = coerceNullOperands(ejs, lhs, opcode, rhs)) != 0) {
            return result;
        }
    }

    /*
     *  Types now match. Both left and right types are both "null"
     */
    switch (opcode) {

    /*
     *  NOTE: strict eq is the same as eq
     */
    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_GE:
    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_NE: case EJS_OP_COMPARE_STRICTLY_NE:
    case EJS_OP_COMPARE_LT: case EJS_OP_COMPARE_GT:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return (EjsVar*) ejs->oneValue;

    /*
     *  Binary operators. Reinvoke with left = zero
     */
    case EJS_OP_ADD: case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejs->zeroValue, opcode, rhs);

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->type->qname.name);
        return 0;
    }
}


/*
 *  iterator native function get(): Iterator
 */
static EjsVar *getNullIterator(Ejs *ejs, EjsVar *np, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, np, NULL, 0, NULL);
}


static EjsVar *getNullProperty(Ejs *ejs, EjsNull *unused, int slotNum)
{
    ejsThrowReferenceError(ejs, "Object reference is null");
    return 0;
}


/*
 *  We dont actually allocate any nulls. We just reuse the singleton instance.
 */

EjsNull *ejsCreateNull(Ejs *ejs)
{
    return (EjsNull*) ejs->nullValue;
}


void ejsCreateNullType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Null"), ejs->objectType, sizeof(EjsNull),
        ES_Null, ES_Null_NUM_CLASS_PROP, ES_Null_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE);
    ejs->nullType = type;

    /*
     *  Define the helper functions.
     */
    type->helpers->castVar          = (EjsCastVarHelper) castNull;
    type->helpers->getProperty      = (EjsGetPropertyHelper) getNullProperty;
    type->helpers->invokeOperator   = (EjsInvokeOperatorHelper) invokeNullOperator;

    ejs->nullValue = ejsCreateVar(ejs, type, 0);
    ejs->nullValue->primitive = 1;
    ejsSetDebugName(ejs->nullValue, "null");
    
    if (!(ejs->flags & EJS_FLAG_EMPTY)) {
        ejsSetProperty(ejs, ejs->global, ES_null, ejs->nullValue);
    }
}


void ejsConfigureNullType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->nullType;

    ejsSetProperty(ejs, ejs->global, ES_null, ejs->nullValue);

    ejsBindMethod(ejs, type, ES_Object_get, getNullIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, getNullIterator);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsNull.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsNumber.c"
 */
/************************************************************************/

/**
    ejsNumber.c - Number type class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */




#if BLD_FEATURE_NUM_TYPE_DOUBLE
#define fixed(n) ((int64) (floor(n)))
#else
#define fixed(n) (n)
#endif

#if UNUSED
#if BLD_WIN_LIKE || VXWORKS
static double localRint(double num)
{
    double low = floor(num);
    double high = ceil(num);
    return ((high - num) >= (num - low)) ? low : high;
}
#define rint localRint
#endif
#endif

/*
    Cast the operand to the specified type
 */
static EjsVar *castNumber(Ejs *ejs, EjsNumber *vp, EjsType *type)
{
    switch (type->id) {
    case ES_Boolean:
        return (EjsVar*) ((vp->value) ? ejs->trueValue : ejs->falseValue);

    case ES_String:
        {
#if BLD_FEATURE_NUM_TYPE_DOUBLE
        char *result = mprDtoa(vp, vp->value, 0, 0, 0);
        return (EjsVar*) ejsCreateStringAndFree(ejs, result);
#elif MPR_64_BIT
        char     numBuf[32];
        mprSprintf(numBuf, sizeof(numBuf), "%Ld", vp->value);
        return (EjsVar*) ejsCreateString(ejs, numBuf);
#else
        char     numBuf[32];
        mprItoa(numBuf, sizeof(numBuf), (int) vp->value, 10);
        return (EjsVar*) ejsCreateString(ejs, numBuf);
#endif
        }

    case ES_Number:
        return (EjsVar*) vp;
            
    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
}


static EjsVar *coerceNumberOperands(Ejs *ejs, EjsVar *lhs, int opcode, EjsVar *rhs)
{
    switch (opcode) {
    /*
     *  Binary operators
     */
    case EJS_OP_ADD:
        if (ejsIsUndefined(rhs)) {
            return (EjsVar*) ejs->nanValue;
        } else if (ejsIsNull(rhs)) {
            return (EjsVar*) lhs;
        } else if (ejsIsBoolean(rhs) || ejsIsDate(rhs)) {
            return ejsInvokeOperator(ejs, lhs, opcode, (EjsVar*) ejsToNumber(ejs, rhs));
        } else {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        break;

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, lhs, opcode, (EjsVar*) ejsToNumber(ejs, rhs));

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_NE:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        if (ejsIsNull(rhs) || ejsIsUndefined(rhs)) {
            return (EjsVar*) ((opcode == EJS_OP_COMPARE_EQ) ? ejs->falseValue: ejs->trueValue);
        } else if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        } else if (ejsIsString(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, lhs, opcode, (EjsVar*) ejsToNumber(ejs, rhs));

    case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return 0;

    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) (((EjsNumber*) lhs)->value ? ejs->trueValue : ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
    case EJS_OP_COMPARE_FALSE:
        return (EjsVar*) (((EjsNumber*) lhs)->value ? ejs->falseValue: ejs->trueValue);

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->falseValue;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }
    return 0;
}


static EjsVar *invokeNumberOperator(Ejs *ejs, EjsNumber *lhs, int opcode, EjsNumber *rhs)
{
    EjsVar      *result;

    mprAssert(lhs);
    
    if (rhs == 0 || lhs->obj.var.type != rhs->obj.var.type) {
        if (!ejsIsA(ejs, (EjsVar*) lhs, ejs->numberType) || !ejsIsA(ejs, (EjsVar*) rhs, ejs->numberType)) {
            if ((result = coerceNumberOperands(ejs, (EjsVar*) lhs, opcode, (EjsVar*) rhs)) != 0) {
                return result;
            }
        }
    }

    /*
     *  Types now match, both numbers
     */
    switch (opcode) {

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ((lhs->value == rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_NE: case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ((lhs->value != rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_LT:
        return (EjsVar*) ((lhs->value < rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_LE:
        return (EjsVar*) ((lhs->value <= rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_GT:
        return (EjsVar*) ((lhs->value > rhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_GE:
        return (EjsVar*) ((lhs->value >= rhs->value) ? ejs->trueValue: ejs->falseValue);

    /*
     *  Unary operators
     */
    case EJS_OP_COMPARE_NOT_ZERO:
        return (EjsVar*) ((lhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ((lhs->value == 0) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_NEG:
        return (EjsVar*) ejsCreateNumber(ejs, -lhs->value);

    case EJS_OP_LOGICAL_NOT:
        return (EjsVar*) ejsCreateBoolean(ejs, !fixed(lhs->value));

    case EJS_OP_NOT:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (~fixed(lhs->value)));


    /*
     *  Binary operations
     */
    case EJS_OP_ADD:
        return (EjsVar*) ejsCreateNumber(ejs, lhs->value + rhs->value);

    case EJS_OP_AND:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (fixed(lhs->value) & fixed(rhs->value)));

    case EJS_OP_DIV:
#if !BLD_FEATURE_NUM_TYPE_DOUBLE
        if (rhs->value == 0) {
            ejsThrowArithmeticError(ejs, "Divisor is zero");
            return 0;
        }
#endif
        return (EjsVar*) ejsCreateNumber(ejs, lhs->value / rhs->value);

    case EJS_OP_MUL:
        return (EjsVar*) ejsCreateNumber(ejs, lhs->value * rhs->value);

    case EJS_OP_OR:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (fixed(lhs->value) | fixed(rhs->value)));

    case EJS_OP_REM:
#if BLD_FEATURE_NUM_TYPE_DOUBLE
        if (rhs->value == 0) {
            ejsThrowArithmeticError(ejs, "Divisor is zero");
            return 0;
        }
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (fixed(lhs->value) % fixed(rhs->value)));
#else
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (fixed(lhs->value) % fixed(rhs->value)));
#endif

    case EJS_OP_SHL:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (fixed(lhs->value) << fixed(rhs->value)));

    case EJS_OP_SHR:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (fixed(lhs->value) >> fixed(rhs->value)));

    case EJS_OP_SUB:
        return (EjsVar*) ejsCreateNumber(ejs, lhs->value - rhs->value);

    case EJS_OP_USHR:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (fixed(lhs->value) >> fixed(rhs->value)));

    case EJS_OP_XOR:
        return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) (fixed(lhs->value) ^ fixed(rhs->value)));

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->obj.var.type->qname.name);
        return 0;
    }
}


/*
    Number constructor.
        function Number(value: Object = null)
 */
static EjsVar *numberConstructor(Ejs *ejs, EjsNumber *np, int argc, EjsVar **argv)
{
    EjsNumber   *num;

    mprAssert(argc == 0 || argc == 1);

    if (argc == 1) {
        num = ejsToNumber(ejs, argv[0]);
        if (num) {
            np->value = num->value;
        }
    }
    return (EjsVar*) np;
}


/*
    Function to iterate and return each number in sequence.
    NOTE: this is not a method of Number. Rather, it is a callback function for Iterator.
 */
static EjsVar *nextNumber(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsNumber   *np;

    np = (EjsNumber*) ip->target;
    if (!ejsIsNumber(np)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    if (ip->index < np->value) {
        return (EjsVar*) ejsCreateNumber(ejs, ip->index++);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
    function integral(size: Number = 32): Number
 */
static EjsVar *integral(Ejs *ejs, EjsNumber *np, int argc, EjsVar **argv)
{
    int64   mask, result;
    int     size;

    size = (argc > 0) ? ejsGetInt(argv[0]) : 32;

    result = ((int64) np->value);
    if (size < 64) {
        mask = 1;
        mask = (mask << size) - 1;
        result &= mask;
    }
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) result);
}


/*
    function get isFinite(): Boolean
 */
static EjsVar *isFinite(Ejs *ejs, EjsNumber *np, int argc, EjsVar **argv)
{
    if (np->value == ejs->nanValue->value || 
            np->value == ejs->infinityValue->value || 
            np->value == ejs->negativeInfinityValue->value) {
        return (EjsVar*) ejs->falseValue;
    }
    return (EjsVar*) ejs->trueValue;
}


/*
    function get isNaN(): Boolean
 */
static EjsVar *isNaN(Ejs *ejs, EjsNumber *np, int argc, EjsVar **argv)
{
#if BLD_FEATURE_NUM_TYPE_DOUBLE
    return (EjsVar*) (mprIsNan(np->value) ? ejs->trueValue : ejs->falseValue);
#else
    return (EjsVar*) ejs->falseValue;
#endif
}


/*
    function toExponential(fractionDigits: Number = 0): String
    Display with only one digit before the decimal point.
 */
static EjsVar *toExponential(Ejs *ejs, EjsNumber *np, int argc, EjsVar **argv)
{
#if BLD_FEATURE_NUM_TYPE_DOUBLE
    char    *result;
    int     ndigits;
    
    ndigits = (argc > 0) ? ejsGetInt(argv[0]): 0;
    result = mprDtoa(np, np->value, ndigits, MPR_DTOA_N_DIGITS, MPR_DTOA_EXPONENT_FORM);
    return (EjsVar*) ejsCreateStringAndFree(ejs, result);
#else
    char    numBuf[32];
    mprItoa(numBuf, sizeof(numBuf), (int) np->value, 10);
    return (EjsVar*) ejsCreateString(ejs, numBuf);
#endif
}


/*
    function toFixed(fractionDigits: Number = 0): String

    Display the specified number of fractional digits
 */
static EjsVar *toFixed(Ejs *ejs, EjsNumber *np, int argc, EjsVar **argv)
{
#if BLD_FEATURE_NUM_TYPE_DOUBLE
    char    *result;
    int     ndigits;
    
    ndigits = (argc > 0) ? ejsGetInt(argv[0]) : 0;
    result = mprDtoa(np, np->value, ndigits, MPR_DTOA_N_FRACTION_DIGITS, MPR_DTOA_FIXED_FORM);
    return (EjsVar*) ejsCreateStringAndFree(ejs, result);
#else
    char    numBuf[32];
    mprItoa(numBuf, sizeof(numBuf), (int) np->value, 10);
    return (EjsVar*) ejsCreateString(ejs, numBuf);
#endif
}


/*
    function toPrecision(numDigits: Number = MAX_VALUE): String
    Display the specified number of total digits
 */
static EjsVar *toPrecision(Ejs *ejs, EjsNumber *np, int argc, EjsVar **argv)
{
#if BLD_FEATURE_NUM_TYPE_DOUBLE
    char    *result;
    int     ndigits;
    
    ndigits = (argc > 0) ? ejsGetInt(argv[0]) : 0;
    result = mprDtoa(np, np->value, ndigits, MPR_DTOA_N_DIGITS, 0);
    return (EjsVar*) ejsCreateStringAndFree(ejs, result);
#else
    char    numBuf[32];
    mprItoa(numBuf, sizeof(numBuf), (int) np->value, 10);
    return (EjsVar*) ejsCreateString(ejs, numBuf);
#endif
}


/*
    Return the default iterator. This returns the index names.
    iterator native function get(): Iterator
 */
static EjsVar *getNumberIterator(Ejs *ejs, EjsVar *np, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, np, (EjsNativeFunction) nextNumber, 0, NULL);
}


/*
    Convert the number to a string.
    intrinsic function toString(): String
 */
static EjsVar *numberToString(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    return castNumber(ejs, (EjsNumber*) vp, ejs->stringType);
}



#ifndef ejsIsNan
int ejsIsNan(double f)
{
#if BLD_FEATURE_NUM_TYPE_DOUBLE
#if BLD_WIN_LIKE
    return _isnan(f);
#elif VXWORKS
    return 0;
#else
    return (f == FP_NAN);
#endif
#else
    return 0;
#endif
}
#endif


bool ejsIsInfinite(MprNumber f)
{
#if BLD_FEATURE_NUM_TYPE_DOUBLE
#if BLD_WIN_LIKE
    return !_finite(f);
#elif VXWORKS
    return 0;
#else
    return (f == FP_INFINITE);
#endif
#else
    return 0;
#endif
}

/*
    Create an initialized number
 */

EjsNumber *ejsCreateNumber(Ejs *ejs, MprNumber value)
{
    EjsNumber   *vp;

    if (value == 0) {
        return ejs->zeroValue;
    } else if (value == 1) {
        return ejs->oneValue;
    } else if (value == -1) {
        return ejs->minusOneValue;
    }

    vp = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    if (vp != 0) {
        vp->value = value;
        vp->obj.var.primitive = 1;
    }
    ejsSetDebugName(vp, "number value");
    return vp;
}


void ejsCreateNumberType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;
#if BLD_FEATURE_NUM_TYPE_DOUBLE
    static double zero = 0.0;
#endif

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Number"), ejs->objectType, sizeof(EjsNumber),
        ES_Number, ES_Number_NUM_CLASS_PROP, ES_Number_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
    ejs->numberType = type;

    /*
     *  Define the helper functions.
     */
    type->helpers->castVar = (EjsCastVarHelper) castNumber;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokeNumberOperator;

    ejs->zeroValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->zeroValue->value = 0;
    ejs->oneValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->oneValue->value = 1;
    ejs->minusOneValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->minusOneValue->value = -1;

#if BLD_FEATURE_NUM_TYPE_DOUBLE
    ejs->infinityValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->infinityValue->value = 1.0 / zero;
    ejs->negativeInfinityValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->negativeInfinityValue->value = -1.0 / zero;
    ejs->nanValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->nanValue->value = 0.0 / zero;

    ejs->maxValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->maxValue->value = 1.7976931348623157e+308;
    ejs->minValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->minValue->value = 5e-324;

    ejsSetDebugName(ejs->infinityValue, "Infinity");
    ejsSetDebugName(ejs->negativeInfinityValue, "NegativeInfinity");
    ejsSetDebugName(ejs->nanValue, "NaN");

#else
    ejs->maxValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->maxValue->value = MAXINT;
    ejs->minValue = (EjsNumber*) ejsCreateVar(ejs, ejs->numberType, 0);
    ejs->minValue->value = -MAXINT;
    ejs->nanValue = ejs->zeroValue;
    ejs->infinityValue = ejs->maxValue;
    ejs->negativeInfinityValue = ejs->minValue;
    ejs->nanValue = (EjsNumber*) ejs->undefinedValue;
#endif

    ejsSetDebugName(ejs->minusOneValue, "-1");
    ejsSetDebugName(ejs->oneValue, "1");
    ejsSetDebugName(ejs->zeroValue, "0");
    ejsSetDebugName(ejs->maxValue, "MaxValue");
    ejsSetDebugName(ejs->minValue, "MinValue");
}


void ejsConfigureNumberType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->numberType;

    ejsSetProperty(ejs, (EjsVar*) type, ES_Number_MaxValue, (EjsVar*) ejs->maxValue);
    ejsSetProperty(ejs, (EjsVar*) type, ES_Number_MinValue, (EjsVar*) ejs->minValue);
    ejsBindMethod(ejs, type, ES_Number_Number, (EjsNativeFunction) numberConstructor);
    ejsBindMethod(ejs, type, ES_Number_integral, (EjsNativeFunction) integral);
    ejsBindMethod(ejs, type, ES_Number_isFinite, (EjsNativeFunction) isFinite);
    ejsBindMethod(ejs, type, ES_Number_isNaN, (EjsNativeFunction) isNaN);
    ejsBindMethod(ejs, type, ES_Number_toExponential, (EjsNativeFunction) toExponential);
    ejsBindMethod(ejs, type, ES_Number_toFixed, (EjsNativeFunction) toFixed);
    ejsBindMethod(ejs, type, ES_Number_toPrecision, (EjsNativeFunction) toPrecision);
    ejsBindMethod(ejs, type, ES_Object_get, getNumberIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, getNumberIterator);
    ejsBindMethod(ejs, type, ES_Object_toString, numberToString);
    ejsSetProperty(ejs, (EjsVar*) type, ES_Number_NEGATIVE_INFINITY, (EjsVar*) ejs->negativeInfinityValue);
    ejsSetProperty(ejs, (EjsVar*) type, ES_Number_POSITIVE_INFINITY, (EjsVar*) ejs->infinityValue);
    ejsSetProperty(ejs, (EjsVar*) type, ES_Number_NaN, (EjsVar*) ejs->nanValue);
    ejsSetProperty(ejs, ejs->global, ES_NegativeInfinity, (EjsVar*) ejs->negativeInfinityValue);
    ejsSetProperty(ejs, ejs->global, ES_Infinity, (EjsVar*) ejs->infinityValue);
    ejsSetProperty(ejs, ejs->global, ES_NaN, (EjsVar*) ejs->nanValue);
    ejsSetProperty(ejs, ejs->global, ES_num, (EjsVar*) type);
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://www.embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsNumber.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsObject.c"
 */
/************************************************************************/

/**
 *  ejsObject.c - Object class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static EjsName  getObjectPropertyName(Ejs *ejs, EjsObject *obj, int slotNum);
static int      growSlots(Ejs *ejs, EjsObject *obj, int size);
static int      hashProperty(EjsObject *obj, int slotNum, EjsName *qname);
static int      lookupObjectProperty(struct Ejs *ejs, EjsObject *obj, EjsName *qname);
static int      makeHash(EjsObject *obj);
static inline int cmpQname(EjsName *a, EjsName *b);
static void     removeHashEntry(EjsObject  *obj, EjsName *qname);
static EjsVar   *objectToString(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv);

#define CMP_QNAME(a,b) cmpQname(a, b)

#if KEEP
static inline int cmpName(EjsName *a, EjsName *b);
#define CMP_NAME(a,b) cmpName(a, b)
#endif

/*
 *  Cast the operand to a primitive type
 *
 *  intrinsic function cast(type: Type) : Object
 */
static EjsVar *castObject(Ejs *ejs, EjsObject *obj, EjsType *type)
{
    EjsString   *result;
    
    mprAssert(ejsIsType(type));

    switch (type->id) {
    case ES_Boolean:
        return (EjsVar*) ejsCreateBoolean(ejs, 1);

    case ES_Number:
        result = ejsToString(ejs, (EjsVar*) obj);
        if (result == 0) {
            ejsThrowMemoryError(ejs);
            return 0;
        }
        return ejsParseVar(ejs, ejsGetString(result), ES_Number);

    case ES_String:
        result = ejsCreateStringAndFree(ejs, mprStrcat(ejs, -1, "[object ", obj->var.type->qname.name, "]", NULL));
        return (EjsVar*) result;

    default:
        if (ejsIsA(ejs, (EjsVar*) obj, type)) {
            return (EjsVar*) obj;
        }
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
}


/*
 *  Create an object which is an instance of a given type. This is used by all scripted types to create objects. NOTE: 
 *  we only initialize the Object base class. It is up to the  caller to complete the initialization for all other base 
 *  classes by calling the appropriate constructors. capacity is the number of property slots to pre-allocate. Slots are 
 *  allocated and the property hash is configured.  If dynamic is true, then the property slots are allocated separately
 *  and can grow. 
 */
EjsObject *ejsCreateObject(Ejs *ejs, EjsType *type, int numExtraSlots)
{
    EjsObject   *obj;
    EjsBlock    *instanceBlock;
    EjsType     *tp;
    int         numSlots, roundSlots, size, hasNativeType;

    mprAssert(type);
    mprAssert(numExtraSlots >= 0);

    instanceBlock = type->instanceBlock;

    numSlots = numExtraSlots;
    if (instanceBlock) {
        numSlots += instanceBlock->obj.numProp;
    }

    /*
     *  Check if a script type is extending a native type. If so, we must not use integrated slots
     */
    hasNativeType = 0;
    if (!type->block.obj.var.native) {
        for (tp = type->baseType; tp && tp != ejs->objectType; tp = tp->baseType) {
            hasNativeType += tp->block.obj.var.native;
        }
    }

    roundSlots = 0;
    if (type->dontPool || hasNativeType || (obj = (EjsObject*) ejsAllocPooledVar(ejs, type->id)) == 0) {
        roundSlots = max(numSlots, EJS_MIN_OBJ_SLOTS);
        if (hasNativeType) {
            if ((obj = (EjsObject*) ejsAllocVar(ejs, type, 0)) == 0) {
                return 0;
            }

        } else {
            if ((obj = (EjsObject*) ejsAllocVar(ejs, type, roundSlots * sizeof(EjsVar*))) == 0) {
                return 0;
            }
            /*
             *  Always begin with objects using integrated slots
             */
            obj->slots = (EjsVar**) &(((char*) obj)[type->instanceSize]);
            obj->capacity = roundSlots;
        }

    } else {
        size = type->instanceSize - sizeof(EjsObject);
    }
    obj->var.type = type;
    obj->var.isObject = 1;
    obj->var.dynamic = type->block.dynamicInstance;
    obj->var.hidden = 0;
    ejsSetDebugName(obj, type->qname.name);

    if (numSlots > 0) {
        if (numSlots > obj->capacity) {
            ejsGrowObject(ejs, obj, numSlots);
        }
        ejsZeroSlots(ejs, obj->slots, numSlots);
    }
    obj->numProp = numSlots;
    obj->names = (instanceBlock) ? instanceBlock->obj.names : 0;
    return obj;
}


EjsObject *ejsCreateSimpleObject(Ejs *ejs)
{
    return ejsCreateObject(ejs, ejs->objectType, 0);
}


EjsObject *ejsCopyObject(Ejs *ejs, EjsObject *src, bool deep)
{
    EjsObject   *dest;
    int         numProp, i;

    numProp = src->numProp;

    dest = ejsCreateObject(ejs, src->var.type, numProp);
    if (dest == 0) {
        return 0;
    }
    
    dest->var.builtin = src->var.builtin;
    dest->var.dynamic = src->var.dynamic;
    dest->var.hasGetterSetter = src->var.hasGetterSetter;
    dest->var.hasNativeBase = src->var.hasNativeBase;
    dest->var.hidden = src->var.hidden;
    dest->var.isFunction = src->var.isFunction;
    dest->var.isObject = src->var.isObject;
    dest->var.isInstanceBlock = src->var.isInstanceBlock;
    dest->var.isType = src->var.isType;
    dest->var.native = src->var.native;
    dest->var.noPool = src->var.noPool;
    dest->var.permanent = src->var.permanent;
    dest->var.primitive = src->var.primitive;
    dest->var.survived = src->var.survived;
    ejsSetDebugName(dest, mprGetName(src));

    if (numProp <= 0) {
        return dest;
    }

    for (i = 0; i < numProp; i++) {
        if (deep) {
            dest->slots[i] = ejsCloneVar(ejs, src->slots[i], deep);
        } else {
            dest->slots[i] = src->slots[i];
        }
    }

    if (dest->names == NULL && ejsGrowObjectNames(dest, numProp) < 0) {
        return 0;
    }
    for (i = 0; i < numProp && src->names; i++) {
        dest->names->entries[i].qname.space = mprStrdup(dest, src->names->entries[i].qname.space);
        dest->names->entries[i].qname.name = mprStrdup(dest, src->names->entries[i].qname.name);
        dest->names->entries[i].nextSlot = src->names->entries[i].nextSlot;
    }
    if (makeHash(dest) < 0) {
        return 0;
    }
    return dest;
}


/*
 *  Define a new property.
 */
static int defineObjectProperty(Ejs *ejs, EjsBlock *block, int slotNum, EjsName *qname, EjsType *propType, int attributes, 
    EjsVar *value)
{

    if (ejsIsBlock(block)) {
        return (ejs->blockHelpers->defineProperty)(ejs, (EjsVar*) block, slotNum, qname, propType, attributes, value);

    } else {
        ejsThrowInternalError(ejs, "Helper not defined for non-block object");
        return 0;
    }
}


/*
 *  Delete an instance property. To delete class properties, use the type as the obj.
 */
static int deleteObjectProperty(Ejs *ejs, EjsObject *obj, int slotNum)
{
    EjsName     qname;

    mprAssert(obj);
    mprAssert(obj->var.type);
    mprAssert(slotNum >= 0);

    if (!obj->var.dynamic && !(ejs->flags & EJS_FLAG_COMPILER)) {
        ejsThrowTypeError(ejs, "Can't delete properties in a non-dynamic object");
        return EJS_ERR;
    }
    if (slotNum < 0 || slotNum >= obj->numProp) {
        ejsThrowReferenceError(ejs, "Invalid property slot to delete");
        return EJS_ERR;
    }
    qname = getObjectPropertyName(ejs, obj, slotNum);
    if (qname.name == 0) {
        return EJS_ERR;
    }
    removeHashEntry(obj, &qname);
    obj->slots[slotNum] = ejs->undefinedValue;
    return 0;
}


/*
 *  Delete an instance property by name
 */
static int deleteObjectPropertyByName(Ejs *ejs, EjsObject *obj, EjsName *qname)
{
    int     slotNum;

    slotNum = lookupObjectProperty(ejs, obj, qname);
    if (slotNum < 0) {
        ejsThrowReferenceError(ejs, "Property does not exist");
        return EJS_ERR;
    } else {
        return deleteObjectProperty(ejs, obj, slotNum);
    }
}


static EjsVar *getObjectProperty(Ejs *ejs, EjsObject *obj, int slotNum)
{
    mprAssert(obj);
    mprAssert(obj->slots);
    mprAssert(slotNum >= 0);

    if (slotNum < 0 || slotNum >= obj->numProp) {
        ejsThrowReferenceError(ejs, "Property at slot \"%d\" is not found", slotNum);
        return 0;
    }
    return obj->slots[slotNum];
}


/*
 *  Return the number of properties in the object
 */
static int getObjectPropertyCount(Ejs *ejs, EjsObject *obj)
{
    mprAssert(obj);
    mprAssert(ejsIsObject(obj));

    return obj->numProp;
}


static EjsName getObjectPropertyName(Ejs *ejs, EjsObject *obj, int slotNum)
{
    EjsName     qname;

    mprAssert(obj);
    mprAssert(ejsIsObject(obj) || ejsIsArray(obj));
    mprAssert(obj->slots);
    mprAssert(slotNum >= 0);
    mprAssert(slotNum < obj->numProp);

    if (slotNum < 0 || slotNum >= obj->numProp || obj->names == 0) {
        qname.name = 0;
        qname.space = 0;
        return qname;
    }
    return obj->names->entries[slotNum].qname;
}


/*
 *  Cast the operands depending on the operation code
 */
EjsVar *ejsCoerceOperands(Ejs *ejs, EjsVar *lhs, int opcode, EjsVar *rhs)
{
    switch (opcode) {

    /*
     *  Binary operators
     */
    case EJS_OP_ADD:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejs->zeroValue, opcode, rhs);

    case EJS_OP_COMPARE_EQ:  case EJS_OP_COMPARE_NE:
        if (ejsIsNull(rhs) || ejsIsUndefined(rhs)) {
            return (EjsVar*) ((opcode == EJS_OP_COMPARE_EQ) ? ejs->falseValue: ejs->trueValue);
        } else if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_STRICTLY_NE:
    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT:
        return 0;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }
    return 0;
}


EjsVar *ejsObjectOperator(Ejs *ejs, EjsVar *lhs, int opcode, EjsVar *rhs)
{
    EjsVar      *result;

    if (rhs == 0 || lhs->type != rhs->type) {
        if ((result = ejsCoerceOperands(ejs, lhs, opcode, rhs)) != 0) {
            return result;
        }
    }

    /*
     *  Types now match
     */
    switch (opcode) {

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_GE:
        return (EjsVar*) ejsCreateBoolean(ejs, (lhs == rhs));

    case EJS_OP_COMPARE_NE: case EJS_OP_COMPARE_STRICTLY_NE:
    case EJS_OP_COMPARE_LT: case EJS_OP_COMPARE_GT:
        return (EjsVar*) ejsCreateBoolean(ejs, !(lhs == rhs));

    /*
     *  Unary operators
     */
    case EJS_OP_COMPARE_NOT_ZERO:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return (EjsVar*) ejs->oneValue;

    /*
     *  Binary operators
     */
    case EJS_OP_ADD: case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL:
    case EJS_OP_REM: case EJS_OP_OR: case EJS_OP_SHL: case EJS_OP_SHR:
    case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, (EjsVar*) ejsToNumber(ejs, rhs));

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->type->qname.name);
        return 0;
    }
    mprAssert(0);
}


/*
 *  Lookup a property with a namespace qualifier in an object and return the slot if found. Return EJS_ERR if not found.
 */
static int lookupObjectProperty(struct Ejs *ejs, EjsObject *obj, EjsName *qname)
{
    EjsNames    *names;
    EjsName     *propName;
    int         slotNum, index;

    mprAssert(qname);
    mprAssert(qname->name);
    mprAssert(qname->space);

    names = obj->names;

    if (names == 0) {
        return EJS_ERR;
    }

    if (names->buckets == 0) {
        /*
         *  No hash. Just do a linear search
         */
        for (slotNum = 0; slotNum < obj->numProp; slotNum++) {
            propName = &names->entries[slotNum].qname;
            if (CMP_QNAME(propName, qname)) {
                return slotNum;
            }
        }
        return EJS_ERR;
    }
    
    /*
     *  Find the property in the hash chain if it exists. Note the hash does not include the namespace portion.
     *  We assume that names rarely clash with different namespaces. We do this so variable lookup and do a one
     *  hash probe and find matching names. Lookup will then pick the right namespace.
     */
    index = ejsComputeHashCode(names, qname);

    for (slotNum = names->buckets[index]; slotNum >= 0;  slotNum = names->entries[slotNum].nextSlot) {
        propName = &names->entries[slotNum].qname;
        /*
         *  Compare the name including the namespace portion
         */
        if (CMP_QNAME(propName, qname)) {
            return slotNum;
        }
    }
    return EJS_ERR;
}


#if KEEP
/*
 *  Lookup a qualified property name and count the number of name portion matches. This routine is used to quickly lookup a 
 *  qualified name AND determine if there are other names with different namespaces but having the same name portion.
 *  Returns EJS_ERR if more than one matching property is found (ie. two properties of the same name but with different 
 *  namespaces). This should be rare! Otherwise, return the slot number of the unique matching property.
 *
 *  This is a special lookup routine for fast varible lookup in the scope chain. Used by ejsLookupVar and ejsLookupScope.
 *  WARNING: updates qname->space
 */
int ejsLookupSingleProperty(Ejs *ejs, EjsObject *obj, EjsName *qname)
{
    EjsNames    *names;
    EjsName     *propName;
    int         i, slotNum, index, count;

    mprAssert(qname);
    mprAssert(qname->name);
    mprAssert(qname->space);
    mprAssert(qname->space[0] == '\0');

    names = obj->names;
    slotNum = -1;
    count = 0;

    if (names) {
        if (names->buckets == 0) {
            /*
             *  No hash. Just do a linear search. Examine all properties.
             */
            for (i = 0; i < obj->numProp; i++) {
                propName = &names->entries[i].qname;
                if (CMP_NAME(propName, qname)) {
                    count++;
                    slotNum = i;
                }
            }

        } else {
            /*
             *  Find the property in the hash chain if it exists. Note the hash does NOT include the namespace portion.
             *  We assume that names rarely clash with different namespaces. We do this so variable lookup and a single hash 
             *  probe will find matching names.
             */
            index = ejsComputeHashCode(names, qname);

            for (i = names->buckets[index]; i >= 0;  i = names->entries[i].nextSlot) {
                propName = &names->entries[i].qname;
                if (CMP_NAME(propName, qname)) {
                    slotNum = i;
                    count++;
                }
            }
        }
        if (count == 1) {
            if (mprLookupHash(ejs->standardSpaces, names->entries[slotNum].qname.space)) {
                qname->space = names->entries[slotNum].qname.space;
            } else {
                slotNum = -2;
            }
        }
    }

    return (count <= 1) ? slotNum : -2;
}
#endif

/*
 *  Mark the object properties for the garbage collector
 */
void ejsMarkObject(Ejs *ejs, EjsVar *parent, EjsObject *obj)
{
    EjsType     *type;
    EjsVar      *vp;
    int         i;

    mprAssert(ejsIsObject(obj) || ejsIsBlock(obj) || ejsIsFunction(obj) || ejsIsArray(obj) || ejsIsXML(obj));

    type = obj->var.type;

    for (i = 0; i < obj->numProp; i++) {
        vp = obj->slots[i];
        if (vp == 0 || vp == ejs->nullValue) {
            continue;
        }
        ejsMarkVar(ejs, (EjsVar*) obj, vp);
    }
}


/*
 *  Validate the supplied slot number. If set to -1, then return the next available property slot number.
 */
inline int ejsCheckObjSlot(Ejs *ejs, EjsObject *obj, int slotNum)
{
    if (slotNum < 0) {
        if (!obj->var.dynamic) {
            ejsThrowReferenceError(ejs, "Object is not dynamic");
            return EJS_ERR;
        }

        slotNum = obj->numProp;
        if (obj->numProp >= obj->capacity) {
            if (ejsGrowObject(ejs, obj, obj->numProp + 1) < 0) {
                ejsThrowMemoryError(ejs);
                return EJS_ERR;
            }
        } else {
            obj->numProp++;
        }

    } else if (slotNum >= obj->numProp) {
        if (ejsGrowObject(ejs, obj, slotNum + 1) < 0) {
            ejsThrowMemoryError(ejs);
            return EJS_ERR;
        }
    }
    return slotNum;
}


/**
 *  Set the value of a property.
 *  @param slot If slot is -1, then allocate the next free slot
 *  @return Return the property slot if successful. Return < 0 otherwise.
 */
static int setObjectProperty(Ejs *ejs, EjsObject *obj, int slotNum, EjsVar *value)
{
    mprAssert(ejs);
    mprAssert(obj);
    
    if ((slotNum = ejsCheckObjSlot(ejs, obj, slotNum)) < 0) {
        return EJS_ERR;
    }
    
    mprAssert(slotNum < obj->numProp);
    mprAssert(obj->numProp <= obj->capacity);

    if (obj->var.permanent && (EjsVar*) obj != ejs->global && !value->permanent) {
        value->permanent = 1;
    }
    mprAssert(value);
    obj->slots[slotNum] = value;
    return slotNum;
}


/*
 *  Set the name for a property. Objects maintain a hash lookup for property names. This is hash is created on demand 
 *  if there are more than N properties. If an object is not dynamic, it will use the types name hash. If dynamic, 
 *  then the types name hash will be copied when required. Callers must supply persistent names strings in qname. 
 */
static int setObjectPropertyName(Ejs *ejs, EjsObject *obj, int slotNum, EjsName *qname)
{
    EjsNames    *names;

    mprAssert(obj);
    mprAssert(qname);
    mprAssert(slotNum >= 0);

    if ((slotNum = ejsCheckObjSlot(ejs, obj, slotNum)) < 0) {
        return EJS_ERR;
    }

    /*
     *  If the hash is owned by the base type and this is a dynamic object, we need a new hash dedicated to the object.
     */
    if (obj->names == NULL) {
        if (ejsGrowObjectNames(obj, obj->numProp) < 0) {
            return EJS_ERR;
        }

    } else if (obj->var.dynamic && obj != mprGetParent(obj->names)) {
        /*
         *  Object is using the type's original names, must copy and use own names from here on.
         */
        if (ejsGrowObjectNames(obj, obj->numProp) < 0) {
            return EJS_ERR;
        }

    } else if (slotNum >= obj->names->sizeEntries) {
        if (ejsGrowObjectNames(obj, obj->numProp) < 0) {
            return EJS_ERR;
        }
    }
    names = obj->names;

    /*
     *  Remove the old hash entry if the name will change
     */
    if (names->entries[slotNum].nextSlot >= 0) {
        if (CMP_QNAME(&names->entries[slotNum].qname, qname)) {
            return slotNum;
        }
        removeHashEntry(obj, &names->entries[slotNum].qname);
    }

    /*
     *  Set the property name
     */
    names->entries[slotNum].qname = *qname;
    
    mprAssert(slotNum < obj->numProp);
    mprAssert(obj->numProp <= obj->capacity);
    
    if (obj->numProp <= EJS_HASH_MIN_PROP || qname->name == NULL) {
        return slotNum;
    }
    if (hashProperty(obj, slotNum, qname) < 0) {
        ejsThrowMemoryError(ejs);
        return EJS_ERR;
    }
    return slotNum;
}


void ejsMakePropertyDontDelete(EjsVar *vp, int dontDelete)
{
}


/*
 *  Set a property's enumerability by for/in. Return true if the property was enumerable.
 */

int ejsMakePropertyEnumerable(EjsVar *vp, bool enumerate)
{
    int     oldValue;

    oldValue = vp->hidden;
    vp->hidden = !enumerate;
    return oldValue;
}


/*
 *  Grow the slot storage for the object and increase numProp
 */
int ejsGrowObject(Ejs *ejs, EjsObject *obj, int count)
{
    int     size;
    
    if (count <= 0) {
        return 0;
    }

    mprAssert(count >= obj->numProp);
    size = EJS_PROP_ROUNDUP(count);

    if (obj->capacity < count) {
        if (growSlots(ejs, obj, size) < 0) {
            return EJS_ERR;
        }
    }   
    if (obj->names && obj->names->sizeEntries < count) {
        if (ejsGrowObjectNames(obj, size) < 0) {
            return EJS_ERR;
        }
        if (obj->numProp > 0 && makeHash(obj) < 0) {
            return EJS_ERR;
        }
        mprAssert(obj->names->sizeEntries >= obj->numProp);
    }   
    if (count > obj->numProp) {
        obj->numProp = count;
    }
    
    mprAssert(count <= obj->capacity);
    mprAssert(obj->numProp <= obj->capacity);
    
    return 0;
}


/*
 *  Insert new slots at the specified offset and move up slots to make room. Increase numProp.
 */
int ejsInsertGrowObject(Ejs *ejs, EjsObject *obj, int incr, int offset)
{
    EjsHashEntry    *entries;
    EjsNames        *names;
    int             i, size, mark;

    mprAssert(obj);
    mprAssert(incr >= 0);

    if (incr == 0) {
        return 0;
    }
    
    /*
     *  Base this comparison on numProp and not on capacity as we may already have room to fit the inserted properties.
     */
    size = obj->numProp + incr;

    if (obj->capacity < size) {
        size = EJS_PROP_ROUNDUP(size);
        if (ejsGrowObjectNames(obj, size) < 0) {
            return EJS_ERR;
        }
        if (growSlots(ejs, obj, size) < 0) {
            return EJS_ERR;
        }
    }
    obj->numProp += incr;
    
    mprAssert(obj->numProp <= obj->capacity);
    
    if (ejsGrowObjectNames(obj, obj->numProp) < 0) {
        return EJS_ERR;
    }
    mprAssert(obj->names);
    names = obj->names;
    mark = offset + incr;
    for (i = obj->numProp - 1; i >= mark; i--) {
        obj->slots[i] = obj->slots[i - mark];
        names->entries[i] = names->entries[i - mark];
    }

    ejsZeroSlots(ejs, &obj->slots[offset], incr);

    entries = names->entries;
    for (i = offset; i < mark; i++) {
        entries[i].nextSlot = -1;
        entries[i].qname.name = "";
        entries[i].qname.space = "";
    }
    if (makeHash(obj) < 0) {
        return EJS_ERR;
    }   
    return 0;
}


/*
 *  Allocate or grow the slots storage for an object
 */
static int growSlots(Ejs *ejs, EjsObject *obj, int capacity)
{
    EjsVar      **slots;
    int         factor;

    mprAssert(obj);

    if (capacity <= obj->capacity) {
        return 0;
    }

    /*
     *  Allocate or grow the slots structures
     */
    if (capacity > obj->capacity) {
        if (obj->capacity > EJS_LOTSA_PROP) {
            /*
             *  Looks like a big object so grow by a bigger chunk.
             */
            factor = max(obj->capacity / 4, EJS_NUM_PROP);
            capacity = (capacity + factor) / factor * factor;
        }
        capacity = EJS_PROP_ROUNDUP(capacity);

        if (obj->slots == 0) {
            mprAssert(obj->capacity == 0);
            mprAssert(capacity > 0);
            obj->slots = (EjsVar**) mprAlloc(obj, sizeof(EjsVar*) * capacity);
            if (obj->slots == 0) {
                return EJS_ERR;
            }
            ejsZeroSlots(ejs, obj->slots, capacity);

        } else {
            if (obj->var.separateSlots) {
                mprAssert(obj->capacity > 0);
                obj->slots = (EjsVar**) mprRealloc(obj, obj->slots, sizeof(EjsVar*) * capacity);
            } else {
                slots = (EjsVar**) mprAlloc(obj, sizeof(EjsVar*) * capacity);
                memcpy(slots, obj->slots, obj->capacity * sizeof(EjsVar*));
                obj->var.separateSlots = 1;
                obj->slots = slots;
            }
            if (obj->slots == 0) {
                return EJS_ERR;
            }
            ejsZeroSlots(ejs, &obj->slots[obj->capacity], (capacity - obj->capacity));
        }
        obj->capacity = capacity;
    }
    return 0;
}


/*
 *  Remove a slot and name. Copy up all other properties. WARNING: this can only be used before property binding and 
 *  should only be used by the compiler.
 */
void ejsRemoveSlot(Ejs *ejs, EjsObject *obj, int slotNum, int compact)
{
    EjsNames    *names;
    int         i;

    mprAssert(obj);
    mprAssert(slotNum >= 0);
    mprAssert(slotNum >= 0);
    mprAssert(ejs->flags & EJS_FLAG_COMPILER);

    names = obj->names;

    if (compact) {
        mprAssert(names);

        for (i = slotNum + 1; i < obj->numProp; i++) {
            obj->slots[i - 1] = obj->slots[i];
            names->entries[i - 1] = names->entries[i];
        }
        obj->numProp--;
        i--;

    } else {
        i = slotNum;
    }

    obj->slots[i] = 0;
    names->entries[i].qname.name = "";
    names->entries[i].qname.space = "";
    names->entries[i].nextSlot = -1;
    
    makeHash(obj);
}



/*
 *  Exponential primes
 */
static int hashSizes[] = {
     19, 29, 59, 79, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 0
};


int ejsGetHashSize(int numProp)
{
    int     i;

    for (i = 0; i < hashSizes[i]; i++) {
        if (numProp < hashSizes[i]) {
            return hashSizes[i];
        }
    }
    return hashSizes[i - 1];
}


/*
 *  Grow the names vector
 */
int ejsGrowObjectNames(EjsObject *obj, int size)
{
    EjsNames        *names;
    EjsHashEntry    *entries;
    bool            ownNames;
    int             i, oldSize;

    if (size == 0) {
        return 0;
    }
    names = obj->names;
    
    ownNames = obj == mprGetParent(names);
    oldSize = (names) ? names->sizeEntries: 0;

    if (names == NULL || !ownNames) {
        names = mprAllocObj(obj, EjsNames);
        if (names == 0) {
            return EJS_ERR;
        }
        names->buckets = 0;
        names->entries = 0;
        names->sizeEntries = 0;
        names->sizeBuckets = 0;
    }

    if (size < names->sizeEntries) {
        return 0;
    }
    size = EJS_PROP_ROUNDUP(size);
    
    if (ownNames) {
        entries = (EjsHashEntry*) mprRealloc(names, names->entries, sizeof(EjsHashEntry) * size);
        if (entries == 0) {
            return EJS_ERR;
        }

    } else {
        entries = (EjsHashEntry*) mprAlloc(names, sizeof(EjsHashEntry) * size);
        if (entries == 0) {
            return EJS_ERR;
        }
        if (obj->names) {
            for (i = 0; i < oldSize; i++) {
                entries[i] = obj->names->entries[i];
            }
        }
    }
    for (i = oldSize; i < size; i++) {
        entries[i].nextSlot = -1;
        entries[i].qname.name = "";
        entries[i].qname.space = "";
    }
    names->sizeEntries = size;
    names->entries = entries;
    obj->names = names;
    return 0;
}


static int hashProperty(EjsObject *obj, int slotNum, EjsName *qname)
{
    EjsNames    *names;
    EjsName     *slotName;
    int         chainSlotNum, lastSlot, index;

    mprAssert(qname);

    names = obj->names;
    mprAssert(names);
  
    /*
     *  Test if the number of hash buckets is too small or non-existant and re-make the hash.
     */
    if (names->sizeBuckets < obj->numProp) {
        return makeHash(obj);
    }

    index = ejsComputeHashCode(names, qname);

    /*
     *  Scan the collision chain
     */
    lastSlot = -1;
    chainSlotNum = names->buckets[index];
    mprAssert(chainSlotNum < obj->numProp);
    mprAssert(chainSlotNum < obj->capacity);

    while (chainSlotNum >= 0) {
        slotName = &names->entries[chainSlotNum].qname;
        if (CMP_QNAME(slotName, qname)) {
            return 0;
        }
        mprAssert(lastSlot != chainSlotNum);
        lastSlot = chainSlotNum;
        mprAssert(chainSlotNum != names->entries[chainSlotNum].nextSlot);
        chainSlotNum = names->entries[chainSlotNum].nextSlot;

        mprAssert(0 <= lastSlot && lastSlot < obj->numProp);
        mprAssert(0 <= lastSlot && lastSlot < obj->capacity);
    }

    if (lastSlot >= 0) {
        mprAssert(lastSlot < obj->numProp);
        mprAssert(lastSlot != slotNum);
        names->entries[lastSlot].nextSlot = slotNum;

    } else {
        /* Start a new hash chain */
        names->buckets[index] = slotNum;
    }

    names->entries[slotNum].nextSlot = -2;
    names->entries[slotNum].qname = *qname;

#if BLD_DEBUG
    if (obj->slots[slotNum]) {
        cchar   *name;
        name = mprGetName(obj);
        if (name && *name) {
            ejsSetDebugName(obj->slots[slotNum], qname->name);
        }
    }
#endif
    return 0;
}


/*
 *  Allocate or grow the properties storage for an object. This routine will also manage the hash index for the object. 
 *  If numInstanceProp is < 0, then grow the number of properties by an increment. Otherwise, set the number of properties 
 *  to numInstanceProp. We currently don't allow reductions.
 */
static int makeHash(EjsObject *obj)
{
    EjsHashEntry    *entries;
    EjsNames        *names;
    int             i, newHashSize;

    mprAssert(obj);

    names = obj->names;

    /*
     *  Don't make the hash if too few properties. Once we have a hash, keep using it even if we have too few properties now.
     */
    if (names == 0 || (obj->numProp <= EJS_HASH_MIN_PROP && names->buckets == 0)) {
        return 0;
    }

    /*
     *  Only reallocate the hash buckets if the hash needs to grow larger
     */
    newHashSize = ejsGetHashSize(obj->numProp);
    if (names->sizeBuckets < newHashSize) {
        mprFree(names->buckets);
        names->buckets = (int*) mprAlloc(names, newHashSize * sizeof(int));
        if (names->buckets == 0) {
            return EJS_ERR;
        }
        names->sizeBuckets = newHashSize;
    }
    mprAssert(names->buckets);

    /*
     *  Clear out hash linkage
     */
    memset(names->buckets, -1, names->sizeBuckets * sizeof(int));
    entries = names->entries;
    for (i = 0; i < names->sizeEntries; i++) {
        entries[i].nextSlot = -1;
    }

    /*
     *  Rehash all existing properties
     */
    for (i = 0; i < obj->numProp; i++) {
        if (entries[i].qname.name && hashProperty(obj, i, &entries[i].qname) < 0) {
            return EJS_ERR;
        }
    }

    return 0;
}


void ejsResetHash(Ejs *ejs, EjsObject *obj)
{
    EjsHashEntry    *entries;
    EjsNames        *names;
    EjsHashEntry    *he;
    int             i;

    names = obj->names;
    entries = names->entries;

    /*
     *  Clear out hash linkage
     */
    memset(names->buckets, -1, names->sizeBuckets * sizeof(int));
    entries = names->entries;
    for (i = 0; i < names->sizeEntries; i++) {
        he = &names->entries[i];
        he->nextSlot = -1;
        he->qname.name = "";
        he->qname.space = "";
    }
}


static void removeHashEntry(EjsObject *obj, EjsName *qname)
{
    EjsNames        *names;
    EjsHashEntry    *he;
    EjsName         *nextName;
    int             index, slotNum, lastSlot;

    names = obj->names;
    if (names == 0) {
        return;
    }

    if (names->buckets == 0) {
        /*
         *  No hash. Just do a linear search
         */
        for (slotNum = 0; slotNum < obj->numProp; slotNum++) {
            he = &names->entries[slotNum];
            if (CMP_QNAME(&he->qname, qname)) {
                he->qname.name = "";
                he->qname.space = "";
                he->nextSlot = -1;
                return;
            }
        }
        mprAssert(0);
        return;
    }


    index = ejsComputeHashCode(names, qname);
    slotNum = names->buckets[index];
    lastSlot = -1;
    while (slotNum >= 0) {
        he = &names->entries[slotNum];
        nextName = &he->qname;
        if (CMP_QNAME(nextName, qname)) {
            if (lastSlot >= 0) {
                names->entries[lastSlot].nextSlot = names->entries[slotNum].nextSlot;
            } else {
                names->buckets[index] = names->entries[slotNum].nextSlot;
            }
            he->qname.name = "";
            he->qname.space = "";
            he->nextSlot = -1;
            return;
        }
        lastSlot = slotNum;
        slotNum = names->entries[slotNum].nextSlot;
    }
    mprAssert(0);
}


int ejsRebuildHash(Ejs *ejs, EjsObject *obj)
{
    return makeHash(obj);
}


/*
 *  Compute a property name hash. Based on work by Paul Hsieh.
 */
int ejsComputeHashCode(EjsNames *names, EjsName *qname)
{
    uchar   *cdata;
    uint    len, hash, rem, tmp;

    mprAssert(names);
    mprAssert(qname);
    mprAssert(qname->name);

    if ((len = (int) strlen(qname->name)) == 0) {
        return 0;
    }

    rem = len & 3;
    hash = len;

#if KEEP_FOR_UNICODE
    for (len >>= 2; len > 0; len--, data += 2) {
        hash  += *data;
        tmp   =  (data[1] << 11) ^ hash;
        hash  =  (hash << 16) ^ tmp;
        hash  += hash >> 11;
    }
#endif

    cdata = (uchar*) qname->name;
    for (len >>= 2; len > 0; len--, cdata += 4) {
        hash  += *cdata | (cdata[1] << 8);
        tmp   =  ((cdata[2] | (cdata[3] << 8)) << 11) ^ hash;
        hash  =  (hash << 16) ^ tmp;
        hash  += hash >> 11;
    }

    switch (rem) {
    case 3: 
        hash += cdata[0] + (cdata[1] << 8);
        hash ^= hash << 16;
        hash ^= cdata[sizeof(ushort)] << 18;
        hash += hash >> 11;
        break;
    case 2: 
        hash += cdata[0] + (cdata[1] << 8);
        hash ^= hash << 11;
        hash += hash >> 17;
        break;
    case 1: hash += cdata[0];
        hash ^= hash << 10;
        hash += hash >> 1;
    }

    /* 
     *  Force "avalanching" of final 127 bits 
     */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    mprAssert(names->sizeBuckets);
    
    return hash % names->sizeBuckets;
}


static inline int cmpQname(EjsName *a, EjsName *b) 
{
    mprAssert(a);
    mprAssert(b);
    mprAssert(a->name);
    mprAssert(a->space);
    mprAssert(b->name);
    mprAssert(b->space);

    if (a->name == b->name && a->space == b->space) {
        return 1;
    }
    if (a->name[0] == b->name[0] && strcmp(a->name, b->name) == 0) {
        if (a->space[0] == b->space[0] && strcmp(a->space, b->space) == 0) {
            return 1;
        }
    }
    return 0;
}


#if KEEP
static inline int cmpName(EjsName *a, EjsName *b) 
{
    mprAssert(a);
    mprAssert(b);
    mprAssert(a->name);
    mprAssert(b->name);

    if (a->name == b->name) {
        return 1;
    }
    if (a->name[0] == b->name[0] && strcmp(a->name, b->name) == 0) {
        return 1;
    }
    return 0;
}
#endif

/*
 *  WARNING: All methods here may be invoked by Native classes that are based on EjsVar and not on EjsObject. Because 
 *  all classes subclass Object, they need to be able to use these methods. They MUST NOT use EjsObject internals.
 */

static EjsVar *cloneObjectMethod(Ejs *ejs, EjsVar *op, int argc, EjsVar **argv)
{
    bool    deep;

    deep = (argc == 1 && argv[0] == (EjsVar*) ejs->trueValue);

    return ejsCloneVar(ejs, op, deep);
}


/*
 *  Function to iterate and return the next element name.
 *  NOTE: this is not a method of Object. Rather, it is a callback function for Iterator.
 */
static EjsVar *nextObjectKey(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsObject   *obj;
    EjsName     qname;

    obj = (EjsObject*) ip->target;
    if (!ejsIsObject(obj)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    for (; ip->index < obj->numProp; ip->index++) {
        qname = ejsGetPropertyName(ejs, (EjsVar*) obj, ip->index);
        if (qname.name == 0) {
            continue;
        }
        /*
         *  Enumerate over properties that have a public public or empty namespace 
         */
        if (qname.space[0] && strcmp(qname.space, EJS_PUBLIC_NAMESPACE) != 0) {
            continue;
        } else if (qname.name[0] == '\0') {
            continue;
        }
        ip->index++;
        return (EjsVar*) ejsCreateString(ejs, qname.name);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return the default iterator.
 *
 *  iterator native function get(deep: Boolean = false, namespaces: Array = null): Iterator
 */
static EjsVar *getObjectIterator(Ejs *ejs, EjsVar *op, int argc, EjsVar **argv)
{
    EjsVar      *namespaces;
    bool        deep;

    deep = (argc == 1) ? ejsGetBoolean(argv[0]): 0;
    namespaces =  (argc == 2) ? argv[1]: 0;

    return (EjsVar*) ejsCreateIterator(ejs, op, (EjsNativeFunction) nextObjectKey, deep, (EjsArray*) namespaces);
}


/*
 *  Function to iterate and return the next element value.
 *  NOTE: this is not a method of Object. Rather, it is a callback function for Iterator
 */
static EjsVar *nextObjectValue(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsObject   *obj;
    EjsVar      *vp;
    EjsName     qname;

    obj = (EjsObject*) ip->target;
    if (!ejsIsObject(obj)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    for (; ip->index < obj->numProp; ip->index++) {
        qname = ejsGetPropertyName(ejs, (EjsVar*) obj, ip->index);
        if (qname.name == 0) {
            continue;
        }
        if (qname.space[0] && strcmp(qname.space, EJS_PUBLIC_NAMESPACE) != 0) {
            continue;
        } else if (qname.name[0] == '\0') {
            continue;
        }
        vp = obj->slots[ip->index];
        if (vp) {
            ip->index++;
            return vp;
        }
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return an iterator to return the next array element value.
 *
 *  iterator native function getValues(deep: Boolean = false, namespaces: Array = null): Iterator
 */
static EjsVar *getObjectValues(Ejs *ejs, EjsVar *op, int argc, EjsVar **argv)
{
    EjsVar      *namespaces;
    bool        deep;

    deep = (argc == 1) ? ejsGetBoolean(argv[0]): 0;
    namespaces =  (argc == 2) ? argv[1]: 0;

    return (EjsVar*) ejsCreateIterator(ejs, op, (EjsNativeFunction) nextObjectValue, deep, (EjsArray*) namespaces);
}


/*
 *  Get the length for the object.
 *
 *  intrinsic function get length(): Number
 */
static EjsVar *getObjectLength(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ejsGetPropertyCount(ejs, vp));
}


#if ES_Object_seal
/**
 *  Seal a dynamic object. Once an object is sealed, further attempts to create or delete properties will fail and will throw
 *  @spec ejs-11
 */
static EjsVar *seal(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    vp->sealed = 1;
    return 0;
}
#endif


/*
 *  Convert the object to a JSON string. This also handles Json for Arrays.
 *
 *  intrinsic function toJSON(): String
 */
static EjsVar *objectToJson(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    MprBuf      *buf;
    EjsVar      *pp, *result;
    EjsBlock    *block;
    EjsName     qname;
    EjsObject   *obj;
    EjsString   *sv;
    char        key[16], *cp;
    int         c, isArray, i, count, slotNum, numInherited, maxDepth, flags, showAll, showBase;

    count = ejsGetPropertyCount(ejs, (EjsVar*) vp);
    if (count == 0 && vp->type != ejs->objectType && vp->type != ejs->arrayType) {
        return (EjsVar*) ejsToString(ejs, vp);
    }

    maxDepth = 99;
    flags = 0;
    showAll = 0;
    showBase = 0;

    isArray = ejsIsArray(vp);
    obj = (EjsObject*) vp;
    buf = mprCreateBuf(ejs, 0, 0);
    mprPutStringToBuf(buf, isArray ? "[\n" : "{\n");

    if (++ejs->serializeDepth <= maxDepth) {

        for (slotNum = 0; slotNum < count && !ejs->exception; slotNum++) {
            if (ejsIsBlock(obj)) {
                block = (EjsBlock*) obj;
                numInherited = ejsGetNumInheritedTraits(block);
                if (slotNum < numInherited && !(flags & EJS_FLAGS_ENUM_INHERITED)) {
                    continue;
                }
            }
            pp = ejsGetProperty(ejs, (EjsVar*) obj, slotNum);
            if (ejs->exception) {
                return 0;
            }
            if (pp == 0 || (pp->hidden && !(flags & EJS_FLAGS_ENUM_ALL))) {
                continue;
            }
            if (ejsIsFunction(pp) && !(flags & EJS_FLAGS_ENUM_ALL)) {
                continue;
            }
            if (isArray) {
                mprItoa(key, sizeof(key), slotNum, 10);
                qname.name = key;
                qname.space = "";
            } else {
                qname = ejsGetPropertyName(ejs, vp, slotNum);
            }

            if (qname.space && strstr(qname.space, ",private") != 0) {
                continue;
            }
            if (qname.space[0] == '\0' && qname.name[0] == '\0') {
                continue;
            }
            for (i = 0; i < ejs->serializeDepth; i++) {
                mprPutStringToBuf(buf, "  ");
            }
            if (!isArray) {
                mprPutCharToBuf(buf, '"');
                for (cp = (char*) qname.name; cp && *cp; cp++) {
                    c = *cp;
                    if (c == '"' || c == '\\') {
                        mprPutCharToBuf(buf, '\\');
                        mprPutCharToBuf(buf, c);
                    } else {
                        mprPutCharToBuf(buf, c);
                    }
                }
                mprPutStringToBuf(buf, "\": ");
            }
            sv = (EjsString*) ejsToJson(ejs, pp);
            if (sv == 0 || !ejsIsString(sv)) {
                if (!ejs->exception) {
                    ejsThrowTypeError(ejs, "Can't serialize property %s", qname.name);
                }
                return 0;
            } else {
                mprPutStringToBuf(buf, sv->value);
            }
            if ((slotNum + 1) < count) {
                mprPutCharToBuf(buf, ',');
            }
            mprPutStringToBuf(buf, "\n");
        }
    }
    for (i = --ejs->serializeDepth; i > 0; i--) {
        mprPutStringToBuf(buf, "  ");
    }
    mprPutCharToBuf(buf, isArray ? ']' : '}');

    mprAddNullToBuf(buf);
    result = (EjsVar*) ejsCreateString(ejs, mprGetBufStart(buf));
    mprFree(buf);
    return result;
}


#if ES_Object_toLocaleString
/*
 *  Convert the object to a localized string.
 *
 *  intrinsic function toLocaleString(): String
 */
static EjsVar *toLocaleString(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    return objectToString(ejs, vp, argc, argv);
}
#endif


/*
 *  Convert the object to a string.
 *
 *  intrinsic function toString(): String
 */
static EjsVar *objectToString(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    if (ejsIsString(vp)) {
        return vp;
    }
    return (EjsVar*) ejsCastVar(ejs, vp, ejs->stringType);
}


static void patchObjectSlots(Ejs *ejs, EjsObject *obj)
{
    EjsType     *type, *ot;
    EjsVar      *vp;
    EjsFunction *fun, *existingFun;
    int         i, j;

    ot = ejs->objectType;
    if (ejsIsType(obj)) {
        type = (EjsType*) obj;
        if (type != ot && ejsIsType(type) && !type->isInterface && type->objectBased && !ejsIsInstanceBlock(type)) {
            for (j = 0; j < ot->block.obj.numProp; j++) {
                fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) ot, j);
                if (ejsIsNativeFunction(fun)) {
                    existingFun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, j);
                    if (!ejsIsFunction(existingFun) || !existingFun->override) {
                        ejsSetProperty(ejs, (EjsVar*) type, j, (EjsVar*) fun);
                    }
                }
            }
        }
        if (type->instanceBlock) {
            patchObjectSlots(ejs, (EjsObject*) type->instanceBlock);
        }
    }
    for (i = 0; i < obj->numProp; i++) {
        if (ejsIsObject(obj)) {
            vp = obj->slots[i];
            if (vp == 0) {
                obj->slots[i] = ejs->nullValue;
            }
        }
    }
}


/*
 *  Create the object type
 */
void ejsCreateObjectType(Ejs *ejs)
{
    EjsName     qname;

    ejs->objectType = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Object"), 0, sizeof(EjsObject), 
        ES_Object, ES_Object_NUM_CLASS_PROP, ES_Object_NUM_INSTANCE_PROP, 
        EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_DYNAMIC_INSTANCE | EJS_ATTR_OBJECT_HELPERS);
}


void ejsConfigureObjectType(Ejs *ejs)
{
    EjsType     *ot;
    EjsObject   *obj;
    int         count, i;

    ot = ejs->objectType;
    mprAssert(ot);

    ejsBindMethod(ejs, ot, ES_Object_clone, cloneObjectMethod);
    ejsBindMethod(ejs, ot, ES_Object_get, getObjectIterator);
    ejsBindMethod(ejs, ot, ES_Object_getValues, getObjectValues);
    ejsBindMethod(ejs, ot, ES_Object_length, getObjectLength);
    ejsBindMethod(ejs, ot, ES_Object_toString, objectToString);
    ejsBindMethod(ejs, ot, ES_Object_toJSON, objectToJson);

#if ES_Object_seal
    ejsBindMethod(ejs, ot, ES_Object_seal, seal);
#endif
#if ES_Object_toLocaleString
    ejsBindMethod(ejs, ot, ES_Object_toLocaleString, toLocaleString);
#endif

    /*
     *  Patch native methods into all objects inheriting from object
     */
    patchObjectSlots(ejs, (EjsObject*) ejs->objectType);
    count = ejsGetPropertyCount(ejs, ejs->global);
    for (i = 0; i < count; i++) {
        obj = (EjsObject*) ejsGetProperty(ejs, ejs->global, i);
        if (ejsIsObject(obj)) {
            patchObjectSlots(ejs, obj);
        }
    }
}


void ejsInitializeObjectHelpers(EjsTypeHelpers *helpers)
{
    /*
     *  call is not implemented, EjsObject does not override and it is handled by the vm.
     */
    helpers->castVar                = (EjsCastVarHelper) castObject;
    helpers->cloneVar               = (EjsCloneVarHelper) ejsCopyObject;
    helpers->createVar              = (EjsCreateVarHelper) ejsCreateObject;
    helpers->defineProperty         = (EjsDefinePropertyHelper) defineObjectProperty;
    helpers->deleteProperty         = (EjsDeletePropertyHelper) deleteObjectProperty;
    helpers->deletePropertyByName   = (EjsDeletePropertyByNameHelper) deleteObjectPropertyByName;
    helpers->getProperty            = (EjsGetPropertyHelper) getObjectProperty;
    helpers->getPropertyCount       = (EjsGetPropertyCountHelper) getObjectPropertyCount;
    helpers->getPropertyName        = (EjsGetPropertyNameHelper) getObjectPropertyName;
    helpers->lookupProperty         = (EjsLookupPropertyHelper) lookupObjectProperty;
    helpers->invokeOperator         = (EjsInvokeOperatorHelper) ejsObjectOperator;
    helpers->markVar                = (EjsMarkVarHelper) ejsMarkObject;
    helpers->setProperty            = (EjsSetPropertyHelper) setObjectProperty;
    helpers->setPropertyName        = (EjsSetPropertyNameHelper) setObjectPropertyName;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsObject.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsReflect.c"
 */
/************************************************************************/

/**
 *  ejsReflect.c - Reflection class and API
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ES_Reflect
/*
 *  Constructor
 *
 *  public function Reflect(o: Object)
 */
static EjsVar *reflectConstructor(Ejs *ejs, EjsReflect *rp, int argc,  EjsVar **argv)
{
    mprAssert(argc == 1);
    rp->subject = argv[0];
    return (EjsVar*) rp;
}


/*
 *  Get the name of the subject
 *
 *  function get name(): String
 */
static EjsVar *getReflectedName(Ejs *ejs, EjsReflect *rp, int argc, EjsVar **argv)
{
    EjsFunction     *fun;
    EjsName         qname;
    EjsVar      *vp;

    mprAssert(argc == 0);

    vp = rp->subject;

    if (ejsIsType(vp)) {
        return (EjsVar*) ejsCreateString(ejs, ((EjsType*) vp)->qname.name);

    } else if (ejsIsFunction(vp)) {
        fun = (EjsFunction*) vp;
        qname = ejsGetPropertyName(ejs, fun->owner, fun->slotNum);
        return (EjsVar*) ejsCreateString(ejs, qname.name);

    } else {
        return (EjsVar*) ejsCreateString(ejs, vp->type->qname.name);
    }
    return ejs->undefinedValue;
}


/*
 *  Get the type of the object.
 *
 *  function get type(): Object
 */
static EjsVar *getReflectedType(Ejs *ejs, EjsReflect *rp, int argc, EjsVar **argv)
{
    EjsType     *type;
    EjsVar      *vp;

    vp = rp->subject;

    if (ejsIsType(vp)) {
        type = (EjsType*) vp;
        if (type->baseType) {
            return (EjsVar*) type->baseType;
        } else {
            return (EjsVar*) ejs->undefinedValue;
        }
    }
    return (EjsVar*) vp->type;
}


/*
 *  Get the type name of the subject
 *
 *  function get typeName(): String
 */
static EjsVar *getReflectedTypeName(Ejs *ejs, EjsReflect *rp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsGetTypeName(ejs, rp->subject);
}


/*
 *  Return the type name of a var as a string. If the var is a type, get the base type.
 */
EjsVar *ejsGetTypeName(Ejs *ejs, EjsVar *vp)
{
    EjsType     *type;

    if (vp == 0) {
        return ejs->undefinedValue;
    }
    if (ejsIsType(vp)) {
        type = (EjsType*) vp;
        if (type->baseType) {
            return (EjsVar*) ejsCreateString(ejs, type->baseType->qname.name);
        } else {
            /* NOTE: the base type of Object is null */
            return (EjsVar*) ejs->nullValue;
        }
    }
    return (EjsVar*) ejsCreateString(ejs, vp->type->qname.name);
}


/*
 *  Get the ecma "typeof" value for an object. Unfortunately, typeof is pretty lame.
 */
EjsVar *ejsGetTypeOf(Ejs *ejs, EjsVar *vp)
{
    if (vp == ejs->undefinedValue) {
        return (EjsVar*) ejsCreateString(ejs, "undefined");

    } else if (vp == ejs->nullValue) {
        /* Yea - I know, ECMAScript is broken */
        return (EjsVar*) ejsCreateString(ejs, "object");

    } if (ejsIsBoolean(vp)) {
        return (EjsVar*) ejsCreateString(ejs, "boolean");

    } else if (ejsIsNumber(vp)) {
        return (EjsVar*) ejsCreateString(ejs, "number");

    } else if (ejsIsString(vp)) {
        return (EjsVar*) ejsCreateString(ejs, "string");

    } else if (ejsIsFunction(vp)) {
        return (EjsVar*) ejsCreateString(ejs, "function");
               
    } else if (ejsIsType(vp)) {
        /* Pretend it is a constructor function */
        return (EjsVar*) ejsCreateString(ejs, "function");
               
    } else {
        return (EjsVar*) ejsCreateString(ejs, "object");
    }
}


#if ES_makeGetter
EjsVar *ejsMakeGetter(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsFunction     *fun;

    mprAssert(argc == 1);
    fun = (EjsFunction*) argv[0];

    if (fun == 0 || !ejsIsFunction(fun)) {
        ejsThrowArgError(ejs, "Argument is not a function");
        return 0;
    }
    fun->getter = 1;
    ((EjsVar*) fun->owner)->hasGetterSetter = 1;
    return (EjsVar*) fun;
}
#endif


#if ES_makeSetter
EjsVar *ejsMakeSetter(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsFunction     *fun;

    mprAssert(argc == 1);
    fun = (EjsFunction*) argv[0];

    if (fun == 0 || !ejsIsFunction(fun)) {
        ejsThrowArgError(ejs, "Argument is not a function");
        return 0;
    }
    fun->getter = 1;
    ((EjsObject*) fun->owner)->hasGetterSetter;
    return (EjsVar*) fun;
}
#endif


#if ES_clearBoundThis
EjsVar *ejsClearBoundThis(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsFunction     *fun;

    mprAssert(argc == 1);
    fun = (EjsFunction*) argv[0];

    if (fun == 0 || !ejsIsFunction(fun)) {
        ejsThrowArgError(ejs, "Argument is not a function");
        return 0;
    }
    fun->thisObj = 0;
    return (EjsVar*) fun;
}
#endif


static void markReflectVar(Ejs *ejs, EjsVar *parent, EjsReflect *rp)
{
    if (rp->subject) {
        ejsMarkVar(ejs, parent, rp->subject);
    }
}


void ejsCreateReflectType(Ejs *ejs)
{
    EjsName     qname;

    ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Reflect"), ejs->objectType, sizeof(EjsReflect),
        ES_Reflect, ES_Reflect_NUM_CLASS_PROP, ES_Reflect_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
}


void ejsConfigureReflectType(Ejs *ejs)
{
    EjsType     *type;

    type = ejsGetType(ejs, ES_Reflect);
    type->helpers->markVar = (EjsMarkVarHelper) markReflectVar;

    ejsBindMethod(ejs, type, type->block.numInherited, (EjsNativeFunction) reflectConstructor);
    ejsBindMethod(ejs, type, ES_Reflect_name, (EjsNativeFunction) getReflectedName);
    ejsBindMethod(ejs, type, ES_Reflect_type, (EjsNativeFunction) getReflectedType);
    ejsBindMethod(ejs, type, ES_Reflect_typeName, (EjsNativeFunction) getReflectedTypeName);

#if ES_makeGetter
    ejsBindFunction(ejs, ejs->globalBlock, ES_makeGetter, (EjsNativeFunction) ejsMakeGetter);
#endif
#if ES_makeSetter
    ejsBindFunction(ejs, ejs->globalBlock, ES_makeSetter, (EjsNativeFunction) ejsMakeSetter);
#endif
#if ES_clearBoundThis
    ejsBindFunction(ejs, ejs->globalBlock, ES_clearBoundThis, (EjsNativeFunction) ejsClearBoundThis);
#endif
}
#endif /* ES_Reflect */


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsReflect.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsRegExp.c"
 */
/************************************************************************/

/**
 *  ejsRegExp.c - RegExp type class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_REGEXP && ES_RegExp


static int parseFlags(EjsRegExp *rp, cchar *flags);
static char *makeFlags(EjsRegExp *rp);

/*
 *  Cast the operand to the specified type
 *
 *  intrinsic function cast(type: Type) : Object
 */

static EjsVar *castRegExp(Ejs *ejs, EjsRegExp *rp, EjsType *type)
{
    char    *pattern, *flags;

    switch (type->id) {

    case ES_Boolean:
        return (EjsVar*) ejs->trueValue;

    case ES_String:
        flags = makeFlags(rp);
        pattern = mprStrcat(rp, -1, "/", rp->pattern, "/", flags, NULL);
        mprFree(flags);
        return (EjsVar*) ejsCreateStringAndFree(ejs, pattern);

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
    return 0;
}


static void destroyRegExp(Ejs *ejs, EjsRegExp *rp)
{
    mprAssert(rp);

    if (rp->compiled) {
        free(rp->compiled);
        rp->compiled = 0;
    }
    ejsFreeVar(ejs, (EjsVar*) rp, -1);
}


/*
 *  RegExp constructor
 *
 *  RegExp(pattern: String, flags: String = null)
 */

static EjsVar *regexConstructor(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    cchar       *errMsg, *flags, *pattern;
    int         column, errCode;

    pattern = ejsGetString(argv[0]);
    rp->options = PCRE_JAVASCRIPT_COMPAT;

    if (argc == 2) {
        flags = ejsGetString(argv[1]);
        rp->options |= parseFlags(rp, flags);
    }
    rp->pattern = mprStrdup(rp, pattern);
    if (rp->compiled) {
        free(rp->compiled);
    }
    rp->compiled = (void*) pcre_compile2(rp->pattern, rp->options, &errCode, &errMsg, &column, NULL);

    if (rp->compiled == NULL) {
        ejsThrowArgError(ejs, "Can't compile regular expression. Error %s at column %d", errMsg, column);
    }
    return (EjsVar*) rp;
}


static EjsVar *getLastIndex(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, rp->endLastMatch);
}


/*
 *  function set lastIndex(value: Number): Void
 */
static EjsVar *setLastIndex(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    rp->endLastMatch = (int) ejsGetNumber(argv[0]);
    return 0;
}


/*
 *  function exec(str: String, start: Number = 0): Array
 */
static EjsVar *exec(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    EjsArray    *results;
    EjsString   *match;
    cchar       *str;
    int         matches[EJS_MAX_REGEX_MATCHES * 3];
    int         start, len, i, count, index;

    str = ejsGetString(argv[0]);
    if (argc == 2) {
        start = (int) ejsGetNumber(argv[1]);
    } else {
        start = rp->endLastMatch;
    }
    rp->matched = 0;

    count = pcre_exec(rp->compiled, NULL, str, (int) strlen(str), start, 0, matches, sizeof(matches) / sizeof(int));
    if (count < 0) {
        rp->endLastMatch = 0;
        return (EjsVar*) ejs->nullValue;
    }

    results = ejsCreateArray(ejs, count);
    for (index = 0, i = 0; i < count; i++, index += 2) {
        len = matches[index + 1] - matches[index];
        match = ejsCreateStringWithLength(ejs, &str[matches[index]], len);
        ejsSetProperty(ejs, (EjsVar*) results, i, (EjsVar*) match);
        if (index == 0) {
            rp->matched = match;
        }
    }
    if (rp->global) {
        rp->startLastMatch = matches[0];
        rp->endLastMatch = matches[1];
    }
    return (EjsVar*) results;
}


static EjsVar *getGlobalFlag(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, rp->global);
}


static EjsVar *getIgnoreCase(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, rp->ignoreCase);
}


static EjsVar *getMultiline(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, rp->multiline);
}


static EjsVar *getSource(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, rp->pattern);
}


static EjsVar *matched(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    if (rp->matched == 0) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) rp->matched;
}


static EjsVar *start(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, rp->startLastMatch);
}


static EjsVar *sticky(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, rp->sticky);
}


static EjsVar *test(Ejs *ejs, EjsRegExp *rp, int argc, EjsVar **argv)
{
    cchar       *str;
    int         count;

    str = ejsGetString(argv[0]);
    count = pcre_exec(rp->compiled, NULL, str, (int) strlen(str), rp->endLastMatch, 0, 0, 0);
    if (count < 0) {
        rp->endLastMatch = 0;
        return (EjsVar*) ejs->falseValue;
    }
    return (EjsVar*) ejs->trueValue;
}


EjsVar *ejsRegExpToString(Ejs *ejs, EjsRegExp *rp)
{
    return castRegExp(ejs, rp, ejs->stringType);
}

/*
 *  Create an initialized regular expression object. The pattern should include
 *  the slash delimiters. For example: /abc/ or /abc/g
 */

EjsRegExp *ejsCreateRegExp(Ejs *ejs, cchar *pattern)
{
    EjsRegExp   *rp;
    cchar       *errMsg;
    char        *flags;
    int         column, errCode;

    mprAssert(pattern[0] == '/');
    
    if (*pattern != '/') {
        ejsThrowArgError(ejs, "Bad regular expression pattern. Must start with '/'");
        return 0;
    }
    rp = (EjsRegExp*) ejsCreateVar(ejs, ejs->regExpType, 0);
    if (rp != 0) {
        /*
            Strip off flags for passing to pcre_compile2
         */
        rp->pattern = mprStrdup(rp, &pattern[1]);
        if ((flags = strrchr(rp->pattern, '/')) != 0) {
            rp->options = parseFlags(rp, &flags[1]);
            *flags = '\0';
        }
        if (rp->compiled) {
            free(rp->compiled);
        }
        rp->compiled = pcre_compile2(rp->pattern, rp->options, &errCode, &errMsg, &column, NULL);
        if (rp->compiled == NULL) {
            ejsThrowArgError(ejs, "Can't compile regular expression. Error %s at column %d", errMsg, column);
            return 0;
        }
    }
    return rp;
}


static int parseFlags(EjsRegExp *rp, cchar *flags)
{
    cchar       *cp;
    int         options;

    if (flags == 0 || *flags == '\0') {
        return 0;
    }

    options = PCRE_JAVASCRIPT_COMPAT;
    for (cp = flags; *cp; cp++) {
        switch (tolower((int) *cp)) {
        case 'g':
            rp->global = 1;
            break;
        case 'i':
            rp->ignoreCase = 1;
            options |= PCRE_CASELESS;
            break;
        case 'm':
            rp->multiline = 1;
            options |= PCRE_MULTILINE;
            break;
        case 's':
            options |= PCRE_DOTALL;
            break;
        case 'y':
            rp->sticky = 1;
            break;
        case 'x':
            options |= PCRE_EXTENDED;
            break;
        case 'X':
            options |= PCRE_EXTRA;
            break;
        case 'U':
            options |= PCRE_UNGREEDY;
            break;
        }
    }
    return options;
}


static char *makeFlags(EjsRegExp *rp)
{
    char    buf[16], *cp;

    cp = buf;
    if (rp->global) {
        *cp++ = 'g';
    }
    if (rp->ignoreCase) {
        *cp++ = 'i';
    }
    if (rp->multiline) {
        *cp++ = 'm';
    }
    if (rp->sticky) {
        *cp++ = 'y';
    }
    if (rp->options & PCRE_DOTALL) {
        *cp++ = 's';
    }
    if (rp->options & PCRE_EXTENDED) {
        *cp++ = 'x';
    }
    if (rp->options & PCRE_EXTRA) {
        *cp++ = 'X';
    }
    if (rp->options & PCRE_UNGREEDY) {
        *cp++ = 'U';
    }
    *cp++ = '\0';
    return mprStrdup(rp, buf);
}


void ejsCreateRegExpType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "RegExp"), ejs->objectType, sizeof(EjsRegExp),
        ES_RegExp, ES_RegExp_NUM_CLASS_PROP, ES_RegExp_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
    ejs->regExpType = type;
    type->needFinalize = 1;

    /*
     *  Define the helper functions.
     */
    type->helpers->castVar = (EjsCastVarHelper) castRegExp;
    type->helpers->destroyVar = (EjsDestroyVarHelper) destroyRegExp;
}


void ejsConfigureRegExpType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->regExpType;

    ejsBindMethod(ejs, type, ES_RegExp_RegExp, (EjsNativeFunction) regexConstructor);
    ejsBindMethod(ejs, type, ES_RegExp_exec, (EjsNativeFunction) exec);
#if KEEP
    ejsSetAccessors(ejs, type, ES_RegExp_lastIndex, (EjsNativeFunction) getLastIndex, ES_RegExp_set_lastIndex, 
        (EjsNativeFunction) setLastIndex);
    ejsSetAccessors(ejs, type, ES_RegExp_global, (EjsNativeFunction) getGlobalFlag, -1, 0);
    ejsSetAccessors(ejs, type, ES_RegExp_ignoreCase, (EjsNativeFunction) getIgnoreCase, -1, 0);
    ejsSetAccessors(ejs, type, ES_RegExp_multiline, (EjsNativeFunction) getMultiline, -1, 0);
    ejsSetAccessors(ejs, type, ES_RegExp_source, (EjsNativeFunction) getSource, -1, 0);
    ejsSetAccessors(ejs, type, ES_RegExp_matched, (EjsNativeFunction) matched, -1, 0);
    ejsSetAccessors(ejs, type, ES_RegExp_start, (EjsNativeFunction) start, -1, 0);
    ejsSetAccessors(ejs, type, ES_RegExp_sticky, (EjsNativeFunction) sticky, -1, 0);
#else
    ejsBindMethod(ejs, type, ES_RegExp_lastIndex, (EjsNativeFunction) getLastIndex);
    ejsBindMethod(ejs, type, ES_RegExp_set_lastIndex, (EjsNativeFunction) setLastIndex);
    ejsBindMethod(ejs, type, ES_RegExp_global, (EjsNativeFunction) getGlobalFlag);
    ejsBindMethod(ejs, type, ES_RegExp_ignoreCase, (EjsNativeFunction) getIgnoreCase);
    ejsBindMethod(ejs, type, ES_RegExp_multiline, (EjsNativeFunction) getMultiline);
    ejsBindMethod(ejs, type, ES_RegExp_source, (EjsNativeFunction) getSource);
    ejsBindMethod(ejs, type, ES_RegExp_matched, (EjsNativeFunction) matched);
    ejsBindMethod(ejs, type, ES_RegExp_start, (EjsNativeFunction) start);
    ejsBindMethod(ejs, type, ES_RegExp_sticky, (EjsNativeFunction) sticky);
#endif
    ejsBindMethod(ejs, type, ES_RegExp_test, (EjsNativeFunction) test);
    ejsBindMethod(ejs, type, ES_Object_toString, (EjsNativeFunction) ejsRegExpToString);
}

#else
void __dummyRegExp() {}
#endif /* BLD_FEATURE_REGEXP */


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsRegExp.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsString.c"
 */
/************************************************************************/

/**
 *  ejsString.c - Ejscript string class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int catString(Ejs *ejs, EjsString *dest, char *str, int len);
static int indexof(cchar *str, int len, cchar *pattern, int patlen, int dir);

/*
 *  Cast the string operand to a primitive type
 */

static EjsVar *castString(Ejs *ejs, EjsString *sp, EjsType *type)
{
    mprAssert(sp);
    mprAssert(type);

    switch (type->id) {
    case ES_Boolean:
        if (sp->value[0]) {
            return (EjsVar*) ejs->trueValue;
        } else {
            return (EjsVar*) ejs->falseValue;
        }

    case ES_Number:
        return (EjsVar*) ejsParseVar(ejs, sp->value, ES_Number);

#if ES_RegExp && BLD_FEATURE_REGEXP
    case ES_RegExp:
        if (sp->value && sp->value[0] == '/') {
            return (EjsVar*) ejsCreateRegExp(ejs, sp->value);
        } else {
            EjsVar      *result;
            char        *buf;
            buf = mprStrcat(ejs, -1, "/", sp->value, "/", NULL);
            result = (EjsVar*) ejsCreateRegExp(ejs, buf);
            mprFree(buf);
            return result;
        }
#endif

    case ES_ejs_io_Path:
        return (EjsVar*) ejsCreatePath(ejs, sp->value);

    case ES_String:
        return (EjsVar*) sp;

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
    return 0;
}


/*
 *  Clone a string. Shallow copies simply return a reference as strings are immutable.
 */
static EjsString *cloneString(Ejs *ejs, EjsString *sp, bool deep)
{
    if (deep) {
        return ejsCreateStringWithLength(ejs, sp->value, sp->length);
    }
    return sp;
}


static void destroyString(Ejs *ejs, EjsString *sp)
{
    mprAssert(sp);

    mprFree(sp->value);
    sp->value = 0;
    ejsFreeVar(ejs, (EjsVar*) sp, -1);
}


/*
 *  Get a string element. Slot numbers correspond to character indicies.
 */
static EjsVar *getStringProperty(Ejs *ejs, EjsString *sp, int index)
{
    if (index < 0 || index >= sp->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad string subscript");
        return 0;
    }
    return (EjsVar*) ejsCreateStringWithLength(ejs, &sp->value[index], 1);
}


static EjsVar *coerceStringOperands(Ejs *ejs, EjsVar *lhs, int opcode,  EjsVar *rhs)
{
    switch (opcode) {
    /*
     *  Binary operators
     */
    case EJS_OP_ADD:
        return ejsInvokeOperator(ejs, lhs, opcode, (EjsVar*) ejsToString(ejs, rhs));

    /*
     *  Overloaded operators
     */
    case EJS_OP_MUL:
        if (ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);
        }
        return ejsInvokeOperator(ejs, lhs, opcode, (EjsVar*) ejsToNumber(ejs, rhs));

    case EJS_OP_REM:
        return 0;

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_OR:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_NE:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        return ejsInvokeOperator(ejs, lhs, opcode, (EjsVar*) ejsToString(ejs, rhs));

    case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToBoolean(ejs, lhs), opcode, rhs);

    case EJS_OP_NOT:
    case EJS_OP_NEG:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, lhs), opcode, rhs);

    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) (((EjsString*) lhs)->value ? ejs->trueValue : ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
    case EJS_OP_COMPARE_FALSE:
        return (EjsVar*) (((EjsString*) lhs)->value ? ejs->falseValue: ejs->trueValue);

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->falseValue;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }
    return 0;
}


static EjsVar *invokeStringOperator(Ejs *ejs, EjsString *lhs, int opcode,  EjsString *rhs, void *data)
{
    EjsVar      *result;
#if BLD_FEATURE_EJS_LANG >= EJS_SPEC_PLUS
    EjsVar      *arg;
#endif

    if (rhs == 0 || lhs->obj.var.type != rhs->obj.var.type) {
        if (!ejsIsA(ejs, (EjsVar*) lhs, ejs->stringType) || !ejsIsA(ejs, (EjsVar*) rhs, ejs->stringType)) {
            if ((result = coerceStringOperands(ejs, (EjsVar*) lhs, opcode, (EjsVar*) rhs)) != 0) {
                return result;
            }
        }
    }
    /*
     *  Types now match, both strings
     */
    switch (opcode) {
    case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_EQ:
        if (lhs == rhs || (lhs->value == rhs->value)) {
            return (EjsVar*) ejs->trueValue;
        }
        return (EjsVar*) ejsCreateBoolean(ejs,  mprMemcmp(lhs->value, lhs->length, rhs->value, rhs->length) == 0);

    case EJS_OP_COMPARE_NE:
    case EJS_OP_COMPARE_STRICTLY_NE:
        if (lhs->length != rhs->length) {
            return (EjsVar*) ejs->trueValue;
        }
        return (EjsVar*) ejsCreateBoolean(ejs,  mprMemcmp(lhs->value, lhs->length, rhs->value, rhs->length) != 0);

    case EJS_OP_COMPARE_LT:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprMemcmp(lhs->value, lhs->length, rhs->value, rhs->length) < 0);

    case EJS_OP_COMPARE_LE:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprMemcmp(lhs->value, lhs->length, rhs->value, rhs->length) <= 0);

    case EJS_OP_COMPARE_GT:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprMemcmp(lhs->value, lhs->length, rhs->value, rhs->length) > 0);

    case EJS_OP_COMPARE_GE:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprMemcmp(lhs->value, lhs->length, rhs->value, rhs->length) >= 0);

    /*
     *  Unary operators
     */
    case EJS_OP_COMPARE_NOT_ZERO:
        return (EjsVar*) ((lhs->value) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ((lhs->value == 0) ? ejs->trueValue: ejs->falseValue);


    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Binary operators
     */
    case EJS_OP_ADD:
        result = (EjsVar*) ejsCreateString(ejs, lhs->value);
        ejsStrcat(ejs, (EjsString*) result, (EjsVar*) rhs);
        return result;

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_OR:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsToNumber(ejs, (EjsVar*) lhs), opcode, (EjsVar*) rhs);

#if BLD_FEATURE_EJS_LANG >= EJS_SPEC_PLUS
    /*
     *  Overloaded
     */
    case EJS_OP_SUB:
        arg = (EjsVar*) rhs;
        return ejsRunFunctionBySlot(ejs, (EjsVar*) lhs, ES_String_MINUS, 1, &arg);

    case EJS_OP_REM:
        arg = (EjsVar*) rhs;
        return ejsRunFunctionBySlot(ejs, (EjsVar*) lhs, ES_String_MOD, 1, &arg);
#endif

    case EJS_OP_NEG:
    case EJS_OP_LOGICAL_NOT:
    case EJS_OP_NOT:
        /* Already handled in coerceStringOperands */
    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->obj.var.type->qname.name);
        return 0;
    }
    mprAssert(0);
}


/*
 *  Lookup an string index.
 */
static int lookupStringProperty(struct Ejs *ejs, EjsString *sp, EjsName *qname)
{
    int     index;

    if (qname == 0 || ! isdigit((int) qname->name[0])) {
        return EJS_ERR;
    }
    index = atoi(qname->name);
    if (index < sp->length) {
        return index;
    }

    return EJS_ERR;
}


/*
 *  String constructor.
 *
 *      function String()
 *      function String(str: String)
 */
static EjsVar *stringConstructor(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsArray    *args;
    EjsString   *str;

    mprAssert(argc == 0 || (argc == 1 && ejsIsArray(argv[0])));
    sp->length = 0;

    if (argc == 1) {
        args = (EjsArray*) argv[0];
        if (args->length > 0) {
            str = ejsToString(ejs, ejsGetProperty(ejs, (EjsVar*) args, 0));
            if (str) {
                sp->value = mprStrdup(sp, str->value);
                sp->length = str->length;
            }
        } else {
            sp->value = mprStrdup(ejs, "");
            if (sp->value == 0) {
                return 0;
            }
            sp->length = 0;
        }

    } else {
        sp->value = mprStrdup(ejs, "");
        if (sp->value == 0) {
            return 0;
        }
        sp->length = 0;
    }
    return (EjsVar*) sp;
}


/*
 *  Do a case sensitive comparison between this string and another.
 *
 *  function caseCompare(compare: String): Number
 */
static EjsVar *caseCompare(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    int     result;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    result = mprStrcmp(sp->value, ((EjsString*) argv[0])->value);

    return (EjsVar*) ejsCreateNumber(ejs, result);
}


/*
 *  Return a string containing the character at a given index
 *
 *  function charAt(index: Number): String
 */
static EjsVar *charAt(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    int     index;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));

    index = ejsGetInt(argv[0]);
    if (index < 0 || index >= sp->length) {
        return (EjsVar*) ejs->emptyStringValue;
    }
    return (EjsVar*) ejsCreateStringWithLength(ejs, &sp->value[index], 1);
}


/*
 *  Return an integer containing the character at a given index
 *
 *  function charCodeAt(index: Number = 0): Number
 */
static EjsVar *charCodeAt(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    int     index;

    index = (argc == 1) ? ejsGetInt(argv[0]) : 0;
    if (index < 0) {
        index = sp->length - 1;
    }
    if (index < 0 || index >= sp->length) {
        return (EjsVar*) ejs->nanValue;
    }
    return (EjsVar*) ejsCreateNumber(ejs, (uchar) sp->value[index]);
}


/*
 *  Catenate args to a string and return a new string.
 *
 *  function concat(...args): String
 */
static EjsVar *concatString(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsArray    *args;
    EjsString   *result;
    int         i, count;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));
    args = (EjsArray*) argv[0];

    result = ejsDupString(ejs, sp);

    count = ejsGetPropertyCount(ejs, (EjsVar*) args);
    for (i = 0; i < args->length; i++) {
        if (ejsStrcat(ejs, result, ejsGetProperty(ejs, (EjsVar*) args, i)) < 0) {
            ejsThrowMemoryError(ejs);
            return 0;
        }
    }
    return (EjsVar*) result;
}


/**
 *  Check if a string contains the pattern (string or regexp)
 *
 *  function contains(pattern: Object): Boolean
 */
static EjsVar *containsString(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsVar      *pat;

    pat = argv[0];

    if (ejsIsString(pat)) {
        return (EjsVar*) ejsCreateBoolean(ejs, strstr(sp->value, ((EjsString*) pat)->value) != 0);

#if BLD_FEATURE_REGEXP
    } else if (ejsIsRegExp(pat)) {
        EjsRegExp   *rp;
        int         count;
        rp = (EjsRegExp*) argv[0];
        count = pcre_exec(rp->compiled, NULL, sp->value, sp->length, 0, 0, 0, 0);
        return (EjsVar*) ejsCreateBoolean(ejs, count >= 0);
#endif
    }
    ejsThrowTypeError(ejs, "Wrong argument type");
    return 0;
}


/**
 *  Check if a string ends with a given pattern
 *
 *  function endsWith(pattern: String): Boolean
 */
static EjsVar *endsWith(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    char        *pattern;
    int         len;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    pattern = ejsGetString(argv[0]);
    len = (int) strlen(pattern);
    if (len > sp->length) {
        return (EjsVar*) ejs->falseValue;
    }
    return (EjsVar*) ejsCreateBoolean(ejs, strncmp(&sp->value[sp->length - len], pattern, len) == 0);
}


/**
 *  Format the arguments
 *
 *  function format(...args): String
 *
 *  Format:         %[modifier][width][precision][type]
    Modifiers:      +- #,
 */
static EjsVar *formatString(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsArray    *args, *inner;
    EjsString   *result;
    EjsVar      *value;
    char        *buf;
    char        fmt[32];
    int         c, i, len, nextArg, start, kind, last;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = (EjsArray*) argv[0];

    /*
     *  Flatten the args if there is only one element and it is itself an array. This happens when invoked
     *  via the overloaded operator '%' which in turn invokes format()
     */
    if (args->length == 1) {
        inner = (EjsArray*) ejsGetProperty(ejs, (EjsVar*) args, 0);
        if (ejsIsArray(inner)) {
            args = inner;
        }
    }

    result = ejsCreateString(ejs, 0);

    if (result == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    /*
     *  Parse the format string and extract one specifier at a time.
     */
    last = 0;
    for (i = 0, nextArg = 0; i < sp->length && nextArg < args->length; i++) {
        c = sp->value[i];
        if (c != '%') {
            continue;
        }
        if (i > last) {
            catString(ejs, result, &sp->value[last], i - last);
        }

        /*
         *  Find the end of the format specifier and determine the format type (kind)
         */
        start = i++;
        i += (int) strspn(&sp->value[i], "-+ #,0*123456789.");
        kind = sp->value[i];

        if (strchr("cdefginopsSuxX", kind)) {
            len = i - start + 1;
            mprMemcpy(fmt, sizeof(fmt) - 4, &sp->value[start], len);
            fmt[len] = '\0';

            value = ejsGetProperty(ejs, (EjsVar*) args, nextArg);

            buf = 0;
            switch (kind) {
            case 'e': case 'g': case 'f':
#if BLD_FEATURE_FLOATING_POINT
                value = (EjsVar*) ejsToNumber(ejs, value);
                buf = mprAsprintf(ejs, -1, fmt, ejsGetNumber(value));
                break;
#else
                //   Fall through to normal number case
#endif
            case 'd': case 'i': case 'o': case 'u':
                value = (EjsVar*) ejsToNumber(ejs, value);
#if BLD_FEATURE_FLOATING_POINT
                /* Convert to floating point format */
                strcpy(&fmt[len - 1], ".0f");
                buf = mprAsprintf(ejs, -1, fmt, ejsGetNumber(value));
#else
                buf = mprAsprintf(ejs, -1, fmt, (int) ejsGetNumber(value));
#endif
                break;

            case 's':
                value = (EjsVar*) ejsToString(ejs, value);
                buf = mprAsprintf(ejs, -1, fmt, ejsGetString(value));
                break;

            case 'X': case 'x':
                buf = mprAsprintf(ejs, -1, fmt, (int) ejsGetNumber(value));
                break;

            case 'n':
                buf = mprAsprintf(ejs, -1, fmt, 0);

            default:
                ejsThrowArgError(ejs, "Bad format specifier");
                return 0;
            }
            catString(ejs, result, buf, strlen(buf));
            mprFree(buf);
            last = i + 1;
            nextArg++;
        }
    }

    i = (int) strlen(sp->value);
    if (i > last) {
        catString(ejs, result, &sp->value[last], i - last);
    }

    return (EjsVar*) result;
}


/*
 *  Create a string from character codes
 *
 *  static function fromCharCode(...codes): String
 */
static EjsVar *fromCharCode(Ejs *ejs, EjsString *unused, int argc, EjsVar **argv)
{
    EjsString   *result;
    EjsArray    *args;
    EjsVar      *vp;
    int         i;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));
    args = (EjsArray*) argv[0];

    result = (EjsString*) ejsCreateBareString(ejs, argc + 1);
    if (result == 0) {
        return 0;
    }

    for (i = 0; i < args->length; i++) {
        vp = ejsGetProperty(ejs, (EjsVar*) args, i);
        result->value[i] = ejsGetInt(ejsToNumber(ejs, vp));
    }
    result->value[i] = '\0';
    result->length = args->length;

    return (EjsVar*) result;
}


/*
 *  Function to iterate and return the next character code.
 *  NOTE: this is not a method of String. Rather, it is a callback function for Iterator
 */
static EjsVar *nextStringKey(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsString   *sp;

    sp = (EjsString*) ip->target;

    if (!ejsIsString(sp)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    if (ip->index < sp->length) {
        return (EjsVar*) ejsCreateNumber(ejs, ip->index++);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return the default iterator. This returns the array index names.
 *
 *  iterator function get(): Iterator
 */
static EjsVar *getStringIterator(Ejs *ejs, EjsVar *sp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, sp, (EjsNativeFunction) nextStringKey, 0, NULL);
}


/*
 *  Function to iterate and return the next string character (as a string).
 *  NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextStringValue(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsString   *sp;

    sp = (EjsString*) ip->target;
    if (!ejsIsString(sp)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    if (ip->index < sp->length) {
        return (EjsVar*) ejsCreateStringWithLength(ejs, &sp->value[ip->index++], 1);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return an iterator to return the next array element value.
 *
 *  iterator function getValues(): Iterator
 */
static EjsVar *getStringValues(Ejs *ejs, EjsVar *sp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, sp, (EjsNativeFunction) nextStringValue, 0, NULL);
}


/*
 *  Get the length of a string.
 *  @return Returns the number of characters in the string
 *
 *  override function get length(): Number
 */

static EjsVar *stringLength(Ejs *ejs, EjsString *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ap->length);
}


/*
 *  Return the position of the first occurance of a substring
 *
 *  function indexOf(pattern: String, startIndex: Number = 0): Number
 */
static EjsVar *indexOf(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    char    *pattern;
    int     index, start, patternLength;

    mprAssert(1 <= argc && argc <= 2);
    mprAssert(ejsIsString(argv[0]));

    pattern = ejsGetString(argv[0]);
    patternLength = ((EjsString*) argv[0])->length;

    if (argc == 2) {
        start = ejsGetInt(argv[1]);
        if (start > sp->length) {
            start = sp->length;
        }
        if (start < 0) {
            start = 0;
        }
    } else {
        start = 0;
    }
    index = indexof(&sp->value[start], sp->length - start, pattern, patternLength, 1);
    if (index < 0) {
        return (EjsVar*) ejs->minusOneValue;
    }
    return (EjsVar*) ejsCreateNumber(ejs, index + start);
}


static EjsVar *isAlpha(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    cchar   *cp;

    if (sp->length == 0) {
        return (EjsVar*) ejs->falseValue;
    }
    for (cp = sp->value; cp < &sp->value[sp->length]; cp++) {
        /* Avoid windows asserts with negative numbers */
        if (*cp & 0x80 || !isalpha((int) *cp)) {
            return (EjsVar*) ejs->falseValue;
        }
    }
    return (EjsVar*) ejs->trueValue;
}


static EjsVar *isDigit(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    cchar   *cp;

    if (sp->length == 0) {
        return (EjsVar*) ejs->falseValue;
    }
    for (cp = sp->value; cp < &sp->value[sp->length]; cp++) {
        /* Avoid windows asserts with negative numbers */
        if (*cp & 0x80 || !isdigit((int) *cp)) {
            return (EjsVar*) ejs->falseValue;
        }
    }
    return (EjsVar*) ejs->trueValue;
}


static EjsVar *isLower(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    cchar   *cp;

    if (sp->length == 0) {
        return (EjsVar*) ejs->falseValue;
    }
    for (cp = sp->value; cp < &sp->value[sp->length]; cp++) {
        if (*cp != tolower((int) *cp)) {
            return (EjsVar*) ejs->falseValue;
        }
    }
    return (EjsVar*) ejs->trueValue;
}


static EjsVar *isSpace(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    cchar   *cp;

    if (sp->length == 0) {
        return (EjsVar*) ejs->falseValue;
    }
    for (cp = sp->value; cp < &sp->value[sp->length]; cp++) {
        if (!isspace((int) *cp)) {
            return (EjsVar*) ejs->falseValue;
        }
    }
    return (EjsVar*) ejs->trueValue;
}


static EjsVar *isUpper(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    cchar   *cp;

    if (sp->length == 0) {
        return (EjsVar*) ejs->falseValue;
    }
    for (cp = sp->value; cp < &sp->value[sp->length]; cp++) {
        if (*cp != toupper((int) *cp)) {
            return (EjsVar*) ejs->falseValue;
        }
    }
    return (EjsVar*) ejs->trueValue;
}


/*
 *  Return the position of the last occurance of a substring
 *
 *  function lastIndexOf(pattern: String, start: Number = -1): Number
 */
static EjsVar *lastIndexOf(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    char    *pattern;
    int     start, patternLength, index;

    mprAssert(1 <= argc && argc <= 2);

    pattern = ejsGetString(argv[0]);
    patternLength = ((EjsString*) argv[0])->length;

    if (argc == 2) {
        start = ejsGetInt(argv[1]);
        if (start >= sp->length) {
            start = sp->length - 1;
        }
        if (start < 0) {
            start = 0;
        }
    } else {
        start = 0;
    }

    index = indexof(sp->value, sp->length, pattern, patternLength, -1);
    if (index < 0) {
        return (EjsVar*) ejs->minusOneValue;
    }
    return (EjsVar*) ejsCreateNumber(ejs, index);
}


#if BLD_FEATURE_REGEXP
/*
 *  Match a pattern
 *
 *  function match(pattern: RegExp): Array
 */
static EjsVar *match(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsRegExp   *rp;
    EjsArray    *results;
    EjsString   *match;
    int         matches[EJS_MAX_REGEX_MATCHES * 3];
    int         i, count, len, resultCount;

    rp = (EjsRegExp*) argv[0];
    rp->endLastMatch = 0;
    results = NULL;
    resultCount = 0;

    do {
        count = pcre_exec(rp->compiled, NULL, sp->value, sp->length, rp->endLastMatch, 0, matches, 
            sizeof(matches) / sizeof(int));
        if (count <= 0) {
            break;
        }
        if (results == 0) {
            results = ejsCreateArray(ejs, count);
        }
        for (i = 0; i < count * 2; i += 2) {
            len = matches[i + 1] - matches[i];
            match = ejsCreateStringWithLength(ejs, &sp->value[matches[i]], len);
            ejsSetProperty(ejs, (EjsVar*) results, resultCount++, (EjsVar*) match);
            rp->endLastMatch = matches[i + 1];
            if (rp->global) {
                break;
            }
        }

    } while (rp->global);

    if (results == NULL) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) results;
}
#endif


static EjsVar *printable(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsString       *result;
    char            buf[16];
    int             i, j, k, len, nonprint;

    nonprint = 0;
    for (i = 0; i < sp->length; i++)  {
        if (!isprint((uchar) sp->value[i])) {
            nonprint++;
        }
    }
    if (nonprint == 0) {
        return (EjsVar*) sp;
    }
    result = ejsCreateBareString(ejs, sp->length + (nonprint * 6) + 1);
    if (result == 0) {
        return 0;
    }
    for (i = 0, j = 0; i < sp->length; i++)  {
        if (isprint((uchar) sp->value[i])) {
            result->value[j++] = sp->value[i];
        } else {
            result->value[j++] = '\\';
            result->value[j++] = 'u';
            mprItoa(buf, 4, (uchar) sp->value[i], 16);
            len = (int) strlen(buf);
            for (k = len; k < 4; k++) {
                result->value[j++] = '0';
            }
            for (k = 0; buf[k]; k++) {
                result->value[j++] = buf[k];
            }
        }
    }
    result->value[j] = '\0';
    result->length = j;
    return (EjsVar*) result;
}


static EjsVar *quote(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsString       *result;

    result = ejsCreateBareString(ejs, sp->length + 2);
    if (result == 0) {
        return 0;
    }

    memcpy(&result->value[1], sp->value, sp->length);
    result->value[0] = '"';
    result->value[sp->length + 1] = '"';
    result->value[sp->length + 2] = '\0';
    result->length = sp->length + 2;

    return (EjsVar*) result;
}


/*
 *  Remove characters and return a new string.
 *
 *  function remove(start: Number, end: Number = -1): String
 *
 */
static EjsVar *removeCharsFromString(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsString       *result;
    int             start, end, i, j;

    mprAssert(1 <= argc && argc <= 2);

    start = ejsGetInt(argv[0]);
    end = ejsGetInt(argv[1]);

    if (start < 0) {
        start += sp->length;
    }
    if (end < 0) {
        end += sp->length;
    }
    if (start >= sp->length) {
        start = sp->length - 1;
    }
    if (end > sp->length) {
        end = sp->length;
    }

    result = ejsCreateBareString(ejs, sp->length - (end - start));
    if (result == 0) {
        return 0;
    }
    for (j = i = 0; i < start; i++, j++) {
        result->value[j] = sp->value[i];
    }
    for (i = end; i < sp->length; i++, j++) {
        result->value[j] = sp->value[i];
    }
    result->value[j] = '\0';

    return (EjsVar*) result;
}


/*
 *  Search and replace.
 *
 *  function replace(pattern: (String|Regexp), replacement: String): String
 *
 */
static EjsVar *replace(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsString   *result, *replacement;
    char        *pattern;
    int         index, patternLength;

    result = 0;
    replacement = (EjsString*) argv[1];

    if (ejsIsString(argv[0])) {
        pattern = ejsGetString(argv[0]);
        patternLength = ((EjsString*) argv[0])->length;

        index = indexof(sp->value, sp->length, pattern, patternLength, 1);
        if (index >= 0) {
            result = ejsCreateString(ejs, 0);
            if (result == 0) {
                return 0;
            }
            catString(ejs, result, sp->value, index);
            catString(ejs, result, replacement->value, replacement->length);

            index += patternLength;
            if (index < sp->length) {
                catString(ejs, result, &sp->value[index], sp->length - index);
            }

        } else {
            result = ejsDupString(ejs, sp);
        }

#if BLD_FEATURE_REGEXP
    } else if (ejsIsRegExp(argv[0])) {
        EjsRegExp   *rp;
        char        *cp, *lastReplace, *end;
        int         matches[EJS_MAX_REGEX_MATCHES * 3];
        int         count, endLastMatch, submatch;

        rp = (EjsRegExp*) argv[0];
        result = ejsCreateString(ejs, 0);
        endLastMatch = 0;

        do {
            count = pcre_exec(rp->compiled, NULL, sp->value, sp->length, endLastMatch, 0, matches, 
                    sizeof(matches) / sizeof(int));
            if (count <= 0) {
                break;
            }

            if (endLastMatch < matches[0]) {
                /* Append prior string text */
                catString(ejs, result, &sp->value[endLastMatch], matches[0] - endLastMatch);
            }

            /*
             *  Process the replacement template
             */
            end = &replacement->value[replacement->length];
            lastReplace = replacement->value;

            for (cp = replacement->value; cp < end; ) {
                if (*cp == '$') {
                    if (lastReplace < cp) {
                        catString(ejs, result, lastReplace, (int) (cp - lastReplace));
                    }
                    switch (*++cp) {
                    case '$':
                        catString(ejs, result, "$", 1);
                        break;
                    case '&':
                        /* Replace the matched string */
                        catString(ejs, result, &sp->value[matches[0]], matches[1] - matches[0]);
                        break;
                    case '`':
                        /* Insert the portion that preceeds the matched string */
                        catString(ejs, result, sp->value, matches[0]);
                        break;
                    case '\'':
                        /* Insert the portion that follows the matched string */
                        catString(ejs, result, &sp->value[matches[1]], sp->length - matches[1]);
                        break;
                    default:
                        /* Insert the nth submatch */
                        if (isdigit((int) *cp)) {
                            submatch = (int) mprAtoi(cp, 10);
                            while (isdigit((int) *++cp))
                                ;
                            cp--;
                            if (submatch < count) {
                                submatch *= 2;
                                catString(ejs, result, &sp->value[matches[submatch]], 
                                    matches[submatch + 1] - matches[submatch]);
                            }

                        } else {
                            ejsThrowArgError(ejs, "Bad replacement $ specification");
                            return 0;
                        }
                    }
                    lastReplace = cp + 1;
                }
                cp++;
            }
            if (lastReplace < cp && lastReplace < end) {
                catString(ejs, result, lastReplace, (int) (cp - lastReplace));
            }
            endLastMatch = matches[1];

        } while (rp->global);

        if (endLastMatch < sp->length) {
            /* Append remaining string text */
            catString(ejs, result, &sp->value[endLastMatch], sp->length - endLastMatch);
        }
#endif

    } else {
        ejsThrowTypeError(ejs, "Wrong argument type");
        return 0;
    }
    return (EjsVar*) result;
}


static EjsVar *reverseString(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    int         i, j, tmp;

    if ((sp = ejsCreateStringWithLength(ejs, sp->value, sp->length)) == 0) {
        return 0;
    }
    if (sp->length <= 1) {
        return (EjsVar*) sp;
    }
    i = (sp->length - 2) / 2;
    j = (sp->length + 1) / 2;
    for (; i >= 0; i--, j++) {
        tmp = sp->value[i];
        sp->value[i] = sp->value[j];
        sp->value[j] = tmp;
    }
    return (EjsVar*) sp;
}


/*
 *  Search for a pattern
 *
 *  function search(pattern: (String | RegExp)): Number
 *
 */
static EjsVar *searchString(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    char        *pattern;
    int         index, patternLength;

    if (ejsIsString(argv[0])) {
        pattern = ejsGetString(argv[0]);
        patternLength = ((EjsString*) argv[0])->length;

        index = indexof(sp->value, sp->length, pattern, patternLength, 1);
        return (EjsVar*) ejsCreateNumber(ejs, index);

#if BLD_FEATURE_REGEXP
    } else if (ejsIsRegExp(argv[0])) {
        EjsRegExp   *rp;
        int         matches[EJS_MAX_REGEX_MATCHES * 3];
        int         count;
        rp = (EjsRegExp*) argv[0];
        count = pcre_exec(rp->compiled, NULL, sp->value, sp->length, 0, 0, matches, sizeof(matches) / sizeof(int));
        if (count < 0) {
            return (EjsVar*) ejs->minusOneValue;
        }
        return (EjsVar*) ejsCreateNumber(ejs, matches[0]);
#endif

    } else {
        ejsThrowTypeError(ejs, "Wrong argument type");
        return 0;
    }
}


/*
 *  Return a substring. End is one past the last character.
 *
 *  function slice(start: Number, end: Number = -1, step: Number = 1): String
 */
static EjsVar *sliceString(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsString       *result;
    int             start, end, step, i, j, size;

    mprAssert(1 <= argc && argc <= 3);

    start = ejsGetInt(argv[0]);
    if (argc >= 2) {
        end = ejsGetInt(argv[1]);
    } else {
        end = sp->length;
    }
    if (argc == 3) {
        step = ejsGetInt(argv[2]);
    } else {
        step = 1;
    }
    if (start < 0) {
        start += sp->length;
    }
    if (start >= sp->length) {
        start = sp->length - 1;
    }
    if (start < 0) {
        start = 0;
    }
    if (end < 0) {
        end += sp->length;
    }
    if (end > sp->length) {
        end = sp->length;
    }
    if (end < 0) {
        end = 0;
    }
    if (step == 0) {
        step = 1;
    }
    if (end < start) {
        end = start;
    }
    size = (start < end) ? end - start : start - end;
    result = ejsCreateBareString(ejs, size / abs(step) + 1);
    if (result == 0) {
        return 0;
    }
    if (step > 0) {
        for (i = start, j = 0; i < end; i += step) {
            result->value[j++] = sp->value[i];
        }

    } else {
#if WAS
        for (i = end - 1, j = 0; i >= start; i += step) {
            result->value[j++] = sp->value[i];
        }
#else
        for (i = start, j = 0; i > end; i += step) {
            result->value[j++] = sp->value[i];
        }
#endif
    }
    result->value[j] = '\0';
    result->length = j;
    return (EjsVar*) result;
}


/*
 *  Split a string
 *
 *  function split(delimiter: (String | RegExp), limit: Number = -1): Array
 */
static EjsVar *split(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsArray    *results;
    EjsString   *elt;
    char        *delim, *cp, *mark, *end;
    int         delimLen, limit;

    mprAssert(1 <= argc && argc <= 2);

    limit = (argc == 2) ? ejsGetInt(argv[1]): -1;
    results = ejsCreateArray(ejs, 0);

    if (ejsIsString(argv[0])) {
        delim = ejsGetString(argv[0]);
        delimLen = (int) strlen(delim);

        if (delimLen == 0) {
            for (cp = sp->value; (--limit != -1) && *cp; cp++) {
                ejsSetProperty(ejs, (EjsVar*) results, -1, (EjsVar*) ejsCreateStringWithLength(ejs, cp, 1));
            }

        } else {
            end = &sp->value[sp->length];
            for (mark = cp = sp->value; (--limit != -1) && mark < end; mark++) {
                if (strncmp(mark, delim, delimLen) == 0) {
                    elt = ejsCreateStringWithLength(ejs, cp, (int) (mark - cp));
                    ejsSetProperty(ejs, (EjsVar*) results, -1, (EjsVar*) elt);
                    cp = mark + delimLen;
                    mark = cp;
                    if (mark >= end) {
                        break;
                    }
                }
            }
            if (mark > cp) {
                elt = ejsCreateStringWithLength(ejs, cp, (int) (mark - cp));
                ejsSetProperty(ejs, (EjsVar*) results, -1, (EjsVar*) elt);
            }
        }
        return (EjsVar*) results;

#if BLD_FEATURE_REGEXP
    } else if (ejsIsRegExp(argv[0])) {
        EjsRegExp   *rp;
        EjsString   *match;
        int         matches[EJS_MAX_REGEX_MATCHES * 3], count, resultCount;
        rp = (EjsRegExp*) argv[0];
        rp->endLastMatch = 0;
        resultCount = 0;
        do {
            count = pcre_exec(rp->compiled, NULL, sp->value, sp->length, rp->endLastMatch, 0, matches, 
                sizeof(matches) / sizeof(int));
            if (count <= 0) {
                break;
            }
            if (rp->endLastMatch < matches[0]) {
                match = ejsCreateStringWithLength(ejs, &sp->value[rp->endLastMatch], matches[0] - rp->endLastMatch);
                ejsSetProperty(ejs, (EjsVar*) results, resultCount++, (EjsVar*) match);
            }
            rp->endLastMatch = matches[1];
        } while (rp->global);

        if (rp->endLastMatch < sp->length) {
            match = ejsCreateStringWithLength(ejs, &sp->value[rp->endLastMatch], sp->length - rp->endLastMatch);
            ejsSetProperty(ejs, (EjsVar*) results, resultCount++, (EjsVar*) match);
        }
        return (EjsVar*) results;
#endif
    }
    ejsThrowTypeError(ejs, "Wrong argument type");
    return 0;
}


/**
 *  Check if a string starts with a given pattern
 *
 *  function startsWith(pattern: String): Boolean
 */
static EjsVar *startsWith(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    char        *pattern;
    int         len;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    pattern = ejsGetString(argv[0]);
    len = (int) strlen(pattern);

    return (EjsVar*) ejsCreateBoolean(ejs, strncmp(&sp->value[0], pattern, len) == 0);
}


/*
 *  Extract a substring. Simple routine with positive indicies.
 *
 *  function substring(start: Number, end: Number = -1): String
 */
static EjsVar *substring(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    int     start, end, tmp;

    start = ejsGetInt(argv[0]);
    if (argc == 2) {
        end = ejsGetInt(argv[1]);
    } else {
        end = sp->length;
    }
    if (start < 0) {
        start = 0;
    }
    if (start >= sp->length) {
        start = sp->length - 1;
    }
    if (end < 0) {
        end = sp->length;
    }
    if (end > sp->length) {
        end = sp->length;
    }
    /*
        Swap if start is bigger than end
     */
    if (start > end) {
        tmp = start;
        start = end;
        end = tmp;
    }
    return (EjsVar*) ejsCreateStringWithLength(ejs, &sp->value[start], end - start);
}


/*
 *  Convert the string to camelCase. Return a new string.
 *
 *  function toCamel(): String
 */
static EjsVar *toCamel(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsString   *result;

    result = ejsCreateStringWithLength(ejs, sp->value, sp->length);
    if (result == 0) {
        return 0;
    }
    result->value[0] = tolower((int) sp->value[0]);

    return (EjsVar*) result;
}


/*
 *  Convert to a JSON string
 *
 *  override function toJSON(): String
 */
static EjsVar *stringToJson(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsVar  *result;
    MprBuf  *buf;
    int     i, c;

    buf = mprCreateBuf(sp, 0, 0);
    mprPutCharToBuf(buf, '"');
    for (i = 0; i < sp->length; i++) {
        c = sp->value[i];
        if (c == '"' || c == '\\') {
            mprPutCharToBuf(buf, '\\');
            mprPutCharToBuf(buf, c);
        } else {
            mprPutCharToBuf(buf, c);
        }
    }
    mprPutCharToBuf(buf, '"');
    mprAddNullToBuf(buf);
    result = (EjsVar*) ejsCreateString(ejs, mprGetBufStart(buf));
    mprFree(buf);
    return result;
}


/*
 *  Convert the string to lower case.
 *
 *  function toLower(): String
 */
static EjsVar *toLower(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    EjsString       *result;
    int             i;

    result = ejsCreateStringWithLength(ejs, sp->value, sp->length);
    if (result == 0) {
        return 0;
    }
    for (i = 0; i < result->length; i++) {
        result->value[i] = tolower((int) result->value[i]);
    }
    return (EjsVar*) result;
}


/*
 *  Convert the string to PascalCase. Return a new string.
 *
 *  function toPascal(): String
 */
static EjsVar *toPascal(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsString   *result;

    result = ejsCreateStringWithLength(ejs, sp->value, sp->length);
    if (result == 0) {
        return 0;
    }
    result->value[0] = toupper((int) sp->value[0]);

    return (EjsVar*) result;
}


/*
 *  Convert to a string
 *
 *  override function toString(): String
 */
static EjsVar *stringToString(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    return (EjsVar*) sp;
}


/*
 *  Convert the string to upper case.
 *  @return Returns a new upper case version of the string.
 *  @spec ejs-11
 *
 *  function toUpper(): String
 */
static EjsVar *toUpper(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    EjsString       *result;
    int             i;

    result = ejsCreateStringWithLength(ejs, sp->value, sp->length);
    if (result == 0) {
        return 0;
    }
    for (i = 0; i < result->length; i++) {
        result->value[i] = toupper((int) result->value[i]);
    }
    return (EjsVar*) result;
}


/*
 *  Scan the input and tokenize according to the format string
 *
 *  function tokenize(format: String): Array
 */
static EjsVar *tokenize(Ejs *ejs, EjsString *sp, int argc, EjsVar **argv)
{
    EjsArray    *result;
    cchar       *fmt;
    char        *end, *buf;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    buf = sp->value;
    fmt = ejsGetString(argv[0]);
    result = ejsCreateArray(ejs, 0);

    for (fmt = ejsGetString(argv[0]); *fmt && buf < &sp->value[sp->length]; ) {
        if (*fmt++ != '%') {
            continue;
        }
        switch (*fmt) {
        case 's':
            for (end = buf; *end; end++) {
                if (isspace((int) *end)) {
                    break;
                }
            }
            ejsSetProperty(ejs, (EjsVar*) result, -1, (EjsVar*) ejsCreateStringWithLength(ejs, buf, (int) (end - buf)));
            buf = end;
            break;

        case 'd':
            ejsSetProperty(ejs, (EjsVar*) result, -1, ejsParseVar(ejs, buf, ES_Number));
            while (*buf && !isspace((int) *buf)) {
                buf++;
            }
            break;

        default:
            ejsThrowArgError(ejs, "Bad format specifier");
            return 0;
        }
        while (*buf && isspace((int) *buf)) {
            buf++;
        }
    }
    return (EjsVar*) result;
}


/**
 *  Returns a trimmed copy of the string. Normally used to trim white space, but can be used to trim any substring
 *  from the start or end of the string.
 *  @param str May be set to a substring to trim from the string. If not set, it defaults to any white space.
 *  @return Returns a (possibly) modified copy of the string
 *  @spec ecma-4
 *
 *  function trim(str: String = null): String
 */
static EjsVar *trimString(Ejs *ejs, EjsString *sp, int argc,  EjsVar **argv)
{
    cchar           *start, *end, *pattern, *mark;
    int             index, patternLength;

    mprAssert(argc == 0 || (argc == 1 && ejsIsString(argv[0])));

    if (argc == 0) {
        for (start = sp->value; start < &sp->value[sp->length]; start++) {
            if (!isspace((int) *start)) {
                break;
            }
        }
        for (end = &sp->value[sp->length - 1]; end >= start; end--) {
            if (!isspace((int) *end)) {
                break;
            }
        }
        end++;

    } else {
        pattern = ejsGetString(argv[0]);
        patternLength = ((EjsString*) argv[0])->length;
        if (patternLength <= 0 || patternLength > sp->length) {
            return (EjsVar*) sp;
        }

        /*
         *  Trim the front
         */
        for (mark = sp->value; &mark[patternLength] < &sp->value[sp->length]; mark += patternLength) {
            index = indexof(mark, patternLength, pattern, patternLength, 1);
            if (index != 0) {
                break;
            }
        }
        start = mark;

        /*
         *  Trim the back
         */
        for (mark = &sp->value[sp->length - patternLength]; mark >= sp->value; mark -= patternLength) {
            index = indexof(mark, patternLength, pattern, patternLength, 1);
            if (index != 0) {
                break;
            }
        }
        end = mark + patternLength;
    }
    return (EjsVar*) ejsCreateStringWithLength(ejs, start, (int) (end - start));
}


/**
 *  Fast append a string. This modifies the original "dest" string. BEWARE: strings are meant to be immutable.
 *  Only use this when constructing strings.
 */
static int catString(Ejs *ejs, EjsString *dest, char *str, int len)
{
    EjsString   *castSrc;
    char        *oldBuf, *buf;
    int         oldLen, newLen;

    mprAssert(dest);

    castSrc = 0;

    oldBuf = dest->value;
    oldLen = dest->length;
    newLen = oldLen + len + 1;

    buf = (char*) mprRealloc(ejs, oldBuf, newLen);
    if (buf == 0) {
        return -1;
    }
    dest->value = buf;
    memcpy(&buf[oldLen], str, len);
    dest->length += len;
    buf[dest->length] = '\0';

    return 0;
}


/**
 *  Fast append a string. This modifies the original "dest" string. BEWARE: strings are meant to be immutable.
 *  Only use this when constructing strings.
 */
int ejsStrcat(Ejs *ejs, EjsString *dest, EjsVar *src)
{
    EjsString   *castSrc;
    char        *str;
    int         len;

    mprAssert(dest);

    castSrc = 0;

    if (ejsIsString(dest)) {
        if (! ejsIsString(src)) {
            castSrc = (EjsString*) ejsToString(ejs, src);
            if (castSrc == 0) {
                return -1;
            }
            len = (int) strlen(castSrc->value);
            str = castSrc->value;

        } else {
            str = ((EjsString*) src)->value;
            len = ((EjsString*) src)->length;
        }

        if (catString(ejs, dest, str, len) < 0) {
            return -1;
        }

    } else {
        /*
         *  Convert the source to a string and then steal the rusult buffer and assign to the destination
         *  BUG - should be freeing the destination string.
         */
        castSrc = (EjsString*) ejsToString(ejs, src);
        dest->value = castSrc->value;
        mprStealBlock(dest, dest->value);
        castSrc->value = 0;
    }
    return 0;
}


/*
 *  Copy a string. Always null terminate.
 */
int ejsStrdup(MprCtx ctx, uchar **dest, cvoid *src, int nbytes)
{
    mprAssert(dest);
    mprAssert(src);

    if (nbytes > 0) {
        *dest = (uchar*) mprAlloc(ctx, nbytes + 1);
        if (*dest == 0) {
            return MPR_ERR_NO_MEMORY;
        }
        strncpy((char*) *dest, (char*) src, nbytes);

    } else {
        *dest = (uchar*) mprAlloc(ctx, 1);
        nbytes = 0;
    }
    (*dest)[nbytes] = '\0';
    return nbytes;
}


/*
 *  Find a substring. Search forward or backwards. Return the index in the string where the pattern was found.
 *  Return -1 if not found.
 */
static int indexof(cchar *str, int len, cchar *pattern, int patternLength, int dir)
{
    cchar   *s1, *s2;
    int     i, j;

    mprAssert(dir == 1 || dir == -1);

    if (dir > 0) {
        for (i = 0; i < len; i++) {
            s1 = &str[i];
            for (j = 0, s2 = pattern; j < patternLength; s1++, s2++, j++) {
                if (*s1 != *s2) {
                    break;
                }
            }
            if (*s2 == '\0') {
                return i;
            }
        }

    } else {
        for (i = len - 1; i >= 0; i--) {
            s1 = &str[i];
            for (j = 0, s2 = pattern; j < patternLength; s1++, s2++, j++) {
                if (*s1 != *s2) {
                    break;
                }
            }
            if (*s2 == '\0') {
                return i;
            }
        }
    }
    return -1;
}



EjsString *ejsCreateString(Ejs *ejs, cchar *value)
{
    EjsString   *sp;

    /*
     *  No need to invoke constructor
     */
    sp = (EjsString*) ejsCreateVar(ejs, ejs->stringType, 0);
    if (sp != 0) {
        sp->value = mprStrdup(ejs, value);
        if (sp->value == 0) {
            return 0;
        }
        sp->length = (int) strlen(sp->value);
        sp->obj.var.primitive = 1;
        ejsSetDebugName(sp, sp->value);
    }
    return sp;
}


EjsString *ejsCreateStringAndFree(Ejs *ejs, char *value)
{
    EjsString   *sp;

    sp = (EjsString*) ejsCreateVar(ejs, ejs->stringType, 0);
    if (sp != 0) {
        if (value == 0) {
            value = mprStrdup(sp, "");
        }
        sp->value = value;
        sp->obj.var.primitive = 1;
        mprStealBlock(sp, value);
        sp->length = (int) strlen(sp->value);
        ejsSetDebugName(sp, sp->value);
    }
    return sp;
}


EjsString *ejsDupString(Ejs *ejs, EjsString *sp)
{
    return ejsCreateStringWithLength(ejs, sp->value, sp->length);
}


/*
 *  Initialize a binary string value.
 */
EjsString *ejsCreateStringWithLength(Ejs *ejs, cchar *value, int len)
{
    EjsString   *sp;
    uchar       *dest;

    //  OPT Would be much faster to allocate the string value in the actual object since strings are immutable
    sp = (EjsString*) ejsCreateVar(ejs, ejs->stringType, 0);
    if (sp != 0) {
        sp->length = ejsStrdup(ejs, &dest, value, len);
        sp->value = (char*) dest;
        sp->obj.var.primitive = 1;
        if (sp->length < 0) {
            return 0;
        }
    }
    return sp;
}


/*
 *  Initialize an string with a pre-allocated buffer but without data..
 */
EjsString *ejsCreateBareString(Ejs *ejs, int len)
{
    EjsString   *sp;
    
    //  OPT Would be much faster to allocate the string value in the actual object since strings are immutable
    sp = (EjsString*) ejsCreateVar(ejs, ejs->stringType, 0);
    if (sp != 0) {
        sp->value = mprAlloc(sp, len + 1);
        if (sp->value == 0) {
            return 0;
        }
        sp->length = len;
        sp->value[len] = '\0';
        sp->obj.var.primitive = 1;
    }
    return sp;
}


void ejsCreateStringType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "String"), ejs->objectType, sizeof(EjsString),
        ES_String, ES_String_NUM_CLASS_PROP,  ES_String_NUM_INSTANCE_PROP,
        EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
    ejs->stringType = type;

    /*
     *  Define the helper functions.
     */
    type->helpers->castVar = (EjsCastVarHelper) castString;
    type->helpers->cloneVar = (EjsCloneVarHelper) cloneString;
    type->helpers->destroyVar = (EjsDestroyVarHelper) destroyString;
    type->helpers->getProperty = (EjsGetPropertyHelper) getStringProperty;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokeStringOperator;
    type->helpers->lookupProperty = (EjsLookupPropertyHelper) lookupStringProperty;

    type->numericIndicies = 1;

    /*
     *  Pre-create the empty string.
     */
    ejs->emptyStringValue = (EjsString*) ejsCreateString(ejs, "");
    ejsSetDebugName(ejs->emptyStringValue, "emptyString");
}


void ejsConfigureStringType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->stringType;

    /*
     *  Define the "string" alias
     */
    ejsSetProperty(ejs, ejs->global, ES_string, (EjsVar*) type);
    ejsBindMethod(ejs, type, ES_String_String, (EjsNativeFunction) stringConstructor);
    ejsBindMethod(ejs, type, ES_String_caseCompare, (EjsNativeFunction) caseCompare);
    ejsBindMethod(ejs, type, ES_String_charAt, (EjsNativeFunction) charAt);
    ejsBindMethod(ejs, type, ES_String_charCodeAt, (EjsNativeFunction) charCodeAt);
    ejsBindMethod(ejs, type, ES_String_concat, (EjsNativeFunction) concatString);
    ejsBindMethod(ejs, type, ES_String_contains, (EjsNativeFunction) containsString);
    ejsBindMethod(ejs, type, ES_String_endsWith, (EjsNativeFunction) endsWith);
    ejsBindMethod(ejs, type, ES_String_format, (EjsNativeFunction) formatString);
    ejsBindMethod(ejs, type, ES_String_fromCharCode, (EjsNativeFunction) fromCharCode);
    ejsBindMethod(ejs, type, ES_Object_get, (EjsNativeFunction) getStringIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, (EjsNativeFunction) getStringValues);
    ejsBindMethod(ejs, type, ES_String_indexOf, (EjsNativeFunction) indexOf);
    ejsBindMethod(ejs, type, ES_String_isDigit, (EjsNativeFunction) isDigit);
    ejsBindMethod(ejs, type, ES_String_isAlpha, (EjsNativeFunction) isAlpha);
    ejsBindMethod(ejs, type, ES_String_isLower, (EjsNativeFunction) isLower);
    ejsBindMethod(ejs, type, ES_String_isSpace, (EjsNativeFunction) isSpace);
    ejsBindMethod(ejs, type, ES_String_isUpper, (EjsNativeFunction) isUpper);
    ejsBindMethod(ejs, type, ES_Object_length, (EjsNativeFunction) stringLength);
    ejsBindMethod(ejs, type, ES_String_lastIndexOf, (EjsNativeFunction) lastIndexOf);
#if BLD_FEATURE_REGEXP
    ejsBindMethod(ejs, type, ES_String_match, (EjsNativeFunction) match);
#endif
    ejsBindMethod(ejs, type, ES_String_remove, (EjsNativeFunction) removeCharsFromString);
    ejsBindMethod(ejs, type, ES_String_slice, (EjsNativeFunction) sliceString);
    ejsBindMethod(ejs, type, ES_String_split, (EjsNativeFunction) split);
    ejsBindMethod(ejs, type, ES_String_printable, (EjsNativeFunction) printable);
    ejsBindMethod(ejs, type, ES_String_quote, (EjsNativeFunction) quote);
    ejsBindMethod(ejs, type, ES_String_replace, (EjsNativeFunction) replace);
    ejsBindMethod(ejs, type, ES_String_reverse, (EjsNativeFunction) reverseString);
    ejsBindMethod(ejs, type, ES_String_search, (EjsNativeFunction) searchString);
    ejsBindMethod(ejs, type, ES_String_startsWith, (EjsNativeFunction) startsWith);
    ejsBindMethod(ejs, type, ES_String_substring, (EjsNativeFunction) substring);
    ejsBindMethod(ejs, type, ES_String_toCamel, (EjsNativeFunction) toCamel);
    ejsBindMethod(ejs, type, ES_Object_toJSON, (EjsNativeFunction) stringToJson);
    ejsBindMethod(ejs, type, ES_String_toLower, (EjsNativeFunction) toLower);
    ejsBindMethod(ejs, type, ES_String_toPascal, (EjsNativeFunction) toPascal);
    ejsBindMethod(ejs, type, ES_Object_toString, (EjsNativeFunction) stringToString);
    ejsBindMethod(ejs, type, ES_String_toUpper, (EjsNativeFunction) toUpper);
    ejsBindMethod(ejs, type, ES_String_tokenize, (EjsNativeFunction) tokenize);
    ejsBindMethod(ejs, type, ES_String_trim, (EjsNativeFunction) trimString);
}

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsString.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsType.c"
 */
/************************************************************************/

/**
 *  ejsType.c - Type class
 *
 *  The type class is the base class for all types (classes) in the system.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static EjsType *createBootstrapType(Ejs *ejs, int numSlots);
static EjsType *createType(Ejs *ejs, EjsName *qname, EjsModule *up, EjsType *baseType, int instanceSize, int numSlots, 
        int attributes, void *typeData);
static EjsBlock *createInstanceBlock(Ejs *ejs, cchar *name, EjsBlock *baseBlock, int numSlots, int attributes);
static void fixInstanceSize(Ejs *ejs, EjsType *type);

/*
 *  Copy a type. 
 *
 *  intrinsic function copy(type: Object): Object
 */
static EjsType *cloneTypeVar(Ejs *ejs, EjsType *src, bool deep)
{
    EjsType     *dest;

    if (! ejsIsType(src)) {
        ejsThrowTypeError(ejs, "Expecting a Type object");
        return 0;
    }

    dest = (EjsType*) (ejs->blockHelpers->cloneVar)(ejs, (EjsVar*) src, deep);
    if (dest == 0) {
        return dest;
    }

    dest->qname = src->qname;
    dest->baseType = src->baseType;
    dest->instanceBlock = src->instanceBlock;
    dest->instanceSize = src->instanceSize;
    dest->helpers = src->helpers;
    dest->module = src->module;
    dest->typeData = src->typeData;
    dest->id = src->id;

    /*
     *  OPT
     */
    dest->subTypeCount = src->subTypeCount;
    dest->callsSuper = src->callsSuper;
    dest->final = src->final;
    dest->fixupDone = src->fixupDone;
    dest->hasBaseConstructors = src->hasBaseConstructors;
    dest->hasBaseInitializers = src->hasBaseInitializers;
    dest->hasBaseStaticInitializers = src->hasBaseStaticInitializers;
    dest->hasConstructor = src->hasConstructor;
    dest->hasInitializer = src->hasInitializer;
    dest->hasStaticInitializer = src->hasStaticInitializer;
    dest->initialized = src->initialized;
    dest->isInterface = src->isInterface;
    dest->objectBased = src->objectBased;
    dest->needFixup = src->needFixup;
    dest->numericIndicies = src->numericIndicies;
    dest->skipScope = src->skipScope;

    /* Don't copy pool. The cloned copy will have its own pool */

    return dest;
}


/*
 *  Create a new Type object. numSlots is the number of property slots to pre-allocate.
 *  This is hand-crafted to create types as small as possible.
 */
static EjsType *createTypeVar(Ejs *ejs, EjsType *typeType, int numSlots)
{
    EjsType         *type;
    EjsObject       *obj;
    EjsVar          *vp;
    EjsHashEntry    *entries;
    char            *start;
    int             typeSize, allocSlots, hashSize, i, dynamic;

    mprAssert(ejs);

    /*
     *  If the compiler is building itself (empty mode), then the types themselves must be dynamic. Otherwise, the type
     *  is fixed and will contain the names hash and traits in one memory block. 
     *  NOTE: don't confuse this with dynamic objects.
     */
    hashSize = 0;

    if ((ejs->flags & (EJS_FLAG_EMPTY | EJS_FLAG_COMPILER | EJS_FLAG_DYNAMIC))) {
        dynamic = 1;
        typeSize = sizeof(EjsType);
        allocSlots = 0;

    } else {
        dynamic = 0;
        allocSlots = numSlots;
        typeSize = sizeof(EjsType);
        typeSize += (int) sizeof(EjsNames);
        typeSize += ((int) sizeof(EjsHashEntry) * numSlots);
        if (numSlots > EJS_HASH_MIN_PROP) {
            hashSize = ejsGetHashSize(numSlots);
            typeSize += (hashSize * (int) sizeof(int*));
        }
        typeSize += (int) sizeof(EjsTrait) * numSlots;
        typeSize += (int) sizeof(EjsVar*) * numSlots;
    }

    if ((vp = (EjsVar*) mprAllocZeroed(ejsGetAllocCtx(ejs), typeSize)) == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    obj = (EjsObject*) vp;
    type = (EjsType*) vp;
    ejsSetDebugName(vp, "type");

    vp->type = type;
    vp->master = (ejs->master == 0);
    vp->isObject = 1;
    vp->type = typeType;
    vp->isType = 1;
    vp->dynamic = dynamic;
    obj->capacity = allocSlots;
    type->subTypeCount = typeType->subTypeCount + 1;
    ejsInitList(&type->block.namespaces);

    if (dynamic) {
        if (numSlots > 0 && ejsGrowBlock(ejs, (EjsBlock*) type, numSlots) < 0) {
            mprAssert(0);
            return 0;
        }
        type->block.numTraits = numSlots;
        mprAssert(type->block.numTraits <= type->block.sizeTraits);

    } else {
        /*
         *  This is for a fixed type. This is the normal case when not compiling. Layout is:
         *
         *  Type                Factor
         *      EjsNames
         *      EjsHashEntry    * numSlots (These are the property names and hash linkage)
         *      Hash buckets    ejsGetHashSize(numslots)
         *      Hash entries    (int*) * numSlots
         *      Traits          * numSlots
         *      Slots           * numSlots
         */
        start = (char*) type + sizeof(EjsType);

        obj->names = (EjsNames*) start;
        obj->names->sizeEntries = numSlots;
        start += sizeof(EjsNames);

        entries = obj->names->entries = (EjsHashEntry*) start;
        for (i = 0; i < numSlots; i++) {
            entries[i].nextSlot = -1;
            entries[i].qname.name = "";
            entries[i].qname.space = "";
        }
        start += (sizeof(EjsHashEntry) * numSlots);

        if (hashSize > 0) {
            obj->names->buckets = (int*) start;
            obj->names->sizeBuckets = hashSize;
            memset(obj->names->buckets, -1, hashSize * sizeof(int));
            start += sizeof(int) * hashSize;
        }

        type->block.traits = (EjsTrait*) start;
        type->block.sizeTraits = numSlots;
        type->block.numTraits = numSlots;
        start += sizeof(EjsTrait) * numSlots;

        obj->slots = (EjsVar**) start;
        obj->numProp = numSlots;
        start += (numSlots * sizeof(EjsVar*));

        mprAssert((start - (char*) type) <= typeSize);
    }
#if BLD_DEBUG
    ejsAddToGcStats(ejs, vp, ES_Type);
#endif
    return type;
}


/*
 *  Create a bootstrap type variable. This is used for the Object, Block and Type types.
 */
static EjsType *createBootstrapType(Ejs *ejs, int numSlots)
{
    EjsType     *type, bootstrap;
    EjsBlock    bootstrapInstanceBlock;

    mprAssert(ejs);

    memset(&bootstrap, 0, sizeof(bootstrap));
    memset(&bootstrapInstanceBlock, 0, sizeof(bootstrapInstanceBlock));

    bootstrap.instanceSize = sizeof(EjsType);
    bootstrap.subTypeCount = -1;
    bootstrap.instanceBlock = &bootstrapInstanceBlock;

    type = (EjsType*) createTypeVar(ejs, &bootstrap, numSlots);
    if (type == 0) {
        return 0;
    }
    /*
     *  This will be hand-crafted later
     */
    type->block.obj.var.type = 0;
    return type;
}


void markType(Ejs *ejs, EjsVar *parent, EjsType *type)
{
    ejsMarkBlock(ejs, parent, (EjsBlock*) type);

    if (type->instanceBlock) {
        ejsMarkVar(ejs, (EjsVar*) type, (EjsVar*) type->instanceBlock);
    }
}


static int setTypeProperty(Ejs *ejs, EjsType *type, int slotNum, EjsVar *value)
{
    if (slotNum < 0 && !type->block.obj.var.dynamic) {
        ejsThrowTypeError(ejs, "Object is not dynamic");
        return EJS_ERR;
    }
    return (ejs->blockHelpers->setProperty)(ejs, (EjsVar*) type, slotNum, value);
}


/*
 *  Create a core built-in type. This is used by core native type code to either create a type or to get a type
 *  that has been made by loading ejs.mod. Handles the EMPTY case when building the compiler itself.
 */
EjsType *ejsCreateCoreType(Ejs *ejs, EjsName *qname, EjsType *baseType, int instanceSize, int slotNum, int numTypeProp,
    int numInstanceProp, int attributes)
{
    EjsType     *type;

    type = ejsCreateType(ejs, qname, 0, baseType, instanceSize, slotNum, numTypeProp, numInstanceProp, attributes, 0);
    if (type == 0) {
        ejs->hasError = 1;
        return 0;
    }
    
    /*
     *  The coreTypes hash allows the loader to match the essential core type objects to those being loaded from a mod file.
     */
    mprAddHash(ejs->coreTypes, qname->name, type);
    return type;
}


EjsBlock *ejsCreateTypeInstanceBlock(Ejs *ejs, EjsType *type, int numInstanceProp)
{
    EjsType     *baseType;
    EjsBlock    *block;
    char        *instanceName;
    int         attributes;

    instanceName = mprStrcat(type, -1, type->qname.name, "InstanceType", NULL);

    attributes = 0;
    if (type->block.obj.var.native) {
       attributes |= EJS_ATTR_NATIVE;
    }
    baseType = type->baseType;
    block = createInstanceBlock(ejs, instanceName, (baseType) ? baseType->instanceBlock: 0, numInstanceProp, attributes);
    if (block == 0) {
        return 0;
    }
    type->instanceBlock = block;
    block->nobind = type->block.nobind;
    block->dynamicInstance = type->block.dynamicInstance;
    return block;
}


EjsType *ejsCreatePrototype(Ejs *ejs, EjsFunction *fun, int *prototypeSlot)
{
    EjsName     qname;
    EjsType     *type;
    int         slotNum;

    qname = ejsGetPropertyName(ejs, fun->owner, fun->slotNum);
    mprAssert(qname.name);

    slotNum = ejsGetPropertyCount(ejs, ejs->global);

    ejs->flags |= EJS_FLAG_DYNAMIC;
    type = ejsCreateType(ejs, &qname, NULL, ejs->objectType, ejs->objectType->instanceSize,
        slotNum, ES_Object_NUM_CLASS_PROP, ES_Object_NUM_INSTANCE_PROP, 
        EJS_ATTR_OBJECT | EJS_ATTR_DYNAMIC_INSTANCE | EJS_ATTR_OBJECT_HELPERS | EJS_ATTR_HAS_CONSTRUCTOR, NULL);
    ejs->flags &= ~EJS_FLAG_DYNAMIC;

    /*
     *  Install function as the constructor (always first class after object)
     */
    fun->constructor = 1;
    fun->thisObj = 0;
    ejsSetPropertyByName(ejs, (EjsVar*) type, &qname, (EjsVar*) fun);

    /*
     *  Setup the type as the Function.prototype
     */
    fun->prototype = type;
    if (fun->properties == 0) {
        fun->properties = ejsCreateSimpleObject(ejs);
    }
    *prototypeSlot = ejsSetPropertyByName(ejs, (EjsVar*) fun->properties, ejsName(&qname, "", "prototype"), (EjsVar*) type);

#if KEEP
    /*
     *  Also install as type.prototype
     */
    ejsSetPropertyByName(ejs, (EjsVar*) type, ejsName(&qname, "", "prototype"), (EjsVar*) type);
#endif
    return type;
}


/*
 *  Create a new type and initialize. BaseType is the super class for instances of the type being created. The
 *  returned EjsType will be an instance of EjsType. numTypeProp and  numInstanceProp should be set to the number
 *  of non-inherited properties.
 */
EjsType *ejsCreateType(Ejs *ejs, EjsName *qname, EjsModule *up, EjsType *baseType, int instanceSize,
                       int slotNum, int numTypeProp, int numInstanceProp, int attributes, void *typeData)
{
    EjsType     *type;
    int         needInstanceBlock;
    
    mprAssert(ejs);
    mprAssert(slotNum >= 0);
    
    needInstanceBlock = numInstanceProp;
    
    if ((ejs->flags & EJS_FLAG_EMPTY) && !ejs->initialized && attributes & EJS_ATTR_NATIVE) {
        /*
         *  If an empty interpreter, must not set a high number of properties based on the last slot generation.
         *  Property counts may be lower or zero this time round.
         */
        numTypeProp = 0;
        numInstanceProp = 0;
    }
    type = createType(ejs, qname, up, baseType, instanceSize, numTypeProp, attributes, typeData);
    if (type == 0) {
        return 0;
    }
    type->id = slotNum;
    ejsSetDebugName(type, type->qname.name);

    if (needInstanceBlock) {
        type->instanceBlock = ejsCreateTypeInstanceBlock(ejs, type, numInstanceProp);
    }
    return type;
}


/*
 *  Create a type object and initialize.
 */
static EjsType *createType(Ejs *ejs, EjsName *qname, EjsModule *up, EjsType *baseType, int instanceSize, int numSlots, 
        int attributes, void *typeData)
{
    EjsType     *type;

    mprAssert(ejs);
    mprAssert(instanceSize > 0);
    
    /*
     *  Create the type. For Object and Type, the value of ejs->typeType will be null. So bootstrap these first two types. 
     */
    if (ejs->typeType == 0) {
        type = (EjsType*) createBootstrapType(ejs, numSlots);

    } else {
        type = (EjsType*) createTypeVar(ejs, ejs->typeType, numSlots);
    }
    if (type == 0) {
        return 0;
    }

    if (baseType) {
        mprAssert(!(attributes & EJS_ATTR_SLOTS_NEED_FIXUP));

        if (baseType->hasConstructor || baseType->hasBaseConstructors) {
            type->hasBaseConstructors = 1;
        }
        if (baseType->hasInitializer || baseType->hasBaseInitializers) {
            type->hasBaseInitializers = 1;
        }
        type->baseType = baseType;
    }

    type->qname.name = qname->name;
    type->qname.space = qname->space;
    type->module = up;
    type->typeData = typeData;
    type->baseType = baseType;
    
    type->block.obj.var.native = (attributes & EJS_ATTR_NATIVE) ? 1 : 0;
    
    type->instanceSize = instanceSize;
    if (baseType) {
        fixInstanceSize(ejs, type);
    }

    /*
     *  OPT - should be able to just read in the attributes without having to stuff some in var and some in type.
     *  Should eliminate all the specific fields and just use BIT MASKS.
     */
    if (attributes & EJS_ATTR_SLOTS_NEED_FIXUP) {
        type->needFixup = 1;
    }
    if (attributes & EJS_ATTR_INTERFACE) {
        type->isInterface = 1;
    }
    if (attributes & EJS_ATTR_FINAL) {
        type->final = 1;
    }
    if (attributes & EJS_ATTR_OBJECT) {
        type->objectBased = 1;
    }
    if (attributes & EJS_ATTR_DYNAMIC_INSTANCE) {
        type->block.dynamicInstance = 1;
    }
    if (attributes & EJS_ATTR_HAS_CONSTRUCTOR) {
        /*
         *  This means the type certainly has a constructor method.
         */
        type->hasConstructor = 1;
    }
    if (attributes & EJS_ATTR_HAS_INITIALIZER) {
        type->hasInitializer = 1;
    }
    if (attributes & EJS_ATTR_HAS_STATIC_INITIALIZER) {
        type->hasStaticInitializer = 1;
    }
    if (attributes & EJS_ATTR_CALLS_SUPER) {
        type->callsSuper = 1;
    }
    if (attributes & EJS_ATTR_NO_BIND) {
        type->block.nobind = 1;
    }
    if (attributes & EJS_ATTR_BLOCK_HELPERS) {
        type->helpers = ejsGetBlockHelpers(ejs);
    } else if (attributes & EJS_ATTR_OBJECT_HELPERS) {
        type->helpers = ejsGetObjectHelpers(ejs);
    } else {
        type->helpers = ejsGetDefaultHelpers(ejs);
    }
    if (ejsGrowBlock(ejs, &type->block, numSlots) < 0) {
        return 0;
    }
    if (baseType && ejsInheritTraits(ejs, (EjsBlock*) type, (EjsBlock*) baseType, baseType->block.numTraits, 0, 0) < 0) {
        return 0;
    }
    return type;
}


/*
 *  Create a type instance block and initialize.
 */
static EjsBlock *createInstanceBlock(Ejs *ejs, cchar *name, EjsBlock *baseBlock, int numSlots, int attributes)
{
    EjsBlock    *block;

    mprAssert(ejs);
    
    /*
     *  Types and instance blocks are always eternal
     */
    block = ejsCreateBlock(ejs, numSlots);
    ejsSetDebugName(block, name);
    
    if (block == 0) {
        return 0;
    }

    /*
     *  OPT - should be able to just read in the attributes without having to stuff some in var and some in type.
     *  Should eliminate all the specific fields and just use BIT MASKS.
     */
    block->obj.var.native = (attributes & EJS_ATTR_NATIVE) ? 1 : 0;
    block->obj.var.isInstanceBlock = 1;
    
    if (numSlots > 0) {
        if (ejsGrowBlock(ejs, block, numSlots) < 0) {
            return 0;
        }
        if (baseBlock && ejsInheritTraits(ejs, (EjsBlock*) block, baseBlock, baseBlock->numTraits, 0, 0) < 0) {
            return 0;
        }
    }
    return block;
}


EjsType *ejsGetType(Ejs *ejs, int slotNum)
{
    EjsType     *type;

    if (slotNum < 0 || slotNum >= ejs->globalBlock->obj.numProp) {
        return 0;
    }
    type = (EjsType*) ejsGetProperty(ejs, ejs->global, slotNum);
    if (type == 0 || !ejsIsType(type)) {
        return 0;
    }
    return type;
}


static void fixInstanceSize(Ejs *ejs, EjsType *type)
{
    EjsType     *tp;

    type->hasNativeBase = 0;
    for (tp = type->baseType; tp && tp != ejs->objectType; tp = tp->baseType) {
        if (tp->instanceSize > type->instanceSize) {
            type->instanceSize = tp->instanceSize;
        }
        if (tp->block.obj.var.native) {
            type->hasNativeBase = 1;
        }
    }

    if (type->hasNativeBase && !type->block.obj.var.native) {
        /*
         *  Can't bind scripted class access if there is a native base class as the native class needs native helpers
         *  to access the properties.
         */
        type->block.nobind = 1;
        mprLog(ejs, 5, "NOTE: Type %s has a non-native base and won't be bound", type->qname.name);
    }
}


/*
 *  Fixup a type. This is used by the compiler and loader when it must first define a type without knowing the properties of 
 *  base classes. Consequently, it must fixup the type and its counts of inherited properties. It must also copy 
 *  inherited slots and traits. It is also used by the loader to fixup forward class references.
 */
int ejsFixupClass(Ejs *ejs, EjsType *type, EjsType *baseType, MprList *implements, int makeRoom)
{
    mprAssert(ejs);
    mprAssert(type);
    mprAssert(type != baseType);

    type->needFixup = 0;
    type->fixupDone = 1;
    type->baseType = baseType;
    
    if (baseType) {
        if (baseType->hasConstructor || baseType->hasBaseConstructors) {
            type->hasBaseConstructors = 1;
        }
        if (baseType->hasInitializer || baseType->hasBaseInitializers) {
            type->hasBaseInitializers = 1;
        }
        if (baseType != ejs->objectType && baseType->block.dynamicInstance) {
            type->block.dynamicInstance = 1;
        }
        type->subTypeCount = baseType->subTypeCount + 1;
    }
    fixInstanceSize(ejs, type);
    return ejsFixupBlock(ejs, (EjsBlock*) type, (EjsBlock*) baseType, implements, makeRoom);
}


/*
 *  Fixup a block. This is used by the compiler and loader when it must first define a type without knowing the properties 
 *  of base classes. Consequently, it must fixup the type and its counts of inherited properties. It must also copy 
 *  inherited slots and traits. It is also used by the loader to fixup forward class references.
 */
int ejsFixupBlock(Ejs *ejs, EjsBlock *block, EjsBlock *baseBlock, MprList *implements, int makeRoom)
{
    EjsType         *ifaceType;
    EjsBlock        *iface;
    EjsNamespace    *nsp;
    bool            isInstanceBlock;
    int             next, offset, count, nextNsp;

    mprAssert(ejs);
    mprAssert(block);
    mprAssert(block != baseBlock);

    isInstanceBlock = block->obj.var.isInstanceBlock;
    
    if (makeRoom) {
        /*
         *  Count the number of inherited traits and insert
         */
        count = (baseBlock) ? baseBlock->numTraits: 0;
        if (implements) {
            for (next = 0; ((iface = mprGetNextItem(implements, &next)) != 0); ) {
                iface = (isInstanceBlock) ? ((EjsType*) iface)->instanceBlock: iface;
                if (iface) {
                    ifaceType = (EjsType*) iface;
                    if (!ifaceType->isInterface) {
                        /*
                         *  Only inherit properties from implemented classes
                         */
                        count += iface->numTraits - iface->numInherited;
                    }
                }
            }
        }
        if (ejsInsertGrowBlock(ejs, block, count, 0) < 0) {
            return EJS_ERR;
        }
    }

    /*
     *  Copy the inherited traits from the base block and all implemented interfaces
     */
    offset = 0;
    if (baseBlock) {
        if (ejsInheritTraits(ejs, block, baseBlock, baseBlock->numTraits, offset, 0) < 0) {
            return EJS_ERR;
        }
        offset += baseBlock->numTraits;
    }
    
    if (implements) {
        for (next = 0; ((iface = mprGetNextItem(implements, &next)) != 0); ) {
            /*
             *  Only insert the first level of inherited traits
             */
            iface = (isInstanceBlock) ? ((EjsType*) iface)->instanceBlock: iface;
            if (iface) {
                ifaceType = (EjsType*) iface;
                if (!ifaceType->isInterface) {
                    count = iface->numTraits - iface->numInherited;
                    ejsInheritTraits(ejs, block, iface, count, offset, 1);
                    offset += iface->numTraits;
                }
                for (nextNsp = 0; (nsp = (EjsNamespace*) ejsGetNextItem(&iface->namespaces, &nextNsp)) != 0; ) {
                    ejsAddNamespaceToBlock(ejs, block, nsp);
                }
            }
        }
    }
    return 0;
}


/*
 *  Set the native method function for a function property
 */
int ejsBindMethod(Ejs *ejs, EjsType *type, int slotNum, EjsNativeFunction nativeProc)
{
    return ejsBindFunction(ejs, &type->block, slotNum, nativeProc);
}


/*
 *  Set the native method function for a function property
 */
int ejsBindFunction(Ejs *ejs, EjsBlock *block, int slotNum, EjsNativeFunction nativeProc)
{
    EjsFunction     *fun;
    EjsName         qname;

    fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) block, slotNum);

    if (fun == 0 || !ejsIsFunction(fun)) {
        mprAssert(fun);
        ejs->hasError = 1;
        mprError(ejs, "Attempt to bind non-existant function for slot %d in block/type \"%s\"", slotNum, 
            ejsGetDebugName(block));
        return EJS_ERR;
    }

    if (fun->body.code.codeLen != 0) {
        qname = ejsGetPropertyName(ejs, fun->owner, fun->slotNum);
        mprError(ejs, "Setting a native method on a non-native function \"%s\" in block/type \"%s\"", qname.name, 
            ejsGetDebugName(block));
        ejs->hasError = 1;
    }
    fun->body.proc = nativeProc;
    fun->nativeProc = 1;
    return 0;
}


/*
 *  Define a global public function. Returns a positive slot number, otherwise a negative MPR error.
 */
int ejsDefineGlobalFunction(Ejs *ejs, cchar *name, EjsNativeFunction fn)
{
    EjsFunction *fun;
    EjsName     qname;

    if ((fun = ejsCreateFunction(ejs, NULL, -1, 0, 0, ejs->objectType, 0, NULL, NULL, 0)) == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    fun->body.proc = fn;
    fun->nativeProc = 1;
    ejsName(&qname, EJS_PUBLIC_NAMESPACE, name);
    return ejsSetPropertyByName(ejs, ejs->global, &qname, (EjsVar*) fun);
}


/*
 *  Define a property. If then property type is null, then use the value's type if supplied. If no value, then set to the 
 *  void type. If OVERRIDE is not set in attributes, then the slotNum is offset above the base class slots.
 */
int ejsDefineInstanceProperty(Ejs *ejs, EjsType *type, int slotNum, EjsName *name, EjsType *propType, int attributes, 
        EjsVar *value)
{
    return ejsDefineProperty(ejs, (EjsVar*) type->instanceBlock, slotNum, name, propType, attributes, value);
}


/*
 *  Return true if target is an instance of type or a sub class of it.
 */
bool ejsIsA(Ejs *ejs, EjsVar *target, EjsType *type)
{
    EjsType     *tp;

    mprAssert(type);

    if (!ejsIsType(type)) {
        return 0;
    }
    if (target == 0) {
        return 0;
    }
    tp = ejsIsType(target) ? (EjsType*) target : target->type;
    return ejsIsTypeSubType(ejs, tp, type);
}


/*
 *  Return true if "target" is a "type", subclass of "type" or implements "type".
 */
bool ejsIsTypeSubType(Ejs *ejs, EjsType *target, EjsType *type)
{
    EjsType     *tp, *iface;
    int         next;

    mprAssert(target);
    mprAssert(type);
    
    if (!ejsIsType(target) || !ejsIsType(type)) {
        return 0;
    }

    /*
     *  See if target is a subtype of type
     */
    for (tp = target; tp; tp = tp->baseType) {
        /*
         *  Test ID also to allow cloned interpreters to match where the IDs are equal
         */
        if (tp == type || tp->id == type->id) {
            return 1;
        }
    }
    
    /*
     *  See if target implements type
     */
    if (target->implements) {
        for (next = 0; (iface = mprGetNextItem(target->implements, &next)) != 0; ) {
            if (iface == type) {
                return 1;
            }
        }
    }

    return 0;
}


/*
 *  Get the attributes of the type property at slotNum.
 *
 */
int ejsGetTypePropertyAttributes(Ejs *ejs, EjsVar *vp, int slotNum)
{
    EjsType     *type;

    if (!ejsIsType(vp)) {
        mprAssert(ejsIsType(vp));
        return EJS_ERR;
    }
    type = (EjsType*) vp;
    return ejsGetTraitAttributes((EjsBlock*) type, slotNum);
}


/*
 *  This call is currently only used to update the type namespace after resolving a run-time namespace.
 */
void ejsSetTypeName(Ejs *ejs, EjsType *type, EjsName *qname)
{
    type->qname.name = qname->name;
    type->qname.space = qname->space;
    ejsSetDebugName(type, qname->name);
    if (type->instanceBlock) {
        ejsSetDebugName(type->instanceBlock, qname->name);
    }
}


/*
 *  Define namespaces for a class. Inherit the protected and internal namespaces from all base classes.
 */
void ejsDefineTypeNamespaces(Ejs *ejs, EjsType *type)
{
    EjsNamespace        *nsp;

    if (type->baseType) {
        /*
         *  Inherit the base class's protected and internal namespaces
         */
        ejsInheritBaseClassNamespaces(ejs, type, type->baseType);
    }
    nsp = ejsDefineReservedNamespace(ejs, (EjsBlock*) type, &type->qname, EJS_PROTECTED_NAMESPACE);
    nsp->flags |= EJS_NSP_PROTECTED;
    nsp = ejsDefineReservedNamespace(ejs, (EjsBlock*) type, &type->qname, EJS_PRIVATE_NAMESPACE);
    nsp->flags |= EJS_NSP_PRIVATE;
}


void ejsTypeNeedsFixup(Ejs *ejs, EjsType *type)
{
    mprAssert(type);

    type->needFixup = 1;
    type->baseType = 0;
}


/*
 *  Return the total memory size used by a type
 */
static int ejsGetBlockSize(Ejs *ejs, EjsBlock *block)
{
    int     size, numProp;

    numProp = ejsGetPropertyCount(ejs, (EjsVar*) block);

    size = sizeof(EjsType) + sizeof(EjsTypeHelpers) + (numProp * sizeof(EjsVar*));
    if (block->obj.names) {
        size += sizeof(EjsNames) + (block->obj.names->sizeEntries * sizeof(EjsHashEntry));
        size += (block->obj.names->sizeBuckets * sizeof(int*));
    }
    size += ejsGetNumTraits(block) * sizeof(EjsTrait);

    return size;
}


/*
 *  Return the total memory size used by a type
 */
int ejsGetTypeSize(Ejs *ejs, EjsType *type)
{
    int     size;

    size = ejsGetBlockSize(ejs, (EjsBlock*) type);
    if (type->instanceBlock) {
        size += ejsGetBlockSize(ejs, type->instanceBlock);
    }
    return size;
}



void ejsCreateTypeType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;
    int         flags;

    flags = EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_BLOCK_HELPERS;
    if (ejs->flags & EJS_FLAG_EMPTY) {
        flags |= EJS_ATTR_DYNAMIC_INSTANCE;
    }

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Type"), ejs->objectType, sizeof(EjsType), 
        ES_Type, ES_Type_NUM_CLASS_PROP, ES_Type_NUM_INSTANCE_PROP, flags);
    ejs->typeType = type;

    /*
     *  Override the createVar helper when creating types.
     */
    type->helpers->cloneVar     = (EjsCloneVarHelper) cloneTypeVar;
    type->helpers->createVar    = (EjsCreateVarHelper) createTypeVar;
    type->helpers->setProperty  = (EjsSetPropertyHelper) setTypeProperty;
    type->helpers->markVar      = (EjsMarkVarHelper) markType;

    /*
     *  WARNING: read closely. This can be confusing. Fixup the helpers for the object type. We need to find
     *  helpers via objectType->var.type->helpers. So we set it to the Type type. We keep objectType->baseType == 0
     *  because Object has no base type. Similarly for the Type type.
     */
    ejs->objectType->block.obj.var.type = ejs->typeType;
    ejs->typeType->block.obj.var.type = ejs->objectType;
}


void ejsConfigureTypeType(Ejs *ejs)
{
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsType.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/core/ejsVoid.c"
 */
/************************************************************************/

/**
 *  ejsVoid.c - Ejscript Void class (aka undefined)
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Cast the void operand to a primitive type
 */

static EjsVar *castVoid(Ejs *ejs, EjsVoid *vp, EjsType *type)
{
    switch (type->id) {
    case ES_Boolean:
        return (EjsVar*) ejs->falseValue;

    case ES_Number:
        return (EjsVar*) ejs->nanValue;

    case ES_Object:
        return vp;

    case ES_String:
        return (EjsVar*) ejsCreateString(ejs, "undefined");

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
}



static EjsVar *coerceVoidOperands(Ejs *ejs, EjsVoid *lhs, int opcode, EjsVoid *rhs)
{
    switch (opcode) {

    case EJS_OP_ADD:
        if (!ejsIsNumber(rhs)) {
            return ejsInvokeOperator(ejs, (EjsVar*) ejsToString(ejs, lhs), opcode, rhs);
        }
        /* Fall through */

    case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return ejsInvokeOperator(ejs, (EjsVar*) ejs->nanValue, opcode, rhs);

    /*
     *  Comparision
     */
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_COMPARE_NE:
        if (ejsIsNull(rhs)) {
            return (EjsVar*) ejs->falseValue;
        }
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_EQ:
        if (ejsIsNull(rhs)) {
            return (EjsVar*) ejs->trueValue;
        }
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return 0;

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->type->qname.name);
        return ejs->undefinedValue;
    }
    return 0;
}



static EjsVar *invokeVoidOperator(Ejs *ejs, EjsVoid *lhs, int opcode, EjsVoid *rhs)
{
    EjsVar      *result;

    if (rhs == 0 || lhs->type != rhs->type) {
        if ((result = coerceVoidOperands(ejs, lhs, opcode, rhs)) != 0) {
            return result;
        }
    }

    /*
     *  Types match, left and right types are both "undefined"
     */
    switch (opcode) {

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_GE:
    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_NE: case EJS_OP_COMPARE_STRICTLY_NE:
    case EJS_OP_COMPARE_LT: case EJS_OP_COMPARE_GT:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Unary operators
     */
    case EJS_OP_LOGICAL_NOT: case EJS_OP_NOT: case EJS_OP_NEG:
        return (EjsVar*) ejs->nanValue;

    /*
     *  Binary operators
     */
    case EJS_OP_ADD: case EJS_OP_AND: case EJS_OP_DIV: case EJS_OP_MUL: case EJS_OP_OR: case EJS_OP_REM:
    case EJS_OP_SHL: case EJS_OP_SHR: case EJS_OP_SUB: case EJS_OP_USHR: case EJS_OP_XOR:
        return (EjsVar*) ejs->nanValue;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->type->qname.name);
        return 0;
    }

    mprAssert(0);
}


/*
 *  iterator native function get(): Iterator
 */
static EjsVar *getVoidIterator(Ejs *ejs, EjsVar *np, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, np, NULL, 0, NULL);
}


static EjsVar *getVoidProperty(Ejs *ejs, EjsVoid *unused, int slotNum)
{
    ejsThrowReferenceError(ejs, "Object reference is null");
    return 0;
}


/*
 *  We don't actually create any instances. We just use a reference to the undefined singleton instance.
 */
EjsVoid *ejsCreateUndefined(Ejs *ejs)
{
    return (EjsVoid*) ejs->undefinedValue;
}



void ejsCreateVoidType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Void"), ejs->objectType, sizeof(EjsVoid),
        ES_Void, ES_Void_NUM_CLASS_PROP, ES_Void_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE);
    ejs->voidType = type;

    /*
     *  Define the helper functions.
     */
    type->helpers->castVar          = (EjsCastVarHelper) castVoid;
    type->helpers->invokeOperator   = (EjsInvokeOperatorHelper) invokeVoidOperator;
    type->helpers->getProperty      = (EjsGetPropertyHelper) getVoidProperty;

    ejs->undefinedValue = ejsCreateVar(ejs, type, 0);
    ejs->undefinedValue->primitive = 1;
    ejsSetDebugName(ejs->undefinedValue, "undefined");

    if (!(ejs->flags & EJS_FLAG_EMPTY)) {
        /*
         *  Define the "undefined" value
         */
        ejsSetProperty(ejs, ejs->global, ES_undefined, ejs->undefinedValue);
    }
}


void ejsConfigureVoidType(Ejs *ejs)
{
    EjsType     *type;

    type = ejs->voidType;

    ejsSetProperty(ejs, ejs->global, ES_undefined, ejs->undefinedValue);

    ejsBindMethod(ejs, type, ES_Object_get, getVoidIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, getVoidIterator);
}



/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/core/ejsVoid.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/events/ejsTimer.c"
 */
/************************************************************************/

/*
 *  ejsTimer.c -- Timer class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ES_ejs_events_Timer

static void timerCallback(EjsTimer *tp, MprEvent *e);

/*
 *  Create a new timer
 *
 *  function Timer(period: Number, callback: Function, drift: Boolean = true)
 */
static EjsVar *constructor(Ejs *ejs, EjsTimer *tp, int argc, EjsVar **argv)
{
    mprAssert(argc == 2 || argc == 3);
    mprAssert(ejsIsNumber(argv[0]));
    mprAssert(ejsIsFunction(argv[1]));

    tp->ejs = ejs;
    tp->period = ejsGetInt(argv[0]);
    tp->callback = (EjsFunction*) argv[1];
    tp->drift = (argc == 3) ? ejsGetInt(argv[2]) : 1;

    tp->event = mprCreateTimerEvent(ejs->dispatcher, (MprEventProc) timerCallback, tp->period, MPR_NORMAL_PRIORITY, 
        tp, MPR_EVENT_CONTINUOUS);
    if (tp->event == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    return 0;
}


/*
 *  Get the timer drift setting
 *
 *  function get drift(): Boolean
 */
static EjsVar *getDrift(Ejs *ejs, EjsTimer *tp, int argc, EjsVar **argv)
{
    mprAssert(argc == 0);
    return (EjsVar*) ejsCreateBoolean(ejs, tp->drift);
}


/*
 *  Set the timer drift setting
 *
 *  function set drift(period: Boolean): Void
 */
static EjsVar *setDrift(Ejs *ejs, EjsTimer *tp, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsBoolean(argv[0]));
    tp->drift = ejsGetBoolean(argv[0]);
    return 0;
}


/*
 *  Get the timer period
 *
 *  function get period(): Number
 */
static EjsVar *getPeriod(Ejs *ejs, EjsTimer *tp, int argc, EjsVar **argv)
{
    mprAssert(argc == 0);
    return (EjsVar*) ejsCreateNumber(ejs, tp->period);
}


/*
 *  Set the timer period and restart the timer
 *
 *  function set period(period: Number): Void
 */
static EjsVar *setPeriod(Ejs *ejs, EjsTimer *tp, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsNumber(argv[0]));

    tp->period = ejsGetInt(argv[0]);
    mprRescheduleEvent(tp->event, tp->period);
    return 0;
}


/*
 *  Restart a timer
 *
 *  function restart(); Void
 */
static EjsVar *restart(Ejs *ejs, EjsTimer *tp, int argc, EjsVar **argv)
{
    mprAssert(argc == 0);
    mprRestartContinuousEvent(tp->event);
    return 0;
}


/*
 *  Stop a timer
 *
 *  function stop(): Void
 */
static EjsVar *stop(Ejs *ejs, EjsTimer *tp, int argc, EjsVar **argv)
{
    mprAssert(argc == 0);
    mprRemoveEvent(tp->event);
    return 0;
}

#endif

EjsObject *ejsCreateTimerEvent(Ejs *ejs, EjsTimer *tp)
{
    EjsObject       *event;

    event = ejsCreateObject(ejs, ejsGetType(ejs, ES_ejs_events_TimerEvent), 0);
    if (event == 0) {
        return 0;
    }
    ejsSetProperty(ejs, (EjsVar*) event, ES_ejs_events_Event_data, (EjsVar*) tp);
    ejsSetProperty(ejs, (EjsVar*) event, ES_ejs_events_Event_timestamp, (EjsVar*) ejsCreateDate(ejs, 0));
    ejsSetProperty(ejs, (EjsVar*) event, ES_ejs_events_Event_bubbles, (EjsVar*) ejs->falseValue);
    ejsSetProperty(ejs, (EjsVar*) event, ES_ejs_events_Event_priority, (EjsVar*) ejsCreateNumber(ejs, MPR_NORMAL_PRIORITY));
    return event;
}


#if ES_ejs_events_Timer
static void timerCallback(EjsTimer *tp, MprEvent *e)
{
    Ejs         *ejs;
    EjsObject   *event;
    EjsVar      *arg;

    mprAssert(tp);

    ejs = tp->ejs;
    event = ejsCreateTimerEvent(ejs, tp);
    if (event == 0) {
        return;
    }
    arg = (EjsVar*) event;
    ejsRunFunction(tp->ejs, tp->callback, NULL, 1, &arg);
}
#endif


void ejsCreateTimerType(Ejs *ejs)
{
    EjsName     qname;

    ejsCreateCoreType(ejs, ejsName(&qname, "ejs.events", "Timer"), ejs->objectType, sizeof(EjsTimer), 
        ES_ejs_events_Timer, ES_ejs_events_Timer_NUM_CLASS_PROP, ES_ejs_events_Timer_NUM_INSTANCE_PROP, 
        EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_HAS_CONSTRUCTOR | EJS_ATTR_OBJECT_HELPERS);
}


void ejsConfigureTimerType(Ejs *ejs)
{
#if ES_ejs_events_Timer
    EjsType     *type;

    type = ejsGetType(ejs, ES_ejs_events_Timer);

    ejsBindMethod(ejs, type, ES_ejs_events_Timer_Timer, (EjsNativeFunction) constructor);
    ejsBindMethod(ejs, type, ES_ejs_events_Timer_restart, (EjsNativeFunction) restart);
    ejsBindMethod(ejs, type, ES_ejs_events_Timer_stop, (EjsNativeFunction) stop);

    ejsBindMethod(ejs, type, ES_ejs_events_Timer_period, (EjsNativeFunction) getPeriod);
    ejsBindMethod(ejs, type, ES_ejs_events_Timer_set_period, (EjsNativeFunction) setPeriod);
    ejsBindMethod(ejs, type, ES_ejs_events_Timer_drift, (EjsNativeFunction) getDrift);
    ejsBindMethod(ejs, type, ES_ejs_events_Timer_set_drift, (EjsNativeFunction) setDrift);
#endif
    ejs->eventType = (EjsType*) ejsGetProperty(ejs, ejs->global, ES_ejs_events_Event);    
    ejs->errorEventType = (EjsType*) ejsGetProperty(ejs, ejs->global, ES_ejs_events_ErrorEvent);
}

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/events/ejsTimer.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/io/ejsFile.c"
 */
/************************************************************************/

/**
 *  ejsFile.c - File class.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




#if BLD_WIN_LIKE
#define isDelim(fp, c)  (c == '/' || c == fp->delimiter)
#else
#define isDelim(fp, c)  (c == fp->delimiter)
#endif

#define FILE_OPEN           0x1
#define FILE_READ           0x2
#define FILE_WRITE          0x4


static int mapMode(cchar *mode);
static EjsVar *openFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv);
static int  readData(Ejs *ejs, EjsFile *fp, EjsByteArray *ap, int offset, int count);


static void destroyFile(Ejs *ejs, EjsFile *fp)
{
    mprAssert(fp);

    mprFree(fp->path);
    fp->path = 0;
    ejsFreeVar(ejs, (EjsVar*) fp, -1);
}


/*
 *  Index into a file and extract a byte. This is random access reading.
 */
static EjsVar *getFileProperty(Ejs *ejs, EjsFile *fp, int slotNum)
{
    int     c, offset;

    if (!(fp->mode & FILE_OPEN)) {
        ejsThrowIOError(ejs, "File is not open");
        return 0;
    }
#if KEEP
    if (fp->mode & FILE_READ) {
        if (slotNum >= fp->info.size) {
            ejsThrowOutOfBoundsError(ejs, "Bad file index");
            return 0;
        }
    }
    if (slotNum < 0) {
        ejsThrowOutOfBoundsError(ejs, "Bad file index");
        return 0;
    }
#endif

    offset = mprSeek(fp->file, SEEK_CUR, 0);
    if (offset != slotNum) {
        if (mprSeek(fp->file, SEEK_SET, slotNum) != slotNum) {
            ejsThrowIOError(ejs, "Can't seek to file offset");
            return 0;
        }
    }
    c = mprPeekc(fp->file);
    if (c < 0) {
        ejsThrowIOError(ejs, "Can't read file");
        return 0;
    }
    return (EjsVar*) ejsCreateNumber(ejs, c);
}


/*
 *  Set a byte in the file at the offset designated by slotNum.
 */
static int setFileProperty(Ejs *ejs, EjsFile *fp, int slotNum, EjsVar *value)
{
    int     c, offset;

    if (!(fp->mode & FILE_OPEN)) {
        ejsThrowIOError(ejs, "File is not open");
        return 0;
    }
    if (!(fp->mode & FILE_WRITE)) {
        ejsThrowIOError(ejs, "File is not opened for writing");
        return 0;
    }
    c = ejsIsNumber(value) ? ejsGetInt(value) : ejsGetInt(ejsToNumber(ejs, value));

    offset = mprSeek(fp->file, SEEK_CUR, 0);
    if (slotNum < 0) {
        //  could have an mprGetPosition(file) API
        slotNum = offset;
    }

    if (offset != slotNum && mprSeek(fp->file, SEEK_SET, slotNum) != slotNum) {
        ejsThrowIOError(ejs, "Can't seek to file offset");
        return 0;
    }
    if (mprPutc(fp->file, c) < 0) {
        ejsThrowIOError(ejs, "Can't write file");
        return 0;
    }
    return slotNum;
}



int ejsGetNumOption(Ejs *ejs, EjsVar *options, cchar *field, int defaultValue, bool optional)
{
    EjsVar      *vp;
    EjsName     qname;
    EjsNumber   *num;

    if (!ejsIsObject(options)) {
        if (!ejs->exception) {
            ejsThrowArgError(ejs, "Bad args. Options not an object");
        }
        return 0;
    }
    vp = ejsGetPropertyByName(ejs, options, ejsName(&qname, "", field));
    if (vp == 0) {
        if (optional) {
            return defaultValue;
        }
        ejsThrowArgError(ejs, "Required option \"%s\" is missing", field);
        return 0;
    }
    num = ejsToNumber(ejs, vp);
    if (!ejsIsNumber(num)) {
        ejsThrowArgError(ejs, "Bad option type for field \"%s\"", field);
        return 0;
    }
    return (int) num->value;
}


cchar *ejsGetStrOption(Ejs *ejs, EjsVar *options, cchar *field, cchar *defaultValue, bool optional)
{
    EjsVar      *vp;
    EjsName     qname;
    EjsString   *str;

    if (!ejsIsObject(options)) {
        if (!ejs->exception) {
            ejsThrowArgError(ejs, "Bad args. Options not an object");
        }
        return 0;
    }
    vp = ejsGetPropertyByName(ejs, options, ejsName(&qname, "", field));
    if (vp == 0) {
        if (optional) {
            return defaultValue;
        }
        ejsThrowArgError(ejs, "Required option %s is missing", field);
        return 0;
    }
    str = ejsToString(ejs, vp);
    if (!ejsIsString(str)) {
        ejsThrowArgError(ejs, "Bad option type for field \"%s\"", field);
        return 0;
    }
    return str->value;
}


/*
 *  Constructor
 *
 *  function File(path: Object, options: Object = null)
 */
static EjsVar *fileConstructor(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    char    *path;

    if (argc < 1 || argc > 2) {
        ejsThrowArgError(ejs, "Bad args");
        return 0;
    }
    if (ejsIsPath(argv[0])) {
        path = ((EjsPath*) argv[0])->path;
    } else if (ejsIsString(argv[0])) {
        path = ejsGetString(argv[0]);
    } else {
        ejsThrowIOError(ejs, "Bad path");
        return NULL;
    }
    fp->path = mprGetNormalizedPath(fp, path);
    if (argc == 2) {
        openFile(ejs, fp, 1, &argv[1]);
    }
    return (EjsVar*) fp;
}


/*
 *  function get canRead(): Boolean
 */
static EjsVar *canReadFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, fp->mode & FILE_OPEN && (fp->mode & FILE_READ));
}


/*
 *  function get canRead(): Boolean
 */
static EjsVar *canWriteFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, fp->mode & FILE_OPEN && (fp->mode & FILE_WRITE));
}

/*
 *  Close the file and free up all associated resources.
 *
 *  function close(graceful: Boolean): void
 */
static EjsVar *closeFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    if (fp->mode & FILE_OPEN && fp->mode & FILE_WRITE) {
        if (mprFlush(fp->file) < 0) {
            ejsThrowIOError(ejs, "Can't flush file data");
            return 0;
        }
    }

    if (fp->file) {
        mprFree(fp->file);
        fp->file = 0;
    }
    fp->mode = 0;
    mprFree(fp->modeString);
    fp->modeString = 0;
    return 0;
}


/*
 *  Flush the stream. This is a noop as all I/O is unbuffered.
 *
 *  function flush(): void
 */
static EjsVar *flushFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
#if KEEP
    if (!(fp->mode & FILE_OPEN)) {
        ejsThrowIOError(ejs, "File is not open");
        return 0;
    }
    if (!(fp->mode & FILE_WRITE)) {
        ejsThrowIOError(ejs, "File is not opened for writing");
        return 0;
    }
    if (mprFlush(fp->file) < 0) {
        ejsThrowIOError(ejs, "Can't flush file data");
        return 0;
    }
#endif
    return 0;
}


/*
 *  Function to iterate and return the next element index.
 *  NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextKey(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsFile     *fp;

    fp = (EjsFile*) ip->target;
    if (!ejsIsFile(fp)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    if (ip->index < fp->info.size) {
        return (EjsVar*) ejsCreateNumber(ejs, ip->index++);
    }

    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return the default iterator for use with "for ... in". This returns byte offsets in the file.
 *
 *  iterator native function get(): Iterator
 */
static EjsVar *getFileIterator(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    mprGetPathInfo(ejs, fp->path, &fp->info);
    return (EjsVar*) ejsCreateIterator(ejs, (EjsVar*) fp, (EjsNativeFunction) nextKey, 0, NULL);
}


/*
 *  Function to iterate and return the next element value.
 *  NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextValue(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsFile     *fp;

    fp = (EjsFile*) ip->target;
    if (!ejsIsFile(fp)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    if (ip->index < fp->info.size) {
#if !BLD_CC_MMU || 1
        if (mprSeek(fp->file, SEEK_CUR, 0) != ip->index) {
            if (mprSeek(fp->file, SEEK_SET, ip->index) != ip->index) {
                ejsThrowIOError(ejs, "Can't seek to %d", ip->index);
                return 0;
            }
        }
        ip->index++;
        return (EjsVar*) ejsCreateNumber(ejs, mprGetc(fp->file));
#else
        return (EjsVar*) ejsCreateNumber(ejs, fp->mapped[ip->index++]);
#endif
    }

    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return an iterator to enumerate the bytes in the file. For use with "for each ..."
 *
 *  iterator native function getValues(): Iterator
 */
static EjsVar *getFileValues(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    mprGetPathInfo(ejs, fp->path, &fp->info);

    return (EjsVar*) ejsCreateIterator(ejs, (EjsVar*) fp, (EjsNativeFunction) nextValue, 0, NULL);
}


/*
 *  Get a path object for the file
 *
 *  function get path(): Path
 */
static EjsVar *getFilePath(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePath(ejs, fp->path);
}


/*
 *  Get the current I/O position in the file.
 *
 *  function get position(): Number
 */
static EjsVar *getFilePosition(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    if (fp->file == 0) {
        ejsThrowStateError(ejs, "File not opened");
        return 0;
    }
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) mprGetFilePosition(fp->file));
}


/*
 *  Seek to a new location in the file and set the File marker to a new read/write position.
 *
 *  function set position(value: Number): void
 */
static EjsVar *setFilePosition(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    long        pos;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));
    pos = ejsGetInt(argv[0]);

    if (fp->file == 0) {
        ejsThrowStateError(ejs, "File not opened");
        return 0;
    }
    pos = ejsGetInt(argv[0]);
    if (mprSeek(fp->file, SEEK_SET, pos) != pos) {
        ejsThrowIOError(ejs, "Can't seek to %ld", pos);
    }
    return 0;
}


/*
 *  function get isOpen(): Boolean
 */
static EjsVar *isFileOpen(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, fp->mode & FILE_OPEN);
}


/*
 *  Constructor
 *
 *  function open(options: Object = null): File
 *
 *  NOTE: options can be an options hash or as mode string
 */
static EjsVar *openFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    EjsVar  *options;
    cchar   *mode;
    int     perms, omode;

    if (argc < 0 || argc > 1) {
        ejsThrowArgError(ejs, "Bad args");
        return 0;
    }
    options = argv[0];
    if (argc == 0 || ejsIsNull(options) || ejsIsUndefined(options)) {
        omode = O_RDONLY | O_BINARY;
        perms = EJS_FILE_PERMS;
        fp->mode = FILE_READ;
        mode = "r";
    } else {
        if (ejsIsString(options)) {
            mode = ejsGetString(options);
            perms = EJS_FILE_PERMS;
        } else {
            perms = ejsGetNumOption(ejs, options, "permissions", EJS_FILE_PERMS, 1);
            mode = ejsGetStrOption(ejs, options, "mode", "r", 1);
            if (ejs->exception) {
                return 0;
            }
        }
        omode = mapMode(mode);
        if (!(omode & O_WRONLY)) {
            fp->mode |= FILE_READ;
        }
        if (omode & (O_WRONLY | O_RDWR)) {
            fp->mode |= FILE_WRITE;
        }
    }

    if (fp->file) {
        mprFree(fp->file);
    }
    fp->modeString = mprStrdup(fp, mode);
    fp->perms = perms;

    fp->file = mprOpen(fp, fp->path, omode, perms);
    if (fp->file == 0) {
        ejsThrowIOError(ejs, "Can't open %s", fp->path);
        return 0;
    }

    fp->mode |= FILE_OPEN;
    return (EjsVar*) fp;
}


static EjsVar *getFileOptions(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    EjsName     qname;
    EjsVar      *options;

    options = (EjsVar*) ejsCreateSimpleObject(ejs);
    ejsSetPropertyByName(ejs, options, ejsName(&qname, "", "mode"), (EjsVar*) ejsCreateString(ejs, fp->modeString));
    ejsSetPropertyByName(ejs, options, ejsName(&qname, "", "permissions"), (EjsVar*) ejsCreateNumber(ejs, fp->perms));
    return options;
}

/*
 *  Read data bytes from a file
 *
 *  function readBytes(count: Number = -1): ByteArray
 */
static EjsVar *readFileBytes(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    EjsByteArray    *result;
    MprPath         info;
    int             count, totalRead;

    if (argc == 0) {
        count = -1;
    } else if (argc != 1) {
        count = 0;
        ejsThrowArgError(ejs, "Bad args");
        return 0;
    } else {
        mprAssert(argc == 1 && ejsIsNumber(argv[0]));
        count = ejsGetInt(argv[0]);
    }

    if (fp->file == 0) {
        ejsThrowStateError(ejs, "File not open");
        return 0;
    }

    if (!(fp->mode & FILE_READ)) {
        ejsThrowStateError(ejs, "File not opened for reading");
        return 0;
    }
    if (count < 0) {
        if (mprGetPathInfo(fp, fp->path, &info) == 0) {
            count = (int) info.size;
            count -= (int) mprGetFilePosition(fp->file);
        } else {
            count = MPR_BUFSIZE;
        }
        mprAssert(count >= 0);
    }
    result = ejsCreateByteArray(ejs, count);
    if (result == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    totalRead = readData(ejs, fp, result, 0, count);
    if (totalRead < 0) {
        ejsThrowIOError(ejs, "Can't read from file: %s", fp->path);
        return 0;
    } else if (totalRead == 0) {
        return ejs->nullValue;
    }
    ejsSetByteArrayPositions(ejs, result, 0, totalRead);

    return (EjsVar*) result;
}


/*
 *  Read data as a string
 *
 *  function readString(count: Number = -1): String
 */
static EjsVar *readFileString(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    EjsString       *result;
    MprPath         info;
    int             count, totalRead;

    if (argc == 0) {
        count = -1;
    } else if (argc != 1) {
        count = 0;
        ejsThrowArgError(ejs, "Bad args");
        return 0;
    } else {
        mprAssert(argc == 1 && ejsIsNumber(argv[0]));
        count = ejsGetInt(argv[0]);
    }

    if (fp->file == 0) {
        ejsThrowStateError(ejs, "File not open");
        return 0;
    }

    if (!(fp->mode & FILE_READ)) {
        ejsThrowStateError(ejs, "File not opened for reading");
        return 0;
    }

    if (count < 0) {
        if (mprGetPathInfo(fp, fp->path, &info) == 0) {
            count = (int) info.size;
            count -= (int) mprGetFilePosition(fp->file);
        } else {
            count = MPR_BUFSIZE;
        }
        mprAssert(count >= 0);
    }
    result = ejsCreateBareString(ejs, count);
    if (result == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    totalRead = mprRead(fp->file, result->value, count);
    if (totalRead != count) {
        ejsThrowIOError(ejs, "Can't read from file: %s", fp->path);
        return 0;
    }
    return (EjsVar*) result;
}


/*
 *  Read data bytes from a file. If offset is < 0, then append to the write position.
 *
 *  function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number
 */
static EjsVar *readFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    EjsByteArray    *buffer;
    MprPath         info;
    int             count, offset, totalRead;

    mprAssert(1 <= argc && argc <= 3);

    buffer = (EjsByteArray*) argv[0];
    offset = (argc >= 2) ? ejsGetInt(argv[1]): 0;
    count = (argc >= 3) ? ejsGetInt(argv[2]): -1;

    if (offset >= buffer->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad read offset value");
        return 0;
    }
    if (offset < 0) {
        offset = buffer->writePosition;
    } else if (offset == 0) {
        ejsSetByteArrayPositions(ejs, buffer, 0, 0);
    }
    if (count < 0) {
        if (mprGetPathInfo(fp, fp->path, &info) == 0) {
            count = (int) info.size;
            count -= (int) mprGetFilePosition(fp->file);
        } else {
            count = MPR_BUFSIZE;
        }
        mprAssert(count >= 0);
    }
    if (fp->file == 0) {
        ejsThrowStateError(ejs, "File not open");
        return 0;
    }
    if (!(fp->mode & FILE_READ)) {
        ejsThrowStateError(ejs, "File not opened for reading");
        return 0;
    }
    totalRead = readData(ejs, fp, buffer, offset, count);
    if (totalRead < 0) {
        return 0;
    } else if (totalRead == 0) {
        return ejs->nullValue;
    }
    ejsSetByteArrayPositions(ejs, buffer, -1, offset + totalRead);
    return (EjsVar*) ejsCreateNumber(ejs, totalRead);
}


/*
 *  Get the size of the file associated with this File object.
 *
 *  override intrinsic function get size(): Number
 */
static EjsVar *getFileSize(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    MprPath     info;

    if (mprGetPathInfo(ejs, fp->path, &info) < 0) {
        return (EjsVar*) ejs->minusOneValue;
    }
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) info.size);
}


/*
 *  function truncate(size: Number): Void
 */
EjsVar *truncateFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    int     size;

    size = ejsGetInt(argv[0]);
    if (mprTruncatePath(ejs, fp->path, size) < 0) {
        ejsThrowIOError(ejs, "Cant truncate %s", fp->path);
    }
    return 0;
}


/*
 *  Write data to the file
 *
 *  function write(data: Object): Number
 */
EjsVar *writeFile(Ejs *ejs, EjsFile *fp, int argc, EjsVar **argv)
{
    EjsArray        *args;
    EjsByteArray    *ap;
    EjsVar          *vp;
    EjsString       *str;
    char            *buf;
    int             i, len, written;

    mprAssert(argc == 1 && ejsIsArray(argv[0]));

    args = (EjsArray*) argv[0];

    if (!(fp->mode & FILE_WRITE)) {
        ejsThrowStateError(ejs, "File not opened for writing");
        return 0;
    }

    written = 0;

    for (i = 0; i < args->length; i++) {
        vp = ejsGetProperty(ejs, (EjsVar*) args, i);
        mprAssert(vp);
        switch (vp->type->id) {
        case ES_ByteArray:
            ap = (EjsByteArray*) vp;
            buf = (char*) &ap->value[ap->readPosition];
            len = ap->writePosition - ap->readPosition;
            break;

        case ES_String:
            buf = ((EjsString*) vp)->value;
            len = ((EjsString*) vp)->length;
            break;

        default:
            str = ejsToString(ejs, vp);
            buf = ejsGetString(str);
            len = str->length;
            break;
        }
        if (mprWrite(fp->file, buf, len) != len) {
            ejsThrowIOError(ejs, "Can't write to %s", fp->path);
            return 0;
        }
        written += len;
    }
    return (EjsVar*) ejsCreateNumber(ejs, written);
}



static int readData(Ejs *ejs, EjsFile *fp, EjsByteArray *ap, int offset, int count)
{
    int     len, bytes;

    if (count <= 0) {
        return 0;
    }
    len = ap->length - offset;
    len = min(count, len);
    bytes = mprRead(fp->file, &ap->value[offset], len);
    if (bytes < 0) {
        ejsThrowIOError(ejs, "Error reading from %s", fp->path);
    }
    return bytes;
}


static int mapMode(cchar *mode)
{
    int     omode;

    omode = O_BINARY;
    if (strchr(mode, 'r')) {
        omode |= O_RDONLY;
    }
    if (strchr(mode, 'w')) {
        if (omode & O_RDONLY) {
            omode &= ~O_RDONLY;
            omode |= O_RDWR;
        } else {
            omode |= O_CREAT | O_WRONLY | O_TRUNC;
        }
    }
    if (strchr(mode, 'a')) {
        omode |= O_WRONLY | O_APPEND;
    }
    if (strchr(mode, '+')) {
        omode &= ~O_TRUNC;
    }
    if (strchr(mode, 't')) {
        omode &= ~O_BINARY;
    }
    return omode;
}



EjsFile *ejsCreateFile(Ejs *ejs, cchar *path)
{
    EjsFile     *fp;
    EjsVar      *arg;

    mprAssert(path && *path);

    fp = (EjsFile*) ejsCreateVar(ejs, ejsGetType(ejs, ES_ejs_io_File), 0);
    if (fp == 0) {
        return 0;
    }
    arg = (EjsVar*) ejsCreateString(ejs, path);
    fileConstructor(ejs, fp, 1, (EjsVar**) &arg);
    return fp;
}


EjsFile *ejsCreateFileFromFd(Ejs *ejs, int fd, cchar *name, int mode)
{
    EjsFile     *fp;

    mprAssert(fd >= 0);
    mprAssert(name);

    fp = (EjsFile*) ejsCreateVar(ejs, ejsGetType(ejs, ES_ejs_io_File), 0);
    if (fp == 0) {
        return 0;
    }

    fp->perms = EJS_FILE_PERMS;
    fp->mode = FILE_OPEN;
    if (!(mode & O_WRONLY)) {
        fp->mode |= FILE_READ;
    }
    if (mode & (O_WRONLY | O_RDWR)) {
        fp->mode |= FILE_WRITE;
    }
    fp->file = mprAttachFd(fp, fd, name, mode);
    if (fp->file == 0) {
        return 0;
    }
    return fp;
}


void ejsCreateFileType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, "ejs.io", "File"), ejs->objectType, sizeof(EjsFile), ES_ejs_io_File,
        ES_ejs_io_File_NUM_CLASS_PROP, ES_ejs_io_File_NUM_INSTANCE_PROP, 
        EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_HAS_CONSTRUCTOR | EJS_ATTR_OBJECT_HELPERS);
    if (type == 0) {
        return;
    }

    /*
     *  Define the helper functions.
     */
    type->helpers->destroyVar = (EjsDestroyVarHelper) destroyFile;
    type->helpers->getProperty = (EjsGetPropertyHelper) getFileProperty;
    type->helpers->setProperty = (EjsSetPropertyHelper) setFileProperty;
    type->numericIndicies = 1;
}


void ejsConfigureFileType(Ejs *ejs)
{
    EjsType     *type;

    if ((type = ejsGetType(ejs, ES_ejs_io_File)) == 0) {
        return;
    }
    mprAssert(type);

    ejsBindMethod(ejs, type, ES_ejs_io_File_File, (EjsNativeFunction) fileConstructor);
    ejsBindMethod(ejs, type, ES_ejs_io_File_canRead, (EjsNativeFunction) canReadFile);
    ejsBindMethod(ejs, type, ES_ejs_io_File_canWrite, (EjsNativeFunction) canWriteFile);
    ejsBindMethod(ejs, type, ES_ejs_io_File_close, (EjsNativeFunction) closeFile);
    ejsBindMethod(ejs, type, ES_ejs_io_File_flush, (EjsNativeFunction) flushFile);
    ejsBindMethod(ejs, type, ES_Object_get, (EjsNativeFunction) getFileIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, (EjsNativeFunction) getFileValues);
    ejsBindMethod(ejs, type, ES_ejs_io_File_isOpen, (EjsNativeFunction) isFileOpen);
    ejsBindMethod(ejs, type, ES_ejs_io_File_open, (EjsNativeFunction) openFile);
    ejsBindMethod(ejs, type, ES_ejs_io_File_options, (EjsNativeFunction) getFileOptions);
    ejsBindMethod(ejs, type, ES_ejs_io_File_path, (EjsNativeFunction) getFilePath);
    ejsBindMethod(ejs, type, ES_ejs_io_File_position, (EjsNativeFunction) getFilePosition);
    ejsBindMethod(ejs, type, ES_ejs_io_File_set_position, (EjsNativeFunction) setFilePosition);
    ejsBindMethod(ejs, type, ES_ejs_io_File_readBytes, (EjsNativeFunction) readFileBytes);
    ejsBindMethod(ejs, type, ES_ejs_io_File_readString, (EjsNativeFunction) readFileString);
    ejsBindMethod(ejs, type, ES_ejs_io_File_read, (EjsNativeFunction) readFile);
    ejsBindMethod(ejs, type, ES_ejs_io_File_size, (EjsNativeFunction) getFileSize);
    ejsBindMethod(ejs, type, ES_ejs_io_File_truncate, (EjsNativeFunction) truncateFile);
    ejsBindMethod(ejs, type, ES_ejs_io_File_write, (EjsNativeFunction) writeFile);
}

#if 0 && !BLD_FEATURE_ROMFS
void ejsCreateFileType(Ejs *ejs) {}
void ejsConfigureFileType(Ejs *ejs) {}
#endif

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/io/ejsFile.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/io/ejsFileSystem.c"
 */
/************************************************************************/

/**
 *  ejsFileSystem.c - FileSystem class.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ES_ejs_io_FileSystem
/*
 *  Constructor
 *
 *  function FileSystem(path: String)
 */
static EjsVar *fileSystemConstructor(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    char    *path;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    path = ejsGetString(argv[0]);
    fp->path = mprGetNormalizedPath(fp, path);
    fp->fs = mprLookupFileSystem(ejs, path);
    return (EjsVar*) fp;
}


#if ES_ejs_io_space
/*
 *  Return the amount of free space in the file system that would contain the given path.
 *  function freeSpace(path: String = null): Number
 */
static EjsVar *fileSystemSpace(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
#if BREW
    Mpr     *mpr;
    uint    space;

    mpr = mprGetMpr(ejs);
    space = IFILEMGR_GetFreeSpace(mpr->fileMgr, 0);
    ejsSetReturnValueToInteger(ejs, space);
#endif
    return 0;
}
#endif


/*
 *  Determine if the file system has a drive specs (C:) in paths
 *
 *  static function hasDrives(): Boolean
 */
static EjsVar *hasDrives(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, fp->fs->hasDriveSpecs);
}


#if ES_ejs_io_isReady
/*
 *  Determine if the file system is ready for I/O
 *
 *  function get isReady(): Boolean
 */
static EjsVar *isReady(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    MprPath     info;
    int         rc;

    rc = mprGetPathInfo(ejs, fp->path, &info);
    return (EjsVar*) ejsCreateBoolean(ejs, rc == 0 && info.isDir);
}
#endif


#if ES_ejs_io_isWritable
static EjsVar *isWritable(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
{
    MprPath     info;
    int         rc;

    rc = mprGetPathInfo(ejs, fp->path, &info);
    return (EjsVar*) ejsCreateBoolean(ejs, rc == 0 && info.isDir);
}
#endif


/*
 *  Get the newline characters
 *
 *  function get newline(): String
 */
static EjsVar *getNewline(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, mprGetPathNewline(ejs, fp->path));
}


/*
 *  set the newline characters
 *
 *  function set newline(terminator: String): Void
 */
static EjsVar *setNewline(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    mprAssert(ejsIsString(argv[0]));
    mprSetPathNewline(ejs, fp->path, ((EjsString*) argv[0])->value);
    return 0;
}


static EjsVar *root(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    char    *path, *cp;
    int     sep;

    sep = mprGetPathSeparator(fp, fp->path);
    path = mprGetAbsPath(ejs, fp->path);
    if ((cp = strchr(path, sep)) != 0) {
        *++cp = '\0';
    }
    return (EjsObj*) ejsCreatePathAndFree(ejs, path);
}


/*
 *  Return the path directory separators
 *
 *  static function get separators(): String
 */
static EjsVar *getSeparators(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, fp->fs->separators);
}


/*
 *  Set the path directory separators
 *
 *  static function set separators(value: String): void
 */
static EjsVar *setSeparators(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsString(argv[0]));
    mprSetPathSeparators(ejs, fp->path, ejsGetString(argv[0]));
    return 0;
}


#if ES_ejs_io_size
static EjsVar *size(Ejs *ejs, EjsFileSystem *fp, int argc, EjsVar **argv)
{
    return 0;
}
#endif


EjsFileSystem *ejsCreateFileSystem(Ejs *ejs, cchar *path)
{
    EjsFileSystem   *fs;
    EjsVar          *arg;

    fs = (EjsFileSystem*) ejsCreateVar(ejs, ejsGetType(ejs, ES_ejs_io_FileSystem), 0);
    if (fs == 0) {
        return 0;
    }

    arg = (EjsVar*) ejsCreateString(ejs, path);
    fileSystemConstructor(ejs, fs, 1, (EjsVar**) &arg);
    return fs;
}


void ejsCreateFileSystemType(Ejs *ejs)
{
    EjsName     qname;

    ejsCreateCoreType(ejs, ejsName(&qname, "ejs.io", "FileSystem"), ejs->objectType, sizeof(EjsFileSystem), 
        ES_ejs_io_FileSystem, ES_ejs_io_FileSystem_NUM_CLASS_PROP, ES_ejs_io_FileSystem_NUM_INSTANCE_PROP, 
        EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_HAS_CONSTRUCTOR | EJS_ATTR_OBJECT_HELPERS);
}


void ejsConfigureFileSystemType(Ejs *ejs)
{
    EjsType     *type;

    if ((type = ejsGetType(ejs, ES_ejs_io_FileSystem)) == 0) {
        return;
    }

    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_FileSystem, (EjsNativeFunction) fileSystemConstructor);
#if ES_ejs_io_space
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_space, (EjsNativeFunction) fileSystemSpace);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_hasDrives, (EjsNativeFunction) hasDrives);
#if ES_ejs_io_isReady
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_isReady, (EjsNativeFunction) isReady);
#endif
#if ES_ejs_io_isWritable
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_isWritable, (EjsNativeFunction) isWritable);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_newline, (EjsNativeFunction) getNewline);
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_set_newline, (EjsNativeFunction) setNewline);
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_root, (EjsNativeFunction) root);
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_separators, (EjsNativeFunction) getSeparators);
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_set_separators, (EjsNativeFunction) setSeparators);
#if ES_ejs_io_size
    ejsBindMethod(ejs, type, ES_ejs_io_FileSystem_size, (EjsNativeFunction) size);
#endif
}

#endif /* ES_FileSystem */
/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/io/ejsFileSystem.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/io/ejsHttp.c"
 */
/************************************************************************/

/**
 *  ejsHttp.c - Http class. This implements a HTTP client.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ES_ejs_io_Http && BLD_FEATURE_HTTP_CLIENT

static EjsVar   *getDateHeader(Ejs *ejs, EjsHttp *hp, cchar *key);
static EjsVar   *getStringHeader(Ejs *ejs, EjsHttp *hp, cchar *key);
static void     httpCallback(EjsHttp *hp, int mask);
static void     prepForm(Ejs *ejs, EjsHttp *hp, char *prefix, EjsVar *data);
static char     *prepUri(MprCtx ctx, cchar *uri);
static EjsVar   *startRequest(Ejs *ejs, EjsHttp *hp, char *method, int argc, EjsVar **argv);
static bool     waitForResponse(EjsHttp *hp, int timeout);
static bool     waitForState(EjsHttp *hp, int state, int timeout, int throw);

/*
 *  Constructor
 *
 *  function Http(uri: String = null)
 */
static EjsVar *httpConstructor(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    hp->ejs = ejs;
    hp->http = mprCreateHttp(hp);
    if (hp->http == 0) {
        ejsThrowMemoryError(ejs);
    }
    mprAssert(hp->http->sock == 0);
    mprAssert(hp->http->state == MPR_HTTP_STATE_BEGIN);
    mprAssert(hp->http->request->chunked == -1);

    if (argc == 1 && argv[0] != ejs->nullValue) {
        hp->uri = prepUri(hp, ejsGetString(argv[0]));
    }
    hp->method = mprStrdup(hp, "GET");
    hp->responseContent = mprCreateBuf(hp, MPR_HTTP_BUFSIZE, -1);
#if BLD_FEATURE_MULTITHREAD
    hp->mutex = mprCreateLock(hp);
#endif
    return (EjsVar*) hp;
}


/*
 *  function addHeader(key: String, value: String, overwrite: Boolean = true): Void
 */
EjsVar *addHeader(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    char    *key, *value;
    bool    overwrite;

    mprAssert(argc >= 2);

    key = ejsGetString(argv[0]);
    value = ejsGetString(argv[1]);
    overwrite = (argc == 3) ? ejsGetBoolean(argv[2]) : 1;
    mprSetHttpHeader(hp->http, overwrite, key, value);
    return 0;
}


/*
 *  function get available(): Number
 */
EjsVar *httpAvailable(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    int     len;

    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    len = mprGetHttpContentLength(hp->http);
    if (len > 0) {
        return (EjsVar*) ejsCreateNumber(ejs, len);
    }
    return (EjsVar*) ejs->minusOneValue;
}


/*
 *  function setCallback(mask: Number cb: Function): Void
 */
EjsVar *setHttpCallback(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    int     mask;

    mprAssert(argc == 1 || argc == 2);

    mask = ejsGetInt(argv[0]);
    if (mask & ~(MPR_READABLE | MPR_WRITABLE)) {
        ejsThrowStateError(ejs, "Bad callback event mask:");
        return 0;
    }
    hp->callback = (EjsFunction*) argv[1];
    mprSetHttpCallback(hp->http, (MprHttpProc) httpCallback, hp, mask);
    return 0;
}


/*
 *  function close(graceful: Boolean = true): Void
 */
static EjsVar *closeHttp(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (hp->http) {
        mprFree(hp->http);
        hp->http = mprCreateHttp(hp);
    }
    return 0;
}


/*
 *  function connect(url = null, data ...): Void
 */
static EjsVar *connectHttp(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return startRequest(ejs, hp, NULL, argc, argv);
}


/**
 *  function get certificate(): String
 */
static EjsVar *getCertificate(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (hp->certFile) {
        return (EjsVar*) ejsCreateString(ejs, hp->certFile);
    }
    return ejs->nullValue;
}


/*
 *  function set setCertificate(value: String): Void
 */
static EjsVar *setCertificate(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    mprFree(hp->certFile);
    hp->certFile = mprStrdup(hp, ejsGetString(argv[0]));
    return 0;
}


#if ES_ejs_io_Http_chunked
/**
 *  function get chunked(): Boolean
 */
static EjsVar *getChunked(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    int     chunked;

    chunked = mprGetHttpChunked(hp->http);
    if (chunked == -1) {
        return ejs->undefinedValue;
    } else if (chunked == 0) {
        return (EjsVar*) ejs->falseValue;
    } else {
        return (EjsVar*) ejs->trueValue;
    }
}
#endif


#if ES_ejs_io_Http_set_chunked
/**
 *  function set chunked(value: Boolean): Void
 */
static EjsVar *setChunked(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    mprAssert(hp->http);
    mprAssert(hp->http->state == MPR_HTTP_STATE_BEGIN || hp->http->state == MPR_HTTP_STATE_COMPLETE);
    if (!(hp->http->state == MPR_HTTP_STATE_BEGIN || hp->http->state == MPR_HTTP_STATE_COMPLETE)) {
        printf("STATE IS %d\n", hp->http->state);
    }

    if (mprSetHttpChunked(hp->http, ejsGetBoolean(argv[0])) < 0) {
        ejsThrowStateError(ejs, "Can't change chunked setting in this state. Request has already started.");
    }
    return 0;
}
#endif


/*
 *  function get code(): Number
 */
static EjsVar *code(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    return (EjsVar*) ejsCreateNumber(ejs, mprGetHttpCode(hp->http));
}


/*
 *  function get codeString(): String
 */
static EjsVar *codeString(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    return (EjsVar*) ejsCreateString(ejs, mprGetHttpMessage(hp->http));
}


/*
 *  function get contentEncoding(): String
 */
static EjsVar *contentEncoding(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return getStringHeader(ejs, hp, "CONTENT-ENCODING");
}


/*
 *  function get contentLength(): Number
 */
static EjsVar *contentLength(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    int     length;

    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    length = mprGetHttpContentLength(hp->http);
    return (EjsVar*) ejsCreateNumber(ejs, length);
}


#if ES_ejs_io_Http_set_contentLength
/*
 *  function set contentLength(length: Number): Void
 */
static EjsVar *setContentLength(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    hp->contentLength = (int) ejsGetNumber(argv[0]);
    return 0;
}
#endif


/*
 *  function get contentType(): String
 */
static EjsVar *contentType(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    return getStringHeader(ejs, hp, "CONTENT-TYPE");
}


/**
 *  function get date(): Date
 */
static EjsVar *date(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return getDateHeader(ejs, hp, "DATE");
}


/*
 *  function del(uri: String = null): Void
 */
static EjsVar *del(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return startRequest(ejs, hp, "DELETE", argc, argv);
}


/**
 *  function get expires(): Date
 */
static EjsVar *expires(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return getDateHeader(ejs, hp, "EXPIRES");
}


/**
 *  function get flush(graceful: Boolean = true): Void
 */
static EjsVar *flushHttp(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    mprFinalizeHttpWriting(hp->http);
    return 0;
}


/*
 *  function form(uri: String = null, formData: Object = null): Void
 *
 *  Issue a POST method with form data
 */
static EjsVar *form(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (argc == 2 && argv[1] != ejs->nullValue) {
        hp->requestContent = NULL;
        hp->contentLength = 0;
        prepForm(ejs, hp, NULL, argv[1]);
        if (hp->requestContent) {
            hp->contentLength = (int) strlen(hp->requestContent);
        }
        mprSetHttpHeader(hp->http, 1, "Content-Type", "application/x-www-form-urlencoded");
    }
    return startRequest(ejs, hp, "POST", argc, argv);
}


/*
 *
 *  static function get followRedirects(): Boolean
 */
static EjsVar *getFollowRedirects(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, hp->http->followRedirects);
}


/*
 *  function set followRedirects(flag: Boolean): Void
 */
static EjsVar *setFollowRedirects(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    mprSetHttpFollowRedirects(hp->http, ejsGetBoolean(argv[0]));
    return 0;
}


/*
 *  function get(uri: String = null, ...data): Void
 *
 *  The spec allows GET methods to have body data, but is rarely if ever used.
 */
static EjsVar *getMethod(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return startRequest(ejs, hp, "GET", argc, argv);
}


/*
 *  function head(uri: String = null): Void
 */
static EjsVar *headMethod(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return startRequest(ejs, hp, "HEAD", argc, argv);
}


/*
 *  function header(key: String): String
 */
static EjsVar *header(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    return (EjsVar*) ejsCreateString(ejs, mprGetHttpHeader(hp->http, ejsGetString(argv[0])));
}


/*
 *  function get headers(): Array
 */
static EjsVar *headers(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    MprHashTable    *hash;
    MprHash         *p;
    EjsVar          *results;
    char            *header;
    int             i;

    hash = mprGetHttpHeadersHash(hp->http);
    if (hash == 0) {
        return (EjsVar*) ejs->nullValue;
    }
    results = (EjsVar*) ejsCreateArray(ejs, mprGetHashCount(hash));
    for (i = 0, p = mprGetFirstHash(hash); p; p = mprGetNextHash(hash, p), i++) {
        header = mprAsprintf(results, -1, "%s=%s", p->key, p->data);
        ejsSetProperty(ejs, (EjsVar*) results, i, (EjsVar*) ejsCreateStringAndFree(ejs, header));
    }
    return (EjsVar*) results;
}


/*
 *  function get isSecure(): Boolean
 */
static EjsVar *isSecure(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, hp->http->secure);
}


/*
 *  function get key(): String
 */
static EjsVar *getKey(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (hp->keyFile) {
        return (EjsVar*) ejsCreateString(ejs, hp->keyFile);
    }
    return ejs->nullValue;
}


/*
 *  function set key(keyFile: String): Void
 */
static EjsVar *setKey(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    mprFree(hp->keyFile);
    hp->keyFile = mprStrdup(hp, ejsGetString(argv[0]));
    return 0;
}


/*
 *  function get lastModified(): Date
 */
static EjsVar *lastModified(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return getDateHeader(ejs, hp, "LAST-MODIFIED");
}


/*
 *  function get method(): String
 */
static EjsVar *getMethodValue(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, hp->method);
}


/*
 *  function set method(value: String): Void
 */
static EjsVar *setMethodValue(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    cchar    *method;

    method = ejsGetString(argv[0]);
    if (strcmp(method, "DELETE") != 0 && strcmp(method, "GET") != 0 &&  strcmp(method, "HEAD") != 0 &&
            strcmp(method, "OPTIONS") != 0 && strcmp(method, "POST") != 0 && strcmp(method, "PUT") != 0 &&
            strcmp(method, "TRACE") != 0) {
        ejsThrowArgError(ejs, "Unknown HTTP method");
        return 0;
    }
    mprFree(hp->method);
    hp->method = mprStrdup(hp, ejsGetString(argv[0]));
    return 0;
}


#if ES_ejs_io_Http_mimeType
/*
 *  static function getMimeType(ext: String): String
 */
EjsVar *getMimeType(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, mprLookupMimeType(ejs, ejsGetString(argv[0])));
}
#endif

/*
 *  function options(uri: String = null, ...data): Void
 */
static EjsVar *optionsMethod(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return startRequest(ejs, hp, "OPTIONS", argc, argv);
}


/*
 *  function post(uri: String = null, ...requestContent): Void
 */
static EjsVar *postMethod(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return startRequest(ejs, hp, "POST", argc, argv);
}


/*
 *  function put(uri: String = null, form object): Void
 */
static EjsVar *putMethod(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return startRequest(ejs, hp, "PUT", argc, argv);
}


/*
 *  Ensure the required number of bytes are read into the content buffer. If allowShortRead is true or if a callback is
 *  being used, then this routine will not block. When count is < 0 this means transfer the entire content.
 *  Returns the number of bytes read. If readTransfer returns zero, call mprIsHttpComplete to determine if the request
 *  has completed.
 */ 
static int readTransfer(Ejs *ejs, EjsHttp *hp, int count, int allowShortRead)
{
    MprBuf      *buf;
    int         nbytes, space, len;

    buf = hp->responseContent;
    while (count < 0 || mprGetBufLength(buf) < count) {
        if (count < 0) {
            nbytes = MPR_HTTP_BUFSIZE;
        } else {
            nbytes = count - mprGetBufLength(buf);
        }
        space = mprGetBufSpace(buf);
        if (space < nbytes) {
            mprGrowBuf(buf, nbytes - space);
        }
        if ((len = mprReadHttp(hp->http, mprGetBufEnd(buf), nbytes)) < 0) {
            if (hp->callback || allowShortRead) {
                break;
            }
            ejsThrowIOError(ejs, "Can't read required data");
            return MPR_ERR_CANT_READ;
        }
        mprAdjustBufEnd(buf, len);
        if (/* CHANGE count < 0 || */ (len == 0 && (mprIsHttpComplete(hp->http) || hp->callback))) {
            break;
        }
    }
    if (count < 0) {
        return mprGetBufLength(buf);
    }
    len = min(count, mprGetBufLength(buf));
    hp->received += len;
    return len;
}


/*
 *  function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number
 *
 *  Returns a count of bytes read. Non-blocking if a callback is defined. Otherwise, blocks.
 */
static EjsVar *readHttpData(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    EjsByteArray    *buffer;
    int             offset, count;

    buffer = (EjsByteArray*) argv[0];
    offset = (argc >= 2) ? ejsGetInt(argv[1]) : 0;
    count = (argc >= 3) ? ejsGetInt(argv[2]): -1;

    if (offset < 0) {
        offset = buffer->writePosition;
    } else if (offset >= buffer->length) {
        ejsThrowOutOfBoundsError(ejs, "Bad read offset value");
        return 0;
    } else {
        ejsSetByteArrayPositions(ejs, buffer, 0, 0);
    }
    if (!waitForResponse(hp, -1)) {
        mprAssert(ejs->exception);
        return 0;
    }
    lock(hp);
    if ((count = readTransfer(ejs, hp, count, 0)) < 0) {
        mprAssert(ejs->exception);
        unlock(hp);
        return 0;
    }
    ejsCopyToByteArray(ejs, buffer, buffer->writePosition, (char*) mprGetBufStart(hp->responseContent), count);
    ejsSetByteArrayPositions(ejs, buffer, -1, buffer->writePosition + count);
    mprAdjustBufStart(hp->responseContent, count);
    unlock(hp);
    return (EjsVar*) ejsCreateNumber(ejs, count);
}


/*
 *  function readString(count: Number = -1): String
 *
 *  Read count bytes (default all) of content as a string. This always starts at the first character of content.
 */
static EjsVar *readStringHttp(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    EjsVar  *result;
    int     count;
    
    count = (argc == 1) ? ejsGetInt(argv[0]) : -1;

    if (!waitForState(hp, MPR_HTTP_STATE_COMPLETE, hp->http->timeoutPeriod, 0)) {
        return 0;
    }
    lock(hp);
    if ((count = readTransfer(ejs, hp, count, 0)) < 0) {
        return 0;
    }
    result = (EjsVar*) ejsCreateStringWithLength(ejs, mprGetBufStart(hp->responseContent), count);
    mprAdjustBufStart(hp->responseContent, count);
    unlock(hp);
    return result;
}


/*
 *  function readLines(count: Number = -1): Array
 *
 *  Read count lines (default all) of content as an array of strings.
 */
static EjsVar *readLines(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    EjsArray    *results;
    EjsVar      *str;
    MprBuf      *buf;
    cchar       *start, *cp;
    int         lineCount, count, nextIndex, len, lineCreated;

    lineCount = (argc == 1) ? ejsGetInt(argv[0]) : MAXINT;
    results = ejsCreateArray(ejs, 0);
    count = 0;

    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    lock(hp);
    buf = hp->responseContent;
    for (nextIndex = 0; nextIndex < lineCount; nextIndex++) {
        lineCreated = 0;
        for (cp = start = mprGetBufStart(buf); cp < buf->end; cp++) {
            if (*cp == '\r' || *cp == '\n') {
                len = cp - start;
                str = (EjsVar*) ejsCreateStringWithLength(ejs, start, len);
                mprAdjustBufStart(buf, len);
                ejsSetProperty(ejs, (EjsVar*) results, nextIndex, str);
                for (start = cp; *cp == '\r' || *cp == '\n'; ) {
                    cp++;
                }
                mprAdjustBufStart(buf, cp - start);
                lineCreated++;
                break;
            }
        }
        if (!lineCreated) {
            if ((count = readTransfer(ejs, hp, MPR_BUFSIZE, 1)) < 0) {
                return 0;
            } else if (count == 0) {
                if (!mprIsHttpComplete(hp->http)) {
                    if (!hp->callback) {
                        ejsThrowIOError(ejs, "Can't read required data");
                        return 0;
                    }
                }
                break;
            }
        }
    }
    if (mprGetBufLength(buf) > 0 && (nextIndex == 0 || count < 0)) {
        /*
         *  Partial line
         */
        str = (EjsVar*) ejsCreateStringWithLength(ejs, mprGetBufStart(buf), mprGetBufLength(buf));
        ejsSetProperty(ejs, (EjsVar*) results, nextIndex++, str);
        mprFlushBuf(buf);
    }
    unlock(hp);
    return (EjsVar*) results;
}


#if BLD_FEATURE_EJS_E4X && UNUSED
/*
 *  function readXml(): XML
 */
static EjsVar *readXml(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    EjsXML  *xml;
    int     count;

    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    xml = ejsCreateXML(ejs, 0, NULL, NULL, NULL);
    lock(hp);
    if ((count = readTransfer(ejs, hp, -1, 0)) < 0) {
        return 0;
    }
    mprAddNullToBuf(hp->responseContent);
    ejsLoadXMLString(ejs, xml, mprGetBufStart(hp->responseContent));
    mprFlushBuf(hp->responseContent);
    unlock(hp);
    return (EjsVar*) xml;
}
#endif


/*
 *  function response(): Stream
 */
static EjsVar *httpResponse(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    if (hp->responseCache) {
        return hp->responseCache;
    }
    hp->responseCache = readStringHttp(ejs, hp, argc, argv);
    return (EjsVar*) hp->responseCache;
}


/*
 *  function setCredentials(username: String, password: String): Void
 */
static EjsVar *setCredentials(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    mprSetHttpCredentials(hp->http, ejsGetString(argv[0]), ejsGetString(argv[1]));
    return 0;
}


/*
 *  function get timeout(): Number
 */
static EjsVar *getTimeout(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, hp->http->timeoutPeriod);
}


/*
 *  function set timeout(value: Number): Void
 */
static EjsVar *setTimeout(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    mprSetHttpTimeout(hp->http, (int) ejsGetNumber(argv[0]));
    return 0;
}


/*
 *  function trace(uri: String = null, ...data): Void
 */
static EjsVar *traceMethod(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return startRequest(ejs, hp, "TRACE", argc, argv);
}


/*
 *  function get uri(): String
 */
static EjsVar *getUri(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, hp->uri);
}


/*
 *  function set uri(uri: String): Void
 */
static EjsVar *setUri(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    mprFree(hp->uri);
    hp->uri = prepUri(hp, ejsGetString(argv[0]));
    return 0;
}


#if ES_ejs_io_Http_wait
/**
 *  Wait for a request to complete. This call will only have effect if an async request is underway.
 *
 *  function wait(timeout: Number = -1): Boolean
 */
static EjsVar *httpWait(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    MprTime     mark;
    int         timeout;

    timeout = (argc == 1) ? ejsGetInt(argv[0]) : MPR_TIMEOUT_HTTP;
    if (timeout < 0) {
        timeout = MAXINT;
    }
    mark = mprGetTime(ejs);

    if (!hp->requestStarted && startRequest(ejs, hp, NULL, 0, NULL) < 0) {
        return 0;
    }
    if (!waitForState(hp, MPR_HTTP_STATE_COMPLETE, timeout, 0)) {
        return (EjsVar*) ejs->falseValue;
    }
    return (EjsVar*) ejs->trueValue;
}
#endif


/*
 *  Write post data to the request stream. Connection must be in async mode.
 *
 *  function write(...data): Void
 */
static EjsVar *httpWrite(Ejs *ejs, EjsHttp *hp, int argc, EjsVar **argv)
{
    EjsByteArray    *data;
    EjsNumber       *written;

    mprAssert(hp->http->request);
    mprAssert(hp->http->sock);

    if (!hp->requestStarted && startRequest(hp->ejs, hp, NULL, 0, NULL) < 0) {
        return 0;
    }
    data = ejsCreateByteArray(ejs, -1);
    written = ejsWriteToByteArray(ejs, data, 1, &argv[0]);

    if (mprWriteHttp(hp->http, (char*) data->value, (int) written->value) != (int) written->value) {
        ejsThrowIOError(ejs, "Can't write post data");
    }
    return 0;
}


/*
 *  Issue a request
 *      startRequest(uri = null, ...data)
 */
static EjsVar *startRequest(Ejs *ejs, EjsHttp *hp, char *method, int argc, EjsVar **argv)
{
    EjsArray        *args;
    EjsByteArray    *data;
    EjsNumber       *written;

    hp->responseCache = 0;

    if (argc >= 1 && argv[0] != ejs->nullValue) {
        mprFree(hp->uri);
        hp->uri = prepUri(hp, ejsGetString(argv[0]));
    }
    if (argc == 2 && ejsIsArray(argv[1])) {
        args = (EjsArray*) argv[1];
        if (args->length > 0) {
            data = ejsCreateByteArray(ejs, -1);
            written = ejsWriteToByteArray(ejs, data, 1, &argv[1]);
            hp->requestContent = (char*) data->value;
            hp->contentLength = (int) written->value;
        }
    }

    if (hp->uri == 0) {
        ejsThrowArgError(ejs, "URL is not defined");
        return 0;
    }

#if BLD_FEATURE_SSL
    if (strncmp(hp->uri, "https", 5) == 0) {
        if (!mprLoadSsl(ejs, 0)) {
            ejsThrowIOError(ejs, "Can't load SSL provider");
            return 0;
        }
    }
#endif
    if (method && strcmp(hp->method, method) != 0) {
        mprFree(hp->method);
        hp->method = mprStrdup(hp, method);
    }
    if (hp->method == 0) {
        ejsThrowArgError(ejs, "HTTP Method is not defined");
        return 0;
    }
    mprFlushBuf(hp->responseContent);
    if (hp->contentLength > 0) {
        mprSetHttpBody(hp->http, hp->requestContent, hp->contentLength);
    }
    hp->requestStarted = 1;
    hp->gotResponse = 0;
    hp->requestContent = 0;
    hp->contentLength = 0;

    /*
     *  Block if a callback has not been defined
     */
    if (mprStartHttpRequest(hp->http, hp->method, hp->uri) < 0) {
        ejsThrowIOError(ejs, "Can't issue request for \"%s\"", hp->uri);
        return 0;
    }
    return 0;
}


static bool waitForState(EjsHttp *hp, int state, int timeout, int throw)
{
    Ejs         *ejs;
    MprHttp     *http;
    MprTime     mark;
    char        *url;
    int         count, transCount;

    ejs = hp->ejs;
    if (timeout < 0) {
        timeout = MAXINT;
    }

    http = hp->http;
    count = -1;
    transCount = 0;
    mark = mprGetTime(hp);

    if (state >= MPR_HTTP_STATE_CONTENT) {
        if (mprFinalizeHttpWriting(http) < 0) {
            if (throw) {
                ejsThrowIOError(ejs, "Can't write request data");
            }
            return 0;
        }
    }
    while (++count < http->retries && transCount < 4 && !ejs->exiting && !mprIsExiting(http) && 
            mprGetElapsedTime(hp, mark) < timeout) {
        if (!hp->requestStarted && startRequest(ejs, hp, NULL, 0, NULL) < 0) {
            return 0;
        }
        if (mprWaitForHttp(http, state, timeout) < 0) {
            continue;
        }
        if (http->state >= MPR_HTTP_STATE_CONTENT && mprNeedHttpRetry(http, &url)) {
            if (url) {
                mprFree(hp->uri);
                hp->uri = prepUri(http, url);
            }
            count--;
            transCount++;
            hp->requestStarted = 0;
            continue;
        }
        break;
    }
    if (http->state < state) {
        if (throw && ejs->exception == 0) {
            ejsThrowIOError(ejs, "Http has not received a response: timeout %d, retryCount %d/%d, trans %d/4",
                timeout, count, http->retries, transCount);
        }
        return 0;
    }
    return 1;
}


static bool waitForResponse(EjsHttp *hp, int timeout)
{
    if (hp->gotResponse) {
        return 1;
    }
    if (!waitForState(hp, MPR_HTTP_STATE_CONTENT, timeout, 1)) {
        return 0;
    }
    hp->gotResponse = 1;
    return 1;
}


static void httpCallback(EjsHttp *hp, int eventMask)
{
    MprHttp     *http;
    Ejs         *ejs;
    EjsVar      *arg;
    EjsType     *eventType;
    EjsObject   *event;

    ejs = hp->ejs;
    http = hp->http;
    if (http->error) {
        eventType = ejsGetType(ejs, ES_ejs_io_HttpErrorEvent);
        arg = (EjsVar*) ejsCreateString(ejs, http->error);
    } else {
        eventType = ejsGetType(ejs, ES_ejs_io_HttpDataEvent);
        arg = (EjsVar*) ejs->nullValue;
    }
    event = (EjsObject*) ejsCreateInstance(ejs, eventType, 1, (EjsVar**) &arg);
    if (event) {
        /*
         *  Invoked as:  callback(e: Event)  where e.data == http
         */
        ejsSetProperty(ejs, (EjsVar*) event, ES_ejs_events_Event_data, (EjsVar*) hp);
        if (!http->error) {
            ejsSetProperty(ejs, (EjsVar*) event, ES_ejs_io_HttpDataEvent_eventMask, 
                (EjsVar*) ejsCreateNumber(ejs, eventMask));
        }
        arg = (EjsVar*) event;
        ejsRunFunction(hp->ejs, hp->callback, 0, 1, &arg);
    }
}


static EjsVar *getDateHeader(Ejs *ejs, EjsHttp *hp, cchar *key)
{
    MprTime     when;
    cchar       *value;

    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    value = mprGetHttpHeader(hp->http, key);
    if (value == 0) {
        return (EjsVar*) ejs->nullValue;
    }
    if (mprParseTime(ejs, &when, value, MPR_UTC_TIMEZONE, NULL) < 0) {
        value = 0;
    }
    return (EjsVar*) ejsCreateDate(ejs, when);
}


static EjsVar *getStringHeader(Ejs *ejs, EjsHttp *hp, cchar *key)
{
    cchar       *value;

    if (!waitForResponse(hp, -1)) {
        return 0;
    }
    value = mprGetHttpHeader(hp->http, key);
    if (value == 0) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) ejsCreateString(ejs, mprGetHttpHeader(hp->http, key));
}


/*
 *  Prepare form data as a series of key-value pairs. Data is an object with properties becoming keys in a 
 *  www-url-encoded string. Objects are flattened into a one level key/value pairs by using JSON encoding. 
 *  E.g.  name=value&address=77%20Park%20Lane
 */
static void prepForm(Ejs *ejs, EjsHttp *hp, char *prefix, EjsVar *data)
{
    EjsName     qname;
    EjsVar      *vp;
    EjsString   *value;
    cchar       *key, *sep;
    char        *encodedKey, *encodedValue, *newPrefix, *newKey;
    int         i, count;

    count = ejsGetPropertyCount(ejs, data);
    for (i = 0; i < count; i++) {
        qname = ejsGetPropertyName(ejs, data, i);
        key = qname.name;

        vp = ejsGetProperty(ejs, data, i);
        if (vp == 0) {
            continue;
        }
        if (ejsGetPropertyCount(ejs, vp) > 0 && !ejsIsArray(vp)) {
            if (prefix) {
                newPrefix = mprAsprintf(hp, -1, "%s.%s", prefix, qname.name);
                prepForm(ejs, hp, newPrefix, vp);
                mprFree(newPrefix);
            } else {
                prepForm(ejs, hp, (char*) qname.name, vp);
            }
        } else {
            if (ejsIsArray(vp)) {
                value = ejsToJson(ejs, vp);
            } else {
                value = ejsToString(ejs, vp);
            }
            sep = (hp->requestContent) ? "&" : "";
            if (prefix) {
                newKey = mprStrcat(hp, -1, prefix, ".", key, NULL);
                encodedKey = mprUrlEncode(hp, newKey); 
                mprFree(newKey);
            } else {
                encodedKey = mprUrlEncode(hp, key); 
            }
            encodedValue = mprUrlEncode(hp, value->value);
            hp->requestContent = mprReallocStrcat(hp, -1, hp->requestContent, sep, encodedKey, "=", encodedValue, NULL);
            mprFree(encodedKey);
            mprFree(encodedValue);
        }
    }
}


static bool isPort(cchar *name)
{
    cchar   *cp;
    
    for (cp = name; *cp && *cp != '/'; cp++) {
        if (!isdigit((int) *cp) || *cp == '.') {
            return 0;
        }
    }
    return 1;
}


/*
 *  Prepare a URL by adding http:// as required
 */
static char *prepUri(MprCtx ctx, cchar *uri) 
{
    char    *newUri;

    if (*uri == '/') {
        newUri = mprStrcat(ctx, MPR_MAX_URL, "http://127.0.0.1", uri, NULL);
    } else if (strstr(uri, "http://") == 0 && strstr(uri, "https://") == 0) {
        if (isPort(uri)) {
            newUri = mprStrcat(ctx, MPR_MAX_URL, "http://127.0.0.1:", uri, NULL);
        } else {
            newUri = mprStrcat(ctx, MPR_MAX_URL, "http://", uri, NULL);
        }
    } else {
        newUri = mprStrdup(ctx, uri);
    }
    return newUri;
}
    

/*
 *  Mark the object properties for the garbage collector
 */
void markHttpVar(Ejs *ejs, EjsVar *parent, EjsHttp *http)
{
    ejsMarkObject(ejs, parent, (EjsObject*) http);
    if (http->responseCache) {
        ejsMarkVar(ejs, parent, (EjsVar*) http->responseCache);
    }
    if (http->callback) {
        ejsMarkVar(ejs, parent, (EjsVar*) http->callback);
    }
}


void ejsCreateHttpType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, "ejs.io", "Http"), ejs->objectType, sizeof(EjsHttp), ES_ejs_io_Http,
        ES_ejs_io_Http_NUM_CLASS_PROP, ES_ejs_io_Http_NUM_INSTANCE_PROP, 
        EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_HAS_CONSTRUCTOR | EJS_ATTR_OBJECT_HELPERS);
    type->dontPool = 1;
    type->helpers->markVar = (EjsMarkVarHelper) markHttpVar;
}


void ejsConfigureHttpType(Ejs *ejs)
{
    EjsType     *type;

    if ((type = ejsGetType(ejs, ES_ejs_io_Http)) == 0) {
        return;
    }
    ejsBindMethod(ejs, type, ES_ejs_io_Http_Http, (EjsNativeFunction) httpConstructor);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_addHeader, (EjsNativeFunction) addHeader);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_available, (EjsNativeFunction) httpAvailable);
#if ES_ejs_io_Http_setCallback
    ejsBindMethod(ejs, type, ES_ejs_io_Http_setCallback, (EjsNativeFunction) setHttpCallback);
#endif
#if ES_ejs_io_Http_chunked
    ejsBindMethod(ejs, type, ES_ejs_io_Http_chunked, (EjsNativeFunction) getChunked);
#endif
#if ES_ejs_io_Http_set_chunked
    ejsBindMethod(ejs, type, ES_ejs_io_Http_set_chunked, (EjsNativeFunction) setChunked);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_Http_close, (EjsNativeFunction) closeHttp);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_connect, (EjsNativeFunction) connectHttp);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_certificate, (EjsNativeFunction) getCertificate);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_set_certificate, (EjsNativeFunction) setCertificate);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_code, (EjsNativeFunction) code);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_codeString, (EjsNativeFunction) codeString);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_contentEncoding, (EjsNativeFunction) contentEncoding);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_contentLength, (EjsNativeFunction) contentLength);
#if ES_ejs_io_Http_set_contentLength
    ejsBindMethod(ejs, type, ES_ejs_io_Http_set_contentLength, (EjsNativeFunction) setContentLength);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_Http_contentType, (EjsNativeFunction) contentType);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_date, (EjsNativeFunction) date);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_del, (EjsNativeFunction) del);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_expires, (EjsNativeFunction) expires);
#if ES_ejs_io_Http_flush
    ejsBindMethod(ejs, type, ES_ejs_io_Http_flush, (EjsNativeFunction) flushHttp);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_Http_followRedirects, (EjsNativeFunction) getFollowRedirects);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_set_followRedirects, (EjsNativeFunction) setFollowRedirects);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_form, (EjsNativeFunction) form);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_get, (EjsNativeFunction) getMethod);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_head, (EjsNativeFunction) headMethod);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_header, (EjsNativeFunction) header);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_headers, (EjsNativeFunction) headers);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_isSecure, (EjsNativeFunction) isSecure);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_key, (EjsNativeFunction) getKey);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_set_key, (EjsNativeFunction) setKey);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_lastModified, (EjsNativeFunction) lastModified);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_method, (EjsNativeFunction) getMethodValue);
#if ES_ejs_io_Http_mimeType
    ejsBindMethod(ejs, type, ES_ejs_io_Http_mimeType, (EjsNativeFunction) getMimeType);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_Http_set_method, (EjsNativeFunction) setMethodValue);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_post, (EjsNativeFunction) postMethod);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_put, (EjsNativeFunction) putMethod);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_read, (EjsNativeFunction) readHttpData);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_readString, (EjsNativeFunction) readStringHttp);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_readLines, (EjsNativeFunction) readLines);
#if BLD_FEATURE_EJS_E4X && UNUSED
    ejsBindMethod(ejs, type, ES_ejs_io_Http_readXml, (EjsNativeFunction) readXml);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_Http_response, (EjsNativeFunction) httpResponse);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_options, (EjsNativeFunction) optionsMethod);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_setCredentials, (EjsNativeFunction) setCredentials);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_timeout, (EjsNativeFunction) getTimeout);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_set_timeout, (EjsNativeFunction) setTimeout);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_trace, (EjsNativeFunction) traceMethod);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_uri, (EjsNativeFunction) getUri);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_set_uri, (EjsNativeFunction) setUri);
    ejsBindMethod(ejs, type, ES_ejs_io_Http_write, (EjsNativeFunction) httpWrite);
#if ES_ejs_io_Http_wait
    ejsBindMethod(ejs, type, ES_ejs_io_Http_wait, (EjsNativeFunction) httpWait);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_Http_write, (EjsNativeFunction) httpWrite);
}


#else /* ES_ejs_io_Http && BLD_FEATURE_HTTP_CLIENT */

void __dummyEjsHttp() {}
#endif /* ES_ejs_io_Http && BLD_FEATURE_HTTP_CLIENT */

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/io/ejsHttp.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/io/ejsPath.c"
 */
/************************************************************************/

/**
    ejsPath.c - Path class.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ES_ejs_io_Path

static char *getPath(Ejs *ejs, EjsVar *vp);


static void destroyPath(Ejs *ejs, EjsPath *pp)
{
    mprAssert(pp);

    mprFree(pp->path);
    pp->path = 0;
    ejsFreeVar(ejs, (EjsVar*) pp, -1);
}


static EjsVar *coercePathOperands(Ejs *ejs, EjsPath *lhs, int opcode,  EjsVar *rhs)
{
    switch (opcode) {
    /*
     *  Binary operators
     */
    case EJS_OP_ADD:
        return ejsInvokeOperator(ejs, (EjsVar*) ejsCreateString(ejs, lhs->path), opcode, rhs);

    case EJS_OP_COMPARE_EQ: case EJS_OP_COMPARE_NE:
    case EJS_OP_COMPARE_LE: case EJS_OP_COMPARE_LT:
    case EJS_OP_COMPARE_GE: case EJS_OP_COMPARE_GT:
        if (ejsIsNull(rhs) || ejsIsUndefined(rhs)) {
            return (EjsVar*) ((opcode == EJS_OP_COMPARE_EQ) ? ejs->falseValue: ejs->trueValue);
        }
        return ejsInvokeOperator(ejs, (EjsVar*) ejsCreateString(ejs, lhs->path), opcode, rhs);

    case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_STRICTLY_EQ:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_COMPARE_NOT_ZERO:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) ejs->trueValue;

    case EJS_OP_COMPARE_ZERO:
    case EJS_OP_COMPARE_FALSE:
        return (EjsVar*) ejs->falseValue;

    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
        return (EjsVar*) ejs->falseValue;

    default:
        ejsThrowTypeError(ejs, "Opcode %d not valid for type %s", opcode, lhs->obj.var.type->qname.name);
        return ejs->undefinedValue;
    }
    return 0;
}


static EjsVar *invokePathOperator(Ejs *ejs, EjsPath *lhs, int opcode,  EjsPath *rhs, void *data)
{
    EjsVar      *result;

    if (rhs == 0 || lhs->obj.var.type != rhs->obj.var.type) {
        if ((result = coercePathOperands(ejs, lhs, opcode, (EjsVar*) rhs)) != 0) {
            return result;
        }
    }
    /*
     *  Types now match, both paths
     */
    switch (opcode) {
    case EJS_OP_COMPARE_STRICTLY_EQ:
    case EJS_OP_COMPARE_EQ:
        if (lhs == rhs || (lhs->path == rhs->path)) {
            return (EjsVar*) ejs->trueValue;
        }
        return (EjsVar*) ejsCreateBoolean(ejs,  mprStrcmp(lhs->path, rhs->path) == 0);

    case EJS_OP_COMPARE_NE:
    case EJS_OP_COMPARE_STRICTLY_NE:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprStrcmp(lhs->path, rhs->path) != 0);

    case EJS_OP_COMPARE_LT:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprStrcmp(lhs->path, rhs->path) < 0);

    case EJS_OP_COMPARE_LE:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprStrcmp(lhs->path, rhs->path) <= 0);

    case EJS_OP_COMPARE_GT:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprStrcmp(lhs->path, rhs->path) > 0);

    case EJS_OP_COMPARE_GE:
        return (EjsVar*) ejsCreateBoolean(ejs,  mprStrcmp(lhs->path, rhs->path) >= 0);

    /*
     *  Unary operators
     */
    case EJS_OP_COMPARE_NOT_ZERO:
        return (EjsVar*) ((lhs->path) ? ejs->trueValue: ejs->falseValue);

    case EJS_OP_COMPARE_ZERO:
        return (EjsVar*) ((lhs->path == 0) ? ejs->trueValue: ejs->falseValue);


    case EJS_OP_COMPARE_UNDEFINED:
    case EJS_OP_COMPARE_NULL:
    case EJS_OP_COMPARE_FALSE:
    case EJS_OP_COMPARE_TRUE:
        return (EjsVar*) ejs->falseValue;

    /*
     *  Binary operators
     */
    case EJS_OP_ADD:
        return (EjsVar*) ejsCreatePath(ejs, mprJoinPath(ejs, lhs->path, rhs->path));

    default:
        ejsThrowTypeError(ejs, "Opcode %d not implemented for type %s", opcode, lhs->obj.var.type->qname.name);
        return 0;
    }
    mprAssert(0);
}


/*
    Constructor

    function Path(path: String)

 */
static EjsVar *pathConstructor(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    mprAssert(argc == 1);
    if ((fp->path = getPath(ejs, argv[0])) == 0) {
        return (EjsVar*) fp;
    }
    fp->path = mprStrdup(fp, fp->path);
    return (EjsVar*) fp;
}


/*
    Return an absolute path name for the file

    function get absolutePath()
 */
static EjsVar *absolutePath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetAbsPath(fp, fp->path));
}


/*
    Get when the file was last accessed.

    function get accessed(): Date
 */
static EjsVar *getAccessedDate(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;

    mprGetPathInfo(ejs, fp->path, &info);
    if (!info.valid) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) ejsCreateDate(ejs, ((MprTime) info.atime) * 1000);
}


/*
    Get the base name of a file

    function basename(): Path
 */
static EjsVar *getPathBasename(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetPathBase(ejs, fp->path));
}


/*
    Get the path components

    function components(): Array
 */
static EjsVar *getPathComponents(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprFileSystem   *fs;
    EjsArray        *ap;
    char            *cp, *last;
    int             index;

    fs = mprLookupFileSystem(ejs, fp->path);
    ap = ejsCreateArray(ejs, 0);
    index = 0;
    for (last = cp = mprGetAbsPath(fp, fp->path); *cp; cp++) {
        if (*cp == fs->separators[0] || *cp == fs->separators[1]) {
            *cp++ = '\0';
            ejsSetProperty(ejs, (EjsVar*) ap, index++, (EjsVar*) ejsCreateString(ejs, last));
            last = cp;
        }
    }
    if (cp > last) {
        ejsSetProperty(ejs, (EjsVar*) ap, index++, (EjsVar*) ejsCreateString(ejs, last));
    }
    return (EjsVar*) ap;
}


/*
    Copy a file

    function copy(to: Object, options: Object = null): Void
 */
static EjsVar *copyPath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprFile     *from, *to;
    char        *buf, *toPath;
    uint        bytes;
    int         rc;

    mprAssert(argc == 1);
    if ((toPath = getPath(ejs, argv[0])) == 0) {
        return 0;
    }

    from = mprOpen(ejs, fp->path, O_RDONLY | O_BINARY, 0);
    if (from == 0) {
        ejsThrowIOError(ejs, "Cant open %s", fp->path);
        return 0;
    }

    to = mprOpen(ejs, toPath, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, EJS_FILE_PERMS);
    if (to == 0) {
        ejsThrowIOError(ejs, "Cant create %s", toPath);
        mprFree(from);
        return 0;
    }

    buf = mprAlloc(ejs, MPR_BUFSIZE);
    if (buf == 0) {
        ejsThrowMemoryError(ejs);
        mprFree(to);
        mprFree(from);
        return 0;
    }

    rc = 0;
    while ((bytes = mprRead(from, buf, MPR_BUFSIZE)) > 0) {
        if (mprWrite(to, buf, bytes) != bytes) {
            ejsThrowIOError(ejs, "Write error to %s", toPath);
            rc = 0;
            break;
        }
    }
    mprFree(from);
    mprFree(to);
    mprFree(buf);
    return 0;
}


/*
    Return when the file was created.

    function get created(): Date
 */
static EjsVar *getCreatedDate(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;

    mprGetPathInfo(ejs, fp->path, &info);
    if (!info.valid) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) ejsCreateDate(ejs, ((MprTime) info.ctime) * 1000);
}


/**
    Get the directory name portion of a file.

    function get dirname(): Path
 */
static EjsVar *getPathDirname(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetPathDir(ejs, fp->path));
}


/*
    Test to see if this file exists.

    function get exists(): Boolean
 */
static EjsVar *getPathExists(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;

    return (EjsVar*) ejsCreateBoolean(ejs, mprGetPathInfo(ejs, fp->path, &info) == 0);
}


/*
    Get the file extension portion of the file name.

    function get extension(): String
 */
static EjsVar *getPathExtension(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    char    *cp;

    if ((cp = strrchr(fp->path, '.')) == 0) {
        return (EjsVar*) ejs->emptyStringValue;
    }
    return (EjsVar*) ejsCreateString(ejs, &cp[1]);
}


/*
    Function to iterate and return the next element index.
    NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextPathKey(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsPath     *fp;

    fp = (EjsPath*) ip->target;
    if (!ejsIsPath(fp)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    if (ip->index < mprGetListCount(fp->files)) {
        return (EjsVar*) ejsCreateNumber(ejs, ip->index++);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
    Return the default iterator for use with "for ... in". This will iterate over the files in a directory.

    iterator native function get(): Iterator
 */
static EjsVar *getPathIterator(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    mprFree(fp->files);
    fp->files = mprGetPathFiles(fp, fp->path, 0);
    return (EjsVar*) ejsCreateIterator(ejs, (EjsVar*) fp, (EjsNativeFunction) nextPathKey, 0, NULL);
}


/*
    Function to iterate and return the next element value.
    NOTE: this is not a method of Array. Rather, it is a callback function for Iterator
 */
static EjsVar *nextPathValue(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsPath     *fp;
    MprDirEntry *dp;

    fp = (EjsPath*) ip->target;
    if (!ejsIsPath(fp)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    if (ip->index < mprGetListCount(fp->files)) {
        dp = (MprDirEntry*) mprGetItem(fp->files, ip->index++);
        return (EjsVar*) ejsCreatePath(ejs, dp->name);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
    Return an iterator to enumerate the bytes in the file. For use with "for each ..."

    iterator native function getValues(): Iterator
 */
static EjsVar *getPathValues(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    mprFree(fp->files);
    fp->files = mprGetPathFiles(fp, fp->path, 0);
    return (EjsVar*) ejsCreateIterator(ejs, (EjsVar*) fp, (EjsNativeFunction) nextPathValue, 0, NULL);
}


/*
    Get the files in a directory.
    function getFiles(enumDirs: Boolean = false): Array
 */
static EjsVar *getPathFiles(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    EjsArray        *array;
    MprList         *list;
    MprDirEntry     *dp;
    char            *path;
    bool            enumDirs, noPath;
    int             next;

    mprAssert(argc == 0 || argc == 1);
    enumDirs = (argc == 1) ? ejsGetBoolean(argv[0]): 0;

    array = ejsCreateArray(ejs, 0);
    if (array == 0) {
        return 0;
    }
    list = mprGetPathFiles(array, fp->path, enumDirs);
    if (list == 0) {
        ejsThrowIOError(ejs, "Can't read directory");
        return 0;
    }
    noPath = (fp->path[0] == '.' && fp->path[1] == '\0') || 
        (fp->path[0] == '.' && fp->path[1] == '/' && fp->path[2] == '\0');

    for (next = 0; (dp = mprGetNextItem(list, &next)) != 0; ) {
        if (strcmp(dp->name, ".") == 0 || strcmp(dp->name, "..") == 0) {
            continue;
        }
        if (enumDirs || !(dp->isDir)) {
            if (noPath) {
                ejsSetProperty(ejs, (EjsVar*) array, -1, (EjsVar*) ejsCreatePath(ejs, dp->name));
            } else {
                /*
                 *  Prepend the directory name
                 */
                path = mprJoinPath(ejs, fp->path, dp->name);
                ejsSetProperty(ejs, (EjsVar*) array, -1, (EjsVar*) ejsCreatePathAndFree(ejs, path));
            }
        }
    }
    mprFree(list);
    return (EjsVar*) array;
}


/*
    Determine if the file path has a drive spec (C:) in the file name

    static function hasDrive(): Boolean
 */
static EjsVar *pathHasDrive(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateBoolean(ejs, 
        (isalpha((int) fp->path[0]) && fp->path[1] == ':' && (fp->path[2] == '/' || fp->path[2] == '\\')));
}


/*
    function get isAbsolute(): Boolean
 */
static EjsVar *isPathAbsolute(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) (mprIsAbsPath(ejs, fp->path) ? ejs->trueValue: ejs->falseValue);
}


/*
    Determine if the file name is a directory

    function get isDir(): Boolean
 */
static EjsVar *isPathDir(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;
    int         rc;

    rc = mprGetPathInfo(ejs, fp->path, &info);
    return (EjsVar*) ejsCreateBoolean(ejs, rc == 0 && info.isDir);
}


/*
    function get isLink(): Boolean
 */
static EjsVar *isPathLink(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;
    int         rc;

    rc = mprGetPathInfo(ejs, fp->path, &info);
    return (EjsVar*) ejsCreateBoolean(ejs, rc == 0 && info.isLink);
}


/*
    Determine if the file name is a regular file

    function get isRegular(): Boolean
 */
static EjsVar *isPathRegular(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;

    mprGetPathInfo(ejs, fp->path, &info);

    return (EjsVar*) ejsCreateBoolean(ejs, info.isReg);
}


/*
    function get isRelative(): Boolean
 */
static EjsVar *isPathRelative(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) (mprIsRelPath(ejs, fp->path) ? ejs->trueValue: ejs->falseValue);
}


/*
    Join paths. Returns a normalized path.

    function join(...others): Path
 */
static EjsVar *joinPath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    EjsArray    *args;
    cchar       *other, *join;
    int         i;

    args = (EjsArray*) argv[0];
    join = fp->path;
    for (i = 0; i < args->length; i++) {
        if ((other = getPath(ejs, ejsGetProperty(ejs, (EjsVar*) args, i))) == NULL) {
            return 0;
        }
        join = mprJoinPath(ejs, join, other);
    }
    return (EjsVar*) ejsCreatePath(ejs, join);;
}


/*
    Join extension

    function joinExt(ext: String): Path
 */
static EjsVar *joinPathExt(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    cchar   *ext;
    if (mprGetPathExtension(ejs, fp->path)) {
        return (EjsVar*) fp;
    }
    ext = ejsGetString(argv[0]);
    while (ext && *ext == '.') {
        ext++;
    }
    return (EjsVar*) ejsCreatePath(ejs, mprStrcat(ejs, -1, fp->path, ".", ext, NULL));
}


/*
    Get the length of the path name.

    override intrinsic function get length(): Number
 */
static EjsVar *pathLength(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, strlen(fp->path));
}


static EjsVar *pathLinkTarget(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    char    *path;

    if ((path = mprGetPathLink(ejs, fp->path)) == 0) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetPathLink(ejs, fp->path));
}


/*
    function makeDir(options: Object = null): Void

    Options: permissions, owner, group
 */
static EjsVar *makePathDir(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;
    EjsName     qname;
    EjsVar      *options, *permissions;
    int         perms;
    
    perms = 0755;

    if (argc == 1) {
        if (!ejsIsObject(argv[0])) {
            ejsThrowArgError(ejs, "Bad args");
        }
        options = argv[0];

        if (ejsIsObject(options)) {
            permissions = ejsGetPropertyByName(ejs, options, ejsName(&qname, EJS_PUBLIC_NAMESPACE, "permissions"));
            if (permissions) {
                perms = ejsGetInt(permissions);
            }
        }
    }
    if (mprGetPathInfo(ejs, fp->path, &info) == 0 && info.isDir) {
        return 0;
    }
    if (mprMakeDir(ejs, fp->path, perms, 1) < 0) {
        ejsThrowIOError(ejs, "Cant create directory %s", fp->path);
        return 0;
    }
    return 0;
}


/*
    function makeLink(target: Path, hard: Boolean = false): Void
 */
static EjsVar *makePathLink(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    cchar   *target;
    int     hard;

    target = ((EjsPath*) argv[0])->path;
    hard = (argc >= 2) ? (argv[1] == (EjsVar*) ejs->trueValue) : 0;
    if (mprMakeLink(ejs, fp->path, target, hard) < 0) {
        ejsThrowIOError(ejs, "Can't make link");
    }
    return 0;
}


/*
    Make a temporary file. Creates a new, uniquely named temporary file.

    function makeTemp(): Path
 */
static EjsVar *makePathTemp(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    char    *path;

    if ((path = mprGetTempPath(ejs, fp->path)) == NULL) {
        ejsThrowIOError(ejs, "Can't make temp file");
        return 0;
    }
    return (EjsVar*) ejsCreatePathAndFree(ejs, path);
}


/*
    function map(separator: String): Path
 */
static EjsVar *pa_map(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    cchar   *sep;
    char    *path;
    int     separator;

    sep = ejsGetString(argv[0]);
    separator = *sep ? *sep : '/';
    path = mprStrdup(ejs, fp->path);
    mprMapSeparators(ejs, path, separator);
    return (EjsVar*) ejsCreatePathAndFree(ejs, path);
}


EjsVar *pa_mimeType(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, mprLookupMimeType(ejs, fp->path));
}


/*
    Get when the file was created or last modified.

    function get modified(): Date
 */
static EjsVar *getModifiedDate(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;

    mprGetPathInfo(ejs, fp->path, &info);
    if (!info.valid) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) ejsCreateDate(ejs, ((MprTime) info.mtime) * 1000);
}


/*
    Get the name of the path as a string.

    function get name(): String
 */
static EjsVar *getPathName(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, fp->path);
}


/*
    function get natural(): Path
 */
static EjsVar *getNaturalPath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetNativePath(ejs, fp->path));
}


/*
    function get normalize(): Path
 */
static EjsVar *normalizePath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetNormalizedPath(ejs, fp->path));
}


/*
    Get the parent directory of the absolute path of the file.

    function get parent(): String
 */
static EjsVar *getPathParent(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetPathParent(fp, fp->path));
}


#if ES_ejs_io_Path_perms
/*
    Get the path permissions

    function get perms(): Number
 */
static EjsVar *getPerms(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;

    if (mprGetPathInfo(ejs, fp->path, &info) < 0) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) ejsCreateNumber(ejs, info.perms);
}
#endif


#if ES_ejs_io_Path_set_perms
/*
    Set the path permissions

    function set perms(perms: Number): Void
 */
static EjsVar *setPerms(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
#if !VXWORKS
    int     perms;

    perms = ejsGetInt(argv[0]);
    if (chmod(fp->path, perms) < 0) {
        ejsThrowIOError(ejs, "Can't update permissions for %s", fp->path);
    }
#endif
    return 0;
}
#endif


/*
    Get a portable (unix-like) representation of the path
  
    function get portable(lower: Boolean = false): Path
 */
static EjsVar *getPortablePath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    char    *path;
    int     lower;

    lower = (argc >= 1 && argv[0] == (EjsVar*) ejs->trueValue);
    path = mprGetPortablePath(ejs, fp->path);
    if (lower) {
        mprStrLower(path);
    }
    return (EjsVar*) ejsCreatePathAndFree(ejs, path);
}



#if KEEP
/*
    Get the file contents as a byte array

    static function readBytes(path: String): ByteArray
 */
static EjsVar *readBytes(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprFile         *file;
    EjsByteArray    *result;
    cchar           *path;
    char            buffer[MPR_BUFSIZE];
    int             bytes, offset, rc;

    mprAssert(argc == 1 && ejsIsString(argv[0]));
    path = ejsGetString(argv[0]);

    file = mprOpen(ejs, path, O_RDONLY | O_BINARY, 0);
    if (file == 0) {
        ejsThrowIOError(ejs, "Can't open %s", path);
        return 0;
    }

    result = ejsCreateByteArray(ejs, (int) mprGetFileSize(file));
    if (result == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    rc = 0;
    offset = 0;
    while ((bytes = mprRead(file, buffer, MPR_BUFSIZE)) > 0) {
        if (ejsCopyToByteArray(ejs, result, offset, buffer, bytes) < 0) {
            ejsThrowMemoryError(ejs);
            rc = -1;
            break;
        }
        offset += bytes;
    }
    ejsSetByteArrayPositions(ejs, result, 0, offset);

    mprFree(file);
    return (EjsVar*) result;
}


/**
    Read the file contents as an array of lines.

    static function readLines(path: String): Array
 */
static EjsVar *readLines(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprFile     *file;
    MprBuf      *data;
    EjsArray    *result;
    cchar       *path;
    char        *start, *end, *cp, buffer[MPR_BUFSIZE];
    int         bytes, rc, lineno;

    mprAssert(argc == 1 && ejsIsString(argv[0]));
    path = ejsGetString(argv[0]);

    result = ejsCreateArray(ejs, 0);
    if (result == NULL) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    file = mprOpen(ejs, path, O_RDONLY | O_BINARY, 0);
    if (file == 0) {
        ejsThrowIOError(ejs, "Can't open %s", path);
        return 0;
    }

    data = mprCreateBuf(ejs, 0, (int) mprGetFileSize(file) + 1);
    result = ejsCreateArray(ejs, 0);
    if (result == NULL || data == NULL) {
        ejsThrowMemoryError(ejs);
        mprFree(file);
        return 0;
    }

    rc = 0;
    while ((bytes = mprRead(file, buffer, MPR_BUFSIZE)) > 0) {
        if (mprPutBlockToBuf(data, buffer, bytes) != bytes) {
            ejsThrowMemoryError(ejs);
            rc = -1;
            break;
        }
    }

    start = mprGetBufStart(data);
    end = mprGetBufEnd(data);
    for (lineno = 0, cp = start; cp < end; cp++) {
        if (*cp == '\n') {
            if (ejsSetProperty(ejs, (EjsVar*) result, lineno++, 
                    (EjsVar*) ejsCreateStringWithLength(ejs, start, (int) (cp - start))) < 0) {
                break;
            }
            start = cp + 1;
        } else if (*cp == '\r') {
            start = cp + 1;
        }
    }
    if (cp > start) {
        ejsSetProperty(ejs, (EjsVar*) result, lineno++, (EjsVar*) ejsCreateStringWithLength(ejs, start, (int) (cp - start)));
    }

    mprFree(file);
    mprFree(data);

    return (EjsVar*) result;
}


/**
    Read the file contents as a string

    static function readString(path: String): String
 */
static EjsVar *readFileAsString(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprFile     *file;
    MprBuf      *data;
    EjsVar      *result;
    cchar       *path;
    char        buffer[MPR_BUFSIZE];
    int         bytes, rc;

    mprAssert(argc == 1 && ejsIsString(argv[0]));
    path = ejsGetString(argv[0]);

    file = mprOpen(ejs, path, O_RDONLY | O_BINARY, 0);
    if (file == 0) {
        ejsThrowIOError(ejs, "Can't open %s", path);
        return 0;
    }

    data = mprCreateBuf(ejs, 0, (int) mprGetFileSize(file) + 1);
    if (data == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }

    rc = 0;
    while ((bytes = mprRead(file, buffer, MPR_BUFSIZE)) > 0) {
        if (mprPutBlockToBuf(data, buffer, bytes) != bytes) {
            ejsThrowMemoryError(ejs);
            rc = -1;
            break;
        }
    }
    result = (EjsVar*) ejsCreateStringWithLength(ejs, mprGetBufStart(data),  mprGetBufLength(data));
    mprFree(file);
    mprFree(data);
    return result;
}


/*
    Get the file contents as an XML object

    static function readXML(path: String): XML
 */
static EjsVar *readXML(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return 0;
}
#endif

/*
    Return a relative path name for the file.

    function get relativePath(): Path
 */
static EjsVar *relativePath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetRelPath(fp, fp->path));
}


/*
    Remove the file associated with the File object. This may be a file or directory.

    function remove(): void
 */
static EjsVar *removePath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprPath     info;

    if (mprGetPathInfo(ejs, fp->path, &info) == 0) {
        if (mprDeletePath(ejs, fp->path) < 0) {
            ejsThrowIOError(ejs, "Cant remove %s", fp->path);
        }
    }
    return 0;
}


/*
    Rename the file

    function rename(to: String): Void
 */
static EjsVar *renamePathFile(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    char    *to;

    mprAssert(argc == 1 && ejsIsString(argv[0]));
    to = ejsGetString(argv[0]);

    unlink(to);
    if (rename(fp->path, to) < 0) {
        ejsThrowIOError(ejs, "Cant rename file %s to %s", fp->path, to);
        return 0;
    }
    return 0;
}


static char *getPathString(Ejs *ejs, EjsVar *vp)
{
    if (ejsIsString(vp)) {
        return (char*) ejsGetString(vp);
    } else if (ejsIsPath(vp)) {
        return ((EjsPath*) vp)->path;
    }
    ejsThrowIOError(ejs, "Bad path");
    return NULL;
}

/*
    Resolve paths against others. Returns a normalized path.
  
    function resolve(...paths): Path
 */
static EjsVar *resolvePath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    EjsArray    *args;
    cchar       *next;
    char        *result, *prior;
    int         i;

    args = (EjsArray*) argv[0];
    result = fp->path;
    for (i = 0; i < args->length; i++) {
        if ((next = getPathString(ejs, ejsGetProperty(ejs, (EjsVar*) args, i))) == NULL) {
            return 0;
        }
        prior = result;
        result = mprResolvePath(ejs, prior, next);
        if (prior != fp->path) {
            mprFree(prior);
        }
    }
    return (EjsVar*) ejsCreatePath(ejs, result);
}


/*
    Return true if the paths refer to the same file.

    function same(other: Object): Boolean
 */
static EjsVar *isPathSame(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    cchar   *other;

    if (ejsIsString(argv[0])) {
        other = ejsGetString(argv[0]);
    } else if (ejsIsPath(argv[0])) {
        other = ((EjsPath*) (argv[0]))->path;
    } else {
        return (EjsVar*) ejs->falseValue;
    }
    return (EjsVar*) (mprSamePath(ejs, fp->path, other) ? ejs->trueValue : ejs->falseValue);
}


/*
    function get separator(): String
 */
static EjsVar *pathSeparator(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprFileSystem   *fs;
    cchar           *cp;

    if ((cp = mprGetFirstPathSeparator(ejs, fp->path)) != 0) {
        return (EjsVar*) ejsCreateStringAndFree(ejs, mprAsprintf(ejs, -1, "%c", (int) *cp));
    }
    fs = mprLookupFileSystem(ejs, fp->path);
    return (EjsVar*) ejsCreateStringAndFree(ejs, mprAsprintf(ejs, -1, "%c", (int) fs->separators[0]));
}


/*
    Get the size of the file associated with this Path

    intrinsic function get size(): Number
 */
static EjsVar *getPathFileSize(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    if (mprGetPathInfo(ejs, fp->path, &fp->info) < 0) {
        return (EjsVar*) ejs->minusOneValue;
    }
    return (EjsVar*) ejsCreateNumber(ejs, (MprNumber) fp->info.size);
}


/*
    function toString(): String
 */
static EjsVar *pathToJSON(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    EjsVar  *result;
    MprBuf  *buf;
    int     i, c, len;

    buf = mprCreateBuf(fp, 0, 0);
    len = strlen(fp->path);
    mprPutCharToBuf(buf, '"');
    for (i = 0; i < len; i++) {
        c = fp->path[i];
        if (c == '"' || c == '\\') {
            mprPutCharToBuf(buf, '\\');
            mprPutCharToBuf(buf, c);
        } else {
            mprPutCharToBuf(buf, c);
        }
    }
    mprPutCharToBuf(buf, '"');
    mprAddNullToBuf(buf);
    result = (EjsVar*) ejsCreateString(ejs, mprGetBufStart(buf));
    mprFree(buf);
    return result;
}

/*
    function toString(): String
 */
static EjsVar *pathToString(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, fp->path);
}


/*
    function trimExt(): Path
 */
static EjsVar *trimExt(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprTrimPathExtension(ejs, fp->path));
}


/*
    function truncate(size: Number): Void
 */
static EjsVar *truncatePath(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    int     size;

    size = ejsGetInt(argv[0]);
    if (mprTruncatePath(ejs, fp->path, size) < 0) {
        ejsThrowIOError(ejs, "Cant truncate %s", fp->path);
    }
    return 0;
}


#if KEEP
/*
    Put the file contents

    static function write(path: String, permissions: Number, ...args): void
 */
static EjsVar *writeToFile(Ejs *ejs, EjsPath *fp, int argc, EjsVar **argv)
{
    MprFile     *file;
    EjsArray    *args;
    char        *path, *data;
    int         i, bytes, length, permissions;

    mprAssert(argc == 3);

    path = ejsGetString(argv[0]);
    permissions = ejsGetInt(argv[1]);
    args = (EjsArray*) argv[2];

    /*
     *  Create fails if already present
     */
    mprDeletePath(ejs, path);
    file = mprOpen(ejs, path, O_CREAT | O_WRONLY | O_BINARY, permissions);
    if (file == 0) {
        ejsThrowIOError(ejs, "Cant create %s", path);
        return 0;
    }

    for (i = 0; i < args->length; i++) {
        data = ejsGetString(ejsToString(ejs, ejsGetProperty(ejs, (EjsVar*) args, i)));
        length = (int) strlen(data);
        bytes = mprWrite(file, data, length);
        if (bytes != length) {
            ejsThrowIOError(ejs, "Write error to %s", path);
            break;
        }
    }
    mprFree(file);
    return 0;
}
#endif


static char *getPath(Ejs *ejs, EjsVar *vp)
{
    if (ejsIsString(vp)) {
        return ejsGetString(vp);
    } else if (ejsIsPath(vp)) {
        return ((EjsPath*) vp)->path;
    }
    ejsThrowIOError(ejs, "Bad path");
    return NULL;
}


EjsPath *ejsCreatePath(Ejs *ejs, cchar *path)
{
    EjsPath     *fp;
    EjsVar      *arg;

    fp = (EjsPath*) ejsCreateVar(ejs, ejsGetType(ejs, ES_ejs_io_Path), 0);
    if (fp == 0) {
        return 0;
    }
    arg = (EjsVar*) ejsCreateString(ejs, path);
    pathConstructor(ejs, fp, 1, (EjsVar**) &arg);
    return fp;
}


EjsPath *ejsCreatePathAndFree(Ejs *ejs, char *value)
{
    EjsPath     *path;

    path = ejsCreatePath(ejs, value);
    mprFree(value);
    return path;
}


void ejsCreatePathType(Ejs *ejs)
{
    EjsName     qname;
    EjsType     *type;

    type = ejsCreateCoreType(ejs, ejsName(&qname, "ejs.io", "Path"), ejs->objectType, sizeof(EjsPath), ES_ejs_io_Path,
        ES_ejs_io_Path_NUM_CLASS_PROP, ES_ejs_io_Path_NUM_INSTANCE_PROP, 
        EJS_ATTR_NATIVE | EJS_ATTR_OBJECT | EJS_ATTR_HAS_CONSTRUCTOR | EJS_ATTR_OBJECT_HELPERS);
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokePathOperator;
    type->helpers->destroyVar = (EjsDestroyVarHelper) destroyPath;
}


void ejsConfigurePathType(Ejs *ejs)
{
    EjsType     *type;

    if ((type = ejsGetType(ejs, ES_ejs_io_Path)) == 0) {
        return;
    }

    ejsBindMethod(ejs, type, ES_ejs_io_Path_Path, (EjsNativeFunction) pathConstructor);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_absolute, (EjsNativeFunction) absolutePath);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_accessed, (EjsNativeFunction) getAccessedDate);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_basename, (EjsNativeFunction) getPathBasename);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_components, (EjsNativeFunction) getPathComponents);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_copy, (EjsNativeFunction) copyPath);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_created, (EjsNativeFunction) getCreatedDate);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_dirname, (EjsNativeFunction) getPathDirname);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_exists, (EjsNativeFunction) getPathExists);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_extension, (EjsNativeFunction) getPathExtension);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_files, (EjsNativeFunction) getPathFiles);
    ejsBindMethod(ejs, type, ES_Object_get, (EjsNativeFunction) getPathIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, (EjsNativeFunction) getPathValues);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_hasDrive, (EjsNativeFunction) pathHasDrive);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_isAbsolute, (EjsNativeFunction) isPathAbsolute);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_isDir, (EjsNativeFunction) isPathDir);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_isLink, (EjsNativeFunction) isPathLink);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_isRegular, (EjsNativeFunction) isPathRegular);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_isRelative, (EjsNativeFunction) isPathRelative);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_join, (EjsNativeFunction) joinPath);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_joinExt, (EjsNativeFunction) joinPathExt);
    ejsBindMethod(ejs, type, ES_Object_length, (EjsNativeFunction) pathLength);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_linkTarget, (EjsNativeFunction) pathLinkTarget);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_makeDir, (EjsNativeFunction) makePathDir);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_makeLink, (EjsNativeFunction) makePathLink);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_makeTemp, (EjsNativeFunction) makePathTemp);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_map, (EjsNativeFunction) pa_map);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_mimeType, (EjsNativeFunction) pa_mimeType);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_modified, (EjsNativeFunction) getModifiedDate);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_name, (EjsNativeFunction) getPathName);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_natural, (EjsNativeFunction) getNaturalPath);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_normalize, (EjsNativeFunction) normalizePath);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_parent, (EjsNativeFunction) getPathParent);
#if ES_ejs_io_Path_perms
    ejsBindMethod(ejs, type, ES_ejs_io_Path_perms, (EjsNativeFunction) getPerms);
#endif
#if ES_ejs_io_Path_set_perms
    ejsBindMethod(ejs, type, ES_ejs_io_Path_set_perms, (EjsNativeFunction) setPerms);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_Path_portable, (EjsNativeFunction) getPortablePath);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_relative, (EjsNativeFunction) relativePath);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_remove, (EjsNativeFunction) removePath);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_rename, (EjsNativeFunction) renamePathFile);
#if ES_ejs_io_Path_resolve
    ejsBindMethod(ejs, type, ES_ejs_io_Path_resolve, (EjsNativeFunction) resolvePath);
#endif
    ejsBindMethod(ejs, type, ES_ejs_io_Path_same, (EjsNativeFunction) isPathSame);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_separator, (EjsNativeFunction) pathSeparator);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_size, (EjsNativeFunction) getPathFileSize);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_trimExt, (EjsNativeFunction) trimExt);
    ejsBindMethod(ejs, type, ES_ejs_io_Path_truncate, (EjsNativeFunction) truncatePath);

    ejsBindMethod(ejs, type, ES_Object_toJSON, (EjsNativeFunction) pathToJSON);
    ejsBindMethod(ejs, type, ES_Object_toString, (EjsNativeFunction) pathToString);
}


#endif /* ES_Path */
/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://www.embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/io/ejsPath.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/sys/ejsApp.c"
 */
/************************************************************************/

/*
 *  ejsApp.c -- App class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Get the application command line arguments
 *
 *  static function get args(): String
 */
static EjsVar *getArgs(Ejs *ejs, EjsObject *unused, int argc, EjsVar **argv)
{
    EjsArray    *args;
    int         i;

    args = ejsCreateArray(ejs, ejs->argc);
    for (i = 0; i < ejs->argc; i++) {
        ejsSetProperty(ejs, (EjsVar*) args, i, (EjsVar*) ejsCreateString(ejs, ejs->argv[i]));
    }
    return (EjsVar*) args;
}


/*
 *  Get the current working directory
 *
 *  function get dir(): Path
 */
static EjsVar *currentDir(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePathAndFree(ejs, mprGetCurrentPath(ejs));
}


/*
 *  Set the current working directory
 *
 *  function chdir(value: String|Path): void
 */
static EjsVar *changeCurrentDir(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    char    *path;

    mprAssert(argc == 1);

    if (ejsIsPath(argv[0])) {
        path = ((EjsPath*) argv[0])->path;
    } else if (ejsIsString(argv[0])) {
        path = ejsGetString(argv[0]);
    } else {
        ejsThrowIOError(ejs, "Bad path");
        return NULL;
    }

    if (chdir(path) < 0) {
        ejsThrowIOError(ejs, "Can't change the current directory");
    }
    return 0;
}

/*
 *  Get an environment var
 *
 *  function getenv(key: String): String
 */
static EjsVar *getEnvVar(Ejs *ejs, EjsObject *app, int argc, EjsVar **argv)
{
    cchar   *value;

    value = getenv(ejsGetString(argv[0]));
    if (value == 0) {
        return (EjsVar*) ejs->nullValue;
    }
    return (EjsVar*) ejsCreateString(ejs, value);
}


/*
 *  Put an environment var
 *
 *  function putenv(key: String, value: String): void
 */
static EjsVar *putEnvVar(Ejs *ejs, EjsObject *app, int argc, EjsVar **argv)
{

#if !WINCE
#if BLD_UNIX_LIKE
    char    *key, *value;

    key = mprStrdup(ejs, ejsGetString(argv[0]));
    value = mprStrdup(ejs, ejsGetString(argv[1]));
    setenv(key, value, 1);
#else
    char   *cmd;

    cmd = mprStrcat(app, -1, ejsGetString(argv[0]), "=", ejsGetString(argv[1]), NULL);
    putenv(cmd);
#endif
#endif
    return 0;
}


static EjsVar *getErrorStream(Ejs *ejs, EjsObject *app, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateFileFromFd(ejs, 2, "stderr", O_WRONLY);
}


#if ES_ejs_sys_App_exeDir
/*
 *  Get the directory containing the application's executable file.
 *
 *  static function get exeDir(): Path
 */
static EjsVar *exeDir(Ejs *ejs, EjsObject *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePath(ejs, mprGetAppDir(ejs));
}
#endif


#if ES_ejs_sys_App_exePath
/*
 *  Get the application's executable filename.
 *
 *  static function get exePath(): Path
 */
static EjsVar *exePath(Ejs *ejs, EjsObject *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreatePath(ejs, mprGetAppPath(ejs));
}
#endif


/**
 *  Exit the application
 *
 *  static function exit(status: Number): void
 */
static EjsVar *exitApp(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    int     status;

    status = argc == 0 ? 0 : ejsGetInt(argv[0]);
    mprBreakpoint();
    if (status != 0) {
        exit(status);
    } else {
        mprTerminate(mprGetMpr(ejs), 1);
    }
    return 0;
}


static EjsVar *getInputStream(Ejs *ejs, EjsObject *app, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateFileFromFd(ejs, 0, "stdin", O_RDONLY);
}


/**
 *  Control if the application will exit when the last script completes.
 *
 *  static function noexit(exit: Boolean): void
 */
static EjsVar *noexit(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    ejs->flags |= EJS_FLAG_NOEXIT;
    return 0;
}


/*
 *  Get the ejs module search path (EJSPATH). Does not actually read the environment.
 *
 *  function get searchPath(): String
 */
static EjsVar *getSearchPath(Ejs *ejs, EjsObject *app, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, ejs->ejsPath);
}


/*
 *  Set the ejs module search path (EJSPATH). Does not actually update the environment.
 *
 *  function set searchPath(path: String): Void
 */
static EjsVar *setSearchPath(Ejs *ejs, EjsObject *app, int argc, EjsVar **argv)
{
    ejsSetSearchPath(ejs, ejsGetString(argv[0]));
    return 0;
}


static EjsVar *getOutputStream(Ejs *ejs, EjsObject *app, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateFileFromFd(ejs, 1, "stdout", O_WRONLY);
}


EjsVar *ejsServiceEvents(Ejs *ejs, int count, int timeout, int flags)
{
    MprTime     mark, remaining;
    int         rc;

    if (count < 0) {
        count = MAXINT;
    }
    if (timeout < 0) {
        timeout = MAXINT;
    }
    mark = mprGetTime(ejs);
    do {
        rc = mprServiceEvents(ejs->dispatcher, timeout, MPR_SERVICE_EVENTS | MPR_SERVICE_ONE_THING);
        if (rc > 0) {
            count -= rc;
        }
        remaining = mprGetRemainingTime(ejs, mark, timeout);
    } while (count > 0 && remaining > 0 && !mprIsExiting(ejs) && !ejs->exiting);
    return 0;
}


/*
 *  static function serviceEvents(count: Number = -1, timeout: Number = -1): void
 */
static EjsVar *serviceEvents(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    int     count, timeout;

    count = (argc > 1) ? ejsGetInt(argv[0]) : MAXINT;
    timeout = (argc > 1) ? ejsGetInt(argv[1]) : MAXINT;
    ejsServiceEvents(ejs, count, timeout, MPR_SERVICE_EVENTS | MPR_SERVICE_ONE_THING);
    return 0;
}


#if ES_ejs_sys_App_sleep
/**
 *  Pause the application
 *
 *  static function sleep(delay: Number = -1): void
 */
static EjsVar *sleepProc(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    int     timeout;

    timeout = (argc > 0) ? ejsGetInt(argv[0]): MAXINT;
    ejsServiceEvents(ejs, -1, timeout, MPR_SERVICE_EVENTS | MPR_SERVICE_ONE_THING);
    return 0;
}
#endif



void ejsCreateAppType(Ejs *ejs)
{
    EjsName     qname;

    ejsCreateCoreType(ejs, ejsName(&qname, "ejs.sys", "App"), ejs->objectType, sizeof(EjsObject), ES_ejs_sys_App,
        ES_ejs_sys_App_NUM_CLASS_PROP, ES_ejs_sys_App_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_OBJECT_HELPERS);
}


void ejsConfigureAppType(Ejs *ejs)
{
    EjsType         *type;

    type = ejsGetType(ejs, ES_ejs_sys_App);

    ejsBindMethod(ejs, type, ES_ejs_sys_App_args, (EjsNativeFunction) getArgs);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_dir, (EjsNativeFunction) currentDir);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_chdir, (EjsNativeFunction) changeCurrentDir);
#if ES_ejs_sys_App_exeDir
    ejsBindMethod(ejs, type, ES_ejs_sys_App_exeDir, (EjsNativeFunction) exeDir);
#endif
#if ES_ejs_sys_App_exePath
    ejsBindMethod(ejs, type, ES_ejs_sys_App_exePath, (EjsNativeFunction) exePath);
#endif
    ejsBindMethod(ejs, type, ES_ejs_sys_App_exit, (EjsNativeFunction) exitApp);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_errorStream, (EjsNativeFunction) getErrorStream);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_getenv, (EjsNativeFunction) getEnvVar);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_putenv, (EjsNativeFunction) putEnvVar);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_inputStream, (EjsNativeFunction) getInputStream);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_noexit, (EjsNativeFunction) noexit);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_outputStream, (EjsNativeFunction) getOutputStream);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_searchPath, (EjsNativeFunction) getSearchPath);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_set_searchPath, (EjsNativeFunction) setSearchPath);
    ejsBindMethod(ejs, type, ES_ejs_sys_App_serviceEvents, (EjsNativeFunction) serviceEvents);
#if ES_ejs_sys_App_sleep
    ejsBindMethod(ejs, type, ES_ejs_sys_App_sleep, (EjsNativeFunction) sleepProc);
#endif
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/sys/ejsApp.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/sys/ejsConfig.c"
 */
/************************************************************************/

/*
 *  ejsConfig.c -- Config class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




void ejsCreateConfigType(Ejs *ejs)
{
    EjsName     qname;

    ejsCreateCoreType(ejs, ejsName(&qname, "ejs.sys", "Config"), ejs->objectType, sizeof(EjsObject), 
        ES_ejs_sys_Config, ES_ejs_sys_Config_NUM_CLASS_PROP, ES_ejs_sys_Config_NUM_INSTANCE_PROP, 
        EJS_ATTR_NATIVE | EJS_ATTR_OBJECT_HELPERS);
}


void ejsConfigureConfigType(Ejs *ejs)
{
    EjsVar      *vp;
    char        version[16];

    vp = (EjsVar*) ejsGetType(ejs, ES_ejs_sys_Config);
    if (vp == 0) {
        return;
    }
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Debug, BLD_DEBUG ? (EjsVar*) ejs->trueValue: (EjsVar*) ejs->falseValue);
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_CPU, (EjsVar*) ejsCreateString(ejs, BLD_HOST_CPU));
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_DB, BLD_FEATURE_EJS_DB ? (EjsVar*) ejs->trueValue: (EjsVar*) ejs->falseValue);
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_E4X, BLD_FEATURE_EJS_E4X ? (EjsVar*) ejs->trueValue: 
        (EjsVar*) ejs->falseValue);
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Floating, 
        BLD_FEATURE_FLOATING_POINT ? (EjsVar*) ejs->trueValue: (EjsVar*) ejs->falseValue);
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Http, BLD_FEATURE_HTTP ? (EjsVar*) ejs->trueValue: (EjsVar*) ejs->falseValue);

    if (BLD_FEATURE_EJS_LANG == EJS_SPEC_ECMA) {
        ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Lang, (EjsVar*) ejsCreateString(ejs, "ecma"));
    } else if (BLD_FEATURE_EJS_LANG == EJS_SPEC_PLUS) {
        ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Lang, (EjsVar*) ejsCreateString(ejs, "plus"));
    } else {
        ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Lang, (EjsVar*) ejsCreateString(ejs, "fixed"));
    }

    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Legacy, 
        BLD_FEATURE_LEGACY_API ? (EjsVar*) ejs->trueValue: (EjsVar*) ejs->falseValue);
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Multithread, 
        BLD_FEATURE_MULTITHREAD ? (EjsVar*) ejs->trueValue: (EjsVar*) ejs->falseValue);

    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_NumberType, 
        (EjsVar*) ejsCreateString(ejs, BLD_FEATURE_NUM_TYPE_STRING));

    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_OS, (EjsVar*) ejsCreateString(ejs, BLD_OS));
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Product, (EjsVar*) ejsCreateString(ejs, BLD_PRODUCT));
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_RegularExpressions, 
        BLD_FEATURE_REGEXP ? (EjsVar*) ejs->trueValue: (EjsVar*) ejs->falseValue);
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Title, (EjsVar*) ejsCreateString(ejs, BLD_NAME));

    mprSprintf(version, sizeof(version), "%s-%s", BLD_VERSION, BLD_NUMBER);
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_Version, (EjsVar*) ejsCreateString(ejs, version));

#if BLD_WIN_LIKE
{
    char    *path;

    /*
     *  Users may install Ejscript in a different location
     */
    path = mprGetAppDir(ejs);
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_BinDir, (EjsVar*) ejsCreatePath(ejs, path));
#if ES_ejs_sys_Config_ModDir
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_ModDir, (EjsVar*) ejsCreatePath(ejs, path));
#endif
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_LibDir, (EjsVar*) ejsCreatePath(ejs, path));
}
#else
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_BinDir, (EjsVar*) ejsCreatePath(ejs, BLD_BIN_PREFIX));
#if ES_ejs_sys_Config_ModDir
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_ModDir, (EjsVar*) ejsCreatePath(ejs, BLD_MOD_PREFIX));
#endif
    ejsSetProperty(ejs, vp, ES_ejs_sys_Config_LibDir, (EjsVar*) ejsCreatePath(ejs, BLD_LIB_PREFIX));
#endif
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/sys/ejsConfig.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/sys/ejsDebug.c"
 */
/************************************************************************/

/*
 *  ejsDebug.c - System.Debug class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if KEEP
/*
 *  function bool isDebugMode()
 */

static int isDebugMode(EjsFiber *fp, EjsVar *thisObj, int argc, EjsVar **argv)
{
    ejsTrace(fp, "isDebugMode()\n");
    ejsSetReturnValueToInteger(fp, mprGetDebugMode(fp));
    return 0;
}


int ejsDefineDebugClass(EjsFiber *fp)
{
    EjsVar  *systemDebugClass;

    systemDebugClass =  ejsDefineClass(fp, "System.Debug", "Object", 0);
    if (systemDebugClass == 0) {
        return MPR_ERR_CANT_INITIALIZE;
    }

    /*
     *  Define the class methods
     */
    ejsDefineCMethod(fp, systemDebugClass, "isDebugMode", isDebugMode,
        0);

    return ejsObjHasErrors(systemDebugClass) ? MPR_ERR_CANT_INITIALIZE : 0;
}
#else
void __dummyEjsDebug() {}
#endif

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/sys/ejsDebug.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/sys/ejsGC.c"
 */
/************************************************************************/

/**
 *  ejsGC.c - Garbage collector class for the EJS Object Model
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  native static function get enabled(): Boolean
 */
static EjsVar *getEnable(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    return (EjsVar*) ((ejs->gc.enabled) ? ejs->trueValue: ejs->falseValue);
}


/*
 *  native static function set enabled(on: Boolean): Void
 */
static EjsVar *setEnable(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsBoolean(argv[0]));
    ejs->gc.enabled = ejsGetBoolean(argv[0]);
    return 0;
}


/*
 *  run(deep: Boolean = false)
 *  Note: deep currently is not implemented
 */
static EjsVar *runGC(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    int     deep;

    deep = ((argc == 1) && ejsIsBoolean(argv[1]));
    ejsCollectGarbage(ejs, EJS_GEN_NEW);
    return 0;
}


/*
 *  native static function get workQuota(): Number
 */
static EjsVar *getWorkQuota(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, ejs->workQuota);
}


/*
 *  native static function set workQuota(quota: Number): Void
 */
static EjsVar *setWorkQuota(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    int     quota;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));
    quota = ejsGetInt(argv[0]);

    if (quota < EJS_GC_SHORT_WORK_QUOTA && quota != 0) {
        ejsThrowArgError(ejs, "Bad work quota");
        return 0;
    }
    ejs->workQuota = quota;
    return 0;
}



void ejsCreateGCType(Ejs *ejs)
{
    EjsName     qname;

    ejsCreateCoreType(ejs, ejsName(&qname, "ejs.sys", "GC"), ejs->objectType, sizeof(EjsObject), ES_ejs_sys_GC,
        ES_ejs_sys_GC_NUM_CLASS_PROP, ES_ejs_sys_GC_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_OBJECT_HELPERS);
}


void ejsConfigureGCType(Ejs *ejs)
{
    EjsType         *type;

    type = ejsGetType(ejs, ES_ejs_sys_GC);

    ejsBindMethod(ejs, type, ES_ejs_sys_GC_enabled, (EjsNativeFunction) getEnable);
    ejsBindMethod(ejs, type, ES_ejs_sys_GC_set_enabled, (EjsNativeFunction) setEnable);
    ejsBindMethod(ejs, type, ES_ejs_sys_GC_workQuota, (EjsNativeFunction) getWorkQuota);
    ejsBindMethod(ejs, type, ES_ejs_sys_GC_set_workQuota, (EjsNativeFunction) setWorkQuota);
    ejsBindMethod(ejs, type, ES_ejs_sys_GC_run, (EjsNativeFunction) runGC);
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/sys/ejsGC.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/sys/ejsLogger.c"
 */
/************************************************************************/

/*
 *  ejsLogger.c - Logger class 
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if KEEP
/*
 *  System.Log.setLog(path);
 *  System.Log.enable;
 */

static void logHandler(MPR_LOC_DEC(ctx, loc), int flags, int level, 
    const char *msg)
{
    Mpr     *app;
    char    *buf;

    app = mprGetApp(ctx);
    if (app->logFile == 0) {
        return;
    }

    if (flags & MPR_LOG_SRC) {
        buf = mprAsprintf(MPR_LOC_PASS(ctx, loc), -1, "Log %d: %s\n", level, msg);

    } else if (flags & MPR_ERROR_SRC) {
        buf = mprAsprintf(MPR_LOC_PASS(ctx, loc), -1, "Error: %s\n", msg);

    } else if (flags & MPR_FATAL_SRC) {
        buf = mprAsprintf(MPR_LOC_PASS(ctx, loc), -1, "Fatal: %s\n", msg);
        
    } else if (flags & MPR_ASSERT_SRC) {
#if BLD_FEATURE_ALLOC_LEAK_TRACK
        buf = mprAsprintf(MPR_LOC_PASS(ctx, loc), -1, "Assertion %s, failed at %s\n", msg, loc);
#else
        buf = mprAsprintf(MPR_LOC_PASS(ctx, loc), -1, "Assertion %s, failed\n", msg);
#endif

    } else if (flags & MPR_RAW) {
        buf = mprAsprintf(MPR_LOC_PASS(ctx, loc), -1, "%s", msg);

    } else {
        return;
    }
    mprPuts(app->logFile, buf, strlen(buf));
    mprFree(buf);
}


/*
 *  function int setLog(string path)
 */

static int setLog(EjsFiber *fp, EjsVar *thisObj, int argc, EjsVar **argv)
{
    const char  *path;
    MprFile     *file;
    Mpr     *app;

    if (argc != 1 || !ejsVarIsString(argv[0])) {
        ejsArgError(fp, "Usage: setLog(path)");
        return -1;
    }

    app = mprGetApp(fp);

    /*
     *  Ignore errors if we can't create the log file.
     *  Use the app context so this will live longer than the interpreter
     *  BUG -- this leaks files.
     */
    path = argv[0]->string;
    file = mprOpen(app, path, O_CREAT | O_TRUNC | O_WRONLY, 0664);
    if (file) {
        app->logFile = file;
        mprSetLogHandler(fp, logHandler);
    }
    mprLog(fp, 0, "Test log");

    return 0;
}

#if KEEP

static int enableSetAccessor(EjsFiber *fp, EjsVar *thisObj, int argc, EjsVar **argv)
{
    if (argc != 1) {
        ejsArgError(fp, "Usage: set(value)");
        return -1;
    }
    ejsSetProperty(fp, thisObj, "_enabled", argv[0]);
    return 0;
}

static int enableGetAccessor(EjsFiber *fp, EjsVar *thisObj, int argc, EjsVar **argv)
{
    ejsSetReturnValue(fp, ejsGetPropertyAsVar(fp, thisObj, "_enabled"));
    return 0;
}

#endif

int ejsDefineLogClass(EjsFiber *fp)
{
    EjsVar          *logClass;

    logClass =  ejsDefineClass(fp, "System.Log", "Object", 0);
    if (logClass == 0) {
        return MPR_ERR_CANT_INITIALIZE;
    }

    ejsDefineCMethod(fp, logClass, "setLog", setLog, 0);

#if KEEP
    EjsProperty     *pp;
    ejsDefineCAccessors(fp, logClass, "enable", enableSetAccessor, 
        enableGetAccessor, 0);

    pp = ejsSetPropertyToBoolean(fp, logClass, "_enabled", 0);
    ejsMakePropertyEnumerable(pp, 0);
#endif

    return ejsObjHasErrors(logClass) ? MPR_ERR_CANT_INITIALIZE : 0;
}

#else
void __dummyEjsLog() {}
#endif

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/sys/ejsLogger.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/sys/ejsMemory.c"
 */
/************************************************************************/

/*
 *  ejsMemory.c - Memory class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */


/*
 *  native static function get allocated(): Number
 */
static EjsVar *getAllocatedMemory(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    MprAlloc    *alloc;

    alloc = mprGetAllocStats(ejs);
    return (EjsVar*) ejsCreateNumber(ejs, (int) alloc->bytesAllocated);
}


/*
 *  native static function callback(fn: Function): Void
 */
static EjsVar *setRedlineCallback(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    mprAssert(argc == 1 && ejsIsFunction(argv[0]));

    if (!ejsIsFunction(argv[0])) {
        ejsThrowArgError(ejs, "Callaback is not a function");
        return 0;
    }
    ejs->memoryCallback = (EjsFunction*) argv[0];
    return 0;
}


/*
 *  native static function get maximum(): Number
 */
static EjsVar *getMaxMemory(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    MprAlloc    *alloc;

    alloc = mprGetAllocStats(ejs);
    return (EjsVar*) ejsCreateNumber(ejs, (int) alloc->maxMemory);
}


/*
 *  native static function set maximum(limit: Number): Void
 */
static EjsVar *setMaxMemory(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    int     maxMemory;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));

    maxMemory = ejsGetInt(argv[0]);
    mprSetAllocLimits(ejs, -1, maxMemory);
    return 0;
}


/*
 *  native static function get peak(): Number
 */
static EjsVar *getPeakMemory(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    MprAlloc    *alloc;

    alloc = mprGetAllocStats(ejs);
    return (EjsVar*) ejsCreateNumber(ejs, (int) alloc->peakAllocated);
}


/*
 *  native static function get redline(): Number
 */
static EjsVar *getRedline(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    MprAlloc    *alloc;

    alloc = mprGetAllocStats(ejs);
    return (EjsVar*) ejsCreateNumber(ejs, (int) alloc->redLine);
}


/*
 *  native static function set redline(limit: Number): Void
 */
static EjsVar *setRedline(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    int     redline;

    mprAssert(argc == 1 && ejsIsNumber(argv[0]));

    redline = ejsGetInt(argv[0]);
    if (redline <= 0) {
        redline = INT_MAX;
    }
    mprSetAllocLimits(ejs, redline, -1);
    return 0;
}


/*
 *  native static function get resident(): Number
 */
static EjsVar *getResident(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    MprAlloc    *alloc;

    alloc = mprGetAllocStats(ejs);
    return (EjsVar*) ejsCreateNumber(ejs, (int) alloc->rss);
}


/*
 *  native static function get stack(): Number
 */
static EjsVar *getStack(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    MprAlloc    *alloc;

    alloc = mprGetAllocStats(ejs);
    return (EjsVar*) ejsCreateNumber(ejs, (int) alloc->peakStack);
}


/*
 *  native static function get system(): Number
 */
static EjsVar *getSystemRam(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    MprAlloc    *alloc;

    alloc = mprGetAllocStats(ejs);
    return (EjsVar*) ejsCreateNumber(ejs, (double) alloc->ram);
}


/*
 *  native static function stats(): Void
 */
static EjsVar *printStats(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    ejsPrintAllocReport(ejs);
    mprPrintAllocReport(ejs, "Memroy Report");
    return 0;
}



void ejsCreateMemoryType(Ejs *ejs)
{
    EjsName     qname;

    ejsCreateCoreType(ejs, ejsName(&qname, "ejs.sys", "Memory"), ejs->objectType, sizeof(EjsObject), ES_ejs_sys_Memory,
        ES_ejs_sys_Memory_NUM_CLASS_PROP, ES_ejs_sys_Memory_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_OBJECT_HELPERS);
}


void ejsConfigureMemoryType(Ejs *ejs)
{
    EjsType         *type;

    type = ejsGetType(ejs, ES_ejs_sys_Memory);

    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_allocated, (EjsNativeFunction) getAllocatedMemory);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_set_callback, (EjsNativeFunction) setRedlineCallback);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_maximum, (EjsNativeFunction) getMaxMemory);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_set_maximum, (EjsNativeFunction) setMaxMemory);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_peak, (EjsNativeFunction) getPeakMemory);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_redline, (EjsNativeFunction) getRedline);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_set_redline, (EjsNativeFunction) setRedline);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_resident, (EjsNativeFunction) getResident);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_stack, (EjsNativeFunction) getStack);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_system, (EjsNativeFunction) getSystemRam);
    ejsBindMethod(ejs, type, ES_ejs_sys_Memory_stats, (EjsNativeFunction) printStats);
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/sys/ejsMemory.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/sys/ejsSystem.c"
 */
/************************************************************************/

/*
 *  ejsSystem.c -- System class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_CMD
#if ES_ejs_sys_System_run
/*
 *  function run(cmd: String): String
 */
static EjsVar *run(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprCmd      *cmd;
    EjsString   *result;
    char        *cmdline, *err, *output;
    int         status;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    cmd = mprCreateCmd(ejs);
    cmdline = ejsGetString(argv[0]);
    status = mprRunCmd(cmd, cmdline, &output, &err, 0);
    if (status) {
        ejsThrowError(ejs, "Command failed: %s\n\nExit status: %d\n\nError Output: \n%s\nPrevious Output: \n%s\n", 
            cmdline, status, err, output);
        mprFree(cmd);
        return 0;
    }
    result = ejsCreateString(ejs, output);
    mprFree(cmd);
    return (EjsVar*) result;
}
#endif


#if ES_ejs_sys_System_runx
/*
 *  function runx(cmd: String): Void
 */
static EjsVar *runx(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprCmd      *cmd;
    char        *err;
    int         status;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    cmd = mprCreateCmd(ejs);
    status = mprRunCmd(cmd, ejsGetString(argv[0]), NULL, &err, 0);
    if (status) {
        ejsThrowError(ejs, "Can't run command: %s\nDetails: %s", ejsGetString(argv[0]), err);
        mprFree(err);
    }
    mprFree(cmd);
    return 0;
}
#endif


#if ES_ejs_sys_System_daemon
/*
 *  function daemon(cmd: String): Number
 */
static EjsVar *runDaemon(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    MprCmd      *cmd;
    int         status, pid;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    cmd = mprCreateCmd(ejs);
    status = mprRunCmd(cmd, ejsGetString(argv[0]), NULL, NULL, MPR_CMD_DETACH);
    if (status) {
        ejsThrowError(ejs, "Can't run command: %s", ejsGetString(argv[0]));
    }
    pid = cmd->pid;
    mprFree(cmd);
    return (EjsVar*) ejsCreateNumber(ejs, pid);
}
#endif


#if ES_ejs_sys_System_exec
/*
 *  function exec(cmd: String): Void
 */
static EjsVar *execCmd(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
#if BLD_UNIX_LIKE
    char    **argVector;
    int     argCount;

    mprMakeArgv(ejs, NULL, ejsGetString(argv[0]), &argCount, &argVector);
    execv(argVector[0], argVector);
#endif
    ejsThrowStateError(ejs, "Can't exec %s", ejsGetString(argv[0]));
    return 0;
}
#endif
#endif

/*
 *  function get hostname(): String
 */
static EjsVar *system_hostname(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateStringAndFree(ejs, mprStrdup(ejs, mprGetHostName(ejs)));
}



void ejsCreateSystemType(Ejs *ejs)
{
    EjsName     qname;

    ejsCreateCoreType(ejs, ejsName(&qname, "ejs.sys", "System"), ejs->objectType, sizeof(EjsObject), ES_ejs_sys_System,
        ES_ejs_sys_System_NUM_CLASS_PROP, ES_ejs_sys_System_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_OBJECT_HELPERS);
}


void ejsConfigureSystemType(Ejs *ejs)
{
    EjsType         *type;

    type = ejsGetType(ejs, ES_ejs_sys_System);

#if BLD_FEATURE_CMD
    ejsBindMethod(ejs, type, ES_ejs_sys_System_daemon, (EjsNativeFunction) runDaemon);
    ejsBindMethod(ejs, type, ES_ejs_sys_System_exec, (EjsNativeFunction) execCmd);
    ejsBindMethod(ejs, type, ES_ejs_sys_System_run, (EjsNativeFunction) run);
    ejsBindMethod(ejs, type, ES_ejs_sys_System_runx, (EjsNativeFunction) runx);
#endif
    ejsBindMethod(ejs, type, ES_ejs_sys_System_hostname, (EjsNativeFunction) system_hostname);
}



/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/sys/ejsSystem.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/sys/ejsWorker.c"
 */
/************************************************************************/

/*
 *  ejsWorker - VM Worker thread classes
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_MULTITHREAD

typedef struct Message {
    EjsWorker   *worker;
    cchar       *callback;
    char        *data;
    char        *message;
    char        *filename;
    char        *stack;
    int         lineNumber;
    int         callbackSlot;
} Message;


static void addWorker(Ejs *ejs, EjsWorker *worker);
static int join(Ejs *ejs, EjsVar *workers, int timeout);
static void handleError(Ejs *ejs, EjsWorker *worker, EjsVar *exception);
static void loadFile(EjsWorker *insideWorker, cchar *filename);
static void removeWorker(Ejs *ejs, EjsWorker *worker);
static void workerMain(EjsWorker *worker, MprWorker *mprWorker);
static EjsVar *workerPreload(Ejs *ejs, EjsWorker *worker, int argc, EjsVar **argv);

/*
 *  function Worker(script: String = null, options: Object = null)
 *
 *  Script is optional. If supplied, the script is run immediately by a worker thread. This call
 *  does not block. Options are: search and name.
 */
static EjsVar *workerConstructor(Ejs *ejs, EjsWorker *worker, int argc, EjsVar **argv)
{
    Ejs             *wejs;
    EjsVar          *options, *value;
    EjsName         qname;
    EjsWorker       *self;
    EjsNamespace    *ns;
    cchar           *search, *name;

    worker->ejs = ejs;
    worker->state = EJS_WORKER_BEGIN;

    options = (argc == 2) ? (EjsVar*) argv[1]: NULL;
    name = 0;

    search = ejs->ejsPath;
    if (options) {
        value = ejsGetPropertyByName(ejs, options, ejsName(&qname, "", "search"));
        if (ejsIsString(value)) {
            search = ejsGetString(value);
        }
        value = ejsGetPropertyByName(ejs, options, ejsName(&qname, "", "name"));
        if (ejsIsString(value)) {
            name = ejsGetString(value);
        }
    }

    if (name) {
        worker->name = mprStrdup(worker, name);
    } else {
        worker->name = mprAsprintf(worker, -1, "worker-%d", mprGetListCount(ejs->workers));
    }

    /*
     *  Create a new interpreter and an "inside" worker object and pair it with the current "outside" worker.
     */
    wejs = ejsCreate(ejs->service, NULL, search, 0);
    if (wejs == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    worker->pair = self = ejsCreateWorker(wejs);
    self->state = EJS_WORKER_BEGIN;
    self->ejs = wejs;
    self->inside = 1;
    self->pair = worker;
    self->name = mprStrcat(self, -1, "inside-", worker->name, NULL);

    ejsSetProperty(ejs,  (EjsVar*) worker, ES_ejs_sys_Worker_name, (EjsVar*) ejsCreateString(ejs, self->name));
    ejsSetProperty(wejs, (EjsVar*) self,   ES_ejs_sys_Worker_name, (EjsVar*) ejsCreateString(wejs, self->name));
    ejsSetProperty(wejs, wejs->global, ES_ejs_sys_worker_self, (EjsVar*) self);

    /*
     *  Workers have a dedicated namespace to enable viewing of the worker globals (self, onmessage, postMessage...)
     */
    ns = ejsDefineReservedNamespace(wejs, wejs->globalBlock, 0, EJS_WORKER_NAMESPACE);

    /*
     *  Make the inside worker permanent so we don't need to worry about whether worker->pair->ejs is valid
     */
    self->obj.var.permanent = 1;
    
    if (argc > 0 && ejsIsPath(argv[0])) {
        addWorker(ejs, worker);
        worker->scriptFile = mprStrdup(worker, ((EjsPath*) argv[0])->path);
        worker->state = EJS_WORKER_STARTED;
        worker->obj.var.permanent = 1;
        if (mprStartWorker(ejs, (MprWorkerProc) workerMain, (void*) worker, MPR_NORMAL_PRIORITY) < 0) {
            ejsThrowStateError(ejs, "Can't start worker");
            worker->obj.var.permanent = 0;
            return 0;
        }
    }
    return (EjsVar*) worker;
}


/*
 *  Add a worker object to the list of workers for this interpreter
 */
static void addWorker(Ejs *ejs, EjsWorker *worker) 
{
    mprAssert(ejs == worker->ejs);
    mprAssert(worker);
    mprAssert(!worker->inside);
    mprAssert(worker->state == EJS_WORKER_BEGIN);

    lock(ejs);
    mprAddItem(ejs->workers, worker);
    unlock(ejs);
}


static void removeWorker(Ejs *ejs, EjsWorker *worker) 
{
    mprAssert(ejs == worker->ejs);
    mprAssert(!worker->inside);
    mprAssert(worker);

    lock(ejs);
    mprRemoveItem(ejs->workers, worker);
    if (ejs->joining) {
        mprWakeDispatcher(ejs->dispatcher);
    }
    unlock(ejs);
}


/*
 *  Start a worker thread. This is called by eval() and load(). Not by preload() or by Worker()
 *  It always joins.
 */
static EjsVar *startWorker(Ejs *ejs, EjsWorker *worker, int timeout)
{
    Ejs     *inside;
    EjsVar  *result;

    mprAssert(ejs);
    mprAssert(worker);
    mprAssert(worker->state == EJS_WORKER_BEGIN);

    if (worker->state > EJS_WORKER_BEGIN) {
        ejsThrowStateError(ejs, "Worker has already started");
        return 0;
    }

    mprAssert(worker->pair->state == EJS_WORKER_BEGIN);

    addWorker(ejs, worker);
    worker->state = EJS_WORKER_STARTED;

    worker->obj.var.permanent = 1;
    if (mprStartWorker(ejs, (MprWorkerProc) workerMain, (void*) worker, MPR_NORMAL_PRIORITY) < 0) {
        ejsThrowStateError(ejs, "Can't start worker");
        worker->obj.var.permanent = 0;
        return 0;
    }
    if (timeout == 0) {
        return ejs->undefinedValue;
    } 
    if (timeout < 0) {
        timeout = MAXINT;
    }
    if (join(ejs, (EjsVar*) worker, timeout) < 0) {
        ejsThrowStateError(ejs, "Timeout (%d)", timeout);
        return ejs->undefinedValue;
    }
    mprAssert(worker->pair);
    mprAssert(worker->pair->ejs);
    inside = worker->pair->ejs;
    result = ejsSerialize(ejs, inside->result, -1, 0, 0);
    if (result == 0) {
        return ejs->nullValue;
    }
    return ejsDeserialize(ejs, (EjsString*) result);
}


/*
 *  function eval(script: String, timeout: Boolean = -1): String
 */
static EjsVar *workerEval(Ejs *ejs, EjsWorker *worker, int argc, EjsVar **argv)
{
    int     timeout;

    mprAssert(ejsIsString(argv[0]));

    worker->scriptLiteral = mprStrdup(worker, ejsGetString(argv[0]));
    timeout = argc == 2 ? ejsGetInt(argv[1]): MAXINT;
    return startWorker(ejs, worker, timeout);
}


/*
 *  static function exit()
 */
static EjsVar *workerExit(Ejs *ejs, EjsWorker *unused, int argc, EjsVar **argv)
{
    ejs->exiting = 1;
    ejs->attention = 1;
    return 0;
}


static int reapJoins(Ejs *ejs, EjsVar *workers)
{
    EjsWorker   *worker;
    EjsArray    *set;
    int         i, joined, completed;

    joined = 0;

    lock(ejs);
    if (workers == 0 || workers == ejs->nullValue) {
        completed = 0;
        for (i = 0; i < mprGetListCount(ejs->workers); i++) {
            worker = mprGetItem(ejs->workers, i);
            if (worker->state >= EJS_WORKER_COMPLETE) {
                completed++;
            }
        }
        if (completed == mprGetListCount(ejs->workers)) {
            unlock(ejs);
            return 1;
        }
    } else if (ejsIsArray(workers)) {
        set = (EjsArray*) workers;
        for (i = 0; i < set->length; i++) {
            worker = (EjsWorker*) set->data[i];
            if (worker->state < EJS_WORKER_COMPLETE) {
                break;
            }
        }
        if (i >= set->length) {
            unlock(ejs);
            return 1;
        }
    } else if (workers->type == ejs->workerType) {
        worker = (EjsWorker*) workers;
        if (worker->state >= EJS_WORKER_COMPLETE) {
            unlock(ejs);
            return 1;
        }
    }
    unlock(ejs);
    return 0;
}


static int join(Ejs *ejs, EjsVar *workers, int timeout)
{
    MprTime     mark, remaining;
    int         result, count, total;

    mark = mprGetTime(ejs);
    ejs->joining = 1;

    do {
        /*
         *  Must process all pending messages
         */
        total = 0;
        while ((count = mprServiceEvents(ejs->dispatcher, 0, MPR_SERVICE_EVENTS)) > 0) { 
            total += count;
        }
        ejs->joining = !reapJoins(ejs, workers);
        if (total == 0 && ejs->joining) {
            mprWaitForCond(ejs->dispatcher->cond, timeout);
        }
        remaining = mprGetRemainingTime(ejs, mark, timeout);
    } while (ejs->joining && remaining > 0 && !ejs->exception);

    if (ejs->exception) {
        return 0;
    }
    result = (ejs->joining) ? MPR_ERR_TIMEOUT: 0;
    ejs->joining = 0;
    return result;
}


/*
 *  static function join(workers: Object = null, timeout: Number = -1): Boolean
 */
static EjsVar *workerJoin(Ejs *ejs, EjsWorker *unused, int argc, EjsVar **argv)
{
    EjsVar      *workers;
    int         timeout;

    workers = (argc > 0) ? argv[0] : NULL;
    timeout = (argc == 2) ? ejsGetInt(argv[1]) : MAXINT;

    return (join(ejs, workers, timeout) == 0) ? (EjsVar*) ejs->trueValue: (EjsVar*) ejs->falseValue;
}


/*
 *  Load a file into the worker. This can be a script file or a module. This runs on the inside interpreter
 */
static void loadFile(EjsWorker *worker, cchar *path)
{
    Ejs         *ejs;
    EjsVar      *result;
    cchar       *cp;

    mprAssert(worker->inside);
    mprAssert(worker->pair && worker->pair->ejs);

    ejs = worker->ejs;
    result = 0;

    if ((cp = strrchr(path, '.')) != NULL && strcmp(cp, EJS_MODULE_EXT) != 0) {
        if (ejs->service->loadScriptFile == 0) {
            ejsThrowIOError(ejs, "load: Compiling is not enabled for %s", path);
            return;
        }
        (ejs->service->loadScriptFile)(ejs, path);

    } else {
        /* This will throw on errors */
        ejsLoadModule(ejs, path, -1, -1, 0, NULL);
    }
}


/*
 *  function load(script: Path, timeout: Number = 0): Void
 */
static EjsVar *workerLoad(Ejs *ejs, EjsWorker *worker, int argc, EjsVar **argv)
{
    int     timeout;

    mprAssert(argc == 0 || ejsIsPath(argv[0]));

    worker->scriptFile = mprStrdup(worker, ((EjsPath*) argv[0])->path);
    timeout = argc == 2 ? ejsGetInt(argv[1]): 0;
    return startWorker(ejs, worker, timeout);
}


/*
 *  static function lookup(name: String): Worker
 */
static EjsVar *workerLookup(Ejs *ejs, EjsVar *unused, int argc, EjsVar **argv)
{
    EjsWorker   *worker;
    cchar       *name;
    int         next;

    name = ejsGetString(argv[0]);
    lock(ejs);
    for (next = 0; (worker = mprGetNextItem(ejs->workers, &next)) != NULL; ) {
        if (worker->name && strcmp(name, worker->name) == 0) {
            unlock(ejs);
            return (EjsVar*) worker;
        }
    }
    unlock(ejs);
    return ejs->nullValue;
}


/*
 *  Process a message sent from postMessage. This may run inside the worker or outside in the parent depending on the
 *  direction of the message. But it ALWAYS runs in the appropriate thread for the interpreter.
 */
static void doMessage(Message *msg, MprEvent *mprEvent)
{
    Ejs         *ejs;
    EjsVar      *event;
    EjsWorker   *worker;
    EjsFunction *callback;
    EjsVar      *argv[1];

    worker = msg->worker;
    ejs = worker->ejs;

    callback = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) worker, msg->callbackSlot);

    switch (msg->callbackSlot) {
    case ES_ejs_sys_Worker_onclose:
        event = ejsCreateVar(ejs, ejs->eventType, 0);
        break;
    case ES_ejs_sys_Worker_onerror:
        event = ejsCreateVar(ejs, ejs->errorEventType, 0);
        break;
    case ES_ejs_sys_Worker_onmessage:
        event = ejsCreateVar(ejs, ejs->eventType, 0);
        break;
    default:
        mprAssert(msg->callbackSlot == 0);
        mprFree(mprEvent);
        return;
    }
    if (msg->data) {
        ejsSetProperty(ejs, event, ES_ejs_events_Event_data, (EjsVar*) ejsCreateStringAndFree(ejs, msg->data));
    }
    if (msg->message) {
        ejsSetProperty(ejs, event, ES_ejs_events_ErrorEvent_message, (EjsVar*) ejsCreateStringAndFree(ejs, msg->message));
    }
    if (msg->filename) {
        ejsSetProperty(ejs, event, ES_ejs_events_ErrorEvent_filename, (EjsVar*) ejsCreateStringAndFree(ejs, msg->filename));
        ejsSetProperty(ejs, event, ES_ejs_events_ErrorEvent_lineno, (EjsVar*) ejsCreateNumber(ejs, msg->lineNumber));
    }
    if (msg->stack) {
        ejsSetProperty(ejs, event, ES_ejs_events_ErrorEvent_stack, (EjsVar*) ejsCreateStringAndFree(ejs, msg->stack));
    }

    if (callback == 0 || (EjsVar*) callback == ejs->nullValue) {
        if (msg->callbackSlot == ES_ejs_sys_Worker_onmessage) {
            mprLog(ejs, 1, "Discard message as no onmessage handler defined for worker");
            
        } else if (msg->callbackSlot == ES_ejs_sys_Worker_onerror) {
            ejsThrowError(ejs, "Exception in Worker: %s", ejsGetErrorMsg(worker->pair->ejs, 1));

        } else {
            /* Ignore onclose message */
        }

    } else if (!ejsIsFunction(callback)) {
        ejsThrowTypeError(ejs, "Worker callback %s is not a function", msg->callback);

    } else {
        argv[0] = event;
        ejsRunFunction(ejs, callback, (EjsVar*) worker, 1, argv);
    }

    if (msg->callbackSlot == ES_ejs_sys_Worker_onclose) {
        mprAssert(!worker->inside);
        worker->state = EJS_WORKER_COMPLETE;
        removeWorker(ejs, worker);
        /*
         *  Now that the inside worker is complete, the outside worker does not need to be protected from GC
         */
        worker->obj.var.permanent = 0;
    }
    mprFree(msg);
    mprFree(mprEvent);
}


/*
 *  function preload(path: Path): String
 *  NOTE: this blocks. 
 */
static EjsVar *workerPreload(Ejs *ejs, EjsWorker *worker, int argc, EjsVar **argv)
{
    Ejs         *inside;
    EjsWorker   *insideWorker;
    EjsVar      *result;

    mprAssert(argc > 0 && ejsIsPath(argv[0]));
    mprAssert(!worker->inside);

    if (worker->state > EJS_WORKER_BEGIN) {
        ejsThrowStateError(ejs, "Worker has already started");
        return 0;
    }
    insideWorker = worker->pair;
    inside = insideWorker->ejs;

    loadFile(worker->pair, ((EjsPath*) argv[0])->path);
    if (inside->exception) {
        handleError(ejs, worker, inside->exception);
        return 0;
    }
    result = ejsSerialize(ejs, inside->result, -1, 0, 0);
    if (result == 0) {
        return ejs->nullValue;
    }
    return ejsDeserialize(ejs, (EjsString*) result);
}


/*
 *  Post a message to this worker. Note: the worker is the destination worker which may be the parent.
 *
 *  function postMessage(data: Object, ports: Array = null): Void
 */
static EjsVar *workerPostMessage(Ejs *ejs, EjsWorker *worker, int argc, EjsVar **argv)
{
    EjsVar          *data;
    EjsWorker       *target;
    MprDispatcher   *dispatcher;
    Message         *msg;

    if (worker->state >= EJS_WORKER_CLOSED) {
        ejsThrowStateError(ejs, "Worker has completed");
        return 0;
    }

    /*
     *  Create the event with serialized data in the originating interpreter. It owns the data.
     */
    if ((data = ejsSerialize(ejs, argv[0], -1, 0, 0)) == 0) {
        ejsThrowArgError(ejs, "Can't serialize message data");
        return 0;
    }
    if ((msg = mprAllocObjZeroed(ejs, Message)) == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    target = worker->pair;
    msg->data = mprStrdup(target->ejs, ejsGetString(data));
    msg->worker = target;
    msg->callback = "onmessage";
    msg->callbackSlot = ES_ejs_sys_Worker_onmessage;

    dispatcher = target->ejs->dispatcher;
    mprCreateEvent(dispatcher, (MprEventProc) doMessage, 0, MPR_NORMAL_PRIORITY, msg, 0);
    mprSignalCond(dispatcher->cond);
    return 0;
}


/*
 *  Worker thread main procedure
 */
static void workerMain(EjsWorker *worker, MprWorker *mprWorker)
{
    Ejs             *ejs, *inside;
    EjsWorker       *insideWorker;
    MprDispatcher   *dispatcher;
    Message         *msg;

    mprAssert(!worker->inside);
    insideWorker = worker->pair;
    mprAssert(insideWorker->state == EJS_WORKER_BEGIN);

    ejs = worker->ejs;
    inside = insideWorker->ejs;
    insideWorker->state = EJS_WORKER_STARTED;
    
    /*
     *  Run the script or file
     */
    if (worker->scriptFile) {
        loadFile(insideWorker, worker->scriptFile);

    } else if (worker->scriptLiteral) {
        if (ejs->service->loadScriptLiteral == 0) {
            ejsThrowIOError(ejs, "worker: Compiling is not enabled");
            return;
        }
        (ejs->service->loadScriptLiteral)(inside, worker->scriptLiteral);
    }

    /*
     *  Check for exceptions
     */
    if (inside->exception) {
        handleError(ejs, worker, inside->exception);
    }
    if ((msg = mprAllocObjZeroed(ejs, Message)) == 0) {
        ejsThrowMemoryError(ejs);
        return;
    }

    /*
     *  Post "onclose" finalization message
     */
    msg->worker = worker;
    msg->callback = "onclose";
    msg->callbackSlot = ES_ejs_sys_Worker_onclose;

    insideWorker->state = EJS_WORKER_CLOSED;
    worker->state = EJS_WORKER_CLOSED;
    insideWorker->obj.var.permanent = 0;
    dispatcher = worker->ejs->dispatcher;
    mprCreateEvent(dispatcher, (MprEventProc) doMessage, 0, MPR_NORMAL_PRIORITY, msg, 0);
    mprSignalCond(dispatcher->cond);
}


/*
 *  function terminate()
 */
static EjsVar *workerTerminate(Ejs *ejs, EjsWorker *worker, int argc, EjsVar **argv)
{    
    if (worker->state == EJS_WORKER_BEGIN) {
        ejsThrowStateError(ejs, "Worker has not yet started");
        return 0;
    }
    if (worker->state >= EJS_WORKER_COMPLETE) {
        return 0;
    }
  
    /*
     *  Switch to the inside worker if called from outside
     */
    mprAssert(worker->pair && worker->pair->ejs);
    ejs = (!worker->inside) ? worker->pair->ejs : ejs;
    worker->terminated = 1;
    ejs->exiting = 1;
    mprWakeDispatcher(ejs->dispatcher);
    return 0;
}


/*
 *  function waitForMessage(timeout: Number = -1): Boolean
 */
static EjsVar *workerWaitForMessage(Ejs *ejs, EjsWorker *worker, int argc, EjsVar **argv)
{
    MprTime     mark, remaining;
    int         timeout;

    timeout = (argc > 0) ? ejsGetInt(argv[0]): MAXINT;
    if (timeout < 0) {
        timeout = MAXINT;
    }
    mark = mprGetTime(ejs);
    do {
        if (mprServiceEvents(ejs->dispatcher, timeout, MPR_SERVICE_EVENTS | MPR_SERVICE_ONE_THING) > 0) {
            return (EjsVar*) ejs->trueValue;
        }
        remaining = mprGetRemainingTime(ejs, mark, timeout);
    } while (remaining > 0 && !mprIsExiting(ejs) && !ejs->exiting);
    return (EjsVar*) ejs->falseValue;
}


/*
 *  WARNING: the inside interpreter owns the exception object. Must fully extract all fields
 */
static void handleError(Ejs *ejs, EjsWorker *worker, EjsVar *exception)
{
    EjsError        *error;
    MprDispatcher   *dispatcher;
    Message         *msg;

    mprAssert(!worker->inside);
    mprAssert(exception);
    mprAssert(ejs == worker->ejs);

    if ((msg = mprAllocObjZeroed(ejs, Message)) == 0) {
        ejsThrowMemoryError(ejs);
        return;
    }
    msg->worker = worker;
    msg->callback = "onerror";
    msg->callbackSlot = ES_ejs_sys_Worker_onerror;
    
    /*
     *  Inside interpreter owns the exception object, so must fully extract all exception. 
     *  Allocate into the outside worker's interpreter.
     */
    if (ejsIsError(exception)) {
        error = (EjsError*) exception;
        msg->message = mprStrdup(ejs, error->message);
        msg->filename = mprStrdup(ejs, error->filename ? error->filename : "script");
        msg->lineNumber = error->lineNumber;
        msg->stack = mprStrdup(ejs, error->stack);

    } else if (ejsIsString(exception)) {
        msg->message = mprStrdup(ejs, ejsGetString(exception));

    } else {
        msg->message = mprStrdup(ejs, ejsGetString(ejsToString(ejs, exception)));
    }
    dispatcher = ejs->dispatcher;
    mprCreateEvent(dispatcher, (MprEventProc) doMessage, 0, MPR_NORMAL_PRIORITY, msg, 0);
    mprSignalCond(dispatcher->cond);
}


EjsWorker *ejsCreateWorker(Ejs *ejs)
{
    return (EjsWorker*) ejsCreateVar(ejs, ejs->workerType, 0);
}


static void destroyWorker(Ejs *ejs, EjsWorker *worker)
{
    if (!worker->inside) {
        removeWorker(ejs, worker);
        mprAssert(worker->pair);
        mprFree(worker->pair->ejs);
        worker->pair = 0;
    }
    ejsFreeVar(ejs, (EjsVar*) worker, -1);
}


static void markWorker(Ejs *ejs, EjsVar *parent, EjsWorker *worker)
{
    ejsMarkObject(ejs, parent, (EjsObject*) worker);
}


void ejsConfigureWorkerType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = (EjsType*) ejsGetPropertyByName(ejs, ejs->global, ejsName(&qname, "ejs.sys", "Worker"));
    if (type) {
        type->instanceSize = sizeof(EjsWorker);
        type->dontPool = 1;
        type->needFinalize = 1;
        type->helpers->destroyVar = (EjsDestroyVarHelper) destroyWorker;
        type->helpers->markVar = (EjsMarkVarHelper) markWorker;
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_Worker, (EjsNativeFunction) workerConstructor);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_eval, (EjsNativeFunction) workerEval);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_exit, (EjsNativeFunction) workerExit);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_join, (EjsNativeFunction) workerJoin);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_load, (EjsNativeFunction) workerLoad);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_lookup, (EjsNativeFunction) workerLookup);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_preload, (EjsNativeFunction) workerPreload);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_postMessage, (EjsNativeFunction) workerPostMessage);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_terminate, (EjsNativeFunction) workerTerminate);
        ejsBindMethod(ejs, type, ES_ejs_sys_Worker_waitForMessage, (EjsNativeFunction) workerWaitForMessage);
        ejs->workerType = type;
    }
}


#else /* BLD_FEATURE_MULTITHREAD */

void __dummyEjsWorker() {}
#endif /* BLD_FEATURE_MULTITHREAD */

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */

/************************************************************************/
/*
 *  End of file "../src/types/sys/ejsWorker.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/xml/ejsXML.c"
 */
/************************************************************************/

/**
 *  ejsXML.c - E4X XML type.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_EJS_E4X

/*
 *  XML methods
 */
static EjsVar   *loadXml(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv);
static EjsVar   *saveXml(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv);
static EjsVar   *xmlToString(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv);

static EjsVar   *xml_parent(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv);

static bool allDigitsForXml(cchar *name);
static bool deepCompare(EjsXML *lhs, EjsXML *rhs);
static int readStringData(MprXml *xp, void *data, char *buf, int size);
static int readFileData(MprXml *xp, void *data, char *buf, int size);


static EjsXML *createXml(Ejs *ejs, EjsType *type, int size)
{
    return (EjsXML*) ejsCreateXML(ejs, 0, NULL, NULL, NULL);
}


static void destroyXml(Ejs *ejs, EjsXML *xml)
{
    ejsFreeVar(ejs, (EjsVar*) xml, -1);
}


static EjsVar *cloneXml(Ejs *ejs, EjsXML *xml, bool deep)
{
    EjsXML  *newXML;

    newXML = (EjsXML*) ejsCreateVar(ejs, xml->obj.var.type, 0);
    if (newXML == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    return (EjsVar*) newXML;
}


/*
 *  Cast the object operand to a primitive type
 */
static EjsVar *castXml(Ejs *ejs, EjsXML *xml, EjsType *type)
{
    EjsXML      *item;
    EjsVar      *result;
    MprBuf      *buf;

    mprAssert(ejsIsXML(xml));

    switch (type->id) {
    case ES_Object:
    case ES_XMLList:
        return (EjsVar*) xml;

    case ES_Boolean:
        return (EjsVar*) ejsCreateBoolean(ejs, 1);

    case ES_Number:
        result = castXml(ejs, xml, ejs->stringType);
        result = (EjsVar*) ejsToNumber(ejs, result);
        return result;

    case ES_String:
        if (xml->kind == EJS_XML_ELEMENT) {
            if (xml->elements == 0) {
                return (EjsVar*) ejs->emptyStringValue;
            }
            if (xml->elements && mprGetListCount(xml->elements) == 1) {
                item = mprGetFirstItem(xml->elements);
                if (item->kind == EJS_XML_TEXT) {
                    return (EjsVar*) ejsCreateString(ejs, item->value);
                }
            }
        }
        buf = mprCreateBuf(ejs, MPR_BUFSIZE, -1);
        if (ejsXMLToString(ejs, buf, xml, -1) < 0) {
            mprFree(buf);
            return 0;
        }
        result = (EjsVar*) ejsCreateString(ejs, (char*) buf->start);
        mprFree(buf);
        return result;

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
    return 0;
}


static int deleteXmlPropertyByName(Ejs *ejs, EjsXML *xml, EjsName *qname)
{
    EjsXML      *item;
    bool        removed;
    int         next;

    removed = 0;

    if (qname->name[0] == '@') {
        /* @ and @* */
        if (xml->attributes) {
            for (next = 0; (item = mprGetNextItem(xml->attributes, &next)) != 0; ) {
                mprAssert(qname->name[0] == '@');
                if (qname->name[1] == '*' || strcmp(item->qname.name, &qname->name[1]) == 0) {
                    mprRemoveItemAtPos(xml->attributes, next - 1);
                    item->parent = 0;
                    removed = 1;
                    next -= 1;
                }
            }
        }

    } else {
        /* name and * */
        if (xml->elements) {
            for (next = 0; (item = mprGetNextItem(xml->elements, &next)) != 0; ) {
                mprAssert(item->qname.name);
                if (qname->name[0] == '*' || strcmp(item->qname.name, qname->name) == 0) {
                    mprRemoveItemAtPos(xml->elements, next - 1);
                    item->parent = 0;
                    removed = 1;
                    next -= 1;
                }
            }
        }
    }
    return (removed) ? 0 : EJS_ERR;
}


static EjsVar *getXmlNodeName(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateString(ejs, xml->qname.name);
}



/*
 *  Function to iterate and return the next element name.
 *  NOTE: this is not a method of Xml. Rather, it is a callback function for Iterator
 */
static EjsVar *nextXmlKey(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsXML  *xml;

    xml = (EjsXML*) ip->target;
    if (!ejsIsXML(xml)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    for (; ip->index < mprGetListCount(xml->elements); ip->index++) {
        return (EjsVar*) ejsCreateNumber(ejs, ip->index++);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return the default iterator. This returns the array index names.
 *
 *  iterator native function get(): Iterator
 */
static EjsVar *getXmlIterator(Ejs *ejs, EjsVar *xml, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, xml, (EjsNativeFunction) nextXmlKey, 0, NULL);
}


/*
 *  Function to iterate and return the next element value.
 *  NOTE: this is not a method of Xml. Rather, it is a callback function for Iterator
 */
static EjsVar *nextXmlValue(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsXML      *xml, *vp;

    xml = (EjsXML*) ip->target;
    if (!ejsIsXML(xml)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    for (; ip->index < mprGetListCount(xml->elements); ip->index++) {
        vp = (EjsXML*) mprGetItem(xml->elements, ip->index);
        if (vp == 0) {
            continue;
        }
        ip->index++;
        return (EjsVar*) vp;
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return an iterator to return the next array element value.
 *
 *  iterator native function getValues(): Iterator
 */
static EjsVar *getXmlValues(Ejs *ejs, EjsVar *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, ap, (EjsNativeFunction) nextXmlValue, 0, NULL);
}


static int getXmlPropertyCount(Ejs *ejs, EjsXML *xml)
{
    return mprGetListCount(xml->elements);
}


/*
 *  Lookup a property by name. There are 7 kinds of lookups:
 *       prop, @att, [prop], *, @*, .name, .@name
 */
static EjsVar *getXmlPropertyByName(Ejs *ejs, EjsXML *xml, EjsName *qname)
{
    EjsXML      *item, *result, *list;
    int         next, nextList;

    result = 0;

    mprAssert(xml->kind < 5);
    if (isdigit((int) qname->name[0]) && allDigitsForXml(qname->name)) {
        /*
         *  Consider xml as a list with only one entry == xml. Then return the 0'th entry
         */
        return (EjsVar*) xml;
    }

    if (qname->name[0] == '@') {
        /* @ and @* */
        result = ejsCreateXMLList(ejs, xml, qname);
        if (xml->attributes) {
            for (next = 0; (item = mprGetNextItem(xml->attributes, &next)) != 0; ) {
                mprAssert(qname->name[0] == '@');
                if (qname->name[1] == '*' || strcmp(item->qname.name, &qname->name[1]) == 0) {
                    result = ejsAppendToXML(ejs, result, item);
                }
            }
        }

    } else if (qname->name[0] == '.') {
        /* Decenders (do ..@ also) */
        result = ejsXMLDescendants(ejs, xml, qname);

    } else {
        /* name and * */
        result = ejsCreateXMLList(ejs, xml, qname);
        if (xml->elements) {
            for (next = 0; (item = mprGetNextItem(xml->elements, &next)) != 0; ) {
                if (item->kind == EJS_XML_LIST) {
                    list = item;
                    for (nextList = 0; (item = mprGetNextItem(list->elements, &nextList)) != 0; ) {
                        mprAssert(item->qname.name);
                        if (qname->name[0] == '*' || strcmp(item->qname.name, qname->name) == 0) {
                            result = ejsAppendToXML(ejs, result, item);
                        }
                    }

                } else if (item->qname.name) {
                    mprAssert(item->qname.name);
                    if (qname->name[0] == '*' || strcmp(item->qname.name, qname->name) == 0) {
                        result = ejsAppendToXML(ejs, result, item);
                    }
                }
            }
        }
    }
    return (EjsVar*) result;
}


static EjsVar *invokeXmlOperator(Ejs *ejs, EjsXML *lhs, int opcode,  EjsXML *rhs)
{
    EjsVar      *result;

    if ((result = ejsCoerceOperands(ejs, (EjsVar*) lhs, opcode, (EjsVar*) rhs)) != 0) {
        return result;
    }

    switch (opcode) {
    case EJS_OP_COMPARE_EQ:
        return (EjsVar*) ejsCreateBoolean(ejs, deepCompare(lhs, rhs));

    case EJS_OP_COMPARE_NE:
        return (EjsVar*) ejsCreateBoolean(ejs, !deepCompare(lhs, rhs));

    default:
        return ejsObjectOperator(ejs, (EjsVar*) lhs, opcode, (EjsVar*) rhs);
    }
}


/*
 *  Set a property attribute by name.
 */
static int setXmlPropertyAttributeByName(Ejs *ejs, EjsXML *xml, EjsName *qname, EjsVar *value)
{
    EjsXML      *elt, *attribute, *rp, *xvalue, *lastElt;
    EjsString   *sv;
    EjsName     qn;
    char        *str;
    int         index, last, next;

    /*
     *  Attribute. If the value is an XML list, convert to a space separated string
     */
    xvalue = (EjsXML*) value;
    if (ejsIsXML(xvalue) && xvalue->kind == EJS_XML_LIST) {
        str = 0;
        for (next = 0; (elt = mprGetNextItem(xvalue->elements, &next)) != 0; ) {
            sv = (EjsString*) ejsCastVar(ejs, (EjsVar*) elt, ejs->stringType);
            str = mprReallocStrcat(ejs, -1, str, " ", sv->value, NULL);
        }
        value = (EjsVar*) ejsCreateString(ejs, str);
        mprFree(str);

    } else {
        value = ejsCastVar(ejs, value, ejs->stringType);
    }
    mprAssert(ejsIsString(value));

    /*
     *  Find the first attribute that matches. Delete all other attributes of the same name.
     */
    index = 0;
    if (xml->attributes) {
        lastElt = 0;
        for (last = -1, index = -1; (elt = mprGetPrevItem(xml->attributes, &index)) != 0; ) {
            mprAssert(qname->name[0] == '@');
            if (strcmp(elt->qname.name, &qname->name[1]) == 0) {
                if (last >= 0) {
                    rp = mprGetItem(xml->attributes, last);
                    mprRemoveItemAtPos(xml->attributes, last);
                }
                last = index;
                lastElt = elt;
            }
        }
        if (lastElt) {
            /*
             *  Found a match. So replace its value
             */
            mprFree(lastElt->value);
            lastElt->value = mprStrdup(lastElt, ((EjsString*) value)->value);
            return last;

        } else {
            index = mprGetListCount(xml->attributes);
        }
    }

    /*
     *  Not found. Create a new attribute node
     */
    mprAssert(ejsIsString(value));
    ejsName(&qn, 0, &qname->name[1]);
    attribute = ejsCreateXML(ejs, EJS_XML_ATTRIBUTE, &qn, xml, ((EjsString*) value)->value);
    if (xml->attributes == 0) {
        xml->attributes = mprCreateList(xml);
    }
    mprSetItem(xml->attributes, index, attribute);

    return index;
}


/*
 *  Create a value node. If the value is an XML node already, we are done. Otherwise, cast the value to a string
 *  and create a text child node containing the string value.
 */
static EjsXML *createValueNode(Ejs *ejs, EjsXML *elt, EjsVar *value)
{
    EjsXML      *text;
    EjsString   *str;

    if (ejsIsXML(value)) {
        return (EjsXML*) value;
    }

    str = (EjsString*) ejsCastVar(ejs, value, ejs->stringType);
    if (str == 0) {
        return 0;
    }

    if (mprGetListCount(elt->elements) == 1) {
        /*
         *  Update an existing text element
         */
        text = mprGetFirstItem(elt->elements);
        if (text->kind == EJS_XML_TEXT) {
            mprFree(text->value);
            text->value = mprStrdup(elt, str->value);
            return elt;
        }
    }

    /*
     *  Create a new text element
     */
    if (str->value && str->value[0] != '\0') {
        text = ejsCreateXML(ejs, EJS_XML_TEXT, NULL, elt, str->value);
        elt = ejsAppendToXML(ejs, elt, text);
    }
    return elt;
}


void ejsMarkXML(Ejs *ejs, EjsVar *parent, EjsXML *xml)
{
    EjsVar          *item;
    int             next;

    ejsMarkObject(ejs, (EjsVar*) parent, (EjsObject*) xml);

    if (xml->parent && !xml->parent->obj.var.visited) {
        ejsMarkVar(ejs, (EjsVar*) xml, (EjsVar*) xml->parent);
    }
    if (xml->targetObject && !xml->targetObject->obj.var.visited) {
        ejsMarkVar(ejs, (EjsVar*) xml, (EjsVar*) xml->targetObject);
    }

    for (next = 0; (item = mprGetNextItem(xml->attributes, &next)) != 0; ) {
        ejsMarkVar(ejs, (EjsVar*) xml, (EjsVar*) item);
    }
    for (next = 0; (item = mprGetNextItem(xml->elements, &next)) != 0; ) {
        ejsMarkVar(ejs, (EjsVar*) xml, (EjsVar*) item);
    }
}


/*
 *  Set a property by name. Implements the ECMA-357 [[put]] method.
 *  There are 7 kinds of qname's: prop, @att, [prop], *, @*, .name, .@name
 */
static int setXmlPropertyByName(Ejs *ejs, EjsXML *xml, EjsName *qname, EjsVar *value)
{
    EjsXML      *elt, *xvalue, *rp, *lastElt;
    EjsVar      *originalValue;
    int         index, last;

    mprLog(ejs, 9, "XMLSet %s.%s = \"%s\"", xml->qname.name, qname->name,
        ((EjsString*) ejsCastVar(ejs, value, ejs->stringType))->value);

    if (isdigit((int) qname->name[0]) && allDigitsForXml(qname->name)) {
        ejsThrowTypeError(ejs, "Integer indicies for set are not allowed");
        return EJS_ERR;
    }

    if (xml->kind != EJS_XML_ELEMENT) {
        return 0;
    }

    /*
     *  Massage the value type.
     */
    originalValue = value;

    xvalue = (EjsXML*) value;
    if (ejsIsXML(xvalue)) {
        if (xvalue->kind == EJS_XML_LIST) {
            value = (EjsVar*) ejsDeepCopyXML(ejs, xvalue);

        } else if (xvalue->kind == EJS_XML_TEXT || xvalue->kind == EJS_XML_ATTRIBUTE) {
            value = ejsCastVar(ejs, originalValue, ejs->stringType);

        } else {
            value = (EjsVar*) ejsDeepCopyXML(ejs, xvalue);
        }

    } else {
        value = ejsCastVar(ejs, value, ejs->stringType);
    }

    if (qname->name[0] == '@') {
        return setXmlPropertyAttributeByName(ejs, xml, qname, value);
    }

    /*
     *  Delete redundant elements by the same name.
     */
    lastElt = 0;
    last = -1;
    if (xml->elements) {
        for (index = -1; (elt = mprGetPrevItem(xml->elements, &index)) != 0; ) {
            if (qname->name[0] == '*' || (elt->kind == EJS_XML_ELEMENT && strcmp(elt->qname.name, qname->name) == 0)) {
                /*
                 *  Must remove all redundant elements of the same name except the first one
                 */
                if (last >= 0) {
                    rp = mprGetItem(xml->elements, last);
                    rp->parent = 0;
                    mprRemoveItemAtPos(xml->elements, last);
                }
                last = index;
                lastElt = elt;
            }
        }
    }

    if (xml->elements == 0) {
        xml->elements = mprCreateList(xml);
    }

    elt = lastElt;
    index = last;

    if (qname->name[0] == '*') {
        /*
         *  Special case when called from XMLList to update the value of an element
         */
        xml = createValueNode(ejs, xml, value);

    } else if (elt == 0) {
        /*
         *  Not found. New node required.
         */
        elt = ejsCreateXML(ejs, EJS_XML_ELEMENT, qname, xml, NULL);
        if (elt == 0) {
            return 0;
        }
        index = mprGetListCount(xml->elements);
        xml = ejsAppendToXML(ejs, xml, createValueNode(ejs, elt, value));

    } else {
        /*
         *  Update existing element.
         */
        xml = ejsSetXML(ejs, xml, index, createValueNode(ejs, elt, value));
    }

    if (xml == 0) {
        return EJS_ERR;
    }
    return index;
}


#if UNUSED
/*
    function attributes(name: String = "*"): XMLList
 */
static EjsVar *xml_attributes(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    EjsXML  *result;
    EjsXML  *item;
    cchar   *name;
    int     next;

    name = argc > 0 ? ejsGetString(argv[0]) : "*";

    result = ejsCreateXMLList(ejs, xml, &xml->targetProperty);
    if (xml->attributes) {
        for (next = 0; (item = mprGetNextItem(xml->attributes, &next)) != 0; ) {
            if (name[0] == '*' || strcmp(item->qname.name, name) == 0) {
                result = ejsAppendToXML(ejs, result, item);
            }
        }
    }
    return (EjsVar*) result;
}


/*
    function elements(name: String = "*"): XMLList
 */
static EjsVar *xml_elements(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    EjsXML  *result;
    EjsXML  *item;
    cchar   *name;
    int     next;

    name = argc > 0 ? ejsGetString(argv[0]) : "*";

    result = ejsCreateXMLList(ejs, xml, &xml->targetProperty);
    if (xml->elements) {
        for (next = 0; (item = mprGetNextItem(xml->elements, &next)) != 0; ) {
            if (name[0] == '*' || strcmp(item->qname.name, name) == 0) {
                result = ejsAppendToXML(ejs, result, item);
            }
        }
    }
    return (EjsVar*) result;
}
#endif


/*
    function parent(): XML
 */
static EjsVar *xml_parent(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    return (xml->parent && xml != xml->parent) ? (EjsVar*) xml->parent : (EjsVar*) ejs->nullValue;
}

/*
 *  Deep compare
 */
static bool deepCompare(EjsXML *lhs, EjsXML *rhs)
{
    EjsXML      *l, *r;
    int         i;

    if (lhs == rhs) {
        return 1;
    }
    if (lhs->kind != rhs->kind) {
        return 0;

    } else  if (mprStrcmp(lhs->qname.name, rhs->qname.name) != 0) {
        return 0;

    } else if (mprGetListCount(lhs->attributes) != mprGetListCount(rhs->attributes)) {
        return 0;

    } else if (mprGetListCount(lhs->elements) != mprGetListCount(rhs->elements)) {
        return 0;

    } else if (mprStrcmp(lhs->value, rhs->value) != 0) {
        return 0;

    } else {
        for (i = 0; i < mprGetListCount(lhs->elements); i++) {
            l = mprGetItem(lhs->elements, i);
            r = mprGetItem(rhs->elements, i);
            if (! deepCompare(l, r)) {
                return 0;
            }
        }
    }
    return 1;
}


EjsXML *ejsXMLDescendants(Ejs *ejs, EjsXML *xml, EjsName *qname)
{
    EjsXML          *item, *result;
    int             next;

    result = ejsCreateXMLList(ejs, xml, qname);
    if (result == 0) {
        return 0;
    }
    if (qname->name[0] == '.' && qname->name[1] == '@') {
        if (xml->attributes) {
            for (next = 0; (item = mprGetNextItem(xml->attributes, &next)) != 0; ) {
                if (qname->name[2] == '*' || strcmp(item->qname.name, &qname->name[2]) == 0) {
                    result = ejsAppendToXML(ejs, result, item);
#if UNUSED
                } else {
                    result = ejsAppendToXML(ejs, result, ejsXMLDescendants(ejs, item, qname));
#endif
                }
            }
        }
        if (xml->elements) {
            for (next = 0; (item = mprGetNextItem(xml->elements, &next)) != 0; ) {
                result = ejsAppendToXML(ejs, result, ejsXMLDescendants(ejs, item, qname));
            }
        }
        
    } else {
        if (xml->elements) {
            for (next = 0; (item = mprGetNextItem(xml->elements, &next)) != 0; ) {
                if (qname->name[0] == '*' || strcmp(item->qname.name, &qname->name[1]) == 0) {
                    result = ejsAppendToXML(ejs, result, item);
                } else {
                    result = ejsAppendToXML(ejs, result, ejsXMLDescendants(ejs, item, qname));
                }
            }
        }
    }
    return result;
}


EjsXML *ejsDeepCopyXML(Ejs *ejs, EjsXML *xml)
{
    EjsXML      *root, *elt;
    int         next;

    if (xml == 0) {
        return 0;
    }

    if (xml->kind == EJS_XML_LIST) {
        root = ejsCreateXMLList(ejs, xml->targetObject, &xml->targetProperty);
    } else {
        root = ejsCreateXML(ejs, xml->kind, &xml->qname, NULL, xml->value);
    }
    if (root == 0) {
        return 0;
    }

    if (xml->attributes) {
        root->attributes = mprCreateList(root);
        for (next = 0; (elt = (EjsXML*) mprGetNextItem(xml->attributes, &next)) != 0; ) {
            elt = ejsDeepCopyXML(ejs, elt);
            if (elt) {
                elt->parent = root;
                mprAddItem(root->attributes, elt);
            }
        }
    }

    if (xml->elements) {
        root->elements = mprCreateList(root);
        for (next = 0; (elt = mprGetNextItem(xml->elements, &next)) != 0; ) {
            mprAssert(ejsIsXML(elt));
            elt = ejsDeepCopyXML(ejs, elt);
            if (elt) {
                elt->parent = root;
                mprAddItem(root->elements, elt);
            }
        }
    }

    if (mprHasAllocError(ejs)) {
        mprFree(root);
        return 0;
    }

    return root;
}

/*
 *  native function XML(value: Object = null)
 */
static EjsVar *xmlConstructor(Ejs *ejs, EjsXML *thisObj, int argc, EjsVar **argv)
{
    EjsVar      *arg, *vp;
    cchar       *str;

    if (thisObj == 0) {
        /*
         *  Called as a function - cast the arg
         */
        if (argc > 0){
            arg = ejsCastVar(ejs, argv[0], ejs->stringType);
            if (arg == 0) {
                return 0;
            }
        }
        thisObj = ejsCreateXML(ejs, 0, NULL, NULL, NULL);
    }
    if (argc == 0) {
        return (EjsVar*) thisObj;
    }

    arg = argv[0];
    mprAssert(arg);

    if (ejsIsNull(arg) || ejsIsUndefined(arg)) {
        return (EjsVar*) thisObj;
    }
    if (ejsIsObject(arg)) {
        arg = ejsCastVar(ejs, argv[0], ejs->stringType);
    }
    if (arg && ejsIsString(arg)) {
        str = ((EjsString*) arg)->value;
        if (str == 0) {
            return 0;
        }
        while (isspace((int) *str)) str++;
        if (*str == '<') {
            /* XML Literal */
            ejsLoadXMLString(ejs, thisObj, str);

        } else {
            /* Load from file */
            loadXml(ejs, thisObj, argc, argv);
        }
    } else if (arg && ejsIsXML(arg)) {
        if ((vp = xmlToString(ejs, argv[0], 0, NULL)) != 0) {
            ejsLoadXMLString(ejs, thisObj, ejsGetString(vp));
        }

    } else {
        ejsThrowArgError(ejs, "Bad type passed to XML constructor");
        return 0;
    }
    return (EjsVar*) thisObj;
}


static EjsVar *loadXml(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    MprFile     *file;
    MprXml      *xp;
    char        *filename;

    mprAssert(argc == 1 && ejsIsString(argv[0]));

    filename = ejsGetString(argv[0]);

    file = mprOpen(ejs, filename, O_RDONLY, 0664);
    if (file == 0) {
        ejsThrowIOError(ejs, "Can't open: %s", filename);
        return 0;
    }

    xp = ejsCreateXmlParser(ejs, xml, filename);
    if (xp == 0) {
        ejsThrowMemoryError(ejs);
        mprFree(xp);
        mprFree(file);
        return 0;
    }
    mprXmlSetInputStream(xp, readFileData, (void*) file);

    if (mprXmlParse(xp) < 0) {
        if (! ejsHasException(ejs)) {
            ejsThrowIOError(ejs, "Can't parse XML file: %s\nDetails %s",  filename, mprXmlGetErrorMsg(xp));
        }
        mprFree(xp);
        mprFree(file);
        return 0;
    }
    mprFree(xp);
    mprFree(file);
    return 0;
}


static EjsVar *saveXml(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    MprBuf      *buf;
    MprFile     *file;
    cchar       *filename;
    int         bytes, len;

    if (argc != 1 || !ejsIsString(argv[0])) {
        ejsThrowArgError(ejs, "Bad args. Usage: save(filename);");
        return 0;
    }
    filename = ((EjsString*) argv[0])->value;

    /*
     *  Create a buffer to hold the output. All in memory.
     */
    buf = mprCreateBuf(ejs, MPR_BUFSIZE, -1);
    mprPutStringToBuf(buf, "<?xml version=\"1.0\"?>\n");

    /*
     * Convert XML to a string
     */
    if (ejsXMLToString(ejs, buf, xml, 0) < 0) {
        mprFree(buf);
        return 0;
    }

    file = mprOpen(ejs, filename,  O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, 0664);
    if (file == 0) {
        ejsThrowIOError(ejs, "Can't open: %s, %d", filename,  mprGetOsError());
        return 0;
    }

    len = mprGetBufLength(buf);
    bytes = mprWrite(file, buf->start, len);
    if (bytes != len) {
        ejsThrowIOError(ejs, "Can't write to: %s", filename);
        mprFree(file);
        return 0;
    }
    mprWrite(file, "\n", 1);
    mprFree(buf);

    mprFree(file);

    return 0;
}


/*
 *  Convert to a JSON string
 *
 *  override function toJSON(): String
 */
static EjsVar *xmlToJson(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    EjsString       *sp;
    MprBuf          *buf;
    EjsVar          *result;
    cchar           *cp;

    /*
        Quote all quotes
     */
    sp = ejsToString(ejs, vp);
    buf = mprCreateBuf(ejs, -1, -1);
    mprPutCharToBuf(buf, '"');
    for (cp = ejsGetString(sp); *cp; cp++) {
        if (*cp == '"') {
            mprPutCharToBuf(buf, '\\');
        }
        mprPutCharToBuf(buf, (uchar) *cp);
    }
    mprPutCharToBuf(buf, '"');
    mprAddNullToBuf(buf);
    result = (EjsVar*) ejsCreateStringAndFree(ejs, mprStealBuf(vp, buf));
    mprFree(buf);
    return result;
}


/*
 *  Convert the XML object to a string.
 *
 *  intrinsic function toString() : String
 */
static EjsVar *xmlToString(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    return (vp->type->helpers->castVar)(ejs, vp, ejs->stringType);
}


/*
 *  Get the length of an array.
 *  @return Returns the number of items in the array
 *
 *  intrinsic public override function get length(): int
 */
static EjsVar *xmlLength(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, mprGetListCount(xml->elements));
}


#if KEEP
/*
 *  Set the length.
 *  intrinsic public override function set length(value: int): void
 */
static EjsVar *setLength(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    int         length;

    mprAssert(ejsIsXML(xml));

    if (argc != 1) {
        ejsThrowArgError(ejs, "usage: obj.length = value");
        return 0;
    }
    length = ejsVarToInteger(ejs, argv[0]);

    if (length < ap->length) {
        for (i = length; i < ap->length; i++) {
            if (ejsSetProperty(ejs, (EjsVar*) ap, i, (EjsVar*) ejs->undefinedValue) < 0) {
                return 0;
            }
        }

    } else if (length > ap->length) {
        if (ejsSetProperty(ejs, (EjsVar*) ap, length - 1,  (EjsVar*) ejs->undefinedValue) < 0) {
            return 0;
        }
    }

    ap->length = length;
    return 0;
}
#endif

/*
 *  Set an indexed element to an XML value
 */
EjsXML *ejsSetXML(Ejs *ejs, EjsXML *xml, int index, EjsXML *node)
{
    EjsXML      *old;

    if (xml == 0 || node == 0) {
        return 0;
    }

    if (xml->elements == 0) {
        xml->elements = mprCreateList(xml);

    } else {
        old = (EjsXML*) mprGetItem(xml->elements, index);
        if (old && old != node) {
            old->parent = 0;
        }
    }

    if (xml->kind != EJS_XML_LIST) {
        node->parent = xml;
    }
    mprSetItem(xml->elements, index, node);

    return xml;
}


EjsXML *ejsAppendToXML(Ejs *ejs, EjsXML *xml, EjsXML *node)
{
    EjsXML      *elt;
    int         next;

    if (xml == 0 || node == 0) {
        return 0;
    }
    if (xml->elements == 0) {
        xml->elements = mprCreateList(xml);
    }

    if (node->kind == EJS_XML_LIST) {
        for (next = 0; (elt = mprGetNextItem(node->elements, &next)) != 0; ) {
            if (xml->kind != EJS_XML_LIST) {
                elt->parent = xml;
            }
            mprAddItem(xml->elements, elt);
        }
        xml->targetObject = node->targetObject;
        xml->targetProperty = node->targetProperty;

    } else {
        if (xml->kind != EJS_XML_LIST) {
            node->parent = xml;
        }
        mprAddItem(xml->elements, node);
    }

    return xml;
}


int ejsAppendAttributeToXML(Ejs *ejs, EjsXML *parent, EjsXML *node)
{
    if (parent->attributes == 0) {
        parent->attributes = mprCreateList(parent);
    }
    node->parent = parent;
    return mprAddItem(parent->attributes, node);
}


static int readFileData(MprXml *xp, void *data, char *buf, int size)
{
    mprAssert(xp);
    mprAssert(data);
    mprAssert(buf);
    mprAssert(size > 0);

    return mprRead((MprFile*) data, buf, size);
}


static int readStringData(MprXml *xp, void *data, char *buf, int size)
{
    EjsXmlState *parser;
    int         rc, len;

    mprAssert(xp);
    mprAssert(buf);
    mprAssert(size > 0);

    parser = (EjsXmlState*) xp->parseArg;

    if (parser->inputPos < parser->inputSize) {
        len = min(size, (parser->inputSize - parser->inputPos));
        rc = mprMemcpy(buf, size, &parser->inputBuf[parser->inputPos], len);
        parser->inputPos += len;
        return rc;
    }
    return 0;
}


static bool allDigitsForXml(cchar *name)
{
    cchar   *cp;

    for (cp = name; *cp; cp++) {
        if (!isdigit((int) *cp) || *cp == '.') {
            return 0;
        }
    }
    return 1;
}


EjsXML *ejsCreateXML(Ejs *ejs, int kind, EjsName *qname, EjsXML *parent, cchar *value)
{
    EjsXML      *xml;

    xml = (EjsXML*) ejsAllocVar(ejs, ejs->xmlType, 0);
    if (xml == 0) {
        return 0;
    }
    if (qname) {
        xml->qname.name = mprStrdup(xml, qname->name);
        xml->qname.space = mprStrdup(xml, qname->space);
    }
    xml->kind = kind;
    xml->parent = parent;
    if (value) {
        xml->value = mprStrdup(xml, value);
    }
    return xml;
}


EjsXML *ejsConfigureXML(Ejs *ejs, EjsXML *xml, int kind, cchar *name, EjsXML *parent, cchar *value)
{
    mprFree((char*) xml->qname.name);
    xml->qname.name = mprStrdup(xml, name);
    xml->kind = kind;
    xml->parent = parent;
    if (value) {
        mprFree(xml->value);
        xml->value = mprStrdup(xml, value);
    }
    return xml;
}


/*
 *  Support routine. Not an class method
 */
void ejsLoadXMLString(Ejs *ejs, EjsXML *xml, cchar *xmlString)
{
    EjsXmlState *parser;
    MprXml      *xp;

    xp = ejsCreateXmlParser(ejs, xml, "string");
    parser = mprXmlGetParseArg(xp);

    parser->inputBuf = xmlString;
    parser->inputSize = (int) strlen(xmlString);

    mprXmlSetInputStream(xp, readStringData, (void*) 0);

    if (mprXmlParse(xp) < 0 && !ejsHasException(ejs)) {
        ejsThrowSyntaxError(ejs, "Can't parse XML string: %s",  mprXmlGetErrorMsg(xp));
    }

    mprFree(xp);
}


void ejsCreateXMLType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "XML"), ejs->objectType, sizeof(EjsXML), ES_XML,
        ES_XML_NUM_CLASS_PROP, ES_XML_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
    if (type == 0) {
        return;
    }
    ejs->xmlType = type;

    /*
     *  Must not bind as XML uses get/setPropertyByName to defer to user XML elements over XML methods
     */
    type->block.nobind = 1;

    /*
     *  Define the helper functions.
     */
    type->helpers->cloneVar = (EjsCloneVarHelper) cloneXml;
    type->helpers->castVar = (EjsCastVarHelper) castXml;
    type->helpers->createVar = (EjsCreateVarHelper) createXml;
    type->helpers->destroyVar = (EjsDestroyVarHelper) destroyXml;
    type->helpers->getPropertyByName = (EjsGetPropertyByNameHelper) getXmlPropertyByName;
    type->helpers->getPropertyCount = (EjsGetPropertyCountHelper) getXmlPropertyCount;
    type->helpers->deletePropertyByName = (EjsDeletePropertyByNameHelper) deleteXmlPropertyByName;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) invokeXmlOperator;
    type->helpers->markVar = (EjsMarkVarHelper) ejsMarkXML;
    type->helpers->setPropertyByName = (EjsSetPropertyByNameHelper) setXmlPropertyByName;
}


void ejsConfigureXMLType(Ejs *ejs)
{
    EjsType     *type;

    if ((type = ejs->xmlType) == 0) {
        return;
    }

    /*
     *  Define the XML class methods
     */
    ejsBindMethod(ejs, type, ES_XML_XML, (EjsNativeFunction) xmlConstructor);
    ejsBindMethod(ejs, type, ES_XML_load, (EjsNativeFunction) loadXml);
    ejsBindMethod(ejs, type, ES_XML_save, (EjsNativeFunction) saveXml);
    ejsBindMethod(ejs, type, ES_XML_name, (EjsNativeFunction) getXmlNodeName);

    ejsBindMethod(ejs, type, ES_XML_parent, (EjsNativeFunction) xml_parent);

    /*
     *  Override these methods
     */
    ejsBindMethod(ejs, type, ES_Object_length, (EjsNativeFunction) xmlLength);
    ejsBindMethod(ejs, type, ES_Object_toString, (EjsNativeFunction) xmlToString);
    ejsBindMethod(ejs, type, ES_Object_toJSON, (EjsNativeFunction) xmlToJson);

    ejsBindMethod(ejs, type, ES_Object_get, getXmlIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, getXmlValues);
}


#else
void __ejsXMLDummy() {}
#endif /* BLD_FEATURE_EJS_E4X */


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/xml/ejsXML.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/xml/ejsXMLList.c"
 */
/************************************************************************/

/**
 *  ejsXMLList.c - E4X XMLList type.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_EJS_E4X

/*
 *  XMLList methods
 */
static EjsVar *xl_parent(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv);

static bool allDigitsForXmlList(cchar *name);
static EjsXML *resolve(Ejs *ejs, EjsXML *obj);
static EjsXML *shallowCopy(Ejs *ejs, EjsXML *xml);


static EjsXML *createXmlListVar(Ejs *ejs, EjsType *type, int size)
{
    return (EjsXML*) ejsCreateXMLList(ejs, NULL, NULL);
}


static void destroyXmlList(Ejs *ejs, EjsXML *list)
{
    ejsFreeVar(ejs, (EjsVar*) list, -1);
}


static EjsVar *cloneXmlList(Ejs *ejs, EjsXML *list, bool deep)
{
    EjsXML  *newList;

    newList = (EjsXML*) ejsCreateVar(ejs, list->obj.var.type, 0);
    if (newList == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    return (EjsVar*) newList;
}


/*
 *  Cast the object operand to a primitive type
 */
static EjsVar *xlCast(Ejs *ejs, EjsXML *vp, EjsType *type)
{
    MprBuf      *buf;
    EjsVar      *result;
    EjsXML      *elt, *item;
    int         next;

    switch (type->id) {
    case ES_Object:
    case ES_XML:
        return (EjsVar*) vp;

    case ES_Boolean:
        return (EjsVar*) ejsCreateBoolean(ejs, 1);

    case ES_Number:
        result = xlCast(ejs, vp, ejs->stringType);
        result = (EjsVar*) ejsToNumber(ejs, result);
        return result;

    case ES_String:
        buf = mprCreateBuf(ejs, MPR_BUFSIZE, -1);
        if (mprGetListCount(vp->elements) == 1) {
            elt = mprGetFirstItem(vp->elements);
            if (elt->kind == EJS_XML_ELEMENT) {
                if (elt->elements == 0) {
                    return (EjsVar*) ejs->emptyStringValue;
                }
                if (elt->elements && mprGetListCount(elt->elements) == 1) {
                    item = mprGetFirstItem(elt->elements);
                    if (item->kind == EJS_XML_TEXT) {
                        return (EjsVar*) ejsCreateString(ejs, item->value);
                    }
                }
            }
        }
        for (next = 0; (elt = mprGetNextItem(vp->elements, &next)) != 0; ) {
            if (ejsXMLToString(ejs, buf, elt, -1) < 0) {
                mprFree(buf);
                return 0;
            }
            if (next < vp->elements->length) {
                mprPutStringToBuf(buf, " ");
            }
        }
        result = (EjsVar*) ejsCreateString(ejs, (char*) buf->start);
        mprFree(buf);
        return result;

    default:
        ejsThrowTypeError(ejs, "Can't cast to this type");
        return 0;
    }
}


static int deleteXmlListPropertyByName(Ejs *ejs, EjsXML *list, EjsName *qname)
{
    EjsXML      *elt;
    int         index, next;

    if (isdigit((int) qname->name[0]) && allDigitsForXmlList(qname->name)) {
        index = atoi(qname->name);

        elt = (EjsXML*) mprGetItem(list->elements, index);
        if (elt) {
            if (elt->parent) {
                if (elt->kind == EJS_XML_ATTRIBUTE) {
                    ejsDeletePropertyByName(ejs, (EjsVar*) elt->parent, &elt->qname);
                } else {
                    mprRemoveItem(elt->parent->elements, elt);
                    elt->parent = 0;
                }
            }
        }
        return 0;
    }

    for (next = 0; (elt = mprGetNextItem(list->elements, &next)) != 0; ) {
        if (elt->kind == EJS_XML_ELEMENT /* && elt->parent */) {
            ejsDeletePropertyByName(ejs, (EjsVar*) elt, qname);
        }
    }
    return 0;
}


static int getXmlListPropertyCount(Ejs *ejs, EjsXML *list)
{
    return mprGetListCount(list->elements);
}


/*
 *  Lookup a property by name. There are 7 kinds of lookups:
 *       prop, @att, [prop], *, @*, .name, .@name
 */
static EjsVar *getXmlListPropertyByName(Ejs *ejs, EjsXML *list, EjsName *qname)
{
    EjsXML      *result, *subList, *item;
    int         nextItem;

    /*
     *  Get the n'th item in the list
     */
    if (isdigit((int) qname->name[0]) && allDigitsForXmlList(qname->name)) {
        return mprGetItem(list->elements, atoi(qname->name));
    }

    result = ejsCreateXMLList(ejs, list, qname);

    /*
     *  Build a list of all the elements that themselves have a property qname.
     */
    for (nextItem = 0; (item = mprGetNextItem(list->elements, &nextItem)) != 0; ) {
        if (item->kind == EJS_XML_ELEMENT) {
            subList = (EjsXML*) ejsGetPropertyByName(ejs, (EjsVar*) item, qname);
            mprAssert(ejsIsXML(subList));
            ejsAppendToXML(ejs, result, subList);

        } else {
            mprAssert(0);
        }
    }

    return (EjsVar*) result;
}


static EjsVar *getXmlListNodeName(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    if (xml->targetProperty.name) {
        return (EjsVar*) ejsCreateString(ejs, xml->targetProperty.name);
    } else if (xml->targetObject) {
        return (EjsVar*) ejsCreateString(ejs, xml->targetObject->qname.name);
    } else {
        return ejs->nullValue;
    }
}



/*
 *  Function to iterate and return the next element name.
 *  NOTE: this is not a method of Xml. Rather, it is a callback function for Iterator
 */
static EjsVar *nextXmlListKey(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsXML  *xml;

    xml = (EjsXML*) ip->target;
    if (!ejsIsXML(xml)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    for (; ip->index < mprGetListCount(xml->elements); ip->index++) {
        return (EjsVar*) ejsCreateNumber(ejs, ip->index++);
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return the default iterator. This returns the array index names.
 *
 *  iterator native function get(): Iterator
 */
static EjsVar *getXmlListIterator(Ejs *ejs, EjsVar *xml, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, xml, (EjsNativeFunction) nextXmlListKey, 0, NULL);
}


/*
 *  Function to iterate and return the next element value.
 *  NOTE: this is not a method of Xml. Rather, it is a callback function for Iterator
 */
static EjsVar *nextXmlListValue(Ejs *ejs, EjsIterator *ip, int argc, EjsVar **argv)
{
    EjsXML      *xml, *vp;

    xml = (EjsXML*) ip->target;
    if (!ejsIsXML(xml)) {
        ejsThrowReferenceError(ejs, "Wrong type");
        return 0;
    }

    for (; ip->index < mprGetListCount(xml->elements); ip->index++) {
        vp = (EjsXML*) mprGetItem(xml->elements, ip->index);
        if (vp == 0) {
            continue;
        }
        ip->index++;
        return (EjsVar*) vp;
    }
    ejsThrowStopIteration(ejs);
    return 0;
}


/*
 *  Return an iterator to return the next array element value.
 *
 *  iterator native function getValues(): Iterator
 */
static EjsVar *getXmlListValues(Ejs *ejs, EjsVar *ap, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateIterator(ejs, ap, (EjsNativeFunction) nextXmlListValue, 0, NULL);
}


/*
 *  Set an alpha property by name.
 */
static int setAlphaPropertyByName(Ejs *ejs, EjsXML *list, EjsName *qname, EjsVar *value)
{
    EjsXML      *elt, *targetObject;
    int         count;

    targetObject = 0;

    count = ejsGetPropertyCount(ejs, (EjsVar*) list);
    if (count > 1) {
        mprAssert(0);
        return 0;
    }

    if (count == 0) {
        /*
         *  Empty list so resolve the real target object and append it to the list.
         */
        targetObject = resolve(ejs, list);
        if (targetObject == 0) {
            return 0;
        }
        if (ejsGetPropertyCount(ejs, (EjsVar*) targetObject) != 1) {
            return 0;
        }
        ejsAppendToXML(ejs, list, targetObject);
    }

    /*
     *  Update the element
     */
    mprAssert(ejsGetPropertyCount(ejs, (EjsVar*) list) == 1);
    elt = mprGetItem(list->elements, 0);
    mprAssert(elt);
    ejsSetPropertyByName(ejs, (EjsVar*) elt, qname, value);
    return 0;
}


static EjsXML *createElement(Ejs *ejs, EjsXML *list, EjsXML *targetObject, EjsName *qname, EjsVar *value)
{
    EjsXML      *elt, *last, *attList;
    int         index;
    int         j;

    if (targetObject && ejsIsXML(targetObject) && targetObject->kind == EJS_XML_LIST) {

        /*
         *  If the target is a list it must have 1 element. So switch to it.
         */
        if (mprGetListCount(targetObject->elements) != 1) {
            return 0;
        }
        targetObject = mprGetFirstItem(targetObject->elements);
    }

    /*
     *  Return if the target object is not an XML element
     */
    if (!ejsIsXML(targetObject) || targetObject->kind != EJS_XML_ELEMENT) {
        return 0;
    }

    elt = ejsCreateXML(ejs, EJS_XML_ELEMENT, &list->targetProperty, targetObject, NULL);

    if (list->targetProperty.name && list->targetProperty.name[0] == '@') {
        elt->kind = EJS_XML_ATTRIBUTE;
        attList = (EjsXML*) ejsGetPropertyByName(ejs, (EjsVar*) targetObject, &list->targetProperty);
        if (attList && mprGetListCount(attList->elements) > 0) {
            /* Spec says so. But this surely means you can't update an attribute? */
            return 0;
        }
    } else if (list->targetProperty.name == 0 || qname->name[0] == '*') {
        elt->kind = EJS_XML_TEXT;
        elt->qname.name = 0;
    }

    index = mprGetListCount(list->elements);

    if (elt->kind != EJS_XML_ATTRIBUTE) {
        if (targetObject) {
            if (index > 0) {
                /*
                 *  Find the place of the last list item in the resolved target object.
                 */
                last = mprGetItem(list->elements, index - 1);
                j = mprLookupItem(targetObject->elements, last);
            } else {
                j = -1;
            } 
            if (j < 0) {
                j = mprGetListCount(targetObject->elements) - 1;
            }
            if (targetObject->elements == 0) {
                targetObject->elements = mprCreateList(targetObject);
            }
            /*
             *  Insert into the target object
             */
            mprInsertItemAtPos(targetObject->elements, j + 1, elt);
        }

        if (ejsIsXML(value)) {
            if (((EjsXML*) value)->kind == EJS_XML_LIST) {
                elt->qname = ((EjsXML*) value)->targetProperty;
            } else {
                elt->qname = ((EjsXML*) value)->qname;
            }
        }

        /*
         *  Insert into the XML list
         */
        mprSetItem(list->elements, index, elt);
    }
    return (EjsXML*) mprGetItem(list->elements, index);
}


/*
 *  Update an existing element
 */
static int updateElement(Ejs *ejs, EjsXML *list, EjsXML *elt, int index, EjsVar *value)
{
    EjsXML      *node;
    EjsName     name;
    int         i, j;

    if (!ejsIsXML(value)) {
        /* Not XML or XMLList -- convert to string */
        value = ejsCastVar(ejs, value, ejs->stringType);
    }
    mprSetItem(list->elements, index, value);

    if (elt->kind == EJS_XML_ATTRIBUTE) {
        mprAssert(ejsIsString(value));
        i = mprLookupItem(elt->parent->elements, elt);
        ejsSetXML(ejs, elt->parent, i, elt);
        ejsSetPropertyByName(ejs, (EjsVar*) elt->parent, &elt->qname, value);
        mprFree(elt->value);
        elt->value = mprStrdup(elt, ((EjsString*) value)->value);
    }

    if (ejsIsXML(value) && ((EjsXML*) value)->kind == EJS_XML_LIST) {
        value = (EjsVar*) shallowCopy(ejs, (EjsXML*) value);
        if (elt->parent) {
            index = mprLookupItem(elt->parent->elements, elt);
            mprAssert(index >= 0);
            for (j = 0; j < mprGetListCount(((EjsXML*) value)->elements); j++) {
                mprInsertItemAtPos(elt->parent->elements, index, value);
            }
        }

    } else if (ejsIsXML(value) || elt->kind != EJS_XML_ELEMENT) {
        if (elt->parent) {
            index = mprLookupItem(elt->parent->elements, elt);
            mprAssert(index >= 0);
            mprSetItem(elt->parent->elements, index, value);
            ((EjsXML*) value)->parent = elt->parent;
            if (ejsIsString(value)) {
                node = ejsCreateXML(ejs, EJS_XML_TEXT, NULL, list, ((EjsString*) value)->value);
                mprSetItem(list->elements, index, node);
            } else {
                mprSetItem(list->elements, index, value);
            }
        }

    } else {
        ejsName(&name, 0, "*");
        ejsSetPropertyByName(ejs, (EjsVar*) elt, &name, value);
    }
    return index;
}


/*
 *  Set a property by name.
 */
static int setXmlListPropertyByName(Ejs *ejs, EjsXML *list, EjsName *qname, EjsVar *value)
{
    EjsXML      *elt, *targetObject;
    int         index;

    if (!isdigit((int) qname->name[0])) {
        return setAlphaPropertyByName(ejs, list, qname, value);
    }

    /*
     *  Numeric property
     */
    targetObject = 0;
    if (list->targetObject) {
        /*
         *  Find the real underlying target object. May be an XML object or XMLList if it contains multiple elements.
         */
        targetObject = resolve(ejs, list->targetObject);
        if (targetObject == 0) {
            return 0;
        }
    }
    index = atoi(qname->name);
    if (index >= mprGetListCount(list->elements)) {
        /*
         *  Create, then fall through to update
         */
        elt = createElement(ejs, list, targetObject, qname, value);
        if (elt == 0) {
            return 0;
        }

    } else {
        elt = mprGetItem(list->elements, index);
    }
    mprAssert(elt);
    updateElement(ejs, list, elt, index, value);
    return index;
}



static bool allDigitsForXmlList(cchar *name)
{
    cchar   *cp;

    for (cp = name; *cp; cp++) {
        if (!isdigit((int) *cp) || *cp == '.') {
            return 0;
        }
    }
    return 1;
}


static EjsXML *shallowCopy(Ejs *ejs, EjsXML *xml)
{
    EjsXML      *root, *elt;
    int         next;

    mprAssert(xml->kind == EJS_XML_LIST);

    if (xml == 0) {
        return 0;
    }

    root = ejsCreateXMLList(ejs, xml->targetObject, &xml->targetProperty);
    if (root == 0) {
        return 0;
    }

    if (xml->elements) {
        root->elements = mprCreateList(root);
        for (next = 0; (elt = mprGetNextItem(xml->elements, &next)) != 0; ) {
            mprAssert(ejsIsXML(elt));
            if (elt) {
                mprAddItem(root->elements, elt);
            }
        }
    }

    if (mprHasAllocError(ejs)) {
        mprFree(root);
        return 0;
    }

    return root;
}


/*
 *  Resolve empty XML list objects to an actual XML object. This is used by SetPropertyByName to find the actual object to update.
 *  This method resolves the value of empty XMLLists. If the XMLList is not empty, the list will be returned. If list is empty,
 *  this method attempts to create an element based on the list targetObject and targetProperty.
 */
static EjsXML *resolve(Ejs *ejs, EjsXML *xml)
{
    EjsXML  *targetObject, *targetPropertyList;

    if (!ejsIsXML(xml) || xml->kind != EJS_XML_LIST) {
        /* Resolved to an XML object */
        return xml;
    }

    if (mprGetListCount(xml->elements) > 0) {
        /* Resolved to a list of items */
        return xml;
    }

    if (xml->targetObject == 0 || xml->targetProperty.name == 0 || xml->targetProperty.name[0] == '*') {
        /* End of chain an no more target objects */
        return 0;
    }

    targetObject = resolve(ejs, xml->targetObject);
    if (targetObject == 0) {
        return 0;
    }
    targetPropertyList = (EjsXML*) ejsGetPropertyByName(ejs, (EjsVar*) targetObject, &xml->targetProperty);
    if (targetPropertyList == 0) {
        return 0;
    }

    if (ejsGetPropertyCount(ejs, (EjsVar*) targetPropertyList) == 0) {
        /*
         *  Property does not exist in the target.
         */
        if (targetObject->kind == EJS_XML_LIST && ejsGetPropertyCount(ejs, (EjsVar*) targetObject) > 1) {
            return 0;
        }
        /*
         *  Create the property as an element (The text value will be optimized away).
         */
        ejsSetPropertyByName(ejs, (EjsVar*) targetObject, &xml->targetProperty, (EjsVar*) ejsCreateString(ejs, ""));
        targetPropertyList = (EjsXML*) ejsGetPropertyByName(ejs, (EjsVar*) targetObject, &xml->targetProperty);
    }
    return targetPropertyList;
}



static EjsVar *xmlListConstructor(Ejs *ejs, EjsVar *thisObj, int argc, EjsVar **argv)
{
    return (EjsVar*) thisObj;
}


/*
 *  Convert to a JSON string
 *
 *  override function toJSON(): String
 */
static EjsVar *xmlListToJson(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    EjsString       *sp;
    MprBuf          *buf;
    EjsVar          *result;
    cchar           *cp;

    /*
        Quote all quotes
     */
    sp = ejsToString(ejs, vp);
    buf = mprCreateBuf(ejs, -1, -1);
    mprPutCharToBuf(buf, '"');
    for (cp = ejsGetString(sp); *cp; cp++) {
        if (*cp == '"') {
            mprPutCharToBuf(buf, '\\');
        }
        mprPutCharToBuf(buf, (uchar) *cp);
    }
    mprPutCharToBuf(buf, '"');
    mprAddNullToBuf(buf);
    result = (EjsVar*) ejsCreateStringAndFree(ejs, mprStealBuf(vp, buf));
    mprFree(buf);
    return result;
}


/*
 *  Convert the XML object to a string.
 *
 *  intrinsic function toString() : String
 */
static EjsVar *xmlListToString(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    return (vp->type->helpers->castVar)(ejs, vp, ejs->stringType);
}


/*
 *  Get the length of an array.
 *  @return Returns the number of items in the array
 *
 *  intrinsic public override function get length(): int
 */

static EjsVar *xlLength(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    return (EjsVar*) ejsCreateNumber(ejs, mprGetListCount(xml->elements));
}


/*
    function parent(): XML
 */
static EjsVar *xl_parent(Ejs *ejs, EjsXML *xml, int argc, EjsVar **argv)
{
    return xml->targetObject ? (EjsVar*) xml->targetObject : (EjsVar*) ejs->nullValue;
}


EjsXML *ejsCreateXMLList(Ejs *ejs, EjsXML *targetObject, EjsName *targetProperty)
{
    EjsType     *type;
    EjsXML      *list;

    type = ejs->xmlListType;

    list = (EjsXML*) ejsAllocVar(ejs, type, 0);
    if (list == 0) {
        return 0;
    }
    list->kind = EJS_XML_LIST;
    list->elements = mprCreateList(list);
    list->targetObject = targetObject;

    if (targetProperty) {
        list->targetProperty.name = mprStrdup(list, targetProperty->name);
    }

#if NOT_NEEDED
    /*
     *  Temporary until we have namespaces
     */
    char        *cp;
    for (cp = name; *cp; cp++) {
        if (*cp == ':') {
            *cp = '_';
        }
    }
#endif

    return list;
}


void ejsCreateXMLListType(Ejs *ejs)
{
    EjsType     *type;
    EjsName     qname;

    type = ejsCreateCoreType(ejs, ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "XMLList"), ejs->objectType, sizeof(EjsXML), 
        ES_XMLList, ES_XMLList_NUM_CLASS_PROP, ES_XMLList_NUM_INSTANCE_PROP, EJS_ATTR_NATIVE | EJS_ATTR_HAS_CONSTRUCTOR);
    if (type == 0) {
        return;
    }
    ejs->xmlListType = type;

    /*
     *  Must not bind as XML uses get/setPropertyByName to defer to user XML elements over XML methods
     */
    type->block.nobind = 1;

    /*
     *  Define the helper functions.
     */
    type->helpers->cloneVar = (EjsCloneVarHelper) cloneXmlList;
    type->helpers->castVar = (EjsCastVarHelper) xlCast;
    type->helpers->createVar = (EjsCreateVarHelper) createXmlListVar;
    type->helpers->destroyVar = (EjsDestroyVarHelper) destroyXmlList;
    type->helpers->getPropertyByName = (EjsGetPropertyByNameHelper) getXmlListPropertyByName;
    type->helpers->getPropertyCount = (EjsGetPropertyCountHelper) getXmlListPropertyCount;
    type->helpers->deletePropertyByName = (EjsDeletePropertyByNameHelper) deleteXmlListPropertyByName;
    type->helpers->invokeOperator = (EjsInvokeOperatorHelper) ejsObjectOperator;
    type->helpers->markVar = (EjsMarkVarHelper) ejsMarkXML;
    type->helpers->setPropertyByName = (EjsSetPropertyByNameHelper) setXmlListPropertyByName;
}


void ejsConfigureXMLListType(Ejs *ejs)
{
    EjsType     *type;

    if ((type = ejs->xmlListType) == 0) {
        return;
    }

    /*
     *  Define the XMLList class methods
     */
    ejsBindMethod(ejs, type, ES_XMLList_XMLList, (EjsNativeFunction) xmlListConstructor);
    ejsBindMethod(ejs, type, ES_XMLList_name, (EjsNativeFunction) getXmlListNodeName);

    ejsBindMethod(ejs, type, ES_XMLList_parent, (EjsNativeFunction) xl_parent);

    /*
     *  Override these methods
     */
    ejsBindMethod(ejs, type, ES_Object_toJSON, (EjsNativeFunction) xmlListToJson);
    ejsBindMethod(ejs, type, ES_Object_toString, (EjsNativeFunction) xmlListToString);
    ejsBindMethod(ejs, type, ES_Object_length, (EjsNativeFunction) xlLength);

    ejsBindMethod(ejs, type, ES_Object_get, getXmlListIterator);
    ejsBindMethod(ejs, type, ES_Object_getValues, getXmlListValues);
}


#else
void __ejsXMLListDummy() {}
#endif /* BLD_FEATURE_EJS_E4X */


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/xml/ejsXMLList.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/types/xml/ejsXMLLoader.c"
 */
/************************************************************************/

/**
 *  ejsXMLLoader.c - Load and save XML data.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_EJS_E4X


static void indent(MprBuf *bp, int level);
static int  parserHandler(MprXml *xp, int state, cchar *tagName, cchar *attName, cchar *value);


MprXml *ejsCreateXmlParser(Ejs *ejs, EjsXML *xml, cchar *filename)
{
    EjsXmlState *parser;
    MprXml      *xp;
    
    xp = mprXmlOpen(ejs, MPR_BUFSIZE, EJS_E4X_BUF_MAX);
    mprAssert(xp);

    /*
     *  Create the parser stack
     */
    parser = mprAllocObjZeroed(xp, EjsXmlState);
    if (parser == 0) {
        mprFree(xp);
        return 0;
    }
    parser->ejs = ejs;
    parser->nodeStack[0].obj = xml;
    
    parser->xmlType = ejs->xmlType;
    parser->xmlListType = ejs->xmlListType;
    parser->filename = filename;

    mprXmlSetParseArg(xp, parser);
    mprXmlSetParserHandler(xp, parserHandler);

    return xp;
}


/*
 *  XML parsing callback. Called for each elt and attribute/value pair. 
 *  For speed, we handcraft the object model here rather than calling 
 *  putXmlProperty.
 *
 *  "<!-- txt -->"      parserHandler(, , MPR_XML_COMMENT);
 *  "<elt"              parserHandler(, , MPR_XML_NEW_ELT);
 *  "...att=value"      parserHandler(, , MPR_XML_NEW_ATT);
 *  "<elt ...>"         parserHandler(, , MPR_XML_ELT_DEFINED);
 *  "<elt/>"            parserHandler(, , MPR_XML_SOLO_ELT_DEFINED);
 *  "<elt> ...<"        parserHandler(, , MPR_XML_ELT_DATA);
 *  "...</elt>"         parserHandler(, , MPR_XML_END_ELT);
 *
 *  Note: we recurse on every new nested elt.
 */

static int parserHandler(MprXml *xp, int state, cchar *tagName, cchar *attName, cchar *value)
{
    Ejs             *ejs;
    EjsXmlState     *parser;
    EjsXmlTagState  *tos;
    EjsName         qname;
    EjsXML          *xml, *node, *parent;

    parser = (EjsXmlState*) xp->parseArg;
    ejs = parser->ejs;
    tos = &parser->nodeStack[parser->topOfStack];
    xml = tos->obj;
    
    mprAssert(xml);

    mprAssert(state >= 0);
    mprAssert(tagName && *tagName);

    switch (state) {
    case MPR_XML_PI:
        node = ejsCreateXML(ejs, EJS_XML_PROCESSING, NULL, xml, value);
        ejsAppendToXML(ejs, xml, node);
        break;

    case MPR_XML_COMMENT:
        node = ejsCreateXML(ejs, EJS_XML_COMMENT, NULL, xml, value);
        ejsAppendToXML(ejs, xml, node);
        break;

    case MPR_XML_NEW_ELT:
        if (parser->topOfStack > E4X_MAX_NODE_DEPTH) {
            ejsThrowSyntaxError(ejs,  "XML nodes nested too deeply in %s at line %d", parser->filename, 
                mprXmlGetLineNumber(xp));
            return MPR_ERR_BAD_SYNTAX;
        }
        if (xml->kind <= 0) {
            ejsConfigureXML(ejs, xml, EJS_XML_ELEMENT, tagName, xml, NULL);
        } else {
            ejsName(&qname, 0, tagName);
            xml = ejsCreateXML(ejs, EJS_XML_ELEMENT, &qname, xml, NULL);
            tos = &parser->nodeStack[++(parser->topOfStack)];
            tos->obj = (EjsXML*) xml;
            tos->attributes = 0;
            tos->comments = 0;
        }
        break;

    case MPR_XML_NEW_ATT:
        ejsName(&qname, 0, attName);
        node = ejsCreateXML(ejs, EJS_XML_ATTRIBUTE, &qname, xml, value);
        ejsAppendAttributeToXML(ejs, xml, node);
        break;

    case MPR_XML_SOLO_ELT_DEFINED:
        if (parser->topOfStack > 0) {
            parent = parser->nodeStack[parser->topOfStack - 1].obj;
            ejsAppendToXML(ejs, parent, xml);
            parser->topOfStack--;
            mprAssert(parser->topOfStack >= 0);
            tos = &parser->nodeStack[parser->topOfStack];
        }
        break;

    case MPR_XML_ELT_DEFINED:
        if (parser->topOfStack > 0) {
            parent = parser->nodeStack[parser->topOfStack - 1].obj;
            ejsAppendToXML(ejs, parent, xml);
        }
        break;

    case MPR_XML_ELT_DATA:
    case MPR_XML_CDATA:
        ejsName(&qname, 0, attName);
        node = ejsCreateXML(ejs, EJS_XML_TEXT, &qname, xml, value);
        ejsAppendToXML(ejs, xml, node);
        break;

    case MPR_XML_END_ELT:
        /*
         *  This is the closing element in a pair "<x>...</x>".
         *  Pop the stack frame off the elt stack
         */
        if (parser->topOfStack > 0) {
            parser->topOfStack--;
            mprAssert(parser->topOfStack >= 0);
            tos = &parser->nodeStack[parser->topOfStack];
        }
        break;

    default:
        ejsThrowSyntaxError(ejs, "XML error in %s at %d\nDetails %s", parser->filename, mprXmlGetLineNumber(xp), 
            mprXmlGetErrorMsg(xp));
        mprAssert(0);
        return MPR_ERR_BAD_SYNTAX;
    }
    return 0;
}


#if KEEP
static bool checkTagName(char *name)
{
    char    *cp;

    for (cp = name; *cp; cp++) {
        if (!isalnum(*cp) && *cp != '_' && *cp != '$' && *cp != '@') {
            return 0;
        }
    }
    return 1;
}
#endif


int ejsXMLToString(Ejs *ejs, MprBuf *buf, EjsXML *node, int indentLevel)
{
    EjsXML      *xml, *child, *attribute, *elt;
    int         sawElements, next;
    
    if (node->obj.var.visited) {
        return 0;
    }
    node->obj.var.visited = 1;

    if (node->kind == EJS_XML_LIST) {
        for (next = 0; (elt = mprGetNextItem(node->elements, &next)) != 0; ) {
            ejsXMLToString(ejs, buf, elt, indentLevel);
        }
        return 0;
    }
    
    mprAssert(ejsIsXML(node));
    xml = (EjsXML*) node;
    
    switch (xml->kind) {
    case EJS_XML_ELEMENT:
        /*
         *  XML object is complex (has elements) so return full XML content.
         */
        if (indentLevel > 0) {
            mprPutCharToBuf(buf, '\n');
        }
        indent(buf, indentLevel);

        mprPutFmtToBuf(buf, "<%s", xml->qname.name);
        if (xml->attributes) {
            for (next = 0; (attribute = mprGetNextItem(xml->attributes, &next)) != 0; ) {
                mprPutFmtToBuf(buf, " %s=\"%s\"",  attribute->qname.name, attribute->value);
            }
        }
        
        sawElements = 0;
        if (xml->elements) {
            mprPutStringToBuf(buf, ">"); 
            for (next = 0; (child = mprGetNextItem(xml->elements, &next)) != 0; ) {
                if (child->kind != EJS_XML_TEXT) {
                    sawElements++;
                }
    
                /* Recurse */
                if (ejsXMLToString(ejs, buf, child, indentLevel < 0 ? -1 : indentLevel + 1) < 0) {
                    return -1;
                }
            }
            if (sawElements && indentLevel >= 0) {
                mprPutCharToBuf(buf, '\n');
                indent(buf, indentLevel);
            }
            mprPutFmtToBuf(buf, "</%s>", xml->qname.name);
            
        } else {
            /* Solo */
            mprPutStringToBuf(buf, "/>");
        }
        break;
        
    case EJS_XML_COMMENT:
        mprPutCharToBuf(buf, '\n');
        indent(buf, indentLevel);
        mprPutFmtToBuf(buf, "<!--%s -->", xml->value);
        break;
        
    case EJS_XML_ATTRIBUTE:
        /*
         *  Only here when converting solo attributes to a string
         */
        mprPutStringToBuf(buf, xml->value);
        break;
        
    case EJS_XML_TEXT:
        mprPutStringToBuf(buf, xml->value);
        break;
    }
    node->obj.var.visited = 0;
    return 0;
}


static void indent(MprBuf *bp, int level)
{
    int     i;

    for (i = 0; i < level; i++) {
        mprPutCharToBuf(bp, '\t');
    }
}


#else
void __ejsXMLLoaderDummy() {}
#endif /* BLD_FEATURE_EJS_E4X */


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/types/xml/ejsXMLLoader.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsByteCode.c"
 */
/************************************************************************/

/**
 *  ejsByteCode.c - Definition of the byte code table.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*
 *  This will define an instance of the EjsOptable which is defined in ejsByteCodeTable.h
 */
#define EJS_DEFINE_OPTABLE 1


EjsOptable *ejsGetOptable(MprCtx ctx)
{
    return ejsOptable;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsByteCode.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsException.c"
 */
/************************************************************************/

/**
 *  ejsException.c - Error Exception class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Throw an exception.
 */
EjsVar *ejsThrowException(Ejs *ejs, EjsVar *obj)
{
    mprAssert(obj);

    /*
     *  Set ejs->exception. The VM will notice this and initiate exception handling
     */
    ejs->exception = obj;
    ejs->attention = 1;
    return obj;
}

void ejsClearException(Ejs *ejs)
{
    ejs->exception = 0;
}


/*
 *  Create an exception object.
 */
static EjsVar *createException(Ejs *ejs, EjsType *type, cchar* fmt, va_list fmtArgs)
{
    EjsVar          *error, *argv[1];
    char            *msg;

    mprAssert(type);

    if (ejs->noExceptions) {
        return 0;
    }
    msg = mprVasprintf(ejs, -1, fmt, fmtArgs);
    argv[0] = (EjsVar*) ejsCreateString(ejs, msg);

    if (argv[0] == 0) {
        mprAssert(argv[0]);
        return 0;
    }
    error = (EjsVar*) ejsCreateInstance(ejs, type, 1, argv);
    if (error == 0) {
        mprAssert(error);
        return 0;
    }
    mprFree(msg);
    return error;
}


EjsVar *ejsCreateException(Ejs *ejs, int slot, cchar *fmt, va_list fmtArgs)
{
    EjsType     *type;
    EjsVar      *error;
    char        *buf;

    if (ejs->exception) {
        buf = mprVasprintf(ejs, 0, fmt, fmtArgs);
        mprError(ejs, "Double exception: %s", buf);
        mprFree(buf);
        return ejs->exception;
    }
    if (!ejs->initialized || (ejs->flags & EJS_FLAG_EMPTY)) {
        buf = mprVasprintf(ejs, 0, fmt, fmtArgs);
        mprError(ejs, "Exception: %s", buf);
        mprFree(buf);
        return ejs->exception;
    }
    type = (EjsType*) ejsGetProperty(ejs, ejs->global, slot);
    if (type == 0) {
        type = ejs->errorType;
    }
    error = createException(ejs, type, fmt, fmtArgs);
    if (error) {
        ejsThrowException(ejs, error);
    }
    return error;
}


EjsVar *ejsThrowArgError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_ArgError, fmt, fmtArgs);
}


EjsVar *ejsThrowArithmeticError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_ArithmeticError, fmt, fmtArgs);
}


EjsVar *ejsThrowAssertError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_AssertError, fmt, fmtArgs);
}


EjsVar *ejsThrowInstructionError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_InstructionError, fmt, fmtArgs);
}


EjsVar *ejsThrowError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_Error, fmt, fmtArgs);
}


EjsVar *ejsThrowIOError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_IOError, fmt, fmtArgs);
}


EjsVar *ejsThrowInternalError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_InternalError, fmt, fmtArgs);
}


EjsVar *ejsThrowMemoryError(Ejs *ejs)
{
    /*
        Don't do double exceptions for memory errors
     */
    if (ejs->exception == 0) {
		va_list dummy = VA_NULL;
        return ejsCreateException(ejs, ES_MemoryError, "Memory Error", dummy);
    }
    return ejs->exception;
}


EjsVar *ejsThrowOutOfBoundsError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_OutOfBoundsError, fmt, fmtArgs);
}


EjsVar *ejsThrowReferenceError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_ReferenceError, fmt, fmtArgs);
}


EjsVar *ejsThrowResourceError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_ResourceError, fmt, fmtArgs);
}


EjsVar *ejsThrowStateError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_StateError, fmt, fmtArgs);
}


EjsVar *ejsThrowSyntaxError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_SyntaxError, fmt, fmtArgs);
}


EjsVar *ejsThrowTypeError(Ejs *ejs, cchar *fmt, ...)
{
    va_list     fmtArgs;

    mprAssert(fmt);
    va_start(fmtArgs, fmt);
    return ejsCreateException(ejs, ES_TypeError, fmt, fmtArgs);
}


/*
 *  Format the stack backtrace
 */
char *ejsFormatStack(Ejs *ejs, EjsError *error)
{
    EjsType         *type;
    EjsFrame        *fp;
    cchar           *typeName, *functionName, *line, *typeSep, *codeSep;
    char            *backtrace, *traceLine;
    int             level, len, oldFlags;

    mprAssert(ejs);

    backtrace = 0;
    len = 0;
    level = 0;

    /*
     *  Pretend to be the compiler so we can access function frame names
     */
    oldFlags = ejs->flags;
    ejs->flags |= EJS_FLAG_COMPILER;

    for (fp = ejs->state->fp; fp; fp = fp->caller) {

        typeName = "";
        functionName = "global";

        if (fp->currentLine == 0) {
            line = "";
        } else {
            for (line = fp->currentLine; *line && isspace((int) *line); line++) {
                ;
            }
        }
        if (fp) {
            if (fp->function.owner && fp->function.slotNum >= 0) {
                functionName = ejsGetPropertyName(ejs, fp->function.owner, fp->function.slotNum).name;
            }
            if (ejsIsType(fp->function.owner)) {
                type = (EjsType*) fp->function.owner;
                if (type) {
                    typeName = type->qname.name;
                }
            }
        }
        typeSep = (*typeName) ? "." : "";
        codeSep = (*line) ? "->" : "";

        if (error && backtrace == 0) {
            error->filename = mprStrdup(error, fp->filename);
            error->lineNumber = fp->lineNumber;
        }
        if ((traceLine = mprAsprintf(ejs, MPR_MAX_STRING, " [%02d] %s, %s%s%s, line %d %s %s\n",
                level++, fp->filename ? fp->filename : "script", typeName, typeSep, functionName,
                fp->lineNumber, codeSep, line)) == NULL) {
            break;
        }
        backtrace = (char*) mprRealloc(ejs, backtrace, len + (int) strlen(traceLine) + 1);
        if (backtrace == 0) {
            return 0;
        }
        memcpy(&backtrace[len], traceLine, strlen(traceLine) + 1);
        len += (int) strlen(traceLine);
        mprFree(traceLine);
    }
    ejs->flags = oldFlags;
    if (error) {
        error->stack = backtrace;
    }
    return backtrace;
}


/*
 *  Public routine to set the error message. Caller MUST NOT free.
 */
char *ejsGetErrorMsg(Ejs *ejs, int withStack)
{
    EjsVar      *message, *stack, *error;
    cchar       *name;
    char        *buf;

    if (ejs->flags & EJS_FLAG_EMPTY) {
        return "";
    }

    error = (EjsVar*) ejs->exception;
    message = stack = 0;
    name = 0;

    if (error) {
        name = error->type->qname.name;
        if (ejsIsA(ejs, error, ejs->errorType)) {
            message = ejsGetProperty(ejs, error, ES_Error_message);
            stack = ejsGetProperty(ejs, error, ES_Error_stack);

        } else if (ejsIsString(error)) {
            name = "Error";
            message = error;

        } else if (ejsIsNumber(error)) {
            name = "Error";
            message = error;
            
        } else if (error == (EjsVar*) ejs->stopIterationType) {
            name = "StopIteration";
            message = (EjsVar*) ejsCreateString(ejs, "Uncaught StopIteration exception");
        }
    }
    if (!withStack) {
        stack = 0;
    }

    if (stack && ejsIsString(stack) && message && ejsIsString(message)){
        buf = mprAsprintf(ejs, -1, "%s Exception: %s\nStack:\n%s", name, ((EjsString*) message)->value, 
            ((EjsString*) stack)->value);

    } else if (message && ejsIsString(message)){
        buf = mprAsprintf(ejs, -1, "%s: %s", name, ((EjsString*) message)->value);

    } else if (message && ejsIsNumber(message)){
        buf = mprAsprintf(ejs, -1, "%s: %d", name, ((EjsNumber*) message)->value);
        
    } else {
        if (error) {
            buf = mprStrdup(ejs, "Unknown exception object type");
        } else {
            buf = mprStrdup(ejs, "");
        }
    }
    mprFree(ejs->errorMsg);
    ejs->errorMsg = buf;
    return buf;
}


bool ejsHasException(Ejs *ejs)
{
    return ejs->exception != 0;
}


EjsVar *ejsGetException(Ejs *ejs)
{
    return ejs->exception;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsException.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsGarbage.c"
 */
/************************************************************************/

/**
 *  ejsGarbage.c - EJS Garbage collector.
 *
 *  This implements a non-compacting, generational mark and sweep collection algorithm with 
 *  fast pooled object allocations.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



static void mark(Ejs *ejs, int generation);
static void markGlobal(Ejs *ejs, int generation);
static inline bool memoryUsageOk(Ejs *ejs);
static inline void pruneTypePools(Ejs *ejs);
static void resetMarks(Ejs *ejs);
static void sweep(Ejs *ejs, int generation);

#if BLD_DEBUG
/*
 *  For debugging it can be helpful to disable the pooling of objects. Because mprFree will fill freed objects in 
 *  debug mode - bad frees will show up quickly with seg faults.
 *
 *  Break on the allocation, mark and freeing of a nominated object
 *  Set ejsBreakAddr to the address of the object to watch and set a breakpoint in the debugger below.
 *  Set ejsBreakSeq to the object sequence number to watch and set a breakpoint in the debugger below.
 */
static void *ejsBreakAddr = (void*) 0;
static int ejsBreakSeq = 0;
static void checkAddr(EjsVar *addr) {
    if ((void*) addr == ejsBreakAddr) { 
        /* Set a breakpoint here */
        addr = ejsBreakAddr;
    }
    if (addr->seq == ejsBreakSeq) {
        addr->seq = ejsBreakSeq;
    }
}
/*
 *  Unique allocation sequence. Helps GC debugging.
 */
static int nextSequence;

#else /* !BLD_DEBUG */
#define checkAddr(addr)
#undef ejsAddToGcStats
#define ejsAddToGcStats(ejs, vp, id)
#endif

/*
 *  Create the GC service
 */
int ejsCreateGCService(Ejs *ejs)
{
    EjsGC       *gc;
    int         i;

    mprAssert(ejs);

    gc = &ejs->gc;
    gc->enabled = !(ejs->flags & (EJS_FLAG_EMPTY));
    gc->firstGlobal = ES_global_NUM_CLASS_PROP;
    gc->numPools = EJS_MAX_TYPE;
    gc->allocGeneration = EJS_GEN_ETERNAL;
    ejs->workQuota = EJS_GC_WORK_QUOTA;

    for (i = 0; i < EJS_MAX_GEN; i++) {
        gc->generations[i] = mprAllocObjZeroed(ejs->heap, EjsGen);
    }
    for (i = 0; i < EJS_MAX_TYPE; i++) {
        gc->pools[i] = mprAllocObjZeroed(ejs->heap, EjsPool);
    }
    ejs->currentGeneration = ejs->gc.generations[EJS_GEN_ETERNAL];
    return 0;
}


void ejsDestroyGCService(Ejs *ejs)
{
    EjsGC       *gc;
    EjsGen      *gen;
    EjsVar      *vp;
    MprBlk      *bp, *next;
    int         generation;
    
    gc = &ejs->gc;
    for (generation = EJS_GEN_ETERNAL; generation >= 0; generation--) {
        gen = gc->generations[generation];
        for (bp = mprGetFirstChild(gen); bp; bp = next) {
            next = bp->next;
            vp = MPR_GET_PTR(bp);
            checkAddr(vp);
            if (vp->type->needFinalize) {
                (vp->type->helpers->destroyVar)(ejs, vp);
            }
        }
    }
}


/*
 *  Allocate a new variable. Size is set to the extra bytes for properties in addition to the type's instance size.
 */
EjsVar *ejsAllocVar(Ejs *ejs, EjsType *type, int extra)
{
    EjsVar      *vp;

    mprAssert(type);

    if ((vp = (EjsVar*) mprAllocZeroed(ejsGetAllocCtx(ejs), type->instanceSize + extra)) == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    vp->type = type;
    vp->master = (ejs->master == 0);
    ejsAddToGcStats(ejs, vp, type->id);
    if (++ejs->workDone >= ejs->workQuota) {
        ejs->gcRequired = 1;
        ejs->attention = 1;
    }
    return (EjsVar*) vp;
}


EjsVar *ejsAllocPooledVar(Ejs *ejs, int id)
{
    EjsPool     *pool;
    EjsVar      *vp;
    MprBlk      *bp, *gp;

    if (id < ejs->gc.numPools) {
        pool = ejs->gc.pools[id];
        if ((bp = mprGetFirstChild(pool)) != NULL) {
            /*
             *  Transfer from the pool to the current generation. Inline for speed.
             */
            gp = MPR_GET_BLK(ejs->currentGeneration);
            if (bp->prev) {
                bp->prev->next = bp->next;
            } else {
                bp->parent->children = bp->next;
            }
            if (bp->next) {
                bp->next->prev = bp->prev;
            }
            bp->parent = gp;
            if (gp->children) {
                gp->children->prev = bp;
            }
            bp->next = gp->children;
            gp->children = bp;
            bp->prev = 0;

            vp = MPR_GET_PTR(bp);
            memset(vp, 0, pool->type->instanceSize);
            vp->type = pool->type;
            vp->master = (ejs->master == 0);
#if BLD_DEBUG
            vp->seq = nextSequence++;
            checkAddr((EjsVar*) vp);
            pool->reuse++;
            pool->count--;
            mprAssert(pool->count >= 0);
            ejsAddToGcStats(ejs, vp, id);
#endif
            if (++ejs->workDone >= ejs->workQuota) {
                ejs->gcRequired = 1;
                ejs->attention = 1;
            }
            return vp;
        }
    }
    return 0;
}


/*
 *  Free a variable. This is should only ever be called by the destroyVar helpers to free a object or recycle the 
 *  object to a type specific free pool. 
 */
void ejsFreeVar(Ejs *ejs, EjsVar *vp, int id)
{
    EjsType     *type;
    EjsPool     *pool;
    EjsGC       *gc;
    MprBlk      *bp, *pp;

    mprAssert(vp);
    checkAddr(vp);

    gc = &ejs->gc;
    type = vp->type;
    if (id < 0) {
        id = type->id;
    }

    if (!vp->noPool && !type->dontPool && 0 <= id && id < gc->numPools) {
        /*
         *  Transfer from the current generation back to the pool. Inline for speed.
         */
        pool = gc->pools[id];
        pool->type = vp->type; 
        pp = MPR_GET_BLK(pool);
        bp = MPR_GET_BLK(vp);
        if (bp->prev) {
            bp->prev->next = bp->next;
        } else {
            bp->parent->children = bp->next;
        }
        if (bp->next) {
            bp->next->prev = bp->prev;
        }
        if (bp->children) {
            mprFreeChildren(vp);
        }
        /*
         *  Add to the pool
         */
        bp->parent = pp;
        if (pp->children) {
            pp->children->prev = bp;
        }
        bp->next = pp->children;
        pp->children = bp;
        bp->prev = 0;

#if BLD_DEBUG
        vp->type = (void*) -1;
        pool->allocated--;
        mprAssert(pool->allocated >= 0);
        pool->count++;
        if (pool->count > pool->peakCount) {
            pool->peakCount = pool->count;
        }
#endif
    } else {
#if BLD_DEBUG
        vp->type = (void*) -1;
        if (0 <= id && id < gc->numPools) {
            pool = gc->pools[id];
            pool->allocated--;
            mprAssert(pool->allocated >= 0);
        }
#endif
        mprFree(vp);
    }
}


/*
 *  Collect the garbage. This is a mark and sweep over all possible objects. If an object is not referenced, it and 
 *  all contained properties will be freed. Collection is done in generations.
 */
void ejsCollectGarbage(Ejs *ejs, int gen)
{
    EjsGC       *gc;
    
    gc = &ejs->gc;
    if (!gc->enabled || gc->collecting || !ejs->initialized) {
        return;
    }
    gc->collecting = 1;

    mark(ejs, gen);
    sweep(ejs, gen);
    if (!memoryUsageOk(ejs)) {
        pruneTypePools(ejs);
    }
    ejs->workDone = 0;
    ejs->gcRequired = 0;
    gc->collecting = 0;
#if BLD_DEBUG
    gc->totalSweeps++;
#endif
}


/*
 *  Mark phase. Mark objects that are still in use and should not be collected.
 */
static void mark(Ejs *ejs, int generation)
{
    EjsModule       *mp;
    EjsGC           *gc;
    EjsBlock        *block;
    EjsVar          *vp, **sp, **top;
    int             next;

    gc = &ejs->gc;
    gc->collectGeneration = generation;

    resetMarks(ejs);
    markGlobal(ejs, generation);

    if (ejs->result) {
        ejsMarkVar(ejs, NULL, ejs->result);
    }
    if (ejs->exception) {
        ejsMarkVar(ejs, NULL, ejs->exception);
    }
    if (ejs->exceptionArg) {
        ejsMarkVar(ejs, NULL, ejs->exceptionArg);
    }
    if (ejs->memoryCallback) {
        ejsMarkVar(ejs, NULL, (EjsVar*) ejs->memoryCallback);
    }

    /*
     *  Mark initializers
     */
    for (next = 0; (mp = (EjsModule*) mprGetNextItem(ejs->modules, &next)) != 0;) {
        if (mp->initializer) {
            ejsMarkVar(ejs, NULL, (EjsVar*) mp->initializer);
        }
    }

    /*
     *  Mark blocks. This includes frames and blocks.
     */
    for (block = ejs->state->bp; block; block = block->prev) {
        ejsMarkVar(ejs, NULL, (EjsVar*) block);
    }

    /*
     *  Mark the evaluation stack
     */
    top = ejs->state->stack;
    for (sp = ejs->state->stackBase; sp <= top; sp++) {
        if ((vp = *sp) != NULL) {
            ejsMarkVar(ejs, NULL, vp);
        }
    }
}


/*
 *  Sweep up the garbage for a given generation
 */
static void sweep(Ejs *ejs, int maxGeneration)
{
    EjsVar      *vp;
    EjsGC       *gc;
    EjsGen      *gen;
    MprBlk      *bp, *next;
    int         destroyed, generation;
    
    /*
     *  Go from oldest to youngest incase moving objects to elder generations and we clear the mark.
     */
    gc = &ejs->gc;
    for (generation = maxGeneration; generation >= 0; generation--) {
        gc->collectGeneration = generation;
        gen = gc->generations[generation];

        for (destroyed = 0, bp = mprGetFirstChild(gen); bp; bp = next) {
            next = bp->next;
            vp = MPR_GET_PTR(bp);
            checkAddr(vp);
            if (!vp->marked && !vp->permanent) {
                (vp->type->helpers->destroyVar)(ejs, vp);
                destroyed++;
            }
        }
#if BLD_DEBUG
        gc->allocatedObjects -= destroyed;
        gc->totalReclaimed += destroyed;
        gen->totalReclaimed += destroyed;
        gen->totalSweeps++;
#endif
    }
}


/*
    Reset all marks prior to doing a mark/sweep
 */
static void resetMarks(Ejs *ejs)
{
    EjsGen      *gen;
    EjsGC       *gc;
    EjsVar      *vp;
    EjsBlock    *block, *b;
    MprBlk      *bp;
    int         i;

    gc = &ejs->gc;
    for (i = 0; i < EJS_MAX_GEN; i++) {
        gen = gc->generations[i];
        for (bp = mprGetFirstChild(gen); bp; bp = bp->next) {
            vp = MPR_GET_PTR(bp);
            vp->marked = 0;
        }
    }
    for (block = ejs->state->bp; block; block = block->prev) {
        block->obj.var.marked = 0;
        if (block->prevException) {
            block->prevException->marked = 0;
        }
        for (b = block->scopeChain; b; b = b->scopeChain) {
            b->obj.var.marked = 0;
        }
    }
}

    
/*
    Mark the global object
 */
static void markGlobal(Ejs *ejs, int generation)
{
    EjsGC       *gc;
    EjsObject   *obj;
    EjsBlock    *block;
    EjsVar      *item;
    MprHash     *hp;
    int         i, next;

    gc = &ejs->gc;

    obj = (EjsObject*) ejs->global;
    obj->var.marked = 1;

    if (generation == EJS_GEN_ETERNAL) {
        for (i = 0; i < obj->numProp; i++) {
            ejsMarkVar(ejs, NULL, obj->slots[i]);
        }
        for (hp = 0; (hp = mprGetNextHash(ejs->standardSpaces, hp)) != 0; ) {
            ejsMarkVar(ejs, NULL, (EjsVar*) hp->data);
        }

    } else {
        for (i = gc->firstGlobal; i < obj->numProp; i++) {
            ejsMarkVar(ejs, NULL, obj->slots[i]);
        }
    }
    block = ejs->globalBlock;
    if (block->prevException) {
        ejsMarkVar(ejs, (EjsVar*) block, (EjsVar*) block->prevException);
    }
    if (block->namespaces.length > 0) {
        for (next = 0; ((item = (EjsVar*) ejsGetNextItem(&block->namespaces, &next)) != 0); ) {
            ejsMarkVar(ejs, (EjsVar*) block, item);
        }
    }
}


/*
 *  Mark a variable as used. All variable marking comes through here.
 *  NOTE: The container is not used by anyone (verified).
 */
void ejsMarkVar(Ejs *ejs, EjsVar *container, EjsVar *vp)
{
    if (vp && !vp->marked) {
        checkAddr(vp);
        vp->marked = 1;
        (vp->type->helpers->markVar)(ejs, container, vp);
    }
}


static inline bool memoryUsageOk(Ejs *ejs)
{
    MprAlloc    *alloc;
    int64        memory;

    memory = mprGetUsedMemory(ejs);
    alloc = mprGetAllocStats(ejs);
    return memory < alloc->redLine;
}


static inline void pruneTypePools(Ejs *ejs)
{
    EjsPool     *pool;
    EjsGC       *gc;
    EjsVar      *vp;
    MprAlloc    *alloc;
    MprBlk      *bp, *next;
    int64       memory;
    int         i;

    gc = &ejs->gc;

    /*
     *  Still insufficient memory, must reclaim all objects from the type pools.
     */
    for (i = 0; i < gc->numPools; i++) {
        pool = gc->pools[i];
        if (pool->count) {
            for (bp = mprGetFirstChild(pool); bp; bp = next) {
                next = bp->next;
                vp = MPR_GET_PTR(bp);
                mprFree(vp);
            }
            pool->count = 0;
        }
    }
    gc->totalRedlines++;

    memory = mprGetUsedMemory(ejs);
    alloc = mprGetAllocStats(ejs);

    if (memory >= alloc->maxMemory) {
        /*
         *  Could not provide sufficient memory. Go into graceful degrade mode
         */
        ejsThrowMemoryError(ejs);
        ejsGracefulDegrade(ejs);
    }
}


/*
 *  Make all eternal allocations permanent. This prevents an eternal GC from collecting core essential values like
 *  ejs->zeroValue. Do this to keep markGlobal() simple, otherwise it would have to enumerate values like this.
 */
void ejsMakeEternalPermanent(Ejs *ejs)
{
    EjsGen      *gen;
    EjsVar      *vp;
    MprBlk      *bp;

    gen = ejs->gc.generations[EJS_GEN_ETERNAL];
    for (bp = mprGetFirstChild(gen); bp; bp = bp->next) {
        vp = MPR_GET_PTR(bp);
        vp->permanent = 1;
    }
}


/*
 *  Permanent objects are never freed
 */
void ejsMakePermanent(Ejs *ejs, EjsVar *vp)
{
    vp->permanent = 1;
}


void ejsMakeTransient(Ejs *ejs, EjsVar *vp)
{
    vp->permanent = 0;
}


/*
 *  Return true if there is time to do a garbage collection and if we will benefit from it.
 *  Currently not used.
 */
int ejsIsTimeForGC(Ejs *ejs, int timeTillNextEvent)
{
    EjsGC       *gc;

    if (timeTillNextEvent < EJS_MIN_TIME_FOR_GC) {
        /*
         *  This is a heuristic where we want a good amount of idle time so that a proactive garbage collection won't 
         *  delay any I/O events.
         */
        return 0;
    }

    /*
     *  Return if we haven't done enough work to warrant a collection. Trigger a little short of the work quota to try 
     *  to run GC before a demand allocation requires it.
     */
    gc = &ejs->gc;
    if (!gc->enabled || ejs->workDone < (ejs->workQuota - EJS_GC_SHORT_WORK_QUOTA)) {
        return 0;
    }
    mprLog(ejs, 7, "Time for GC. Work done %d, time till next event %d", ejs->workDone, timeTillNextEvent);
    return 1;
}


int ejsEnableGC(Ejs *ejs, bool on)
{
    int     old;

    old = ejs->gc.enabled;
    ejs->gc.enabled = on;
    return old;
}


/*
 *  On a memory allocation failure, go into graceful degrade mode. Set all slab allocation chunk increments to 1 
 *  so we can create an exception block to throw.
 */
void ejsGracefulDegrade(Ejs *ejs)
{
    mprLog(ejs, 1, "WARNING: Memory almost depleted. In graceful degrade mode");
    ejs->gc.degraded = 1;
    mprSignalExit(ejs);
}


int ejsSetGeneration(Ejs *ejs, int generation)
{
    int     old;
    
    old = ejs->gc.allocGeneration;
    ejs->gc.allocGeneration = generation;
    ejs->currentGeneration = ejs->gc.generations[generation];
    return old;
}


#undef ejsAddToGcStats

/*
 *  Update GC stats for a new object allocation
 */
void ejsAddToGcStats(Ejs *ejs, EjsVar *vp, int id)
{
#if BLD_DEBUG
    EjsPool     *pool;
    EjsGC       *gc;

    gc = &ejs->gc;
    if (id < ejs->gc.numPools) {
        pool = ejs->gc.pools[id];
        pool->allocated++;
        mprAssert(pool->allocated >= 0);
        if (pool->allocated > pool->peakAllocated) {
            pool->peakAllocated = pool->allocated;
        }
    }
    gc->totalAllocated++;
    gc->allocatedObjects++;
    if (gc->allocatedObjects >= gc->peakAllocatedObjects) {
        gc->peakAllocatedObjects = gc->allocatedObjects;
    }
    if (vp->type == ejs->typeType) {
        gc->allocatedTypes++;
        if (gc->allocatedTypes >= gc->peakAllocatedTypes) {
            gc->peakAllocatedTypes = gc->allocatedTypes;
        }
    }
    /* Convenient place for this */
    vp->seq = nextSequence++;
    checkAddr(vp);
#endif
}


void ejsPrintAllocReport(Ejs *ejs)
{
#if BLD_DEBUG
    EjsType         *type;
    EjsGC           *gc;
    EjsPool         *pool;
    MprAlloc        *ap;
    int             i, maxSlot, typeMemory, count, peakCount, freeCount, peakFreeCount, reuseCount;

    gc = &ejs->gc;
    ap = mprGetAllocStats(ejs);
    
    /*
     *  EJS stats
     */
    mprLog(ejs, 0, "\n\nEJS Memory Statistics");
    mprLog(ejs, 0, "  Types allocated        %,14d", gc->allocatedTypes / 2);
    mprLog(ejs, 0, "  Objects allocated      %,14d", gc->allocatedObjects);
    mprLog(ejs, 0, "  Peak objects allocated %,14d", gc->peakAllocatedObjects);

    /*
     *  Per type
     */
    mprLog(ejs, 0, "\nObject Cache Statistics");
    mprLog(ejs, 0, "------------------------");
    mprLog(ejs, 0, "Name                TypeSize  ObjectSize  ObjectCount  PeakCount  FreeList  PeakFreeList   ReuseCount");
    
    maxSlot = ejsGetPropertyCount(ejs, ejs->global);
    typeMemory = 0;

    count = peakCount = freeCount = peakFreeCount = reuseCount = 0;
    for (i = 0; i < gc->numPools; i++) {
        pool = gc->pools[i];
        type = ejsGetType(ejs, i);
        if (type == 0) {
            continue;
        }
        if (type->id != i) {
            /* Skip type alias (string == String) */
            continue;
        }
        mprLog(ejs, 0, "%-22s %,5d %,8d %,10d  %,10d, %,9d, %,10d, %,14d", type->qname.name, ejsGetTypeSize(ejs, type), 
            type->instanceSize, pool->allocated, pool->peakAllocated, pool->count, pool->peakCount, pool->reuse);

        typeMemory += ejsGetTypeSize(ejs, type);
        count += pool->allocated;
        peakCount += pool->peakAllocated;
        freeCount += pool->count;
        peakFreeCount += pool->peakCount;
        reuseCount += pool->reuse;
    }
    mprLog(ejs, 0, "%-22s                %,10d  %,10d, %,9d, %,10d, %,14d", "Total", 
        count, peakCount, freeCount, peakFreeCount, reuseCount);
    mprLog(ejs, 0, "\nTotal type memory        %,14d K", typeMemory / 1024);

    mprLog(ejs, 0, "\nEJS Garbage Collector Statistics");
    mprLog(ejs, 0, "  Total allocations      %,14d", gc->totalAllocated);
    mprLog(ejs, 0, "  Total reclaimations    %,14d", gc->totalReclaimed);
    mprLog(ejs, 0, "  Total sweeps           %,14d", gc->totalSweeps);
    mprLog(ejs, 0, "  Total redlines         %,14d", gc->totalRedlines);
    mprLog(ejs, 0, "  Object GC work quota   %,14d", ejs->workQuota);
#endif
}

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsGarbage.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsInterp.c"
 */
/************************************************************************/

/*
 *  ejsInterp.c - Virtual Machine Interpreter for Ejscript.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 *
 *  NEXT
 *      - Optimize and cache stack.top
 *      - Fix BAD BINDING
 *      - Move DEBUG op codes out of line
 */



/*
 *  The stack is a stack of pointers to EjsVar. The top of stack (stack.top) always points to the current top item 
 *  on the stack. To push a new value, top is incremented then the value is stored. To pop, simply copy the value at 
 *  top and decrement top ptr.
 */
#define top                     (*state.stack)
#define pop(ejs)                (*state.stack--)

#define push(value)             (*(++(state.stack))) = ((EjsVar*) (value))
#define popString(ejs)          ((EjsString*) pop(ejs))
#define popOutside(ejs)         *(ejs->state->stack)--
#define pushOutside(ejs, value) (*(++(ejs->state->stack))) = ((EjsVar*) (value))

#define FRAME                   state.fp
#define FUNCTION                state.fp.function
#define BLOCK                   state.bp
#define HANDLE_GETTER           0x1

#define SWAP if (1) { \
        EjsVar *swap = state.stack[0]; \
        state.stack[0] = state.stack[-1]; \
        state.stack[-1] = swap; \
    }

#define TRACE if (1) { \
        FRAME->filename = GET_STRING(); \
        FRAME->lineNumber = GET_INT(); \
        FRAME->currentLine = GET_STRING(); \
    }

#define DO_GETTER(thisObj) \
    if (top && unlikely(top->isFunction) && !ejs->exception) { \
        handleGetters(ejs, (EjsFunction*) top, (EjsVar*) (thisObj)); \
        CHECK; \
    } else

/*
 *  Get a slot value when we don't know if the object is an EjsObject
 */
#define GET_SLOT(obj, slotNum) getSlot(ejs, (EjsObject*) obj, slotNum)

static MPR_INLINE EjsVar *getSlot(Ejs *ejs, EjsObject *obj, int slotNum) {
    if (ejsIsObject(obj) && slotNum < obj->numProp) {
        return obj->slots[slotNum];
    } else {
        return ejsGetProperty(ejs, (EjsVar*) obj, slotNum);
    }
}

/*
 *  Set a slot value when we don't know if the object is an EjsObject
 */
#define SET_SLOT(obj, slotNum, value, thisObj) \
    if (!storePropertyToSlot(ejs, (EjsObject*) obj, slotNum, (EjsVar*) value, (EjsVar*) thisObj)) { \
        return; \
    } else 

static MPR_INLINE void handleSetters(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, EjsVar *value);
static bool payAttention(Ejs *ejs);
static void callFunction(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, int argc, int stackAdjust);
static void callProperty(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, int argc, int stackAdjust);

#define GET_BYTE()      *(FRAME)->pc++
#define GET_DOUBLE()    ejsDecodeDouble(ejs, &(FRAME)->pc)
#define GET_INT()       (int) ejsDecodeNum(&(FRAME)->pc)
#define GET_NUM()       ejsDecodeNum(&(FRAME)->pc)
#define GET_NAME()      getNameArg(FRAME)
#define GET_STRING()    getStringArg(FRAME)
#define GET_TYPE()      ((EjsType*) getGlobalArg(ejs, FRAME))
#define GET_WORD()      ejsDecodeWord(&(FRAME)->pc)
#undef THIS
#define THIS            FRAME->function.thisObj
#define FILL(mark)      while (mark < FRAME->pc) { *mark++ = EJS_OP_NOP; }

#if BLD_DEBUG
    static EjsOpCode traceCode(Ejs *ejs, EjsOpCode opcode);
    static int opcount[256];
#else
    #define traceCode(ejs, opcode) opcode
#endif

#if BLD_UNIX_LIKE || VXWORKS
    #define CASE(opcode) opcode
    #define BREAK \
        if (1) { \
            opcode = GET_BYTE(); \
            goto *opcodeJump[traceCode(ejs, opcode)]; \
        } else
    #define CHECK \
        if (unlikely(ejs->attention) && !payAttention(ejs)) { \
            ejs->state = priorState; \
            return; \
        }
#else
    /*
     *  Traditional switch for compilers (looking at you MS) without computed goto.
     */
    #define BREAK break
    #define CHECK
    #define CASE(opcode) case opcode
#endif


static void callConstructor(Ejs *ejs, EjsFunction *vp, int argc, int stackAdjust);
static void callInterfaceInitializers(Ejs *ejs, EjsType *type);
static void callFunction(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, int argc, int stackAdjust);
static EjsVar *evalBinaryExpr(Ejs *ejs, EjsVar *lhs, EjsOpCode opcode, EjsVar *rhs);
static EjsVar *evalUnaryExpr(Ejs *ejs, EjsVar *lhs, EjsOpCode opcode);
static inline uint findEndException(Ejs *ejs);
static inline EjsEx *findExceptionHandler(Ejs *ejs, int kind);
static EjsName getNameArg(EjsFrame *fp);
static EjsVar *getNthBase(Ejs *ejs, EjsVar *obj, int nthBase);
static EjsVar *getNthBaseFromBottom(Ejs *ejs, EjsVar *obj, int nthBase);
static MPR_INLINE EjsVar *getNthBlock(Ejs *ejs, int nth);
static char *getStringArg(EjsFrame *fp);
static EjsVar *getGlobalArg(Ejs *ejs, EjsFrame *fp);
static bool manageExceptions(Ejs *ejs);
static void checkExceptionHandlers(Ejs *ejs);
static void handleGetters(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj);
static EjsBlock *popExceptionBlock(Ejs *ejs);
static void createExceptionBlock(Ejs *ejs, EjsEx *ex, int flags);
static void storeProperty(Ejs *ejs, EjsVar *obj, EjsName *name, bool dup);
static MPR_INLINE int storePropertyToSlot(Ejs *ejs, EjsObject *obj, int slotNum, EjsVar *value, EjsVar *thisObj);
static void storePropertyToScope(Ejs *ejs, EjsName *qname, bool dup);
static void throwNull(Ejs *ejs);

/*
 *  Virtual Machine byte code evaluation
 */
static void VM(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, int argc, int stackAdjust)
{
    /*
     *  Ordered to assist debugging
     */
    EjsState        *priorState, state;
    EjsName         qname;
    EjsVar          *result, *vp, *v1, *v2;
    EjsObject       *obj;
    int             slotNum, nthBase;

    EjsBlock        *blk;
    EjsVar          *vobj;
    EjsString       *nameVar, *spaceVar;
    EjsType         *type;
    EjsObject       *global;
    EjsLookup       lookup;
    EjsEx           *ex;
    EjsFrame        *newFrame;
    char            *str;
    uchar           *mark;
    int             i, offset, count, opcode;

#if BLD_UNIX_LIKE || VXWORKS
    /*
     *  Direct threading computed goto processing. Include computed goto jump table.
     */
static void *opcodeJump[] = {
    &&EJS_OP_ADD,
    &&EJS_OP_ADD_NAMESPACE,
    &&EJS_OP_ADD_NAMESPACE_REF,
    &&EJS_OP_AND,
    &&EJS_OP_BRANCH_EQ,
    &&EJS_OP_BRANCH_STRICTLY_EQ,
    &&EJS_OP_BRANCH_FALSE,
    &&EJS_OP_BRANCH_GE,
    &&EJS_OP_BRANCH_GT,
    &&EJS_OP_BRANCH_LE,
    &&EJS_OP_BRANCH_LT,
    &&EJS_OP_BRANCH_NE,
    &&EJS_OP_BRANCH_STRICTLY_NE,
    &&EJS_OP_BRANCH_NULL,
    &&EJS_OP_BRANCH_NOT_ZERO,
    &&EJS_OP_BRANCH_TRUE,
    &&EJS_OP_BRANCH_UNDEFINED,
    &&EJS_OP_BRANCH_ZERO,
    &&EJS_OP_BRANCH_FALSE_8,
    &&EJS_OP_BRANCH_TRUE_8,
    &&EJS_OP_BREAKPOINT,
    &&EJS_OP_CALL,
    &&EJS_OP_CALL_GLOBAL_SLOT,
    &&EJS_OP_CALL_OBJ_SLOT,
    &&EJS_OP_CALL_THIS_SLOT,
    &&EJS_OP_CALL_BLOCK_SLOT,
    &&EJS_OP_CALL_OBJ_INSTANCE_SLOT,
    &&EJS_OP_CALL_OBJ_STATIC_SLOT,
    &&EJS_OP_CALL_THIS_STATIC_SLOT,
    &&EJS_OP_CALL_OBJ_NAME,
    &&EJS_OP_CALL_SCOPED_NAME,
    &&EJS_OP_CALL_CONSTRUCTOR,
    &&EJS_OP_CALL_NEXT_CONSTRUCTOR,
    &&EJS_OP_CAST,
    &&EJS_OP_CAST_BOOLEAN,
    &&EJS_OP_CLOSE_BLOCK,
    &&EJS_OP_COMPARE_EQ,
    &&EJS_OP_COMPARE_STRICTLY_EQ,
    &&EJS_OP_COMPARE_FALSE,
    &&EJS_OP_COMPARE_GE,
    &&EJS_OP_COMPARE_GT,
    &&EJS_OP_COMPARE_LE,
    &&EJS_OP_COMPARE_LT,
    &&EJS_OP_COMPARE_NE,
    &&EJS_OP_COMPARE_STRICTLY_NE,
    &&EJS_OP_COMPARE_NULL,
    &&EJS_OP_COMPARE_NOT_ZERO,
    &&EJS_OP_COMPARE_TRUE,
    &&EJS_OP_COMPARE_UNDEFINED,
    &&EJS_OP_COMPARE_ZERO,
    &&EJS_OP_DEBUG,
    &&EJS_OP_DEFINE_CLASS,
    &&EJS_OP_DEFINE_FUNCTION,
    &&EJS_OP_DELETE_NAME_EXPR,
    &&EJS_OP_DELETE_SCOPED_NAME_EXPR,
    &&EJS_OP_DIV,
    &&EJS_OP_DUP,
    &&EJS_OP_DUP2,
    &&EJS_OP_END_CODE,
    &&EJS_OP_END_EXCEPTION,
    &&EJS_OP_GOTO,
    &&EJS_OP_GOTO_8,
    &&EJS_OP_INC,
    &&EJS_OP_INIT_DEFAULT_ARGS,
    &&EJS_OP_INIT_DEFAULT_ARGS_8,
    &&EJS_OP_INST_OF,
    &&EJS_OP_IS_A,
    &&EJS_OP_LOAD_0,
    &&EJS_OP_LOAD_1,
    &&EJS_OP_LOAD_2,
    &&EJS_OP_LOAD_3,
    &&EJS_OP_LOAD_4,
    &&EJS_OP_LOAD_5,
    &&EJS_OP_LOAD_6,
    &&EJS_OP_LOAD_7,
    &&EJS_OP_LOAD_8,
    &&EJS_OP_LOAD_9,
    &&EJS_OP_LOAD_DOUBLE,
    &&EJS_OP_LOAD_FALSE,
    &&EJS_OP_LOAD_GLOBAL,
    &&EJS_OP_LOAD_INT,
    &&EJS_OP_LOAD_M1,
    &&EJS_OP_LOAD_NAMESPACE,
    &&EJS_OP_LOAD_NULL,
    &&EJS_OP_LOAD_REGEXP,
    &&EJS_OP_LOAD_STRING,
    &&EJS_OP_LOAD_THIS,
    &&EJS_OP_LOAD_TRUE,
    &&EJS_OP_LOAD_UNDEFINED,
    &&EJS_OP_LOAD_XML,
    &&EJS_OP_GET_LOCAL_SLOT_0,
    &&EJS_OP_GET_LOCAL_SLOT_1,
    &&EJS_OP_GET_LOCAL_SLOT_2,
    &&EJS_OP_GET_LOCAL_SLOT_3,
    &&EJS_OP_GET_LOCAL_SLOT_4,
    &&EJS_OP_GET_LOCAL_SLOT_5,
    &&EJS_OP_GET_LOCAL_SLOT_6,
    &&EJS_OP_GET_LOCAL_SLOT_7,
    &&EJS_OP_GET_LOCAL_SLOT_8,
    &&EJS_OP_GET_LOCAL_SLOT_9,
    &&EJS_OP_GET_OBJ_SLOT_0,
    &&EJS_OP_GET_OBJ_SLOT_1,
    &&EJS_OP_GET_OBJ_SLOT_2,
    &&EJS_OP_GET_OBJ_SLOT_3,
    &&EJS_OP_GET_OBJ_SLOT_4,
    &&EJS_OP_GET_OBJ_SLOT_5,
    &&EJS_OP_GET_OBJ_SLOT_6,
    &&EJS_OP_GET_OBJ_SLOT_7,
    &&EJS_OP_GET_OBJ_SLOT_8,
    &&EJS_OP_GET_OBJ_SLOT_9,
    &&EJS_OP_GET_THIS_SLOT_0,
    &&EJS_OP_GET_THIS_SLOT_1,
    &&EJS_OP_GET_THIS_SLOT_2,
    &&EJS_OP_GET_THIS_SLOT_3,
    &&EJS_OP_GET_THIS_SLOT_4,
    &&EJS_OP_GET_THIS_SLOT_5,
    &&EJS_OP_GET_THIS_SLOT_6,
    &&EJS_OP_GET_THIS_SLOT_7,
    &&EJS_OP_GET_THIS_SLOT_8,
    &&EJS_OP_GET_THIS_SLOT_9,
    &&EJS_OP_GET_SCOPED_NAME,
    &&EJS_OP_GET_SCOPED_NAME_EXPR,
    &&EJS_OP_GET_OBJ_NAME,
    &&EJS_OP_GET_OBJ_NAME_EXPR,
    &&EJS_OP_GET_BLOCK_SLOT,
    &&EJS_OP_GET_GLOBAL_SLOT,
    &&EJS_OP_GET_LOCAL_SLOT,
    &&EJS_OP_GET_OBJ_SLOT,
    &&EJS_OP_GET_THIS_SLOT,
    &&EJS_OP_GET_TYPE_SLOT,
    &&EJS_OP_GET_THIS_TYPE_SLOT,
    &&EJS_OP_IN,
    &&EJS_OP_LIKE,
    &&EJS_OP_LOGICAL_NOT,
    &&EJS_OP_MUL,
    &&EJS_OP_NEG,
    &&EJS_OP_NEW,
    &&EJS_OP_NEW_OBJECT,
    &&EJS_OP_NOP,
    &&EJS_OP_NOT,
    &&EJS_OP_OPEN_BLOCK,
    &&EJS_OP_OPEN_WITH,
    &&EJS_OP_OR,
    &&EJS_OP_POP,
    &&EJS_OP_POP_ITEMS,
    &&EJS_OP_PUSH_CATCH_ARG,
    &&EJS_OP_PUSH_RESULT,
    &&EJS_OP_PUT_LOCAL_SLOT_0,
    &&EJS_OP_PUT_LOCAL_SLOT_1,
    &&EJS_OP_PUT_LOCAL_SLOT_2,
    &&EJS_OP_PUT_LOCAL_SLOT_3,
    &&EJS_OP_PUT_LOCAL_SLOT_4,
    &&EJS_OP_PUT_LOCAL_SLOT_5,
    &&EJS_OP_PUT_LOCAL_SLOT_6,
    &&EJS_OP_PUT_LOCAL_SLOT_7,
    &&EJS_OP_PUT_LOCAL_SLOT_8,
    &&EJS_OP_PUT_LOCAL_SLOT_9,
    &&EJS_OP_PUT_OBJ_SLOT_0,
    &&EJS_OP_PUT_OBJ_SLOT_1,
    &&EJS_OP_PUT_OBJ_SLOT_2,
    &&EJS_OP_PUT_OBJ_SLOT_3,
    &&EJS_OP_PUT_OBJ_SLOT_4,
    &&EJS_OP_PUT_OBJ_SLOT_5,
    &&EJS_OP_PUT_OBJ_SLOT_6,
    &&EJS_OP_PUT_OBJ_SLOT_7,
    &&EJS_OP_PUT_OBJ_SLOT_8,
    &&EJS_OP_PUT_OBJ_SLOT_9,
    &&EJS_OP_PUT_THIS_SLOT_0,
    &&EJS_OP_PUT_THIS_SLOT_1,
    &&EJS_OP_PUT_THIS_SLOT_2,
    &&EJS_OP_PUT_THIS_SLOT_3,
    &&EJS_OP_PUT_THIS_SLOT_4,
    &&EJS_OP_PUT_THIS_SLOT_5,
    &&EJS_OP_PUT_THIS_SLOT_6,
    &&EJS_OP_PUT_THIS_SLOT_7,
    &&EJS_OP_PUT_THIS_SLOT_8,
    &&EJS_OP_PUT_THIS_SLOT_9,
    &&EJS_OP_PUT_OBJ_NAME_EXPR,
    &&EJS_OP_PUT_OBJ_NAME,
    &&EJS_OP_PUT_SCOPED_NAME,
    &&EJS_OP_PUT_SCOPED_NAME_EXPR,
    &&EJS_OP_PUT_BLOCK_SLOT,
    &&EJS_OP_PUT_GLOBAL_SLOT,
    &&EJS_OP_PUT_LOCAL_SLOT,
    &&EJS_OP_PUT_OBJ_SLOT,
    &&EJS_OP_PUT_THIS_SLOT,
    &&EJS_OP_PUT_TYPE_SLOT,
    &&EJS_OP_PUT_THIS_TYPE_SLOT,
    &&EJS_OP_REM,
    &&EJS_OP_RETURN,
    &&EJS_OP_RETURN_VALUE,
    &&EJS_OP_SAVE_RESULT,
    &&EJS_OP_SHL,
    &&EJS_OP_SHR,
    &&EJS_OP_SUB,
    &&EJS_OP_SUPER,
    &&EJS_OP_SWAP,
    &&EJS_OP_THROW,
    &&EJS_OP_TYPE_OF,
    &&EJS_OP_USHR,
    &&EJS_OP_XOR,
    &&EJS_OP_FINALLY,
};
#endif
    mprAssert(ejs);
    mprAssert(!mprHasAllocError(ejs));

    vp = 0;
    slotNum = -1;
    global = (EjsObject*) ejs->global;

    priorState = ejs->state;
    state = *ejs->state;
    ejs->state = &state;

    callFunction(ejs, fun, thisObj, argc, stackAdjust);
    FRAME->caller = 0;
    FRAME->currentLine = 0;
    FRAME->filename = 0;
    FRAME->lineNumber = 0;

#if BLD_UNIX_LIKE || VXWORKS
    /*
     *  Direct threading computed goto processing. Include computed goto jump table.
     */
    CHECK; 
    BREAK;
#else
    /*
     *  Traditional switch for compilers (looking at you MS) without computed goto.
     */
    while (1) {
        opcode = (EjsOpCode) GET_BYTE();
        traceCode(ejs, opcode);
        switch (opcode) {
#endif
        /*
         *  Symbolic source code debug information
         *      Debug <filename> <lineNumber> <sourceLine>
         */
        CASE (EJS_OP_DEBUG):
            TRACE; BREAK;

        /*
         *  End of a code block. Used to mark the end of a script. Saves testing end of code block in VM loop.
         *      EndCode
         */
        CASE (EJS_OP_END_CODE):
            /*
             *  The "ejs" command needs to preserve the current ejs->result for interactive sessions.
             */
            if (ejs->result == 0) {
                // OPT - remove this
                ejs->result = ejs->undefinedValue;
            }
            if (FRAME->function.getter) {
                push(ejs->result);
            }
            FRAME = 0;
            goto done;

        /*
         *  Return from a function with a result
         *      ReturnValue
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_RETURN_VALUE):
            ejs->result = pop(ejs);
            mprAssert(ejs->exception || ejs->result);
            if (FRAME->caller == 0) {
                goto done;
            }
            state.stack = FRAME->stackReturn;
            if (FRAME->function.getter) {
                push(ejs->result);
            }
            state.bp = FRAME->function.block.prev;
            newFrame = FRAME->caller;
            FRAME = newFrame;
            BREAK;

        /*
         *  Return from a function without a result
         *      Return
         */
        CASE (EJS_OP_RETURN):
            ejs->result = ejs->undefinedValue;
            if (FRAME->caller == 0) {
                goto done;
            }
            state.stack = FRAME->stackReturn;
            state.bp = FRAME->function.block.prev;
            newFrame = FRAME->caller;
            FRAME = newFrame;
            BREAK;

        /*
         *  Load the catch argument
         *      PushCatchArg
         *      Stack before (top)  []
         *      Stack after         [catchArg]
         */
        CASE (EJS_OP_PUSH_CATCH_ARG):
            push(ejs->exceptionArg);
            ejs->exceptionArg = 0;
            BREAK;

        /*
         *  Push the function call result
         *      PushResult
         *      Stack before (top)  []
         *      Stack after         [result]
         */
        CASE (EJS_OP_PUSH_RESULT):
            push(ejs->result);
            BREAK;

        /*
         *  Save the top of stack and store in the interpreter result register
         *      SaveResult
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_SAVE_RESULT):
            ejs->result = pop(ejs);
            mprAssert(ejs->exception || ejs->result);
            BREAK;

        /*
         *  Load Constants -----------------------------------------------
         */

        /*
         *  Load a float constant
         *      LoadDouble          <double>
         *      Stack before (top)  []
         *      Stack after         [Double]
         */
        CASE (EJS_OP_LOAD_DOUBLE):
#if BLD_FEATURE_FLOATING_POINT
            push(ejsCreateNumber(ejs, GET_DOUBLE()));
#else
            ejsThrowReferenceError(ejs, "No floating point support");
#endif
            CHECK; BREAK;

        /*
         *  Load a signed integer constant (up to 55 bits worth of data)
         *      LoadInt.64          <int64>
         *      Stack before (top)  []
         *      Stack after         [Number]
         */
        CASE (EJS_OP_LOAD_INT):
            push(ejsCreateNumber(ejs, (MprNumber) GET_NUM()));
            CHECK; BREAK;

        /*
         *  Load integer constant between 0 and 9
         *      Load0, Load1, ... Load9
         *      Stack before (top)  []
         *      Stack after         [Number]
         */
        CASE (EJS_OP_LOAD_0):
        CASE (EJS_OP_LOAD_1):
        CASE (EJS_OP_LOAD_2):
        CASE (EJS_OP_LOAD_3):
        CASE (EJS_OP_LOAD_4):
        CASE (EJS_OP_LOAD_5):
        CASE (EJS_OP_LOAD_6):
        CASE (EJS_OP_LOAD_7):
        CASE (EJS_OP_LOAD_8):
        CASE (EJS_OP_LOAD_9):
            push(ejsCreateNumber(ejs, opcode - EJS_OP_LOAD_0));
            CHECK; BREAK;

        /*
         *  Load the -1 integer constant
         *      LoadMinusOne
         *      Stack before (top)  []
         *      Stack after         [Number]
         */
        CASE (EJS_OP_LOAD_M1):
            push(ejsCreateNumber(ejs, -1));
            BREAK;

        /*
         *  Load a string constant
         *      LoadString          <string>
         *      Stack before (top)  []
         *      Stack after         [String]
         */
        CASE (EJS_OP_LOAD_STRING):
            str = GET_STRING();
            push(ejsCreateString(ejs, str));
            CHECK; BREAK;

        /*
         *  Load a namespace constant
         *      LoadNamespace       <UriString>
         *      Stack before (top)  []
         *      Stack after         [Namespace]
         */
        CASE (EJS_OP_LOAD_NAMESPACE):
            str = GET_STRING();
            push(ejsCreateNamespace(ejs, str, str));
            CHECK; BREAK;


        /*
         *  Load an XML constant
         *      LoadXML             <xmlString>
         *      Stack before (top)  []
         *      Stack after         [XML]
         */
        CASE (EJS_OP_LOAD_XML):
#if BLD_FEATURE_EJS_E4X
            v1 = (EjsVar*) ejsCreateXML(ejs, 0, 0, 0, 0);
            str = GET_STRING();
            ejsLoadXMLString(ejs, (EjsXML*) v1, str);
            push(v1);
#endif
            CHECK; BREAK;

        /*
         *  Load a Regexp constant
         *      LoadRegExp
         *      Stack before (top)  []
         *      Stack after         [RegExp]
         */
        CASE (EJS_OP_LOAD_REGEXP):
            str = GET_STRING();
#if BLD_FEATURE_REGEXP
            v1 = (EjsVar*) ejsCreateRegExp(ejs, str);
            push(v1);
#else
            ejsThrowReferenceError(ejs, "No regular expression support");
#endif
            CHECK; BREAK;

        /*
         *  Load a null constant
         *      LoadNull
         *      Stack before (top)  []
         *      Stack after         [Null]
         */
        CASE (EJS_OP_LOAD_NULL):
            push(ejs->nullValue);
            BREAK;

        /*
         *  Load a void / undefined constant
         *      LoadUndefined
         *      Stack before (top)  []
         *      Stack after         [undefined]
         */
        CASE (EJS_OP_LOAD_UNDEFINED):
            push(ejs->undefinedValue);
            BREAK;

        /*
         *  Load the "this" value
         *      LoadThis
         *      Stack before (top)  []
         *      Stack after         [this]
         */
        CASE (EJS_OP_LOAD_THIS):
            push(THIS);
            BREAK;

        /*
         *  Load the "global" value
         *      LoadGlobal
         *      Stack before (top)  []
         *      Stack after         [global]
         */
        CASE (EJS_OP_LOAD_GLOBAL):
            push(ejs->global);
            BREAK;

        /*
         *  Load the "true" value
         *      LoadTrue
         *      Stack before (top)  []
         *      Stack after         [true]
         */
        CASE (EJS_OP_LOAD_TRUE):
            push(ejs->trueValue);
            BREAK;

        /*
         *  Load the "false" value
         *      LoadFalse
         *      Stack before (top)  []
         *      Stack after         [false]
         */
        CASE (EJS_OP_LOAD_FALSE):
            push(ejs->falseValue);
            BREAK;

        /*
         *  Load a global variable by slot number
         *      GetGlobalSlot       <slot>
         *      Stack before (top)  []
         *      Stack after         [PropRef]
         */
        CASE (EJS_OP_GET_GLOBAL_SLOT):
            push(GET_SLOT(global, GET_INT()));
            DO_GETTER(NULL); 
            BREAK;

        /*
         *  Load a local variable by slot number
         *      GetLocalSlot        <slot>
         *      Stack before (top)  []
         *      Stack after         [PropRef]
         */
        CASE (EJS_OP_GET_LOCAL_SLOT):
            push(GET_SLOT(FRAME, GET_INT()));
            DO_GETTER(NULL); 
            BREAK;

        /*
         *  Load a local variable in slot 0-9
         *      GetLocalSlot0, GetLocalSlot1, ... GetLocalSlot9
         *      Stack before (top)  []
         *      Stack after         [PropRef]
         */
        CASE (EJS_OP_GET_LOCAL_SLOT_0):
        CASE (EJS_OP_GET_LOCAL_SLOT_1):
        CASE (EJS_OP_GET_LOCAL_SLOT_2):
        CASE (EJS_OP_GET_LOCAL_SLOT_3):
        CASE (EJS_OP_GET_LOCAL_SLOT_4):
        CASE (EJS_OP_GET_LOCAL_SLOT_5):
        CASE (EJS_OP_GET_LOCAL_SLOT_6):
        CASE (EJS_OP_GET_LOCAL_SLOT_7):
        CASE (EJS_OP_GET_LOCAL_SLOT_8):
        CASE (EJS_OP_GET_LOCAL_SLOT_9):
            push(GET_SLOT(FRAME, opcode - EJS_OP_GET_LOCAL_SLOT_0));
            DO_GETTER(NULL); 
            BREAK;

        /*
         *  Load a block scoped variable by slot number
         *      GetBlockSlot        <slot> <nthBlock>
         *      Stack before (top)  []
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_BLOCK_SLOT):
            slotNum = GET_INT();
            obj = (EjsObject*) getNthBlock(ejs, GET_INT());
            push(GET_SLOT(obj, slotNum));
            DO_GETTER(NULL); 
            BREAK;

        /*
         *  Load a property in thisObj by slot number
         *      GetThisSlot         <slot>
         *      Stack before (top)  []
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_THIS_SLOT):
            push(GET_SLOT(THIS, GET_INT()));
            DO_GETTER(THIS); 
            BREAK;

        /*
         *  Load a property in slot 0-9
         *      GetThisSlot0, GetThisSlot1,  ... GetThisSlot9
         *      Stack before (top)  []
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_THIS_SLOT_0):
        CASE (EJS_OP_GET_THIS_SLOT_1):
        CASE (EJS_OP_GET_THIS_SLOT_2):
        CASE (EJS_OP_GET_THIS_SLOT_3):
        CASE (EJS_OP_GET_THIS_SLOT_4):
        CASE (EJS_OP_GET_THIS_SLOT_5):
        CASE (EJS_OP_GET_THIS_SLOT_6):
        CASE (EJS_OP_GET_THIS_SLOT_7):
        CASE (EJS_OP_GET_THIS_SLOT_8):
        CASE (EJS_OP_GET_THIS_SLOT_9):
            push(GET_SLOT(THIS, opcode - EJS_OP_GET_THIS_SLOT_0));
            DO_GETTER(THIS); 
            BREAK;

        /*
         *  Load a property in an object by slot number
         *      GetObjSlot          <slot>
         *      Stack before (top)  [obj]
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_OBJ_SLOT):
            vp = pop(ejs);
            push(GET_SLOT(vp, GET_INT()));
            DO_GETTER(vp); 
            BREAK;

        /*
         *  Load a property in an object from slot 0-9
         *      GetObjSlot0, GetObjSlot1, ... GetObjSlot9
         *      Stack before (top)  [obj]
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_OBJ_SLOT_0):
        CASE (EJS_OP_GET_OBJ_SLOT_1):
        CASE (EJS_OP_GET_OBJ_SLOT_2):
        CASE (EJS_OP_GET_OBJ_SLOT_3):
        CASE (EJS_OP_GET_OBJ_SLOT_4):
        CASE (EJS_OP_GET_OBJ_SLOT_5):
        CASE (EJS_OP_GET_OBJ_SLOT_6):
        CASE (EJS_OP_GET_OBJ_SLOT_7):
        CASE (EJS_OP_GET_OBJ_SLOT_8):
        CASE (EJS_OP_GET_OBJ_SLOT_9):
            vp = pop(ejs);
            push(GET_SLOT(vp, opcode - EJS_OP_GET_OBJ_SLOT_0));
            DO_GETTER(vp); 
            BREAK;

        /*
         *  Load a variable from a type by slot number
         *      GetTypeSlot         <slot> <nthBase>
         *      Stack before (top)  [objRef]
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_TYPE_SLOT):
            slotNum = GET_INT();
            obj = (EjsObject*) pop(ejs);
            vp = getNthBase(ejs, (EjsVar*) obj, GET_INT());
            push(GET_SLOT(vp, slotNum));
            DO_GETTER(obj); 
            BREAK;

        /*
         *  Load a type variable by slot number from this. NthBase counts from Object up rather than "this" down.
         *      GetThisTypeSlot     <slot> <nthBaseFromBottom>
         *      Stack before (top)  []
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_THIS_TYPE_SLOT):
            slotNum = GET_INT();
            type = (EjsType*) getNthBaseFromBottom(ejs, THIS, GET_INT());
            if (type == 0) {
                ejsThrowReferenceError(ejs, "Bad base class reference");
            } else {
                push(GET_SLOT(type, slotNum));
                DO_GETTER(THIS);
            }
            BREAK;

        /*
         *  Load a variable by an unqualified name
         *      GetScopedName       <qname>
         *      Stack before (top)  []
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_SCOPED_NAME):
            mark = FRAME->pc - 1;
            qname = GET_NAME();
            vp = ejsGetVarByName(ejs, NULL, &qname, &lookup);
            if (unlikely(vp == 0)) {
                if (ejs->flags & EJS_FLAG_COMPILER) {
                    push(ejs->undefinedValue);
                } else {
                    ejsThrowReferenceError(ejs, "%s is not defined", qname.name);
                }
                CHECK;
            } else {
                push(vp);
                DO_GETTER(NULL);
            }
#if DYNAMIC_BINDING
            //  OPT FUNCTIONALIZE inline
            if (ejs->flags & EJS_FLAG_COMPILER || lookup.obj->type == ejs->objectType || lookup.slotNum >= 4096) {
                BREAK;
            }
            if (lookup.obj == ejs->global) {
                *mark++ = EJS_OP_GET_GLOBAL_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else if (lookup->obj == state->fp) {
                *mark++ = EJS_OP_GET_LOCAL_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else if (lookup->obj == state->fp->thisObj) {
                *mark++ = EJS_OP_GET_THIS_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else if (ejsIsA(ejs, THIS, (EjsType*) lookup.obj)) {
                *mark++ = EJS_OP_GET_BLOCK_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);
                mark += ejsEncodeUint(mark, lookup.nthBlock);

            } else {
                BREAK;
            }
            FILL(mark);
#endif
            BREAK;
                
        /*
         *  Load a variable by an unqualified name expression
         *      GetScopedNameExpr
         *      Stack before (top)  [name]
         *                          [space]
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_SCOPED_NAME_EXPR):
            mark = FRAME->pc - 1;
            qname.name = ejsToString(ejs, pop(ejs))->value;
            v1 = pop(ejs);
            if (ejsIsNamespace(v1)) {
                qname.space = ((EjsNamespace*) v1)->uri;
            } else {
                qname.space = ejsToString(ejs, v1)->value;
            }
            vp = ejsGetVarByName(ejs, NULL, &qname, &lookup);
            if (unlikely(vp == 0)) {
                if (ejs->flags & EJS_FLAG_COMPILER) {
                    push(ejs->undefinedValue);
                } else {
                    ejsThrowReferenceError(ejs, "%s is not defined", qname.name);
                }
                CHECK;
            } else {
                push(vp);
                DO_GETTER(NULL);
            }
#if DYNAMIC_BINDING
            if (ejs->flags & EJS_FLAG_COMPILER || lookup.obj->type == ejs->objectType || lookup.slotNum >= 4096) {
                BREAK;
            }
            if (lookup.obj == ejs->global) {
                *mark++ = EJS_OP_GET_GLOBAL_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else if (lookup->obj == state->fp) {
                *mark++ = EJS_OP_GET_LOCAL_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else if (lookup->obj == state->fp->thisObj) {
                *mark++ = EJS_OP_GET_THIS_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else if (ejsIsA(ejs, THIS, (EjsType*) lookup.obj)) {
                *mark++ = EJS_OP_GET_BLOCK_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);
                mark += ejsEncodeUint(mark, lookup.nthBlock);

            } else {
                BREAK;
            }
            FILL(mark);
#endif
            BREAK;
                
        /*
         *  Load a property by property name
         *      GetObjName          <qname>
         *      Stack before (top)  [obj]
         *      Stack after         [result]
         */
        CASE (EJS_OP_GET_OBJ_NAME):
#if DYNAMIC_BINDING
            mark = FRAME->pc - 1;
#endif
            qname = GET_NAME();
            vp = pop(ejs);
            if (vp == 0 || vp == ejs->nullValue || vp == ejs->undefinedValue) {
                ejsThrowReferenceError(ejs, "Object reference is null");
                CHECK; BREAK;
            }
            v1 = ejsGetVarByName(ejs, vp, &qname, &lookup);
            push(v1 ? v1 : ejs->undefinedValue);
#if DYNAMIC_BINDING
            if (lookup.slotNum < 0 || lookup.slotNum > 4096 || ejs->flags & EJS_FLAG_COMPILER) {
                BREAK;
            }
            if (lookup.obj == ejs->global) {
                *mark++ = EJS_OP_GET_GLOBAL_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else if (lookup.obj == (EjsVar*) state.fp) {
                *mark++ = EJS_OP_GET_LOCAL_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else if (lookup.obj == state.fp->thisObj) {
                *mark++ = EJS_OP_GET_THIS_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);
                
            } else if (ejsIsType(lookup.obj) && ejsIsA(ejs, THIS, (EjsType*) lookup.obj)) {
                *mark++ = EJS_OP_GET_TYPE_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);
                mark += ejsEncodeUint(mark, lookup.nthBlock + 1);

            } else if ((EjsVar*) vp->type == lookup.obj) {
                *mark++  = EJS_OP_GET_TYPE_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);
                mark += ejsEncodeUint(mark, lookup.nthBase);

            } else if (ejsIsObject(lookup.obj)) {
                mprAssert(lookup.obj == vp);
                *mark++  = EJS_OP_GET_OBJ_SLOT;
                mark += ejsEncodeUint(mark, lookup.slotNum);

            } else {
                BREAK;
            }
            FILL(mark);
#endif
            DO_GETTER(vp);
            BREAK;

        /*
         *  Load a property by property a qualified name expression
         *      GetObjNameExpr
         *      Stack before (top)  [name]
         *                          [space]
         *                          [obj]
         *      Stack after         [value]
         */
        CASE (EJS_OP_GET_OBJ_NAME_EXPR):
            v1 = pop(ejs);
            v2 = pop(ejs);
            vp = pop(ejs);
            if (vp == 0 || vp == ejs->nullValue || vp == ejs->undefinedValue) {
                ejsThrowReferenceError(ejs, "Object reference is null");
                CHECK; BREAK;
            }
            if (vp->type->numericIndicies && ejsIsNumber(v1)) {
                vp = ejsGetProperty(ejs, vp, ejsGetInt(v1));
                push(vp == 0 ? ejs->nullValue : vp);
                CHECK; BREAK;
            } else {
                qname.name = ejsToString(ejs, v1)->value;
                if (ejsIsNamespace(v2)) {
                    qname.space = ((EjsNamespace*) v2)->uri;
                } else {
                    qname.space = ejsToString(ejs, v2)->value;
                }
                v2 = ejsGetVarByName(ejs, vp, &qname, &lookup);
                push(v2 ? v2 : ejs->undefinedValue);
                DO_GETTER(vp);
                CHECK; BREAK;
            }

        /*
         *  Store -------------------------------
         */

        /*
         *  Store a global variable by slot number
         *      Stack before (top)  [value]
         *      Stack after         []
         *      PutGlobalSlot       <slot>
         */
        CASE (EJS_OP_PUT_GLOBAL_SLOT):
            SET_SLOT(global, GET_INT(), pop(ejs), NULL);
            BREAK;

        /*
         *  Store a local variable by slot number
         *      Stack before (top)  [value]
         *      Stack after         []
         *      PutLocalSlot        <slot>
         */
        CASE (EJS_OP_PUT_LOCAL_SLOT):
            SET_SLOT(FRAME, GET_INT(), pop(ejs), NULL);
            BREAK;

        /*
         *  Store a local variable from slot 0-9
         *      PutLocalSlot0, PutLocalSlot1, ... PutLocalSlot9
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_LOCAL_SLOT_0):
        CASE (EJS_OP_PUT_LOCAL_SLOT_1):
        CASE (EJS_OP_PUT_LOCAL_SLOT_2):
        CASE (EJS_OP_PUT_LOCAL_SLOT_3):
        CASE (EJS_OP_PUT_LOCAL_SLOT_4):
        CASE (EJS_OP_PUT_LOCAL_SLOT_5):
        CASE (EJS_OP_PUT_LOCAL_SLOT_6):
        CASE (EJS_OP_PUT_LOCAL_SLOT_7):
        CASE (EJS_OP_PUT_LOCAL_SLOT_8):
        CASE (EJS_OP_PUT_LOCAL_SLOT_9):
            SET_SLOT(FRAME, opcode - EJS_OP_PUT_LOCAL_SLOT_0, pop(ejs), NULL);
            BREAK;

        /*
         *  Store a block variable by slot number
         *      PutBlockSlot        <slot> <nthBlock>
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_BLOCK_SLOT):
            slotNum = GET_INT();
            obj = (EjsObject*) getNthBlock(ejs, GET_INT());
            SET_SLOT(obj, slotNum, pop(ejs), NULL);
            BREAK;

        /*
         *  Store a property by slot number
         *      PutThisSlot         <slot>
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_THIS_SLOT):
            slotNum = GET_INT();
            SET_SLOT(THIS, slotNum, pop(ejs), THIS);
            BREAK;

        /*
         *  Store a property to slot 0-9
         *      PutThisSlot0, PutThisSlot1, ... PutThisSlot9,
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_THIS_SLOT_0):
        CASE (EJS_OP_PUT_THIS_SLOT_1):
        CASE (EJS_OP_PUT_THIS_SLOT_2):
        CASE (EJS_OP_PUT_THIS_SLOT_3):
        CASE (EJS_OP_PUT_THIS_SLOT_4):
        CASE (EJS_OP_PUT_THIS_SLOT_5):
        CASE (EJS_OP_PUT_THIS_SLOT_6):
        CASE (EJS_OP_PUT_THIS_SLOT_7):
        CASE (EJS_OP_PUT_THIS_SLOT_8):
        CASE (EJS_OP_PUT_THIS_SLOT_9):
            SET_SLOT(THIS, opcode - EJS_OP_PUT_THIS_SLOT_0, pop(ejs), THIS);
            BREAK;

        /* 
         *  Store a property by slot number
         *      PutObjSlot          <slot>
         *      Stack before (top)  [obj]
         *                          [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_OBJ_SLOT):
            vp = pop(ejs);
            SET_SLOT(vp, GET_INT(), pop(ejs), NULL);
            BREAK;

        /*
         *  Store a property to slot 0-9
         *      PutObjSlot0, PutObjSlot1, ... PutObjSlot9
         *      Stack before (top)  [obj]
         *                          [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_OBJ_SLOT_0):
        CASE (EJS_OP_PUT_OBJ_SLOT_1):
        CASE (EJS_OP_PUT_OBJ_SLOT_2):
        CASE (EJS_OP_PUT_OBJ_SLOT_3):
        CASE (EJS_OP_PUT_OBJ_SLOT_4):
        CASE (EJS_OP_PUT_OBJ_SLOT_5):
        CASE (EJS_OP_PUT_OBJ_SLOT_6):
        CASE (EJS_OP_PUT_OBJ_SLOT_7):
        CASE (EJS_OP_PUT_OBJ_SLOT_8):
        CASE (EJS_OP_PUT_OBJ_SLOT_9):
            vp = pop(ejs);
            SET_SLOT(vp, opcode - EJS_OP_PUT_OBJ_SLOT_0, pop(ejs), NULL);
            BREAK;

        /*
         *  Store a variable by an unqualified name
         *      PutScopedName       <qname>
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_SCOPED_NAME):
            qname = GET_NAME();
            storePropertyToScope(ejs, &qname, 0);
            CHECK; BREAK;

        /*
         *  Store a variable by an unqualified name expression
         *      PutScopedName 
         *      Stack before (top)  [name]
         *                          [space]
         *                          [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_SCOPED_NAME_EXPR):
            qname.name = ejsToString(ejs, pop(ejs))->value;
            v1 = pop(ejs);
            if (ejsIsNamespace(v1)) {
                qname.space = ((EjsNamespace*) v1)->uri;
            } else {
                qname.space = ejsToString(ejs, v1)->value;
            }
            storePropertyToScope(ejs, &qname, 1);
            CHECK; BREAK;

        /*
         *  Store a property by property name to an object
         *      PutObjName
         *      Stack before (top)  [value]
         *                          [objRef]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_OBJ_NAME):
            qname = GET_NAME();
            vp = pop(ejs);
            storeProperty(ejs, vp, &qname, 0);
            CHECK; BREAK;

        /*
         *  Store a property by a qualified property name expression to an object
         *      PutObjNameExpr
         *      Stack before (top)  [nameExpr]
         *                          [spaceExpr]
         *                          [objRef]
         *                          [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_OBJ_NAME_EXPR):
            v1 = pop(ejs);
            v2 = pop(ejs);
            vp = pop(ejs);
            if (vp->type->numericIndicies && ejsIsNumber(v1)) {
                ejsSetProperty(ejs, vp, ejsGetInt(v1), pop(ejs));
            } else {
                qname.name = ejsToString(ejs, v1)->value;
                if (ejsIsNamespace(v2)) {
                    qname.space = ((EjsNamespace*) v2)->uri;
                } else {
                    qname.space = ejsToString(ejs, v2)->value;
                }
                if (qname.name && qname.space) {
                    storeProperty(ejs, vp, &qname, 1);
                }
            }
            CHECK; BREAK;

        /*
         *  Store a type variable by slot number
         *      PutTypeSlot         <slot> <nthBase>
         *      Stack before (top)  [obj]
         *                          [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_TYPE_SLOT):
            slotNum = GET_INT();
            vobj = pop(ejs);
            vp = getNthBase(ejs, vobj, GET_INT());
            SET_SLOT(vp, slotNum, pop(ejs), vobj);
            BREAK;

        /*
         *  Store a variable to a slot in the nthBase class of the current "this" object
         *      PutThisTypeSlot     <slot> <nthBase>
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_PUT_THIS_TYPE_SLOT):
            slotNum = GET_INT();
            type = (EjsType*) getNthBaseFromBottom(ejs, THIS, GET_INT());
            if (type == 0) {
                ejsThrowReferenceError(ejs, "Bad base class reference");
            } else {
                SET_SLOT(type, slotNum, pop(ejs), THIS);
            }
            CHECK; BREAK;


        /*
         *  Function calling and return
         */

        /*
         *  Call a function by reference
         *      Stack before (top)  [args]
         *                          [function]
         *                          [thisObj]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL):
            argc = GET_INT();
            vp = state.stack[-argc - 1];
            fun = (EjsFunction*) state.stack[-argc];
            callProperty(ejs, fun, vp, argc, 2);
            CHECK; BREAK;

        /*
         *  Call a global function by slot on the given type
         *      CallGlobalSlot      <slot> <argc>
         *      Stack before (top)  [args]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_GLOBAL_SLOT):
            slotNum = GET_INT();
            argc = GET_INT();
            callProperty(ejs, (EjsFunction*) global->slots[slotNum], NULL, argc, 0);
            CHECK; BREAK;

        /*
         *  Call a function by slot number on the pushed object
         *      CallObjSlot         <slot> <argc>
         *      Stack before (top)  [args]
         *                          [obj]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_OBJ_SLOT):
            slotNum = GET_INT();
            argc = GET_INT();
            vp = state.stack[-argc];
            if (vp == 0 || vp == ejs->nullValue || vp == ejs->undefinedValue) {
                if (vp && (slotNum == ES_Object_get || slotNum == ES_Object_getValues)) {
                    callProperty(ejs, (EjsFunction*) vp->type->block.obj.slots[slotNum], vp, argc, 1);
                } else {
                    ejsThrowReferenceError(ejs, "Object reference is null or undefined");
                }
            } else {
                callProperty(ejs, (EjsFunction*) vp->type->block.obj.slots[slotNum], vp, argc, 1);
            }
            CHECK; BREAK;

        /*
         *  Call a function by slot number on the current this object.
         *      CallThisSlot        <slot> <argc>
         *      Stack before (top)  [args]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_THIS_SLOT):
            slotNum = GET_INT();
            argc = GET_INT();
            obj = (EjsObject*) THIS->type;
            if (ejsIsObject(obj) && slotNum < obj->numProp) {
                callProperty(ejs, (EjsFunction*) obj->slots[slotNum], NULL, argc, 0);
            } else {
                ejsThrowTypeError(ejs, "Property is not a function");
            }
            CHECK; BREAK;

        /*
         *  Call a function by slot number on the nth enclosing block
         *      CallBlockSlot        <slot> <nthBlock> <argc>
         *      Stack before (top)  [args]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_BLOCK_SLOT):
            slotNum = GET_INT();
            obj = (EjsObject*) getNthBlock(ejs, GET_INT());
            argc = GET_INT();
            callProperty(ejs, (EjsFunction*) obj->slots[slotNum], NULL, argc, 0);
            CHECK; BREAK;

        /*
         *  Call a function by slot number on an object.
         *      CallObjInstanceSlot <slot> <argc>
         *      Stack before (top)  [args]
         *                          [obj]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_OBJ_INSTANCE_SLOT):
            slotNum = GET_INT();
            argc = GET_INT();
            vp = state.stack[-argc];
            if (vp == 0 || vp == ejs->nullValue) {
                ejsThrowReferenceError(ejs, "Object reference is null");
            } else {
                fun = (EjsFunction*) ejsGetProperty(ejs, vp, slotNum);
                callProperty(ejs, fun, vp, argc, 1);
            }
            CHECK; BREAK;

        /*
         *  Call a static function by slot number on the pushed object
         *      CallObjStaticSlot   <slot> <nthBase> <argc>
         *      Stack before (top)  [args]
         *                          [obj]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_OBJ_STATIC_SLOT):
            slotNum = GET_INT();
            nthBase = GET_INT();
            argc = GET_INT();
            vp = state.stack[-argc];
            if (vp == ejs->nullValue) {
                throwNull(ejs);
            } else {
                type = (EjsType*) getNthBase(ejs, vp, nthBase);
                callProperty(ejs, (EjsFunction*) type->block.obj.slots[slotNum], (EjsVar*) type, argc, 1);
            }
            CHECK; BREAK;

        /*
         *  Call a static function by slot number on the nth base class of the current "this" object
         *      CallThisStaticSlot  <slot> <nthBase> <argc>
         *      Stack before (top)  [args]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_THIS_STATIC_SLOT):
            slotNum = GET_INT();
            nthBase = GET_INT();
            argc = GET_INT();
            type = (EjsType*) getNthBase(ejs, THIS, nthBase);
            if (type == ejs->objectType) {
                ejsThrowReferenceError(ejs, "Bad type reference");
                CHECK; BREAK;
            }
            callProperty(ejs, (EjsFunction*) type->block.obj.slots[slotNum], (EjsVar*) type, argc, 0);
            CHECK; BREAK;

        /*
         *  Call a function by name on the pushed object
         *      CallObjName         <qname> <argc>
         *      Stack before (top)  [args]
         *                          [obj]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_OBJ_NAME):
            qname = GET_NAME();
            argc = GET_INT();
            vp = state.stack[-argc];
            if (vp == 0) {
                ejsThrowReferenceError(ejs, "%s is not defined", qname.name);
                throwNull(ejs);
                CHECK; BREAK;
            }
            slotNum = ejsLookupVar(ejs, (EjsVar*) vp, &qname, &lookup);
            if (slotNum < 0) {
                ejsThrowReferenceError(ejs, "Can't find function \"%s\"", qname.name);
            } else {
                fun = (EjsFunction*) ejsGetProperty(ejs, lookup.obj, slotNum);
                if (fun->getter) {
                    fun = (EjsFunction*) ejsRunFunction(ejs, fun, vp, 0, NULL);
                }
                callProperty(ejs, fun, vp, argc, 1);
            }
            CHECK; BREAK;

        /*
         *  Call a function by name in the current scope. Use existing "this" object if defined.
         *      CallName            <qname> <argc>
         *      Stack before (top)  [args]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_SCOPED_NAME):
            qname = GET_NAME();
            argc = GET_INT();
            lookup.storing = 0;
            slotNum = ejsLookupScope(ejs, &qname, &lookup);
            if (slotNum < 0) {
                ejsThrowReferenceError(ejs, "Can't find method %s", qname.name);
                CHECK; BREAK;
            }
            fun = (EjsFunction*) ejsGetProperty(ejs, lookup.obj, slotNum);
            if (!ejsIsFunction(fun)) {
                callConstructor(ejs, fun, argc, 0);
            } else {
                /*
                 *  Calculate the "this" to use for the function. If required function is a method in the current 
                 *  "this" object use the current thisObj. If the lookup.obj is a type, then use it. Otherwise global.
                 */
                if ((vp = fun->thisObj) == 0) {
                    if (ejsIsA(ejs, THIS, (EjsType*) lookup.obj)) {
                        vp = THIS;
                    } else if (ejsIsType(lookup.obj)) {
                        vp = lookup.obj;
                    } else {
                        vp = ejs->global;
                    }
                }
                if (fun->getter) {
                    fun = (EjsFunction*) ejsRunFunction(ejs, fun, vp, 0, NULL);
                }
                callProperty(ejs, fun, vp, argc, 0);
            }
            CHECK; BREAK;

        /*
         *  Call a constructor
         *      CallConstructor     <argc>
         *      Stack before (top)  [args]
         *                          [obj]
         *      Stack after         [obj]
         */
        CASE (EJS_OP_CALL_CONSTRUCTOR):
            argc = GET_INT();
            vp = state.stack[-argc];
            if (vp == 0 || vp == ejs->nullValue) {
                throwNull(ejs);
                CHECK; BREAK;
            }
            type = vp->type;
            mprAssert(type);
            if (type && type->hasConstructor) {
                mprAssert(type->baseType);
                //  Constructor is always at slot 0 (offset by base properties)
                slotNum = type->block.numInherited;
                fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, slotNum);
                callFunction(ejs, fun, vp, argc, 0);
            }
            CHECK; BREAK;

        /*
         *  Call the next constructor
         *      CallNextConstructor <argc>
         *      Stack before (top)  [args]
         *      Stack after         []
         */
        CASE (EJS_OP_CALL_NEXT_CONSTRUCTOR):
            argc = GET_INT();
            type = (EjsType*) FRAME->function.owner;
            mprAssert(type);
            type = type->baseType;
            if (type) {
                mprAssert(type->hasConstructor);
                slotNum = type->block.numInherited;
                fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, slotNum);
                callFunction(ejs, fun, NULL, argc, 0);
            } else {
                mprAssert(0);
            }
            CHECK; BREAK;

        /*
         *  Add a literal namespace to the set of open namespaces for the current block
         *      AddNamespace <string>
         */
        CASE (EJS_OP_ADD_NAMESPACE):
            str = GET_STRING();
            ejsAddNamespaceToBlock(ejs, state.bp, ejsCreateNamespace(ejs, str, str));
            CHECK; BREAK;

        /*
         *  Add a namespace expression (reference) to the set of open namespaces for the current block. (use namespace).
         *      Stack before (top)  [namespace]
         *      Stack after         []
         *      AddNamespaceRef
         */
        CASE (EJS_OP_ADD_NAMESPACE_REF):
            ejsAddNamespaceToBlock(ejs, state.bp, (EjsNamespace*) pop(ejs));
            CHECK; BREAK;

        /*
         *  Push a new scope block on the scope chain
         *      OpenBlock <slotNum> <nthBlock>
         */
        CASE (EJS_OP_OPEN_BLOCK):
            slotNum = GET_INT();
            vp = getNthBlock(ejs, GET_INT());
            v1 = GET_SLOT(vp, slotNum);
            if (!ejsIsBlock(v1)) {
                ejsThrowReferenceError(ejs, "Reference is not a block");
                CHECK; BREAK;
            }
            //  OPT
            blk = ejsCopyBlock(ejs, (EjsBlock*) v1, 0);
            blk->prev = blk->scopeChain = state.bp;
            state.bp = blk;
            blk->stackBase = state.stack;
            ejsSetDebugName(state.bp, mprGetName(v1));
            CHECK; BREAK;

        /*
         *  Add a new scope block from the stack onto on the scope chain
         *      OpenWith
         */
        CASE (EJS_OP_OPEN_WITH):
            vp = pop(ejs);
            blk = ejsCreateBlock(ejs, 0);
            ejsSetDebugName(blk, "with");
            memcpy((void*) blk, vp, vp->type->instanceSize);
            blk->prev = blk->scopeChain = state.bp;
            state.bp->referenced = 1;
            state.bp = blk;
            CHECK; BREAK;

        /*
         *  Store the top scope block off the scope chain
         *      CloseBlock
         *      CloseWith
         */
        CASE (EJS_OP_CLOSE_BLOCK):
            state.bp = state.bp->prev;
            CHECK; BREAK;

        /*
         *  Define a class and initialize by calling any static initializer.
         *      DefineClass <type>
         */
        CASE (EJS_OP_DEFINE_CLASS):
            type = GET_TYPE();
            if (type == 0 || !ejsIsType(type)) {
                ejsThrowReferenceError(ejs, "Reference is not a class");
            } else {
                type->block.scopeChain = state.bp;
                if (type && type->hasStaticInitializer) {
                    //  Static initializer is always immediately after the constructor (if present)
                    slotNum = type->block.numInherited;
                    if (type->hasConstructor) {
                        slotNum++;
                    }
                    fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, slotNum);
                    callFunction(ejs, fun, (EjsVar*) type, 0, 0);
                    if (type->implements && !ejs->exception) {
                        callInterfaceInitializers(ejs, type);
                    }
                    state.bp = &FRAME->function.block;
                }
            }
            ejs->result = (EjsVar*) type;
            mprAssert(ejs->exception || ejs->result);
            CHECK; BREAK;

        /*
         *  Define a function. This is used only for non-method functions to capture the scope chain.
         *      DefineFunction <slot> <nthBlock>
         */
        CASE (EJS_OP_DEFINE_FUNCTION):
            slotNum = GET_INT();
            vp = getNthBlock(ejs, GET_INT());
            mprAssert(vp != ejs->global);
            fun = (EjsFunction*) GET_SLOT(vp, slotNum);
            if (!ejsIsFunction(fun)) {
                ejsThrowReferenceError(ejs, "Reference is not a function");
            } else if (fun->fullScope) {
                fun = ejsCopyFunction(ejs, fun);
                fun->block.scopeChain = state.bp;
                state.bp->referenced = 1;
                fun->thisObj = FRAME->function.thisObj;
                SET_SLOT(vp, slotNum, fun, NULL);
            }
            CHECK; BREAK;

        /*
         *  Exception Handling --------------------------------------------
         */

        /*
         *  Invoke finally blocks before acting on: return, returnValue and break (goto) opcodes.
         *  Injected by the compiler prior to break, continue and return statements. Also at the end of a try block
         *  if there is a finally block.
         *
         *      finally
         */
        CASE (EJS_OP_FINALLY):
            if ((ex = findExceptionHandler(ejs, EJS_EX_FINALLY)) != 0) {
                if (FRAME->function.inCatch) {
                    popExceptionBlock(ejs);
                    push(FRAME->pc);
                    createExceptionBlock(ejs, ex, EJS_EX_FINALLY);
                    BLOCK->breakCatch = 1;
                } else {
                    createExceptionBlock(ejs, ex, EJS_EX_FINALLY);
                }
            }
            BREAK;

        /*
         *  End of an exception block. Put at the end of the last catch/finally block
         *      EndException
         */
        CASE (EJS_OP_END_EXCEPTION):
            if (FRAME->function.inException) {
                FRAME->function.inCatch = 0;
                FRAME->function.inException = 0;
                if (BLOCK->breakCatch) {
                    /* Restart the original instruction (return, break, continue) */
                    popExceptionBlock(ejs);
                    FRAME->pc = (uchar*) pop(ejs);
                } else {
                    offset = findEndException(ejs);
                    FRAME->pc = &FRAME->function.body.code.byteCode[offset];
                    popExceptionBlock(ejs);
                }
            }
            CHECK; BREAK;

        /*
         *  Throw an exception
         *      Stack before (top)  [exceptionObj]
         *      Stack after         []
         *      Throw
         */
        CASE (EJS_OP_THROW):
            ejs->exception = pop(ejs);
            ejs->attention = 1;
            CHECK; BREAK;

        /*
         *  Stack management ----------------------------------------------
         */

        /*
         *  Pop one item off the stack
         *      Pop
         *      Stack before (top)  [value]
         *      Stack after         []
         */
        CASE (EJS_OP_POP):
            ejs->result = pop(ejs);
#if MACOSX || UNUSED
            mprAssert(ejs->result != (void*) 0xf7f7f7f7f7f7f7f7);
#endif
            mprAssert(ejs->exception || ejs->result);
            BREAK;

        /*
         *  Pop N items off the stack
         *      PopItems            <count>
         *      Stack before (top)  [value]
         *                          [...]
         *      Stack after         []
         */
        CASE (EJS_OP_POP_ITEMS):
            state.stack -= GET_BYTE();
            BREAK;

        /*
         *  Duplicate one item on the stack
         *      Stack before (top)  [value]
         *      Stack after         [value]
         *                          [value]
         */
        CASE (EJS_OP_DUP):
            vp = state.stack[0];
            push(vp);
            BREAK;

        /*
         *  Duplicate two items on the stack
         *      Dup2
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [value1]
         *                          [value2]
         *                          [value1]
         *                          [value2]
         */
        CASE (EJS_OP_DUP2):
            v1 = state.stack[-1];
            push(v1);
            v1 = state.stack[0];
            push(v1);
            BREAK;

        /*
         *  Swap the top two items on the stack
         *      Swap
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [value2]
         *                          [value1]
         */
        CASE (EJS_OP_SWAP):
            SWAP; BREAK;

        /*
         *  Branching
         */

        /*
         *  Default function argument initialization. Computed goto for functions with more than 256 parameters.
         *      InitDefault         <tableSize>
         */
        CASE (EJS_OP_INIT_DEFAULT_ARGS): {
            int tableSize, numNonDefault;
            /*
             *  Use the argc value for the current function. Compare with the number of default args.
             */
            tableSize = (schar) GET_BYTE();
            numNonDefault = FRAME->function.numArgs - FRAME->function.numDefault;
            mprAssert(numNonDefault >= 0);
            offset = FRAME->argc - numNonDefault;
            if (offset < 0 || offset > tableSize) {
                offset = tableSize;
            }
            FRAME->pc += ((uint*) FRAME->pc)[offset];
            CHECK; BREAK;
        }

        /*
         *  Default function argument initialization. Computed goto for functions with less than 256 parameters.
         *      InitDefault.8       <tableSize.8>
         */
        CASE (EJS_OP_INIT_DEFAULT_ARGS_8): {
            int tableSize, numNonDefault;
            tableSize = (schar) GET_BYTE();
            numNonDefault = FRAME->function.numArgs - FRAME->function.numDefault;
            mprAssert(numNonDefault >= 0);
            offset = FRAME->argc - numNonDefault;
            if (offset < 0 || offset > tableSize) {
                offset = tableSize;
            }
            FRAME->pc += FRAME->pc[offset];
            CHECK; BREAK;
        }

        /*
         *  Unconditional branch to a new location
         *      Goto                <offset.32>
         */
        CASE (EJS_OP_GOTO):
            offset = GET_WORD();
            FRAME->pc = &FRAME->pc[offset];
            BREAK;

        /*
         *  Unconditional branch to a new location (8 bit)
         *      Goto.8              <offset.8>
         */
        CASE (EJS_OP_GOTO_8):
            offset = (schar) GET_BYTE();
            FRAME->pc = &FRAME->pc[offset];
            BREAK;

        /*
         *  Branch to offset if false
         *      BranchFalse
         *      Stack before (top)  [boolValue]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_FALSE):
            offset = GET_WORD();
            goto commonBoolBranchCode;

        /*
         *  Branch to offset if true
         *      BranchTrue
         *      Stack before (top)  [boolValue]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_TRUE):
            offset = GET_WORD();
            goto commonBoolBranchCode;

        /*
         *  Branch to offset if false (8 bit)
         *      BranchFalse.8
         *      Stack before (top)  [boolValue]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_FALSE_8):
            opcode = (EjsOpCode) (opcode - EJS_OP_BRANCH_TRUE_8 + EJS_OP_BRANCH_TRUE);
            offset = (schar) GET_BYTE();
            goto commonBoolBranchCode;

        /*
         *  Branch to offset if true (8 bit)
         *      BranchTrue.8
         *      Stack before (top)  [boolValue]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_TRUE_8):
            /* We want sign extension here */
            opcode = (EjsOpCode) (opcode - EJS_OP_BRANCH_TRUE_8 + EJS_OP_BRANCH_TRUE);
            offset = (schar) GET_BYTE();

        /*
         *  Common boolean branch code
         */
        commonBoolBranchCode:
            v1 = pop(ejs);
            if (v1 == 0 || !ejsIsBoolean(v1)) {
                v1 = ejsCastVar(ejs, v1, ejs->booleanType);
                if (ejs->exception) {
                    CHECK; BREAK;
                }
            }
            if (!ejsIsBoolean(v1)) {
                ejsThrowTypeError(ejs, "Result of a comparision must be boolean");
                CHECK; BREAK;
            }
            if (opcode == EJS_OP_BRANCH_TRUE) {
                if (((EjsBoolean*) v1)->value) {
                    FRAME->pc = &FRAME->pc[offset];
                }
            } else {
                if (((EjsBoolean*) v1)->value == 0) {
                    FRAME->pc = &FRAME->pc[offset];
                }
            }
            BREAK;

        /*
         *  Branch to offset if [value1] == null
         *      BranchNull
         *      Stack before (top)  [boolValue]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_NULL):
            push(ejs->nullValue);
            offset = GET_WORD();
            goto commonBranchCode;

        /*
         *  Branch to offset if [value1] == undefined
         *      BranchUndefined
         *      Stack before (top)  [boolValue]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_UNDEFINED):
            push(ejs->undefinedValue);
            offset = GET_WORD();
            goto commonBranchCode;

        /*
         *  Branch to offset if [tos] value is zero
         *      BranchZero
         *      Stack before (top)  [boolValue]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_ZERO):
            /* Fall through */

        /*
         *  Branch to offset if [tos] value is not zero
         *      BranchNotZero
         *      Stack before (top)  [boolValue]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_NOT_ZERO):
            push(ejs->zeroValue);
            offset = GET_WORD();
            goto commonBranchCode;

        /*
         *  Branch to offset if [value1] == [value2]
         *      BranchEQ
         *      Stack before (top)  [value1]
         *      Stack before (top)  [value2]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_EQ):

        /*
         *  Branch to offset if [value1] === [value2]
         *      BranchStrictlyEQ
         *      Stack before (top)  [value1]
         *      Stack after         [value2]
         */
        CASE (EJS_OP_BRANCH_STRICTLY_EQ):

        /*
         *  Branch to offset if [value1] != [value2]
         *      BranchNotEqual
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_NE):

        /*
         *  Branch to offset if [value1] !== [value2]
         *      BranchStrictlyNE
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_STRICTLY_NE):

        /*
         *  Branch to offset if [value1] <= [value2]
         *      BranchLE
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_LE):

        /*
         *  Branch to offset if [value1] < [value2]
         *      BranchLT
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_LT):

        /*
         *  Branch to offset if [value1] >= [value2]
         *      BranchGE
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_GE):

        /*
         *  Branch to offset if [value1] > [value2]
         *      BranchGT
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         []
         */
        CASE (EJS_OP_BRANCH_GT):
            offset = GET_WORD();
            goto commonBranchCode;

        /*
         *  Handle all branches here. We convert to a compare opcode and pass to the type to handle.
         */
        commonBranchCode:
            opcode = (EjsOpCode) (opcode - EJS_OP_BRANCH_EQ + EJS_OP_COMPARE_EQ);
            v2 = pop(ejs);
            v1 = pop(ejs);
            result = evalBinaryExpr(ejs, v1, opcode, v2);
            if (!ejsIsBoolean(result)) {
                ejsThrowTypeError(ejs, "Result of a comparision must be boolean");
                CHECK;
            } else {
                if (((EjsBoolean*) result)->value) {
                    FRAME->pc = &FRAME->pc[offset];
                }
            }
            BREAK;

        /*
         *  Compare if [value1] == true
         *      CompareTrue
         *      Stack before (top)  [value]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_TRUE):

        /*
         *  Compare if ![value1]
         *      CompareNotTrue
         *      Stack before (top)  [value]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_FALSE):
            v1 = pop(ejs);
            result = evalUnaryExpr(ejs, v1, opcode);
            mprAssert(ejs->exception || ejs->result);
            push(result);
            CHECK; BREAK;

        /*
         *  Compare if [value1] == NULL
         *      CompareNull
         *      Stack before (top)  [value]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_NULL):
            push(ejs->nullValue);
            goto binaryExpression;

        /*
         *  Compare if [item] == undefined
         *      CompareUndefined
         *      Stack before (top)  [value]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_UNDEFINED):
            push(ejs->undefinedValue);
            goto binaryExpression;

        /*
         *  Compare if [item] value is zero
         *      CompareZero
         *      Stack before (top)  [value]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_ZERO):
            push(ejsCreateNumber(ejs, 0));
            goto binaryExpression;

        /*
         *  Compare if [tos] value is not zero
         *      CompareZero
         *      Stack before (top)  [value]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_NOT_ZERO):
            push(ejsCreateNumber(ejs, 0));
            goto binaryExpression;

        /*
         *  Compare if [value1] == [item2]
         *      CompareEQ
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_EQ):

        /*
         *  Compare if [value1] === [item2]
         *      CompareStrictlyEQ
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_STRICTLY_EQ):

        /*
         *  Compare if [value1] != [item2]
         *      CompareNE
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_NE):

        /*
         *  Compare if [value1] !== [item2]
         *      CompareStrictlyNE
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_STRICTLY_NE):

        /*
         *  Compare if [value1] <= [item2]
         *      CompareLE
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_LE):

        /*
         *  Compare if [value1] < [item2]
         *      CompareStrictlyLT
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_LT):

        /*
         *  Compare if [value1] >= [item2]
         *      CompareGE
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_GE):

        /*
         *  Compare if [value1] > [item2]
         *      CompareGT
         *      Stack before (top)  [value1]
         *                          [value2]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_COMPARE_GT):

        /*
         *  Binary expressions
         *      Stack before (top)  [right]
         *                          [left]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_ADD):
        CASE (EJS_OP_SUB):
        CASE (EJS_OP_MUL):
        CASE (EJS_OP_DIV):
        CASE (EJS_OP_REM):
        CASE (EJS_OP_SHL):
        CASE (EJS_OP_SHR):
        CASE (EJS_OP_USHR):
        CASE (EJS_OP_AND):
        CASE (EJS_OP_OR):
        CASE (EJS_OP_XOR):
        binaryExpression:
            v2 = pop(ejs);
            v1 = pop(ejs);
            ejs->result = evalBinaryExpr(ejs, v1, opcode, v2);
            push(ejs->result);
            CHECK; BREAK;

        /*
         *  Unary operators
         */

        /*
         *  Negate a value
         *      Neg
         *      Stack before (top)  [value]
         *      Stack after         [result]
         */
        CASE (EJS_OP_NEG):
            v1 = pop(ejs);
            result = evalUnaryExpr(ejs, v1, opcode);
            push(result);
            CHECK; BREAK;

        /*
         *  Logical not (~value)
         *      LogicalNot
         *      Stack before (top)  [value]
         *      Stack after         [result]
         */
        CASE (EJS_OP_LOGICAL_NOT):
            v1 = pop(ejs);
            v1 = ejsCastVar(ejs, v1, ejs->booleanType);
            result = evalUnaryExpr(ejs, v1, opcode);
            push(result);
            CHECK; BREAK;

        /*
         *  Bitwise not (!value)
         *      BitwiseNot
         *      Stack before (top)  [value]
         *      Stack after         [result]
         */
        CASE (EJS_OP_NOT):
            v1 = pop(ejs);
            result = evalUnaryExpr(ejs, v1, opcode);
            push(result);
            CHECK; BREAK;

        /*
         *  Increment a stack variable
         *      Inc                 <increment>
         *      Stack before (top)  [value]
         *      Stack after         [result]
         */
        CASE (EJS_OP_INC):
            v1 = pop(ejs);
            count = (schar) GET_BYTE();
            result = evalBinaryExpr(ejs, v1, EJS_OP_ADD, (EjsVar*) ejsCreateNumber(ejs, count));
            push(result);
            CHECK; BREAK;

        /*
         *  Object creation
         */

        /*
         *  Create a new object
         *      New
         *      Stack before (top)  [type]
         *      Stack after         [obj]
         */
        CASE (EJS_OP_NEW):
            type = (EjsType*) pop(ejs);
            if (ejsIsType(type)) {
                v1 = ejsCreateVar(ejs, type, 0);
                push(v1);
                ejs->result = v1;
                mprAssert(ejs->exception || ejs->result);
            } else {
                if (ejsIsFunction(type)) {
                    fun = (EjsFunction*) type;
                    if ((type = fun->prototype) == 0) {
                        type = ejsCreatePrototype(ejs, fun, &slotNum);
                    }
                }
                if (ejsIsType(type)) {
                    v1 = ejsCreateVar(ejs, type, 0);
                    push(v1);
                    ejs->result = v1;
                    mprAssert(ejs->exception || ejs->result);
                } else {
                    ejsThrowReferenceError(ejs, "Can't locate type");
                }
            }
            CHECK; BREAK;

        /*
         *  Create a new object literal
         *      NewObject           <type> <argc.
         *      Stack before (top)  [args]
         *      Stack after         []
         */
        CASE (EJS_OP_NEW_OBJECT):
            type = GET_TYPE();
            argc = GET_INT();
            vp = (EjsVar*) ejsCreateObject(ejs, type, 0);
            for (i = 1 - (argc * 3); i <= 0; ) {
                spaceVar = ejsToString(ejs, state.stack[i++]);
                nameVar = ejsToString(ejs, state.stack[i++]);
                v1 = state.stack[i++];
                if (v1 && nameVar && spaceVar) {
                    if (ejsIsFunction(v1)) {
                        fun = (EjsFunction*) v1;
                        if (fun->literalGetter) {
                            fun->getter = 1;
                        }
                        if (fun->getter || fun->setter) {
                             vp->hasGetterSetter = 1;
                        }
                    }
                    ejsName(&qname, mprStrdup(vp, spaceVar->value), mprStrdup(vp, nameVar->value));
                    vp->noPool = 1;
                    ejsSetPropertyByName(ejs, vp, &qname, v1);
                }
            }
            state.stack -= (argc * 3);
            push(vp);
            CHECK; BREAK;

        /*
         *  Reference the super class
         *      Super
         *      Stack before (top)  [obj]
         *      Stack after         [type]
         */
        CASE (EJS_OP_SUPER):
            push(FRAME->function.thisObj->type);
            BREAK;

        /*
         *  Delete an object property by name expression
         *      DeleteNameExpr
         *      Stack before (top)  [name]
         *                          [space]
         *                          [obj]
         *      Stack after         []
         */
        CASE (EJS_OP_DELETE_NAME_EXPR):
            qname.name = ejsToString(ejs, pop(ejs))->value;
            v1 = pop(ejs);
            if (ejsIsNamespace(v1)) {
                qname.space = ((EjsNamespace*) v1)->uri;
            } else {
                qname.space = ejsToString(ejs, v1)->value;
            }
            vp = pop(ejs);
#if OLD
            slotNum = ejsLookupVar(ejs, vp, &qname, &lookup);
            if (slotNum < 0) {
                ejsThrowReferenceError(ejs, "Property \"%s\" does not exist", qname.name);
            } else {
                slotNum = ejsDeleteProperty(ejs, vp, slotNum);
            }
#endif
            ejsDeletePropertyByName(ejs, vp, &qname);
            CHECK; BREAK;

        /*
         *  Delete an object property from the current scope
         *      DeleteScopedNameExpr
         *      Stack before (top)  [name]
         *                          [space]
         *      Stack after         []
         */
        CASE (EJS_OP_DELETE_SCOPED_NAME_EXPR):
            qname.name = ejsToString(ejs, pop(ejs))->value;
            v1 = pop(ejs);
            if (ejsIsNamespace(v1)) {
                qname.space = ((EjsNamespace*) v1)->uri;
            } else {
                qname.space = ejsToString(ejs, v1)->value;
            }
            lookup.storing = 0;
            slotNum = ejsLookupScope(ejs, &qname, &lookup);
            if (slotNum < 0) {
                ejsThrowReferenceError(ejs, "Property \"%s\" does not exist", qname.name);
            } else {
                ejsDeleteProperty(ejs, lookup.obj, slotNum);
            }
            CHECK; BREAK;

        /*
         *  No operation. Does nothing.
         *      Nop
         */
        CASE (EJS_OP_NOP):
            BREAK;

        /*
         *  Check if object is a given type
         *      IsA, Like, InstanceOf
         *      Stack before (top)  [type]
         *                          [obj]
         *      Stack after         [boolean]
         */
        CASE (EJS_OP_INST_OF):
        CASE (EJS_OP_IS_A):
        CASE (EJS_OP_LIKE):
            type = (EjsType*) pop(ejs);
            v1 = pop(ejs);
            push(ejsCreateBoolean(ejs, ejsIsA(ejs, v1, type)));
            BREAK;

        /*
         *  Get the type of an object.
         *      TypeOf              <obj>
         *      Stack before (top)  [obj]
         *      Stack after         [string]
         */
        CASE (EJS_OP_TYPE_OF):
            v1 = pop(ejs);
            push(ejsGetTypeOf(ejs, v1));
            BREAK;

        /*
         *  Cast an object to the given the type. Throw if not castable.
         *      Cast
         *      Stack before (top)  [type]
         *                          [obj]
         *      Stack after         [result]
         */
        CASE (EJS_OP_CAST):
            type = (EjsType*) pop(ejs);
            if (!ejsIsType(type)) {
                ejsThrowTypeError(ejs, "Not a type");
            } else {
                v1 = pop(ejs);
                push(ejsCastVar(ejs, v1, type));
            }
            CHECK; BREAK;

        /*
         *  Cast to a boolean type
         *      CastBoolean
         *      Stack before (top)  [value]
         *      Stack after         [result]
         */
        CASE (EJS_OP_CAST_BOOLEAN):
            v1 = ejsCastVar(ejs, pop(ejs), ejs->booleanType);
            push(v1);
            CHECK; BREAK;

        /*
         *  Test if a given name is the name of a property "in" an object
         *      Cast
         *      Stack before (top)  [obj]
         *                          [name]
         *      Stack after         [result]
         */
        CASE (EJS_OP_IN):
            v1 = pop(ejs);
            nameVar = ejsToString(ejs, pop(ejs));
            if (nameVar == 0) {
                ejsThrowTypeError(ejs, "Can't convert to a name");
            } else {
                ejsName(&qname, "", nameVar->value);                        //  Don't consult namespaces
                slotNum = ejsLookupProperty(ejs, v1, &qname);
                if (slotNum < 0) {
                    slotNum = ejsLookupVar(ejs, v1, &qname, &lookup);
                    if (slotNum < 0 && ejsIsType(v1)) {
                        slotNum = ejsLookupVar(ejs, (EjsVar*) ((EjsType*) v1)->instanceBlock, &qname, &lookup);
                    }
                }
                push(ejsCreateBoolean(ejs, slotNum >= 0));
            }
            CHECK; BREAK;

        /*
         *  Unimplemented op codes
         */
        CASE (EJS_OP_BREAKPOINT):
            mprAssert(0);
            BREAK;

#if !BLD_UNIX_LIKE && !VXWORKS
        }
        if (ejs->attention && !payAttention(ejs)) {
            goto done;
        }
    }
#endif
    
done:
    ejs->state = priorState;
}


static MPR_INLINE int storePropertyToSlot(Ejs *ejs, EjsObject *obj, int slotNum, EjsVar *value, EjsVar *thisObj)
{
    EjsFunction     *fun;

    if (ejsIsObject(obj) && slotNum < obj->numProp) {
        fun = (EjsFunction*) obj->slots[slotNum];
    } else {
        fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) obj, slotNum);
    }
    if (unlikely(fun && ((EjsVar*) fun)->isFunction && fun->getter && fun->nextSlot >= 0)) {
        handleSetters(ejs, fun, thisObj, value);
        if (unlikely(ejs->attention) && !payAttention(ejs)) {
            return 0;
        }
        return 1;
    }
    if (ejsIsObject(obj) && slotNum < obj->numProp) {
        obj->slots[slotNum] = value;
    } else {
        ejsSetProperty(ejs, (EjsVar*) obj, slotNum, (EjsVar*) value);
        if (unlikely(ejs->attention) && !payAttention(ejs)) {
            return 0;
        }
    }
    return 1;
}


/*
 *  Store a property by name in the given object. Will create if the property does not already exist.
 */
static void storeProperty(Ejs *ejs, EjsVar *obj, EjsName *qname, bool dup)
{
    EjsFunction     *fun;
    EjsLookup       lookup;
    EjsVar          *value, *origObj;
    int             slotNum;

    mprAssert(qname);
    mprAssert(qname->name);
    mprAssert(obj);

    slotNum = -1;
    origObj = obj;

    if (obj->type->helpers->setPropertyByName) {
        /*
         *  If a setPropertyByName helper is defined, defer to it. Allows types like XML to not offer slot based APIs.
         */
        ejs->result = value = popOutside(ejs);
        mprAssert(ejs->exception || ejs->result);
        slotNum = (*obj->type->helpers->setPropertyByName)(ejs, obj, qname, value);
        if (slotNum >= 0) {
            return;
        }
        pushOutside(ejs, value);
    }

    slotNum = ejsLookupVar(ejs, obj, qname, &lookup);
    if (slotNum >= 0) {
        obj = lookup.obj;
        /*
         *  Handle setters. Setters, if present, are chained off the getter.
         */
        if (obj->hasGetterSetter) {
            fun = (EjsFunction*) ejsGetProperty(ejs, obj, slotNum);
            if (ejsIsFunction(fun) && fun->getter && fun->nextSlot) {
                fun = (EjsFunction*) ejsGetProperty(ejs, fun->owner, fun->nextSlot);
                callFunction(ejs, fun, origObj, 1, 0);
                return;
            }
        }
        ejs->result = value = popOutside(ejs);
        mprAssert(ejs->exception || ejs->result);
        slotNum = ejsSetProperty(ejs, obj, slotNum, value);
        
    } else {
        ejs->result = value = popOutside(ejs);
        mprAssert(ejs->exception || ejs->result);
        slotNum = ejsSetProperty(ejs, obj, slotNum, value);
        if (slotNum >= 0) {
            if (dup) {
                qname->name = mprStrdup(obj, qname->name);
                qname->space = mprStrdup(obj, qname->space);
                obj->noPool = 1;
            }
            ejsSetPropertyName(ejs, obj, slotNum, qname);
        }
    }
}


/*
 *  Store a property by name in the scope chain. Will create properties if the given name does not already exist.
 */
static void storePropertyToScope(Ejs *ejs, EjsName *qname, bool dup)
{
    EjsFrame        *fp;
    EjsFunction     *fun;
    EjsVar          *value, *obj;
    EjsLookup       lookup;
    int             slotNum;

    mprAssert(qname);

    fp = ejs->state->fp;
    lookup.storing = 1;
    slotNum = ejsLookupScope(ejs, qname, &lookup);

    if (slotNum >= 0) {
        obj = lookup.obj;
        fun = (EjsFunction*) ejsGetProperty(ejs, obj, slotNum);
        if (fun && ejsIsFunction(fun) && fun->getter && fun->nextSlot) {
            fun = (EjsFunction*) ejsGetProperty(ejs, fun->owner, fun->nextSlot);
            callFunction(ejs, fun, obj, 1, 0);
            return;
        }
    } else if (fp->function.lang & EJS_SPEC_FIXED && fp->caller) {
        obj = (EjsVar*) fp;
    } else {
        obj = ejs->global;
    }
    value = popOutside(ejs);
    ejs->result = value;
    mprAssert(ejs->exception || ejs->result);
    slotNum = ejsSetProperty(ejs, obj, slotNum, value);
    if (slotNum >= 0) {
        if (dup) {
            qname->name = mprStrdup(obj, qname->name);
            qname->space = mprStrdup(obj, qname->space);
            obj->noPool = 1;
        }
        ejsSetPropertyName(ejs, obj, slotNum, qname);
    }
}


/*
 *  Attend to unusual circumstances. Memory allocation errors, exceptions and forced exits.
 */
static bool payAttention(Ejs *ejs)
{
    ejs->attention = 0;

    if (ejs->gcRequired) {
        ejsCollectGarbage(ejs, EJS_GEN_NEW);
    }
    if (mprHasAllocError(ejs)) {
        mprResetAllocError(ejs);
        ejsThrowMemoryError(ejs);
        ejs->attention = 1;
        return 0;
    }
    if (ejs->exception && !manageExceptions(ejs)) {
        ejs->attention = 1;
        return 0;
    }
    if (ejs->exiting || mprIsExiting(ejs)) {
        return 0;
    }
    return 1;
}


/*
 *  Run the module initializer
 */
EjsVar *ejsRunInitializer(Ejs *ejs, EjsModule *mp)
{
    EjsModule   *dp;
    EjsVar      *result;
    int         next;

    if (mp->initialized || !mp->hasInitializer) {
        mp->initialized = 1;
        return ejs->nullValue;
    }
    mp->initialized = 1;

    if (mp->dependencies) {
        for (next = 0; (dp = (EjsModule*) mprGetNextItem(mp->dependencies, &next)) != 0;) {
            if (dp->hasInitializer && !dp->initialized) {
                if (ejsRunInitializer(ejs, dp) == 0) {
                    mprAssert(ejs->exception);
                    return 0;
                }
            }
        }
    }
    mprLog(ejs, 6, "Running initializer for module %s", mp->name);
    result = ejsRunFunction(ejs, mp->initializer, ejs->global, 0, NULL);

    ejsMakeTransient(ejs, (EjsVar*) mp->initializer);
    mprAssert(ejs->exception || ejs->result);
    return result;
}


/*
 *  Run all initializers for all modules
 */
int ejsRun(Ejs *ejs)
{
    EjsModule   *mp;
    int         next;

    /*
     *  This is used by ejs to interpret scripts. OPT. Should not run through old modules every time
     */
    for (next = 0; (mp = (EjsModule*) mprGetNextItem(ejs->modules, &next)) != 0;) {
        if (mp->initialized) {
            continue;
        }
        if (ejsRunInitializer(ejs, mp) == 0) {
            mprAssert(ejs->exception);
            return EJS_ERR;
        }
    }
    return 0;
}


/*
 *  Run a function with the given parameters
 */
EjsVar *ejsRunFunction(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, int argc, EjsVar **argv)
{
    int         i;
    
    mprAssert(ejs);
    mprAssert(fun);
    mprAssert(ejsIsFunction(fun));

    if (thisObj == 0) {
        if ((thisObj = fun->thisObj) == 0) {
            thisObj = ejs->state->fp->function.thisObj;
        }
    }    

    if (ejsIsNativeFunction(fun)) {
        mprAssert(fun->body.proc);
        if (fun->body.proc == 0) {
            ejsThrowArgError(ejs, "Native function is not defined");
            return 0;
        }
        ejs->result = (fun->body.proc)(ejs, thisObj, argc, argv);
        if (ejs->result == 0) {
            ejs->result = ejs->nullValue;
        }
        mprAssert(ejs->exception || ejs->result);

    } else {
        mprAssert(fun->body.code.byteCode);
        mprAssert(fun->body.code.codeLen > 0);
        
        for (i = 0; i < argc; i++) {
            pushOutside(ejs, argv[i]);
        }
        VM(ejs, fun, thisObj, argc, 0);
        ejs->state->stack -= argc;
        if (ejs->exiting || mprIsExiting(ejs)) {
            ejs->attention = 1;
        }
    }
    return (ejs->exception) ? 0 : ejs->result;
}


/*
 *  Run a function by slot.
 */
EjsVar *ejsRunFunctionBySlot(Ejs *ejs, EjsVar *obj, int slotNum, int argc, EjsVar **argv)
{
    EjsFunction     *fun;

    if (obj == 0) {
        mprAssert(0);
        return 0;
    }

    if (obj == ejs->global) {
        fun = (EjsFunction*) ejsGetProperty(ejs, obj, slotNum);
    } else {
        fun = (EjsFunction*) ejsGetProperty(ejs, ejsIsType(obj) ? obj : (EjsVar*) obj->type, slotNum);
    }
    if (fun == 0) {
        return 0;
    }
    return ejsRunFunction(ejs, fun, obj, argc, argv);
}


/*
 *  Validate the args. This routine handles ...rest args and parameter type checking and casts. Returns the new argc 
 *  or < 0 on errors.
 */
static int validateArgs(Ejs *ejs, EjsFunction *fun, int argc, EjsVar **argv)
{
    EjsTrait        *trait;
    EjsVar          *newArg;
    EjsArray        *rest;
    int             nonDefault, i, limit, numRest;

    nonDefault = fun->numArgs - fun->numDefault;

    if (argc < nonDefault) {
        if (!fun->rest || argc != (fun->numArgs - 1)) {
            if (fun->lang < EJS_SPEC_FIXED) {
                /*
                 *  Create undefined values for missing args
                 */
                for (i = argc; i < nonDefault; i++) {
                    pushOutside(ejs, ejs->undefinedValue);
                }
                argc = nonDefault;

            } else {
                ejsThrowArgError(ejs, "Insufficient actual parameters. Call requires %d parameter(s).", nonDefault);
                return EJS_ERR;
            }
        }
    }

    if ((uint) argc > fun->numArgs && !fun->rest) {
        /*
         *  Discard excess arguments for scripted functions. No need to discard for native procs. This allows
         *  ejsDefineGlobalFunction to not have to bother with specifying the number of args for native procs.
         */
        if (!ejsIsNativeFunction(fun)) {
            ejs->state->stack -=  (argc - fun->numArgs);
            argc = fun->numArgs;
        }
    }

    /*
     *  Handle rest "..." args
     */
    if (fun->rest) {
        numRest = argc - fun->numArgs + 1;
        rest = ejsCreateArray(ejs, numRest);
        if (rest == 0) {
            return EJS_ERR;
        }
        for (i = numRest - 1; i >= 0; i--) {
            ejsSetProperty(ejs, (EjsVar*) rest, i, popOutside(ejs));
        }
        argc = argc - numRest + 1;
        pushOutside(ejs, rest);
    }

    if (fun->block.numTraits == 0) {
        return argc;
    }

    mprAssert((uint) fun->block.numTraits >= fun->numArgs);

    /*
     *  Cast args to the right types
     */
    limit = min((uint) argc, fun->numArgs);
    for (i = 0; i < limit; i++) {
        if ((trait = ejsGetTrait((EjsBlock*) fun, i)) == 0 || trait->type == 0) {
            continue;
        }
        if (argv[i] == ejs->nullValue && (trait->attributes & EJS_ATTR_NOT_NULLABLE)) {
            continue;
        }
        if (!ejsIsA(ejs, argv[i], trait->type)) {
            if (ejsIsNull(argv[i]) || ejsIsUndefined(argv[i])) {
                newArg = argv[i];
            } else {
                if (trait->type == ejs->stringType) {
                    /*
                     *  Do this to pickup toString overrides.
                     */
                    newArg = (EjsVar*) ejsToString(ejs, argv[i]);
                } else {
                    newArg = ejsCastVar(ejs, argv[i], trait->type);
                }
            }
            if (newArg == 0) {
                mprAssert(ejs->exception);
                return EJS_ERR;
            }
            argv[i] = newArg;
        }
    }

    return argc;
}


/*
 *  Call a function aka pushFunctionFrame. Supports both native and scripted functions. If native, the function is fully 
 *  invoked here. If scripted, a new frame is created and the pc adjusted to point to the new function.
 */
static void callConstructor(Ejs *ejs, EjsFunction *vp, int argc, int stackAdjust)
{
    EjsFunction     *fun;
    EjsType         *type;
    EjsVar          *obj;
    int             slotNum;

    mprAssert(!ejsIsFunction(vp));

    if ((EjsVar*) vp == (EjsVar*) ejs->undefinedValue) {
        ejsThrowReferenceError(ejs, "Function is undefined");
        return;

    } else if (ejsIsType(vp)) {
        /* 
         *  Handle calling a constructor to create a new instance 
         */
        type = (EjsType*) vp;
        obj = ejsCreateVar(ejs, type, 0);

        if (type->hasConstructor) {
            /*
             *  Constructor is always at slot 0 (offset by base properties)
             */
            slotNum = type->block.numInherited;
            fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, slotNum);

            if (ejsIsNativeFunction(fun)) {
                callFunction(ejs, fun, obj, argc, stackAdjust);
                mprAssert(ejs->result || ejs->exception);

            } else {
                VM(ejs, fun, obj, argc, stackAdjust);
                ejs->state->stack -= (argc + stackAdjust);
                if (ejs->exiting || mprIsExiting(ejs)) {
                    ejs->attention = 1;
                }
            }
        }
        ejs->result = obj;
        mprAssert(ejs->exception || ejs->result);

    } else {
        ejsThrowReferenceError(ejs, "Reference is not a function");
    }
}


/*
 *  Find the right base class to use as "this" for a static method
 */
static EjsVar *getStaticThis(Ejs *ejs, EjsType *type, int slotNum)
{
    while (type) {
        /*
         *  Use baseType->numTraits rather than numInherited because implemented traits are not accessed via the base type.
         */
        if (slotNum >= type->baseType->block.numTraits) {
            break;
        }
        type = type->baseType;
    }
    return (EjsVar*) type;
}


static void callInterfaceInitializers(Ejs *ejs, EjsType *type)
{
    EjsType     *iface;
    EjsFunction *fun;
    int         next, slotNum;

    for (next = 0; ((iface = mprGetNextItem(type->implements, &next)) != 0); ) {
        if (iface->hasStaticInitializer) {
            slotNum = iface->block.numInherited;
            if (iface->hasConstructor) {
                slotNum++;
            }
            fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) iface, slotNum);
            callFunction(ejs, fun, (EjsVar*) type, 0, 0);
        }
    }
}


/*
 *  Push a block. Used by the compiler.
 */
EjsBlock *ejsPushBlock(Ejs *ejs, EjsBlock *original)
{
    EjsBlock    *block;

    mprAssert(!ejsIsFunction(original));

    //  OPT. Need a faster way to do this. Should this copy slots? old code did. 
    block = ejsCopyBlock(ejs, original, 0);
    block->scopeChain = ejs->state->bp;
    block->prev = ejs->state->bp;
    block->stackBase = ejs->state->stack;
    ejs->state->bp = block;
    return block;
}


/*
 *  Pop a block frame and return to the previous frame.  This pops functions and/or lexical blocks.
 */
EjsBlock *ejsPopBlock(Ejs *ejs)
{
    EjsBlock    *bp;

    bp = ejs->state->bp;
    ejs->state->stack = bp->stackBase;
    return ejs->state->bp = bp->prev;
}


/*
 *  Pop an exception block.
 */
static EjsBlock *popExceptionBlock(Ejs *ejs)
{
    EjsBlock     *prev;

    if ((prev = ejs->state->bp->prev) != 0) {
        if (ejs->exception == 0) {
            ejs->exception = prev->prevException;
            prev->prevException = 0;
            if (ejs->exception) {
                ejs->attention = 1;
                /* Advance by one as checkExceptionHandlers expects the PC to be advanced after parsing the opcode */
                ejs->state->fp->pc++;
            }
        }
    }
    ejs->state->bp = prev;
    return prev;
}


/*
 *  Manage exceptions. Bubble up the exception until we find an exception handler for it.
 */
static bool manageExceptions(Ejs *ejs)
{
    EjsState        *state;

    state = ejs->state;

    /*
     *  Check at each function level for a handler to process the exception.
     */
    while (state->fp) {
        checkExceptionHandlers(ejs);
        if (ejs->exception == 0) {
            return 1;
        }
        state->stack = state->fp->stackReturn;
        state->bp = state->fp->function.block.prev;
        state->fp = state->fp->caller;
    }
    return 0;
}


static inline EjsEx *findExceptionHandler(Ejs *ejs, int kind)
{
    EjsEx       *ex;
    EjsFrame    *fp;
    EjsCode     *code;
    uint        pc;
    int         i;

    ex = 0;
    fp = ejs->state->fp;
    code = &fp->function.body.code;
    pc = (uint) (fp->pc - code->byteCode - 1);

    /*
     *  Exception handlers are sorted with the inner most handlers first.
     */
    for (i = 0; i < code->numHandlers; i++) {
        ex = code->handlers[i];
        if (ex->tryStart <= pc && pc < ex->handlerEnd && (ex->flags & kind)) {
            if (kind == EJS_EX_FINALLY || ex->catchType == ejs->voidType || 
                    ejsIsA(ejs, (EjsVar*) ejs->exception, ex->catchType)) {
                return ex;
            }
        }
    }
    return 0;
}

static inline EjsEx *inHandler(Ejs *ejs, int kind)
{
    EjsEx       *ex;
    EjsFrame    *fp;
    EjsCode     *code;
    uint        pc;
    int         i;
    
    ex = 0;
    fp = ejs->state->fp;
    code = &fp->function.body.code;
    pc = (uint) (fp->pc - code->byteCode - 1);
    
    /*
     *  Exception handlers are sorted with the inner most handlers first.
     */
    for (i = 0; i < code->numHandlers; i++) {
        ex = code->handlers[i];
        if (ex->handlerStart <= pc && pc < ex->handlerEnd && (ex->flags & kind)) {
            return ex;
        }
    }
    return 0;
}


/*
 *  Find the end of the last catch/finally handler.
 */
static inline uint findEndException(Ejs *ejs)
{
    EjsFrame    *fp;
    EjsEx       *best, *ex;
    EjsCode     *code;
    uint        offset, pc;
    int         i;

    ex = 0;
    fp = ejs->state->fp;
    code = &fp->function.body.code;
    pc = (uint) (fp->pc - code->byteCode - 1);
    offset = 0;

    for (best = 0, i = 0; i < code->numHandlers; i++) {
        ex = code->handlers[i];
        /*
         *  Comparison must include try and all catch handlers, incase there are multiple catch handlers
         */
        if (ex->tryStart <= pc && pc < ex->handlerEnd) {
            offset = ex->handlerEnd;
            for (++i; i < code->numHandlers; i++) {
                /* Find the last handler of this try block. Use tryEnd as nested try blocks can start at the same location */
                if (ex->tryEnd == code->handlers[i]->tryEnd) {
                    offset = code->handlers[i]->handlerEnd;
                }
            }
        }
    }
    mprAssert(offset);
    return offset;
}


/*
 *  Search for an exception handler at this level to process the exception. Return true if the exception is handled.
 */
static void checkExceptionHandlers(Ejs *ejs)
{
    EjsFrame        *fp;
    EjsCode         *code;
    EjsEx           *ex;
    uint            pc;

    ex = 0;
    fp = ejs->state->fp;
    code = &fp->function.body.code;

    if (code->numHandlers == 0) {
        return;
    }

    /*
     *  The PC is always one advanced from the throwing instruction. ie. the PC has advanced past the offending 
     *  instruction so reverse by one.
     */
    pc = (uint) (fp->pc - code->byteCode - 1);
    mprAssert(pc >= 0);

rescan:
    if (!fp->function.inException || (ejs->exception == (EjsVar*) ejs->stopIterationType)) {
        /*
         *  Normal exception in a try block. NOTE: the catch will jump or fall through to the finally block code.
         *  ie. We won't come here again for the finally code unless there is an exception in the catch block.
         *  Otherwise, No catch handler at this level and need to bubble up.
         */
        if ((ex = findExceptionHandler(ejs, EJS_EX_CATCH)) != 0) {
            createExceptionBlock(ejs, ex, ex->flags);
            return;
        }

    } else {
        /*
         *  Exception in a catch or finally block. If in a catch block, must first run the finally
         *  block before bubbling up. If in a finally block, we are done and upper levels will handle. We can be
         *  in a finally block and inException == 0. This happens because try blocks jump through directly
         *  into finally blocks (fast). But we need to check here if we are in the finally block explicitly.
         */
        if ((ex = inHandler(ejs, EJS_EX_FINALLY)) != 0) {
            /*
             *  If in a finally block, must advance the outer blocks's pc to be outside [tryStart .. finallyStart]
             *  This prevents this try block from handling this exception again.
             */
            fp->pc = &fp->function.body.code.byteCode[ex->handlerEnd + 1];
            fp->function.inCatch = fp->function.inException = 0;
            goto rescan;            
        }
    }

    /*
     *  Exception without a catch block or exception in a catch block. 
     */
    if ((ex = findExceptionHandler(ejs, EJS_EX_FINALLY)) != 0) {
        if (fp->function.inCatch) {
            popExceptionBlock(ejs);
        }
        createExceptionBlock(ejs, ex, EJS_EX_FINALLY);
    } else {
        fp->function.inCatch = fp->function.inException = 0;
    }
}


/*
 *  Called for catch and finally blocks
 */
static void createExceptionBlock(Ejs *ejs, EjsEx *ex, int flags)
{
    EjsBlock        *block;
    EjsFrame        *fp;
    EjsState        *state;
    EjsCode         *code;
    int             i, count;

    state = ejs->state;
    fp = state->fp;
    code = &state->fp->function.body.code;
    mprAssert(ex);

    if (flags & EJS_EX_ITERATION) {
        /*
         *  Empty handler is a special case for iteration. We simply do a break to the handler location
         *  which targets the end of the for/in loop.
         */
        fp->pc = &fp->function.body.code.byteCode[ex->handlerStart];
        ejs->exception = 0;
        return;
    }

    /*
     *  Discard all lexical blocks defined inside the try block.
     *  Count the lexical blocks. We know how many there should be before the try block (ex->numBlocks). 
     */
    if (!fp->function.inCatch) {
        for (count = 0, block = state->bp; block != (EjsBlock*) state->fp; block = block->prev) {
            count++;
        }
        count -= ex->numBlocks;
        mprAssert(count >= 0);
        for (i = 0; i < count; i++) {
            ejsPopBlock(ejs);
        }
#if OLD
        count = (state->stack - fp->stackReturn - fp->argc);
#else
        count = state->stack - fp->stackBase;
#endif
        state->stack -= (count - ex->numStack);
    }
    
    /*
     *  Allocate a new frame in which to execute the handler
     */
    block = ejsCreateBlock(ejs, 0);
    ejsSetDebugName(block, "exception");
    if (block == 0) {
        /*  Exception will continue to bubble up */
        return;
    }
    block->prev = block->scopeChain = state->bp;
    block->stackBase = state->stack;
    state->bp = block;
    state->fp->function.block.referenced = 1;

    /*
     *  Move the PC outside of the try region. If this is a catch block, this allows the finally block to still
     *  be found. But if this is processing a finally block, the scanning for a matching handler will be forced
     *  to bubble up.
     */
    fp->pc = &fp->function.body.code.byteCode[ex->handlerStart];

    if (flags & EJS_EX_CATCH) {
        ejs->exceptionArg = ejs->exception;
        fp->function.inCatch = 1;

    } else {
        mprAssert(flags & EJS_EX_FINALLY);
        /*
         *  Mask the exception while processing the finally block
         */
        block->prev->prevException = ejs->exception;
        ejs->attention = 1;
        fp->function.inCatch = 0;
    }
    ejs->exception = 0;
    fp->function.inException = 1;
}


typedef struct OperMap {
    int         opcode;
    cchar       *name;
} OperMap;

static OperMap operMap[] = {
        { EJS_OP_MUL,           "*"     },
        { EJS_OP_DIV,           "/"     },
        { EJS_OP_REM,           "%"     },
        { EJS_OP_COMPARE_LT,    "<"     },
        { EJS_OP_COMPARE_GT,    ">"     },
        { EJS_OP_COMPARE_LE,    "<="    },
        { EJS_OP_COMPARE_GE,    ">="    },
        { 0,                    0       },
};


static int lookupOverloadedOperator(Ejs *ejs, EjsOpCode opcode, EjsVar *lhs)
{
    EjsName     qname;
    int         i;

    for (i = 0; operMap[i].opcode; i++) {
        if (operMap[i].opcode == opcode) {
            ejsName(&qname, "", operMap[i].name);
            break;
        }
    }
    return ejsLookupProperty(ejs, (EjsVar*) lhs->type, &qname);
}


/*
 *  Evaluate a binary expression.
 *  OPT -- simplify and move back inline into eval loop.
 */
static EjsVar *evalBinaryExpr(Ejs *ejs, EjsVar *lhs, EjsOpCode opcode, EjsVar *rhs)
{
    EjsVar      *result;
    int         slotNum;

    if (lhs == 0) {
        lhs = ejs->undefinedValue;
    }
    if (rhs == 0) {
        rhs = ejs->undefinedValue;
    }

    result = ejsInvokeOperator(ejs, lhs, opcode, rhs);

    if (result == 0 && ejs->exception == 0) {
        slotNum = lookupOverloadedOperator(ejs, opcode, lhs);
        if (slotNum >= 0) {
            result = ejsRunFunctionBySlot(ejs, lhs, slotNum, 1, &rhs);
        }
    }
    mprAssert(ejs->exception || result);
    return result;
}


/*
 *  Evaluate a unary expression.
 *  OPT -- once simplified, move back inline into eval loop.
 */
static EjsVar *evalUnaryExpr(Ejs *ejs, EjsVar *lhs, EjsOpCode opcode)
{
    EjsVar  *result;
    result = ejsInvokeOperator(ejs, lhs, opcode, 0);
    mprAssert(ejs->exception || result);
    return result;
}


int ejsInitStack(Ejs *ejs)
{
    EjsState    *state;

    state = ejs->state = ejs->masterState = mprAllocObjZeroed(ejs, EjsState);

    /*
     *  Allocate the stack
     */
    state->stackSize = MPR_PAGE_ALIGN(EJS_STACK_MAX, mprGetPageSize(ejs));

    /*
     *  This will allocate memory virtually for systems with virutal memory. Otherwise, it will just use malloc.
     */
    state->stackBase = mprMapAlloc(ejs, state->stackSize, MPR_MAP_READ | MPR_MAP_WRITE);
    if (state->stackBase == 0) {
        mprSetAllocError(ejs);
        return EJS_ERR;
    }
    state->stack = &state->stackBase[-1];
    return 0;
}


/*
 *  Exit the script
 */
void ejsExit(Ejs *ejs, int status)
{
    ejs->flags |= EJS_FLAG_EXIT;
}


static EjsName getNameArg(EjsFrame *fp)
{
    EjsName     qname;

    qname.name = getStringArg(fp);
    qname.space = getStringArg(fp);
    return qname;
}


/*
 *  Get an interned string. String constants are stored as token offsets into the constant pool. The pool
 *  contains null terminated UTF-8 strings.
 */
static char *getStringArg(EjsFrame *fp)
{
    int     number;

    number = (int) ejsDecodeNum(&fp->pc);

    mprAssert(fp->function.body.code.constants);
    return &fp->function.body.code.constants->pool[number];
}


static EjsVar *getGlobalArg(Ejs *ejs, EjsFrame *fp)
{
    EjsVar      *vp;
    EjsName     qname;
    int         t, slotNum;

    t = (int) ejsDecodeNum(&fp->pc);
    if (t < 0) {
        return 0;
    }

    slotNum = -1;
    qname.name = 0;
    qname.space = 0;
    vp = 0;

    /*
     *  OPT. Could this encoding be optimized?
     */
    switch (t & EJS_ENCODE_GLOBAL_MASK) {
    default:
        mprAssert(0);
        return 0;

    case EJS_ENCODE_GLOBAL_NOREF:
        return 0;

    case EJS_ENCODE_GLOBAL_SLOT:
        slotNum = t >> 2;
        if (0 <= slotNum && slotNum < ejsGetPropertyCount(ejs, ejs->global)) {
            vp = ejsGetProperty(ejs, ejs->global, slotNum);
        }
        break;

    case EJS_ENCODE_GLOBAL_NAME:
        qname.name = &fp->function.body.code.constants->pool[t >> 2];
        if (qname.name == 0) {
            mprAssert(0);
            return 0;
        }
        qname.space = getStringArg(fp);
        if (qname.space == 0) {
            return 0;
        }
        if (qname.name) {
            vp = ejsGetPropertyByName(ejs, ejs->global, &qname);
        }
        break;
    }
    return vp;
}


/*
 *  Get a function reference. This handles getters and binds the "this" value for method extraction.
 */
static void handleGetters(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj)
{
    if (fun->getter) {
        ejs->state->stack--;
        callFunction(ejs, fun, thisObj, 0, 0);
        if (ejsIsNativeFunction(fun)) {
            pushOutside(ejs, ejs->result);
        }

    } else if (!fun->thisObj && thisObj) {
        /*
         *  Function extraction. Bind the "thisObj" into a clone of the function
         */
        fun = ejsCopyFunction(ejs, fun);
        fun->thisObj = thisObj;
        ejs->state->stack--;
        pushOutside(ejs, fun);
    }
}


/*
 *  Handle setters. Setters, if present, are chained off the getter.
 */
static MPR_INLINE void handleSetters(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, EjsVar *value)
{
    /*
     *  Step from the getter to retrieve the setter
     */
    fun = (EjsFunction*) ejsGetProperty(ejs, fun->owner, fun->nextSlot);
    pushOutside(ejs, value);
    callFunction(ejs, fun, thisObj, 1, 0);
}


static void callProperty(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, int argc, int stackAdjust)
{
    if (ejsIsFunction(fun) && fun->getter) {
        fun = (EjsFunction*) ejsRunFunction(ejs, fun, thisObj, 0, NULL);
    }
    callFunction(ejs, fun, thisObj, argc, stackAdjust);
}


/*
 *  Call a function aka pushFunctionFrame. Supports both native and scripted functions. If native, the function is fully 
 *  invoked here. If scripted, a new frame is created and the pc adjusted to point to the new function.
 */
static void callFunction(Ejs *ejs, EjsFunction *fun, EjsVar *thisObj, int argc, int stackAdjust)
{
    EjsState        *state;
    EjsFrame        *fp;
    EjsName         qname;
    EjsVar          **argv;

    mprAssert(fun);

    state = ejs->state;

    if (unlikely(!ejsIsFunction(fun))) {
        callConstructor(ejs, fun, argc, stackAdjust);
        return;
    }
    if (thisObj == 0) {
        if ((thisObj = fun->thisObj) == 0) {
            thisObj = state->fp->function.thisObj;
        } 
    } 
    if (fun->staticMethod && !ejsIsType(thisObj)) {
        /*
         *  Calling a static method via an instance object
         */
        thisObj = getStaticThis(ejs, thisObj->type, fun->slotNum);
    }

    /*
     *  Validate the args. Cast to the right type, handle rest args and return with argc adjusted.
     */
    argv = NULL;
    if (argc > 0 || fun->numArgs || fun->rest) {
        argv = &(state->stack[1 - argc]);
        if ((argc = validateArgs(ejs, fun, argc, argv)) < 0) {
            return;
        }
    }

    if (ejsIsNativeFunction(fun)) {
        if (fun->body.proc == 0) {
            qname = ejsGetPropertyName(ejs, fun->owner, fun->slotNum);
            ejsThrowInternalError(ejs, "Native function \"%s\" is not defined", qname.name);
            return;
        }
        ejs->result = (fun->body.proc)(ejs, thisObj, argc, argv);
        if (ejs->result == 0) {
            ejs->result = ejs->nullValue;
        }
        mprAssert(ejs->exception || ejs->result);
        state->stack -= (argc + stackAdjust);

    } else {
        fp = ejsCreateFrame(ejs, fun);
        fp->function.block.prev = state->bp;
        fp->function.thisObj = thisObj;
        fp->caller = state->fp;
        fp->stackBase = state->stack;
        fp->stackReturn = state->stack - argc - stackAdjust;
        if (argc > 0) {
            fp->argc = argc;
            if ((uint) argc < (fun->numArgs - fun->numDefault) || (uint) argc > fun->numArgs) {
                ejsThrowArgError(ejs, "Incorrect number of arguments");
                return;
            }
            memcpy(fp->function.block.obj.slots, argv, argc * sizeof(EjsVar*));
        }
        state->fp = fp;
        state->bp = (EjsBlock*) fp;
    }
}


static void throwNull(Ejs *ejs)
{
    ejsThrowReferenceError(ejs, "Object reference is null");
}


/*
 *  Object can be an instance or a type. If an instance, then step to the immediate base type to begin the count.
 */
static EjsVar *getNthBase(Ejs *ejs, EjsVar *obj, int nthBase)
{
    EjsType     *type;

    if (obj) {
        if (ejsIsType(obj) || obj == ejs->global) {
            type = (EjsType*) obj;
        } else {
            type = obj->type;
            nthBase--;
        }
        for (; type && nthBase > 0; type = type->baseType) {
            nthBase--;
        }
        if (nthBase > 0) {
            ejsThrowReferenceError(ejs, "Can't find correct base class");
            return 0;
        }
        obj = (EjsVar*) type;
    }
    return obj;
}


static EjsVar *getNthBaseFromBottom(Ejs *ejs, EjsVar *obj, int nthBase)
{
    EjsType     *type, *tp;
    int         count;

    if (obj) {
        if (ejsIsType(obj) || obj == ejs->global) {
            type = (EjsType*) obj;
        } else {
            type = obj->type;
        }
        for (count = 0, tp = type->baseType; tp; tp = tp->baseType) {
            count++;
        }
        nthBase = count - nthBase;
        for (; type && nthBase > 0; type = type->baseType) {
            nthBase--;
        }
        obj = (EjsVar*) type;
    }
    return obj;
}


static MPR_INLINE EjsVar *getNthBlock(Ejs *ejs, int nth)
{
    EjsBlock    *block;

    mprAssert(ejs);
    mprAssert(nth >= 0);

    for (block = ejs->state->bp; block && --nth >= 0; ) {
        if (block->obj.var.hidden) nth++;
        block = block->scopeChain;
    }
    return (EjsVar*) block;
}


/*
 *  Enter a mesage into the log file
 */
void ejsLog(Ejs *ejs, const char *fmt, ...)
{
    va_list     args;
    char        buf[MPR_MAX_LOG_STRING];

    va_start(args, fmt);
    mprVsprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    mprLog(ejs, 0, "%s", buf);
}


void ejsShowStack(Ejs *ejs, EjsFunction *fp)
{
    char    *stack;
    
    stack = ejsFormatStack(ejs, NULL);
    mprLog(ejs, 7, "Stack\n%s", stack);
    mprFree(stack);
}


#if BLD_DEBUG

int ejsOpCount = 0;
static EjsOpCode traceCode(Ejs *ejs, EjsOpCode opcode)
{
    EjsFrame        *fp;
    EjsState        *state;
    EjsOptable      *optable;
    int             len;
    static int      once = 0;
    static int      stop = 1;

    state = ejs->state;
    fp = state->fp;
    opcount[opcode]++;

    if (ejs->initialized && opcode != EJS_OP_DEBUG) {
        //  OPT - should strip '\n' in the compiler
        if (fp->currentLine) {
            len = (int) strlen(fp->currentLine) - 1;
            if (fp->currentLine[len] == '\n') {
                ((char*) fp->currentLine)[len] = '\0';
            }
        }
        optable = ejsGetOptable(ejs);
        mprLog(ejs, 7, "%04d: [%d] %02x: %-35s # %s:%d %s",
            (uint) (fp->pc - fp->function.body.code.byteCode) - 1, (int) (state->stack - fp->stackReturn),
            (uchar) opcode, optable[opcode].name, fp->filename, fp->lineNumber, fp->currentLine);
        if (stop && once++ == 0) {
            mprSleep(ejs, 0);
        }
    }
    ejsOpCount++;
    return opcode;
}
#endif

#undef top

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsInterp.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsList.c"
 */
/************************************************************************/

/**
 *  ejsList.c - Simple static list type.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int growList(MprCtx ctx, EjsList *lp, int incr);

#define CAPACITY(lp) (mprGetBlockSize(lp) / sizeof(void*))

//  OPT - inline some of these functions as macros

/*
 *  Initialize a list which may not be a memory context.
 */
void ejsInitList(EjsList *lp)
{
    lp->length = 0;
    lp->maxSize = MAXINT;
    lp->items = 0;
}


/*
 *  Define the list maximum size. If the list has not yet been written to, the initialSize will be observed.
 */
int ejsSetListLimits(MprCtx ctx, EjsList *lp, int initialSize, int maxSize)
{
    int         size;

    if (initialSize <= 0) {
        initialSize = MPR_LIST_INCR;
    }
    if (maxSize <= 0) {
        maxSize = MAXINT;
    }
    size = initialSize * sizeof(void*);

    if (lp->items == 0) {
        lp->items = (void**) mprAllocZeroed(ctx, size);
        if (lp->items == 0) {
            mprFree(lp);
            return MPR_ERR_NO_MEMORY;
        }
    }
    lp->maxSize = maxSize;
    return 0;
}


/*
 *  Add an item to the list and return the item index.
 */
int ejsAddItem(MprCtx ctx, EjsList *lp, cvoid *item)
{
    int     index, capacity;

    mprAssert(lp);
    mprAssert(lp->length >= 0);

    capacity = CAPACITY(lp->items);
    mprAssert(capacity >= 0);

    if (lp->items == 0 || lp->length >= capacity) {
        if (growList(ctx, lp, 1) < 0) {
            return MPR_ERR_TOO_MANY;
        }
    }
    index = lp->length++;
    lp->items[index] = (void*) item;
    return index;
}


int ejsAddItemToSharedList(MprCtx ctx, EjsList *lp, cvoid *item)
{
    EjsList     tmp;

    if (lp->items == NULL || mprGetParent(lp->items) != ctx) {
        tmp = *lp;
        lp->items = 0;
        lp->length = 0;
        if (ejsCopyList(ctx, lp, &tmp) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    return ejsAddItem(ctx, lp, item);
}


EjsList *ejsAppendList(MprCtx ctx, EjsList *list, EjsList *add)
{
    void        *item;
    int         next;

    mprAssert(list);
    mprAssert(list != add);

    for (next = 0; ((item = ejsGetNextItem(add, &next)) != 0); ) {
        if (ejsAddItem(ctx, list, item) < 0) {
            mprFree(list);
            return 0;
        }
    }
    return list;
}


void ejsClearList(EjsList *lp)
{
    int     i;

    mprAssert(lp);

    for (i = 0; i < lp->length; i++) {
        lp->items[i] = 0;
    }
    lp->length = 0;
}


int ejsCopyList(MprCtx ctx, EjsList *dest, EjsList *src)
{
    void        *item;
    int         next, capacity;

    ejsClearList(dest);

    capacity = CAPACITY(src->items);
    if (ejsSetListLimits(ctx, dest, capacity, src->maxSize) < 0) {
        return MPR_ERR_NO_MEMORY;
    }
    for (next = 0; (item = ejsGetNextItem(src, &next)) != 0; ) {
        if (ejsAddItem(ctx, dest, item) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    return 0;
}


void *ejsGetItem(EjsList *lp, int index)
{
    mprAssert(lp);

    if (index < 0 || index >= lp->length) {
        return 0;
    }
    return lp->items[index];
}


void *ejsGetLastItem(EjsList *lp)
{
    mprAssert(lp);

    if (lp == 0) {
        return 0;
    }

    if (lp->length == 0) {
        return 0;
    }
    return lp->items[lp->length - 1];
}


void *ejsGetNextItem(EjsList *lp, int *next)
{
    void    *item;
    int     index;

    mprAssert(next);
    mprAssert(*next >= 0);

    if (lp == 0) {
        return 0;
    }
    index = *next;
    if (index < lp->length) {
        item = lp->items[index];
        *next = ++index;
        return item;
    }
    return 0;
}


int ejsGetListCount(EjsList *lp)
{
    if (lp == 0) {
        return 0;
    }

    return lp->length;
}


void *ejsGetPrevItem(EjsList *lp, int *next)
{
    int     index;

    mprAssert(next);

    if (lp == 0) {
        return 0;
    }
    if (*next < 0) {
        *next = lp->length;
    }
    index = *next;
    if (--index < lp->length && index >= 0) {
        *next = index;
        return lp->items[index];
    }
    return 0;
}


int ejsRemoveLastItem(EjsList *lp)
{
    mprAssert(lp);
    mprAssert(lp->length > 0);

    if (lp->length <= 0) {
        return MPR_ERR_NOT_FOUND;
    }
    return ejsRemoveItemAtPos(lp, lp->length - 1);
}


/*
 *  Remove an index from the list. Return the index where the item resided.
 */
int ejsRemoveItemAtPos(EjsList *lp, int index)
{
    void    **items;
    int     i;

    mprAssert(lp);
    mprAssert(index >= 0);
    mprAssert(lp->length > 0);

    if (index < 0 || index >= lp->length) {
        return MPR_ERR_NOT_FOUND;
    }
    items = lp->items;
    for (i = index; i < (lp->length - 1); i++) {
        items[i] = items[i + 1];
    }
    lp->length--;
    lp->items[lp->length] = 0;
    return index;
}


/*
 *  Grow the list by the requried increment
 */
static int growList(MprCtx ctx, EjsList *lp, int incr)
{
    int     len, memsize, capacity;

    /*
     *  Need to grow the list
     */
    capacity = CAPACITY(lp->items);
    mprAssert(capacity >= 0);
    
    if (capacity >= lp->maxSize) {
        if (lp->maxSize == 0) {
            lp->maxSize = INT_MAX;
        } else {
            mprAssert(capacity < lp->maxSize);
            return MPR_ERR_TOO_MANY;
        }
    }

    /*
     *  If growing by 1, then use the default increment which exponentially grows.
     *  Otherwise, assume the caller knows exactly how the list needs to grow.
     */
    if (incr <= 1) {
        len = MPR_LIST_INCR + capacity + capacity;
    } else {
        len = capacity + incr;
    }
    memsize = len * sizeof(void*);

    /*
     *  Grow the list of items. Use the existing context for lp->items if it already exists. Otherwise use the list as the
     *  memory context owner.
     */
    lp->items = (void**) mprRealloc(ctx, lp->items, memsize);

    /*
     *  Zero the new portion (required for no-compact lists)
     */
    memset(&lp->items[capacity], 0, sizeof(void*) * (len - capacity));
    return 0;
}


int ejsLookupItem(EjsList *lp, cvoid *item)
{
    int     i;

    mprAssert(lp);
    
    for (i = 0; i < lp->length; i++) {
        if (lp->items[i] == item) {
            return i;
        }
    }
    return MPR_ERR_NOT_FOUND;
}


#if KEEP
/*
 *  Change the item in the list at index. Return the old item.
 */
void *ejsSetItem(MprCtx ctx, EjsList *lp, int index, cvoid *item)
{
    void    *old;

    mprAssert(lp);
    mprAssert(lp->length >= 0);

    if (index >= lp->length) {
        lp->length = index + 1;
    }
    capacity = CAPACITY(lp->items);
    if (lp->length > capacity) {
        if (growList(ctx, lp, lp->length - capacity) < 0) {
            return 0;
        }
    }
    old = lp->items[index];
    lp->items[index] = (void*) item;
    return old;
}


/*
 *  Insert an item to the list at a specified position. We insert before "index".
 */
int ejsInsertItemAtPos(MprCtx ctx, EjsList *lp, int index, cvoid *item)
{
    void    **items;
    int     i;

    mprAssert(lp);
    mprAssert(lp->length >= 0);

    if (lp->length >= CAPACITY(lp->items)) {
        if (growList(ctx, lp, 1) < 0) {
            return MPR_ERR_TOO_MANY;
        }
    }

    /*
     *  Copy up items to make room to insert
     */
    items = lp->items;
    for (i = lp->length; i > index; i--) {
        items[i] = items[i - 1];
    }
    lp->items[index] = (void*) item;
    lp->length++;
    return index;
}


/*
 *  Remove an item from the list. Return the index where the item resided.
 */
int ejsRemoveItem(MprCtx ctx, EjsList *lp, void *item)
{
    int     index;

    mprAssert(lp);
    mprAssert(lp->length > 0);

    index = ejsLookupItem(lp, item);
    if (index < 0) {
        return index;
    }
    return ejsRemoveItemAtPos(ctx, lp, index);
}


/*
 *  Remove a set of items. Return 0 if successful.
 */
int ejsRemoveRangeOfItems(EjsList *lp, int start, int end)
{
    void    **items;
    int     i, count, capacity;

    mprAssert(lp);
    mprAssert(lp->length > 0);
    mprAssert(start > end);

    if (start < 0 || start >= lp->length) {
        return MPR_ERR_NOT_FOUND;
    }
    if (end < 0 || end >= lp->length) {
        return MPR_ERR_NOT_FOUND;
    }
    if (start > end) {
        return MPR_ERR_BAD_ARGS;
    }

    /*
     *  Copy down to coejsess
     */
    items = lp->items;
    count = end - start;
    for (i = start; i < (lp->length - count); i++) {
        items[i] = items[i + count];
    }
    lp->length -= count;
    capacity = CAPACITY(lp->items);
    for (i = lp->length; i < capacity; i++) {
        items[i] = 0;
    }
    return 0;
}


void *ejsGetFirstItem(EjsList *lp)
{
    mprAssert(lp);

    if (lp == 0) {
        return 0;
    }
    if (lp->length == 0) {
        return 0;
    }
    return lp->items[0];
}


int ejsGetListCapacity(EjsList *lp)
{
    mprAssert(lp);

    if (lp == 0) {
        return 0;
    }
    return CAPACITY(lp->items);
}

#endif

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsList.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsLoader.c"
 */
/************************************************************************/

/**
 *  ejsLoader.c - Ejscript module file file loader
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int  addFixup(Ejs *ejs, int kind, EjsVar *target, int slotNum, EjsTypeFixup *fixup);
static EjsTypeFixup *createFixup(Ejs *ejs, EjsName *qname, int slotNum);
static int  fixupTypes(Ejs *ejs, MprList *list);
static int  initializeModule(Ejs *ejs, EjsModule *mp, cchar *path);
static int  loadBlockSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static int  loadClassSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static int  loadDependencySection(Ejs *ejs, MprFile *file, EjsModule *mp);
static int  loadEndBlockSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static int  loadEndFunctionSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static int  loadEndClassSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static int  loadEndModuleSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static int  loadExceptionSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static int  loadFunctionSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static EjsModule *loadModuleSection(Ejs *ejs, MprFile *file, EjsModuleHdr *hdr, int *created, int flags);
static int  loadSections(Ejs *ejs, MprFile *file, EjsModuleHdr *hdr, int flags);
static int  loadPropertySection(Ejs *ejs, MprFile *file, EjsModule *mp, int sectionType);
static int  loadScriptModule(Ejs *ejs, MprFile *file, cchar *path, int flags);
static char *makeModuleName(MprCtx ctx, cchar *name);
static int  readNumber(Ejs *ejs, MprFile *file, int *number);
static int  readWord(Ejs *ejs, MprFile *file, int *number);
static char *search(Ejs *ejs, char *filename, int minVersion, int maxVersion);
static double swapDoubleWord(Ejs *ejs, double a);
static int  swapWord(Ejs *ejs, int word);
static char *tokenToString(EjsModule *mp, int   token);

#if !BLD_FEATURE_STATIC
static int  loadNativeLibrary(Ejs *ejs, EjsModule *mp, cchar *path);
#endif

#if BLD_FEATURE_EJS_DOC
static int  loadDocSection(Ejs *ejs, MprFile *file, EjsModule *mp);
static void setDoc(Ejs *ejs, EjsModule *mp, EjsVar *block, int slotNum);
#endif

/**
 *  Load a module file and return a list of the loaded modules. This is used to load scripted module files with
 *  optional native (shared / DLL) implementations. If loading a scripted module that has native declarations, a
 *  search for the corresponding native DLL will be performed and both scripted and native module files will be loaded.
 *  NOTE: this may recursively call itself as it loads dependent modules.
 *
 *  @param ejs Ejs handle
 *  @param nameArg Module name to load. May be "." separated path. May include or omit the ".mod" extension.
 *  @param minVersion Minimum acceptable version (inclusive). Set to zero for unversioned.
 *  @param maxVersion Maximum acceptable version (inclusive). Set to -1 for all versions.
 *  @param flags Reserved. Must be set to zero.
 *  @param modulesArg List of modules loaded. Will only return a list if successful and doing a top level load. 
 *      When ejsLoadModule is called to load dependant modules, not list of modules will be returned.
 *      The final list of modules aggregates all modules loaded including those from dependant modules.
 *  @return Returns the last loaded module.
 */
int ejsLoadModule(Ejs *ejs, cchar *nameArg, int minVersion, int maxVersion, int flags, MprList **modulesArg)
{
    MprFile         *file;
    MprList         *modules;
    MprCtx          ctx;
    EjsLoadState    *ls;
    EjsModule       *mp;
    char            *cp, *path, *name, *base;
    int             next, status;

    mprAssert(nameArg && *nameArg);

    path = 0;
    status = 0;
    modules = 0;
    ctx = mprAlloc(ejs, 0);

    name = makeModuleName(ctx, nameArg);
    base = mprGetPathBase(ctx, nameArg);
    if ((cp = strrchr(base, '.')) != 0 && strcmp(cp, EJS_MODULE_EXT) == 0) {
        *cp = '\0';
    }

    if ((mp = ejsLookupModule(ejs, base, minVersion, maxVersion)) != 0) {
        if (mp->compiling && strcmp(name, EJS_DEFAULT_MODULE) != 0) {
            ejsThrowStateError(ejs, "Attempt to load module \"%s\" that is currently being compiled.", name);
            mprFree(ctx);
            return MPR_ERR_ALREADY_EXISTS;
        }
        if (modulesArg && ejs->loadState == 0) {
            modules = mprCreateList(ejs);
            mprAddItem(modules, mp);
        }
        mprFree(ctx);
        return 0;
    }
    if ((path = search(ejs, name, minVersion, maxVersion)) == 0) {
        mprFree(ctx);
        return MPR_ERR_CANT_ACCESS;
    }
    mprLog(ejs, 3, "Loading module %s", path);

    if ((file = mprOpen(ctx, path, O_RDONLY | O_BINARY, 0666)) == 0) {
        ejsThrowIOError(ejs, "Can't open module file %s", path);
        mprFree(path);
        mprFree(ctx);
        return MPR_ERR_CANT_OPEN;
    }
    mprEnableFileBuffering(file, 0, 0);

    if (ejs->loadState == 0) {
        ls = ejs->loadState = mprAllocObjZeroed(ejs, EjsLoadState);
        ls->typeFixups = mprCreateList(ls);
        modules = ls->modules = mprCreateList(ls);

        status = loadScriptModule(ejs, file, path, flags);
        ejs->loadState = 0;

        /*
         *  Delay doing fixups and running initializers until all modules are loaded. Then do all fixups first prior to
         *  running the initializers. This solves the forward type reference problem.
         */
        if (status == 0 && fixupTypes(ejs, ls->typeFixups) == 0) {
            for (next = 0; (mp = mprGetNextItem(modules, &next)) != 0; ) {
                if ((status = initializeModule(ejs, mp, path)) < 0) {
                    break;
                }
            }
        }
        if (modulesArg) {
            *modulesArg = modules;
            mprStealBlock(ejs, modules);
            mprAddItem(modules, mp);
        }
        mprFree(ls);

    } else {
        status = loadScriptModule(ejs, file, path, flags);
    }

    mprFree(path);
    mprFree(ctx);
    return status;
}


static int initializeModule(Ejs *ejs, EjsModule *mp, cchar *path)
{
    EjsNativeCallback   moduleCallback;

    if (mp->hasNative && !mp->configured) {
        /*
         *  See if a native module initialization routine has been registered. If so, use that. Otherwise, look
         *  for a backing DSO.
         */
        if ((moduleCallback = (EjsNativeCallback) mprLookupHash(ejs->service->nativeModules, mp->name)) != 0) {
            if ((moduleCallback)(ejs, mp, path) < 0) {
                return MPR_ERR_CANT_INITIALIZE;
            }
            
        } else {
#if !BLD_FEATURE_STATIC
            int     rc;
            char *dir = mprGetPathDir(ejs, path);
            rc = loadNativeLibrary(ejs, mp, dir);
            mprFree(dir);
            if (rc < 0) {
                if (ejs->exception == 0) {
                    ejsThrowIOError(ejs, "Can't load the native module file \"%s\"", path);
                }
                return MPR_ERR_CANT_INITIALIZE;
            }
#endif
        }
    }
    mp->configured = 1;
    if (!(ejs->flags & EJS_FLAG_NO_EXE) && ejsRunInitializer(ejs, mp) == 0) {
        return MPR_ERR_CANT_INITIALIZE;
    }
    return 0;
}


/*
 *  Strip existing extension and replace with .mod 
 */
static char *makeModuleName(MprCtx ctx, cchar *nameArg)
{
    char        *name, *cp, *filename;

    name = mprAlloc(ctx, strlen(nameArg) + strlen(EJS_MODULE_EXT) + 1);
    strcpy(name, nameArg);
    if ((cp = strrchr(name, '.')) != 0 && strcmp(cp, EJS_MODULE_EXT) == 0) {
        *cp = '\0';
    }
    filename = mprStrcat(ctx, -1, name, EJS_MODULE_EXT, NULL);
    mprFree(name);
    return filename;
}



static char *search(Ejs *ejs, char *filename, int minVersion, int maxVersion) 
{
    char        *path;

    if ((path = ejsSearchForModule(ejs, filename, minVersion, maxVersion)) == 0) {
        mprLog(ejs, 2, "Can't find module file \"%s\" in search path \"%s\"", filename, 
            ejs->ejsPath ? ejs->ejsPath : "");
        ejsThrowReferenceError(ejs,  "Can't find module file \"%s\", min version %d.%d.%d, max version %d.%d.%d", filename, 
            EJS_MAJOR(minVersion), EJS_MINOR(minVersion), EJS_PATCH(minVersion),
            EJS_MAJOR(maxVersion), EJS_MINOR(maxVersion), EJS_PATCH(maxVersion));
        return 0;
    }
    return path;
}


/*
 *  Load the sections: classes, properties and functions. Return the first module loaded in pup.
 */
static int loadSections(Ejs *ejs, MprFile *file, EjsModuleHdr *hdr, int flags)
{
    EjsModule   *mp, *firstModule;
    int         rc, sectionType, created;

    created = 0;

    firstModule = mp = 0;

    while ((sectionType = mprGetc(file)) >= 0) {

        if (sectionType < 0 || sectionType >= EJS_SECT_MAX) {
            mprError(ejs, "Bad section type %d in %s", sectionType, mp->name);
            return EJS_ERR;
        }
        mprLog(ejs, 9, "Load section type %d", sectionType);

        rc = 0;
        switch (sectionType) {

        case EJS_SECT_BLOCK:
            rc = loadBlockSection(ejs, file, mp);
            break;

        case EJS_SECT_BLOCK_END:
            rc = loadEndBlockSection(ejs, file, mp);
            break;

        case EJS_SECT_CLASS:
            rc = loadClassSection(ejs, file, mp);
            break;

        case EJS_SECT_CLASS_END:
            rc = loadEndClassSection(ejs, file, mp);
            break;

        case EJS_SECT_DEPENDENCY:
            rc = loadDependencySection(ejs, file, mp);
            break;

        case EJS_SECT_EXCEPTION:
            rc = loadExceptionSection(ejs, file, mp);
            break;

        case EJS_SECT_FUNCTION:
            rc = loadFunctionSection(ejs, file, mp);
            break;

        case EJS_SECT_FUNCTION_END:
            rc = loadEndFunctionSection(ejs, file, mp);
            break;

        case EJS_SECT_MODULE:
            mp = loadModuleSection(ejs, file, hdr, &created, flags);
            if (mp == 0) {
                return 0;
            }
            if (firstModule == 0) {
                firstModule = mp;
            }
            ejsAddModule(ejs, mp);
            mprAddItem(ejs->loadState->modules, mp);
            break;

        case EJS_SECT_MODULE_END:
            rc = loadEndModuleSection(ejs, file, mp);
            break;

        case EJS_SECT_PROPERTY:
            rc = loadPropertySection(ejs, file, mp, sectionType);
            break;

#if BLD_FEATURE_EJS_DOC
        case EJS_SECT_DOC:
            rc = loadDocSection(ejs, file, mp);
            break;
#endif

        default:
            mprAssert(0);
            return EJS_ERR;
        }

        if (rc < 0) {
            if (mp && mp->name && created) {
                ejsRemoveModule(ejs, mp);
                mprRemoveItem(ejs->loadState->modules, mp);
                mprFree(mp);
            }
            return rc;
        }
    }
    return 0;
}


/*
 *  Load a module section and constant pool.
 */
static EjsModule *loadModuleSection(Ejs *ejs, MprFile *file, EjsModuleHdr *hdr, int *created, int flags)
{
    EjsModule   *mp;
    char        *pool, *name;
    int         rc, version, checksum, poolSize, nameToken;

    mprAssert(created);

    *created = 0;
    checksum = 0;

    /*
     *  We don't have the constant pool yet so we cant resolve the name yet.
     */
    rc = 0;
    rc += readNumber(ejs, file, &nameToken);
    rc += readNumber(ejs, file, &version);
    rc += readWord(ejs, file, &checksum);
    rc += readNumber(ejs, file, &poolSize);
    if (rc < 0 || poolSize <= 0 || poolSize > EJS_MAX_POOL) {
        return 0;
    }

    /*
     *  Read the string constant pool. The pool calls Steal when the module is created.
     */
    pool = (char*) mprAlloc(file, poolSize);
    if (pool == 0) {
        return 0;
    }
    if (mprRead(file, pool, poolSize) != poolSize) {
        mprFree(pool);
        return 0;
    }

    /*
     *  Convert module token into a name
     */
    if (nameToken < 0 || nameToken >= poolSize) {
        mprAssert(0);
        return 0;
    }
    name = &pool[nameToken];
    if (name == 0) {
        mprAssert(name);
        mprFree(pool);
        return 0;
    }

    mp = ejsCreateModule(ejs, name, version);
    if (mp == 0) {
        mprFree(pool);
        return 0;
    }
    ejsSetModuleConstants(ejs, mp, pool, poolSize);
    mp->scopeChain = ejs->globalBlock;
    mp->checksum = checksum;
    *created = 1;

    if (strcmp(name, EJS_DEFAULT_MODULE) != 0) {
        /*
         *  Signify that loading the module has begun. We allow multiple loads into the default module.
         */
        mp->loaded = 1;
        mp->constants->locked = 1;
    }
    mp->file = file;
    mp->flags = flags;
    mp->firstGlobalSlot = ejsGetPropertyCount(ejs, ejs->global);

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_MODULE, mp);
    }
    mprLog(ejs, 9, "Load module section %s", name);
    return mp;
}


static int loadEndModuleSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    mprLog(ejs, 9, "End module section %s", mp->name);

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_MODULE_END, mp);
    }
    return 0;
}


static int loadDependencySection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    EjsModule   *module;
    void        *saveCallback;
    char        *name;
    int         rc, next, minVersion, maxVersion, checksum, nextModule;

    mprAssert(ejs);
    mprAssert(file);
    mprAssert(mp);

    name = ejsModuleReadString(ejs, mp);
    ejsModuleReadNumber(ejs, mp, &checksum);
    ejsModuleReadNumber(ejs, mp, &minVersion);
    ejsModuleReadNumber(ejs, mp, &maxVersion);
    if (mp->hasError) {
        return MPR_ERR_CANT_READ;
    }
    
    saveCallback = ejs->loaderCallback;
    ejs->loaderCallback = NULL;
    nextModule = mprGetListCount(ejs->loadState->modules);

    mprLog(ejs, 5, "    Load dependency section %s", name);
    rc = ejsLoadModule(ejs, name, minVersion, maxVersion, mp->flags, NULL);

    ejs->loaderCallback = saveCallback;
    if (rc < 0) {
        return rc;
    }
    if ((module = ejsLookupModule(ejs, name, minVersion, maxVersion)) != 0) {
        if (checksum != module->checksum) {
            ejsThrowIOError(ejs, "Can't load module %s.\n"
                "It was compiled using a different version of module %s.", 
                mp->name, name);
            return MPR_ERR_BAD_STATE;
        }
    }

    if (mp->dependencies == 0) {
        mp->dependencies = mprCreateList(mp);
    }
    for (next = nextModule; (module = mprGetNextItem(ejs->loadState->modules, &next)) != 0; ) {
        mprAddItem(mp->dependencies, module);
        if (ejs->loaderCallback) {
            (ejs->loaderCallback)(ejs, EJS_SECT_DEPENDENCY, mp, module);
        }
    }
    /*
     *  Note the first free global slot after loading the module.
     */
    mp->firstGlobalSlot = ejsGetPropertyCount(ejs, ejs->global);
    return 0;
}


static int loadBlockSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    EjsBlock    *block;
    EjsVar      *owner;
    EjsName     qname;
    int         slotNum, numSlot;

    qname.space = EJS_BLOCK_NAMESPACE;
    qname.name = ejsModuleReadString(ejs, mp);
    ejsModuleReadNumber(ejs, mp, &slotNum);
    ejsModuleReadNumber(ejs, mp, &numSlot);

    if (mp->hasError) {
        return MPR_ERR_CANT_READ;
    }
    
    block = ejsCreateBlock(ejs, numSlot);
    ejsSetDebugName(block, qname.name);
    owner = (EjsVar*) mp->scopeChain;

    if (ejsLookupProperty(ejs, owner, &qname) >= 0) {
        ejsThrowReferenceError(ejs, "Block \"%s\" already loaded", qname.name);
        return MPR_ERR_CANT_CREATE;
    }

    slotNum = ejsDefineProperty(ejs, owner, slotNum, &qname, ejs->blockType, 0, (EjsVar*) block);
    if (slotNum < 0) {
        return MPR_ERR_CANT_WRITE;
    }

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_BLOCK, mp, owner, slotNum, qname.name, numSlot, block);
    }
    block->scopeChain = mp->scopeChain;
    mp->scopeChain = block;
    return 0;
}


static int loadEndBlockSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    mprLog(ejs, 9, "    End block section %s", mp->name);

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_BLOCK_END, mp);
    }
    mp->scopeChain = mp->scopeChain->scopeChain;
    return 0;
}


static int loadClassSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    EjsType         *type, *baseType, *iface, *nativeType;
    EjsTypeFixup    *fixup, *ifixup;
    EjsName         qname, baseClassName, ifaceClassName;
    EjsBlock        *block;
    int             attributes, numTypeProp, numInstanceProp, slotNum, numInterfaces, i;

    fixup = 0;
    ifixup = 0;
    
    qname.name = ejsModuleReadString(ejs, mp);
    qname.space = ejsModuleReadString(ejs, mp);
    
    ejsModuleReadNumber(ejs, mp, &attributes);
    ejsModuleReadNumber(ejs, mp, &slotNum);
    ejsModuleReadType(ejs, mp, &baseType, &fixup, &baseClassName, 0);
    ejsModuleReadNumber(ejs, mp, &numTypeProp);
    ejsModuleReadNumber(ejs, mp, &numInstanceProp);
    ejsModuleReadNumber(ejs, mp, &numInterfaces);

    if (mp->hasError) {
        return MPR_ERR_CANT_READ;
    }
    if (ejsLookupProperty(ejs, ejs->global, &qname) >= 0) {
        ejsThrowReferenceError(ejs, "Class \"%s\" already loaded", qname.name);
        return MPR_ERR_CANT_CREATE;
    }
    if (fixup || (baseType && baseType->needFixup)) {
        attributes |= EJS_ATTR_SLOTS_NEED_FIXUP;
    }

    /*
     *  Find pre-existing native types.
     */
    if (attributes & EJS_ATTR_NATIVE) {
        type = nativeType = (EjsType*) mprLookupHash(ejs->coreTypes, qname.name);
        if (type == 0) {
            mprLog(ejs, 1, "WARNING: can't find native type \"%s\"", qname.name);
        }
    } else {
        type = nativeType = 0;
#if BLD_DEBUG
        if (mprLookupHash(ejs->coreTypes, qname.name)) {
            mprError(ejs, "WARNING: type \"%s\" defined as a native type but not declared as native", qname.name);
        }
#endif
    }

    if (mp->flags & EJS_MODULE_BUILTIN) {
        attributes |= EJS_ATTR_BUILTIN;
    }
    if (attributes & EJS_ATTR_SLOTS_NEED_FIXUP) {
        baseType = 0;
        if (fixup == 0) {
            fixup = createFixup(ejs, (baseType) ? &baseType->qname : &ejs->objectType->qname, -1);
        }
    }
    
    mprLog(ejs, 9, "    Load %s class %s for module %s at slot %d", qname.space, qname.name, mp->name, slotNum);

    /*
     *  If the module is fully bound, then we install the type at the prescribed slot number.
     */
    if (slotNum < 0) {
        slotNum = ejs->globalBlock->obj.numProp;
    }
    
    if (type == 0) {
        attributes |= EJS_ATTR_OBJECT | EJS_ATTR_OBJECT_HELPERS;
        type = ejsCreateType(ejs, &qname, mp, baseType, sizeof(EjsObject), slotNum, numTypeProp, numInstanceProp, 
            attributes, 0);
        if (type == 0) {
            ejsThrowInternalError(ejs, "Can't create class %s", qname.name);
            return MPR_ERR_BAD_STATE;
        }

    } else {
        mp->hasNative = 1;
        if (!type->block.obj.var.native) {
            mprError(ejs, "WARNING: type not defined as native: \"%s\"", type->qname.name);
        }
    }
    
    /*
     *  Read implemented interfaces. Add to type->implements. Create fixup record if the interface type is not yet known.
     */
    if (numInterfaces > 0) {
        type->implements = mprCreateList(type);
        for (i = 0; i < numInterfaces; i++) {
            if (ejsModuleReadType(ejs, mp, &iface, &ifixup, &ifaceClassName, 0) < 0) {
                return MPR_ERR_CANT_READ;
            }
            if (iface) {
                mprAddItem(type->implements, iface);
            } else {
                if (addFixup(ejs, EJS_FIXUP_INTERFACE_TYPE, (EjsVar*) type, -1, ifixup) < 0) {
                    ejsThrowMemoryError(ejs);
                    return MPR_ERR_NO_MEMORY;
                }
            }
        }
    }

    if (mp->flags & EJS_MODULE_BUILTIN) {
        type->block.obj.var.builtin = 1;
    }
    if (attributes & EJS_ATTR_HAS_STATIC_INITIALIZER) {
        type->hasStaticInitializer = 1;
    }
    if (attributes & EJS_ATTR_DYNAMIC_INSTANCE) {
        type->block.dynamicInstance = 1;
    }

    slotNum = ejsDefineProperty(ejs, ejs->global, slotNum, &qname, ejs->typeType, attributes, (EjsVar*) type);
    if (slotNum < 0) {
        ejsThrowMemoryError(ejs);
        return MPR_ERR_NO_MEMORY;
    }
    type->module = mp;

    if (fixup) {
        if (addFixup(ejs, EJS_FIXUP_BASE_TYPE, (EjsVar*) type, -1, fixup) < 0) {
            ejsThrowMemoryError(ejs);
            return MPR_ERR_NO_MEMORY;
        }
        
    } else {
        if (ejs->flags & EJS_FLAG_EMPTY) {
            if (attributes & EJS_ATTR_NATIVE) {
                /*
                 *  When empty, native types are created with no properties and with numTraits equal to zero. 
                 *  This is so the compiler can compile the core ejs module. For ejsmod which may also run in 
                 *  empty mode, we set numInherited here to the correct value for native types.
                 */
                if (type->block.numInherited == 0 && type->baseType) {
                    type->block.numInherited = type->baseType->block.numTraits;
                }
            }
        }
    }

#if BLD_FEATURE_EJS_DOC
    setDoc(ejs, mp, ejs->global, slotNum);
#endif

    block = (EjsBlock*) type;
    block->scopeChain = mp->scopeChain;
    mp->scopeChain = block;

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_CLASS, mp, slotNum, qname, type, attributes);
    }
    return 0;
}


static int loadEndClassSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    EjsType     *type;

    mprLog(ejs, 9, "    End class section");

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_CLASS_END, mp, mp->scopeChain);
    }
    type = (EjsType*) mp->scopeChain;
    if (type->block.hasScriptFunctions && type->baseType) {
        ejsDefineTypeNamespaces(ejs, type);
    }
    mp->scopeChain = mp->scopeChain->scopeChain;
    return 0;
}


static int loadFunctionSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    EjsType         *returnType;
    EjsTypeFixup    *fixup;
    EjsFunction     *fun;
    EjsName         qname, returnTypeName;
    EjsBlock        *block;
    uchar           *code;
    int             slotNum, numArgs, codeLen, numLocals, numExceptions, attributes, nextSlot, lang;

    lang = 0;

    qname.name = ejsModuleReadString(ejs, mp);
    qname.space = ejsModuleReadString(ejs, mp);
    ejsModuleReadNumber(ejs, mp, &nextSlot);
    ejsModuleReadNumber(ejs, mp, &attributes);
    ejsModuleReadByte(ejs, mp, &lang);
 
    ejsModuleReadType(ejs, mp, &returnType, &fixup, &returnTypeName, 0);
    ejsModuleReadNumber(ejs, mp, &slotNum);
    ejsModuleReadNumber(ejs, mp, &numArgs);
    ejsModuleReadNumber(ejs, mp, &numLocals);
    ejsModuleReadNumber(ejs, mp, &numExceptions);
    ejsModuleReadNumber(ejs, mp, &codeLen);

    if (mp->hasError) {
        return MPR_ERR_CANT_READ;
    }

    block = (EjsBlock*) mp->scopeChain;
    mprAssert(block);
    mprAssert(numArgs >= 0 && numArgs < EJS_MAX_ARGS);
    mprAssert(numLocals >= 0 && numLocals < EJS_MAX_LOCALS);
    mprAssert(numExceptions >= 0 && numExceptions < EJS_MAX_EXCEPTIONS);

    mprLog(ejs, 9, "Loading function %s:%s at slot %d", qname.space, qname.name, slotNum);

    /*
     *  Read the code. We pass ownership of the code to createMethod i.e. don't free.
     */
    if (codeLen > 0) {
        code = (uchar*) mprAlloc(ejsGetAllocCtx(ejs), codeLen);
        if (code == 0) {
            return MPR_ERR_NO_MEMORY;
        }
        if (mprRead(file, code, codeLen) != codeLen) {
            mprFree(code);
            return MPR_ERR_CANT_READ;
        }
        block->hasScriptFunctions = 1;
    } else {
        code = 0;
    }

    if (attributes & EJS_ATTR_NATIVE) {
        mp->hasNative = 1;
    }
    if (attributes & EJS_ATTR_INITIALIZER) {
        mp->hasInitializer = 1;
    }
    if (mp->flags & EJS_MODULE_BUILTIN) {
        attributes |= EJS_ATTR_BUILTIN;
    }

    if (ejsLookupProperty(ejs, (EjsVar*) block, &qname) >= 0 && !(attributes & EJS_ATTR_OVERRIDE)) {
        if (ejsIsType(block)) {
            ejsThrowReferenceError(ejs,
                "function \"%s\" already defined in type \"%s\". Try adding \"override\" to the function declaration.", 
                qname.name, ((EjsType*) block)->qname.name);
        } else {
            ejsThrowReferenceError(ejs,
                "function \"%s\" already defined. Try adding \"override\" to the function declaration.", qname.name);
        }
        return MPR_ERR_CANT_CREATE;
    }

    /*
     *  Create the function using the current scope chain. Non-methods revise this scope chain via the 
     *  DefineFunction op code.
     */
    fun = ejsCreateFunction(ejs, code, codeLen, numArgs, numExceptions, returnType, attributes, mp->constants, 
        mp->scopeChain, lang);
    if (fun == 0) {
        mprFree(code);
        return MPR_ERR_NO_MEMORY;
    }
    if (code) {
        mprStealBlock(fun, code);
    }

    ejsSetDebugName(fun, qname.name);

    if (block == (EjsBlock*) ejs->global && slotNum < 0) {
        if (attributes & EJS_ATTR_OVERRIDE) {
            slotNum = ejsLookupProperty(ejs, (EjsVar*) block, &qname);
            if (slotNum < 0) {
                mprError(ejs, "Can't find method \"%s\" to override", qname.name);
                return MPR_ERR_NO_MEMORY;
            }

        } else {
            slotNum = -1;
        }
    }

    if (mp->flags & EJS_MODULE_BUILTIN) {
        fun->block.obj.var.builtin = 1;
    }

    if (attributes & EJS_ATTR_INITIALIZER && block == (EjsBlock*) ejs->global) {
        mp->initializer = fun;
        slotNum = -1;

    } else {
        slotNum = ejsDefineProperty(ejs, (EjsVar*) block, slotNum, &qname, ejs->functionType, attributes, (EjsVar*) fun);
        if (slotNum < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    
    ejsSetNextFunction(fun, nextSlot);

    if (fixup) {
        mprAssert(returnType == 0);
        if (addFixup(ejs, EJS_FIXUP_RETURN_TYPE, (EjsVar*) fun, -1, fixup) < 0) {
            ejsThrowMemoryError(ejs);
            return MPR_ERR_NO_MEMORY;
        }
    }

#if BLD_FEATURE_EJS_DOC
    setDoc(ejs, mp, (EjsVar*) block, slotNum);
#endif

    mp->currentMethod = fun;
    fun->block.scopeChain = mp->scopeChain;
    mp->scopeChain = &fun->block;
    fun->loading = 1;

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_FUNCTION, mp, block, slotNum, qname, fun, attributes);
    }
    return 0;
}


static int loadEndFunctionSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    EjsTrait            *trait;
    EjsFunction         *fun;
    int                 i;

    mprLog(ejs, 9, "    End function section");

    fun = (EjsFunction*) mp->scopeChain;

    for (i = 0; i < (int) fun->numArgs; i++) {
        trait = ejsGetPropertyTrait(ejs, (EjsVar*) fun, i);
        if (trait && trait->attributes & EJS_ATTR_DEFAULT) {
            fun->numDefault++;
        }
    }
    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_FUNCTION_END, mp, fun);
    }
    mp->scopeChain = mp->scopeChain->scopeChain;
    fun->loading = 0;

    return 0;
}


static int loadExceptionSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    EjsFunction         *fun;
    EjsType             *catchType;
    EjsTypeFixup        *fixup;
    EjsCode             *code;
    EjsEx               *ex;
    int                 tryStart, tryEnd, handlerStart, handlerEnd, numBlocks, numStack, flags, i;

    fun = mp->currentMethod;
    mprAssert(fun);

    flags = 0;
    code = &fun->body.code;

    for (i = 0; i < code->numHandlers; i++) {
        ejsModuleReadByte(ejs, mp, &flags);
        ejsModuleReadNumber(ejs, mp, &tryStart);
        ejsModuleReadNumber(ejs, mp, &tryEnd);
        ejsModuleReadNumber(ejs, mp, &handlerStart);
        ejsModuleReadNumber(ejs, mp, &handlerEnd);
        ejsModuleReadNumber(ejs, mp, &numBlocks);
        ejsModuleReadNumber(ejs, mp, &numStack);
        ejsModuleReadType(ejs, mp, &catchType, &fixup, 0, 0);

        if (mp->hasError) {
            return MPR_ERR_CANT_READ;
        }
        ex = ejsAddException(fun, tryStart, tryEnd, catchType, handlerStart, handlerEnd, numBlocks, numStack, flags, i);
        if (fixup) {
            mprAssert(catchType == 0);
            if (addFixup(ejs, EJS_FIXUP_EXCEPTION, (EjsVar*) ex, 0, fixup) < 0) {
                mprAssert(0);
                return MPR_ERR_NO_MEMORY;
            }
        }
    }
    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_EXCEPTION, mp, fun);
    }
    return 0;
}

/*
 *  Define a global, class or block property. Not used for function locals or args.
 */
static int loadPropertySection(Ejs *ejs, MprFile *file, EjsModule *mp, int sectionType)
{
    EjsType         *type;
    EjsTypeFixup    *fixup;
    EjsName         qname, propTypeName;
    EjsVar          *block, *value;
    cchar           *str;
    int             slotNum, attributes, fixupKind;

    value = 0;
    block = (EjsVar*) mp->scopeChain;
    mprAssert(block);
    
    qname.name = ejsModuleReadString(ejs, mp);
    qname.space = ejsModuleReadString(ejs, mp);
    
    ejsModuleReadNumber(ejs, mp, &attributes);
    ejsModuleReadNumber(ejs, mp, &slotNum);
    ejsModuleReadType(ejs, mp, &type, &fixup, &propTypeName, 0);

    if (attributes & EJS_ATTR_HAS_VALUE) {
        if ((str = ejsModuleReadString(ejs, mp)) == 0) {
            return MPR_ERR_CANT_READ;
        }
        /*  Only doing for namespaces currently */
        value = (EjsVar*) ejsCreateNamespace(ejs, str, str);
    }

    mprLog(ejs, 9, "Loading property %s:%s at slot %d", qname.space, qname.name, slotNum);

    if (attributes & EJS_ATTR_NATIVE) {
        mp->hasNative = 1;
    }
    if (mp->flags & EJS_MODULE_BUILTIN) {
        attributes |= EJS_ATTR_BUILTIN;
    }

    if (ejsLookupProperty(ejs, block, &qname) >= 0) {
        ejsThrowReferenceError(ejs, "property \"%s\" already loaded", qname.name);
        return MPR_ERR_CANT_CREATE;
    }

    if (ejsIsFunction(block)) {
        fixupKind = EJS_FIXUP_LOCAL;

    } else if (ejsIsType(block) && !(attributes & EJS_ATTR_STATIC) && block != ejs->global) {
        mprAssert(((EjsType*) block)->instanceBlock);
        block = (EjsVar*) ((EjsType*) block)->instanceBlock;
        fixupKind = EJS_FIXUP_INSTANCE_PROPERTY;

    } else {
        fixupKind = EJS_FIXUP_TYPE_PROPERTY;
    }

    slotNum = ejsDefineProperty(ejs, block, slotNum, &qname, type, attributes, value);
    if (slotNum < 0) {
        return MPR_ERR_CANT_WRITE;
    }

    if (fixup) {
        mprAssert(type == 0);
        if (addFixup(ejs, fixupKind, block, slotNum, fixup) < 0) {
            ejsThrowMemoryError(ejs);
            return MPR_ERR_NO_MEMORY;
        }
    }

#if BLD_FEATURE_EJS_DOC
    setDoc(ejs, mp, block, slotNum);
#endif

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_PROPERTY, mp, block, slotNum, qname, attributes, propTypeName);
    }
    return 0;
}


#if BLD_FEATURE_EJS_DOC
static int loadDocSection(Ejs *ejs, MprFile *file, EjsModule *mp)
{
    char        *doc;

    mprLog(ejs, 9, "    Documentation section");

    doc = ejsModuleReadString(ejs, mp);

    if (ejs->flags & EJS_FLAG_DOC) {
        mp->doc = doc;
        if (ejs->loaderCallback) {
            (ejs->loaderCallback)(ejs, EJS_SECT_DOC, doc);
        }
    }
    return 0;
}
#endif



#if !BLD_FEATURE_STATIC
/*
 *  Check if a native module exists at the given path. If so, load it. If the path is a scripted module
 *  but has a corresponding native module, then load that. Return 1 if loaded, -1 for errors, 0 if no
 *  native module found.
 */
static int loadNativeLibrary(Ejs *ejs, EjsModule *mp, cchar *dir)
{
    char    path[MPR_MAX_PATH], initName[MPR_MAX_PATH], moduleName[MPR_MAX_PATH], *cp;

    if (ejs->flags & EJS_FLAG_NO_EXE) {
        return 0;
    }

    mprSprintf(path, sizeof(path), "%s/%s%s", dir, mp->name, BLD_SHOBJ);
    if (! mprPathExists(ejs, path, R_OK)) {
        mprError(ejs, "Native library not found %s", path);
        return 0;
    }

    /*
     *  Build the DSO entry point name. Format is "NameModuleInit" where Name has "." converted to "_"
     */
    mprStrcpy(moduleName, sizeof(moduleName), mp->name);
    moduleName[0] = tolower((int) moduleName[0]);
    mprSprintf(initName, sizeof(initName), "%sModuleInit", moduleName);
    for (cp = initName; *cp; cp++) {
        if (*cp == '.') {
            *cp = '_';
        }
    }
    if (mprLookupModule(ejs, mp->name) != 0) {
        mprLog(ejs, 1, "Native module \"%s\" is already loaded", path);
        return 0;
    }
    mprLog(ejs, 4, "Loading native library %s", path);
    if (mprLoadModule(ejs, path, initName) == 0) {
        return MPR_ERR_CANT_OPEN;
    }
    return 1;
}
#endif


/*
 *  Load a scripted module file. Return a modified list of modules.
 */
static int loadScriptModule(Ejs *ejs, MprFile *file, cchar *path, int flags)
{
    EjsModuleHdr    hdr;
    int             status;

    mprAssert(path);

    /*
     *  Read module file header
     */
    if ((mprRead(file, &hdr, sizeof(hdr))) != sizeof(hdr)) {
        ejsThrowIOError(ejs, "Error reading module file %s, corrupt header", path);
        return EJS_ERR;
    }
    if ((int) swapWord(ejs, hdr.magic) != EJS_MODULE_MAGIC) {
        ejsThrowIOError(ejs, "Bad module file format in %s", path);
        return EJS_ERR;
    }
    if ((int) swapWord(ejs, hdr.fileVersion) != EJS_MODULE_VERSION) {
        ejsThrowIOError(ejs, "Incompatible module file format in %s", path);
        return EJS_ERR;
    }

    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_START, path, &hdr);
    }

    /*
     *  Load the sections: classes, properties and functions. This may load multiple modules.
     */
    if ((status = loadSections(ejs, file, &hdr, flags)) < 0) {
        if (ejs->exception == 0) {
            ejsThrowReferenceError(ejs, "Can't load module file %s", path);
        }
        return status;
    }
    if (ejs->loaderCallback) {
        (ejs->loaderCallback)(ejs, EJS_SECT_END, ejs->loadState->modules, 0);
    }
    return 0;
}


static int fixupTypes(Ejs *ejs, MprList *list)
{
    EjsTypeFixup    *fixup;
    EjsModule       *mp;
    EjsType         *type, *targetType;
    EjsBlock        *instanceBlock;
    EjsTrait        *trait;
    EjsFunction     *targetFunction;
    EjsEx           *targetException;
    int             next;

    for (next = 0; (fixup = (EjsTypeFixup*) mprGetNextItem(list, &next)) != 0; ) {
        mp = 0;
        type = 0;
        if (fixup->typeSlotNum >= 0) {
            type = (EjsType*) ejsGetProperty(ejs, ejs->global, fixup->typeSlotNum);

        } else if (fixup->typeName.name) {
            mprAssert(fixup->typeSlotNum < 0);
            type = (EjsType*) ejsGetPropertyByName(ejs, ejs->global, &fixup->typeName);
            
        } else {
            continue;
        }
        if (type == 0) {
            ejsThrowReferenceError(ejs, "Can't fixup forward type reference for %s. Fixup kind %d", 
                fixup->typeName.name, fixup->kind);
            mprError(ejs, "Can't fixup forward type reference for %s. Fixup kind %d", fixup->typeName.name, fixup->kind);
            return EJS_ERR;
        }

        switch (fixup->kind) {
        case EJS_FIXUP_BASE_TYPE:
            mprAssert(fixup->target);
            targetType = (EjsType*) fixup->target;
            targetType->needFixup = 1;
            ejsFixupClass(ejs, targetType, type, targetType->implements, 0);
            instanceBlock = targetType->instanceBlock;
            if (instanceBlock && type) {
                ejsFixupBlock(ejs, instanceBlock, type->instanceBlock, targetType->implements, 0);
            }
            if (targetType->block.namespaces.length == 0 && type->block.hasScriptFunctions) {
                ejsDefineTypeNamespaces(ejs, targetType);
            }
            break;

        case EJS_FIXUP_INTERFACE_TYPE:
            targetType = (EjsType*) fixup->target;
            mprAddItem(targetType->implements, type);
            break;

        case EJS_FIXUP_RETURN_TYPE:
            mprAssert(fixup->target);
            targetFunction = (EjsFunction*) fixup->target;
            targetFunction->resultType = type;
            break;

        case EJS_FIXUP_TYPE_PROPERTY:
            mprAssert(fixup->target);
            trait = ejsGetPropertyTrait(ejs, fixup->target, fixup->slotNum);
            mprAssert(trait);
            if (trait) {
                trait->type = type;
            }
            break;

        case EJS_FIXUP_INSTANCE_PROPERTY:
            mprAssert(fixup->target);
            mprAssert(ejsIsBlock(fixup->target));
            mprAssert(fixup->target->isInstanceBlock);
            trait = ejsGetPropertyTrait(ejs, fixup->target, fixup->slotNum);
            mprAssert(trait);
            if (trait) {
                trait->type = type;
            }
            break;

        case EJS_FIXUP_LOCAL:
            mprAssert(fixup->target);
            trait = ejsGetPropertyTrait(ejs, fixup->target, fixup->slotNum);
            mprAssert(trait);
            if (trait) {
                trait->type = type;
            }
            break;

        case EJS_FIXUP_EXCEPTION:
            mprAssert(fixup->target);
            targetException = (EjsEx*) fixup->target;
            targetException->catchType = type;
            break;

        default:
            mprAssert(0);
        }
    }
    return 0;
}


/*
 *  Search for a file. If found, Return the path where the file was located. Otherwise return null.
 */
static char *probe(Ejs *ejs, cchar *path, int minVersion, int maxVersion)
{
    MprDirEntry     *dp, *best;
    MprList         *files;
    char            *dir, *base, *vp, *tok, *ext, *result;
    int             nameLen, version, next, major, minor, patch, bestVersion;

    mprAssert(ejs);
    mprAssert(path);

    mprLog(ejs, 7, "Probe for file %s", path);

    if (maxVersion == 0) {
        if (mprPathExists(ejs, path, R_OK)) {
            return mprStrdup(ejs, path);
        }
        return 0;
    }

    dir = mprGetPathDir(ejs, path);
    base = mprGetPathBase(ejs, path);
    if ((ext = strrchr(base, '.')) != 0) {
        *ext = '\0';
    }
    files = mprGetPathFiles(ejs, dir, 0);
    nameLen = strlen(base);
    bestVersion = -1;
    best = 0;

    for (next = 0; (dp = mprGetNextItem(files, &next)) != 0; ) {
        if (strncmp(dp->name, base, nameLen) != 0) {
            continue;
        }
        if ((ext = strrchr(dp->name, '.')) == 0 || strcmp(ext, EJS_MODULE_EXT) != 0) {
            continue;
        }
        if (dp->name[nameLen] == '-') {
            vp = &dp->name[nameLen + 1];
            minor = patch = 0;
            major = (int) mprAtoi(vp, 10);
            if ((tok = strchr(vp, '.')) != 0) {
                minor = (int) mprAtoi(++tok, 10);
            }
            if ((tok = strchr(vp, '.')) != 0) {
                patch = (int) mprAtoi(++tok, 10);
            }
            version = EJS_MAKE_VERSION(major, minor, patch);
        } else if (&dp->name[nameLen] == ext) {
            version = 0;
        } else {
            continue;
        }
        if (minVersion <= version && version <= maxVersion) {
            if (best == 0 || bestVersion < version) {
                bestVersion = version;
                best = dp;
            }
        }
    }
    if (best == 0) {
        result = 0;
    } else {
        result = mprJoinPath(ejs, dir, best->name);
    }
    mprFree(files);
    return result;
}


/*
 *  Search for a module. 
 *  The search strategy is: Given a name "a.b.c", scan for:
 *
 *      1. File named a.b.c
 *      2. File named a/b/c
 *      3. File named a.b.c in EJSPATH
 *      4. File named a/b/c in EJSPATH
 *      5. File named c in EJSPATH
 */
char *ejsSearchForModule(Ejs *ejs, cchar *nameArg, int minVersion, int maxVersion)
{
    MprCtx  ctx;
    char    *path, *ejsPath, *fileName, *baseName, *searchPath, *dir, *tok, *cp, *slashName, *name;

    slashName = 0;
    ejsPath = ejs->ejsPath;
    mprAssert(ejsPath);

    if (maxVersion <= 0) {
        maxVersion = MAXINT;
    }
    ctx = name = mprGetNormalizedPath(ejs, nameArg);

    mprLog(ejs, 5, "Search for module \"%s\" in ejspath %s", name, ejsPath);

    /*
     *  1. Search for path directly
     */
    if ((path = probe(ejs, name, minVersion, maxVersion)) != 0) {
        mprLog(ejs, 5, "Found %s at %s", name, path);
        mprFree(name);
        return path;
    }

    /*
     *  2. Search for "a/b/c"
     */
    slashName = mprStrdup(ejs, name);
    for (cp = slashName; *cp; cp++) {
        if (*cp == '.') {
            *cp = mprGetPathSeparator(ejs, name);
        }
    }
    if ((path = probe(ejs, slashName, minVersion, maxVersion)) != 0) {
        mprLog(ejs, 5, "Found %s at %s", name, path);
        mprFree(name);
        return path;
    }

    /*
     *  3. Search for "a.b.c" in EJSPATH
     */
    searchPath = mprStrdup(ejs, ejsPath);
    dir = mprStrTok(searchPath, MPR_SEARCH_SEP, &tok);
    while (dir && *dir) {
        fileName = mprStrcat(ctx, -1, dir, "/", name, NULL);
        if ((path = probe(ejs, fileName, minVersion, maxVersion)) != 0) {
            mprLog(ejs, 5, "Found %s at %s", name, path);
            mprFree(ctx);
            return path;
        }
        dir = mprStrTok(NULL, MPR_SEARCH_SEP, &tok);
    }
    mprFree(searchPath);

    /*
     *  4. Search for "a/b/c" in EJSPATH
     */
    searchPath = mprStrdup(ejs, ejsPath);
    dir = mprStrTok(searchPath, MPR_SEARCH_SEP, &tok);
    while (dir && *dir) {
        fileName = mprStrcat(ctx, -1, dir, "/", slashName, NULL);
        if ((path = probe(ejs, fileName, minVersion, maxVersion)) != 0) {
            mprLog(ejs, 5, "Found %s at %s", name, path);
            mprFree(ctx);
            return path;
        }
        dir = mprStrTok(NULL, MPR_SEARCH_SEP, &tok);
    }
    mprFree(searchPath);

    /*
     *  5. Search for "c" in EJSPATH
     */
    baseName = mprGetPathBase(ctx, slashName);
    searchPath = mprStrdup(ctx, ejsPath);
    dir = mprStrTok(searchPath, MPR_SEARCH_SEP, &tok);
    while (dir && *dir) {
        fileName = mprStrcat(ctx, -1, dir, "/", baseName, NULL);
        if ((path = probe(ejs, fileName, minVersion, maxVersion)) != 0) {
            mprLog(ejs, 5, "Found %s at %s", name, path);
            mprFree(ctx);
            return path;
        }
        dir = mprStrTok(NULL, MPR_SEARCH_SEP, &tok);
    }
    mprFree(ctx);
    return 0;
}


/*
 *  Read a string constant. String constants are stored as token offsets into
 *  the constant pool. The pool contains null terminated UTF-8 strings.
 */
char *ejsModuleReadString(Ejs *ejs, EjsModule *mp)
{
    int     t;

    mprAssert(mp);

    if (ejsModuleReadNumber(ejs, mp, &t) < 0) {
        return 0;
    }
    return tokenToString(mp, t);
}


/*
 *  Read a type reference. Types are stored as either global property slot numbers or as strings (token offsets into the 
 *  constant pool). The lowest bit is set if the reference is a string. The type and name arguments are optional and may 
 *  be set to null. Return EJS_ERR for errors, otherwise 0. Return the 0 if successful, otherwise return EJS_ERR. If the 
 *  type could not be resolved, allocate a fixup record and return in *fixup. The caller should then call addFixup.
 */
int ejsModuleReadType(Ejs *ejs, EjsModule *mp, EjsType **typeRef, EjsTypeFixup **fixup, EjsName *typeName, int *slotNum)
{
    EjsType         *type;
    EjsName         qname;
    int             t, slot;

    mprAssert(mp);
    mprAssert(typeRef);
    mprAssert(fixup);

    *typeRef = 0;
    *fixup = 0;

    if (typeName) {
        typeName->name = 0;
        typeName->space = 0;
    }

    if (ejsModuleReadNumber(ejs, mp, &t) < 0) {
        mprAssert(0);
        return EJS_ERR;
    }

    slot = -1;
    qname.name = 0;
    qname.space = 0;
    type = 0;

    switch (t & EJS_ENCODE_GLOBAL_MASK) {
    default:
        mp->hasError = 1;
        mprAssert(0);
        return EJS_ERR;

    case EJS_ENCODE_GLOBAL_NOREF:
        return 0;

    case EJS_ENCODE_GLOBAL_SLOT:
        /*
         *  Type is a builtin primitive type or we are binding globals.
         */
        slot = t >> 2;
        if (0 <= slot && slot < ejsGetPropertyCount(ejs, ejs->global)) {
            type = (EjsType*) ejsGetProperty(ejs, ejs->global, slot);
            if (type) {
                qname = type->qname;
            }
        }
        break;

    case EJS_ENCODE_GLOBAL_NAME:
        /*
         *  Type was unbound at compile time
         */
        qname.name = tokenToString(mp, t >> 2);
        if (qname.name == 0) {
            mp->hasError = 1;
            mprAssert(0);
            return EJS_ERR;
        }
        if ((qname.space = ejsModuleReadString(ejs, mp)) == 0) {
            mp->hasError = 1;
            mprAssert(0);
            return EJS_ERR;
        }
        if (qname.name) {
            slot = ejsLookupProperty(ejs, ejs->global, &qname);
            if (slot >= 0) {
                type = (EjsType*) ejsGetProperty(ejs, ejs->global, slot);
            }
        }
        break;
    }

    if (type) {
        if (!ejsIsType(type)) {
            mp->hasError = 1;
            mprAssert(0);
            return EJS_ERR;
        }
        *typeRef = type;

    } else if (type == 0 && fixup) {
        *fixup = createFixup(ejs, &qname, slot);
    }

    if (typeName) {
        *typeName = qname;
    }
    if (slotNum) {
        *slotNum = slot;
    }

    return 0;
}


static EjsTypeFixup *createFixup(Ejs *ejs, EjsName *qname, int slotNum)
{
    EjsTypeFixup    *fixup;

    mprAssert(ejs->loadState->typeFixups);

    fixup = mprAllocZeroed(ejs->loadState->typeFixups, sizeof(EjsTypeFixup));
    if (fixup == 0) {
        return 0;
    }
    fixup->typeName = *qname;
    fixup->typeSlotNum = slotNum;
    return fixup;
}


static int addFixup(Ejs *ejs, int kind, EjsVar *target, int slotNum, EjsTypeFixup *fixup)
{
    int     index;

    mprAssert(ejs);
    mprAssert(fixup);
    mprAssert(ejs->loadState->typeFixups);

    fixup->kind = kind;
    fixup->target = target;
    fixup->slotNum = slotNum;

    index = mprAddItem(ejs->loadState->typeFixups, fixup);
    if (index < 0) {
        mprAssert(0);
        return EJS_ERR;
    }
    return 0;
}


/*
 *  Convert a token index into a string.
 */
static char *tokenToString(EjsModule *mp, int token)
{
    if (token < 0 || token >= mp->constants->len) {
        mprAssert(0);
        return 0;
    }
    mprAssert(mp->constants);
    if (mp->constants == 0) {
        mprAssert(0);
        return 0;
    }
    return &mp->constants->pool[token];
}


/*
 *  Decode an encoded 32-bit word
 */
int ejsDecodeWord(uchar **pp)
{
    uchar   *start;
    int     value;

    start = *pp;
    value = (int) ejsDecodeNum(pp);
    *pp = start + 4;
    return value;
}


/*
 *  Get an encoded 64 bit number. Variable number of bytes.
 */
int64 ejsDecodeNum(uchar **pp)
{
    uchar   *pos;
    uint64  t;
    uint    c;
    int     sign, shift;

    pos = *pp;
    c = (uint) *pos++;

    /*
     *  Map sign bit (0,1) to 1,-1
     */
    sign = 1 - ((c & 0x1) << 1);
    t = (c >> 1) & 0x3f;
    shift = 6;

    while (c & 0x80) {
        c = *pos++;
        t |= (c & 0x7f) << shift;
        shift += 7;
    }
    *pp = pos;
    return t * sign;
}


/*
 *  Decode a 4 byte number from a file
 */
static int readWord(Ejs *ejs, MprFile *file, int *number)
{
    uchar   buf[4], *pp;

    mprAssert(file);
    mprAssert(number);

    if (mprRead(file, buf, 4) != 4) {
        return MPR_ERR_CANT_READ;
    }
    pp = buf;
    *number = ejsDecodeWord(&pp);
    return 0;
}


/*
 *  Decode a number from a file. Same as ejsDecodeNum but reading from a file.
 */
static int readNumber(Ejs *ejs, MprFile *file, int *number)
{
    uint    t, c;
    int     sign, shift;

    mprAssert(file);
    mprAssert(number);

    if ((c = mprGetc(file)) < 0) {
        return MPR_ERR_CANT_READ;
    }

    /*
     *  Map sign bit (0,1) to 1,-1
     */
    sign = 1 - ((c & 0x1) << 1);
    t = (c >> 1) & 0x3f;
    shift = 6;
    
    while (c & 0x80) {
        if ((c = mprGetc(file)) < 0) {
            return MPR_ERR_CANT_READ;
        }
        t |= (c & 0x7f) << shift;
        shift += 7;
    }
    *number = (int) (t * sign);
    return 0;
}


#if BLD_FEATURE_FLOATING_POINT
double ejsDecodeDouble(Ejs *ejs, uchar **pp)
{
    double   value;

    memcpy(&value, *pp, sizeof(double));
    value = swapDoubleWord(ejs, value);
    *pp += sizeof(double);
    return value;
}
#endif


/*
 *  Encode a number in a RLL encoding. Encoding is:
 *      Bit     0:  Sign
 *      Bits  1-6:  Low 6 bits (0-64)
 *      Bit     7:  Extension bit
 *      Bits 8-15:  Next 7 bits
 *      Bits   16:  Extension bit
 *      ...
 */
int ejsEncodeNum(uchar *pos, int64 number)
{
    uchar       *start;
    uint        encoded;

    mprAssert(pos);

    start = pos;
    if (number < 0) {
        number = -number;
        encoded = (uint) (((number & 0x3F) << 1) | 1);
    } else {
        encoded = (uint) (((number & 0x3F) << 1));
    }
    number >>= 6;

    while (number) {
        *pos++ = encoded | 0x80;
        encoded = (int) (number & 0x7f);
        number >>= 7;
    }
    *pos++ = encoded;
    mprAssert((pos - start) < 11);
    return (int) (pos - start);
}


int ejsEncodeUint(uchar *pos, int number)
{
    uchar       *start;
    uint        encoded;

    mprAssert(pos);

    start = pos;
    encoded = (uint) (((number & 0x3F) << 1));
    number >>= 6;

    while (number) {
        *pos++ = encoded | 0x80;
        encoded = (int) (number & 0x7f);
        number >>= 7;
    }
    *pos++ = encoded;
    mprAssert((pos - start) < 11);
    return (int) (pos - start);
}


/*
 *  Encode a 32-bit number. Always emit exactly 4 bytes.
 */
int ejsEncodeWord(uchar *pos, int number)
{
    int         len;

    mprAssert(pos);

    if (abs(number) > EJS_ENCODE_MAX_WORD) {
        mprError(mprGetMpr(NULL), "Code generation error. Word %d exceeds maximum %d", number, EJS_ENCODE_MAX_WORD);
        return 0;
    }
    len = ejsEncodeNum(pos, (int64) number);
    mprAssert(len <= 4);
    return 4;
}


int ejsEncodeDouble(Ejs *ejs, uchar *pos, double number)
{
    number = swapDoubleWord(ejs, number);
    memcpy(pos, &number, sizeof(double));
    return sizeof(double);
}


int ejsEncodeByteAtPos(uchar *pos, int value)
{
    mprAssert(pos);

    *pos = value;
    return 0;
}


int ejsEncodeWordAtPos(uchar *pos, int value)
{
    mprAssert(pos);

    return ejsEncodeWord(pos, value);
}



/*
 *  Read an encoded number. Numbers are little-endian encoded in 7 bits with
 *  the 0x80 bit of each byte being a continuation bit.
 */
int ejsModuleReadNumber(Ejs *ejs, EjsModule *mp, int *number)
{
    mprAssert(ejs);
    mprAssert(mp);
    mprAssert(number);

    if (readNumber(ejs, mp->file, number) < 0) {
        mp->hasError = 1;
        return -1;
    }
    return 0;
}


int ejsModuleReadByte(Ejs *ejs, EjsModule *mp, int *number)
{
    int     c;

    mprAssert(mp);
    mprAssert(number);

    if ((c = mprGetc(mp->file)) < 0) {
        mp->hasError = 1;
        return MPR_ERR_CANT_READ;
    }
    *number = c;
    return 0;
}


#if BLD_FEATURE_EJS_DOC
static void setDoc(Ejs *ejs, EjsModule *mp, EjsVar *block, int slotNum)
{
    if (mp->doc && ejsIsBlock(block)) {
        ejsCreateDoc(ejs, (EjsBlock*) block, slotNum, mp->doc);
        mp->doc = 0;
    }
}


EjsDoc *ejsCreateDoc(Ejs *ejs, EjsBlock *block, int slotNum, cchar *docString)
{
    EjsDoc      *doc;
    char        key[32];

    doc = mprAllocZeroed(ejs, sizeof(EjsDoc));
    if (doc == 0) {
        return 0;
    }
    doc->docString = mprStrdup(doc, docString);
    if (ejs->doc == 0) {
        ejs->doc = mprCreateHash(ejs, EJS_DOC_HASH_SIZE);
    }

    /*
     *  This is slow, but not critical path
     */
    mprSprintf(key, sizeof(key), "%Lx %d", PTOL(block), slotNum);
    mprAddHash(ejs->doc, key, doc);
    return doc;
}
#endif


#if UNUSED && KEEP
static int swapShort(Ejs *ejs, int word)
{
    if (mprGetEndian(ejs) == MPR_LITTLE_ENDIAN) {
        return word;
    }
    word = ((word & 0xFFFF) << 16) | ((word & 0xFFFF0000) >> 16);
    return ((word & 0xFF) << 8) | ((word & 0xFF00) >> 8);
}
#endif

static int swapWord(Ejs *ejs, int word)
{
    if (mprGetEndian(ejs) == MPR_LITTLE_ENDIAN) {
        return word;
    }
    return ((word & 0xFF000000) >> 24) | ((word & 0xFF0000) >> 8) | ((word & 0xFF00) << 8) | ((word & 0xFF) << 24);
}


static double swapDoubleWord(Ejs *ejs, double a)
{
    int64   low, high;

    if (mprGetEndian(ejs) == MPR_LITTLE_ENDIAN) {
        return a;
    }
    low = ((int64) a) & 0xFFFFFFFF;
    high = (((int64) a) >> 32) & 0xFFFFFFFF;
    return  (double) ((low & 0xFF) << 24 | (low & 0xFF00 << 8) | (low & 0xFF0000 >> 8) | (low & 0xFF000000 >> 16) |
            ((high & 0xFF) << 24 | (high & 0xFF00 << 8) | (high & 0xFF0000 >> 8) | (high & 0xFF000000 >> 16)) << 32);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsLoader.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsModule.c"
 */
/************************************************************************/

/**
 *  ejsModule.c - Ejscript module management
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




EjsModule *ejsCreateModule(Ejs *ejs, cchar *name, int version)
{
    EjsModule   *mp;

    mprAssert(version >= 0);

    mp = (EjsModule*) mprAllocZeroed(ejs, sizeof(EjsModule));
    if (mp == 0) {
        mprAssert(mp);
        return 0;
    }
    mp->name = mprStrdup(mp, name);
    mp->version = version;
    if (version) {
        mp->vname = mprAsprintf(mp, -1, "%s-%d", name, version);
    } else {
        mp->vname = mp->name;
    }

    mp->constants = mprAllocZeroed(mp, sizeof(EjsConst));
    if (mp->constants == 0) {
        return 0;
    }
    mp->constants->table = mprCreateHash(mp->constants, 0);
    return mp;
}


int ejsSetModuleConstants(Ejs *ejs, EjsModule *mp, cchar *pool, int poolSize)
{
    mprStealBlock(mp, pool);
    mp->constants->pool = (char*) pool;
    mp->constants->size = poolSize;
    mp->constants->len = poolSize;
    return 0;
}


/*
 *  Lookup a module name in the set of loaded modules
 *  If minVersion is <= 0, then any version up to, but not including maxVersion is acceptable.
 *  If maxVersion is < 0, then any version greater than minVersion is acceptable.
 *  If both are zero, then match the name itself and ignore minVersion and maxVersion
 *  If both are -1, then any version is acceptable.
 *  If both are equal, then only that version is acceptable.
 */
EjsModule *ejsLookupModule(Ejs *ejs, cchar *name, int minVersion, int maxVersion)
{
    EjsModule   *mp, *best;
    int         next;

    if (maxVersion < 0) {
        maxVersion = MAXINT;
    }
    best = 0;
    for (next = 0; (mp = (EjsModule*) mprGetNextItem(ejs->modules, &next)) != 0; ) {
        if ((minVersion == 0 && maxVersion == 0) || (minVersion <= mp->version && mp->version <= maxVersion)) {
            if (strcmp(mp->name, name) == 0) {
                if (best == 0 || best->version < mp->version) {
                    best = mp;
                }
            }
        }
    }
    return best;
}


int ejsAddModule(Ejs *ejs, EjsModule *mp)
{
    mprAssert(ejs->modules);
    return mprAddItem(ejs->modules, mp);
}


int ejsRemoveModule(Ejs *ejs, EjsModule *mp)
{
    mprAssert(ejs->modules);
    return mprRemoveItem(ejs->modules, mp);
}


MprList *ejsGetModuleList(Ejs *ejs)
{
    return ejs->modules;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsModule.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsScope.c"
 */
/************************************************************************/

/**
 *  ejsScope.c - Lookup variables in the scope chain.
 *
 *  This modules provides scope chain management including lookup, get and set services for variables. It will 
 *  lookup variables using the current execution variable scope and the set of open namespaces.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Look for a variable by name in the scope chain and return the location in "lookup" and a positive slot number if found. 
 *  If the name.space is non-null/non-empty, then only the given namespace will be used. otherwise the set of open 
 *  namespaces will be used. The lookup structure will contain details about the location of the variable.
 */
int ejsLookupScope(Ejs *ejs, EjsName *name, EjsLookup *lookup)
{
    EjsFrame        *fp;
    EjsBlock        *block;
    EjsState        *state;
    int             slotNum, nth;

    mprAssert(ejs);
    mprAssert(name);
    mprAssert(lookup);

    slotNum = -1;
    state = ejs->state;
    fp = state->fp;

    /*
     *  Look for the name in the scope chain considering each block scope. LookupVar will consider base classes and 
     *  namespaces. Don't search the last scope chain entry which will be global. For cloned interpreters, global 
     *  will belong to the master interpreter, so we must do that explicitly below to get the right global.
     */
    for (nth = 0, block = state->bp; block->scopeChain; block = block->scopeChain) {

        if (fp->function.thisObj && block == (EjsBlock*) fp->function.thisObj->type) {
            /*
             *  This will lookup the instance and all base classes
             */
            if ((slotNum = ejsLookupVar(ejs, fp->function.thisObj, name, lookup)) >= 0) {
                lookup->nthBlock = nth;
                break;
            }
            
        } else {
            if ((slotNum = ejsLookupVar(ejs, (EjsVar*) block, name, lookup)) >= 0) {
                lookup->nthBlock = nth;
                break;
            }
        }
        nth++;
    }
    if (slotNum < 0 && ((slotNum = ejsLookupVar(ejs, ejs->global, name, lookup)) >= 0)) {
        lookup->nthBlock = nth;
    }
    lookup->slotNum = slotNum;
    return slotNum;
}


/*
 *  Find a property in an object or type and its base classes.
 */
int ejsLookupVar(Ejs *ejs, EjsVar *vp, EjsName *name, EjsLookup *lookup)
{
    EjsType     *type;
    int         slotNum;

    mprAssert(vp);
    mprAssert(vp->type);
    mprAssert(name);

    /*
     *  OPT - bit field initialization
     */
    lookup->nthBase = 0;
    lookup->nthBlock = 0;
    lookup->useThis = 0;
    lookup->instanceProperty = 0;
    lookup->ownerIsType = 0;

    /*
     *  Search through the inheritance chain of base classes. nthBase counts the subtypes that must be traversed. 
     */
    for (slotNum = -1, lookup->nthBase = 0; vp; lookup->nthBase++) {
        if ((slotNum = ejsLookupVarWithNamespaces(ejs, vp, name, lookup)) >= 0) {
            break;
        }
        /*
         *  Follow the base type chain. If an instance, first base type is vp->type.
         */
        vp = (vp->isType) ? (EjsVar*) ((EjsType*) vp)->baseType: (EjsVar*) vp->type;
        type = (EjsType*) vp;
        if (type == 0 || type->skipScope) {
            break;
        }
    }
    return lookup->slotNum = slotNum;
}


/*
 *  Find a variable in a block. Scope blocks are provided by the global object, types, functions and statement blocks.
 */
int ejsLookupVarWithNamespaces(Ejs *ejs, EjsVar *vp, EjsName *name, EjsLookup *lookup)
{
    EjsNamespace    *nsp;
    EjsName         qname;
    EjsBlock        *b;
    EjsVar          *owner;
    int             slotNum, nextNsp;

    mprAssert(vp);
    mprAssert(name);
    mprAssert(name->name);
    mprAssert(name->space);
    mprAssert(lookup);

    if ((slotNum = ejsLookupProperty(ejs, vp, name)) >= 0) {
        lookup->obj = vp;
        lookup->name = *name;
        return slotNum;
    } else if (name->space[0] != EJS_EMPTY_NAMESPACE[0]) {
        lookup->obj = vp;
        lookup->name = *name;
        return slotNum;
    }

    slotNum = -1;
    qname = *name;
    for (b = ejs->state->bp; b; b = b->scopeChain) {
        for (nextNsp = -1; (nsp = (EjsNamespace*) ejsGetPrevItem(&b->namespaces, &nextNsp)) != 0; ) {

            if (nsp->flags & EJS_NSP_PROTECTED && vp->isType && ejs->state->fp) {
                /*
                 *  Protected access. See if the type containing the method we are executing is a sub class of the type 
                 *  containing the property ie. Can we see protected properties?
                 */
                owner = (EjsVar*) ejs->state->fp->function.owner;
                if (owner && !ejsIsA(ejs, owner, (EjsType*) vp)) {
                    continue;
                }
            }
            qname.space = nsp->uri;
            mprAssert(qname.space);
            if (qname.space) {
                slotNum = ejsLookupProperty(ejs, vp, &qname);
                if (slotNum >= 0) {
                    lookup->name = qname;
                    lookup->obj = vp;
                    lookup->slotNum = slotNum;
                    return slotNum;
                }
            }
        }
    }
    return -1;
}


/*
 *  Get a variable by name. If vp is specified, it contains an explicit object in which to search for the variable name. 
 *  Otherwise, the full execution scope is consulted. The lookup fields will be set as residuals.
 */
EjsVar *ejsGetVarByName(Ejs *ejs, EjsVar *vp, EjsName *name, EjsLookup *lookup)
{
    EjsVar  *result;
    int     slotNum;

    mprAssert(ejs);
    mprAssert(name);

    //  OPT - really nice to remove this
    if (vp && vp->type->helpers->getPropertyByName) {
        result = (*vp->type->helpers->getPropertyByName)(ejs, vp, name);
        if (result) {
            return result;
        }
    }

    if (vp) {
        slotNum = ejsLookupVar(ejs, vp, name, lookup);
    } else {
        slotNum = ejsLookupScope(ejs, name, lookup);
    }
    if (slotNum < 0) {
        return 0;
    }
    return ejsGetProperty(ejs, lookup->obj, slotNum);
}


void ejsShowBlockScope(Ejs *ejs, EjsBlock *block)
{
#if BLD_DEBUG
    EjsNamespace    *nsp;
    EjsList         *namespaces;
    int             nextNsp;

    mprLog(ejs, 6, "\n  Block scope");
    for (; block; block = block->scopeChain) {
        mprLog(ejs, 6, "    Block \"%s\" 0x%08x", mprGetName(block), block);
        namespaces = &block->namespaces;
        if (namespaces) {
            for (nextNsp = 0; (nsp = (EjsNamespace*) ejsGetNextItem(namespaces, &nextNsp)) != 0; ) {
                mprLog(ejs, 6, "        \"%s\"", nsp->uri);
            }
        }
    }
#endif
}


void ejsShowCurrentScope(Ejs *ejs)
{
#if BLD_DEBUG
    EjsNamespace    *nsp;
    EjsList         *namespaces;
    EjsBlock        *block;
    int             nextNsp;

    mprLog(ejs, 6, "\n  Current scope");
    for (block = ejs->state->bp; block; block = block->scopeChain) {
        mprLog(ejs, 6, "    Block \"%s\" 0x%08x", mprGetName(block), block);
        namespaces = &block->namespaces;
        if (namespaces) {
            for (nextNsp = 0; (nsp = (EjsNamespace*) ejsGetNextItem(namespaces, &nextNsp)) != 0; ) {
                mprLog(ejs, 6, "        \"%s\"", nsp->uri);
            }
        }
    }
#endif
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsScope.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsService.c"
 */
/************************************************************************/

/**
 *  ejsService.c - Ejscript interpreter factory
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if BLD_FEATURE_EJS_WEB
#endif


static void allocNotifier(Ejs *ejs, uint size, uint total, bool granted);
static int  cloneMaster(Ejs *ejs, Ejs *master);
static int  configureEjsModule(Ejs *ejs, EjsModule *mp, cchar *path);
static int  defineTypes(Ejs *ejs);
static void defineHelpers(Ejs *ejs);
static int  destroyEjs(Ejs *ejs);
static int  runSpecificMethod(Ejs *ejs, cchar *className, cchar *methodName);
static int  searchForMethod(Ejs *ejs, cchar *methodName, EjsType **typeReturn);
static void setDefaultSearchPath(Ejs *ejs);

#if BLD_FEATURE_STATIC || BLD_FEATURE_EJS_ALL_IN_ONE
#if BLD_FEATURE_MYSQL
static int configureMysqlModule(Ejs *ejs, EjsModule *mp, cchar *path);
#endif
#if BLD_FEATURE_SQLITE
static int configureSqliteModule(Ejs *ejs, EjsModule *mp, cchar *path);
#endif
#if BLD_FEATURE_EJS_WEB
static int configureWebModule(Ejs *ejs, EjsModule *mp, cchar *path);
#endif
#endif

/*
 *  Initialize the EJS subsystem
 */
EjsService *ejsCreateService(MprCtx ctx)
{
    EjsService  *sp;

    sp = mprAllocObjZeroed(ctx, EjsService);
    if (sp == 0) {
        return 0;
    }
    mprGetMpr(ctx)->ejsService = sp;
    sp->nativeModules = mprCreateHash(sp, 0);

    /*
     *  The native module callbacks are invoked after loading the module files. This allows the callback routines 
     *  to configure native methods and do other native type adjustments. configureEjsModule is always invoked even 
     *  if SHARED because it is never loaded from a shared library. Normally, when loading from a shared library, 
     *  the init routine is invoked immediately after loading the mod file and it should call the configuration routine.
     */
    ejsAddNativeModule(ctx, "ejs", configureEjsModule);

#if BLD_FEATURE_STATIC || BLD_FEATURE_EJS_ALL_IN_ONE
#if BLD_FEATURE_SQLITE
    ejsAddNativeModule(ctx, "ejs.db.sqlite", configureSqliteModule);
#endif
#if BLD_FEATURE_EJS_WEB
    ejsAddNativeModule(ctx, "ejs.web", configureWebModule);
#endif
#endif
    return sp;
}


Ejs *ejsCreate(MprCtx ctx, Ejs *master, cchar *searchPath, int flags)
{
    Ejs     *ejs;
    char    *env;

    /*
     *  Create interpreter structure
     */
    ejs = mprAllocObjWithDestructorZeroed(ctx, Ejs, destroyEjs);
    if (ejs == 0) {
        return 0;
    }
    mprSetAllocNotifier(ejs, (MprAllocNotifier) allocNotifier);
    ejs->service = mprGetMpr(ctx)->ejsService;

    /*
     *  Probably not necessary, but it keeps the objects in one place
     */
    ejs->heap = mprAllocHeap(ejs, "Ejs Object Heap", 1, 0, NULL);

    /*
     *  Flags may include COMPILER to pretend to be compiling. Ejsmod uses this to allow compiler-style access to
     *  getters and setters.
     */
    ejs->flags |= (flags & (EJS_FLAG_EMPTY | EJS_FLAG_COMPILER | EJS_FLAG_NO_EXE | EJS_FLAG_DOC));
    ejs->dispatcher = mprCreateDispatcher(ejs);

    if (ejsInitStack(ejs) < 0) {
        mprFree(ejs);
        return 0;
    }
    ejsCreateGCService(ejs);
    
    /*
     *  The search path consists of:
     *      EJSPATH : APP_EXE_DIR : MOD_DIR : . : searchPath
     */
    setDefaultSearchPath(ejs);
    env = getenv("EJSPATH");
    if (env && *env) {
        ejsAppendSearchPath(ejs, env);
    }
    if (searchPath) {
        ejsAppendSearchPath(ejs, searchPath);
    }
    ejsSetGeneration(ejs, EJS_GEN_ETERNAL);

    if (master == 0) {
        ejs->modules = mprCreateList(ejs);
        ejs->workers = mprCreateList(ejs);
        ejs->coreTypes = mprCreateHash(ejs, 0);
        ejs->standardSpaces = mprCreateHash(ejs, 0);

        defineHelpers(ejs);
        if (defineTypes(ejs) < 0) {
            mprFree(ejs);
            return 0;
        }
    } else {
        cloneMaster(ejs, master);
    }

#if BLD_FEATURE_MULTITHREAD
    ejs->mutex = mprCreateLock(ejs);
#endif

    if (mprHasAllocError(ejs)) {
        mprError(ejs, "Memory allocation error during initialization");
        mprFree(ejs);
        return 0;
    }
    ejs->initialized = 1;
    ejsCollectGarbage(ejs, EJS_GEN_ETERNAL);
    ejsSetGeneration(ejs, EJS_GEN_NEW);
    return ejs;
}


static int destroyEjs(Ejs *ejs)
{
    EjsState    *state;

    ejsDestroyGCService(ejs);
    state = ejs->masterState;
    if (state->stackBase) {
        mprMapFree(state->stackBase, state->stackSize);
    }
    mprFree(ejs->heap);
    return 0;
}


static void defineHelpers(Ejs *ejs)
{
    ejs->defaultHelpers = (EjsTypeHelpers*) mprAllocZeroed(ejs, sizeof(EjsTypeHelpers));
    ejsInitializeDefaultHelpers(ejs->defaultHelpers);

    /*
     *  Object inherits the default helpers. Block inherits the object helpers
     */
    ejs->objectHelpers = (EjsTypeHelpers*) mprMemdup(ejs, (void*) ejs->defaultHelpers, sizeof(EjsTypeHelpers));
    ejsInitializeObjectHelpers(ejs->objectHelpers);

    ejs->blockHelpers = (EjsTypeHelpers*) mprMemdup(ejs, (void*) ejs->objectHelpers, sizeof(EjsTypeHelpers));
    ejsInitializeBlockHelpers(ejs->blockHelpers);
}


/*
 *  Create the core language types. These are native types and are created prior to loading ejs.mod.
 *  The loader then matches these types to the loaded definitions.
 */
static int createTypes(Ejs *ejs)
{
    /*
     *  Create the essential bootstrap types: Object, Type and the global object, these are the foundation.
     *  All types are instances of Type. Order matters here.
     */
    ejsCreateObjectType(ejs);
    ejsCreateTypeType(ejs);
    ejsCreateBlockType(ejs);
    ejsCreateNamespaceType(ejs);
    ejsCreateFunctionType(ejs);
    ejsCreateGlobalBlock(ejs);
    ejsCreateNullType(ejs);

    /*
     *  Now create the rest of the native types.
     */
    ejsCreateArrayType(ejs);
    ejsCreateBooleanType(ejs);
    ejsCreateByteArrayType(ejs);
    ejsCreateDateType(ejs);
    ejsCreateErrorType(ejs);
    ejsCreateIteratorType(ejs);
    ejsCreateVoidType(ejs);
    ejsCreateNumberType(ejs);
    ejsCreateReflectType(ejs);
    ejsCreateStringType(ejs);
#if ES_XML && BLD_FEATURE_EJS_E4X
    ejsCreateXMLType(ejs);
    ejsCreateXMLListType(ejs);
#endif
#if ES_RegExp && BLD_FEATURE_REGEXP
    ejsCreateRegExpType(ejs);
#endif
    ejsCreateAppType(ejs);
    ejsCreateConfigType(ejs);
    ejsCreateGCType(ejs);
    ejsCreateMemoryType(ejs);
    ejsCreateSystemType(ejs);
    ejsCreateTimerType(ejs);
    ejsCreateFileType(ejs);
    ejsCreatePathType(ejs);
    ejsCreateFileSystemType(ejs);
#if ES_ejs_io_Http && BLD_FEATURE_HTTP_CLIENT
    ejsCreateHttpType(ejs);
#endif

    if (ejs->hasError || ejs->errorType == 0 || mprHasAllocError(ejs)) {
        return MPR_ERR;
    }
    return 0;
}


/*
 *  This will configure all the core types by defining native methods and properties
 */
static int configureEjsModule(Ejs *ejs, EjsModule *mp, cchar *path)
{
    EjsModule   *pp;
    
    if (ejs->flags & EJS_FLAG_EMPTY) {
        return 0;
    }
    if (mp->checksum != _ES_CHECKSUM_ejs) {
        ejsThrowIOError(ejs, "Module \"%s\" does not match native code", path);
        return EJS_ERR;
    }

    /*
     *  Order matters
     */
    ejsConfigureObjectType(ejs);
    ejsConfigureArrayType(ejs);
    ejsConfigureBlockType(ejs);
    ejsConfigureBooleanType(ejs);
    ejsConfigureByteArrayType(ejs);
    ejsConfigureDateType(ejs);
    ejsConfigureFunctionType(ejs);
    ejsConfigureGlobalBlock(ejs);
    ejsConfigureErrorType(ejs);
    ejsConfigureIteratorType(ejs);
    ejsConfigureMathType(ejs);
    ejsConfigureNamespaceType(ejs);
    ejsConfigureVoidType(ejs);
    ejsConfigureNumberType(ejs);
    ejsConfigureNullType(ejs);
    ejsConfigureReflectType(ejs);
    ejsConfigureStringType(ejs);
    ejsConfigureTypeType(ejs);
#if ES_XML && BLD_FEATURE_EJS_E4X
    ejsConfigureXMLType(ejs);
    ejsConfigureXMLListType(ejs);
#endif
#if ES_RegExp && BLD_FEATURE_REGEXP
    ejsConfigureRegExpType(ejs);
#endif

    ejsConfigureAppType(ejs);
    ejsConfigureConfigType(ejs);
#if BLD_FEATURE_MULTITHREAD
    ejsConfigureWorkerType(ejs);
#endif
    ejsConfigureGCType(ejs);
    ejsConfigureMemoryType(ejs);
    ejsConfigureSystemType(ejs);
    ejsConfigureTimerType(ejs);
    ejsConfigurePathType(ejs);
    ejsConfigureFileType(ejs);
    ejsConfigureFileSystemType(ejs);
#if ES_ejs_io_Http && BLD_FEATURE_HTTP_CLIENT
    ejsConfigureHttpType(ejs);
#endif

    if (ejs->hasError || ejs->errorType == 0 || mprHasAllocError(ejs)) {
        mprAssert(0);
        return MPR_ERR;
    }
    
    if ((pp = ejsLookupModule(ejs, "ejs.events", -1, -1)) != 0) {
        pp->configured = 1;
    }
    if ((pp = ejsLookupModule(ejs, "ejs.sys", -1, -1)) != 0) {
        pp->configured = 1;
    }
    if ((pp = ejsLookupModule(ejs, "ejs.io", -1, -1)) != 0) {
        pp->configured = 1;
    }

    /*
     *  This makes all essential values that have no references (like ejs->zeroValue) permanent
     */
    ejsMakeEternalPermanent(ejs);
    return 0;
}


#if BLD_FEATURE_STATIC || BLD_FEATURE_EJS_ALL_IN_ONE
#if BLD_FEATURE_MYSQL
static int configureMysqlModule(Ejs *ejs, EjsModule *mp, cchar *path)
{
    ejsConfigureMysqlTypes(ejs);
    if (ejs->hasError || ejs->errorType == 0 || mprHasAllocError(ejs)) {
        mprAssert(0);
        return MPR_ERR;
    }
    mp->configured = 1;
    return 0;
}
#endif


#if BLD_FEATURE_SQLITE
static int configureSqliteModule(Ejs *ejs, EjsModule *mp, cchar *path)
{
    if (ejs->flags & EJS_FLAG_EMPTY) {
        return 0;
    }
    ejsConfigureSqliteTypes(ejs);
    if (ejs->hasError || ejs->errorType == 0 || mprHasAllocError(ejs)) {
        mprAssert(0);
        return MPR_ERR;
    }
    if (mp->checksum != _ES_CHECKSUM_ejs_db_sqlite) {
        ejsThrowIOError(ejs, "Module \"%s\" does not match native code", path);
        return EJS_ERR;
    }
    mp->configured = 1;
    return 0;
}
#endif


#if BLD_FEATURE_EJS_WEB
static int configureWebModule(Ejs *ejs, EjsModule *mp, cchar *path)
{
    if (ejs->flags & EJS_FLAG_EMPTY) {
        return 0;
    }
    ejsConfigureWebTypes(ejs);

    if (ejs->hasError || ejs->errorType == 0 || mprHasAllocError(ejs)) {
        mprAssert(0);
        return MPR_ERR;
    }
    if (mp->checksum != _ES_CHECKSUM_ejs_web) {
        ejsThrowIOError(ejs, "Module \"%s\" does not match native code", path);
        return EJS_ERR;
    }
    mp->configured = 1;
    return 0;
}
#endif
#endif


/*
 *  Register a native module callback to be invoked when it it time to configure the module. This is used by loadable modules
 *  when they are built statically.
 */
int ejsAddNativeModule(MprCtx ctx, char *name, EjsNativeCallback callback)
{
    EjsService  *sp;

    sp = mprGetMpr(ctx)->ejsService;
    if (mprAddHash(sp->nativeModules, name, callback) == 0) {
        return EJS_ERR;
    }
    return 0;
}


static int defineTypes(Ejs *ejs)
{
    /*
     *  Create all the builtin types. These are defined and hashed. Not defined in global.
     */
    if (createTypes(ejs) < 0 || ejs->hasError) {
        mprError(ejs, "Can't create core types");
        return EJS_ERR;
    }

    /*
     *  Load the builtin module. This will create all the type definitions and match with builtin native types.
     *  This will call the configure routines defined in moduleConfig and will run the module initializers.
     *  Modules are check summed and configureEjsModule will check that the sum matches.
     */
    if (! (ejs->flags & EJS_FLAG_EMPTY)) {
        if (ejsLoadModule(ejs, EJS_MOD, 0, 0, EJS_MODULE_BUILTIN, NULL) < 0) {
            mprError(ejs, "Can't load " EJS_MOD);
            return EJS_ERR;
        }
    }
    return 0;
}


/*
 *  When cloning the master interpreter, the new interpreter references the master's core types. The core types MUST
 *  be immutable for this to work.
 */
static int cloneMaster(Ejs *ejs, Ejs *master)
{
    EjsName     qname;
    EjsType     *type;
    EjsVar      *vp;
    EjsTrait    *trait;
    int         i, count;

    mprAssert(master);

    ejs->master = master;
    ejs->service = master->service;
    ejs->defaultHelpers = master->defaultHelpers;
    ejs->objectHelpers = master->objectHelpers;
    ejs->blockHelpers = master->blockHelpers;
    ejs->objectType = master->objectType;

    ejs->arrayType = master->arrayType;
    ejs->blockType = master->blockType;
    ejs->booleanType = master->booleanType;
    ejs->byteArrayType = master->byteArrayType;
    ejs->dateType = master->dateType;
    ejs->errorType = master->errorType;
    ejs->eventType = master->eventType;
    ejs->errorEventType = master->errorEventType;
    ejs->functionType = master->functionType;
    ejs->iteratorType = master->iteratorType;
    ejs->namespaceType = master->namespaceType;
    ejs->nullType = master->nullType;
    ejs->numberType = master->numberType;
    ejs->objectType = master->objectType;
    ejs->regExpType = master->regExpType;
    ejs->stringType = master->stringType;
    ejs->stopIterationType = master->stopIterationType;
    ejs->typeType = master->typeType;
    ejs->voidType = master->voidType;
    ejs->workerType = master->workerType;

#if BLD_FEATURE_EJS_E4X
    ejs->xmlType = master->xmlType;
    ejs->xmlListType = master->xmlListType;
#endif

    ejs->emptyStringValue = master->emptyStringValue;
    ejs->falseValue = master->falseValue;
    ejs->infinityValue = master->infinityValue;
    ejs->minusOneValue = master->minusOneValue;
    ejs->nanValue = master->nanValue;
    ejs->negativeInfinityValue = master->negativeInfinityValue;
    ejs->nullValue = master->nullValue;
    ejs->oneValue = master->oneValue;
    ejs->trueValue = master->trueValue;
    ejs->undefinedValue = master->undefinedValue;
    ejs->zeroValue = master->zeroValue;

    ejs->configSpace = master->configSpace;
    ejs->emptySpace = master->emptySpace;
    ejs->eventsSpace = master->eventsSpace;
    ejs->ioSpace = master->ioSpace;
    ejs->intrinsicSpace = master->intrinsicSpace;
    ejs->iteratorSpace = master->iteratorSpace;
    ejs->internalSpace = master->internalSpace;
    ejs->publicSpace = master->publicSpace;
    ejs->sysSpace = master->sysSpace;

    ejs->argv = master->argv;
    ejs->argc = master->argc;
    ejs->coreTypes = master->coreTypes;
    ejs->standardSpaces = master->standardSpaces;

    ejs->modules = mprDupList(ejs, master->modules);
    ejs->sqlite = master->sqlite;

    //  Push this code into ejsGlobal.c. Call ejsCloneGlobal
    ejs->globalBlock = ejsCreateBlock(ejs, master->globalBlock->obj.capacity);
    ejs->global = (EjsVar*) ejs->globalBlock; 
    ejsSetDebugName(ejs->global, "global");
    ejs->globalBlock->obj.numProp = master->globalBlock->obj.numProp;
    ejsGrowBlock(ejs, ejs->globalBlock, ejs->globalBlock->obj.numProp);
    
    ejsCopyList(ejs->globalBlock, &ejs->globalBlock->namespaces, &master->globalBlock->namespaces);

    count = ejsGetPropertyCount(master, master->global);
    // count = ES_global_NUM_CLASS_PROP;
    for (i = 0; i < count; i++) {
        vp = ejsGetProperty(ejs, master->global, i);
        if (vp) {
            ejsSetProperty(ejs, ejs->global, i, ejsGetProperty(master, master->global, i));
            qname = ejsGetPropertyName(master, master->global, i);
            ejsSetPropertyName(ejs, ejs->global, i, &qname);
            trait = ejsGetTrait(master->globalBlock, i);
            if (trait) {
                ejsSetTrait(ejs->globalBlock, i, trait->type, trait->attributes);
            }
        }
    }
    
    /*
     *  Clone some mutable types.
     */
    type = (EjsType*) ejsGetProperty(ejs, ejs->global, ES_XML);
    ejsSetProperty(ejs, ejs->global, ES_XML, ejsCloneVar(ejs, (EjsVar*) type, 0));

#if ES_ejs_db_Database
    type = (EjsType*) ejsGetProperty(ejs, ejs->global, ES_ejs_db_Database); 
    if (type) {
        ejsSetProperty(ejs, ejs->global, ES_ejs_db_Database, ejsCloneVar(ejs, (EjsVar*) type, 0));
    }
#else
    /*
     *  Building shared. The web framework preloads these modules before cloning interpreters.
     *  The ejs command will not have them defined.
     */
    type = (EjsType*) ejsGetPropertyByName(ejs, ejs->global, ejsName(&qname, "ejs.db", "Database"));
    if (type) {
        ejsSetPropertyByName(ejs, ejs->global, &qname, ejsCloneVar(ejs, (EjsVar*) type, 0));
    }
#endif

#if ES_ejs_web_GoogleConnector
    type = (EjsType*) ejsGetProperty(ejs, ejs->global, ES_ejs_web_GoogleConnector); 
    if (type) {
        ejsSetProperty(ejs, ejs->global, ES_ejs_web_GoogleConnector, ejsCloneVar(ejs, (EjsVar*) type, 0));
    }
#else
    /*
     *  Shared.
     */
    type = (EjsType*) ejsGetPropertyByName(ejs, ejs->global, ejsName(&qname, "ejs.web", "GoogleConnector")); 
    if (type) {
        ejsSetPropertyByName(ejs, ejs->global, &qname, ejsCloneVar(ejs, (EjsVar*) type, 0));
    }
#endif
    ejsSetProperty(ejs, ejs->global, ES_global, ejs->global);
    return 0;
}


/*
 *  Notifier callback function. Invoked by mprAlloc on allocation errors. This will prevent the allocation error
 *  bubbling up to the global memory failure handler.
 */
static void allocNotifier(Ejs *ejs, uint size, uint total, bool granted)
{
    MprAlloc    *alloc;
    EjsVar      *argv[2], *thisObj;
	va_list     dummy = VA_NULL;
    char        msg[MPR_MAX_STRING];

    if (!ejs->exception) {
        ejs->attention = 1;
        alloc = mprGetAllocStats(ejs);
        if (granted) {
            if (ejs->memoryCallback) {
                argv[0] = (EjsVar*) ejsCreateNumber(ejs, size);
                argv[1] = (EjsVar*) ejsCreateNumber(ejs, total);
                thisObj = ejs->memoryCallback->thisObj ? ejs->memoryCallback->thisObj : ejs->global; 
                ejsRunFunction(ejs, ejs->memoryCallback, thisObj, 2, argv);
            } else {
                /* Static to avoid a memory allocation */
                mprSprintf(msg, sizeof(msg), "Low memory condition. Total mem: %d. Request for %d bytes granted.", 
                    total, size);
                ejsCreateException(ejs, ES_MemoryError, msg, dummy);
            }

        } else {
            mprSprintf(msg, sizeof(msg), "Memory depleted. Total mem: %d. Request for %d bytes denied.", total, size);
            ejsCreateException(ejs, ES_MemoryError, msg, dummy);
        }
    }
}


/*
 *  Set the module search path
 */
void ejsSetSearchPath(Ejs *ejs, cchar *searchPath)
{
    mprAssert(ejs);
    mprAssert(searchPath && searchPath);

    setDefaultSearchPath(ejs);
    ejsAppendSearchPath(ejs, searchPath);

    mprLog(ejs, 4, "ejs: Set search path to %s", ejs->ejsPath);
}


/*
 *  Append a search path to the system defaults
 */
void ejsAppendSearchPath(Ejs *ejs, cchar *searchPath)
{
    char    *oldPath;

    mprAssert(ejs);
    mprAssert(searchPath && searchPath);
    mprAssert(ejs->ejsPath);

    oldPath = ejs->ejsPath;
    mprAssert(oldPath);
    ejs->ejsPath = mprAsprintf(ejs, -1, "%s" MPR_SEARCH_SEP "%s", oldPath, searchPath);
    mprFree(oldPath);
    mprLog(ejs, 3, "ejs: set search path to %s", ejs->ejsPath);
}


/*
 *  Create a default module search path. This is set to:
 *      APP_EXE_DIR : MOD_DIR : .
 *  Where MOD_DIR is either the installed application modules directory or the local source modules directory.
 */
static void setDefaultSearchPath(Ejs *ejs)
{
    char    *search;

/*
 *  Always use the same directory as the app executable. This is sufficient for windows. For linux and if 
 *  running installed, use BLD_MOD_PREFIX. If running locally, BLD_ABS_MOD_DIR.
 */
#if VXWORKS
    search = mprGetCurrentPath(ejs);
#elif WIN
    search = mprAsprintf(ejs, -1, "%s" MPR_SEARCH_SEP ".", mprGetAppDir(ejs));
#else
    search = mprAsprintf(ejs, -1, "%s" MPR_SEARCH_SEP "%s" MPR_SEARCH_SEP ".", mprGetAppDir(ejs),
        mprSamePath(ejs, BLD_BIN_PREFIX, mprGetAppDir(ejs)) ? BLD_MOD_PREFIX: BLD_ABS_MOD_DIR);
#endif
    mprFree(ejs->ejsPath);
    ejs->ejsPath = search;
    mprLog(ejs, 4, "ejs: set default search path to %s", ejs->ejsPath);
}


EjsVar *ejsGetGlobalObject(Ejs *ejs)
{
    return (EjsVar*) ejs->global;
}


void ejsSetHandle(Ejs *ejs, void *handle)
{
    ejs->handle = handle;
}


void *ejsGetHandle(Ejs *ejs)
{
    return ejs->handle;
}


/*
 *  Evaluate a module file
 */
int ejsEvalModule(cchar *path)
{
    EjsService      *vm;   
    Ejs             *ejs;
    Mpr             *mpr;

    mpr = mprCreate(0, NULL, NULL);
    if ((vm = ejsCreateService(mpr)) == 0) {
        mprFree(mpr);
        return MPR_ERR_NO_MEMORY;
    }
    if ((ejs = ejsCreate(vm, NULL, NULL, 0)) == 0) {
        mprFree(mpr);
        return MPR_ERR_NO_MEMORY;
    }
    if (ejsLoadModule(ejs, path, -1, -1, 0, NULL) < 0) {
        mprFree(mpr);
        return MPR_ERR_CANT_READ;
    }
    if (ejsRun(ejs) < 0) {
        mprFree(mpr);
        return EJS_ERR;
    }
    mprFree(mpr);
    return 0;
}


/*
 *  Run a program. This will run a program assuming that all the required modules are already loaded. It will
 *  optionally service events until instructed to exit.
 */
int ejsRunProgram(Ejs *ejs, cchar *className, cchar *methodName)
{
    /*
     *  Run all module initialization code. This includes plain old scripts.
     */
    if (ejsRun(ejs) < 0) {
        return EJS_ERR;
    }
    /*
     *  Run the requested method. This will block until completion
     */
    if (className || methodName) {
        if (runSpecificMethod(ejs, className, methodName) < 0) {
            return EJS_ERR;
        }
    }
    if (ejs->flags & EJS_FLAG_NOEXIT) {
        /*
         *  This will service events until App.exit() is called
         */
        mprServiceEvents(ejs->dispatcher, -1, MPR_SERVICE_EVENTS);
    }
    return 0;
}


/*
 *  Run the specified method in the named class. If methodName is null, default to "main".
 *  If className is null, search for the first class containing the method name.
 */
static int runSpecificMethod(Ejs *ejs, cchar *className, cchar *methodName)
{
    EjsType         *type;
    EjsFunction     *fun;
    EjsName         qname;
    EjsVar          *args;
    int             attributes, i;

    type = 0;
    if (className == 0 && methodName == 0) {
        return 0;
    }

    if (methodName == 0) {
        methodName = "main";
    }

    /*
     *  Search for the first class with the given name
     */
    if (className == 0) {
        if (searchForMethod(ejs, methodName, &type) < 0) {
            return EJS_ERR;
        }

    } else {
        ejsName(&qname, EJS_PUBLIC_NAMESPACE, className);
        type = (EjsType*) ejsGetPropertyByName(ejs, ejs->global, &qname);
    }

    if (type == 0 || !ejsIsType(type)) {
        mprError(ejs, "Can't find class \"%s\"", className);
        return EJS_ERR;
    }

    ejsName(&qname, EJS_PUBLIC_NAMESPACE, methodName);
    fun = (EjsFunction*) ejsGetPropertyByName(ejs, (EjsVar*) type, &qname);
    if (fun == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    if (! ejsIsFunction(fun)) {
        mprError(ejs, "Property \"%s\" is not a function");
        return MPR_ERR_BAD_STATE;
    }

    attributes = ejsGetTypePropertyAttributes(ejs, (EjsVar*) type, fun->slotNum);
    if (!(attributes & EJS_ATTR_STATIC)) {
        mprError(ejs, "Method \"%s\" is not declared static");
        return EJS_ERR;
    }

    args = (EjsVar*) ejsCreateArray(ejs, ejs->argc);
    for (i = 0; i < ejs->argc; i++) {
        ejsSetProperty(ejs, args, i, (EjsVar*) ejsCreateString(ejs, ejs->argv[i]));
    }

    if (ejsRunFunction(ejs, fun, 0, 1, &args) == 0) {
        return EJS_ERR;
    }
    return 0;
}


/*
 *  Search for the named method in all types.
 */
static int searchForMethod(Ejs *ejs, cchar *methodName, EjsType **typeReturn)
{
    EjsFunction *method;
    EjsType     *type;
    EjsName     qname;
    EjsVar      *global, *vp;
    int         globalCount, slotNum, methodCount;
    int         methodSlot;

    mprAssert(methodName && *methodName);
    mprAssert(typeReturn);

    global = ejs->global;
    globalCount = ejsGetPropertyCount(ejs, global);

    /*
     *  Search for the named method in all types
     */
    for (slotNum = 0; slotNum < globalCount; slotNum++) {
        vp = ejsGetProperty(ejs, global, slotNum);
        if (vp == 0 || !ejsIsType(vp)) {
            continue;
        }
        type = (EjsType*) vp;

        methodCount = ejsGetPropertyCount(ejs, (EjsVar*) type);

        for (methodSlot = 0; methodSlot < methodCount; methodSlot++) {
            method = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, methodSlot);
            if (method == 0) {
                continue;
            }

            qname = ejsGetPropertyName(ejs, (EjsVar*) type, methodSlot);
            if (qname.name && strcmp(qname.name, methodName) == 0) {
                *typeReturn = type;
            }
        }
    }
    return 0;
}


static void logHandler(MprCtx ctx, int flags, int level, const char *msg)
{
    Mpr         *mpr;
    MprFile     *file;
    char        *prefix;

    mpr = mprGetMpr(ctx);
    file = (MprFile*) mpr->logHandlerData;
    prefix = mpr->name;

    while (*msg == '\n') {
        mprFprintf(file, "\n");
        msg++;
    }

    if (flags & MPR_LOG_SRC) {
        mprFprintf(file, "%s: %d: %s\n", prefix, level, msg);

    } else if (flags & MPR_ERROR_SRC) {
        /*
         *  Use static printing to avoid malloc when the messages are small.
         *  This is important for memory allocation errors.
         */
        if (strlen(msg) < (MPR_MAX_STRING - 32)) {
            mprStaticPrintfError(file, "%s: Error: %s\n", prefix, msg);
        } else {
            mprFprintf(file, "%s: Error: %s\n", prefix, msg);
        }

    } else if (flags & MPR_FATAL_SRC) {
        mprFprintf(file, "%s: Fatal: %s\n", prefix, msg);
        
    } else if (flags & MPR_RAW) {
        mprFprintf(file, "%s", msg);
    }
}


int ejsStartLogging(Mpr *mpr, char *logSpec)
{
    MprFile     *file;
    char        *levelSpec;
    int         level;

    level = 0;
    logSpec = mprStrdup(mpr, logSpec);

    if ((levelSpec = strchr(logSpec, ':')) != 0) {
        *levelSpec++ = '\0';
        level = atoi(levelSpec);
    }

    if (strcmp(logSpec, "stdout") == 0) {
        file = mpr->fileSystem->stdOutput;

    } else if (strcmp(logSpec, "stderr") == 0) {
        file = mpr->fileSystem->stdError;

    } else {
        if ((file = mprOpen(mpr, logSpec, O_CREAT | O_WRONLY | O_TRUNC | O_TEXT, 0664)) == 0) {
            mprPrintfError(mpr, "Can't open log file %s\n", logSpec);
            mprFree(logSpec);
            return EJS_ERR;
        }
    }

    mprSetLogLevel(mpr, level);
    mprSetLogHandler(mpr, logHandler, (void*) file);

    mprFree(logSpec);
    return 0;
}


/*
 *  Global memory allocation handler. This is invoked when there is no notifier to handle an allocation failure.
 *  The interpreter has an allocNotifier (see ejsService: allocNotifier) and it will handle allocation errors.
 */
void ejsMemoryFailure(MprCtx ctx, int64 size, int64 total, bool granted)
{
    if (!granted) {
        mprPrintfError(ctx, "Can't allocate memory block of size %d\n", size);
        mprPrintfError(ctx, "Total memory used %d\n", total);
        exit(255);
    }
    mprPrintfError(ctx, "Memory request for %d bytes exceeds memory red-line\n", size);
    mprPrintfError(ctx, "Total memory used %d\n", total);
}


void ejsReportError(Ejs *ejs, char *fmt, ...)
{
    va_list     arg;
    const char  *msg;
    char        *buf;

    va_start(arg, fmt);
    
    /*
     *  Compiler error format is:
     *      program:line:errorCode:SEVERITY: message
     *  Where program is either "ec" or "ejs"
     *  Where SEVERITY is either "error" or "warn"
     */
    buf = mprVasprintf(ejs, 0, fmt, arg);
    msg = ejsGetErrorMsg(ejs, 1);
    
    mprError(ejs, "%s", (msg) ? msg: buf);
    mprFree(buf);
    va_end(arg);
}

#if BLD_FEATURE_MULTITHREAD
void ejsLockVm(Ejs *ejs)
{
    mprLock(ejs->mutex);
}

void ejsUnlockVm(Ejs *ejs)
{
    mprUnlock(ejs->mutex);
}
#endif

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsService.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/vm/ejsVar.c"
 */
/************************************************************************/

/**
    ejsVar.c - Helper methods for the ejsVar interface.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static EjsVar *getProperty(Ejs *ejs, EjsVar *vp, int slotNum);
static MprNumber parseNumber(Ejs *ejs, cchar *str);
static bool      parseBoolean(Ejs *ejs, cchar *s);

/*
    Get the type owning a property
 */
static inline EjsType *getOwningType(EjsVar *vp, int slotNum)
{
    EjsType     *type;

    type = vp->type;
    if (type->hasNativeBase) {
        if (vp->isType) {
            if (slotNum < type->block.numInherited) {
                do {
                    type = type->baseType;
                } while (slotNum < type->block.numInherited);
            }
        } else if (type->instanceBlock) {
            if (slotNum < type->instanceBlock->numInherited) {
                do {
                    type = type->baseType;
                } while (slotNum < type->instanceBlock->numInherited);
            }
        }
    }
    return type;
}

/**
    Cast the variable to a given target type.
    @return Returns a variable with the result of the cast or null if an exception is thrown.
 */
EjsVar *ejsCastVar(Ejs *ejs, EjsVar *vp, EjsType *targetType)
{
    mprAssert(ejs);
    mprAssert(targetType);

    if (vp == 0) {
        vp = ejs->undefinedValue;
    }
    if (vp->type == targetType) {
        return vp;
    }
    if (vp->type->helpers->castVar) {
        return (vp->type->helpers->castVar)(ejs, vp, targetType);
    }
    ejsThrowInternalError(ejs, "Cast helper not defined for type \"%s\"", vp->type->qname.name);
    return 0;
}


/*
    Create a new instance of a variable. Delegate to the type specific create.
 */
EjsVar *ejsCreateVar(Ejs *ejs, EjsType *type, int numSlots)
{
#if VXWORKS
    /*
     *  The VxWorks cc386 invoked linker crashes without this test. Ugh!
     */
    if (type == 0) {
        return 0;
    }
#endif
    mprAssert(type->helpers->createVar);
    return (type->helpers->createVar)(ejs, type, numSlots);
}


/**
    Copy a variable by copying all properties. If a property is a reference  type, just copy the reference.
    See ejsDeepClone for a complete recursive copy of all reference contents.
    @return Returns a variable or null if an exception is thrown.
 */
EjsVar *ejsCloneVar(Ejs *ejs, EjsVar *vp, bool deep)
{
    if (vp == 0) {
        return 0;
    }
    mprAssert(vp->type->helpers->cloneVar);
    return (vp->type->helpers->cloneVar)(ejs, vp, deep);
}


/*
    Define a property and its traits.
    @return Return the slot number allocated for the property.
 */
int ejsDefineProperty(Ejs *ejs, EjsVar *vp, int slotNum, EjsName *name, EjsType *propType, int attributes, EjsVar *value)
{
    EjsName     qname;

    mprAssert(name);
    mprAssert(name->name);
    mprAssert(name->space);
    
    if (ejs->flags & EJS_FLAG_COMPILER) {
        qname.name = mprStrdup(vp, name->name);
        qname.space = mprStrdup(vp, name->space);
        name = &qname;
    }
    mprAssert(vp->type->helpers->defineProperty);
    return (vp->type->helpers->defineProperty)(ejs, vp, slotNum, name, propType, attributes, value);
}


/**
    Delete a property in an object variable. The stack is unchanged.
    @return Returns a status code.
 */
int ejsDeleteProperty(Ejs *ejs, EjsVar *vp, int slotNum)
{
    EjsType     *type;

    mprAssert(slotNum >= 0);
    
    type = getOwningType(vp, slotNum);
    mprAssert(type->helpers->deleteProperty);
    return (type->helpers->deleteProperty)(ejs, vp, slotNum);
}


/**
    Delete a property in an object variable. The stack is unchanged.
    @return Returns a status code.
 */
int ejsDeletePropertyByName(Ejs *ejs, EjsVar *vp, EjsName *qname)
{
    EjsLookup   lookup;
    int         slotNum;

    mprAssert(qname);
    mprAssert(qname->name);
    mprAssert(qname->space);
    
    if (vp->type->helpers->deletePropertyByName) {
        return (vp->type->helpers->deletePropertyByName)(ejs, vp, qname);
    } else {
        slotNum = ejsLookupVar(ejs, vp, qname, &lookup);
        if (slotNum < 0) {
            ejsThrowReferenceError(ejs, "Property \"%s\" does not exist", qname->name);
            return 0;
        }
        return ejsDeleteProperty(ejs, vp, slotNum);
    }
}


void ejsDestroyVar(Ejs *ejs, EjsVar *vp)
{
    EjsType     *type;

    mprAssert(vp);

    type = vp->type;
    mprAssert(type->helpers->destroyVar);
    (type->helpers->destroyVar)(ejs, vp);
}


/**
    Get a property at a given slot in a variable.
    @return Returns the requested property varaible.
 */
EjsVar *ejsGetProperty(Ejs *ejs, EjsVar *vp, int slotNum)
{
    EjsType     *type;

    mprAssert(ejs);
    mprAssert(vp);
    mprAssert(slotNum >= 0);

    type = getOwningType(vp, slotNum);
    mprAssert(type->helpers->getProperty);
    return (type->helpers->getProperty)(ejs, vp, slotNum);
}


/*
    Get a property given a name.
 */
EjsVar *ejsGetPropertyByName(Ejs *ejs, EjsVar *vp, EjsName *name)
{
    int     slotNum;

    mprAssert(ejs);
    mprAssert(vp);
    mprAssert(name);

    /*
     *  WARNING: this is not implemented by most types
     */
    if (vp->type->helpers->getPropertyByName) {
        return (vp->type->helpers->getPropertyByName)(ejs, vp, name);
    }

    /*
     *  Fall back and use a two-step lookup and get
     */
    slotNum = ejsLookupProperty(ejs, vp, name);
    if (slotNum < 0) {
        return 0;
    }
    return ejsGetProperty(ejs, vp, slotNum);
}


/**
    Return the number of properties in the variable.
    @return Returns the number of properties.
 */
int ejsGetPropertyCount(Ejs *ejs, EjsVar *vp)
{
    mprAssert(vp->type->helpers->getPropertyCount);
    return (vp->type->helpers->getPropertyCount)(ejs, vp);
}


/**
    Return the name of a property indexed by slotNum.
    @return Returns the property name.
 */
EjsName ejsGetPropertyName(Ejs *ejs, EjsVar *vp, int slotNum)
{
    EjsType     *type;

    type = getOwningType(vp, slotNum);
    mprAssert(type->helpers->getPropertyName);
    return (type->helpers->getPropertyName)(ejs, vp, slotNum);
}


/**
    Return the trait for the indexed by slotNum.
    @return Returns the property name.
 */
EjsTrait *ejsGetPropertyTrait(Ejs *ejs, EjsVar *vp, int slotNum)
{
    EjsType     *type;

    type = getOwningType(vp, slotNum);
    mprAssert(type->helpers->getPropertyTrait);
    return (type->helpers->getPropertyTrait)(ejs, vp, slotNum);
}


/**
    Get a property slot. Lookup a property name and return the slot reference. If a namespace is supplied, the property
    must be defined with the same namespace.
    @return Returns the slot number or -1 if it does not exist.
 */
int ejsLookupProperty(Ejs *ejs, EjsVar *vp, EjsName *name)
{
    mprAssert(ejs);
    mprAssert(vp);
    mprAssert(name);
    mprAssert(name->name);
    mprAssert(name->space);

    mprAssert(vp->type->helpers->lookupProperty);
    return (vp->type->helpers->lookupProperty)(ejs, vp, name);
}


/*
    Invoke an operator.
    vp is left-hand-side
    @return Return a variable with the result or null if an exception is thrown.
 */
EjsVar *ejsInvokeOperator(Ejs *ejs, EjsVar *vp, int opCode, EjsVar *rhs)
{
    mprAssert(vp);

    mprAssert(vp->type->helpers->invokeOperator);
    return (vp->type->helpers->invokeOperator)(ejs, vp, opCode, rhs);
}


/*
    ejsMarkVar is in ejsGarbage.c
 */


/*
    Set a property and return the slot number. Incoming slot may be -1 to allocate a new slot.
 */
int ejsSetProperty(Ejs *ejs, EjsVar *vp, int slotNum, EjsVar *value)
{
    if (vp == 0) {
        ejsThrowReferenceError(ejs, "Object is null");
        return EJS_ERR;
    }
    mprAssert(vp->type->helpers->setProperty);
    return (vp->type->helpers->setProperty)(ejs, vp, slotNum, value);
}


/*
    Set a property given a name.
 */
int ejsSetPropertyByName(Ejs *ejs, EjsVar *vp, EjsName *qname, EjsVar *value)
{
    int     slotNum;

    mprAssert(ejs);
    mprAssert(vp);
    mprAssert(qname);

    /*
     *  WARNING: Not all types implement this
     */
    if (vp->type->helpers->setPropertyByName) {
        return (vp->type->helpers->setPropertyByName)(ejs, vp, qname, value);
    }

    /*
     *  Fall back and use a two-step lookup and get
     */
    slotNum = ejsLookupProperty(ejs, vp, qname);
    if (slotNum < 0) {
        slotNum = ejsSetProperty(ejs, vp, -1, value);
        if (slotNum < 0) {
            return EJS_ERR;
        }
        if (ejsSetPropertyName(ejs, vp, slotNum, qname) < 0) {
            return EJS_ERR;
        }
        return slotNum;
    }
    return ejsSetProperty(ejs, vp, slotNum, value);
}


/*
    Set the property name and return the slot number. Slot may be -1 to allocate a new slot.
 */
int ejsSetPropertyName(Ejs *ejs, EjsVar *vp, int slot, EjsName *qname)
{
    mprAssert(vp->type->helpers->setPropertyName);
    return (vp->type->helpers->setPropertyName)(ejs, vp, slot, qname);
}


int ejsSetPropertyTrait(Ejs *ejs, EjsVar *vp, int slot, EjsType *propType, int attributes)
{
    mprAssert(vp->type->helpers->setPropertyTrait);
    return (vp->type->helpers->setPropertyTrait)(ejs, vp, slot, propType, attributes);
}


/**
    Get a string representation of a variable.
    @return Returns a string variable or null if an exception is thrown.
 */
EjsString *ejsToString(Ejs *ejs, EjsVar *vp)
{
    EjsFunction     *fn;

    if (vp == 0) {
        return ejsCreateString(ejs, "undefined");
    } else if (ejsIsString(vp)) {
        return (EjsString*) vp;
    }

    /*
     *  Types can provide a toString method or a castVar helper
     */
    if (vp->type->helpers->getProperty != getProperty) { 
        fn = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) vp->type, ES_Object_toString);
        if (ejsIsFunction(fn)) {
            return (EjsString*) ejsRunFunction(ejs, fn, vp, 0, NULL);
        }
    }
    if (vp->type->helpers->castVar) {
        return (EjsString*) (vp->type->helpers->castVar)(ejs, vp, ejs->stringType);
    }
    ejsThrowInternalError(ejs, "CastVar helper not defined for type \"%s\"", vp->type->qname.name);
    return 0;
}


/**
    Get a numeric representation of a variable.
    @return Returns a number variable or null if an exception is thrown.
 */
EjsNumber *ejsToNumber(Ejs *ejs, EjsVar *vp)
{
    if (vp == 0 || ejsIsNumber(vp)) {
        return (EjsNumber*) vp;
    }
    if (vp->type->helpers->castVar) {
        return (EjsNumber*) (vp->type->helpers->castVar)(ejs, vp, ejs->numberType);
    }
    ejsThrowInternalError(ejs, "CastVar helper not defined for type \"%s\"", vp->type->qname.name);
    return 0;
}


/**
    Get a boolean representation of a variable.
    @return Returns a number variable or null if an exception is thrown.
 */
EjsBoolean *ejsToBoolean(Ejs *ejs, EjsVar *vp)
{
    if (vp == 0 || ejsIsBoolean(vp)) {
        return (EjsBoolean*) vp;
    }
    if (vp->type->helpers->castVar) {
        return (EjsBoolean*) (vp->type->helpers->castVar)(ejs, vp, ejs->booleanType);
    }
    ejsThrowInternalError(ejs, "CastVar helper not defined for type \"%s\"", vp->type->qname.name);
    return 0;
}


/**
    Get a serialized string representation of a variable using JSON encoding.
    @return Returns a string variable or null if an exception is thrown.
 */
EjsString *ejsToJson(Ejs *ejs, EjsVar *vp)
{
    EjsFunction     *fn;
    EjsString       *result;

    if (vp == 0) {
        return ejsCreateString(ejs, "undefined");
    }
    if (vp->jsonVisited) {
        return ejsCreateString(ejs, "this");
    }
    vp->jsonVisited = 1;

    /*
     *  Types can provide a toJSON method, a serializeVar helper. If neither are provided, toString is used as a fall-back.
     */
    fn = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) vp->type, ES_Object_toJSON);
    if (ejsIsFunction(fn)) {
        result = (EjsString*) ejsRunFunction(ejs, fn, vp, 0, NULL);
    } else {
        result = ejsToString(ejs, vp);
    }
    vp->jsonVisited = 0;
    return result;
}


/*
    Fully construct a new object. We create a new instance and call all required constructors.
 */
EjsVar *ejsCreateInstance(Ejs *ejs, EjsType *type, int argc, EjsVar **argv)
{
    EjsFunction     *fun;
    EjsVar          *vp;
    int             slotNum;

    mprAssert(type);

    vp = ejsCreateVar(ejs, type, 0);
    if (vp == 0) {
        ejsThrowMemoryError(ejs);
        return 0;
    }
    if (type->hasConstructor) {
        slotNum = type->block.numInherited;
        fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, slotNum);
        if (fun == 0) {
            return 0;
        }
        if (!ejsIsFunction(fun)) {
            return 0;
        }
        vp->permanent = 1;
        ejsRunFunction(ejs, fun, vp, argc, argv);
        vp->permanent = 0;
    }
    return vp;
}


/*
    Report reference errors
 */
static void reportError(Ejs *ejs, EjsVar *vp)
{
    if (ejsIsNull(vp)) {
        ejsThrowReferenceError(ejs, "Object reference is null");

    } else if (ejsIsUndefined(vp)) {
        ejsThrowReferenceError(ejs, "Reference is undefined");

    } else {
        ejsThrowReferenceError(ejs, "Undefined setProperty helper");
    }
}


static EjsVar *createVar(Ejs *ejs, EjsType *type, int size)
{
    EjsVar      *vp;

    if (size == 0 && (vp = ejsAllocPooledVar(ejs, type->id)) != NULL) {
        memset(vp, 0, type->instanceSize);
        vp->type = type;
        return vp;
    }
    return ejsAllocVar(ejs, type, size);
}


static EjsVar *castVar(Ejs *ejs, EjsVar *vp, EjsType *toType)
{
    EjsString   *result;
    char        *buf;

    buf = mprAsprintf(ejs, -1, "[object %s]", vp->type->qname.name);
    result = ejsCreateString(ejs, buf);
    mprFree(buf);
    return (EjsVar*) result;
}


/*
    Default clone is just to do a shallow copy since most types are immutable.
 */
static EjsVar *cloneVar(Ejs *ejs, EjsVar *vp, bool depth)
{
    return vp;
}


static int defineProperty(Ejs *ejs, EjsVar *vp, int slotNum, EjsName *name, EjsType *propType, int attributes, EjsVar *value)
{
    reportError(ejs, vp);
    return EJS_ERR;
}


static int deleteProperty(Ejs *ejs, EjsVar *vp, int slotNum)
{
    reportError(ejs, vp);
    return EJS_ERR;
}


static void destroyVar(Ejs *ejs, EjsVar *vp)
{
    ejsFreeVar(ejs, vp, -1);
}


static EjsVar *getProperty(Ejs *ejs, EjsVar *vp, int slotNum)
{
    reportError(ejs, vp);
    return 0;
}


static int getPropertyCount(Ejs *ejs, EjsVar *vp)
{
    return 0;
}


static EjsName getPropertyName(Ejs *ejs, EjsVar *vp, int slotNum)
{
    static EjsName  qname;

    reportError(ejs, vp);

    qname.name = 0;
    qname.space = 0;

    return qname;
}


static EjsTrait *getPropertyTrait(Ejs *ejs, EjsVar *vp, int slotNum)
{
    return ejsGetTrait((EjsBlock*) vp->type, slotNum);
}


static EjsVar *invokeOperator(Ejs *ejs, EjsVar *lhs, int opCode, EjsVar *rhs)
{
    switch (opCode) {

    case EJS_OP_BRANCH_EQ:
    case EJS_OP_BRANCH_STRICTLY_EQ:
        return (EjsVar*) ejsCreateBoolean(ejs, lhs == rhs);

    case EJS_OP_BRANCH_NE:
    case EJS_OP_BRANCH_STRICTLY_NE:
        return (EjsVar*) ejsCreateBoolean(ejs, !(lhs == rhs));

    default:
        /*
         *  Pass to the standard Object helpers to implement Object methods.
         */
        return (ejs->objectHelpers->invokeOperator)(ejs, lhs, opCode, rhs);
    }
    return 0;
}


static int lookupProperty(Ejs *ejs, EjsVar *vp, EjsName *name)
{
    /*
     *  Don't throw or report error if lookupProperty is not implemented
     */
    return EJS_ERR;
}


static void markVar(Ejs *ejs, EjsVar *parent, EjsVar *vp)
{
}


static int setProperty(Ejs *ejs, EjsVar *vp, int slotNum, EjsVar *value)
{
    reportError(ejs, vp);
    return EJS_ERR;
}


static int setPropertyName(Ejs *ejs, EjsVar *vp, int slotNum, EjsName *name)
{
    reportError(ejs, vp);
    return EJS_ERR;
}


static int setPropertyTrait(Ejs *ejs, EjsVar *vp, int slotNum, EjsType *propType, int attributes)
{
    reportError(ejs, vp);
    return EJS_ERR;
}


/*
    Native types are responsible for completing the rest of the helpers
 */
void ejsInitializeDefaultHelpers(EjsTypeHelpers *helpers)
{
    helpers->castVar            = castVar;
    helpers->createVar          = createVar;
    helpers->cloneVar           = cloneVar;
    helpers->defineProperty     = defineProperty;
    helpers->deleteProperty     = deleteProperty;
    helpers->destroyVar         = destroyVar;
    helpers->getProperty        = getProperty;
    helpers->getPropertyTrait   = getPropertyTrait;
    helpers->getPropertyCount   = getPropertyCount;
    helpers->getPropertyName    = getPropertyName;
    helpers->invokeOperator     = invokeOperator;
    helpers->lookupProperty     = lookupProperty;
    helpers->markVar            = markVar;
    helpers->setProperty        = setProperty;
    helpers->setPropertyName    = setPropertyName;
    helpers->setPropertyTrait   = setPropertyTrait;
}


EjsTypeHelpers *ejsGetDefaultHelpers(Ejs *ejs)
{
    return (EjsTypeHelpers*) mprMemdup(ejs, ejs->defaultHelpers, sizeof(EjsTypeHelpers));
}


EjsTypeHelpers *ejsGetObjectHelpers(Ejs *ejs)
{
    /*
     *  . Does every type need a copy -- NO. But the loader uses this and allows
     *  native types to selectively override. See samples/types/native.
     */
    return (EjsTypeHelpers*) mprMemdup(ejs, ejs->objectHelpers, sizeof(EjsTypeHelpers));
}


EjsTypeHelpers *ejsGetBlockHelpers(Ejs *ejs)
{
    return (EjsTypeHelpers*) mprMemdup(ejs, ejs->blockHelpers, sizeof(EjsTypeHelpers));
}



EjsName *ejsAllocName(MprCtx ctx, cchar *name, cchar *space)
{
    EjsName     *np;

    np = mprAllocObj(ctx, EjsName);
    if (np) {
        np->name = mprStrdup(np, name);
        np->space = mprStrdup(np, space);
    }
    return np;
}


EjsName ejsCopyName(MprCtx ctx, EjsName *qname)
{
    EjsName     name;

    name.name = mprStrdup(ctx, qname->name);
    name.space = mprStrdup(ctx, qname->space);

    return name;
}


EjsName *ejsDupName(MprCtx ctx, EjsName *qname)
{
    return ejsAllocName(ctx, qname->name, qname->space);
}


EjsName *ejsName(EjsName *np, cchar *space, cchar *name)
{
    np->name = name;
    np->space = space;
    return np;
}


void ejsZeroSlots(Ejs *ejs, EjsVar **slots, int count)
{
    int     i;

    for (i = 0; i < count; i++) {
        slots[i] = ejs->nullValue;
    }
}


/*
    Parse a string based on formatting instructions and intelligently create a variable.
    Number formats:
        [(+|-)][0][OCTAL_DIGITS]
        [(+|-)][0][(x|X)][HEX_DIGITS]
        [(+|-)][DIGITS]
        [+|-][DIGITS][.][DIGITS][(e|E)[+|-]DIGITS]
 */
EjsVar *ejsParseVar(Ejs *ejs, cchar *buf, int preferredType)
{
    int         type;

    mprAssert(buf);

    type = preferredType;
    if (preferredType == ES_Void || preferredType < 0) {
        if ((*buf == '-' || *buf == '+') && isdigit((int) buf[1])) {
            type = ejs->numberType->id;

        } else if (!isdigit((int) *buf) && *buf != '.') {
            if (strcmp(buf, "true") == 0) {
                return (EjsVar*) ejs->trueValue;

            } else if (strcmp(buf, "false") == 0) {
                return (EjsVar*) ejs->falseValue;
            }
            type = ES_String;

        } else {
            type = ES_Number;
        }
    }

    switch (type) {
    case ES_Object:
    case ES_Void:
    case ES_Null:
    default:
        break;

    case ES_Number:
        return (EjsVar*) ejsCreateNumber(ejs, parseNumber(ejs, buf));

    case ES_Boolean:
        return (EjsVar*) ejsCreateBoolean(ejs, parseBoolean(ejs, buf));

    case ES_String:
        if (strcmp(buf, "null") == 0) {
            return (EjsVar*) ejsCreateNull(ejs);

        } else if (strcmp(buf, "undefined") == 0) {
            return (EjsVar*) ejsCreateUndefined(ejs);
        }
        return (EjsVar*) ejsCreateString(ejs, buf);
    }
    return (EjsVar*) ejsCreateUndefined(ejs);
}


/*
    Convert the variable to a number type. Only works for primitive types.
 */
static bool parseBoolean(Ejs *ejs, cchar *s)
{
    if (s == 0 || *s == '\0') {
        return 0;
    }
    if (strcmp(s, "false") == 0 || strcmp(s, "FALSE") == 0) {
        return 0;
    }
    return 1;
}


/*
    Convert the string buffer to a Number.
 */
static MprNumber parseNumber(Ejs *ejs, cchar *str)
{
    int64       num;
    int         radix, c, negative;

    mprAssert(str);

    num = 0;
    negative = 0;

    if (*str == '-') {
        str++;
        negative = 1;
    } else if (*str == '+') {
        str++;
    }

#if BLD_FEATURE_FLOATING_POINT
    if (*str != '.' && !isdigit((int) *str)) {
        return ejs->nanValue->value;
    }

    /*
        Floating format: [DIGITS].[DIGITS][(e|E)[+|-]DIGITS]
     */
    if (!(*str == '0' && tolower((int) str[1]) == 'x')) {
        MprNumber   n;
        cchar       *cp;
        for (cp = str; *cp; cp++) {
            if (*cp == '.' || tolower((int) *cp) == 'e') {
                n = atof(str);
                if (negative) {
                    n = 0.0 - n;
                }
                return n;
            }
        }
    }
#else
    if (*str != '.' && !isdigit(*str)) {
        return 0;
    }
#endif

    /*
        Parse an integer. Observe hex and octal prefixes (0x, 0).
     */
    if (*str != '0') {
        /*
         *  Normal numbers (Radix 10)
         */
        while (isdigit((int) *str)) {
            num = (*str - '0') + (num * 10);
            str++;
        }
    } else {
        str++;
        if (tolower((int) *str) == 'x') {
            str++;
            radix = 16;
            while (*str) {
                c = tolower((int) *str);
                if (isdigit(c)) {
                    num = (c - '0') + (num * radix);
                } else if (c >= 'a' && c <= 'f') {
                    num = (c - 'a' + 10) + (num * radix);
                } else {
                    break;
                }
                str++;
            }

        } else{
            radix = 8;
            while (*str) {
                c = tolower((int) *str);
                if (isdigit(c) && c < '8') {
                    num = (c - '0') + (num * radix);
                } else {
                    break;
                }
                str++;
            }
        }
    }

    if (negative) {
        return (MprNumber) (0 - num);
    }
    return (MprNumber) num;
}


int _ejsIs(struct EjsVar *vp, int slot)
{
    EjsType     *tp;

    if (vp == 0) {
        return 0;
    }
    if (vp->type->id == slot) {
        return 1;
    }
    for (tp = ((EjsVar*) vp)->type->baseType; tp; tp = tp->baseType) {
        if (tp->id == slot) {
            return 1;
        }
    }
    return 0;
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://www.embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/vm/ejsVar.c"
 */
/************************************************************************/

