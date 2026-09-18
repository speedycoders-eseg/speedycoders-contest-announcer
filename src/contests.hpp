#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
namespace bot {
using json = nlohmann::json;
struct Contest { std::string id, platform, name, url, type; int64_t start, duration; };
std::string classify(const std::string& platform, const std::string& name, const std::string& id);
std::vector<Contest> codeforces(const std::string& body);
std::vector<Contest> atcoder(const std::string& body);
json load(const std::filesystem::path& path);
void save(const std::filesystem::path& path, const json& state);
std::vector<std::string> due(const Contest& c, const json& state, int64_t now);
json payload(const Contest& c, const std::string& kind);
void process(const std::vector<Contest>& contests, json& state, const std::function<int64_t()>& now,
             const std::function<void(const json&)>& send, const std::function<void()>& checkpoint);
std::string get(const std::string& url);
std::string normalize_webhook(const std::string& webhook);
void post(const std::string& webhook, const json& body);
}
