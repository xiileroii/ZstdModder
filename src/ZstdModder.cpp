#include <algorithm>
#include <cctype>
#include <cstdio>
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

// Fallback just in case the Makefile didn't define it
#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

namespace fs = std::filesystem;

// OS Detection for command prefixes
#ifdef _WIN32
constexpr const char *kFfmpeg = "ffmpeg.exe";
constexpr const char *kZstd = "zstd.exe";
#else
constexpr const char *kFfmpeg = "ffmpeg";
constexpr const char *kZstd = "zstd";
#endif

// Statically linked tools
extern int brstm_converter_main(int argc, char **args);
extern int auto_bars_patcher_main(int argc, char **args);

// Global State
bool g_list_only = false;
std::string g_exe_path;

// --- Structs ---
struct AudioMetadata
{
    bool is_audio = false;
    int channels = 0;
    int sample_rate = 0;
};

struct BarsTarget
{
    std::string original_zstd;
    std::string uncompressed;
    std::string patched;
    std::string patched_zstd;
    std::vector<std::string> names;
};

struct AppConfig
{
    std::string atmosphere_path;
    std::string game_dump_path;
};

// --- Helpers ---
std::string Trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n\"");
    size_t end = s.find_last_not_of(" \t\r\n\"");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

bool EndsWith(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool IsValidAudioExtension(const fs::path &path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".flac" || ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".m4a");
}

// --- THE NATIVE BWAV PARSER ---
AudioMetadata GetBwavMetadata(const fs::path &path)
{
    AudioMetadata meta = {false, 0, 0};

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return meta;

    uint8_t header[24];
    if (!file.read(reinterpret_cast<char *>(header), 24))
        return meta;

    if (header[0] != 'B' || header[1] != 'W' || header[2] != 'A' || header[3] != 'V')
        return meta;

    bool is_big_endian = (header[4] == 0xFE && header[5] == 0xFF);

    if (is_big_endian)
    {
        meta.channels = (header[0x0E] << 8) | header[0x0F];
        meta.sample_rate = (header[0x14] << 24) | (header[0x15] << 16) | (header[0x16] << 8) | header[0x17];
    }
    else
    {
        meta.channels = (header[0x0F] << 8) | header[0x0E];
        meta.sample_rate = (header[0x17] << 24) | (header[0x16] << 16) | (header[0x15] << 8) | header[0x14];
    }

    meta.is_audio = true;
    return meta;
}

AppConfig GetOrPromptConfig(bool is_headless)
{
    AppConfig config;
    std::ifstream config_file("config.ini");

    if (config_file.is_open())
    {
        std::string line;
        while (std::getline(config_file, line))
        {
            if (line.rfind("ATMOSPHERE_PATH=", 0) == 0)
                config.atmosphere_path = Trim(line.substr(16));
            if (line.rfind("GAME_DUMP_PATH=", 0) == 0)
                config.game_dump_path = Trim(line.substr(15));
        }
        config_file.close();
    }

    if (config.game_dump_path.empty() && fs::exists("Stream"))
        config.game_dump_path = "Stream";

    if (!is_headless)
    {
        bool save_needed = false;
        if (config.atmosphere_path.empty())
        {
            std::cout << "Enter absolute path to Atmosphere contents folder: ";
            std::getline(std::cin, config.atmosphere_path);
            config.atmosphere_path = Trim(config.atmosphere_path);
            save_needed = true;
        }
        if (config.game_dump_path.empty())
        {
            std::cout << "Enter absolute path to original game 'Stream' dump folder: ";
            std::getline(std::cin, config.game_dump_path);
            config.game_dump_path = Trim(config.game_dump_path);
            save_needed = true;
        }
        if (save_needed)
        {
            std::ofstream out_file("config.ini");
            out_file << "ATMOSPHERE_PATH=" << config.atmosphere_path << "\n";
            out_file << "GAME_DUMP_PATH=" << config.game_dump_path << "\n";
        }
    }
    else if (config.game_dump_path.empty() || config.atmosphere_path.empty())
    {
        if (!g_list_only)
        {
            std::cerr << "[Fatal Error] config.ini is missing Atmosphere or Game Dump paths.\n";
            exit(1);
        }
    }
    return config;
}

