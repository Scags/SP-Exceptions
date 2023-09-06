#include "natives.h"
#include "exception.h"
#include "plugin-context.h"
#include "environment.h"
#include "smx-v1-image.h"
#include "smx/smx-v1-opcodes.h"

template <typename ClassType, typename ParamType = cell_t>
static ClassType *ReadHandle(const ParamType param, IPluginContext *pContext, HandleType_t htype, HandleError *err = nullptr)
{
	Handle_t hndl = (Handle_t)param;
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());
	ClassType *h;
	HandleError errerr;
	HandleError *perrerr = err ? err : &errerr;
	*perrerr = handlesys->ReadHandle(hndl, htype, &sec, (void **)&h);
	return h;
}

cell_t Native_InitCatch(IPluginContext *, const cell_t *);

static cell_t Native_Try(IPluginContext* pContext, const cell_t *params)
{
	uintptr_t *stack = (uintptr_t *)_AddressOfReturnAddress();

	sp::PluginContext *pCtx = (sp::PluginContext *)pContext;
	Tryer *currtry = new Tryer(pCtx, stack);
	while (!g_TryStack.empty())
	{
		// Case example 1:
		/*
			try
			{
				return
			}
			catch
			{
			}
		*/
		// There is no cleanup code called
		// The only way to figure out if this happened is either
		// A: sp::Environment::top() or sp::Environment::FrameId is different or
		// B: the frm or sp of the current try is > the last

		Tryer *prev = g_TryStack.top();
		if (currtry->Env()->FrameId() != prev->Env()->FrameId())
		{
			// smutils->LogMessage(myself, "del frameid");
			delete prev;
			g_TryStack.pop();
		}
		else if (currtry->StackPtr() > prev->StackPtr() || currtry->FramePtr() > prev->FramePtr())
		{
			// smutils->LogMessage(myself, "del stk frm");
			delete prev;
			g_TryStack.pop();
		}
		// If this is the case, then it is assured that this nested try is
		// in the same function, so we can see if the cip is <= the previous
		// Thankfully goto was removed, otherwise this would be pretty much impossible
		// to detect strictly from an extension

		// Case example 1a:
		/*
			for (;;)
			{
				try	// < cip 100
				{
					for (;;)
					{
						try // < cip 108
						{
							break;
						}
						catch
						{}
					}
				}
				catch
				{}
			}
		*/
		else if (currtry->Cip() <= prev->Cip())
		{
			// smutils->LogMessage(myself, "del cip");
			delete prev;
			g_TryStack.pop();
		}
		else break;
	}
	// while (!g_TryStack.empty())
	// {
	// 	// Case example 2:
	// 	/*
	// 		try
	// 		{
	// 			SomeNativeThatThrowsAnError();
	// 		}
	// 		catch
	// 		{
	// 			return;
	// 		}
	// 	*/
	// 	// __FreeCatch() is not called,
	// 	// so we see if there's a Tryer with a MyExc that already
	// 	// exists, then go ahead and free that
	// 	// Note that there can be multiple of these, hence the while()
	// 	Tryer *prev = g_TryStack.top();
	// 	if (!prev->MyExc())
	// 		break;

	// 	// In the case of natives, use Tryer's identity
	// 	HandleSecurity sec(((IPluginContext *)prev->Context())->GetIdentity(), myself->GetIdentity());
	// 	smutils->LogMessage(myself, "prev");
	// 	HandleError e;
	// 	if ((e = handlesys->ReadHandle(prev->MyExc()->Handle(), g_ExceptionHandle, &sec, nullptr)) == HandleError_None)
	// 	{
	// 		smutils->LogMessage(myself, "preverror %d", e);
	// 		handlesys->FreeHandle(prev->MyExc()->Handle(), &sec);
	// 	}

	// 	delete prev;
	// 	g_TryStack.pop();
	// }

	// smutils->LogMessage(myself, "stack -> %p", stack);
	// smutils->LogMessage(myself, "top_ -> %p", currtry->Env()->top());
	// smutils->LogMessage(myself, "FrameId -> %p", currtry->Env()->FrameId());
	// smutils->LogMessage(myself, "sp -> %p", currtry->StackPtr());
	// smutils->LogMessage(myself, "fp -> %p", currtry->FramePtr());

	g_TryStack.push(currtry);

	currtry->CatchRet() = currtry->FindMatchingCatch(Native_InitCatch);

	if (currtry->CatchRet() == nullptr)
	{
		delete currtry;
		g_TryStack.pop();
		return pContext->ThrowNativeError("'try' declared with no matching 'catch' statement!");
	}

	if (currtry->Env()->debugbreak() == nullptr)
	{
		currtry->Env()->SetDebugBreakHandler(Tryer::DebugBreakHandler);
	}

	// So, regardless, an error is spewed into the error log
	// If we set the debugger to null, this won't happen
	// This will be set back in the dtor when no try's remain
	if (currtry->Env()->debugger())
	{
		currtry->OldListener() = currtry->Env()->debugger();
		currtry->Env()->SetDebugger(nullptr);
	}

	// __debugbreak();

	return 1;
}

static cell_t Native_InitCatch(IPluginContext *pContext, const cell_t *params)
{
	if (g_TryStack.empty())
	{
		// smutils->LogMessage(myself, "empty1");
		return pContext->ThrowNativeError("Do not call this native >:(");
	}

	// Get the matching try that has its catch address as this
	// native's return address
	void *retaddr = _ReturnAddress();
	// __debugbreak();
	Tryer *currtry;
	while (!g_TryStack.empty())
	{
		// Cleanup for edge cases similar to __Try
		currtry = g_TryStack.top();
		if (currtry->CatchRet() == retaddr)
			break;

		delete currtry;
		g_TryStack.pop();
	}

	// smutils->LogMessage(myself, "len = %d", g_TryStack.size());
	if (g_TryStack.empty())
	{
		// smutils->LogMessage(myself, "empty2");
		return pContext->ThrowNativeError("Do not call this native >:(");
	}

	// Now, the try stack is aligned so the very next __Catch call will
	// grab the proper one

	return 0;
}

static cell_t Native_Catch(IPluginContext* pContext, const cell_t *params)
{
	// smutils->LogMessage(myself, "catch %d", g_TryStack.size());
	if (g_TryStack.empty())
		return 0;

	Tryer *currtry = g_TryStack.top();
	if (currtry->Caught())
	{
		return 0;
	}

	// smutils->LogMessage(myself, "caught");
	currtry->Caught() = true;

	return 1;
}

static cell_t Native_FreeCatch(IPluginContext *pContext, const cell_t *params)
{
	if (g_TryStack.empty())
	{
		return pContext->ThrowNativeError("Do not call this native >:(");
	}

	// __debugbreak();

	return 0;
}

sp_nativeinfo_t g_Natives[] = {
	{"__Try", Native_Try},
	{"__InitCatch", Native_InitCatch},
	{"__Catch", Native_Catch},
	{"__FreeCatch", Native_FreeCatch},
	{NULL, NULL}};