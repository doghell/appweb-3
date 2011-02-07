/*
    customize.h -- Place to override standard product defintions
 */

#ifndef _h_CUSTOMIZE_h
#define _h_CUSTOMIZE_h 1

/*
    Override definitions here

    Use this to define the default appweb.conf file
        #define BLD_FEATURE_CONFIGFILE  "/path/to/default/appweb.conf"

    Use this to define the default server root directory
        #define BLD_FEATURE_SERVERROOT  "/var/spool/chroot/jail"

    Use this to add define a function to initialize staticaly linked modules. The function signature is:
        extern MprModule *myModuleInit(MaHttp *http, cchar *path);

        #define BLD_STATIC_MODULE "myModuleInit"
 */

#if SAMPLE
extern MprModule *myModuleInit(MaHttp *http, cchar *path);
#define BLD_STATIC_MODULE "myModuleInit"
#endif

#endif /* _h_CUSTOMIZE_h */
