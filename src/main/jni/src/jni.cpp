#include "util.hpp"

extern "C" {
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
}

#include <android/bitmap.h>
#include <utils/Mutex.h>

using namespace android;

#include <fpdfview.h>
#include <fpdf_doc.h>
#include <fpdf_text.h>
#include <string>
#include <vector>

static Mutex sLibraryLock;

static int sLibraryReferenceCount = 0;

static void initLibraryIfNeed() {
    Mutex::Autolock lock(sLibraryLock);
    if (sLibraryReferenceCount == 0) {
        LOGD("Init FPDF library");
        FPDF_InitLibrary();
    }
    sLibraryReferenceCount++;
}

static void destroyLibraryIfNeed() {
    Mutex::Autolock lock(sLibraryLock);
    sLibraryReferenceCount--;
    if (sLibraryReferenceCount == 0) {
        LOGD("Destroy FPDF library");
        FPDF_DestroyLibrary();
    }
}

struct rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

inline long getFileSize(int fd) {
    struct stat file_state;

    if (fstat(fd, &file_state) >= 0) {
        return (long) (file_state.st_size);
    } else {
        LOGE("Error getting file size");
        return 0;
    }
}

int getFD(JNIEnv *env, jobject pfd) {
    jclass cls = env->GetObjectClass(pfd);
    jfieldID fid = env->GetFieldID(cls, "descriptor", "I");
    return env->GetIntField(pfd, fid);
}

FPDF_WIDESTRING GetStringUTF16LEChars(JNIEnv *env, jstring str) {
    jclass stringCls = env->GetObjectClass(str);
    jmethodID stringGetBytes = env->GetMethodID(stringCls, "getBytes", "(Ljava/lang/String;)[B");
    const jstring charsetName = env->NewStringUTF("UTF-16LE");
    const jbyteArray stringBytes = (jbyteArray) env->CallObjectMethod(str, stringGetBytes,
                                                                      charsetName);
    env->DeleteLocalRef(charsetName);
    jsize length = env->GetArrayLength(stringBytes);
    jbyte *s = env->GetByteArrayElements(stringBytes, NULL);
    jbyte *ss = (jbyte *) malloc(length + 2);
    ss[length] = 0;
    ss[length + 1] = 0;
    memcpy((void *) ss, s, length);
    env->ReleaseByteArrayElements(stringBytes, s, JNI_ABORT);
    env->DeleteLocalRef(stringBytes);
    return (FPDF_WIDESTRING) ss;
}

void ReleaseStringUTF16LEChars(FPDF_WIDESTRING ss) {
    free((void *) ss);
}

jstring NewStringUTF16LE(JNIEnv *env, const jbyte *buf, int len) {
    jclass stringCls = env->FindClass("java/lang/String");
    jmethodID constructorID = env->GetMethodID(stringCls, "<init>", "([BLjava/lang/String;)V");
    const jstring charsetName = env->NewStringUTF("UTF-16LE");
    jbyteArray ba = env->NewByteArray(len);
    env->SetByteArrayRegion(ba, 0, len, buf);
    const jstring str = (jstring) env->NewObject(stringCls, constructorID, ba,
                                                 charsetName);
    env->DeleteLocalRef(charsetName);
    return str;
}

static char *getErrorDescription(const long error) {
    char *description = NULL;
    switch (error) {
        case FPDF_ERR_SUCCESS:
            asprintf(&description, "No error.");
            break;
        case FPDF_ERR_FILE:
            asprintf(&description, "File not found or could not be opened.");
            break;
        case FPDF_ERR_FORMAT:
            asprintf(&description, "File not in PDF format or corrupted.");
            break;
        case FPDF_ERR_PASSWORD:
            asprintf(&description, "Incorrect password.");
            break;
        case FPDF_ERR_SECURITY:
            asprintf(&description, "Unsupported security scheme.");
            break;
        case FPDF_ERR_PAGE:
            asprintf(&description, "Page not found or content error.");
            break;
        default:
            asprintf(&description, "Unknown error.");
    }

    return description;
}

