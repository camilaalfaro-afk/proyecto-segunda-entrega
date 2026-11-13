#include "SearchEngine.hpp"
#include "TextUtils.hpp"
#include <algorithm>
#include <iostream>
#include <future>

namespace {
    constexpr int PESO_TITULO = 5;
    constexpr int PESO_PLOT   = 1;
    constexpr int PESO_TAG    = 4;

    class FullTextQuery : public ISearchQuery {
    public:
        std::vector<SearchResult> execute(const SearchEngine& engine, const std::string& query) const override {
            return engine.searchText(query);
        }
    };

    class TagQuery : public ISearchQuery {
        TagField field_;
    public:
        explicit TagQuery(TagField field) : field_(field) {}
        std::vector<SearchResult> execute(const SearchEngine& engine, const std::string& query) const override {
            return engine.searchTag(query, field_);
        }
    };
}

// --- Implementacion Factory Method ---
std::unique_ptr<ISearchQuery> SearchQueryFactory::create(Type type) {
    switch (type) {
        case Type::Text:     return std::make_unique<FullTextQuery>();
        case Type::Director: return std::make_unique<TagQuery>(TagField::Director);
        case Type::Cast:     return std::make_unique<TagQuery>(TagField::Cast);
        case Type::Genre:    return std::make_unique<TagQuery>(TagField::Genre);
    }
    return std::make_unique<FullTextQuery>();
}

// --- Implementacion Singleton ---
StreamingAppSingleton::StreamingAppSingleton() = default;
StreamingAppSingleton::~StreamingAppSingleton() = default;

StreamingAppSingleton& StreamingAppSingleton::getInstance() {
    static StreamingAppSingleton instance;
    return instance;
}

void StreamingAppSingleton::initialize(std::vector<Movie>&& movies) {
    movies_ = std::move(movies);
    engine_ = std::make_unique<SearchEngine>(movies_);
    engine_->buildIndexes();
}

// --- Implementacion SearchEngine ---
SearchEngine::SearchEngine(const std::vector<Movie>& movies)
    : movies_(movies),
      rankingStrategy_(std::make_unique<DefaultRankingStrategy>()) {}

void SearchEngine::setRankingStrategy(std::unique_ptr<IRankingStrategy> strategy) {
    if (strategy) {
        rankingStrategy_ = std::move(strategy);
    }
}

void SearchEngine::buildIndexes() {
    // Programacion paralela: los 4 indices y sus SuffixTries se construyen
    // concurrentemente en hilos separados usando std::async.
    auto fContent = std::async(std::launch::async, [this]() {
        for (const auto& m : movies_) {
            contentIndex_.indexTokens(m.id, TextUtils::tokenize(m.title), PESO_TITULO);
            contentIndex_.indexTokens(m.id, TextUtils::tokenize(m.plot),  PESO_PLOT);
        }
        contentIndex_.build();
    });

    auto fDirector = std::async(std::launch::async, [this]() {
        for (const auto& m : movies_) {
            if (!m.director.empty())
                directorIndex_.indexTokens(m.id, TextUtils::tokenize(m.director), PESO_TAG);
        }
        directorIndex_.build();
    });

    auto fCast = std::async(std::launch::async, [this]() {
        for (const auto& m : movies_) {
            if (!m.castTokens.empty())
                castIndex_.indexTokens(m.id, m.castTokens, PESO_TAG);
        }
        castIndex_.build();
    });

    auto fGenre = std::async(std::launch::async, [this]() {
        for (const auto& m : movies_) {
            if (!m.genreTokens.empty())
                genreIndex_.indexTokens(m.id, m.genreTokens, PESO_TAG);
        }
        genreIndex_.build();
    });

    fContent.get();
    fDirector.get();
    fCast.get();
    fGenre.get();
}

std::vector<SearchResult> SearchEngine::rankAndCombine(const std::vector<std::string>& queryTokens,
                                                        const TextIndex& index) const {
    if (queryTokens.empty()) return {};

    // Programacion paralela: cada palabra de la consulta se busca en el
    // SuffixTrie en paralelo y luego se combinan los mapas de puntaje.
    std::vector<std::future<TextIndex::ScoreMap>> futures;
    futures.reserve(queryTokens.size());

    for (const auto& term : queryTokens) {
        futures.push_back(std::async(std::launch::async, [&index, &term]() {
            return index.searchSubstring(term);
        }));
    }

    std::unordered_map<int, std::pair<int, int>> acc;
    for (auto& fut : futures) {
        auto hits = fut.get();
        for (const auto& [movieId, weight] : hits) {
            auto& entry = acc[movieId];
            entry.first  += weight;
            entry.second += 1;
        }
    }

    // Delegar el ordenamiento al patron Strategy activo
    return rankingStrategy_->rank(acc, movies_);
}

std::vector<SearchResult> SearchEngine::searchText(const std::string& query) const {
    return rankAndCombine(TextUtils::tokenize(query), contentIndex_);
}

std::vector<SearchResult> SearchEngine::searchTag(const std::string& query, TagField field) const {
    const TextIndex* idx = nullptr;
    switch (field) {
        case TagField::Director: idx = &directorIndex_; break;
        case TagField::Cast:     idx = &castIndex_;      break;
        case TagField::Genre:    idx = &genreIndex_;     break;
    }
    return rankAndCombine(TextUtils::tokenize(query), *idx);
}

void SearchEngine::printIndexStats(std::ostream& os) const {
    os << "Peliculas indexadas: " << movies_.size() << "\n";
    os << "Vocabulario titulo+sinopsis: " << contentIndex_.vocabularySize()
       << " palabras, " << contentIndex_.trieNodeCount() << " nodos en el Trie de sufijos\n";
    os << "Vocabulario director: " << directorIndex_.vocabularySize() << " palabras\n";
    os << "Vocabulario cast: " << castIndex_.vocabularySize() << " palabras\n";
    os << "Vocabulario genero: " << genreIndex_.vocabularySize() << " palabras\n";
}