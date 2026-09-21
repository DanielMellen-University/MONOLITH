    std::string getWallpaperFit() const override {
        return wm ? wm->getWallpaperFit() : std::string{"cover"};
    }

    void setWallpaperFit(const std::string& fit) override {
        if (wm) {
            wm->setWallpaperFit(fit);
        }
    }
