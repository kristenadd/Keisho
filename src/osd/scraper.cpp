// ██╗  ██╗███████╗██╗███████╗██╗  ██╗ ██████╗
// ██║ ██╔╝██╔════╝██║██╔════╝██║  ██║██╔═══██╗
// █████╔╝ █████╗  ██║███████╗███████║██║   ██║
// ██╔═██╗ ██╔══╝  ██║╚════██║██╔══██║██║   ██║
// ██║  ██╗███████╗██║███████║██║  ██║╚██████╔╝
// ╚═╝  ╚═╝╚══════╝╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝
//            継 承  ( k e i s h ō )
//
// A Sega Model 1, 2 & 3 arcade emulator.
// Copyright (c) 2025+ Daniel Martin (dmanlfc)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This header must not be removed. Keisho is licensed under the GNU General
// Public License, version 3 or later, with additional terms under section 7 of
// that licence. See LICENSE and NOTICE.
//
#include "osd/scraper.h"

#include "core/log.h"
#include "core/types.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#if defined(SM2_HAVE_CURL)
#include <curl/curl.h>
#endif

namespace sm2::osd {
namespace {

// Not a general JSON parser: ArcadeDB's reply and the cache file are both flat
// objects of string values.

/// Decode a JSON string body, resolving \" \\ \/ \n \r \t \b \f and \uXXXX
/// (BMP only, to UTF-8).
[[nodiscard]] std::string decode_json_string(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (usize i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c != '\\' || i + 1 >= raw.size()) {
            out.push_back(c);
            continue;
        }
        const char esc = raw[++i];
        switch (esc) {
            case '"':  out.push_back('"');  break;
            case '\\': out.push_back('\\'); break;
            case '/':  out.push_back('/');  break;
            case 'n':  out.push_back('\n'); break;
            case 'r':  out.push_back('\r'); break;
            case 't':  out.push_back('\t'); break;
            case 'b':  out.push_back('\b'); break;
            case 'f':  out.push_back('\f'); break;
            case 'u': {
                if (i + 4 >= raw.size()) { out.push_back('?'); break; }
                unsigned code = 0;
                bool     ok   = true;
                for (int k = 0; k < 4; ++k) {
                    const char h = raw[i + 1 + static_cast<usize>(k)];
                    code <<= 4;
                    if (h >= '0' && h <= '9')      code |= static_cast<unsigned>(h - '0');
                    else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                    else { ok = false; break; }
                }
                if (!ok) { out.push_back('?'); break; }
                i += 4;
                // BMP -> UTF-8; surrogate pairs are not handled (ArcadeDB's text
                // stays in the BMP).
                if (code < 0x80) {
                    out.push_back(static_cast<char>(code));
                } else if (code < 0x800) {
                    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else {
                    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                break;
            }
            default: out.push_back(esc); break;
        }
    }
    return out;
}

/// String value of `"key"` in a flat JSON object, or empty. Escapes in the
/// value are honoured so an embedded \" does not end it early.
[[nodiscard]] std::string json_string_field(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    usize pos = 0;
    while ((pos = json.find(needle, pos)) != std::string::npos) {
        usize i = pos + needle.size();
        // Require a colon, so "title" does not match inside "short_title".
        while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) ++i;
        if (i >= json.size() || json[i] != ':') { pos += needle.size(); continue; }
        ++i;
        while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) ++i;
        if (i >= json.size() || json[i] != '"') { pos += needle.size(); continue; }
        ++i;
        const usize start = i;
        while (i < json.size()) {
            if (json[i] == '\\') { i += 2; continue; }
            if (json[i] == '"') break;
            ++i;
        }
        return decode_json_string(json.substr(start, i - start));
    }
    return {};
}

/// Integer value of `"key"` (bare or quoted digits), or `fallback`.
[[nodiscard]] long long json_number_field(const std::string& json, const std::string& key,
                                          long long fallback = 0)
{
    const std::string needle = "\"" + key + "\"";
    usize pos = 0;
    while ((pos = json.find(needle, pos)) != std::string::npos) {
        usize i = pos + needle.size();
        while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) ++i;
        if (i >= json.size() || json[i] != ':') { pos += needle.size(); continue; }
        ++i;
        while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '"')) ++i;
        const usize start = i;
        if (i < json.size() && json[i] == '-') ++i;
        while (i < json.size() && json[i] >= '0' && json[i] <= '9') ++i;
        if (i == start) return fallback;
        return std::strtoll(json.substr(start, i - start).c_str(), nullptr, 10);
    }
    return fallback;
}

