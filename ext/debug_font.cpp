
#include <QtOpenGL/qgl.h>
#include <assert.h>
#include <string>
#include <vector>
#include "qopenglerrorcheck.h"
#include "debug_font.h"

#define TL fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

static float kDebugFontSizeW = 8;	// default
static float kDebugFontSizeH = 16;

typedef struct {
	const char *image;
	const unsigned char *pixels;
	int size, width, height, channels;
} EmbedImageItem;

extern const unsigned char embed_1[], embed_2[];
const EmbedImageItem embed_debug_font[] = {
	{"dos-8x16.bmp", embed_1, 196608, 128, 512, GL_RGB},
	{"dos-8x8.bmp", embed_2, 98304, 128, 256, GL_RGB},
	{NULL, NULL, 0, 0, 0, 0}
};


typedef struct {
    float x, y;
} pt_t;
#define pt(a, b) (pt_t){a, b}
inline static pt_t operator + (const pt_t& a, const float b) { return pt(a.x + b, a.y + b); }
inline static pt_t operator + (const float a, const pt_t& b) { return pt(a + b.x, a + b.y); }
inline static pt_t operator + (const pt_t& a, const pt_t& b) { return pt(a.x + b.x, a.y + b.y); }
inline static pt_t operator * (const pt_t& a, const float b) { return pt(a.x * b, a.y * b); }
inline static pt_t operator * (const float a, const pt_t& b) { return pt(a * b.x, a * b.y); }
inline static pt_t operator * (const pt_t& a, const pt_t& b) { return pt(a.x * b.x, a.y * b.y); }

static GLuint debugtexturefont = 0;
static std::vector<std::string> messages; // text buffer
static std::string status; // status line
static unsigned int max_lines = 0;

static struct _char_info_t {
    pt_t v[4], t[4];
} chars[512];

void dbgLoadFont()
{
	static bool init = false;
	
	if (!init) {

		init = true;
	    
	    glGenTextures(1, &debugtexturefont);
	    glBindTexture(GL_TEXTURE_2D, debugtexturefont);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	    GLCHK( glTexImage2D(GL_TEXTURE_2D, 0, embed_debug_font[0].channels, embed_debug_font[0].width, embed_debug_font[0].height, 0, embed_debug_font[0].channels, GL_UNSIGNED_BYTE, embed_debug_font[0].pixels) );
	
	    /* 16x32 chars */ for (int loop=0; loop<512; ++loop) // Loop Through All 512 Chars
	    {
	        float cx=(float)(loop%16)/16.0f;        // X Position Of Current Character
	        float cy=(float)(loop/16)/32.0f;        // Y Position Of Current Character
	        
	        const int col = loop%16, row = loop/16, ch = row*16+col;
	        
	        chars[ch].t[0] = pt(cx,          cy); /* 0, 0 */
	        chars[ch].t[1] = pt(cx,          cy+0.03125f); /* 0, 1 */
	        chars[ch].t[2] = pt(cx+0.0625f,  cy); /* 1, 0 */
	        chars[ch].t[3] = pt(cx+0.0625f,  cy+0.03125f); /* 1, 1 */
	    }
	
	    messages.push_back(BOX_DDR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DDL);
	    messages.push_back(BOX_DUD "   Debug console             " BOX_DUD);
	    messages.push_back(BOX_DUD "   KomSoft Oprogramowanie    " BOX_DUD);
	    messages.push_back(BOX_DUR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DLR BOX_DUL);
	    messages.push_back(" \020 Hello!");
	#ifdef DEBUG
	    messages.push_back(" \020 DEBUG build");
	#endif
	}
}

static float pixelScale = 1;
void dbgSetPixelRatio(float scale)
{
    pixelScale = scale;
}

static bool inverted = false;
void dbgSetInvert(bool inv)
{
    inverted = inv;
}
void dbgToggleInvert()
{
    inverted = !inverted;
}

static bool changed = true;
static int _AppendMessage(const char* fmt, va_list args)
{
  char *message = NULL;
  vasprintf(&message, fmt, args);
  if (message)
  {
	  messages.push_back( message ); if (max_lines>0 && messages.size()>max_lines) {
	    messages.erase(messages.begin());
	    changed = true;
	  }
	  return messages.size() - 1;
  }
  return -1;
}

