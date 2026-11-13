#pragma once
#include <string>
#include <vector>
#include <memory>
#include <ostream>
#include "Movie.hpp"
#include "TextIndex.hpp"
#include "DesignPatterns.hpp"

class SearchEngine {
public:
    explicit SearchEngine(const std::vector<Movie>& movies);

    // Construccion paralela de los 4 indices
    void buildIndexes();

    // Permite cambiar el algoritmo de ranking en tiempo de ejecucion (Strategy)
    void setRankingStrategy(std::unique_ptr<IRankingStrategy> strategy);

    std::vector<SearchResult> searchText(const std::string& query) const;
    std::vector<SearchResult> searchTag(const std::string& query, TagField field) const;
    void printIndexStats(std::ostream& os) const;

private:
    const std::vector<Movie>& movies_;
    TextIndex contentIndex_;
    TextIndex directorIndex_;
    TextIndex castIndex_;
    TextIndex genreIndex_;
    std::unique_ptr<IRankingStrategy> rankingStrategy_;

    std::vector<SearchResult> rankAndCombine(const std::vector<std::string>& queryTokens,
                                              const TextIndex& index) const;
};