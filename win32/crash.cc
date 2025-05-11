/**
**  win32/crash.cc - Windows Crash Handler
**/

/*
Copyright (C) 2025 The Exult Team

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA  02111-1307, USA.
*/

/* This file contans a crash handler written for Windows

If a crash occurs it will output basic details about the crash (what type,
where it was, contents of regisers, etc), it will verify game files and
finally attempt to make a crash save if in game

It is assumed that the process  is still stable and the Address space hasn't
been completely trashed. If the process is no longer stable,  then there isn't
thart can be done

*/
#include "Configuration.h"
#include "exceptions.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "listfiles.h"
#include "modmgr.h"
#include "server.h"
#include "utils.h"
#include "verify.h"

// Turm clang format off so it doesn't reorder the windows headers
// clang-format off
#include <Windows.h>
#include <DbgHelp.h>
// clang-format on

#include <csignal>
#include <exception>
#include <iostream>
#include <memory>
#include <thread>

static MINIDUMP_TYPE    miniDump_Type = MiniDumpNormal;
static EXCEPTION_RECORD ExceptionRecord;
static CONTEXT          ContextRecord;
static DWORD            FaultingThreadId;
static DWORD            MainThreadId;
static int              exception_count = 0;
static bool             was_signal      = false;

// This #if block contains code to simulate various types of crashes
#if 0
volatile int ivalue1          = 0;
volatile int ivalue2          = 0;
volatile int* volatile pValue = nullptr;

void DoCrash() {
	//throw std::exception();
	std::terminate();
	//std::abort();
	//*pValue = 0;
	//ivalue1 = ivalue2 / ivalue1;
}
#endif

static BOOL CALLBACK Minidumpcallback(
		PVOID, PMINIDUMP_CALLBACK_INPUT CallbackInput,
		PMINIDUMP_CALLBACK_OUTPUT) {
	// Exclude this thread from the minidump
	if (CallbackInput->CallbackType == IncludeThreadCallback
		&& CallbackInput->IncludeThread.ThreadId == GetCurrentThreadId()) {
		return FALSE;
	}
	return TRUE;
}