/// The objects of the array `"key"`, each as its own JSON text (nested objects
/// stay inside their parent). Strings are skipped so braces in them are inert.
[[nodiscard]] std::vector<std::string> json_array_objects(const std::string& json, const std::string& key)
{
    std::vector<std::string> objects;
    const usize k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return objects;
    usize i = json.find('[', k);
    if (i == std::string::npos) return objects;
    int   depth = 0;
    usize start = 0;
    for (++i; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '"') {
            for (++i; i < json.size() && json[i] != '"'; ++i) {
                if (json[i] == '\\') ++i;
            }
            continue;
        }
        if (c == '{') {
            if (depth++ == 0) start = i;
        } else if (c == '}') {
            if (--depth == 0) objects.push_back(json.substr(start, i - start + 1));
        } else if (c == ']' && depth == 0) {
            break;
        }
    }
    return objects;
}

/// A title reduced for comparison: ASCII letters and digits, lower case, "&"
/// read as "and", a leading "the" dropped. "The House of the Dead 2" and
/// "House of The Dead 2" compare equal.
[[nodiscard]] std::string normalized_title(const std::string& title)
{
    std::string out;
    for (const char ch : title) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) && c < 0x80) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == '&') {
            out += "and";
        }
    }
    if (out.rfind("the", 0) == 0 && out.size() > 3) out.erase(0, 3);
    return out;
}

/// A set's title without its "(Revision A)" / "[Japan]" style qualifiers.
[[nodiscard]] std::string search_title(const std::string& title)
{
    std::string out;
    int         depth = 0;
    for (const char c : title) {
        if (c == '(' || c == '[') { ++depth; continue; }
        if (c == ')' || c == ']') { if (depth > 0) --depth; continue; }
        if (depth == 0) out.push_back(c);
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    while (!out.empty() && out.front() == ' ') out.erase(0, 1);
    usize sp;
    while ((sp = out.find("  ")) != std::string::npos) out.erase(sp, 1);
    return out;
}

/// The calendar year of a Unix time (seconds, or milliseconds if that large).
[[nodiscard]] int year_of_unix_time(long long t)
{
    if (t <= 0) return 0;
    if (t > 100000000000LL) t /= 1000;
    const std::chrono::sys_days day = std::chrono::floor<std::chrono::days>(
        std::chrono::sys_seconds{std::chrono::seconds{t}});
    return static_cast<int>(std::chrono::year_month_day{day}.year());
}

/// Whether `"result"` is an empty array -- ArcadeDB's "not found" reply.
[[nodiscard]] bool result_is_empty(const std::string& json)
{
    const usize r = json.find("\"result\"");
    if (r == std::string::npos) return true;
    usize i = r + 8;
    while (i < json.size() && json[i] != '[') ++i;
    ++i;
    while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n'
                               || json[i] == '\r')) ++i;
    return i < json.size() && json[i] == ']';
}

