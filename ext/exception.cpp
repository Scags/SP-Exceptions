#include "exception.h"
#include "smx/smx-v1-opcodes.h"
#include "plugin-context.h"
#include "x86/assembler-x86.h"

std::stack<Tryer *> g_TryStack;

std::unique_ptr<Catcher> &Tryer::MyCatch()
{
	return m_Catch;
}

std::shared_ptr<Exception> &Tryer::MyExc()
{
	return m_Exc;
}

IDebugListener *&Tryer::OldListener()
{
	return m_OldListener;
}

DetectExceptions *&Tryer::Handler()
{
	return m_Handler;
}

sp::Environment *Tryer::Env()
{
	return m_Env;
}

uint8_t *&Tryer::CatchRet()
{
	return m_CatchRet;
}

cell_t &Tryer::StackPtr()
{
	return m_sp;
}

cell_t &Tryer::FramePtr()
{
	return m_fp;
}

cell_t &Tryer::HeapPtr()
{
	return m_hp;
}

cell_t &Tryer::Cip()
{
	return m_cip;
}

uintptr_t *Tryer::StackPos()
{
	return m_StackPos;
}

Tryer::StackSnapshot *Tryer::Stackshot()
{
	return &m_Stackshot;
}

sp::PluginContext *&Tryer::Context()
{
	return m_Context;
}

void Tryer::MakeAsm()
{
	if (m_Assembler == nullptr)
		m_Assembler = std::make_unique<sp::Assembler>();
}

sp::Assembler *Tryer::Asm()
{
	return m_Assembler.get();
}

void *&Tryer::Pivot()
{
	return m_Pivot;
}

bool &Tryer::Caught()
{
	return m_Caught;
}

// John you ding dong the function is already compiled

// bool Tryer::MakeTrampoline(cell_t cip, IPluginContext *pCtx)
// {
// 	ISourcePawnEngine *eng = Env()->APIv1();
// 	sp::PluginContext *spCtx = (sp::PluginContext *)pCtx;

// 	sp::PluginRuntime *pRuntime = (sp::PluginRuntime *)pCtx->GetRuntime();

// 	// Setup trampoline
// 	{
// 		const uint8_t *codebase = pRuntime->code().bytes;

// 		// Get stack ptr and calculate difference to restore
// 		cell_t sp = spCtx->sp();
// 		cell_t stackdiff = StackPtr() - sp;

// 		// Call a native
// 		// This is in case our midhook overwrites our catch or any other code
// 		// that might get called
// 		uint32_t excnative;
// 		// HACK; Since natives only get an index when a plugin uses them, we have to either
// 		// A: Force a way for a native that we make to get an index by a plugin, or
// 		// B: Reuse one of our natives
// 		// B is much easier
// 		if (pCtx->GetRuntime()->FindNativeByName("__Catch", &excnative) != SP_ERROR_NONE)
// 		{
// 			return false;
// 		}

// 		// Calculate address of catch statement
// 		cell_t jmpaddr = (cell_t)((ucell_t)CatchRet() - (ucell_t)codebase);

//		smutils->LogMessage(myself, "stackdiff -> %x", stackdiff);
//		smutils->LogMessage(myself, "sp -> %p", sp);
//		smutils->LogMessage(myself, "rewind sp -> %p", sp + stackdiff);
//		smutils->LogMessage(myself, "pjmp -> %p", CatchRet());
//		smutils->LogMessage(myself, "jmpaddr -> %p", jmpaddr);

// 		// With 0 debug break statements, no more exception handlers will be triggered
// 		// and execution can continue all the way until __Catch
// 		cell_t trampoline[] = {
// 			// stack 0xn
// 			sp::OP_STACK,
// 			stackdiff,
// 			// sysreq.n __ExceptionHandler 0x0
// 			sp::OP_SYSREQ_N,
// 			(cell_t)excnative,
// 			0x0,
// 			// jump jmpaddr
// 			sp::OP_JUMP,
// 			jmpaddr
// 		};

// 		m_Trampoline = (cell_t *)eng->AllocatePageMemory(sizeof(trampoline));

// 		if (!m_Trampoline)
// 			return false;

// 		memcpy(m_Trampoline, trampoline, sizeof(trampoline));
//		smutils->LogMessage(myself, "m_Trampoline -> %p", m_Trampoline);
// 	}

// 	// Setup the midhook
// 	{
// 		m_MidHook = (cell_t *)(pRuntime->code().bytes + (ucell_t)cip);
//		smutils->LogMessage(myself, "cipaddr -> %p", m_MidHook);
//		smutils->LogMessage(myself, "m_MidHook::op -> %x", *m_MidHook);
// 		m_MidHook += (*m_MidHook == sp::OP_SYSREQ_N ? 3 : 2);	// Size of sysreq.n/c instruction

//		smutils->LogMessage(myself, "m_MidHook -> %p", m_MidHook);

// 		// Save bytes that we're fixing to write over
// 		memcpy(m_ByteRestore, m_MidHook, sizeof(m_ByteRestore));