// --- Cross-Platform Keyboard ---
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
    auto it = std::search(str.begin(), str.end(), query.begin(), query.end(),
                          [](char ch1, char ch2)
                          { return std::toupper(ch1) == std::toupper(ch2); });
    return (it != str.end());
}

class CrossPlatformModder
{
public:
    void RunCmd(const std::string &cmd)
    {
        std::cout << "[Executing]: " << cmd << std::endl; // Flush guarantees we see this before a crash
        int ret = std::system(cmd.c_str());
        if (ret != 0)
        {
            std::cerr << "Command failed with exit code " << ret << ".\n";
            exit(1);
        }
    }

    // --- BUSYBOX ROUTING ---
    void RunConverter(const std::string &input, const std::string &output)
    {
#ifdef _WIN32
        _putenv("__COMPAT_LAYER=VistaSP2");
        std::cout << "[System]: Applying VistaSP2 Compatibility Layer for integrated brstm_converter...\n";
#endif

        // The outer double-quotes prevent cmd.exe from stripping inner path quotes and failing syntax
        std::string cmd = "\"\"" + g_exe_path + "\" brstm_converter \"" + input + "\" -o \"" + output + "\"\"";
        RunCmd(cmd);

#ifdef _WIN32
        _putenv("__COMPAT_LAYER=");
#endif
    }

    void RunPatcher(const std::string &og_stream_dir, const std::string &mod_stream_dir, const std::string &og_bars, const std::string &patched_bars)
    {
        std::cout << "[System]: Running integrated auto_bars_patcher...\n";
        std::string cmd = "\"\"" + g_exe_path + "\" auto_bars_patcher --og-stream-dir \"" + og_stream_dir + "\" --mod-stream-dir \"" + mod_stream_dir + "\" --og-bars-file \"" + og_bars + "\" --bars-output-file \"" + patched_bars + "\"\"";
        RunCmd(cmd);
    }

