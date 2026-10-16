#include "pysdl.h"

#ifdef PYSDL_HAVE_TTF

// SDL3_ttf: the SDL3.Font type plus the TTF_* module functions and constants
// (registered in PyInit_SDL3 under PYSDL_HAVE_TTF). Built only when setup.py
// finds SDL3_ttf.
//
// Text is always a Python str, passed as UTF-8 with its length; glyphs are
// 32-bit codepoints (a one-character str or an int). Each Render* method
// takes an optional wrap_length in place of the separate *_Wrapped functions.
//
// Sources: a path (str / os.PathLike) or the font file's bytes, through
// PySDL_IOFromObject. SDL_ttf reads the stream lazily for the font's whole
// life, so the bytes stay pinned in `data` until TTF_CloseFont (which closes
// the stream: it is opened with autoclose, and SDL_ttf also closes it when
// the open fails).
//
// Lifetime: fonts must be closed before SDL_ttf shuts down (TTF_CloseFont
// after the final TTF_Quit touches freed FreeType state). Every Font records
// the init "session" it was opened in; once TTF_Quit really shuts SDL_ttf
// down the session advances, and older fonts raise on use and are dropped,
// not closed.
//
// The GIL is held throughout: a TTF_Font is not safe to use from two threads
// at once.

static int        PySDL_Font_Type_init    (PySDL_Font*, PyObject*, PyObject*);
static void       PySDL_Font_Type_dealloc (PySDL_Font*);

