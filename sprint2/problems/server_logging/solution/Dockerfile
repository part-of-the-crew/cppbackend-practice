# --- Сборочный образ ---
FROM ubuntu:22.04 as build

# Установка зависимостей сборки
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
      build-essential \
      curl \
    && \
    pip3 install conan==1.59.0

# Настройка профиля Conan (важно для корректного обнаружения ABI)
RUN conan profile new default --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

COPY conanfile.txt /app/
WORKDIR /app/build

# Принудительно пересобираем boost внутри контейнера с нужным ABI
RUN conan install .. --build=boost -s build_type=Release

# Копируем исходники
COPY ./src /app/src
COPY CMakeLists.txt /app/

# Сборка проекта
RUN cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j $(nproc)

# --- Финальный образ ---
FROM ubuntu:22.04 as run

RUN apt update && apt install -y libstdc++6 && rm -rf /var/lib/apt/lists/*

# Создаем пользователя
RUN groupadd -r www && useradd -r -g www www
USER www
WORKDIR /app

# Копируем бинарный файл
COPY --from=build /app/build/bin/game_server /app/
# Если вы используете динамические библиотеки (shared=True), нужно скопировать и их!
# Но проще использовать статические библиотеки в Conan (см. ниже)

COPY ./data /app/data

ENTRYPOINT ["/app/game_server", "/app/data/config.json"]