static void _UpdateMessage(int line, const char* fmt, va_list args)
{
  char *message = NULL;
  vasprintf(&message, fmt, args);
  if (message && messages.size()>line)
  {
    messages[line].assign(message);
    changed = true;
  }
}

static uint32_t text_colors[16] = {
    /* BLACK        */ 0x00000000,
    /* BLUE         */ 0xFFFF0000,
    /* GREEN        */ 0xFF00FF00,
    /* CYAN         */ 0xFFFFFF00,
    /* RED          */ 0xFF0000FF,
    /* MAGENTA      */ 0xFFFF00FF,
    /* BROWN        */ 0xFF2A2AA5,
    /* LIGHTGRAY    */ 0xFFD3D3D3,
    /* DARKGRAY     */ 0xFFA9A9A9,
    /* LIGHTBLUE    */ 0xFFE6D8AD,
    /* LIGHTGREEN   */ 0xFF90EE90,
    /* LIGHTCYAN    */ 0xFFFFFFE0,
    /* LIGHTRED     */ 0xFFCBCCFF,
    /* LIGHTMAGENTA */ 0xFFF942FF,
    /* YELLOW       */ 0xFF0FFEFF,
    /* WHITE        */ 0xFFFFFFFF
};

#define _flushLine() \
    if(text[rev].cnt < kTextVertBuf) { \
        int base = inverted?0x100:0; \
        for(unsigned char *p = (unsigned char*)string; *p; ++p) { \
            if (*p == 0xff) \
                base = base^0x100; /* inv on/off */ \
            else { \
                const int chr = *p+base; \
                text[rev]().v1 = xx + v0; text[rev]().t1 = chars[chr].t[0]; \
                text[rev]().v2 = xx + v1; text[rev]().t2 = chars[chr].t[1]; \
                text[rev]().v3 = xx + v2; text[rev]().t3 = chars[chr].t[2]; \
                text[rev]().v4 = xx + v2; text[rev]().t4 = chars[chr].t[2]; \
                text[rev]().v5 = xx + v1; text[rev]().t5 = chars[chr].t[1]; \
                text[rev]().v6 = xx + v3; text[rev]().t6 = chars[chr].t[3]; \
                ++text[rev]; xx.x += space; \
                if (xx.x >= scwidth) break; /* out of the screen */ \
                if(text[rev].cnt == kTextVertBuf) break; /* out of buffer */ \
            } \
        } \
    }

