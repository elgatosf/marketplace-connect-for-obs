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

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	// Start iterating through processes
	if (Process32First(hProcessSnap, &pe32)) {
		do {
			// Convert the process name to lowercase for comparison
			std::wstring processName(pe32.szExeFile);
			ToLowerCase(processName);

			// Compare process name to "obs64.exe" in lowercase
			if (processName == L"obs64.exe") {
				CloseHandle(hProcessSnap);
				return true;  // obs64.exe is running
			}
		} while (Process32Next(hProcessSnap, &pe32));  // Move to next process
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
		printf("Error taking snapshot of processes.");
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
			ShowWindow(hwnd, SW_RESTORE);  // Restore if minimized
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
	int connect_attempts_remaining = 6;

	while (connect_attempts_remaining-- > 0 &&
		pipe == INVALID_HANDLE_VALUE) {
		pipe_number = 0;
		while (pipe_number < 10) {
			attempt_name = base_name + std::to_string(pipe_number);
			printf("Attempting %s\n", attempt_name.c_str());
			pipe = CreateFileA(attempt_name.c_str(), GENERIC_WRITE,
				0, NULL, OPEN_EXISTING, 0, NULL);
			if (pipe != INVALID_HANDLE_VALUE) {
				printf("Success\n");
				break;
			}
			pipe_number++;
		}
		if (pipe == INVALID_HANDLE_VALUE) {
			Sleep(2000);
		}
	}
	if (pipe == INVALID_HANDLE_VALUE) {
		printf("Could not open named pipe!");
		return 1;
	}
	DWORD mode = PIPE_READMODE_MESSAGE;
	auto success = SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
	if (!success) {
		CloseHandle(pipe);
		printf("Could not configure named pipe!");
		return 1;
	}

	DWORD written = 0;
	success = WriteFile(pipe, payload.c_str(), static_cast<DWORD>(payload.size()), &written,
		NULL);
	if (!success || written < payload.size()) {
		printf("Failed to write to named pipe!");
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

		printf("Try create %ls with %ls\n", launch_path.c_str(),
			wd.c_str());

		//WCHAR lpCommandLine[] = L"";
		if (CreateProcess(launch_path.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, wd.c_str(), &si, &pi) == 0) {
			printf("Failed to launch OBS.");
			return 1;
		}

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		return 0;
	} else if (running) {
		DWORD processId = GetProcessIdFromExe(L"obs64.exe");
		if (processId == 0) {
			printf("Process not found!");
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

	printf("CHECKING %ls\n", name.c_str());
	HANDLE h = OpenMutexW(SYNCHRONIZE, false, name.c_str());
	return !!h;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Please provide an argument");
		return 1;
	}

	std::string payload = argv[1];

	if (payload.find("elgatolink://auth") == 0) {
		// Send auth to OBS which requested it.
		return send_auth_to_obs(payload);
	} else {
		return launch_obs();
	}
	printf("No obs found to launch.");
	return 1;
}
