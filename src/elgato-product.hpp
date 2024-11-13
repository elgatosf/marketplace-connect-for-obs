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

#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace elgatocloud {

class ElgatoProductItem;
class StreamPackageSetupWizard;

class ElgatoProduct {
public:
	std::string name;
	std::string thumbnailUrl;
	std::vector<unsigned char> thumbnail;
	std::string thumbnailPath;
	std::string thumbnailDir;
	std::string variantId;

	ElgatoProduct(nlohmann::json &productData);
	ElgatoProduct(std::string name);
	inline void SetProductItem(ElgatoProductItem *item)
	{
		_productItem = item;
	}
	inline ~ElgatoProduct() {};
	inline bool ready() { return _thumbnailReady; }
	void DownloadProduct();
	static void DownloadProgress(void *ptr, bool finished, bool downloading,
				     uint64_t fileSize, uint64_t chunkSize,
				     uint64_t downloaded);
	static void Install(std::string filename_utf8, void *data, bool fromDownload);
	static void ThumbnailProgress(void *ptr, bool finished,
				      bool downloading, uint64_t fileSize,
				      uint64_t chunkSize, uint64_t downloaded);
	static void SetThumbnail(std::string filename, void *data);

private:
	void _downloadThumbnail();
	bool _thumbnailReady;
	size_t _fileSize;
	ElgatoProductItem *_productItem = nullptr;
};

} // namespace elgatocloud
