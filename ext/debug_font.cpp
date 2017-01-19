
#include <QtOpenGL/qgl.h>
#include <assert.h>
#include <string>
#include <vector>
#include "qopenglerrorcheck.h"
#include "debug_font.h"

#define TL fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

float kDebugFontSizeW = 8;	// default
float kDebugFontSizeH = 16;

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

static bool changed = true;
static void _AppendMessage(const char* fmt, va_list args)
{
  char *message = NULL;
  vasprintf(&message, fmt, args);
  if (message)
  {
	  messages.push_back( message ); if (max_lines>0 && messages.size()>max_lines) {
	    messages.erase(messages.begin());
	    changed = true;
	  }
  }
}

static const int kTextVertBuf = 2048;
#ifndef DEBUG_FG_BG
static struct {
    pt_t v1, t1;
    pt_t v2, t2;
    pt_t v3, t3;
    pt_t v4, t4;
    pt_t v5, t5;
    pt_t v6, t6;
} text[kTextVertBuf];
#else
static struct {
    pt_t v1; union { pt_t t1b; pt_t t1; }; union { pt_t t1f; pt_t r1; }; union { uint32_t c1b; uint32_t c1; }; union { uint32_t c1f; uint32_t a1; };
    pt_t v2; union { pt_t t2b; pt_t t2; }; union { pt_t t2f; pt_t r2; }; union { uint32_t c2b; uint32_t c2; }; union { uint32_t c2f; uint32_t a2; };
    pt_t v3; union { pt_t t3b; pt_t t3; }; union { pt_t t3f; pt_t r3; }; union { uint32_t c3b; uint32_t c3; }; union { uint32_t c3f; uint32_t a3; };
    pt_t v4; union { pt_t t4b; pt_t t4; }; union { pt_t t4f; pt_t r4; }; union { uint32_t c4b; uint32_t c4; }; union { uint32_t c4f; uint32_t a4; };
    pt_t v5; union { pt_t t5b; pt_t t5; }; union { pt_t t5f; pt_t r5; }; union { uint32_t c5b; uint32_t c5; }; union { uint32_t c5f; uint32_t a5; };
    pt_t v6; union { pt_t t6b; pt_t t6; }; union { pt_t t6f; pt_t r6; }; union { uint32_t c6b; uint32_t c6; }; union { uint32_t c6f; uint32_t a6; };
} text[kTextVertBuf];
#endif

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
	int base = 0; \
	for(unsigned char *p = (unsigned char*)string; *p; ++p) { \
	    if (*p == 0xff) \
	        base = base^0xff; /* inv on/off */ \
	    else { \
	        const int chr = *p+base; \
	        text[cnt].v1 = xx + v0; text[cnt].t1 = chars[chr].t[0]; \
	        text[cnt].v2 = xx + v1; text[cnt].t2 = chars[chr].t[1]; \
	        text[cnt].v3 = xx + v2; text[cnt].t3 = chars[chr].t[2]; \
	        text[cnt].v4 = xx + v2; text[cnt].t4 = chars[chr].t[2]; \
	        text[cnt].v5 = xx + v1; text[cnt].t5 = chars[chr].t[1]; \
	        text[cnt].v6 = xx + v3; text[cnt].t6 = chars[chr].t[3]; \
	        ++cnt; xx.x += space; \
        	if (xx.x >= scwidth) break; /* out of the screen */ \
	    } \
	}

void dbgFlush()
{
	int data[4];
	glGetIntegerv(GL_VIEWPORT, data);

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

    int cnt = 0; pt_t xx = {0, 0}; for(uint s=0; s<messages.size(); ++s) {
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

	glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, debugtexturefont);	// select our font texture
    glDisable(GL_DEPTH_TEST);						// disables depth testing
    glEnable(GL_BLEND); glBlendEquation(GL_MIN);
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
    
	glTexCoordPointer(2, GL_FLOAT, 2*sizeof(pt_t), &text->t1);
	glVertexPointer(2, GL_FLOAT, 2*sizeof(pt_t), &text->v1);
	glDrawArrays(GL_TRIANGLES, 0, cnt*6);           // 2 triangles per char

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glMatrixMode(GL_PROJECTION);					// select the projection matrix
    glPopMatrix();                                  // restore the old projection matrix
    glMatrixMode(GL_MODELVIEW);                     // select the modelview matrix
    glPopMatrix();                                  // restore the old projection matrix
    glDisable(GL_BLEND); glBlendEquation(GL_FUNC_ADD);// restore default blend
    glEnable(GL_DEPTH_TEST);						// enables depth testing
}

void dbgAppendMessage(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _AppendMessage(fmt, args);
    va_end(args);
}

void dbgSetStatusLine(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
  	char *message = NULL;
  	vasprintf(&message, fmt, args);
  	if (message) status = message;
    va_end(args);
}

#include "debug_font.txt"