    std::string ReadNullTerminatedString(std::ifstream &fs)
    {
        std::string s;
        char c;
        while (fs.read(&c, 1) && c != '\0')
            s += c;
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
                names.push_back(asset_name);
        }
        return names;
    }

    std::string InteractiveFileFilter(const std::string &input_filename, const std::vector<std::string> &bars_names)
    {
        std::cout << "\n[!] Target file (" << input_filename << ") not found in the BARS.\n";
        std::cout << "Start typing to search for a target BWAV to replace.\n";

        int display_limit = 10, selected_index = 0, scroll_offset = 0;
        int lines_to_print = display_limit + 3;
        std::string query = "";

        for (int i = 0; i < lines_to_print; i++)
            std::cout << "\n";

        while (true)
        {
            std::vector<std::string> filtered;
            for (const auto &name : bars_names)
                if (CaseInsensitiveMatch(name, query))
                    filtered.push_back(name);
            if (selected_index >= static_cast<int>(filtered.size()))
                selected_index = std::max(0, static_cast<int>(filtered.size()) - 1);
            if (selected_index < scroll_offset)
                scroll_offset = selected_index;
            if (selected_index >= scroll_offset + display_limit)
                scroll_offset = selected_index - display_limit + 1;
            if (scroll_offset > std::max(0, static_cast<int>(filtered.size()) - display_limit))
                scroll_offset = std::max(0, static_cast<int>(filtered.size()) - display_limit);

            std::cout << "\x1b[" << lines_to_print << "A\r";
            std::cout << "\x1b[2KSearch: " << query << "_\n------------------------------\n";
            for (int i = 0; i < display_limit; i++)
            {
                int item_index = scroll_offset + i;
                if (item_index < static_cast<int>(filtered.size()))
                {
                    if (item_index == selected_index)
                        std::cout << "\x1b[2K\x1b[36m> " << filtered[item_index] << "\x1b[0m\n";
                    else
                        std::cout << "\x1b[2K  " << filtered[item_index] << "\n";
                }
                else
                    std::cout << "\x1b[2K\n";
            }
            std::cout << "\x1b[2KMatches: " << filtered.size() << " | Up/Down to navigate | Enter to confirm\n";

            KeyPress kp = GetKeyPress();
            if (kp.action == kEnter && !filtered.empty())
                return filtered[selected_index];
            else if (kp.action == kBackspace && !query.empty())
            {
                query.pop_back();
                selected_index = 0;
                scroll_offset = 0;
            }
            else if (kp.action == kUp && selected_index > 0)
                selected_index--;
            else if (kp.action == kDown && selected_index < (int)filtered.size() - 1)
                selected_index++;
            else if (kp.action == kOther && !std::iscntrl(kp.ch))
            {
                query += kp.ch;
                selected_index = 0;
                scroll_offset = 0;
            }
        }
    }

    void Build(const fs::path &input_path,
               const std::vector<BarsTarget> &bars_targets,
               const std::vector<std::string> &all_bars_names,
               const std::vector<std::string> &existing_mods,
               const AppConfig &config,
               const std::string &target_override, int channel_override, bool auto_yes, bool is_headless)
    {

        std::string original_name = input_path.stem().string();
        std::string full_filename = input_path.filename().u8string();
        std::string target_bwav_name = original_name;

        std::cout << "\n========================================\n";
        std::cout << "[Processing]: " << full_filename << "\n";

        if (!target_override.empty())
        {
            target_bwav_name = target_override;
            std::cout << "[UI Override]: Forcing target name -> '" << target_bwav_name << "'\n";
        }
        else if (std::find(all_bars_names.begin(), all_bars_names.end(), target_bwav_name) == all_bars_names.end())
        {
            if (is_headless)
            {
                std::cerr << "[Fatal Error] '" << target_bwav_name << "' not in BARS.\n";
                return;
            }
            else
            {
                target_bwav_name = InteractiveFileFilter(full_filename, all_bars_names);
                std::cout << "\nMapping Original File -> '" << target_bwav_name << "'\n";
            }
        }

        int final_channels = channel_override > 0 ? channel_override : 2;
        int final_sample_rate = 48000;

        if (channel_override <= 0)
        {
            fs::path dump_bwav = fs::path(config.game_dump_path) / (target_bwav_name + ".bwav");
            if (fs::exists(dump_bwav))
            {
                AudioMetadata nintendo_meta = GetBwavMetadata(dump_bwav);
                if (nintendo_meta.is_audio)
                {
                    final_channels = nintendo_meta.channels;
                    final_sample_rate = nintendo_meta.sample_rate;
                    std::cout << "[Native Parser]: Analyzed original Nintendo file.\n";
                    std::cout << "                 -> Game expects exactly " << final_channels << " channels at " << final_sample_rate << "Hz.\n";
                }
            }
            else
            {
                std::cout << "[Warning]: Original .bwav not found in dump. Defaulting to Stereo 48kHz.\n";
            }
        }
        else
        {
            std::cout << "[UI Override]: Forcing channel count -> " << final_channels << "\n";
        }

        if (std::find(existing_mods.begin(), existing_mods.end(), target_bwav_name) != existing_mods.end())
        {
            if (auto_yes)
            {
                std::cout << "[UI Override]: Auto-overwriting existing mod.\n";
            }
            else if (is_headless)
            {
                std::cerr << "\n[Fatal Error] Mod exists for '" << target_bwav_name << "'.\n";
                return;
            }
            else
            {
                std::string input;
                while (true)
                {
                    std::cout << "\n[Warning] Mod exists. Overwrite? (Y/N): ";
                    std::getline(std::cin, input);
                    input = Trim(input);
                    if (input == "Y" || input == "y")
                        break;
                    if (input == "N" || input == "n")
                    {
                        std::cout << "Skipping.\n";
                        return;
                    }
                }
            }
        }

        std::string output_bwav = target_bwav_name + ".bwav";
        std::string safe_input_ext = input_path.extension().string();
        std::string safe_input = "temp_in_" + target_bwav_name + safe_input_ext;
        std::string temp_wav = "temp_out_" + target_bwav_name + ".wav";

        std::error_code ec;
        fs::copy_file(input_path, safe_input, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            std::cerr << "Failed to read input file: " << ec.message() << "\n";
            return;
        }

        std::cout << "\nConverting and Encoding -> " << output_bwav << "\n";

        RunCmd(std::string(kFfmpeg) + " -y -i \"" + safe_input +
               "\" -ar " + std::to_string(final_sample_rate) + " -ac " + std::to_string(final_channels) + " \"" + temp_wav + "\"");

        RunConverter(temp_wav, "ModStream/" + output_bwav);

        try
        {
            fs::remove(safe_input);
            fs::remove(temp_wav);
        }
        catch (...)
        {
        }
    }

    void FinalizeBuild(const AppConfig &config, const std::vector<BarsTarget> &targets)
    {
        bool has_mods = false;
        if (fs::exists("ModStream"))
        {
            for (const auto &entry : fs::directory_iterator("ModStream"))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".bwav")
                {
                    has_mods = true;
                    break;
                }
            }
        }
        if (!has_mods)
        {
            std::cout << "\n[System] No new files in ./ModStream. Skipping BARS patching.\n";
            return;
        }

        std::cout << "\n[Patching BARS files]\n";

        for (const auto &target : targets)
        {
            RunPatcher(config.game_dump_path, "./ModStream", target.uncompressed, target.patched);
            RunCmd(std::string(kZstd) + " -f " + target.patched + " -o " + target.patched_zstd);
        }

        if (!config.atmosphere_path.empty())
        {
            fs::path dest_stream = fs::path(config.atmosphere_path) / "romfs/Sound/Resource/Stream";
            fs::path dest_bars = fs::path(config.atmosphere_path) / "romfs/Sound/Resource";

            std::cout << "\nDeploying to: " << dest_bars.string() << "\n";
            fs::create_directories(dest_stream);

            try
            {
                for (const auto &entry : fs::directory_iterator("ModStream"))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".bwav")
                    {
                        fs::copy_file(entry.path(), dest_stream / entry.path().filename(), fs::copy_options::overwrite_existing);
                    }
                }
                for (const auto &target : targets)
                {
                    fs::copy_file(target.patched_zstd, dest_bars / target.original_zstd, fs::copy_options::overwrite_existing);
                }
                std::cout << "Successfully deployed to Atmosphere!\n";

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
    // FATAL FIX 2: Disable all buffering immediately so Python captures crash logs!
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc > 0 && argv[0] != nullptr)
    {
        g_exe_path = argv[0];
    }
    else
    {
        g_exe_path = "ZstdModder.exe"; // Fallback
    }

    // --- BUSYBOX ROUTING ---
    // If the executable is called with an internal tool name as the first argument,
    // intercept it, shift the arguments, and run the statically linked module.
    if (argc > 1)
    {
        std::string tool_mode = argv[1];
        if (tool_mode == "brstm_converter" || tool_mode == "auto_bars_patcher")
        {
            std::vector<char *> sub_argv;
            sub_argv.push_back(argv[0]);
            for (int i = 2; i < argc; ++i)
                sub_argv.push_back(argv[i]);

            // FATAL FIX 1: Provide the NULL terminator to prevent C library segfaults!
            sub_argv.push_back(nullptr);

            int sub_argc = sub_argv.size() - 1; // Size minus the nullptr

            if (tool_mode == "brstm_converter")
                return brstm_converter_main(sub_argc, sub_argv.data());
            if (tool_mode == "auto_bars_patcher")
                return auto_bars_patcher_main(sub_argc, sub_argv.data());
        }
    }

    std::cout << "ZstdModder v" << APP_VERSION << " initialized.\n";

    fs::path input_path;
    bool is_headless = false, auto_yes = false;
    bool no_patch = false, patch_only = false;
    std::string t_over = "";
    int c_over = 0;

