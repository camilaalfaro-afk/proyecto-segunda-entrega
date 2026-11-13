#include "UserProfile.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <future>
#include <thread>

UserProfile::UserProfile(std::string path) : path_(std::move(path)) {
    load();
}

void UserProfile::addObserver(IProfileObserver* observer) {
    if (observer && std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
        observers_.push_back(observer);
    }
}

void UserProfile::removeObserver(IProfileObserver* observer) {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
}

void UserProfile::notifyObservers() {
    for (auto* obs : observers_) {
        obs->onProfileUpdated();
    }
}

void UserProfile::load() {
    std::ifstream in(path_);
    if (!in.is_open()) return;

    std::string line;
    enum class Section { None, Likes, WatchLater } section = Section::None;
    while (std::getline(in, line)) {
        if (line == "LIKES") { section = Section::Likes; continue; }
        if (line == "WATCHLATER") { section = Section::WatchLater; continue; }
        if (line.empty()) continue;
        try {
            int id = std::stoi(line);
            if (section == Section::Likes && liked_.insert(id).second) likedOrder_.push_back(id);
            if (section == Section::WatchLater && watchLater_.insert(id).second) watchLaterOrder_.push_back(id);
        } catch (...) {}
    }
}

void UserProfile::save() const {
    std::ofstream out(path_, std::ios::trunc);
    if (!out.is_open()) return;
    out << "LIKES\n";
    for (int id : likedOrder_) out << id << "\n";
    out << "WATCHLATER\n";
    for (int id : watchLaterOrder_) out << id << "\n";
}

void UserProfile::like(int movieId) {
    if (liked_.insert(movieId).second) {
        likedOrder_.push_back(movieId);
        save();
        notifyObservers();
    }
}

void UserProfile::unlike(int movieId) {
    if (liked_.erase(movieId)) {
        likedOrder_.erase(std::remove(likedOrder_.begin(), likedOrder_.end(), movieId), likedOrder_.end());
        save();
        notifyObservers();
    }
}

void UserProfile::addToWatchLater(int movieId) {
    if (watchLater_.insert(movieId).second) {
        watchLaterOrder_.push_back(movieId);
        save();
        notifyObservers();
    }
}

void UserProfile::removeFromWatchLater(int movieId) {
    if (watchLater_.erase(movieId)) {
        watchLaterOrder_.erase(std::remove(watchLaterOrder_.begin(), watchLaterOrder_.end(), movieId), watchLaterOrder_.end());
        save();
        notifyObservers();
    }
}

bool UserProfile::isLiked(int movieId) const { return liked_.count(movieId) > 0; }
bool UserProfile::isInWatchLater(int movieId) const { return watchLater_.count(movieId) > 0; }

double UserProfile::similarity(const Movie& a, const Movie& b) {
    std::unordered_set<std::string> setA, setB;
    for (const auto& g : a.genreTokens) setA.insert("g:" + g);
    for (const auto& c : a.castTokens)  setA.insert("c:" + c);
    if (!a.director.empty())            setA.insert("d:" + a.director);

    for (const auto& g : b.genreTokens) setB.insert("g:" + g);
    for (const auto& c : b.castTokens)  setB.insert("c:" + c);
    if (!b.director.empty())            setB.insert("d:" + b.director);

    if (setA.empty() || setB.empty()) return 0.0;

    size_t interseccion = 0;
    for (const auto& item : setA) if (setB.count(item)) ++interseccion;
    size_t unionSize = setA.size() + setB.size() - interseccion;
    if (unionSize == 0) return 0.0;
    return static_cast<double>(interseccion) / static_cast<double>(unionSize);
}

std::vector<int> UserProfile::recommendSimilarToLikes(const std::vector<Movie>& allMovies, size_t topN) const {
    if (likedOrder_.empty() || allMovies.empty()) return {};

    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    size_t chunkSize = (allMovies.size() + numThreads - 1) / numThreads;
    std::vector<std::future<std::vector<std::pair<int, double>>>> futures;

    for (unsigned int t = 0; t < numThreads; ++t) {
        size_t start = t * chunkSize;
        if (start >= allMovies.size()) break;
        size_t end = std::min(start + chunkSize, allMovies.size());

        futures.push_back(std::async(std::launch::async, [this, &allMovies, start, end]() {
            std::vector<std::pair<int, double>> localScores;
            for (size_t idx = start; idx < end; ++idx) {
                const Movie& candidate = allMovies[idx];
                if (liked_.count(candidate.id) || watchLater_.count(candidate.id)) continue;

                double maxSim = 0.0;
                for (int likedId : likedOrder_) {
                    if (likedId < 0 || likedId >= static_cast<int>(allMovies.size())) continue;
                    double s = similarity(allMovies[likedId], candidate);
                    if (s > maxSim) maxSim = s;
                }
                if (maxSim > 0.0) {
                    localScores.emplace_back(candidate.id, maxSim);
                }
            }
            return localScores;
        }));
    }

    std::vector<std::pair<int, double>> ranked;
    for (auto& fut : futures) {
        auto partial = fut.get();
        ranked.insert(ranked.end(), partial.begin(), partial.end());
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::vector<int> result;
    for (size_t i = 0; i < ranked.size() && i < topN; ++i) {
        result.push_back(ranked[i].first);
    }
    return result;
}