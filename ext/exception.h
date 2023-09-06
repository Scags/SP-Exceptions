#pragma once

#include "extension.h"
#include "environment.h"
#include "plugin-context.h"
#include "x86/assembler-x86.h"

#include <stack>
#include <string>
#include <memory>

class Exception;
class Catcher;

class Tryer
{
public:
	Tryer(sp::PluginContext *pContext, uintptr_t *stackPtr)
		: m_Context(pContext),
		  m_Env((sp::Environment *)pContext->APIv2()->Environment()),
		  m_Handler(::new DetectExceptions(pContext)),
		  m_sp(pContext->sp()),
		  m_fp(pContext->frm()),
		  m_hp(pContext->hp()),
		  m_cip(Tryer::GetCip(pContext)),
		  m_StackPos(stackPtr),
		  m_Stackshot((void *)stackPtr)
	{
	}
	~Tryer();

	// Stack (16 bytes):
	//   12: Saved EDX
	//    8: Saved HP
	//    4: Cells
	//    0: Context
	//    -4: ret
	struct StackSnapshot
	{
		StackSnapshot() = default;
		StackSnapshot(void *stackptr)
		{
			memcpy(this, stackptr, sizeof(StackSnapshot));
		}
		intptr_t ret;
		intptr_t pcxt;
		intptr_t params;
		intptr_t hp;
		intptr_t edx;
	};

	std::shared_ptr<Exception> &MyExc();
	std::unique_ptr<Catcher> &MyCatch();
	IDebugListener *&OldListener();
	DetectExceptions *&Handler();
	sp::Environment *Env();
	uint8_t *&CatchRet();
	cell_t &StackPtr();
	cell_t &FramePtr();
	cell_t &HeapPtr();
	cell_t &Cip();
	uintptr_t *StackPos();
	StackSnapshot *Stackshot();
	sp::PluginContext *&Context();
	void MakeAsm();
	sp::Assembler *Asm();
	void *&Pivot();
	bool &Caught();

	uint8_t *Tryer::FindMatchingCatch(const void *);

	static void DebugBreakHandler(IPluginContext *, sp_debug_break_info_t &, const IErrorReport *);
	static cell_t GetCip(IPluginContext *);

private:
	sp::PluginContext *m_Context = {};
	std::shared_ptr<Exception> m_Exc = {};
	std::unique_ptr<Catcher> m_Catch = {};
	IDebugListener *m_OldListener = {};
	DetectExceptions *m_Handler = {};
	sp::Environment *m_Env = {};
	uint8_t *m_CatchRet = {};
	cell_t m_sp = {};
	cell_t m_fp = {};
	cell_t m_hp = {};
	cell_t m_cip = {};
	uintptr_t *m_StackPos = {};
	StackSnapshot m_Stackshot = {};
	std::unique_ptr<sp::Assembler> m_Assembler = {};
	void *m_Pivot = {};
	bool m_Caught = {};
};

class Catcher
{
public:
	Catcher(Tryer *tryer)
		: m_Tryer(tryer),
		  m_Env(tryer->Env())
	{
	}
	~Catcher();

	Tryer *&MyTry();
	std::shared_ptr<Exception> &MyExc();
	sp::Environment *Env();

private:
	Tryer *m_Tryer = {};
	std::shared_ptr<Exception> m_Exc = {};
	sp::Environment *m_Env = {};
};

class Exception
{
public:
	Exception(IPluginContext *pContext, sp_debug_break_info_t &info, const IErrorReport *report)
		: m_Ctx(pContext),
		  m_debuginfo(info),
		  m_info(report)
	{
	}
	Tryer *&MyTry();
	Catcher *&MyCatch();
	Handle_t &Handle();
	struct ReportInfo;
	ReportInfo *Info();

	struct ReportInfo
	{
		ReportInfo() = default;
		ReportInfo(const IErrorReport *report)
			: blame(report->Blame()),
			  code(report->Code()),
			  ctx(report->Context()),
			  fatal(report->IsFatal()),
			  message(report->Message())
		{
		}
		IPluginFunction *blame;
		int code;
		IPluginContext *ctx;
		bool fatal;
		std::string message;
	};

private:
	Tryer *m_Tryer = {};
	Catcher *m_Catch = {};
	IPluginContext *m_Ctx = {};
	Handle_t m_Hndl = {};
	sp_debug_break_info_t m_debuginfo = {};
	ReportInfo m_info = {};
};

extern std::stack<Tryer *> g_TryStack;