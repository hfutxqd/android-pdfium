package com.github.axet.pdfium;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;

import java.io.FileDescriptor;
import java.io.IOException;

public class Pdfium {
    private static final String TAG = Pdfium.class.getName();

    public static final int FPDF_MATCHCASE = 0x00000001;  // If not set, it will not match case by default.
    public static final int FPDF_MATCHWHOLEWORD = 0x00000002; // If not set, it will not match the whole word by default.

    public static final String META_TITLE = "Title";
    public static final String META_AUTHOR = "Author";
    public static final String META_SUBJECT = "Subject";
    public static final String META_KEYWORDS = "Keywords";
    public static final String META_CREATOR = "Creator";
    public static final String META_PRODUCER = "Producer";
    public static final String META_CREATIONDATE = "CreationDate";
    public static final String META_MODDATE = "ModDate";

    public static final int FPDF_ANNOT = 0x01; // Set if annotations are to be rendered.
    public static final int FPDF_LCD_TEXT = 0x02; // Set if using text rendering optimized for LCD display.
    public static final int FPDF_NO_NATIVETEXT = 0x04; // Don't use the native text output available on some platforms
    public static final int FPDF_GRAYSCALE = 0x08; // Grayscale output.
    public static final int FPDF_DEBUG_INFO = 0x80; // Set if you want to get some debug info.
    public static final int FPDF_NO_CATCH = 0x100; // Set if you don't want to catch exceptions.
    public static final int FPDF_RENDER_LIMITEDIMAGECACHE = 0x200; // Limit image cache size.
    public static final int FPDF_RENDER_FORCEHALFTONE = 0x400; // Always use halftone for image stretching.
    public static final int FPDF_PRINTING = 0x800; // Render for printing.
    public static final int FPDF_RENDER_NO_SMOOTHTEXT = 0x1000; // Set to disable anti-aliasing on text.
    public static final int FPDF_RENDER_NO_SMOOTHIMAGE = 0x2000; // Set to disable anti-aliasing on images.
    public static final int FPDF_RENDER_NO_SMOOTHPATH = 0x4000; // Set to disable anti-aliasing on paths.

    private long handle;

    static {
        if (Config.natives) {
            System.loadLibrary("modpdfium");
            System.loadLibrary("pdfiumjni");
        }
    }

    public static native void FPDF_InitLibrary();

    public static native void FPDF_DestroyLibrary();

    public class Page {
        private long handle;

        public native Text open();

        /**
         * Render page fragment on {@link Bitmap}.<br>
         * Page must be opened before rendering.
         * <p>
         * Supported bitmap configurations:
         * <ul>
         * <li>ARGB_8888 - best quality, high memory usage, higher possibility of OutOfMemoryError
         * <li>RGB_565 - little worse quality, twice less memory usage
         * </ul>
         */
        public void render(Bitmap bitmap, int startX, int startY, int drawSizeX, int drawSizeY) {
            render(bitmap, startX, startY, drawSizeX, drawSizeY, 0);
        }

        /**
         * Render page fragment on {@link Bitmap}. This method allows to set render flags.
         */
        public native void render(Bitmap bitmap, int startX, int startY, int drawSizeX, int drawSizeY, int flags);

        /**
         * Get all links from given page
         */
        public native Link[] getLinks();

        public Rect toDevice(int startX, int startY, int sizeX, int sizeY, int rotate, Rect rect) {
            Point leftTop = toDevice(startX, startY, sizeX, sizeY, rotate, rect.left, rect.top);
            Point rightBottom = toDevice(startX, startY, sizeX, sizeY, rotate, rect.right, rect.bottom);
            return new Rect(leftTop.x, leftTop.y, rightBottom.x, rightBottom.y);
        }