// 		cell_t trampoline = (cell_t)((uint8_t *)m_Trampoline - pRuntime->code().bytes);

// 		cell_t jmp[] = {
// 			sp::OP_JUMP,
// 			trampoline
// 		};

// 		memcpy(m_MidHook, jmp, sizeof(jmp));
// 	}

// 	return true;
// }

uint8_t *Tryer::FindMatchingCatch(const void *funcaddr)
{
	// We start from the return address of __Try, then scan 
	// ahead and see where the following call to __Catch is
	// This can somewhat be cheesed since the invocation of 'try'
	// returns true/false and because of that + the following
	// 'catch' else statement, the code location for the 'catch'
	// is presented all nicely for us to read
	// If it's a jz, the catch code is at the jmp target address
	// If it's a jnz, the catch code is the very next address

	uint8_t *start = (uint8_t *)Stackshot()->ret;
	uint8_t *p = start;
	uint8_t *jmpaddr{};
	bool foundLabel{};
	// smutils->LogMessage(myself, "start -> %p", start);
	// smutils->LogMessage(myself, "funcaddr -> %p", funcaddr);
	// Should I be using Zydis? Yes
	// Will I? No
	do
	{
		// test eax, eax
		// jz label
		if (!foundLabel && memcmp(p, "\x85\xC0\x0F\x84", 4) == 0)
		{
			jmpaddr = (uint8_t *)((intptr_t)(p + 2 + 2 + 4) + *(int32_t *)(p + 2 + 2));
			p = jmpaddr;
			foundLabel = true;
			// smutils->LogMessage(myself, "found label at %p", p);
		}
		// test eax, eax
		// jnz label
		else if (!foundLabel && memcmp(p, "\x85\xC0\x0F\x85", 4) == 0)
		{
			jmpaddr = p + 2 + 2 + 4;
			p = jmpaddr;
			foundLabel = true;
			// smutils->LogMessage(myself, "found label at %p", p);
		}
		// push PluginContext
		// call function
		else if (foundLabel && *p == 0x68 && p[5] == 0xE8)
		{
			void *targetaddr = (void *)((intptr_t)(p + 5 + 1 + 4) + *(int32_t *)(p + 5 + 1));
			// smutils->LogMessage(myself, "targetaddr -> %p", targetaddr);
			return targetaddr == funcaddr ? p + 5 + 1 + 4 : nullptr;
		}

		// Emergency
		if (abs(p - start) > 0x4000)
			return nullptr;

		// Ends with a jmp $-5
	} while (memcmp(p++, "\xE9\x00\x00\x00\x00", 5) != 0);

	return nullptr;
}

