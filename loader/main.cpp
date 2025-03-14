#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>

#include <iostream>

#define BUFFER_SIZE 8000

void ToLowerCase(std::wstring& str) {
	std::transform(str.begin(), str.end(), str.begin(), ::towlower);
}

bool IsObs64Running() {
	// Take a snapshot of all processes
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		return false;  // Failed to take snapshot
	}

	PROCESSENTRY32 processEntry;
	processEntry.dwSize = sizeof(PROCESSENTRY32);

	// Start iterating through processes
	if (Process32First(hProcessSnap, &processEntry)) {
		do {
			// Convert the process name to lowercase for comparison
			std::wstring processName(processEntry.szExeFile);
			ToLowerCase(processName);

			// Compare process name to "obs64.exe" in lowercase
			if (processName == L"obs64.exe") {
				CloseHandle(hProcessSnap);
				return true;  // obs64.exe is running
			}
		} while (Process32Next(hProcessSnap, &processEntry));  // Move to next process
	}

	CloseHandle(hProcessSnap);
	return false;  // obs64.exe is not running
}

DWORD GetProcessIdFromExe(const std::wstring& exeName) {
	PROCESSENTRY32 processEntry;
	processEntry.dwSize = sizeof(PROCESSENTRY32);

	// Create a snapshot of all running processes
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		std::cerr << "[ERROR]: Error taking snapshot of processes." << std::endl;
		return 0;
	}

	// Iterate through all processes
	if (Process32First(hSnapshot, &processEntry)) {
		do {
			// Compare the process executable name with the target exe name
			if (exeName == processEntry.szExeFile) {
				CloseHandle(hSnapshot);
				return processEntry.th32ProcessID; // Return the process ID
			}
		} while (Process32Next(hSnapshot, &processEntry));
	}

	CloseHandle(hSnapshot);
	return 0; // Not found
}

// Callback function to find the window associated with a given process ID
BOOL CALLBACK WindowToForeground(HWND hwnd, LPARAM lParam) {
	DWORD processId;
	GetWindowThreadProcessId(hwnd, &processId);

	if (processId == lParam) {
		wchar_t windowTitle[256];
		int titleLength = GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(wchar_t));
		std::wstring title = windowTitle;
		// Bring the window to the foreground
		if (title.rfind(L"OBS ", 0) == 0) { // the main OBS window title starts with 'OBS '
			SetForegroundWindow(hwnd);
			ShowWindow(hwnd, SW_RESTORE);
			return FALSE;  // Stop enumerating windows (we found the window)
		}
	}
	return TRUE;  // Continue enumerating windows
}

int send_auth_to_obs(std::string payload) {
	int pipe_number = 0;
	std::string pipe_name = "elgato_cloud";
	std::string base_name = "\\\\.\\pipe\\" + pipe_name;
	std::string attempt_name;
	HANDLE pipe = INVALID_HANDLE_VALUE;
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;
	InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = FALSE;

	int connect_attempts_remaining = 6;
	std::cout << "Attempting to connect to Marketplace Connect Plugin" << std::flush;
	while (connect_attempts_remaining-- > 0 &&
		pipe == INVALID_HANDLE_VALUE) {
		pipe_number = 0;
		while (pipe_number < 10) {
			attempt_name = base_name + std::to_string(pipe_number);
			std::cout << "." << std::flush;
			pipe = CreateFileA(attempt_name.c_str(), GENERIC_WRITE,
				0, &sa, OPEN_EXISTING, 0, NULL);
			if (pipe != INVALID_HANDLE_VALUE) {
				std::cout << "\nSuccess" << std::endl;
				break;
			}
			pipe_number++;
		}
		if (pipe == INVALID_HANDLE_VALUE) {
			Sleep(2000);
		}
	}
	if (pipe == INVALID_HANDLE_VALUE) {
		std::cerr << "[ERROR] Could not find connection Marketplace Connect Plugin." << std::endl;
		return 1;
	}
	DWORD mode = PIPE_READMODE_MESSAGE;
	auto success = SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
	if (!success) {
		CloseHandle(pipe);
		std::cerr << "[ERROR] Could not negotiate connection with Marketplace Connect Plugin." << std::endl;
		return 1;
	}

	DWORD written = 0;
	success = WriteFile(pipe, payload.c_str(), static_cast<DWORD>(payload.size()), &written,
		NULL);
	if (!success || written < payload.size()) {
		std::cerr << "[ERROR] Could not send data to Marketplace Connect Plugin." << std::endl;
		CloseHandle(pipe);
		return 1;
	}

	CloseHandle(pipe);
	return 0;
}

