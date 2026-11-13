#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include "Movie.hpp"
#include "DesignPatterns.hpp"

class UserProfile {
public:
    explicit UserProfile(std::string path);

    void addObserver(IProfileObserver* observer);
    void removeObserver(IProfileObserver* observer);

    void like(int movieId);
    void unlike(int movieId);
    void addToWatchLater(int movieId);
    void removeFromWatchLater(int movieId);

    bool isLiked(int movieId) const;
    bool isInWatchLater(int movieId) const;

    const std::vector<int>& likedMovies() const { return likedOrder_; }
    const std::vector<int>& watchLaterMovies() const { return watchLaterOrder_; }

    void save() const;
    std::vector<int> recommendSimilarToLikes(const std::vector<Movie>& allMovies, size_t topN = 5) const;

private:
    std::string path_;
    std::unordered_set<int> liked_;
    std::unordered_set<int> watchLater_;
    std::vector<int> likedOrder_;
    std::vector<int> watchLaterOrder_;
    std::vector<IProfileObserver*> observers_;

    void load();
    void notifyObservers();
    static double similarity(const Movie& a, const Movie& b);
};