// Server program for the OBS Studio node module.
// Copyright(C) 2017 Streamlabs (General Workings Inc)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.

#include <chrono>
#include <inttypes.h>
#include <iostream>
#include <ipc-class.hpp>
#include <ipc-function.hpp>
#include <ipc-server.hpp>
#include <memory>
#include <thread>
#include <vector>
#include "error.hpp"
#include "nodeobs_api.h"
#include "nodeobs_autoconfig.h"
#include "nodeobs_content.h"
#include "nodeobs_service.h"
#include "nodeobs_settings.h"
#include "osn-fader.hpp"
#include "osn-filter.hpp"
#include "osn-global.hpp"
#include "osn-input.hpp"
#include "osn-properties.hpp"
#include "osn-scene.hpp"
#include "osn-sceneitem.hpp"
#include "osn-source.hpp"
#include "osn-transition.hpp"
#include "osn-video.hpp"
#include "osn-volmeter.hpp"
#include "osn-module.hpp"
#include "util-crashmanager.h"

extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;

#define BUFFSIZE 512

struct ServerData
{
	std::mutex                                     mtx;
	std::chrono::high_resolution_clock::time_point last_connect, last_disconnect;
	size_t                                         count_connected = 0;
};

bool ServerConnectHandler(void* data, int64_t)
{
	ServerData*                  sd = reinterpret_cast<ServerData*>(data);
	std::unique_lock<std::mutex> ulock(sd->mtx);
	sd->last_connect = std::chrono::high_resolution_clock::now();
	sd->count_connected++;
	return true;
}

void ServerDisconnectHandler(void* data, int64_t)
{
	ServerData*                  sd = reinterpret_cast<ServerData*>(data);
	std::unique_lock<std::mutex> ulock(sd->mtx);
	sd->last_disconnect = std::chrono::high_resolution_clock::now();
	sd->count_connected--;
}

namespace System
{
	static void
	    Shutdown(void* data, const int64_t id, const std::vector<ipc::value>& args, std::vector<ipc::value>& rval)
	{
		bool* shutdown = (bool*)data;
		*shutdown      = true;
		rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
		return;
	}
} // namespace System

#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <windows.h>

#include <WinBase.h>
#include "DbgHelp.h"
#pragma comment(lib, "Dbghelp.lib")

void BindCrtHandlesToStdHandles(bool bindStdIn, bool bindStdOut, bool bindStdErr)
{
	// Re-initialize the C runtime "FILE" handles with clean handles bound to "nul". We do this because it has been
	// observed that the file number of our standard handle file objects can be assigned internally to a value of -2
	// when not bound to a valid target, which represents some kind of unknown internal invalid state. In this state our
	// call to "_dup2" fails, as it specifically tests to ensure that the target file number isn't equal to this value
	// before allowing the operation to continue. We can resolve this issue by first "re-opening" the target files to
	// use the "nul" device, which will place them into a valid state, after which we can redirect them to our target
	// using the "_dup2" function.
	if (bindStdIn) {
		FILE* dummyFile;
		freopen_s(&dummyFile, "nul", "r", stdin);
	}
	if (bindStdOut) {
		FILE* dummyFile;
		freopen_s(&dummyFile, "nul", "w", stdout);
	}
	if (bindStdErr) {
		FILE* dummyFile;
		freopen_s(&dummyFile, "nul", "w", stderr);
	}

	// Redirect unbuffered stdin from the current standard input handle
	if (bindStdIn) {
		HANDLE stdHandle = GetStdHandle(STD_INPUT_HANDLE);
		if (stdHandle != INVALID_HANDLE_VALUE) {
			int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
			if (fileDescriptor != -1) {
				FILE* file = _fdopen(fileDescriptor, "r");
				if (file != NULL) {
					int dup2Result = _dup2(_fileno(file), _fileno(stdin));
					if (dup2Result == 0) {
						setvbuf(stdin, NULL, _IONBF, 0);
					}
				}
			}
		}
	}

	// Redirect unbuffered stdout to the current standard output handle
	if (bindStdOut) {
		HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (stdHandle != INVALID_HANDLE_VALUE) {
			int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
			if (fileDescriptor != -1) {
				FILE* file = _fdopen(fileDescriptor, "w");
				if (file != NULL) {
					int dup2Result = _dup2(_fileno(file), _fileno(stdout));
					if (dup2Result == 0) {
						setvbuf(stdout, NULL, _IONBF, 0);
					}
				}
			}
		}
	}

	// Redirect unbuffered stderr to the current standard error handle
	if (bindStdErr) {
		HANDLE stdHandle = GetStdHandle(STD_ERROR_HANDLE);
		if (stdHandle != INVALID_HANDLE_VALUE) {
			int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
			if (fileDescriptor != -1) {
				FILE* file = _fdopen(fileDescriptor, "w");
				if (file != NULL) {
					int dup2Result = _dup2(_fileno(file), _fileno(stderr));
					if (dup2Result == 0) {
						setvbuf(stderr, NULL, _IONBF, 0);
					}
				}
			}
		}
	}

	// Clear the error state for each of the C++ standard stream objects. We need to do this, as attempts to access the
	// standard streams before they refer to a valid target will cause the iostream objects to enter an error state. In
	// versions of Visual Studio after 2005, this seems to always occur during startup regardless of whether anything
	// has been read from or written to the targets or not.
	if (bindStdIn) {
		std::wcin.clear();
		std::cin.clear();
	}
	if (bindStdOut) {
		std::wcout.clear();
		std::cout.clear();
	}
	if (bindStdErr) {
		std::wcerr.clear();
		std::cerr.clear();
	}
}