static PyObject * PySDL_Font_Close              (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetStyle           (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetStyle           (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetOutline         (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetOutline         (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetHinting         (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetHinting         (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetKerning         (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetKerning         (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_Height             (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_Ascent             (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_Descent            (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_LineSkip           (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetLineSkip        (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_Faces              (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_FaceIsFixedWidth   (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_IsScalable         (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_FaceFamilyName     (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_FaceStyleName      (PySDL_Font*, PyObject*);
#if SDL_TTF_VERSION_ATLEAST(3,2,2)
static PyObject * PySDL_Font_GetWeight          (PySDL_Font*, PyObject*);
#endif
static PyObject * PySDL_Font_GetSize            (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetSize            (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetSizeDPI         (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetDPI             (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetSDF             (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetSDF             (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetWrappedAlign    (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetWrappedAlign    (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetDirection       (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetDirection       (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetScript          (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetScript          (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SetLanguage        (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GlyphIsProvided    (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GlyphMetrics       (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_GetKerningSize     (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_Size               (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_SizeWrapped        (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_Measure            (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_RenderSolid        (PySDL_Font*, PyObject*, PyObject*);
static PyObject * PySDL_Font_RenderShaded       (PySDL_Font*, PyObject*, PyObject*);
static PyObject * PySDL_Font_RenderBlended      (PySDL_Font*, PyObject*, PyObject*);
static PyObject * PySDL_Font_RenderLCD          (PySDL_Font*, PyObject*, PyObject*);
static PyObject * PySDL_Font_RenderGlyphSolid   (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_RenderGlyphShaded  (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_RenderGlyphBlended (PySDL_Font*, PyObject*);
static PyObject * PySDL_Font_RenderGlyphLCD     (PySDL_Font*, PyObject*);

// Methods drop TTF_ and Font; the pysdl2 names come first, SDL3 names follow
// as aliases.
static PyMethodDef PySDL_Font_methods[] = {
    { "Close",              (PyCFunction)PySDL_Font_Close,              METH_NOARGS  },
    { "GetStyle",           (PyCFunction)PySDL_Font_GetStyle,           METH_NOARGS  },
    { "SetStyle",           (PyCFunction)PySDL_Font_SetStyle,           METH_O       },
    { "GetOutline",         (PyCFunction)PySDL_Font_GetOutline,         METH_NOARGS  },
    { "SetOutline",         (PyCFunction)PySDL_Font_SetOutline,         METH_O       },
    { "GetHinting",         (PyCFunction)PySDL_Font_GetHinting,         METH_NOARGS  },
    { "SetHinting",         (PyCFunction)PySDL_Font_SetHinting,         METH_O       },
    { "GetKerning",         (PyCFunction)PySDL_Font_GetKerning,         METH_NOARGS  },
    { "SetKerning",         (PyCFunction)PySDL_Font_SetKerning,         METH_O       },
    { "Height",             (PyCFunction)PySDL_Font_Height,             METH_NOARGS  },
    { "GetHeight",          (PyCFunction)PySDL_Font_Height,             METH_NOARGS  },
    { "Ascent",             (PyCFunction)PySDL_Font_Ascent,             METH_NOARGS  },
    { "GetAscent",          (PyCFunction)PySDL_Font_Ascent,             METH_NOARGS  },
    { "Descent",            (PyCFunction)PySDL_Font_Descent,            METH_NOARGS  },
    { "GetDescent",         (PyCFunction)PySDL_Font_Descent,            METH_NOARGS  },
    { "LineSkip",           (PyCFunction)PySDL_Font_LineSkip,           METH_NOARGS  },
    { "GetLineSkip",        (PyCFunction)PySDL_Font_LineSkip,           METH_NOARGS  },
    { "SetLineSkip",        (PyCFunction)PySDL_Font_SetLineSkip,        METH_O       },
    { "Faces",              (PyCFunction)PySDL_Font_Faces,              METH_NOARGS  },
    { "GetNumFaces",        (PyCFunction)PySDL_Font_Faces,              METH_NOARGS  },
    { "FaceIsFixedWidth",   (PyCFunction)PySDL_Font_FaceIsFixedWidth,   METH_NOARGS  },
    { "IsFixedWidth",       (PyCFunction)PySDL_Font_FaceIsFixedWidth,   METH_NOARGS  },
    { "IsScalable",         (PyCFunction)PySDL_Font_IsScalable,         METH_NOARGS  },
    { "FaceFamilyName",     (PyCFunction)PySDL_Font_FaceFamilyName,     METH_NOARGS  },
    { "GetFamilyName",      (PyCFunction)PySDL_Font_FaceFamilyName,     METH_NOARGS  },
    { "FaceStyleName",      (PyCFunction)PySDL_Font_FaceStyleName,      METH_NOARGS  },
    { "GetStyleName",       (PyCFunction)PySDL_Font_FaceStyleName,      METH_NOARGS  },
#if SDL_TTF_VERSION_ATLEAST(3,2,2)
    { "GetWeight",          (PyCFunction)PySDL_Font_GetWeight,          METH_NOARGS  },
#endif
    { "GetSize",            (PyCFunction)PySDL_Font_GetSize,            METH_NOARGS  },
    { "SetSize",            (PyCFunction)PySDL_Font_SetSize,            METH_O       },
    { "SetSizeDPI",         (PyCFunction)PySDL_Font_SetSizeDPI,         METH_VARARGS },
    { "GetDPI",             (PyCFunction)PySDL_Font_GetDPI,             METH_NOARGS  },
    { "GetSDF",             (PyCFunction)PySDL_Font_GetSDF,             METH_NOARGS  },
    { "SetSDF",             (PyCFunction)PySDL_Font_SetSDF,             METH_O       },
    { "GetWrappedAlign",    (PyCFunction)PySDL_Font_GetWrappedAlign,    METH_NOARGS  },
    { "GetWrapAlignment",   (PyCFunction)PySDL_Font_GetWrappedAlign,    METH_NOARGS  },
    { "SetWrappedAlign",    (PyCFunction)PySDL_Font_SetWrappedAlign,    METH_O       },
    { "SetWrapAlignment",   (PyCFunction)PySDL_Font_SetWrappedAlign,    METH_O       },
    { "GetDirection",       (PyCFunction)PySDL_Font_GetDirection,       METH_NOARGS  },
    { "SetDirection",       (PyCFunction)PySDL_Font_SetDirection,       METH_O       },
    { "GetScript",          (PyCFunction)PySDL_Font_GetScript,          METH_NOARGS  },
    { "SetScript",          (PyCFunction)PySDL_Font_SetScript,          METH_O       },
    { "SetScriptName",      (PyCFunction)PySDL_Font_SetScript,          METH_O       },
    { "SetLanguage",        (PyCFunction)PySDL_Font_SetLanguage,        METH_O       },
    { "GlyphIsProvided",    (PyCFunction)PySDL_Font_GlyphIsProvided,    METH_O       },
    { "HasGlyph",           (PyCFunction)PySDL_Font_GlyphIsProvided,    METH_O       },
    { "GlyphMetrics",       (PyCFunction)PySDL_Font_GlyphMetrics,       METH_O       },
    { "GetGlyphMetrics",    (PyCFunction)PySDL_Font_GlyphMetrics,       METH_O       },
    { "GetKerningSize",     (PyCFunction)PySDL_Font_GetKerningSize,     METH_VARARGS },
    { "GetGlyphKerning",    (PyCFunction)PySDL_Font_GetKerningSize,     METH_VARARGS },
    { "Size",               (PyCFunction)PySDL_Font_Size,               METH_O       },
    { "GetStringSize",      (PyCFunction)PySDL_Font_Size,               METH_O       },
    { "SizeWrapped",        (PyCFunction)PySDL_Font_SizeWrapped,        METH_VARARGS },
    { "GetStringSizeWrapped", (PyCFunction)PySDL_Font_SizeWrapped,      METH_VARARGS },
    { "Measure",            (PyCFunction)PySDL_Font_Measure,            METH_VARARGS },
    { "MeasureString",      (PyCFunction)PySDL_Font_Measure,            METH_VARARGS },
    { "RenderSolid",        (PyCFunction)PySDL_Font_RenderSolid,        METH_VARARGS | METH_KEYWORDS },
    { "RenderShaded",       (PyCFunction)PySDL_Font_RenderShaded,       METH_VARARGS | METH_KEYWORDS },
    { "RenderBlended",      (PyCFunction)PySDL_Font_RenderBlended,      METH_VARARGS | METH_KEYWORDS },
    { "RenderLCD",          (PyCFunction)PySDL_Font_RenderLCD,          METH_VARARGS | METH_KEYWORDS },
    { "RenderGlyphSolid",   (PyCFunction)PySDL_Font_RenderGlyphSolid,   METH_VARARGS },
    { "RenderGlyphShaded",  (PyCFunction)PySDL_Font_RenderGlyphShaded,  METH_VARARGS },
    { "RenderGlyphBlended", (PyCFunction)PySDL_Font_RenderGlyphBlended, METH_VARARGS },
    { "RenderGlyphLCD",     (PyCFunction)PySDL_Font_RenderGlyphLCD,     METH_VARARGS },
    { NULL }
};

PyTypeObject PySDL_Font_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Font",
    .tp_basicsize = sizeof(PySDL_Font),
    .tp_dealloc   = (destructor)PySDL_Font_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Font(src, ptsize=12.0, index=0, *, hdpi=0, vdpi=0): src is a path or the font file's bytes",
    .tp_methods   = PySDL_Font_methods,
    .tp_init      = (initproc)PySDL_Font_Type_init,
    .tp_new       = PyType_GenericNew
};

static PyObject * PySDL_TTF_Init             (PyObject*, PyObject*);
static PyObject * PySDL_TTF_Quit             (PyObject*, PyObject*);
static PyObject * PySDL_TTF_WasInit          (PyObject*, PyObject*);
static PyObject * PySDL_TTF_Version          (PyObject*, PyObject*);
static PyObject * PySDL_TTF_GetFreeTypeVersion (PyObject*, PyObject*);
static PyObject * PySDL_TTF_GetHarfBuzzVersion (PyObject*, PyObject*);
static PyObject * PySDL_TTF_StringToTag      (PyObject*, PyObject*);
static PyObject * PySDL_TTF_TagToString      (PyObject*, PyObject*);
static PyObject * PySDL_TTF_GetGlyphScript   (PyObject*, PyObject*);

// Module functions keep the TTF_ prefix, like IMG_.
PyMethodDef pysdl_ttf_methods[] = {
    { "TTF_Init",               PySDL_TTF_Init,               METH_NOARGS },
    { "TTF_Quit",               PySDL_TTF_Quit,               METH_NOARGS },
    { "TTF_WasInit",            PySDL_TTF_WasInit,            METH_NOARGS },
    { "TTF_Version",            PySDL_TTF_Version,            METH_NOARGS },
    { "TTF_Linked_Version",     PySDL_TTF_Version,            METH_NOARGS },  // pysdl2 name
    { "TTF_GetFreeTypeVersion", PySDL_TTF_GetFreeTypeVersion, METH_NOARGS },
    { "TTF_GetHarfBuzzVersion", PySDL_TTF_GetHarfBuzzVersion, METH_NOARGS },
    { "TTF_StringToTag",        PySDL_TTF_StringToTag,        METH_O      },
    { "TTF_TagToString",        PySDL_TTF_TagToString,        METH_O      },
    { "TTF_GetGlyphScript",     PySDL_TTF_GetGlyphScript,     METH_O      },
    { NULL }
};

//=========================================================
// helpers
//=========================================================

static unsigned _session = 1;  // advances each time SDL_ttf fully shuts down

static int _alive(PySDL_Font *self) {
    return NULL != self->font && TTF_WasInit() > 0 && self->session == _session;
}

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

static void _close(PySDL_Font *self) {
    if(_alive(self)) {
        TTF_CloseFont(self->font);  // also closes the stream (opened with autoclose)
    }
    self->font = NULL;
    if(NULL != self->data.obj) {
        PyBuffer_Release(&self->data);  // only once SDL_ttf has let go of it
    }
}

static TTF_Font * _font(PySDL_Font *self) {
    if(NULL == self->font) {
        PyErr_SetString(pysdl_Error, "Font is not open");
        return NULL;
    }
    if(!_alive(self)) {
        PyErr_SetString(pysdl_Error, "Font is no longer valid: SDL3.TTF_Quit() was called");
        return NULL;
    }
    return self->font;
}

// str -> UTF-8 and its length in bytes. NUL characters are refused: a length
// of 0 means "NUL-terminated" to SDL_ttf, and its wrapping stops at a NUL.
static const char * _utf8(PyObject *text, size_t *length) {
    Py_ssize_t len;
    const char *utf8 = PyUnicode_AsUTF8AndSize(text, &len);
    if(NULL == utf8) {
        return NULL;
    }
    if(NULL != memchr(utf8, '\0', (size_t)len)) {
        PyErr_SetString(PyExc_ValueError, "text must not contain NUL characters");
        return NULL;
    }
    *length = (size_t)len;
    return utf8;
}

// A one-character str or an int codepoint.
static int _codepoint(PyObject *obj, Uint32 *out) {
    if(PyUnicode_Check(obj)) {
        if(1 != PyUnicode_GET_LENGTH(obj)) {
            PyErr_SetString(PyExc_ValueError, "expected a single character");
            return 0;
        }
        *out = PyUnicode_READ_CHAR(obj, 0);
        return 1;
    }
    unsigned long value = PyLong_AsUnsignedLong(obj);
    if((unsigned long)-1 == value && PyErr_Occurred()) {
        return 0;
    }
    if(value > 0x10FFFF) {
        PyErr_SetString(PyExc_ValueError, "not a Unicode codepoint");
        return 0;
    }
    *out = (Uint32)value;
    return 1;
}

// An OpenType tag: a str of up to 4 ASCII characters (e.g. "Latn") or an int.
static int _tag(PyObject *obj, Uint32 *out) {
    if(PyUnicode_Check(obj)) {
        Py_ssize_t len;
        const char *text = PyUnicode_AsUTF8AndSize(obj, &len);
        if(NULL == text) {
            return 0;
        }
        if(len > 4 || (Py_ssize_t)strlen(text) != len) {
            PyErr_SetString(PyExc_ValueError, "a tag is at most 4 characters, e.g. 'Latn'");
            return 0;
        }
        *out = TTF_StringToTag(text);
        return 1;
    }
    unsigned long value = PyLong_AsUnsignedLong(obj);
    if((unsigned long)-1 == value && PyErr_Occurred()) {
        return 0;
    }
    if(value > 0xFFFFFFFFUL) {
        PyErr_SetString(PyExc_OverflowError, "tag does not fit in 32 bits");
        return 0;
    }
    *out = (Uint32)value;
    return 1;
}

static PyObject * _tag_to_str(Uint32 tag) {
    char text[5];
    TTF_TagToString(tag, text, sizeof(text));
    return PyUnicode_FromString(text);
}

static PyObject * _str_or_none(const char *text) {
    if(NULL == text) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(text);
}

// Wrap a surface SDL_ttf allocated as an owned SDL3.Surface; frees it on failure.
static PyObject * _wrap_surface(SDL_Surface *surface) {
    if(NULL == surface) {
        return _raise();
    }
    PySDL_Surface *wrapper = (PySDL_Surface *)PySDL_New(&PySDL_Surface_Type);
    if(NULL == wrapper) {
        SDL_DestroySurface(surface);
        return NULL;
    }
    wrapper->surface = surface;
    return (PyObject *)wrapper;
}

//=========================================================
// type
//=========================================================

static int PySDL_Font_Type_init(PySDL_Font *self, PyObject *args, PyObject *kwds) {
    PyObject *src = Py_None;
    float ptsize = 12.0f;
    long long index = 0;
    int hdpi = 0, vdpi = 0;

    static char *kwlist[] = {"src", "ptsize", "index", "hdpi", "vdpi", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|OfL$ii", kwlist, &src, &ptsize, &index, &hdpi, &vdpi)) {
        return -1;
    }

    _close(self);  // re-running __init__ on an open font
    self->session = _session;

    // No source: internal allocation, nothing to open.
    if(src == Py_None) {
        return 0;
    }
    if(0 == TTF_WasInit()) {
        PyErr_SetString(pysdl_Error, "call SDL3.TTF_Init() before opening a font");
        return -1;
    }
    if(hdpi < 0 || vdpi < 0) {
        PyErr_SetString(PyExc_ValueError, "hdpi / vdpi must be >= 0 (0 = the default)");
        return -1;
    }

    SDL_IOStream *io = PySDL_IOFromObject(src, &self->data);
    if(NULL == io) {
        return -1;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    bool ok = 0 != props
        && SDL_SetPointerProperty(props, TTF_PROP_FONT_CREATE_IOSTREAM_POINTER, io)
        && SDL_SetBooleanProperty(props, TTF_PROP_FONT_CREATE_IOSTREAM_AUTOCLOSE_BOOLEAN, true)
        && SDL_SetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, ptsize)
        && SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_FACE_NUMBER, (Sint64)index)
        && SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_HORIZONTAL_DPI_NUMBER, hdpi)
        && SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_VERTICAL_DPI_NUMBER, vdpi);
    if(ok) {
        self->font = TTF_OpenFontWithProperties(props);  // closes io itself on failure
    } else {
        SDL_CloseIO(io);
    }
    if(0 != props) {
        SDL_DestroyProperties(props);
    }
    if(NULL == self->font) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        if(NULL != self->data.obj) {
            PyBuffer_Release(&self->data);
        }
        return -1;
    }
    return 0;
}

static void PySDL_Font_Type_dealloc(PySDL_Font *self) {
    _close(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject * PySDL_Font_Close(PySDL_Font *self, PyObject *ign) {
    _close(self);
    Py_RETURN_NONE;
}

//=========================================================
// attributes
//=========================================================

#define FONT_GET_INT(NAME, CALL) \
    static PyObject * PySDL_Font_##NAME(PySDL_Font *self, PyObject *ign) { \
        TTF_Font *font = _font(self); \
        return font ? PyLong_FromLong((long)CALL(font)) : NULL; \
    }

#define FONT_GET_BOOL(NAME, CALL) \
    static PyObject * PySDL_Font_##NAME(PySDL_Font *self, PyObject *ign) { \
        TTF_Font *font = _font(self); \
        return font ? PyBool_FromLong(CALL(font)) : NULL; \
    }

FONT_GET_INT(GetStyle,        TTF_GetFontStyle)
FONT_GET_INT(GetOutline,      TTF_GetFontOutline)
FONT_GET_INT(GetHinting,      TTF_GetFontHinting)
FONT_GET_INT(Height,          TTF_GetFontHeight)
FONT_GET_INT(Ascent,          TTF_GetFontAscent)
FONT_GET_INT(Descent,         TTF_GetFontDescent)
FONT_GET_INT(LineSkip,        TTF_GetFontLineSkip)
FONT_GET_INT(Faces,           TTF_GetNumFontFaces)
FONT_GET_INT(GetWrappedAlign, TTF_GetFontWrapAlignment)
FONT_GET_INT(GetDirection,    TTF_GetFontDirection)
#if SDL_TTF_VERSION_ATLEAST(3,2,2)
FONT_GET_INT(GetWeight,       TTF_GetFontWeight)
#endif

FONT_GET_BOOL(GetKerning,       TTF_GetFontKerning)
FONT_GET_BOOL(GetSDF,           TTF_GetFontSDF)
FONT_GET_BOOL(FaceIsFixedWidth, TTF_FontIsFixedWidth)
FONT_GET_BOOL(IsScalable,       TTF_FontIsScalable)

// Setters SDL_ttf cannot fail (it returns void).
#define FONT_SET_INT(NAME, CALL, TYPE) \
    static PyObject * PySDL_Font_##NAME(PySDL_Font *self, PyObject *arg) { \
        TTF_Font *font = _font(self); \
        long value = font ? PyLong_AsLong(arg) : 0; \
        if(NULL == font || (-1 == value && PyErr_Occurred())) { \
            return NULL; \
        } \
        CALL(font, (TYPE)value); \
        Py_RETURN_NONE; \
    }

FONT_SET_INT(SetStyle,        TTF_SetFontStyle,         TTF_FontStyleFlags)
FONT_SET_INT(SetHinting,      TTF_SetFontHinting,       TTF_HintingFlags)
FONT_SET_INT(SetWrappedAlign, TTF_SetFontWrapAlignment, TTF_HorizontalAlignment)
FONT_SET_INT(SetLineSkip,     TTF_SetFontLineSkip,      int)

static PyObject * PySDL_Font_SetOutline(PySDL_Font *self, PyObject *arg) {
    TTF_Font *font = _font(self);
    long outline = font ? PyLong_AsLong(arg) : 0;
    if(NULL == font || (-1 == outline && PyErr_Occurred())) {
        return NULL;
    }
    if(outline < 0 || outline > INT_MAX) {
        PyErr_SetString(PyExc_ValueError, "outline must be >= 0");
        return NULL;
    }
    if(!TTF_SetFontOutline(font, (int)outline)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Font_SetKerning(PySDL_Font *self, PyObject *arg) {
    TTF_Font *font = _font(self);
    int enabled = font ? PyObject_IsTrue(arg) : -1;
    if(NULL == font || -1 == enabled) {
        return NULL;
    }
    TTF_SetFontKerning(font, enabled ? true : false);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Font_SetSDF(PySDL_Font *self, PyObject *arg) {
    TTF_Font *font = _font(self);
    int enabled = font ? PyObject_IsTrue(arg) : -1;
    if(NULL == font || -1 == enabled) {
        return NULL;
    }
    if(!TTF_SetFontSDF(font, enabled ? true : false)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Font_FaceFamilyName(PySDL_Font *self, PyObject *ign) {
    TTF_Font *font = _font(self);
    return font ? _str_or_none(TTF_GetFontFamilyName(font)) : NULL;
}

static PyObject * PySDL_Font_FaceStyleName(PySDL_Font *self, PyObject *ign) {
    TTF_Font *font = _font(self);
    return font ? _str_or_none(TTF_GetFontStyleName(font)) : NULL;
}

static PyObject * PySDL_Font_GetSize(PySDL_Font *self, PyObject *ign) {
    TTF_Font *font = _font(self);
    return font ? PyFloat_FromDouble(TTF_GetFontSize(font)) : NULL;
}

static PyObject * PySDL_Font_SetSize(PySDL_Font *self, PyObject *arg) {
    TTF_Font *font = _font(self);
    double ptsize = font ? PyFloat_AsDouble(arg) : 0.0;
    if(NULL == font || (-1.0 == ptsize && PyErr_Occurred())) {
        return NULL;
    }
    if(!TTF_SetFontSize(font, (float)ptsize)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

// SetSizeDPI(ptsize, hdpi, vdpi)
static PyObject * PySDL_Font_SetSizeDPI(PySDL_Font *self, PyObject *args) {
    float ptsize;
    int hdpi, vdpi;
    TTF_Font *font = _font(self);
    if(NULL == font || !PyArg_ParseTuple(args, "fii", &ptsize, &hdpi, &vdpi)) {
        return NULL;
    }
    if(!TTF_SetFontSizeDPI(font, ptsize, hdpi, vdpi)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

// GetDPI() -> (hdpi, vdpi)
static PyObject * PySDL_Font_GetDPI(PySDL_Font *self, PyObject *ign) {
    int hdpi = 0, vdpi = 0;
    TTF_Font *font = _font(self);
    if(NULL == font) {
        return NULL;
    }
    if(!TTF_GetFontDPI(font, &hdpi, &vdpi)) {
        return _raise();
    }
    return Py_BuildValue("(ii)", hdpi, vdpi);
}

static PyObject * PySDL_Font_SetDirection(PySDL_Font *self, PyObject *arg) {
    TTF_Font *font = _font(self);
    long direction = font ? PyLong_AsLong(arg) : 0;
    if(NULL == font || (-1 == direction && PyErr_Occurred())) {
        return NULL;
    }
    if(!TTF_SetFontDirection(font, (TTF_Direction)direction)) {
        return _raise();  // e.g. SDL_ttf built without HarfBuzz
    }
    Py_RETURN_NONE;
}

// GetScript() -> the ISO 15924 script tag used for shaping, e.g. "Latn"
// ("" for automatic).
static PyObject * PySDL_Font_GetScript(PySDL_Font *self, PyObject *ign) {
    TTF_Font *font = _font(self);
    return font ? _tag_to_str(TTF_GetFontScript(font)) : NULL;
}

// SetScript("Latn") (also SetScriptName): a tag str or its int; 0 = automatic.
static PyObject * PySDL_Font_SetScript(PySDL_Font *self, PyObject *arg) {
    Uint32 script;
    TTF_Font *font = _font(self);
    if(NULL == font || !_tag(arg, &script)) {
        return NULL;
    }
    if(!TTF_SetFontScript(font, script)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

// SetLanguage("en-US"), a BCP 47 code, or None for the default.
static PyObject * PySDL_Font_SetLanguage(PySDL_Font *self, PyObject *arg) {
    TTF_Font *font = _font(self);
    if(NULL == font) {
        return NULL;
    }
    const char *language = NULL;
    if(arg != Py_None && NULL == (language = PyUnicode_AsUTF8(arg))) {
        return NULL;
    }
    if(!TTF_SetFontLanguage(font, language)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

//=========================================================
// glyphs and measuring
//=========================================================

static PyObject * PySDL_Font_GlyphIsProvided(PySDL_Font *self, PyObject *arg) {
    Uint32 ch;
    TTF_Font *font = _font(self);
    if(NULL == font || !_codepoint(arg, &ch)) {
        return NULL;
    }
    return PyBool_FromLong(TTF_FontHasGlyph(font, ch));
}

// GlyphMetrics(ch) -> (minx, maxx, miny, maxy, advance)
static PyObject * PySDL_Font_GlyphMetrics(PySDL_Font *self, PyObject *arg) {
    Uint32 ch;
    TTF_Font *font = _font(self);
    if(NULL == font || !_codepoint(arg, &ch)) {
        return NULL;
    }
    int minx, maxx, miny, maxy, advance;
    if(!TTF_GetGlyphMetrics(font, ch, &minx, &maxx, &miny, &maxy, &advance)) {
        return _raise();
    }
    return Py_BuildValue("(iiiii)", minx, maxx, miny, maxy, advance);
}

// GetKerningSize(previous_ch, ch) -> pixels to adjust between the two glyphs
static PyObject * PySDL_Font_GetKerningSize(PySDL_Font *self, PyObject *args) {
    PyObject *a_py, *b_py;
    Uint32 a, b;
    TTF_Font *font = _font(self);
    if(NULL == font || !PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !_codepoint(a_py, &a) || !_codepoint(b_py, &b)) {
        return NULL;
    }
    int kerning = 0;
    if(!TTF_GetGlyphKerning(font, a, b, &kerning)) {
        return _raise();
    }
    return PyLong_FromLong(kerning);
}

// Size(text) -> (w, h) the rendered text would take, without rendering it.
static PyObject * PySDL_Font_Size(PySDL_Font *self, PyObject *arg) {
    size_t length;
    TTF_Font *font = _font(self);
    const char *text = font ? _utf8(arg, &length) : NULL;
    if(NULL == text) {
        return NULL;
    }
    int w = 0, h = 0;
    if(!TTF_GetStringSize(font, text, length, &w, &h)) {
        return _raise();
    }
    return Py_BuildValue("(ii)", w, h);
}

// SizeWrapped(text, wrap_width) -> (w, h) of the text wrapped at wrap_width
// pixels (0 = only at newlines).
static PyObject * PySDL_Font_SizeWrapped(PySDL_Font *self, PyObject *args) {
    PyObject *text_py;
    int wrap;
    size_t length;
    TTF_Font *font = _font(self);
    if(NULL == font || !PyArg_ParseTuple(args, "Oi", &text_py, &wrap)) {
        return NULL;
    }
    const char *text = _utf8(text_py, &length);
    if(NULL == text) {
        return NULL;
    }
    if(wrap < 0) {
        PyErr_SetString(PyExc_ValueError, "wrap_width must be >= 0");
        return NULL;
    }
    int w = 0, h = 0;
    if(!TTF_GetStringSizeWrapped(font, text, length, wrap, &w, &h)) {
        return _raise();
    }
    return Py_BuildValue("(ii)", w, h);
}

// Measure(text, width) -> (extent, count): how many characters fit in `width`
// pixels, and how wide they are. SDL counts UTF-8 bytes; count is characters.
static PyObject * PySDL_Font_Measure(PySDL_Font *self, PyObject *args) {
    PyObject *text_py;
    int width;
    size_t length;
    TTF_Font *font = _font(self);
    if(NULL == font || !PyArg_ParseTuple(args, "Oi", &text_py, &width)) {
        return NULL;
    }
    const char *text = _utf8(text_py, &length);
    if(NULL == text) {
        return NULL;
    }
    int extent = 0;
    size_t used = 0;
    if(!TTF_MeasureString(font, text, length, width, &extent, &used)) {
        return _raise();
    }
    Py_ssize_t count = 0;
    for(size_t idx = 0; idx < used && idx < length; ++idx) {
        count += 0x80 != ((unsigned char)text[idx] & 0xC0);  // skip continuation bytes
    }
    return Py_BuildValue("(in)", extent, count);
}

//=========================================================
// rendering -> Surface
//=========================================================

enum { R_SOLID, R_SHADED, R_BLENDED, R_LCD };

// Render{Solid,Shaded,Blended,LCD}(text, fg[, bg], wrap_length=None). With a
// wrap_length the *_Wrapped variant runs (0 = break only at newlines).
static PyObject * _render_text(PySDL_Font *self, PyObject *args, PyObject *kwds, int kind) {
    PyObject *text_py, *fg_py, *bg_py = NULL, *wrap_py = Py_None;
    int with_bg = (R_SHADED == kind || R_LCD == kind);
    static char *kw_fg[] = {"text", "fg", "wrap_length", NULL};
    static char *kw_bg[] = {"text", "fg", "bg", "wrap_length", NULL};

    TTF_Font *font = _font(self);
    if(NULL == font) {
        return NULL;
    }
    int ok = with_bg
        ? PyArg_ParseTupleAndKeywords(args, kwds, "OOO|O", kw_bg, &text_py, &fg_py, &bg_py, &wrap_py)
        : PyArg_ParseTupleAndKeywords(args, kwds, "OO|O", kw_fg, &text_py, &fg_py, &wrap_py);
    if(!ok) {
        return NULL;
    }

    size_t length;
    SDL_Color fg, bg = {0, 0, 0, 0};
    const char *text = _utf8(text_py, &length);
    if(NULL == text || !PyToColor(fg_py, &fg) || (with_bg && !PyToColor(bg_py, &bg))) {
        return NULL;
    }

    SDL_Surface *surface = NULL;
    if(wrap_py == Py_None) {
        switch(kind) {
        case R_SOLID:   surface = TTF_RenderText_Solid(font, text, length, fg);       break;
        case R_SHADED:  surface = TTF_RenderText_Shaded(font, text, length, fg, bg);  break;
        case R_BLENDED: surface = TTF_RenderText_Blended(font, text, length, fg);     break;
        case R_LCD:     surface = TTF_RenderText_LCD(font, text, length, fg, bg);     break;
        }
        return _wrap_surface(surface);
    }

    long wrap = PyLong_AsLong(wrap_py);
    if(-1 == wrap && PyErr_Occurred()) {
        return NULL;
    }
    if(wrap < 0 || wrap > INT_MAX) {
        PyErr_SetString(PyExc_ValueError, "wrap_length must be >= 0");
        return NULL;
    }
    switch(kind) {
    case R_SOLID:   surface = TTF_RenderText_Solid_Wrapped(font, text, length, fg, (int)wrap);       break;
    case R_SHADED:  surface = TTF_RenderText_Shaded_Wrapped(font, text, length, fg, bg, (int)wrap);  break;
    case R_BLENDED: surface = TTF_RenderText_Blended_Wrapped(font, text, length, fg, (int)wrap);     break;
    case R_LCD:     surface = TTF_RenderText_LCD_Wrapped(font, text, length, fg, bg, (int)wrap);     break;
    }
    return _wrap_surface(surface);
}

static PyObject * PySDL_Font_RenderSolid(PySDL_Font *self, PyObject *args, PyObject *kwds) {
    return _render_text(self, args, kwds, R_SOLID);
}

static PyObject * PySDL_Font_RenderShaded(PySDL_Font *self, PyObject *args, PyObject *kwds) {
    return _render_text(self, args, kwds, R_SHADED);
}

static PyObject * PySDL_Font_RenderBlended(PySDL_Font *self, PyObject *args, PyObject *kwds) {
    return _render_text(self, args, kwds, R_BLENDED);
}

static PyObject * PySDL_Font_RenderLCD(PySDL_Font *self, PyObject *args, PyObject *kwds) {
    return _render_text(self, args, kwds, R_LCD);
}

// RenderGlyph{Solid,Shaded,Blended,LCD}(ch, fg[, bg])
static PyObject * _render_glyph(PySDL_Font *self, PyObject *args, int kind) {
    PyObject *ch_py, *fg_py, *bg_py = NULL;
    int with_bg = (R_SHADED == kind || R_LCD == kind);
    TTF_Font *font = _font(self);
    if(NULL == font) {
        return NULL;
    }
    int ok = with_bg ? PyArg_ParseTuple(args, "OOO", &ch_py, &fg_py, &bg_py)
                     : PyArg_ParseTuple(args, "OO", &ch_py, &fg_py);
    if(!ok) {
        return NULL;
    }
    Uint32 ch;
    SDL_Color fg, bg = {0, 0, 0, 0};
    if(!_codepoint(ch_py, &ch) || !PyToColor(fg_py, &fg) || (with_bg && !PyToColor(bg_py, &bg))) {
        return NULL;
    }

    SDL_Surface *surface = NULL;
    switch(kind) {
    case R_SOLID:   surface = TTF_RenderGlyph_Solid(font, ch, fg);       break;
    case R_SHADED:  surface = TTF_RenderGlyph_Shaded(font, ch, fg, bg);  break;
    case R_BLENDED: surface = TTF_RenderGlyph_Blended(font, ch, fg);     break;
    case R_LCD:     surface = TTF_RenderGlyph_LCD(font, ch, fg, bg);     break;
    }
    return _wrap_surface(surface);
}

static PyObject * PySDL_Font_RenderGlyphSolid(PySDL_Font *self, PyObject *args) {
    return _render_glyph(self, args, R_SOLID);
}

static PyObject * PySDL_Font_RenderGlyphShaded(PySDL_Font *self, PyObject *args) {
    return _render_glyph(self, args, R_SHADED);
}

static PyObject * PySDL_Font_RenderGlyphBlended(PySDL_Font *self, PyObject *args) {
    return _render_glyph(self, args, R_BLENDED);
}

static PyObject * PySDL_Font_RenderGlyphLCD(PySDL_Font *self, PyObject *args) {
    return _render_glyph(self, args, R_LCD);
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_TTF_Init(PyObject *self, PyObject *ign) {
    if(!TTF_Init()) {
        return _raise();
    }
    Py_RETURN_NONE;
}

// TTF_Init / TTF_Quit are counted; the last TTF_Quit invalidates open fonts.
static PyObject * PySDL_TTF_Quit(PyObject *self, PyObject *ign) {
    if(TTF_WasInit() > 0) {
        TTF_Quit();
        if(0 == TTF_WasInit()) {
            ++_session;
        }
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_TTF_WasInit(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(TTF_WasInit());
}

// TTF_Version() -> (major, minor, micro) of the linked SDL_ttf
static PyObject * PySDL_TTF_Version(PyObject *self, PyObject *ign) {
    int version = TTF_Version();
    return Py_BuildValue("(iii)", SDL_VERSIONNUM_MAJOR(version),
        SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version));
}

static PyObject * PySDL_TTF_GetFreeTypeVersion(PyObject *self, PyObject *ign) {
    int major = 0, minor = 0, patch = 0;
    TTF_GetFreeTypeVersion(&major, &minor, &patch);
    return Py_BuildValue("(iii)", major, minor, patch);
}

// (0, 0, 0) when SDL_ttf was built without HarfBuzz.
static PyObject * PySDL_TTF_GetHarfBuzzVersion(PyObject *self, PyObject *ign) {
    int major = 0, minor = 0, patch = 0;
    TTF_GetHarfBuzzVersion(&major, &minor, &patch);
    return Py_BuildValue("(iii)", major, minor, patch);
}

// TTF_StringToTag("Latn") -> int
static PyObject * PySDL_TTF_StringToTag(PyObject *self, PyObject *arg) {
    Uint32 tag;
    if(!PyUnicode_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "expected a str tag, e.g. 'Latn'");
        return NULL;
    }
    if(!_tag(arg, &tag)) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(tag);
}

// TTF_TagToString(int) -> "Latn"
static PyObject * PySDL_TTF_TagToString(PyObject *self, PyObject *arg) {
    Uint32 tag;
    if(!PyLong_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "expected an int tag");
        return NULL;
    }
    if(!_tag(arg, &tag)) {
        return NULL;
    }
    return _tag_to_str(tag);
}

// TTF_GetGlyphScript(ch) -> the ISO 15924 script tag of a character, e.g. "Latn"
static PyObject * PySDL_TTF_GetGlyphScript(PyObject *self, PyObject *arg) {
    Uint32 ch;
    if(!_codepoint(arg, &ch)) {
        return NULL;
    }
    Uint32 script = TTF_GetGlyphScript(ch);
    if(0 == script) {
        return _raise();
    }
    return _tag_to_str(script);
}

//=========================================================
// constants
//=========================================================

int PySDL_TTF_Constants(PyObject *module) {
    static const struct { const char *name; long value; } constants[] = {
        { "TTF_STYLE_NORMAL",            TTF_STYLE_NORMAL            },
        { "TTF_STYLE_BOLD",              TTF_STYLE_BOLD              },
        { "TTF_STYLE_ITALIC",            TTF_STYLE_ITALIC            },
        { "TTF_STYLE_UNDERLINE",         TTF_STYLE_UNDERLINE         },
        { "TTF_STYLE_STRIKETHROUGH",     TTF_STYLE_STRIKETHROUGH     },

        { "TTF_HINTING_NORMAL",          TTF_HINTING_NORMAL          },
        { "TTF_HINTING_LIGHT",           TTF_HINTING_LIGHT           },
        { "TTF_HINTING_MONO",            TTF_HINTING_MONO            },
        { "TTF_HINTING_NONE",            TTF_HINTING_NONE            },
        { "TTF_HINTING_LIGHT_SUBPIXEL",  TTF_HINTING_LIGHT_SUBPIXEL  },

        { "TTF_HORIZONTAL_ALIGN_LEFT",   TTF_HORIZONTAL_ALIGN_LEFT   },
        { "TTF_HORIZONTAL_ALIGN_CENTER", TTF_HORIZONTAL_ALIGN_CENTER },
        { "TTF_HORIZONTAL_ALIGN_RIGHT",  TTF_HORIZONTAL_ALIGN_RIGHT  },
        { "TTF_WRAPPED_ALIGN_LEFT",      TTF_HORIZONTAL_ALIGN_LEFT   },  // pysdl2 names
        { "TTF_WRAPPED_ALIGN_CENTER",    TTF_HORIZONTAL_ALIGN_CENTER },
        { "TTF_WRAPPED_ALIGN_RIGHT",     TTF_HORIZONTAL_ALIGN_RIGHT  },

        { "TTF_DIRECTION_INVALID",       TTF_DIRECTION_INVALID       },
        { "TTF_DIRECTION_LTR",           TTF_DIRECTION_LTR           },
        { "TTF_DIRECTION_RTL",           TTF_DIRECTION_RTL           },
        { "TTF_DIRECTION_TTB",           TTF_DIRECTION_TTB           },
        { "TTF_DIRECTION_BTT",           TTF_DIRECTION_BTT           },

#if SDL_TTF_VERSION_ATLEAST(3,2,2)
        { "TTF_FONT_WEIGHT_THIN",        TTF_FONT_WEIGHT_THIN        },
        { "TTF_FONT_WEIGHT_EXTRA_LIGHT", TTF_FONT_WEIGHT_EXTRA_LIGHT },
        { "TTF_FONT_WEIGHT_LIGHT",       TTF_FONT_WEIGHT_LIGHT       },
        { "TTF_FONT_WEIGHT_NORMAL",      TTF_FONT_WEIGHT_NORMAL      },
        { "TTF_FONT_WEIGHT_MEDIUM",      TTF_FONT_WEIGHT_MEDIUM      },
        { "TTF_FONT_WEIGHT_SEMI_BOLD",   TTF_FONT_WEIGHT_SEMI_BOLD   },
        { "TTF_FONT_WEIGHT_BOLD",        TTF_FONT_WEIGHT_BOLD        },
        { "TTF_FONT_WEIGHT_EXTRA_BOLD",  TTF_FONT_WEIGHT_EXTRA_BOLD  },
        { "TTF_FONT_WEIGHT_BLACK",       TTF_FONT_WEIGHT_BLACK       },
        { "TTF_FONT_WEIGHT_EXTRA_BLACK", TTF_FONT_WEIGHT_EXTRA_BLACK },
#endif
    };
    for(size_t idx = 0; idx < SDL_arraysize(constants); ++idx) {
        if(0 > PyModule_AddIntConstant(module, constants[idx].name, constants[idx].value)) {
            return -1;
        }
    }
    return 0;
}

#endif // PYSDL_HAVE_TTF