static void HandlerWorkerThreadFunc() {
	std::cerr << "Crash Details:" << std::endl << std::flush;

	// Suspend the Main thread if it wasn't the faulting thread
	// If it was the faulting thread it is already suspended waiting for us to
	// return
	if (FaultingThreadId != MainThreadId) {
		HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, MainThreadId);
		if (hThread) {
			SuspendThread(hThread);
			CloseHandle(hThread);
		}
	}
	if (was_signal) {
		was_signal = false;

		switch (ExceptionRecord.ExceptionCode) {
		case SIGABRT: {
			std::cerr << "SIGABORT" << std::endl << std::flush;
		} break;
		case SIGTERM: {
			std::cerr << "SIGTERM" << std::endl << std::flush;
		} break;

		default:
			std::cerr << "Signal: " << ExceptionRecord.ExceptionCode
					  << std::endl
					  << std::flush;
		}
	} else {
		switch (ExceptionRecord.ExceptionCode) {
		case EXCEPTION_ACCESS_VIOLATION: {
			std::cerr << "Access Violation ";
			if (ExceptionRecord.NumberParameters > 1
				&& ExceptionRecord.ExceptionInformation[0] == 0) {
				std::cerr << "reading from "
						  << PVOID(ExceptionRecord.ExceptionInformation[1])
						  << std::endl;
			}
			if (ExceptionRecord.NumberParameters > 1
				&& ExceptionRecord.ExceptionInformation[0] == 1) {
				std::cerr << "writing to "
						  << PVOID(ExceptionRecord.ExceptionInformation[1])
						  << std::endl;
			}
			if (ExceptionRecord.NumberParameters > 1
				&& ExceptionRecord.ExceptionInformation[0] == 8) {
				std::cerr << "executing from "
						  << PVOID(ExceptionRecord.ExceptionInformation[1])
						  << std::endl;
			} else {
			}
		} break;
		// In page error is not going to be a problem with our code.It pretty
		// much means wnidows was unable to read a page from storage
		case EXCEPTION_IN_PAGE_ERROR: {
			std::cerr << "In Page Error ";
			if (ExceptionRecord.NumberParameters > 1
				&& ExceptionRecord.ExceptionInformation[0] == 0) {
				std::cerr << "reading from "
						  << PVOID(ExceptionRecord.ExceptionInformation[1])
						  << std::endl;
			}
			if (ExceptionRecord.NumberParameters > 1
				&& ExceptionRecord.ExceptionInformation[0] == 1) {
				std::cerr << "writing to "
						  << PVOID(ExceptionRecord.ExceptionInformation[1])
						  << std::endl;
			}
			if (ExceptionRecord.NumberParameters > 1
				&& ExceptionRecord.ExceptionInformation[0] == 8) {
				std::cerr << "executing from "
						  << PVOID(ExceptionRecord.ExceptionInformation[1])
						  << std::endl;
			} else {
			}
		} break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO: {
			std::cerr << "Floating Point Divide by zero" << std::endl
					  << std::flush;
		} break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO: {
			std::cerr << "Integer  Divide by zero" << std::endl << std::flush;
		} break;
		case EXCEPTION_FLT_OVERFLOW: {
			std::cerr << "Floating point Overflow" << std::endl << std::flush;
		} break;
		case EXCEPTION_INT_OVERFLOW: {
			std::cerr << "Integer Overflow" << std::endl << std::flush;
		} break;
		case EXCEPTION_STACK_OVERFLOW: {
			std::cerr << "Stack Overflow" << std::endl << std::flush;
		} break;
		case EXCEPTION_ILLEGAL_INSTRUCTION: {
			std::cerr << "Illegal Instruction" << std::endl << std::flush;
		} break;
		case EXCEPTION_PRIV_INSTRUCTION: {
			std::cerr << "Privledged Instruction" << std::endl << std::flush;
		} break;
		default: {
			std::cerr << "Exception code 0x" << std::hex
					  << ExceptionRecord.ExceptionCode << std::endl
					  << std::flush;
		} break;
		}

		std::cerr << "Exception Address: " << ExceptionRecord.ExceptionAddress
				  << std::endl
				  << std::flush;

		std::cerr << "Exception Flags: 0x" << std::hex
				  << ExceptionRecord.ExceptionFlags << std::endl
				  << std::flush;

		// Print the faulting module. If its not in exult.exe it might be hard
		// to debug
		HMODULE hmod;
		if (GetModuleHandleExA(
					GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
					LPCSTR(ExceptionRecord.ExceptionAddress), &hmod)) {
			char modfn[MAX_PATH];
			GetModuleFileNameA(hmod, modfn, MAX_PATH);
			modfn[MAX_PATH - 1] = 0;
			std::cerr << "Exception occurred within module " << modfn
					  << std::endl;
		}
	}

	// CreateMidiDump
	auto minidumpfn = get_system_path("<HOME>/minidump.dmp");
	std::cerr << "Writing a minidump to: " << minidumpfn << std::endl;
	HANDLE hminidumpfile = CreateFileA(
			minidumpfn.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
	MINIDUMP_EXCEPTION_INFORMATION mdei;
	MINIDUMP_CALLBACK_INFORMATION  mdci;
	mdei.ThreadId = FaultingThreadId;
	EXCEPTION_POINTERS ep;
	ep.ContextRecord       = &ContextRecord;
	ep.ExceptionRecord     = &ExceptionRecord;
	mdei.ExceptionPointers = &ep;
	mdei.ClientPointers    = FALSE;
	mdci.CallbackRoutine   = Minidumpcallback;
	mdci.CallbackParam     = nullptr;
	MiniDumpWriteDump(
			GetCurrentProcess(), GetCurrentProcessId(), hminidumpfile,
			miniDump_Type, &mdei, nullptr, &mdci);

	CloseHandle(hminidumpfile);

	if (GAME_BG || GAME_SI) {
		std::cerr << " Verifying game files look in stdout for verification "
					 "results"
				  << std::endl;
		verify_files(nullptr);
	}

	auto gwin = Game_window::get_instance();
	// if there is no gwin or main actor we are not in a game So do nothing
	if (gwin && gwin->get_main_actor()) {
		gwin->MakeEmergencySave();
	}
}