int jniThrowException(JNIEnv *env, const char *className, const char *message) {
    jclass exClass = env->FindClass(className);
    if (exClass == NULL) {
        LOGE("Unable to find exception class %s", className);
        return -1;
    }

    if (env->ThrowNew(exClass, message) != JNI_OK) {
        LOGE("Failed throwing '%s' '%s'", className, message);
        return -1;
    }

    return 0;
}

int jniThrowExceptionFmt(JNIEnv *env, const char *className, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msgBuf[512];
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    return jniThrowException(env, className, msgBuf);
    va_end(args);
}

uint16_t rgb_to_565(unsigned char R8, unsigned char G8, unsigned char B8) {
    unsigned char R5 = (R8 * 249 + 1014) >> 11;
    unsigned char G6 = (G8 * 253 + 505) >> 10;
    unsigned char B5 = (B8 * 249 + 1014) >> 11;
    return (R5 << 11) | (G6 << 5) | (B5);
}

void rgbBitmapTo565(void *source, int sourceStride, void *dest, AndroidBitmapInfo *info) {
    rgb *srcLine;
    uint16_t *dstLine;
    int y, x;
    for (y = 0; y < info->height; y++) {
        srcLine = (rgb *) source;
        dstLine = (uint16_t *) dest;
        for (x = 0; x < info->width; x++) {
            rgb *r = &srcLine[x];
            dstLine[x] = rgb_to_565(r->red, r->green, r->blue);
        }
        source = (char *) source + sourceStride;
        dest = (char *) dest + info->stride;
    }
}

extern "C" { //For JNI support

JNI_FUNC(void, Pdfium, FPDF_1InitLibrary)(JNIEnv *env, jclass cls) {
    if (sLibraryReferenceCount == 0)
        initLibraryIfNeed();
}

JNI_FUNC(void, Pdfium, FPDF_1DestroyLibrary)(JNIEnv *env, jclass cls) {
    if (sLibraryReferenceCount > 0)
        destroyLibraryIfNeed();
}

static int getBlock(void *param, unsigned long position, unsigned char *outBuffer,
                    unsigned long size) {
    const int fd = reinterpret_cast<intptr_t>(param);
    const int readCount = pread(fd, outBuffer, size, position);
    if (readCount < 0) {
        LOGE("Cannot read from file descriptor. Error:%d", errno);
        return 0;
    }
    return 1;
}

JNI_FUNC(void, Pdfium, open)(JNI_ARGS, jobject pfd, jstring password) {
    if (sLibraryReferenceCount == 0)
        initLibraryIfNeed();

    int fd = getFD(env, pfd);

    size_t fileLength = (size_t) getFileSize(fd);
    if (fileLength <= 0) {
        jniThrowException(env, "java/io/IOException",
                          "File is empty");
        return;
    }

    FPDF_FILEACCESS loader;
    loader.m_FileLen = fileLength;
    loader.m_Param = reinterpret_cast<void *>(intptr_t(fd));
    loader.m_GetBlock = &getBlock;

    const char *cpassword = NULL;
    if (password != NULL) {
        cpassword = env->GetStringUTFChars(password, NULL);
    }

    FPDF_DOCUMENT document = FPDF_LoadCustomDocument(&loader, cpassword);

    if (cpassword != NULL) {
        env->ReleaseStringUTFChars(password, cpassword);
    }

    if (!document) {
        const long errorNum = FPDF_GetLastError();
        if (errorNum == FPDF_ERR_PASSWORD) {
            jniThrowException(env, "com/github/axet/pdfium/Pdfium$PdfPasswordException",
                              "Password required or incorrect password.");
        } else {
            char *error = getErrorDescription(errorNum);
            jniThrowExceptionFmt(env, "java/io/IOException",
                                 "cannot create document: %s", error);

            free(error);
        }

        return;
    }

    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    env->SetLongField(thiz, fid, (jlong) document);
}

JNI_FUNC(void, Pdfium, close)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_DOCUMENT doc = (FPDF_DOCUMENT) env->GetLongField(thiz, fid);
    if (doc != NULL) {
        FPDF_CloseDocument(doc);
    }
    env->SetLongField(thiz, fid, (jlong) 0);
}