        /**
         * Map page coordinates to device screen coordinates
         *
         * @param startX left pixel position of the display area in device coordinates
         * @param startY top pixel position of the display area in device coordinates
         * @param sizeX  horizontal size (in pixels) for displaying the page
         * @param sizeY  vertical size (in pixels) for displaying the page
         * @param rotate page orientation: 0 (normal), 1 (rotated 90 degrees clockwise),
         *               2 (rotated 180 degrees), 3 (rotated 90 degrees counter-clockwise)
         * @param pageX  X value in page coordinates
         * @param pageY  Y value in page coordinate
         * @return mapped coordinates
         */
        public native Point toDevice(int startX, int startY, int sizeX, int sizeY, int rotate, double pageX, double pageY);

        public Rect toPage(int startX, int startY, int sizeX, int sizeY, int rotate, Rect rect) {
            Point leftTop = toPage(startX, startY, sizeX, sizeY, rotate, rect.left, rect.top);
            Point rightBottom = toPage(startX, startY, sizeX, sizeY, rotate, rect.right, rect.bottom);
            return new Rect(leftTop.x, leftTop.y, rightBottom.x, rightBottom.y);
        }

        public native Point toPage(int startX, int startY, int sizeX, int sizeY, int rotate, int deviceX, int deviceY);

        public native void close();
    }

    public static class Bookmark {
        public String title;
        public int page;
        public int level;

        public Bookmark(String t, int i, int l) {
            title = t;
            page = i;
            level = l;
        }
    }

    public static class Link {
        public String uri;
        public int index;
        public Rect bounds;

        public Link(String uri, int index, Rect bounds) {
            this.bounds = bounds;
            this.index = index;
            this.uri = uri;
        }
    }

    public static class Text {
        private long handle;

        public native int getCount();

        public native int getIndex(int x, int y);

        public native String getText(int start, int count);

        public native Rect[] getBounds(int start, int count);

        public native Search search(String str, int flags, int index);

        public native void close();
    }

    public static class TextResult {
        public int start;
        public int count;

        public TextResult(int s, int c) {
            start = s;
            count = c;
        }
    }

    public static class Search {
        private long handle;

        public native TextResult result();

        public native boolean next();

        public native boolean prev();

        public native void close();
    }

    public static class Size {
        public int width;
        public int height;

        public Size(int width, int height) {
            this.width = width;
            this.height = height;
        }

        public int getWidth() {
            return width;
        }

        public int getHeight() {
            return height;
        }

        @Override
        public boolean equals(final Object obj) {
            if (obj == null) {
                return false;
            }
            if (this == obj) {
                return true;
            }
            if (obj instanceof Size) {
                Size other = (Size) obj;
                return width == other.width && height == other.height;
            }
            return false;
        }

        @Override
        public String toString() {
            return width + "x" + height;
        }

        @Override
        public int hashCode() { // assuming most sizes are <2^16, doing a rotate will give us perfect hashing
            return height ^ ((width << (Integer.SIZE / 2)) | (width >>> (Integer.SIZE / 2)));
        }
    }

    public static class PdfPasswordException extends IOException {
        public PdfPasswordException() {
            super();
        }

        public PdfPasswordException(String detailMessage) {
            super(detailMessage);
        }
    }

    public Pdfium() {
    }

    /**
     * Create new document from file
     */
    public void open(FileDescriptor fd) {
        open(fd, null);
    }

    /**
     * Create new document from file with password
     */
    public native void open(FileDescriptor fd, String password);

    /**
     * Get total numer of pages in document
     */
    public native int getPagesCount();

    public native Size getPageSize(int pageIndex);

    /**
     * Open page
     */
    public native Page openPage(int pageIndex);

    /**
     * Release native resources and opened file
     */
    public native void close();

    /**
     * Get metadata for given document
     */
    public native String getMeta(String s);

    /**
     * Get table of contents (bookmarks) for given document
     */
    public native Bookmark[] getTOC();

    /**
     * The PDF file version. File version: 14 for 1.4, 15 for 1.5, ...
     */
    public native int getVersion();
}