void dbgFlush()
{
	int data[4];
	glGetIntegerv(GL_VIEWPORT, data);

    data[2] /= pixelScale;
    data[3] /= pixelScale;
    
	const int rect_w = data[2] - data[2]%int(kDebugFontSizeW);
	const int rect_h = data[3] - data[3]%int(kDebugFontSizeH);

	max_lines = rect_h / kDebugFontSizeH;

    const int scwidth = rect_w, scheight = rect_h-status.empty()*kDebugFontSizeH;
    const static int space = kDebugFontSizeW;  // odleglosc miedzy pisanymi znakami
    // char corners:
    static const pt_t v0 = pt(0, 0);                             /* 0, 0 */
    static const pt_t v1 = pt(0, kDebugFontSizeH);               /* 0, 1 */
    static const pt_t v2 = pt(kDebugFontSizeW, 0);               /* 1, 0 */
    static const pt_t v3 = pt(kDebugFontSizeW, kDebugFontSizeH); /* 1, 1 */

    static const int kTextVertBuf = 2048;
    #ifndef DEBUG_FG_BG
    static struct {
        int cnt;
        struct text_vert_t {
            pt_t v1, t1;
            pt_t v2, t2;
            pt_t v3, t3;
            pt_t v4, t4;
            pt_t v5, t5;
            pt_t v6, t6;
        } buf[kTextVertBuf];
        text_vert_t & operator =(int v) { cnt = v; return buf[cnt]; }
        text_vert_t & operator ()() { return buf[cnt]; } // reference to current buffer element
        text_vert_t & operator ++() { return buf[++cnt]; }
    } text[2] = {0,{0}};
    #else
    static struct {
        int cnt;
        static struct {
            pt_t v1; union { pt_t t1b; pt_t t1; }; union { pt_t t1f; pt_t r1; }; union { uint32_t c1b; uint32_t c1; }; union { uint32_t c1f; uint32_t a1; };
            pt_t v2; union { pt_t t2b; pt_t t2; }; union { pt_t t2f; pt_t r2; }; union { uint32_t c2b; uint32_t c2; }; union { uint32_t c2f; uint32_t a2; };
            pt_t v3; union { pt_t t3b; pt_t t3; }; union { pt_t t3f; pt_t r3; }; union { uint32_t c3b; uint32_t c3; }; union { uint32_t c3f; uint32_t a3; };
            pt_t v4; union { pt_t t4b; pt_t t4; }; union { pt_t t4f; pt_t r4; }; union { uint32_t c4b; uint32_t c4; }; union { uint32_t c4f; uint32_t a4; };
            pt_t v5; union { pt_t t5b; pt_t t5; }; union { pt_t t5f; pt_t r5; }; union { uint32_t c5b; uint32_t c5; }; union { uint32_t c5f; uint32_t a5; };
            pt_t v6; union { pt_t t6b; pt_t t6; }; union { pt_t t6f; pt_t r6; }; union { uint32_t c6b; uint32_t c6; }; union { uint32_t c6f; uint32_t a6; };
        } buf[kTextVertBuf];
        text_vert_t & operator =(int v) { cnt = v; return buf[cnt]; }
        text_vert_t & operator ()() { return buf[cnt]; } // reference to current buffer element
        text_vert_t & operator ++() { return buf[++cnt]; }
    } text[2] = {0,{0}};
    #endif

    if (changed) {
        text[0] = 0; text[1] = 0; // reset buffers
        int rev = 0; pt_t xx = {0, 0}; for(uint s=0; s<messages.size(); ++s) {
            const char *string = messages[s].c_str();
            _flushLine();
            xx.x = 0; xx.y += kDebugFontSizeH;
            if (xx.y >= scheight) break; // out of the screen
        }
        if (!status.empty())
        {
            const char *string = status.c_str();
            pt_t xx = {0, (max_lines-1)*kDebugFontSizeH}; _flushLine();
        }
    }

	glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, debugtexturefont);	// select our font texture
    glDisable(GL_DEPTH_TEST);						// disables depth testing
    glEnable(GL_BLEND); 
    glBlendEquation(inverted?GL_MAX:GL_MIN);        // font can be black or white
    glMatrixMode(GL_PROJECTION);					// select the Projection matrix
    glPushMatrix();                                 // store the Projection matrix
    glLoadIdentity();								// reset the Projection matrix
    glOrtho(0, rect_w, rect_h, 0, -1, 1);	    	// set up an ortho screen (top-left)
    glMatrixMode(GL_MODELVIEW);                     // select the modelview matrix
    glPushMatrix();                                 // store the modelview matrix
    glLoadIdentity();								// reset the modelview matrix
    
    glUseProgram(0);

    glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
    
    if (text[0].cnt) {
        glTexCoordPointer(2, GL_FLOAT, 2*sizeof(pt_t), &text[0].buf->t1);
        glVertexPointer(2, GL_FLOAT, 2*sizeof(pt_t), &text[0].buf->v1);
        glDrawArrays(GL_TRIANGLES, 0, text[0].cnt*6);// 2 triangles per char
    }
    if (text[1].cnt) {
        glTexCoordPointer(2, GL_FLOAT, 2*sizeof(pt_t), &text[1].buf->t1);
        glVertexPointer(2, GL_FLOAT, 2*sizeof(pt_t), &text[1].buf->v1);
        glDrawArrays(GL_TRIANGLES, 0, text[1].cnt*6);// 2 triangles per char
    }

    glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glMatrixMode(GL_PROJECTION);					// select the projection matrix
    glPopMatrix();                                  // restore the old projection matrix
    glMatrixMode(GL_MODELVIEW);                     // select the modelview matrix
    glPopMatrix();                                  // restore the old projection matrix
    glDisable(GL_BLEND); glBlendEquation(GL_FUNC_ADD);// restore default blend
    glEnable(GL_DEPTH_TEST);						// enables depth testing
}

int dbgAppendMessage(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ln = _AppendMessage(fmt, args);
    va_end(args);
    return ln;
}

void dbgUpdateMessage(int line, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _UpdateMessage(line, fmt, args);
    va_end(args);
}

void dbgSetStatusLine(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
  	char *message = NULL;
  	vasprintf(&message, fmt, args);
  	if (message && strcmp(message, status.c_str())!=0) {
        status = message;
        changed = true;
    }
    va_end(args);
}

#include "debug_font.txt"
