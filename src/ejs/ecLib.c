#include "ejs.h"
#include "ec.h"

/******************************************************************************/
/* 
 *  This file is an amalgamation of all the individual source code files for
 *  Embedthis Ejscript 1.0.2.
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
 *  Start of file "../src/compiler/ecAst.c"
 */
/************************************************************************/

/**
 *  ecAst.c - Process AST nodes and define all variables.
 *
 *  Note on error handling. If a non-recoverable error occurs, then EcCompiler.hasFatalError will be set and
 *  processing will be aborted. If a recoverable error occurs, then hasError will be set and processing will continue.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  State level macros. Enter/Leave manage state and inheritance of state.
 */
#undef ENTER
#define ENTER(cp) if (ecEnterState(cp) < 0) { return; } else

#undef LEAVE
#define LEAVE(cp) ecLeaveState(cp)


static void     addGlobalProperty(EcCompiler *cp, EcNode *np, EjsName *qname);
static void     addScope(EcCompiler *cp, EjsBlock *block);
static void     allocName(Ejs *ejs, EjsName *qname);
static void     astBinaryOp(EcCompiler *cp, EcNode *np);
static void     astBindName(EcCompiler *cp, EcNode *np);
static void     astBlock(EcCompiler *cp, EcNode *np);
static void     astBreak(EcCompiler *cp, EcNode *np);
static void     astCall(EcCompiler *cp, EcNode *np);
static void     astCaseElements(EcCompiler *cp, EcNode *np);
static void     astCaseLabel(EcCompiler *cp, EcNode *np);
static void     astCatch(EcCompiler *cp, EcNode *np);
static void     astClass(EcCompiler *cp, EcNode *np);
static void     astDirectives(EcCompiler *cp, EcNode *np);
static void     astDot(EcCompiler *cp, EcNode *np);
static void     astDo(EcCompiler *cp, EcNode *np);
static void     astError(EcCompiler *cp, EcNode *np, char *fmt, ...);
static void     astExpressions(EcCompiler *cp, EcNode *np);
static void     astField(EcCompiler *cp, EcNode *np);
static void     astFor(EcCompiler *cp, EcNode *np);
static void     astForIn(EcCompiler *cp, EcNode *np);
static void     astFunction(EcCompiler *cp, EcNode *np);
static void     astHash(EcCompiler *cp, EcNode *np);
static void     astIf(EcCompiler *cp, EcNode *np);
static void     astName(EcCompiler *cp, EcNode *np);
static void     astNew(EcCompiler *cp, EcNode *np);
static void     astObjectLiteral(EcCompiler *cp, EcNode *np);
static void     astPostfixOp(EcCompiler *cp, EcNode *np);
static void     astPragmas(EcCompiler *cp, EcNode *np);
static void     astPragma(EcCompiler *cp, EcNode *np);
static void     astProgram(EcCompiler *cp, EcNode *np);
static void     astReturn(EcCompiler *cp, EcNode *np);
static void     astSuper(EcCompiler *cp, EcNode *np);
static void     astSwitch(EcCompiler *cp, EcNode *np);
static void     astThis(EcCompiler *cp, EcNode *np);
static void     astThrow(EcCompiler *cp, EcNode *np);
static void     astTry(EcCompiler *cp, EcNode *np);
static void     astUnaryOp(EcCompiler *cp, EcNode *np);
static void     astModule(EcCompiler *cp, EcNode *np);
static void     astUseNamespace(EcCompiler *cp, EcNode *np);
static void     astVar(EcCompiler *cp, EcNode *np, int varKind, EjsVar *value);
static void     astVarDefinition(EcCompiler *cp, EcNode *np, int *codeRequired, int *instanceCode);
static void     astVoid(EcCompiler *cp, EcNode *np);
static void     astWarn(EcCompiler *cp, EcNode *np, char *fmt, ...);
static void     astWith(EcCompiler *cp, EcNode *np);
static void     badAst(EcCompiler *cp, EcNode *np);
static void     bindVariableDefinition(EcCompiler *cp, EcNode *np);
static void     closeBlock(EcCompiler *cp);
static EjsNamespace *createHoistNamespace(EcCompiler *cp, EjsVar *obj);
static EjsModule    *createModule(EcCompiler *cp, EcNode *np);
static EjsFunction *createModuleInitializer(EcCompiler *cp, EcNode *np, EjsModule *mp);
static int      defineParameters(EcCompiler *cp, EcNode *np);
static void     defineVar(EcCompiler *cp, EcNode *np, int varKind, EjsVar *value);
static void     fixupBlockSlots(EcCompiler *cp, EjsType *type);
static EcNode   *getNextAstNode(EcCompiler *cp, EcNode *np, int *next);
static EjsVar   *getProperty(EcCompiler *cp, EjsVar *vp, EjsName *name);
static bool     hoistBlockVar(EcCompiler *cp, EcNode *np);
static void     openBlock(EcCompiler *cp, EcNode *np, EjsBlock *block);
static void     processAstNode(EcCompiler *cp, EcNode *np);
static void     removeProperty(EcCompiler *cp, EjsVar *block, EcNode *np);
static EjsNamespace *resolveNamespace(EcCompiler *cp, EcNode *np, EjsVar *block, bool *modified);
static void     removeScope(EcCompiler *cp);
static int      resolveName(EcCompiler *cp, EcNode *node, EjsVar *vp,  EjsName *name);
static void     setAstDocString(Ejs *ejs, EcNode *np, EjsVar *block616G, int slotNum);
static EjsNamespace *ejsLookupNamespace(Ejs *ejs, cchar *namespace);

/*
 *  Top level AST node processing.
 */
static int astProcess(EcCompiler *cp, EcNode *np)
{
    Ejs     *ejs;
    int     phase;

    if (ecEnterState(cp) < 0) {
        return EJS_ERR;
    }

    ejs = cp->ejs;
    cp->blockState = cp->state;

    /*
     *  We do 5 phases over all the nodes: define, conditional, fixup, bind and erase
     */
    for (phase = 0; phase < EC_AST_PHASES && cp->errorCount == 0; phase++) {

        /*
         *  Looping through the input source files. A single top level node describes the source file.
         */
        cp->phase = phase;
        cp->fileState = cp->state;
        cp->fileState->mode = cp->defaultMode;
        cp->fileState->lang = cp->lang;

        processAstNode(cp, np);
    }
    ecLeaveState(cp);
    cp->fileState = 0;
    cp->blockState = 0;
    cp->error = 0;
    return (cp->errorCount > 0) ? EJS_ERR : 0;
}


int ecAstProcess(EcCompiler *cp, int argc, EcNode **nodes)
{
    Ejs         *ejs;
    EcNode      *np;
    int         phase, i;

    if (ecEnterState(cp) < 0) {
        return EJS_ERR;
    }

    ejs = cp->ejs;
    cp->blockState = cp->state;

    /*
     *  We do 5 phases over all the nodes: define, load, fixup, block vars and bind
     */
    for (phase = 0; phase < EC_AST_PHASES && cp->errorCount == 0; phase++) {
        cp->phase = phase;

        /*
         *  Loop over each source file
         */
        for (i = 0; i < argc && !cp->fatalError; i++) {
            /*
             *  Looping through the input source files. A single top level node describes the source file.
             */
            np = nodes[i];
            if (np == 0) {
                continue;
            }

            cp->fileState = cp->state;
            cp->fileState->mode = cp->defaultMode;
            cp->fileState->lang = cp->lang;

            processAstNode(cp, np);
        }
    }
    ecLeaveState(cp);
    cp->fileState = 0;
    cp->blockState = 0;
    cp->error = 0;
    return (cp->errorCount > 0) ? EJS_ERR : 0;
}


static void astArgs(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    ENTER(cp);

    mprAssert(np->kind == N_ARGS);

    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        processAstNode(cp, child);
    }
    LEAVE(cp);
}


/*
 *  Generate an assignment expression
 */
static void astAssignOp(EcCompiler *cp, EcNode *np)
{
    EcState     *state;
    EjsFunction *fun;
    int         rc, next;

    ENTER(cp);

    state = cp->state;
    rc = 0;
    next = 0;

    mprAssert(np->kind == N_ASSIGN_OP);
    mprAssert(np->left);
    mprAssert(np->right);

    if (state->inSettings && cp->phase >= EC_PHASE_BIND) {
        /*
         *  Assignment in a class initializer. The lhs must be scoped outside the block. The rhs must be scoped inside.
         */
        fun = state->currentFunction;
        openBlock(cp, state->currentFunctionNode->function.body, (EjsBlock*) fun);
        ejsDefineReservedNamespace(cp->ejs, (EjsBlock*) fun, 0, EJS_PRIVATE_NAMESPACE);
        processAstNode(cp, np->right);
        closeBlock(cp);

    } else {
        processAstNode(cp, np->right);
    }

    state->onLeft = 1;
    processAstNode(cp, np->left);
    LEAVE(cp);
}


/*
 *  Handle a binary operator. We recursively process left and right nodes.
 */
static void astBinaryOp(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np->kind == N_BINARY_OP);

    if (np->left) {
        processAstNode(cp, np->left);
    }
    if (np->right) {
        processAstNode(cp, np->right);
    }
    LEAVE(cp);
}


static void defineBlock(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EcState     *state;
    EjsBlock    *block;
    EjsVar      *letBlock;
    int         slotNum;

    ejs = cp->ejs;
    state = cp->state;
    letBlock = state->letBlock;

    mprAssert(cp->phase == EC_PHASE_CONDITIONAL);
    mprAssert(np->kind == N_BLOCK || np->kind == N_MODULE);

    block = np->blockRef;

    if (np->createBlockObject) {
        allocName(ejs, &np->qname);
        slotNum = ejsDefineProperty(ejs, letBlock, -1, &np->qname, block->obj.var.type, 0, (EjsVar*) block);
        if (slotNum < 0) {
            astError(cp, np, "Can't define block");

        } else {
            np->blockCreated = 1;
            if (letBlock == ejs->global) {
                addGlobalProperty(cp, np, &np->qname);
            }
        }
    } else {
        block->obj.var.hidden = 1;
    }
}


static void bindBlock(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EjsBlock    *block;
    int         rc;
    
    mprAssert(cp->phase == EC_PHASE_BIND);
    mprAssert(np->kind == N_BLOCK || np->kind == N_MODULE);

    ejs = cp->ejs;
    block = np->blockRef;
    mprAssert(block);

    rc = resolveName(cp, np, 0, &np->qname);

    if (np->blockCreated) {
        if (! np->createBlockObject) {
            mprAssert(cp->lookup.obj);
            mprAssert(np->lookup.slotNum >= 0);
            ejsDeleteProperty(ejs, np->lookup.obj, np->lookup.slotNum);
            np->blockCreated = 0;
            np->lookup.ref->hidden = 1;

        } else {
            /*
             *  Mark the parent block as needing to be created to hold this block.
             */
            if (cp->state->prev->letBlockNode) {
                cp->state->prev->letBlockNode->createBlockObject = 1;
            }
        }
    }
}


static void astBlock(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    ENTER(cp);
    
    if (cp->phase == EC_PHASE_BIND) {
        /*
         *  Bind the block here before processing the child nodes so we can mark the block as hidden if it will be expunged.
         */
        bindBlock(cp, np);
    }

    /*
     *  Open block will change state->letBlock which we need preserved in defineBlock. Use ENTER/LEAVE to save and restore.
     */
    ENTER(cp);
    openBlock(cp, np, NULL);

    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        processAstNode(cp, child);
    }

    closeBlock(cp);
    LEAVE(cp);

    if (cp->phase == EC_PHASE_CONDITIONAL) {
        /*
         *  Do define block after the variables have been processed. This allows us to determine if the block is 
         *  really needed.
         */
        defineBlock(cp, np);

        /*
         *  Try to hoist the block object itself
         */
        if (np->blockCreated && !hoistBlockVar(cp, np)) {
            cp->state->letBlockNode->createBlockObject = 1;
        }
    }
    LEAVE(cp);
}


static void astBreak(EcCompiler *cp, EcNode *np)
{
    mprAssert(np->kind == N_BREAK);
}


static void astCall(EcCompiler *cp, EcNode *np)
{
    EcState         *state;

    mprAssert(np->kind == N_CALL);

    ENTER(cp);
    
    state = cp->state;

    if (state->onLeft) {
        astError(cp, np, "Invalid call expression on the left hand side of assignment");
        LEAVE(cp);
        return;
    }

    if (np->right) {
        mprAssert(np->right->kind == N_ARGS);
        astArgs(cp, np->right);
    }
    processAstNode(cp, np->left);

    /*
     *  Propagate up the right side qname and lookup.
     */
    if (cp->phase >= EC_PHASE_BIND) {
        if (np->left) {
            np->lookup = np->left->lookup;
            np->qname = np->left->qname;
        }
    }

    LEAVE(cp);
}


static void astCaseElements(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    ENTER(cp);

    mprAssert(np->kind == N_CASE_ELEMENTS);

    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        processAstNode(cp, child);
    }
    LEAVE(cp);
}


static void astCaseLabel(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    ENTER(cp);

    mprAssert(np->kind == N_CASE_LABEL);

    if (np->caseLabel.kind == EC_SWITCH_KIND_CASE) {
        mprAssert(np->caseLabel.expression);
        processAstNode(cp, np->caseLabel.expression);

    } else {
        mprAssert(np->caseLabel.kind == EC_SWITCH_KIND_DEFAULT);
    }

    /*
     *  Process the directives for this case label
     */
    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        processAstNode(cp, child);
    }
    LEAVE(cp);
}


static void astCatch(EcCompiler *cp, EcNode *np)
{
    EjsBlock    *block;

    ENTER(cp);

    block = ejsCreateBlock(cp->ejs, 0);
    ejsSetDebugName(block, "catch");
    addScope(cp, block);

    processAstNode(cp, np->left);
    removeScope(cp);
    LEAVE(cp);
}


/*
 *  Define a new class
 */
static EjsType *defineClass(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsType         *type;
    EcState         *state;
    EcNode          *constructor;
    EjsName         qname;
    EjsNamespace    *nsp;
    int             attributes, slotNum;
    
    mprAssert(np->kind == N_CLASS);

    ejs = cp->ejs;
    state = cp->state;
    type = np->klass.ref;
    
    if (ecLookupVar(cp, ejs->global, &np->qname, 0) >= 0) {
        astError(cp, np, "%s Class %s is already defined.", np->qname.space, np->qname.name);
        return 0;
    }
    attributes = np->attributes | EJS_ATTR_OBJECT_HELPERS | EJS_ATTR_SLOTS_NEED_FIXUP;

    if (np->klass.isInterface) {
        attributes |= EJS_ATTR_INTERFACE;
    }

    /*
     *  Create the class
     */
    slotNum = ejsGetPropertyCount(ejs, ejs->global);
    allocName(ejs, &np->qname);
    type = ejsCreateType(ejs, &np->qname, state->currentModule, NULL, sizeof(EjsObject), slotNum, 0, 0, attributes, np);
    if (type == 0) {
        astError(cp, np, "Can't create type %s", type->qname.name);
        return 0;
    }
    np->klass.ref = type;
    type->compiling = 1;

    nsp = ejsDefineReservedNamespace(ejs, (EjsBlock*) type, &type->qname, EJS_PROTECTED_NAMESPACE);
    nsp->flags |= EJS_NSP_PROTECTED;

    nsp = ejsDefineReservedNamespace(ejs, (EjsBlock*) type, &type->qname, EJS_PRIVATE_NAMESPACE);
    nsp->flags |= EJS_NSP_PRIVATE;

    /*
     *  Define a property for the type in global
     */
    allocName(ejs, &np->qname);
    slotNum = ejsDefineProperty(ejs, ejs->global, slotNum, &np->qname, ejs->typeType, attributes, (EjsVar*) type);
    if (slotNum < 0) {
        astError(cp, np, "Can't install type %s",  np->qname.name);
        return 0;
    }

    /*
     *  Reserve two slots for the constructor and static initializer to ensure they are the first two non-inherited slots.
     *  These slots may be reclaimed during fixup not required. Instance initializers are prepended to the constructor.
     *  Set a dummy name for the constructor as it will be defined by calling defineFunction from astClass.
     */
    if (!type->isInterface) {
        ejsSetProperty(ejs, (EjsVar*) type, 0, ejs->nullValue);
        ejsSetPropertyName(ejs, (EjsVar*) type, 0, ejsName(&qname, "", ""));

        qname.name = mprStrcat(type, -1, type->qname.name, "-initializer", NULL);
        qname.space = EJS_INIT_NAMESPACE;
        ejsDefineProperty(ejs, (EjsVar*) type, 1, &qname, ejs->functionType, 0, ejs->nullValue);

        constructor = np->klass.constructor;
        if (constructor && !constructor->function.isDefaultConstructor) {
            type->hasConstructor = 1;
        }
    }
    return type;
}


static void validateFunction(EcCompiler *cp, EcNode *np, EjsFunction *spec, EjsFunction *fun)
{
}

static void validateClass(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EjsType     *type, *iface, *baseType;
    EjsName     qname;
    EjsFunction *fun;
    EjsVar      *vp;
    EcState     *state;
    int         next, i, count;

    ejs = cp->ejs;
    state = cp->state;
    type = np->klass.ref;

    baseType = type->baseType;
    if (baseType && baseType->final) {
        astError(cp, np, "Class \"%s\" is attempting to subclass a final class \"%s\"", type->qname.name, 
            baseType->qname.name);
    }

    /*
     *  Ensure the class implements all required implemented methods
     */
    for (next = 0; ((iface = (EjsType*) mprGetNextItem(type->implements, &next)) != 0); ) {
        count = iface->block.numTraits;
        for (i = 0; i < count; i++) {
            fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) iface, i);
            if (!ejsIsFunction(fun) || fun->isInitializer) {
                continue;
            }
            qname = ejsGetPropertyName(ejs, (EjsVar*) iface, i);
            vp = ejsGetPropertyByName(ejs, (EjsVar*) type, &qname);
            if (vp == 0 || !ejsIsFunction(vp)) {
                astError(cp, np, "Missing method \"%s\" required by interface \"%s\"", qname.name, iface->qname.name);
            } else {
                validateFunction(cp, np, fun, (EjsFunction*) vp);
            }
        }
    }
    if (type->implements) {
        if (mprGetListCount(type->implements) > 1 || (type->baseType && strcmp(type->baseType->qname.name, "Object") != 0)) {
            astError(cp, np, "Only one implements or one extends supported");
        }
    }        
}


/*
 *  Lookup the set of open namespaces for the required namespace for this class
 */
static void bindClass(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EjsType     *type;
    EjsFunction *fun;
    EjsModule   *mp;
    EcState     *state;
    int         slotNum;
    bool        modified;

    ejs = cp->ejs;
    state = cp->state;
    type = np->klass.ref;

    mprAssert(cp->phase == EC_PHASE_BIND);

    if (type->hasStaticInitializer) {
        /*
         *  Create the static initializer function. Code gen will fill out the code. The type must be on the scope chain.
         */
        mp = state->currentModule;

        fun = ejsCreateFunction(ejs, NULL, -1, 0, 0, cp->ejs->voidType, 0, mp->constants, NULL, cp->fileState->lang);
        fun->isInitializer = 1;
        np->klass.initializer = fun;
        slotNum = type->block.numInherited;
        if (type->hasConstructor) {
            slotNum++;
        }
        ejsSetProperty(ejs, (EjsVar*) type, slotNum, (EjsVar*) fun);
        ejsSetFunctionLocation(fun, (EjsVar*) type, slotNum);
    }

    modified = 0;
    if (!np->literalNamespace && resolveNamespace(cp, np, ejs->global, &modified) == 0) {
        return;
    }
    if (modified) {
        ejsSetTypeName(ejs, type, &np->qname);
    }
    addGlobalProperty(cp, np, &type->qname);

    if (np->klass.constructor == 0 && type->hasBaseConstructors) {
        mprAssert(type->hasConstructor == 1);
        type->hasConstructor = 1;
    }

    if (resolveName(cp, np, ejs->global, &type->qname) < 0) {
        return;
    }
    
    /*
     *  Now that all the properties are defined, make the entire class permanent.
     */
    if (!type->block.obj.var.dynamic) {
        ejsMakePermanent(ejs, (EjsVar*) type);
        ejsMakePermanent(ejs, (EjsVar*) type->instanceBlock);
    }
     setAstDocString(ejs, np, np->lookup.obj, np->lookup.slotNum);
}


/*
 *  Process a class node
 */
static void astClass(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsType         *type;
    EcState         *state;
    EcNode          *constructor;
    bool            hasStaticInitializer;

    mprAssert(np->kind == N_CLASS);
    
    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    cp->classState = state;
    type = np->klass.ref;
    
    if (np->klass.implements) {
        processAstNode(cp, np->klass.implements);
    }        
    if (state->disabled) {
        if (cp->phase == EC_PHASE_CONDITIONAL) {
            removeProperty(cp, ejs->global, np);
        }
        LEAVE(cp);
        return;
    }

    if (cp->phase == EC_PHASE_DEFINE) {
        type = defineClass(cp, np);

    } else if (cp->phase == EC_PHASE_FIXUP) {
        fixupBlockSlots(cp, type);

    } else if (cp->phase >= EC_PHASE_BIND) {
        validateClass(cp, np);
        bindClass(cp, np);
    }

    if (cp->error) {
        LEAVE(cp);
        return;
    }

    state->currentClass = type;
    state->currentClassName = type->qname;
    state->inClass = 1;
    state->inFunction = 0;

    /*
     *  Add the type to the scope chain and the static initializer if present. Use push frame to make it eaiser to
     *  pop the type off the scope chain later.
     */
    hasStaticInitializer = 0;
    addScope(cp, (EjsBlock*) type);
    if (np->klass.initializer) {
        openBlock(cp, np, (EjsBlock*) np->klass.initializer);
        hasStaticInitializer++;
    }

    if (cp->phase == EC_PHASE_FIXUP && type->baseType) {
        ejsInheritBaseClassNamespaces(ejs, type, type->baseType);
    }

    state->optimizedLetBlock = (EjsVar*) type;
    state->letBlock = (EjsVar*) type;
    state->varBlock = (EjsVar*) type;

    /*
     *  Process the class body
     */
    mprAssert(np->left->kind == N_DIRECTIVES);
    processAstNode(cp, np->left);

    /*
     *  Only need to do this if this is a default constructor, ie. does not exist in the class body.
     */
    constructor = np->klass.constructor;
    if (constructor && constructor->function.isDefaultConstructor) {
        astFunction(cp, constructor);
    }
    if (hasStaticInitializer) {
        closeBlock(cp);
    }
    removeScope(cp);
    LEAVE(cp);
}


static void astDirectives(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    Ejs         *ejs;
    int         next;

    mprAssert(np->kind == N_DIRECTIVES);

    ENTER(cp);

    ejs = cp->ejs;
    cp->state->blockNestCount++;
    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        processAstNode(cp, child);
    }
    cp->state->blockNestCount--;
    LEAVE(cp);
}


/*
 *  Handle a do statement
 */
static void astDo(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np->kind == N_DO);

    if (np->forLoop.cond) {
        processAstNode(cp, np->forLoop.cond);
    }
    if (np->forLoop.body) {
        processAstNode(cp, np->forLoop.body);
    }
    LEAVE(cp);
}


/*
 *  Handle property dereferencing via "." and "[". This routine will bind a
 *  name path reference into slot bindings if possible. The dot node is a
 *  binary node.
 *
 *          local.a.b.c
 *          arg.a.b.c
 *          obj.a.b.c
 *          static.a.b.c
 *          any[expression]
 *          unqualifiedName         - dynamic bound
 *          expression              - dynamic bound
 */
static void astDot(EcCompiler *cp, EcNode *np)
{
    EcState     *state;
    EcNode      *left;

    mprAssert(np->kind == N_DOT);
    mprAssert(np->left);
    mprAssert(np->right);

    ENTER(cp);

    state = cp->state;
    state->onLeft = 0;
    left = np->left;

    /*
     *  Optimize to assist with binding. Remove an expressions node which has a sole QNAME.
     */
    if (left && left->kind == N_EXPRESSIONS && left->left && left->left->kind == N_QNAME && left->right == 0) {
        np->left = np->left->left;
    }

    /*
     *  Process the left of the "."
     */
    processAstNode(cp, np->left);

    state->currentObjectNode = np->left;
    
    /*
     *  If the right is a terminal node, then assume the parent state's onLeft status
     */
    switch (np->right->kind) {
    case N_QNAME:
/*
 *  Need to allow obj[fun()] = 7
 *  case N_EXPRESSIONS: 
 */
    case N_LITERAL:
    case N_OBJECT_LITERAL:
        cp->state->onLeft = cp->state->prev->onLeft;
        break;

    default:
        break;
    }
    processAstNode(cp, np->right);

    /*
     *  Propagate up the right side qname and lookup.
     */
    if (cp->phase >= EC_PHASE_BIND) {
        np->lookup = np->right->lookup;
        np->qname = np->right->qname;
    }
    LEAVE(cp);
}


/*
 *  Process an expressions node
 */
static void astExpressions(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    mprAssert(np->kind == N_EXPRESSIONS);

    ENTER(cp);

    /*
     *  No current object when computing an expression. E.g. obj[a + b]
     *  We don't want obj set as the context object for a or b.
     */
    cp->state->currentObjectNode = 0;

    next = 0;
    while ((child = getNextAstNode(cp, np, &next)) != 0) {
        processAstNode(cp, child);
    }

    /*
     *  Propagate up the right side qname and lookup.
     */
    if (cp->phase >= EC_PHASE_BIND) {
        child = mprGetLastItem(np->children);
        if (child) {
            np->lookup = child->lookup;
            np->qname = child->qname;
        }
    }
    LEAVE(cp);
}


static EjsFunction *defineFunction(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EcState         *state;
    EcNode          *parameters;
    EjsFunction     *fun;
    EjsVar          *block;
    int             numArgs, numLocals, numExceptions;
    int             slotNum, attributes;

    mprAssert(np->kind == N_FUNCTION);
    mprAssert(cp->phase == EC_PHASE_DEFINE);

    ejs = cp->ejs;
    state = cp->state;

    if (np->function.isMethod) {
        block = state->varBlock;

    } else {
        block = state->optimizedLetBlock;
        if (state->optimizedLetBlock != state->varBlock) {
            state->letBlockNode->createBlockObject = 1;
        }
    }

    /*
     *  Check if this function has already been defined in this block. Can't check base classes yes. Must wait till 
     *  bindFunction()
     */
    slotNum = ejsLookupProperty(ejs, block, &np->qname);
    if (slotNum >= 0) {
        astError(cp, np, "Property \"%s\" is already defined.", np->qname);
        return 0;
    }

    parameters = np->function.parameters;
    numArgs = (parameters) ? mprGetListCount(parameters->children) : 0;
    numLocals = numExceptions = 0;

    /*
     *  Create a function object. Don't have code yet so we create without it. Can't resolve the return type yet, so we 
     *  leave it unset.
     */
    fun = ejsCreateFunction(ejs, 0, 0, numArgs, numExceptions, 0, np->attributes, state->currentModule->constants, 
        NULL, cp->fileState->lang);
    if (fun == 0) {
        astError(cp, np, "Can't create function \"%s\"", np->qname.name);
        return 0;
    }

    attributes = np->attributes;

    if (np->function.isConstructor) {
        slotNum = 0;
        attributes |= EJS_ATTR_CONSTRUCTOR;
        np->qname.space = EJS_CONSTRUCTOR_NAMESPACE;
    } else {
        slotNum = -1;
    }
    allocName(ejs, &np->qname);
    slotNum = ejsDefineProperty(ejs, block, slotNum, &np->qname, fun->block.obj.var.type, attributes, (EjsVar*) fun);
    if (slotNum < 0) {
        astError(cp, np, "Can't define function in type \"%s\"", state->currentClass->qname.name);
        return 0;
    }
    np->function.functionVar = fun;

    ejsSetDebugName(fun, np->qname.name);

    return fun;
}


/*
 *  Define function parameters during the DEFINE phase.
 */
static int defineParameters(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsFunction     *fun;
    EcNode          *nameNode, *child, *parameters;
    EjsName         qname;
    int             attributes, next, slotNum, numDefault;

    ejs = cp->ejs;
    parameters = np->function.parameters;

    if (parameters == 0) {
        return 0;
    }

    fun = np->function.functionVar;
    slotNum = 0;
    next = 0;
    numDefault = 0;

    while ((child = getNextAstNode(cp, parameters, &next))) {
        mprAssert(child->kind == N_VAR_DEFINITION);

        attributes = 0;
        nameNode = 0;

        if (child->left->kind == N_QNAME) {
            nameNode = child->left;
        } else if (child->left->kind == N_ASSIGN_OP) {
            numDefault++;
            nameNode = child->left->left;
            attributes |= EJS_ATTR_DEFAULT;
        }
        attributes |= nameNode->attributes;

        ejsName(&qname, EJS_PRIVATE_NAMESPACE, nameNode->qname.name);
        allocName(ejs, &qname);
        slotNum = ejsDefineProperty(ejs, (EjsVar*) fun, slotNum, &qname, NULL, attributes, NULL);
        mprAssert(slotNum >= 0);

        /*
         *  We can assign the lookup information here as these never need fixups.
         */
        nameNode->lookup.slotNum = slotNum;
        nameNode->lookup.obj = (EjsVar*) fun;
        nameNode->lookup.trait = ejsGetPropertyTrait(ejs, (EjsVar*) fun, slotNum);
        mprAssert(nameNode->lookup.trait);
        slotNum++;
    }
    fun->numDefault = numDefault;
    if (fun->getter && fun->numArgs != 0) {
        astError(cp, np, "Getter function \"%s\" must not define parameters.", np->qname.name);
    }
    if (fun->setter && fun->numArgs != 1) {
        astError(cp, np, "Setter function \"%s\" must define exactly one parameter.", np->qname.name);
    }
    return 0;
}


/*
 *  Bind the function parameter types. Local variables get bound as the block gets traversed.
 */
static void bindParameters(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EcNode          *child, *varNode, *assignNode, *parameters, *localType;
    EjsTrait        *trait;
    EjsFunction     *fun;
    EjsName         qname;
    int             next, slotNum;

    ejs = cp->ejs;

    fun = np->function.functionVar;
    mprAssert(fun);

    next = 0;
    parameters = np->function.parameters;
    if (parameters) {
        while ((child = getNextAstNode(cp, parameters, &next))) {
            mprAssert(child->kind == N_VAR_DEFINITION);
            varNode = 0;

            if (child->left->kind == N_QNAME) {
                varNode = child->left;

            } else if (child->left->kind == N_ASSIGN_OP) {
                /*
                 *  Bind defaulting parameters. Only need to do if there is a body. Native functions ignore this code as they
                 *  have no body. The lhs must be scoped inside the function. The rhs must be scoped outside.
                 */
                if (np->function.body) {
                    assignNode = child->left;
                    openBlock(cp, np->function.body, (EjsBlock*) fun);
                    ejsDefineReservedNamespace(ejs, (EjsBlock*) fun, 0, EJS_PRIVATE_NAMESPACE);
                    processAstNode(cp, assignNode->left);
                    closeBlock(cp);

                    processAstNode(cp, assignNode->right);
                }
                varNode = child->left->left;
            }

            mprAssert(varNode);
            mprAssert(varNode->kind == N_QNAME);

            trait = ejsGetPropertyTrait(ejs, (EjsVar*) fun, next - 1);

            if (varNode->typeNode == 0) {
                if (varNode->name.isRest) {
                    ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Array");
                    slotNum = ejsLookupProperty(ejs, ejs->global, &qname);
                    mprAssert(slotNum >= 0);
                    ejsSetTraitType(trait, (EjsType*) ejsGetProperty(ejs, ejs->global, slotNum));
                    fun->rest = 1;
                }

            } else {
                localType = varNode->typeNode;
                processAstNode(cp, localType);
                if (localType->lookup.ref) {
                    ejsSetTraitType(trait, (EjsType*) localType->lookup.ref);
                }
            }
        }
    }
}


/*
 *  Utility routine to bind function return type and locals/args
 */
static EjsFunction *bindFunction(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EcNode          *resultTypeNode;
    EcState         *state;
    EjsType         *iface;
    EjsFunction     *fun, *otherFun;
    EjsVar          *block;
    EjsName         qname;
    cchar           *tok;
    int             slotNum, otherSlot, next;

    mprAssert(cp->phase >= EC_PHASE_BIND);
    mprAssert(np->kind == N_FUNCTION);
    mprAssert(np->qname.name);

    state = cp->state;
    ejs = cp->ejs;

    fun = np->function.functionVar;
    mprAssert(fun);

    block = (np->function.isMethod) ? state->varBlock: state->optimizedLetBlock;
    resultTypeNode = np->function.resultType;

    if (cp->phase == EC_PHASE_BIND) {
        /*
         *  Exclude a literalNamespace as the empty phase as the namespace name is changed for the URI.
         *  Exclude constructors which are hidden in the virtual constructor namespace.
         */
        if (!np->literalNamespace && !np->function.isConstructor) {
            if (resolveNamespace(cp, np, block, 0) == 0) {
                return 0;
            }
        }
        if (block == ejs->global) {
            addGlobalProperty(cp, np, &np->qname);
        }
    }
    
    /*
     *  Test for clashes with non-overridden methods in base classes.
     */
    if (state->currentClass && state->currentClass->baseType) {
        slotNum = ecLookupVar(cp, (EjsVar*) state->currentClass->baseType, &np->qname, 0);
        if (slotNum >= 0 && cp->lookup.obj == (EjsVar*) state->currentClass->baseType) {
            if (!(np->attributes & EJS_ATTR_OVERRIDE) && !state->currentClass->baseType->isInterface) {
                astError(cp, np, 
                    "Function \"%s\" is already defined in a base class. Try using \"override\" keyword.", np->qname.name);
                return 0;
            }

            /*
             *  Install the new function into the v-table by overwriting the method in the closest base class.
             *  Must now define the name of the property and attributes.
             */
            allocName(ejs, &np->qname);
            ejsDefineProperty(ejs, (EjsVar*) block, slotNum, &np->qname, 0, np->attributes, (EjsVar*) fun);
        }
    }

    /*
     *  Test for clashes with non-overridden methods in base classes and implemented classes.
     */
    if (state->currentClass && state->currentClass->implements) {
        next = 0;
        while ((iface = (EjsType*) mprGetNextItem(state->currentClass->implements, &next))) {
            slotNum = ecLookupVar(cp, (EjsVar*) iface, &np->qname, 0);
            if (slotNum >= 0 && cp->lookup.obj == (EjsVar*) iface) {
                if (!iface->isInterface) {
                    if (!(np->attributes & EJS_ATTR_OVERRIDE)) {
                        astError(cp, np, 
                            "Function \"%s\" is already defined in an implemented class. Use the \"override\" keyword.", 
                            np->qname.name);
                        return 0;
                    }

                    /*
                     *  Install the new function into the v-table by overwriting the inherited implemented method.
                     */
                    allocName(ejs, &np->qname);
                    ejsDefineProperty(ejs, (EjsVar*) block, slotNum, &np->qname, 0, np->attributes, (EjsVar*) fun);
                }
            }
        }
    }

    if (resultTypeNode) {
        if (resolveName(cp, resultTypeNode, cp->ejs->global, &resultTypeNode->qname) < 0) {
            if (STRICT_MODE(cp)) {
                astError(cp, np, "Can't find type \"%s\". All variables must be declared and typed in strict mode.", 
                    resultTypeNode->qname.name);
            }
        } else {
            resultTypeNode->qname.space = resultTypeNode->lookup.name.space;
        }
    }

    /*
     *  We dont have a trait for the function return type.
     */
    if (resolveName(cp, np, block, &np->qname) < 0) {
        astError(cp, np, "Internal error. Can't bind function %s", np->qname.name);
        resolveName(cp, np, block, &np->qname);
    }
    if (np->lookup.slotNum >= 0) {
        ejsSetFunctionLocation(fun, block, np->lookup.slotNum);
        setAstDocString(ejs, np, np->lookup.obj, np->lookup.slotNum);
    }

    /*
     *  Bind the result type. Set the result type in np->trait->type
     */
    if (resultTypeNode) {
        mprAssert(resultTypeNode->lookup.ref == 0 || ejsIsType(resultTypeNode->lookup.ref));
        fun->resultType = (EjsType*) resultTypeNode->lookup.ref;
    }

    /*
     *  Cross link getters and setters
     */
    if (fun->getter || fun->literalGetter) {
        qname = np->qname;
        qname.name = mprStrcat(np, -1, "set-", qname.name, NULL);
        otherSlot = ecLookupVar(cp, block, &qname, 0);
        if (otherSlot >= 0) {
            otherFun = (EjsFunction*) ejsGetProperty(ejs, cp->lookup.obj, cp->lookup.slotNum);
            fun->nextSlot = otherSlot;
            otherFun->nextSlot = fun->slotNum;
        }
        mprFree((char*) qname.name);

    } else if (fun->setter) {
        qname = np->qname;
        tok = strchr(qname.name, '-');
        mprAssert(tok);
        qname.name = tok + 1;
        otherSlot = ecLookupVar(cp, block, &qname, 0);
        if (otherSlot >= 0) {
            otherFun = (EjsFunction*) ejsGetProperty(ejs, cp->lookup.obj, cp->lookup.slotNum);
            otherFun->nextSlot = fun->slotNum;
        }
    }

    /*
     *  Optimize away closures
     */
    if (fun->owner == ejs->global || np->function.isMethod || np->attributes & EJS_ATTR_NATIVE) {
        fun->fullScope = 0;
    } else {
        fun->fullScope = 1;
    }
    fun->block.scopeChain = ejs->state->bp->scopeChain;
    return fun;
}


/*
 *  Process the N_FUNCTION node and bind the return type and parameter types
 */
static void astFunction(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsFunction     *fun;
    EcState         *state;

    mprAssert(np->kind == N_FUNCTION);

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    fun = np->function.functionVar;

    if (state->disabled) {
        if (cp->phase == EC_PHASE_CONDITIONAL) {
            removeProperty(cp, state->optimizedLetBlock, np);
        }
        LEAVE(cp);
        return;
    }

    /*
     *  Process the function definition (no parameters yet)
     */
    if (cp->phase == EC_PHASE_DEFINE) {
        fun = defineFunction(cp, np);

    } else if (cp->phase >= EC_PHASE_BIND) {
        fun = bindFunction(cp, np);
    }

    if (fun == 0) {
        LEAVE(cp);
        return;
    }

    /*
     *  Define and bind the parameters and their types
     */
    if (cp->phase == EC_PHASE_DEFINE) {
        defineParameters(cp, np);

    } else if (cp->phase >= EC_PHASE_BIND) {
        bindParameters(cp, np);
    }

    state->currentFunction = fun;
    state->currentFunctionNode = np;

    state->inFunction = 1;
    state->inMethod = state->inMethod || np->function.isMethod;
    state->blockIsMethod = np->function.isMethod;

    state->optimizedLetBlock = (EjsVar*) fun;
    state->letBlock = (EjsVar*) fun;
    state->varBlock = (EjsVar*) fun;

    if (np->function.body) {
        openBlock(cp, np->function.body, (EjsBlock*) fun);
        ejsDefineReservedNamespace(ejs, (EjsBlock*) fun, 0, EJS_PRIVATE_NAMESPACE);
        mprAssert(np->function.body->kind == N_DIRECTIVES);
        processAstNode(cp, np->function.body);
        closeBlock(cp);
    }

    if (np->function.constructorSettings) {
        state->inSettings = 1;
        processAstNode(cp, np->function.constructorSettings);
        state->inSettings = 0;
    }

    /*
     *  Namespaces are done on each phase because pragmas must apply only from the point of declaration onward 
     *  (use namespace)
     */
    if (cp->phase >= EC_PHASE_BIND) {
        if (!np->function.hasReturn && (np->function.resultType != 0 || fun->getter)) {
            if (fun->resultType == 0 || fun->resultType != ejs->voidType || fun->getter) {
                /*
                 *  Native classes have no body defined in script, so we can't verify whether or not it has 
                 *  an appropriate return.
                 */
                if (!(state->currentClass && state->currentClass->isInterface) && !(np->attributes & EJS_ATTR_NATIVE)) {
                    /*
                     *  When building slots for the core VM (empty mode), we can't test ejs->voidType as this won't equal
                     *  the parsed Void class
                     */
                    if (!cp->empty || fun->resultType == 0 || strcmp(fun->resultType->qname.name, "Void") != 0) {
                        astError(cp, np, "Function \"%s\" must return a value",  np->qname.name);
                    }
                }
            }
        }
    }
    LEAVE(cp);
}


/*
 *  Handle a for statement
 */
static void astFor(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np->kind == N_FOR);

    if (np->forLoop.initializer) {
        processAstNode(cp, np->forLoop.initializer);
    }
    if (np->forLoop.cond) {
        processAstNode(cp, np->forLoop.cond);
    }
    if (np->forLoop.perLoop) {
        processAstNode(cp, np->forLoop.perLoop);
    }
    if (np->forLoop.body) {
        processAstNode(cp, np->forLoop.body);
    }
    LEAVE(cp);
}


/*
 *  Handle a for/in statement
 */
static void astForIn(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EjsType     *iteratorType;
    EjsName     qname;

    ENTER(cp);

    mprAssert(np->kind == N_FOR_IN);
    
    ejs = cp->ejs;

    if (np->forInLoop.iterVar) {
        processAstNode(cp, np->forInLoop.iterVar);
    }
    if (np->forInLoop.iterGet) {
        processAstNode(cp, np->forInLoop.iterGet);
    }

    /*
     *  Link to the iterGet node so we can bind the "next" call.
     */
    if (cp->phase >= EC_PHASE_BIND) {
        ejsName(&qname, "iterator", "Iterator");
        iteratorType = (EjsType*) ejsGetPropertyByName(ejs, ejs->global, &qname);
        mprAssert(iteratorType);
        if (iteratorType) {
            ejsName(&qname, "public", "next");
            resolveName(cp, np->forInLoop.iterNext, (EjsVar*) iteratorType, &qname);
        }
    }
    if (np->forInLoop.body) {
        processAstNode(cp, np->forInLoop.body);
    }
    LEAVE(cp);
}


static EjsVar *evalNode(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EjsModule   *mp;
    EjsVar      *result;

    ejs = cp->ejs;
    mp = ejsCreateModule(cp->ejs, "__conditional__", 0);
    if (mp == 0) {
        return 0;
    }
    mp->initializer = createModuleInitializer(cp, np, mp);
    mp->initializer->isInitializer = 1;
    mp->hasInitializer = 1;

    if (astProcess(cp, np) < 0) {
        return 0;
    }
    ecResetParser(cp);

    ecGenConditionalCode(cp, np, mp);
    if (cp->errorCount > 0) {
        return 0;
    }
    result = ejsRunInitializer(ejs, mp);
    if (result == 0) {
        return 0;
    }
    return result;
}


/*
 *  Handle an hash statement (conditional compilation)
 */
static void astHash(EcCompiler *cp, EcNode *np)
{
    EjsVar          *result;
    int             savePhase;

    ENTER(cp);

    mprAssert(np->kind == N_HASH);
    mprAssert(np->hash.expr);
    mprAssert(np->hash.body);

    cp->state->inHashExpression = 1;

    if (cp->phase < EC_PHASE_CONDITIONAL) {
        processAstNode(cp, np->hash.expr);

    } else if (cp->phase == EC_PHASE_CONDITIONAL) {

        ENTER(cp);
        savePhase = cp->phase;
        result = evalNode(cp, np->hash.expr);
        cp->phase = savePhase;
        LEAVE(cp);

        if (result) {
            result = (EjsVar*) ejsToBoolean(cp->ejs, result);
            if (result && !ejsGetBoolean(result)) {
                result = 0;
            }
        }
        if (result == 0) {
            np->hash.disabled = 1;
        }
    }

    if (np->hash.disabled) {
        cp->state->disabled = 1;
    }
    cp->state->inHashExpression = 0;
    processAstNode(cp, np->hash.body);
    LEAVE(cp);
}


/*
 *  Handle an if statement (tenary node)
 */
static void astIf(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np->kind == N_IF);

    mprAssert(np->tenary.cond);
    mprAssert(np->tenary.thenBlock);

    processAstNode(cp, np->tenary.cond);
    processAstNode(cp, np->tenary.thenBlock);

    if (np->tenary.elseBlock) {
        processAstNode(cp, np->tenary.elseBlock);
    }
    LEAVE(cp);
}


static void astImplements(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;
    
    ENTER(cp);
    
    mprAssert(np->kind == N_TYPE_IDENTIFIERS);
    
    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        processAstNode(cp, child);
    }
    LEAVE(cp);
}


/*
 *  Generate a name reference. This routine will bind a name path reference into slot bindings if possible.
 *  The node and its children represent a  name path.
 */
static void astName(EcCompiler *cp, EcNode *np)
{
    if (np->name.qualifierExpr) {
        processAstNode(cp, np->name.qualifierExpr);
    }
    if (np->name.nameExpr) {
        processAstNode(cp, np->name.nameExpr);
    }
    if (cp->phase >= EC_PHASE_BIND) {
        astBindName(cp, np);
        return;
    }
}


static void astBindName(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsLookup       *lookup;
    EjsType         *type;
    EjsFunction     *fun, *currentFunction;
    EjsBlock        *block;
    EcNode          *left;
    EcState         *state;
    int             rc;

    mprAssert(cp->phase >= EC_PHASE_BIND);
    mprAssert(np->kind == N_QNAME);
    
    if (np->qname.name == 0 || np->name.qualifierExpr || np->name.nameExpr) {
        return;
    }
    ENTER(cp);
    state = cp->state;

    /*
     *  If resolving a name to the right of a "." or "[", then only search relative to the object to the left of the dot.
     */
    left = state->currentObjectNode;
    ejs = cp->ejs;
    rc = -1;

    if (np->name.isType) {
        rc = resolveName(cp, np, ejs->global, &np->qname);
        if (rc < 0) {
            //  NOTE: np->qname.space may be null
            astError(cp, np, "Can't find class \"%s\". Ensure the class is visible.", np->qname.name);
        }

    } else if (left) {
        if (left->kind == N_THIS) {
            /*
             *  Explicit "this.property"
             */
            if (state->currentClass) {
                rc = resolveName(cp, np, (EjsVar*) state->currentClass, &np->qname);
                if (rc < 0 && STRICT_MODE(cp)) {
                    astError(cp, np, "Can't find property \"%s\" in this class %s.", np->qname.name, 
                             state->currentClass->qname.name);
                }
            }

        /*
         *  Do we know the type of the left side?
         */
        } else if (left->lookup.trait && left->lookup.trait->type) {
            /*
             *  We must handle 2 cases differently:
             *      1. obj.property
             *      2. Type.property
             *
             *  This is because in the first case, we must extract the type of an object, whereas in the 2nd case,
             *  we already have the type via an explicit type reference.
             */
            if (left->lookup.ref && (ejsIsType(left->lookup.ref) || ejsIsInstanceBlock(left->lookup.ref))) {
                /*
                 *  Case 2. Type.property. We have resolved the type reference.
                 */
                np->lookup.ownerIsType = 1;
                rc = resolveName(cp, np, left->lookup.ref, &np->qname);
                if (rc < 0 && STRICT_MODE(cp) && !((EjsType*) left->lookup.ref)->block.dynamicInstance) {
                    astError(cp, np, "Can't find property \"%s\" in class \"%s\".", np->qname.name,
                        ((EjsType*) left->lookup.ref)->qname.name);

                } else if (np->lookup.trait && !(np->lookup.trait->attributes & EJS_ATTR_STATIC) &&
                        np->lookup.obj != ejs->global) {
                    /* Exclude the case of calling a function (constructor) to create a new instance */
                    if (!(left->kind == N_CALL || left->kind == N_EXPRESSIONS)) {
                        astError(cp, np, "Accessing instance level propery \"%s\" without an instance", np->qname.name);
                    }
                    
                } else if (left->kind == N_CALL) {
                    /*
                     *  Calling a constructor as a function. This will return an instance
                     */
                    np->lookup.nthBase++;
                }

            } else {
                fun = (EjsFunction*) left->lookup.ref;
                if (fun && ejsIsFunction(fun) && fun->getter) {
                    /* 
                     *  Can't use a getter to bind to as the value is determined at run time.
                     */
                    rc = -1;

                } else {

                    /*
                     *  Case 1: Left side is a normal object. We use the type of the lhs to search for name.
                     */
                    rc = resolveName(cp, np, (EjsVar*) left->lookup.trait->type, &np->qname);
                    if (rc == 0) {
                        /*
                         *  Since we searched above on the type of the object and the lhs is an object, increment nthBase.
                         *  BUG: but what if lhs is a type? then nthBase is one too many
                         */
                        if (!np->lookup.instanceProperty) {
                            np->lookup.nthBase++;
                        }
                    }
                }
            }

        } else if (left->kind == N_EXPRESSIONS) {
            /* 
             *  Suppress error message below. We can't know the left because it is an expression. 
             *  So we can't bind the variable 
             */
            rc = 0;
        }

    } else {
        /*
         *  No left side, so search the scope chain
         */
        rc = resolveName(cp, np, 0, &np->qname);

        /*
         *  Check for static function code accessing instance properties or instance methods
         */
        lookup = &np->lookup;
        if (rc == 0 && state->inClass && !state->instanceCode) {
            if (lookup->obj->isInstanceBlock || 
                    (ejsIsType(lookup->obj) && (lookup->trait && !(lookup->trait->attributes & EJS_ATTR_STATIC)))) {
                if (!state->inFunction || (state->currentFunctionNode->attributes & EJS_ATTR_STATIC)) {
                    astError(cp, np, "Accessing instance level property \"%s\" without an instance", np->qname.name);
                    rc = -1;
                }
            }
        }
    }

    if (rc < 0) {
        if (STRICT_MODE(cp) && !cp->error) {
            astError(cp, np, "Can't find a declaration for \"%s\". All variables must be declared and typed in strict mode.",
                np->qname.name);
        }

    } else {
        if (np->lookup.trait) {
            np->attributes |= np->lookup.trait->attributes;
        }
    }

    /*
     *  Disable binding of names in certain cases.
     */
    lookup = &np->lookup;
    if (lookup->slotNum >= 0) {
        /*
         *  Unbind if slot number won't fit in one byte or the object is not a standard Object. The bound op codes 
         *  require one byte slot numbers.
         */
        if (lookup->slotNum >= 256 || (lookup->obj != ejs->global && !ejsIsObject(lookup->obj))) {
            lookup->slotNum = -1;
        }

        if (lookup->obj == ejs->global && !cp->bind) {
            /*
             *  Unbind non-core globals
             */
            if ((lookup->slotNum >= ES_global_NUM_CLASS_PROP) &&
                    !(lookup->trait == 0 && (lookup->trait->attributes & EJS_ATTR_BUILTIN))) {
                lookup->slotNum = -1;
            }
        }

        if (ejsIsType(np->lookup.obj)) {
            type = (EjsType*) np->lookup.obj;
            if (type->block.nobind || type->isInterface) {
                /*
                 *  Type requires non-bound access. Types that implement interfaces will have different slots.
                 */
                lookup->slotNum = -1;

            } else if (type->block.dynamicInstance && !type->block.obj.var.builtin) {
                /*
                 *  Don't bind non-core dynamic properties
                 */
                lookup->slotNum = -1;

            } else {
                if (type == ejs->xmlType || type == ejs->xmlListType) {
                    if (np->parent == 0 || np->parent->parent == 0 || np->parent->parent->kind != N_CALL) {
                        lookup->slotNum = -1;
                    }
                }
            }

        } else if (ejsIsInstanceBlock(np->lookup.obj)) {
            block = (EjsBlock*) np->lookup.obj;
            if (block->nobind) {
                lookup->slotNum = -1;
            } else if (block->dynamicInstance && !block->obj.var.builtin) {
                lookup->slotNum = -1;
            }
        }
    }

    /*
     *  If accessing unbound variables, then the function will require full scope if a closure is ever required.
     */
    currentFunction = state->currentFunction;
    if (lookup->slotNum < 0) {
        if (cp->phase == EC_PHASE_BIND && cp->warnLevel > 5) {
            astWarn(cp, np, "Using unbound variable reference for \"%s\"", np->qname.name);
        }
    }
    LEAVE(cp);
}


static void astNew(EcCompiler *cp, EcNode *np)
{
    EjsType     *type;
    EcNode      *left;

    mprAssert(np->kind == N_NEW);
    mprAssert(np->left);
    mprAssert(np->left->kind == N_QNAME || np->left->kind == N_DOT);
    mprAssert(np->right == 0);

    ENTER(cp);

    left = np->left;
    processAstNode(cp, left);

    if (cp->phase != EC_PHASE_BIND) {
        LEAVE(cp);
        return;
    }

    mprAssert(cp->phase >= EC_PHASE_BIND);

    np->newExpr.callConstructors = 1;

    if (left->lookup.ref) {
        type = (EjsType*) left->lookup.ref;
        if (type && ejsIsType(type)) {
            /* Type is bound, has no constructor or base class constructors */
            if (!type->hasConstructor && !type->hasBaseConstructors) {
                np->newExpr.callConstructors = 0;
            }
            
            /*
             *  Propagate up the left side. Increment nthBase because it is an instance.
             */
            np->qname = left->qname;
            np->lookup = left->lookup;
            np->lookup.trait = mprAllocObj(cp->ejs, EjsTrait);
            np->lookup.trait->type = (EjsType*) np->lookup.ref;
            np->lookup.ref = 0;
            np->lookup.instanceProperty = 1;
        }
    }
    LEAVE(cp);
}


static void astObjectLiteral(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    mprAssert(np->kind == N_OBJECT_LITERAL);

    processAstNode(cp, np->objectLiteral.typeNode);
    next = 0;
    while ((child = getNextAstNode(cp, np, &next)) != 0) {
        processAstNode(cp, child);
    }
}


static void astField(EcCompiler *cp, EcNode *np)
{
    if (np->field.fieldKind == FIELD_KIND_VALUE) {
        processAstNode(cp, np->field.expr);
    }
}


static void astPragmas(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EcNode      *child;
    int         next;

    mprAssert(np->kind == N_PRAGMAS);

    ENTER(cp);

    ejs = cp->ejs;

    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        processAstNode(cp, child);
    }
    LEAVE(cp);
}


static void astPragma(EcCompiler *cp, EcNode *np)
{
    mprAssert(np->kind == N_PRAGMA);

    ENTER(cp);
    if (np->pragma.mode) {
        cp->fileState->mode = np->pragma.mode;
        cp->fileState->lang = np->pragma.lang;
    }
    LEAVE(cp);
}



static void astPostfixOp(EcCompiler *cp, EcNode *np)
{
    EcNode      *left;
    
    mprAssert(np->kind == N_POSTFIX_OP);

    ENTER(cp);

    left = np->left;
    if (left->kind == N_LITERAL) {
        astError(cp, np, "Invalid postfix operand");
    } else {
        processAstNode(cp, np->left);
    }
    LEAVE(cp);
}


static void astProgram(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EcState         *state;
    EcNode          *child;
    int             next;

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    state->namespace = np->qname.name;

    next = 0;
    while ((child = getNextAstNode(cp, np, &next)) != 0) {
        processAstNode(cp, child);
    }
    LEAVE(cp);
}


static void astReturn(EcCompiler *cp, EcNode *np)
{
    EjsFunction     *fun;
    EcNode          *functionNode;
    EcState         *state;

    ENTER(cp);

    state = cp->state;

    mprAssert(state->currentFunctionNode->kind == N_FUNCTION);
    state->currentFunctionNode->function.hasReturn = 1;

    if (np->left) {
        processAstNode(cp, np->left);
    }

    if (cp->phase >= EC_PHASE_BIND) {
        mprAssert(state->currentFunction);
        mprAssert(state->currentFunction);
        functionNode = state->currentFunctionNode;
        state->currentFunction->hasReturn = functionNode->function.hasReturn;

        fun = state->currentFunction;
        if (fun->hasReturn) {
            if (np->left) {
                if (fun->resultType && fun->resultType == cp->ejs->voidType) {
                    /*
                     *  Allow block-less function expressions where a return node was generated by the parser.
                     */
                    if (!np->ret.blockLess) {
                        astError(cp, np, "Void function \"%s\" can't return a value", functionNode->qname.name);
                    }

                } else if (fun->setter) {
                    astError(cp, np, "Setter function \"%s\" can't return a value", functionNode->qname.name);
                }

            } else {
                if (fun->resultType && fun->resultType != cp->ejs->voidType) {
                    if (! (cp->empty && strcmp(fun->resultType->qname.name, "Void") == 0)) {
                        astError(cp, np, "Return in function \"%s\" must return a value", functionNode->qname.name);
                    }
                }
            }
        }
    }
    LEAVE(cp);
}


static void astSuper(EcCompiler *cp, EcNode *np)
{
    EcState     *state;

    ENTER(cp);

    state = cp->state;
    if (state->currentObjectNode == 0) {

        if (state->currentFunction == 0) {
            if (cp->phase == EC_PHASE_DEFINE) {
                astError(cp, np, "Can't use unqualified \"super\" outside a method");
            }
            LEAVE(cp);
            return;
        }

        if (!state->currentFunction->constructor) {
            if (cp->phase == EC_PHASE_DEFINE) {
                astError(cp, np, "Can't use unqualified \"super\" outside a constructor");
            }
            LEAVE(cp);
            return;
        }

        if (cp->phase >= EC_PHASE_BIND) {
            if (state->currentClass->hasBaseConstructors == 0) {
                astError(cp, np, "No base class constructors exist to call via super");
                LEAVE(cp);
                return;
            }
        }

        state->currentClass->callsSuper = 1;
        if (np->left && np->left->kind != N_NOP) {
            processAstNode(cp, np->left);
        }

    } else {
        astError(cp, np, "Can't use unqualified \"super\" outside a method");
    }
    LEAVE(cp);
}


static void astSwitch(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    ENTER(cp);

    mprAssert(np->kind == N_SWITCH);

    mprAssert(np->right->kind == N_CASE_ELEMENTS);

    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        processAstNode(cp, child);
    }
    LEAVE(cp);
}


static void astThis(EcCompiler *cp, EcNode *np)
{
    EcState     *state;

    ENTER(cp);

    state = cp->state;

    switch (np->thisNode.thisKind) {
    case N_THIS_GENERATOR:
        break;

    case N_THIS_CALLEE:
        break;

    case N_THIS_TYPE:
        if (!state->inClass) {
            astError(cp, np, "\"this type\" is only valid inside a class");
        } else {
            np->lookup.obj = (EjsVar*) state->currentClass;
            np->lookup.slotNum = 0;
        }
        break;

    case N_THIS_FUNCTION:
        if (!state->inFunction) {
            astError(cp, np, "\"this function\" is not valid outside a function");
        } else {
            np->lookup.obj = (EjsVar*) state->currentFunction;
            np->lookup.slotNum = 0;
        }
        break;

    default:
        np->lookup.obj = (EjsVar*) state->currentClass;
        np->lookup.slotNum = 0;
    }
    LEAVE(cp);
}


static void astThrow(EcCompiler *cp, EcNode *np)
{
    mprAssert(np->left);
    processAstNode(cp, np->left);
}


/*
 *  Try, catch, finally
 */
static void astTry(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EcNode      *child;
    EjsBlock    *block;
    int         next, count;

    ENTER(cp);

    mprAssert(np->kind == N_TRY);
    mprAssert(np->exception.tryBlock);

    ejs = cp->ejs;

    processAstNode(cp, np->exception.tryBlock);

    if (cp->phase == EC_PHASE_BIND) {
        /*
         *  Calculate the number of lexical blocks in the try block. These must be discarded by the VM when executing
         *  catch and finally blocks.
         */
        for (count = 0, block = ejs->state->bp->scopeChain; block && !ejsIsFunction(block); block = block->scopeChain) {
            if (!block->obj.var.hidden) {
                count++;
            }
        }
        np->exception.numBlocks = count;
    }
    if (np->exception.catchClauses) {
        next = 0;
        while ((child = getNextAstNode(cp, np->exception.catchClauses, &next))) {
            processAstNode(cp, child);
        }
    }
    if (np->exception.finallyBlock) {
        block = ejsCreateBlock(cp->ejs, 0);
        ejsSetDebugName(block, "finally");
        addScope(cp, block);
        processAstNode(cp, np->exception.finallyBlock);
        removeScope(cp);
    }
    LEAVE(cp);
}


/*
 *  Handle a unary operator.
 */
static void astUnaryOp(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np->kind == N_UNARY_OP);
    mprAssert(np->left);

    if (np->left->kind == N_LITERAL && (np->tokenId == T_PLUS_PLUS || np->tokenId == T_MINUS_MINUS)) {
        astError(cp, np, "Invalid prefix operand");
    } else {
        processAstNode(cp, np->left);
    }
    LEAVE(cp);
}


/*
 *  Create a module defined via a module directive.
 */
static void astModule(EcCompiler *cp, EcNode *np)
{
    EjsModule       *mp, *core;
    Ejs             *ejs;
    EcState         *state;
    EcNode          *child;
    EjsBlock        *saveChain;
    int             next;

    mprAssert(np->kind == N_MODULE);

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    
    if (cp->phase == EC_PHASE_DEFINE) {
        mp = createModule(cp, np);

    } else {
        mp = np->module.ref;
        mprAssert(mp);
    }
    if (mp == 0) {
        return;
    }
    mprAssert(mp->initializer);

    /*
     *  Start a new scope chain for this module. ie. Don't nest modules in the scope chain.
     */
    saveChain = ejs->state->bp->scopeChain;
    ejs->state->bp->scopeChain = mp->scopeChain;

    /*
     *  Create a block for the module initializer. There is also a child block but that is to hide namespace declarations 
     *  from other compilation units. Open the block explicitly rather than using astBlock. We do this because we need 
     *  varBlock to be set to ejs->global and let block to be mp->initializer. The block is really only used to scope 
     *  namespaces.
     */
    openBlock(cp, np, (EjsBlock*) mp->initializer);
    
    if (cp->phase == EC_PHASE_BIND) {
        /*
         *  Bind the block here before processing the child nodes so we can mark the block as hidden if it will be expunged.
         */
        bindBlock(cp, np->left);
    }
    
    /*
     *  Open the child block here so we can set the letBlock and varBlock values inside the block.
     */
    mprAssert(np->left->kind == N_BLOCK);
    openBlock(cp, np->left, NULL);
    
    state->optimizedLetBlock = ejs->global;
    state->varBlock = ejs->global;
    state->letBlock = (EjsVar*) mp->initializer;
    state->currentModule = mp;

    if (mp->dependencies == 0) {
        mp->dependencies = mprCreateList(mp);
        core = ejsLookupModule(ejs, "ejs", 0, 0);
        if (core && mprLookupItem(mp->dependencies, core) < 0) {
            mprAddItem(mp->dependencies, core);
        }
    }

    /*
     *  Skip the first (block) child that was processed manually above.
     */
    for (next = 0; (child = getNextAstNode(cp, np->left, &next)); ) {
        processAstNode(cp, child);
    }

    closeBlock(cp);
    closeBlock(cp);
    
    if (cp->phase == EC_PHASE_CONDITIONAL) {
        /*
         *  Define block after the variables have been processed. This allows us to determine if the block is really needed.
         */
        defineBlock(cp, np->left);
    }

    ejs->state->bp->scopeChain = saveChain;
    LEAVE(cp);
}


/*
 *  Use Namespace
 */
static void astUseNamespace(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsNamespace    *namespace;
    EcState         *state, *s;

    ENTER(cp);

    state = cp->state;

    mprAssert(np->kind == N_USE_NAMESPACE);

    ejs = cp->ejs;
    namespace = 0;

    if (cp->phase == EC_PHASE_DEFINE) {
        /*
         *  At the define phase, we create a dummy namespace assuming that it will exist somewhere in this block or an 
         *  outer block. At the fixup phase, we actually resolve the reference to the namespace unless it is a string 
         *  literal namespace.
         */
        namespace = ejsCreateNamespace(ejs, np->qname.name, np->qname.name);
        np->namespaceRef = namespace;

    } else if (cp->phase >= EC_PHASE_BIND) {

        if (np->useNamespace.isLiteral) {
            namespace = np->namespaceRef;

        } else {
            /*
             *  Resolve the real namespace. Must be visible in the current scope (even in standard mode). 
             *  Then update the URI. URI not used.
             */
            if (resolveName(cp, np, 0, &np->qname) < 0) {
                astError(cp, np, "Can't find namespace \"%s\"", np->qname.name);

            } else {
                namespace = (EjsNamespace*) np->lookup.ref;
                np->namespaceRef->uri = namespace->uri;

                if (!ejsIsNamespace(namespace)) {
                    astError(cp, np, "The variable \"%s\" is not a namespace", np->qname.name);

                } else {
                    np->namespaceRef = namespace;
                }
            }
            if (np->useNamespace.isDefault) {
                /*
                 *  Apply the namespace URI to all upper blocks
                 */
                for (s = cp->state; s; s = s->prev) {
                    s->namespace = (char*) namespace->uri;
                    if (s == cp->blockState) {
                        break;
                    }
                }
            }
        }

    } else {
        namespace = np->namespaceRef;
    }

    if (namespace) {
        if (state->letBlockNode) {
            state->letBlockNode->createBlockObject = 1;
        }
        if (state->inClass && !state->inFunction) {
            /*
             *  Must attach to the class itself and not to the outermost block
             */
            ejsAddNamespaceToBlock(ejs, (EjsBlock*) state->currentClass, namespace);
        } else {
            ejsAddNamespaceToBlock(ejs, (EjsBlock*) state->letBlock, namespace);
        }
    }
    LEAVE(cp);
}


/*
 *  Module depenency
 */
static void astRequire(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EjsModule   *currentModule, *mp;
    int         flags;

    mprAssert(np->kind == N_USE_MODULE);
    mprAssert(np->qname.name);

    ENTER(cp);

    ejs = cp->ejs;

    if (cp->phase == EC_PHASE_DEFINE) {
        /*
         *  Is this a module we are currently compiling?
         */
        mp = ecLookupModule(cp, np->qname.name, np->useModule.minVersion, np->useModule.maxVersion);
        if (mp == 0) {
            /*
             *  Is this module already loaded by the vm?
             */
            mp = ejsLookupModule(ejs, np->qname.name, np->useModule.minVersion, np->useModule.maxVersion);
            if (mp == 0) {
                flags = (cp->run) ? 0 : EJS_MODULE_DONT_INIT;
                if (ejsLoadModule(ejs, np->qname.name, np->useModule.minVersion, 
                        np->useModule.maxVersion, flags, NULL) < 0) {
                    astError(cp, np, "Error loading module \"%s\"\n%s", np->qname.name, ejsGetErrorMsg(ejs, 0));
                    cp->fatalError = 1;
                    LEAVE(cp);
                    return;
                }
                mp = ejsLookupModule(ejs, np->qname.name, np->useModule.minVersion, np->useModule.maxVersion);
            }
        }

        if (mp == 0) {
            astError(cp, np, "Can't find dependent module \"%s\"", np->qname.name);

        } else {
            currentModule = cp->state->currentModule;
            mprAssert(currentModule);

            if (currentModule->dependencies == 0) {
                currentModule->dependencies = mprCreateList(currentModule);
            }
            if (mprLookupItem(currentModule->dependencies, mp) < 0 && mprAddItem(currentModule->dependencies, mp) < 0) {
                mprAssert(0);
            }
        }
        mprAssert(np->left->kind == N_USE_NAMESPACE);
        np->left->qname.name = mp->vname;
    }
    mprAssert(np->left->kind == N_USE_NAMESPACE);
    processAstNode(cp, np->left);
    LEAVE(cp);
}


static void astWith(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EjsLookup   *lookup;
    bool        pushed;

    ENTER(cp);

    ejs = cp->ejs;
    pushed = 0;

    processAstNode(cp, np->with.object);

    if (cp->phase >= EC_PHASE_BIND) {
        processAstNode(cp, np->with.object);
        /*
         *  To permit early binding, if the object is typed, then add that type to the scope chain.
         */
        lookup = &np->with.object->lookup;
        if (lookup->trait && lookup->trait->type) {
            addScope(cp, (EjsBlock*) lookup->trait->type);
            pushed++;
        }
    }

    processAstNode(cp, np->with.statement);

    if (pushed) {
        removeScope(cp);
    }
    LEAVE(cp);

}


/*
 *  Determine the block in which to define a variable.
 */
static EjsVar *getBlockForDefinition(EcCompiler *cp, EcNode *np, EjsVar *block, int *attributes)
{
    EcState     *state;
    EjsType     *type;

    state = cp->state;

    if (ejsIsType(block) && state->inClass) {
        if (!(*attributes & EJS_ATTR_STATIC) && !state->inFunction &&
            cp->classState->blockNestCount == (cp->state->blockNestCount - 1)) {
            /*
             *  If not static, outside a function and in the top level block.
             */
            type = (EjsType*) block;
            block = (EjsVar*) type->instanceBlock;
            if (block == 0) {
                block = (EjsVar*) ejsCreateTypeInstanceBlock(cp->ejs, type, 0);
            }
            np->name.instanceVar = 1;
        }
    }
    return block;
}


static bool typeIsCompatible(EcCompiler *cp, EjsType *first, EjsType *second)
{
    Ejs     *ejs;

    ejs = cp->ejs;
    if (first == 0 || second == 0) {
        return 1;
    }
    if (strcmp(first->qname.name, second->qname.name) == 0 && strcmp(first->qname.space, second->qname.space) == 0) {
        return 1;
    }
    return 0;
}


/*
 *  Define a variable
 */
static void defineVar(EcCompiler *cp, EcNode *np, int varKind, EjsVar *value)
{
    Ejs             *ejs;
    EjsFunction     *method;
    EjsVar          *obj;
    EcState         *state;
    int             slotNum, attributes;

    ejs = cp->ejs;
    mprAssert(cp->phase == EC_PHASE_DEFINE);

    state = cp->state;

    method = state->currentFunction;
    attributes = np->attributes;

    /*
     *  Only create block scope vars if the var block is different to the let block. This converts global let vars to vars.
     */
    np->name.letScope = 0;
    if (varKind & KIND_LET && (state->varBlock != state->optimizedLetBlock)) {
        np->name.letScope = 1;
    }

    if (np->name.letScope) {
        mprAssert(varKind & KIND_LET);

        /*
         *  Look in the current block scope but only one level deep. We lookup without any namespace decoration
         *  so we can prevent users defining variables in more than once namespace. (ie. public::x and private::x).
         */
        if (ecLookupScope(cp, &np->qname, 1) >= 0 && cp->lookup.obj == (EjsVar*) ejs->state->fp) {
            obj = cp->lookup.obj;
            slotNum = cp->lookup.slotNum;
            if (cp->fileState->lang == EJS_SPEC_FIXED) {
                astError(cp, np, "Variable \"%s\" is already defined", np->qname.name);
                return;
            }

        } else {
            obj = getBlockForDefinition(cp, np, state->optimizedLetBlock, &attributes);
            allocName(ejs, &np->qname);
            slotNum = ejsDefineProperty(ejs, obj, -1, &np->qname, 0, attributes, value);
        }

    } else {

        if (ecLookupVar(cp, state->varBlock, &np->qname, 1) >= 0) {
            obj = cp->lookup.obj;
            slotNum = cp->lookup.slotNum;
            if (cp->fileState->lang == EJS_SPEC_FIXED) {
                astError(cp, np, "Variable \"%s\" is already defined.", np->qname.name);
                return;
            }
        }

        /*
         *  Var declarations are hoisted to the nearest function, class or global block (never nested block scope)
         */
        obj = getBlockForDefinition(cp, np, (EjsVar*) state->varBlock, &attributes);
        allocName(ejs, &np->qname);
        slotNum = ejsDefineProperty(ejs, obj, -1, &np->qname, 0, attributes, value);
    }

    if (slotNum < 0) {
        astError(cp, np, "Can't define variable %s", np->qname.name);
        return;
    }
}


/*
 *  Hoist a block scoped variable and define in the nearest function, class or global block. This runs during the
 *  Hoist conditional phase. We hoist the variable by defining with a "-hoisted-%d" namespace which is added to the set of
 *  Hoist open namespaces. This namespace is only used when compiling and not at runtime. All access to the variable is bound
 */
static bool hoistBlockVar(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EcState     *state;
    EjsVar      *block, *obj, *vp;
    int         slotNum, attributes;

    mprAssert(cp->phase == EC_PHASE_CONDITIONAL);

    if (cp->optimizeLevel == 0) {
        return 0;
    }

    ejs = cp->ejs;
    state = cp->state;
    block = (np->kind == N_BLOCK) ? state->letBlock : state->optimizedLetBlock;
    attributes = np->attributes;

    if (state->inClass && state->inFunction) {
        obj = state->varBlock;

    } else {
        /*
         *  Global or class level block
         */
        mprAssert(!state->instanceCode);
        obj = state->varBlock;
        attributes |= EJS_ATTR_STATIC;
    }

    if (!cp->bind && obj == ejs->global) {
        /* Can't hoist variables to global scope if not binding */
        return 0;
    }

    /*
     *  Delete the property from the original block. Don't reclaim slot, delete will set to 0.
     */
    slotNum = ejsLookupProperty(ejs, block, &np->qname);
    mprAssert(slotNum >= 0);
    vp = ejsGetProperty(ejs, block, slotNum);
    ejsDeleteProperty(ejs, block, slotNum);

    /*
     *  Redefine hoisted in the outer var block. Use a unique hoisted namespace to avoid clashes with other
     *  hoisted variables of the same name.
     */
    np->namespaceRef = createHoistNamespace(cp, obj);
    np->qname.space = np->namespaceRef->uri;

    allocName(ejs, &np->qname);
    slotNum = ejsDefineProperty(ejs, obj, -1, &np->qname, 0, attributes, vp);
    if (slotNum < 0) {
        astError(cp, np, "Can't define local variable %s::%s", np->qname.space, np->qname.name);
        return 0;
    }
    if (obj == ejs->global) {
        addGlobalProperty(cp, np, &np->qname);
    }
    np->name.letScope = 0;
    return 1;
}


/*
 *  Fully bind a variable definition. We already know the owning type and the slot number.
 *  We now need to  bind the variable type and set the trait reference.
 */
static void bindVariableDefinition(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsFunction     *fun;
    EjsVar          *block;
    EjsTrait        *trait;
    EcState         *state;
    EcNode          *typeNode;

    ENTER(cp);

    mprAssert(cp->phase >= EC_PHASE_BIND);

    ejs = cp->ejs;
    state = cp->state;
    fun = state->currentFunction;
    block = (np->name.letScope) ? state->optimizedLetBlock : state->varBlock;
    
    if (!state->inFunction) {
        if (!np->literalNamespace && resolveNamespace(cp, np, block, 0) == 0) {
            LEAVE(cp);
            return;
        }
    }

    if (cp->phase == EC_PHASE_BIND && block == ejs->global) {
        addGlobalProperty(cp, np, &np->qname);
    }

    /*
     *  Check if there is a name clash with any subclasses. Must do after fixup so that the base type has been defined.
     *  Look in the current type for any public property of the same name.
     */
    if (state->inClass && !state->inFunction && state->currentClass->baseType) {

        if (ecLookupVar(cp, (EjsVar*) state->currentClass->baseType, &np->qname, 0) >= 0) {
            astError(cp, np, "Public property %s is already defined in a base class", np->qname.name);
            LEAVE(cp);
            return;
        }
    }

    if (resolveName(cp, np, block, &np->qname) < 0) {
        astError(cp, np, "Can't find variable \"%s::%s\"", np->qname.space, np->qname.name);
    }

    typeNode = np->typeNode;
    if (typeNode && np->lookup.trait) {
        if (typeNode->lookup.ref) {
            if (cp->fileState->lang == EJS_SPEC_PLUS) {
                /*
                 *  Allow variable redefinitions providing they are compatible
                 */
                trait = ejsGetPropertyTrait(ejs, np->lookup.obj, np->lookup.slotNum);
                if (!typeIsCompatible(cp, trait->type, (EjsType*) typeNode->lookup.ref)) {
                    astError(cp, np, "Redefinition of \"%s\" is not compatible with prior definition", np->qname.name);
                    LEAVE(cp);
                    return;
                }
            }
            ejsSetTraitType(np->lookup.trait, (EjsType*) typeNode->lookup.ref);
        }
    }
    setAstDocString(ejs, np, np->lookup.obj, np->lookup.slotNum);
    LEAVE(cp);
}


/*
 *  Define a variable
 */
static void astVar(EcCompiler *cp, EcNode *np, int varKind, EjsVar *value)
{
    EcState     *state;
    Ejs         *ejs;
    EjsVar      *obj;

    ejs = cp->ejs;
    state = cp->state;

    if (state->disabled) {
        if (cp->phase == EC_PHASE_CONDITIONAL) {
            obj = getBlockForDefinition(cp, np, (EjsVar*) state->varBlock, &np->attributes);
            removeProperty(cp, obj, np);
        }
        return;
    }

    state->instanceCode = 0;
    if (state->inClass && !(np->attributes & EJS_ATTR_STATIC)) {
        if (state->inMethod) {
            state->instanceCode = 1;

        } else if (cp->classState->blockNestCount == (cp->state->blockNestCount - 1)) {
            /*
             *  Top level var declaration without a static attribute
             */
            state->instanceCode = 1;
        }

    } else {
        mprAssert(state->instanceCode == 0);
    }

    if (np->typeNode) {
        if (np->typeNode->kind != N_QNAME) {
            astError(cp, np, "Bad type name");
            return;
        }
        if (strcmp(np->typeNode->qname.name, "*") != 0) {
            processAstNode(cp, np->typeNode);
        }
    }

    if (cp->phase == EC_PHASE_DEFINE) {
        defineVar(cp, np, varKind, value);

    } else if (cp->phase == EC_PHASE_CONDITIONAL && np->name.letScope) {
        if (!hoistBlockVar(cp, np)) {
            /*
             *  Unhoisted let scoped variable.
             */
            state->letBlockNode->createBlockObject = 1;
        }

    } else if (cp->phase >= EC_PHASE_BIND) {
        if (np->namespaceRef) {
            /*
             *  Add any hoist namespaces that were defined in hoistBlockVar in the conditional phase
             */
            ejsAddNamespaceToBlock(ejs, (EjsBlock*) cp->state->optimizedLetBlock, np->namespaceRef);
        }
        bindVariableDefinition(cp, np);
    }
}


/*
 *  Define variables
 */
static void astVarDefinition(EcCompiler *cp, EcNode *np, int *codeRequired, int *instanceCode)
{
    Ejs         *ejs;
    EcNode      *child, *var, *right;
    EcState     *state;
    int         next, varKind, slotNum;

    mprAssert(np->kind == N_VAR_DEFINITION);

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    mprAssert(state);

    varKind = np->def.varKind;

    next = 0;
    while ((child = getNextAstNode(cp, np, &next))) {
        if (child->kind == N_ASSIGN_OP) {
            var = child->left;
        } else {
            var = child;
        }

        astVar(cp, var, np->def.varKind, var->name.value);
        
        if (state->disabled) {
            continue;
        }

        if (child->kind == N_ASSIGN_OP) {
            *instanceCode = state->instanceCode;
            *codeRequired = 1;
        }

        if (child->kind == N_ASSIGN_OP) {
            astAssignOp(cp, child);

            right = child->right;
            mprAssert(right);

            /*
             *  Define constants here so they can be used for conditional compilation and "use namespace". We erase after the
             *  conditional phase.
             */
            if (right->kind == N_LITERAL && !(np->def.varKind & KIND_LET) && !(var->attributes & EJS_ATTR_NATIVE)) {
                mprAssert(var->kind == N_QNAME);
                mprAssert(right->literal.var);
                /* Exclude class instance variables */
                if (! (state->inClass && !(var->attributes & EJS_ATTR_STATIC))) {
                    slotNum = ejsLookupProperty(ejs, state->varBlock, &var->qname);
                    if (cp->phase == EC_PHASE_DEFINE) {
                        ejsSetProperty(ejs, state->varBlock, slotNum, right->literal.var);

                    } else if (cp->phase >= EC_PHASE_BIND && !var->name.isNamespace && slotNum >= 0) {
                        /*
                         *  Erase the value incase being run in the ejs shell. Must not prematurely define values.
                         */
                        ejsSetProperty(ejs, state->varBlock, slotNum, ejs->undefinedValue);
                    }
                }
            }
        }
    }
    LEAVE(cp);
}


/*
 *  Void type node
 */
static void astVoid(EcCompiler *cp, EcNode *np)
{
    EjsName     qname;

    mprAssert(np->kind == N_VOID);

    ENTER(cp);

    if (cp->phase >= EC_PHASE_BIND) {
        qname.name = "Void";
        qname.space = EJS_INTRINSIC_NAMESPACE;

        if (resolveName(cp, np, 0, &qname) < 0) {
            astError(cp, np, "Can't find variable \"%s::%s\"", qname.space, qname.name);
        }
    }
    LEAVE(cp);
}


/*
 *  Create a function to hold the module initialization code. Set a basic scope chain here incase running in ejs.
 */

static EjsFunction *createModuleInitializer(EcCompiler *cp, EcNode *np, EjsModule *mp)
{
    Ejs             *ejs;
    EjsFunction     *fun;

    ejs = cp->ejs;

    fun = ejsCreateFunction(ejs, 0, -1, 0, 0, ejs->voidType, 0, mp->constants, mp->scopeChain, cp->state->lang);
    if (fun == 0) {
        astError(cp, np, "Can't create initializer function");
        return 0;
    }
    fun->isInitializer = 1;
    ejsSetDebugName(fun, mp->name);
    return fun;
}


/*
 *  Create the required module
 */
static EjsModule *createModule(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsModule       *mp;
    ejs = cp->ejs;

    mprAssert(np->kind == N_MODULE);

    mp = ecLookupModule(cp, np->qname.name, np->module.version, np->module.version);
    if (mp == 0) {
        mp = ejsCreateModule(cp->ejs, np->qname.name, np->module.version);
        if (mp == 0) {
            astError(cp, np, "Can't create module %s", np->qname.name);
            return 0;
        }
        mp->scopeChain = ejs->globalBlock;
        if (ecAddModule(cp, mp) < 0) {
            astError(cp, 0, "Can't insert module");
            return 0;
        }
        /*
         *  This will prevent the loading of any module that uses this module.
         */
        if (strcmp(mp->name, EJS_DEFAULT_MODULE) != 0) {
            mp->compiling = 1;
        }
    }

    if (mp->initializer == 0) {
        mp->initializer = createModuleInitializer(cp, np, mp);
    }
    np->module.ref = mp;

    if (cp->outputFile) {
        np->module.filename = cp->outputFile;
    } else {
        np->module.filename = mprStrcat(np, -1, np->qname.name, EJS_MODULE_EXT, NULL);
    }
    return mp;
}


static void astError(EcCompiler *cp, EcNode *np, char *fmt, ...)
{
    va_list     arg;
    char        *msg;

    va_start(arg, fmt);

    if ((msg = mprVasprintf(cp, 0, fmt, arg)) == NULL) {
        msg = "Memory allocation error";
    }
    cp->errorCount++;
    cp->error = 1;
    cp->noout = 1;
    
    if (np) {
        ecReportError(cp, "error", np->filename, np->lineNumber, np->currentLine, np->column, msg);
    } else {
        ecReportError(cp, "error", 0, 0, 0, 0, msg);
    }
    mprFree(msg);
    va_end(arg);
}


static void astWarn(EcCompiler *cp, EcNode *np, char *fmt, ...)
{
    va_list     arg;
    char        *msg;

    va_start(arg, fmt);

    if ((msg = mprVasprintf(cp, 0, fmt, arg)) == NULL) {
        msg = "Memory allocation error";
        cp->errorCount++;
        cp->error = 1;
    }
    cp->warningCount++;
    ecReportError(cp, "warning", np->filename, np->lineNumber, np->currentLine, np->column, msg);
    mprFree(msg);
    va_end(arg);
}


static void badAst(EcCompiler *cp, EcNode *np)
{
    cp->fatalError = 1;
    cp->errorCount++;
    mprError(cp, "Unsupported language feature\nUnknown AST node kind %d",  np->kind);
}


static EcNode *getNextAstNode(EcCompiler *cp, EcNode *np, int *next)
{
    if (cp->fatalError) {
        return 0;
    }
    if (np == 0 || np->children == 0) {
        return 0;
    }
    return (EcNode*) mprGetNextItem(np->children, next);
}


static void processAstNode(EcCompiler *cp, EcNode *np)
{
    EcState     *state;
    EjsType     *type;
    int         codeRequired, instanceCode;

    ENTER(cp);

    mprAssert(np->parent || np->kind == N_PROGRAM);

    state = cp->state;
    codeRequired = 0;

    instanceCode = state->instanceCode;
    
    switch (np->kind) {
    case N_ARGS:
        astArgs(cp, np);
        codeRequired++;
        break;

    case N_ARRAY_LITERAL:
        processAstNode(cp, np->left);
        codeRequired++;
        break;

    case N_ASSIGN_OP:
        astAssignOp(cp, np);
        codeRequired++;
        break;

    case N_BINARY_OP:
        astBinaryOp(cp, np);
        codeRequired++;
        break;

    case N_BLOCK:
        astBlock(cp, np);
        break;

    case N_BREAK:
        astBreak(cp, np);
        break;

    case N_CALL:
        astCall(cp, np);
        codeRequired++;
        break;

    case N_CASE_ELEMENTS:
        astCaseElements(cp, np);
        codeRequired++;
        break;

    case N_CASE_LABEL:
        astCaseLabel(cp, np);
        codeRequired++;
        break;

    case N_CATCH:
        astCatch(cp, np);
        codeRequired++;
        break;

    case N_CATCH_ARG:
        codeRequired++;
        break;

    case N_CLASS:
        astClass(cp, np);
        type = np->klass.ref;
        codeRequired++;
        break;

    case N_CONTINUE:
        break;

    case N_DIRECTIVES:
        astDirectives(cp, np);
        break;

    case N_DO:
        astDo(cp, np);
        codeRequired++;
        break;

    case N_DOT:
        astDot(cp, np);
        codeRequired++;
        break;

    case N_END_FUNCTION:
        break;

    case N_EXPRESSIONS:
        astExpressions(cp, np);
        break;

    case N_FOR:
        astFor(cp, np);
        codeRequired++;
        break;

    case N_FOR_IN:
        astForIn(cp, np);
        codeRequired++;
        break;

    case N_FUNCTION:
        astFunction(cp, np);
        break;

    case N_LITERAL:
        codeRequired++;
        break;

    case N_OBJECT_LITERAL:
        astObjectLiteral(cp, np);
        codeRequired++;
        break;

    case N_FIELD:
        astField(cp, np);
        codeRequired++;
        break;

    case N_QNAME:
        astName(cp, np);
        codeRequired++;
        break;

    case N_NEW:
        astNew(cp, np);
        codeRequired++;
        break;

    case N_NOP:
        break;

    case N_POSTFIX_OP:
        astPostfixOp(cp, np);
        codeRequired++;
        break;

    case N_PRAGMAS:
        astPragmas(cp, np);
        break;

    case N_PRAGMA:
        astPragma(cp, np);
        break;

    case N_PROGRAM:
        astProgram(cp, np);
        break;

    case N_REF:
        codeRequired++;
        break;

    case N_RETURN:
        astReturn(cp, np);
        codeRequired++;
        break;

    case N_SUPER:
        astSuper(cp, np);
        codeRequired++;
        break;

    case N_SWITCH:
        astSwitch(cp, np);
        codeRequired++;
        break;

    case N_HASH:
        astHash(cp, np);
        break;

    case N_IF:
        astIf(cp, np);
        codeRequired++;
        break;

    case N_THIS:
        astThis(cp, np);
        codeRequired++;
        break;

    case N_THROW:
        astThrow(cp, np);
        codeRequired++;
        break;

    case N_TRY:
        astTry(cp, np);
        break;

    case N_UNARY_OP:
        astUnaryOp(cp, np);
        codeRequired++;
        break;

    case N_MODULE:
        astModule(cp, np);
        break;
            
    case N_TYPE_IDENTIFIERS:
        astImplements(cp, np);
        break;

    case N_USE_NAMESPACE:
        astUseNamespace(cp, np);
        /*
         *  Namespaces by themselves don't required code. Need something to use the namespace.
         */
        break;

    case N_USE_MODULE:
        astRequire(cp, np);
        break;

    case N_VAR_DEFINITION:
        astVarDefinition(cp, np, &codeRequired, &instanceCode);
        break;

    case N_VOID:
        astVoid(cp, np);
        break;

    case N_WITH:
        astWith(cp, np);
        break;

    default:
        mprAssert(0);
        badAst(cp, np);
    }
    
    /*
     *  Determine if classes need initializers. If class code is generated outside of a method, then some form of
     *  initialization will be required. Either a class constructor, initializer or a global initializer.
     */
    if (cp->phase == EC_PHASE_DEFINE && codeRequired && !state->inMethod && !state->inHashExpression) {

        if (state->inClass && !state->currentClass->isInterface) {
            if (instanceCode) {
                state->currentClass->hasConstructor = 1;
                state->currentClass->hasInitializer = 1;
            } else {
                state->currentClass->hasStaticInitializer = 1;
            }
        } else {
            state->currentModule->hasInitializer = 1;
        }
    }
    mprAssert(state == cp->state);
    LEAVE(cp);
}


static void removeProperty(EcCompiler *cp, EjsVar *block, EcNode *np)
{
    Ejs             *ejs;
    EjsName         *prop;
    MprList         *globals;
    int             next, slotNum;

    mprAssert(block);

    ejs = cp->ejs;

    if (np->globalProp) {
        globals = cp->state->currentModule->globalProperties;
        mprAssert(globals);

        for (next = 0; ((prop = (EjsName*) mprGetNextItem(globals, &next)) != 0); ) {
            if (strcmp(np->qname.space, prop->space) == 0 && strcmp(np->qname.name, prop->name) == 0) {
                mprRemoveItem(globals, prop);
                break;
            }
        }
    }
        
    slotNum = ejsLookupProperty(ejs, block, &np->qname);
    ejsRemoveProperty(ejs, (EjsBlock*) block, slotNum);

}


/*
 *  Fixup all slot definitions in types. When types are first created, they do not reserve space for inherited slots.
 *  Now that all types should have been resolved, we can reserve room for inherited slots. Override functions also 
 *  must be removed.
 */
static void fixupBlockSlots(EcCompiler *cp, EjsType *type)
{
    Ejs             *ejs;
    EjsType         *baseType, *iface, *owner;
    EjsFunction     *fun;
    EcNode          *np, *child;
    EjsName         qname;
    EjsTrait        *trait;
    int             rc, slotNum, attributes, next;

    if (type->block.obj.var.visited || !type->needFixup) {
        return;
    }

    mprAssert(cp);
    mprAssert(type);
    mprAssert(ejsIsType(type));

    ENTER(cp);

    rc = 0;
    ejs = cp->ejs;
    type->block.obj.var.visited = 1;
    np = (EcNode*) type->typeData;
    baseType = type->baseType;

    if (baseType == 0) {
        if (np && np->kind == N_CLASS && !np->klass.isInterface) {
            if (np->klass.extends) {
                ejsName(&qname, 0, np->klass.extends);
                baseType = (EjsType*) getProperty(cp, ejs->global, &qname);

            } else {
                if (! (cp->empty && strcmp(type->qname.name, "Object") == 0)) {
                    ejsName(&qname, EJS_INTRINSIC_NAMESPACE, "Object");
                    baseType = (EjsType*) getProperty(cp, ejs->global, &qname);
                }
            }
        }
    }

    if (np->klass.implements) {
        type->implements = mprCreateList(type);
        next = 0;
        while ((child = getNextAstNode(cp, np->klass.implements, &next))) {
            iface = (EjsType*) getProperty(cp, ejs->global, &child->qname);
            if (iface) {
                mprAddItem(type->implements, iface);
            } else {
                astError(cp, np, "Can't find interface \"%s\"", child->qname.name);
                type->block.obj.var.visited = 0;
                LEAVE(cp);
                return;
            }
        }
    }

    if (baseType == 0) {
        if (!(cp->empty && strcmp(type->qname.name, "Object") == 0) && !np->klass.isInterface) {
            astError(cp, np, "Can't find base type for %s", type->qname.name);
            type->block.obj.var.visited = 0;
            LEAVE(cp);
            return;
        }
    }

    if (baseType) {
        if (baseType->needFixup) {
            fixupBlockSlots(cp, baseType);
        }
        if (baseType->hasConstructor) {
            type->hasBaseConstructors = 1;
        }
        if (baseType->hasInitializer) {
            type->hasBaseInitializers = 1;
        }
        if (baseType->hasStaticInitializer) {
            type->hasBaseStaticInitializers = 1;
        }
    }

    if (type->implements) {
        for (next = 0; ((iface = mprGetNextItem(type->implements, &next)) != 0); ) {
            if (iface->needFixup) {
                fixupBlockSlots(cp, iface);
            }
            if (iface->hasConstructor) {
                type->hasBaseConstructors = 1;
            }
            if (iface->hasInitializer) {
                type->hasBaseInitializers = 1;
            }
            if (iface->hasStaticInitializer) {
                type->hasBaseStaticInitializers = 1;
            }
            
        }
    }

    if (!type->block.obj.var.isInstanceBlock && (EjsVar*) type != ejs->global && !type->isInterface) {
        /*
         *  Remove the static initializer slot if this class does not require a static initializer
         *  By convention, it is installed in slot number 1.
         */
        if (type->hasBaseStaticInitializers) {
            type->hasStaticInitializer = 1;
        }
        if (!type->hasStaticInitializer) {
            ejsRemoveProperty(ejs, (EjsBlock*) type, 1);
        }

        /*
         *  Remove the constructor slot if this class does not require a constructor. ie. no base classes have constructors,
         */
        if (type->hasBaseConstructors) {
            type->hasConstructor = 1;
        }
        if (!type->hasConstructor) {
            if (np && np->klass.constructor && np->klass.constructor->function.isDefaultConstructor) {
                ejsRemoveProperty(ejs, (EjsBlock*) type, 0);
                np->klass.constructor = 0;
            }
        }
    }

    if (cp->empty) {
        if (type->hasConstructor && strcmp("Object", type->qname.name) == 0 && 
                strcmp(EJS_INTRINSIC_NAMESPACE, type->qname.space) == 0) {
            astWarn(cp, np, "Object class requires a constructor, but the native class does not implement one.");
        }
    }

    /*
     *  Now that we've recursively done all base types above, we can fixup this type.
     */
    ejsFixupClass(ejs, type, baseType, type->implements, 1);
    
    /*
     *  Mark all methods implemented for interfaces as implicitly overridden
     */
    for (slotNum = 0; slotNum < type->block.numInherited; slotNum++) {
        qname = ejsGetPropertyName(ejs, (EjsVar*) type, slotNum);
        fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, slotNum);
        if (fun && ejsIsFunction(fun)) {
            owner = (EjsType*) fun->owner;
            if (owner != type && ejsIsType(owner) && owner->isInterface) {
                fun->override = 1;
            }
        }
    }
    
    /*
     *  Remove the original overridden method slots. Set the inherited slot to the overridden method.
     */
    for (slotNum = type->block.numInherited; slotNum < type->block.obj.numProp; slotNum++) {

        qname = ejsGetPropertyName(ejs, (EjsVar*) type, slotNum);
        trait = ejsGetPropertyTrait(ejs, (EjsVar*) type, slotNum);
        if (trait == 0) {
            continue;
        }
        attributes = trait->attributes;

        if (attributes & EJS_ATTR_OVERRIDE) {

            fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, slotNum);
            mprAssert(fun && ejsIsFunction(fun));
            ejsRemoveProperty(ejs, (EjsBlock*) type, slotNum);
            slotNum--;

            if (resolveName(cp, 0, (EjsVar*) type, &qname) < 0 || cp->lookup.slotNum < 0) {
                astError(cp, 0, "Can't find method \"%s::%s\" in base type of \"%s\" to override", qname.space, qname.name, 
                    type->qname.name);
                
            } else {
                fun->slotNum = cp->lookup.slotNum;
                ejsSetProperty(ejs, (EjsVar*) type, cp->lookup.slotNum, (EjsVar*) fun);
                trait = ejsGetTrait((EjsBlock*) type, cp->lookup.slotNum);
                ejsSetTraitAttributes(trait, attributes);
            }
        }
    }
    
    if (baseType && (baseType->instanceBlock || type->implements)) {
        mprAssert(type->baseType == baseType);
        if (type->instanceBlock == 0) {
            type->instanceBlock = ejsCreateTypeInstanceBlock(ejs, type, 0);
        }
        ejsFixupBlock(ejs, type->instanceBlock, baseType->instanceBlock, type->implements, 1);
    }
    type->block.obj.var.visited = 0;
    LEAVE(cp);
}


/*
 *  Lookup a namespace in the current scope. We look for the namespace variable declaration if it is a user
 *  defined namespace. Otherwise, we trust that if the set of open namespaces has the namespace -- it must exist.
 */
static EjsNamespace *resolveNamespace(EcCompiler *cp, EcNode *np, EjsVar *block, bool *modified)
{
    Ejs             *ejs;
    EjsName         qname;
    EjsNamespace    *namespace;
    int             slotNum;

    ejs = cp->ejs;

    if (modified) {
        *modified = 0;
    }

    /*
     *  Resolve the namespace. Must be visible in the current scope. Then update the URI.
     */
    qname.name = np->qname.space;
    qname.space = 0;
    namespace = (EjsNamespace*) getProperty(cp, 0, &qname);
    if (namespace == 0 || !ejsIsNamespace(namespace)) {
        namespace = ejsLookupNamespace(cp->ejs, np->qname.space);
    }

    if (namespace == 0) {
        if (strcmp(cp->state->namespace, np->qname.space) == 0) {
            namespace = ejsCreateNamespace(ejs, np->qname.space, np->qname.space);
        }
    }

    if (namespace == 0) {
        if (!np->literalNamespace) {
            astError(cp, np, "Can't find namespace \"%s\"", qname.name);
        }

    } else {
        if (strcmp(namespace->uri, np->qname.space) != 0) {
            slotNum = ejsLookupProperty(ejs, block, &np->qname);
            mprAssert(slotNum >= 0);
            if (slotNum >= 0) {
                mprFree((char*) np->qname.space);
                /*
                 *  Change the name to use the namespace URI. This will change the property name and set
                 *  "modified" so that the caller can modify the type name if block is a type.
                 */
                np->qname.space = mprStrdup(np, namespace->uri);
                ejsSetPropertyName(ejs, block, slotNum, &np->qname);
                if (modified) {
                    *modified = 1;
                }
            }
        }
    }

    return namespace;
}


/*
 *  Locate a property via lookup and determine the best way to address the property.
 */
static int resolveName(EcCompiler *cp, EcNode *np, EjsVar *vp, EjsName *qname)
{
    Ejs         *ejs;
    EjsLookup   *lookup;
    EjsType     *type, *currentClass, *tp;
    EcState     *state;
    EjsBlock    *block;

    ejs = cp->ejs;
    state = cp->state;
    lookup = &cp->lookup;

    if (vp) {
        if (ecLookupVar(cp, vp, qname, 1) < 0) {
            return EJS_ERR;
        }
        lookup->originalObj = vp;

    } else {
        if (ecLookupScope(cp, qname, 1) < 0) {
            return EJS_ERR;
        }
        lookup->originalObj = lookup->obj;
    }

    /*
     *  Revise the nth block to account for blocks that will be erased
     */
    lookup->nthBlock = 0;
    for (block = ejs->state->bp->scopeChain; block; block = block->scopeChain) {
        if ((EjsVar*) block == lookup->obj) {
            break;
        }
        if (ejsIsType(block)) {
            type = (EjsType*) block;
            if ((EjsVar*) type->instanceBlock == lookup->obj) {
                break;
            }
        }
        if (!block->obj.var.hidden) {
            lookup->nthBlock++;
        }
    }
    if (block == 0) {
        lookup->nthBlock = 0;
    }

    lookup->ref = ejsGetProperty(ejs, lookup->obj, lookup->slotNum);
    
    if (lookup->ref == ejs->nullValue) {
        lookup->ref = 0;
    }

    mprAssert(lookup->trait == 0);
    lookup->trait = ejsGetPropertyTrait(ejs, lookup->obj, lookup->slotNum);

    if ((ejsIsType(lookup->obj) || ejsIsInstanceBlock(lookup->obj)) && state->currentObjectNode == 0) {
        mprAssert(lookup->obj != ejs->global);
        //  NOTE: could potentially do this for static properties as well
        if (lookup->trait) {
            /*
             *  class instance or method properties
             */
            type = (EjsType*) lookup->obj;
            currentClass = state->currentClass;
            if (currentClass) {
                mprAssert(state->inClass);
                for (tp = currentClass; tp; tp = tp->baseType) {
                    if ((EjsVar*) tp == lookup->obj || (EjsVar*) tp->instanceBlock == lookup->obj) {
                        /*
                         *  Method code or class level instance initialization code. This is code that is a subtype of the 
                         *  type owning the property, so we can use the thisObj to access it.
                         */
                        if (state->inClass) {
                            lookup->useThis = 1;
                        }
                    }
                }
            }
        }
    }

    if (np) {
        np->lookup = cp->lookup;
#if DISABLE || 1
        if (np->lookup.slotNum >= 0) {
            /*
             *  Once we have resolved the name, we now know the selected namespace. Update it in "np" so that if
             *  --nobind is selected, we still get the variable of the correct namespace.
             */
            np->qname.space = np->lookup.name.space;
        }
#endif
    }
    return 0;
}


/*
 *  Locate a property in context. NOTE this only works for type properties not instance properties.
 */
static EjsVar *getProperty(EcCompiler *cp, EjsVar *vp, EjsName *name)
{
    EcNode      node;

    mprAssert(cp);

    if (resolveName(cp, &node, vp, name) < 0) {
        return 0;
    }

    /*
     *  NOTE: ref may be null if searching for an instance property.
     */
    return node.lookup.ref;
}


/*
 *  Wrap the define property routine. Need to keep a module to property mapping
 */
static void addGlobalProperty(EcCompiler *cp, EcNode *np, EjsName *qname)
{
    Ejs             *ejs;
    EjsModule       *up;
    EjsName         *prop, *p;
    int             next;

    ejs = cp->ejs;

    up = cp->state->currentModule;
    mprAssert(up);

    prop = mprAllocObjZeroed(cp, EjsName);
    *prop = *qname;

    if (up->globalProperties == 0) {
        up->globalProperties = mprCreateList(up);
    }

    for (next = 0; (p = (EjsName*) mprGetNextItem(up->globalProperties, &next)) != 0; ) {
        if (strcmp(p->name, prop->name) == 0 && strcmp(p->space, prop->space) == 0) {
            return;
        }
    }
    next = mprAddItem(up->globalProperties, prop);
    if (np) {
        np->globalProp = prop;
    }
}


static void setAstDocString(Ejs *ejs, EcNode *np, EjsVar *block, int slotNum)
{
#if BLD_FEATURE_EJS_DOC
    mprAssert(block);

    if (np->doc && slotNum >= 0 && ejsIsBlock(block)) {
        ejsCreateDoc(ejs, (EjsBlock*) block, slotNum, np->doc);
    }
#endif
}


static void addScope(EcCompiler *cp, EjsBlock *block)
{
    block->scopeChain = cp->ejs->state->bp->scopeChain;
    cp->ejs->state->bp->scopeChain = block;
}


static void removeScope(EcCompiler *cp)
{
    EjsBlock    *block;

    block = cp->ejs->state->bp;
    block->scopeChain = block->scopeChain->scopeChain;
}


/*
 *  Create a new lexical block scope and open it
 */
static void openBlock(EcCompiler *cp, EcNode *np, EjsBlock *block)
{
    Ejs             *ejs;
    EcState         *state;
    EjsNamespace    *namespace;
    char            *debugName;
    int             next;

    ejs = cp->ejs;
    state = cp->state;

    if (cp->phase == EC_PHASE_DEFINE) {
        if (block == 0) {
            static int index = 0;
            if (np->filename == 0) {
                debugName = mprAsprintf(np, -1, "block_%04d", index++);
            } else {
                debugName = mprAsprintf(np, -1, "block_%04d_%d", np->lineNumber, index++);
            }
            block = ejsCreateBlock(cp->ejs, 0);
            ejsSetDebugName(block, debugName);
            np->qname.name = debugName;
            np->qname.space = EJS_BLOCK_NAMESPACE;
        }
        np->blockRef = block;

    } else {
        /*
         *  Must reset the namespaces each phase. This is because pragmas must apply from the point of use in a block onward
         *  only. Except for hoisted variable namespaces which must apply from the start of the block. They are applied below
         */
        if (block == 0) {
            block = np->blockRef;
        }
        mprAssert(block != ejs->globalBlock);
        ejsResetBlockNamespaces(ejs, block);
    }
    
    state->namespaceCount = ejsGetNamespaceCount(block);

    /*
     *  Special case for the outermost module block. The module (file) block is created to provide a compilation unit
     *  level scope. However, we do not use the block for the let or var scope, rather we use the global scope.
     *  Namespaces always use this new block.
     */
    if (! (state->letBlock == ejs->global && np->parent->kind == N_MODULE)) {
        state->optimizedLetBlock = (EjsVar*) block;
    }
    state->letBlock = (EjsVar*) block;
    state->letBlockNode = np;

    /*
     *  Add namespaces that must apply from the start of the block. Current users: hoisted let vars.
     */
    if (np->namespaces) {
        for (next = 0; (namespace = (EjsNamespace*) mprGetNextItem(np->namespaces, &next)) != 0; ) {
            ejsAddNamespaceToBlock(ejs, block, namespace);
        }
    }
    /*
     *  Mark the state corresponding to the last opened block
     */
    state->prevBlockState = cp->blockState;
    cp->blockState = state;
    addScope(cp, block);
}


static void closeBlock(EcCompiler *cp)
{
    ejsPopBlockNamespaces((EjsBlock*) cp->state->letBlock, cp->state->namespaceCount);
    cp->blockState = cp->state->prevBlockState;
    removeScope(cp);
}


static EjsNamespace *createHoistNamespace(EcCompiler *cp, EjsVar *obj)
{
    EjsNamespace    *namespace;
    Ejs             *ejs;
    EcNode          *letBlockNode;
    char            *spaceName;

    ejs = cp->ejs;
    spaceName = mprAsprintf(cp, -1, "-hoisted-%d", ejsGetPropertyCount(ejs, obj));
    namespace = ejsCreateNamespace(ejs, spaceName, spaceName);

    letBlockNode = cp->state->letBlockNode;
    if (letBlockNode->namespaces == 0) {
        letBlockNode->namespaces = mprCreateList(letBlockNode);
    }
    mprAddItem(letBlockNode->namespaces, namespace);
    ejsAddNamespaceToBlock(ejs, (EjsBlock*) cp->state->optimizedLetBlock, namespace);

    return namespace;
}


static EjsNamespace *ejsLookupNamespace(Ejs *ejs, cchar *namespace)
{
    EjsList         *namespaces;
    EjsNamespace    *nsp;
    EjsBlock        *block;
    int             nextNamespace;

    /*
     *  Lookup the scope chain considering each block and the open namespaces at that block scope.
     */
    for (block = ejs->state->bp; block; block = block->scopeChain) {
        if (!ejsIsBlock(block)) {
            continue;
        }

        namespaces = &block->namespaces;
        if (namespaces == 0) {
            namespaces = &ejs->globalBlock->namespaces;
        }

#if BLD_DEBUG
        mprLog(ejs, 7, "ejsLookupNamespace in %s", ejsGetDebugName(block));
        for (nextNamespace = 0; (nsp = (EjsNamespace*) ejsGetNextItem(namespaces, &nextNamespace)) != 0; ) {
            mprLog(ejs, 7, "    scope \"%s\"", nsp->uri);
        }
#endif

        mprLog(ejs, 7, "    SEARCH for qname \"%s\"", namespace);
        for (nextNamespace = -1; (nsp = (EjsNamespace*) ejsGetPrevItem(namespaces, &nextNamespace)) != 0; ) {
            if (strcmp(nsp->uri, namespace) == 0) {
                return nsp;
            }
        }
    }

    return 0;
}


/*
 *  Look for a variable by name in the scope chain and return the location in "cp->lookup" and a positive slot 
 *  number if found.  If the name.space is non-null/non-empty, then only the given namespace will be used. 
 *  otherwise the set of open namespaces will be used. The lookup structure will contain details about the location 
 *  of the variable.
 */
int ecLookupScope(EcCompiler *cp, EjsName *name, bool anySpace)
{
    Ejs             *ejs;
    EjsBlock        *block;
    EjsLookup       *lookup;
    int             slotNum, nth;

    mprAssert(name);

    ejs = cp->ejs;
    slotNum = -1;

    if (name->space == 0) {
        name->space = "";
    }

    lookup = &cp->lookup;
    lookup->ref = 0;
    lookup->trait = 0;
    lookup->name.name = 0;
    lookup->name.space = 0;

    /*
     *  Look in the scope chain considering each block scope. LookupVar will consider base classes and namespaces.
     */
    for (nth = 0, block = ejs->state->bp; block; block = block->scopeChain) {

        if ((slotNum = ecLookupVar(cp, (EjsVar*) block, name, anySpace)) >= 0) {
            lookup->nthBlock = nth;
            break;
        }
        nth++;
    }
    lookup->slotNum = slotNum;

    return slotNum;
}


/*
 *  Find a property in an object or type and its base classes.
 */
int ecLookupVar(EcCompiler *cp, EjsVar *vp, EjsName *name, bool anySpace)
{
    Ejs             *ejs;
    EjsLookup       *lookup;
    EjsType         *type;
    int             slotNum;

    mprAssert(vp);
    mprAssert(vp->type);
    mprAssert(name);

    ejs = cp->ejs;

    if (name->space == 0) {
        name->space = "";
    }    

    lookup = &cp->lookup;
    lookup->ref = 0;
    lookup->trait = 0;
    lookup->name.name = 0;
    lookup->name.space = 0;

    /*
     *  OPT - bit field initialization
     */
    lookup->nthBase = 0;
    lookup->nthBlock = 0;
    lookup->useThis = 0;
    lookup->instanceProperty = 0;
    lookup->ownerIsType = 0;

    /*
     *  Search through the inheritance chain of base classes.
     *  nthBase is incremented from zero for every subtype that must be traversed. 
     */
    for (slotNum = -1, lookup->nthBase = 0; vp; lookup->nthBase++) {
        if ((slotNum = ejsLookupVarWithNamespaces(ejs, vp, name, lookup)) >= 0) {
            break;
        }
        if (! ejsIsType(vp)) {
            vp = (EjsVar*) vp->type;
            continue;
        }
    
        type = (EjsType*) vp;
        if (type->instanceBlock && type->instanceBlock->obj.numProp > 0) {
            /*
             *  Search the instance object
             */
            if ((slotNum = ejsLookupVarWithNamespaces(ejs, (EjsVar*) type->instanceBlock, name, lookup)) >= 0) {
                lookup->instanceProperty = 1;
                break;
            }
        }
        vp = (EjsVar*) ((EjsType*) vp)->baseType;
    }
    return lookup->slotNum = slotNum;
}


static void allocName(Ejs *ejs, EjsName *qname)
{
    qname->space = mprStrdup(ejs, qname->space);
    qname->name = mprStrdup(ejs, qname->name);
}

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
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
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/compiler/ecAst.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/compiler/ecCodeGen.c"
 */
/************************************************************************/

/**
 *  ecCodeGen.c - Ejscript code generator
 *
 *  This module generates code for a program that is represented by an in-memory AST set of nodes.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




/*
 *  State level macros. Enter/Leave manage state and inheritance of state.
 */
#undef ENTER
#define ENTER(a)    if (ecEnterState(a) < 0) { return; } else

#undef LEAVE
#define LEAVE(cp)   ecLeaveState(cp)

#define SAVE_ONLEFT(cp)                                     \
    if (1) {                                                \
            cp->state->saveOnLeft = cp->state->onLeft;      \
            cp->state->onLeft = 0;                          \
    } else

#define RESTORE_ONLEFT(cp)                                  \
    cp->state->onLeft = cp->state->saveOnLeft


static void     addDebugInstructions(EcCompiler *cp, EcNode *np);
static void     addException(EcCompiler *cp, uint tryStart, uint tryEnd, EjsType *catchType, uint handlerStart, 
                    uint handlerEnd, int numBlocks, int numStack, int flags);
static void     addJump(EcCompiler *cp, EcNode *np, int kind);
static void     addModule(EcCompiler *cp, EjsModule *mp);
static EcCodeGen *allocCodeBuffer(EcCompiler *cp);
static void     badNode(EcCompiler *cp, EcNode *np);
static void     copyCodeBuffer(EcCompiler *cp, EcCodeGen *dest, EcCodeGen *code);
static void     createInitializer(EcCompiler *cp, EjsModule *mp);
static void     discardStackItems(EcCompiler *cp, int preserve);
static void     emitNamespace(EcCompiler *cp, EjsNamespace *nsp);
static int      flushModule(MprFile *file, EcCodeGen *code);
static void     genBinaryOp(EcCompiler *cp, EcNode *np);
static void     genBlock(EcCompiler *cp, EcNode *np);
static void     genBreak(EcCompiler *cp, EcNode *np);
static void     genBoundName(EcCompiler *cp, EcNode *np);
static void     genCall(EcCompiler *cp, EcNode *np);
static void     genCatchArg(EcCompiler *cp, EcNode *np);
static void     genClass(EcCompiler *cp, EcNode *child);
static void     genClassName(EcCompiler *cp, EjsType *type);
static void     genContinue(EcCompiler *cp, EcNode *np);
static void     genDirectives(EcCompiler *cp, EcNode *np, bool saveResult);
static void     genDo(EcCompiler *cp, EcNode *np);
static void     genDot(EcCompiler *cp, EcNode *np, EcNode **rightMost);
static void     genError(EcCompiler *cp, EcNode *np, char *fmt, ...);
static void     genEndFunction(EcCompiler *cp, EcNode *np);
static void     genExpressions(EcCompiler *cp, EcNode *np);
static void     genFor(EcCompiler *cp, EcNode *np);
static void     genForIn(EcCompiler *cp, EcNode *np);
static void     genFunction(EcCompiler *cp, EcNode *np);
static void     genHash(EcCompiler *cp, EcNode *np);
static void     genIf(EcCompiler *cp, EcNode *np);
static void     genLeftHandSide(EcCompiler *cp, EcNode *np);
static void     genLiteral(EcCompiler *cp, EcNode *np);
static void     genLogicalOp(EcCompiler *cp, EcNode *np);
static void     genModule(EcCompiler *cp, EcNode *np);
static void     genName(EcCompiler *cp, EcNode *np);
static void     genNameExpr(EcCompiler *cp, EcNode *np);
static void     genNew(EcCompiler *cp, EcNode *np);
static void     genObjectLiteral(EcCompiler *cp, EcNode *np);
static void     genProgram(EcCompiler *cp, EcNode *np);
static void     genPragmas(EcCompiler *cp, EcNode *np);
static void     genPostfixOp(EcCompiler *cp, EcNode *np);
static void     genReturn(EcCompiler *cp, EcNode *np);
static void     genSuper(EcCompiler *cp, EcNode *np);
static void     genSwitch(EcCompiler *cp, EcNode *np);
static void     genThis(EcCompiler *cp, EcNode *np);
static void     genThrow(EcCompiler *cp, EcNode *np);
static void     genTry(EcCompiler *cp, EcNode *np);
static void     genUnaryOp(EcCompiler *cp, EcNode *np);
static void     genUnboundName(EcCompiler *cp, EcNode *np);
static void     genUseNamespace(EcCompiler *cp, EcNode *np);
static void     genVar(EcCompiler *cp, EcNode *np);
static void     genVarDefinition(EcCompiler *cp, EcNode *np);
static void     genWith(EcCompiler *cp, EcNode *np);
static int      getCodeLength(EcCompiler *cp, EcCodeGen *code);
static EcNode   *getNextNode(EcCompiler *cp, EcNode *np, int *next);
static EcNode   *getPrevNode(EcCompiler *cp, EcNode *np, int *next);
static int      getStackCount(EcCompiler *cp);
static int      mapToken(EcCompiler *cp, EjsOpCode tokenId);
static MprFile  *openModuleFile(EcCompiler *cp, cchar *filename);
static void     patchJumps(EcCompiler *cp, int kind, int target);
static void     popStack(EcCompiler *cp, int count);
static void     processNode(EcCompiler *cp, EcNode *np);
static void     processModule(EcCompiler *cp, EjsModule *mp);
static void     pushStack(EcCompiler *cp, int count);
static void     setCodeBuffer(EcCompiler *cp, EcCodeGen *saveCode);
static void     setFunctionCode(EcCompiler *cp, EjsFunction *fun, EcCodeGen *code);
static void     setStack(EcCompiler *cp, int count);

/*
 *  Generate code for evaluating conditional compilation directives
 */
void ecGenConditionalCode(EcCompiler *cp, EcNode *np, EjsModule *mp)
{
    EcState         *state;

    ENTER(cp);

    state = cp->state;
    mprAssert(state);

    addModule(cp, mp);
    genDirectives(cp, np, 1);

    if (cp->errorCount > 0) {
        ecRemoveModule(cp, mp);
        LEAVE(cp);
        return;
    }
    createInitializer(cp, mp);
    ecRemoveModule(cp, mp);
    LEAVE(cp);
}


/*
 *  Top level for code generation. Loop through the AST nodes recursively.
 */
int ecCodeGen(EcCompiler *cp, int argc, EcNode **nodes)
{
    EjsModule   *mp;
    EcNode      *np;
    int         i, version, next;

    if (ecEnterState(cp) < 0) {
        return EJS_ERR;
    }
    for (i = 0; i < argc; i++) {
        np = nodes[i];
        cp->fileState = cp->state;
        cp->fileState->mode = cp->defaultMode;
        cp->fileState->lang = cp->lang;
        if (np) {
            processNode(cp, np);
        }
    }

    /*
     *  Open once if merging into a single output file
     */
    if (cp->outputFile) {
        for (version = next = 0; (mp = mprGetNextItem(cp->modules, &next)) != 0; ) {
            mprAssert(!mp->loaded);
            if (next <= 1 || mp->globalProperties || mp->hasInitializer || strcmp(mp->name, EJS_DEFAULT_MODULE) != 0) {
                version = mp->version;
                break;
            }
        }
        if (openModuleFile(cp, cp->outputFile) == 0) {
            return EJS_ERR;
        }
    }

    /*
     *  Now generate code for all the modules
     */
    for (next = 0; (mp = mprGetNextItem(cp->modules, &next)) != 0; ) {
        if (mp->loaded) {
            continue;
        }
        /*
         *  Don't generate the default module unless it contains some real code or definitions and 
         *  we have more than one module.
         */
        if (mprGetListCount(cp->modules) == 1 || mp->globalProperties || mp->hasInitializer || 
                strcmp(mp->name, EJS_DEFAULT_MODULE) != 0) {
            mp->initialized = 0;
            processModule(cp, mp);
        }
    }

    if (cp->outputFile) {
        if (flushModule(cp->file, cp->state->code) < 0) {
            genError(cp, 0, "Can't write to module file %s", cp->outputFile);
        }
        mprFree(cp->file);
        cp->file = 0;
    }
    cp->file = 0;
    ecLeaveState(cp);
    return (cp->fatalError) ? EJS_ERR : 0;
}


static void genArgs(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    ENTER(cp);

    mprAssert(np->kind == N_ARGS);

    cp->state->needsValue = 1;

    next = 0;
    while ((child = getNextNode(cp, np, &next)) && !cp->error) {
        if (child->kind == N_ASSIGN_OP) {
            child->needDup = 1;
        }
        processNode(cp, child);
        child->needDup = 0;
    }
    LEAVE(cp);
}


static void genArrayLiteral(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    ENTER(cp);

    next = 0;
    while ((child = getNextNode(cp, np, &next)) != 0) {
        /* Don't propagate needsValue here. We have a new and that will take care of the residual value */
        cp->state->needsValue = 0;
        processNode(cp, child);
    }
    LEAVE(cp);
}


/*
 *  Generate an assignment expression
 */
static void genAssignOp(EcCompiler *cp, EcNode *np)
{
    EcState     *state;
    int         rc, next;

    ENTER(cp);

    state = cp->state;
    rc = 0;
    next = 0;

    mprAssert(np->kind == N_ASSIGN_OP);
    mprAssert(np->left);
    mprAssert(np->right);

    state->onLeft = 0;

    /*
     *  Dup the object on the stack so it is available for subsequent operations
     */
    if (np->needDupObj) {
        ecEncodeOpcode(cp, EJS_OP_DUP);
        pushStack(cp, 1);
    }

    /*
     *  Process the expression on the right. Leave the result on the stack.
     */
    if (np->right->kind == N_ASSIGN_OP) {
        np->right->needDup = 1;
    }

    state->needsValue = 1;
    processNode(cp, np->right);
    state->needsValue = 0;

    if (np->needDupObj) {
        /*
         *  Get the object on the top above the value
         */
        ecEncodeOpcode(cp, EJS_OP_SWAP);
    }

    /*
     *  If this expression is part of a function argument, the result must be preserved.
     */
    if (np->needDup || state->prev->needsValue) {
        ecEncodeOpcode(cp, EJS_OP_DUP);
        pushStack(cp, 1);
    }

    /*
     *  Store to the left hand side
     */
    genLeftHandSide(cp, np->left);
    LEAVE(cp);
}


static void genBinaryOp(EcCompiler *cp, EcNode *np)
{
    EcState     *state;

    ENTER(cp);

    state = cp->state;
    state->needsValue = 1;

    mprAssert(np->kind == N_BINARY_OP);

    switch (np->tokenId) {
    case T_LOGICAL_AND:
    case T_LOGICAL_OR:
        genLogicalOp(cp, np);
        break;

    default:
        if (np->left) {
            processNode(cp, np->left);
        }
        if (np->right) {
            processNode(cp, np->right);
        }
        ecEncodeOpcode(cp, mapToken(cp, np->tokenId));
        popStack(cp, 2);
        pushStack(cp, 1);
        break;
    }

    mprAssert(state == cp->state);
    LEAVE(cp);
}


static void genBreak(EcCompiler *cp, EcNode *np)
{
    EcState     *state;

    ENTER(cp);
    state = cp->state;

    if (state->captureBreak) {
        ecEncodeOpcode(cp, EJS_OP_FINALLY);
    }
    if (state->code->jumps == 0 || !(state->code->jumpKinds & EC_JUMP_BREAK)) {
        genError(cp, np, "Illegal break statement");
    } else {
        discardStackItems(cp, state->code->breakMark);
        ecEncodeOpcode(cp, EJS_OP_GOTO);
        addJump(cp, np, EC_JUMP_BREAK);
        ecEncodeWord(cp, 0);
    }
    LEAVE(cp);
}


static void genBlock(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsNamespace    *namespace;
    EcState         *state;
    EjsBlock        *block;
    EjsLookup       *lookup;
    EcNode          *child;
    int             next;

    ENTER(cp);

    state = cp->state;
    ejs = cp->ejs;
    block = (EjsBlock*) np->blockRef;
    
    if (block && np->createBlockObject) {
        state->prevBlockState = cp->blockState;
        cp->blockState = state;

        lookup = &np->lookup;
        if (lookup->slotNum >= 0) {
            ecEncodeOpcode(cp, EJS_OP_OPEN_BLOCK);
            ecEncodeNumber(cp, lookup->slotNum);
            ecEncodeNumber(cp, lookup->nthBlock);
        }

        /*
         *  Emit block namespaces
         */
        if (block->namespaces.length > 0) {
            for (next = 0; ((namespace = (EjsNamespace*) ejsGetNextItem(&block->namespaces, &next)) != 0); ) {
                if (namespace->name[0] == '-') {
                    emitNamespace(cp, namespace);
                }
            }
        }
        state->letBlock = (EjsVar*) block;
        state->letBlockNode = np;

        next = 0;
        while ((child = getNextNode(cp, np, &next))) {
            processNode(cp, child);
        }
        if (lookup->slotNum >= 0) {
            ecEncodeOpcode(cp, EJS_OP_CLOSE_BLOCK);
        }
        cp->blockState = state->prevBlockState;
        ecAddNameConstant(cp, &np->qname);

    } else {
        next = 0;
        while ((child = getNextNode(cp, np, &next))) {
            processNode(cp, child);
        }
    }
    LEAVE(cp);
}


/*
 *  Block scope variable reference
 */
static void genBlockName(EcCompiler *cp, int slotNum, int nthBlock)
{
    int         code;

    mprAssert(slotNum >= 0);

    code = (!cp->state->onLeft) ?  EJS_OP_GET_BLOCK_SLOT :  EJS_OP_PUT_BLOCK_SLOT;
    ecEncodeOpcode(cp, code);
    ecEncodeNumber(cp, slotNum);
    ecEncodeNumber(cp, nthBlock);
    pushStack(cp, (cp->state->onLeft) ? -1 : 1);
}


static void genContinue(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    if (cp->state->captureBreak) {
        ecEncodeOpcode(cp, EJS_OP_FINALLY);
    }
    if (cp->state->code->jumps == 0 || !(cp->state->code->jumpKinds & EC_JUMP_CONTINUE)) {
        genError(cp, np, "Illegal continue statement");
    } else {
        ecEncodeOpcode(cp, EJS_OP_GOTO);
        addJump(cp, np, EC_JUMP_CONTINUE);
        ecEncodeWord(cp, 0);
    }
    LEAVE(cp);
}


static void genDelete(EcCompiler *cp, EcNode *np)
{
    EcNode      *left, *lright;

    ENTER(cp);

    mprAssert(np);

    left = np->left;
    mprAssert(left);

    switch (left->kind) {
    case N_DOT:
        processNode(cp, left->left);
        lright = left->right;
        if (lright->kind == N_QNAME) {
            /* delete obj.name */
            genNameExpr(cp, lright);
            ecEncodeOpcode(cp, EJS_OP_DELETE_NAME_EXPR);
            popStack(cp, 3);
        } else {
            /* delete obj[expr] */
            ecEncodeOpcode(cp, EJS_OP_LOAD_STRING);
            ecEncodeString(cp, EJS_EMPTY_NAMESPACE);
            processNode(cp, lright);
            ecEncodeOpcode(cp, EJS_OP_DELETE_NAME_EXPR);
            popStack(cp, 2);
        }
        break;

    case N_QNAME:
        /* delete space::name */
        genNameExpr(cp, left);
        ecEncodeOpcode(cp, EJS_OP_DELETE_SCOPED_NAME_EXPR);
        popStack(cp, 2);
        break;

    default:
        mprAssert(0);
    }
    LEAVE(cp);
}


/*
 *  Global variable
 */
static void genGlobalName(EcCompiler *cp, int slotNum)
{
    int     code;

    mprAssert(slotNum >= 0);

    code = (!cp->state->onLeft) ?  EJS_OP_GET_GLOBAL_SLOT :  EJS_OP_PUT_GLOBAL_SLOT;
    ecEncodeOpcode(cp, code);
    ecEncodeNumber(cp, slotNum);

    pushStack(cp, (cp->state->onLeft) ? -1 : 1);
}


/*
 *  Function local variable or argument reference
 */
static void genLocalName(EcCompiler *cp, int slotNum)
{
    int     code;

    mprAssert(slotNum >= 0);

    if (slotNum < 10) {
        code = (!cp->state->onLeft) ?  EJS_OP_GET_LOCAL_SLOT_0 :  EJS_OP_PUT_LOCAL_SLOT_0;
        ecEncodeOpcode(cp, code + slotNum);

    } else {
        code = (!cp->state->onLeft) ?  EJS_OP_GET_LOCAL_SLOT :  EJS_OP_PUT_LOCAL_SLOT;
        ecEncodeOpcode(cp, code);
        ecEncodeNumber(cp, slotNum);
    }
    pushStack(cp, (cp->state->onLeft) ? -1 : 1);
}


/*
 *  Generate code for a logical operator. Called by genBinaryOp
 *
 *  (expression OP expression)
 */
static void genLogicalOp(EcCompiler *cp, EcNode *np)
{
    EcState     *state;
    EcCodeGen   *saveCode;
    int         doneIfTrue, rightLen;

    ENTER(cp);

    state = cp->state;
    saveCode = state->code;

    mprAssert(np->kind == N_BINARY_OP);

    switch (np->tokenId) {
    case T_LOGICAL_AND:
        doneIfTrue = 0;
        break;

    case T_LOGICAL_OR:
        doneIfTrue = 1;
        break;

    default:
        doneIfTrue = 1;
        mprAssert(0);
        ecEncodeOpcode(cp, mapToken(cp, np->tokenId));
        break;
    }

    /*
     *  Process the conditional test. Put the pop for the branch here prior to the right hand side.
     */
    processNode(cp, np->left);
    ecEncodeOpcode(cp, EJS_OP_DUP);
    pushStack(cp, 1);
    popStack(cp, 1);

    if (np->right) {
        state->code = allocCodeBuffer(cp);
        np->binary.rightCode = state->code;
        /*
         *  Evaluating right hand side, so we must pop the left side duped value.
         */
        ecEncodeOpcode(cp, EJS_OP_POP);
        popStack(cp, 1);
        processNode(cp, np->right);
    }

    rightLen = mprGetBufLength(np->binary.rightCode->buf);

    /*
     *  Now copy the code to the output code buffer
     */
    setCodeBuffer(cp, saveCode);

    /*
     *  Jump to done if we know the result due to lazy evalation.
     */
    if (rightLen > 0 && rightLen < 0x7f && cp->optimizeLevel > 0) {
        ecEncodeOpcode(cp, (doneIfTrue) ? EJS_OP_BRANCH_TRUE_8: EJS_OP_BRANCH_FALSE_8);
        ecEncodeByte(cp, rightLen);
    } else {
        ecEncodeOpcode(cp, (doneIfTrue) ? EJS_OP_BRANCH_TRUE: EJS_OP_BRANCH_FALSE);
        ecEncodeWord(cp, rightLen);
    }

    copyCodeBuffer(cp, state->code, np->binary.rightCode);
    mprAssert(state == cp->state);
    LEAVE(cp);
}


/*
 *  Generate a property name reference based on the object already pushed.
 *  The owning object (pushed on the VM stack) may be an object or a type.
 */
static void genPropertyName(EcCompiler *cp, int slotNum)
{
    EcState     *state;
    int         code;

    mprAssert(slotNum >= 0);

    state = cp->state;

    if (slotNum < 10) {
        code = (!state->onLeft) ?  EJS_OP_GET_OBJ_SLOT_0 :  EJS_OP_PUT_OBJ_SLOT_0;
        ecEncodeOpcode(cp, code + slotNum);

    } else {
        code = (!state->onLeft) ?  EJS_OP_GET_OBJ_SLOT :  EJS_OP_PUT_OBJ_SLOT;
        ecEncodeOpcode(cp, code);
        ecEncodeNumber(cp, slotNum);
    }

    popStack(cp, 1);
    pushStack(cp, (state->onLeft) ? -1 : 1);
}


/*
 *  Generate a class property name reference
 *  The owning object (pushed on the VM stack) may be an object or a type. We must access its base class.
 */
static void genBaseClassPropertyName(EcCompiler *cp, int slotNum, int nthBase)
{
    EcState     *state;
    int         code;

    mprAssert(slotNum >= 0);

    state = cp->state;

    code = (!cp->state->onLeft) ?  EJS_OP_GET_TYPE_SLOT : EJS_OP_PUT_TYPE_SLOT;

    ecEncodeOpcode(cp, code);
    ecEncodeNumber(cp, slotNum);
    ecEncodeNumber(cp, nthBase);

    popStack(cp, 1);
    pushStack(cp, (state->onLeft) ? -1 : 1);
}


/*
 *  Generate a class property name reference
 *  The owning object (pushed on the VM stack) may be an object or a type. We must access its base class.
 */
static void genThisBaseClassPropertyName(EcCompiler *cp, EjsType *type, int slotNum)
{
    Ejs         *ejs;
    EcState     *state;
    int         code, nthBase;

    mprAssert(slotNum >= 0);
    mprAssert(type && ejsIsType(type));

    ejs = cp->ejs;
    state = cp->state;

    /*
     *  Count based up from object 
     */
    for (nthBase = 0; type->baseType; type = type->baseType) {
        nthBase++;
    }
    code = (!state->onLeft) ?  EJS_OP_GET_THIS_TYPE_SLOT :  EJS_OP_PUT_THIS_TYPE_SLOT;
    ecEncodeOpcode(cp, code);
    ecEncodeNumber(cp, slotNum);
    ecEncodeNumber(cp, nthBase);
    pushStack(cp, (state->onLeft) ? -1 : 1);
}


/*
 *  Generate a class name reference or a global reference.
 */
static void genClassName(EcCompiler *cp, EjsType *type)
{
    Ejs         *ejs;
    int         slotNum;

    mprAssert(type);

    ejs = cp->ejs;

    if (type == (EjsType*) ejs->global) {
        ecEncodeOpcode(cp, EJS_OP_LOAD_GLOBAL);
        pushStack(cp, 1);
        return;
    }

    if (cp->bind || type->block.obj.var.builtin) {
        slotNum = ejsLookupProperty(ejs, ejs->global, &type->qname);
        mprAssert(slotNum >= 0);
        genGlobalName(cp, slotNum);

    } else {
        ecEncodeOpcode(cp, EJS_OP_LOAD_GLOBAL);
        pushStack(cp, 1);
        ecEncodeOpcode(cp, EJS_OP_GET_OBJ_NAME);
        ecEncodeName(cp, &type->qname);
        popStack(cp, 1);
        pushStack(cp, 1);
    }
}


/*
 *  Generate a property reference in the current object
 */
static void genPropertyViaThis(EcCompiler *cp, int slotNum)
{
    Ejs             *ejs;
    EcState         *state;
    int             code;

    mprAssert(slotNum >= 0);

    ejs = cp->ejs;
    state = cp->state;

    /*
     *  Property in the current "this" object
     */
    if (slotNum < 10) {
        code = (!state->onLeft) ?  EJS_OP_GET_THIS_SLOT_0 :  EJS_OP_PUT_THIS_SLOT_0;
        ecEncodeOpcode(cp, code + slotNum);

    } else {
        code = (!state->onLeft) ?  EJS_OP_GET_THIS_SLOT :  EJS_OP_PUT_THIS_SLOT;
        ecEncodeOpcode(cp, code);
        ecEncodeNumber(cp, slotNum);
    }
    pushStack(cp, (cp->state->onLeft) ? -1 : 1);
}


/*
 *  Generate code for a bound name reference. We already know the slot for the property and its owning type.
 */
static void genBoundName(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EcState     *state;
    EjsLookup   *lookup;

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    lookup = &np->lookup;

    mprAssert(lookup->slotNum >= 0);

    if (lookup->obj == ejs->global) {
        /*
         *  Global variable.
         */
        if (lookup->slotNum < 0 || (!cp->bind && (lookup->trait == 0 || !(lookup->trait->attributes & EJS_ATTR_BUILTIN)))) {
            lookup->slotNum = -1;
            genUnboundName(cp, np);

        } else {
            genGlobalName(cp, lookup->slotNum);
        }

    } else if (ejsIsFunction(lookup->obj) && lookup->nthBlock == 0) {
        genLocalName(cp, lookup->slotNum);

    } else if ((ejsIsBlock(lookup->obj) || ejsIsFunction(lookup->obj)) && 
            (!ejsIsType(lookup->obj) && !ejsIsInstanceBlock(lookup->obj))) {
        genBlockName(cp, lookup->slotNum, lookup->nthBlock);

    } else if (lookup->useThis) {
        if (lookup->instanceProperty) {
            /*
             *  Property being accessed via the current object "this" or an explicit object?
             */
            genPropertyViaThis(cp, lookup->slotNum);

        } else {
            genThisBaseClassPropertyName(cp, (EjsType*) lookup->obj, lookup->slotNum);
        }

    } else if (!state->currentObjectNode) {
        if (lookup->instanceProperty) {
            genBlockName(cp, lookup->slotNum, lookup->nthBlock);

        } else {
            /*
             *  Static property with no explicit object. ie. Not "obj.property". The property was found via a scope search.
             *  We ignore nthBase as we use the actual type (lookup->obj) where the property was found.
             */
            if (state->inClass && state->inFunction && state->currentFunction->staticMethod) {
                genThisBaseClassPropertyName(cp, (EjsType*) lookup->obj, lookup->slotNum);
                
            } else {
                if (state->inFunction && ejsIsA(ejs, (EjsVar*) state->currentClass, (EjsType*) lookup->obj)) {
                    genThisBaseClassPropertyName(cp, (EjsType*) lookup->obj, lookup->slotNum);
                    
                } else {
                    SAVE_ONLEFT(cp);
                    genClassName(cp, (EjsType*) lookup->obj);
                    RESTORE_ONLEFT(cp);
                    genPropertyName(cp, lookup->slotNum);
                }
            }
        }

    } else {
        /*
         *  Explicity object. ie. "obj.property". The object in a dot expression is already pushed on the stack.
         *  Determine if we can access the object itself or if we need to use the type of the object to access
         *  static properties.
         */
        if (lookup->instanceProperty) {
            genPropertyName(cp, lookup->slotNum);

        } else {
            /*
             *  Property is in the nth base class from the object already pushed on the stack (left hand side).
             */
            genBaseClassPropertyName(cp, lookup->slotNum, lookup->nthBase);
        }
    }
    LEAVE(cp);
}


static void processNodeGetValue(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);
    cp->state->needsValue = 1;
    processNode(cp, np);
    LEAVE(cp);
}


static int genCallArgs(EcCompiler *cp, EcNode *np) 
{
    if (np == 0) {
        return 0;
    }
    processNode(cp, np);
    return mprGetListCount(np->children);
}


static void genCallSequence(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsType         *type;
    EcNode          *left, *right;
    EcState         *state;
    EjsFunction     *fun;
    EjsLookup       *lookup;
    int             fast, argc, staticMethod;    
        
    ejs = cp->ejs;
    state = cp->state;
    left = np->left;
    right = np->right;
    lookup = &np->left->lookup;
    argc = 0;
    
    if (lookup->slotNum < 0) {
        /*
         *  Unbound or Function expression or instance variable containing a function. Can't use fast path op codes below.
         */
        if (left->kind == N_QNAME) {
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_SCOPED_NAME);
            ecEncodeName(cp, &np->qname);
            
        } else if (left->kind == N_DOT && left->right->kind == N_QNAME) {
            processNodeGetValue(cp, left->left);
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_OBJ_NAME);
            ecEncodeName(cp, &np->qname);
            popStack(cp, 1);
            
        } else {
#if POSSIBLE_BUG || 1
            ecEncodeOpcode(cp, EJS_OP_LOAD_GLOBAL);
#else
            //  See master branch for fix
            processNodeGetValue(cp, left);
            ecEncodeOpcode(cp, EJS_OP_DUP);
#endif
            pushStack(cp, 1);
            processNodeGetValue(cp, left);
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL);
            popStack(cp, 2);
        }
        ecEncodeNumber(cp, argc); 
        popStack(cp, argc);
        return;
    }
        
    fun = (EjsFunction*) lookup->ref;
    staticMethod = (ejsIsFunction(fun) && fun->staticMethod);
        
    /*
     *  Use fast opcodes when the call sequence is bindable and either:
     *      expression.name()
     *      name
     */
    fast = (left->kind == N_DOT && left->right->kind == N_QNAME) || left->kind == N_QNAME;      
        
    if (!fast) {
        /*
         *  Resolve a reference to a function expression
         */
        if (left->kind == N_EXPRESSIONS) {
            if (left->right == 0) {
                left->left->needThis = 1;
            } else {
                left->right->needThis = 1;
            }
        } else {
            left->needThis = 1;
        }
        processNodeGetValue(cp, left);
        argc = genCallArgs(cp, right);
        ecEncodeOpcode(cp, EJS_OP_CALL);
        popStack(cp, 2);
        ecEncodeNumber(cp, argc); 
        popStack(cp, argc);
        return;
    }
    
    if (staticMethod) {
        mprAssert(ejsIsType(lookup->obj));
        if (state->currentClass && state->inFunction && 
                ejsIsA(ejs, (EjsVar*) state->currentClass, (EjsType*) lookup->originalObj)) {
            /*
             *  Calling a static method from within a class or subclass. So we can use "this".
             */
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_THIS_STATIC_SLOT);
            ecEncodeNumber(cp, lookup->slotNum);
            /*
             *  If searching the scope chain (i.e. without a qualifying obj.property), and if the current class is not the 
             *  original object, then see how far back on the inheritance chain we must go.
             */
            if (lookup->originalObj != lookup->obj) {
                for (type = state->currentClass; type != (EjsType*) lookup->originalObj; type = type->baseType) {
                    lookup->nthBase++;
                }
            }
            if (!state->currentFunction->staticMethod) {
                /*
                 *  If calling from within an instance function, need to step over the instance also
                 */
                lookup->nthBase++;
            }
            ecEncodeNumber(cp, lookup->nthBase);
            
        } else if (left->kind == N_DOT && left->right->kind == N_QNAME) {
            /*
             *  Calling a static method with an explicit object or expression. Call via the object.
             */
            processNode(cp, left->left);
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_OBJ_STATIC_SLOT);
            ecEncodeNumber(cp, lookup->slotNum);
            if (lookup->ownerIsType) {
                lookup->nthBase--;
            }
            ecEncodeNumber(cp, lookup->nthBase);
            popStack(cp, 1);
            
        } else {
            /*
             *  Foreign static method. Call directly on the correct class type object.
             */
            genClassName(cp, (EjsType*) lookup->obj);
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_OBJ_STATIC_SLOT);
            ecEncodeNumber(cp, lookup->slotNum);
            ecEncodeNumber(cp, 0);
            popStack(cp, 1);
        }
        
    } else {
        // pushStack(cp, 1);
        if (left->kind == N_DOT && left->right->kind == N_QNAME) {
            if (left->left->kind == N_THIS) {
                lookup->useThis = 1;
            }
        }
        
        if (lookup->useThis && !lookup->instanceProperty) {
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_THIS_SLOT);
            ecEncodeNumber(cp, lookup->slotNum);
            
        } else if (lookup->obj == ejs->global) {
            /*
             *  Instance function or type being invoked as a constructor (e.g. Reflect(obj))
             */
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_GLOBAL_SLOT);
            ecEncodeNumber(cp, lookup->slotNum);
            
        } else if (lookup->instanceProperty && left->left) {
            processNodeGetValue(cp, left->left);
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_OBJ_INSTANCE_SLOT);
            ecEncodeNumber(cp, lookup->slotNum);
            popStack(cp, 1);
            
        } else if (ejsIsType(lookup->obj) || ejsIsInstanceBlock(lookup->obj)) {
            if (left->kind == N_DOT && left->right->kind == N_QNAME) {
                processNodeGetValue(cp, left->left);
                argc = genCallArgs(cp, right);
                ecEncodeOpcode(cp, EJS_OP_CALL_OBJ_SLOT);
                ecEncodeNumber(cp, lookup->slotNum);
                popStack(cp, 1);
                
            } else {
                left->needThis = 1;
                processNodeGetValue(cp, left);
                argc = genCallArgs(cp, right);
                ecEncodeOpcode(cp, EJS_OP_CALL);
                popStack(cp, 2);
            }
            
        } else if (ejsIsBlock(lookup->obj)) {
            argc = genCallArgs(cp, right);
            ecEncodeOpcode(cp, EJS_OP_CALL_BLOCK_SLOT);
            ecEncodeNumber(cp, lookup->slotNum);
            ecEncodeNumber(cp, lookup->nthBlock);
        }
    }
    ecEncodeNumber(cp, argc); 
    popStack(cp, argc);
}


/*
 *  Code generation for function calls
 */
static void genCall(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EcNode          *left, *right;
    EcState         *state;
    EjsFunction     *fun;
    int             argc, hasResult;

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    left = np->left;
    right = np->right;
    fun = (EjsFunction*) np->lookup.ref;    
    
    if (left->kind == N_NEW && !left->newExpr.callConstructors) {
        processNode(cp, left);
        LEAVE(cp);
        return;
    }

    if (left->kind == N_NEW) {
        processNode(cp, left);
        argc = genCallArgs(cp, right);
        ecEncodeOpcode(cp, EJS_OP_CALL_CONSTRUCTOR);
        ecEncodeNumber(cp, argc);
        popStack(cp, argc);
        LEAVE(cp);
        return;
    }
    
    genCallSequence(cp, np);

    /*
     *  Process the function return value. Call by ref has a this pointer plus method reference plus args
     */
    hasResult = 0;
    if (fun && ejsIsFunction(fun)) {
        if (fun->resultType && fun->resultType != ejs->voidType) {
            hasResult = 1;

        } else if (fun->hasReturn) {
            /*
             *  Untyped function, but it has a return stmt.
             *  We don't do data flow to make sure all return cases have returns (sorry).
             */
            hasResult = 1;
        }
        if (state->needsValue && !hasResult) {
            genError(cp, np, "Function call does not return a value.");
        }
    }

    /*
     *  If calling a type as a constructor (Date()), must push result
     */
    if (state->needsValue || ejsIsType(np->lookup.ref)) {
        ecEncodeOpcode(cp, EJS_OP_PUSH_RESULT);
        pushStack(cp, 1);
    }
    LEAVE(cp);
}


static void genCatchArg(EcCompiler *cp, EcNode *np)
{
    ecEncodeOpcode(cp, EJS_OP_PUSH_CATCH_ARG);
    pushStack(cp, 1);
}


/*
 *  Process a class node.
 */
static void genClass(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsType         *type, *baseType;
    EjsFunction     *constructor;
    EjsEx           *ex;
    EcCodeGen       *code;
    EcState         *state;
    EcNode          *constructorNode;
    EjsName         qname;
    MprBuf          *codeBuf;
    uchar           *buf;
    int             next, len, initializerLen, constructorLen, slotNum, i;

    ENTER(cp);

    mprAssert(np->kind == N_CLASS);

    ejs = cp->ejs;
    state = cp->state;
    type = (EjsType*) np->klass.ref;
    mprAssert(type);

    state->inClass = 1;
    state->inFunction = 0;

    /*
     *  Op code to define the class. This goes into the module code buffer. DefineClass will capture the current scope
     *  including the internal namespace for this file.
     *  OPT See above todo
     */
    ecEncodeOpcode(cp, EJS_OP_DEFINE_CLASS);
    ecEncodeGlobal(cp, (EjsVar*) type, &type->qname);

    state->letBlock = (EjsVar*) type;
    state->varBlock = (EjsVar*) type;
    state->currentClass = type;
    state->currentClassNode = np;

    constructorNode = np->klass.constructor;

    /*
     *  Create code buffers to hold the static and instance level initialization code. The AST module will always
     *  create a constructor node for us if there is instance level initialization code. We currently put the class
     *  initialization code in the constructor. Static variable initialization code will go into the current
     *  module buffer (cp->currentModule) and will be run when the module is loaded. 
     *  BUG - CLASS INITIALIZATION ORDERING.
     */
    state->code = state->currentModule->code;
 
    /*
     *  Create a code buffer for static initialization code and set it as the default buffer
     */
    state->code = state->staticCodeBuf = allocCodeBuffer(cp);

    if (type->hasConstructor) {
        state->instanceCodeBuf = allocCodeBuffer(cp);
    }

    /*
     *  The current code buffer is the static initializer buffer. genVar will redirect to the instanceCodeBuf as required.
     */
    if (!type->isInterface) {
        mprAssert(np->left->kind == N_DIRECTIVES);
        processNode(cp, np->left);
    }

    if (type->hasStaticInitializer) {
        /*
         *  Create the static initializer
         */
        ecEncodeOpcode(cp, EJS_OP_RETURN);
        setFunctionCode(cp, np->klass.initializer, state->staticCodeBuf);
    }

    if (type->hasConstructor) {
        mprAssert(constructorNode);
        mprAssert(state->instanceCodeBuf);
        code = state->code = state->instanceCodeBuf;
        codeBuf = code->buf;

        constructor = state->currentFunction = constructorNode->function.functionVar;
        mprAssert(constructor);
        state->currentFunctionName = constructorNode->qname.name;

        if (constructorNode->function.isDefaultConstructor) {
            /*
             *  Generate the default constructor. Append the default constructor instructions after any initialization code.
             */
            baseType = type->baseType;
            if (baseType && baseType->hasConstructor) {
                ecEncodeOpcode(cp, EJS_OP_CALL_NEXT_CONSTRUCTOR);
                ecEncodeNumber(cp, 0);
            }
            ecEncodeOpcode(cp, EJS_OP_RETURN);
            setFunctionCode(cp, constructor, code);
            ecAddConstant(cp, EJS_PUBLIC_NAMESPACE);
            ecAddConstant(cp, EJS_CONSTRUCTOR_NAMESPACE);

        } else if (type->hasInitializer) {

            /*
             *  Inject initializer code into the pre-existing constructor code. It is injected before any constructor code.
             */
            initializerLen = mprGetBufLength(codeBuf);
            mprAssert(initializerLen >= 0);
            constructorLen = constructor->body.code.codeLen;
            mprAssert(constructorLen >= 0);

            len = initializerLen + constructorLen;
            if (len > 0) {
                buf = (uchar*) mprAlloc(state, len);
                if (buf == 0) {
                    genError(cp, np, "Can't allocate code buffer");
                    LEAVE(cp);
                }

                mprMemcpy((char*) buf, initializerLen, mprGetBufStart(codeBuf), initializerLen);
                if (constructorLen) {
                    mprMemcpy((char*) &buf[initializerLen], constructorLen, (char*) constructor->body.code.byteCode, 
                        constructorLen);
                }
                ejsSetFunctionCode(constructor, buf, len);

                /*
                 *  Adjust existing exception blocks to accomodate injected code.
                 *  Then define new try/catch blocks encountered.
                 */
                for (i = 0; i < constructor->body.code.numHandlers; i++) {
                    ex = constructor->body.code.handlers[i];
                    ex->tryStart += initializerLen;
                    ex->tryEnd += initializerLen;
                    ex->handlerStart += initializerLen;
                    ex->handlerEnd += initializerLen;
                }
                for (next = 0; (ex = (EjsEx*) mprGetNextItem(code->exceptions, &next)) != 0; ) {
                    ejsAddException(constructor, ex->tryStart, ex->tryEnd, ex->catchType, ex->handlerStart, ex->handlerEnd, 
                        ex->numBlocks, ex->numStack, ex->flags, -1);
                }
            }
        }
    }

    /*
     *  Add extra constants
     */
    ecAddNameConstant(cp, &np->qname);

    if (type->hasStaticInitializer) {
        slotNum = type->block.numInherited;
        if (type->hasConstructor) {
            slotNum++;
        }
        qname = ejsGetPropertyName(ejs, (EjsVar*) type, slotNum);
        ecAddNameConstant(cp, &qname);
    }
    if (type->baseType) {
        ecAddNameConstant(cp, &type->baseType->qname);
    }

    /*
     *  Emit any properties implemented via another class (there is no Node for these)
     */
    ecAddBlockConstants(cp, (EjsBlock*) type);
    if (type->instanceBlock) {
        ecAddBlockConstants(cp, (EjsBlock*) type->instanceBlock);
    }

#if BLD_FEATURE_EJS_DOC
    if (cp->ejs->flags & EJS_FLAG_DOC) {
        ecAddDocConstant(cp, np->lookup.trait, ejs->global, np->lookup.slotNum);
    }
#endif
    LEAVE(cp);
}


static void genDirectives(EcCompiler *cp, EcNode *np, bool saveResult)
{
    EcState     *lastDirectiveState;
    EcNode      *child;
    int         next, lastKind, mark;

    ENTER(cp);

    lastDirectiveState = cp->directiveState;
    lastKind = -1;
    next = 0;
    mark = getStackCount(cp);
    while ((child = getNextNode(cp, np, &next)) && !cp->error) {
        lastKind = child->kind;
        cp->directiveState = cp->state;
        processNode(cp, child);
        if (!saveResult) {
            discardStackItems(cp, mark);
        }
    }
    if (saveResult) {
        ecEncodeOpcode(cp, EJS_OP_SAVE_RESULT);
    }
    cp->directiveState = lastDirectiveState;
    LEAVE(cp);
}


/*
 *  Handle property dereferencing via "." and "[". This routine generates code for bound properties where we know
 *  the slot offsets and also for unbound references. Return the right most node in right.
 */
static void genDot(EcCompiler *cp, EcNode *np, EcNode **rightMost)
{
    EcState     *state;
    EcNode      *left, *right;
    int         put;

    ENTER(cp);

    state = cp->state;
    state->onLeft = 0;
    left = np->left;
    right = np->right;

    /*
     *  Process the left of the dot and leave an object reference on the stack
     */
    switch (left->kind) {
    case N_DOT:
    case N_EXPRESSIONS:
    case N_LITERAL:
    case N_THIS:
    case N_REF:
    case N_QNAME:
    case N_CALL:
    case N_SUPER:
    case N_OBJECT_LITERAL:
        state->needsValue = 1;
        processNode(cp, left);
        state->needsValue = state->prev->needsValue;
        break;

    case N_ARRAY_LITERAL:
        processNode(cp, left);
        break;

    default:
        mprAssert(0);
    }

    state->currentObjectNode = np->left;

    if (np->needThis) {
        ecEncodeOpcode(cp, EJS_OP_DUP);
        pushStack(cp, 1);
        np->needThis = 0;
    }

    put = state->prev->onLeft;

    /*
     *  Process the right
     */
    switch (right->kind) {
    case N_CALL:
        state->needsValue = state->prev->needsValue;
        genCall(cp, right);
        state->needsValue = 0;
        break;

    case N_QNAME:
        state->onLeft = state->prev->onLeft;
        genName(cp, right);
        break;

    case N_SUPER:
        ecEncodeOpcode(cp, EJS_OP_SUPER);
        break;

    case N_LITERAL:
    case N_OBJECT_LITERAL:
    default:
        state->currentObjectNode = 0;
        state->needsValue = 1;
        ecEncodeOpcode(cp, EJS_OP_LOAD_STRING);
        ecEncodeString(cp, EJS_EMPTY_NAMESPACE);
        pushStack(cp, 1);
        if (right->kind == N_LITERAL) {
            genLiteral(cp, right);
        } else if (right->kind == N_OBJECT_LITERAL) {
            genObjectLiteral(cp, right);
        } else {
            processNode(cp, right);
        }
        state->onLeft = state->prev->onLeft;
        ecEncodeOpcode(cp, put ? EJS_OP_PUT_OBJ_NAME_EXPR :  EJS_OP_GET_OBJ_NAME_EXPR);
        popStack(cp, (put) ? 4 : 2);
        break;
    }

    if (rightMost) {
        *rightMost = right;
    }
    LEAVE(cp);
}


static void genEndFunction(EcCompiler *cp, EcNode *np)
{
    EjsFunction     *fun;

    ENTER(cp);

    mprAssert(np);

    fun = cp->state->currentFunction;
    
    if (cp->lastOpcode != EJS_OP_RETURN_VALUE && cp->lastOpcode != EJS_OP_RETURN) {
        /*
         *  Ensure code cannot run off the end of a method.
         */
        if (fun->resultType == 0) {
            if (fun->hasReturn) {
                ecEncodeOpcode(cp, EJS_OP_LOAD_NULL);
                ecEncodeOpcode(cp, EJS_OP_RETURN_VALUE);
            } else {
                ecEncodeOpcode(cp, EJS_OP_RETURN);
            }

        } else if (fun->resultType == cp->ejs->voidType) {
            ecEncodeOpcode(cp, EJS_OP_RETURN);

        } else {
            ecEncodeOpcode(cp, EJS_OP_LOAD_NULL);
            ecEncodeOpcode(cp, EJS_OP_RETURN_VALUE);
        }

        addDebugInstructions(cp, np);
    }
    LEAVE(cp);
}


static void genExpressions(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    int         next;

    mprAssert(np->kind == N_EXPRESSIONS);

    ENTER(cp);

    next = 0;
    while ((child = getNextNode(cp, np, &next)) != 0) {
        processNode(cp, child);
    }
    LEAVE(cp);
}


/*
 *  This handles "do { ... } while" constructs.
 *
 *  do {
 *       body
 *  } while (conditional)
 *
 *  Labels:
 *      topOfLoop:
 *          body
 *      continueLabel:
 *          conditional
 *          bxx topOfLoop
 *      endLoop:
 */
static void genDo(EcCompiler *cp, EcNode *np)
{
    EcCodeGen   *outerBlock, *code;
    EcState     *state;
    int         condLen, bodyLen, len, condShortJump, continueLabel, breakLabel, mark;

    ENTER(cp);

    state = cp->state;
    state->captureBreak = 0;

    mprAssert(np->kind == N_DO);

    outerBlock = state->code;
    code = state->code = allocCodeBuffer(cp);

    ecStartBreakableStatement(cp, EC_JUMP_BREAK | EC_JUMP_CONTINUE);

    if (np->forLoop.body) {
        np->forLoop.bodyCode = state->code = allocCodeBuffer(cp);
        mark = getStackCount(cp);
        processNode(cp, np->forLoop.body);
        discardStackItems(cp, mark);
    }
    if (np->forLoop.cond) {
        np->forLoop.condCode = state->code = allocCodeBuffer(cp);
        processNode(cp, np->forLoop.cond);
    }

    /*
     *  Get the lengths of code blocks
     */
    condLen = bodyLen = 0;

    if (np->forLoop.condCode) {
        condLen = mprGetBufLength(np->forLoop.condCode->buf);
    }
    if (np->forLoop.bodyCode) {
        bodyLen = mprGetBufLength(np->forLoop.bodyCode->buf);
    }

    /*
     *  Now that we know the body length, we can calculate the jump back to the top.
     */
    condShortJump = 0;
    len = bodyLen + condLen;
    if (len > 0) {
        if (len < 0x7f && cp->optimizeLevel > 0) {
            condShortJump = 1;
            condLen += 2;
        } else {
            condLen += 5;
        }
    }

    setCodeBuffer(cp, code);
    if (np->forLoop.cond) {
        pushStack(cp, 1);
    }
    continueLabel = mprGetBufLength(cp->state->code->buf);

    /*
     *  Add the body
     */
    if (np->forLoop.bodyCode) {
        copyCodeBuffer(cp, state->code, np->forLoop.bodyCode);
    }


    /*
     *  Copy the conditional code and add condition jump to the end of the for loop, then copy the body code.
     */
    if (np->forLoop.condCode) {
        copyCodeBuffer(cp, state->code, np->forLoop.condCode);
        len = bodyLen + condLen;
        if (condShortJump) {
            ecEncodeOpcode(cp, EJS_OP_BRANCH_TRUE_8);
            ecEncodeByte(cp, -len);
        } else {
            ecEncodeOpcode(cp, EJS_OP_BRANCH_TRUE);
            ecEncodeWord(cp, -len);
        }
        popStack(cp, 1);
    }

    breakLabel = mprGetBufLength(cp->state->code->buf);
    patchJumps(cp, EC_JUMP_BREAK, breakLabel);
    patchJumps(cp, EC_JUMP_CONTINUE, continueLabel);

    copyCodeBuffer(cp, outerBlock, state->code);
    LEAVE(cp);
}


/*
 *  This handles "for" and while" constructs but not "for .. in"
 *
 *  for (initializer; conditional; perLoop) { body }
 *
 *  Labels:
 *          initializer
 *      topOfLoop:
 *          conditional
 *          bxx endLoop
 *      topOfBody:
 *          body
 *      continueLabel:
 *          perLoop
 *      endIteration:
 *          goto topOfLoop
 *      endLoop:
 */
static void genFor(EcCompiler *cp, EcNode *np)
{
    EcCodeGen   *outerBlock, *code;
    EcState     *state;
    int         condLen, bodyLen, perLoopLen, len, condShortJump, perLoopShortJump, continueLabel, breakLabel, mark;
    int         startMark;

    ENTER(cp);

    mprAssert(np->kind == N_FOR);

    state = cp->state;
    outerBlock = state->code;
    code = state->code = allocCodeBuffer(cp);
    startMark = getStackCount(cp);
    state->captureBreak = 0;

    /*
     *  initializer is outside the loop
     */
    if (np->forLoop.initializer) {
        mark = getStackCount(cp);
        processNode(cp, np->forLoop.initializer);
        discardStackItems(cp, mark);
    }

    /*
     *  For conditional
     */
    ecStartBreakableStatement(cp, EC_JUMP_BREAK | EC_JUMP_CONTINUE);

    if (np->forLoop.cond) {
        np->forLoop.condCode = state->code = allocCodeBuffer(cp);
        state->needsValue = 1;
        processNode(cp, np->forLoop.cond);
        state->needsValue = 0;
        /* Leaves one item on the stack. But this will be cleared when compared */
        mprAssert(state->code->stackCount >= 1);
        popStack(cp, 1);
    }

    if (np->forLoop.body) {
        mark = getStackCount(cp);
        np->forLoop.bodyCode = state->code = allocCodeBuffer(cp);
        processNode(cp, np->forLoop.body);
        discardStackItems(cp, mark);
    }

    /*
     *  Per loop iteration
     */
    if (np->forLoop.perLoop) {
        np->forLoop.perLoopCode = state->code = allocCodeBuffer(cp);
        mark = getStackCount(cp);
        processNode(cp, np->forLoop.perLoop);
        discardStackItems(cp, mark);
    }

    /*
     *  Get the lengths of code blocks
     */
    perLoopLen = condLen = bodyLen = 0;

    if (np->forLoop.condCode) {
        condLen = mprGetBufLength(np->forLoop.condCode->buf);
    }
    if (np->forLoop.bodyCode) {
        bodyLen = mprGetBufLength(np->forLoop.bodyCode->buf);
    }
    if (np->forLoop.perLoopCode) {
        perLoopLen = mprGetBufLength(np->forLoop.perLoopCode->buf);
    }

    /*
     *  Now that we know the body length, we can calculate the jump at the top. This is the shorter of
     *  the two jumps as it does not span the conditional code, so we optimize it first incase the saving
     *  of 3 bytes allows us to also optimize the branch back to the top. Subtract 5 to the test with 0x7f to
     *  account for the worst-case jump at the bottom back to the top
     */
    condShortJump = 0;
    if (condLen > 0) {
        len = bodyLen + perLoopLen;
        if (len < (0x7f - 5) && cp->optimizeLevel > 0) {
            condShortJump = 1;
            condLen += 2;
        } else {
            condLen += 5;
        }
    }

    /*
     *  Calculate the jump back to the top of the loop (per-iteration jump). Subtract 5 to account for the worst case
     *  where the per loop jump is a long jump.
     */
    len = condLen + bodyLen + perLoopLen;
    if (len < (0x7f - 5) && cp->optimizeLevel > 0) {
        perLoopShortJump = 1;
        perLoopLen += 2;
    } else {
        perLoopShortJump = 0;
        perLoopLen += 5;
    }

    /*
     *  Copy the conditional code and add condition jump to the end of the for loop, then copy the body code.
     */
    setCodeBuffer(cp, code);
    if (np->forLoop.condCode) {
        copyCodeBuffer(cp, state->code, np->forLoop.condCode);
        len = bodyLen + perLoopLen;
        if (condShortJump) {
            ecEncodeOpcode(cp, EJS_OP_BRANCH_FALSE_8);
            ecEncodeByte(cp, len);
        } else {
            ecEncodeOpcode(cp, EJS_OP_BRANCH_FALSE);
            ecEncodeWord(cp, len);
        }
    }

    /*
     *  Add the body and per loop code
     */
    if (np->forLoop.bodyCode) {
        copyCodeBuffer(cp, state->code, np->forLoop.bodyCode);
    }
    continueLabel = mprGetBufLength(state->code->buf);
    if (np->forLoop.perLoopCode) {
        copyCodeBuffer(cp, state->code, np->forLoop.perLoopCode);
    }

    /*
     *  Add the per-loop jump back to the top of the loop
     */
    len = condLen + bodyLen + perLoopLen;
    if (perLoopShortJump) {
        ecEncodeOpcode(cp, EJS_OP_GOTO_8);
        ecEncodeByte(cp, -len);
    } else {
        ecEncodeOpcode(cp, EJS_OP_GOTO);
        ecEncodeWord(cp, -len);
    }
    breakLabel = mprGetBufLength(state->code->buf);
    discardStackItems(cp, startMark);

    patchJumps(cp, EC_JUMP_BREAK, breakLabel);
    patchJumps(cp, EC_JUMP_CONTINUE, continueLabel);

    copyCodeBuffer(cp, outerBlock, state->code);
    LEAVE(cp);
}


/*
 *  This routine is a little atypical in that it hand-crafts an exception block.
 */
static void genForIn(EcCompiler *cp, EcNode *np)
{
    EcCodeGen   *outerBlock, *code;
    EcState     *state;
    int         len, breakLabel, tryStart, tryEnd, handlerStart, mark, startMark;

    ENTER(cp);

    mprAssert(cp->state->code->stackCount >= 0);
    mprAssert(np->kind == N_FOR_IN);

    state = cp->state;
    outerBlock = state->code;
    code = state->code = allocCodeBuffer(cp);
    startMark = getStackCount(cp);
    state->captureBreak = 0;

    ecStartBreakableStatement(cp, EC_JUMP_BREAK | EC_JUMP_CONTINUE);

    processNode(cp, np->forInLoop.iterVar);

    /*
     *  Consider:
     *      for (i in obj.get())
     *          body
     *
     *  Now process the obj.get()
     */
    np->forInLoop.initCode = state->code = allocCodeBuffer(cp);

    processNode(cp, np->forInLoop.iterGet);
    ecEncodeOpcode(cp, EJS_OP_PUSH_RESULT);
    pushStack(cp, 1);
    
    mprAssert(cp->state->code->stackCount >= 1);

    /*
     *  Process the iter.next()
     */
    np->forInLoop.bodyCode = state->code = allocCodeBuffer(cp);

    /*
     *  Dup the iterator reference each time round the loop as iter.next() will consume the object.
     */
    ecEncodeOpcode(cp, EJS_OP_DUP);
    pushStack(cp, 1);

    /*
     *  Emit code to invoke the iterator
     */
    tryStart = getCodeLength(cp, np->forInLoop.bodyCode);

    ecEncodeOpcode(cp, EJS_OP_CALL_OBJ_SLOT);
    ecEncodeNumber(cp, np->forInLoop.iterNext->lookup.slotNum);
    ecEncodeNumber(cp, 0);
    popStack(cp, 1);
    
    tryEnd = getCodeLength(cp, np->forInLoop.bodyCode);

    /*
     *  Save the result of the iter.next() call
     */
    ecEncodeOpcode(cp, EJS_OP_PUSH_RESULT);
    pushStack(cp, 1);
    genLeftHandSide(cp, np->forInLoop.iterVar->left);

    if (np->forInLoop.iterVar->kind == N_VAR_DEFINITION && np->forInLoop.iterVar->def.varKind == KIND_LET) {
        ecAddConstant(cp, np->forInLoop.iterVar->left->qname.name);
        ecAddConstant(cp, np->forInLoop.iterVar->left->qname.space);
    }

    /*
     *  Now the loop body. Must hide the pushed iterator on the stack as genDirectives will clear the stack.
     */
    mark = getStackCount(cp);
    if (np->forInLoop.body) {
        processNode(cp, np->forInLoop.body);
        discardStackItems(cp, mark);
    }

    len = getCodeLength(cp, np->forInLoop.bodyCode);
    if (len < (0x7f - 5)) {
        ecEncodeOpcode(cp, EJS_OP_GOTO_8);
        len += 2;
        ecEncodeByte(cp, -len);
    } else {
        ecEncodeOpcode(cp, EJS_OP_GOTO);
        len += 5;
        ecEncodeWord(cp, -len);
    }

    /*
     *  Create exception catch block around iter.next() to catch the StopIteration exception.
     *  Note: we have a zero length handler (noop)
     */
    handlerStart = ecGetCodeOffset(cp);
    addException(cp, tryStart, tryEnd, cp->ejs->stopIterationType, handlerStart, handlerStart, 0, startMark,
        EJS_EX_CATCH | EJS_EX_ITERATION);

    /*
     *  Patch break/continue statements.
     */
    discardStackItems(cp, startMark);
    breakLabel = mprGetBufLength(state->code->buf);

    patchJumps(cp, EC_JUMP_BREAK, breakLabel);
    patchJumps(cp, EC_JUMP_CONTINUE, 0);

    /*
     *  Copy the code fragments to the outer code buffer
     */
    setCodeBuffer(cp, code);
    copyCodeBuffer(cp, state->code, np->forInLoop.initCode);
    copyCodeBuffer(cp, state->code, np->forInLoop.bodyCode);

    copyCodeBuffer(cp, outerBlock, state->code);
    LEAVE(cp);
}


/*
 *  Generate code for default parameters. Native classes must handle this themselves. We
 *  generate the code for all default parameters in sequence with a computed goto at the front.
 */
static void genDefaultParameterCode(EcCompiler *cp, EcNode *np, EjsFunction *fun)
{
    EcNode          *parameters, *child;
    EcState         *state;
    EcCodeGen       **buffers, *saveCode;
    int             next, len, needLongJump, count, firstDefault;

    state = cp->state;
    saveCode = state->code;

    parameters = np->function.parameters;
    mprAssert(parameters);

    count = mprGetListCount(parameters->children);
    buffers = (EcCodeGen**) mprAllocZeroed(state, count * sizeof(EcCodeGen*));

    for (next = 0; (child = getNextNode(cp, parameters, &next)) && !cp->error; ) {
        mprAssert(child->kind == N_VAR_DEFINITION);

        if (child->left->kind == N_ASSIGN_OP) {
            buffers[next - 1] = state->code = allocCodeBuffer(cp);
            genAssignOp(cp, child->left);
        }
    }

    firstDefault = fun->numArgs - fun->numDefault;
    mprAssert(firstDefault >= 0);
    needLongJump = cp->optimizeLevel > 0 ? 0 : 1;

    /*
     *  Compute the worst case jump size. Start with 4 because the table is always one larger than the
     *  number of default args.
     */
    len = 4;
    for (next = firstDefault; next < count; next++) {
        if (buffers[next]) {
            len = mprGetBufLength(buffers[next]->buf) + 4;
            if (len >= 0x7f) {
                needLongJump = 1;
                break;
            }
        }
    }

    setCodeBuffer(cp, saveCode);

    /*
     *  This is a jump table where each parameter initialization segments falls through to the next one.
     *  We have one more entry in the table to jump over the entire computed jump section.
     */
    ecEncodeOpcode(cp, (needLongJump) ? EJS_OP_INIT_DEFAULT_ARGS: EJS_OP_INIT_DEFAULT_ARGS_8);
    ecEncodeByte(cp, fun->numDefault + 1);

    len = (fun->numDefault + 1) * ((needLongJump) ? 4 : 1);

    for (next = firstDefault; next < count; next++) {
        if (buffers[next] == 0) {
            continue;
        }
        if (needLongJump) {
            ecEncodeWord(cp, len);
        } else {
            ecEncodeByte(cp, len);
        }
        len += mprGetBufLength(buffers[next]->buf);
    }
    /*
     *  Add one more jump to jump over the entire jump table
     */
    if (needLongJump) {
        ecEncodeWord(cp, len);
    } else {
        ecEncodeByte(cp, len);
    }

    /*
     *  Now copy all the initialization code
     */
    for (next = firstDefault; next < count; next++) {
        if (buffers[next]) {
            copyCodeBuffer(cp, state->code, buffers[next]);
        }
    }
    mprFree(buffers);
}


static void genFunction(EcCompiler *cp, EcNode *np)
{
    Ejs             *ejs;
    EjsEx           *ex;
    EcState         *state;
    EcCodeGen       *code;
    EjsFunction     *fun;
    EjsType         *baseType;
    EjsName         qname;
    EjsTrait        *trait;
    EjsLookup       *lookup;
    int             i;

    ENTER(cp);

    mprAssert(np->kind == N_FUNCTION);
    
    ejs = cp->ejs;
    state = cp->state;
    mprAssert(state);

    mprAssert(np->function.functionVar);
    fun = np->function.functionVar;

    state->inFunction = 1;
    state->inMethod = state->inMethod || np->function.isMethod;
    state->blockIsMethod = np->function.isMethod;
    state->currentFunctionName = np->qname.name;
    state->currentFunction = fun;

    /*
     *  Capture the scope chain by the defineFunction op code. Emit this into the existing code buffer. 
     *  Don't do if a method as they get scope via other means.. Native methods also don't use this as an optimization.
     *  Native methods must handle scope explicitly.
     *
     *  We only need to define the function if it needs full scope (unbound property access) or it is a nested function.
     */
    if (fun->fullScope) {
        lookup = &np->lookup;
        mprAssert(lookup->slotNum >= 0);
        ecEncodeOpcode(cp, EJS_OP_DEFINE_FUNCTION);
        ecEncodeNumber(cp, lookup->slotNum);
        ecEncodeNumber(cp, lookup->nthBlock);
    }

    code = state->code = allocCodeBuffer(cp);
    addDebugInstructions(cp, np);

    /*
     *  Generate code for any parameter default initialization.
     *  Native classes must do default parameter initialization themselves.
     */
    if (fun->numDefault > 0 && !(np->attributes & EJS_ATTR_NATIVE)) {
        genDefaultParameterCode(cp, np, fun);
    }

    if (np->function.constructorSettings) {
        genDirectives(cp, np->function.constructorSettings, 0);
    }

    state->letBlock = (EjsVar*) fun;
    state->varBlock = (EjsVar*) fun;

    if (np->function.isConstructor) {
        /*
         *  Function is a constructor. Call any default constructors if required.
         *  Should this be before or after default variable initialization?
         */
        mprAssert(state->currentClass);
        baseType = state->currentClass->baseType;
        if (!state->currentClass->callsSuper && baseType && baseType->hasConstructor && !(np->attributes & EJS_ATTR_NATIVE)) {
            ecEncodeOpcode(cp, EJS_OP_CALL_NEXT_CONSTRUCTOR);
            ecEncodeNumber(cp, 0);
        }
    }

    /*
     *  May be no body for native functions
     */
    if (np->function.body) {
        mprAssert(np->function.body->kind == N_DIRECTIVES);
        processNode(cp, np->function.body);
    }

    if (cp->errorCount > 0) {
        LEAVE(cp);
        return;
    }

    setFunctionCode(cp, fun, code);

    /*
     *  Add string constants
     */
    ecAddNameConstant(cp, &np->qname);

    for (i = 0; i < fun->block.obj.numProp; i++) {
        qname = ejsGetPropertyName(ejs, (EjsVar*) fun, i);
        ecAddNameConstant(cp, &qname);
        trait = ejsGetPropertyTrait(ejs, (EjsVar*) fun, i);
        if (trait && trait->type) {
            ecAddNameConstant(cp, &trait->type->qname);
        }
    }
    if (fun->resultType) {
        ecAddNameConstant(cp, &fun->resultType->qname);
    }
    for (i = 0; i < fun->body.code.numHandlers; i++) {
        ex = fun->body.code.handlers[i];
        if (ex && ex->catchType) {
            ecAddNameConstant(cp, &ex->catchType->qname);
        }
    }

#if BLD_FEATURE_EJS_DOC
    if (cp->ejs->flags & EJS_FLAG_DOC) {
        ecAddDocConstant(cp, 0, fun->owner, fun->slotNum);
    }
#endif
    LEAVE(cp);
}


static void genHash(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    if (!np->hash.disabled) {
        processNode(cp, np->hash.body);
    }
    LEAVE(cp);
}


static void genIf(EcCompiler *cp, EcNode *np)
{
    EcCodeGen   *saveCode;
    EcState     *state;
    int         thenLen, elseLen, mark;

    ENTER(cp);

    mprAssert(np->kind == N_IF);

    state = cp->state;
    saveCode = state->code;

    /*
     *  Process the conditional. 
     */
    state->needsValue = 1;
    processNode(cp, np->tenary.cond);
    state->needsValue = 0;
    popStack(cp, 1);

    /*
     *  Process the "then" block.
     */
    np->tenary.thenCode = state->code = allocCodeBuffer(cp);
    mark = getStackCount(cp);
    processNode(cp, np->tenary.thenBlock);
    if (state->prev->needsValue) {
        /* Part of a tenary expression */
        if (state->code->stackCount != (mark + 1)) {
            genError(cp, np, "Then expression does not evaluate to a value. Check if operands are void");
        }
        discardStackItems(cp, mark + 1);
        if (np->tenary.elseBlock) {
            setStack(cp, mark);
        }
    } else {
        discardStackItems(cp, mark);
    }

    /*
     *  Else block (optional)
     */
    if (np->tenary.elseBlock) {
        np->tenary.elseCode = state->code = allocCodeBuffer(cp);
        state->needsValue = state->prev->needsValue;
        processNode(cp, np->tenary.elseBlock);
        state->needsValue = 0;
        if (state->prev->needsValue) {
            if (state->code->stackCount != (mark + 1)) {
                genError(cp, np, "Else expression does not evaluate to a value. Check if operands are void");
            }
            discardStackItems(cp, mark + 1);
        } else {
            discardStackItems(cp, mark);
        }
    }

    /*
     *  Calculate jump lengths. Then length will vary depending on if the jump at the end of the "then" block
     *  can jump over the "else" block with a short jump.
     */
    elseLen = (np->tenary.elseCode) ? mprGetBufLength(np->tenary.elseCode->buf) : 0;
    thenLen = mprGetBufLength(np->tenary.thenCode->buf);
    thenLen += (elseLen < 0x7f && cp->optimizeLevel > 0) ? 2 : 5;

    /*
     *  Now copy the basic blocks into the output code buffer, starting with the jump around the "then" code.
     */
    setCodeBuffer(cp, saveCode);

    if (thenLen < 0x7f && cp->optimizeLevel > 0) {
        ecEncodeOpcode(cp, EJS_OP_BRANCH_FALSE_8);
        ecEncodeByte(cp, thenLen);
    } else {
        ecEncodeOpcode(cp, EJS_OP_BRANCH_FALSE);
        ecEncodeWord(cp, thenLen);
    }

    /*
     *  Copy the then code
     */
    copyCodeBuffer(cp, state->code, np->tenary.thenCode);

    /*
     *  Create the jump to the end of the if statement
     */
    if (elseLen < 0x7f && cp->optimizeLevel > 0) {
        ecEncodeOpcode(cp, EJS_OP_GOTO_8);
        ecEncodeByte(cp, elseLen);
    } else {
        ecEncodeOpcode(cp, EJS_OP_GOTO);
        ecEncodeWord(cp, elseLen);
    }

    if (np->tenary.elseCode) {
        copyCodeBuffer(cp, state->code, np->tenary.elseCode);
    }
    if (state->prev->needsValue) {
        /* setCodeBuffer above will have reset the stack to what it was before this function */
        pushStack(cp, 1);
    }
    LEAVE(cp);
}


/*
 *  Expect data on the stack already to assign
 */
static void genLeftHandSide(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(cp);
    mprAssert(np);

    cp->state->onLeft = 1;

    switch (np->kind) {
    case N_DOT:
    case N_QNAME:
    case N_SUPER:
    case N_EXPRESSIONS:
        processNode(cp, np);
        break;

    case N_CALL:
    default:
        genError(cp, np, "Illegal left hand side");
    }
    LEAVE(cp);
}


static void genLiteral(EcCompiler *cp, EcNode *np)
{
    EjsNamespace    *nsp;
    EjsBoolean      *bp;
    EjsNumber       *ip;
    EjsString       *pattern;
    int64           n;
    int             id;

    ENTER(cp);

    /*
     *  Map Numbers to the configured real type
     */
    id = np->literal.var->type->id;

    switch (id) {
    case ES_Boolean:
        bp = (EjsBoolean*) np->literal.var;
        if (bp->value) {
            ecEncodeOpcode(cp, EJS_OP_LOAD_TRUE);
        } else {
            ecEncodeOpcode(cp, EJS_OP_LOAD_FALSE);
        }
        break;

    case ES_Number:
        /*
         *  These are signed values
         */
        ip = (EjsNumber*) np->literal.var;
#if BLD_FEATURE_FLOATING_POINT
        if (ip->value != floor(ip->value) || ip->value <= -MAXINT || ip->value >= MAXINT) {
            ecEncodeOpcode(cp, EJS_OP_LOAD_DOUBLE);
            ecEncodeDouble(cp, ip->value);
        } else
#endif
        {
            n = (int64) ip->value;
            if (0 <= n && n <= 9) {
                ecEncodeOpcode(cp, EJS_OP_LOAD_0 + (int) n);
            } else {
                ecEncodeOpcode(cp, EJS_OP_LOAD_INT);
                ecEncodeNumber(cp, n);
            }
        }
        break;

    case ES_Namespace:
        ecEncodeOpcode(cp, EJS_OP_LOAD_NAMESPACE);
        nsp = (EjsNamespace*) np->literal.var;
        ecEncodeString(cp, nsp->uri);
        break;

    case ES_Null:
        ecEncodeOpcode(cp, EJS_OP_LOAD_NULL);
        break;

    case ES_String:
        ecEncodeOpcode(cp, EJS_OP_LOAD_STRING);
        ecEncodeString(cp, ((EjsString*) np->literal.var)->value);
        break;

    case ES_XML:
        ecEncodeOpcode(cp, EJS_OP_LOAD_XML);
        ecEncodeString(cp, mprGetBufStart(np->literal.data));
        break;

    case ES_RegExp:
        ecEncodeOpcode(cp, EJS_OP_LOAD_REGEXP);
        pattern = (EjsString*) ejsRegExpToString(cp->ejs, (EjsRegExp*) np->literal.var);
        ecEncodeString(cp, pattern->value);
        mprFree(pattern);
        break;

    case ES_Void:
        ecEncodeOpcode(cp, EJS_OP_LOAD_UNDEFINED);
        break;

    default:
        mprAssert(0);
        break;
    }
    pushStack(cp, 1);
    LEAVE(cp);
}


/*
 *  Generate code for name reference. This routine handles both loads and stores.
 */
static void genName(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np->kind == N_QNAME || np->kind == N_USE_NAMESPACE);

    if (np->needThis) {
        if (np->lookup.useThis) {
            ecEncodeOpcode(cp, EJS_OP_LOAD_THIS);

        } else if (np->lookup.obj == cp->ejs->global){
            ecEncodeOpcode(cp, EJS_OP_LOAD_GLOBAL);

        } else if (cp->state->currentObjectNode) {
            ecEncodeOpcode(cp, EJS_OP_DUP);

        } else {
            /*
             *  Unbound function
             */
            ecEncodeOpcode(cp, EJS_OP_LOAD_GLOBAL);
        }
        pushStack(cp, 1);
        np->needThis = 0;
    }

    if (np->lookup.slotNum >= 0) {
        genBoundName(cp, np);

    } else {
        genUnboundName(cp, np);
    }
    LEAVE(cp);
}


static void genNew(EcCompiler *cp, EcNode *np)
{
    EcState     *state;
    int         argc;

    ENTER(cp);

    mprAssert(np->kind == N_NEW);

    state = cp->state;
    argc = 0;

    /*
     *  Process the type reference to instantiate
     */
    processNode(cp, np->left);
    ecEncodeOpcode(cp, EJS_OP_NEW);
    popStack(cp, 1);
    pushStack(cp, 1);
    LEAVE(cp);
}


static void genObjectLiteral(EcCompiler *cp, EcNode *np)
{
    EcNode      *child, *typeNode;
    EjsType     *type;
    int         next, argc;

    ENTER(cp);

    /*
     *  Push all the args
     */
    next = 0;
    while ((child = getNextNode(cp, np, &next)) != 0) {
        processNode(cp, child);
    }
    argc = next;

    ecEncodeOpcode(cp, EJS_OP_NEW_OBJECT);
    typeNode = np->objectLiteral.typeNode;
    type = (EjsType*) typeNode->lookup.ref;
    ecEncodeGlobal(cp, (EjsVar*) type, (type) ? &type->qname: 0);
    ecEncodeNumber(cp, argc);
    pushStack(cp, 1);
    popStack(cp, argc * 3);
    LEAVE(cp);
}


static void genField(EcCompiler *cp, EcNode *np)
{
    EcNode      *fieldName;

    fieldName = np->field.fieldName;

    if (fieldName->kind == N_QNAME) {
        ecEncodeOpcode(cp, EJS_OP_LOAD_STRING);
        ecEncodeString(cp, np->field.fieldName->qname.space);
        ecEncodeOpcode(cp, EJS_OP_LOAD_STRING);
        ecEncodeString(cp, np->field.fieldName->qname.name);
        pushStack(cp, 2);

    } else if (fieldName->kind == N_LITERAL) {
        ecEncodeOpcode(cp, EJS_OP_LOAD_STRING);
        ecEncodeString(cp, EJS_EMPTY_NAMESPACE);
        pushStack(cp, 1);
        genLiteral(cp, fieldName);

    } else {
        mprAssert(0);
        processNode(cp, fieldName);
    }

    if (np->field.fieldKind == FIELD_KIND_VALUE) {
        processNode(cp, np->field.expr);
    } else {
        processNode(cp, np->field.fieldName);
    }
}


static void genPostfixOp(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    /*
     *  Dup before inc
     */
    processNode(cp, np->left);
    ecEncodeOpcode(cp, EJS_OP_DUP);
    ecEncodeOpcode(cp, EJS_OP_INC);
    ecEncodeByte(cp, (np->tokenId == T_PLUS_PLUS) ? 1 : -1);
    genLeftHandSide(cp, np->left);
    pushStack(cp, 1);
    LEAVE(cp);
}


static void genProgram(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EcNode      *child;
    int         next;

    ENTER(cp);

    ejs = cp->ejs;

    next = 0;
    while ((child = getNextNode(cp, np, &next)) && !cp->error) {

        switch (child->kind) {
        case N_MODULE:
            genModule(cp, child);
            break;

        case N_DIRECTIVES:
            genDirectives(cp, child, 0);
            break;

        default:
            badNode(cp, np);
        }
    }
    LEAVE(cp);
}


static void genPragmas(EcCompiler *cp, EcNode *np)
{
    EcNode  *child;
    int     next;

    next = 0;
    while ((child = getNextNode(cp, np, &next))) {
        processNode(cp, child);
    }
}


/*
 *  Generate code for function returns
 */
static void genReturn(EcCompiler *cp, EcNode *np)
{
    EjsFunction     *fun;

    ENTER(cp);

    if (cp->state->captureBreak) {
        ecEncodeOpcode(cp, EJS_OP_FINALLY);
    }
    if (np->left) {
        fun = cp->state->currentFunction;
        if (fun->resultType == NULL || fun->resultType != cp->ejs->voidType) {
            cp->state->needsValue = 1;
            processNode(cp, np->left);
            cp->state->needsValue = 0;
            ecEncodeOpcode(cp, EJS_OP_RETURN_VALUE);
            popStack(cp, 1);

        } else if (np->ret.blockLess) {
            /*
             *  The return was inserted by the parser. So we must still process the statement
             */
            processNode(cp, np->left);
        }

    } else {
        /*
         *  return;
         */
        ecEncodeOpcode(cp, EJS_OP_RETURN);
    }
    LEAVE(cp);
}


/*
 *  Load the super pointer. Super function calls (super()) are handled via N_CALL.
 */
static void genSuper(EcCompiler *cp, EcNode *np)
{
    int     argc;

    ENTER(cp);

    mprAssert(np->kind == N_SUPER);

    if (np->left) {
        argc = mprGetListCount(np->left->children);
        if (argc > 0) {
            processNode(cp, np->left);
        }
        ecEncodeOpcode(cp, EJS_OP_CALL_NEXT_CONSTRUCTOR);
        ecEncodeNumber(cp, argc);        
        popStack(cp, argc);
    } else {
        ecEncodeOpcode(cp, EJS_OP_SUPER); 
        pushStack(cp, 1);
    }
    LEAVE(cp);
}


static void genSwitch(EcCompiler *cp, EcNode *np)
{
    EcNode      *caseItem, *elements;
    EcCodeGen   *code, *outerBlock;
    EcState     *state;
    int         next, len, nextCaseLen, nextCodeLen, totalLen, mark;

    ENTER(cp);

    state = cp->state;
    state->captureBreak = 0;

    outerBlock = state->code;
    code = state->code = allocCodeBuffer(cp);

    /*
     *  Generate code for the switch (expression)
     */
    processNode(cp, np->left);

    ecStartBreakableStatement(cp, EC_JUMP_BREAK);
    
    /*
     *  Generate the code for each case label expression and case statements.
     *  next set to one to skip the switch expression.
     */
    elements = np->right;
    mprAssert(elements->kind == N_CASE_ELEMENTS);

    next = 0;
    while ((caseItem = getNextNode(cp, elements, &next)) && !cp->error) {
        /*
         *  Allocate a buffer for the case expression and generate that code
         */
        mark = getStackCount(cp);
        mprAssert(caseItem->kind == N_CASE_LABEL);
        if (caseItem->caseLabel.kind == EC_SWITCH_KIND_CASE) {
            caseItem->caseLabel.expressionCode = state->code = allocCodeBuffer(cp);
            /*
             *  Dup the switch expression value to preserve it for later cases.
             *  OPT - don't need to preserve for default cases or if this is the last case
             */
            addDebugInstructions(cp, caseItem->caseLabel.expression);
            ecEncodeOpcode(cp, EJS_OP_DUP);
            mprAssert(caseItem->caseLabel.expression);
            processNode(cp, caseItem->caseLabel.expression);
            popStack(cp, 1);
        }

        /*
         *  Generate code for the case directives themselves.
         */
        caseItem->code = state->code = allocCodeBuffer(cp);
        mprAssert(caseItem->left->kind == N_DIRECTIVES);
        processNode(cp, caseItem->left);
        setStack(cp, mark);
    }

    /*
     *  Calculate jump lengths. Start from the last case and work backwards.
     */
    nextCaseLen = 0;
    nextCodeLen = 0;
    totalLen = 0;

    next = -1;
    while ((caseItem = getPrevNode(cp, elements, &next)) && !cp->error) {

        if (caseItem->kind != N_CASE_LABEL) {
            break;
        }
        /*
         *  CODE jump
         *  Jump to the code block of the next case. In the last block, we just fall out the bottom.
         */
        caseItem->caseLabel.nextCaseCode = nextCodeLen;
        if (nextCodeLen > 0) {
            len = (caseItem->caseLabel.nextCaseCode < 0x7f && cp->optimizeLevel > 0) ? 2 : 5;
            nextCodeLen += len;
            nextCaseLen += len;
            totalLen += len;
        }

        /*
         *  CASE jump
         *  Jump to the next case expression evaluation.
         */
        len = getCodeLength(cp, caseItem->code);
        nextCodeLen += len;
        nextCaseLen += len;
        totalLen += len;

        caseItem->jumpLength = nextCaseLen;
        nextCodeLen = 0;

        if (caseItem->caseLabel.kind == EC_SWITCH_KIND_CASE) {
            /*
             *  Jump to the next case expression test. Increment the length depending on whether we are using a
             *  goto_8 (2 bytes) or goto (4 bytes). Add one for the CMPEQ instruction (3 vs 6)
             */
            len = (caseItem->jumpLength < 0x7f && cp->optimizeLevel > 0) ? 3 : 6;
            nextCodeLen += len;
            totalLen += len;

            if (caseItem->caseLabel.expressionCode) {
                len = getCodeLength(cp, caseItem->caseLabel.expressionCode);
                nextCodeLen += len;
                totalLen += len;
            }
        }
        nextCaseLen = 0;
    }

    /*
     *  Now copy the basic blocks into the output code buffer.
     */
    setCodeBuffer(cp, code);

    next = 0;
    while ((caseItem = getNextNode(cp, elements, &next)) && !cp->error) {

        if (caseItem->caseLabel.expressionCode) {
            copyCodeBuffer(cp, state->code, caseItem->caseLabel.expressionCode);
        }
        /*
         *  Encode the jump to the next case
         */
        if (caseItem->caseLabel.kind == EC_SWITCH_KIND_CASE) {
            ecEncodeOpcode(cp, EJS_OP_COMPARE_STRICTLY_EQ);
            if (caseItem->jumpLength < 0x7f && cp->optimizeLevel > 0) {
                ecEncodeOpcode(cp, EJS_OP_BRANCH_FALSE_8);
                ecEncodeByte(cp, caseItem->jumpLength);
            } else {
                ecEncodeOpcode(cp, EJS_OP_BRANCH_FALSE);
                ecEncodeWord(cp, caseItem->jumpLength);
            }
        }
        mprAssert(caseItem->code);
        copyCodeBuffer(cp, state->code, caseItem->code);

        /*
         *  Encode the jump to the next case's code. Last case/default block may have zero length jump.
         */
        if (caseItem->caseLabel.nextCaseCode > 0) {
            if (caseItem->caseLabel.nextCaseCode < 0x7f && cp->optimizeLevel > 0) {
                ecEncodeOpcode(cp, EJS_OP_GOTO_8);
                ecEncodeByte(cp, caseItem->caseLabel.nextCaseCode);
            } else {
                ecEncodeOpcode(cp, EJS_OP_GOTO);
                ecEncodeWord(cp, caseItem->caseLabel.nextCaseCode);
            }
        }
    }
    popStack(cp, 1);

    totalLen = mprGetBufLength(state->code->buf);
    patchJumps(cp, EC_JUMP_BREAK, totalLen);

    /*
     *  Pop the switch value
     */
    ecEncodeOpcode(cp, EJS_OP_POP);
    copyCodeBuffer(cp, outerBlock, state->code);
    LEAVE(cp);
}


/*
 *  Load the this pointer.
 */
static void genThis(EcCompiler *cp, EcNode *np)
{
    EcState     *state;

    ENTER(cp);

    state = cp->state;

    switch (np->thisNode.thisKind) {
    case N_THIS_GENERATOR:
        break;

    case N_THIS_CALLEE:
        break;

    case N_THIS_TYPE:
        genClassName(cp, state->currentClass);
        break;

    case N_THIS_FUNCTION:
        genName(cp, state->currentFunctionNode);
        break;

    default:
        ecEncodeOpcode(cp, EJS_OP_LOAD_THIS);
        pushStack(cp, 1);
    }
    LEAVE(cp);
}


/*
 *
 */
static void genThrow(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    cp->state->needsValue = 1;
    processNode(cp, np->left);
    ecEncodeOpcode(cp, EJS_OP_THROW);
    popStack(cp, 1);
    LEAVE(cp);
}


/*
 *  Try, catch, finally.
 */
static void genTry(EcCompiler *cp, EcNode *np)
{
    EjsFunction *fun;
    EcNode      *child, *arg, *assignOp;
    EcCodeGen   *saveCode;
    EcState     *state;
    EjsType     *catchType;
    uint        tryStart, tryEnd, handlerStart, handlerEnd;
    int         next, len, numStack;

    ENTER(cp);

    state = cp->state;
    fun = state->currentFunction;
    mprAssert(fun);

    /*
     *  Switch to a new code buffer for the try block
     */
    numStack = getStackCount(cp);
    saveCode = state->code;
    mprAssert(saveCode);
    np->exception.tryBlock->code = state->code = allocCodeBuffer(cp);

    addDebugInstructions(cp, np);

    /*
     *  Process the try block. Will add a goto into either the finally block or if no finally block,
     *  to after the last catch.
     */
    processNode(cp, np->exception.tryBlock);

    if (np->exception.catchClauses) {
        next = 0;
        /*
         *  If there is a finally block it must be invoked before acting on any break/continue and return statements 
         */
        state->captureBreak = np->exception.finallyBlock ? 1 : 0;
        while ((child = getNextNode(cp, np->exception.catchClauses, &next)) && !cp->error) {
            child->code = state->code = allocCodeBuffer(cp);
            mprAssert(child->left);
            processNode(cp, child->left);
            if (np->exception.finallyBlock == 0) {
                ecEncodeOpcode(cp, EJS_OP_END_EXCEPTION);
            }
            /* Add jumps below */
        }
        state->captureBreak = 0;
    }

    if (np->exception.finallyBlock) {
        np->exception.finallyBlock->code = state->code = allocCodeBuffer(cp);
        /* Finally pushes the original PC */
        pushStack(cp, 1);
        processNode(cp, np->exception.finallyBlock);
        ecEncodeOpcode(cp, EJS_OP_END_EXCEPTION);
        popStack(cp, 1);
    }

    /*
     *  Calculate jump lengths for the catch block into a finally block. Start from the last catch block and work backwards.
     */
    len = 0;
    if (np->exception.catchClauses) {
        next = -1;
        while ((child = getPrevNode(cp, np->exception.catchClauses, &next)) && !cp->error) {
            child->jumpLength = len;
            if (child->jumpLength > 0 && np->exception.finallyBlock) {
                /*
                 *  Add jumps if there is a finally block. Otherwise, we use and end_ecception instruction
                 *  Increment the length depending on whether we are using a goto_8 (2 bytes) or goto (4 bytes)
                 */
                len += (child->jumpLength < 0x7f && cp->optimizeLevel > 0) ? 2 : 5;
            }
            len += getCodeLength(cp, child->code);
        }
    }

    /*
     *  Now copy the code. First the try block. Restore the primary code buffer and copy try/catch/finally
     *  code blocks into the code buffer.
     */
    setCodeBuffer(cp, saveCode);

    tryStart = ecGetCodeOffset(cp);

    /*
     *  Copy the try code and add a jump
     */
    copyCodeBuffer(cp, state->code, np->exception.tryBlock->code);

    if (np->exception.finallyBlock) {
        ecEncodeOpcode(cp, EJS_OP_FINALLY);
    }
    if (len < 0x7f && cp->optimizeLevel > 0) {
        ecEncodeOpcode(cp, EJS_OP_GOTO_8);
        ecEncodeByte(cp, len);
    } else {
        ecEncodeOpcode(cp, EJS_OP_GOTO);
        ecEncodeWord(cp, len);
    }
    tryEnd = ecGetCodeOffset(cp);


    /*
     *  Now the copy the catch blocks and add jumps
     */
    if (np->exception.catchClauses) {
        next = 0;
        while ((child = getNextNode(cp, np->exception.catchClauses, &next)) && !cp->error) {
            handlerStart = ecGetCodeOffset(cp);
            copyCodeBuffer(cp, state->code, child->code);
            if (child->jumpLength > 0 && np->exception.finallyBlock) {
                if (child->jumpLength < 0x7f && cp->optimizeLevel > 0) {
                    ecEncodeOpcode(cp, EJS_OP_GOTO_8);
                    ecEncodeByte(cp, child->jumpLength);
                } else {
                    ecEncodeOpcode(cp, EJS_OP_GOTO);
                    ecEncodeWord(cp, child->jumpLength);
                }
            }
            handlerEnd = ecGetCodeOffset(cp);

            /*
             *  Create exception handler record
             */
            catchType = 0;
            arg = 0;
            if (child->catchBlock.arg && child->catchBlock.arg->left) {
                assignOp = child->catchBlock.arg->left;
                arg = assignOp->left;
            }
            if (arg && arg->typeNode && ejsIsType(arg->typeNode->lookup.ref)) {
                catchType = (EjsType*) arg->typeNode->lookup.ref;
            }
            if (catchType == 0) {
                catchType = cp->ejs->voidType;
            }
            ecAddNameConstant(cp, &catchType->qname);
            addException(cp, tryStart, tryEnd, catchType, handlerStart, handlerEnd, np->exception.numBlocks, numStack,
                EJS_EX_CATCH);
        }
    }

    /*
     *  Finally, the finally block
     */
    if (np->exception.finallyBlock) {
        handlerStart = ecGetCodeOffset(cp);
        copyCodeBuffer(cp, state->code, np->exception.finallyBlock->code);
        handlerEnd = ecGetCodeOffset(cp);
        addException(cp, tryStart, tryEnd, cp->ejs->voidType, handlerStart, handlerEnd, np->exception.numBlocks, numStack,
            EJS_EX_FINALLY);
    }
    LEAVE(cp);
}


static void genUnaryOp(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np->kind == N_UNARY_OP);
    mprAssert(np->left);

    switch (np->tokenId) {
    case T_DELETE:
        genDelete(cp, np);
        break;

    case T_LOGICAL_NOT:
        cp->state->needsValue = 1;
        processNode(cp, np->left);
        ecEncodeOpcode(cp, EJS_OP_LOGICAL_NOT);
        break;

    case T_PLUS:
        /* Just ignore the plus */
        processNode(cp, np->left);
        break;

    case T_PLUS_PLUS:
        processNode(cp, np->left);
        ecEncodeOpcode(cp, EJS_OP_INC);
        ecEncodeByte(cp, 1);
        ecEncodeOpcode(cp, EJS_OP_DUP);
        pushStack(cp, 1);
        genLeftHandSide(cp, np->left);
        break;

    case T_MINUS:
        processNode(cp, np->left);
        ecEncodeOpcode(cp, EJS_OP_NEG);
        break;

    case T_MINUS_MINUS:
        processNode(cp, np->left);
        ecEncodeOpcode(cp, EJS_OP_INC);
        ecEncodeByte(cp, -1);
        ecEncodeOpcode(cp, EJS_OP_DUP);
        pushStack(cp, 1);
        genLeftHandSide(cp, np->left);
        break;

    case T_TILDE:
        /* Bitwise not */
        cp->state->needsValue = 1;
        processNode(cp, np->left);
        ecEncodeOpcode(cp, EJS_OP_NOT);
        break;

    case T_TYPEOF:
        cp->state->needsValue = 1;
        processNode(cp, np->left);
        ecEncodeOpcode(cp, EJS_OP_TYPE_OF);
        break;

    case T_VOID:
        /* Ignore the node and just push a void */
        ecEncodeOpcode(cp, EJS_OP_LOAD_UNDEFINED);
        pushStack(cp, 1);
        break;
    }
    LEAVE(cp);
}


static void genNameExpr(EcCompiler *cp, EcNode *np)
{
    EcState     *state;
    
    ENTER(cp);
    
    state = cp->state;
    state->currentObjectNode = 0;
    state->onLeft = 0;
    
    if (np->name.qualifierExpr) {
        processNode(cp, np->name.qualifierExpr);
    } else {
        ecEncodeOpcode(cp, EJS_OP_LOAD_STRING);
        ecEncodeString(cp, np->qname.space);
        pushStack(cp, 1);
    }
    if (np->name.nameExpr) {
        processNode(cp, np->name.nameExpr);
    } else {
        ecEncodeOpcode(cp, EJS_OP_LOAD_STRING);
        ecEncodeString(cp, np->qname.name);
        pushStack(cp, 1);
    }
    LEAVE(cp);
}


/*
 *  Generate code for an unbound name reference. We don't know the slot.
 */
static void genUnboundName(EcCompiler *cp, EcNode *np)
{
    Ejs         *ejs;
    EcState     *state;
    EjsVar      *owner;
    EjsLookup   *lookup;
    int         code;

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;

    mprAssert(np->lookup.slotNum < 0 || !cp->bind);

    lookup = &np->lookup;
    owner = lookup->obj;
    
    if (state->currentObjectNode && np->needThis) {
        ecEncodeOpcode(cp, EJS_OP_DUP);
        pushStack(cp, 1);
        np->needThis = 0;
    }

    if (np->name.qualifierExpr || np->name.nameExpr) {
        genNameExpr(cp, np);
        if (state->currentObjectNode) {
            code = (!cp->state->onLeft) ? EJS_OP_GET_OBJ_NAME_EXPR :  EJS_OP_PUT_OBJ_NAME_EXPR;
            popStack(cp, (cp->state->onLeft) ? 4 : 2);
        } else {
            code = (!cp->state->onLeft) ? EJS_OP_GET_SCOPED_NAME_EXPR :  EJS_OP_PUT_SCOPED_NAME_EXPR;
            popStack(cp, (cp->state->onLeft) ? 3 : 1);
        }
        ecEncodeOpcode(cp, code);
        LEAVE(cp);
        return;
    }

    if (state->currentObjectNode) {
        /*
         *  Property name (requires obj on stack)
         *  Store: -2, load: 0
         */
        code = (!state->onLeft) ?  EJS_OP_GET_OBJ_NAME :  EJS_OP_PUT_OBJ_NAME;
        ecEncodeOpcode(cp, code);
        ecEncodeName(cp, &np->qname);

        popStack(cp, 1);
        pushStack(cp, (state->onLeft) ? -1 : 1);

    } else if (owner == ejs->global) {
        if (np->needThis) {
            mprAssert(0);
            ecEncodeOpcode(cp, EJS_OP_LOAD_GLOBAL);
            pushStack(cp, 1);
            np->needThis = 0;
        }

        ecEncodeOpcode(cp, EJS_OP_LOAD_GLOBAL);
        pushStack(cp, 1);

        code = (!state->onLeft) ?  EJS_OP_GET_OBJ_NAME :  EJS_OP_PUT_OBJ_NAME;
        ecEncodeOpcode(cp, code);
        ecEncodeName(cp, &np->qname);

        /*
         *  Store: -2, load: 0
         */
        popStack(cp, 1);
        pushStack(cp, (state->onLeft) ? -1 : 1);

    } else if (lookup->useThis) {

        ecEncodeOpcode(cp, EJS_OP_LOAD_THIS);
        pushStack(cp, 1);
        if (np->needThis) {
            mprAssert(0);
            ecEncodeOpcode(cp, EJS_OP_DUP);
            pushStack(cp, 1);
            np->needThis = 0;
        }

        code = (!state->onLeft) ?  EJS_OP_GET_OBJ_NAME :  EJS_OP_PUT_OBJ_NAME;
        ecEncodeOpcode(cp, code);
        ecEncodeName(cp, &np->qname);

        /*
         *  Store: -2, load: 0
         */
        popStack(cp, 1);
        pushStack(cp, (state->onLeft) ? -1 : 1);


    } else if (owner && ejsIsType(owner)) {

        SAVE_ONLEFT(cp);
        genClassName(cp, (EjsType*) owner);
        RESTORE_ONLEFT(cp);

        if (np->needThis) {
            mprAssert(0);
            ecEncodeOpcode(cp, EJS_OP_DUP);
            pushStack(cp, 1);
            np->needThis = 0;
        }
        code = (!state->onLeft) ?  EJS_OP_GET_OBJ_NAME :  EJS_OP_PUT_OBJ_NAME;
        ecEncodeOpcode(cp, code);
        ecEncodeName(cp, &np->qname);

        /*
         *  Store: -2, load: 0
         */
        popStack(cp, 1);
        pushStack(cp, (state->onLeft) ? -1 : 1);

    } else {
        /*
         *  Unqualified name
         */
        if (np->needThis) {
            mprAssert(0);
            ecEncodeOpcode(cp, EJS_OP_LOAD_THIS);
            pushStack(cp, 1);
            np->needThis = 0;
        }
        code = (!state->onLeft) ?  EJS_OP_GET_SCOPED_NAME :  EJS_OP_PUT_SCOPED_NAME;
        ecEncodeOpcode(cp, code);
        ecEncodeName(cp, &np->qname);

        /*
         *  Store: -1, load: 1
         */
        pushStack(cp, (state->onLeft) ? -1 : 1);
    }
    LEAVE(cp);
}


static void genModule(EcCompiler *cp, EcNode *np)
{    
    ENTER(cp);

    mprAssert(np->kind == N_MODULE);

    addModule(cp, np->module.ref);
    genBlock(cp, np->left);
    LEAVE(cp);
}


static void genUseModule(EcCompiler *cp, EcNode *np)
{
    EcNode      *child;
    Ejs         *ejs;
    int         next;

    ENTER(cp);

    ejs = cp->ejs;

    mprAssert(np->kind == N_USE_MODULE);

    next = 0;
    while ((child = getNextNode(cp, np, &next))) {
        processNode(cp, child);
    }
    LEAVE(cp);
}


static void genUseNamespace(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np->kind == N_USE_NAMESPACE);

    /*
     *  Load the namespace reference. NOTE: use default space; will not add a namespace to the set of open spaces.
     */
    if (np->useNamespace.isLiteral) {
        ecEncodeOpcode(cp, EJS_OP_ADD_NAMESPACE);
        ecEncodeString(cp, np->qname.name);

    } else {
        genName(cp, np);
        ecEncodeOpcode(cp, EJS_OP_ADD_NAMESPACE_REF);
        popStack(cp, 1);
    }
    LEAVE(cp);
}


static void genVar(EcCompiler *cp, EcNode *np)
{
    EcState     *state;

    ENTER(cp);

    mprAssert(np->kind == N_QNAME);

    state = cp->state;

    /*
     *  Add string constants
     */
    ecAddNameConstant(cp, &np->qname);

    if (np->lookup.trait && np->lookup.trait->type) {
        ecAddConstant(cp, np->lookup.trait->type->qname.name);
    }

#if BLD_FEATURE_EJS_DOC
    if (cp->ejs->flags & EJS_FLAG_DOC) {
        ecAddDocConstant(cp, np->lookup.trait, np->lookup.obj, np->lookup.slotNum);
    }
#endif
    LEAVE(cp);
}


static void genVarDefinition(EcCompiler *cp, EcNode *np)
{
    EcState     *state;
    EcNode      *child, *var;
    int         next, varKind;

    ENTER(cp);

    mprAssert(np->kind == N_VAR_DEFINITION);

    state = cp->state;
    varKind = np->def.varKind;

    for (next = 0; (child = getNextNode(cp, np, &next)) != 0; ) {

        var = (child->kind == N_ASSIGN_OP) ? child->left : child;
        mprAssert(var->kind == N_QNAME);

        genVar(cp, var);

        if (child->kind == N_ASSIGN_OP) {
            /*
             *  Class level variable initializations must go into the instance code buffer.
             */
            if (var->name.instanceVar) {
                state->instanceCode = 1;
                mprAssert(state->instanceCodeBuf);
                state->code = state->instanceCodeBuf;
            }
            genAssignOp(cp, child);

        } else {
            addDebugInstructions(cp, var);
        }
    }
    LEAVE(cp);
}


static void genWith(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    processNode(cp, np->with.object);
    ecEncodeOpcode(cp, EJS_OP_OPEN_WITH);
    popStack(cp, 1);
    processNode(cp, np->with.statement);
    ecEncodeOpcode(cp, EJS_OP_CLOSE_BLOCK);
    LEAVE(cp);
}


/*
 *  Create the module file.
 */

static MprFile *openModuleFile(EcCompiler *cp, cchar *filename)
{
    EcState     *state;

    mprAssert(cp);
    mprAssert(filename && *filename);

    state = cp->state;

    if (cp->noout) {
        return 0;
    }
    if ((cp->file = mprOpen(cp, filename,  O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, 0664)) == 0) {
        genError(cp, 0, "Can't create %s", filename);
        return 0;
    }

    /*
     *  Create a module header once per file instead of per-module in the file
     */
    state->code = allocCodeBuffer(cp);
    if (ecCreateModuleHeader(cp) < 0) {
        genError(cp, 0, "Can't write module file header");
        return 0;
    }
    return cp->file;
}


/*
 *  Create a new code buffer
 */
static EcCodeGen *allocCodeBuffer(EcCompiler *cp)
{
    EcState     *state;
    EcCodeGen   *code;

    mprAssert(cp);

    state = cp->state;
    mprAssert(state);

    code = mprAllocObjZeroed(state, EcCodeGen);
    if (code == 0) {
        cp->fatalError = 1;
        return 0;
    }
    code->buf = mprCreateBuf(cp, EC_CODE_BUFSIZE, 0);
    if (code->buf == 0) {
        mprAssert(0);
        cp->fatalError = 1;
        return 0;
    }

    code->exceptions = mprCreateList(code);
    if (code->exceptions == 0) {
        mprAssert(0);
        return 0;
    }

    /*
     *  Jumps are fully processed before the state is freed
     */
    code->jumps = mprCreateList(code);
    if (code->jumps == 0) {
        mprAssert(0);
        return 0;
    }

    /*
     *  Inherit the allowable jump kinds and stack level
     */
    if (state->code) {
        code->jumpKinds = state->code->jumpKinds;
        code->stackCount = state->code->stackCount;
        code->breakMark = state->code->breakMark;
    }
    return code;
}


static int getCodeLength(EcCompiler *cp, EcCodeGen *code)
{
    return mprGetBufLength(code->buf);
}


static void copyCodeBuffer(EcCompiler *cp, EcCodeGen *dest, EcCodeGen *src)
{
    EjsEx           *exception;
    EcJump          *jump;
    EcState         *state;
    uint            baseOffset;
    int             len, next;

    state = cp->state;
    mprAssert(state);
    mprAssert(dest != src);

    len = getCodeLength(cp, src);
    if (len <= 0) {
        return;
    }

    /*
     *  Copy the code
     */
    baseOffset = mprGetBufLength(dest->buf);
    if (mprPutBlockToBuf(dest->buf, mprGetBufStart(src->buf), len) != len) {
        mprAssert(0);
        return;
    }

    /*
     *  Copy and fix the jump offset of jump patch records. jump->offset starts out being relative to the current code src.
     *  We add the original length of dest to make it absolute to the new dest buffer.
     */
    if (src->jumps) {
        if (src->jumps != dest->jumps) {
            next = 0;
            while ((jump = (EcJump*) mprGetNextItem(src->jumps, &next)) != 0) {
                jump->offset += baseOffset;
                mprAddItem(dest->jumps, jump);
                mprStealBlock(dest->jumps, jump);
            }
        }
    }

    /*
     *  Copy and fix exception target addresses
     */
    if (src->exceptions) {
        next = 0;
        while ((exception = (EjsEx*) mprGetNextItem(src->exceptions, &next)) != 0) {
            exception->tryStart += baseOffset;
            exception->tryEnd += baseOffset;
            exception->handlerStart += baseOffset;
            exception->handlerEnd += baseOffset;
            mprAddItem(dest->exceptions, exception);
        }
    }
}


/*
 *  Patch jump addresses a code buffer. Kind is the kind of jump (break | continue)
 */
static void patchJumps(EcCompiler *cp, int kind, int target)
{
    EcJump      *jump;
    EcCodeGen   *code;
    uchar       *pos;
    int         next;
    int         offset;

    code = cp->state->code;
    mprAssert(code);

rescan:
    next = 0;
    while ((jump = (EcJump*) mprGetNextItem(code->jumps, &next)) != 0) {
        if (jump->kind == kind) {
            offset = target - jump->offset - 4;
            mprAssert(-10000 < offset && offset < 10000);
            mprAssert(jump->offset < mprGetBufLength(code->buf));
            pos = (uchar*) mprGetBufStart(code->buf) + jump->offset;
            mprLog(cp, 7, "Patch 0x%x at address %d", offset, jump->offset);
            ecEncodeWordAtPos(cp, pos, offset);
            mprRemoveItem(code->jumps, jump);
            goto rescan;
        }
    }
}


/*
 *  Write the module contents
 */
static int flushModule(MprFile *file, EcCodeGen *code)
{
    int         len;

    len = mprGetBufLength(code->buf);
    if (len > 0) {
        if (mprWrite(file, mprGetBufStart(code->buf), len) != len) {
            return EJS_ERR;
        }
        mprFlushBuf(code->buf);
    }
    return 0;
}


/*
 *  Create the module initializer
 */
static void createInitializer(EcCompiler *cp, EjsModule *mp)
{
    Ejs             *ejs;
    EjsFunction     *fun;
    EcState         *state;
    EcCodeGen       *code;
    int             len;

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    mprAssert(state);

    /*
     *  Note: if hasInitializer is false, we may still have some code in the buffer if --debug is used.
     *  We can safely just ignore this debug code.
     */
    if (! mp->hasInitializer) {
        LEAVE(cp);
        return;
    }
    mprAssert(mprGetBufLength(mp->code->buf) > 0);

    if (cp->errorCount > 0) {
        LEAVE(cp);
        return;
    }
    if (cp->noout && !cp->run) {
        LEAVE(cp);
        return;
    }

    state->code = mp->code;
    cp->directiveState = state;
    code = cp->state->code;
    len = mprGetBufLength(code->buf);
    mprAssert(len > 0);

    ecEncodeOpcode(cp, EJS_OP_END_CODE);

    /*
     *  Extract the initialization code
     */
    fun = state->currentFunction = mp->initializer;
    if (fun) {
        setFunctionCode(cp, fun, code);
    }
    LEAVE(cp);
}


static void genError(EcCompiler *cp, EcNode *np, char *fmt, ...)
{
    va_list     arg;
    char        *msg;

    va_start(arg, fmt);

    if ((msg = mprVasprintf(cp, 0, fmt, arg)) == NULL) {
        msg = "Memory allocation error";
    }

    cp->errorCount++;
    cp->error = 1;
    cp->noout = 1;

    if (np) {
        ecReportError(cp, "error", np->filename, np->lineNumber, np->currentLine, np->column, msg);
    } else {
        ecReportError(cp, "error", 0, 0, 0, 0, msg);
    }
    mprFree(msg);
    va_end(arg);
}


static void badNode(EcCompiler *cp, EcNode *np)
{
    cp->fatalError = 1;
    cp->errorCount++;
    mprError(cp, "Unsupported language feature\nUnknown AST node kind %d", np->kind);
}


static EcNode *getNextNode(EcCompiler *cp, EcNode *np, int *next)
{
    if (cp->error) {
        return 0;
    }
    return (EcNode*) mprGetNextItem(np->children, next);
}


static EcNode *getPrevNode(EcCompiler *cp, EcNode *np, int *next)
{
    if (cp->fatalError || cp->error) {
        return 0;
    }
    return (EcNode*) mprGetPrevItem(np->children, next);
}


/*
 *  Map a lexical token to an op code
 */
static int mapToken(EcCompiler *cp, EjsOpCode tokenId)
{
    int     cond;

    cond = cp->state->conditional;

    switch (tokenId) {
    case T_BIT_AND:
        return EJS_OP_AND;

    case T_BIT_OR:
        return EJS_OP_OR;

    case T_BIT_XOR:
        return EJS_OP_XOR;

    case T_DIV:
        return EJS_OP_DIV;

    case T_EQ:
        return (cond) ? EJS_OP_BRANCH_EQ : EJS_OP_COMPARE_EQ;

    case T_NE:
        return (cond) ? EJS_OP_BRANCH_NE : EJS_OP_COMPARE_NE;

    case T_GT:
        return (cond) ? EJS_OP_BRANCH_GT : EJS_OP_COMPARE_GT;

    case T_GE:
        return (cond) ? EJS_OP_BRANCH_GE : EJS_OP_COMPARE_GE;

    case T_LT:
        return (cond) ? EJS_OP_BRANCH_LT : EJS_OP_COMPARE_LT;

    case T_LE:
        return (cond) ? EJS_OP_BRANCH_LE : EJS_OP_COMPARE_LE;

    case T_STRICT_EQ:
        return (cond) ? EJS_OP_BRANCH_STRICTLY_EQ : EJS_OP_COMPARE_STRICTLY_EQ;

    case T_STRICT_NE:
        return (cond) ? EJS_OP_BRANCH_STRICTLY_NE : EJS_OP_COMPARE_STRICTLY_NE;

    case T_LSH:
        return EJS_OP_SHL;

    case T_LOGICAL_NOT:
        return EJS_OP_NOT;

    case T_MINUS:
        return EJS_OP_SUB;

    case T_MOD:
        return EJS_OP_REM;

    case T_MUL:
        return EJS_OP_MUL;

    case T_PLUS:
        return EJS_OP_ADD;

    case T_RSH:
        return EJS_OP_SHR;

    case T_RSH_ZERO:
        return EJS_OP_USHR;

    case T_IS:
        return EJS_OP_IS_A;

    case T_INSTANCEOF:
        return EJS_OP_INST_OF;

    case T_LIKE:
        return EJS_OP_LIKE;

    case T_CAST:
        return EJS_OP_CAST;

    case T_IN:
        return EJS_OP_IN;

    default:
        mprAssert(0);
        return -1;
    }
}


static void addDebugInstructions(EcCompiler *cp, EcNode *np)
{
    if (!cp->debug || cp->state->code == 0) {
        return;
    }
    if (np->lineNumber != cp->currentLineNumber) {
        ecEncodeOpcode(cp, EJS_OP_DEBUG);
        ecEncodeString(cp, np->filename);
        ecEncodeNumber(cp, np->lineNumber);
        ecEncodeString(cp, np->currentLine);
        cp->currentLineNumber = np->lineNumber;
    }
}


static void processNode(EcCompiler *cp, EcNode *np)
{
    EcState     *state;

    ENTER(cp);

    state = cp->state;

    mprAssert(np->parent || np->kind == N_PROGRAM || np->kind == N_MODULE);

    if (np->kind != N_TRY && np->kind != N_END_FUNCTION && np->kind != N_HASH) {
        addDebugInstructions(cp, np);
    }
    switch (np->kind) {
    case N_ARGS:
        state->needsValue = 1;
        genArgs(cp, np);
        break;

    case N_ARRAY_LITERAL:
        genArrayLiteral(cp, np);
        break;

    case N_ASSIGN_OP:
        genAssignOp(cp, np);
        break;

    case N_BINARY_OP:
        genBinaryOp(cp, np);
        break;

    case N_BLOCK:
        genBlock(cp, np);
        break;

    case N_BREAK:
        genBreak(cp, np);
        break;

    case N_CALL:
        genCall(cp, np);
        break;

    case N_CLASS:
        genClass(cp, np);
        break;

    case N_CATCH_ARG:
        genCatchArg(cp, np);
        break;

    case N_CONTINUE:
        genContinue(cp, np);
        break;

    case N_DIRECTIVES:
        genDirectives(cp, np, 0);
        break;

    case N_DO:
        genDo(cp, np);
        break;

    case N_DOT:
        genDot(cp, np, 0);
        break;

    case N_END_FUNCTION:
        genEndFunction(cp, np);
        break;

    case N_EXPRESSIONS:
        genExpressions(cp, np);
        break;

    case N_FOR:
        genFor(cp, np);
        break;

    case N_FOR_IN:
        genForIn(cp, np);
        break;

    case N_FUNCTION:
        genFunction(cp, np);
        break;

    case N_HASH:
        genHash(cp, np);
        break;

    case N_IF:
        genIf(cp, np);
        break;

    case N_LITERAL:
        genLiteral(cp, np);
        break;

    case N_OBJECT_LITERAL:
        genObjectLiteral(cp, np);
        break;

    case N_FIELD:
        genField(cp, np);
        break;

    case N_QNAME:
        genName(cp, np);
        break;

    case N_NEW:
        genNew(cp, np);
        break;

    case N_NOP:
        break;

    case N_POSTFIX_OP:
        genPostfixOp(cp, np);
        break;

    case N_PRAGMA:
        break;

    case N_PRAGMAS:
        genPragmas(cp, np);
        break;

    case N_PROGRAM:
        genProgram(cp, np);
        break;

    case N_REF:
        break;

    case N_RETURN:
        genReturn(cp, np);
        break;

    case N_SUPER:
        genSuper(cp, np);
        break;

    case N_SWITCH:
        genSwitch(cp, np);
        break;

    case N_THIS:
        genThis(cp, np);
        break;

    case N_THROW:
        genThrow(cp, np);
        break;

    case N_TRY:
        genTry(cp, np);
        break;

    case N_UNARY_OP:
        genUnaryOp(cp, np);
        break;

    case N_USE_NAMESPACE:
        genUseNamespace(cp, np);
        break;

    case N_VAR_DEFINITION:
        genVarDefinition(cp, np);
        break;

    case N_MODULE:
        genModule(cp, np);
        break;

    case N_USE_MODULE:
        genUseModule(cp, np);
        break;

    case N_WITH:
        genWith(cp, np);
        break;

    default:
        mprAssert(0);
        badNode(cp, np);
    }
    mprAssert(state == cp->state);
    LEAVE(cp);
}


/*
 *  Oputput one module.
 */
static void processModule(EcCompiler *cp, EjsModule *mp)
{
    Ejs         *ejs;
    EcState     *state;
    EcCodeGen   *code;
    char        *path;

    ENTER(cp);

    ejs = cp->ejs;
    state = cp->state;
    state->currentModule = mp;

    createInitializer(cp, mp);

    if (cp->noout) {
        return;
    }
    if (! cp->outputFile) {
        if (mp->version) {
            path = mprAsprintf(cp->state, -1, "%s-%d.%d.%d%s", mp->name, 
                EJS_MAJOR(mp->version), EJS_MINOR(mp->version), EJS_PATCH(mp->version), EJS_MODULE_EXT);
        } else {
            path = mprStrcat(cp->state, -1, mp->name, EJS_MODULE_EXT, NULL);
        }
        if ((mp->file = openModuleFile(cp, path)) == 0) {
            mprFree(path);
            LEAVE(cp);
            return;
        }
        mprFree(path);

    } else {
        mp->file = cp->file;
    }
    mprAssert(mp->code);
    mprAssert(mp->file);

    code = state->code;

    if (mp->hasInitializer) {
        ecAddConstant(cp, EJS_INITIALIZER_NAME);
        ecAddConstant(cp, EJS_INTRINSIC_NAMESPACE);
    }
    if (ecCreateModuleSection(cp) < 0) {
        genError(cp, 0, "Can't write module sections");
        LEAVE(cp);
        return;
    }
    if (flushModule(mp->file, code) < 0) {
        genError(cp, 0, "Can't write to module file %s", mp->name);
        LEAVE(cp);
        return;
    }
    if (! cp->outputFile) {
        mprFree(mp->file);
        mp->file = 0;
        mp->code = 0;

    } else {
        mprFree(mp->code);
        mp->code = 0;
    }
    mp->file = 0;
}


/*
 *  Keep a list of modules potentially containing generated code and declarations.
 */
static void addModule(EcCompiler *cp, EjsModule *mp)
{
    EjsModule       *module;
    Ejs             *ejs;
    EcState         *state;
    int             count, i;

    mprAssert(cp);

    state = cp->state;
    ejs = cp->ejs;

    if (mp->code == 0 || cp->interactive) {
        mprFree(mp->code);
        mp->code = state->code = allocCodeBuffer(cp);
        mprStealBlock(mp, mp->code);
    }
    mp->loaded = 0;

    state->code = mp->code;

    mprAssert(mp->code);
    mprAssert(mp->code->buf);

    state->currentModule = mp;
    state->varBlock = ejs->global;
    state->letBlock = ejs->global;

    mprAssert(mp->initializer);
    state->currentFunction = mp->initializer;

    /*
     *  Merge means aggregate dependent input modules with the output
     */
    if (mp->dependencies && !cp->merge) {
        count = mprGetListCount(mp->dependencies);
        for (i = 0; i < count; i++) {
            module = (EjsModule*) mprGetItem(mp->dependencies, i);
            ecAddConstant(cp, module->name);
        }
    }
}


static int level = 8;

static void pushStack(EcCompiler *cp, int count)
{
    EcCodeGen       *code;

    code = cp->state->code;

    mprAssert(code);

    mprAssert(code->stackCount >= 0);
    code->stackCount += count;
    mprAssert(code->stackCount >= 0);

    mprLog(cp, level, "Stack %d, after push %d", code->stackCount, count);
}


static void popStack(EcCompiler *cp, int count)
{
    EcCodeGen       *code;

    code = cp->state->code;

    mprAssert(code);
    mprAssert(code->stackCount >= 0);
    code->stackCount -= count;
    mprAssert(code->stackCount >= 0);
    mprLog(cp, level, "Stack %d, after pop %d", code->stackCount, count);
}


static void setStack(EcCompiler *cp, int count)
{
    EcCodeGen       *code;

    code = cp->state->code;
    mprAssert(code);
    code->stackCount = count;
}


static int getStackCount(EcCompiler *cp)
{
    return cp->state->code->stackCount;
}


static void discardStackItems(EcCompiler *cp, int preserve)
{
    EcCodeGen       *code;
    int             count;

    code = cp->state->code;

    mprAssert(code);
    count = code->stackCount - preserve;

    if (count <= 0) {
        return;
    }
    if (count == 1) {
        ecEncodeOpcode(cp, EJS_OP_POP);
    } else {
        ecEncodeOpcode(cp, EJS_OP_POP_ITEMS);
        ecEncodeByte(cp, count);
    }
    code->stackCount -= count;
    mprAssert(code->stackCount >= 0);
    mprLog(cp, level, "Stack %d, after discard\n", code->stackCount);
}


/*
 *  Set the default code buffer
 */
static void setCodeBuffer(EcCompiler *cp, EcCodeGen *saveCode)
{
    cp->state->code = saveCode;
    mprLog(cp, level, "Stack %d, after restore code buffer\n", cp->state->code->stackCount);
}


static void addException(EcCompiler *cp, uint tryStart, uint tryEnd, EjsType *catchType, uint handlerStart, uint handlerEnd, 
    int numBlocks, int numStack, int flags)
{
    EcCodeGen       *code;
    EcState         *state;
    EjsEx           *exception;

    state = cp->state;
    mprAssert(state);

    code = state->code;
    mprAssert(code);

    exception = mprAllocObjZeroed(cp, EjsEx);
    if (exception == 0) {
        mprAssert(0);
        return;
    }
    exception->tryStart = tryStart;
    exception->tryEnd = tryEnd;
    exception->catchType = catchType;
    exception->handlerStart = handlerStart;
    exception->handlerEnd = handlerEnd;
    exception->numBlocks = numBlocks;
    exception->numStack = numStack;
    exception->flags = flags;
    mprAddItem(code->exceptions, exception);
}


static void addJump(EcCompiler *cp, EcNode *np, int kind)
{
    EcJump      *jump;

    ENTER(cp);

    jump = mprAllocObjZeroed(cp, EcJump);
    mprAssert(jump);

    jump->kind = kind;
    jump->node = np;
    jump->offset = ecGetCodeOffset(cp);

    mprAddItem(cp->state->code->jumps, jump);

    LEAVE(cp);
}


static void setFunctionCode(EcCompiler *cp, EjsFunction *fun, EcCodeGen *code)
{
    EjsEx       *ex;
    int         next, len;

    /*
     *  Define any try/catch blocks encountered
     */
    next = 0;
    while ((ex = (EjsEx*) mprGetNextItem(code->exceptions, &next)) != 0) {
        ejsAddException(fun, ex->tryStart, ex->tryEnd, ex->catchType, ex->handlerStart, ex->handlerEnd, ex->numBlocks, 
            ex->numStack, ex->flags, -1);
    }

    /*
     *  Define the code for the function
     */
    len = mprGetBufLength(code->buf);
    mprAssert(len >= 0);

    if (len > 0) {
        ejsSetFunctionCode(fun, (uchar*) mprGetBufStart(code->buf), len);
    }
}


static void emitNamespace(EcCompiler *cp, EjsNamespace *nsp)
{
    ecEncodeOpcode(cp, EJS_OP_ADD_NAMESPACE);
    ecEncodeString(cp, nsp->uri);
}


/*
 *  Aggregate the allowable kinds of jumps
 */
void ecStartBreakableStatement(EcCompiler *cp, int kinds)
{
    EcState     *state;

    mprAssert(cp);

    state = cp->state;
    state->code->jumpKinds |= kinds;
    state->breakState = state;
    state->code->breakMark = state->code->stackCount;
}

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
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
 *  @end
 */

/************************************************************************/
/*
 *  End of file "../src/compiler/ecCodeGen.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/compiler/ecCompile.c"
 */
/************************************************************************/

/**
    ecCompile.c - Interface to the compiler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static EjsVar   *loadScriptLiteral(Ejs *ejs, cchar *script);
static EjsVar   *loadScriptFile(Ejs *ejs, cchar *path);


int ecInitCompiler(EjsService *service)
{
    service->loadScriptLiteral = loadScriptLiteral;
    service->loadScriptFile = loadScriptFile;
    return 0;
}


/*
    Load a script file. This indirect routine is used by the core VM to compile a file when required.
 */
static EjsVar *loadScriptFile(Ejs *ejs, cchar *path)
{
    if (ejsLoadScriptFile(ejs, path, EC_FLAGS_RUN | EC_FLAGS_NO_OUT | EC_FLAGS_DEBUG | EC_FLAGS_BIND | EC_FLAGS_THROW) < 0) {
        return 0;
    }
    return ejs->result;
}


/*
    Function for ejs->loadScriptLiteral. This indirect routine is used by the core VM to compile a script when required.
 */
static EjsVar *loadScriptLiteral(Ejs *ejs, cchar *path)
{
    if (ejsLoadScriptLiteral(ejs, path, EC_FLAGS_RUN | EC_FLAGS_NO_OUT | EC_FLAGS_DEBUG | EC_FLAGS_BIND | EC_FLAGS_THROW) < 0) {
        return 0;
    }
    return ejs->result;
}


/*
    Load and initialize a script file
 */
int ejsLoadScriptFile(Ejs *ejs, cchar *path, int flags)
{
    EcCompiler      *ec;

    if ((ec = ecCreateCompiler(ejs, flags, BLD_FEATURE_EJS_LANG)) == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    if (ecCompile(ec, 1, (char**) &path, 0) < 0) {
        if (flags & EC_FLAGS_THROW) {
            ejsThrowSyntaxError(ejs, "%s", ec->errorMsg ? ec->errorMsg : "Can't parse script");
        }
        mprFree(ec);
        return EJS_ERR;
    }
    mprFree(ec);
    if (ejsRun(ejs) < 0) {
        return EJS_ERR;
    }
    return 0;
}


/*
    Load and initialize a script literal
 */
int ejsLoadScriptLiteral(Ejs *ejs, cchar *script, int flags)
{
    EcCompiler      *ec;
    cchar           *path;

    if ((ec = ecCreateCompiler(ejs, EC_FLAGS_RUN | EC_FLAGS_NO_OUT | EC_FLAGS_DEBUG | EC_FLAGS_BIND,
            BLD_FEATURE_EJS_LANG)) == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    if (ecOpenMemoryStream(ec->lexer, (uchar*) script, (int) strlen(script)) < 0) {
        mprError(ejs, "Can't open memory stream");
        mprFree(ec);
        return EJS_ERR;
    }
    path = "__script__";
    if (ecCompile(ec, 1, (char**) &path, 0) < 0) {
        if (flags & EC_FLAGS_THROW) {
            ejsThrowSyntaxError(ejs, "%s", ec->errorMsg ? ec->errorMsg : "Can't parse script");
        }
        mprFree(ec);
        return EJS_ERR;
    }
    ecCloseStream(ec->lexer);
    if (ejsRun(ejs) < 0) {
        mprFree(ec);
        return EJS_ERR;
    }
    mprFree(ec);
    return 0;
}


/*
    One-line embedding. Evaluate a file. This will compile and interpret the given Ejscript source file.
 */
int ejsEvalFile(cchar *path)
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
    if (ejsLoadScriptFile(ejs, path, EC_FLAGS_RUN | EC_FLAGS_NO_OUT | EC_FLAGS_DEBUG | EC_FLAGS_BIND) == 0) {
        ejsReportError(ejs, "Error in program");
        mprFree(mpr);
        return MPR_ERR;
    }
    mprFree(mpr);
    return 0;
}


/*
    One-line embedding. Evaluate a script. This will compile and interpret the given script.
 */
int ejsEvalScript(cchar *script)
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
    if (ejsLoadScriptLiteral(ejs, script, 
            EC_FLAGS_RUN | EC_FLAGS_NO_OUT | EC_FLAGS_DEBUG | EC_FLAGS_BIND) == 0) {
        ejsReportError(ejs, "Error in program");
        mprFree(mpr);
        return MPR_ERR;
    }
    mprFree(mpr);
    return 0;
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.

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

    @end
 */
/************************************************************************/
/*
 *  End of file "../src/compiler/ecCompile.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/compiler/ecLex.c"
 */
/************************************************************************/

/**
 *  ecLex.c - Lexical analyzer
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




typedef struct ReservedWord
{
    char    *name;
    int     groupMask;
    int     tokenId;
    int     subId;
} ReservedWord;



/*
 *  Reserved keyword table
 *  The "true", "false", "null" and "undefined" are handled as global variables
 */

static ReservedWord keywords[] =
{
  { "break",            G_RESERVED,         T_BREAK,                    0, },
  { "case",             G_RESERVED,         T_CASE,                     0, },
  { "callee",           G_CONREV,           T_CALLEE,                   0, },
  { "cast",             G_CONREV,           T_CAST,                     0, },
  { "catch",            G_RESERVED,         T_CATCH,                    0, },
  { "class",            G_RESERVED,         T_CLASS,                    0, },
  { "const",            G_CONREV,           T_CONST,                    0, },
  { "continue",         G_RESERVED,         T_CONTINUE,                 0, },
  { "default",          G_RESERVED,         T_DEFAULT,                  0, },
  { "delete",           G_RESERVED,         T_DELETE,                   0, },
  { "do",               G_RESERVED,         T_DO,                       0, },
  { "dynamic",          G_CONREV,           T_ATTRIBUTE,                T_DYNAMIC, },
  { "each",             G_CONREV,           T_EACH,                     0, },
  { "else",             G_RESERVED,         T_ELSE,                     0, },
  { "enum",             G_RESERVED,         T_ENUM,                     T_ENUM, },
  { "extends",          G_RESERVED,         T_EXTENDS,                  0, },
  { "false",            G_RESERVED,         T_FALSE,                    0, },
  { "final",            G_CONREV,           T_ATTRIBUTE,                T_FINAL, },
  { "finally",          G_RESERVED,         T_FINALLY,                  0, },
  { "for",              G_RESERVED,         T_FOR,                      0, },
  { "function",         G_RESERVED,         T_FUNCTION,                 0, },
  { "generator",        G_CONREV,           T_GENERATOR,                0, },
  { "get",              G_CONREV,           T_GET,                      0, },
  { "goto",             G_CONREV,           T_GOTO,                     0, },
  { "has",              G_CONREV,           T_HAS,                      0, },
  { "if",               G_RESERVED,         T_IF,                       0, },
  { "implements",       G_CONREV,           T_IMPLEMENTS,               0, },
  { "in",               G_RESERVED,         T_IN,                       0, },
  { "include",          G_CONREV,           T_INCLUDE,                  0, },
  { "instanceof",       G_RESERVED,         T_INSTANCEOF,               0, },
  { "interface",        G_CONREV,           T_INTERFACE,                0, },
  { "internal",         G_CONREV,           T_RESERVED_NAMESPACE,       T_INTERNAL, },
  { "intrinsic",        G_CONREV,           T_RESERVED_NAMESPACE,       T_INTRINSIC, },
  { "is",               G_CONREV,           T_IS,                       0, },
  { "lang",             G_CONREV,           T_LANG,                     0, },
  { "like",             G_CONREV,           T_LIKE,                     0, },
  { "let",              G_CONREV,           T_LET,                      0, },
  { "module",           G_RESERVED,         T_MODULE,                   0, },
  { "namespace",        G_CONREV,           T_NAMESPACE,                0, },
  { "native",           G_CONREV,           T_ATTRIBUTE,                T_NATIVE, },
  { "new",              G_RESERVED,         T_NEW,                      0, },
  { "null",             G_RESERVED,         T_NULL,                     0, },
  { "override",         G_CONREV,           T_ATTRIBUTE,                T_OVERRIDE, },
  { "private",          G_CONREV,           T_RESERVED_NAMESPACE,       T_PRIVATE, },
  { "protected",        G_CONREV,           T_RESERVED_NAMESPACE,       T_PROTECTED, },
  { "prototype",        G_CONREV,           T_ATTRIBUTE,                T_PROTOTYPE, },
  { "public",           G_CONREV,           T_RESERVED_NAMESPACE,       T_PUBLIC, },
  { "require",          G_RESERVED,         T_REQUIRE,                  0, },
  { "return",           G_RESERVED,         T_RETURN,                   0, },
  { "set",              G_CONREV,           T_SET,                      0, },
  { "standard",         G_CONREV,           T_STANDARD,                 0, },
  { "static",           G_CONREV,           T_ATTRIBUTE,                T_STATIC, },
  { "strict",           G_CONREV,           T_STRICT,                   0, },
  { "super",            G_RESERVED,         T_SUPER,                    0, },
  { "switch",           G_RESERVED,         T_SWITCH,                   0, },
  { "this",             G_RESERVED,         T_THIS,                     0, },
  { "throw",            G_RESERVED,         T_THROW,                    0, },
  { "to",               G_CONREV,           T_TO,                       0, },
  { "true",             G_RESERVED,         T_TRUE,                     0, },
  { "try",              G_RESERVED,         T_TRY,                      0, },
  { "typeof",           G_RESERVED,         T_TYPEOF,                   0, },
  { "type",             G_CONREV,           T_TYPE,                     0, },
  { "var",              G_RESERVED,         T_VAR,                      0, },
  { "undefined",        G_CONREV,           T_UNDEFINED,                0, },
  { "use",              G_CONREV,           T_USE,                      0, },
  { "void",             G_RESERVED,         T_VOID,                     0, },
  { "while",            G_RESERVED,         T_WHILE,                    0, },
  { "with",             G_RESERVED,         T_WITH,                     0, },
  { "yield",            G_CONREV,           T_YIELD,                    0, },

    /*
     *  Ejscript enhancements
     */
  { "enumerable",       G_RESERVED,         T_ATTRIBUTE,                T_ENUMERABLE, },
  { "readonly",         G_RESERVED,         T_ATTRIBUTE,                T_READONLY, },
  { "synchronized",     G_RESERVED,         T_ATTRIBUTE,                T_SYNCHRONIZED, },
  { "volatile",         G_CONREV,           T_VOLATILE,                 0, },

  /*
   *    Reserved but not implemented
   */
  { "abstract",         G_RESERVED,         T_ABSTRACT,                 0, },
  { 0,                  0,                  0, },
};


static int  addCharToToken(EcToken *tp, int c);
static int  addFormattedStringToToken(EcToken *tp, char *fmt, ...);
static int  addStringToToken(EcToken *tp, char *str);
static int  decodeNumber(EcInput *input, int radix, int length);
static void initializeToken(EcToken *tp, EcStream *stream);
static int  destroyLexer(EcLexer *lp);
static int  finishToken(EcToken *tp, int tokenId, int subId, int groupMask);
static int  getNumberToken(EcInput *input, EcToken *tp, int c);
static int  getAlphaToken(EcInput *input, EcToken *tp, int c);
static int  getComment(EcInput *input, EcToken *tp, int c);
static int  getNextChar(EcStream *stream);
static int  getQuotedToken(EcInput *input, EcToken *tp, int c);
static int  makeSubToken(EcToken *tp, int c, int tokenId, int subId, int groupMask);
static int  makeToken(EcToken *tp, int c, int tokenId, int groupMask);
static void putBackChar(EcStream *stream, int c);
static void setTokenCurrentLine(EcToken *tp);



EcLexer *ecCreateLexer(EcCompiler *cp)
{
    EcLexer         *lp;
    ReservedWord    *rp;
    int             size;

    lp = mprAllocObjWithDestructorZeroed(cp, EcLexer, destroyLexer);
    if (lp == 0) {
        return 0;
    }
    lp->input = mprAllocObjZeroed(lp, EcInput);
    if (lp->input == 0) {
        mprFree(lp);
        return 0;
    }
    lp->input->lexer = lp;
    lp->input->compiler = cp;
    lp->compiler = cp;

    size = sizeof(keywords) / sizeof(ReservedWord);
    lp->keywords = mprCreateHash(lp, size);
    if (lp->keywords == 0) {
        mprFree(lp);
        return 0;
    }
    for (rp = keywords; rp->name; rp++) {
        mprAddHash(lp->keywords, rp->name, rp);
    }
    return lp;
}


static int destroyLexer(EcLexer *lp)
{
    return 0;
}


void ecDestroyLexer(EcCompiler *cp)
{
    mprFree(cp->lexer);
    cp->lexer = 0;
}


int ecGetToken(EcInput *input)
{
    EcToken     *token, *tp;
    EcStream    *stream;
    int         c;

    token = input->token;

    if ((tp = input->putBack) != 0) {
        input->putBack = tp->next;
        input->token = tp;

        /*
         *  Move any old token to free list
         */
        if (token) {
            token->next = input->freeTokens;
            input->freeTokens = token;
        }
        return tp->tokenId;
    }

    if (token == 0) {
        //  TBD -- need an API for this
        input->token = mprAllocObjZeroed(input, EcToken);
        if (input->token == 0) {
            //  TBD -- err code
            return -1;
        }
        input->token->lineNumber = 1;
    }

    stream = input->stream;
    tp = input->token;
    mprAssert(tp);

    initializeToken(tp, stream);

    while (1) {

        c = getNextChar(stream);

        /*
         *  Overloadable operators
         *
         *  + - ~ * / % < > <= >= == << >> >>> & | === != !==
         */
        switch (c) {
        default:
            if (isdigit(c)) {
                return getNumberToken(input, tp, c);

            } else if (c == '\\') {
                c = getNextChar(stream);
                if (c == '\n') {
                    break;
                }
                putBackChar(stream, c);
                c = '\n';
            }
            if (isalpha(c) || c == '_' || c == '\\' || c == '$') {
                return getAlphaToken(input, tp, c);
            }
            return makeToken(tp, 0, T_ERR, 0);

        case -1:
            return makeToken(tp, 0, T_ERR, 0);

        case 0:
            if (stream->flags & EC_STREAM_EOL) {
                return makeToken(tp, 0, T_NOP, 0);
            } else {
                return makeToken(tp, 0, T_EOF, 0);
            }

        case ' ':
        case '\t':
            break;

        case '\r':
        case '\n':
            if (tp->textLen == 0 && tp->lineNumber != stream->lineNumber) {
                tp->currentLine = 0;
            }
            break;

        case '"':
        case '\'':
            return getQuotedToken(input, tp, c);

        case '#':
            return makeToken(tp, c, T_HASH, 0);

        case '[':
            //  EJS extension to consider this an operator
            return makeToken(tp, c, T_LBRACKET, G_OPERATOR);

        case ']':
            return makeToken(tp, c, T_RBRACKET, 0);

        case '(':
            //  EJS extension to consider this an operator
            return makeToken(tp, c, T_LPAREN, G_OPERATOR);

        case ')':
            return makeToken(tp, c, T_RPAREN, 0);

        case '{':
            return makeToken(tp, c, T_LBRACE, 0);

        case '}':
            return makeToken(tp, c, T_RBRACE, 0);

        case '@':
            return makeToken(tp, c, T_AT, 0);

        case ';':
            return makeToken(tp, c, T_SEMICOLON, 0);

        case ',':
            return makeToken(tp, c, T_COMMA, 0);

        case '?':
            return makeToken(tp, c, T_QUERY, 0);

        case '~':
            return makeToken(tp, c, T_TILDE, G_OPERATOR);

        case '+':
            c = getNextChar(stream);
            if (c == '+') {
                addCharToToken(tp, '+');
                return makeToken(tp, c, T_PLUS_PLUS, G_OPERATOR);
            } else if (c == '=') {
                addCharToToken(tp, '+');
                return makeSubToken(tp, c, T_ASSIGN, T_PLUS_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
            }
            putBackChar(stream, c);
            return makeToken(tp, '+', T_PLUS, G_OPERATOR);

        case '-':
            c = getNextChar(stream);
            if (isdigit(c)) {
                putBackChar(stream, c);
                return makeToken(tp, '-', T_MINUS, G_OPERATOR);

            } else if (c == '-') {
                addCharToToken(tp, '-');
                return makeToken(tp, c, T_MINUS_MINUS, G_OPERATOR);

            } else if (c == '=') {
                addCharToToken(tp, '-');
                return makeSubToken(tp, c, T_ASSIGN, T_MINUS_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
            }
            putBackChar(stream, c);
            return makeToken(tp, '-', T_MINUS, G_OPERATOR);

        case '*':
            c = getNextChar(stream);
            if (c == '=') {
                addCharToToken(tp, '*');
                return makeSubToken(tp, c, T_ASSIGN, T_MUL_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
            }
            putBackChar(stream, c);
            return makeToken(tp, '*', T_MUL, G_OPERATOR);

        case '/':
            c = getNextChar(stream);
            if (c == '=') {
                addCharToToken(tp, '/');
                return makeSubToken(tp, c, T_ASSIGN, T_DIV_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);

            } else if (c == '>') {
                addCharToToken(tp, '/');
                return makeToken(tp, c, T_SLASH_GT, G_OPERATOR);

            } else if (c == '*' || c == '/') {
                /*
                 *  C and C++ comments
                 */
                if (getComment(input, tp, c) < 0) {
                    return tp->tokenId;
                }
#if BLD_FEATURE_EJS_DOC
                if (tp->text && tp->text[0] == '*') {
                    mprFree(input->doc);
                    input->doc = mprStrdup(input, (char*) tp->text);
                }
#endif
                initializeToken(tp, stream);
                break;
            }
            putBackChar(stream, c);
            return makeToken(tp, '/', T_DIV, G_OPERATOR);

        case '%':
            c = getNextChar(stream);
            if (c == '=') {
                addCharToToken(tp, '%');
                return makeSubToken(tp, c, T_ASSIGN, T_MOD_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
            }
            putBackChar(stream, c);
            return makeToken(tp, '%', T_MOD, G_OPERATOR);

        case '.':
            c = getNextChar(stream);
            if (c == '.') {
                c = getNextChar(stream);
                if (c == '.') {
                    addStringToToken(tp, "..");
                    return makeToken(tp, c, T_ELIPSIS, 0);
                }
                putBackChar(stream, c);
                addCharToToken(tp, '.');
                return makeToken(tp, '.', T_DOT_DOT, 0);
            } else if (isdigit(c)) {
                putBackChar(stream, c);
                return getNumberToken(input, tp, '.');
            }
            putBackChar(stream, c);
            //  EJS extension to consider this an operator
            return makeToken(tp, '.', T_DOT, G_OPERATOR);

        case ':':
            c = getNextChar(stream);
            if (c == ':') {
                addCharToToken(tp, ':');
                return makeToken(tp, c, T_COLON_COLON, 0);
            }
            putBackChar(stream, c);
            return makeToken(tp, ':', T_COLON, 0);

        case '!':
            c = getNextChar(stream);
            if (c == '=') {
                c = getNextChar(stream);
                if (c == '=') {
                    addStringToToken(tp, "!=");
                    return makeToken(tp, c, T_STRICT_NE, G_OPERATOR);
                }
                putBackChar(stream, c);
                addCharToToken(tp, '!');
                return makeToken(tp, '=', T_NE, G_OPERATOR);
            }
            putBackChar(stream, c);
            return makeToken(tp, '!', T_LOGICAL_NOT, G_OPERATOR);

        case '&':
            c = getNextChar(stream);
            if (c == '&') {
                addCharToToken(tp, '&');
                c = getNextChar(stream);
                if (c == '=') {
                    addCharToToken(tp, '&');
                    return makeSubToken(tp, '=', T_ASSIGN, T_LOGICAL_AND_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
                }
                putBackChar(stream, c);
                return makeToken(tp, '&', T_LOGICAL_AND, G_OPERATOR);

            } else if (c == '=') {
                addCharToToken(tp, '&');
                return makeSubToken(tp, c, T_ASSIGN, T_BIT_AND_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
            }
            putBackChar(stream, c);
            return makeToken(tp, '&', T_BIT_AND, G_OPERATOR);

        case '<':
            c = getNextChar(stream);
            if (c == '=') {
                addCharToToken(tp, '<');
                return makeToken(tp, c, T_LE, G_OPERATOR);
            } else if (c == '<') {
                c = getNextChar(stream);
                if (c == '=') {
                    addStringToToken(tp, "<<");
                    return makeSubToken(tp, c, T_ASSIGN, T_LSH_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
                }
                putBackChar(stream, c);
                addCharToToken(tp, '<');
                return makeToken(tp, c, T_LSH, G_OPERATOR);

            } else if (c == '/') {
                addCharToToken(tp, '<');
                return makeToken(tp, c, T_LT_SLASH, 0);
            }
            putBackChar(stream, c);
            return makeToken(tp, '<', T_LT, G_OPERATOR);

        case '=':
            c = getNextChar(stream);
            if (c == '=') {
                c = getNextChar(stream);
                if (c == '=') {
                    addStringToToken(tp, "==");
                    return makeToken(tp, c, T_STRICT_EQ, G_OPERATOR);
                }
                putBackChar(stream, c);
                addCharToToken(tp, '=');
                return makeToken(tp, c, T_EQ, G_OPERATOR);
            }
            putBackChar(stream, c);
            return makeToken(tp, '=', T_ASSIGN, G_OPERATOR);

        case '>':
            c = getNextChar(stream);
            if (c == '=') {
                addCharToToken(tp, '<');
                return makeToken(tp, c, T_GE, G_OPERATOR);
            } else if (c == '>') {
                c = getNextChar(stream);
                if (c == '=') {
                    addStringToToken(tp, ">>");
                    return makeSubToken(tp, c, T_ASSIGN, T_RSH_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
                } else if (c == '>') {
                    c = getNextChar(stream);
                    if (c == '=') {
                        addStringToToken(tp, ">>>");
                        return makeSubToken(tp, c, T_ASSIGN, T_RSH_ZERO_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
                    }
                    putBackChar(stream, c);
                    addStringToToken(tp, ">>");
                    return makeToken(tp, '>', T_RSH_ZERO, G_OPERATOR);
                }
                putBackChar(stream, c);
                addCharToToken(tp, '>');
                return makeToken(tp, '>', T_RSH, G_OPERATOR);
            }
            putBackChar(stream, c);
            return makeToken(tp, '>', T_GT, G_OPERATOR);

        case '^':
            c = getNextChar(stream);
            if (c == '^') {
                addCharToToken(tp, '^');
                c = getNextChar(stream);
                if (c == '=') {
                    addCharToToken(tp, '^');
                    return makeSubToken(tp, '=', T_ASSIGN, T_LOGICAL_XOR_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
                }
                putBackChar(stream, c);
                return makeToken(tp, '^', T_LOGICAL_XOR, G_OPERATOR);

            } else if (c == '=') {
                addCharToToken(tp, '^');
                return makeSubToken(tp, '=', T_ASSIGN, T_BIT_XOR_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
            }
            putBackChar(stream, c);
            return makeToken(tp, '^', T_BIT_XOR, G_OPERATOR);

        case '|':
            c = getNextChar(stream);
            if (c == '|') {
                addCharToToken(tp, '|');
                c = getNextChar(stream);
                if (c == '=') {
                    addCharToToken(tp, '|');
                    return makeSubToken(tp, '=', T_ASSIGN, T_LOGICAL_OR_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);
                }
                putBackChar(stream, c);
                return makeToken(tp, '|', T_LOGICAL_OR, G_OPERATOR);

            } else if (c == '=') {
                addCharToToken(tp, '|');
                return makeSubToken(tp, '=', T_ASSIGN, T_BIT_OR_ASSIGN, G_OPERATOR | G_COMPOUND_ASSIGN);

            }
            putBackChar(stream, c);
            return makeToken(tp, '|', T_BIT_OR, G_OPERATOR);
        }
    }
}


int ecGetRegExpToken(EcInput *input)
{
    EcToken     *token, *tp;
    EcStream    *stream;
    int         c;

    stream = input->stream;
    tp = token = input->token;
    mprAssert(tp != 0);

    initializeToken(tp, stream);
    addCharToToken(tp, '/');

    while (1) {

        c = getNextChar(stream);

        switch (c) {
        case -1:
            return makeToken(tp, 0, T_ERR, 0);

        case 0:
            if (stream->flags & EC_STREAM_EOL) {
                return makeToken(tp, 0, T_NOP, 0);
            }
            return makeToken(tp, 0, T_EOF, 0);

        case '/':
            addCharToToken(tp, '/');
            while (1) {
                c = getNextChar(stream);
                if (c != 'g' && c != 'i' && c != 'm' && c != 'y' && c != 'x' && c != 'X' && c != 'U' && c != 's') {
                    putBackChar(stream, c);
                    break;
                }
                addCharToToken(tp, c);
            }
            return makeToken(tp, 0, T_REGEXP, 0);

        case '\\':
            c = getNextChar(stream);
            if (c == '\r' || c == '\n' || c == 0) {
                ecReportError(input->compiler, "warning", stream->name, stream->lineNumber, 0, stream->column,
                    "Illegal newline in regular expression");
                return makeToken(tp, 0, T_ERR, 0);
            }
            addCharToToken(tp, '\\');
            addCharToToken(tp, c);
            break;

        case '\r':
        case '\n':
            ecReportError(input->compiler, "warning", stream->name, stream->lineNumber, 0, stream->column,
                "Illegal newline in regular expression");
            return makeToken(tp, 0, T_ERR, 0);

        default:
            addCharToToken(tp, c);
        }
    }
}


/*
 *  Put back the current input token
 */
int ecPutToken(EcInput *input)
{
    ecPutSpecificToken(input, input->token);
    input->token = 0;

    return 0;
}


/*
 *  Put the given (specific) token back on the input queue. The current input
 *  token is unaffected.
 */
int ecPutSpecificToken(EcInput *input, EcToken *tp)
{
    mprAssert(tp);
    mprAssert(tp->tokenId > 0);

    /*
     *  Push token back on the input putBack queue
     */
    tp->next = input->putBack;
    input->putBack = tp;

    return 0;
}


/*
 *  Take the current token. Memory ownership retains with input unless the caller calls mprSteal.
 */
EcToken *ecTakeToken(EcInput *input)
{
    EcToken *token;

    token = input->token;
    input->token = 0;
    return token;
}


void ecFreeToken(EcInput *input, EcToken *token)
{
    token->next = input->freeTokens;
    input->freeTokens = token;
}


/*
    Hex:        0(x|X)[DIGITS]
    Octal:      0[DIGITS]
    Float:      [DIGITS].[DIGITS][(e|E)[+|-]DIGITS]
 */
static int getNumberToken(EcInput *input, EcToken *tp, int c)
{
    EcStream    *stream;

    stream = input->stream;
    if (c == '0') {
        c = getNextChar(stream);
        if (tolower(c) == 'x') {
            /* Hex */
            addCharToToken(tp, '0');
            do {
                addCharToToken(tp, c);
                c = getNextChar(stream);
            } while (isxdigit(c));
            putBackChar(stream, c);
            return finishToken(tp, T_NUMBER, -1, 0);

        } else if ('0' <= c && c <= '7') {
            /* Octal */
            addCharToToken(tp, '0');
            do {
                addCharToToken(tp, c);
                c = getNextChar(stream);
            } while ('0' <= c && c <= '7');
            putBackChar(stream, c);
            return finishToken(tp, T_NUMBER, -1, 0);

        } else {
            putBackChar(stream, c);
            c = '0';
        }
    }

    /*
        Float
     */
    while (isdigit(c)) {
        addCharToToken(tp, c);
        c = getNextChar(stream);
    }
    if (c == '.') {
        addCharToToken(tp, c);
        c = getNextChar(stream);
    }
    while (isdigit(c)) {
        addCharToToken(tp, c);
        c = getNextChar(stream);
    }
    if (tolower(c) == 'e') {
        addCharToToken(tp, c);
        c = getNextChar(stream);
        if (c == '+' || c == '-') {
            addCharToToken(tp, c);
            c = getNextChar(stream);
        }
        while (isdigit(c)) {
            addCharToToken(tp, c);
            c = getNextChar(stream);
        }
    }
    putBackChar(stream, c);
    return finishToken(tp, T_NUMBER, -1, 0);
}


static int getAlphaToken(EcInput *input, EcToken *tp, int c)
{
    ReservedWord    *rp;
    EcStream        *stream;

    /*
     *  We know that c is an alpha already
     */
    stream = input->stream;

    while (isalnum(c) || c == '_' || c == '$' || c == '\\') {
        if (c == '\\') {
            c = getNextChar(stream);
            if (c == '\n' || c == '\r') {
                break;
            } else if (c == 'u') {
                c = decodeNumber(input, 16, 4);
            }
        }
        addCharToToken(tp, c);
        c = getNextChar(stream);
    }
    if (c) {
        putBackChar(stream, c);
    }

    rp = (ReservedWord*) mprLookupHash(input->lexer->keywords, (char*) tp->text);
    if (rp) {
        return finishToken(tp, rp->tokenId, rp->subId, rp->groupMask);
    } else {
        return finishToken(tp, T_ID, -1, 0);
    }
}


static int getQuotedToken(EcInput *input, EcToken *tp, int c)
{
    EcStream    *stream;
    int         quoteType;

    stream = input->stream;
    quoteType = c;

    for (c = getNextChar(stream); c && c != quoteType; c = getNextChar(stream)) {
        if (c == 0) {
            return makeToken(tp, 0, T_ERR, 0);
        }
        if (c == '\\') {
            c = getNextChar(stream);
            switch (c) {
            //  TBD -- others
            case '\\':
                break;
            case '\'':
            case '\"':
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u':
            case 'x':
                c = decodeNumber(input, 16, 4);
                break;
            case '0':
                c = decodeNumber(input, 8, 3);
                break;
            default:
                break;
            }
        }
        addCharToToken(tp, c);
    }
    return finishToken(tp, T_STRING, -1, 0);
}


static int decodeNumber(EcInput *input, int radix, int length)
{
    char        buf[16];
    int         i, c, lowerc;

    for (i = 0; i < length; i++) {
        c = getNextChar(input->stream);
        if (c == 0) {
            break;
        }
        if (radix <= 10) {
            if (!isdigit(c)) {
                break;
            }
        } else if (radix == 16) {
            lowerc = tolower(c);
            if (!isdigit(lowerc) && !('a' <= lowerc && lowerc <= 'f')) {
                break;
            }
        }
        buf[i] = c;
    }

    if (i < length) {
        putBackChar(input->stream, c);
    }

    buf[i] = '\0';
    return (int) mprAtoi(buf, radix);
}


/*
 *  C, C++ and doc style comments. Return token or zero for no token.
 */
static int getComment(EcInput *input, EcToken *tp, int c)
{
    EcStream    *stream;
    int         form, startLine;

    startLine = tp->stream->lineNumber;

    stream = input->stream;
    form = c;

    for (form = c; c > 0;) {
        c = getNextChar(stream);
        if (c <= 0) {
            /*
             *  Unterminated Comment
             */
            addFormattedStringToToken(tp, "Unterminated comment starting on line %d", startLine);
            makeToken(tp, 0, form == '/' ? T_EOF: T_ERR, 0);
            return 1;
        }

        if (form == '/') {
            if (c == '\n') {
                break;
            }

        } else {
            if (c == '*') {
                c = getNextChar(stream);
                if (c == '/') {
                    break;
                }
                addCharToToken(tp, '*');
                putBackChar(stream, c);

            } else if (c == '/') {
                c = getNextChar(stream);
                if (c == '*') {
                    /*
                     *  Nested comment
                     */
                    if (input->compiler->warnLevel > 0) {
                        ecReportError(input->compiler, "warning", stream->name, stream->lineNumber, 0,
                                stream->column, "Possible nested comment");
                    }
                }
                addCharToToken(tp, '/');
            }
        }
        addCharToToken(tp, c);
    }

    return 0;
}


static int addCharToToken(EcToken *tp, int c)
{
    if (tp->currentLine == 0) {
        setTokenCurrentLine(tp);
    }
    if (tp->textLen >= (tp->textBufSize - 1)) {
        tp->text = (uchar*) mprRealloc(tp, tp->text, tp->textBufSize += EC_TOKEN_INCR);
        if (tp->text == 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    tp->text[tp->textLen++] = c;
    mprAssert(tp->textLen < tp->textBufSize);
    tp->text[tp->textLen] = '\0';
    return 0;
}


static int addStringToToken(EcToken *tp, char *str)
{
    char    *cp;

    for (cp = str; *cp; cp++) {
        if (addCharToToken(tp, *cp) < 0) {
            return MPR_ERR_NO_MEMORY;
        }
    }
    return 0;
}


static int addFormattedStringToToken(EcToken *tp, char *fmt, ...)
{
    va_list     args;
    char        *buf;

    va_start(args, fmt);
    buf = mprVasprintf(tp, MPR_MAX_STRING, fmt, args);
    addStringToToken(tp, buf);
    mprFree(buf);
    va_end(args);

    return 0;
}


/*
 *  Called at the start of every token to initialize the token
 */
static void initializeToken(EcToken *tp, EcStream *stream)
{
    tp->textLen = 0;
    tp->stream = stream;

    if (tp->lineNumber != stream->lineNumber) {
        tp->currentLine = 0;
    }
}


/*
 *  Set the token's source debug information. 
 */
static void setTokenCurrentLine(EcToken *tp)
{
    tp->currentLine = tp->stream->currentLine;
    tp->lineNumber = tp->stream->lineNumber;
    /*
        The column is less one because we have already consumed one character.
     */
    tp->column = max(tp->stream->column - 1, 0);
    mprAssert(tp->column >= 0);
    tp->filename = tp->stream->name;
}


/*
 *  Called to complete any token
 */
static int finishToken(EcToken *tp, int tokenId, int subId, int groupMask)
{
    EcStream        *stream;
    char            *end;
    int             len;

    mprAssert(tp);
    stream = tp->stream;
    mprAssert(stream);

    tp->tokenId = tokenId;
    tp->subId = subId;
    tp->groupMask = groupMask;

    if (tp->currentLine == 0) {
        setTokenCurrentLine(tp);
    }
    if (tp->currentLine) {
        end = strchr(tp->currentLine, '\n');
        len = end ? (int) (end - tp->currentLine) : (int) strlen(tp->currentLine);

        tp->currentLine = (char*) mprMemdup(tp, tp->currentLine, len + 1);
        tp->currentLine[len] = '\0';
        mprAssert(tp->currentLine);

        mprLog(tp, 9, "Lex lineNumber %d \"%s\" %s", tp->lineNumber, tp->text, tp->currentLine);
    }
    return tokenId;
}


static int makeToken(EcToken *tp, int c, int tokenId, int groupMask)
{
    if (c && addCharToToken(tp, c) < 0) {
        return T_ERR;
    }
    return finishToken(tp, tokenId, -1, groupMask);
}


static int makeSubToken(EcToken *tp, int c, int tokenId, int subId, int groupMask)
{
    if (addCharToToken(tp, c) < 0) {
        return T_ERR;
    }
    return finishToken(tp, tokenId, subId, groupMask);
}


/*
 *  Get the next input char from the input stream.
 */
static int getNextChar(EcStream *stream)
{
    int         c;

    if (stream->nextChar >= stream->end && stream->gets) {
        if (stream->gets(stream) < 0) {
            return 0;
        }
    }
    if (stream->nextChar < stream->end) {
        c = (uchar) *stream->nextChar++;
        if (c == '\n') {
            stream->lineNumber++;
            stream->lastColumn = stream->column;
            stream->column = 0;
            stream->lastLine = stream->currentLine;
            stream->currentLine = stream->nextChar;
        } else {
            stream->column++;
        }
        return c;
    }
    return 0;
}


/*
 *  Put back a character onto the input stream.
 */
static void putBackChar(EcStream *stream, int c)
{
    if (stream->buf < stream->nextChar && c) {
        stream->nextChar--;
        mprAssert(c == (uchar) *stream->nextChar);
        if (c == '\n') {
            stream->currentLine = stream->lastLine;
            stream->column = stream->lastColumn + 1;
            mprAssert(stream->column >= 0);
            stream->lineNumber--;
            mprAssert(stream->lineNumber >= 0);
        }
        stream->column--;
        mprAssert(stream->column >= 0);
    }
}


char *ecGetInputStreamName(EcLexer *lp)
{
    mprAssert(lp);
    mprAssert(lp->input);
    mprAssert(lp->input->stream);

    return lp->input->stream->name;
}


int ecOpenFileStream(EcLexer *lp, const char *path)
{
    EcFileStream    *fs;
    MprPath         info;
    int             c;

    fs = mprAllocObjZeroed(lp->input, EcFileStream);
    if (fs == 0) {
        return MPR_ERR_NO_MEMORY;
    }

    if ((fs->file = mprOpen(lp, path, O_RDONLY | O_BINARY, 0666)) == 0) {
        mprFree(fs);
        return MPR_ERR_CANT_OPEN;
    }

    if (mprGetPathInfo(fs, path, &info) < 0 || info.size < 0) {
        mprFree(fs);
        return MPR_ERR_CANT_ACCESS;
    }

    /* Sanity check */
    mprAssert(info.size < (100 * 1024 * 1024));
    mprAssert(info.size >= 0);

    fs->stream.buf = (char*) mprAlloc(fs, (int) info.size + 1);
    if (fs->stream.buf == 0) {
        mprFree(fs);
        return MPR_ERR_NO_MEMORY;
    }
    if (mprRead(fs->file, fs->stream.buf, (int) info.size) != (int) info.size) {
        mprFree(fs);
        return MPR_ERR_CANT_READ;
    }

    fs->stream.buf[info.size] = '\0';
    fs->stream.nextChar = fs->stream.buf;
    fs->stream.end = &fs->stream.buf[info.size];
    fs->stream.currentLine = fs->stream.buf;
    fs->stream.lineNumber = 1;
    fs->stream.compiler = lp->compiler;

    fs->stream.name = mprStrdup(lp, path);

    mprFree(lp->input->stream);
    lp->input->stream = (EcStream*) fs;

    lp->input->putBack = 0;
    lp->input->token = 0;
    lp->input->state = 0;
    lp->input->next = 0;

    /*
     *  Initialize the stream line and column data.
     */
    c = getNextChar(&fs->stream);
    putBackChar(&fs->stream, c);
    return 0;
}


int ecOpenMemoryStream(EcLexer *lp, const uchar *buf, int len)
{
    EcMemStream     *ms;
    int             c;

    ms = mprAllocObjZeroed(lp->input, EcMemStream);
    if (ms == 0) {
        return MPR_ERR_NO_MEMORY;
    }

    ms->stream.lineNumber = 0;

    ms->stream.buf = mprMemdup(ms, buf, len + 1);
    ms->stream.buf[len] = '\0';
    ms->stream.nextChar = ms->stream.buf;
    ms->stream.end = &ms->stream.buf[len];
    ms->stream.currentLine = ms->stream.buf;
    ms->stream.lineNumber = 1;
    ms->stream.compiler = lp->compiler;

    mprFree(lp->input->stream);
    lp->input->stream = (EcStream*) ms;

    lp->input->putBack = 0;
    lp->input->token = 0;
    lp->input->state = 0;
    lp->input->next = 0;

    /*
     *  Initialize the stream line and column data.
     */
    c = getNextChar(&ms->stream);
    putBackChar(&ms->stream, c);

    return 0;
}


int ecOpenConsoleStream(EcLexer *lp, EcStreamGet gets)
{
    EcConsoleStream     *cs;

    cs = mprAllocObjZeroed(lp->input, EcConsoleStream);
    if (cs == 0) {
        return MPR_ERR_NO_MEMORY;
    }

    cs->stream.lineNumber = 0;
    cs->stream.nextChar = 0;
    cs->stream.end = 0;
    cs->stream.currentLine = 0;
    cs->stream.gets = gets;
    cs->stream.compiler = lp->compiler;

    mprFree(lp->input->stream);
    lp->input->stream = (EcStream*) cs;

    lp->input->putBack = 0;
    lp->input->token = 0;
    lp->input->state = 0;
    lp->input->next = 0;

    return 0;
}


void ecCloseStream(EcLexer *lp)
{
    /*
     *  This will close file streams
     */
    mprFree(lp->input->stream);
    lp->input->stream = 0;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
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
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/compiler/ecLex.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/compiler/ecModuleWrite.c"
 */
/************************************************************************/

/**
 *  ejsModuleWrite.c - Routines to encode and emit Ejscript byte code.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




static int  createBlockSection(EcCompiler *cp, EjsVar *block, int slotNum, EjsBlock *vp);
static int  createClassSection(EcCompiler *cp, EjsVar *block, int slotNum, EjsVar *vp);
static int  createDependencySection(EcCompiler *cp);
static int  createExceptionSection(EcCompiler *cp, EjsFunction *mp);
static int  createFunctionSection(EcCompiler *cp, EjsVar *block, int slotNum, EjsFunction *fun);
static int  createGlobalProperties(EcCompiler *cp);
static int  createGlobalType(EcCompiler *cp, EjsType *klass);
static int  createPropertySection(EcCompiler *cp, EjsVar *block, int slotNum, EjsVar *vp);
static int  createSection(EcCompiler *cp, EjsVar *block, int slotNum);
static int  reserveRoom(EcCompiler *cp, int room);
static int  sum(cchar *name, int value);
static int  swapWord(EcCompiler *cp, int word);

#if BLD_FEATURE_EJS_DOC
static int  createDocSection(EcCompiler *cp, EjsVar *block, int slotNum, EjsTrait *trait);
#endif

/*
 *  Write out the module file header
 */
int ecCreateModuleHeader(EcCompiler *cp)
{
    EjsModuleHdr    hdr;

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = swapWord(cp, EJS_MODULE_MAGIC);
    hdr.fileVersion = swapWord(cp, EJS_MODULE_VERSION);

    if (cp->empty) {
        hdr.flags |= EJS_MODULE_INTERP_EMPTY;
    }
    if (ecEncodeBlock(cp, (uchar*) &hdr, sizeof(hdr)) < 0) {
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}


/*
 *  Create a module section. This writes all classes, functions, variables and blocks contained by the module.
 */
int ecCreateModuleSection(EcCompiler *cp)
{
    Ejs         *ejs;
    EjsConst    *constants;
    EjsModule   *mp;
    EcState     *state;
    MprBuf      *buf;
    int         rc;

    state = cp->state;
    buf = state->code->buf;
    mp = state->currentModule;

    mprLog(cp, 5, "Create module section %s", mp->name);

    ejs = cp->ejs;
    constants = mp->constants;

    rc = 0;
    rc += ecEncodeByte(cp, EJS_SECT_MODULE);
    rc += ecEncodeString(cp, mp->name);
    rc += ecEncodeNumber(cp, mp->version);

    /*
     *  Remember this location for the module checksum. Reserve 4 bytes.
     */
    state->checksumOffset = mprGetBufEnd(buf) - buf->data;
    mprAdjustBufEnd(buf, 4);

    /*
     *  Write the constant pool and lock it against further updates.
     */
    mp->constants->locked = 1;
    rc += ecEncodeNumber(cp, constants->len);
    rc += ecEncodeBlock(cp, (uchar*) constants->pool, constants->len);

    if (createDependencySection(cp) < 0) {
        return EJS_ERR;
    }
    if (mp->hasInitializer) {
        if (createFunctionSection(cp, 0, -1, mp->initializer) < 0) {
            return EJS_ERR;
        }
    }
    if (createGlobalProperties(cp) < 0) {
        return EJS_ERR;
    }
    rc += ecEncodeByte(cp, EJS_SECT_MODULE_END);
    if (rc < 0) {
        return MPR_ERR_CANT_WRITE;
    }

    /*
     *  Update the checksum
     */
    mp->checksum += (sum(mp->name, 0) & EJS_ENCODE_MAX_WORD);
    ejsEncodeWord((uchar*) &buf->data[state->checksumOffset], mp->checksum);
    return 0;
}


static int createDependencySection(EcCompiler *cp)
{
    Ejs         *ejs;
    EjsModule   *module, *mp;
    int         rc, i, count, version;

    mp = cp->state->currentModule;
    mprAssert(mp);

    ejs = cp->ejs;

    /*
     *  If merging, don't need references to dependent modules as they are aggregated onto the output
     */
    if (mp->dependencies && !cp->merge) {
        count = mprGetListCount(mp->dependencies);
        for (i = 0; i < count; i++) {
            module = (EjsModule*) mprGetItem(mp->dependencies, i);

            if (module->compiling) {
                continue;
            }
            if (strcmp(mp->name, module->name) == 0) {
                /* A module can't depend on itself */
                continue;
            }

            rc = 0;
            rc += ecEncodeByte(cp, EJS_SECT_DEPENDENCY);
            rc += ecEncodeString(cp, module->name);
            rc += ecEncodeNumber(cp, module->checksum);

            if (!cp->bind) {
                rc += ecEncodeNumber(cp, module->minVersion);
                rc += ecEncodeNumber(cp, module->maxVersion);
            } else {
                version = EJS_MAKE_COMPAT_VERSION(module->version);
                rc += ecEncodeNumber(cp, version);
                rc += ecEncodeNumber(cp, version);
            }
            if (rc < 0) {
                return MPR_ERR_CANT_WRITE;
            }
            mp->checksum += sum(module->name, 0);
            mprLog(cp, 5, "    dependency section for %s from module %s", module->name, mp->name);
        }
    }
    return 0;
}


/*
 *  Emit all global classes, functions, variables and blocks.
 */
static int createGlobalProperties(EcCompiler *cp)
{
    Ejs             *ejs;
    EjsName         *prop;
    EjsModule       *mp;
    EjsVar          *vp;
    int             next, slotNum;

    ejs = cp->ejs;
    mp = cp->state->currentModule;

    if (mp->globalProperties == 0) {
        return 0;
    }

    for (next = 0; (prop = (EjsName*) mprGetNextItem(mp->globalProperties, &next)) != 0; ) {
        slotNum = ejsLookupProperty(ejs, ejs->global, prop);
        if (slotNum < 0) {
            mprError(ejs, "Code generation error. Can't find global property %s.", prop->name);
            return EJS_ERR;
        }
        vp = ejsGetProperty(ejs, ejs->global, slotNum);
        if (vp->visited) {
            continue;
        }
        if (ejsIsType(vp)) {
            if (createGlobalType(cp, (EjsType*) vp) < 0) {
                return EJS_ERR;
            }
        } else {
            if (createSection(cp, ejs->global, slotNum) < 0) {
                return EJS_ERR;
            }
        }
    }
    for (next = 0; (prop = (EjsName*) mprGetNextItem(mp->globalProperties, &next)) != 0; ) {
        slotNum = ejsLookupProperty(ejs, ejs->global, prop);
        vp = ejsGetProperty(ejs, ejs->global, slotNum);
        vp->visited = 0;
    }
    return 0;
}


/*
 *  Recursively emit a class and its base classes
 */
static int createGlobalType(EcCompiler *cp, EjsType *type)
{
    Ejs             *ejs;
    EjsModule       *mp;
    EjsType         *iface;
    int             slotNum, next;

    ejs = cp->ejs;
    mp = cp->state->currentModule;

    if (type->block.obj.var.visited || type->module != mp) {
        return 0;
    }
    type->block.obj.var.visited = 1;

    if (type->baseType && !type->baseType->block.obj.var.visited) {
        createGlobalType(cp, type->baseType);
    }

    if (type->implements) {
        for (next = 0; (iface = mprGetNextItem(type->implements, &next)) != 0; ) {
            createGlobalType(cp, iface);
        }
    }

    slotNum = ejsLookupProperty(ejs, ejs->global, &type->qname);
    mprAssert(slotNum >= 0);

    if (createSection(cp, ejs->global, slotNum) < 0) {
        return EJS_ERR;
    }
    return 0;
}


static int createSection(EcCompiler *cp, EjsVar *block, int slotNum)
{
    Ejs         *ejs;
    EjsTrait    *trait;
    EjsName     qname;
    EjsVar      *vp;

    ejs = cp->ejs;
    vp = ejsGetProperty(ejs, (EjsVar*) block, slotNum);
    qname = ejsGetPropertyName(ejs, block, slotNum);
    trait = ejsGetPropertyTrait(ejs, block, slotNum);

    /*
     *  hoistBlockVar will delete hoisted properties but will not (yet) compact to reclaim the slot.
     */
    if (slotNum < 0 || trait == 0 || vp == 0 || qname.name[0] == '\0') {
        return 0;
    }

    mprAssert(qname.name);

    if (ejsIsType(vp)) {
        return createClassSection(cp, block, slotNum, vp);

    } else if (ejsIsFunction(vp)) {
        return createFunctionSection(cp, block, slotNum, (EjsFunction*) vp);

    } else if (ejsIsBlock(vp)) {
        return createBlockSection(cp, block, slotNum, (EjsBlock*) vp);
    }
    return createPropertySection(cp, block, slotNum, vp);
}


/*
 *  Create a type section in the module file.
 */
static int createClassSection(EcCompiler *cp, EjsVar *block, int slotNum, EjsVar *klass)
{
    Ejs             *ejs;
    EjsModule       *mp;
    EjsType         *type, *iface;
    EjsBlock        *instanceBlock;
    EjsTrait        *trait;
    EjsFunction     *fun;
    EjsName         qname, pname;
    int             next, i, rc, attributes, interfaceCount, instanceTraits;

    ejs = cp->ejs;
    mp = cp->state->currentModule;

    trait = ejsGetPropertyTrait(ejs, ejs->global, slotNum);
#if BLD_FEATURE_EJS_DOC
    createDocSection(cp, ejs->global, slotNum, trait);
#endif
    qname = ejsGetPropertyName(ejs, ejs->global, slotNum);
    mprAssert(qname.name);

    mprLog(cp, 5, "    type section %s for module %s", qname.name, mp->name);

    type = (EjsType*) ejsGetProperty(ejs, ejs->global, slotNum);
    mprAssert(type);
    mprAssert(ejsIsType(type));

    rc = 0;
    rc += ecEncodeByte(cp, EJS_SECT_CLASS);
    rc += ecEncodeString(cp, qname.name);
    rc += ecEncodeString(cp, qname.space);

    attributes = (trait) ? trait->attributes : 0;
    attributes &= ~(EJS_ATTR_BLOCK_HELPERS | EJS_ATTR_OBJECT_HELPERS | EJS_ATTR_SLOTS_NEED_FIXUP);

    if (type->hasStaticInitializer) {
        attributes |= EJS_ATTR_HAS_STATIC_INITIALIZER;
    }
    if (type->hasConstructor) {
        attributes |= EJS_ATTR_HAS_CONSTRUCTOR;
    }
    if (type->hasInitializer) {
        attributes |= EJS_ATTR_HAS_INITIALIZER;
    }
    if (type->callsSuper) {
        attributes |= EJS_ATTR_CALLS_SUPER;
    }
    if (type->block.obj.var.native) {
        attributes |= EJS_ATTR_NATIVE;

    } else if (type->implements) {
        for (next = 0; (iface = mprGetNextItem(type->implements, &next)) != 0; ) {
            if (!iface->isInterface) {
                attributes |= EJS_ATTR_SLOTS_NEED_FIXUP;
                break;
            }
        }
    }
    rc += ecEncodeNumber(cp, attributes);
    rc += ecEncodeNumber(cp, (cp->bind) ? slotNum: -1);

    mprAssert(type != type->baseType);
    rc += ecEncodeGlobal(cp, (EjsVar*) type->baseType, &type->baseType->qname);
    rc += ecEncodeNumber(cp, type->block.numTraits);

    instanceTraits = (type->instanceBlock) ? type->instanceBlock->numTraits: 0;
    rc += ecEncodeNumber(cp, instanceTraits);
    
    interfaceCount = (type->implements) ? mprGetListCount(type->implements) : 00;
    rc += ecEncodeNumber(cp, interfaceCount);

    if (type->implements) {
        for (next = 0; (iface = mprGetNextItem(type->implements, &next)) != 0; ) {
            rc += ecEncodeGlobal(cp, (EjsVar*) iface, &iface->qname);
        }
    }
    if (rc < 0) {
        return MPR_ERR_CANT_WRITE;
    }

    /*
     *  Loop over type traits
     */
    for (i = 0; i < type->block.numTraits; i++) {

        pname = ejsGetPropertyName(ejs, (EjsVar*) type, i);
        trait = ejsGetPropertyTrait(ejs, (EjsVar*) type, i);
        if (trait == 0) {
            continue;
        }
        if (i < type->block.numInherited) {
            /*
             *  Skip inherited and implemented functions that are not overridden. We must emit overridden functions so
             *  the loader will create a unique function defintion for the overridden method.
             */
            fun = (EjsFunction*) ejsGetProperty(ejs, (EjsVar*) type, i);
            if (fun == 0 || !fun->block.obj.var.isFunction || !fun->override) {
                continue;
            }
            if (trait->attributes & EJS_ATTR_INHERITED) {
                continue;
            }
        }
        if (createSection(cp, (EjsVar*) type, i) < 0) {
            return rc;
        }
    }

    /*
     *  Loop over non-inherited instance properties. This skips implemented and inherited properties. They will be 
     *  copied by the loader when the module is loaded.
     */
    instanceBlock = type->instanceBlock;
    if (instanceBlock) {
        for (slotNum = instanceBlock->numInherited; slotNum < instanceBlock->numTraits; slotNum++) {
            pname = ejsGetPropertyName(ejs, (EjsVar*) instanceBlock, slotNum);
            if (createSection(cp, (EjsVar*) instanceBlock, slotNum) < 0) {
                return rc;
            }
        }
    }
    mp->checksum += sum(type->qname.name, slotNum + type->block.numTraits + instanceTraits + interfaceCount);

    if (ecEncodeByte(cp, EJS_SECT_CLASS_END) < 0) {
        return EJS_ERR;
    }
    return 0;
}


/*
 *  NOTE: static methods and methods are both stored in the typeTraits.
 *  The difference is in how the methods are called by the VM op codes.
 */
static int createFunctionSection(EcCompiler *cp, EjsVar *block, int slotNum, EjsFunction *fun)
{
    Ejs             *ejs;
    EjsModule       *mp;
    EjsTrait        *trait;
    EjsName         qname;
    EjsCode         *code;
    EjsType         *resultType;
    int             rc, i, attributes;

    mprAssert(fun);

    rc = 0;
    mp = cp->state->currentModule;
    ejs = cp->ejs;
    block = fun->owner;
    slotNum = fun->slotNum;

    code = &fun->body.code;
    mprAssert(code);

    if (block && slotNum >= 0) {
        trait = ejsGetPropertyTrait(ejs, block, slotNum);
#if BLD_FEATURE_EJS_DOC
        createDocSection(cp, block, slotNum, trait);
#endif
        qname = ejsGetPropertyName(ejs, block, slotNum);
        attributes = trait->attributes;
        if (fun->isInitializer) {
            attributes |= EJS_ATTR_INITIALIZER;
        }

    } else {
        attributes = EJS_ATTR_INITIALIZER;
        qname.name = EJS_INITIALIZER_NAME;
        qname.space = EJS_INTRINSIC_NAMESPACE;
    }
    rc += ecEncodeByte(cp, EJS_SECT_FUNCTION);
    rc += ecEncodeString(cp, qname.name);
    rc += ecEncodeString(cp, qname.space);

    /*
     *  Map -1 to 0. Won't matter as it can't be a getter/setter
     */
    rc += ecEncodeNumber(cp, fun->nextSlot < 0 ? 0 : fun->nextSlot);

    if (fun->constructor) {
        attributes |= EJS_ATTR_CONSTRUCTOR;
    }
    if (fun->rest) {
        attributes |= EJS_ATTR_REST;
    }
    if (fun->fullScope) {
        attributes |= EJS_ATTR_FULL_SCOPE;
    }
    if (fun->hasReturn) {
        attributes |= EJS_ATTR_HAS_RETURN;
    }

    rc += ecEncodeNumber(cp, attributes);
    rc += ecEncodeByte(cp, fun->lang);

    resultType = fun->resultType;
    rc += ecEncodeGlobal(cp, (EjsVar*) resultType, (resultType) ? &resultType->qname : 0);

    rc += ecEncodeNumber(cp, (cp->bind || (block != ejs->global)) ? slotNum: -1);
    rc += ecEncodeNumber(cp, fun->numArgs);
    rc += ecEncodeNumber(cp, fun->block.obj.numProp - fun->numArgs);
    rc += ecEncodeNumber(cp, code->numHandlers);

    /*
     *  Output the code
     */
    rc += ecEncodeNumber(cp, code->codeLen);
    if (code->codeLen > 0) {
        rc += ecEncodeBlock(cp, code->byteCode, code->codeLen);
    }
    if (code->numHandlers > 0) {
        rc += createExceptionSection(cp, fun);
    }

    /*
     *  Recursively write args, locals and any nested functions and blocks.
     */
    attributes = 0;
    for (i = 0; i < fun->block.obj.numProp; i++) {
        createSection(cp, (EjsVar*) fun, i);
    }
    if (ecEncodeByte(cp, EJS_SECT_FUNCTION_END) < 0) {
        return EJS_ERR;
    }
    mp->checksum += sum(qname.name, slotNum + fun->numArgs + fun->block.obj.numProp - fun->numArgs + code->numHandlers);
    return rc;
}


/*
 *  NOTE: static methods and methods are both stored in the typeTraits.
 *  The difference is in how the methods are called by the VM op codes.
 */
static int createExceptionSection(EcCompiler *cp, EjsFunction *fun)
{
    Ejs         *ejs;
    EjsEx       *ex;
    EjsModule   *mp;
    int         rc, i;

    mprAssert(fun);

    rc = 0;
    mp = cp->state->currentModule;
    ejs = cp->ejs;

    rc += ecEncodeByte(cp, EJS_SECT_EXCEPTION);

    for (i = 0; i < fun->body.code.numHandlers; i++) {
        ex = fun->body.code.handlers[i];
        rc += ecEncodeByte(cp, ex->flags);
        rc += ecEncodeNumber(cp, ex->tryStart);
        rc += ecEncodeNumber(cp, ex->tryEnd);
        rc += ecEncodeNumber(cp, ex->handlerStart);
        rc += ecEncodeNumber(cp, ex->handlerEnd);
        rc += ecEncodeNumber(cp, ex->numBlocks);
        rc += ecEncodeNumber(cp, ex->numStack);
        rc += ecEncodeGlobal(cp, (EjsVar*) ex->catchType, ex->catchType ? &ex->catchType->qname : 0);
        // mp->checksum += sum(NULL, ex->tryStart + ex->tryEnd);
    }
    return rc;
}


static int createBlockSection(EcCompiler *cp, EjsVar *parent, int slotNum, EjsBlock *block)
{
    Ejs             *ejs;
    EjsModule       *mp;
    EjsName         qname;
    int             i, rc;

    ejs = cp->ejs;
    mp = cp->state->currentModule;

    if (ecEncodeByte(cp, EJS_SECT_BLOCK) < 0) {
        return MPR_ERR_CANT_WRITE;
    }

    qname = ejsGetPropertyName(ejs, parent, slotNum);
    if (ecEncodeString(cp, qname.name) < 0) {
        return MPR_ERR_CANT_WRITE;
    }
    rc = 0;
    rc += ecEncodeNumber(cp, (cp->bind || (block != ejs->globalBlock)) ? slotNum : -1);
    rc += ecEncodeNumber(cp, block->obj.numProp);
    if (rc < 0) {
        return MPR_ERR_CANT_WRITE;
    }

    /*
     *  Now emit all the properties
     */
    for (i = 0; i < block->obj.numProp; i++) {
        createSection(cp, (EjsVar*) block, i);
    }

    if (ecEncodeByte(cp, EJS_SECT_BLOCK_END) < 0) {
        return EJS_ERR;
    }
    // mp->checksum += sum(qname.name, block->obj.numProp + slotNum);
    return 0;
}


static int createPropertySection(EcCompiler *cp, EjsVar *block, int slotNum, EjsVar *vp)
{
    Ejs         *ejs;
    EjsTrait    *trait;
    EjsName     qname;
    EjsModule   *mp;
    int         rc, attributes;

    ejs = cp->ejs;
    mp = cp->state->currentModule;

    trait = ejsGetPropertyTrait(ejs, block, slotNum);
    qname = ejsGetPropertyName(ejs, block, slotNum);
    
    mprAssert(qname.name[0] != '\0');
    attributes = trait->attributes;

#if BLD_FEATURE_EJS_DOC
    createDocSection(cp, block, slotNum, trait);
#endif

    mprLog(cp, 5, "    global property section %s", qname.name);

    if (trait->type) {
        if (trait->type == ejs->namespaceType || (cp->empty && (strcmp(trait->type->qname.name, "Namespace") == 0))) {
            attributes |= EJS_ATTR_HAS_VALUE;
        }
    }
    rc = 0;
    rc += ecEncodeByte(cp, EJS_SECT_PROPERTY);
    rc += ecEncodeName(cp, &qname);

    rc += ecEncodeNumber(cp, attributes);
    rc += ecEncodeNumber(cp, (cp->bind || (block != ejs->global)) ? slotNum : -1);
    rc += ecEncodeGlobal(cp, (EjsVar*) trait->type, trait->type ? &trait->type->qname : 0);

    if (attributes & EJS_ATTR_HAS_VALUE) {
        if (vp && ejsIsNamespace(vp)) {
            rc += ecEncodeString(cp, ((EjsNamespace*) vp)->name);
        } else {
            rc += ecEncodeString(cp, 0);
        }
    }
    mp->checksum += sum(qname.name, slotNum);
    return rc;
}


#if BLD_FEATURE_EJS_DOC
static int createDocSection(EcCompiler *cp, EjsVar *block, int slotNum, EjsTrait *trait)
{
    Ejs         *ejs;
    EjsName     qname;
    EjsDoc      *doc;
    char        key[32];

    ejs = cp->ejs;
    
    if (trait == 0 || !(ejs->flags & EJS_FLAG_DOC)) {
        return 0;
    }

    if (ejs->doc == 0) {
        ejs->doc = mprCreateHash(ejs, EJS_DOC_HASH_SIZE);
    }
    if (slotNum < 0) {
        mprAssert(ejsIsBlock(block));
        slotNum = trait - ((EjsBlock*) block)->traits;
    }
    mprAssert(slotNum >= 0);
    mprSprintf(key, sizeof(key), "%Lx %d", PTOL(block), slotNum);
    doc = (EjsDoc*) mprLookupHash(ejs->doc, key);
    if (doc == 0) {
        return 0;
    }

    qname = ejsGetPropertyName(ejs, block, slotNum);
    mprAssert(qname.name);

    mprLog(cp, 5, "Create doc section for %s::%s", qname.space, qname.name);

    if (ecEncodeByte(cp, EJS_SECT_DOC) < 0) {
        return MPR_ERR_CANT_WRITE;
    }
    if (ecEncodeString(cp, doc->docString) < 0) {
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}
#endif /* BLD_FEATURE_EJS_DOC */


/*
 *  Add a constant to the constant pool. Grow if required and return the
 *  constant string offset into the pool.
 */
int ecAddConstant(EcCompiler *cp, cchar *str)
{
    int    offset;

    mprAssert(cp);

    if (str) {
        offset = ecAddModuleConstant(cp, cp->state->currentModule, str);
        if (offset < 0) {
            cp->fatalError = 1;
            mprAssert(offset > 0);
            return EJS_ERR;
        }

    } else {
        offset = 0;
    }
    return offset;
}


int ecAddNameConstant(EcCompiler *cp, EjsName *qname)
{
    if (ecAddConstant(cp, qname->name) < 0 || ecAddConstant(cp, qname->space) < 0) {
        return EJS_ERR;
    }
    return 0;
}


void ecAddFunctionConstants(EcCompiler *cp, EjsFunction *fun)
{
    if (fun->resultType) {
        ecAddNameConstant(cp, &fun->resultType->qname);
    }

#if BLD_FEATURE_EJS_DOC
    if (cp->ejs->flags & EJS_FLAG_DOC) {
        ecAddDocConstant(cp, 0, fun->owner, fun->slotNum);
    }
#endif

    ecAddBlockConstants(cp, (EjsBlock*) fun);
}


void ecAddBlockConstants(EcCompiler *cp, EjsBlock *block)
{
    Ejs         *ejs;
    EjsName     qname;
    EjsTrait    *trait;
    EjsVar      *vp;
    int         i;

    ejs = cp->ejs;

    for (i = 0; i < block->numTraits; i++) {
        qname = ejsGetPropertyName(ejs, (EjsVar*) block, i);
        ecAddNameConstant(cp, &qname);
        trait = ejsGetPropertyTrait(ejs, (EjsVar*) block, i);
        if (trait && trait->type) {
            ecAddNameConstant(cp, &trait->type->qname);
        }
        vp = ejsGetProperty(ejs, (EjsVar*) block, i);
        if (ejsIsFunction(vp)) {
            ecAddFunctionConstants(cp, (EjsFunction*) vp);
        } else if (ejsIsBlock(vp)) {
            ecAddBlockConstants(cp, (EjsBlock*) vp);
        }
    }
}


#if BLD_FEATURE_EJS_DOC
/*
 *  Allow 2 methods to get the doc string: by trait and by block:slotNum
 */
int ecAddDocConstant(EcCompiler *cp, EjsTrait *trait, EjsVar *block, int slotNum)
{
    Ejs         *ejs;
    EjsDoc      *doc;
    char        key[32];

    ejs = cp->ejs;

    if (trait == 0 && slotNum >= 0) {
        trait = ejsGetPropertyTrait(cp->ejs, block, slotNum);
    }
    if (trait) {
        if (ejs->doc == 0) {
            ejs->doc = mprCreateHash(ejs, EJS_DOC_HASH_SIZE);
        }
        if (slotNum < 0) {
            mprAssert(ejsIsBlock(block));
            slotNum = trait - ((EjsBlock*) block)->traits;
        }
        mprAssert(slotNum >= 0);
        mprSprintf(key, sizeof(key), "%Lx %d", PTOL(block), slotNum);
        doc = (EjsDoc*) mprLookupHash(ejs->doc, key);
        if (doc && doc->docString) {
            if (ecAddConstant(cp, doc->docString) < 0) {
                return EJS_ERR;
            }
        }
    }
    return 0;
}
#endif


/*
 *  Add a constant and encode the offset.
 */
int ecAddModuleConstant(EcCompiler *cp, EjsModule *mp, cchar *str)
{
    Ejs         *ejs;
    EjsConst    *constants;
    MprHash     *sp;
    int         len, oldLen, size;

    mprAssert(mp);

    if (str == 0) {
        /* Just ignore null names */
        return 0;
    }

    ejs = cp->ejs;
    mprAssert(ejs);
    constants = mp->constants;

    /*
     *  Maintain a symbol table for quick location
     */
    sp = mprLookupHashEntry(constants->table, str);
    if (sp != 0) {
        return PTOI(sp->data);
    }

    if (constants->locked) {
        mprError(ejs, "Constant pool for module %s is locked. Can't add \"%s\", try another module name.", 
            mp->name, str);
        cp->fatalError = 1;
        return MPR_ERR_CANT_CREATE;
    }

    /*
     *  First string always starts at 1.
     */
    if (constants->len == 0) {
        constants->len = 1;
        constants->size = EC_BUFSIZE;
        constants->pool = (char*) mprAllocZeroed(constants, constants->size);
        if (constants->pool == 0) {
            cp->fatalError = 1;
            return MPR_ERR_CANT_CREATE;
        }
    }
    oldLen = constants->len;

    /*
     *  Add one for the null
     */
    len = (int) strlen(str) + 1;

    if ((oldLen + len) >= constants->size) {
        size = constants->size + len;
        size = (size + EC_BUFSIZE) / EC_BUFSIZE * EC_BUFSIZE;
        constants->pool = (char*) mprRealloc(constants, constants->pool, size);
        if (constants->pool == 0) {
            cp->fatalError = 1;
            return MPR_ERR_CANT_CREATE;
        }
        memset(&constants->pool[constants->size], 0, size - constants->size);
        constants->size = size;
    }

    mprStrcpy(&constants->pool[oldLen], len, str);
    constants->len += len;
    mprAddHash(constants->table, str, ITOP(oldLen));
    return oldLen;
}

/*
 *  Emit an encoded string ored with flags. The name index is shifted by 2.
 */
static int encodeTypeName(EcCompiler *cp, cchar *name, int flags)
{
    int        offset;

    mprAssert(name && *name);

    offset = ecAddModuleConstant(cp, cp->state->currentModule, name);
    if (offset < 0) {
        cp->fatalError = 1;
        mprAssert(offset > 0);
        return EJS_ERR;
    }
    return ecEncodeNumber(cp, offset << 2 | flags);
}


/*
 *  Encode a global variable (usually a type). The encoding is untyped: 0, bound type: slot number, unbound or 
 *  unresolved type: name.
 */
int ecEncodeGlobal(EcCompiler *cp, EjsVar *obj, EjsName *qname)
{
    Ejs         *ejs;
    int         slotNum;

    ejs = cp->ejs;
    slotNum = -1;

    if (obj == 0) {
        ecEncodeNumber(cp, EJS_ENCODE_GLOBAL_NOREF);
        return 0;
    }

    /*
     *  If binding globals, we can encode the slot number of the type.
     */
    if (obj->builtin || cp->bind) {
        slotNum = ejsLookupProperty(ejs, ejs->global, qname);
        if (slotNum >= 0) {
            ecEncodeNumber(cp, (slotNum << 2) | EJS_ENCODE_GLOBAL_SLOT);
            return 0;
        }
    }

    /*
     *  So here we encode the type name and namespace name.
     */
    encodeTypeName(cp, qname->name, EJS_ENCODE_GLOBAL_NAME);
    ecEncodeString(cp, qname->space);
    return 0;
}


/*
 *  Reserve a small amount of room sufficient for the next encoding
 */
static int reserveRoom(EcCompiler *cp, int room)
{
    EcCodeGen       *code;

    code = cp->state->code;
    mprAssert(code);

    if (mprGetBufSpace(code->buf) < room) {
        if (mprGrowBuf(code->buf, -1) < 0) {
            cp->fatalError = 1;
            cp->memError = 1;
            mprAssert(0);
            return MPR_ERR_NO_MEMORY;
        }
    }
    return 0;
}


/*
 *  Encode an Ejscript instruction operation code
 */
int ecEncodeOpcode(EcCompiler *cp, int code)
{
    mprAssert(code < 240);
    mprAssert(cp);

    cp->lastOpcode = code;
    return ecEncodeByte(cp, code);
}


/*
 *  Encode a <name><namespace> pair
 */
int ecEncodeName(EcCompiler *cp, EjsName *qname)
{
    int     rc;

    mprAssert(qname->name);

    rc = 0;
    rc += ecEncodeString(cp, qname->name);
    rc += ecEncodeString(cp, qname->space);
    return rc;

}


int ecEncodeString(EcCompiler *cp, cchar *str)
{
    int    offset;

    mprAssert(cp);

    if (str) {
        offset = ecAddModuleConstant(cp, cp->state->currentModule, str);
        if (offset < 0) {
            cp->error = 1;
            cp->fatalError = 1;
            return EJS_ERR;
        }

    } else {
        offset = 0;
    }
    return ecEncodeNumber(cp, offset);
}


int ecEncodeByte(EcCompiler *cp, int value)
{
    EcCodeGen   *code;
    uchar       *pc;

    mprAssert(cp);
    code = cp->state->code;

    if (reserveRoom(cp, sizeof(uchar)) < 0) {
        mprAssert(0);
        return EJS_ERR;
    }

    pc = (uchar*) mprGetBufEnd(code->buf);
    *pc++ = value;
    mprAdjustBufEnd(code->buf, sizeof(uchar));
    return 0;
}


int ecEncodeNumber(EcCompiler *cp, int64 number)
{
    MprBuf      *buf;
    int         len;

    mprAssert(cp);
    buf = cp->state->code->buf;
    if (reserveRoom(cp, sizeof(int64) + 2) < 0) {
        mprAssert(0);
        return EJS_ERR;
    }
    len = ejsEncodeNum((uchar*) mprGetBufEnd(buf), number);
    mprAdjustBufEnd(buf, len);
    return 0;
}


#if BLD_FEATURE_FLOATING_POINT
int ecEncodeDouble(EcCompiler *cp, double value)
{
    MprBuf      *buf;
    int         len;

    mprAssert(cp);
    buf = cp->state->code->buf;
    if (reserveRoom(cp, sizeof(double) + 4) < 0) {
        mprAssert(0);
        return EJS_ERR;
    }
    len = ejsEncodeDouble(cp->ejs, (uchar*) mprGetBufEnd(buf), value);
    mprAdjustBufEnd(buf, len);
    return 0;
}
#endif


/*
 *  Encode a 32-bit number. Always emit exactly 4 bytes.
 */
int ecEncodeWord(EcCompiler *cp, int number)
{
    MprBuf      *buf;
    int         len;

    mprAssert(cp);
    buf = cp->state->code->buf;

    if (reserveRoom(cp, sizeof(int) / sizeof(char)) < 0) {
        mprAssert(0);
        return EJS_ERR;
    }
    len = ejsEncodeWord((uchar*) mprGetBufEnd(buf), number);
    mprAssert(len == 4);
    mprAdjustBufEnd(buf, len);
    return 0;
}


int ecEncodeByteAtPos(EcCompiler *cp, uchar *pos, int value)
{
    return ejsEncodeByteAtPos(pos, value);
}


int ecEncodeWordAtPos(EcCompiler *cp, uchar *pos, int value)
{
    if (abs(value) > EJS_ENCODE_MAX_WORD) {
        mprError(cp, "Code generation error. Word %d exceeds maximum %d", value, EJS_ENCODE_MAX_WORD);
        return -1;
    }
    return ejsEncodeWordAtPos(pos, value);
}


int ecEncodeBlock(EcCompiler *cp, uchar *buf, int len)
{
    EcCodeGen   *code;

    code = cp->state->code;

    if (reserveRoom(cp, len) < 0) {
        mprAssert(0);
        return EJS_ERR;
    }
    if (mprPutBlockToBuf(code->buf, (char*) buf, len) != len) {
        cp->fatalError = 1;
        cp->memError = 1;
        mprAssert(0);
        return EJS_ERR;
    }
    return 0;
}


uint ecGetCodeOffset(EcCompiler *cp)
{
    EcCodeGen   *code;

    code = cp->state->code;
    return (uint) ((uchar*) mprGetBufEnd(code->buf) - (uchar*) mprGetBufStart(code->buf));
}


int ecGetCodeLen(EcCompiler *cp, uchar *mark)
{
    EcCodeGen   *code;

    code = cp->state->code;
    return (int) (((uchar*) mprGetBufEnd(code->buf)) - mark);
}


/*
 *  Copy the code at "pos" of length "size" the distance specified by "dist". Dist may be postitive or negative.
 */
void ecCopyCode(EcCompiler *cp, uchar *pos, int size, int dist)
{
    mprMemcpy((char*) &pos[dist], size, (char*) pos, size);
}


void ecAdjustCodeLength(EcCompiler *cp, int adj)
{
    EcCodeGen   *code;

    code = cp->state->code;
    mprAdjustBufEnd(code->buf, adj);
}


#if UNUSED
static int swapShort(EcCompiler *cp, int word)
{
    if (mprGetEndian(cp) == MPR_LITTLE_ENDIAN) {
        return word;
    }
    word = ((word & 0xFFFF) << 16) | ((word & 0xFFFF0000) >> 16);
    return ((word & 0xFF) << 8) | ((word & 0xFF00) >> 8);
}
#endif

static int swapWord(EcCompiler *cp, int word)
{
    if (mprGetEndian(cp) == MPR_LITTLE_ENDIAN) {
        return word;
    }
    return ((word & 0xFF000000) >> 24) | ((word & 0xFF0000) >> 8) | ((word & 0xFF00) << 8) | ((word & 0xFF) << 24);
}

/*
 *  Simple checksum of name and slots. Not meant to be rigorous.
 */
static int sum(cchar *name, int value)
{
    cchar    *cp;
    int     checksum;

    checksum = value;
    if (name) {
        for (cp = name; *cp; cp++) {
            checksum += *cp;
        }
    }
    return checksum;
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
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
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/compiler/ecModuleWrite.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/compiler/ecParser.c"
 */
/************************************************************************/

/**
 *  ecParser. Parse ejscript source files.
 *
 *  Parse source and create an internal abstract syntax tree of nodes representing the program.
 *
 *  The Abstract Syntax Tree (AST) is comprised of a linked set of EcNodes. EjsNodes have a left and right pointer.
 *  Node with a list of children are represented by right hand links.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */




#define peekToken(cp)   peekAheadToken(cp, 1)

/*
 *  State level macros. Enter/Leave manage state and inheritance of state.
 */
#undef ENTER
#define ENTER(a)        if (ecEnterState(a) < 0) { return 0; } else

#undef LEAVE
#define LEAVE(cp, np)   ecLeaveStateWithResult(cp, np)


static void     addTokenToBuf(EcCompiler *cp, EcNode *np);
static void     appendDocString(EcCompiler *cp, EcNode *np, EcNode *parameter, EcNode *value);
static EcNode   *appendNode(EcNode *top, EcNode *np);
static void     applyAttributes(EcCompiler *cp, EcNode *np, EcNode *attributes, cchar *namespaceName);
static void     copyDocString(EcCompiler *cp, EcNode *np, EcNode *from);
static int      compileInner(EcCompiler *cp, int argc, char **argv, int flags);
static int      compileInput(EcCompiler *cp, EcNode **nodes, cchar *path);
static EcNode   *createAssignNode(EcCompiler *cp, EcNode *lhs, EcNode *rhs, EcNode *parent);
static EcNode   *createBinaryNode(EcCompiler *cp, EcNode *lhs, EcNode *rhs, EcNode *parent);
static EcNode   *createNameNode(EcCompiler *cp, cchar *name, cchar *space);
static EcNode   *createNamespaceNode(EcCompiler *cp, cchar *name, bool isDefault, bool isLiteral);
static EcNode   *createNode(EcCompiler *cp, int kind);
static void     dummy(int junk);
static EcNode   *expected(EcCompiler *cp, const char *str);
static int      getToken(EcCompiler *cp);
static EcNode   *insertNode(EcNode *top, EcNode *np, int pos);
static EcNode   *linkNode(EcNode *np, EcNode *node);
static const char *getExt(const char *path);
static EcNode   *parseAnnotatableDirective(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseArgumentList(EcCompiler *cp);
static EcNode   *parseArguments(EcCompiler *cp);
static EcNode   *parseArrayType(EcCompiler *cp);
static EcNode   *parseAssignmentExpression(EcCompiler *cp);
static EcNode   *parseAttribute(EcCompiler *cp);
static EcNode   *parseAttributeName(EcCompiler *cp);
static EcNode   *parseBlock(EcCompiler *cp);
static EcNode   *parseBlockStatement(EcCompiler *cp);
static EcNode   *parseBrackets(EcCompiler *cp);
static EcNode   *parseBreakStatement(EcCompiler *cp);
static EcNode   *parseCaseElements(EcCompiler *cp);
static EcNode   *parseCaseLabel(EcCompiler *cp);
static EcNode   *parseCatchClause(EcCompiler *cp);
static EcNode   *parseCatchClauses(EcCompiler *cp);
static EcNode   *parseClassBody(EcCompiler *cp);
static EcNode   *parseClassDefinition(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseClassInheritance(EcCompiler *cp);
static EcNode   *parseClassName(EcCompiler *cp);
static EcNode   *parseConstructorSignature(EcCompiler *cp, EcNode *np);
static EcNode   *parseConstructorInitializer(EcCompiler *cp);
static EcNode   *parseContinueStatement(EcCompiler *cp);
static EcNode   *parseDirective(EcCompiler *cp);
static EcNode   *parseDirectives(EcCompiler *cp);
static EcNode   *parseDoStatement(EcCompiler *cp);
static EcNode   *parseDirectivesPrefix(EcCompiler *cp);
static EcNode   *parseElementList(EcCompiler *cp, EcNode *newNode);
static EcNode   *parseElements(EcCompiler *cp, EcNode *newNode);
static EcNode   *parseElementTypeList(EcCompiler *cp);
static EcNode   *parseFieldList(EcCompiler *cp, EcNode *np);
static EcNode   *parseEmptyStatement(EcCompiler *cp);
static EcNode   *parseError(EcCompiler *cp, char *fmt, ...);
static EcNode   *parseExpressionStatement(EcCompiler *cp);
static EcNode   *parseFieldName(EcCompiler *cp);
static int      parseFile(EcCompiler *cp, char *path, EcNode **nodes);
static EcNode   *parseForStatement(EcCompiler *cp);
static EcNode   *parseFunctionDeclaration(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseFunctionDefinition(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseFunctionBody(EcCompiler *cp, EcNode *fun);
static EcNode   *parseFunctionExpression(EcCompiler *cp);
static EcNode   *parseFunctionExpressionBody(EcCompiler *cp);
static EcNode   *parseFunctionName(EcCompiler *cp);
static EcNode   *parseFunctionSignature(EcCompiler *cp, EcNode *np);
static EcNode   *parseHashStatement(EcCompiler *cp);
static EcNode   *parseIdentifier(EcCompiler *cp);
static EcNode   *parseIfStatement(EcCompiler *cp);
static EcNode   *parseInterfaceBody(EcCompiler *cp);
static EcNode   *parseInterfaceInheritance(EcCompiler *cp);
static EcNode   *parseInitializerList(EcCompiler *cp, EcNode *np);
static EcNode   *parseInitializer(EcCompiler *cp);
static EcNode   *parseParameter(EcCompiler *cp, bool rest);
static EcNode   *parseParameterInit(EcCompiler *cp, EcNode *args);
static EcNode   *parseInterfaceDefinition(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseLabeledStatement(EcCompiler *cp);
static EcNode   *parseLeftHandSideExpression(EcCompiler *cp);
static EcNode   *parseLetBindingList(EcCompiler *cp);
static EcNode   *parseLetExpression(EcCompiler *cp);
static EcNode   *parseLetStatement(EcCompiler *cp);
static EcNode   *parseLiteralElement(EcCompiler *cp);
static EcNode   *parseLiteralField(EcCompiler *cp);
static EcNode   *parseListExpression(EcCompiler *cp);
static EcNode   *parseNamespaceAttribute(EcCompiler *cp);
static EcNode   *parseNamespaceDefinition(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseNamespaceInitialisation(EcCompiler *cp, EcNode *nameNode);
static EcNode   *parseNonemptyParameters(EcCompiler *cp, EcNode *list);
static EcNode   *parseNullableTypeExpression(EcCompiler *cp);
static EcNode   *parseOptionalExpression(EcCompiler *cp);
static EcNode   *parseOverloadedOperator(EcCompiler *cp);
static EcNode   *parseParenListExpression(EcCompiler *cp);
static EcNode   *parseParameterisedTypeName(EcCompiler *cp);
static EcNode   *parseParameterKind(EcCompiler *cp);
static EcNode   *parseParameters(EcCompiler *cp, EcNode *args);
static EcNode   *parsePath(EcCompiler *cp, EcNode *lhs);
static EcNode   *parsePattern(EcCompiler *cp);
static EcNode   *parsePragmaItems(EcCompiler *cp, EcNode *np);
static EcNode   *parsePragmaItem(EcCompiler *cp);
static EcNode   *parsePragmas(EcCompiler *cp, EcNode *np);
static EcNode   *parsePrimaryExpression(EcCompiler *cp);
static EcNode   *parsePrimaryName(EcCompiler *cp);
static EcNode   *parseProgram(EcCompiler *cp, cchar *path);
static EcNode   *parsePropertyName(EcCompiler *cp);
static EcNode   *parsePropertyOperator(EcCompiler *cp);
static EcNode   *parseQualifiedNameIdentifier(EcCompiler *cp);
static EcNode   *parseRegularExpression(EcCompiler *cp);
static EcNode   *parseRequireItem(EcCompiler *cp);
static EcNode   *parseRequireItems(EcCompiler *cp, EcNode *np);
static EcNode   *parseReservedNamespace(EcCompiler *cp);
static EcNode   *parseRestParameter(EcCompiler *cp);
static EcNode   *parseResultType(EcCompiler *cp);
static EcNode   *parseReturnStatement(EcCompiler *cp);
static EcNode   *parseSimplePattern(EcCompiler *cp);
static EcNode   *parseSimpleQualifiedName(EcCompiler *cp);
static EcNode   *parseStatement(EcCompiler *cp);
static EcNode   *parseSubstatement(EcCompiler *cp);
static EcNode   *parseSuperInitializer(EcCompiler *cp);
static EcNode   *parseSwitchStatement(EcCompiler *cp);
static EcNode   *parseThrowStatement(EcCompiler *cp);
static EcNode   *parseTryStatement(EcCompiler *cp);
static EcNode   *parseTypeDefinition(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseTypeExpression(EcCompiler *cp);
static EcNode   *parseTypeIdentifierList(EcCompiler *cp);
static EcNode   *parseTypeInitialisation(EcCompiler *cp);
static EcNode   *parseModuleBody(EcCompiler *cp);
static EcNode   *parseModuleName(EcCompiler *cp);
static EcNode   *parseModuleDefinition(EcCompiler *cp);
static EcNode   *parseUsePragma(EcCompiler *cp, EcNode *np);
static EcNode   *parseVariableBinding(EcCompiler *cp, EcNode *varList, EcNode *attributes);
static EcNode   *parseVariableBindingList(EcCompiler *cp, EcNode *list, EcNode *attributes);
static EcNode   *parseVariableDefinition(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseVariableDefinitionKind(EcCompiler *cp, EcNode *attributes);
static EcNode   *parseVariableInitialisation(EcCompiler *cp);
static int       parseVersion(EcCompiler *cp, int parseMax);
static EcNode   *parseWhileStatement(EcCompiler *cp);
static EcNode   *parseWithStatement(EcCompiler *cp);
struct EcNode   *parseXMLAttribute(EcCompiler *cp, EcNode *np);
struct EcNode   *parseXMLAttributes(EcCompiler *cp, EcNode *np);
struct EcNode   *parseXMLElement(EcCompiler *cp, EcNode *np);
struct EcNode   *parseXMLElementContent(EcCompiler *cp, EcNode *np);
struct EcNode   *parseXMLTagContent(EcCompiler *cp, EcNode *np);
struct EcNode   *parseXMLTagName(EcCompiler *cp, EcNode *np);
static EcNode   *parseYieldExpression(EcCompiler *cp);
static int      peekAheadToken(EcCompiler *cp, int ahead);
static EcToken  *peekAheadTokenStruct(EcCompiler *cp, int ahead);
static void     putSpecificToken(EcCompiler *cp, EcToken *token);
static void     putToken(EcCompiler *cp);
static EcNode   *removeNode(EcNode *np, EcNode *child);
static void     setNodeDoc(EcCompiler *cp, EcNode *np);
static void     setId(EcNode *np, char *name);
static EcNode   *unexpected(EcCompiler *cp);
static void     updateTokenInfo(EcCompiler *cp);

#if BLD_DEBUG
/*
 *  Just for debugging. Generated via tokens.ksh
 */
char *tokenNames[] = {
    "",
    "as",
    "assign",
    "at",
    "attribute",
    "bit_and",
    "bit_and_assign",
    "bit_or",
    "bit_or_assign",
    "bit_xor",
    "bit_xor_assign",
    "break",
    "call",
    "case",
    "cast",
    "catch",
    "class",
    "close_tag",
    "colon",
    "colon_colon",
    "comma",
    "const",
    "context_reserved_id",
    "continue",
    "debugger",
    "decimal",
    "decrement",
    "default",
    "delete",
    "div",
    "div_assign",
    "do",
    "dot",
    "dot_dot",
    "dot_less",
    "double",
    "dynamic",
    "each",
    "elipsis",
    "else",
    "enumerable",
    "eof",
    "eq",
    "extends",
    "false",
    "final",
    "finally",
    "float",
    "for",
    "function",
    "ge",
    "get",
    "goto",
    "gt",
    "id",
    "if",
    "implements",
    "import",                   //  UNUSED
    "in",
    "include",
    "increment",
    "instanceof",
    "int",
    "int64",
    "interface",
    "internal",
    "intrinsic",
    "is",
    "lbrace",
    "lbracket",
    "le",
    "let",
    "logical_and",
    "logical_and_assign",
    "logical_not",
    "logical_or",
    "logical_or_assign",
    "logical_xor",
    "logical_xor_assign",
    "lparen",
    "lsh",
    "lsh_assign",
    "lt",
    "minus",
    "minus_assign",
    "minus_minus",
    "mod",
    "module",
    "mod_assign",
    "mul",
    "mul_assign",
    "namespace",
    "native",
    "ne",
    "new",
    "null",
    "number",
    "open_tag",
    "override",
    "package",              //  UNUSED
    "plus",
    "plus_assign",
    "plus_plus",
    "private",
    "protected",
    "prototype",
    "public",
    "query",
    "rbrace",
    "rbracket",
    "readonly",
    "return",
    "rounding",             //  UNUSED
    "rparen",
    "rsh",
    "rsh_assign",
    "rsh_zero",
    "rsh_zero_assign",
    "semicolon",
    "set",
    "nspace",
    "standard",
    "static",
    "strict",
    "strict_eq",
    "strict_ne",
    "string",
    "super",
    "switch",
    "sync",
    "this",
    "throw",
    "tilde",
    "to",
    "true",
    "try",
    "type",
    "typeof",
    "uint",
    "use",
    "var",
    "void",
    "while",
    "with",
    "xml",                  //  UNUSED
    "yield",
    "early",
    "enum",
    "has",
    "precision",            //  UNUSED
    "undefined",
    "boolean",
    "long",
    "volatile",
    "ulong",
    "hash",
    "abstract",
    "callee",
    "generator",
    "number",
    "UNUSED-unit",
    "xml_comment_start",
    "xml_comment_end",
    "cdata_start",
    "cdata_end",
    "xml_pi_start",
    "xml_pi_end",
    "lt_slash",
    "slash_gt",
    "like",
    "regexp",
    "language",
    "require",
    "nop",
    "err",
    0,
};


/*
 *  Just for debugging. Generated via tokens.ksh
 */
char *nodes[] = {
    "",
    "n_value",
    "n_literal",
    "N_qname",
    "n_import",                     //  UNUSED
    "n_binary_type_op",
    "n_binary_op",
    "n_assign_op",
    "n_number_type",
    "n_unary_op",
    "n_if",
    "n_var_definition",
    "n_pragma",
    "N_namespace_definition",
    "n_function",
    "n_parameter",
    "n_class",
    "n_package",                    //  UNUSED
    "n_directives",
    "n_type",                       //  UNUSED
    "n_program",
    "n_packages",                   //  UNUSED
    "n_expressions",
    "n_pragmas",
    "n_type_identifiers",
    "n_block",
    "n_dot",
    "n_return",
    "n_call",
    "n_args",
    "n_this",
    "n_new",
    "n_for",
    "n_for_in",
    "n_postfix_op",
    "n_super",
    "n_try",
    "n_catch",
    "n_catch_clauses",
    "n_throw",
    "n_end_function",
    "n_nop",
    "n_ref",
    "n_switch",
    "n_case_label",
    "n_case_elements",
    "n_break",
    "n_continue",
    "n_goto",
    "n_use_namespace",
    "n_attributes",
    "n_do",
    "n_module",
    "n_use_module",
    "n_void",
    "n_hash",
    "n_object_literal",
    "n_field",
    "n_array_literal",
    "n_catch_arg",
    "n_with",
    0,
};


#endif  /* BLD_DEBUG */

/*
 *  Create a compiler instance
 */

EcCompiler *ecCreateCompiler(Ejs *ejs, int flags, int langLevel)
{
    EcCompiler      *cp;

    cp = mprAllocObjWithDestructorZeroed(ejs, EcCompiler, NULL);
    if (cp == 0) {
        return 0;
    }

    cp->ejs = ejs;
    cp->defaultMode = PRAGMA_MODE_STANDARD;
    cp->lang = langLevel;
    cp->tabWidth = EC_TAB_WIDTH;
    cp->warnLevel = 1;
    cp->shbang = 1;
    cp->optimizeLevel = 9;
    cp->warnLevel = 1;

    if (flags & EC_FLAGS_BIND) {
        cp->bind = 1;
    }
    if (flags & EC_FLAGS_DEBUG) {
        cp->debug = 1;
    }
    if (flags & EC_FLAGS_EMPTY) {
        cp->empty = 1;
    }
    if (flags & EC_FLAGS_RUN) {
        cp->run = 1;
    }
    if (flags & EC_FLAGS_MERGE) {
        cp->merge = 1;
    }
    if (flags & EC_FLAGS_NO_OUT) {
        cp->noout = 1;
    }
    if (ecResetModuleList(cp) < 0) {
        mprFree(cp);
        return 0;
    }
    cp->lexer = ecCreateLexer(cp);
    if (cp->lexer == 0) {
        mprFree(cp);
        return 0;
    }
    ecResetParser(cp);
    return cp;
}


int ecCompile(EcCompiler *cp, int argc, char **argv, int flags)
{
    int     rc, old;

    cp->ejs->flags |= EJS_FLAG_COMPILER;
    old = ejsEnableGC(cp->ejs, 0);
    rc = compileInner(cp, argc, argv, flags);
    ejsEnableGC(cp->ejs, old);
    cp->ejs->flags &= ~EJS_FLAG_COMPILER;
    return rc;
}


static int compileInner(EcCompiler *cp, int argc, char **argv, int flags)
{
    Ejs         *ejs;
    EjsModule   *mp;
    EcNode      **nodes;
    EjsBlock    *block;
    MprList     *modules;
    cchar       *ext;
    int         saveFlags, i, j, next;

    ejs = cp->ejs;
    saveFlags = ejs->flags;
    ejs->flags |= EJS_FLAG_COMPILER;

    nodes = (EcNode**) mprAllocZeroed(cp, sizeof(EcNode*) * argc);
    if (nodes == 0) {
        return EJS_ERR;
    }

    /*
     *  Warn about source files mentioned multiple times.
     */
    for (i = 0; i < argc; i++) {
        for (j = 0; j < argc; j++) {
            if (i == j) {
                continue;
            }
            if (strcmp(argv[i], argv[j]) == 0) {
                parseError(cp, "Loading source %s multiple times. Ignoring extra copies.", argv[i]);
                return EJS_ERR;
            }
        }
        if (cp->outputFile && strcmp(cp->outputFile, argv[i]) == 0) {
            parseError(cp, "Output file is the same as input file: %s", argv[i]);
            return EJS_ERR;
        }
    }

    /*
     *  Compile source files and load any module files
     */
    for (i = 0; i < argc && !cp->fatalError; i++) {
        ext = getExt(argv[i]);

        if (mprStrcmpAnyCase(ext, EJS_MODULE_EXT) == 0 || mprStrcmpAnyCase(ext, BLD_SHOBJ) == 0) {
            if ((ejsLoadModule(cp->ejs, argv[i], 0, 0, EJS_MODULE_DONT_INIT, &modules)) < 0) {
                parseError(cp, "Can't load module file %s\n%s", argv[i], ejsGetErrorMsg(cp->ejs, 0));
                return EJS_ERR;
            }
            if (cp->merge) {
                /*
                 *  If merging, we must emit the loaded module into the output. So add to the compiled modules list.
                 */
                for (next = 0; (mp = mprGetNextItem(modules, &next)) != 0; ) {
                    if (mprLookupItem(cp->modules, mp) < 0 && mprAddItem(cp->modules, mp) < 0) {
                        parseError(cp, "Can't add module %s", mp->name);
                    }
                }
            }
            mprFree(modules);
            nodes[i] = 0;

        } else  {
            parseFile(cp, argv[i], &nodes[i]);
        }
    }

    /*
     *  Allocate the eval frame stack. This is used for property lookups. We have one dummy block at the top always.
     */
    block = ejsCreateBlock(ejs, 0);
    ejsSetDebugName(block, "Compiler");
    ejsPushBlock(ejs, block);
    
    /*
     *  Process the internal representation and generate code
     */
    if (!cp->parseOnly && cp->errorCount == 0) {

        ecResetParser(cp);
        if (ecAstProcess(cp, argc, nodes) < 0) {
            ejsPopBlock(ejs);
            mprFree(nodes);
            return EJS_ERR;
        }
        if (cp->errorCount == 0) {
            ecResetParser(cp);
            if (ecCodeGen(cp, argc, nodes) < 0) {
                ejsPopBlock(ejs);
                mprFree(nodes);
                return EJS_ERR;
            }
        }
    }
    ejsPopBlock(ejs);
    mprFree(nodes);
    if (cp->errorCount > 0) {
        return EJS_ERR;
    }

    ejs->flags = saveFlags;

    /*
     *  Add compiled modules to the interpreter
     */
    for (next = 0; ((mp = (EjsModule*) mprGetNextItem(cp->modules, &next)) != 0); ) {
        ejsAddModule(cp->ejs, mp);
    }
    return 0;
}


/*
 *  Compile the input stream and parse all directives into the given nodes reference.
 *  path is optional.
 */
static int compileInput(EcCompiler *cp, EcNode **nodes, cchar *path)
{
    EcNode      *np;

    mprAssert(cp);
    mprAssert(nodes);

    *nodes = 0;

    if (ecEnterState(cp) < 0) {
        return EJS_ERR;
    }

    /*
     *  Alias for convenient access. 
     */
    cp->input = cp->lexer->input;
    cp->token = cp->lexer->input->token;

    cp->fileState = cp->state;
    cp->fileState->mode = cp->defaultMode;
    cp->blockState = cp->state;

    if (cp->shbang) {
        if (getToken(cp) == T_HASH && peekToken(cp) == T_LOGICAL_NOT) {
            while (cp->token->lineNumber <= 1 && cp->token->tokenId != T_EOF && cp->token->tokenId != T_NOP) {
                getToken(cp);
            }
        }
        putToken(cp);
    }

    np = parseProgram(cp, path);
    mprAssert(np || cp->error);
    np = ecLeaveStateWithResult(cp, np);
    *nodes = np;
    cp->fileState = 0;

    if (np == 0 || cp->errorCount > 0) {
        return EJS_ERR;
    }
    return 0;
}


/*
 *  Compile a source file and parse all directives into the given nodes reference.
 *  This may be called with the input stream already setup to parse a script.
 */
static int parseFile(EcCompiler *cp, char *path, EcNode **nodes)
{
    int         rc, opened;

    mprAssert(path);
    mprAssert(nodes);

    opened = 0;
    path = mprGetNormalizedPath(cp, path);
    if (cp->lexer->input->stream == 0) {
        if (ecOpenFileStream(cp->lexer, path) < 0) {
            parseError(cp, "Can't open %s", path);
            mprFree(path);
            return EJS_ERR;
        }
        opened = 1;
    }
    rc = compileInput(cp, nodes, path);
    if (opened) {
        ecCloseStream(cp->lexer);
    }
    mprFree(path);
    return rc;
}


/*
 *  Lookup a module of the right version
 *  If max is <= 0, then accept any version from min upwards.
 *  This allows the caller to provide -1, -1 to match all versions.
 *  If both are equal, then only that version is acceptable.
 */
EjsModule *ecLookupModule(EcCompiler *cp, cchar *name, int minVersion, int maxVersion)
{
    EjsModule   *mp, *best;
    int         next;

    if (maxVersion <= 0) {
        maxVersion = MAXINT;
    }
    best = 0;
    for (next = 0; (mp = (EjsModule*) mprGetNextItem(cp->modules, &next)) != 0; ) {
        if (minVersion <= mp->version && mp->version <= maxVersion) {
            if (strcmp(mp->name, name) == 0) {
                if (best == 0 || best->version < mp->version) {
                    best = mp;
                }
            }
        }
    }
    return best;
}


int ecAddModule(EcCompiler *cp, EjsModule *mp)
{
    mprAssert(cp->modules);
    return mprAddItem(cp->modules, mp);
}


int ecRemoveModule(EcCompiler *cp, EjsModule *mp)
{
    mprAssert(cp->modules);
    return mprRemoveItem(cp->modules, mp);
}


int ecResetModuleList(EcCompiler *cp)
{
    mprFree(cp->modules);
    cp->modules = mprCreateList(cp);
    if (cp->modules == 0) {
        return EJS_ERR;
    }
    return 0;
}


void ecResetParser(EcCompiler *cp)
{
    cp->token = 0;
}


/*
 *  XMLComment (ECMA-357)
 *
 *  Input Sequences
 *      <!-- XMLCommentCharacters -->
 *
 *  AST
 */
static EcNode *parseXMLComment(EcCompiler *cp)
{
    EcNode  *np;

    ENTER(cp);

    np = 0;
    if (getToken(cp) == T_XML_COMMENT_START) { }
    if (getToken(cp) != T_XML_COMMENT_END) {
        return expected(cp, "-->");
    }
    return LEAVE(cp, np);
}


/*
 *  XMLCdata (ECMA-357)
 *
 *  Input Sequences
 *      <![CDATA[ XMLCDataCharacters ]]>
 *
 *  AST
 */
static EcNode *parseXMLCdata(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = 0;
    if (getToken(cp) == T_CDATA_START) { }
    if (getToken(cp) != T_CDATA_END) {
        return expected(cp, "]]>");
    }
    return LEAVE(cp, np);
}


/*
 *  XMLPI (ECMA-357)
 *
 *  Input Sequences
 *      <? .... ?>
 *
 *  AST
 */
static EcNode *parseXMLPI(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = 0;
    if (getToken(cp) == T_XML_PI_START) { }
    if (getToken(cp) != T_XML_PI_END) {
        return expected(cp, "?>");
    }
    return LEAVE(cp, np);
}


/*
 *  XMLMarkup (ECMA-357)
 *      XMLComment
 *      XMLCDATA
 *      XMLPI
 *
 *  Input Sequences
 *      <!--
 *      [CDATA
 *      <?
 *
 *  AST
 */
static EcNode *parseXMLMarkup(EcCompiler *cp, EcNode *np)
{
    switch (peekToken(cp)) {
    case T_XML_COMMENT_START:
        return parseXMLComment(cp);

    case T_CDATA_START:
        return parseXMLCdata(cp);

    case T_XML_PI_START:
        return parseXMLPI(cp);

    default:
        getToken(cp);
        return LEAVE(cp, unexpected(cp));
    }
}


/*
 *  XMLText (ECMA-357)
 *
 *  Input Sequences
 *      SourceCharacters but no { or <
 *
 *  AST
 */
static EcNode *parseXMLText(EcCompiler *cp, EcNode *np)
{
    uchar   *p;
    int     count;

    peekToken(cp);
    for (count = 0; np; count++) {
        for (p = cp->peekToken->text; p && *p; p++) {
            if (*p == '{' || *p == '<') {
                if (cp->peekToken->text < p) {
                    mprPutBlockToBuf(np->literal.data, (cchar*) cp->token->text, p - cp->token->text);
                    mprAddNullToBuf(np->literal.data);
                    if (getToken(cp) == T_EOF || cp->token->tokenId == T_ERR || cp->token->tokenId == T_NOP) {
                        return 0;
                    }
                }
                return np;
            }
        }
        if (getToken(cp) == T_EOF || cp->token->tokenId == T_ERR || cp->token->tokenId == T_NOP) {
            return 0;
        }
        if (isalnum(cp->token->text[0]) && count > 0) {
            mprPutCharToBuf(np->literal.data, ' ');
        }
        addTokenToBuf(cp, np);
        peekToken(cp);
    }
    return np;
}


/*
 *  XMLName (ECMA-357)
 *      XMLNameStart
 *      XMLName XMLNamePart
 *
 *  Input Sequences
 *      UnicodeLetter
 *      _       underscore
 *      :       colon
 *
 *  AST
 */
static EcNode *parseXMLName(EcCompiler *cp, EcNode *np)
{
    int         c;

    ENTER(cp);

    getToken(cp);
    if (cp->token == 0 || cp->token->text == 0) {
        return LEAVE(cp, unexpected(cp));
    }
    c = cp->token->text[0];
    if (isalpha(c) || c == '_' || c == ':') {
        addTokenToBuf(cp, np);
    } else {
        np = parseError(cp, "Not an XML Name \"%s\"", cp->token->text);
    }
    return LEAVE(cp, np);
}


/*
 *  XMLAttributeValue (ECMA-357)
 *      XMLDoubleStringCharacters
 *      XMLSingleStringCharacters
 *
 *  Input Sequences
 *      "
 *      '
 *
 *  AST
 *      Add data to literal.data buffer
 */
static EcNode *parseXMLAttributeValue(EcCompiler *cp, EcNode *np)
{
    if (getToken(cp) != T_STRING) {
        return expected(cp, "quoted string");
    }
    mprPutCharToBuf(np->literal.data, '\"');
    addTokenToBuf(cp, np);
    mprPutCharToBuf(np->literal.data, '\"');
    return np;
}


/*
 *  Identifier (1)
 *      ID |
 *      ContextuallyReservedIdentifier
 *
 *  Input Sequences
 *      ID
 *      ContextuallyReservedIdentifier
 *
 *  AST
 *      N_QNAME
 *          name
 *              id
 */

static EcNode *parseIdentifier(EcCompiler *cp)
{
    EcNode      *np;
    int         tid;

    ENTER(cp);

    tid = getToken(cp);
    if (cp->token->groupMask & G_CONREV) {
        tid = T_ID;
    }

    switch (tid) {
    case T_ID:
    case T_MUL:
        np = createNode(cp, N_QNAME);
        setId(np, (char*) cp->token->text);
        break;

    default:
        np = parseError(cp, "Not an identifier \"%s\"", cp->token->text);
    }
    return LEAVE(cp, np);
}


/*
 *  Qualifier (3)
 *      *
 *      Identifier
 *      ReservedNamespace
 *      "StringLiteral"
 *
 *  Input Sequences:
 *      *
 *      ID
 *
 *  AST
 *      N_ATTRIBUTES
 *          namespace
 */

static EcNode *parseQualifier(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_ID:
        np = parseIdentifier(cp);
        break;

    case T_STRING:
        getToken(cp);
        np = createNode(cp, N_QNAME);
        np->qname.space = mprStrdup(np, (char*) cp->token->text);
        np->literalNamespace = 1;
        break;

    case T_MUL:
        getToken(cp);
        np = createNode(cp, N_ATTRIBUTES);
        np->qname.space = mprStrdup(np, (char*) cp->token->text);
        break;

    case T_RESERVED_NAMESPACE:
        np = parseReservedNamespace(cp);
        break;

    default:
        getToken(cp);
        np = unexpected(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  ReservedNamespace (6)
 *      internal
 *      intrinsic
 *      private
 *      protected
 *      public
 *
 *  Input
 *      internal
 *      intrinsic
 *      private
 *      protected
 *      public
 *
 *  AST
 *      N_ATTRIBUTES
 *          namespace
 *          attributes
 */
static EcNode *parseReservedNamespace(EcCompiler *cp)
{
    EcNode      *np;
    int         attributes;

    ENTER(cp);

    if (getToken(cp) != T_RESERVED_NAMESPACE) {
        return LEAVE(cp, expected(cp, "reserved namespace"));
    }

    np = createNode(cp, N_ATTRIBUTES);

    attributes = 0;

    switch (cp->token->subId) {
    case T_INTRINSIC:
        break;

    case T_INTERNAL:
    case T_PRIVATE:
    case T_PROTECTED:
    case T_PUBLIC:
        np->specialNamespace = 1;
        break;

    default:
        return LEAVE(cp, parseError(cp, "Unknown reserved namespace %s", cp->token->text));
    }
    np->attributes = attributes;
    np->qname.space = mprStrdup(np, (char*) cp->token->text);
    return LEAVE(cp, np);
}


/*
 *  QualifiedNameIdentifier (11)
 *      Identifier
 *      ReservedIdentifier
 *      StringLiteral
 *      NumberLiteral
 *      Brackets
 *      OverloadedOperator
 *
 *  Notes:
 *      Can be used to the right of a namespace qualifier. Eg. public::QualfiedNameIdentifier
 *
 *  Input
 *      Identifier
 *      ReservedIdentifier
 *      Number
 *      "String"
 *      [
 *      Overloaded Operator
 *
 *  AST
 *      N_QNAME
 *          name:
 *              namespace
 *              id
 *          left: N_EXPRESSIONS
 */
static EcNode *parseQualifiedNameIdentifier(EcCompiler *cp)
{
    EcNode      *np;
    EjsVar      *vp;
    int         tid, reservedWord;

    ENTER(cp);

    tid = peekToken(cp);
    reservedWord = (cp->peekToken->groupMask & G_RESERVED);

    if (reservedWord) {
        np = createNode(cp, N_QNAME);
        setId(np, (char*) cp->token->text);

    } else switch (tid) {
        case T_ID:
        case T_TYPE:
            np = parseIdentifier(cp);
            break;

        case T_NUMBER:
            getToken(cp);
            np = createNode(cp, N_QNAME);
            setId(np, (char*) cp->token->text);
            vp = ejsParseVar(cp->ejs, (char*) cp->token->text, -1);
            np->literal.var = vp;
            break;

        case T_STRING:
            getToken(cp);
            np = createNode(cp, N_QNAME);
            setId(np, (char*) cp->token->text);
            vp = (EjsVar*) ejsCreateString(cp->ejs, (char*) cp->token->text);
            np->literal.var = vp;
            break;

        case T_LBRACKET:
            np = parseBrackets(cp);
            break;

        default:
            if (cp->token->groupMask == G_OPERATOR) {
                np = parseOverloadedOperator(cp);
            } else {
                getToken(cp);
                np = unexpected(cp);
            }
            break;
    }
    return LEAVE(cp, np);
}


/*
 *  SimpleQualifiedName (17)
 *      Identifier
 *      Qualifier :: QualifiedNameIdentifier
 *
 *  Notes:
 *      Optionally namespace qualified name
 *
 *  Input
 *      Identifier
 *      *
 *
 *  AST
 *      N_QNAME
 *          name
 *              id
 *          qualifier: N_ATTRIBUTES
 */
static EcNode *parseSimpleQualifiedName(EcCompiler *cp)
{
    EcNode      *np, *name, *qualifier;

    ENTER(cp);

    if (peekToken(cp) == T_MUL || cp->peekToken->tokenId == T_STRING) {

        if (peekAheadToken(cp, 2) == T_COLON_COLON) {
            qualifier = parseQualifier(cp);
            getToken(cp);
            np = parseQualifiedNameIdentifier(cp);
            if (np->kind == N_EXPRESSIONS) {
                name = np;
                np = createNode(cp, N_QNAME);
                np->name.nameExpr = linkNode(np, name);
                np->qname.space = mprStrdup(np, (char*) qualifier->qname.space);
            } else {
                np->qname.space = mprStrdup(np, (char*) qualifier->qname.space);
                np->literalNamespace = 1;
            }
        } else {
            np = parseIdentifier(cp);
        }

    } else {
        np = parseIdentifier(cp);
        if (peekToken(cp) == T_COLON_COLON) {
            getToken(cp);
            qualifier = np;
            np = parseQualifiedNameIdentifier(cp);
            if (np) {
                if (np->kind == N_EXPRESSIONS) {
                    name = np;
                    np = createNode(cp, N_QNAME);
                    np->name.nameExpr = linkNode(np, name);
                }
                np->name.qualifierExpr = linkNode(np, qualifier);
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ExpressionQualifiedName (15)
 *      ParenListExpression :: QualifiedNameIdentifier
 *
 *  Input
 *      ( ListExpression ) :: *
 *      ( ListExpression ) :: Identifier
 *      ( ListExpression ) :: ReservedIdentifier
 *      ( ListExpression ) :: Number
 *      ( ListExpression ) :: String
 *      ( ListExpression ) :: [ ... ]
 *      ( ListExpression ) :: OverloadedOperator
 *
 *  AST
 *      N_QNAME
 *          left: N_EXPRESSIONS
 *          qualifier: N_ATTRIBUTES
 */
static EcNode *parseExpressionQualifiedName(EcCompiler *cp)
{
    EcNode      *np, *qualifier;

    ENTER(cp);

    if (getToken(cp) != T_LPAREN) {
        return LEAVE(cp, unexpected(cp));
    }
    qualifier = parseListExpression(cp);

    if (getToken(cp) == T_COLON_COLON) {
        np = parseQualifiedNameIdentifier(cp);
        np->name.qualifierExpr = linkNode(np, qualifier);

    } else {
        np = expected(cp, "\"::\"");
    }
    return LEAVE(cp, np);
}


/*
 *  PropertyName (20)
 *      SimpleQualifiedName         |
 *      ExpressionQualifiedName
 *
 *  Input
 *      Identifier
 *      *
 *      internal, intrinsic, private, protected, public
 *      (
 *
 *  AST
 *      N_QNAME
 *          name
 *              namespace
 *              id
 *          left: N_EXPRESSIONS
 *          right: N_EXPRESSIONS
 */
static EcNode *parsePropertyName(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (peekToken(cp) == T_LPAREN) {
        np = parseExpressionQualifiedName(cp);
    } else {
        np = parseSimpleQualifiedName(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  AttributeName (22)
 *      @ Brackets
 *      @ PropertyName
 *
 *  Input
 *      @ [ ... ]
 *      @ PropertyName
 *          @ *
 *          @ ID
 *          @ Qualifier :: *
 *          @ Qualifier :: ID
 *          @ Qualifier :: ReservedIdentifier
 *          @ Qualifier :: Brackets
 *          @ ( ListExpression ) :: *
 *          @ ( ListExpression ) :: ID
 *          @ ( ListExpression ) :: ReservedIdentifier
 *          @ ( ListExpression ) :: [ ... ]
 *
 *  AST
 *      N_QNAME
 *          name
 *              id
 *              namespace
 *              isAttribute
 *          left: N_EXPRESSIONS
 */
static EcNode *parseAttributeName(EcCompiler *cp)
{
    EcNode      *np;
    char        *attribute;

    ENTER(cp);

    if (getToken(cp) != T_AT) {
        return LEAVE(cp, expected(cp, "@ prefix"));
    }
    if (peekToken(cp) == T_LBRACKET) {
        np = createNode(cp, N_QNAME);
        np = appendNode(np, parseBrackets(cp));
    } else {
        np = parsePropertyName(cp);
    }
    if (np && np->kind == N_QNAME) {
        np->name.isAttribute = 1;
        attribute = mprStrcat(np, -1, "@", np->qname.name, NULL);
        mprFree((char*) np->qname.name);
        np->qname.name = attribute;
    }
    return LEAVE(cp, np);
}


/*
 *  QualifiedName (24)
 *      AttributeName
 *      PropertyName
 *
 *  Input
 *      @ ...
 *      Identifier
 *      *
 *      internal, intrinsic, private, protected, public
 *      (
 *
 *  AST
 *      N_QNAME
 *          name
 *              id
 *              namespace
 *              isAttribute
 *          left: listExpression
 *          right: bracketExpression
 */
static EcNode *parseQualifiedName(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (peekToken(cp) == T_AT) {
        np = parseAttributeName(cp);
    } else {
        np = parsePropertyName(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  PrimaryName (26)
 *      Path . PropertyName
 *      PropertyName
 *
 *  Input
 *      *
 *      internal, intrinsic, private, protected, public
 *      Identifier
 *      (
 *
 *  AST
 *      N_QNAME
 *      N_DOT
 */
static EcNode *parsePrimaryName(EcCompiler *cp)
{
    EcNode      *np, *path;
    int         tid, id;

    ENTER(cp);

    np = 0;
    tid = peekToken(cp);
    if (cp->peekToken->groupMask & G_CONREV) {
        tid = T_ID;
    }

    if (tid == T_ID && peekAheadToken(cp, 2) == T_DOT) {
        EcToken     *tok;
        tok = peekAheadTokenStruct(cp, 3);
        id = tok->tokenId;
        if (tok->groupMask & G_CONREV) {
            id = T_ID;
        }
        if (id == T_ID || id == T_MUL || id == T_RESERVED_NAMESPACE || id == T_LPAREN) {
            path = parsePath(cp, 0);
            np = createNode(cp, N_DOT);
            np = appendNode(np, path);
            getToken(cp);
        }
    }

    if (np) {
        np = appendNode(np, parsePropertyName(cp));
    } else {
        np = parsePropertyName(cp);
    }

    return LEAVE(cp, np);
}


/*
 *  Path (28)
 *      Identifier |
 *      Path . Identifier
 *
 *  Input
 *      ID
 *      ID. ... .ID
 *
 *  AST
 *      N_QNAME
 *      N_DOT
 *
 *  "dontConsumeLast" will be set if parsePath should not consume the last Identifier.
 */
static EcNode *parsePath(EcCompiler *cp, EcNode *lhs)
{
    EcNode      *np;
    EcToken     *tok;
    int         tid;

    ENTER(cp);

    if (lhs) {
        np = appendNode(createNode(cp, N_DOT), lhs);
        np = appendNode(np,  parseIdentifier(cp));
    } else {
        np = parseIdentifier(cp);
    }

    /*
     *  parsePath is called only from parsePrimaryName which requires that a ".PropertyName" be preserved.
     */
    if (peekToken(cp) == T_DOT && peekAheadToken(cp, 2) == T_ID) {
        if (peekAheadToken(cp, 3) == T_DOT) {
            tok = peekAheadTokenStruct(cp, 4);
            tid = tok->tokenId;
            if (tok->groupMask & G_CONREV) {
                tid = T_ID;
            }
            if (tid == T_ID || tid == T_MUL || tid == T_RESERVED_NAMESPACE || tid == T_LPAREN) {
                getToken(cp);
                np = parsePath(cp, np);
            }
        }
    }

    return LEAVE(cp, np);
}


/*
 *  ParenListExpression (31)
 *      ( ListExpression )
 *
 *  Input
 *      (
 *
 *  AST
 *      N_EXPRESSIONS
 */
static EcNode *parseParenListExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_LPAREN) {
        return LEAVE(cp, expected(cp, "("));
    }

    np = parseListExpression(cp);

    if (getToken(cp) != T_RPAREN) {
        np = expected(cp, ")");
    }
    return LEAVE(cp, np);
}


/*
 *  FunctionExpression (32)
 *      function Identifier FunctionSignature FunctionBody
 *      function FunctionSignature FunctionBody
 *
 *  Input
 *      function id ( args ) { body }
 *      function ( args ) { body }
 *
 *  AST
 *      N_FUNCTION
 */
static EcNode *parseFunctionExpression(EcCompiler *cp)
{
    EcState     *state;
    EcNode      *np, *funRef;

    ENTER(cp);

    state = cp->state;

    if (getToken(cp) != T_FUNCTION) {
        return LEAVE(cp, unexpected(cp));
    }

    np = createNode(cp, N_FUNCTION);

    if (peekToken(cp) == T_ID) {
        getToken(cp);
        np->qname.name = mprStrdup(np, (char*) cp->token->text);
    }
    if (np->qname.name == 0) {
        /*
         *  Create a property name for the function expression.
         */
        np->qname.name = mprAsprintf(np, -1, "--fun_%d--", np->seqno);
    }

    np->qname.space = mprStrdup(np, state->inFunction ? EJS_PRIVATE_NAMESPACE: cp->fileState->namespace);

    np = parseFunctionSignature(cp, np);
    if (np == 0) {
        return LEAVE(cp, np);
    }
    if (STRICT_MODE(cp)) {
        if (np->function.resultType == 0) {
            return LEAVE(cp, parseError(cp,
                "Function has not defined a return type. Fuctions must be typed when declared in strict mode"));
        }
    }

    cp->state->currentFunctionNode = np;
    np->function.body = linkNode(np, parseFunctionExpressionBody(cp));
    mprStealBlock(np, np->function.body);

    if (np->function.body == 0) {
        return LEAVE(cp, 0);
    }

    /*
     *  The function must get linked into the top var block. It must not get processed inline at this point in the AST tree.
     */
    mprAssert(cp->state->topVarBlockNode);
    appendNode(cp->state->topVarBlockNode, np);

    /*
     *  Create a name node to reference the function. This is the value of this function expression.
     *  The funRef->name will be filled in by the AST processing for the function node.
     */
    funRef = createNode(cp, N_QNAME);
    funRef->qname = ejsCopyName(funRef, &np->qname);
    return LEAVE(cp, funRef);
}


/*
 *  FunctionExpressionBody (34)
 *      Block
 *      AssignmentExpression
 *
 *  Input
 *      {
 *  AST
 */
static EcNode *parseFunctionExpressionBody(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (peekToken(cp) == T_LBRACE) {
        np = parseBlock(cp);
        if (np) {
            np = np->left;
        }

    } else {
        np = parseAssignmentExpression(cp);
    }
    if (np) {
        mprAssert(np->kind == N_DIRECTIVES);
    }

    np = appendNode(np, createNode(cp, N_END_FUNCTION));

    return LEAVE(cp, np);
}


/*
 *  ObjectLiteral (36)
 *      { FieldList }
 *      { FieldList } : NullableTypeExpression
 *
 *  Input
 *      { LiteralField , ... }
 *
 *  AST
 *      N_EXPRESSIONS
 */
static EcNode *parseObjectLiteral(EcCompiler *cp)
{
    EcNode  *typeNode, *np;

    ENTER(cp);

    np = createNode(cp, N_OBJECT_LITERAL);

    if (getToken(cp) != T_LBRACE) {
        return LEAVE(cp, unexpected(cp));
    }

    np = parseFieldList(cp, np);
    if (np == 0) {
        return LEAVE(cp, 0);
    }

    if (peekToken(cp) == T_COLON) {
        getToken(cp);
        typeNode = parseNullableTypeExpression(cp);

    } else {
        /*
         *  Defaults to Object type
         */
        typeNode = createNode(cp, N_QNAME);
        setId(typeNode, (char*) cp->ejs->objectType->qname.name);
    }
    np->objectLiteral.typeNode = linkNode(np, typeNode);

    if (getToken(cp) != T_RBRACE) {
        return LEAVE(cp, unexpected(cp));
    }

    return LEAVE(cp, np);
}


/*
 *  FieldList (41)
 *      EMPTY
 *      LiteralField
 *      LiteralField , LiteralField
 *
 *  Input
 *      LiteralField , ...
 *
 *  AST
 */
static EcNode *parseFieldList(EcCompiler *cp, EcNode *np)
{
    EcNode      *elt;

    ENTER(cp);

    while (peekToken(cp) != T_RBRACE) {
        elt = parseLiteralField(cp);
        if (elt) {
            np = appendNode(np, elt);
        }
        if (peekToken(cp) != T_COMMA) {
            break;
        }
        getToken(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  LiteralField (42)
 *      FieldKind FieldName : AssignmentExpression
 *      get Identifier FunctionSignature FunctionBody
 *      set Identifier FunctionSignature FunctionBody
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseLiteralField(EcCompiler *cp)
{
    EcNode  *fp, *np, *id, *funRef;

    ENTER(cp);

    np = 0;

    if (peekToken(cp) == T_GET || cp->peekToken->tokenId == T_SET) {
        fp = createNode(cp, N_FUNCTION);
        if (getToken(cp) == T_GET) {
            fp->function.getter = 1;
            fp->attributes |= EJS_ATTR_GETTER | EJS_ATTR_LITERAL_GETTER;
        } else {
            fp->function.setter = 1;
            fp->attributes |= EJS_ATTR_SETTER;
        }
        id = parseIdentifier(cp);
        if (id == 0) {
            return LEAVE(cp, 0);
        }
        if (fp->function.getter) {
            fp->qname.name = mprStrdup(fp, id->qname.name);
        } else {
            fp->qname.name = mprStrcat(fp, -1, "set-", id->qname.name, NULL);
        }
        fp->qname.space = mprStrdup(fp, "");

        cp->state->currentFunctionNode = fp;

        fp = parseFunctionSignature(cp, fp);
        fp->function.body = linkNode(fp, parseFunctionBody(cp, fp));
        if (fp->function.body == 0) {
            return LEAVE(cp, 0);
        }

        /*
         *  The function must get linked into the current var block. It must not get processed inline at
         *  this point in the AST tree.
         */
        mprAssert(cp->state->topVarBlockNode);
        appendNode(cp->state->topVarBlockNode, fp);

        np = createNode(cp, N_FIELD);
        np->field.fieldKind = FIELD_KIND_FUNCTION;

        funRef = createNode(cp, N_QNAME);
        funRef->qname = ejsCopyName(funRef, &fp->qname);
        np->field.fieldName = linkNode(np, funRef);

    } else {
        if (getToken(cp) == T_CONST) {
            np = createNode(cp, N_FIELD);
            np->field.varKind = KIND_CONST;
        } else {
            putToken(cp);
            np = createNode(cp, N_FIELD);
        }
        np->field.fieldKind = FIELD_KIND_VALUE;
        np->field.fieldName = linkNode(np, parseFieldName(cp));
        if (getToken(cp) != T_COLON) {
            return LEAVE(cp, expected(cp, ":"));
        }
        np->field.expr = linkNode(np, parseAssignmentExpression(cp));
    }

    return LEAVE(cp, np);
}


#if ROLLED_UP
/*
 *  FieldKind (45)
 *      EMPTY
 *      const
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseFieldKind(EcCompiler *cp, EcNode *np)
{
    EcNode  *np;

    ENTER(cp);

    if (peekToken(cp) == T_CONST) {
        getToken(cp);
        np->def.varKind = KIND_CONST;
    }
    return LEAVE(cp, np);
}
#endif


/*
 *  FieldName (47)
 *      PropertyName
 *      StringLiteral
 *      NumberLiteral
 *      ReservedIdentifier
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseFieldName(EcCompiler *cp)
{
    EcNode  *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_ID:
    case T_NUMBER:
    case T_STRING:
        np = parsePrimaryExpression(cp);
        break;

    default:
        np = parsePropertyName(cp);
        break;
    }
    return LEAVE(cp, np);
}


/*
 *  ArrayLiteral (51)
 *      [ Elements ]
 *
 *  ArrayLiteral (52)
 *      [ Elements ] : NullableTypeExpression
 *
 *  Input sequence
 *      [
 *
 *  AST
 *      N_EXPRESSIONS
 *          N_NEW
 *              N_QNAME
 *          N_EXPRESSIONS
 *              N_ASSIGN_OP
 *                  N_DOT
 *                      N_PARENT
 *                      N_LITERAL
 *                  ANY
 */
static EcNode *parseArrayLiteral(EcCompiler *cp)
{
    EjsType     *type;
    EcNode      *np, *typeNode, *elementsNode, *newNode, *lit;

    ENTER(cp);

    typeNode = 0;
    newNode = createNode(cp, N_NEW);

    if (getToken(cp) != T_LBRACKET) {
        np = parseError(cp, "Expecting \"[\"");

    } else {
        np = parseElements(cp, newNode);
        if (getToken(cp) != T_RBRACKET) {
            np = parseError(cp, "Expecting \"[\"");

        } else {
            if (peekToken(cp) == T_COLON) {
                typeNode = parseArrayType(cp);
                if (typeNode == 0) {
                    return LEAVE(cp, 0);
                }
            }
        }

        if (np) {
            elementsNode = np;

            if (typeNode == 0) {
                /*
                 *  Defaults to Array type
                 */
                type = (EjsType*) cp->ejs->arrayType;
                mprAssert(type);
                typeNode = createNode(cp, N_QNAME);
                mprAssert(typeNode);
                setId(typeNode, (char*) type->qname.name);
            }

            newNode = appendNode(newNode, typeNode);
            np = createNode(cp, N_EXPRESSIONS);
            np = appendNode(np, newNode);
            np = appendNode(np, elementsNode);
        }
    }
    lit = createNode(cp, N_ARRAY_LITERAL);
    np = appendNode(lit, np);
    return LEAVE(cp, np);
}


/*
 *  Elements (54)
 *      ElementList
 *      ElementComprehension
 *
 *  Input sequence
 *
 *
 *  AST
 *      N_EXPRESSIONS
 *          N_ASSIGN_OP
 *              N_DOT
 *                  N_REF
 *                  N_LITERAL
 *              ANY
 */
static EcNode *parseElements(EcCompiler *cp, EcNode *newNode)
{
    EcNode      *np;

    ENTER(cp);

    np = parseElementList(cp, newNode);
    return LEAVE(cp, np);
}


/*
 *  ElementList (56)
 *      EMPTY
 *      LiteralElement
 *      , ElementList
 *      LiteralElement , ElementList
 *
 *  Input sequence
 *
 *  AST
 *      N_EXPRESSIONS
 *          N_ASSIGN_OP
 *              N_DOT
 *                  N_REF
 *                  N_LITERAL
 *              ANY
 */
static EcNode *parseElementList(EcCompiler *cp, EcNode *newNode)
{
    EcNode      *np, *valueNode, *left;
    int         index;

    ENTER(cp);

    np = createNode(cp, N_EXPRESSIONS);
    index = 0;

    do {
        /*
         *  Leading comma, or dual commas means a gap in the indicies
         */
        if (peekToken(cp) == T_COMMA) {
            getToken(cp);
            index++;
            continue;

        } else if (cp->peekToken->tokenId == T_RBRACKET) {
            break;
        }

        valueNode = parseLiteralElement(cp);
        if (valueNode) {
            /*
             *  Update the array index
             */
            mprAssert(valueNode->kind == N_ASSIGN_OP);

            left = valueNode->left;
            mprAssert(left->kind == N_DOT);

            /*
             *  Set the array index
             */
            mprAssert(left->right->kind == N_LITERAL);
            left->right->literal.var = (EjsVar*) ejsCreateNumber(cp->ejs, index);

            /*
             *  Update the reference (array) node reference. This refers to the actual array object.
             */
            mprAssert(left->left->kind == N_REF);
            left->left->ref.node = newNode;
            np = appendNode(np, valueNode);

        } else {
            np = 0;
        }

    } while (np);

    return LEAVE(cp, np);
}


/*
 *  LiteralElement (60)
 *      AssignmentExpression -noList, allowin-
 *
 *  Input sequence
 *
 *  AST
 *      N_ASSIGN_OP
 *          N_DOT
 *              N_REF
 *              N_LITERAL (empty - caller must set node->var)
 *          ANY
 */
static EcNode *parseLiteralElement(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = createNode(cp, N_DOT);

    np = appendNode(np, createNode(cp, N_REF));
    np = appendNode(np, createNode(cp, N_LITERAL));

    parent = createNode(cp, N_ASSIGN_OP);
    np = createAssignNode(cp, np, parseAssignmentExpression(cp), parent);
    if (np == 0) {
        return LEAVE(cp, np);
    }

    /*
     *  To allow multiple literal elements, we must not consume the object on the stack.
     */
    mprAssert(np->kind == N_ASSIGN_OP);

    //  TOD - bad name. Only used by literals
    np->needDupObj = 1;
    return LEAVE(cp, np);
}


/*
 *  OptionalIfCondition (66)
 *      EMPTY
 *      if ParenListExpression
 *
 *  Input
 *      this
 *
 *  AST
 */


/*
 *  XMLInitializer (68)
 *      XMLMarkup
 *      XMLElement
 *      < > XMLElementContent </ >
 *
 *  Input
 *      <!--
 *      [CDATA
 *      <?
 *      <
 *
 *  AST
 *      Add data to literal.data buffer
 *
 */
struct EcNode *parseXMLInitializer(EcCompiler *cp)
{
#if BLD_FEATURE_EJS_E4X
    EcNode      *np;
    EjsVar      *vp;

    ENTER(cp);

    np = createNode(cp, N_LITERAL);
    np->literal.data = mprCreateBuf(np, 0, 0);

    vp = (EjsVar*) ejsCreateXML(cp->ejs, 0, 0, 0, 0);
    np->literal.var = vp;

    switch (peekToken(cp)) {
    case T_XML_COMMENT_START:
    case T_CDATA_START:
    case T_XML_PI_START:
        return parseXMLMarkup(cp, np);

    case T_LT:
        if (peekAheadToken(cp, 2) == T_GT) {
            getToken(cp);
            getToken(cp);
            mprPutStringToBuf(np->literal.data, "<>");
            np = parseXMLElementContent(cp, np);
            if (getToken(cp) != T_LT_SLASH) {
                return LEAVE(cp, expected(cp, "</"));
            }
            if (getToken(cp) != T_GT) {
                return LEAVE(cp, expected(cp, ">"));
            }
            mprPutStringToBuf(np->literal.data, "</>");

        } else {
            return parseXMLElement(cp, np);
        }
        break;

    default:
        getToken(cp);
        return LEAVE(cp, unexpected(cp));
    }
    np = 0;
    return LEAVE(cp, np);
#else
    return parseError(cp, "E4X and XML support is not configured");
#endif
}


/*
 *  XMLElementContent (71)
 *      { ListExpression } XMLElementContent
 *      XMLMarkup XMLElementContent
 *      XMLText XMLElementContent
 *      XMLElement XMLElementContent
 *      EMPTY
 *  Input
 *      {
 *      <!--
 *      [CDATA
 *      <?
 *      <
 *      Text
 *  AST
 */
struct EcNode *parseXMLElementContent(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    if (np == 0) {
        return LEAVE(cp, np);
    }
    
    switch (peekToken(cp)) {
    case T_LBRACE:
        getToken(cp);
        np = parseListExpression(cp);
        if (getToken(cp) != T_RBRACE) {
            return LEAVE(cp, expected(cp, "}"));
        }
        np = parseXMLElementContent(cp, np);
        break;

    case T_XML_COMMENT_START:
    case T_CDATA_START:
    case T_XML_PI_START:
        np = parseXMLMarkup(cp, np);
        break;

    case T_LT:
        np = parseXMLElement(cp, np);
        np = parseXMLElementContent(cp, np);
        break;

    case T_LT_SLASH:
        break;

    case T_EOF:
    case T_ERR:
    case T_NOP:
        return LEAVE(cp, 0);

    default:
        np = parseXMLText(cp, np);
        np = parseXMLElementContent(cp, np);
    }
    return LEAVE(cp, np);
}


/*
 *  XMLElement (76)
 *      < XMLTagContent XMLWhitespace />
 *      < XMLTagContent XMLWhitespace > XMLElementContent </ XMLTagName XMLWhitespace >
 *
 *  Input
 *      <
 *
 *  AST
 *      Add data to literal.data buffer
 *
 */
struct EcNode *parseXMLElement(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    if (getToken(cp) != T_LT) {
        return LEAVE(cp, expected(cp, "<"));
    }
    addTokenToBuf(cp, np);

    np = parseXMLTagContent(cp, np);
    if (np == 0) {
        return LEAVE(cp, np);
    }

    if (getToken(cp) == T_SLASH_GT) {
        addTokenToBuf(cp, np);
        return LEAVE(cp, np);

    } else if (cp->token->tokenId != T_GT) {
        return LEAVE(cp, unexpected(cp));
    }

    addTokenToBuf(cp, np);

    np = parseXMLElementContent(cp, np);
    if (getToken(cp) != T_LT_SLASH) {
        return LEAVE(cp, expected(cp, "</"));
    }
    addTokenToBuf(cp, np);

    np = parseXMLTagName(cp, np);
    if (getToken(cp) != T_GT) {
        return LEAVE(cp, expected(cp, ">"));
    }
    addTokenToBuf(cp, np);
    return LEAVE(cp, np);
}


/*
 *  XMLTagContent (79)
 *      XMLTagName XMLAttributes
 *
 *  Input
 *      {
 *      UnicodeLetter
 *      _       underscore
 *      :       colon
 *
 *  AST
 *      Add data to literal.data buffer
 *
 */
struct EcNode *parseXMLTagContent(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    np = parseXMLTagName(cp, np);
    if (np) {
        np = parseXMLAttributes(cp, np);
    }
    return LEAVE(cp, np);
}


/*
 *  XMLTagName (80)
 *      { ListExpression }
 *      XMLName
 *
 *  Input
 *      {
 *      UnicodeLetter
 *      _       underscore
 *      :       colon
 *
 *  AST
 *      Add data to literal.data buffer
 */
struct EcNode *parseXMLTagName(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    if (np == 0) {
        return LEAVE(cp, np);
    }
    if (peekToken(cp) == T_LBRACE) {
        getToken(cp);
        np = parseListExpression(cp);
        if (getToken(cp) != T_RBRACE) {
            return LEAVE(cp, expected(cp, "}"));
        }
    } else {
        np = parseXMLName(cp, np);
    }
    return LEAVE(cp, np);
}


/*
 *  XMLAttributes (82)
 *      XMLWhitespace { ListExpression }
 *      XMLAttribute XMLAttributes
 *      EMPTY
 *  Input
 *
 *  AST
 *      Add data to literal.data buffer
 */
struct EcNode *parseXMLAttributes(EcCompiler *cp, EcNode *np)
{
    int         tid;

    ENTER(cp);

    tid = peekToken(cp);
    if (tid == T_LBRACE) {
        parseListExpression(cp);
        if (peekToken(cp) == T_RBRACE) {
            return LEAVE(cp, expected(cp, "}"));
        }

    } else {
        while (tid != T_GT && tid != T_SLASH_GT) {
            if ((np = parseXMLAttribute(cp, np)) == 0) {
                break;
            }
            tid = peekToken(cp);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  XMLAttributes (85)
 *      XMLWhitespace XMLName XMLWhitespace = XMLWhitepace { ListExpression (allowIn) }
 *      XMLWhitespace XMLName XMLWhitespace = XMLAttributeValue
 *
 *  Input
 *      UnicodeLetter
 *      _       underscore
 *      :       colon
 *
 *  AST
 *      Add data to literal.data buffer
 *
 */
struct EcNode *parseXMLAttribute(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprPutCharToBuf(np->literal.data, ' ');
    np = parseXMLName(cp, np);

    if (getToken(cp) != T_ASSIGN) {
        return LEAVE(cp, expected(cp, "="));
    }
    mprPutCharToBuf(np->literal.data, '=');

    if (peekToken(cp) == T_LBRACE) {
        np = parseListExpression(cp);
        if (getToken(cp) != T_RBRACE) {
            return LEAVE(cp, expected(cp, "}"));
        }
    } else {
        np = parseXMLAttributeValue(cp, np);
    }
    return LEAVE(cp, np);
}


/*
 *  ThisExpression (87)
 *      this
 *      this callee
 *      this generator
 *      this function
 *  Input
 *      this
 *
 *  AST
 */
static EcNode *parseThisExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_THIS) {
        return LEAVE(cp, unexpected(cp));
    }

    np = createNode(cp, N_THIS);

    switch (peekToken(cp)) {
    case T_TYPE:
        getToken(cp);
        np->thisNode.thisKind = N_THIS_TYPE;
        break;

    case T_FUNCTION:
        getToken(cp);
        np->thisNode.thisKind = N_THIS_FUNCTION;
        break;

    case T_CALLEE:
        getToken(cp);
        np->thisNode.thisKind = N_THIS_CALLEE;
        break;

    case T_GENERATOR:
        getToken(cp);
        np->thisNode.thisKind = N_THIS_GENERATOR;
        break;
    }
    return LEAVE(cp, np);
}


/*
 *  PrimaryExpression (90)
 *      null
 *      true
 *      false
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      ThisExpression
 *      XMLInitializer
 *      ParenListExpression
 *      ArrayLiteral
 *      ObjectLiteral
 *      FunctionExpression
 *      AttributeName
 *      PrimaryName
 *
 *  Input sequence
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      ContextuallyReservedIdentifier
 *      {
 *      [
 *      (
 *      @
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *
 *
 *  AST
 *      N_FUNCTION
 *      N_NEW           (array / object literals)
 *      N_LITERAL
 *      N_QNAME
 *      N_THIS
 *
 */
static EcNode *parsePrimaryExpression(EcCompiler *cp)
{
    EcNode      *np;
    EjsVar      *vp;
    int         tid;

    ENTER(cp);

    tid = peekToken(cp);
    if (cp->peekToken->groupMask & G_CONREV) {
        tid = T_ID;
    }

    np = 0;
    switch (tid) {
    case T_STRING:
        if (peekAheadToken(cp, 2) == T_COLON_COLON) {
            np = parsePrimaryName(cp);
        } else {
            getToken(cp);
            vp = (EjsVar*) ejsCreateString(cp->ejs, (char*) cp->token->text);
            np = createNode(cp, N_LITERAL);
            np->literal.var = vp;
        }
        break;

    case T_ID:
        np = parsePrimaryName(cp);
        break;

    case T_AT:
        np = appendNode(np, parseAttributeName(cp));
        break;

    case T_NUMBER:
        getToken(cp);
        vp = ejsParseVar(cp->ejs, (char*) cp->token->text, -1);
        np = createNode(cp, N_LITERAL);
        np->literal.var = vp;
        break;

    case T_NULL:
        getToken(cp);
        if (cp->empty) {
            np = createNode(cp, N_QNAME);
            setId(np, (char*) cp->token->text);
        } else {
            np = createNode(cp, N_LITERAL);
            np->literal.var = cp->ejs->nullValue;
        }
        break;

    case T_UNDEFINED:
        getToken(cp);
        if (cp->empty) {
            np = createNode(cp, N_QNAME);
            setId(np, (char*) cp->token->text);
        } else {
            np = createNode(cp, N_LITERAL);
            np->literal.var = cp->ejs->undefinedValue;
        }
        break;

    case T_TRUE:
        getToken(cp);
        if (cp->empty) {
            np = createNode(cp, N_QNAME);
            setId(np, (char*) cp->token->text);
        } else {
            np = createNode(cp, N_LITERAL);
            vp = (EjsVar*) ejsCreateBoolean(cp->ejs, 1);
            np->literal.var = vp;
        }
        break;

    case T_FALSE:
        getToken(cp);
        if (cp->empty) {
            np = createNode(cp, N_QNAME);
            setId(np, (char*) cp->token->text);
        } else {
            np = createNode(cp, N_LITERAL);
            vp = (EjsVar*) ejsCreateBoolean(cp->ejs, 0);
            np->literal.var = vp;
        }
        break;

    case T_THIS:
        np = parseThisExpression(cp);
        break;

    case T_LPAREN:
        np = parseParenListExpression(cp);
        break;

    case T_LBRACKET:
        np = parseArrayLiteral(cp);
        break;

    case T_LBRACE:
        np = parseObjectLiteral(cp);
        break;

    case T_FUNCTION:
        np = parseFunctionExpression(cp);
        break;

    case T_VOID:
    case T_NAMESPACE:
    case T_TYPEOF:
        getToken(cp);
        if (cp->empty) {
            np = createNode(cp, N_QNAME);
            setId(np, (char*) cp->token->text);
        } else {
            np = unexpected(cp);
        }
        break;

    case T_LT:
    case T_XML_COMMENT_START:
    case T_CDATA_START:
    case T_XML_PI_START:
        np = parseXMLInitializer(cp);
        break;

    case T_DIV:
        np = parseRegularExpression(cp);
        break;

    case T_ERR:
    default:
        getToken(cp);
        np = unexpected(cp);
        break;
    }
    return LEAVE(cp, np);
}


static EcNode *parseRegularExpression(EcCompiler *cp)
{
    EcNode      *np;
    EjsVar      *vp;
    int         id;

    ENTER(cp);

    /*
     *  Flush peek ahead buffer
     */
    while (cp->input->putBack) {
        getToken(cp);
    }

    id = ecGetRegExpToken(cp->input);
    updateTokenInfo(cp);
    cp->peekToken = 0;
#if BLD_DEBUG
    cp->peekTokenName = 0;
#endif

    if (id != T_REGEXP) {
        return LEAVE(cp, parseError(cp, "Can't parse regular expression"));
    }

#if BLD_FEATURE_REGEXP
    vp = (EjsVar*) ejsCreateRegExp(cp->ejs, (char*) cp->token->text);
    if (vp == 0) {
        return LEAVE(cp, parseError(cp, "Can't compile regular expression"));
    }
#else
    vp = (EjsVar*) cp->ejs->undefinedValue;
#endif

    np = createNode(cp, N_LITERAL);
    np->literal.var = vp;
    return LEAVE(cp, np);
}


/*
 *  SuperExpression (104)
 *      super
 *      super ParenExpression
 *
 *  Input
 *      super
 *
 *  AST
 *      N_SUPER
 *
 *  NOTES:
 *      Using Arguments instead of ParenExpression so we can have multiple args.
 */
static EcNode *parseSuperExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_SUPER) {
        np = unexpected(cp);

    } else {
        if (peekToken(cp) == T_LPAREN) {
            np = createNode(cp, N_SUPER);
            np = appendNode(np, parseArguments(cp));
        } else {
            np = createNode(cp, N_SUPER);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  Arguments (106)
 *      ( )
 *      ( ArgumentList )
 *
 *  Input
 *      (
 *
 *  AST
 *      N_ARGS
 */
static EcNode *parseArguments(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_LPAREN) {
        np = parseError(cp, "Expecting \"(\"");

    } else if (peekToken(cp) == T_RPAREN) {
        getToken(cp);
        np = createNode(cp, N_ARGS);

    } else {
        np = parseArgumentList(cp);
        if (np && getToken(cp) != T_RPAREN) {
            np = parseError(cp, "Expecting \")\"");
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ArgumentList (118)
 *      AssignmentExpression
 *      ArgumentList , AssignmentExpression
 *
 *  Input
 *
 *  AST N_ARGS
 *      children: arguments
 */
static EcNode *parseArgumentList(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = createNode(cp, N_ARGS);
    np = appendNode(np, parseAssignmentExpression(cp));

    while (np && peekToken(cp) == T_COMMA) {
        getToken(cp);
        np = appendNode(np, parseAssignmentExpression(cp));
    }
    return LEAVE(cp, np);
}


/*
 *  PropertyOperator (110)
 *      . ReservedIdentifier
 *      . PropertyName
 *      . AttributeName
 *      .. QualifiedName
 *      . ParenListExpression
 *      . ParenListExpression :: QualifiedNameIdentifier
 *      Brackets
 *      TypeApplication
 *
 *  Input
 *      .
 *      ..
 *      [
 *
 *  AST
 *      N_DOT
 */
static EcNode *parsePropertyOperator(EcCompiler *cp)
{
    EcNode      *np, *name;
    char        *id;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_DOT:
        np = createNode(cp, N_DOT);
        getToken(cp);
        switch (peekToken(cp)) {
        case T_LPAREN:
            np = appendNode(np, parseParenListExpression(cp));
            break;

        case T_TYPE:
        case T_ID:
        case T_STRING:
        case T_RESERVED_NAMESPACE:
        case T_MUL:
            if (cp->token->groupMask & G_RESERVED) {
                np = appendNode(np, parseIdentifier(cp));
            } else {
                np = appendNode(np, parsePropertyName(cp));
            }
            break;

        case T_AT:
            np = appendNode(np, parseAttributeName(cp));
            break;

        case T_SUPER:
            getToken(cp);
            np = appendNode(np, createNode(cp, N_SUPER));
            break;

        default:
            getToken(cp);
            np = unexpected(cp);
            break;
        }
        break;

    case T_LBRACKET:
        np = createNode(cp, N_DOT);
        np = appendNode(np, parseBrackets(cp));
        break;

    case T_DOT_DOT:
        getToken(cp);
        np = createNode(cp, N_DOT);
        name = parseQualifiedName(cp);
        if (name && name->kind == N_QNAME) {
            id = mprStrcat(name, -1, ".", name->qname.name, NULL);
            mprFree((char*) name->qname.name);
            name->qname.name = id;
        }
        np = appendNode(np, name);
        break;

    default:
        getToken(cp);
        np = parseError(cp, "Expecting property operator . .. or [");
        break;
    }
    return LEAVE(cp, np);
}


/*
 *  Brackets (125)
 *      [ ListExpression ]
 *      [ SliceExpression ]
 *
 *  Input
 *      [
 *
 *  AST
 *      N_EXPRESSIONS
 */
static EcNode *parseBrackets(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_LBRACKET) {
        np = parseError(cp, "Expecting \"[\"");
    }

    if (peekToken(cp) == T_COLON) {
        /*
         *  Slice expression
         */
        /* First optional expression in a slice expression is empty */
        np = parseOptionalExpression(cp);
        if (getToken(cp) != T_COLON) {
            np = parseError(cp, "Expecting \":\"");
        }
        np = appendNode(np, parseOptionalExpression(cp));

    } else {

        np = parseListExpression(cp);

        if (peekToken(cp) == T_COLON) {
            /*
             *  Slice expression
             */
            np = appendNode(np, parseOptionalExpression(cp));
            if (peekToken(cp) != T_COLON) {
                getToken(cp);
                np = parseError(cp, "Expecting \":\"");
            }
            np = appendNode(np, parseOptionalExpression(cp));
        }
    }

    if (getToken(cp) != T_RBRACKET) {
        np = parseError(cp, "Expecting \"[\"");
    }
    return LEAVE(cp, np);
}


/*
 *  OptionalExpression (123)
 *      ListExpression -allowin-
 *      EMPTY
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseOptionalExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    np = parseListExpression(cp);
    return LEAVE(cp, np);
}


/*
 *  MemberExpression (125) -a,b-
 *      PrimaryExpression -a,b-
 *      new MemberExpression Arguments
 *      SuperExpression PropertyOperator
 *      MemberExpression PropertyOperator
 *
 *  Input
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      {
 *      [
 *      (
 *      @
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *
 *  AST
 *      N_FUNCTION
 *      N_DOT
 *          left: N_QNAME | N_DOT | N_EXPRESSIONS | N_FUNCTION
 *          right: N_QNAME | N_EXPRESSIONS | N_FUNCTION
 *      N_LITERAL
 *      N_NEW           (array / object literals)
 *      N_QNAME
 *      N_SUPER
 *      N_THIS
 */
static EcNode *parseMemberExpression(EcCompiler *cp)
{
    EcNode      *np, *newNode;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_SUPER:
        np = parseSuperExpression(cp);
        if (peekToken(cp) == T_DOT || cp->peekToken->tokenId == T_DOT_DOT ||
                cp->peekToken->tokenId == T_LBRACKET) {
            np = insertNode(parsePropertyOperator(cp), np, 0);
        }
        break;

    case T_NEW:
        getToken(cp);
        newNode = createNode(cp, N_NEW);
        newNode = appendNode(newNode, parseMemberExpression(cp));
        np = createNode(cp, N_NEW);
        np = appendNode(np, newNode);
        np = appendNode(np, parseArguments(cp));
        break;

    default:
        np = parsePrimaryExpression(cp);
        break;
    }

    while (np && (peekToken(cp) == T_DOT || cp->peekToken->tokenId == T_DOT_DOT ||
            cp->peekToken->tokenId == T_LBRACKET)) {
        np = insertNode(parsePropertyOperator(cp), np, 0);
    }
    return LEAVE(cp, np);
}


/*
 *  CallExpression (129) -a,b-
 *      MemberExpression Arguments
 *      CallExpression Arguments
 *      CallExpression PropertyOperator
 *
 *  Input
 *
 *  AST
 *      N_CALL
 *
 *  "me" is to to an already parsed member expression
 */
static EcNode *parseCallExpression(EcCompiler *cp, EcNode *me)
{
    EcNode      *np;

    ENTER(cp);

    np = 0;

    while (1) {
        peekToken(cp);
        if (me && me->lineNumber != cp->peekToken->lineNumber) {
            return LEAVE(cp, np);
        }
        switch (peekToken(cp)) {
        case T_LPAREN:
            np = createNode(cp, N_CALL);
            np = appendNode(np, me);
            np = appendNode(np, parseArguments(cp));
            if (np && cp->token) {
                np->lineNumber = cp->token->lineNumber;
            }
            break;

        case T_DOT:
        case T_DOT_DOT:
        case T_LBRACKET:
            if (me == 0) {
                getToken(cp);
                return LEAVE(cp, unexpected(cp));
            }
            np = insertNode(parsePropertyOperator(cp), me, 0);
            break;

        default:
            if (np == 0) {
                getToken(cp);
                return LEAVE(cp, unexpected(cp));
            }
            return LEAVE(cp, np);
        }
        if (np == 0) {
            return LEAVE(cp, np);
        }
        me = np;
    }
    return LEAVE(cp, np);
}


/*
 *  NewExpression (132)
 *      MemberExpression
 *      new NewExpression
 *
 *  Input
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      {
 *      [
 *      (
 *      @
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *
 *  AST
 *      N_FUNCTION
 *      N_DOT
 *          left: N_QNAME | N_DOT | N_EXPRESSIONS | N_FUNCTION
 *          right: N_QNAME | N_EXPRESSIONS | N_FUNCTION
 *      N_LITERAL
 *      N_NEW           (array / object literals)
 *      N_QNAME
 *      N_SUPER
 *      N_THIS
 */
static EcNode *parseNewExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (peekToken(cp) == T_NEW) {
        getToken(cp);
        np = createNode(cp, N_NEW);
        np = appendNode(np, parseNewExpression(cp));

    } else {
        np = parseMemberExpression(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  LeftHandSideExpression (134) -a,b-
 *      NewExpression
 *      CallExpression
 *
 *  Where NewExpression is:
 *      MemberExpression
 *      new NewExpression
 *
 *  Where CallExpression is:
 *      MemberExpression Arguments
 *      CallExpression Arguments
 *      CallExpression PropertyOperator
 *
 *  Where MemberExpression is:
 *      PrimaryExpression -a,b-
 *      new MemberExpression Arguments
 *      SuperExpression PropertyOperator
 *      MemberExpression PropertyOperator
 *
 *  So look ahead problem on MemberExpression. We don't know if it is a newExpression or a CallExpression. This requires
 *  large lookahead. So, refactored to be:
 *
 *  LeftHandSideExpression (136) -a,b-
 *      new MemberExpression LeftHandSidePrime
 *      MemberExpression LeftHandSidePrime
 *
 *  and where LeftHandSidePrime is:
 *      Arguments LeftHandSidePrime
 *      PropertyOperator LeftHandSidePrime
 *      EMPTY
 *
 *  Input
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      {
 *      [
 *      (
 *      @
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *
 *  Also:
 *      new MemberExpression
 *      MemberExpression
 *      MemberExpression (
 *      MemberExpression .
 *      MemberExpression ..
 *      MemberExpression [
 *
 *
 *  AST
 *      N_CALL
 *      N_FUNCTION
 *      N_DOT
 *          left: N_QNAME | N_DOT | N_EXPRESSIONS | N_FUNCTION
 *          right: N_QNAME | N_EXPRESSIONS | N_FUNCTION
 *      N_LITERAL
 *      N_NEW           (array / object literals)
 *      N_QNAME
 *      N_SUPER
 *      N_THIS
 */
static EcNode *parseLeftHandSideExpression(EcCompiler *cp)
{
    EcNode      *np, *callNode;

    ENTER(cp);

    if (peekToken(cp) == T_NEW) {
        np = parseNewExpression(cp);

    } else {
        np = parseMemberExpression(cp);
    }

    if (np) {
        /*
         *  Refactored CallExpression processing
         */
        switch (peekToken(cp)) {
        case T_LPAREN:
        case T_DOT:
        case T_DOT_DOT:
        case T_LBRACKET:
            /* CHANGE: was np->lineNumber */
            if (cp->token->lineNumber == cp->peekToken->lineNumber) {
                /*
                 *  May have multiline function expression: x = (function ..... multiple lines)() 
                 */
                np->lineNumber = cp->token->lineNumber;
                np = parseCallExpression(cp, np);
            }
            break;

        default:
            if (np->kind == N_NEW) {
                /*
                 *  Create a dummy call to the constructor
                 */
                callNode = createNode(cp, N_CALL);
                np = appendNode(callNode, np);
                np = appendNode(np, createNode(cp, N_ARGS));
            }
            break;
        }
    }
    return LEAVE(cp, np);
}


/*
 *  UnaryTypeExpression (136)
 *      LeftHandSideExpression
 *      type NullableTypeExpression
 *
 *  Input
 *      type
 *
 *  AST
 */
static EcNode *parseUnaryTypeExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    if (peekToken(cp) == T_TYPE) {
        getToken(cp);
        np = parseNullableTypeExpression(cp);
    } else {
        np = parseLeftHandSideExpression(cp);
    }

    return LEAVE(cp, np);
}


/*
 *  PostfixExpression (138)
 *      UnaryTypeExpression
 *      LeftHandSideExpression [no line break] ++
 *      LeftHandSideExpression [no line break] --
 *
 *  Input
 *
 *  AST
 */
static EcNode *parsePostfixExpression(EcCompiler *cp)
{
    EcNode      *parent, *np;

    ENTER(cp);

    if (peekToken(cp) == T_TYPE) {
        np = parseUnaryTypeExpression(cp);
    } else {
        np = parseLeftHandSideExpression(cp);
        if (np) {
            if (peekToken(cp) == T_PLUS_PLUS || cp->peekToken->tokenId == T_MINUS_MINUS) {
                getToken(cp);
                parent = createNode(cp, N_POSTFIX_OP);
                np = appendNode(parent, np);
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  UnaryExpression (141)
 *      PostfixExpression
 *      delete PostfixExpression
 *      void UnaryExpression
 *      typeof UnaryExpression
 *      ++ PostfixExpression
 *      -- PostfixExpression
 *      + UnaryExpression
 *      - UnaryExpression
 *      ~ UnaryExpression           (bitwise not)
 *      ! UnaryExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseUnaryExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_DELETE:
    case T_LOGICAL_NOT:
    case T_PLUS:
    case T_PLUS_PLUS:
    case T_MINUS:
    case T_MINUS_MINUS:
    case T_TILDE:
    case T_TYPEOF:
    case T_VOID:
        getToken(cp);
        np = createNode(cp, N_UNARY_OP);
        np = appendNode(np, parsePostfixExpression(cp));
        break;

    default:
        np = parsePostfixExpression(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  MultiplicativeExpression (152) -a,b-
 *      UnaryExpression
 *      MultiplicativeExpression * UnaryExpression
 *      MultiplicativeExpression / UnaryExpression
 *      MultiplicativeExpression % UnaryExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseMultiplicativeExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseUnaryExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_MUL:
        case T_DIV:
        case T_MOD:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseUnaryExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  AdditiveExpression (156)
 *      MultiplicativeExpression
 *      AdditiveExpression + MultiplicativeExpression
 *      AdditiveExpression - MultiplicativeExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseAdditiveExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseMultiplicativeExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_PLUS:
        case T_MINUS:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseMultiplicativeExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ShiftExpression (159) -a,b-
 *      AdditiveExpression
 *      ShiftExpression << AdditiveExpression
 *      ShiftExpression >> AdditiveExpression
 *      ShiftExpression >>> AdditiveExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseShiftExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseAdditiveExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_LSH:
        case T_RSH:
        case T_RSH_ZERO:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseAdditiveExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  RelationalExpression (163) -allowin-
 *      ShiftExpression
 *      RelationalExpression < ShiftExpression
 *      RelationalExpression > ShiftExpression
 *      RelationalExpression <= ShiftExpression
 *      RelationalExpression >= ShiftExpression
 *      RelationalExpression [in] ShiftExpression
 *      RelationalExpression instanceOf ShiftExpression
 *      RelationalExpression cast ShiftExpression
 *      RelationalExpression to ShiftExpression
 *      RelationalExpression is ShiftExpression
 *      RelationalExpression like ShiftExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseRelationalExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseShiftExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_IN:
            if (cp->state->noin) {
                return LEAVE(cp, np);
            }
            /* Fall through */

        case T_LT:
        case T_LE:
        case T_GT:
        case T_GE:
        case T_INSTANCEOF:
        case T_IS:
        case T_LIKE:
        case T_CAST:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseShiftExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  EqualityExpression (182)
 *      RelationalExpression
 *      EqualityExpression == RelationalExpression
 *      EqualityExpression != RelationalExpression
 *      EqualityExpression === RelationalExpression
 *      EqualityExpression !== RelationalExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseEqualityExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseRelationalExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_EQ:
        case T_NE:
        case T_STRICT_EQ:
        case T_STRICT_NE:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseRelationalExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  BitwiseAndExpression (187)
 *      EqualityExpression
 *      BitwiseAndExpression & EqualityExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseBitwiseAndExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseEqualityExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_BIT_AND:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseEqualityExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  BitwiseXorExpression (189)
 *      BitwiseAndExpression
 *      BitwiseXorExpression ^ BitwiseAndExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseBitwiseXorExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseBitwiseAndExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_BIT_XOR:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseBitwiseAndExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  BitwiseOrExpression (191)
 *      BitwiseXorExpression
 *      BitwiseOrExpression | BitwiseXorExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseBitwiseOrExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseBitwiseXorExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_BIT_OR:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseBitwiseXorExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  LogicalAndExpression (193)
 *      BitwiseOrExpression
 *      LogicalAndExpression && BitwiseOrExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseLogicalAndExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseBitwiseOrExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_LOGICAL_AND:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseBitwiseOrExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  LogicalOrExpression (195)
 *      LogicalAndExpression
 *      LogicalOrExpression || LogicalOrExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseLogicalOrExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parseLogicalAndExpression(cp);

    while (np) {
        switch (peekToken(cp)) {
        case T_LOGICAL_OR:
            getToken(cp);
            parent = createNode(cp, N_BINARY_OP);
            np = createBinaryNode(cp, np, parseLogicalOrExpression(cp), parent);
            break;

        default:
            return LEAVE(cp, np);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ConditionalExpression (197) -allowList,b-
 *      LetExpression -b-
 *      YieldExpression -b-
 *      LogicalOrExpression -a,b-
 *      LogicalOrExpression -allowList,b-
 *          ? AssignmentExpression : AssignmentExpression
 *
 *  ConditionalExpression (197) -noList,b-
 *      SimpleYieldExpression
 *      LogicalOrExpression -a,b-
 *      LogicalOrExpression -allowList,b-
 *          ? AssignmentExpression : AssignmentExpression
 *
 *  Input
 *      let
 *      yield
 *      (
 *      .
 *      ..
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      delete
 *      {
 *      [
 *      (
 *      @
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *
 *  AST
 *      N_CALL
 *      N_EXPRESSIONS
 *      N_FUNCTION
 *      N_DOT
 *          left: N_QNAME | N_DOT | N_EXPRESSIONS | N_FUNCTION
 *          right: N_QNAME | N_EXPRESSIONS | N_FUNCTION
 *      N_LITERAL
 *      N_NEW           (array / object literals)
 *      N_QNAME
 *      N_SUPER
 *      N_THIS
 */
static EcNode *parseConditionalExpression(EcCompiler *cp)
{
    EcNode      *np, *cond;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_LET:
        np = parseLetExpression(cp);
        break;

    case T_YIELD:
        np = parseYieldExpression(cp);
        break;

    default:
        np = parseLogicalOrExpression(cp);
        if (np) {
            if (peekToken(cp) == T_QUERY) {
                getToken(cp);
                cond = np;
                np = createNode(cp, N_IF);
                np->tenary.cond = linkNode(np, cond);
                np->tenary.thenBlock = linkNode(np, parseAssignmentExpression(cp));
                if (getToken(cp) != T_COLON) {
                    // putToken(cp);
                    np = parseError(cp, "Expecting \":\"");
                } else {
                    np->tenary.elseBlock = linkNode(np, parseAssignmentExpression(cp));
                }
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  NonAssignmentExpression -allowList,b- (199)
 *      LetExpression
 *      YieldExpression
 *      LogicalOrExpression -a,b-
 *      LogicalOrExpression -allowList,b-
 *          ? AssignmentExpression : AssignmentExpression
 *
 *  NonAssignmentExpression -noList,b-
 *      SimpleYieldExpression
 *      LogicalOrExpression -a,b-
 *      LogicalOrExpression -allowList,b-
 *          ? AssignmentExpression : AssignmentExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseNonAssignmentExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_LET:
        np = parseLetExpression(cp);
        break;

    case T_YIELD:
        np = parseYieldExpression(cp);
        break;

    default:
        np = parseLogicalOrExpression(cp);
        if (np) {
            if (peekToken(cp) == T_QUERY) {
                getToken(cp);
                np = parseAssignmentExpression(cp);
                if (getToken(cp) != T_COLON) {
                    // putToken(cp);
                    np = parseError(cp, "Expecting \":\"");
                } else {
                    np = parseAssignmentExpression(cp);
                }
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  LetExpression (204)
 *      let ( LetBindingList ) ListExpression
 *
 *  Input
 *      let
 *
 *  AST
 */
static EcNode *parseLetExpression(EcCompiler *cp)
{
    EcNode  *np;

    if (getToken(cp) != T_LET) {
        return LEAVE(cp, expected(cp, "let"));
    }
    if (getToken(cp) != T_LPAREN) {
        return LEAVE(cp, expected(cp, "("));
    }
    np = parseLetBindingList(cp);
    if (getToken(cp) != T_RPAREN) {
        return LEAVE(cp, expected(cp, ")"));
    }
    return 0;
}


/*
 *  LetBindingList (205)
 *      EMPTY
 *      NonemptyLetBindingList -allowList-
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseLetBindingList(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    np = 0;
    mprAssert(0);
    return LEAVE(cp, np);
}


/*
 *  YieldExpression (209)
 *      yield
 *      yield [no line break] ListExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseYieldExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    np = 0;
    mprAssert(0);
    return LEAVE(cp, np);
}


/*
 *  Rewrite compound assignment. Eg. given x += 3  rewrite as
 *      x = x + 3;
 */
static EcNode *rewriteCompoundAssignment(EcCompiler *cp, int subId, EcNode *lhs, EcNode *rhs)
{
    EcNode      *np, *parent;

    ENTER(cp);

    /*
     *  Map the operator token to its non-assignment counterpart
     */
    np = createNode(cp, N_BINARY_OP);
    np->tokenId = subId - 1;

    np = appendNode(np, lhs);
    np = appendNode(np, rhs);
    parent = createNode(cp, N_ASSIGN_OP);
    np = createAssignNode(cp, lhs, np, parent);

    return LEAVE(cp, np);
}


/*
 *  AssignmentExpression (211)
 *      ConditionalExpression
 *      Pattern -a,b-allowin- = AssignmentExpression -a,b-
 *      SimplePattern -a,b-allowExpr- CompoundAssignmentOperator
 *              AssignmentExpression -a,b-
 *
 *  Where
 *      SimplePattern is:
 *          LeftHandSideExpression -a,b-
 *          Identifier
 *      ConditionalExpression is:
 *          LetExpression -b-
 *          YieldExpression -b-
 *          LogicalOrExpression -a,b-
 *
 *  Input
 *      (
 *      .
 *      ..
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      {
 *      [
 *      (
 *      @
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *      let
 *      yield
 *
 *  AST
 *      N_CALL
 *      N_EXPRESSIONS
 *      N_FUNCTION
 *      N_DOT
 *          left: N_QNAME | N_DOT | N_EXPRESSIONS | N_FUNCTION
 *          right: N_QNAME | N_EXPRESSIONS | N_FUNCTION
 *      N_LITERAL
 *      N_NEW           (array / object literals)
 *      N_QNAME
 *      N_SUPER
 *      N_THIS
 *      N_DELETE
 */
static EcNode *parseAssignmentExpression(EcCompiler *cp)
{
    EcNode      *np, *parent;
    int         subId;

    ENTER(cp);

    /*
     *  A LeftHandSideExpression is allowed in both ConditionalExpression and in a SimplePattern. So allow the longest
     *  matching production to have first  crack at the input.
     */
    np = parseConditionalExpression(cp);
    if (np) {
        if (peekToken(cp) == T_ASSIGN) {
            getToken(cp);
            subId = cp->token->subId;
            if (cp->token->groupMask & G_COMPOUND_ASSIGN) {
                np = rewriteCompoundAssignment(cp, subId, np, parseAssignmentExpression(cp));

            } else {
                parent = createNode(cp, N_ASSIGN_OP);
                np = createAssignNode(cp, np, parseAssignmentExpression(cp), parent);
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ListExpression (227)
 *      AssignmentExpression -allowList,b-
 *      ListExpression -b- , AssignmentExpression -allowList,b-
 *
 *  Input
 *      x = ...
 *
 *  AST
 *      N_EXPRESSIONS
 *
 */
static EcNode *parseListExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = createNode(cp, N_EXPRESSIONS);
    mprAssert(np);

    do {
        np = appendNode(np, parseAssignmentExpression(cp));
    } while (np && getToken(cp) == T_COMMA);

    if (np) {
        putToken(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  Pattern -a,b,g- (231)
 *      SimplePattern -a,b-g-
 *      ObjectPattern
 *      ArrayPattern
 *
 *  Input
 *      Identifier
 *      {
 *      [
 *
 *  AST
 */
static EcNode *parsePattern(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    default:
        np = parseSimplePattern(cp);
        break;
    }

    return LEAVE(cp, np);
}


/*
 *  SimplePattern -a,b,noExpr- (232)
 *      Identifier
 *
 *  SimplePattern -a,b,noExpr- (233)
 *      LeftHandSideExpression -a,b-
 *
 *  Input
 *
 *  AST
 *      N_QNAME
 *      N_LIST_EXPRESSION
 */
static EcNode *parseSimplePattern(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = parseLeftHandSideExpression(cp);
    if (np == 0 && peekToken(cp) == T_ID) {
        np = parseIdentifier(cp);
    }

    return LEAVE(cp, np);
}


/*
 *  TypedIdentifier (258)
 *      SimplePattern -noList,noin,noExpr-
 *      SimplePattern -a,b,noExpr- : TypeExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseTypedIdentifier(EcCompiler *cp)
{
    EcNode      *np, *typeNode;

    ENTER(cp);

    np = parseSimplePattern(cp);

    if (peekToken(cp) == T_COLON) {

        getToken(cp);

        typeNode = parseTypeExpression(cp);

        if (typeNode) {
            np->typeNode = linkNode(np, typeNode);

        } else {
            np = parseError(cp, "Expecting type");
        }
    }
    return LEAVE(cp, np);
}


/*
 *  TypedPattern (248)
 *      SimplePattern -a,b,noExpr-
 *      SimplePattern -a,b,noExpr- : NullableTypeExpression
 *      ObjectPattern -noExpr-
 *      ObjectPattern -noExpr- : TypeExpression
 *      ArrayPattern -noExpr-
 *      ArrayPattern -noExpr- : TypeExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseTypedPattern(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    default:
        np = parseSimplePattern(cp);
        if (peekToken(cp) == T_COLON) {
            getToken(cp);
            np->typeNode = linkNode(np, parseNullableTypeExpression(cp));
        }
        break;
    }
    mprAssert(np == 0 || np->kind == N_QNAME);
    return LEAVE(cp, np);
}


/*
 *  NullableTypeExpression (266)
 *      null
 *      undefined
 *      TypeExpression
 *      TypeExpression ?            # Nullable
 *      TypeExpression !            # Non-Nullable
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseNullableTypeExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_NULL:
    case T_UNDEFINED:
        np = createNode(cp, N_QNAME);
        setId(np, (char*) cp->token->text);
        np->name.isType = 1;
        break;

    default:
        np = parseTypeExpression(cp);
        if (peekToken(cp) == T_QUERY) {
            getToken(cp);
            np->nullable = 1;

        } else if (cp->peekToken->tokenId == T_LOGICAL_NOT) {
            getToken(cp);
            np->nullable = 0;
        }
        break;
    }
    return LEAVE(cp, np);
}


/*
 *  TypeExpression (271)
 *      FunctionType
 *      UnionType
 *      RecordType
 *      ArrayType
 *      PrimaryName
 *
 *  Input
 *      function
 *      (
 *      {
 *      [
 *      Identifier
 *
 *  AST
 *      N_QNAME
 *      N_DOT
 */
static EcNode *parseTypeExpression(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_STRING:
    case T_ID:
    case T_MUL:
        np = parsePrimaryName(cp);
        if (np) {
            np->name.isType = 1;
        }

        break;

    default:
        getToken(cp);
        np = unexpected(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  ArrayType (297)
 *      [ ElementTypeList ]
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseArrayType(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_LBRACKET) {
        np = expected(cp, "[");
    } else {
        np = parseElementTypeList(cp);
        if (np) {
            if (getToken(cp) != T_LBRACKET) {
                np = expected(cp, "[");
            }
        }
    }

    return LEAVE(cp, np);
}


/*
 *  ElementTypeList (298)
 *      EMPTY
 *      NullableTypeExpression
 *      , ElementTypeList
 *      NullableTypeExpression , ElementTypeList
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseElementTypeList(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    np = 0;
    mprAssert(0);
    return LEAVE(cp, np);
}


/*
 *  Statement (289) -t,o-
 *      Block -t-
 *      BreakStatement Semicolon -o-
 *      ContinueStatement Semicolon -o-
 *      DefaultXMLNamespaceStatement Semicolon -o-
 *      DoStatement Semicolon -o-
 *      ExpresssionStatement Semicolon -o-
 *      ForStatement -o-
 *      IfStatement -o-
 *      LabeledStatement -o-
 *      LetStatement -o-
 *      ReturnStatement Semicolon -o-
 *      SwitchStatement
 *      SwitchTypeStatement
 *      ThrowStatement Semicolon -o-
 *      TryStatement
 *      WhileStatement -o-
 *      WithStatement -o-
 *
 *  Input
 *      EMPTY
 *      {
 *      (
 *      .
 *      ..
 *      [
 *      (
 *      @
 *      break
 *      continue
 *      ?? DefaultXML
 *      do
 *      for
 *      if
 *      let
 *      return
 *      switch
 *      throw
 *      try
 *      while
 *      with
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *
 *  AST
 *      N_BLOCK
 *      N_CONTINUE
 *      N_BREAK
 *      N_FOR
 *      N_FOR_IN
 *      N_HASH
 *      N_IF
 *      N_SWITCH
 *      N_THROW
 *      N_TRY
 *      N_WHILE
 *
 */
static EcNode *parseStatement(EcCompiler *cp)
{
    EcNode      *np;
    int         expectSemi, tid;

    ENTER(cp);

    expectSemi = 0;
    np = 0;

    switch ((tid = peekToken(cp))) {
    case T_AT:
    case T_DELETE:
    case T_DOT:
    case T_DOT_DOT:
    case T_FALSE:
    case T_FUNCTION:
    case T_LBRACKET:
    case T_LPAREN:
    case T_MINUS_MINUS:
    case T_NEW:
    case T_NUMBER:
    case T_NULL:
    case T_PLUS_PLUS:
    case T_STRING:
    case T_SUPER:
    case T_THIS:
    case T_TRUE:
    case T_TYPEOF:
    case T_RESERVED_NAMESPACE:
        np = parseExpressionStatement(cp);
        expectSemi++;
        break;

    case T_BREAK:
        np = parseBreakStatement(cp);
        expectSemi++;
        break;

    case T_CONTINUE:
        np = parseContinueStatement(cp);
        expectSemi++;
        break;

    case T_DO:
        np = parseDoStatement(cp);
        expectSemi++;
        break;

    case T_FOR:
        np = parseForStatement(cp);
        break;

    case T_HASH:
        np = parseHashStatement(cp);
        break;

    case T_ID:
        if (tid == T_ID && peekAheadToken(cp, 2) == T_COLON) {
            np = parseLabeledStatement(cp);
        } else {
            np = parseExpressionStatement(cp);
            expectSemi++;
        }
        break;

    case T_IF:
        np = parseIfStatement(cp);
        break;

    case T_LBRACE:
        np = parseBlockStatement(cp);
        break;

    case T_LET:
        np = parseLetStatement(cp);
        break;

    case T_RETURN:
        np = parseReturnStatement(cp);
        expectSemi++;
        break;

    case T_SWITCH:
        np = parseSwitchStatement(cp);
        break;

    case T_THROW:
        np = parseThrowStatement(cp);
        expectSemi++;
        break;

    case T_TRY:
        np = parseTryStatement(cp);
        break;

    case T_WHILE:
        np = parseWhileStatement(cp);
        break;

    case T_WITH:
        np = parseWithStatement(cp);
        break;

    default:
        getToken(cp);
        np = unexpected(cp);
    }

    if (np && expectSemi) {
        if (getToken(cp) != T_SEMICOLON) {
            if (np->lineNumber < cp->token->lineNumber || cp->token->tokenId == T_EOF || cp->token->tokenId == T_NOP) {
                putToken(cp);
            } else {
                np = unexpected(cp);
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  Substatement -o- (320)
 *      EmptyStatement
 *      Statement -o-
 *
 *  Statement:
 *      Block -t-
 *      BreakStatement Semicolon -o-
 *      ContinueStatement Semicolon -o-
 *      DefaultXMLNamespaceStatement Semicolon -o-
 *      DoStatement Semicolon -o-
 *      ExpresssionStatement Semicolon -o-
 *      ForStatement -o-
 *      IfStatement -o-
 *      LabeledStatement -o-
 *      LetStatement -o-
 *      ReturnStatement Semicolon -o-
 *      SwitchStatement
 *      SwitchTypeStatement
 *      ThrowStatement Semicolon -o-
 *      TryStatement
 *      WhileStatement -o-
 *      WithStatement -o-
 *
 *  Input
 *      EMPTY
 *      {
 *      (
 *      .
 *      ..
 *      [
 *      (
 *      @
 *      break
 *      continue
 *      ?? DefaultXML
 *      do
 *      for
 *      if
 *      let
 *      return
 *      switch
 *      throw
 *      try
 *      while
 *      with
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *
 *  AST
 *      N_NOP
 *      N_BLOCK
 *      N_CONTINUE
 *      N_BREAK
 *      N_FOR
 *      N_FOR_IN
 *      N_IF
 *      N_SWITCH
 *      N_THROW
 *      N_TRY
 *      N_WHILE
 */

static EcNode *parseSubstatement(EcCompiler *cp)
{
    EcNode      *np;
    int         tid;

    ENTER(cp);

    np = 0;

    switch ((tid = peekToken(cp))) {
    case T_AT:
    case T_BREAK:
    case T_CONTINUE:
    case T_DO:
    case T_DOT:
    case T_DOT_DOT:
    case T_FALSE:
    case T_FOR:
    case T_FUNCTION:
    case T_IF:
    case T_LBRACE:
    case T_LBRACKET:
    case T_LET:
    case T_LPAREN:
    case T_NEW:
    case T_NUMBER:
    case T_NULL:
    case T_RETURN:
    case T_STRING:
    case T_SUPER:
    case T_SWITCH:
    case T_THIS:
    case T_THROW:
    case T_TRUE:
    case T_TRY:
    case T_WHILE:
    case T_WITH:
        np = parseStatement(cp);
        break;

    case T_ID:
        if (peekAheadToken(cp, 2) == T_COLON) {
            /* Labeled expression */
            np = parseStatement(cp);
        } else {
            np = parseStatement(cp);
        }
        break;


    default:
        np = createNode(cp, N_NOP);
    }

    return LEAVE(cp, np);
}


/*
 *  EmptyStatement (33)
 *      ;
 *
 *  Input
 *      EMPTY
 *
 *  AST
 *      N_NOP
 */
static EcNode *parseEmptyStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    np = createNode(cp, N_NOP);;
    return LEAVE(cp, np);
}


/*
 *  ExpressionStatement (331)
 *      [lookahead !function,{}] ListExpression -allowin-
 *
 *  Input
 *      (
 *      .
 *      ..
 *      null
 *      true
 *      false
 *      this
 *      function
 *      delete
 *      Identifier
 *      [
 *      (
 *      @
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *
 *  AST
 *      N_CALL
 *      N_DELETE
 *      N_DOT
 *          left: N_QNAME | N_DOT | N_EXPRESSIONS | N_FUNCTION
 *          right: N_QNAME | N_EXPRESSIONS | N_FUNCTION
 *      N_EXPRESSIONS
 *      N_LITERAL
 *      N_NEW           (array / object literals)
 *      N_QNAME
 *      N_SUPER
 *      N_THIS
 */
static EcNode *parseExpressionStatement(EcCompiler *cp)
{
    EcNode      *np;
    int         tid;

    ENTER(cp);
    tid = peekToken(cp);
    if (tid == T_FUNCTION || tid == T_LBRACE) {
        np = createNode(cp, 0);
    } else {
        np = parseListExpression(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  BlockStatement (318)
 *      Block
 *
 *  Input
 *      {
 *
 *  AST
 *      N_BLOCK
 */
static EcNode *parseBlockStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    np = parseBlock(cp);
    return LEAVE(cp, np);
}


/*
 *  LabeledStatement -o- (319)
 *      Identifier : Substatement
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseLabeledStatement(EcCompiler *cp)
{
    getToken(cp);
    return parseError(cp, "Labeled statements are not yet implemented");
}


/*
 *  IfStatement -abbrev- (320)
 *      if ParenListExpression Substatement
 *      if ParenListExpression Substatement else Substatement
 *
 *  IfStatement -full- (322)
 *      if ParenListExpression Substatement
 *      if ParenListExpression Substatement else Substatement
 *
 *  IfStatement -noShortif- (324)
 *      if ParenListExpression Substatement else Substatement
 *
 *  Input
 *      if ...
 *
 *  AST
 */
static EcNode *parseIfStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_IF) {
        return LEAVE(cp, parseError(cp, "Expecting \"if\""));
    }
    if (peekToken(cp) != T_LPAREN) {
        getToken(cp);
        return LEAVE(cp, parseError(cp, "Expecting \"(\""));
    }

    np = createNode(cp, N_IF);
    np->tenary.cond = linkNode(np, parseParenListExpression(cp));
    np->tenary.thenBlock = linkNode(np, parseSubstatement(cp));

    if (peekToken(cp) == T_ELSE) {
        getToken(cp);
        np->tenary.elseBlock = linkNode(np, parseSubstatement(cp));
    }
    return LEAVE(cp, np);
}


/*
 *  SwitchStatement (328)
 *      switch ParenListExpression { CaseElements }
 *      switch type ( ListExpression -allowList,allowin- : TypeExpression )
 *              { TypeCaseElements }
 *
 *  Input
 *      switch ...
 *
 *  AST
 *      N_SWITCH
 *          N_EXPRESSIONS           ( ListExpression )
 *          N_CASE_ELEMENTS
 */
static EcNode *parseSwitchStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = createNode(cp, N_SWITCH);

    if (getToken(cp) != T_SWITCH) {
        np = unexpected(cp);

    } else {
        if (peekToken(cp) != T_TYPE) {
            np = appendNode(np, parseParenListExpression(cp));
            if (getToken(cp) != T_LBRACE) {
                np = parseError(cp, "Expecting \"{\"");
            } else {
                np = appendNode(np, parseCaseElements(cp));
                if (getToken(cp) != T_RBRACE) {
                    np = parseError(cp, "Expecting \"{\"");
                }
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  CaseElements (342)
 *      EMPTY
 *      CaseLabel
 *      CaseLabel CaseElementsPrefix CaseLabel
 *      CaseLabel CaseElementsPrefix Directive -abbrev-
 *
 *  Refactored as:
 *      EMPTY
 *      CaseLable Directives
 *      CaseElements
 *
 *  Input
 *      case
 *      default
 *
 *  AST
 *      N_CASE_ELEMENTS
 *          N_CASE_LABEL: kind, expression
 */
static EcNode *parseCaseElements(EcCompiler *cp)
{
    EcNode      *np, *caseLabel, *directives;

    ENTER(cp);

    np = createNode(cp, N_CASE_ELEMENTS);

    while (np && (peekToken(cp) == T_CASE || cp->peekToken->tokenId == T_DEFAULT)) {

        caseLabel = parseCaseLabel(cp);
        directives = createNode(cp, N_DIRECTIVES);
        caseLabel = appendNode(caseLabel, directives);

        while (caseLabel && directives && peekToken(cp) != T_CASE && cp->peekToken->tokenId != T_DEFAULT) {
            if (cp->peekToken->tokenId == T_RBRACE) {
                break;
            }
            directives = appendNode(directives, parseDirective(cp));
        }
        np = appendNode(np, caseLabel);
    }
    return LEAVE(cp, np);
}


/*
 *  CaseLabel (349)
 *      case ListExpression -allowin- :
 *      default :
 *
 *  Input
 *      case .. :
 *      default :
 *
 *  AST
 *      N_CASE_LABEL  kind, expression
 */
static EcNode *parseCaseLabel(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = 0;

    if (peekToken(cp) == T_CASE) {
        getToken(cp);
        np = createNode(cp, N_CASE_LABEL);
        np->caseLabel.kind = EC_SWITCH_KIND_CASE;
        np->caseLabel.expression = linkNode(np, parseListExpression(cp));
        if (np->caseLabel.expression == 0) {
            return LEAVE(cp, np);
        }

    } else if (cp->peekToken->tokenId == T_DEFAULT) {
        getToken(cp);
        np = createNode(cp, N_CASE_LABEL);
        np->caseLabel.kind = EC_SWITCH_KIND_DEFAULT;
    }
    if (getToken(cp) != T_COLON) {
        np = expected(cp, ":");
    }
    return LEAVE(cp, np);
}


/*
 *  DoStatement (355)
 *      do Substatement -abbrev- while ParenListExpresison
 *
 *  Input
 *      do
 *
 *  AST
 *      N_FOR
 */
static EcNode *parseDoStatement(EcCompiler *cp)
{
    EcNode      *np;


    ENTER(cp);

    np = createNode(cp, N_DO);

    if (getToken(cp) != T_DO) {
        return LEAVE(cp, unexpected(cp));
    }
    np->forLoop.body = linkNode(np, parseSubstatement(cp));

    if (getToken(cp) != T_WHILE) {
        np = expected(cp, "while");
    } else {
        np->forLoop.cond = linkNode(np, parseParenListExpression(cp));
    }
    return LEAVE(cp, np);
}


/*
 *  WhileStatement (356)
 *      while ParenListExpresison Substatement -o-
 *
 *  Input
 *      while
 *
 *  AST
 *      N_FOR
 */
static EcNode *parseWhileStatement(EcCompiler *cp)
{
    EcNode      *np, *initializer;

    ENTER(cp);

    initializer = 0;

    if (getToken(cp) != T_WHILE) {
        return LEAVE(cp, parseError(cp, "Expecting \"while\""));
    }
    /*
     *  Convert into a "for" AST
     */
    np = createNode(cp, N_FOR);
    np->forLoop.cond = linkNode(np, parseParenListExpression(cp));
    np->forLoop.body = linkNode(np, parseSubstatement(cp));
    return LEAVE(cp, np);
}


/*
 *  ForStatement -o- (357)
 *      for ( ForInitializer ; OptionalExpression ; OptionalExpression )
 *              Substatement
 *      for ( ForInBinding in ListExpression -allowin- ) Substatement
 *      for each ( ForInBinding in ListExpression -allowin- ) Substatement
 *
 *  Where:
 *
 *  ForIntializer (360)
 *      EMPTY
 *      ListExpression -noin-
 *      VariableDefinition -noin-
 *
 *  ForInBinding (363)
 *      Pattern -allowList,noIn,allowExpr-
 *      VariableDefinitionKind VariableBinding -allowList,noIn-
 *
 *  VariableDefinition -b- (429)
 *      VariableDefinitionKind VariableBindingList -allowList,b-
 *
 *  VariableDefinitionKind (430)
 *      const
 *      let
 *      let const
 *      var
 *
 *  MemberExpression Input tokens
 *      null
 *      true
 *      false
 *      this
 *      function
 *      Identifier
 *      {
 *      [
 *      (
 *      @
 *      NumberLiteral
 *      StringLiteral
 *      RegularExpression
 *      XMLInitializer: <!--, [CDATA, <?, <
 *      super
 *      new
 *
 *  Input
 *      for ( 'const|let|let const|var' '[|{'
 *
 *  AST
 *      N_FOR
 *      N_FOR_IN
 */
static EcNode *parseForStatement(EcCompiler *cp)
{
    EcNode      *np, *initializer, *body, *iterGet, *block, *callGet, *dot;
    int         each, forIn;

    ENTER(cp);

    initializer = 0;
    np = 0;
    forIn = 0;
    each = 0;

    if (getToken(cp) != T_FOR) {
        return LEAVE(cp, parseError(cp, "Expecting \"for\""));
    }

    if (peekToken(cp) == T_EACH) {
        each++;
        getToken(cp);
    }

    if (getToken(cp) != T_LPAREN) {
        return LEAVE(cp, parseError(cp, "Expecting \"(\""));
    }

    if (peekToken(cp) == T_ID && peekAheadToken(cp, 2) == T_IN) {
        /*
         *  For in forces the variable to be a let scoped var
         */
        initializer = createNode(cp, N_VAR_DEFINITION);
        if (initializer) {
            initializer->def.varKind = KIND_LET;
            initializer = parseVariableBindingList(cp, initializer, 0);
        }

    } else if (peekToken(cp) == T_CONST || cp->peekToken->tokenId == T_LET || cp->peekToken->tokenId == T_VAR) {
        initializer = parseVariableDefinition(cp, 0);

    } else if (cp->peekToken->tokenId != T_SEMICOLON) {
        cp->state->noin = 1;
        initializer = parseListExpression(cp);
    }
    if (initializer == 0 && cp->error) {
        return LEAVE(cp, 0);
    }

    if (getToken(cp) == T_SEMICOLON) {
        forIn = 0;
        np = createNode(cp, N_FOR);
        np->forLoop.initializer = linkNode(np, initializer);
        if (peekToken(cp) != T_SEMICOLON) {
            np->forLoop.cond = linkNode(np, parseOptionalExpression(cp));
        }
        if (getToken(cp) != T_SEMICOLON) {
            np = parseError(cp, "Expecting \";\"");
        } else if (peekToken(cp) != T_RPAREN) {
            np->forLoop.perLoop = linkNode(np, parseOptionalExpression(cp));
        }

    } else if (cp->token->tokenId == T_IN) {
        forIn = 1;
        np = createNode(cp, N_FOR_IN);
        np->forInLoop.iterVar = linkNode(np, initializer);

        /*
         *  Create a "listExpression.get/values" node
         */
        dot = createNode(cp, N_DOT);
        iterGet = appendNode(dot, parseListExpression(cp));
        iterGet = appendNode(iterGet, createNameNode(cp, (each) ? "getValues" : "get", EJS_ITERATOR_NAMESPACE));

        /*
         *  Create a call node for "get"
         */
        callGet = createNode(cp, N_CALL);
        callGet = appendNode(callGet, iterGet);
        callGet = appendNode(callGet, createNode(cp, N_ARGS));
        np->forInLoop.iterGet = linkNode(np, callGet);

        np->forInLoop.iterNext = linkNode(np, createNode(cp, N_NOP));

        if (np->forInLoop.iterVar == 0 || np->forInLoop.iterGet == 0) {
            return LEAVE(cp, 0);
        }

    } else {
        return LEAVE(cp, unexpected(cp));
    }

    if (getToken(cp) != T_RPAREN) {
        np = parseError(cp, "Expecting \")\"");
    }

    body = linkNode(np, parseSubstatement(cp));
    if (body == 0) {
        return LEAVE(cp, body);
    }

    /*
     *  Fixup the body block and move it outside the entire for loop.
     */
    if (body->kind == N_BLOCK) {
        block = body;
        body = removeNode(block, block->left);

    } else {
        block = createNode(cp, N_BLOCK);
    }

    if (forIn) {
        np->forInLoop.body = linkNode(np, body);
        np->forInLoop.each = each;

    } else {
        if (each) {
            return LEAVE(cp, parseError(cp, "\"for each\" can only be used with \"for .. in\""));
        }
        mprAssert(np != body);
        np->forLoop.body = linkNode(np, body);
    }

    /*
     *  Now make the for loop node a child of the outer block. Block will initially be a child of np, so must re-parent first
     */
    mprAssert(block != np);
    mprStealBlock(cp->state, block);
    np = appendNode(block, np);
    return LEAVE(cp, np);
}


/*
 *  HashStatement (EJS)
 *      # ListExpression
 *
 *  Input
 *      # expression directive
 *
 *  AST
 *      N_HASH
 */
static EcNode *parseHashStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_HASH) {
        return LEAVE(cp, parseError(cp, "Expecting \"#\""));
    }

    np = createNode(cp, N_HASH);
    np->hash.expr = linkNode(np, parseListExpression(cp));
    np->hash.body = linkNode(np, parseDirective(cp));

    return LEAVE(cp, np);
}


/*
 *  LetStatement (367)
 *      let ( LetBindingList ) Substatement -o-
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseLetStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    np = 0;
    mprAssert(0);
    return LEAVE(cp, np);
}


/*
 *  WithStatement -o- (368)
 *      with ( ListExpression -allowin- ) Substatement -o-
 *      with ( ListExpression -allowin- : TypeExpression ) Substatement -o-
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseWithStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_WITH) {
        return LEAVE(cp, expected(cp, "with"));
    }
    if (getToken(cp) != T_LPAREN) {
        return LEAVE(cp, expected(cp, "("));
    }

    np = createNode(cp, N_WITH);
    np->with.object = linkNode(np, parseListExpression(cp));

    if (getToken(cp) != T_RPAREN) {
        return LEAVE(cp, expected(cp, ")"));
    }
    np->with.statement = linkNode(np, parseSubstatement(cp));
    return LEAVE(cp, np);
}


/*
 *  ContinueStatement (370)
 *      continue
 *      continue [no line break] Identifier
 *
 *  Input
 *      continue
 *
 *  AST
 *      N_CONTINUE
 */
static EcNode *parseContinueStatement(EcCompiler *cp)
{
    EcNode      *np;
    int         lineNumber;

    ENTER(cp);

    if (getToken(cp) != T_CONTINUE) {
        np = expected(cp, "continue");
    } else {
        np = createNode(cp, N_CONTINUE);
        lineNumber = cp->token->lineNumber;
        if (peekToken(cp) == T_ID && lineNumber == cp->peekToken->lineNumber) {
            np = appendNode(np, parseIdentifier(cp));
        }
    }
    return LEAVE(cp, np);
}


/*
 *  BreakStatement (372)
 *      break
 *      break [no line break] Identifier
 *
 *  Input
 *      break
 *
 *  AST
 *      N_BREAK
 */
static EcNode *parseBreakStatement(EcCompiler *cp)
{
    EcNode      *np;
    int         lineNumber;

    ENTER(cp);

    if (getToken(cp) != T_BREAK) {
        np = expected(cp, "break");
    } else {
        np = createNode(cp, N_BREAK);
        lineNumber = cp->token->lineNumber;
        if (peekToken(cp) == T_ID && lineNumber == cp->peekToken->lineNumber) {
            np = appendNode(np, parseIdentifier(cp));
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ReturnStatement (374)
 *      return
 *      return [no line break] ListExpression -allowin-
 *
 *  Input
 *      return ...
 *
 *  AST
 *      N_RETURN
 */
static EcNode *parseReturnStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_RETURN) {
        np = unexpected(cp);

    } else {
        if (cp->state->currentFunctionNode == 0) {
            np = parseError(cp, "Return statemeout outside function");

        } else {
            np = createNode(cp, N_RETURN);
            if (peekToken(cp) != T_SEMICOLON && np->lineNumber == cp->peekToken->lineNumber) {
                np = appendNode(np, parseListExpression(cp));
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ThrowStatement (376)
 *      throw ListExpression -allowin-
 *
 *  Input
 *      throw ...
 *
 *  AST
 */
static EcNode *parseThrowStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_THROW) {
        return LEAVE(cp, unexpected(cp));
    }
    np = createNode(cp, N_THROW);
    np = appendNode(np, parseListExpression(cp));
    return LEAVE(cp, np);
}


/*
 *  TryStatement (377)
 *      try Block -local- CatchClauses
 *      try Block -local- CatchClauses finally Block -local-
 *      try Block -local- finally Block -local-
 *
 *  Input
 *      try ...
 *
 *  AST
 */
static EcNode *parseTryStatement(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    /*
     *  Just ignore try / catch for now
     */
    if (getToken(cp) != T_TRY) {
        return LEAVE(cp, unexpected(cp));
    }
    np = createNode(cp, N_TRY);
    if (np) {
        np->exception.tryBlock = linkNode(np, parseBlock(cp));
        if (peekToken(cp) == T_CATCH) {
            np->exception.catchClauses = linkNode(np, parseCatchClauses(cp));
        }
        if (peekToken(cp) == T_FINALLY) {
            getToken(cp);
            np->exception.finallyBlock = linkNode(np, parseBlock(cp));
        }
    }
    return LEAVE(cp, np);
}


/*
 *  CatchClauses (380)
 *      CatchClause
 *      CatchClauses CatchClause
 *
 *  Input
 *      catch
 */
static EcNode *parseCatchClauses(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (peekToken(cp) != T_CATCH) {
        getToken(cp);
        return LEAVE(cp, unexpected(cp));
    }
    np = createNode(cp, N_CATCH_CLAUSES);

    do {
        np = appendNode(np, parseCatchClause(cp));
    } while (peekToken(cp) == T_CATCH);
    return LEAVE(cp, np);
}


/*
 *  CatchClause (382)
 *      catch ( Parameter ) Block -local-
 *
 *  Input
 *      catch
 *
 *  AST
 *      T_CATCH
 */
static EcNode *parseCatchClause(EcCompiler *cp)
{
    EcNode      *np, *block, *arg, *varDef, *parent;

    ENTER(cp);


    if (getToken(cp) != T_CATCH) {
        return LEAVE(cp, unexpected(cp));
    }

    np = createNode(cp, N_CATCH);

    /*
     *  EJS enhancement: allow no (Parameter)
     */
    varDef = 0;
    arg = 0;
    if (peekToken(cp) == T_LPAREN) {
        getToken(cp);
        varDef = parseParameter(cp, 0);
        if (varDef) {
            mprAssert(varDef->kind == N_VAR_DEFINITION);
            varDef->def.varKind = KIND_LET;
            arg = varDef->left;
            mprAssert(arg->kind == N_QNAME);
            removeNode(varDef, arg);
            arg->qname.space = cp->state->namespace;
        }
        if (getToken(cp) != T_RPAREN) {
            return LEAVE(cp, unexpected(cp));
        }
        /*
         *  Insert an assign node and value
         */
        if (varDef) {
            parent = createNode(cp, N_ASSIGN_OP);
            arg = createAssignNode(cp, arg, createNode(cp, N_CATCH_ARG), parent);
            varDef = appendNode(varDef, arg);
        }
    }
    np->catchBlock.arg = varDef;

    block = parseBlock(cp);
    if (block) {
        if (varDef) {
            block = insertNode(block, varDef, 0);
        }
    }
    np = appendNode(np, block);
    return LEAVE(cp, np);
}


/* -t- == global, class, interface, local */

/*
 *  Directives -t- (367)
 *      EMPTY
 *      DirectivesPrefix Directive -t,abbrev-
 *
 *  Input
 *      #
 *      import
 *      use
 *      {
 *      (
 *      break
 *      continue
 *      ?? DefaultXML
 *      do
 *      ?? ExpressionS
 *      for
 *      if
 *      label :
 *      let
 *      new
 *      return
 *      switch
 *      throw
 *      try
 *      while
 *      with
 *      internal, intrinsic, private, protected, public
 *      final, native, override, prototype, static
 *      [ attribute AssignmentExpression
 *      Identifier
 *      const
 *      let
 *      var
 *      function
 *      interface
 *      namespace
 *      type
 *      module
 *
 *  AST
 *      N_DIRECTIVES
 */
static EcNode *parseDirectives(EcCompiler *cp)
{
    EcNode      *np;
    EcState     *saveState;
    EcState     *state;

    ENTER(cp);

    np = createNode(cp, N_DIRECTIVES);
    state = cp->state;
    state->topVarBlockNode = np;

    saveState = cp->directiveState;
    cp->directiveState = state;

    state->blockNestCount++;

    do {
        switch (peekToken(cp)) {
        case T_ERR:
            cp->directiveState = saveState;
            getToken(cp);
            return LEAVE(cp, unexpected(cp));

        case T_EOF:
            cp->directiveState = saveState;
            return LEAVE(cp, np);

        case T_USE:
        case T_REQUIRE:
            np = appendNode(np, parseDirectivesPrefix(cp));
            break;

        case T_RBRACE:
            if (state->blockNestCount == 1) {
                getToken(cp);
            }
            cp->directiveState = saveState;
            return LEAVE(cp, np);

        case T_SEMICOLON:
            getToken(cp);
            break;

        case T_ATTRIBUTE:
        case T_BREAK:
        case T_CLASS:
        case T_CONST:
        case T_CONTINUE:
        case T_DELETE:
        case T_DO:
        case T_DOT:
        case T_FALSE:
        case T_FOR:
        case T_FINAL:
        case T_FUNCTION:
        case T_HASH:
        case T_ID:
        case T_IF:
        case T_INTERFACE:
        case T_MINUS_MINUS:
        case T_LBRACKET:
        case T_LBRACE:
        case T_LPAREN:
        case T_LET:
        case T_NAMESPACE:
        case T_NATIVE:
        case T_NEW:
        case T_NUMBER:
        case T_RESERVED_NAMESPACE:
        case T_RETURN:
        case T_PLUS_PLUS:
        case T_STRING:
        case T_SUPER:
        case T_SWITCH:
        case T_THIS:
        case T_THROW:
        case T_TRUE:
        case T_TRY:
        case T_TYPEOF:
        case T_VAR:
        case T_WHILE:
        case T_MODULE:
        case T_WITH:
            np = appendNode(np, parseDirective(cp));
            break;

        case T_NOP:
            if (state->blockNestCount == 1) {
                getToken(cp);
                break;

            } else {
                /*
                 *  NOP tokens are injected when reading from the console. If we are nested,
                 *  we need to eat all input. Then continue.
                 */
                ecResetInput(cp);
            }
            break;

        default:
            getToken(cp);
            np = unexpected(cp);
            cp->directiveState = saveState;
            return LEAVE(cp, np);
        }

        if (cp->error && !cp->fatalError) {
            np = ecResetError(cp, np, 1);
        }
    } while (np && (!cp->interactive || state->blockNestCount > 1));

    cp->directiveState = saveState;
    return LEAVE(cp, np);
}


/*
 *
 *  DirectivesPrefix -t- (369)
 *      EMPTY
 *      Pragmas
 *      DirectivesPrefix Directive -t,full-
 *
 *  Rewritten as:
 *      DirectivesPrefix
 *
 *  Input
 *      use
 *      import
 *
 *  AST
 *      N_PRAGMAS
 */
static EcNode *parseDirectivesPrefix(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = createNode(cp, N_PRAGMAS);

    do {
        switch (peekToken(cp)) {
        case T_ERR:
            return LEAVE(cp, unexpected(cp));

        case T_EOF:
            return LEAVE(cp, np);

        case T_USE:
        case T_REQUIRE:
            np = parsePragmas(cp, np);
            break;

        default:
            return LEAVE(cp, np);
        }

    } while (np);
    return LEAVE(cp, np);
}


/*
 *  Scan ahead and see if this is an annotatable directive
 */
static int isAttribute(EcCompiler *cp)
{
    int     i, tid;

    /*
     *  Assume we have just seen an ID. Handle the following patterns:
     *      nspace var
     *      nspace function
     *      nspace class
     *      nspace interface
     *      nspace let
     *      nspace const
     *      nspace type
     *      nspace namespace
     *      a.nspace namespace
     *      a.b.c.nspace::nspace namespace
     */
    for (i = 2; i < EC_MAX_LOOK_AHEAD + 2; i++) {
        peekAheadToken(cp, i);
        tid = cp->peekToken->tokenId;
        switch (tid) {
        case T_ATTRIBUTE:
        case T_CLASS:
        case T_CONST:
        case T_FUNCTION:
        case T_INTERFACE:
        case T_LET:
        case T_MUL:
        case T_NAMESPACE:
        case T_RESERVED_NAMESPACE:
        case T_TYPE:
        case T_VAR:
            return 1;

        case T_COLON_COLON:
        case T_DOT:
            break;

        default:
            return 0;
        }

        /*
         *  Just saw a "." or "::".  Make sure this is part of a PropertyName
         */
        tid = peekAheadToken(cp, ++i);
        if (tid != T_ID && tid != T_RESERVED_NAMESPACE && tid == T_MUL && tid != T_STRING && tid != T_NUMBER &&
                tid != T_LBRACKET && !(cp->peekToken->groupMask & G_RESERVED)) {
            return 0;
        }
    }
    return 0;
}


/*
 *  Directive -t,o- (372)
 *      EmptyStatement
 *      Statement
 *      AnnotatableDirective -t,o-
 *
 *  Input
 *      #
 *      {
 *      break
 *      continue
 *      ?? DefaultXML
 *      do
 *      ?? Expressions
 *      for
 *      if
 *      label :
 *      let
 *      return
 *      switch
 *      throw
 *      try
 *      while
 *      with
 *      *
 *      internal, intrinsic, private, protected, public
 *      final, native, override, prototype, static
 *      [ attribute AssignmentExpression
 *      Identifier
 *      const
 *      let
 *      var
 *      function
 *      interface
 *      namespace
 *      type
 *
 *  AST
 *      N_BLOCK
 *      N_CONTINUE
 *      N_BREAK
 *      N_FOR
 *      N_FOR_IN
 *      N_HASH
 *      N_IF
 *      N_SWITCH
 *      N_THROW
 *      N_TRY
 *      N_WHILE
 */
static EcNode *parseDirective(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (peekToken(cp)) {
    case T_EOF:
        getToken(cp);
        return LEAVE(cp, 0);

    case T_ERR:
        getToken(cp);
        return LEAVE(cp, unexpected(cp));

    /* EmptyStatement */
    case T_SEMICOLON:
        np = parseEmptyStatement(cp);
        break;

    /* Statement */
    /*
     *  TBD -- missing:
     *      - DefaultXMLNamespaceStatement
     *      - ExpressionStatement
     *      - LabeledStatement
     */
    case T_LBRACE:
    case T_BREAK:
    case T_CONTINUE:
    case T_DELETE:
    case T_DO:
    case T_FOR:
    case T_HASH:
    case T_IF:
    case T_RETURN:
    case T_SUPER:
    case T_SWITCH:
    case T_THROW:
    case T_TRY:
    case T_WHILE:
    case T_WITH:
        np = parseStatement(cp);
        break;

    case T_ID:
        if (isAttribute(cp)) {
            np = parseAnnotatableDirective(cp, 0);
        } else {
            np = parseStatement(cp);
        }
        break;

    case T_RESERVED_NAMESPACE:
        if (peekAheadToken(cp, 2) == T_COLON_COLON) {
            np = parseStatement(cp);
            break;
        }
        /* Fall through */
                
    /* AnnotatableDirective */
    case T_ATTRIBUTE:
    case T_CLASS:
    case T_CONST:
    case T_FUNCTION:
    case T_INTERFACE:
    case T_LET:
    case T_MUL:
    case T_NAMESPACE:
    case T_TYPE:
    case T_MODULE:
    case T_VAR:
        np = parseAnnotatableDirective(cp, 0);
        break;

    case T_STRING:
        //  TDOO - should we test let ...?
        if (peekAheadToken(cp, 2) == T_VAR || peekAheadToken(cp, 2) == T_CLASS || peekAheadToken(cp, 2) == T_FUNCTION) {
            np = parseAnnotatableDirective(cp, 0);
        } else {
            np = parseStatement(cp);
        }
        break;

    default:
        np = parseStatement(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  AnnotatableDirective -global,o- (375)
 *      Attributes [no line break] AnnotatableDirective -t,o-
 *      VariableDefinition -allowin- Semicolon -o-
 *      FunctionDefinition -global-
 *      ClassDefinition
 *      InterfaceDefintion
 *      NamespaceDefinition Semicolon -o-
 *      TypeDefinition Semicolon
 *      PackageDefinition
 *      ModuleDefinition
 *
 *  AnnotatableDirective -interface,o- (384)
 *      Attributes [no line break] AnnotatableDirective -t,o-
 *      FunctionDeclaration Semicolon -o-
 *      TypeDefinition Semicolon -o-
 *
 *  AnnotatableDirective -t,o- (387)
 *      Attributes [no line break] AnnotatableDirective -t,o-
 *      VariableDefinition -allowin- Semicolon -o-
 *      FunctionDeclaration -t-
 *      NamespaceDefintion Semicolon -o-
 *      TypeDefinition Semicolon -o-
 *
 *  Input
 *      internal, intrinsic, private, protected, public
 *      final, native, override, prototype, static
 *      [ attribute AssignmentExpression
 *      Identifier
 *      const
 *      let
 *      var
 *      function
 *      interface
 *      namespace
 *      type
 *      module
 *      package
 *
 *  AST
 *      N_CLASS
 *      N_FUNCTION
 *      N_QNAME
 *      N_NAMESPACE
 *      N_VAR_DEFINITION
 *      N_MODULE??
 */
static EcNode *parseAnnotatableDirective(EcCompiler *cp, EcNode *attributes)
{
    EcState     *state;
    EcNode      *nextAttribute, *np;
    int         expectSemi;

    ENTER(cp);

    np = 0;
    expectSemi = 0;
    state = cp->state;

    switch (peekToken(cp)) {

    /* Attributes AnnotatableDirective */
    case T_STRING:
    case T_ATTRIBUTE:
    case T_RESERVED_NAMESPACE:
    case T_ID:
        nextAttribute = parseAttribute(cp);
        if (nextAttribute) {
            getToken(cp);
            putToken(cp);
            if (nextAttribute->lineNumber < cp->token->lineNumber) {
                /* Must be no line break after the attribute */
                return LEAVE(cp, unexpected(cp));
            }

            /*
             *  Aggregate the attributes and pass in. Must do this to allow "private static var a, b, c"
             */
            if (attributes) {
                nextAttribute->attributes |= attributes->attributes;
                if (attributes->qname.space && nextAttribute->qname.space) {
                    return LEAVE(cp, parseError(cp, "Can't define multiple namespaces for directive"));
                }
                if (attributes->qname.space) {
                    nextAttribute->qname.space = mprStrdup(nextAttribute, attributes->qname.space);
                }
            }

            np = parseAnnotatableDirective(cp, nextAttribute);
        }
        break;


    case T_CONST:
    case T_LET:
    case T_VAR:
        np = parseVariableDefinition(cp, attributes);
        expectSemi++;
        break;

    case T_FUNCTION:
        if (state->inInterface) {
            np = parseFunctionDeclaration(cp, attributes);
        } else {
            np = parseFunctionDefinition(cp, attributes);
        }
        break;

    case T_CLASS:
#if OLD && UNUSED
        if (state->inClass == 0) {
            /* Nested classes are not supported */
            np = parseClassDefinition(cp, attributes);
        } else {
            getToken(cp);
            np = unexpected(cp);
        }
#else
        np = parseClassDefinition(cp, attributes);
#endif
        break;

    case T_INTERFACE:
        if (state->inClass == 0) {
            np = parseInterfaceDefinition(cp, attributes);
        } else {
            np = unexpected(cp);
        }
        break;

    case T_NAMESPACE:
        np = parseNamespaceDefinition(cp, attributes);
        expectSemi++;
        break;

    case T_TYPE:
        np = parseTypeDefinition(cp, attributes);
        expectSemi++;
        break;

    case T_MODULE:
        np = parseModuleDefinition(cp);
        break;

    default:
        getToken(cp);
        np = parseError(cp, "Unknown directive \"%s\"", cp->token->text);
    }

    if (np && expectSemi) {
        if (getToken(cp) != T_SEMICOLON) {
            if (np->lineNumber < cp->token->lineNumber || cp->token->tokenId == T_EOF) {
                putToken(cp);
            } else if (cp->token->tokenId != T_NOP) {
                np = unexpected(cp);
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  Attribute -global- (391)
 *      NamespaceAttribute
 *      dynamic
 *      final
 *      native
 *      [ AssignmentExpression allowList,allowIn]
 *
 *  Attribute -class- (396)
 *      NamespaceAttribute
 *      final
 *      native
 *      override
 *      prototype
 *      static
 *      [ AssignmentExpression allowList,allowIn]
 *
 *  Attribute -interface- (419)
 *      NamespaceAttribute
 *
 *  Attribute -local- (420)
 *      NamespaceAttribute
 *
 *
 *  Input -common-
 *      NamespaceAttribute
 *          Path . Identifier
 *          Identifier
 *          public
 *          internal
 *      final
 *      native
 *      [
 *
 *  Input -global-
 *      NamespaceAttribute -global-
 *          intrinsic
 *      dynamic
 *
 *  Input -class-
 *      NamespaceAttribute -class-
 *          intrinsic
 *          private
 *          protected
 *      override
 *      prototype
 *      static
 *
 *  AST
 *      N_ATTRIBUTES
 *          attribute
 *              flags
 *          attributes
 */
static EcNode *parseAttribute(EcCompiler *cp)
{
    EcNode      *np;
    EcState     *state;
    int         inClass, subId;

    ENTER(cp);

    state = cp->state;
    np = 0;
    inClass = (cp->state->inClass) ? 1 : 0;

    if (state->inFunction) {
        np = parseNamespaceAttribute(cp);
        return LEAVE(cp, np);
    }

    peekToken(cp);
    subId = cp->peekToken->subId;
    switch (cp->peekToken->tokenId) {
    case T_ID:
    case T_RESERVED_NAMESPACE:
    case T_STRING:
        if (!inClass && (subId == T_PRIVATE || subId ==  T_PROTECTED)) {
            getToken(cp);
            return LEAVE(cp, parseError(cp, "Can't use private or protected in this context"));
        }
        np = parseNamespaceAttribute(cp);
        break;

    case T_ATTRIBUTE:
        getToken(cp);
        np = createNode(cp, N_ATTRIBUTES);
        switch (cp->token->subId) {
        case T_DYNAMIC:
            if (inClass) {
                np = unexpected(cp);
            } else {
                np->attributes |= EJS_ATTR_DYNAMIC_INSTANCE;
            }
            break;

        case T_FINAL:
            np->attributes |= EJS_ATTR_FINAL;
            break;

        case T_NATIVE:
            np->attributes |= EJS_ATTR_NATIVE;
            break;

        case T_OVERRIDE:
            if (inClass) {
                np->attributes |= EJS_ATTR_OVERRIDE;
            } else {
                np = unexpected(cp);
            }
            break;

#if ECMA
        case T_PROTOTYPE:
            if (inClass) {
                np->attributes |= EJS_ATTR_PROTOTYPE;
            } else {
                np = unexpected(cp);
            }
            break;
#endif

        case T_STATIC:
            if (inClass) {
                np->attributes |= EJS_ATTR_STATIC;
            } else {
                np = unexpected(cp);
            }
            break;

        case T_ENUMERABLE:
            if (inClass) {
                np->attributes |= EJS_ATTR_ENUMERABLE;
            } else {
                np = unexpected(cp);
            }
            break;

        default:
            np = parseError(cp, "Unknown or invalid attribute in this context %s", cp->token->text);
        }
        break;

    case T_LBRACKET:
        np = appendNode(np, parseAssignmentExpression(cp));
        break;

    default:
        np = parseError(cp, "Unknown or invalid attribute in this context %s", cp->token->text);
        break;
    }
    return LEAVE(cp, np);
}


/*
 *  NamespaceAttribute -global- (405)
 *      public
 *      internal
 *      intrinsic
 *      PrimaryName
 *
 *  NamespaceAttribute -class- (409)
 *      ReservedNamespace
 *      PrimaryName
 *
 *  Input -common-
 *      Identifier
 *      internal
 *      public
 *
 *  Input -global-
 *      intrinsic
 *
 *  Input -class-
 *      intrinsic
 *      private
 *      protected
 *
 *  AST
 *      N_ATTRIBUTES
 *          attribute
 *              flags
 *          left: N_QNAME | N_DOT
 */
static EcNode *parseNamespaceAttribute(EcCompiler *cp)
{
    EcNode      *np, *qualifier;
    int         inClass, subId;

    ENTER(cp);

    inClass = (cp->state->inClass) ? 1 : 0;

    peekToken(cp);
    subId = cp->peekToken->subId;

    np = createNode(cp, N_ATTRIBUTES);
    np->lineNumber = cp->peekToken->lineNumber;

    switch (cp->peekToken->tokenId) {
    case T_RESERVED_NAMESPACE:
        if (!inClass && (subId == T_PRIVATE || subId ==  T_PROTECTED)) {
            getToken(cp);
            return LEAVE(cp, unexpected(cp));
        }
        qualifier = parseReservedNamespace(cp);
        np->attributes = qualifier->attributes;
        np->specialNamespace = qualifier->specialNamespace;
        np->qname.space = mprStrdup(np, qualifier->qname.space);
        break;

    case T_ID:
        qualifier = parsePrimaryName(cp);
        if (qualifier->kind == N_QNAME) {
            np->attributes = qualifier->attributes;
            np->qname.space = mprStrdup(np, qualifier->qname.name);
        } else {
            /*
             *  This is a N_DOT expression compile-time constant expression.
             */
            mprAssert(0);
        }
        break;

    case T_STRING:
        getToken(cp);
        np->qname.space = mprStrdup(np, (char*) cp->token->text);
        np->literalNamespace = 1;
        break;

    case T_ATTRIBUTE:
        getToken(cp);
        np = parseError(cp, "Attribute \"%s\" not supported on local variables", cp->token->text);
        break;

    default:
        np = unexpected(cp);
        break;
    }
    return LEAVE(cp, np);
}


/*
 *  VariableDefinition -b- (411)
 *      VariableDefinitionKind VariableBindingList -allowList,b-
 *
 *  Input
 *      const
 *      let
 *      let const
 *      var
 *
 *  AST
 *      N_VAR_DEFINITION
 *          def: varKind
 */
static EcNode *parseVariableDefinition(EcCompiler *cp, EcNode *attributes)
{
    EcNode      *np;

    ENTER(cp);
    np = parseVariableDefinitionKind(cp, attributes);
    np = parseVariableBindingList(cp, np, attributes);
    return LEAVE(cp, np);
}


/*
 *  VariableDefinitionKind (412)
 *      const
 *      let
 *      let const
 *      var
 *
 *  Input
 *
 *  AST
 *      N_VAR_DEFINITION
 *          def: varKind
 *
 */
static EcNode *parseVariableDefinitionKind(EcCompiler *cp, EcNode *attributes)
{
    EcNode      *np;

    ENTER(cp);

    np = createNode(cp, N_VAR_DEFINITION);
    setNodeDoc(cp, np);

    switch (getToken(cp)) {
    case T_CONST:
        np->def.varKind = KIND_CONST;
        np->attributes |= EJS_ATTR_CONST;
        break;

    case T_LET:
        if (attributes && attributes->attributes & EJS_ATTR_STATIC) {
            np = parseError(cp, "Static and let are not a valid combination. Use var instead.");

        } else {
            np->def.varKind = KIND_LET;
            if (peekToken(cp) == T_CONST) {
                np->def.varKind |= KIND_CONST;
            }
        }
        break;

    case T_VAR:
        np->def.varKind = KIND_VAR;
        break;

    default:
        np = parseError(cp, "Bad variable definition kind");
    }
    return LEAVE(cp, np);
}


/*
 *  VariableBindingList -a,b- (416)
 *      VariableBinding
 *      VariableBindingList -noList,b- , VariableBinding -a,b-
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseVariableBindingList(EcCompiler *cp, EcNode *varList, EcNode *attributes)
{
    ENTER(cp);

    varList = appendNode(varList, parseVariableBinding(cp, varList, attributes));

    while (peekToken(cp) == T_COMMA) {
        getToken(cp);
        varList = appendNode(varList, parseVariableBinding(cp, varList, attributes));
    }
    return LEAVE(cp, varList);
}


/*
 *  VariableBinding -a,b- (418)
 *      TypedIdentifier (258)
 *      TypedPattern (260) -noList,noIn- VariableInitialisation -a,b-
 *
 *  TypedIdentifier (258)
 *      SimplePattern -noList,noin,noExpr-
 *      SimplePattern -a,b,noExpr- : TypeExpression
 *
 *  TypedPattern (260)
 *      SimplePattern -a,b,noExpr-
 *      SimplePattern -a,b,noExpr- : NullableTypeExpression
 *      ObjectPattern -noExpr-
 *      ObjectPattern -noExpr- : TypeExpression
 *      ArrayPattern -noExpr-
 *      ArrayPattern -noExpr- : TypeExpression
 *
 *  Input
 *
 *  AST
 *      N_QNAME variableId
 *      N_ASSIGN
 *          left: N_QNAME variableId
 *          right: N_LITERAL
 *
 */
static EcNode *parseVariableBinding(EcCompiler *cp, EcNode *varList, EcNode *attributes)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = 0;

    switch (peekToken(cp)) {
    case T_LBRACKET:
        getToken(cp);
        unexpected(cp);
        break;

    case T_LBRACE:
        getToken(cp);
        unexpected(cp);
        break;

    default:
        np = parseTypedIdentifier(cp);
        if (np == 0) {
            return LEAVE(cp, np);
        }
        if (np->kind != N_QNAME) {
            return LEAVE(cp, parseError(cp, "Bad variable name"));
        }
        np->attributes = varList->attributes;
        applyAttributes(cp, np, attributes, 0);
        copyDocString(cp, np, varList);

        if (STRICT_MODE(cp)) {
            if (np->typeNode == 0) {
                parseError(cp, "Variable untyped. Variables must be typed when declared in strict mode");
                np = ecResetError(cp, np, 0);
                /* Keep parsing */
            }
        }
        if (peekToken(cp) == T_ASSIGN) {
            parent = createNode(cp, N_ASSIGN_OP);
            np = createAssignNode(cp, np, parseVariableInitialisation(cp), parent);
        }

        break;
    }
    return LEAVE(cp, np);
}


/*
 *  VariableInitialisation -a,b- (426)
 *      = AssignmentExpression -a,b-
 *
 *  Input
 *      =
 *
 *  AST
 *      N_EXPRESSIONS
 */
static EcNode *parseVariableInitialisation(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) == T_ASSIGN) {
        np = parseAssignmentExpression(cp);

    } else {
        np = unexpected(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  FunctionDeclaration (421)                                   # For interfaces
 *      function FunctionName FunctionSignature
 *
 *  Notes:
 *      This is for function declarations in interfaces only.
 *
 *  Input
 *      function
 *
 *  AST
 *      N_FUNCTION
 *          function: name, getter, setter, block,
 *              children: parameters
 */
static EcNode *parseFunctionDeclaration(EcCompiler *cp, EcNode *attributes)
{
    EcNode      *np;
    int         tid;

    ENTER(cp);

    cp->state->defaultNamespace = NULL;

    if (getToken(cp) != T_FUNCTION) {
        return LEAVE(cp, parseError(cp, "Expecting \"function\""));
    }

    tid = peekToken(cp);
    if (tid != T_ID && tid != T_GET && tid != T_SET) {
        getToken(cp);
        return LEAVE(cp, parseError(cp, "Expecting function or class name"));
    }

    np = parseFunctionName(cp);
    if (np) {
        setNodeDoc(cp, np);
        applyAttributes(cp, np, attributes, 0);
        np = parseFunctionSignature(cp, np);
        if (np) {
            np->function.isMethod = 1;
            if (STRICT_MODE(cp)) {
                if (np->function.resultType == 0) {
                    np = parseError(cp, 
                        "Function has not defined a return type. Fuctions must be typed when declared in strict mode");
                }
            }
        }
    }

    return LEAVE(cp, np);
}


/*
 *  FunctionDefinition -class- (424)
 *      function ClassName ConstructorSignature FunctionBody -allowin-
 *      function FunctionName FunctionSignature FunctionBody -allowin-
 *
 *  FunctionDeclaration -t- (442)
 *      function FunctionName FunctionSignature FunctionBody -allowin-
 *      let function FunctionName FunctionSignature FunctionBody -allowin-
 *      const function FunctionName FunctionSignature FunctionBody -allowin-
 *
 *  Input
 *      function
 *      let
 *      const
 *
 *  AST N_FUNCTION
 *      function: name, getter, setter, block,
 *          children: parameters
 */
static EcNode *parseFunctionDefinition(EcCompiler *cp, EcNode *attributeNode)
{
    EcNode      *np, *className;
    EcState     *state;

    ENTER(cp);

    state = cp->state;
    state->defaultNamespace = NULL;

    if (getToken(cp) != T_FUNCTION) {
        return LEAVE(cp, parseError(cp, "Expecting \"function\""));
    }

    if (getToken(cp) != T_ID && !(cp->token->groupMask & (G_CONREV | G_OPERATOR))) {
        return LEAVE(cp, parseError(cp, "Expecting function or class name"));
    }

    if (cp->state->currentClassName.name && strcmp(cp->state->currentClassName.name, (char*) cp->token->text) == 0) {
        /*
         *  Constructor
         */
        putToken(cp);
        np = createNode(cp, N_FUNCTION);
        setNodeDoc(cp, np);
        applyAttributes(cp, np, attributeNode, EJS_PUBLIC_NAMESPACE);
        className = parseClassName(cp);

        if (className) {
            np->qname.name = mprStrdup(np, className->qname.name);
            np->function.isConstructor = 1;
            cp->state->currentClassNode->klass.constructor = np;
        }

        if (np) {
            np = parseConstructorSignature(cp, np);
            if (np) {
                cp->state->currentFunctionNode = np;
                if (!(np->attributes & EJS_ATTR_NATIVE)) {
                    np->function.body = linkNode(np, parseFunctionBody(cp, np));
                    mprStealBlock(np, np->function.body);
                    if (np->function.body == 0) {
                        return LEAVE(cp, 0);
                    }
                }
                np->function.isMethod = 1;
            }
        }

    } else {
        putToken(cp);
        np = parseFunctionName(cp);
        if (np) {
            setNodeDoc(cp, np);
            applyAttributes(cp, np, attributeNode, 0);
            np = parseFunctionSignature(cp, np);
            if (np) {
                cp->state->currentFunctionNode = np;
                if (attributeNode && (attributeNode->attributes & EJS_ATTR_NATIVE)) {
                    if (peekToken(cp) == T_LBRACE) {
                        return LEAVE(cp, parseError(cp, "Native functions declarations must not have bodies"));
                    }

                } else {
                    np->function.body = linkNode(np, parseFunctionBody(cp, np));
                    if (np->function.body == 0) {
                        return LEAVE(cp, 0);
                    }
                }
                if (state->inClass && !state->inFunction && 
                        cp->classState->blockNestCount == (cp->state->blockNestCount - 1)) {
                    np->function.isMethod = 1;
                }
            }

            if (STRICT_MODE(cp)) {
                if (np->function.resultType == 0) {
                    parseError(cp, "Function has not defined a return type. Functions must be typed in strict mode");
                    np = ecResetError(cp, np, 0);
                    /* Keep parsing */
                }
            }
        }
    }

    return LEAVE(cp, np);
}


/*
 *  FunctionName (427)
 *      Identifier
 *      OverloadedOperator
 *      get Identifier
 *      set Identifier
 *
 *  Input
 *      Identifier
 *      get
 *      set
 *      + - ~ * / % < > <= >= == << >> >>> & | === != !==
 *
 *  AST N_FUNCTION
 *      function: name, getter, setter
 */
static EcNode *parseFunctionName(EcCompiler *cp)
{
    EcNode      *name, *np;
    int         accessorId, tid;

    ENTER(cp);

    tid = peekToken(cp);
    if (tid != T_GET && tid != T_SET) {
        if (cp->peekToken->groupMask & G_CONREV) {
            tid = T_ID;
        }
    }

    switch (tid) {
    case T_GET:
    case T_SET:
    case T_DELETE:
        getToken(cp);
        accessorId = cp->token->tokenId;

        tid = peekToken(cp);
        if (cp->peekToken->groupMask & G_CONREV) {
            tid = T_ID;
        }
        if (tid == T_LPAREN) {
            /*
             *  Function is called get() or set(). So put back the name and fall through to T_ID
             */
            putToken(cp);

        } else {
            if (tid != T_ID) {
                getToken(cp);
                return LEAVE(cp, parseError(cp, "Expecting identifier"));
            }
            name = parseIdentifier(cp);
            np = createNode(cp, N_FUNCTION);
            if (accessorId == T_GET) {
                np->function.getter = 1;
                np->qname.name = mprStrdup(np, name->qname.name);
                np->attributes |= EJS_ATTR_GETTER;
            } else {
                np->function.setter = 1;
                np->attributes |= EJS_ATTR_SETTER;
                np->qname.name = mprStrcat(np, -1, "set-", name->qname.name, NULL);
            }
            break;
        }
        /* Fall through */

    case T_ID:
        name = parseIdentifier(cp);
        np = createNode(cp, N_FUNCTION);
        np->qname.name = mprStrdup(np, name->qname.name);
        break;

    default:
        getToken(cp);
        if (cp->token->groupMask == G_OPERATOR) {
            putToken(cp);
            np = parseOverloadedOperator(cp);
        } else {
            np = unexpected(cp);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  OverloadedOperator (431)
 *      + - ~ * / % < > <= >= == << >> >>> & | === != !==
 *
 *  Input
 *      + - ~ * / % < > <= >= == << >> >>> & | === != !==
 *      [ . (  =        EJS exceptions
 *
 *  AST
 *      N_QNAME
 */
static EcNode *parseOverloadedOperator(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    switch (getToken(cp)) {
    /*
     *  EJS extensions
     */
    case T_LBRACKET:
    case T_LPAREN:
    case T_DOT:
    case T_ASSIGN:
        /* Fall through */

    case T_PLUS:
    case T_MINUS:
    case T_TILDE:
    case T_MUL:
    case T_DIV:
    case T_MOD:
    case T_LT:
    case T_GT:
    case T_LE:
    case T_GE:
    case T_EQ:
    case T_LSH:
    case T_RSH:
    case T_RSH_ZERO:
    case T_BIT_AND:
    case T_BIT_OR:
    case T_STRICT_EQ:
    case T_NE:
    case T_STRICT_NE:
        /* Node holds the token */
        np = createNode(cp, N_FUNCTION);
        np->qname.name = mprStrdup(np, (char*) cp->token->text);
        break;

    default:
        np = unexpected(cp);
        break;
    }

    return LEAVE(cp, np);
}


/*
 *  FunctionSignature (450) (See also FunctionSignatureType)
 *      TypeParameters ( Parameters ) ResultType
 *      TypeParameters ( this : PrimaryName ) ResultType
 *      TypeParameters ( this : PrimaryName , NonemptyParameters )
 *              ResultType
 *
 *  Input
 *      .< TypeParameterList >
 *
 *  AST
 */
static EcNode *parseFunctionSignature(EcCompiler *cp, EcNode *np)
{
    EcNode      *parameters;

    if (np == 0) {
        return np;
    }

    ENTER(cp);

    mprAssert(np->kind == N_FUNCTION);

    if (getToken(cp) != T_LPAREN) {
        return LEAVE(cp, parseError(cp, "Expecting \"(\""));
    }

    np->function.parameters = linkNode(np, createNode(cp, N_ARGS));

    if (peekToken(cp) == T_ID || cp->peekToken->tokenId == T_ELIPSIS || cp->peekToken->groupMask & G_CONREV) {
        if (strcmp((char*) cp->peekToken->text, "this") == 0) {
            ;
        } else {
            parameters = parseParameters(cp, np->function.parameters);
            if (parameters == 0) {
                while (getToken(cp) != T_RPAREN && cp->token->tokenId != T_EOF);
                return LEAVE(cp, 0);
            }
            np->function.parameters = linkNode(np, parameters);
        }
    }
    if (getToken(cp) != T_RPAREN) {
        return LEAVE(cp, parseError(cp, "Expecting \")\""));
    }

    if (np) {
        if (peekToken(cp) == T_COLON) {
            np->function.resultType = linkNode(np, parseResultType(cp));
        }
    }

    return LEAVE(cp, np);
}


/*
 *  Parameters (457)
 *      EMPTY
 *      NonemptyParameters
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseParameters(EcCompiler *cp, EcNode *args)
{
    ENTER(cp);

    if (peekToken(cp) != T_RPAREN) {
        args = parseNonemptyParameters(cp, args);
    }
    return LEAVE(cp, args);
}


/*
 *  NonemptyParameters (459)
 *      ParameterInit
 *      ParameterInit , NonemptyParameters
 *      RestParameter
 *
 *  Input
 *      Identifier
 *      ...
 *
 *  AST
 *      N_ARGS
 *          N_VAR_DEFN
 *              N_QNAME
 *              N_ASSIGN_OP
 *                  N_QNAME, N_LITERAL
 *
 */
static EcNode *parseNonemptyParameters(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    if (peekToken(cp) == T_ELIPSIS) {
        np = appendNode(np, parseRestParameter(cp));

    } else {
        np = appendNode(np, parseParameterInit(cp, np));
        if (np) {
            if (peekToken(cp) == T_COMMA) {
                getToken(cp);
                np = parseNonemptyParameters(cp, np);
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ParameterInit (462)
 *      Parameter
 *      Parameter = NonAssignmentExpression -noList,allowIn-
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseParameterInit(EcCompiler *cp, EcNode *args)
{
    EcNode      *np, *assignOp, *lastArg;

    ENTER(cp);

    np = parseParameter(cp, 0);

    if (peekToken(cp) == T_ASSIGN) {
        getToken(cp);
        /*
         *  Insert a N_ASSIGN_OP node under the VAR_DEFN
         */
        assignOp = createNode(cp, N_ASSIGN_OP);
        assignOp = appendNode(assignOp, np->left);
        mprAssert(mprGetListCount(np->children) == 1);
        mprRemoveItem(np->children, mprGetItem(np->children, 0));
        assignOp = appendNode(assignOp, parseNonAssignmentExpression(cp));
        np = appendNode(np, assignOp);

        if (assignOp) {
            appendDocString(cp, args->parent, assignOp->left, assignOp->right);
        }

    } else if (args->children) {
        lastArg = (EcNode*) mprGetLastItem(args->children);
        if (lastArg && lastArg->left->kind == N_ASSIGN_OP) {
            np = parseError(cp, "Cannot have required parameters after parameters with initializers");
        }
    }
    return LEAVE(cp, np);
}


/*
 *  Parameter (464)
 *      ParameterKind TypedPattern -noList,noIn-
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseParameter(EcCompiler *cp, bool rest)
{
    EcNode      *np, *parameter;

    ENTER(cp);

    np = parseParameterKind(cp);
    parameter = parseTypedPattern(cp);
    np = appendNode(np, parameter);

    if (parameter) {
        if (STRICT_MODE(cp)) {
            if (parameter->typeNode == 0 && !rest) {
                parseError(cp, "Parameter untyped. Parameters must be typed when declared in strict mode.");
                np = ecResetError(cp, np, 0);
                /* Keep parsing */
            }
        }
    }
    return LEAVE(cp, np);
}


/*
 *  ParameterKind (465)
 *      EMPTY
 *      const
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseParameterKind(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = createNode(cp, N_VAR_DEFINITION);

    if (peekToken(cp) == T_CONST) {
        getToken(cp);
        np->def.varKind = KIND_CONST;
    }
    return LEAVE(cp, np);
}


/*
 *  RestParameter (467)
 *      ...
 *      ... Parameter
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseRestParameter(EcCompiler *cp)
{
    EcNode      *np, *varNode;

    ENTER(cp);

    if (getToken(cp) == T_ELIPSIS) {
        np = parseParameter(cp, 1);
        if (np && np->left) {
            if (np->left->kind == N_QNAME) {
                varNode = np->left;
            } else if (np->left->kind == N_ASSIGN_OP) {
                varNode = np->left->left;
            } else {
                varNode = 0;
                mprAssert(0);
            }
            if (varNode) {
                mprAssert(varNode->kind == N_QNAME);
                varNode->name.isRest = 1;
            }
        }

    } else {
        np = unexpected(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  ResultType (469)
 *      EMPTY
 *      : void
 *      : NullableTypeExpression
 *
 *  Input
 *
 *  AST
 *      N_DOT
 *      N_QNAME
 *      N_VOID
 *
 *  NOTE: we do not handle EMPTY here. Caller must handle.
 */
static EcNode *parseResultType(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (peekToken(cp) == T_COLON) {
        getToken(cp);
        if (peekToken(cp) == T_VOID) {
            getToken(cp);
            np = createNode(cp, N_QNAME);
            setId(np, "Void");
            np->name.isType = 1;

        } else {
            np = parseNullableTypeExpression(cp);
        }

    } else {
        /*  Don't handle EMPTY here */
        mprAssert(0);
        np = unexpected(cp);;
    }
    return LEAVE(cp, np);
}


/*
 *  ConstructorSignature (472)
 *      TypeParameters ( Parameters )
 *      TypeParameters ( Parameters ) : ConstructorInitializer
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseConstructorSignature(EcCompiler *cp, EcNode *np)
{
    if (np == 0) {
        return np;
    }

    ENTER(cp);

    mprAssert(np->kind == N_FUNCTION);

    if (getToken(cp) != T_LPAREN) {
        return LEAVE(cp, parseError(cp, "Expecting \"(\""));
    }

    np->function.parameters = linkNode(np, createNode(cp, N_ARGS));
    np->function.parameters =
        linkNode(np, parseParameters(cp, np->function.parameters));

    if (getToken(cp) != T_RPAREN) {
        return LEAVE(cp, parseError(cp, "Expecting \")\""));
    }

    if (np) {
        if (peekToken(cp) == T_COLON) {
            getToken(cp);
            np->function.constructorSettings = linkNode(np, parseConstructorInitializer(cp));
            // mprStealBlock(np, np->function.settings);
        }
    }

    return LEAVE(cp, np);
}


/*
 *  ConstructorInitializer (462)
 *      InitializerList
 *      InitializerList SuperInitializer
 *      SuperInitializer
 *
 *  Input
 *      TDB
 *      super
 *
 *  AST
 *      N_DIRECTIVES
 */
static EcNode *parseConstructorInitializer(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = createNode(cp, N_DIRECTIVES);

    if (peekToken(cp) != T_SUPER) {
        np = parseInitializerList(cp, np);
    }
    if (peekToken(cp) == T_SUPER) {
        np = appendNode(np, parseSuperInitializer(cp));
    }
    return LEAVE(cp, np);
}


/*
 *  InitializerList (465)
 *      Initializer
 *      InitializerList , Initializer
 *
 *  Input
 *      TBD
 *
 *  AST
 *      N_DIRECTIVES
 */
static EcNode *parseInitializerList(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    mprAssert(np && np->kind == N_DIRECTIVES);

    while (1) {
        np = appendNode(np, parseInitializer(cp));
        if (peekToken(cp) == T_COMMA) {
            getToken(cp);
        } else {
            break;
        }
    }
    return LEAVE(cp, np);
}


/*
 *  Initializer (467)
 *      Pattern -noList,noIn,noExpr- VariableInitialisation -nolist,allowIn-
 *
 *  Input
 *      TBD
 *
 *  AST
 *      N_ASSIGN
 */
static EcNode *parseInitializer(EcCompiler *cp)
{
    EcNode      *np, *parent;

    ENTER(cp);

    np = parsePattern(cp);

    if (peekToken(cp) != T_ASSIGN) {
        return LEAVE(cp, expected(cp, "="));
    }
    parent = createNode(cp, N_ASSIGN_OP);
    np = createAssignNode(cp, np, parseVariableInitialisation(cp), parent);
    return LEAVE(cp, np);
}


/*
 *  SuperInitializer (481)
 *      super Arguments
 *
 *  Input
 *      super
 *
 *  AST
 *      N_SUPER
 */
static EcNode *parseSuperInitializer(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (getToken(cp) != T_SUPER) {
        return LEAVE(cp, expected(cp, "super"));
    }
    np = createNode(cp, N_SUPER);

    np = appendNode(np, parseArguments(cp));
    return LEAVE(cp, np);
}


/*
 *  FunctionBody -b- (469)
 *      Block -local-
 *      AssignmentExpression -b-
 *
 *  Input
 *      {
 *      (
 *
 *  AST
 */
static EcNode *parseFunctionBody(EcCompiler *cp, EcNode *fun)
{
    EcNode      *np, *end, *ret;

    ENTER(cp);

    cp->state->inFunction = 1;
    cp->state->namespace = EJS_PRIVATE_NAMESPACE;

    if (peekToken(cp) == T_LBRACE) {
        np = parseBlock(cp);
        if (np) {
            np = np->left;
        }

    } else {
        /*
         *  Create a return for block-less functions
         */
        np = createNode(cp, N_DIRECTIVES);
        ret = createNode(cp, N_RETURN);
        ret->ret.blockLess = 1;
        ret = appendNode(ret, parseAssignmentExpression(cp));
        np = appendNode(np, ret);
    }
    if (np) {
        end = createNode(cp, N_END_FUNCTION);
        np = appendNode(np, end);
    }
    return LEAVE(cp, np);
}


/*
 *  ClassDefinition (484)
 *      class ClassName ClassInheritance ClassBody
 *
 *  Input
 *      class id ...
 *
 *  AST
 *      N_CLASS
 *          name
 *              id
 *          extends: id
 *
 */
static EcNode *parseClassDefinition(EcCompiler *cp, EcNode *attributeNode)
{
    EcState     *state;
    EcNode      *np, *classNameNode, *inheritance, *constructor;
    int         tid;

    ENTER(cp);

    state = cp->state;

    if (getToken(cp) != T_CLASS) {
        return LEAVE(cp, expected(cp, "class"));
    }

    if (peekToken(cp) != T_ID) {
        getToken(cp);
        return LEAVE(cp, expected(cp, "identifier"));
    }

    np = createNode(cp, N_CLASS);
    state->currentClassNode = np;
    state->topVarBlockNode = np;
    cp->classState = state;
    state->defaultNamespace = NULL;

    classNameNode = parseClassName(cp);
    if (classNameNode == 0) {
        return LEAVE(cp, 0);
    }

    applyAttributes(cp, np, attributeNode, 0);
    setNodeDoc(cp, np);

    np->qname.name = mprStrdup(np, classNameNode->qname.name);
    state->currentClassName = np->qname;
    state->inClass = 1;

    tid = peekToken(cp);
    if (tid == T_EXTENDS || tid == T_IMPLEMENTS) {
        inheritance = parseClassInheritance(cp);
        if (inheritance->klass.extends) {
            np->klass.extends = mprStrdup(np, inheritance->klass.extends);
        }
        if (inheritance->klass.implements) {
            np->klass.implements = inheritance->klass.implements;
            mprStealBlock(np, np->klass.implements);
        }
    }

    if (peekToken(cp) != T_LBRACE) {
        getToken(cp);
        return LEAVE(cp, expected(cp, "{"));
    }

    np = appendNode(np, parseClassBody(cp));

    if (np && np->klass.constructor == 0) {
        /*
         *  Create a default constructor because the user did not supply a constructor. We always
         *  create a constructor node even if one is not required (or generated). This makes binding easier later.
         */
        constructor = createNode(cp, N_FUNCTION);
        np->klass.constructor = linkNode(np, constructor);
        constructor->qname.name = mprStrdup(np, np->qname.name);
        applyAttributes(cp, constructor, 0, EJS_PUBLIC_NAMESPACE);
        constructor->function.isConstructor = 1;
        constructor->function.isDefaultConstructor = 1;
    }
    return LEAVE(cp, np);
}


/*
 *  ClassName (485)
 *      ParameterisedTypeName
 *      ParameterisedTypeName !
 *  Input
 *
 *  AST
 */
static EcNode *parseClassName(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = parseParameterisedTypeName(cp);
    if (peekToken(cp) == T_LOGICAL_NOT) {
        getToken(cp);
    }
    return LEAVE(cp, np);
}


/*
 *  ParameterisedTypeName (487)
 *      Identifier
 *      Identifier TypeParameters
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseParameterisedTypeName(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (peekToken(cp) != T_ID) {
        getToken(cp);
        return LEAVE(cp, expected(cp, "identifier"));
    }
    np = parseIdentifier(cp);
    return LEAVE(cp, np);
}


/*
 *  ClassInheritance (489)
 *      EMPTY
 *      extends PrimaryName
 *      implements TypeIdentifierList
 *      extends PrimaryName implements TypeIdentifierList
 *
 *  Input
 *
 *  AST N_CLASS
 *      extends: id
 *      left: implements list if ids
 */
static EcNode *parseClassInheritance(EcCompiler *cp)
{
    EcNode      *np, *id;

    ENTER(cp);

    np = createNode(cp, N_CLASS);

    switch (getToken(cp)) {
    case T_EXTENDS:
        id = parsePrimaryName(cp);
        if (id) {
            np->klass.extends = mprStrdup(np, id->qname.name);
        }
        if (peekToken(cp) == T_IMPLEMENTS) {
            getToken(cp);
            np->klass.implements = linkNode(np, parseTypeIdentifierList(cp));
            mprStealBlock(np, np->klass.implements);
        }
        break;

    case T_IMPLEMENTS:
        np->klass.implements = linkNode(np, parseTypeIdentifierList(cp));
        mprStealBlock(np, np->klass.implements);
        break;

    default:
        putToken(cp);
        break;
    }
    return LEAVE(cp, np);
}


/*
 *  TypeIdentifierList (493)
 *      PrimaryName
 *      PrimaryName , TypeIdentifierList
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseTypeIdentifierList(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    np = createNode(cp, N_TYPE_IDENTIFIERS);
    while (peekToken(cp) == T_ID) {
        np = appendNode(np, parsePrimaryName(cp));
        if (peekToken(cp) != T_COMMA) {
            break;
        }
        getToken(cp);
    }

    /*
     *  Discard the first NOP node
     */
    return LEAVE(cp, np);
}


/*
 *  ClassBody (495)
 *      Block -class-
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseClassBody(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    cp->state->inFunction = 0;

    if (peekToken(cp) != T_LBRACE) {
        getToken(cp);
        return LEAVE(cp, expected(cp, "class body { }"));
    }

    np = parseBlock(cp);
    if (np) {
        np = np->left;
        mprAssert(np->kind == N_DIRECTIVES);
    }

    return LEAVE(cp, np);
}


/*
 *  InterfaceDefinition (496)
 *      interface ClassName InterfaceInheritance InterfaceBody
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseInterfaceDefinition(EcCompiler *cp, EcNode *attributeNode)
{
    EcState     *state;
    EcNode      *np, *classNameNode, *inheritance;

    ENTER(cp);
    
    state = cp->state;

    np = 0;
    if (getToken(cp) != T_INTERFACE) {
        return LEAVE(cp, expected(cp, "interface"));
    }
    
    if (peekToken(cp) != T_ID) {
        getToken(cp);
        return LEAVE(cp, expected(cp, "identifier"));
    }

    np = createNode(cp, N_CLASS);
    state->currentClassNode = np;
    state->topVarBlockNode = np;
    cp->classState = state;
    state->defaultNamespace = NULL;
    
    classNameNode = parseClassName(cp);
    if (classNameNode == 0) {
        return LEAVE(cp, 0);
    }

    applyAttributes(cp, np, attributeNode, 0);
    setNodeDoc(cp, np);
    
    np->qname.name = mprStrdup(np, classNameNode->qname.name);
    np->klass.isInterface = 1;
    state->currentClassName.name = np->qname.name;
    state->inInterface = 1;
    
    if (peekToken(cp) == T_EXTENDS) {
        inheritance = parseInterfaceInheritance(cp);
        if (inheritance->klass.extends) {
            np->klass.extends = mprStrdup(np, inheritance->klass.extends);
        }
    }

    if (peekToken(cp) != T_LBRACE) {
        getToken(cp);
        return LEAVE(cp, expected(cp, "{"));
    }

    np = appendNode(np, parseInterfaceBody(cp));

    return LEAVE(cp, np);
}


/*
 *  InterfaceInheritance (497)
 *      EMPTY
 *      extends TypeIdentifierList
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseInterfaceInheritance(EcCompiler *cp)
{
    EcNode      *np, *id;

    ENTER(cp);

    np = createNode(cp, N_CLASS);

    if (peekToken(cp) == T_EXTENDS) {
        id = parseTypeIdentifierList(cp);
        if (id) {
            np->klass.extends = mprStrdup(np, id->qname.name);
        }
    }
    return LEAVE(cp, np);
}


/*
 *  InterfaceBody (499)
 *      Block -interface-
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseInterfaceBody(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);

    if (peekToken(cp) != T_LBRACE) {
        getToken(cp);
        return LEAVE(cp, expected(cp, "interface body { }"));
    }

    np = parseBlock(cp);
    if (np) {
        np = np->left;
        mprAssert(np->kind == N_DIRECTIVES);
    }

    return LEAVE(cp, np);
}


/*
 *  NamespaceDefinition (500)
 *      namespace Identifier NamespaceInitialisation
 *
 *  Input
 *      namespace
 *
 *  AST
 *      N_NAMESPACE
 */
static EcNode *parseNamespaceDefinition(EcCompiler *cp, EcNode *attributeNode)
{
    EcNode      *varDefNode, *assignNode, *nameNode, *typeNode, *namespaceNode, *parent;
    EjsVar      *vp;

    ENTER(cp);

    if (getToken(cp) != T_NAMESPACE) {
        return LEAVE(cp, unexpected(cp));
    }

    /*
     *  Handle namespace definitions like:
     *      let NAME : Namespace = NAMESPACE_LITERAL
     */
    nameNode = parseIdentifier(cp);
    nameNode->name.isNamespace = 1;
    setNodeDoc(cp, nameNode);

    /*
     *  Hand-craft a "Namespace" type node
     */
    typeNode = createNode(cp, N_QNAME);
    typeNode->qname.name = "Namespace";
    nameNode->typeNode = linkNode(nameNode, typeNode);
    applyAttributes(cp, nameNode, attributeNode, 0);

    if (peekToken(cp) == T_ASSIGN) {
        namespaceNode = parseNamespaceInitialisation(cp, nameNode);

    } else {
        /*
         *  Create a namespace literal node from which to assign.
         */
        namespaceNode = createNode(cp, N_LITERAL);
        vp = (EjsVar*) ejsCreateNamespace(cp->ejs, nameNode->qname.name, nameNode->qname.name);
        namespaceNode->literal.var = vp;
        nameNode->name.value = vp;
    }

    parent = createNode(cp, N_ASSIGN_OP);
    assignNode = createAssignNode(cp, nameNode, namespaceNode, parent);

    varDefNode = createNode(cp, N_VAR_DEFINITION);
    varDefNode->def.varKind = KIND_VAR;

    varDefNode = appendNode(varDefNode, assignNode);

    return LEAVE(cp, varDefNode);
}


/*
 *  NamespaceInitialisation (501)
 *      EMPTY
 *      = StringLiteral
 *      = SimpleQualifiedName
 *
 *  Input
 *      =
 *
 *  AST
 *      N_LITERAL
 *      N_QNAME
 *      N_DOT
 */
static EcNode *parseNamespaceInitialisation(EcCompiler *cp, EcNode *nameNode)
{
    EcNode      *np;
    EjsVar      *vp;

    ENTER(cp);

    if (getToken(cp) != T_ASSIGN) {
        return LEAVE(cp, unexpected(cp));
    }

    if (peekToken(cp) == T_STRING) {
        getToken(cp);
        np = createNode(cp, N_LITERAL);
        vp = (EjsVar*) ejsCreateNamespace(cp->ejs, nameNode->qname.name, mprStrdup(np, (char*) cp->token->text));
        np->literal.var = vp;

    } else {
        np = parsePrimaryName(cp);
    }

    return LEAVE(cp, np);
}


/*
 *  TypeDefinition (504)
 *      type ParameterisedTypeName TypeInitialisation
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseTypeDefinition(EcCompiler *cp, EcNode *attributeNode)
{
    EcNode      *np;

    ENTER(cp);
    np = parseTypeInitialisation(cp);
    return LEAVE(cp, np);
}


/*
 *  TypeInitialisation (505)
 *      = NullableTypeExpression
 *
 *  Input
 *
 *  AST
 */
static EcNode *parseTypeInitialisation(EcCompiler *cp)
{
    EcNode      *np;

    ENTER(cp);
    np = 0;
    mprAssert(0);
    return LEAVE(cp, np);
}


/*
 *  ModuleDefinition (493)
 *      module ModuleBody
 *      module ModuleName ModuleBody
 *
 *  Input
 *      module ...
 *
 *  AST
 *      N_MODULE
 */
static EcNode *parseModuleDefinition(EcCompiler *cp)
{
    EcNode      *np, *moduleName, *body;
    cchar       *name, *namespace;
    int         next, isDefault, pos, version;

    ENTER(cp);
    version = 0;

    if (getToken(cp) != T_MODULE) {
        return LEAVE(cp, unexpected(cp));
    }
    np = createNode(cp, N_MODULE);

    if (peekToken(cp) == T_ID) {
        isDefault = 0;
        moduleName = parseModuleName(cp);
        if (moduleName == 0) {
            return LEAVE(cp, 0);
        }
        version = 0;
        if (peekToken(cp) == T_STRING || cp->peekToken->tokenId == T_NUMBER) {
            if ((version = parseVersion(cp, 0)) < 0) {
                expected(cp, "A version number NUM[.NUM[.NUM]]");
                return 0;
            }
        }
        np->module.name = mprStrdup(np, moduleName->qname.name);
        if (version) {
            namespace = mprAsprintf(np, -1, "%s-%d", moduleName->qname.name, version);
        } else {
            namespace = mprStrdup(np, moduleName->qname.name);
        }
    } else {
        isDefault = 1;
    }
    
    if (isDefault) {
        /*
         *  No module name. Set the namespace to the unique internal namespace name.
         */
        np->module.name = mprStrdup(np, EJS_DEFAULT_MODULE);
        namespace = cp->fileState->namespace;
    }
    np->qname.name = np->module.name;
    np->module.version = version;
    cp->state->defaultNamespace = namespace;

    body = parseModuleBody(cp);
    if (body == 0) {
        return LEAVE(cp, 0);
    }
    
    /* 
     *  Append the module namespace and also modules provided via ec/ejs --use switch
     */
    pos = 0;
    if (!isDefault) {
        body = insertNode(body, createNamespaceNode(cp, cp->fileState->namespace, 0, 1), pos++);
    }
    body = insertNode(body, createNamespaceNode(cp, namespace, 1, 1), pos++);
    for (next = 0; (name = mprGetNextItem(cp->useModules, &next)) != 0; ) {
        body = insertNode(body, createNamespaceNode(cp, name, 0, 1), pos++);
    }
    
    mprAssert(body->kind == N_BLOCK);
    np = appendNode(np, body);

    return LEAVE(cp, np);
}


/*
 *  ModuleName (494)
 *      Identifier
 *      ModuleName . Identifier
 *
 *  Input
 *      ID
 *      ID. ... .ID
 *
 *  AST
 *      N_QNAME
 *          name: name
 */
static EcNode *parseModuleName(EcCompiler *cp)
{
    EcNode      *np, *idp;
    Ejs *ejs;
    EjsVar      *lastPackage;
    char        *name;

    ENTER(cp);

    np = parseIdentifier(cp);
    if (np == 0) {
        return LEAVE(cp, np);
    }
    name = mprStrdup(np, np->qname.name);

    ejs = cp->ejs;
    lastPackage = 0;

    while (np && getToken(cp) == T_DOT) {
        /*
         *  Stop if not "identifier"
         */
        if (peekAheadToken(cp, 1) != T_ID) {
            break;
        }
        idp = parseIdentifier(cp);
        if (idp == 0) {
            return LEAVE(cp, idp);
        }
        np = appendNode(np, idp);
        name = mprReallocStrcat(np, -1, name, ".", idp->qname.name, NULL);
    }
    putToken(cp);
    setId(np, name);
    return LEAVE(cp, np);
}


/*
 *  ModuleBody (496)
 *      Block -global-
 *
 *  Input
 *      {
 *
 *  AST
 *      N_BLOCK
 */
static EcNode *parseModuleBody(EcCompiler *cp)
{
    return parseBlock(cp);
}


/*
 *  Pragma (505)
 *      UsePragma Semicolon |
 *      ImportPragma Semicolon
 *
 *  Input
 *      use ...
 *      import ...
 *
 *  AST
 *      N_IMPORT
 *      N_PRAGMAS
 */
static EcNode *parsePragma(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    switch (peekToken(cp)) {
    case T_USE:
    case T_REQUIRE:
        np = parseUsePragma(cp, np);
        break;

    default:
        np = unexpected(cp);
        break;
    }
    return LEAVE(cp, np);
}


/*
 *  Pragmas (497)
 *      Pragma
 *      Pragmas Pragma
 *
 *  Input
 *      use ...
 *      import
 *
 *  AST
 *      N_PRAGMAS
 *          N_IMPORT
 *          N_PRAGMA
 */
static EcNode *parsePragmas(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    while (peekToken(cp) == T_USE || cp->peekToken->tokenId == T_REQUIRE) {
        np = parsePragma(cp, np);
        if (np == 0) {
            break;
        }
    }
    return LEAVE(cp, np);
}


/*
 *  UsePragma (501)
 *      use PragmaItems
 *      require
 *
 *  Input
 *      use ...
 *
 *  AST
 *      N_PRAGMAS
 */
static EcNode *parseUsePragma(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    if (peekToken(cp) == T_REQUIRE) {
        getToken(cp);
        np = parseRequireItems(cp, np);
    } else if (peekToken(cp) == T_USE) {
        getToken(cp);
        np = parsePragmaItems(cp, np);
    } else{
        getToken(cp);
        np = parseError(cp, "Expecting \"use\" or \"require\"");
    }
    return LEAVE(cp, np);
}


static EcNode *parseRequireItems(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);
    do {
        np = appendNode(np, parseRequireItem(cp));
    } while (peekToken(cp) == T_COMMA);
    return LEAVE(cp, np);
}


/*
 *  Parse "NUM[.NUM[.NUM]]" and return a version number. Return < 0 on parse errors.
 */
static int parseVersion(EcCompiler *cp, int parseMax)
{
    char    *str, *p, *next;
    int     major, minor, patch;

    if (parseMax) {
        major = minor = patch = EJS_VERSION_FACTOR - 1;
    } else {
        major = minor = patch = 0;
    }
    if (getToken(cp) != T_STRING && cp->token->tokenId != T_NUMBER) {
        return MPR_ERR_BAD_VALUE;
    }

    str = mprStrdup(cp, (char*) cp->token->text);
    if ((p = mprStrTok(str, ".", &next)) != 0) {
        major = (int) mprAtoi(p, 10);
    }
    if ((p = mprStrTok(next, ".", &next)) != 0) {
        minor = (int) mprAtoi(p, 10);
    }
    if ((p = mprStrTok(next, ".", &next)) != 0) {
        patch = (int) mprAtoi(p, 10);
    }
    mprFree(str);
    return EJS_MAKE_VERSION(major, minor, patch);
}


/*
 *  Parse [version:version]. Valid forms include:
 *      [version]
 *      [:version]
 *      [version:version]
 */
static int parseVersions(EcCompiler *cp, int *minVersion, int *maxVersion)
{
    *minVersion = 0;
    *maxVersion = EJS_MAX_VERSION;

    getToken(cp);
    if (peekToken(cp) != T_COLON) {
        if ((*minVersion= parseVersion(cp, 0)) < 0) {
            expected(cp, "A version number NUM[.NUM[.NUM]]");
            return MPR_ERR_BAD_VALUE;
        }
    }
    if (peekToken(cp) == T_COLON) {
        getToken(cp);
        if ((*maxVersion = parseVersion(cp, 1)) < 0) {
            expected(cp, "A version number NUM[.NUM[.NUM]]");
            return MPR_ERR_BAD_VALUE;
        }
    }
    if (getToken(cp) != T_RBRACKET) {
        expected(cp, "]");
    }
    return 0;
}


static EcNode *parseRequireItem(EcCompiler *cp)
{
    EcNode      *np, *ns, *moduleName;
    int         minVersion, maxVersion;

    ENTER(cp);
    
    np = createNode(cp, N_USE_MODULE);
    ns = createNode(cp, N_USE_NAMESPACE);
    np->useModule.minVersion = 0;
    np->useModule.maxVersion = EJS_MAX_VERSION;

    moduleName = parseModuleName(cp);
    np->qname.name = mprStrdup(np, moduleName->qname.name);

    /*
     *  Optional [version:version]
     */
    if (peekToken(cp) == T_LBRACKET) {
        if (parseVersions(cp, &minVersion, &maxVersion) < 0) {
            return 0;
        }
        np->useModule.minVersion = minVersion;
        np->useModule.maxVersion = maxVersion;
    }

    ns->qname.name = mprStrdup(ns, np->qname.name);
    ns->useNamespace.isLiteral = 1;
    np = appendNode(np, ns);
    return LEAVE(cp, np);
}


/*
 *  PragmaItems (502)
 *      PragmaItem
 *      PragmaItems , PragmaItem
 *
 *  Input
 *      decimal
 *      default
 *      namespace
 *      standard
 *      strict
 *      module
 *      language
 *
 *  AST
 *      N_PRAGMAS
 *      N_MODULE
 */
static EcNode *parsePragmaItems(EcCompiler *cp, EcNode *np)
{
    ENTER(cp);

    do {
        np = appendNode(np, parsePragmaItem(cp));
    } while (peekToken(cp) == T_COMMA);
    return LEAVE(cp, np);
}


/*
 *  PragmaItem (504)
 *      decimal LeftHandSideExpression
 *      default namespace PrimaryName
 *      // default number [decimal | default | double | int | long | uint | ulong]
 *      namespace PrimaryName
 *      standard
 *      strict
 *      module ModuleName OptionalStringLiteral
 *      language ecma | plus | fixed
 *
 *  Input
 *      See above
 *
 *  AST
 *      N_PRAGMA
 *      N_MODULE
 */
static EcNode *parsePragmaItem(EcCompiler *cp)
{
    EcNode      *np, *ns, *lang;
    EcState     *upper;
    int         attributes;

    ENTER(cp);

    attributes = 0;

    np = createNode(cp, N_PRAGMA);
    np->pragma.mode = cp->fileState->mode;
    np->pragma.lang = cp->fileState->lang;

    /*
     *  PragmaIdentifiers (737)
     */
    switch (getToken(cp)) {
    case T_DECIMAL:
        np->pragma.decimalContext = linkNode(np, parseLeftHandSideExpression(cp));
        break;

    case T_DEFAULT:
        getToken(cp);
        if (cp->token->tokenId == T_NAMESPACE) {
            if (peekToken(cp) == T_STRING) {
                getToken(cp);
                np = createNode(cp, N_USE_NAMESPACE);
                np->qname.name = mprStrdup(np, (char*) cp->token->text);
                np->useNamespace.isLiteral = 1;

            } else {
                ns = parsePrimaryName(cp);
                if (ns) {
                    np = createNode(cp, N_USE_NAMESPACE);
                    np->qname.name = mprStrdup(np, ns->qname.name);
                }
            }
            if (np) {
                /*
                 *  Must apply this default namespace upwards to all blocks below the blockState. It will define the
                 *  new namespace value. Note that functions and classes null this so it does not propagate into classes or
                 *  functions.
                 */
                for (upper = cp->state->prev; upper; upper = upper->prev) {
                    upper->defaultNamespace = np->qname.name;
                    if (upper == cp->blockState) {
                        break;
                    }
                }
                cp->blockState->namespace = np->qname.name;
                np->useNamespace.isDefault = 1;
            }
        }
        break;

    case T_STANDARD:
        np->pragma.mode = PRAGMA_MODE_STANDARD;
        cp->fileState->mode = np->pragma.mode;
        break;

    case T_STRICT:
        np->pragma.mode = PRAGMA_MODE_STRICT;
        cp->fileState->mode = np->pragma.mode;
        break;

    case T_NAMESPACE:
        np = createNode(cp, N_USE_NAMESPACE);
        if (peekToken(cp) == T_STRING) {
            getToken(cp);
            np->qname.name = mprStrdup(np, (char*) cp->token->text);
            np->useNamespace.isLiteral = 1;

        } else {
            ns = parsePrimaryName(cp);
            if (ns) {
                np = appendNode(np, ns);
                if (ns->kind == N_DOT) {
                    np->qname.name = ns->right->qname.name;
                } else {
                    np->qname.name = ns->qname.name;
                }
            }
        }
        break;

    case T_MODULE:
        np = parseRequireItem(cp);
        break;

    case T_LANG:
        lang = parseIdentifier(cp);
        if (lang == 0) {
            return LEAVE(cp, lang);
        }
        np->pragma.lang = cp->fileState->lang;
        if (strcmp(lang->qname.name, "ecma") == 0) {
            cp->fileState->lang = np->pragma.lang = EJS_SPEC_ECMA;
        } else if (strcmp(lang->qname.name, "plus") == 0) {
            cp->fileState->lang = np->pragma.lang = EJS_SPEC_PLUS;
        } else if (strcmp(lang->qname.name, "fixed") == 0) {
            cp->fileState->lang = np->pragma.lang = EJS_SPEC_FIXED;
        } else {
            np = parseError(cp, "Unknown language specification ");
        }
        break;

    default:
        np = parseError(cp, "Unknown pragma identifier");
    }
    return LEAVE(cp, np);
}


/*
 *  Block -t- (514)
 *      { Directives }
 *
 *  Input
 *      {
 *
 *  AST
 *      N_BLOCK
 */
static EcNode *parseBlock(EcCompiler *cp)
{
    EcNode      *np;
    EcState     *state, *saveState;

    ENTER(cp);

    state = cp->state;
    saveState = cp->blockState;
    cp->blockState = state;

    if (getToken(cp) != T_LBRACE) {
        // putToken(cp);
        np = parseError(cp, "Expecting \"{\"");

    } else {
        np = createNode(cp, N_BLOCK);
        np = appendNode(np, parseDirectives(cp));

        if (np) {
            if (getToken(cp) != T_RBRACE) {
                // putToken(cp);
                np = parseError(cp, "Expecting \"}\"");
            }
        }
    }
    cp->blockState = saveState;
    return LEAVE(cp, np);
}


/*
 *  Program (515)
 *      Directives -global-
 *
 *  Input
 *      AnyDirective
 *
 *  AST N_PROGRAM
 *      N_DIRECTIVES ...
 *
 */
static EcNode *parseProgram(EcCompiler *cp, cchar *path)
{
    EcState     *state;
    EcNode      *np, *module, *block;
    cchar       *name;
    int         next;

    ENTER(cp);

    state = cp->state;
    np = createNode(cp, N_PROGRAM);

    /*
     *  Create an unforgeable internal namespace name
     */
    np->qname.space = EJS_PUBLIC_NAMESPACE;

    if (cp->fileState->lang == EJS_SPEC_ECMA) {
        np->qname.name = EJS_PUBLIC_NAMESPACE;
    } else {
        if (path) {
            np->qname.name = mprAsprintf(np, -1, "%s-%d", EJS_INTERNAL_NAMESPACE, cp->uid++);
        } else {
            np->qname.name = EJS_INTERNAL_NAMESPACE;
        }
    }
    state->namespace = np->qname.name;
    cp->fileState->namespace = state->namespace;

    /*
     *  Create the default module node
     */
    module = createNode(cp, N_MODULE);
    module->qname.name = EJS_DEFAULT_MODULE;

    /*
     *  Create a block to hold the namespaces. Add a use namespace node for the default module and add modules specified 
     *  via --use switch
     */
    block = createNode(cp, N_BLOCK);
    block = appendNode(block, createNamespaceNode(cp, cp->fileState->namespace, 0, 1));
    for (next = 0; (name = mprGetNextItem(cp->useModules, &next)) != 0; ) {
        block = appendNode(block, createNamespaceNode(cp, name, 0, 1));
    }

    block = appendNode(block, parseDirectives(cp));
    module = appendNode(module, block);
    np = appendNode(np, module);

    if (!cp->interactive && peekToken(cp) != T_EOF) {
        if (np) {
            np = unexpected(cp);
        }
        return LEAVE(cp, np);
    }

    /*
     *  Reset the line number to prevent debug source lines preceeding these elements
     */
    if (np) {
        np->lineNumber = 0;
    }
    if (module) {
        module->lineNumber = 0;
    }
    if (block) {
        block->lineNumber = 0;
    }
    return LEAVE(cp, np);
}


/*
 *  Report an error. Return a null EcNode so callers can report an error and return the null in one statement.
 */
static EcNode *parseError(EcCompiler *cp, char *fmt, ...)
{
    EcToken     *tp;
    va_list     arg;
    char        *msg;

    va_start(arg, fmt);

    if ((msg = mprVasprintf(cp, 0, fmt, arg)) == NULL) {
        msg = "Memory allocation error";
    }
    cp->errorCount++;
    cp->error = 1;
    tp = cp->token;
    if (tp) {
        ecReportError(cp, "error", tp->filename, tp->lineNumber, tp->currentLine, tp->column, msg);
    } else {
        ecReportError(cp, "error", 0, 0, 0, 0, msg);
    }
    mprFree(msg);
    va_end(arg);
    return 0;
}


EcNode *ecParseWarning(EcCompiler *cp, char *fmt, ...)
{
    EcToken     *tp;
    va_list     arg;
    char        *msg;

    va_start(arg, fmt);

    if ((msg = mprVasprintf(cp, 0, fmt, arg)) == NULL) {
        msg = "Memory allocation error";
    }

    cp->warningCount++;

    tp = cp->token;
    ecReportError(cp, "warning", tp->filename, tp->lineNumber, tp->currentLine, tp->column, msg);

    mprFree(msg);
    va_end(arg);

    return 0;
}


/*
 *  Recover from a parse error to allow parsing to continue.
 */
EcNode *ecResetError(EcCompiler *cp, EcNode *np, bool eatInput)
{
    int     tid;

    mprAssert(cp->error);

    if (cp->error) {
        if (!cp->fatalError && cp->errorCount < EC_MAX_ERRORS) {
            cp->error = 0;
            np = createNode(cp, N_DIRECTIVES);
        }
    }


    /*
     *  Try to resync by eating input up to the next statement / directive
     */
    while (!cp->interactive) {
        tid = peekToken(cp);
        if (tid == T_SEMICOLON || tid == T_RBRACE || tid == T_RBRACKET || tid == T_RPAREN || tid == T_ERR || tid == T_EOF)  {
            break;
        }
        if (np && np->lineNumber < cp->peekToken->lineNumber) {
            /* Virtual semicolon */
            break;
        }
        getToken(cp);
    }

    return np;
}


/*
 *  Create a line of spaces with an "^" pointer at the current parse error.
 *  Returns an allocated buffer. Caller must free.
 */
static char *makeHighlight(EcCompiler *cp, char *src, int col)
{
    char    *p, *dest;
    int     tabCount, len, i;

    tabCount = 0;

    for (p = src; *p; p++) {
        if (*p == '\t') {
            tabCount++;
        }
    }

    len = (int) strlen(src) + (tabCount * cp->tabWidth);
    len = max(len, col);

    /*
     *  Allow for "^" to be after the last char, plus one null.
     */
    dest = (char*) mprAlloc(cp, len + 2);
    if (dest == 0) {
        mprAssert(dest);
        return src;
    }
    for (i = 0, p = dest; *src; src++, i++) {
        if (*src== '\t') {
            *p++ = *src;
        } else {
            *p++ = ' ';
        }
    }

    /*
     *  Cover the case where the ^ must go after the end of the input
     */
    if (col >= 0) {
        dest[col] = '^';
        if (p == &dest[col]) {
            ++p;
        }
        *p = '\0';
    }
    return dest;
}



void ecReportError(EcCompiler *cp, cchar *severity, cchar *filename, int lineNumber, char *currentLine, int column, char *msg)
{
    cchar   *appName;
    char    *highlightPtr, *errorMsg;
    int     errCode;

    errCode = 0;

    appName = mprGetAppName(cp);
    if (filename == 0 || *filename == '\0') {
        filename = "stdin";
    }
    if (currentLine) {
        highlightPtr = makeHighlight(cp, (char*) currentLine, column);
        errorMsg = mprAsprintf(cp, -1, "%s: %s: %d: %s: %s\n  %s  \n  %s\n", appName, filename, lineNumber, severity,
            msg, currentLine, highlightPtr);

    } else if (lineNumber >= 0) {
        errorMsg = mprAsprintf(cp, -1, "%s: %s: %d: %s: %s\n", appName, filename, lineNumber, severity, msg);

    } else {
        errorMsg = mprAsprintf(cp, -1, "%s: %s: 0: %s: %s\n", appName, filename, severity, msg);
    }
    cp->errorMsg = mprReallocStrcat(cp, -1, cp->errorMsg, errorMsg, NULL);
    mprPrintfError(cp, "%s", cp->errorMsg);
    mprBreakpoint();
}


/*
 *
 */
static void updateTokenInfo(EcCompiler *cp)
{
    mprAssert(cp);
    mprAssert(cp->input);

    cp->token = cp->input->token;

#if BLD_DEBUG
    /*
     *  Update source file and line number information.
     */
    if (cp->token) {
        cp->tokenName = tokenNames[cp->token->tokenId];
        cp->currentLine = cp->token->currentLine;
    }
#endif
}


/*
 *  Get the next input token. May have been previous obtained and putback.
 */
static int getToken(EcCompiler *cp)
{
    int         id;

    if (cp->fatalError) {
        return T_ERR;
    }
    id = ecGetToken(cp->input);
    updateTokenInfo(cp);

    cp->peekToken = 0;
#if BLD_DEBUG
    cp->peekTokenName = 0;
#endif
    return id;
}


/*
 *  Peek ahead (K) tokens and return the token id
 */
static int peekAheadToken(EcCompiler *cp, int ahead)
{
    EcToken     *token;

    token = peekAheadTokenStruct(cp, ahead);
    if (token == 0) {
        return EJS_ERR;
    }
    return token->tokenId;
}


int ecPeekToken(EcCompiler *cp)
{
    return peekAheadToken(cp, 1);
}


/*
 *  Peek ahead (K) tokens and return the token.
 */
static EcToken *peekAheadTokenStruct(EcCompiler *cp, int ahead)
{
    EcToken     *token, *currentToken, *tokens[EC_MAX_LOOK_AHEAD];
    int         i;

    mprAssert(ahead > 0 && ahead <= EC_MAX_LOOK_AHEAD);

    cp->peeking = 1;

    if (ahead == 1) {

        /*
         *  Fast look ahead of one token.
         */
        if (cp->input->putBack) {
#if BLD_DEBUG
            cp->peekTokenName = tokenNames[cp->input->putBack->tokenId];
#endif
            cp->peekToken = cp->input->putBack;
            return cp->input->putBack;
        }
    }

    /*
     *  takeToken will take the current token and remove it from the input
     *  We must preserve the current token throughout.
     */
    currentToken = ecTakeToken(cp->input);
    for (i = 0; i < ahead; i++) {
        if (ecGetToken(cp->input) < 0) {
            cp->peeking = 0;
            mprAssert(0);
            return 0;
        }
        tokens[i] = ecTakeToken(cp->input);
    }

    /*
     *  Peek at the token of interest
     */
    token = tokens[i - 1];

    for (i = ahead - 1; i >= 0; i--) {
        putSpecificToken(cp, tokens[i]);
    }

    if (currentToken) {
        ecPutSpecificToken(cp->input, currentToken);
        ecGetToken(cp->input);
        updateTokenInfo(cp);
    }

#if BLD_DEBUG
    cp->peekTokenName = tokenNames[token->tokenId];
#endif

    cp->peekToken = token;
    cp->peeking = 0;
    return token;
}


static void putToken(EcCompiler *cp)
{
    ecPutToken(cp->input);
}


static void putSpecificToken(EcCompiler *cp, EcToken *token)
{
    ecPutSpecificToken(cp->input, token);
}


/*
 *  Create a new node. This will be automatically freed when returning from a non-terminal production (ie. the state
 *  is destroyed). Returning results are preserved by stealing the node from the state memory context.
 *
 *  NOTE: we are using a tree based memory allocator with destructors.
 */
static EcNode *createNode(EcCompiler *cp, int kind)
{
    EcNode      *np;
    EcToken     *token;
    int         len;

    mprAssert(cp->state);

    np = mprAllocObjZeroed(cp->state, EcNode);
    if (np == 0) {
        cp->memError = 1;
        return 0;
    }

    np->seqno = cp->nextSeqno++;
    np->kind = kind;
    np->cp = cp;
    np->slotNum = -1;

#if BLD_DEBUG
    np->kindName = nodes[kind];
#endif

    np->lookup.slotNum = -1;

    /*
     *  Remember the current input token. Don't do for initial program and module nodes.
     */
    if (cp->token == 0 && cp->state->blockNestCount > 0) {
        getToken(cp);
        putToken(cp);
        peekToken(cp);
    }

    token = cp->token;
    if (token) {
        np->tokenId = token->tokenId;
        np->groupMask = token->groupMask;
        np->subId = token->subId;

#if BLD_DEBUG
        if (token->tokenId >= 0) {
            np->tokenName = tokenNames[token->tokenId];
        }
#endif
    }

    np->children = mprCreateList(np);

    if (token && token->currentLine) {
        np->filename = mprStrdup(np, token->filename);
        np->currentLine = mprStrdup(np, token->currentLine);
        len = (int) strlen(np->currentLine);
        if (len > 0 && np->currentLine[len - 1] == '\n') {
            np->currentLine[len - 1] = '\0';
        }
        np->lineNumber = token->lineNumber;
        np->column = token->column;
        mprLog(np, 9, "At line %d, token \"%s\", line %s", token->lineNumber, token->text, np->currentLine);
    }

    /*
     *  Per AST node type initialisation
     */
    switch (kind) {
    case N_LITERAL:
        break;
    }
    return np;
}


static void setNodeDoc(EcCompiler *cp, EcNode *np)
{
#if BLD_FEATURE_EJS_DOC
    Ejs     *ejs;

    ejs = cp->ejs;

    if (ejs->flags & EJS_FLAG_DOC && cp->input->doc) {
        np->doc = cp->input->doc;
        cp->input->doc = 0;
        mprStealBlock(np, np->doc);
    }
#endif
}


static void appendDocString(EcCompiler *cp, EcNode *np, EcNode *parameter, EcNode *value)
{
#if BLD_FEATURE_EJS_DOC
    char        *doc, arg[MPR_MAX_STRING];
    cchar       *defaultValue;
    int         found;
    
    if (!(cp->ejs->flags & EJS_FLAG_DOC)) {
        return;
    }
    if (np == 0 || parameter == 0 || parameter->kind != N_QNAME || value == 0) {
        return;
    }

    defaultValue = 0;
    if (value->kind == N_QNAME) {
        defaultValue = value->qname.name;
    } else if (value->kind == N_UNARY_OP) {
        if (value->left->kind == N_LITERAL) {
            if (value->tokenId == T_MINUS) {
                defaultValue = mprAsprintf(np, -1, "-%s", ejsToString(cp->ejs, value->left->literal.var)->value);
            }
        }
    } else if (value->kind == N_LITERAL) {
        defaultValue = ejsToString(cp->ejs, value->literal.var)->value;
    }
    if (defaultValue == 0) {
        defaultValue = "expression";
    }

    if (np->doc) {
        found = 0;
        mprSprintf(arg, sizeof(arg), "@param %s ", parameter->qname.name);
        if (strstr(np->doc, arg) != 0) {
            found++;
        } else {
            mprSprintf(arg, sizeof(arg), "@params %s ", parameter->qname.name);
            if (strstr(np->doc, arg) != 0) {
                found++;
            }
        }
        if (found) {
            doc = mprAsprintf(np, -1, "%s\n@default %s %s", np->doc, parameter->qname.name, defaultValue);
        } else {
            doc = mprAsprintf(np, -1, "%s\n@param %s\n@default %s %s", np->doc, parameter->qname.name,
                parameter->qname.name, defaultValue);
        }
        mprFree(np->doc);
        np->doc = doc;
    }
#endif
}


static void copyDocString(EcCompiler *cp, EcNode *np, EcNode *from)
{
#if BLD_FEATURE_EJS_DOC
    Ejs     *ejs;

    ejs = cp->ejs;

    if (ejs->flags & EJS_FLAG_DOC && from->doc) {
        np->doc = from->doc;
        from->doc = 0;
        mprStealBlock(np, np->doc);
    }
#endif
}


/*
 *  This is used outside the parser. It must reset the line number as the
 *  node will not correspond to any actual source code line;
 */
EcNode *ecCreateNode(EcCompiler *cp, int kind)
{
    EcNode  *node;

    node = createNode(cp, kind);
    if (node) {
        node->lineNumber = -1;
        node->currentLine = 0;
    }
    return node;
}


static EcNode *createNameNode(EcCompiler *cp, cchar *name, cchar *space)
{
    EcNode      *np;

    np = createNode(cp, N_QNAME);
    if (np) {
        np->qname.name = mprStrdup(np, name);
        np->qname.space = mprStrdup(np, space);
    }
    return np;
}


static EcNode *createNamespaceNode(EcCompiler *cp, cchar *name, bool isDefault, bool isLiteral)
{
    EcNode      *np;
    
    np = createNode(cp, N_USE_NAMESPACE);
    np->qname.name = mprStrdup(np, name);
    np->useNamespace.isDefault = isDefault;
    np->useNamespace.isLiteral = isLiteral;
    return np;
}


/*
 *  This is used outside the parser.
 */
EcNode *ecLinkNode(EcNode *np, EcNode *child)
{
    return linkNode(np, child);
}


/*
 *  Create a binary tree node.
 */
static EcNode *createBinaryNode(EcCompiler *cp, EcNode *lhs, EcNode *rhs, EcNode *parent)
{
    mprAssert(cp);
    mprAssert(lhs);
    mprAssert(parent);

    /*
     *  appendNode will return the parent if no error
     */
    parent = appendNode(parent, lhs);
    parent = appendNode(parent, rhs);

    return parent;
}


/*
 *  Create an assignment op node.
 */
static EcNode *createAssignNode(EcCompiler *cp, EcNode *lhs, EcNode *rhs, EcNode *parent)
{
    mprAssert(cp);
    mprAssert(lhs);
    mprAssert(parent);

    /*
     *  appendNode will return the parent if no error
     */
    parent = appendNode(parent, lhs);
    parent = appendNode(parent, rhs);

    return parent;
}


/*
 *  Add a child node. If an allocation error, return 0, otherwise return the
 *  parent node.
 */
static EcNode *appendNode(EcNode *np, EcNode *child)
{
    EcCompiler      *cp;
    MprList         *list;
    int             index;

    mprAssert(np != child);

    if (child == 0 || np == 0) {
        return 0;
    }
    list = np->children;

    cp = np->cp;

    index = mprAddItem(list, child);
    if (index < 0) {
        cp->memError = 1;
        return 0;
    }

    if (index == 0) {
        np->left = (EcNode*) list->items[index];
    } else if (index == 1) {
        np->right = (EcNode*) list->items[index];
    }
    child->parent = np;
    mprStealBlock(list, child);
    return np;
}


EcNode *ecAppendNode(EcNode *np, EcNode *child)
{
    return appendNode(np, child);
}


EcNode *ecChangeNode(EcNode *np, EcNode *oldNode, EcNode *newNode)
{
    EcNode      *child;
    MprList     *list;
    int         next, index;

    next = 0;
    while ((child = (EcNode*) mprGetNextItem(np->children, &next))) {
        if (child == oldNode) {
            index = next - 1;
            mprSetItem(np->children, index, newNode);
            mprStealBlock(np, newNode);
            list = np->children;
            if (index == 0) {
                np->left = (EcNode*) list->items[index];
            } else if (index == 1) {
                np->right = (EcNode*) list->items[index];
            }
            newNode->parent = np;
            return np;
        }
    }
    mprAssert(0);
    return 0;
}


/*
 *  Link a node. This only steals the node.
 */
static EcNode *linkNode(EcNode *np, EcNode *node)
{
    if (node == 0 || np == 0) {
        return 0;
    }
    node->parent = np;
    mprStealBlock(np, node);

    return node;
}


/*
 *  Insert a child node. If an allocation error, return 0, otherwise return the parent node.
 */
static EcNode *insertNode(EcNode *np, EcNode *child, int pos)
{
    EcCompiler      *cp;
    MprList         *list;
    int             index, len;

    if (child == 0 || np == 0) {
        return 0;
    }
    list = np->children;

    cp = np->cp;

    index = mprInsertItemAtPos(list, pos, child);
    if (index < 0) {
        cp->memError = 1;
        return 0;
    }

    len = mprGetListCount(list);
    if (len > 0) {
        np->left = (EcNode*) list->items[0];
    }
    if (len > 1) {
        np->right = (EcNode*) list->items[1];
    }
    child->parent = np;
    mprStealBlock(list, child);
    return np;
}


/*
 *  Remove a child node and return it.
 */
static EcNode *removeNode(EcNode *np, EcNode *child)
{
    EcCompiler      *cp;
    MprList         *list;
    int             index;

    if (child == 0 || np == 0) {
        return 0;
    }
    list = np->children;

    cp = np->cp;

    index = mprRemoveItem(list, child);
    mprAssert(index >= 0);

    if (index == 0) {
        np->left = np->right;
    } else if (index == 1) {
        np->right = 0;
    }
    child->parent = 0;
    return child;
}


/* XXX */

static void setId(EcNode *np, char *name)
{
    mprAssert(np);
    mprAssert(np->kind == N_QNAME || np->kind == N_VOID);
    mprAssert(name);

    if (np->qname.name != name) {
        mprFree((char*) np->qname.name);
        np->qname.name = mprStrdup(np, name);
    }
}



static EcNode *unexpected(EcCompiler *cp)
{
    int     junk = 0;

    /*
     *  This is just to avoid a Vxworks 5.4 linker bug. The link crashes when this function has no local vars.
     */
    dummy(junk);
    return parseError(cp, "Unexpected input \"%s\"", cp->token->text);
}


static EcNode *expected(EcCompiler *cp, const char *str)
{
    return parseError(cp, "Expected input \"%s\"", str);
}


static const char *getExt(const char *path)
{
    char    *cp;

    if ((cp = strrchr(path, '.')) != 0) {
        return cp;
    }
    return "";
}


static void applyAttributes(EcCompiler *cp, EcNode *np, EcNode *attributeNode, cchar *overrideNamespace)
{
    EcState     *state;
    cchar       *namespace;
    int         attributes;

    state = cp->state;

    attributes = 0;
    namespace = 0;

    if (attributeNode) {
        /*
         *  Attribute node passed in.
         */
        attributes = attributeNode->attributes;
        if (attributeNode->qname.space) {
            namespace = mprStrdup(np, attributeNode->qname.space);
        }
        if (attributeNode->literalNamespace) {
            np->literalNamespace = 1;
        }

    } else {
        /*
         *  "space"::var
         */
        if (np->qname.space) {
            namespace = np->qname.space;
        }
    }

    if (namespace == 0) {
        if (overrideNamespace) {
            namespace = overrideNamespace;
        } else if (cp->blockState->defaultNamespace) {
            namespace = cp->blockState->defaultNamespace;
        } else {
            namespace = cp->blockState->namespace;
        }
    }
    mprAssert(namespace && *namespace);

    if (state->inFunction) {
        /*
         *  Functions don't need qualification of private properties.
         */
        if (strcmp(namespace, EJS_PRIVATE_NAMESPACE) == 0) {
            namespace = (char*) mprStrdup(np, namespace);
        } else {
            namespace = (char*) mprStrdup(np, namespace);
        }

    } else if (state->inClass) {
        if (strcmp(namespace, EJS_INTERNAL_NAMESPACE) == 0) {
            namespace = mprStrdup(np, cp->fileState->namespace);

        } else if (strcmp(namespace, EJS_PRIVATE_NAMESPACE) == 0 || strcmp(namespace, EJS_PROTECTED_NAMESPACE) == 0) {
            namespace = (char*) ejsFormatReservedNamespace(np, &state->currentClassName, namespace);

        } else {
            namespace = (char*) mprStrdup(np, namespace);
        }

    } else {
        if (strcmp(namespace, EJS_INTERNAL_NAMESPACE) == 0) {
            namespace = mprStrdup(np, cp->fileState->namespace);

        } else {
            namespace = (char*) mprStrdup(np, namespace);
        }
    }
    np->qname.space = namespace;

    mprLog(np, 7, "Parser apply attributes namespace = \"%s\", current line %s", namespace, np->currentLine);
    mprAssert(np->qname.space && np->qname.space[0]);

    np->attributes |= attributes;
}


static void addTokenToBuf(EcCompiler *cp, EcNode *np)
{
    MprBuf      *buf;

    if (np) {
        buf = np->literal.data;
        mprPutStringToBuf(buf, (cchar*) cp->token->text);
        mprAddNullToBuf(buf);
        mprLog(cp, 7, "Literal: \n%s\n", buf->start);
    }
}


/*
 *  Reset the input. Eat all tokens, clear errors, exceptions and the result value. Used by ejs for console input.
 */
void ecResetInput(EcCompiler *cp)
{
    EcToken     *tp;

    while ((tp = cp->input->putBack) != 0 && (tp->tokenId == T_EOF || tp->tokenId == T_NOP)) {
        ecGetToken(cp->input);
    }
    cp->input->stream->flags &= ~EC_STREAM_EOL;

    cp->error = 0;
    cp->ejs->exception = 0;
    cp->ejs->result = cp->ejs->undefinedValue;
}


void ecSetOptimizeLevel(EcCompiler *cp, int level)
{
    cp->optimizeLevel = level;
}


void ecSetWarnLevel(EcCompiler *cp, int level)
{
    cp->warnLevel = level;
}


void ecSetDefaultMode(EcCompiler *cp, int mode)
{
    cp->defaultMode = mode;
}


void ecSetTabWidth(EcCompiler *cp, int width)
{
    cp->tabWidth = width;
}


void ecSetOutputFile(EcCompiler *cp, cchar *outputFile)
{
    if (outputFile) {
        mprFree(cp->outputFile);
        cp->outputFile = mprStrdup(cp, outputFile);
    }
}


void ecSetCertFile(EcCompiler *cp, cchar *certFile)
{
    mprFree(cp->certFile);
    cp->certFile = mprStrdup(cp, certFile);
}

/*
 *  Just part of a VxWorks 5.4 compiler bug to avoid a linker crash
 */
static void dummy(int junk) { }

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
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
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/compiler/ecParser.c"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/compiler/ecState.c"
 */
/************************************************************************/

/**
 *  ecState.c - Manage state for the parser
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */



/*
 *  Push the state onto the stack
 */
int ecPushState(EcCompiler *cp, EcState *newState)
{
    EcState     *prev;

    prev = cp->state;
    if (prev) {
        /*
         *  Copy inherited fields.
         *  OPT - could we just use structure assignment?
         */
        newState->inClass = prev->inClass;
        newState->inFunction = prev->inFunction;
        newState->captureBreak = prev->captureBreak;
        newState->inMethod = prev->inMethod;
        newState->blockIsMethod = prev->blockIsMethod;

        newState->stateLevel = prev->stateLevel;
        newState->currentClass = prev->currentClass;
        newState->currentClassNode = prev->currentClassNode;
        newState->currentClassName = prev->currentClassName;
        newState->currentModule = prev->currentModule;
        newState->currentFunction = prev->currentFunction;
        newState->currentFunctionName = prev->currentFunctionName;
        newState->currentFunctionNode = prev->currentFunctionNode;
        newState->topVarBlockNode = prev->topVarBlockNode;
        newState->currentObjectNode = prev->currentObjectNode;
        newState->onLeft = prev->onLeft;
        newState->needsValue = prev->needsValue;
        newState->needsStackReset = prev->needsStackReset;
        newState->code = prev->code;
        newState->varBlock = prev->varBlock;
        newState->optimizedLetBlock = prev->optimizedLetBlock;
        newState->letBlock = prev->letBlock;
        newState->letBlockNode = prev->letBlockNode;
        newState->conditional = prev->conditional;
        newState->instanceCode = prev->instanceCode;
        newState->instanceCodeBuf = prev->instanceCodeBuf;
        newState->staticCodeBuf = prev->staticCodeBuf;
        newState->mode = prev->mode;
        newState->lang = prev->lang;
        newState->inheritedTraits = prev->inheritedTraits;
        newState->disabled = prev->disabled;
        newState->inHashExpression = prev->inHashExpression;
        newState->inSettings = prev->inSettings;
        newState->noin = prev->noin;
        newState->blockNestCount = prev->blockNestCount;
        newState->namespace = prev->namespace;
        newState->defaultNamespace = prev->defaultNamespace;
        newState->breakState = prev->breakState;
        newState->inInterface = prev->inInterface;

    } else {
        newState->lang = cp->lang;
        newState->mode = cp->defaultMode;
    }
    newState->prev = prev;
    newState->stateLevel++;
    cp->state = newState;
    return 0;
}


/*
 *  Pop the state. Clear out old notes and put onto the state free list.
 */
EcState *ecPopState(EcCompiler *cp)
{
    EcState *prev, *state;

    state = cp->state;
    mprAssert(state);

    prev = state->prev;
    mprFree(state);
    return prev;
}


/*
 *  Enter a new level. For the parser, this is a new production rule. For the ASP processor or code generator, 
 *  it is a new AST node. Push old state and setup a new production state
 */
int ecEnterState(EcCompiler *cp)
{
    EcState *state;

    //  OPT - keep state free list for speed
    state = mprAllocObjZeroed(cp, EcState);
    if (state == 0) {
        mprAssert(state);
        //  TBD -- convenience function for this.
        cp->memError = 1;
        cp->error = 1;
        cp->fatalError = 1;
        /* Memory erorrs are reported globally */
        return MPR_ERR_NO_MEMORY;
    }
    if (ecPushState(cp, state) < 0) {
        cp->memError = 1;
        cp->error = 1;
        cp->fatalError = 1;
        return MPR_ERR_NO_MEMORY;
    }
    return 0;
}


/*
 *  Leave a level. Pop the state and pass back the current node.
 */
EcNode *ecLeaveStateWithResult(EcCompiler *cp, EcNode *np)
{
    /*
     *  Steal the result from the current state and pass back to be owned by the previous state.
     */
    if (cp->state->prev) {
        mprStealBlock(cp->state->prev, np);
    } else {
        mprAssert(cp->state);
        mprStealBlock(cp, np);
    }
    cp->state = ecPopState(cp);

    if (cp->fatalError || cp->error) {
        return 0;
    }
    return np;
}


/*
 *  Leave a level. Pop the state and pass back the current node.
 */
void ecLeaveState(EcCompiler *cp)
{
    cp->state = ecPopState(cp);
}


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
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
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/compiler/ecState.c"
 */
/************************************************************************/

