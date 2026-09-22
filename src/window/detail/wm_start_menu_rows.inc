const auto entries = filterStartMenuRows(buildStartMenuRows(), m_startMenuFilter);
    const size_t entryCount = entries.size();
    const int itemHLogical = 26;
    const int itemGapLogical = 2;
    const int categoryHLogical = 20;
    const int separatorHLogical = 10;
    const int bottomPadLogical = 8;
    const int topPadLogical = 6;
    const int contentHLogical = startMenuRowsContentHeightLogical(
        entries, topPadLogical, itemHLogical, itemGapLogical,
        categoryHLogical, separatorHLogical, bottomPadLogical);
