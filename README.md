    for (PdfDocument.Bookmark b : tree) {

        Log.e(TAG, String.format("%s %s, p %d", sep, b.getTitle(), b.getPageIdx()));

        if (b.hasChildren()) {
            printBookmarksTree(b.getChildren(), sep + "-");
        }
    }
