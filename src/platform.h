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

#include <string>
#include <functional>

std::string get_scene_collections_path();
bool is_symlink(std::string path);
bool is_directory(std::string path);
std::string get_plugin_data_path();
bool path_begins_with(const std::string &haystack, const std::string &needle);

// Just blindly listens on a named pipe waiting for a string, and submits it to the callback
bool listen_on_pipe(const std::string &pipe_name,
		    std::function<void(std::string)> callback);

FILE *open_tmp_file(const char *mode, std::string &outFilename);
bool move_file(const std::string &from, const std::string &to);

// Moves file from 'from' to 'to' but renames it if there's a collision
std::string move_file_safe(const std::string &from, const std::string &to);