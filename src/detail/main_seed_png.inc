        // Seed a sample PNG wallpaper so Settings can complete/load non-BMP paths.
        if (!monolithFs.exists("/Wallpapers/sample.png")) {
            bool seededPng = false;
            const char* pngCandidates[] = {
                "assets/wallpapers/sample.png",
                "../assets/wallpapers/sample.png",
                nullptr
            };
            for (int i = 0; pngCandidates[i]; ++i) {
                std::ifstream in(pngCandidates[i], std::ios::binary);
                if (!in) continue;
                std::string bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
                if (!bytes.empty() && monolithFs.writeFile("/Wallpapers/sample.png", bytes)) {
                    std::cout << "Seeded /Wallpapers/sample.png from " << pngCandidates[i] << std::endl;
                    seededPng = true;
                    break;
                }
            }
            if (!seededPng) {
                std::cout << "No sample.png asset found to seed /Wallpapers/sample.png" << std::endl;
            }
        }