#ifdef _WIN32
    int wargc;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv)
    {
        for (int i = 1; i < wargc; ++i)
        {
            std::wstring wa(wargv[i]);
            std::string a(wa.begin(), wa.end());
            if (a == "-y")
            {
                auto_yes = true;
                is_headless = true;
            }
            else if (a == "--list")
            {
                g_list_only = true;
                is_headless = true;
            }
            else if (a == "--no-patch")
            {
                no_patch = true;
                is_headless = true;
            }
            else if (a == "--patch-only")
            {
                patch_only = true;
                is_headless = true;
            }
            else if (a == "-t" && i + 1 < wargc)
            {
                std::wstring wn(wargv[++i]);
                t_over = std::string(wn.begin(), wn.end());
                is_headless = true;
            }
            else if (a == "-c" && i + 1 < wargc)
            {
                std::wstring wn(wargv[++i]);
                c_over = std::stoi(std::string(wn.begin(), wn.end()));
                is_headless = true;
            }
            else if (input_path.empty())
                input_path = fs::path(wargv[i]);
        }
        LocalFree(wargv);
    }
#else
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string a(argv[i]);
            if (a == "-y")
            {
                auto_yes = true;
                is_headless = true;
            }
            else if (a == "--list")
            {
                g_list_only = true;
                is_headless = true;
            }
            else if (a == "--no-patch")
            {
                no_patch = true;
                is_headless = true;
            }
            else if (a == "--patch-only")
            {
                patch_only = true;
                is_headless = true;
            }
            else if (a == "-t" && i + 1 < argc)
            {
                t_over = std::string(argv[++i]);
                is_headless = true;
            }
            else if (a == "-c" && i + 1 < argc)
            {
                c_over = std::stoi(std::string(argv[++i]));
                is_headless = true;
            }
            else if (input_path.empty())
            {
                input_path = fs::path(argv[i]);
            }
        }
    }