JNI_FUNC(jint, Pdfium, getPagesCount)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_DOCUMENT doc = (FPDF_DOCUMENT) env->GetLongField(thiz, fid);
    return (jint) FPDF_GetPageCount(doc);
}


JNI_FUNC(jstring, Pdfium, getMeta)(JNI_ARGS, jstring str) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_DOCUMENT doc = (FPDF_DOCUMENT) env->GetLongField(thiz, fid);

    const char *ctag = env->GetStringUTFChars(str, NULL);

    size_t bufferLen = FPDF_GetMetaText(doc, ctag, NULL, 0);
    if (bufferLen > 0) {
        jbyte *msg = (jbyte *) malloc(bufferLen);
        FPDF_GetMetaText(doc, ctag, msg, bufferLen);
        env->ReleaseStringUTFChars(str, ctag);
        jstring s = NewStringUTF16LE(env, msg, bufferLen - 2);
        free(msg);
        return s;
    }

    return NULL;
}

typedef struct {
    FPDF_BOOKMARK bm;
    int level;
} BOOKMARK;

void loadTOC(JNIEnv *env, std::vector<BOOKMARK> &list, FPDF_DOCUMENT doc, FPDF_BOOKMARK bookmark,
             int level) {
    while (bookmark != NULL) {
        BOOKMARK bm = {
                bookmark,
                level
        };
        list.push_back(bm);
        FPDF_BOOKMARK sub = FPDFBookmark_GetFirstChild(doc, bookmark);
        if (sub != 0)
            loadTOC(env, list, doc, sub, level + 1);
        bookmark = FPDFBookmark_GetNextSibling(doc, bookmark);
    }
}

JNI_FUNC(jobjectArray, Pdfium, getTOC)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_DOCUMENT doc = (FPDF_DOCUMENT) env->GetLongField(thiz, fid);

    std::vector<BOOKMARK> list;
    jclass bookmarkCls = env->FindClass("com/github/axet/pdfium/Pdfium$Bookmark");
    jmethodID constructorID = env->GetMethodID(bookmarkCls, "<init>", "(Ljava/lang/String;II)V");
    FPDF_BOOKMARK bookmark = FPDFBookmark_GetFirstChild(doc, NULL);
    loadTOC(env, list, doc, bookmark, 0);
    jobjectArray ar = env->NewObjectArray(list.size(), bookmarkCls, 0);
    for (int i = 0; i < list.size(); i++) {
        BOOKMARK bm = list[i];

        jstring s = 0;
        size_t bufferLen = FPDFBookmark_GetTitle(bm.bm, NULL, 0);
        if (bufferLen > 0) {
            jbyte *msg = (jbyte *) malloc(bufferLen);
            FPDFBookmark_GetTitle(bm.bm, msg, bufferLen);
            s = NewStringUTF16LE(env, msg, bufferLen - 2);
            free(msg);
        }

        int page = -1;
        FPDF_DEST dest = FPDFBookmark_GetDest(doc, bm.bm);
        if (dest != 0) {
            page = (int) FPDFDest_GetPageIndex(doc, dest);
        }

        jobject o = env->NewObject(bookmarkCls, constructorID, s, page, bm.level);
        env->SetObjectArrayElement(ar, i, o);
        env->DeleteLocalRef(o);
        env->DeleteLocalRef(s);
    }
    return ar;
}

JNI_FUNC(jobject, Pdfium, openPage)(JNI_ARGS, jint page) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_DOCUMENT doc = (FPDF_DOCUMENT) env->GetLongField(thiz, fid);

    FPDF_PAGE p = FPDF_LoadPage(doc, page);
    if (p != 0) {
        jclass clazz = env->FindClass("com/github/axet/pdfium/Pdfium$Page");
        jmethodID constructorID = env->GetMethodID(clazz, "<init>",
                                                   "(Lcom/github/axet/pdfium/Pdfium;)V");
        jfieldID fid1 = env->GetFieldID(clazz, "handle", "J");
        jobject o = env->NewObject(clazz, constructorID, thiz);
        env->SetLongField(o, fid1, (jlong) p);
        return o;
    } else {
        return 0;
    }
}