int open_obs_mp_window()
{
	int pipe_number = 0;
	std::string pipe_name = "elgato_cloud";
	std::string base_name = "\\\\.\\pipe\\" + pipe_name;
	std::string attempt_name;
	HANDLE pipe = INVALID_HANDLE_VALUE;
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;
	InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = FALSE;

	int connect_attempts_remaining = 60;

	std::cout << "Waiting for OBS to launch" << std::flush;

	while (connect_attempts_remaining-- > 0 &&
		pipe == INVALID_HANDLE_VALUE) {
		pipe_number = 0;
		attempt_name = base_name + std::to_string(pipe_number);
		std::cout << "." << std::flush;
		pipe = CreateFileA(attempt_name.c_str(), GENERIC_WRITE,
			0, &sa, OPEN_EXISTING, 0, NULL);
		if (pipe != INVALID_HANDLE_VALUE) {
			std::cout << "\nConnected To OBS" << std::endl;
			break;
		}
		if (pipe == INVALID_HANDLE_VALUE) {
			Sleep(1000);
		}
	}
	if (pipe == INVALID_HANDLE_VALUE) {
		std::cerr << "[ERROR] Could not find connection Marketplace Connect Plugin." << std::endl;
		return 1;
	}
	DWORD mode = PIPE_READMODE_MESSAGE;
	auto success = SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
	if (!success) {
		CloseHandle(pipe);
		std::cerr << "[ERROR] Could not negotiate connection with Marketplace Connect Plugin." << std::endl;
		return 1;
	}

	DWORD written = 0;
	std::string payload = "elgatolink://open";
	success = WriteFile(pipe, payload.c_str(), static_cast<DWORD>(payload.size()), &written,
		NULL);
	if (!success || written < payload.size()) {
		std::cerr << "[ERROR] Could not send data to Marketplace Connect Plugin." << std::endl;
		CloseHandle(pipe);
		return 1;
	}

	CloseHandle(pipe);
	return 0;
}

int launch_obs()
{
	DWORD buffer_size = BUFFER_SIZE;
	wchar_t buffer[BUFFER_SIZE];

	std::wstring config_path;
	std::wstring launch_path;

	buffer_size = BUFFER_SIZE;
	auto status = RegGetValueW(HKEY_CURRENT_USER, L"SOFTWARE\\elgato\\obs", L"",
		RRF_RT_REG_SZ, nullptr, buffer, &buffer_size);
	// Ignore if the value is too long for our buffer. The user can suffer if OBS is installed in a path longer than 8000 characters long
	if (status == ERROR_SUCCESS) {
		launch_path = std::wstring(buffer, buffer_size);
	}

	bool running = IsObs64Running();

	if (!running && launch_path.size() > 0) {
		auto wd = launch_path.substr(
			0, launch_path.find_last_of(L"\\/"));

		STARTUPINFOW si = { sizeof(STARTUPINFO) };
		PROCESS_INFORMATION pi;

		//printf("Try create %ls with %ls\n", launch_path.c_str(),
		//	wd.c_str());

		//WCHAR lpCommandLine[] = L"";
		if (CreateProcess(launch_path.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, wd.c_str(), &si, &pi) == 0) {
			std::cerr << "[ERROR] Failed to launch OBS." << std::endl;
			return 1;
		}

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		return 0;
	} else if (running) {
		DWORD processId = GetProcessIdFromExe(L"obs64.exe");
		if (processId == 0) {
			std::cerr << "[ERROR] OBS appears to have quit." << std::endl;
			return 1;
		}
		EnumWindows(WindowToForeground, processId);
	}
	return 0;
}

bool obs_is_running(std::wstring name) {
	for (size_t i = 0; i < name.size(); ++i) {
		if (!iswalnum(name[i]))
			name[i] = L'_';
	}
	HANDLE h = OpenMutexW(SYNCHRONIZE, false, name.c_str());
	return !!h;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		std::cerr << "[ERROR] No argument provided." << std::endl;
		system("pause");
		return 1;
	}
	int resp;

	std::string payload = argv[1];

	if (payload.find("elgatolink://auth") == 0) {
		// Send auth to OBS which requested it.
		resp = send_auth_to_obs(payload);
	} else {
		resp = launch_obs();
		if(resp == 0) resp = open_obs_mp_window();
	}
	
	if (resp == 1) {
		system("pause");
	}

	return resp;
}
