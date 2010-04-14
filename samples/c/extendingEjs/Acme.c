#include    "ejs.h"
#include   "Acme.slots.h"

static int count = 10;

static EjsVar *countdown(Ejs *ejs, EjsVar *vp, int argc, EjsVar **argv)
{
    mprBreakpoint();
    print("IN COUNTDOWN");
    return (EjsVar*) ejsCreateNumber(ejs, count--);
}


MprModule *acmeModuleInit(Ejs *ejs)
{
    MprModule   *module;
    EjsType     *type;
    EjsName     qname;

    mprBreakpoint();
    mprLog(ejs, 1, "Loading Acme module");
    if ((module = mprCreateModule(ejs, "acme", BLD_VERSION, 0, 0, 0)) == 0) {
        return 0;
    }
    type = (EjsType*) ejsGetPropertyByName(ejs, ejs->global, ejsName(&qname, "Acme", "Rocket"));
    if (type == 0) {
        mprError(ejs, "Can't find type %s", qname.name);
        return 0;
    }
    ejsBindMethod(ejs, type, ES_Acme_Rocket_countdown, (EjsNativeFunction) countdown);
    return module;
}

