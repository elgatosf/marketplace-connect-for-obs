/*
Elgato Deep-Linking OBS Plug-In
Copyright (C) 2024 Corsair Memory Inc. oss.elgato@corsair.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <vector>
#include <string>
#include <functional>
#include <random>
#include <obs-module.h>
#include <nlohmann/json.hpp>

template<class T>
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	T &result = *static_cast<T *>(userdata);

	size_t end = result.size();
	result.resize(result.size() + size * nmemb);
	memcpy(&result[end], ptr, size * nmemb);

	return size * nmemb;
};

obs_data_t* get_module_config();
void save_module_config(obs_data_t* config);

int get_major_version();
bool filename_json(std::string& filename);
std::string get_current_scene_collection_filename();

std::string fetch_string_from_get(std::string url, std::string token);
std::string fetch_string_from_post(std::string url, std::string postdata);
std::vector<char> fetch_bytes_from_url(std::string url);

void replace_all(std::string &haystack, std::string needle, std::string word);
void monitor_for_files(std::string directory,
		       std::function<void(std::string)> callback);
void monitor_for_files_thread(const std::string &directory,
			      std::function<void(std::string)> callback);
void clear_dir(std::string path);
nlohmann::json data_to_json(obs_data_t *data);
nlohmann::json data_to_json(obs_data_array_t *data);
obs_data_array_t *data_array_from_json(const nlohmann::json &j);
obs_data_t *data_from_json(const nlohmann::json &j);
char toupper_ascii(char c);
std::string gen_uuid();
std::string random_name(size_t length = 6);

extern std::mt19937 rand_gen;

template<class T, class U> class auto_closer {
	T *self;
	U closer;

public:
	auto_closer(T *self, U closer) : self(self), closer(closer) {}
	~auto_closer()
	{
		if (self) {
			closer(self);
		}
	}
};
template<class T, class U> auto_closer<T, U> auto_close(T *self, U closer)
{
	return auto_closer<T, U>(self, closer);
}
