

#include "serializing_listener.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <cassert>
#include <fstream>
#include <iostream>

#include "serialization.h"

namespace ser_listener {

using namespace std::literals;
namespace fs = std::filesystem;

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

bool SerializingListener::OnTick(std::chrono::milliseconds delta) {
    // using namespace std::chrono;
    if (!app_) {
        return false;
    }
    if (save_period_ == std::chrono::milliseconds(0)) {
        return false;
    }
    if (pathToStateFile_.empty()) {
        return false;
    }
    if (delta == std::chrono::milliseconds(0)) {
        return false;
    }
    time_since_save_ += delta;
    if (time_since_save_ < save_period_) {
        return false;
    }
    // time_since_save_ -= save_period_;

    time_since_save_ = {};
    SaveStateInFile();
    return true;
}

std::filesystem::path AddSuffix(std::filesystem::path path, std::string_view suffix) {
    auto stem = path.stem();
    auto ext = path.extension();

    path.replace_filename(stem.string() + suffix.data() + ext.string());

    return path;
}

bool SerializingListener::SaveStateInFile() {
    if (pathToStateFile_.empty() || !app_) {
        return false;
    }

    // 1. Create a temporary path
    auto tempPath = pathToStateFile_;
    tempPath += ".temp";

    try {
        // 2. Ensure directory exists
        if (auto parent = pathToStateFile_.parent_path(); !parent.empty()) {
            fs::create_directories(parent);
        }

        // 3. Open the TEMP file
        std::ofstream output_archive{tempPath};
        if (!output_archive.is_open()) {
            std::cerr << "Failed to open file for saving: " << tempPath << std::endl;
            return false;
        }

        // 4. Serialize
        serialization::ApplicationRepr repr(*app_);
        OutputArchive ar{output_archive};
        ar << repr;

        // Flush to ensure data is physically on disk
        output_archive.close();

        // 5. ATOMIC RENAME
        fs::rename(tempPath, pathToStateFile_);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving state: " << e.what() << std::endl;
        std::error_code ec;
        fs::remove(tempPath, ec);
        return false;
    }
}

bool SerializingListener::TryLoadStateFromFile() {
    if (pathToStateFile_.empty() || !app_) {
        return false;
    }

    // 1. Check if file exists
    if (!fs::exists(pathToStateFile_)) {
        std::cerr << "Save file not found (starting new game): " << pathToStateFile_ << std::endl;
        return false;
    }

    try {
        // 2. Open file
        std::ifstream input_archive{pathToStateFile_};
        if (!input_archive.is_open()) {
            return false;
        }

        // Check for empty file (avoids immediate EOF error)
        if (input_archive.peek() == std::ifstream::traits_type::eof()) {
            std::cerr << "Save file is empty. Skipping load." << std::endl;
            return false;
        }

        // 3. Deserialize safely
        serialization::ApplicationRepr repr;
        InputArchive ar{input_archive};
        ar >> repr;

        // 4. Restore application state
        repr.Restore(*app_);

        std::cout << "Game state loaded successfully from " << pathToStateFile_ << std::endl;
        return true;

    } catch (const std::exception& e) {
        // This catches "input stream error", "unsupported version", etc.
        std::cerr << "CRITICAL: Failed to load save file (" << e.what() << ")." << std::endl;

        // Optional: Rename the corrupted file so it doesn't crash the server next time
        std::string backup_name = pathToStateFile_.string() + ".corrupted";
        std::cerr << "Renaming corrupted file to " << backup_name << " and starting fresh." << std::endl;

        try {
            fs::rename(pathToStateFile_, backup_name);
        } catch (...) {
        }

        return false;
    }
}
}  // namespace ser_listener
