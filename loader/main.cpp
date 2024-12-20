#include <string>
#include <windows.h>


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

	#define BUFFER_SIZE 8000
	DWORD buffer_size = BUFFER_SIZE;
	//wchar_t buffer[BUFFER_SIZE];

	std::wstring config_path;
	std::wstring launch_path;

	/* auto status = RegGetValueW(HKEY_CURRENT_USER,
					L"SOFTWARE\\elgato\\obs",
				   L"config path", RRF_RT_REG_SZ, nullptr, buffer,
				   &buffer_size);
	if (status == ERROR_SUCCESS) {
		config_path = std::wstring(buffer, buffer_size);
	}*/

	buffer_size = BUFFER_SIZE;
	/*
	auto status = RegGetValueW(HKEY_CURRENT_USER, L"SOFTWARE\\elgato\\obs", L"",
			      RRF_RT_REG_SZ, nullptr, buffer, &buffer_size);
	// Ignore if the value is too long for our buffer. The user can suffer if OBS is installed in a path longer than 8000 characters long
	if (status == ERROR_SUCCESS) {
		launch_path = std::wstring(buffer, buffer_size);
	}

	bool running = obs_is_running(L"OBSStudioCore");

	if (!running) {
		config_path = launch_path;
		int remaining = 3;
		while (remaining-- > 0) {
			auto pos = config_path.find_last_of(L"\\/");
			if (pos != std::string::npos) {
				config_path.erase(config_path.begin() + pos,
						  config_path.end());
			}
		}
		config_path += L"\\config";
		running = obs_is_running(std::wstring(L"OBSStudioPortable") + config_path);
	}

	int connect_attempts_remaining = 1;

	if (!running && launch_path.size() > 0) {
		auto wd = launch_path.substr(
			0, launch_path.find_last_of(L"\\/"));

		STARTUPINFOW si;
		PROCESS_INFORMATION pi;

		printf("Try create %ls with %ls\n", launch_path.c_str(),
			    wd.c_str());

		CreateProcessW(launch_path.c_str(), L"", NULL, NULL, FALSE, 0, NULL, wd.c_str(), &si, &pi);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		connect_attempts_remaining = 6;
	}
	*/

	std::string payload = argv[1];

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
		while (true) Sleep(100);
		return 1;
	}
	DWORD mode = PIPE_READMODE_MESSAGE;
	auto success = SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
	if (!success) {
		CloseHandle(pipe);
		printf("Could not configure named pipe!");
		while (true) Sleep(100);
		return 1;
	}

	DWORD written = 0;
	success = WriteFile(pipe, payload.c_str(), static_cast<DWORD>(payload.size()), &written,
			    NULL);
	if (!success || written < payload.size()) {
		printf("Failed to write to named pipe!");
		CloseHandle(pipe);
		while (true) Sleep(100);
		return 1;
	}
	CloseHandle(pipe);
	while (true) Sleep(100);
	return 0;
}