JNI_FUNC(jobject, Pdfium, getPageSize)(JNI_ARGS, jint pageIndex) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_DOCUMENT doc = (FPDF_DOCUMENT) env->GetLongField(thiz, fid);

    double width, height;
    int result = FPDF_GetPageSizeByIndex(doc, pageIndex, &width, &height);

    if (result == 0) {
        width = 0;
        height = 0;
    }

    jclass clazz = env->FindClass("com/github/axet/pdfium/Pdfium$Size");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(II)V");
    return env->NewObject(clazz, constructorID, (int) width, (int) height);
}

JNI_FUNC(void, Pdfium_00024Page, render)(JNI_ARGS, jobject bitmap,
                                         jint startX, jint startY,
                                         jint drawSizeHor, jint drawSizeVer,
                                         jboolean renderAnnot) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_PAGE page = (FPDF_DOCUMENT) env->GetLongField(thiz, fid);

    AndroidBitmapInfo info;
    int ret;
    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("Fetching bitmap info failed: %s", strerror(ret * -1));
        return;
    }

    int canvasHorSize = info.width;
    int canvasVerSize = info.height;

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
        info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
        LOGE("Bitmap format must be RGBA_8888 or RGB_565");
        return;
    }

    void *addr;
    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &addr)) != 0) {
        LOGE("Locking bitmap failed: %s", strerror(ret * -1));
        return;
    }

    void *tmp;
    int format;
    int sourceStride;
    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        tmp = malloc(canvasVerSize * canvasHorSize * sizeof(rgb));
        sourceStride = canvasHorSize * sizeof(rgb);
        format = FPDFBitmap_BGR;
    } else {
        tmp = addr;
        sourceStride = info.stride;
        format = FPDFBitmap_BGRA;
    }

    FPDF_BITMAP pdfBitmap = FPDFBitmap_CreateEx(canvasHorSize, canvasVerSize,
                                                format, tmp, sourceStride);

    /*LOGD("Start X: %d", startX);
    LOGD("Start Y: %d", startY);
    LOGD("Canvas Hor: %d", canvasHorSize);
    LOGD("Canvas Ver: %d", canvasVerSize);
    LOGD("Draw Hor: %d", drawSizeHor);
    LOGD("Draw Ver: %d", drawSizeVer);*/

    if (drawSizeHor < canvasHorSize || drawSizeVer < canvasVerSize) {
        FPDFBitmap_FillRect(pdfBitmap, 0, 0, canvasHorSize, canvasVerSize,
                            0x848484FF); //Gray
    }

    int baseHorSize = (canvasHorSize < drawSizeHor) ? canvasHorSize : (int) drawSizeHor;
    int baseVerSize = (canvasVerSize < drawSizeVer) ? canvasVerSize : (int) drawSizeVer;
    int baseX = (startX < 0) ? 0 : (int) startX;
    int baseY = (startY < 0) ? 0 : (int) startY;
    int flags = FPDF_REVERSE_BYTE_ORDER;

    if (renderAnnot) {
        flags |= FPDF_ANNOT;
    }

    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        FPDFBitmap_FillRect(pdfBitmap, baseX, baseY, baseHorSize, baseVerSize,
                            0xFFFFFFFF); //White
    }

    FPDF_RenderPageBitmap(pdfBitmap, page,
                          startX, startY,
                          (int) drawSizeHor, (int) drawSizeVer,
                          0, flags);

    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        rgbBitmapTo565(tmp, sourceStride, addr, &info);
        free(tmp);
    }

    AndroidBitmap_unlockPixels(env, bitmap);
}

