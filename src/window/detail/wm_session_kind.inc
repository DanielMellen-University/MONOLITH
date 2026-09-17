                if (!w.editedFilePath.empty()) {
                    kind = "editor";
                    path = w.editedFilePath;
                } else if (!w.drawingFilePath.empty()) {
                    kind = "drawing";
                    path = w.drawingFilePath;
                } else if (const AppRegistryEntry* reg = findAppByBaseTitle(w.appBaseTitle);
                           reg && reg->sessionKey) {
                    kind = reg->sessionKey;
                } else {
                    continue; // unknown / placeholder windows are not restored
                }
