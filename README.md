# Pdfium Android library

Original Android pdfium has few rendering issues.

# build natives

Natives shall be build manually. You need Ubuntu (see supported platforms in `./build/install-build-deps-android.sh`). Read more about setup OS and build scripts from links below. Use `./build.sh` and `build.diff`

## Links

  * https://pdfium.googlesource.com/pdfium/
  * https://chromium.googlesource.com/chromium/src/+/master/docs/android_build_instructions.md
  * https://github.com/pvginkel/PdfiumViewer/wiki/Building-PDFium
  * https://github.com/barteksc/PdfiumAndroid

Build tweaks:

  * library renamed from `libpdfium.so` to `libmodpdfium.so` because API21 && API22 failed to lookup symbols due to conflict with `/system/lib/libpdfium.so`.

Original library:

  * https://github.com/barteksc/PdfiumAndroid

## Example

``` java
    ParcelFileDescriptor fd = ...;
    int pageNum = 0;
    Pdfium pdfium = new Pdfium();
    pdfium.open(fd);
    Pdfium.Page page = pdfium.getPage(pageNum);

    Pdfium.Size size = pdfium.getPageSize(pageNum);

    // ARGB_8888 - best quality, high memory usage, higher possibility of OutOfMemoryError
    // RGB_565 - little worse quality, twice less memory usage
    Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565);
    
    pdfiumCore.render(pdfDocument, bitmap, pageNum, 0, 0, width, height);

    Log.e(TAG, "title = " + pdfium.getMeta(Pdfium.META_TITLE));
    Log.e(TAG, "author = " + pdfium.getMeta(Pdfium.META_AUTHOR));

    for (Pdfium.Bookmark b : pdfium.getTOC()) {
        Log.e(TAG, String.format("- %s, p %d", sep, b.title, b.page));
    }

    p.close();
    
    pdfium.close();
```

## Reading links

Version 1.8.0 introduces `PdfiumCore#getPageLinks(PdfDocument, int)` method, which allows to get list
of links from given page. Links are returned as `List` of type `PdfDocument.Link`.
`PdfDocument.Link` holds destination page (may be null), action URI (may be null or empty)
and link bounds in document page coordinates. To map page coordinates to screen coordinates you may use
`PdfiumCore#mapRectToDevice(...)`. See `PdfiumCore#mapPageCoordsToDevice(...)` for parameters description.

Sample usage:
``` java
PdfiumCore core = ...;
PdfDocument document = ...;
int pageIndex = 0;
core.openPage(document, pageIndex);
List<PdfDocument.Link> links = core.getPageLinks(document, pageIndex);
for (PdfDocument.Link link : links) {
    RectF mappedRect = core.mapRectToDevice(document, pageIndex, ..., link.getBounds())

    if (clickedArea(mappedRect)) {
        String uri = link.getUri();
        if (link.getDestPageIdx() != null) {
            // jump to page
        } else if (uri != null && !uri.isEmpty()) {
            // open URI using Intent
        }
    }
}

```