static LONG WINAPI ExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
	// Only attempt to handle the first crash, if theHandler Thread crashes we
	// try to immediately exit

	if (exception_count++ == 0) {
		std::cerr << "Exult Crashed" << std::endl << std::flush;

		// Do actual processing of the exception on a separate thread because
		// the stack of this thread might be close to overflow and that might
		// cause problem creating the minidump

		// Save details into globals that the thread needs
		ExceptionRecord  = *ExceptionInfo->ExceptionRecord;
		ContextRecord    = *ExceptionInfo->ContextRecord;
		FaultingThreadId = GetCurrentThreadId();

		try {
			auto thread = std::thread(HandlerWorkerThreadFunc);
			thread.join();
		} catch (const std::exception&) {
		}
		std::cerr << "Done" << std::endl << std::flush;
	} else if (exception_count == 2) {
		std::cerr << "Exult Seems to have crashed while executing the crash "
					 "handler"
				  << std::endl
				  << std::flush;
	} else {
		std::cerr << "Exult Seems to have crashed while executing the crash "
					 "handler again"
				  << std::endl
				  << std::flush;
		if (exception_count > 3) {
			// everything else seems to be failing so just tell Winodws to kill
			// us
			ExitProcess(ExceptionInfo->ExceptionRecord->ExceptionCode);
		}
	}
	// try to use std::exit with the exception code
	std::exit(ExceptionInfo->ExceptionRecord->ExceptionCode);
}

static void signalhandler(int sig) {
	was_signal = true;
	RaiseException(sig, 0, 0, nullptr);

	std::exit(sig);
}

static void terminatehandler() {
	// The runtime should call terminate when there is an unhandled c++
	// exception so get the exception if one exists
	auto exp_ptr = std::current_exception();
	if (exp_ptr) {
		try {
			// Rethrow the exception so we can catch it and get the actual
			// exception type
			std::rethrow_exception(exp_ptr);
		} catch (std::exception& e) {
			std::cerr << "unhandled c++ Exception: " << e.what() << std::endl;
		} catch (...) {
			std::cerr << "unhandled c++ Exception of unknown type" << std::endl;
		}
	}

	// Pass it is the signalhandler as SIGTERM
	signalhandler(SIGTERM);
}

void WindowsInstallCrashHandler() {
	MainThreadId = GetCurrentThreadId();
	// Only do this if a debugger is not attached
	if (!IsDebuggerPresent()) {
		std::string value = "normal";
		if (config) {
			// config setting can be "full" or "normal" or "".
			config->value(
					"config/debugging/windows/minidumptype", value, "normal");
			if (value == "") {
				value = "normal";
			}
			// Make FullMemory minidumps (big!!)
			if (value == "full") {
				miniDump_Type = MiniDumpWithFullMemory;
			}
			// If the value was something other than "full" or "normal" or "",
			// set the value to normal
			else if (value != "normal") {
				value = "normal";
				config->set(
						"config/debugging/windows/minidumptype", "normal",
						true);
			}
		}
		std::cerr << "Installing Windows crash Handler with " << value
				  << " minidumps" << std::endl;
		// Adds an Unhandled Exception Handler. This will replace the one
		// installed by the runtime. This will catch all Win32 Exceptions that
		// weren't handled by a Structured or Vectored Exception handler. In
		// MSVC this includes unhandled c++ exceptions
		SetUnhandledExceptionFilter(ExceptionHandler);

		// Add signal handler for SIGABRT so we can convert it to a Win32
		// exception and create the mididump Other signals we don't handle as
		// they are not expected to occur
		// THe C runtime may raise SIGABRT in response to invalid operations
		std::signal(SIGABRT, signalhandler);

		// Add a terminate handler as the c++ runtime should call this on
		// unhandled C++ exceptions
		std::set_terminate(terminatehandler);

	} else {
		std::cerr << "Debugger Attached, Not Installing Windows crash Handler"
				  << std::endl;
	}
	// DoCrash();
}
