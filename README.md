# Programación III: Proyecto Final (2026-2) — Segunda Entrega

## Integrantes (5)
* Dylan van Oordt Arbulú
* Hector Sebastian Choque Dueñas
* Camila Araceli Alfaro Chuquino
* Piero Alejandro Ortega Capacute
* John Dayron Blas Huete

---
## Estructura del repositorio

---

## Plataforma de Streaming — Buscador y visualizador de sinopsis de películas

Aplicación de escritorio en **C++17** que carga una base de datos de películas desde un `.csv`, la indexa en paralelo mediante un **Trie de sufijos (`SuffixTrie`)** para búsqueda rápida por palabra, frase o sub-cadena (y por tag: director/actor/género), ordena los resultados mediante estrategias intercambiables de importancia, y ofrece una **interfaz gráfica (Dear ImGui + GLFW + OpenGL3)** para explorar sinopsis, gestionar **Likes** / **Ver más tarde** y recibir **recomendaciones** en tiempo real.

El detalle de diseño y justificación de la estructura de árbol se encuentra en [`docs/ARQUITECTURA.md`](docs/ARQUITECTURA.md), y el mapeo contra el enunciado en [`docs/RUBRICA.md`](docs/RUBRICA.md).

---

## Novedades de la Segunda Entrega

### 1. Estructura de Árbol (`SuffixTrie` + Índice Invertido)
* Implementado en `include/SuffixTrie.hpp` y `src/SuffixTrie.cpp`.
* Indexa todos los sufijos de cada palabra única del vocabulario (~138,600 palabras $\rightarrow$ ~858,000 nodos contiguos en un `std::vector<Node>`), permitiendo encontrar coincidencias por sub-cadena interna (ej. `"obber"` $\rightarrow$ `"robbery"`) en tiempo proporcional al largo de la consulta.
* Incluye deduplicación $O(1)$ amortizada mediante `lastWordId` para evitar entradas repetidas en palabras con caracteres recurrentes.

### 2. Patrones de Diseño Implementados (`include/DesignPatterns.hpp`)
1. **Singleton (`StreamingAppSingleton`)**: Garantiza una única instancia compartida en memoria del catálogo de películas y del motor de búsqueda (`SearchEngine`), evitando duplicar el `SuffixTrie` entre la lógica y la interfaz gráfica.
2. **Factory Method (`SearchQueryFactory`)**: Encapsula la instanciación de consultas (`FullTextQuery` y `TagQuery` para director, reparto o género), desacoplando la interfaz gráfica de la lógica de filtrado.
3. **Strategy (`IRankingStrategy`)**: Permite cambiar en tiempo de ejecución el algoritmo de ordenamiento de resultados entre **Ranking por Relevancia** (`DefaultRankingStrategy`) y **Ranking por Estrenos Recientes** (`RecencyRankingStrategy`).
4. **Observer (`IProfileObserver`)**: Conecta `UserProfile` con `GuiProfileObserver` para refrescar automáticamente las listas de *Likes*, *Ver más tarde* y *Recomendaciones* en pantalla cada vez que el usuario interactúa con una película.

### 3. Programación Paralela (`std::async` / `std::future`)
* **Indexación concurrente (`SearchEngine::buildIndexes`)**: Construye los 4 índices (`contentIndex_`, `directorIndex_`, `castIndex_`, `genreIndex_`) y sus respectivos `SuffixTrie` en 4 hilos simultáneos.
* **Búsqueda multi-término paralela (`SearchEngine::rankAndCombine`)**: Cada palabra de una consulta compuesta se busca en paralelo en el `SuffixTrie` mediante `std::future` independientes, combinando los mapas de puntaje sin bloqueos por `mutex`.
* **Recomendaciones por bloques (`UserProfile::recommendSimilarToLikes`)**: Divide el catálogo entre los núcleos disponibles (`std::thread::hardware_concurrency()`) para calcular la similitud de Jaccard en paralelo.

### 4. Interfaz Gráfica (`src/main_gui.cpp`)
* Construida con **Dear ImGui + GLFW + OpenGL3** (descargados automáticamente por CMake mediante `FetchContent`).
* Incluye selector de tipo de búsqueda, cambio de estrategia de ranking en vivo, control de **Zoom dinámico** (`Ctrl + Rueda del ratón` o deslizador `0.8x–1.8x`), paginación de 5 en 5 resultados y persistencia automática en `user_profile.txt`.

---

## Dataset

Se utilizó el dataset de Wikipedia Movie Plots (`wiki_movie_plots_deduped.csv`, ~34,886 películas válidas tras limpieza). Por su tamaño (~78 MB), el archivo completo no se incluye en el repositorio; descárgalo y colócalo en `data/wiki_movie_plots_deduped.csv`. Para pruebas rápidas sin descargar el dataset completo, el repositorio incluye `data/muestra_100_peliculas.csv`.

**Preprocesamiento aplicado (`CSVReader` + `TextUtils`):**
* Descarte de filas sin título.
* Normalización de `"unknown"` o vacíos en `Genre` y `Director`.
* Conversión a minúsculas, limpieza de diacríticos UTF-8 y tokenización alfanumérica.

---

## Cómo compilar y ejecutar

### Requisitos
* Compilador compatible con **C++17** (GCC/MinGW, Clang o MSVC) y **CMake 3.16+**.
* En Linux (Ubuntu/Debian), instalar dependencias gráficas si no están presentes:
  ```bash
  sudo apt-get install libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
  ```
### Compilación
  ```bash
  mkdir build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  cmake --build . -j4
  ```
### Ejecución
  ```bash
  # Ejecutar con el dataset completo (~34,886 películas):
  ./plataforma_peliculas ../data/wiki_movie_plots_deduped.csv
  # Ejecutar con el dataset de muestra (100 películas, arranque instantáneo):
  ./plataforma_peliculas ../data/muestra_100_peliculas.csv
  # Opcional: limitar el número de filas a cargar (ej. primeras 3,000 películas):
  ./plataforma_peliculas ../data/wiki_movie_plots_deduped.csv 3000
  ```
---

