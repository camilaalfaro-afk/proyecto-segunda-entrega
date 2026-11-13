#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <algorithm>

#include "CSVReader.hpp"
#include "SearchEngine.hpp"
#include "UserProfile.hpp"
#include "DesignPatterns.hpp"

// Observer concreto que escucha cambios en UserProfile y refresca la GUI
class GuiProfileObserver : public IProfileObserver {
public:
    bool needsRefresh = true;
    void onProfileUpdated() override {
        needsRefresh = true;
    }
};

int main(int argc, char** argv) {
    std::string csvPath = (argc > 1) ? argv[1] : "../data/wiki_movie_plots_deduped.csv";
    size_t maxRows = (argc > 2) ? static_cast<size_t>(std::stoul(argv[2])) : 0;

    std::cout << "Cargando base de datos e indexando en paralelo...\n";
    auto movies = CSVReader::load(csvPath, maxRows);
    if (movies.empty()) {
        std::cerr << "Error: No se pudo abrir el dataset en " << csvPath << "\n";
        return 1;
    }

    // 1. Inicializar Singleton
    auto& app = StreamingAppSingleton::getInstance();
    app.initialize(std::move(movies));
    app.getEngine().printIndexStats(std::cout);

    // 2. Inicializar Perfil de Usuario y registrar Observer
    UserProfile user("user_profile.txt");
    GuiProfileObserver guiObserver;
    user.addObserver(&guiObserver);

    // 3. Inicializar GLFW + OpenGL + Dear ImGui
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(1366, 768, "Plataforma de Streaming - Suffix Trie C++", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Cargar fuente nitida del sistema a 20px (evita que se vea pixelado al escalar)
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 20.0f);
    if (!font) {
        io.Fonts->AddFontDefault();
        io.FontGlobalScale = 1.5f;
    }

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing  = ImVec2(10, 8);
    style.WindowRounding = 6.0f;
    style.FrameRounding  = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Variables de estado de la interfaz
    char searchBuffer[256] = "";
    int searchTypeIdx = 0; // 0: Texto, 1: Director, 2: Cast, 3: Genero
    int strategyIdx = 0;   // 0: Relevancia, 1: Estrenos Recientes
    float uiZoom = 1.0f;
    std::vector<SearchResult> currentResults;
    std::vector<int> recommendedIds;
    int selectedMovieId = -1;
    size_t page = 0;
    constexpr size_t PAGE_SIZE = 5;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Permitir hacer zoom con Ctrl + Rueda del raton
        if (io.KeyCtrl && io.MouseWheel != 0.0f) {
            uiZoom = std::clamp(uiZoom + io.MouseWheel * 0.1f, 0.8f, 2.0f);
            io.FontGlobalScale = uiZoom;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Reaccion automatica del Patron Observer al dar/quitar Like
        if (guiObserver.needsRefresh) {
            recommendedIds = user.recommendSimilarToLikes(app.getMovies(), 5);
            guiObserver.needsRefresh = false;
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("PlataformaStreaming", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // --- BARRA SUPERIOR ---
        ImGui::Text("BUSCADOR DE PELICULAS (Suffix Trie + Concurrencia + Patrones)");
        ImGui::SameLine(io.DisplaySize.x - 210 * uiZoom);
        ImGui::SetNextItemWidth(130 * uiZoom);
        if (ImGui::SliderFloat("Zoom", &uiZoom, 0.8f, 1.8f, "%.1fx")) {
            io.FontGlobalScale = uiZoom;
        }
        ImGui::Separator();

        const char* tipos[] = { "Titulo / Sinopsis", "Director", "Actor / Cast", "Genero" };
        ImGui::SetNextItemWidth(220 * uiZoom);
        ImGui::Combo("##Tipo", &searchTypeIdx, tipos, IM_ARRAYSIZE(tipos));
        ImGui::SameLine();

        ImGui::SetNextItemWidth(450 * uiZoom);
        bool enterPressed = ImGui::InputTextWithHint(
            "##Query", "Escribe palabra, frase o sub-cadena...",
            searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();

        if (ImGui::Button("Buscar") || enterPressed) {
            auto queryObj = SearchQueryFactory::create(static_cast<SearchQueryFactory::Type>(searchTypeIdx));
            currentResults = queryObj->execute(app.getEngine(), searchBuffer);
            page = 0;
            selectedMovieId = currentResults.empty() ? -1 : currentResults[0].movieId;
        }

        ImGui::SameLine();
        const char* estrategias[] = { "Ranking: Relevancia", "Ranking: Recientes" };
        ImGui::SetNextItemWidth(230 * uiZoom);
        if (ImGui::Combo("##Estrategia", &strategyIdx, estrategias, IM_ARRAYSIZE(estrategias))) {
            if (strategyIdx == 0) {
                app.getEngine().setRankingStrategy(std::make_unique<DefaultRankingStrategy>());
            } else {
                app.getEngine().setRankingStrategy(std::make_unique<RecencyRankingStrategy>());
            }
            if (std::strlen(searchBuffer) > 0) {
                auto queryObj = SearchQueryFactory::create(static_cast<SearchQueryFactory::Type>(searchTypeIdx));
                currentResults = queryObj->execute(app.getEngine(), searchBuffer);
                page = 0;
            }
        }

        ImGui::Separator();

        // --- PANEL IZQUIERDO ---
        float leftWidth = std::min(560.0f * uiZoom, io.DisplaySize.x * 0.45f);
        ImGui::BeginChild("LeftPane", ImVec2(leftWidth, 0), true);
        ImGui::Text("Resultados (%zu encontrados)", currentResults.size());
        ImGui::Separator();

        const auto& allMovies = app.getMovies();
        size_t start = page * PAGE_SIZE;
        size_t end = std::min(start + PAGE_SIZE, currentResults.size());

        for (size_t i = start; i < end; ++i) {
            const Movie& m = allMovies[currentResults[i].movieId];
            std::string label = m.title + " (" + std::to_string(m.releaseYear) + ")##res_" + std::to_string(m.id);
            if (ImGui::Selectable(label.c_str(), selectedMovieId == m.id)) {
                selectedMovieId = m.id;
            }
            ImGui::TextDisabled("  Score: %d | Dir: %s",
                                currentResults[i].score,
                                m.director.empty() ? "N/A" : m.director.c_str());
        }

        if (!currentResults.empty()) {
            ImGui::Spacing();
            if (page > 0 && ImGui::Button("< Anterior")) page--;
            ImGui::SameLine();
            if (end < currentResults.size() && ImGui::Button("Ver 5 mas >")) page++;
            ImGui::SameLine();
            ImGui::Text("Pagina %zu", page + 1);
        }

        // Lista: Ver mas tarde
        ImGui::Spacing(); ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "VER MAS TARDE (%zu)", user.watchLaterMovies().size());
        for (int id : user.watchLaterMovies()) {
            if (id >= 0 && id < static_cast<int>(allMovies.size())) {
                std::string label = allMovies[id].title + "##wl_" + std::to_string(id);
                if (ImGui::Selectable(label.c_str(), selectedMovieId == id)) {
                    selectedMovieId = id;
                }
            }
        }

        // Lista: Likes
        ImGui::Spacing(); ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "MIS LIKES (%zu)", user.likedMovies().size());
        for (int id : user.likedMovies()) {
            if (id >= 0 && id < static_cast<int>(allMovies.size())) {
                std::string label = allMovies[id].title + "##lk_" + std::to_string(id);
                if (ImGui::Selectable(label.c_str(), selectedMovieId == id)) {
                    selectedMovieId = id;
                }
            }
        }

        // Lista: Recomendaciones
        ImGui::Spacing(); ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "RECOMENDADAS POR TUS LIKES");
        for (int id : recommendedIds) {
            if (id >= 0 && id < static_cast<int>(allMovies.size())) {
                std::string label = allMovies[id].title + "##rec_" + std::to_string(id);
                if (ImGui::Selectable(label.c_str(), selectedMovieId == id)) {
                    selectedMovieId = id;
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // --- PANEL DERECHO: DETALLE DE LA PELICULA ---
        ImGui::BeginChild("RightPane", ImVec2(0, 0), true);
        if (selectedMovieId >= 0 && selectedMovieId < static_cast<int>(allMovies.size())) {
            const Movie& m = allMovies[selectedMovieId];
            ImGui::TextWrapped("%s (%d)", m.title.c_str(), m.releaseYear);
            ImGui::Separator();
            ImGui::Text("Director: %s", m.director.empty() ? "Desconocido" : m.director.c_str());
            ImGui::Text("Genero:   %s", m.genre.empty() ? "Desconocido" : m.genre.c_str());
            ImGui::TextWrapped("Reparto:  %s", m.cast.empty() ? "Desconocido" : m.cast.c_str());
            ImGui::Spacing();

            bool liked = user.isLiked(m.id);
            if (ImGui::Button(liked ? "Quitar Like" : "Dar Like")) {
                if (liked) user.unlike(m.id);
                else user.like(m.id);
            }
            ImGui::SameLine();
            bool wl = user.isInWatchLater(m.id);
            if (ImGui::Button(wl ? "Quitar de Ver mas tarde" : "+ Ver mas tarde")) {
                if (wl) user.removeFromWatchLater(m.id);
                else user.addToWatchLater(m.id);
            }

            ImGui::Spacing(); ImGui::Separator();
            ImGui::Text("SINOPSIS:");
            ImGui::TextWrapped("%s", m.plot.empty() ? "(No disponible)" : m.plot.c_str());
        } else {
            ImGui::TextDisabled("Selecciona una pelicula de la lista izquierda para ver su sinopsis completa.");
        }
        ImGui::EndChild();

        ImGui::End();
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    user.removeObserver(&guiObserver);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}