#endif

    if (input_path.empty())
        input_path = fs::current_path();
    AppConfig cfg = GetOrPromptConfig(is_headless);
    fs::create_directories("ModStream");

    std::vector<std::string> ex_mods;
    if (!cfg.atmosphere_path.empty() && !g_list_only)
    {
        fs::path d_stream = fs::path(cfg.atmosphere_path) / "romfs/Sound/Resource/Stream";
        if (fs::exists(d_stream))
        {
            for (const auto &e : fs::directory_iterator(d_stream))
                if (e.path().extension() == ".bwav")
                {
                    ex_mods.push_back(e.path().stem().string());
                    if (!patch_only)
                        fs::copy_file(e.path(), fs::path("ModStream") / e.path().filename(), fs::copy_options::overwrite_existing);
                }
        }
    }

    CrossPlatformModder modder;
    std::vector<BarsTarget> b_targets;
    std::vector<std::string> all_names;

    bool should_extract = g_list_only || patch_only || (!is_headless && !no_patch);

    if (should_extract)
    {
        for (const auto &e : fs::directory_iterator(fs::current_path()))
        {
            std::string fn = e.path().filename().string();
            if (EndsWith(fn, ".bars.zs") && !EndsWith(fn, "Upd.bars.zs"))
            {
                BarsTarget t;
                t.original_zstd = fn;
                t.uncompressed = fn.substr(0, fn.size() - 3);
                t.patched = t.uncompressed.substr(0, t.uncompressed.size() - 5) + "Upd.bars";
                t.patched_zstd = t.patched + ".zs";

                if (!g_list_only)
                {
                    modder.RunCmd(std::string(kZstd) + " -d -f " + t.original_zstd + " -o " + t.uncompressed);
                }
                else
                {
                    std::system((std::string(kZstd) + " -d -q -f " + t.original_zstd + " -o " + t.uncompressed).c_str());
                }

                t.names = modder.ParseBarsNames(t.uncompressed);
                all_names.insert(all_names.end(), t.names.begin(), t.names.end());
                b_targets.push_back(t);
            }
        }

        if (b_targets.empty())
        {
            std::cerr << "[Fatal Error] No .bars.zs files found in the current directory.\n";
            return 1;
        }
    }

    if (g_list_only)
    {
        for (const auto &name : all_names)
            std::cout << "BARS_TARGET:" << name << "\n";
        return 0;
    }

    if (patch_only)
    {
        modder.FinalizeBuild(cfg, b_targets);
        return 0;
    }

    // --- Processing ---
    if (fs::is_directory(input_path))
    {
        for (const auto &e : fs::directory_iterator(input_path))
        {
            if (IsValidAudioExtension(e.path()))
            {
                modder.Build(e.path(), b_targets, all_names, ex_mods, cfg, t_over, c_over, auto_yes, is_headless);
            }
        }
    }
    else
    {
        if (IsValidAudioExtension(input_path))
        {
            modder.Build(input_path, b_targets, all_names, ex_mods, cfg, t_over, c_over, auto_yes, is_headless);
        }
        else
        {
            std::cerr << "[Fatal Error] Input file is not a supported audio format.\n";
        }
    }

    if (!no_patch)
    {
        modder.FinalizeBuild(cfg, b_targets);
    }
    return 0;
}