int main(int argc, char* argv[])
{
	// Allocate a console window for this process
	AllocConsole();

	// Update the C/C++ runtime standard input, output, and error targets to use the console window
	BindCrtHandlesToStdHandles(true, true, true);

	// Initialize the crash manager
	util::CrashManager crashManager;
	if (!crashManager.Initialize()) {
		return 0;
	}

	// Usage:
	// argv[0] = Path to this application. (Usually given by default if run via path-based command!)
	// argv[1] = Path to a named socket.

	if (argc != 2) {
		std::cerr << "There must be exactly one parameter." << std::endl;
		return -1;
	}

	// Instance
	ipc::server myServer;
	bool        doShutdown = false;
	ServerData  sd;
	sd.last_disconnect = sd.last_connect = std::chrono::high_resolution_clock::now();
	sd.count_connected                   = 0;

	// Classes
	/// System
	{
		std::shared_ptr<ipc::collection> cls = std::make_shared<ipc::collection>("System");
		cls->register_function(
		    std::make_shared<ipc::function>("Shutdown", std::vector<ipc::type>{}, System::Shutdown, &doShutdown));
		myServer.register_collection(cls);
	};

	/// OBS Studio Node
	osn::Global::Register(myServer);
	osn::Source::Register(myServer);
	osn::Input::Register(myServer);
	osn::Filter::Register(myServer);
	osn::Transition::Register(myServer);
	osn::Scene::Register(myServer);
	osn::SceneItem::Register(myServer);
	osn::Fader::Register(myServer);
	osn::VolMeter::Register(myServer);
	osn::Properties::Register(myServer);
	osn::Video::Register(myServer);
	osn::Module::Register(myServer);
	OBS_API::Register(myServer);
	OBS_content::Register(myServer);
	OBS_service::Register(myServer);
	OBS_settings::Register(myServer);
	OBS_settings::Register(myServer);
	autoConfig::Register(myServer);

	// Register Connect/Disconnect Handlers
	myServer.set_connect_handler(ServerConnectHandler, &sd);
	myServer.set_disconnect_handler(ServerDisconnectHandler, &sd);

	// Initialize Server
	try {
		myServer.initialize(argv[1]);
	} catch (std::exception e) {
		std::cerr << "Initialization failed with error " << e.what() << "." << std::endl;
		return -2;
	} catch (...) {
		std::cerr << "Failed to initialize server" << std::endl;
		return -2;
	}

	// Reset Connect/Disconnect time.
	sd.last_disconnect = sd.last_connect = std::chrono::high_resolution_clock::now();

	bool waitBeforeClosing = false;

	while (!doShutdown) {
		if (sd.count_connected == 0) {
			auto tp    = std::chrono::high_resolution_clock::now();
			auto delta = tp - sd.last_disconnect;
			if (std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() > 5000) {
				doShutdown = true;
				waitBeforeClosing = true;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	// Wait on receive the exit message from the crash-handler
	if (waitBeforeClosing) {
		HANDLE hPipe;
		TCHAR  chBuf[BUFFSIZE];
		DWORD  cbRead;
		hPipe = CreateNamedPipe(
		    TEXT("\\\\.\\pipe\\exit-slobs-crash-handler"),
		    PIPE_ACCESS_DUPLEX,
		    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		    1,
		    BUFFSIZE * sizeof(TCHAR),
		    BUFFSIZE * sizeof(TCHAR),
		    NULL,
		    NULL);

		if (hPipe != INVALID_HANDLE_VALUE) {
			if (ConnectNamedPipe(hPipe, NULL) != FALSE) {
				BOOL fSuccess = ReadFile(hPipe, chBuf, BUFFSIZE * sizeof(TCHAR), &cbRead, NULL);

				if (!fSuccess)
					return 0;
				CloseHandle(hPipe);
			}
		}
	}

	// Finalize Server
	myServer.finalize();

	return 0;
}