JNI_FUNC(jobjectArray, Pdfium_00024Page, getLinks)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_PAGE page = (FPDF_PAGE) env->GetLongField(thiz, fid);

    FPDF_DOCUMENT doc = (FPDF_DOCUMENT) outerHandle(env, thiz);

    int pos = 0;
    std::vector<jlong> links;
    FPDF_LINK link;
    while (FPDFLink_Enumerate(page, &pos, &link)) {
        links.push_back(reinterpret_cast<jlong>(link));
    }

    jclass linkClass = env->FindClass("com/github/axet/pdfium/Pdfium$Link");
    jobjectArray result = env->NewObjectArray(links.size(), linkClass, 0);
    for (int i = 0; i < links.size(); i++) {
        int index = -1;
        FPDF_DEST dest = FPDFLink_GetDest(doc, link);
        if (dest != 0)
            index = (int) FPDFDest_GetPageIndex(doc, dest);

        jstring s = 0;
        FPDF_ACTION action = FPDFLink_GetAction(link);
        if (action != 0) {
            size_t bufferLen = FPDFAction_GetURIPath(doc, action, NULL, 0);
            if (bufferLen > 0) {
                char *buf = (char *) malloc(bufferLen);
                FPDFAction_GetURIPath(doc, action, buf, bufferLen);
                s = env->NewStringUTF(buf);
                free(buf);
            }
        }

        jobject rect = 0;
        FS_RECTF fsRectF;
        if (FPDFLink_GetAnnotRect(link, &fsRectF)) {
            jclass clazz = env->FindClass("android/graphics/Rect");
            jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(IIII)V");
            rect = env->NewObject(clazz, constructorID, (int) fsRectF.left, (int) fsRectF.top,
                                  (int) fsRectF.right, (int) fsRectF.bottom);
        }

        jmethodID constructorID = env->GetMethodID(linkClass, "<init>",
                                                   "(Ljava/lang/String;ILandroid/graphics/Rect;)V");
        jobject v = env->NewObject(linkClass, constructorID, s, index, rect);

        env->SetObjectArrayElement(result, i, v);

        env->DeleteLocalRef(v);
        env->DeleteLocalRef(rect);
        env->DeleteLocalRef(s);
    }
    return result;
}

JNI_FUNC(jobject, Pdfium_00024Page, toDevice)(JNI_ARGS, jint startX,
                                              jint startY, jint sizeX,
                                              jint sizeY, jint rotate,
                                              jdouble pageX,
                                              jdouble pageY) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_PAGE page = (FPDF_PAGE) env->GetLongField(thiz, fid);

    int deviceX, deviceY;

    FPDF_PageToDevice(page, startX, startY, sizeX, sizeY, rotate, pageX, pageY, &deviceX, &deviceY);

    jclass clazz = env->FindClass("android/graphics/Point");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(II)V");
    return env->NewObject(clazz, constructorID, deviceX, deviceY);
}

JNI_FUNC(jobject, Pdfium_00024Page, toPage)(JNI_ARGS, jint startX,
                                            jint startY, jint sizeX,
                                            jint sizeY, jint rotate,
                                            jint deviceX,
                                            jint deviceY) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_PAGE page = (FPDF_PAGE) env->GetLongField(thiz, fid);

    double pageX, pageY;

    FPDF_DeviceToPage(page, startX, startY, sizeX, sizeY, rotate, deviceX, deviceY, &pageX, &pageY);

    jclass clazz = env->FindClass("android/graphics/Point");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "(II)V");
    return env->NewObject(clazz, constructorID, (int) pageX, (int) pageY);
}

JNI_FUNC(jobject, Pdfium_00024Page, open)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_PAGE page = (FPDF_PAGE) env->GetLongField(thiz, fid);

    jclass clazz = env->FindClass("com/github/axet/pdfium/Pdfium$Text");
    jmethodID constructorID = env->GetMethodID(clazz, "<init>", "()V");
    jfieldID fidText = env->GetFieldID(clazz, "handle", "J");
    jobject o = env->NewObject(clazz, constructorID);
    FPDF_TEXTPAGE text = FPDFText_LoadPage((FPDF_PAGE) page);
    env->SetLongField(o, fidText, (jlong) text);
    return o;
}

JNI_FUNC(void, Pdfium_00024Page, close)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_PAGE page = (FPDF_PAGE) env->GetLongField(thiz, fid);
    if (page != 0)
        FPDF_ClosePage(page);
    env->SetLongField(thiz, fid, 0);
}

JNI_FUNC(jlong, Pdfium_00024Text, getCount)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_TEXTPAGE text = (FPDF_TEXTPAGE) env->GetLongField(thiz, fid);
    return FPDFText_CountChars(text);
}

JNI_FUNC(jint, Pdfium_00024Text, getIndex)(JNI_ARGS, jint x, jint y) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_TEXTPAGE text = (FPDF_TEXTPAGE) env->GetLongField(thiz, fid);
    return FPDFText_GetCharIndexAtPos(text, x, y, 1, 1);
}

