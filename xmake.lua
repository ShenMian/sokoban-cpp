add_requires("sfml 3.0.0", "sqlitecpp 3.3.*")

set_languages("c++23")

target("gomoku")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("sfml", "sqlitecpp")
