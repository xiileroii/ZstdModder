#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// OS Detection for command prefixes
#ifdef _WIN32
constexpr const char *kFfmpeg = "ffmpeg.exe";
constexpr const char *kConverter = "brstm_converter.exe";
constexpr const char *kZstd = "zstd.exe";
constexpr const char *kPatcher = "auto_bars_patcher.exe";
#else
constexpr const char *kFfmpeg = "ffmpeg";
constexpr const char *kConverter = "./brstm_converter";
constexpr const char *kZstd = "zstd";
constexpr const char *kPatcher = "./auto_bars_patcher";
#endif

struct ModProfile
{
    int channels;
};

// --- Struct to hold dynamic BARS target data ---
struct BarsTarget
{
    std::string original_zstd;
    std::string uncompressed;
    std::string patched;
    std::string patched_zstd;
    std::vector<std::string> names;
};

// --- Helper: Trim Strings ---
std::string Trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n\"");
    size_t end = s.find_last_not_of(" \t\r\n\"");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// --- Helper: Ends With ---
bool EndsWith(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// --- Helper: Check if file is an audio file ---
bool IsAudioFile(const fs::path &path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    return (ext == ".wav" || ext == ".flac" || ext == ".mp3" || ext == ".ogg" ||
            ext == ".m4a" || ext == ".aac" || ext == ".wma");
}

// --- Config Management ---
std::string GetOrPromptAtmospherePath()
{
    std::string path;
    std::ifstream config_file("config.ini");

    if (config_file.is_open())
    {
        std::string line;
        while (std::getline(config_file, line))
        {
            if (line.rfind("ATMOSPHERE_PATH=", 0) == 0)
            {
                path = Trim(line.substr(16));
                break;
            }
        }
        config_file.close();
    }

    if (path.empty())
    {
        std::cout << "Enter the absolute path to your Atmosphere contents folder\n";
        std::cout << "(e.g., .../atmosphere/contents/[TitleID]): ";
        std::getline(std::cin, path);
        path = Trim(path);

        std::ofstream out_file("config.ini", std::ios::app);
        if (out_file.is_open())
        {
            out_file << "ATMOSPHERE_PATH=" << path << "\n";
            out_file.close();
            std::cout << "[Info] Saved Atmosphere path to config.ini file.\n";
        }
    }
    else
    {
        std::cout << "[Info] Loaded Atmosphere path from config.ini: " << path << "\n";
    }
    return path;
}

// --- Cross-Platform Raw Keyboard Input ---
enum KeyAction
{
    kUp,
    kDown,
    kEnter,
    kBackspace,
    kOther,
    kIgnore
};

struct KeyPress
{
    KeyAction action;
    char ch;
};

KeyPress GetKeyPress()
{
#ifdef _WIN32
    int c = _getch();
    if (c == 0 || c == 224)
    {
        int c2 = _getch();
        if (c2 == 72)
            return {kUp, 0};
        if (c2 == 80)
            return {kDown, 0};
        return {kIgnore, 0};
    }
    if (c == 13)
        return {kEnter, 0};
    if (c == 8)
        return {kBackspace, 0};
    return {kOther, static_cast<char>(c)};
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int c = getchar();

    if (c == 27)
    {
        int c2 = getchar();
        if (c2 == 91 || c2 == 79)
        {
            int c3 = getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            if (c3 == 65)
                return {kUp, 0};
            if (c3 == 66)
                return {kDown, 0};
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return {kIgnore, 0};
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (c == 10)
        return {kEnter, 0};
    if (c == 127 || c == 8)
        return {kBackspace, 0};
    return {kOther, static_cast<char>(c)};
#endif
}

bool CaseInsensitiveMatch(const std::string &str, const std::string &query)
{
    if (query.empty())
        return true;
    auto it = std::search(
        str.begin(), str.end(), query.begin(), query.end(),
        [](char ch1, char ch2)
        { return std::toupper(ch1) == std::toupper(ch2); });
    return (it != str.end());
}

class CrossPlatformModder
{
public:
    void RunCmd(const std::string &cmd)
    {
        std::cout << "[Executing]: " << cmd << "\n";
        if (std::system(cmd.c_str()) != 0)
        {
            std::cerr << "Command failed.\n";
            exit(1);
        }
    }

    std::string ReadNullTerminatedString(std::ifstream &fs)
    {
        std::string s;
        char c;
        while (fs.read(&c, 1) && c != '\0')
        {
            s += c;
        }
        return s;
    }

    std::vector<std::string> ParseBarsNames(const std::string &path)
    {
        std::vector<std::string> names;
        std::ifstream fs(path, std::ios::binary);
        if (!fs)
            return names;

        char magic[5] = {0};
        fs.read(magic, 4);
        if (std::string(magic) != "BARS")
            return names;

        uint32_t size, asset_count;
        uint16_t endian, version;
        fs.read(reinterpret_cast<char *>(&size), 4);
        fs.read(reinterpret_cast<char *>(&endian), 2);
        fs.read(reinterpret_cast<char *>(&version), 2);
        fs.read(reinterpret_cast<char *>(&asset_count), 4);

        std::vector<uint32_t> amta_offsets;
        for (uint32_t i = 0; i < asset_count; i++)
            fs.seekg(4, std::ios::cur);
        for (uint32_t i = 0; i < asset_count; i++)
        {
            uint32_t amta_offset;
            fs.read(reinterpret_cast<char *>(&amta_offset), 4);
            amta_offsets.push_back(amta_offset);
            fs.seekg(4, std::ios::cur);
        }

        for (uint32_t offset : amta_offsets)
        {
            fs.seekg(offset, std::ios::beg);
            fs.read(magic, 4);
            uint16_t amta_endian, amta_version;
            uint32_t amta_size;
            fs.read(reinterpret_cast<char *>(&amta_endian), 2);
            fs.read(reinterpret_cast<char *>(&amta_version), 2);
            fs.read(reinterpret_cast<char *>(&amta_size), 4);

            std::string asset_name = "";
            if (amta_version == 0x0500)
            {
                fs.seekg(24, std::ios::cur);
                uint32_t data_size;
                auto data_start_pos = fs.tellg();
                fs.read(reinterpret_cast<char *>(&data_size), 4);

                fs.seekg(static_cast<long>(data_start_pos) + data_size, std::ios::beg);
                asset_name = ReadNullTerminatedString(fs);
            }
            else if (amta_version == 0x0400)
            {
                uint32_t data_offset, mark_offset, ext_offset, strg_offset;
                fs.read(reinterpret_cast<char *>(&data_offset), 4);
                fs.read(reinterpret_cast<char *>(&mark_offset), 4);
                fs.read(reinterpret_cast<char *>(&ext_offset), 4);
                fs.read(reinterpret_cast<char *>(&strg_offset), 4);

                fs.seekg(offset + strg_offset + 8, std::ios::beg);
                asset_name = ReadNullTerminatedString(fs);
            }

            if (!asset_name.empty())
            {
                names.push_back(asset_name);
            }
        }
        return names;
    }

    std::string InteractiveFileFilter(const std::string &base_name,
                                      const std::vector<std::string> &bars_names)
    {
        std::cout << "\n[!] Target file not found in the BARS automatically.\n";
        std::cout << "Start typing to search for a target BWAV to replace.\n";

        int display_limit = 10;
        int lines_to_print = display_limit + 3;
        std::string query = "";
        int selected_index = 0;
        int scroll_offset = 0;

        for (int i = 0; i < lines_to_print; i++)
            std::cout << "\n";

        while (true)
        {
            std::vector<std::string> filtered;
            for (const auto &name : bars_names)
            {
                if (CaseInsensitiveMatch(name, query))
                {
                    filtered.push_back(name);
                }
            }

            if (selected_index >= static_cast<int>(filtered.size()))
                selected_index = std::max(0, static_cast<int>(filtered.size()) - 1);

            if (selected_index < scroll_offset)
                scroll_offset = selected_index;
            if (selected_index >= scroll_offset + display_limit)
                scroll_offset = selected_index - display_limit + 1;

            if (scroll_offset >
                std::max(0, static_cast<int>(filtered.size()) - display_limit))
                scroll_offset =
                    std::max(0, static_cast<int>(filtered.size()) - display_limit);

            std::cout << "\x1b[" << lines_to_print << "A\r";

            std::cout << "\x1b[2KSearch: " << query << "_\n";
            std::cout << "\x1b[2K------------------------------\n";

            for (int i = 0; i < display_limit; i++)
            {
                int item_index = scroll_offset + i;
                if (item_index < static_cast<int>(filtered.size()))
                {
                    if (item_index == selected_index)
                    {
                        std::cout << "\x1b[2K\x1b[36m> " << filtered[item_index]
                                  << "\x1b[0m\n";
                    }
                    else
                    {
                        std::cout << "\x1b[2K  " << filtered[item_index] << "\n";
                    }
                }
                else
                {
                    std::cout << "\x1b[2K\n";
                }
            }

            std::string status = "Matches: " + std::to_string(filtered.size()) +
                                 " | Up/Down to navigate | Enter to confirm";
            std::cout << "\x1b[2K" << status << "\n";

            KeyPress kp = GetKeyPress();

            if (kp.action == kEnter)
            {
                if (!filtered.empty())
                    return filtered[selected_index];
            }
            else if (kp.action == kBackspace)
            {
                if (!query.empty())
                {
                    query.pop_back();
                    selected_index = 0;
                    scroll_offset = 0;
                }
            }
            else if (kp.action == kUp)
            {
                if (selected_index > 0)
                    selected_index--;
            }
            else if (kp.action == kDown)
            {
                if (selected_index < static_cast<int>(filtered.size()) - 1)
                    selected_index++;
            }
            else if (kp.action == kOther && !std::iscntrl(kp.ch))
            {
                query += kp.ch;
                selected_index = 0;
                scroll_offset = 0;
            }
        }
    }

    void Build(const fs::path &input_path, ModProfile profile,
               const std::vector<std::string> &all_bars_names,
               const std::vector<std::string> &existing_mods)
    {
        std::string original_name = input_path.stem().string();
        std::string target_bwav_name = original_name;

        // 1. Smart Tag Detection (Look for _1c, _2c, etc.)
        int custom_channels = 0;
        if (original_name.length() > 3 && original_name[original_name.length() - 2] == 'c' && original_name[original_name.length() - 3] == '_')
        {
            char channel_char = original_name[original_name.length() - 1];
            if (isdigit(channel_char))
            {
                custom_channels = channel_char - '0';
                target_bwav_name = original_name.substr(0, original_name.length() - 3);
                std::cout << "\n[Info] Detected channel tag: Forcing " << custom_channels << " channels.\n";
            }
        }

        if (std::find(all_bars_names.begin(), all_bars_names.end(),
                      target_bwav_name) == all_bars_names.end())
        {
            target_bwav_name = InteractiveFileFilter(target_bwav_name, all_bars_names);
            std::cout << "\nMapping Original File -> '" << target_bwav_name << "'\n";
        }

        // 2. Collision Detection for Pre-Existing Mods
        if (std::find(existing_mods.begin(), existing_mods.end(), target_bwav_name) != existing_mods.end())
        {
            std::string input;
            while (true)
            {
                std::cout << "\n[Warning] An existing mod for '" << target_bwav_name << ".bwav' was found in your Atmosphere directory.\n";
                std::cout << "Do you want to overwrite it with this new audio file? (Y/N): ";
                std::getline(std::cin, input);
                input = Trim(input);
                if (input == "Y" || input == "y")
                {
                    break; // Proceed with overwrite
                }
                else if (input == "N" || input == "n")
                {
                    std::cout << "Skipping '" << target_bwav_name << "'.\n";
                    return; // Abort this specific audio file
                }
                else
                {
                    std::cout << "Invalid input. Please enter Y or N.\n";
                }
            }
        }

        // 3. Interactive Fallback Prompt (If no tag was found)
        if (custom_channels == 0)
        {
            std::string input;
            while (true)
            {
                std::cout << "\nHow many channels does the original '" << target_bwav_name << "' use?\n";
                std::cout << "  [1] Mono (Standard for Sound Effects / Voices)\n";
                std::cout << "  [2] Stereo (Standard for Background Music)\n";
                std::cout << "Enter 1 or 2: ";
                std::getline(std::cin, input);
                input = Trim(input);

                if (input == "1" || input == "2")
                {
                    custom_channels = std::stoi(input);
                    break;
                }
                else
                {
                    std::cout << "Invalid input. Please enter 1 or 2.\n";
                }
            }
        }

        profile.channels = custom_channels;
        std::string output_bwav = target_bwav_name + ".bwav";
        std::string safe_input_ext = input_path.extension().string();
        std::string safe_input = "temp_safe_input" + safe_input_ext;
        std::string temp_wav = "temp_safe_output.wav";

        std::error_code ec;
        fs::copy_file(input_path, safe_input, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            std::cerr << "Failed to read input file: " << ec.message() << "\n";
            return;
        }

        std::cout << "\nProcessing -> " << output_bwav << "\n";

        RunCmd(std::string(kFfmpeg) + " -y -i \"" + safe_input +
               "\" -ar 48000 -ac " + std::to_string(profile.channels) + " \"" +
               temp_wav + "\"");

        RunCmd(std::string(kConverter) + " \"" + temp_wav + "\" -o \"ModStream/" +
               output_bwav + "\"");

        try
        {
            fs::remove(safe_input);
            fs::remove(temp_wav);
        }
        catch (...)
        {
            // Ignore cleanup failures
        }
    }

    void FinalizeBuild(const std::string &atmosphere_path,
                       const std::vector<BarsTarget> &targets)
    {
        std::cout << "\n[Patching BARS files]\n";

        for (const auto &target : targets)
        {
            RunCmd(std::string(kPatcher) +
                   " --og-stream-dir ./Stream --mod-stream-dir ./ModStream "
                   "--og-bars-file " +
                   target.uncompressed + " --bars-output-file " + target.patched);
            RunCmd(std::string(kZstd) + " -f " + target.patched + " -o " +
                   target.patched_zstd);
        }

        if (!atmosphere_path.empty())
        {
            fs::path dest_stream =
                fs::path(atmosphere_path) / "romfs/Sound/Resource/Stream";
            fs::path dest_bars = fs::path(atmosphere_path) / "romfs/Sound/Resource";

            std::cout << "Deploying to: " << dest_bars.string() << "\n";

            fs::create_directories(dest_stream);

            try
            {
                for (const auto &entry : fs::directory_iterator("ModStream"))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".bwav")
                    {
                        fs::copy_file(entry.path(), dest_stream / entry.path().filename(),
                                      fs::copy_options::overwrite_existing);
                    }
                }

                for (const auto &target : targets)
                {
                    fs::copy_file(target.patched_zstd, dest_bars / target.original_zstd,
                                  fs::copy_options::overwrite_existing);
                }

                std::cout << "Successfully deployed to Atmosphere!\n";

                // --- POST-DEPLOYMENT CLEANUP ---
                std::error_code ec;
                fs::remove_all("ModStream", ec);

                for (const auto &target : targets)
                {
                    fs::remove(target.uncompressed, ec);
                    fs::remove(target.patched, ec);
                    fs::remove(target.patched_zstd, ec);
                }
                std::cout << "Cleaned up ModStream directory and temporary BARS files.\n";
            }
            catch (const fs::filesystem_error &e)
            {
                std::cerr << "Deploy Error: " << e.what() << "\n";
            }
        }
    }
};

int main(int argc, char *argv[])
{
    fs::path input_path;
    bool use_current_dir = false;

#ifdef _WIN32
    int wargc;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv && wargc > 1)
    {
        input_path = fs::path(wargv[1]);
    }
    else
    {
        use_current_dir = true;
    }
    if (wargv)
        LocalFree(wargv);
#else
    if (argc > 1)
    {
        input_path = fs::path(argv[1]);
    }
    else
    {
        use_current_dir = true;
    }
#endif

    if (use_current_dir)
    {
        input_path = fs::current_path();
        std::cout << "[Info] No input specified. Scanning current directory for "
                     "audio files...\n";
    }

    // Retrieve Atmosphere path first so we can scan for existing mods
    std::string atmosphere_path = GetOrPromptAtmospherePath();
    std::vector<std::string> existing_mods;

    // --- Initialize Workspace and Pre-Load Existing Mods ---
    fs::create_directories("ModStream");

    if (!atmosphere_path.empty())
    {
        fs::path dest_stream = fs::path(atmosphere_path) / "romfs/Sound/Resource/Stream";
        if (fs::exists(dest_stream))
        {
            for (const auto &entry : fs::directory_iterator(dest_stream))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".bwav")
                {
                    std::string mod_name = entry.path().stem().string();
                    existing_mods.push_back(mod_name);

                    // Copy to local ModStream so the BARS patcher integrates it into the new BARS archive
                    std::error_code ec;
                    fs::copy_file(entry.path(), fs::path("ModStream") / entry.path().filename(), fs::copy_options::overwrite_existing, ec);
                }
            }
            if (!existing_mods.empty())
            {
                std::cout << "[Info] Found " << existing_mods.size() << " existing audio mod(s) in Atmosphere directory. They will be integrated into the new BARS archive.\n";
            }
        }
    }

    CrossPlatformModder modder;
    ModProfile se_record = {1};

    // --- Dynamic BARS Scanner ---
    std::vector<BarsTarget> bars_targets;
    for (const auto &entry : fs::directory_iterator(fs::current_path()))
    {
        if (entry.is_regular_file())
        {
            std::string filename = entry.path().filename().string();
            if (EndsWith(filename, ".bars.zs") && !EndsWith(filename, "Upd.bars.zs"))
            {
                BarsTarget target;
                target.original_zstd = filename;
                target.uncompressed = filename.substr(0, filename.size() - 3);

                std::string base =
                    target.uncompressed.substr(0, target.uncompressed.size() - 5);
                target.patched = base + "Upd.bars";
                target.patched_zstd = target.patched + ".zs";

                bars_targets.push_back(target);
            }
        }
    }

    if (bars_targets.empty())
    {
        std::cerr << "Error: No .bars.zs files found in the current directory.\n";
        return 1;
    }

    // Process all discovered BARS files
    std::vector<std::string> all_bars_names;
    for (auto &target : bars_targets)
    {
        modder.RunCmd(std::string(kZstd) + " -d -f " + target.original_zstd +
                      " -o " + target.uncompressed);
        target.names = modder.ParseBarsNames(target.uncompressed);

        all_bars_names.insert(all_bars_names.end(), target.names.begin(),
                              target.names.end());
        std::cout << "[Info] Parsed " << target.names.size()
                  << " internal filenames from " << target.original_zstd << ".\n";
    }
    std::cout << "\n[Info] Total combined parsed filenames ready for search: "
              << all_bars_names.size() << "\n";

    // --- Audio Processing ---
    if (fs::is_directory(input_path))
    {
        bool found_audio = false;
        for (const auto &entry : fs::directory_iterator(input_path))
        {
            if (entry.is_regular_file() && IsAudioFile(entry.path()))
            {
                found_audio = true;
                // Pass existing_mods into Build for collision detection
                modder.Build(entry.path(), se_record, all_bars_names, existing_mods);
            }
        }

        if (!found_audio && use_current_dir)
        {
            std::cout << "\n[Info] No supported audio files found in the current "
                         "directory.\n";
            return 0;
        }
    }
    else if (fs::is_regular_file(input_path))
    {
        modder.Build(input_path, se_record, all_bars_names, existing_mods);
    }
    else
    {
        std::cerr << "Error: Input path is not a valid file or directory.\n";
        return 1;
    }

    // Deploy and Cleanup
    modder.FinalizeBuild(atmosphere_path, bars_targets);

    return 0;
}