JNI_FUNC(jstring, Pdfium_00024Text, getText)(JNI_ARGS, jint start, jint count) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_TEXTPAGE text = (FPDF_TEXTPAGE) env->GetLongField(thiz, fid);
    jchar *str = (jchar *) malloc((count + 1) * sizeof(jchar));
    int len = FPDFText_GetText(text, start, count, str);
    if (len > 0) {
        jstring s = env->NewString(str, len - 1); // no trailing zero
        free(str);
        return s;
    } else {
        return 0;
    }
}

JNI_FUNC(jobjectArray, Pdfium_00024Text, getBounds)(JNI_ARGS, jint start, jint count) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_TEXTPAGE text = (FPDF_TEXTPAGE) env->GetLongField(thiz, fid);
    int c = FPDFText_CountRects(text, start, count);
    jclass rectCls = env->FindClass("android/graphics/Rect");
    jmethodID constructorID = env->GetMethodID(rectCls, "<init>", "(IIII)V");
    jobjectArray ar = env->NewObjectArray(c, rectCls, 0);
    for (int i = 0; i < c; i++) {
        double l, t, r, b;
        FPDFText_GetRect(text, i, &l, &t, &r, &b);
        jobject v = env->NewObject(rectCls, constructorID, (int) l, (int) t, (int) r, (int) b);
        env->SetObjectArrayElement(ar, i, v);
        env->DeleteLocalRef(v);
    }
    return ar;
}

JNI_FUNC(jobject, Pdfium_00024Text, search)(JNI_ARGS, jstring str, jint flags, jint index) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_TEXTPAGE tp = (FPDF_TEXTPAGE) env->GetLongField(thiz, fid);

    FPDF_WIDESTRING ss = GetStringUTF16LEChars(env, str);
    FPDF_SCHHANDLE search = FPDFText_FindStart(tp, ss, (unsigned long) flags, index);
    ReleaseStringUTF16LEChars(ss);

    jclass searchClass = env->FindClass("com/github/axet/pdfium/Pdfium$Search");
    jmethodID constructorID = env->GetMethodID(searchClass, "<init>", "()V");
    jobject o = env->NewObject(searchClass, constructorID);
    jfieldID fid2 = env->GetFieldID(searchClass, "handle", "J");
    env->SetLongField(o, fid2, (jlong) search);
    return o;
}

JNI_FUNC(void, Pdfium_00024Text, close)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_TEXTPAGE text = (FPDF_TEXTPAGE) env->GetLongField(thiz, fid);
    if (text != 0)
        FPDFText_ClosePage(text);
    env->SetLongField(thiz, fid, (jlong) 0);
}

JNI_FUNC(jboolean, Pdfium_00024Search, next)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_SCHHANDLE search = (FPDF_SCHHANDLE) env->GetLongField(thiz, fid);
    return (jboolean) FPDFText_FindNext(search);
}

JNI_FUNC(jboolean, Pdfium_00024Search, prev)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_SCHHANDLE search = (FPDF_SCHHANDLE) env->GetLongField(thiz, fid);
    return (jboolean) FPDFText_FindPrev(search);
}

JNI_FUNC(jobject, Pdfium_00024Search, result)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_SCHHANDLE search = (FPDF_SCHHANDLE) env->GetLongField(thiz, fid);
    jclass klass = env->FindClass("com/github/axet/pdfium/Pdfium$TextResult");
    jmethodID constructorID = env->GetMethodID(klass, "<init>", "(II)V");
    int s = FPDFText_GetSchResultIndex(search);
    int c = FPDFText_GetSchCount(search);
    return env->NewObject(klass, constructorID, s, c);
}

JNI_FUNC(void, Pdfium_00024Search, close)(JNI_ARGS) {
    jclass cls = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(cls, "handle", "J");
    FPDF_SCHHANDLE search = (FPDF_SCHHANDLE) env->GetLongField(thiz, fid);
    if (search != 0)
        FPDFText_FindClose(search);
    env->SetLongField(thiz, fid, (jlong) 0);
}

} // extern C
