# SP-Exceptions
A POC that implements try/catch into SourcePawn

# NOTE
This is not intended to be used in a production environment. This exists solely as a proof-of-concept on how to manipulate pawn JIT from an extension.

As such, it contains bugs, memory leaks, and various other problems that prevent it from being a full-fledged product.

To summarize, do not use this unless you know what you're doing. I have no plans to support this, yet I'm providing it for educational purposes. Future SourceMod/SourcePawn builds can and will break this extension.

This was last tested on SourceMod 1.12.0.7041

# Usage
A very simple try/catch statement, in SourcePawn.

```cpp
#include <exception>

public void OnPluginStart()
{
	try
	{
		PrintToServer("Hello, ");
		LoadFromAddress(view_as< Address >(0), NumberType_Int32);
		PrintToServer("world!")
	}
	catch
	{
		PrintToServer("Sike!");
	}
	// Outputs>
	/*
		Hello,
		Sike!
	*/
}
```

Any errors that are thrown with `IPluginContext::ThrowNativeError` or `IPluginContext::ReportError` are caught and handled by the try block, forcing the instruction pointer to move to its partnering catch block.

The `try` and `catch` keywords are actually clever `#define` statements that call natives under the hood:

```cpp
#define try 	if (__Try())
#define catch  	else for (__InitCatch(); __Catch(); __FreeCatch())
```

Calling any of these natives manually will most likely induce a segfault.

Note, several edge cases can induce undefined behavior, such as:
- Multiple try/catch blocks inside of each other. E.g. `try {try{}catch{}} catch{}`.
- Breaking/returning in the middle of a try/catch block.

# Methodology, Explained for Nerds

In the SourceMod API, a framework is provided for setting your own debug break handler. When an error is reported via a native, `sp::Environment::DispatchReport` (which is invoked by any sort of error-throwing context API), invokes a callback if it exists. This callback is dubbed as the debug break handler, and can be freely set via `sp::Environment::SetDebugBreakHandler`.

When a `try` block is declared, the underlying `__Try()` native is invoked. In this native, a snapshot of the stack is taken. This snapshot contains some details about the SourcePawn stack and heap, yet also contains the return address which has instructions which can be manually scanned to find the matching `catch` statement. Then, a new debug break handler callback is set if it does not already exist.

When an exception occurs, and thus the debug break handler callback is invoked, a few things happen.

1. The exception code is cached and then replaced with 0/`SP_ERROR_NONE`. This is so that Pawn code can continue executing.
2. The stack pointer is acquired and scanned backwards to try and find a portion of it that matches the stack snapshot taken during the `__Try()` invocation. This is a volatile method but is the only surefire way to find the return address to the JIT compiled pawn code.
3. Once this return address is found on the stack, it is replaced with emitted assembly so that when the remainder of the C++-side code is run, it will return into this custom hook function via a ROP attack.

In this assembly function, the SP stack, heap, and frame pointers are rewound and are equivalent to what they were when the `try` block was declared.

Then, the return address is replaced once again with the return address from the `__Try()` stack snapshot so that when the function `ret`s, the IP will be back after the call to `__Try()`. However this time, instead of returning true, which `__Try()` always does, it will return false so that the `else` code (which `catch` is under the hood) will be called instead.
