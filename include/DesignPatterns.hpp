#pragma once
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include "Movie.hpp"

// Estructuras compartidas de busqueda
struct SearchResult {
    int movieId;
    int score;
    int matchedTerms;
};

enum class TagField { Director, Cast, Genre };

// Forward declaration para evitar dependencias circulares
class SearchEngine;

// ============================================================================
// PATRON 1: STRATEGY (Algoritmos intercambiables de Ranking / Importancia)
// ============================================================================
class IRankingStrategy {
public:
    virtual ~IRankingStrategy() = default;
    virtual std::vector<SearchResult> rank(
        const std::unordered_map<int, std::pair<int, int>>& acc,
        const std::vector<Movie>& movies) const = 0;
};

class DefaultRankingStrategy : public IRankingStrategy {
public:
    std::vector<SearchResult> rank(
        const std::unordered_map<int, std::pair<int, int>>& acc,
        const std::vector<Movie>& movies) const override {
        constexpr int BONO_POR_TERMINO_EXTRA = 50;
        std::vector<SearchResult> results;
        results.reserve(acc.size());

        for (const auto& [movieId, scoreAndCount] : acc) {
            int importancia = scoreAndCount.first + BONO_POR_TERMINO_EXTRA * (scoreAndCount.second - 1);
            results.push_back({movieId, importancia, scoreAndCount.second});
        }

        std::sort(results.begin(), results.end(), [&movies](const SearchResult& a, const SearchResult& b) {
            if (a.matchedTerms != b.matchedTerms) return a.matchedTerms > b.matchedTerms;
            if (a.score != b.score) return a.score > b.score;
            return movies[a.movieId].releaseYear > movies[b.movieId].releaseYear;
        });
        return results;
    }
};

class RecencyRankingStrategy : public IRankingStrategy {
public:
    std::vector<SearchResult> rank(
        const std::unordered_map<int, std::pair<int, int>>& acc,
        const std::vector<Movie>& movies) const override {
        std::vector<SearchResult> results;
        results.reserve(acc.size());

        for (const auto& [movieId, scoreAndCount] : acc) {
            int yearBonus = (movies[movieId].releaseYear > 2000)
                            ? (movies[movieId].releaseYear - 2000) * 2
                            : 0;
            int importancia = scoreAndCount.first + yearBonus;
            results.push_back({movieId, importancia, scoreAndCount.second});
        }

        std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
            if (a.matchedTerms != b.matchedTerms) return a.matchedTerms > b.matchedTerms;
            return a.score > b.score;
        });
        return results;
    }
};

// ============================================================================
// PATRON 2: OBSERVER (Notificacion reactiva de cambios en el perfil)
// ============================================================================
class IProfileObserver {
public:
    virtual ~IProfileObserver() = default;
    virtual void onProfileUpdated() = 0;
};

// ============================================================================
// PATRON 3: FACTORY METHOD (Creacion de objetos de consulta segun filtro)
// ============================================================================
class ISearchQuery {
public:
    virtual ~ISearchQuery() = default;
    virtual std::vector<SearchResult> execute(const SearchEngine& engine, const std::string& query) const = 0;
};

class SearchQueryFactory {
public:
    enum class Type { Text = 0, Director = 1, Cast = 2, Genre = 3 };
    static std::unique_ptr<ISearchQuery> create(Type type);
};

// ============================================================================
// PATRON 4: SINGLETON (Gestor global unico del catalogo y motor de busqueda)
// ============================================================================
class StreamingAppSingleton {
public:
    static StreamingAppSingleton& getInstance();

    StreamingAppSingleton(const StreamingAppSingleton&) = delete;
    StreamingAppSingleton& operator=(const StreamingAppSingleton&) = delete;

    void initialize(std::vector<Movie>&& movies);
    const std::vector<Movie>& getMovies() const { return movies_; }
    SearchEngine& getEngine() { return *engine_; }

private:
    StreamingAppSingleton();
    ~StreamingAppSingleton();

    std::vector<Movie> movies_;
    std::unique_ptr<SearchEngine> engine_;
};