[[nodiscard]] std::string encode_json_string(const std::string& text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

/// Written into each <name>.sgdb.json; bump to look every set up again.
constexpr const char* kSgdbCacheVersion = "1";

[[nodiscard]] std::string read_file_text(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

#if defined(SM2_HAVE_CURL)
namespace {

constexpr const char* kUserAgent =
    "Keisho/1.0 (Sega Model 1, 2 & 3 emulator; game picker artwork)";
constexpr long kConnectTimeoutSeconds = 8;
constexpr long kTotalTimeoutSeconds   = 15;
/// Written into each cache file; bump to re-fetch sets recorded as not found.
constexpr const char* kCacheVersion = "2";

size_t append_to_string(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

/// GET `url` into `out`; true on a 2xx with a body. Time-bounded and non-fatal.
/// `bearer` adds an Authorization header; `status` receives the HTTP code.
[[nodiscard]] bool http_get(const std::string& url, std::string* out,
                            const std::string& bearer = {}, long* status = nullptr)
{
    if (status != nullptr) *status = 0;
    CURL* curl = curl_easy_init();
    if (curl == nullptr) return false;

    curl_slist* headers = nullptr;
    if (!bearer.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + bearer).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    out->clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTotalTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);  // thread-safe timeouts
    // ArcadeDB gzips its JSON whether or not it was asked to; "" offers every
    // encoding this libcurl can decode, so the body arrives as plain text.
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);

    const CURLcode rc = curl_easy_perform(curl);
    if (status != nullptr) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    if (rc != CURLE_OK) {
        // Never log the URL with a key in it; SteamGridDB's key is a header.
        SM2_DEBUG("scraper: GET %s failed: %s", url.c_str(), curl_easy_strerror(rc));
        return false;
    }
    return !out->empty();
}

/// `text` percent-encoded for a URL path segment or query value.
[[nodiscard]] std::string url_escape(const std::string& text)
{
    std::string out;
    static const char* hex = "0123456789ABCDEF";
    for (const char ch : text) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) && c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

constexpr const char* kSgdbApi = "https://www.steamgriddb.com/api/v2";

}  // namespace
#endif  // SM2_HAVE_CURL

Scraper::~Scraper()
{
    stop();
}

void Scraper::start(std::string artwork_dir, std::vector<Entry> entries, bool enabled,
                    std::string steamgriddb_key)
{
    m_artwork_dir = std::move(artwork_dir);
    m_entries     = std::move(entries);
    m_sgdb_key    = std::move(steamgriddb_key);
    while (!m_sgdb_key.empty() && std::isspace(static_cast<unsigned char>(m_sgdb_key.back()))) m_sgdb_key.pop_back();
    while (!m_sgdb_key.empty() && std::isspace(static_cast<unsigned char>(m_sgdb_key.front()))) m_sgdb_key.erase(0, 1);

#if defined(SM2_HAVE_CURL)
    m_scraping = enabled;
#else
    m_scraping = false;
    if (enabled) {
        SM2_INFO("scraper: built without libcurl; artwork scraping unavailable, "
                 "picker runs offline");
    }
#endif

    if (!m_scraping) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(m_artwork_dir, error);
    if (error) {
        SM2_WARN("scraper: could not create artwork dir '%s'; running offline",
                 m_artwork_dir.c_str());
        m_scraping = false;
        return;
    }

    m_stop.store(false);
    m_thread = std::thread(&Scraper::run, this);
}

void Scraper::stop()
{
    m_stop.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

std::vector<std::string> Scraper::take_ready()
{
    std::vector<std::string> ready;
    std::lock_guard<std::mutex> lock(m_ready_mutex);
    while (!m_ready.empty()) {
        ready.push_back(std::move(m_ready.front()));
        m_ready.pop();
    }
    return ready;
}

Scraper::Metadata Scraper::load_metadata(const std::string& name,
                                         const std::string& fallback_title) const
{
    Metadata meta;
    meta.title = fallback_title;

    const std::filesystem::path json_path =
        std::filesystem::path(m_artwork_dir) / (name + ".json");
    const std::string json = read_file_text(json_path);
    if (json.empty()) {
        return meta;  // nothing cached yet
    }

    meta.found = json_string_field(json, "found") == "true";
    const std::string title = json_string_field(json, "title");
    if (!title.empty()) {
        meta.title = title;
    }
    meta.year         = json_string_field(json, "year");
    meta.manufacturer = json_string_field(json, "manufacturer");
    meta.genre        = json_string_field(json, "genre");
    meta.description  = json_string_field(json, "description");

    // "images" is a comma-separated filename list; keep those still on disk.
    const std::string images = json_string_field(json, "images");
    usize             start  = 0;
    while (start <= images.size()) {
        const usize comma = images.find(',', start);
        const std::string file =
            images.substr(start, comma == std::string::npos ? std::string::npos
                                                            : comma - start);
        if (!file.empty()) {
            const std::filesystem::path path = std::filesystem::path(m_artwork_dir) / file;
            std::error_code             error;
            if (std::filesystem::exists(path, error) && !error) {
                meta.image_paths.push_back(path.string());
            }
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }

    // A SteamGridDB vertical cover replaces ArcadeDB's pictures; those stay
    // on disk as the fallback for sets SteamGridDB has no match for.
    const std::string grid  = read_file_text(std::filesystem::path(m_artwork_dir) / (name + ".sgdb.json"));
    const std::string image = json_string_field(grid, "image");
    if (!image.empty()) {
        const std::filesystem::path path = std::filesystem::path(m_artwork_dir) / image;
        std::error_code             error;
        if (std::filesystem::exists(path, error) && !error) {
            meta.image_paths.assign(1, path.string());
        }
    }
    return meta;
}

bool Scraper::sgdb_cached(const std::string& name) const
{
    const std::string cached =
        read_file_text(std::filesystem::path(m_artwork_dir) / (name + ".sgdb.json"));
    return json_string_field(cached, "cache") == kSgdbCacheVersion;
}

#if defined(SM2_HAVE_CURL)

bool Scraper::fetch_arcadedb(const Entry& entry)
{
    // A cached marker (done or recorded-missing) means leave it alone,
    // except a not-found marker from before kCacheVersion: older builds
    // read ArcadeDB's gzipped reply as empty and marked every set missing.
    const std::filesystem::path json_path =
        std::filesystem::path(m_artwork_dir) / (entry.name + ".json");
    std::error_code error;
    if (std::filesystem::exists(json_path, error) && !error) {
        const std::string cached = read_file_text(json_path);
        if (json_string_field(cached, "found") == "true"
            || json_string_field(cached, "cache") == kCacheVersion) {
            return false;
        }
    }

    const std::string meta_url =
        "https://adb.arcadeitalia.net/service_scraper.php?ajax=query_mame&lang=en"
        "&game_name=" + entry.name;
    std::string body;
    if (!http_get(meta_url, &body) || body.find("\"result\"") == std::string::npos) {
        return false;  // transient failure or not JSON: no marker, retry next run
    }

    std::string              title, year, manufacturer, genre, description;
    std::vector<std::string> image_files;
    bool                     found = false;

    if (!result_is_empty(body)) {
        found        = true;
        title        = json_string_field(body, "short_title");
        if (title.empty()) title = json_string_field(body, "title");
        year         = json_string_field(body, "year");
        manufacturer = json_string_field(body, "manufacturer");
        genre        = json_string_field(body, "genre");
        description  = json_string_field(body, "history");

        // Every artwork type the server has, in display order. The URLs
        // carry resize=0 (full size, large and slow); rewrite to a bounded
        // size so each fetch is quick and the cache stays small.
        const struct { const char* field; const char* suffix; } kinds[] = {
            {"url_image_flyer",   "flyer"},
            {"url_image_title",   "title"},
            {"url_image_ingame",  "ingame"},
            {"url_image_marquee", "marquee"},
        };
        for (const auto& kind : kinds) {
            if (m_stop.load()) break;
            std::string url = json_string_field(body, kind.field);
            if (url.empty()) continue;
            const usize r = url.find("resize=0");
            if (r != std::string::npos) {
                url.replace(r, 8, "resize=400");
            }
            std::string image_bytes;
            if (http_get(url, &image_bytes) && image_bytes.size() > 64) {
                const std::string file =
                    entry.name + "." + kind.suffix + ".png";
                std::ofstream img(std::filesystem::path(m_artwork_dir) / file,
                                  std::ios::binary | std::ios::trunc);
                img.write(image_bytes.data(),
                          static_cast<std::streamsize>(image_bytes.size()));
                image_files.push_back(file);
            }
        }
    }

    // The cache file, written even for a not-found set so it is not
    // re-fetched every launch.
    {
        std::string image_list;
        for (usize k = 0; k < image_files.size(); ++k) {
            if (k != 0) image_list += ",";
            image_list += image_files[k];
        }
        std::ofstream out(json_path, std::ios::trunc);
        out << "{\n"
            << "  \"cache\": \"" << kCacheVersion << "\",\n"
            << "  \"found\": \"" << (found ? "true" : "false") << "\",\n"
            << "  \"title\": \"" << encode_json_string(title) << "\",\n"
            << "  \"year\": \"" << encode_json_string(year) << "\",\n"
            << "  \"manufacturer\": \"" << encode_json_string(manufacturer) << "\",\n"
            << "  \"genre\": \"" << encode_json_string(genre) << "\",\n"
            << "  \"images\": \"" << encode_json_string(image_list) << "\",\n"
            << "  \"description\": \"" << encode_json_string(description) << "\"\n"
            << "}\n";
    }
    return true;
}

bool Scraper::fetch_steamgriddb(const Entry& entry, std::map<std::string, std::string>* grid_by_title)
{
    namespace fs = std::filesystem;
    const fs::path dir       = m_artwork_dir;
    const fs::path json_path = dir / (entry.name + ".sgdb.json");

    // Search by ArcadeDB's clean title and year when cached, else the
    // games.xml title without its "(Revision A)" style qualifiers.
    const std::string adb  = read_file_text(dir / (entry.name + ".json"));
    std::string       name = json_string_field(adb, "title");
    if (name.empty()) name = entry.title;
    name = search_title(name);
    const int year = std::atoi(json_string_field(adb, "year").c_str());
    if (name.empty()) return false;

    const auto write_marker = [&](bool found, long long game_id, const std::string& game_name,
                                  const std::string& image) {
        std::ofstream out(json_path, std::ios::trunc);
        out << "{\n"
            << "  \"cache\": \"" << kSgdbCacheVersion << "\",\n"
            << "  \"found\": \"" << (found ? "true" : "false") << "\",\n"
            << "  \"searched\": \"" << encode_json_string(name) << "\",\n"
            << "  \"game_id\": \"" << game_id << "\",\n"
            << "  \"game_name\": \"" << encode_json_string(game_name) << "\",\n"
            << "  \"image\": \"" << encode_json_string(image) << "\"\n"
            << "}\n";
    };

    // Clones share their parent's title: reuse the cover already fetched.
    const std::string key = normalized_title(name) + "|" + std::to_string(year);
    if (const auto it = grid_by_title->find(key); it != grid_by_title->end()) {
        if (it->second.empty()) {
            write_marker(false, 0, {}, {});
            return true;
        }
        const std::string ext  = fs::path(it->second).extension().string();
        const std::string file = entry.name + ".grid" + ext;
        std::error_code   error;
        fs::copy_file(dir / it->second, dir / file, fs::copy_options::overwrite_existing, error);
        if (error) return false;
        write_marker(true, 0, {}, file);
        return true;
    }

    // 1. Find the game: the name must match; the year must too when both are
    //    known (within a year, for regional and port release dates). ArcadeDB
    //    titles can carry several names ("Scud Race / Sega Super GT") and a
    //    cabinet ("Sega Rally Championship - Twin/DX"): each name is tried
    //    whole, then without its " - " tail, until one matches.
    std::vector<std::string> candidates;
    const auto add_candidate = [&](std::string c) {
        c = search_title(c);
        if (!c.empty() && std::find(candidates.begin(), candidates.end(), c) == candidates.end()) {
            candidates.push_back(std::move(c));
        }
    };
    add_candidate(name);
    for (usize from = 0;;) {
        const usize slash = name.find(" / ", from);
        std::string part  = name.substr(from, slash == std::string::npos ? std::string::npos : slash - from);
        add_candidate(part);
        if (const usize dash = part.find(" - "); dash != std::string::npos) add_candidate(part.substr(0, dash));
        if (slash == std::string::npos) break;
        from = slash + 3;
    }

    long        status = 0;
    std::string body;
    long long   best_id  = 0;
    std::string best_name;
    for (const std::string& candidate : candidates) {
        if (m_stop.load()) return false;
        if (!http_get(std::string(kSgdbApi) + "/search/autocomplete/" + url_escape(candidate), &body,
                      m_sgdb_key, &status)) {
            if (status == 401 || status == 403) {
                SM2_WARN("scraper: SteamGridDB refused the API key (HTTP %ld); skipping it this run", status);
                m_sgdb_key.clear();
            }
            if (status == 404) continue;  // no results for this name
            return false;
        }
        const std::string wanted   = normalized_title(candidate);
        int               best_gap = 1000;
        for (const std::string& game : json_array_objects(body, "data")) {
            const std::string game_name = json_string_field(game, "name");
            if (normalized_title(game_name) != wanted) continue;
            const int game_year = year_of_unix_time(json_number_field(game, "release_date"));
            int       gap       = 999;  // a date missing on either side: a weak match
            if (year > 0 && game_year > 0) {
                gap = std::abs(game_year - year);
                if (gap > 1) continue;
            }
            if (gap < best_gap) {
                best_gap  = gap;
                best_id   = json_number_field(game, "id");
                best_name = game_name;
            }
        }
        if (best_id > 0) break;
    }
    if (best_id <= 0) {
        SM2_INFO("scraper: SteamGridDB has no match for '%s' (%d)", name.c_str(), year);
        write_marker(false, 0, {}, {});
        (*grid_by_title)[key] = {};
        return true;
    }

    // 2. Its best-rated vertical grid: Steam's 600x900 capsule first, then the
    //    older portrait sizes. PNG or JPEG only (stb reads no WebP).
    std::string url;
    for (const char* dims : {"600x900", "342x482,660x930"}) {
        if (m_stop.load()) return false;
        const std::string grids_url = std::string(kSgdbApi) + "/grids/game/" + std::to_string(best_id)
                                    + "?dimensions=" + dims
                                    + "&mimes=image/png,image/jpeg&types=static&nsfw=false&humor=false";
        if (!http_get(grids_url, &body, m_sgdb_key, &status)) {
            if (status != 404) return false;  // transient: retry next run
            continue;
        }
        for (const std::string& grid : json_array_objects(body, "data")) {
            const std::string u = json_string_field(grid, "url");
            if (!u.empty() && u.find(".webp") == std::string::npos) { url = u; break; }
        }
        if (!url.empty()) break;
    }
    if (url.empty()) {
        SM2_INFO("scraper: SteamGridDB has no vertical grid for '%s'", best_name.c_str());
        write_marker(false, best_id, best_name, {});
        (*grid_by_title)[key] = {};
        return true;
    }

    // 3. The image itself, from SteamGridDB's CDN (no key needed).
    std::string bytes;
    if (!http_get(url, &bytes) || bytes.size() < 64) return false;
    const bool        jpeg = url.find(".jpg") != std::string::npos || url.find(".jpeg") != std::string::npos;
    const std::string file = entry.name + (jpeg ? ".grid.jpg" : ".grid.png");
    {
        std::ofstream img(dir / file, std::ios::binary | std::ios::trunc);
        img.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    SM2_INFO("scraper: SteamGridDB cover for %s: '%s' (game %lld)", entry.name.c_str(),
             best_name.c_str(), best_id);
    write_marker(true, best_id, best_name, file);
    (*grid_by_title)[key] = file;
    return true;
}

void Scraper::run()
{
    // Safe unguarded because this is the only thread that ever touches curl.
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        SM2_WARN("scraper: curl_global_init failed; running offline");
        return;
    }

    // One SteamGridDB lookup per title and year; clones reuse the cover.
    std::map<std::string, std::string> grid_by_title;

    for (const Entry& entry : m_entries) {
        if (m_stop.load()) break;

        bool changed = fetch_arcadedb(entry);
        if (!m_sgdb_key.empty() && !m_stop.load() && !sgdb_cached(entry.name)) {
            changed = fetch_steamgriddb(entry, &grid_by_title) || changed;
        }
        if (!changed) continue;

        {
            std::lock_guard<std::mutex> lock(m_ready_mutex);
            m_ready.push(entry.name);
        }

        // A small gap between sets so a full sweep is not a burst; stop-flag
        // breaks it early.
        for (int i = 0; i < 10 && !m_stop.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

    curl_global_cleanup();
}

#else  // no libcurl

void Scraper::run()
{
    // Never reached (m_scraping stays false without libcurl); defined so the
    // thread ctor's address-of resolves.
}

#endif  // SM2_HAVE_CURL

}  // namespace sm2::osd
