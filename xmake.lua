add_requires("sfml 3.0.0", "sqlitecpp 3.3.*")

set_languages("c++23")

target("gomoku")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("sfml", "sqlitecpp")
    set_configdir("$(buildir)/$(plat)/$(arch)/$(mode)")
    add_configfiles("assets/audio/*.wav", {prefixdir = "assets/audio", onlycopy = true})
    add_configfiles("assets/img/*.png", {prefixdir = "assets/img", onlycopy = true})
    add_configfiles("assets/level/*.xsb", {prefixdir = "assets/level", onlycopy = true})