void Tryer::DebugBreakHandler(IPluginContext *pContext, sp_debug_break_info_t &info, const IErrorReport *report)
{
	if (g_TryStack.empty())
	{
		smutils->LogError(myself, "Empty try stack in an exception handler!");
		((sp::Environment *)pContext->APIv2()->Environment())->SetDebugBreakHandler(nullptr);
		return;
	}

	// Detect if this is takes place in a catch block
	Tryer *currtry = g_TryStack.top();
	{
		// Push back each try block (in the case of nested try/catch)
		std::stack<Tryer*> rstack;
		while (!g_TryStack.empty())
		{
			Tryer *backtry = g_TryStack.top();
			if (backtry->MyExc() != nullptr)
			{
				rstack.push(backtry);
				g_TryStack.pop();
			}
			// No exception here, so this will be the mutated try object
			else
			{
				currtry = backtry;
				break;
			}
		}

		// Uncaught, so we free every try at this point
		if (g_TryStack.empty())
		{
			// To dance around the dtor, we have to add them all
			// back to the stack then delete them
			while (!rstack.empty())
			{
				g_TryStack.push(rstack.top());
				rstack.pop();
			}

			while (!g_TryStack.empty())
			{
				delete g_TryStack.top();
				g_TryStack.pop();
			}

			// Done here, so return and let the exception handler do its thing
			return;
		}

		// currtry is updated here, so in either:
		// A: a new call to __Try or
		// B: a call to __FreeCatch
		// will clean up the remaining try objects
	}

	// smutils->LogMessage(myself, "DebugBreakHandler:");
	// smutils->LogMessage(myself, "\tcip -> %p", info.cip);
	// smutils->LogMessage(myself, "\tfrm -> %p", info.frm);
	// smutils->LogMessage(myself, "\tMessage -> %s", report->Message());

	currtry->MyExc() = std::make_shared<Exception>(pContext, info, report);

	// Clear exception code so we continue executing
	*(int *)currtry->Env()->addressOfExceptionCode() = SP_ERROR_NONE;

	// Alakazam!
	// #define MAGIC 0x4DC

	uint32_t *stack = (uint32_t *)_AddressOfReturnAddress();
	uint32_t *stackrewind = stack;

	// Scan the stack and look for our plugin context and heap pointer
	uint32_t *retaddr{};
	cell_t targethp = currtry->Context()->hp();
	do
	{
		// With the context found, along with the stored hp,
		// HOPEFULLY the previous dword in the stack is the
		// return address of the native that threw the error
		Tryer::StackSnapshot *snap = (Tryer::StackSnapshot *)stackrewind;
		// snap->params aka stk can be optimized away, so the
		// best we can do is look for pcxt and hp
		if (snap->pcxt == (intptr_t)pContext && snap->hp == targethp)
		{
			retaddr = (uint32_t *)&snap->ret;
			break;
		}
	} while (++stackrewind);

	// smutils->LogMessage(myself, "Snapshot::ret -> %p", currtry->Stackshot()->ret);
	// smutils->LogMessage(myself, "Snapshot::pcxt -> %p", currtry->Stackshot()->pcxt);
	// smutils->LogMessage(myself, "Snapshot::params -> %p", currtry->Stackshot()->params);
	// smutils->LogMessage(myself, "Snapshot::hp -> %p", currtry->Stackshot()->hp);
	// smutils->LogMessage(myself, "Snapshot::edx -> %p", currtry->Stackshot()->edx);
	// smutils->LogMessage(myself, "esi -> %p", currtry->Stackshot()->params /* stk */ - currtry->StackPtr());
	intptr_t esi = currtry->Stackshot()->params /* stk */ - currtry->StackPtr();

	// smutils->LogMessage(myself, "params -> %p", currtry->Stackshot()->params);
	// smutils->LogMessage(myself, "stackptr -> %p", currtry->StackPtr());
	// smutils->LogMessage(myself, "esi -> %p", esi);

	currtry->MakeAsm();
	currtry->Asm()->subl(sp::esp, (intptr_t)retaddr - (intptr_t)currtry->StackPos() + 4);
	currtry->Asm()->movl(sp::edi, currtry->StackPtr());
	currtry->Asm()->movl(sp::ebx, currtry->FramePtr() + esi);
	currtry->Asm()->movl(sp::esi, esi);
	currtry->Asm()->movl(sp::Operand(sp::esp, 0), currtry->Stackshot()->ret);
	currtry->Asm()->movl(sp::Operand(sp::esp, 12), currtry->Stackshot()->hp);
	currtry->Asm()->movl(sp::Operand(sp::esp, 16), currtry->Stackshot()->edx);
	currtry->Asm()->xorl(sp::eax, sp::eax);
	currtry->Asm()->ret();

	currtry->Pivot() = smutils->GetScriptingEngine()->AllocatePageMemory(currtry->Asm()->length());
	currtry->Asm()->emitToExecutableMemory(currtry->Pivot());

	// smutils->LogMessage(myself, "retaddr -> %p", retaddr);
	// smutils->LogMessage(myself, "retaddr -> %p", *(void **)retaddr);
	// smutils->LogMessage(myself, "pivot -> %p", currtry->Pivot());

	*(void **)retaddr = currtry->Pivot();

	// __debugbreak();
}

cell_t Tryer::GetCip(IPluginContext *pContext)
{
	// Hack to extract the cip from an IFrameIterator
	// Clone of sp::FrameIterator
	class FrameIteratorHack : public IFrameIterator
	{
	public:
		cell_t cip() { return frame_cursor_->cip(); }
	private:
		sp::InvokeFrame *ivk_;
		sp::PluginRuntime *runtime_;
		intptr_t *next_exit_fp_;
		std::unique_ptr<sp::InlineFrameIterator> frame_cursor_;
	};

	IFrameIterator *iter = pContext->CreateFrameIterator();

	cell_t cip;
	for (;!iter->Done(); iter->Next())
	{
		if (iter->IsScriptedFrame())
		{
			cip = ((FrameIteratorHack *)iter)->cip();
			break;
		}
	}

	pContext->DestroyFrameIterator(iter);
	return cip;
}

Tryer::~Tryer()
{
	// Size is updated after dtor
	if (g_TryStack.size() == 1)
	{
		if (OldListener())
			Env()->SetDebugger(OldListener());
		Env()->SetDebugBreakHandler(nullptr);
	}

	::operator delete(m_Handler);

	if (Pivot() != nullptr)
		smutils->GetScriptingEngine()->FreePageMemory(Pivot());

	// smutils->LogMessage(myself, "delete");
}

Tryer *&Catcher::MyTry()
{
	return m_Tryer;
}

std::shared_ptr<Exception> &Catcher::MyExc()
{
	return m_Exc;
}

sp::Environment *Catcher::Env()
{
	return m_Env;
}

Catcher::~Catcher()
{
	if (MyTry() != nullptr)
		MyTry()->MyCatch() = nullptr;

	if (MyExc() != nullptr)
		*(int *)Env()->addressOfExceptionCode() = MyExc()->Info()->code;
}

Tryer *&Exception::MyTry()
{
	return m_Tryer;
}

Catcher *&Exception::MyCatch()
{
	return m_Catch;
}

Handle_t &Exception::Handle()
{
	return m_Hndl;
}

Exception::ReportInfo *Exception::Info()
{
	return &m_info;
}
