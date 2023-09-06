#if defined __exception_included
  #endinput
#endif
#define __exception_included

#define try 	if (__Try())
#define catch  	else for (__InitCatch(); __Catch(); __FreeCatch())

native void __Try();
native void __InitCatch();
native bool __Catch();
native void __FreeCatch();

public Extension __ext_spexceptions =
{
    name = "SP-Exceptions",
    file = "sp-exceptions.ext",
#if defined AUTOLOAD_EXTENSIONS
    autoload = 1,
#else
    autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
    required = 1,
#else
    required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS
public void __ext_spexceptions_SetNTVOptional()
{
	MarkNativeAsOptional("__Try");
	MarkNativeAsOptional("__InitCatch");
	MarkNativeAsOptional("__Catch");
	MarkNativeAsOptional("__FreeCatch");
}
#endif