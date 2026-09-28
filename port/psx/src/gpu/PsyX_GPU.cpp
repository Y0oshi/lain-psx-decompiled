#include "PsyX_GPU.h"

#include "PsyX/PsyX_public.h"
#include "PsyX/PsyX_globals.h"
#include "PsyX/PsyX_render.h"

#include "../PsyX_main.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include <unordered_map>
#include <vector>

#define GET_TPAGE_FORMAT(tpage) ((TexFormat)((tpage >> 7) & 0x3))
#define GET_TPAGE_BLEND(tpage)  ((BlendMode)(((tpage >> 5) & 3) + 1))

#define GET_TPAGE_DITHER(tpage) ((tpage >> 9) & 0x1)

#define GET_CLUT_X(clut)        ((clut & 0x3F) << 4)
#define GET_CLUT_Y(clut)        (clut >> 6)

OT_TAG prim_terminator = { (uintptr_t)-1, 0 }; // P_TAG with zero primLength (lain: explicit cast, narrowing is an error on GCC)

DISPENV currentDispEnv;
DISPENV activeDispEnv;
DRAWENV activeDrawEnv;

static const char* currentSplitDebugText = nullptr;
TextureID overrideTexture = 0;
int overrideTextureWidth = 0;
int overrideTextureHeight = 0;

int g_GPUDisabledState = 0;
int g_DrawPrimMode = 0;

struct GPUDrawSplit
{
	DRAWENV			drawenv;
	DISPENV			dispenv;

	BlendMode		blendMode;

	TexFormat		texFormat;
	TextureID		textureId;

	int				drawPrimMode;

	u_short			startVertex;
	u_short			numVerts;

	const char*		debugText;
};

#define MAX_DRAW_SPLITS	 4096

GrVertex g_vertexBuffer[MAX_VERTEX_BUFFER_SIZE];
GPUDrawSplit g_splits[MAX_DRAW_SPLITS];

int g_vertexIndex = 0;
int g_splitIndex = 0;

static void LainInterp_ClearPending();

void ClearSplits()
{
	LainInterp_ClearPending(); // lain
	currentSplitDebugText = nullptr;
	g_vertexIndex = 0;
	g_splitIndex = 0;
	g_splits[0].texFormat = (TexFormat)0xFFFF;
}

template<class T>
void DrawEnvDimensions(T& width, T& height)
{
	if (activeDrawEnv.dfe)
	{
		width = activeDispEnv.disp.w;
		height = activeDispEnv.disp.h;
	}
	else
	{
		width = activeDrawEnv.clip.w;
		height = activeDrawEnv.clip.h;
	}
}

void DrawEnvOffset(float& ofsX, float& ofsY)
{
	if (activeDrawEnv.dfe)
	{
		// also make offset in draw dimensions range to prevent flicker
		const int x = activeDispEnv.disp.x;
		const int y = activeDispEnv.disp.y;
		ofsX = activeDrawEnv.ofs[0] - activeDispEnv.disp.x;
		ofsY = activeDrawEnv.ofs[1] - activeDispEnv.disp.y;
	}
	else
	{
		ofsX = 0.0f;
		ofsY = 0.0f;
	}
}

// remaps screen coordinates to [0..1]
// without clamping
inline void ScreenCoordsToEmulator(GrVertex* vertex, int count)
{
#if USE_PGXP
	float w, h;
	DrawEnvDimensions(w, h);

	while (count--)
	{
		vertex[count].x = vertex[count].x / w - 0.5f;
		vertex[count].y = vertex[count].y / h - 0.5f;
	}
#endif
}

void LineSwapSourceVerts(VERTTYPE*& p0, VERTTYPE*& p1, unsigned char*& c0, unsigned char*& c1)
{
	// swap line coordinates for left-to-right and up-to-bottom direction
	if ((p0[0] > p1[0]) ||
		(p0[1] > p1[1] && p0[0] == p1[0]))
	{
		VERTTYPE* tmp = p0;
		p0 = p1;
		p1 = tmp;

		unsigned char* tmpCol = c0;
		c0 = c1;
		c1 = tmpCol;
	}
}

// lain: the PS1 GPU only uses the low 11 bits of vertex coordinates (signed,
// -1024..1023) and games rely on the wrap-around (Lain's zooming item grid
// computes x = 0x98 + a multiple of 2048).
#if USE_PGXP
#define GPU_XY(v) (v)
#else
#define GPU_XY(v) ((float)(((int)(v) << 21) >> 21))
#endif

void MakeLineArray(GrVertex* vertex, VERTTYPE* p0, VERTTYPE* p1, ushort gteidx)
{
	const VERTTYPE dx = p1[0] - p0[0];
	const VERTTYPE dy = p1[1] - p0[1];

	float ofsX, ofsY;
	DrawEnvOffset(ofsX, ofsY);

	memset(vertex, 0, sizeof(GrVertex) * 4);

	if (dx > abs((short)dy)) 
	{ // horizontal
		vertex[0].x = GPU_XY(p0[0]) + ofsX;
		vertex[0].y = GPU_XY(p0[1]) + ofsY;

		vertex[1].x = GPU_XY(p1[0]) + ofsX + 1;
		vertex[1].y = GPU_XY(p1[1]) + ofsY;

		vertex[2].x = vertex[1].x;
		vertex[2].y = vertex[1].y + 1;

		vertex[3].x = vertex[0].x;
		vertex[3].y = vertex[0].y + 1;
	}
	else 
	{ // vertical
		vertex[0].x = GPU_XY(p0[0]) + ofsX;
		vertex[0].y = GPU_XY(p0[1]) + ofsY;

		vertex[1].x = GPU_XY(p1[0]) + ofsX;
		vertex[1].y = GPU_XY(p1[1]) + ofsY + 1;

		vertex[2].x = vertex[1].x + 1;
		vertex[2].y = vertex[1].y;

		vertex[3].x = vertex[0].x + 1;
		vertex[3].y = vertex[0].y;
	} // TODO diagonal line alignment

#if USE_PGXP
	vertex[0].scr_h = vertex[1].scr_h = vertex[2].scr_h = vertex[3].scr_h = 0.0f;
#endif

	ScreenCoordsToEmulator(vertex, 4);
}

inline void ApplyVertexPGXP(GrVertex* v, VERTTYPE* p, float ofsX, float ofsY, ushort gteidx, int lookupOfs)
{
#if USE_PGXP
	uint lookup = PGXP_LOOKUP_VALUE(p[0], p[1]);

	PGXPVData vd;
	if (gteidx != 0xffff &&
		g_cfg_pgxpTextureCorrection && 
		PGXP_GetCacheData(&vd, lookup, gteidx + lookupOfs))
	{
		v->x = vd.px;
		v->y = vd.py;
		v->z = vd.pz;

		// calculate offset for our perspective matrix based on supposed GTE transformed geometry offset
		float dispW, dispH;
		DrawEnvDimensions(dispW, dispH);

		const float gteOfsX = fmodf(vd.ofx, dispW) - dispW * 0.5f;
		const float gteOfsY = fmodf(vd.ofy, dispH) - dispH * 0.5f;

		v->ofsX = (ofsX + gteOfsX) / dispW * 2.0f;
		v->ofsY = (ofsY + gteOfsY) / dispH * 2.0f;
		v->scr_h = vd.scr_h;
	}
	else
	{
		v->scr_h = 0.0f;
		v->z = 0.0f;
	}
#endif
}

void MakeVertexTriangle(GrVertex* vertex, VERTTYPE* p0, VERTTYPE* p1, VERTTYPE* p2, ushort gteidx)
{
	assert(p0);
	assert(p1);
	assert(p2);

	float ofsX, ofsY;
	DrawEnvOffset(ofsX, ofsY);

	memset(vertex, 0, sizeof(GrVertex) * 3);

	vertex[0].x = GPU_XY(p0[0]) + ofsX;
	vertex[0].y = GPU_XY(p0[1]) + ofsY;

	vertex[1].x = GPU_XY(p1[0]) + ofsX;
	vertex[1].y = GPU_XY(p1[1]) + ofsY;

	vertex[2].x = GPU_XY(p2[0]) + ofsX;
	vertex[2].y = GPU_XY(p2[1]) + ofsY;

	ApplyVertexPGXP(&vertex[0], p0, ofsX, ofsY, gteidx, -2);
	ApplyVertexPGXP(&vertex[1], p1, ofsX, ofsY, gteidx, -1);
	ApplyVertexPGXP(&vertex[2], p2, ofsX, ofsY, gteidx, 0);

	ScreenCoordsToEmulator(vertex, 3);
}

void MakeVertexQuad(GrVertex* vertex, VERTTYPE* p0, VERTTYPE* p1, VERTTYPE* p2, VERTTYPE* p3, ushort gteidx)
{
	assert(p0);
	assert(p1);
	assert(p2);
	assert(p3);

	float ofsX, ofsY;
	DrawEnvOffset(ofsX, ofsY);

	memset(vertex, 0, sizeof(GrVertex) * 4);

	vertex[0].x = GPU_XY(p0[0]) + ofsX;
	vertex[0].y = GPU_XY(p0[1]) + ofsY;

	vertex[1].x = GPU_XY(p1[0]) + ofsX;
	vertex[1].y = GPU_XY(p1[1]) + ofsY;

	vertex[2].x = GPU_XY(p2[0]) + ofsX;
	vertex[2].y = GPU_XY(p2[1]) + ofsY;

	vertex[3].x = GPU_XY(p3[0]) + ofsX;
	vertex[3].y = GPU_XY(p3[1]) + ofsY;

	ApplyVertexPGXP(&vertex[0], p0, ofsX, ofsY, gteidx, -3);
	ApplyVertexPGXP(&vertex[1], p1, ofsX, ofsY, gteidx, -2);
	ApplyVertexPGXP(&vertex[2], p2, ofsX, ofsY, gteidx, -1);
	ApplyVertexPGXP(&vertex[3], p3, ofsX, ofsY, gteidx, 0);

	ScreenCoordsToEmulator(vertex, 4);
}

void MakeVertexRect(GrVertex* vertex, VERTTYPE* p0, short w, short h, ushort gteidx)
{
	assert(p0);

	float ofsX, ofsY;
	DrawEnvOffset(ofsX, ofsY);

	memset(vertex, 0, sizeof(GrVertex) * 4);

	vertex[0].x = GPU_XY(p0[0]) + ofsX;
	vertex[0].y = GPU_XY(p0[1]) + ofsY;

	vertex[1].x = vertex[0].x;
	vertex[1].y = vertex[0].y + h;

	vertex[2].x = vertex[0].x + w;
	vertex[2].y = vertex[0].y + h;

	vertex[3].x = vertex[0].x + w;
	vertex[3].y = vertex[0].y;

#if USE_PGXP
	vertex[0].scr_h = vertex[1].scr_h = vertex[2].scr_h = vertex[3].scr_h = 0.0f;
#endif

	ScreenCoordsToEmulator(vertex, 4);
}

void MakeTexcoordQuad(GrVertex* vertex, unsigned char* uv0, unsigned char* uv1, unsigned char* uv2, unsigned char* uv3, short page, short clut, unsigned char dither)
{
	assert(uv0);
	assert(uv1);
	assert(uv2);
	assert(uv3);

	const unsigned char bright = 2;

	vertex[0].u = uv0[0];
	vertex[0].v = uv0[1];
	vertex[0].bright = bright;
	vertex[0].dither = dither;
	vertex[0].page = page;
	vertex[0].clut = clut;

	vertex[1].u = uv1[0];
	vertex[1].v = uv1[1];
	vertex[1].bright = bright;
	vertex[1].dither = dither;
	vertex[1].page = page;
	vertex[1].clut = clut;

	vertex[2].u = uv2[0];
	vertex[2].v = uv2[1];
	vertex[2].bright = bright;
	vertex[2].dither = dither;
	vertex[2].page = page;
	vertex[2].clut = clut;

	vertex[3].u = uv3[0];
	vertex[3].v = uv3[1];
	vertex[3].bright = bright;
	vertex[3].dither = dither;
	vertex[3].page = page;
	vertex[3].clut = clut;
	/*
	if (g_cfg_bilinearFiltering)
	{
		vertex[0].tcx = -1;
		vertex[0].tcy = -1;

		vertex[1].tcx = -1;
		vertex[1].tcy = -1;

		vertex[2].tcx = -1;
		vertex[2].tcy = -1;

		vertex[3].tcx = -1;
		vertex[3].tcy = -1;
	}*/
}

void MakeTexcoordTriangle(GrVertex* vertex, unsigned char* uv0, unsigned char* uv1, unsigned char* uv2, short page, short clut, unsigned char dither)
{
	assert(uv0);
	assert(uv1);
	assert(uv2);

	const unsigned char bright = 2;

	vertex[0].u = uv0[0];
	vertex[0].v = uv0[1];
	vertex[0].bright = bright;
	vertex[0].dither = dither;
	vertex[0].page = page;
	vertex[0].clut = clut;

	vertex[1].u = uv1[0];
	vertex[1].v = uv1[1];
	vertex[1].bright = bright;
	vertex[1].dither = dither;
	vertex[1].page = page;
	vertex[1].clut = clut;

	vertex[2].u = uv2[0];
	vertex[2].v = uv2[1];
	vertex[2].bright = bright;
	vertex[2].dither = dither;
	vertex[2].page = page;
	vertex[2].clut = clut;
	/*
	if (g_cfg_bilinearFiltering)
	{
		vertex[0].tcx = -1;
		vertex[0].tcy = -1;

		vertex[1].tcx = -1;
		vertex[1].tcy = -1;

		vertex[2].tcx = -1;
		vertex[2].tcy = -1;

		vertex[3].tcx = -1;
		vertex[3].tcy = -1;
	}*/
}

void MakeTexcoordRect(GrVertex* vertex, unsigned char* uv, short page, short clut, short w, short h)
{
	assert(uv);

	// sim overflow
	if (int(uv[0]) + w > 255) w = 255 - uv[0];
	if (int(uv[1]) + h > 255) h = 255 - uv[1];

	const unsigned char bright = 2;
	const unsigned char dither = 0;

	vertex[0].u = uv[0];
	vertex[0].v = uv[1];
	vertex[0].bright = bright;
	vertex[0].dither = dither;
	vertex[0].page = page;
	vertex[0].clut = clut;

	vertex[1].u = uv[0];
	vertex[1].v = uv[1] + h;
	vertex[1].bright = bright;
	vertex[1].dither = dither;
	vertex[1].page = page;
	vertex[1].clut = clut;

	vertex[2].u = uv[0] + w;
	vertex[2].v = uv[1] + h;
	vertex[2].bright = bright;
	vertex[2].dither = dither;
	vertex[2].page = page;
	vertex[2].clut = clut;

	vertex[3].u = uv[0] + w;
	vertex[3].v = uv[1];
	vertex[3].bright = bright;
	vertex[3].dither = dither;
	vertex[3].page = page;
	vertex[3].clut = clut;

	if (g_cfg_bilinearFiltering)
	{
		vertex[0].tcx = -1;
		vertex[0].tcy = -1;

		vertex[1].tcx = -1;
		vertex[1].tcy = -1;

		vertex[2].tcx = -1;
		vertex[2].tcy = -1;

		vertex[3].tcx = -1;
		vertex[3].tcy = -1;
	}
}

void MakeTexcoordLineZero(GrVertex* vertex, unsigned char dither)
{
	const unsigned char bright = 1;

	vertex[0].u = 0;
	vertex[0].v = 0;
	vertex[0].bright = bright;
	vertex[0].dither = dither;
	vertex[0].page = 0;
	vertex[0].clut = 0;

	vertex[1].u = 0;
	vertex[1].v = 0;
	vertex[1].bright = bright;
	vertex[1].dither = dither;
	vertex[1].page = 0;
	vertex[1].clut = 0;

	vertex[2].u = 0;
	vertex[2].v = 0;
	vertex[2].bright = bright;
	vertex[2].dither = dither;
	vertex[2].page = 0;
	vertex[2].clut = 0;

	vertex[3].u = 0;
	vertex[3].v = 0;
	vertex[3].bright = bright;
	vertex[3].dither = dither;
	vertex[3].page = 0;
	vertex[3].clut = 0;
}

void MakeTexcoordTriangleZero(GrVertex* vertex, unsigned char dither)
{
	const unsigned char bright = 1;

	vertex[0].u = 0;
	vertex[0].v = 0;
	vertex[0].bright = bright;
	vertex[0].dither = dither;
	vertex[0].page = 0;
	vertex[0].clut = 0;

	vertex[1].u = 0;
	vertex[1].v = 0;
	vertex[1].bright = bright;
	vertex[1].dither = dither;
	vertex[1].page = 0;
	vertex[1].clut = 0;

	vertex[2].u = 0;
	vertex[2].v = 0;
	vertex[2].bright = bright;
	vertex[2].dither = dither;
	vertex[2].page = 0;
	vertex[2].clut = 0;
}

void MakeTexcoordQuadZero(GrVertex* vertex, unsigned char dither)
{
	const unsigned char bright = 1;

	vertex[0].u = 0;
	vertex[0].v = 0;
	vertex[0].bright = bright;
	vertex[0].dither = dither;
	vertex[0].page = 0;
	vertex[0].clut = 0;

	vertex[1].u = 0;
	vertex[1].v = 0;
	vertex[1].bright = bright;
	vertex[1].dither = dither;
	vertex[1].page = 0;
	vertex[1].clut = 0;

	vertex[2].u = 0;
	vertex[2].v = 0;
	vertex[2].bright = bright;
	vertex[2].dither = dither;
	vertex[2].page = 0;
	vertex[2].clut = 0;

	vertex[3].u = 0;
	vertex[3].v = 0;
	vertex[3].bright = bright;
	vertex[3].dither = dither;
	vertex[3].page = 0;
	vertex[3].clut = 0;
}

void MakeColourNoShade(GrVertex* vertex, int n)
{
	--n;
	while (n >= 0)
	{
		vertex[n].r = 128;
		vertex[n].g = 128;
		vertex[n].b = 128;
		vertex[n].a = 255;
		--n;
	}
}

void MakeColourLine(GrVertex* vertex, bool shadeTexOn, unsigned char* col0, unsigned char* col1)
{
	if (!shadeTexOn)
	{
		MakeColourNoShade(vertex, 4);
		return;
	}
	assert(col0);
	assert(col1);

	vertex[0].r = col0[0];
	vertex[0].g = col0[1];
	vertex[0].b = col0[2];
	vertex[0].a = 255;

	vertex[1].r = col1[0];
	vertex[1].g = col1[1];
	vertex[1].b = col1[2];
	vertex[1].a = 255;

	vertex[2].r = col1[0];
	vertex[2].g = col1[1];
	vertex[2].b = col1[2];
	vertex[2].a = 255;

	vertex[3].r = col0[0];
	vertex[3].g = col0[1];
	vertex[3].b = col0[2];
	vertex[3].a = 255;
}

void MakeColourTriangle(GrVertex* vertex, bool shadeTexOn, unsigned char* col0, unsigned char* col1, unsigned char* col2)
{
	if (!shadeTexOn)
	{
		MakeColourNoShade(vertex, 3);
		return;
	}

	assert(col0);
	assert(col1);
	assert(col2);

	vertex[0].r = col0[0];
	vertex[0].g = col0[1];
	vertex[0].b = col0[2];
	vertex[0].a = 255;

	vertex[1].r = col1[0];
	vertex[1].g = col1[1];
	vertex[1].b = col1[2];
	vertex[1].a = 255;

	vertex[2].r = col2[0];
	vertex[2].g = col2[1];
	vertex[2].b = col2[2];
	vertex[2].a = 255;
}

void MakeColourQuad(GrVertex* vertex, bool shadeTexOn, unsigned char* col0, unsigned char* col1, unsigned char* col2, unsigned char* col3)
{
	if (!shadeTexOn)
	{
		MakeColourNoShade(vertex, 4);
		return;
	}

	assert(col0);
	assert(col1);
	assert(col2);
	assert(col3);

	vertex[0].r = col0[0];
	vertex[0].g = col0[1];
	vertex[0].b = col0[2];
	vertex[0].a = 255;

	vertex[1].r = col1[0];
	vertex[1].g = col1[1];
	vertex[1].b = col1[2];
	vertex[1].a = 255;

	vertex[2].r = col2[0];
	vertex[2].g = col2[1];
	vertex[2].b = col2[2];
	vertex[2].a = 255;

	vertex[3].r = col3[0];
	vertex[3].g = col3[1];
	vertex[3].b = col3[2];
	vertex[3].a = 255;
}

void TriangulateQuad()
{
	/*
	Triangulate like this:

	v0--v1
	|  / |
	| /  |
	v2--v3

	NOTE: v2 swapped with v3 during primitive parsing but it not shown here
	*/

	g_vertexBuffer[g_vertexIndex + 4] = g_vertexBuffer[g_vertexIndex + 3];

	g_vertexBuffer[g_vertexIndex + 5] = g_vertexBuffer[g_vertexIndex + 2];
	g_vertexBuffer[g_vertexIndex + 2] = g_vertexBuffer[g_vertexIndex + 3];
	g_vertexBuffer[g_vertexIndex + 3] = g_vertexBuffer[g_vertexIndex + 1];
}

//------------------------------------------------------------------------------------------------------------------------

void GR_SetStpPass(int pass); // lain: PsyX_render.cpp

static void AddSplit(bool semiTrans, bool textured)
{
	int tpage = activeDrawEnv.tpage;
	GPUDrawSplit& curSplit = g_splits[g_splitIndex];

	BlendMode blendMode = semiTrans ? GET_TPAGE_BLEND(tpage) : BM_NONE;
	TexFormat texFormat = GET_TPAGE_FORMAT(tpage);
	TextureID textureId = textured ? g_vramTexture : g_whiteTexture;

	if (textured && overrideTexture != 0)
	{
		// override texture format, zero tpage
		texFormat = TF_32_BIT_RGBA;
		textureId = overrideTexture;
	}

	// FIXME: compare drawing environment too?
	if (curSplit.blendMode == blendMode &&
		curSplit.texFormat == texFormat &&
		curSplit.textureId == textureId &&
		curSplit.drawPrimMode == g_DrawPrimMode &&
		curSplit.drawenv.clip.x == activeDrawEnv.clip.x &&
		curSplit.drawenv.clip.y == activeDrawEnv.clip.y &&
		curSplit.drawenv.clip.w == activeDrawEnv.clip.w &&
		curSplit.drawenv.clip.h == activeDrawEnv.clip.h &&
		curSplit.drawenv.dfe == activeDrawEnv.dfe &&
		curSplit.debugText == currentSplitDebugText)
	{
		return;
	}

	curSplit.numVerts = g_vertexIndex - curSplit.startVertex;

	if (g_splitIndex + 1 >= MAX_DRAW_SPLITS)
	{
		eprinterr("MAX_DRAW_SPLITS reached (too many blend modes, texture formats, drawEnv clip rects, dfe switches), expect rendering errors\n");
		return;
	}

	GPUDrawSplit& split = g_splits[++g_splitIndex];
	split.blendMode = blendMode;
	split.texFormat = texFormat;
	split.textureId = textureId;
	split.drawPrimMode = g_DrawPrimMode;
	split.drawenv = activeDrawEnv;
	split.dispenv = activeDispEnv;
	split.debugText = currentSplitDebugText;

	split.drawenv.tw.w = overrideTextureWidth;
	split.drawenv.tw.h = overrideTextureHeight;

	split.startVertex = g_vertexIndex;
	split.numVerts = 0;
}

void DrawSplit(const GPUDrawSplit& split)
{
	if(split.debugText)
		GR_PushDebugLabel(split.debugText);

	GR_SetStencilMode(split.drawPrimMode);	// draw with mask 0x16

	GR_SetTexture(split.textureId, split.texFormat);

	if (split.texFormat == TF_32_BIT_RGBA)
		GR_SetOverrideTextureSize(split.drawenv.tw.w, split.drawenv.tw.h);

	const bool drawOnScreen = split.drawenv.dfe;
	GR_SetupClipMode(&split.drawenv.clip, drawOnScreen);
	GR_SetOffscreenState(&split.drawenv.clip, !drawOnScreen);

	// lain: on the PS1, semi-transparency only applies to texels with the STP bit;
	// the others draw opaque. The average mode gets this from the texel alpha; the
	// additive and subtractive modes ignore it, so they draw in two passes.
	if (split.blendMode != BM_NONE && split.blendMode != BM_AVERAGE &&
		split.textureId != g_whiteTexture && split.texFormat != TF_32_BIT_RGBA)
	{
		GR_SetBlendMode(BM_NONE);
		GR_EnableDepth(0);
		GR_SetStpPass(1);
		GR_DrawTriangles(split.startVertex, split.numVerts / 3);
		GR_SetBlendMode(split.blendMode);
		GR_SetStpPass(2);
		GR_DrawTriangles(split.startVertex, split.numVerts / 3);
		GR_SetStpPass(0);
	}
	else
	{
		GR_SetBlendMode(split.blendMode);
		GR_DrawTriangles(split.startVertex, split.numVerts / 3);
	}

	if (split.debugText)
		GR_PopDebugLabel();
}

extern int g_dbg_polygonSelected;
void LainInterp_NoteBatch();

//
// Draws all polygons after AggregatePTAG
//
void DrawAllSplits()
{
#ifdef _DEBUG
	if (g_dbg_emulatorPaused)
	{
		for (int i = 0; i < 3; i++)
		{
			GrVertex* vert = &g_vertexBuffer[g_dbg_polygonSelected + i];
			vert->r = 255;
			vert->g = 0;
			vert->b = 0;

			eprintf("==========================================\n");
			eprintf("POLYGON: %d\n", g_dbg_polygonSelected);
#if USE_PGXP
			eprintf("X: %.2f Y: %.2f\n", (float)vert->x, (float)vert->y);
			eprintf("U: %.2f V: %.2f\n", (float)vert->u, (float)vert->v);
			eprintf("TP: %d CLT: %d\n", (int)vert->page, (int)vert->clut);
#else
			eprintf("X: %d Y: %d\n", vert->x, vert->y);
			eprintf("U: %d V: %d\n", vert->u, vert->v);
			eprintf("TP: %d CLT: %d\n", vert->page, vert->clut);
#endif
			
			eprintf("==========================================\n");
		}

		PsyX_UpdateInput();
	}
#endif // _DEBUG

	// next code ideally should be called before EndScene
	GR_UpdateVertexBuffer(g_vertexBuffer, g_vertexIndex);

	for (int i = 1; i <= g_splitIndex; i++)
		DrawSplit(g_splits[i]);

	LainInterp_NoteBatch(); // lain
	ClearSplits();
}

// forward declarations
int ParsePrimitive(P_TAG* polyTag);

void LainInterp_NotePrim(const void* base, const void* prim, int v0, int n);
void LainInterp_NoteBatch();

void ParsePrimitivesLinkedList(u_long* p, int singlePrimitive)
{
	if (!p)
		return;

	// lain: upload pending VRAM changes before the primitives pick their texture
	// (not in DrawSync; see LIBGPU.C).
	GR_UpdateVRAM();

	// setup single primitive flag (needed for AddSplits)
	g_DrawPrimMode = singlePrimitive;

	if (singlePrimitive)
	{
		P_TAG* polyTag = reinterpret_cast<P_TAG*>(p);
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
		// force PGXP off
		polyTag->pgxp_index = 0xFFFF;
#endif
		const int v0 = g_vertexIndex; // lain: frame interpolation
		ParsePrimitive(polyTag);
		LainInterp_NotePrim(p, p, v0, g_vertexIndex - v0);

		GPUDrawSplit& lastSplit = g_splits[g_splitIndex];
		lastSplit.numVerts = g_vertexIndex - lastSplit.startVertex;
	}
	else
	{
		// walk OT_TAG linked list
		for (uintptr_t basePacket = reinterpret_cast<uintptr_t>(p);; basePacket = reinterpret_cast<uintptr_t>(nextPrim(basePacket)))
		{
			const int tagLength = getlen(basePacket);
			if (tagLength > 0)
			{
				if (tagLength > 32)
				{
					eprinterr("got invalid tag length %d, code %d\n", tagLength, reinterpret_cast<P_TAG*>(basePacket)->code);
				}

				uintptr_t currentPacket = basePacket;
				const uintptr_t endPacket = basePacket + (tagLength + P_LEN) * sizeof(u_int);
				int primLength = 0;
				while (currentPacket < endPacket)
				{
					const int v0 = g_vertexIndex; // lain: frame interpolation
					primLength = ParsePrimitive(reinterpret_cast<P_TAG*>(currentPacket));
					LainInterp_NotePrim(reinterpret_cast<void*>(basePacket), reinterpret_cast<void*>(currentPacket), v0, g_vertexIndex - v0);
					currentPacket += (primLength + P_LEN) * sizeof(u_int);
				}

				if (currentPacket != endPacket)
				{
					eprinterr("did not output valid primitive or ptag length is not valid (diff=%d)\n", endPacket-currentPacket);
				}
			}

			GPUDrawSplit& lastSplit = g_splits[g_splitIndex];
			lastSplit.numVerts = g_vertexIndex - lastSplit.startVertex;

			if (isendprim(basePacket))
				break;
		}
	}
}

inline int IsNull(POLY_FT3* poly)
{
	return  poly->x0 == -1 &&
		poly->y0 == -1 &&
		poly->x1 == -1 &&
		poly->y1 == -1 &&
		poly->x2 == -1 &&
		poly->y2 == -1;
}

static int ProcessFlatLines(P_TAG* polyTag)
{
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
	const u_short gteIndex = polyTag->pgxp_index;
#else
	const u_short gteIndex = 0xFFFF;
#endif

	const bool shadeTexOn = true;
	const bool semiTrans = (polyTag->code & 2);
	const int primSubType = polyTag->code & 0x0C;

	switch (primSubType)
	{
	case 0x0:
	{
		LINE_F2* poly = (LINE_F2*)polyTag;

		AddSplit(semiTrans, false);

		VERTTYPE* p0 = &poly->x0;
		VERTTYPE* p1 = &poly->x1;
		unsigned char* c0 = &poly->r0;
		unsigned char* c1 = c0;

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		LineSwapSourceVerts(p0, p1, c0, c1);
		MakeLineArray(firstVertex, p0, p1, gteIndex);
		MakeTexcoordLineZero(firstVertex, 0);
		MakeColourLine(firstVertex, shadeTexOn, c0, c1);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 3;
	}
	case 0x8: // TODO (unused)
	{
		LINE_F3* poly = (LINE_F3*)polyTag;

		AddSplit(semiTrans, false);

		{
			VERTTYPE* p0 = &poly->x0;
			VERTTYPE* p1 = &poly->x1;
			unsigned char* c0 = &poly->r0;
			unsigned char* c1 = c0;

			GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
			LineSwapSourceVerts(p0, p1, c0, c1);
			MakeLineArray(firstVertex, p0, p1, gteIndex);
			MakeTexcoordLineZero(firstVertex, 0);
			MakeColourLine(firstVertex, shadeTexOn, c0, c1);

			TriangulateQuad();

			g_vertexIndex += 6;
#if defined(DEBUG_POLY_COUNT)
			polygon_count++;
#endif
		}

		{
			VERTTYPE* p0 = &poly->x1;
			VERTTYPE* p1 = &poly->x2;
			unsigned char* c0 = &poly->r0;
			unsigned char* c1 = c0;

			GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
			LineSwapSourceVerts(p0, p1, c0, c1);
			MakeLineArray(firstVertex, p0, p1, gteIndex);
			MakeTexcoordLineZero(firstVertex, 0);
			MakeColourLine(firstVertex, shadeTexOn, c0, c1);

			TriangulateQuad();

			g_vertexIndex += 6;
#if defined(DEBUG_POLY_COUNT)
			polygon_count++;
#endif
		}

		return 5;
	}
	case 0xc:
	{
		int i;
		LINE_F4* poly = (LINE_F4*)polyTag;

		AddSplit(semiTrans, false);

		{
			VERTTYPE* p0 = &poly->x0;
			VERTTYPE* p1 = &poly->x1;
			unsigned char* c0 = &poly->r0;
			unsigned char* c1 = c0;

			GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
			LineSwapSourceVerts(p0, p1, c0, c1);
			MakeLineArray(firstVertex, p0, p1, gteIndex);
			MakeTexcoordLineZero(firstVertex, 0);
			MakeColourLine(firstVertex, shadeTexOn, c0, c1);

			TriangulateQuad();

			g_vertexIndex += 6;
#if defined(DEBUG_POLY_COUNT)
			polygon_count++;
#endif
		}

		{
			VERTTYPE* p0 = &poly->x1;
			VERTTYPE* p1 = &poly->x2;
			unsigned char* c0 = &poly->r0;
			unsigned char* c1 = c0;

			GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
			LineSwapSourceVerts(p0, p1, c0, c1);
			MakeLineArray(firstVertex, p0, p1, gteIndex);
			MakeTexcoordLineZero(firstVertex, 0);
			MakeColourLine(firstVertex, shadeTexOn, c0, c1);

			TriangulateQuad();

			g_vertexIndex += 6;
#if defined(DEBUG_POLY_COUNT)
			polygon_count++;
#endif
		}

		{
			VERTTYPE* p0 = &poly->x2;
			VERTTYPE* p1 = &poly->x3;
			unsigned char* c0 = &poly->r0;
			unsigned char* c1 = c0;

			GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
			LineSwapSourceVerts(p0, p1, c0, c1);
			MakeLineArray(firstVertex, p0, p1, gteIndex);
			MakeTexcoordLineZero(firstVertex, 0);
			MakeColourLine(firstVertex, shadeTexOn, c0, c1);

			TriangulateQuad();

			g_vertexIndex += 6;
#if defined(DEBUG_POLY_COUNT)
			polygon_count++;
#endif
		}

		return 6;
	}
	}
	return 0;
}

static int ProcessGouraudLines(P_TAG* polyTag)
{
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
	const u_short gteIndex = polyTag->pgxp_index;
#else
	const u_short gteIndex = 0xFFFF;
#endif

	const bool shadeTexOn = true;
	const bool semiTrans = (polyTag->code & 2);
	const int primSubType = polyTag->code & 0x0C;

	switch (primSubType)
	{
	case 0x0:
	{
		LINE_G2* poly = (LINE_G2*)polyTag;

		AddSplit(semiTrans, false);

		VERTTYPE* p0 = &poly->x0;
		VERTTYPE* p1 = &poly->x1;
		unsigned char* c0 = &poly->r0;
		unsigned char* c1 = &poly->r1;

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		LineSwapSourceVerts(p0, p1, c0, c1);
		MakeLineArray(firstVertex, p0, p1, gteIndex);
		MakeTexcoordLineZero(firstVertex, 0);
		MakeColourLine(firstVertex, shadeTexOn, c0, c1);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 4;
	}
	case 0x8:
	{
		// TODO: LINE_G3
		return 7;
	}
	case 0xC:
	{
		// TODO: LINE_G4
		return 9;
	}
	}
	return 0;
}

static int ProcessFlatPoly(P_TAG* polyTag)
{
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
	const u_short gteIndex = polyTag->pgxp_index;
#else
	const u_short gteIndex = 0xFFFF;
#endif

	const bool shadeTexOn = (polyTag->code & 1) == 0;
	const bool semiTrans = (polyTag->code & 2);
	const int primSubType = polyTag->code & 0x0C;

	switch (primSubType)
	{
	case 0x0:
	{
		POLY_F3* poly = (POLY_F3*)polyTag;

		AddSplit(semiTrans, false);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexTriangle(firstVertex, &poly->x0, &poly->x1, &poly->x2, gteIndex);
		MakeTexcoordTriangleZero(firstVertex, 0);
		MakeColourTriangle(firstVertex, shadeTexOn, &poly->r0, &poly->r0, &poly->r0);

		g_vertexIndex += 3;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 4;
	}
	case 0x4:
	{
		POLY_FT3* poly = (POLY_FT3*)polyTag;
		activeDrawEnv.tpage = poly->tpage;

		// It is an official hack from SCE devs to not use DR_TPAGE and instead use null polygon
		if (!IsNull(poly))
		{
			AddSplit(semiTrans, true);

			GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
			MakeVertexTriangle(firstVertex, &poly->x0, &poly->x1, &poly->x2, gteIndex);
			MakeTexcoordTriangle(firstVertex, &poly->u0, &poly->u1, &poly->u2, poly->tpage, poly->clut, GET_TPAGE_DITHER(activeDrawEnv.tpage) || activeDrawEnv.dtd);
			MakeColourTriangle(firstVertex, shadeTexOn, &poly->r0, &poly->r0, &poly->r0);

			g_vertexIndex += 3;

#if defined(DEBUG_POLY_COUNT)
			polygon_count++;
#endif
		}
		return 7;
	}
	case 0x8:
	{
		POLY_F4* poly = (POLY_F4*)polyTag;

		AddSplit(semiTrans, false);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexQuad(firstVertex, &poly->x0, &poly->x1, &poly->x3, &poly->x2, gteIndex);
		MakeTexcoordQuadZero(firstVertex, 0);
		MakeColourQuad(firstVertex, shadeTexOn, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;
#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 5;
	}
	case 0xC:
	{
		POLY_FT4* poly = (POLY_FT4*)polyTag;
		activeDrawEnv.tpage = poly->tpage;

		AddSplit(semiTrans, true);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexQuad(firstVertex, &poly->x0, &poly->x1, &poly->x3, &poly->x2, gteIndex);
		MakeTexcoordQuad(firstVertex, &poly->u0, &poly->u1, &poly->u3, &poly->u2, poly->tpage, poly->clut, GET_TPAGE_DITHER(activeDrawEnv.tpage) || activeDrawEnv.dtd);
		MakeColourQuad(firstVertex, shadeTexOn, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 9;
	}
	}
	return 0;
}

static int ProcessGouraudPoly(P_TAG* polyTag)
{
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
	const u_short gteIndex = polyTag->pgxp_index;
#else
	const u_short gteIndex = 0xFFFF;
#endif

	const bool shadeTexOn = true;
	const bool semiTrans = (polyTag->code & 2);
	const int primSubType = polyTag->code & 0x0C;

	switch (primSubType)
	{
	case 0x0:
	{
		POLY_G3* poly = (POLY_G3*)polyTag;

		AddSplit(semiTrans, false);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexTriangle(firstVertex, &poly->x0, &poly->x1, &poly->x2, gteIndex);
		MakeTexcoordTriangleZero(firstVertex, 1);
		MakeColourTriangle(firstVertex, shadeTexOn, &poly->r0, &poly->r1, &poly->r2);

		g_vertexIndex += 3;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 6;
	}
	case 0x4:
	{
		POLY_GT3* poly = (POLY_GT3*)polyTag;
		activeDrawEnv.tpage = poly->tpage;

		AddSplit(semiTrans, true);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexTriangle(firstVertex, &poly->x0, &poly->x1, &poly->x2, gteIndex);
		MakeTexcoordTriangle(firstVertex, &poly->u0, &poly->u1, &poly->u2, poly->tpage, poly->clut, GET_TPAGE_DITHER(activeDrawEnv.tpage) || activeDrawEnv.dtd);
		MakeColourTriangle(firstVertex, shadeTexOn, &poly->r0, &poly->r1, &poly->r2);

		g_vertexIndex += 3;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 9;
	}
	case 0x8:
	{
		POLY_G4* poly = (POLY_G4*)polyTag;

		AddSplit(semiTrans, false);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexQuad(firstVertex, &poly->x0, &poly->x1, &poly->x3, &poly->x2, gteIndex);
		MakeTexcoordQuadZero(firstVertex, 1);
		MakeColourQuad(firstVertex, shadeTexOn, &poly->r0, &poly->r1, &poly->r3, &poly->r2);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 8;
	}
	case 0xC:
	{
		POLY_GT4* poly = (POLY_GT4*)polyTag;
		activeDrawEnv.tpage = poly->tpage;

		AddSplit(semiTrans, true);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexQuad(firstVertex, &poly->x0, &poly->x1, &poly->x3, &poly->x2, gteIndex);
		MakeTexcoordQuad(firstVertex, &poly->u0, &poly->u1, &poly->u3, &poly->u2, poly->tpage, poly->clut, GET_TPAGE_DITHER(activeDrawEnv.tpage) || activeDrawEnv.dtd);
		MakeColourQuad(firstVertex, shadeTexOn, &poly->r0, &poly->r1, &poly->r3, &poly->r2);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 12;
	}
	}
	return 0;
}

static int ProcessTileAndSprt(P_TAG* polyTag)
{
#if USE_PGXP && USE_EXTENDED_PRIM_POINTERS
	const u_short gteIndex = polyTag->pgxp_index;
#else
	const u_short gteIndex = 0xFFFF;
#endif

	// NOTE: TILE does not support switching shadeTex on real PSX
	const bool shadeTexOn = (polyTag->code & 1) == 0;
	const bool semiTrans = (polyTag->code & 2);

	// lain: mask bit 0 too, so raw-texture sprites (0x65/0x67) match; shadeTexOn
	// above already covers it.
	switch (polyTag->code & 0xFC)
	{
	case 0x60:
	{
		TILE* poly = (TILE*)polyTag;

		AddSplit(semiTrans, false);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexRect(firstVertex, &poly->x0, poly->w, poly->h, gteIndex);
		MakeTexcoordQuadZero(firstVertex, 0);
		MakeColourQuad(firstVertex, shadeTexOn, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 3;
	}
	case 0x64:
	{
		SPRT* poly = (SPRT*)polyTag;

		AddSplit(semiTrans, true);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexRect(firstVertex, &poly->x0, poly->w, poly->h, gteIndex);
		MakeTexcoordRect(firstVertex, &poly->u0, activeDrawEnv.tpage, poly->clut, poly->w, poly->h);
		MakeColourQuad(firstVertex, shadeTexOn, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 4;
	}
	case 0x68:
	{
		TILE_1* poly = (TILE_1*)polyTag;

		AddSplit(semiTrans, false);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexRect(firstVertex, &poly->x0, 1, 1, gteIndex);
		MakeTexcoordQuadZero(firstVertex, 0);
		MakeColourQuad(firstVertex, true, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 2;
	}
	case 0x70:
	{
		TILE_8* poly = (TILE_8*)polyTag;

		AddSplit(semiTrans, false);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexRect(firstVertex, &poly->x0, 8, 8, gteIndex);
		MakeTexcoordQuadZero(firstVertex, 0);
		MakeColourQuad(firstVertex, true, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 2;
	}
	case 0x74:
	{
		SPRT_8* poly = (SPRT_8*)polyTag;

		AddSplit(semiTrans, true);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexRect(firstVertex, &poly->x0, 8, 8, gteIndex);
		MakeTexcoordRect(firstVertex, &poly->u0, activeDrawEnv.tpage, poly->clut, 8, 8);
		MakeColourQuad(firstVertex, shadeTexOn, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 3;
	}
	case 0x78:
	{
		TILE_16* poly = (TILE_16*)polyTag;

		AddSplit(semiTrans, false);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexRect(firstVertex, &poly->x0, 16, 16, gteIndex);
		MakeTexcoordQuadZero(firstVertex, 0);
		MakeColourQuad(firstVertex, true, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 2;
	}
	case 0x7C:
	{
		SPRT_16* poly = (SPRT_16*)polyTag;

		AddSplit(semiTrans, true);

		GrVertex* firstVertex = &g_vertexBuffer[g_vertexIndex];
		MakeVertexRect(firstVertex, &poly->x0, 16, 16, gteIndex);
		MakeTexcoordRect(firstVertex, &poly->u0, activeDrawEnv.tpage, poly->clut, 16, 16);
		MakeColourQuad(firstVertex, shadeTexOn, &poly->r0, &poly->r0, &poly->r0, &poly->r0);

		TriangulateQuad();

		g_vertexIndex += 6;

#if defined(DEBUG_POLY_COUNT)
		polygon_count++;
#endif
		return 3;
	}
	}
	return 0;
}

static int ProcessDrawEnv(P_TAG* polyTag)
{
	const u_int* codePtr = (u_int*)&polyTag->pad0;
	int processedLongs = 0;
	for (int i = 0; i < polyTag->len; ++i)
	{
		const u_int code = codePtr[i];
		const int primSubType = code >> 24 & 0x0F;

		switch (primSubType)
		{
		case 0x1:
		{
			// DR_TPAGE
			activeDrawEnv.tpage = (code & 0x1FF);
			activeDrawEnv.dtd = (code >> 9) & 1;
			activeDrawEnv.dfe = (code >> 10) & 1;
			break;
		}
		case 0x2:
		{
			// DR_TWIN
			activeDrawEnv.tw.w = (code & 0x1F);
			activeDrawEnv.tw.h = ((code >> 5) & 0x1F);
			activeDrawEnv.tw.x = ((code >> 10) & 0x1F);
			activeDrawEnv.tw.y = ((code >> 15) & 0x1F);
			break;
		}
		case 0x3:
		{
			// DR_AREA
			activeDrawEnv.clip.x = code & 1023;
			activeDrawEnv.clip.y = (code >> 10) & 1023;
			break;
		}
		case 0x4:
		{
			// DR_AREA (second part)
			activeDrawEnv.clip.w = code & 1023;
			activeDrawEnv.clip.h = (code >> 10) & 1023;

			activeDrawEnv.clip.w -= activeDrawEnv.clip.x;
			activeDrawEnv.clip.h -= activeDrawEnv.clip.y;
			break;
		}
		case 0x5:
		{
			// DR_OFFSET
			// TODO
			activeDrawEnv.ofs[0] = code & 2047;
			activeDrawEnv.ofs[1] = (code >> 11) & 2047;
			break;
		}
		case 0x6:
		{
			eprintf("Mask setting: %08x\n", code);
			//MaskSetOR = (*cb & 1) ? 0x8000 : 0x0000;
			//MaskEvalAND = (*cb & 2) ? 0x8000 : 0x0000;
			break;
		}
		case 0:
			// proceed to next primitive tag
			return processedLongs;
		}
		++processedLongs;
	}

	return processedLongs;
}

static int ProcessPsyXPrims(P_TAG* polyTag)
{
	const int primType = polyTag->code & 0xF0;
	const int primSubType = polyTag->code & 0x0F;

	switch (primSubType)
	{
	case 0x01:
	{
		DR_PSYX_TEX* psytex = (DR_PSYX_TEX*)polyTag;
		overrideTexture = psytex->code[0] & 0xFFFFFF;
		overrideTextureWidth = psytex->code[1] & 0xFFF;
		overrideTextureHeight = psytex->code[1] >> 16 & 0xFFF;
		return 2;
	}
	case 0x02:
	{
		// [A] Psy-X custom texture packet
		DR_PSYX_DBGMARKER* psydbg = (DR_PSYX_DBGMARKER*)polyTag;
		currentSplitDebugText = psydbg->text;
		return 2;
	}
	}

	return 0;
}

// Processes primitive
// returns processed primitive primLength in longs
// lain: the PS1 GPU does not draw a polygon whose vertices are more than 1023
// pixels apart horizontally or 511 vertically (after the 11-bit wrap). Games
// leave such polygons in their lists (e.g. sprites zoomed past the screen).
// Returns the polygon's length in words if it must be skipped, else 0.
static int PolyCulledBySize(P_TAG* polyTag)
{
#if USE_PGXP
	return 0;
#else
	const u_char code = polyTag->code;
	const int tex = (code & 0x04) != 0, gour = (code & 0x10) != 0, quad = (code & 0x08) != 0;
	const int stride = 1 + tex + gour;
	const int nverts = quad ? 4 : 3;
	const u_int* w = (const u_int*)&polyTag->pad0;
	int minx = 4096, maxx = -4096, miny = 4096, maxy = -4096;

	for (int i = 0; i < nverts; i++)
	{
		const u_int xy = w[1 + i * stride];
		const int x = ((int)(xy << 21)) >> 21;
		const int y = ((int)((xy >> 16) << 21)) >> 21;
		minx = x < minx ? x : minx;
		maxx = x > maxx ? x : maxx;
		miny = y < miny ? y : miny;
		maxy = y > maxy ? y : maxy;
	}
	if (maxx - minx <= 1023 && maxy - miny <= 511)
		return 0;

	// F3 4, FT3 7, G3 6, GT3 9, F4 5, FT4 9, G4 8, GT4 12
	static const int lengths[2][2][2] = { { { 4, 7 }, { 6, 9 } }, { { 5, 9 }, { 8, 12 } } };
	return lengths[quad][gour][tex];
#endif
}

int ParsePrimitive(P_TAG* polyTag)
{
	const int primType = polyTag->code & 0xF0;

	int primLength = 0;

	if (primType == 0x20 || primType == 0x30) // lain
	{
		const int culled = PolyCulledBySize(polyTag);
		if (culled)
			return culled;
	}

	switch (primType)
	{
	case 0x00:
	{
		const int primSubType = polyTag->code & 0x0F;
		if (primSubType == 0x0)
		{
			primLength = 3;
		}
		else if (primSubType == 0x1)
		{
			DR_MOVE* drmove = (DR_MOVE*)polyTag;

			const int y = drmove->code[3] >> 0x10 & 0xFFFF;
			const int x = drmove->code[3] & 0xFFFF;

			RECT16 rect;
			*(uint*)&rect.x = *(uint*)&drmove->code[2];
			*(uint*)&rect.w = *(uint*)&drmove->code[4];

			MoveImage(&rect, x, y);
			primLength = 5;
		}
		break;
	}
	case 0x20:
		// Flat polygons
		primLength = ProcessFlatPoly(polyTag);
		break;
	case 0x30:
		// Gouraud shaded polygons
		primLength = ProcessGouraudPoly(polyTag);
		break;
	case 0x40:
		// Flat (single colour) Lines
		primLength = ProcessFlatLines(polyTag);
		break;
	case 0x50:
		// Gouraud lines
		primLength = ProcessGouraudLines(polyTag);
		break;
	case 0x60:
	case 0x70:
		// TILE and SPRT
		primLength = ProcessTileAndSprt(polyTag);
		break;
	case 0xA0:
		// DR_LOAD
		{
			DR_LOAD* drload = (DR_LOAD*)polyTag;

			RECT16 rect;
			*(uint*)&rect.x = *(uint*)&drload->code[1];
			*(uint*)&rect.w = *(uint*)&drload->code[2];

			LoadImage(&rect, (u_int*)drload->p); // lain: LoadImage takes u_int*
			//Emulator_UpdateVRAM();			// FIXME: should it be updated immediately?

			// FIXME: is there othercommands?
		}
		primLength = getlen(polyTag);
		break;
	case 0xB0:
		// [A] Psy-X custom primitives
		primLength = ProcessPsyXPrims(polyTag);
		break;
	case 0xE0:
		// Draw Env setup
		primLength = ProcessDrawEnv(polyTag);
		break;
	//default:
	//	eprinterr("got %0x primitive\n", primType);
	}

	if(primLength == 0)
	{
		eprinterr("Unhandled zero length %0x primitive\n", primType);
	}

	return primLength;
}


// ---- lain: frame interpolation -------------------------------------------------
// Screens the game draws at 12 or 30 fps (it waits several vblanks per frame)
// can be shown at the full 60 Hz: every scene's draw batches are recorded, and
// while the game waits in VSync the renderer draws the current scene again
// with each primitive moved part of the way from where it was in the previous
// scene. Primitives are matched by the tag libgs gives each packet (a hash of
// the object/polygon or sprite it draws); untagged ones by address. Game logic
// is unaffected, and the real frame is shown unchanged at the end of the wait.
// g_cfg_interpolate turns it on.

extern "C" uint64_t LainGs_PacketTag(const void* pkt);
extern "C" int LainGs_PacketIs2D(const void* pkt);
void GR_InterpSnapshotBase();
int GR_InterpSnapshotVRAM(int slot);
TextureID GR_InterpTexture(TextureID recorded, int slot);
void GR_InterpBegin();
void GR_InterpEnd();
void GR_InterpRestore();
void GR_SwapWindow();
void GR_UpdateVertexBuffer(const GrVertex* vertices, int num_vertices);

int g_cfg_interpolate = 0;
extern int g_PreviousOffscreenState; // PsyX_render.cpp: drawing into VRAM right now
#define INTERP_MAX_BATCHES 8        // = INTERP_MAX_SNAPS in PsyX_render.cpp
void (*g_lain_onInterpPresent)(void) = NULL;

// A vertex the GTE projected: the model vertex and the transform (lain)
struct InterpRec
{
	float r[9];     // rotation, 4.12
	float t[3];
	float h, ofx, ofy;
	short v[3];
};

struct InterpGroup
{
	uint64_t id;
	int v0, n;
	int r0;         // first of n records in the batch, -1 if not all vertices have one
	bool sprite;    // a 2D picture sorted by libgs
	float ox, oy;   // draw offset
};

struct InterpBatch
{
	std::vector<GrVertex> verts;
	std::vector<GPUDrawSplit> splits;
	std::vector<InterpGroup> groups;
	std::vector<InterpRec> recs;
	int snap;
};

struct InterpOp
{
	int batch;            // >= 0: draw batch; -1: clear
	int x, y, w, h;
	u_char r, g, b;
};

struct InterpScene
{
	std::vector<InterpOp> ops;
	std::vector<InterpBatch> batches;
	unsigned seq = 0;
	bool open = false;    // between BeginScene and EndScene
	bool ended = false;   // presented
	bool broken = false;  // something that can't be replayed happened
	std::vector<RECT16> uploads; // VRAM written since the previous scene ended
};

static InterpScene s_iscenes[2];
static int s_icur = 0;
static unsigned s_iseq = 0;
static std::vector<InterpGroup> s_ipending;
static bool s_ireplaying = false;
static int s_iprevIndexedSeq = -1;
static std::unordered_map<uint64_t, std::pair<int, int>> s_iprevIndex;
static std::vector<GrVertex> s_iverts;
static std::vector<RECT16> s_iuploads; // VRAM writes between scenes
static std::vector<InterpRec> s_irecs;   // records of the groups in s_ipending

static bool LainInterp_Recording()
{
	return g_cfg_interpolate && !s_ireplaying && s_iscenes[s_icur].open;
}

static void LainInterp_ClearPending()
{
	s_ipending.clear();
	s_irecs.clear();
}

// The last projection to each screen position, for the batch being sorted and
// the one before it
#define INTERP_CANDS 4
struct InterpCands
{
	InterpRec c[INTERP_CANDS]; // latest first
	int n;
};
static std::unordered_map<uint32_t, InterpCands> s_iproj[2];
static unsigned s_istamp = 1; // counts batches

static uint32_t LainInterp_ProjKey(int sx, int sy)
{
	return (uint32_t)(sx & 0xFFFF) | ((uint32_t)(sy & 0xFFFF) << 16);
}

// A polygon libgs built from a TMD, with its vertices and transform
struct InterpPacket
{
	InterpRec v[4];
	short sxy[4][2];
	int nv;
	unsigned stamp;
};
static std::unordered_map<const void*, InterpPacket> s_ipackets;

extern "C" void LainInterp_NotePacket(const void* prim, int nv, const short* v, const unsigned* sxy,
	const int* r, const int* t, int h, int ofx, int ofy)
{
	if (!g_cfg_interpolate || s_ireplaying)
		return;
	InterpPacket& p = s_ipackets[prim];
	p.nv = nv;
	p.stamp = s_istamp;
	for (int k = 0; k < nv; k++)
	{
		InterpRec& e = p.v[k];
		for (int i = 0; i < 9; i++)
			e.r[i] = (float)r[i];
		for (int i = 0; i < 3; i++)
		{
			e.t[i] = (float)t[i];
			e.v[i] = v[k * 3 + i];
		}
		e.h = (float)h;
		e.ofx = ofx / 65536.0f;
		e.ofy = ofy / 65536.0f;
		p.sxy[k][0] = (short)(sxy[k] & 0xFFFF);
		p.sxy[k][1] = (short)(sxy[k] >> 16);
	}
}

// GTE RTPS/RTPT, for each vertex it projects
extern "C" void LainInterp_NoteProjection(int sx, int sy, const short* v, const int* r, const int* t, int h, int ofx, int ofy)
{
	if (!g_cfg_interpolate || s_ireplaying)
		return;
	InterpRec e;
	for (int i = 0; i < 9; i++)
		e.r[i] = (float)r[i];
	for (int i = 0; i < 3; i++)
	{
		e.t[i] = (float)t[i];
		e.v[i] = v[i];
	}
	e.h = (float)h;
	e.ofx = ofx / 65536.0f;
	e.ofy = ofy / 65536.0f;
	InterpCands& cs = s_iproj[s_istamp & 1][LainInterp_ProjKey(sx, sy)];
	for (int i = 0; i < cs.n; i++)
		if (memcmp(&cs.c[i], &e, sizeof(e)) == 0)
			return; // a vertex shared by two polygons
	const int keep = cs.n < INTERP_CANDS ? cs.n : INTERP_CANDS - 1;
	memmove(&cs.c[1], &cs.c[0], keep * sizeof(InterpRec));
	cs.c[0] = e;
	cs.n = keep + 1;
}

static void Mat3Mul(const float* a, const float* b, float* o)
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			o[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
}

static void Mat3Apply(const float* a, const float* v, float* o)
{
	for (int i = 0; i < 3; i++)
		o[i] = a[i * 3] * v[0] + a[i * 3 + 1] * v[1] + a[i * 3 + 2] * v[2];
}

static void RecToView(const InterpRec& r, float* o)
{
	float rm[9], v[3] = { (float)r.v[0], (float)r.v[1], (float)r.v[2] };
	for (int i = 0; i < 9; i++)
		rm[i] = r.r[i] / 4096.0f;
	Mat3Apply(rm, v, o);
	for (int i = 0; i < 3; i++)
		o[i] += r.t[i];
}

// Where a vertex is at t (0..1) on its way from a to c, in view space, moving
// along the rigid motion between the two transforms: a turn about some axis
// plus a slide along it. A straight line between the two screen positions would
// cut across the turn and bend the shape.
static bool LainInterp_Screw(const InterpRec& a, const InterpRec& c, float t, float* p)
{
	if (memcmp(a.v, c.v, sizeof(a.v)) != 0)
		return false;
	float ra[9], rc[9], inv[9], m[9];
	for (int i = 0; i < 9; i++)
	{
		ra[i] = a.r[i] / 4096.0f;
		rc[i] = c.r[i] / 4096.0f;
	}
	inv[0] = ra[4] * ra[8] - ra[5] * ra[7];
	inv[1] = ra[2] * ra[7] - ra[1] * ra[8];
	inv[2] = ra[1] * ra[5] - ra[2] * ra[4];
	inv[3] = ra[5] * ra[6] - ra[3] * ra[8];
	inv[4] = ra[0] * ra[8] - ra[2] * ra[6];
	inv[5] = ra[2] * ra[3] - ra[0] * ra[5];
	inv[6] = ra[3] * ra[7] - ra[4] * ra[6];
	inv[7] = ra[1] * ra[6] - ra[0] * ra[7];
	inv[8] = ra[0] * ra[4] - ra[1] * ra[3];
	const float det = ra[0] * inv[0] + ra[1] * inv[3] + ra[2] * inv[6];
	if (fabsf(det) < 1e-6f)
		return false;
	for (int i = 0; i < 9; i++)
		inv[i] /= det;
	Mat3Mul(rc, inv, m); // view_c = m * view_a + b

	// Only a turn can be followed, not a change of scale.
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
		{
			const float d = m[i * 3] * m[j * 3] + m[i * 3 + 1] * m[j * 3 + 1] + m[i * 3 + 2] * m[j * 3 + 2];
			if (fabsf(d - (i == j ? 1.0f : 0.0f)) > 0.02f)
				return false;
		}

	float pa[3], b[3], mt[3];
	RecToView(a, pa);
	Mat3Apply(m, a.t, mt);
	for (int i = 0; i < 3; i++)
		b[i] = c.t[i] - mt[i];

	// Axis and angle, through a unit quaternion.
	float qw, qx, qy, qz;
	const float tr = m[0] + m[4] + m[8];
	if (tr > 0.0f)
	{
		const float s = sqrtf(tr + 1.0f) * 2.0f;
		qw = 0.25f * s;
		qx = (m[7] - m[5]) / s;
		qy = (m[2] - m[6]) / s;
		qz = (m[3] - m[1]) / s;
	}
	else if (m[0] > m[4] && m[0] > m[8])
	{
		const float s = sqrtf(1.0f + m[0] - m[4] - m[8]) * 2.0f;
		qw = (m[7] - m[5]) / s;
		qx = 0.25f * s;
		qy = (m[1] + m[3]) / s;
		qz = (m[2] + m[6]) / s;
	}
	else if (m[4] > m[8])
	{
		const float s = sqrtf(1.0f + m[4] - m[0] - m[8]) * 2.0f;
		qw = (m[2] - m[6]) / s;
		qx = (m[1] + m[3]) / s;
		qy = 0.25f * s;
		qz = (m[5] + m[7]) / s;
	}
	else
	{
		const float s = sqrtf(1.0f + m[8] - m[0] - m[4]) * 2.0f;
		qw = (m[3] - m[1]) / s;
		qx = (m[2] + m[6]) / s;
		qy = (m[5] + m[7]) / s;
		qz = 0.25f * s;
	}
	{
		const float l = sqrtf(qw * qw + qx * qx + qy * qy + qz * qz);
		qw /= l; qx /= l; qy /= l; qz /= l;
		if (qw < 0.0f)
		{
			qw = -qw; qx = -qx; qy = -qy; qz = -qz;
		}
	}
	const float sh = sqrtf(qx * qx + qy * qy + qz * qz); // sin(angle / 2)
	const float angle = 2.0f * atan2f(sh, qw);
	if (angle > 1.75f)
		return false; // too far to tell which way it turned

	if (sh < 1e-5f)
	{
		for (int i = 0; i < 3; i++)
			p[i] = pa[i] + b[i] * t;
		return true;
	}
	const float u[3] = { qx / sh, qy / sh, qz / sh };
	const float along = u[0] * b[0] + u[1] * b[1] + u[2] * b[2];
	float bp[3], ctr[3], r[3], cr[3];
	for (int i = 0; i < 3; i++)
		bp[i] = b[i] - along * u[i];
	// the axis passes through ctr
	const float k = qw / sh; // cot(angle / 2)
	ctr[0] = 0.5f * (bp[0] + k * (u[1] * bp[2] - u[2] * bp[1]));
	ctr[1] = 0.5f * (bp[1] + k * (u[2] * bp[0] - u[0] * bp[2]));
	ctr[2] = 0.5f * (bp[2] + k * (u[0] * bp[1] - u[1] * bp[0]));
	for (int i = 0; i < 3; i++)
		r[i] = pa[i] - ctr[i];
	const float ang = angle * t, cs = cosf(ang), sn = sinf(ang);
	const float ud = u[0] * r[0] + u[1] * r[1] + u[2] * r[2];
	cr[0] = u[1] * r[2] - u[2] * r[1];
	cr[1] = u[2] * r[0] - u[0] * r[2];
	cr[2] = u[0] * r[1] - u[1] * r[0];
	for (int i = 0; i < 3; i++)
		p[i] = r[i] * cs + cr[i] * sn + u[i] * ud * (1.0f - cs) + ctr[i] + u[i] * along * t;
	return true;
}

static bool LainInterp_Project(const float* p, float h, float ofx, float ofy, float* xy)
{
	if (p[2] < 1.0f)
		return false;
	xy[0] = ofx + p[0] * h / p[2];
	xy[1] = ofy + p[1] * h / p[2];
	return true;
}

// Sets a vertex position with the fraction of a pixel in the spare bytes
// (a_extra.zw in the shader).
static void LainInterp_SetXY(GrVertex& v, float x, float y)
{
	const float fx = floorf(x), fy = floorf(y);
	v.x = (short)fx;
	v.y = (short)fy;
	v._p0 = (char)fminf((x - fx) * 128.0f, 127.0f);
	v._p1 = (char)fminf((y - fy) * 128.0f, 127.0f);
}

static bool LainInterp_ScrewGroup(const GrVertex* a, const InterpRec* ra, GrVertex* c, const InterpRec* rc, int n, float t, float ox, float oy)
{
	float out[8][2];
	if (n > 8)
		return false;
	for (int k = 0; k < n; k++)
	{
		if (abs(c[k].x - a[k].x) > 400 || abs(c[k].y - a[k].y) > 400)
			return false; // a cut
		// The same model vertex in the previous frame, under the most similar
		// transform. It can be another corner of the polygon: some are built
		// with their corners in a different order each frame.
		const InterpRec* rpa = NULL;
		const InterpRec* rpc = NULL;
		const GrVertex* va = NULL;
		float best = 0.0f;
		for (int ka = 0; ka < n; ka++)
			for (int i = 0; i < INTERP_CANDS; i++)
				for (int j = 0; j < INTERP_CANDS; j++)
				{
					const InterpRec& x = ra[ka * INTERP_CANDS + i];
					const InterpRec& y = rc[k * INTERP_CANDS + j];
					if (x.h < 0.0f || y.h < 0.0f || memcmp(x.v, y.v, sizeof(x.v)) != 0)
						continue;
					float d = ka == k ? 0.0f : 0.5f;
					for (int m = 0; m < 3; m++)
						d += fabsf(x.t[m] - y.t[m]);
					for (int m = 0; m < 9; m++)
						d += fabsf(x.r[m] - y.r[m]) * 0.25f;
					if (!rpa || d < best)
					{
						rpa = &x;
						rpc = &y;
						va = &a[ka];
						best = d;
					}
				}
		// Geometry the game moves itself (the rings): the same corner, blended
		// in 3D before the perspective.
		bool line = false;
		if (!rpa)
		{
			for (int i = 0; i < INTERP_CANDS; i++)
				for (int j = 0; j < INTERP_CANDS; j++)
				{
					const InterpRec& x = ra[k * INTERP_CANDS + i];
					const InterpRec& y = rc[k * INTERP_CANDS + j];
					if (x.h < 0.0f || y.h < 0.0f)
						continue;
					float vx[3], vy[3];
					RecToView(x, vx);
					RecToView(y, vy);
					const float d = fabsf(vx[0] - vy[0]) + fabsf(vx[1] - vy[1]) + fabsf(vx[2] - vy[2]);
					if (!rpa || d < best)
					{
						rpa = &x;
						rpc = &y;
						va = &a[k];
						best = d;
					}
				}
			if (!rpa)
				return false;
			line = true;
		}
		float p[3], pa[3], pc[3], xt[2], xa[2], xc[2];
		const float h = (*rpa).h + ((*rpc).h - (*rpa).h) * t;
		const float ofx = (*rpa).ofx + ((*rpc).ofx - (*rpa).ofx) * t;
		const float ofy = (*rpa).ofy + ((*rpc).ofy - (*rpa).ofy) * t;
		RecToView((*rpa), pa);
		RecToView((*rpc), pc);
		if (line)
		{
			for (int i = 0; i < 3; i++)
				p[i] = pa[i] + (pc[i] - pa[i]) * t;
		}
		else if (!LainInterp_Screw((*rpa), (*rpc), t, p))
			return false;
		if (!LainInterp_Project(p, h, ofx, ofy, xt) ||
			!LainInterp_Project(pa, (*rpa).h, (*rpa).ofx, (*rpa).ofy, xa) ||
			!LainInterp_Project(pc, (*rpc).h, (*rpc).ofx, (*rpc).ofy, xc))
			return false;
		// The GTE clamps the perspective of vertices close to the camera; there
		// the exact projection is not where the PS1 drew them.
		if (fabsf(xa[0] + ox - va->x) > 2.0f || fabsf(xa[1] + oy - va->y) > 2.0f ||
			fabsf(xc[0] + ox - c[k].x) > 2.0f || fabsf(xc[1] + oy - c[k].y) > 2.0f)
			return false;
		out[k][0] = xt[0] + ox;
		out[k][1] = xt[1] + oy;
	}
	for (int k = 0; k < n; k++)
		LainInterp_SetXY(c[k], out[k][0], out[k][1]);
	return true;
}

// With smooth motion the real frames are drawn at the exact projected
// positions too, not rounded to whole pixels, so they lie on the same path as
// the in-between frames.
static void LainInterp_Exact(int v0, int n, const InterpRec* r, float ox, float oy)
{
	float xy[8][2];
	for (int k = 0; k < n; k++)
	{
		float p[3];
		RecToView(r[k * INTERP_CANDS], p);
		if (!LainInterp_Project(p, r[k * INTERP_CANDS].h, r[k * INTERP_CANDS].ofx, r[k * INTERP_CANDS].ofy, xy[k]))
			return;
		xy[k][0] += ox;
		xy[k][1] += oy;
		const GrVertex& v = g_vertexBuffer[v0 + k];
		if (fabsf(xy[k][0] - v.x) > 2.0f || fabsf(xy[k][1] - v.y) > 2.0f)
			return; // not this vertex after all
	}
	for (int k = 0; k < n; k++)
		LainInterp_SetXY(g_vertexBuffer[v0 + k], xy[k][0], xy[k][1]);
}

void LainInterp_NotePrim(const void* base, const void* prim, int v0, int n)
{
	if (!LainInterp_Recording() || n <= 0)
		return;
	const uint64_t tag = LainGs_PacketTag(base);
	uint64_t id;
	if (tag)
		id = tag + (uint64_t)((const char*)prim - (const char*)base);
	else
		id = (uint64_t)(uintptr_t)prim * 0x9E3779B97F4A7C15ull; // untagged: by address

	float ox, oy;
	DrawEnvOffset(ox, oy);
	int r0 = (int)s_irecs.size();

	// A TMD polygon: its own vertices, in the order the renderer splits it
	// (a quad becomes triangles p0 p1 p2 and p1 p2 p3).
	{
		static const int tri[3] = { 0, 1, 2 }, quad[6] = { 0, 1, 2, 1, 2, 3 };
		auto it = s_ipackets.find(prim);
		if (it != s_ipackets.end() && s_istamp - it->second.stamp <= 1 &&
			((it->second.nv == 3 && n == 3) || (it->second.nv == 4 && n == 6)))
		{
			const InterpPacket& p = it->second;
			const int* order = p.nv == 3 ? tri : quad;
			bool ok = true;
			for (int k = 0; k < n && ok; k++)
			{
				const GrVertex& v = g_vertexBuffer[v0 + k];
				ok = v.x - (int)ox == p.sxy[order[k]][0] && v.y - (int)oy == p.sxy[order[k]][1];
			}
			if (ok)
			{
				InterpRec none;
				none.h = -1.0f;
				for (int k = 0; k < n; k++)
				{
					s_irecs.push_back(p.v[order[k]]);
					for (int i = 1; i < INTERP_CANDS; i++)
						s_irecs.push_back(none);
				}
				s_ipending.push_back({ id, v0, n, r0, false, ox, oy });
				LainInterp_Exact(v0, n, &s_irecs[r0], ox, oy);
				return;
			}
		}
	}

	// Anything else the GTE projected: found by screen position. Several
	// vertices can land on one pixel; the replay picks the one that is the same
	// model vertex in both frames.
	for (int k = 0; k < n; k++)
	{
		const GrVertex& v = g_vertexBuffer[v0 + k];
		const int sx = v.x - (int)ox, sy = v.y - (int)oy;
		int found = 0;
		for (int m = 0; m < 2 && n <= 8; m++)
		{
			const auto& map = s_iproj[(s_istamp + m) & 1];
			auto it = map.find(LainInterp_ProjKey(sx, sy));
			if (it == map.end())
				continue;
			for (int i = 0; i < it->second.n && found < INTERP_CANDS; i++)
				s_irecs.push_back(it->second.c[i]), found++;
		}
		if (!found)
		{
			s_irecs.resize(r0);
			r0 = -1;
			break;
		}
		InterpRec none;
		none.h = -1.0f; // unused slot
		for (; found < INTERP_CANDS; found++)
			s_irecs.push_back(none);
	}
	s_ipending.push_back({ id, v0, n, r0, tag && LainGs_PacketIs2D(base), ox, oy });
	if (r0 >= 0)
		LainInterp_Exact(v0, n, &s_irecs[r0], ox, oy);
}

void LainInterp_NoteBatch()
{
	if (!LainInterp_Recording() || g_vertexIndex <= 0)
		return;
	InterpScene& sc = s_iscenes[s_icur];
	if (sc.batches.size() >= INTERP_MAX_BATCHES)
	{
		sc.broken = true;
		return;
	}
	InterpBatch b;
	b.verts.assign(g_vertexBuffer, g_vertexBuffer + g_vertexIndex);
	b.splits.assign(g_splits + 1, g_splits + 1 + g_splitIndex);
	b.groups = s_ipending;
	b.recs = s_irecs;
	s_istamp++;
	s_iproj[s_istamp & 1].clear();
	b.snap = (int)sc.batches.size();
	GR_InterpSnapshotVRAM(b.snap);
	sc.ops.push_back({ (int)sc.batches.size(), 0, 0, 0, 0, 0, 0, 0 });
	sc.batches.push_back(std::move(b));
}

extern "C" void LainInterp_NoteClear(int x, int y, int w, int h, u_char r, u_char g, u_char b)
{
	if (!LainInterp_Recording())
		return;
	s_iscenes[s_icur].ops.push_back({ -1, x, y, w, h, r, g, b });
}

// PsyX_BeginScene, after the VRAM background and the background clear.
// A new picture uploaded into a primitive's texture (an animation frame, a movie
// frame) is a cut, not motion: those primitives are drawn where they are now.
extern "C" void LainInterp_NoteUpload(int x, int y, int w, int h)
{
	if (!g_cfg_interpolate || s_ireplaying)
		return;
	RECT16 r;
	r.x = (short)x; r.y = (short)y; r.w = (short)w; r.h = (short)h;
	InterpScene& sc = s_iscenes[s_icur];
	(sc.open ? sc.uploads : s_iuploads).push_back(r);
}

static bool LainInterp_TextureUploaded(const GrVertex* v, int n, const InterpScene& a, const InterpScene& b)
{
	int umin = 255, vmin = 255, umax = 0, vmax = 0;
	for (int k = 0; k < n; k++)
	{
		if (v[k].u < umin) umin = v[k].u;
		if (v[k].u > umax) umax = v[k].u;
		if (v[k].v < vmin) vmin = v[k].v;
		if (v[k].v > vmax) vmax = v[k].v;
	}
	if (umax == 0 && vmax == 0)
		return false; // untextured
	// Texel columns as 16-bit VRAM words; wider than needed for 4/8-bit textures.
	const int page = (int)v[0].page;
	const int x0 = (page % 16) * 64 + umin, x1 = (page % 16) * 64 + umax;
	const int y0 = (page / 16) * 256 + vmin, y1 = (page / 16) * 256 + vmax;
	const InterpScene* scenes[2] = { &a, &b };
	for (const InterpScene* sc : scenes)
		for (const RECT16& r : sc->uploads)
			if (x0 < r.x + r.w && r.x <= x1 && y0 < r.y + r.h && r.y <= y1)
				return true;
	return false;
}

extern "C" void LainInterp_BeginScene()
{
	if (!g_cfg_interpolate)
	{
		// Forget recorded scenes, so turning it back on never blends with an old one.
		s_iscenes[0].ended = s_iscenes[1].ended = false;
		s_iscenes[0].open = s_iscenes[1].open = false;
		s_iseq++;
		return;
	}
	if (s_iscenes[s_icur].ended)
		s_icur ^= 1; // the ended scene becomes the previous one
	InterpScene& sc = s_iscenes[s_icur];
	sc.ops.clear();
	sc.batches.clear();
	sc.seq = ++s_iseq;
	sc.open = true;
	sc.ended = false;
	sc.broken = false;
	sc.uploads.swap(s_iuploads);
	s_iuploads.clear();
	s_ipending.clear();
	GR_InterpSnapshotBase();
}

extern "C" void LainInterp_EndScene()
{
	InterpScene& sc = s_iscenes[s_icur];
	if (!sc.open)
		return;
	sc.open = false;
	sc.ended = true;
}

static void LainInterp_IndexPrev(const InterpScene& prev)
{
	if (s_iprevIndexedSeq == (int)prev.seq)
		return;
	s_iprevIndexedSeq = (int)prev.seq;
	s_iprevIndex.clear();
	for (int b = 0; b < (int)prev.batches.size(); b++)
	{
		const std::vector<InterpGroup>& gs = prev.batches[b].groups;
		for (int g = 0; g < (int)gs.size(); g++)
		{
			// a repeated id is ambiguous: don't blend it
			auto ins = s_iprevIndex.emplace(gs[g].id, std::make_pair(b, g));
			if (!ins.second)
				ins.first->second = std::make_pair(-1, -1);
		}
	}
}

// Draws the current scene with primitives blended from the previous scene by t
// (0..1) and shows it. Returns 1 if a frame was shown.
extern "C" int LainInterp_Present(float t)
{
	if (!g_cfg_interpolate || t <= 0.0f || t >= 1.0f)
		return 0;
	const InterpScene& cur = s_iscenes[s_icur];
	const InterpScene& prev = s_iscenes[s_icur ^ 1];
	if (!cur.open || cur.broken || cur.batches.empty() || !prev.ended || prev.broken ||
		prev.seq + 1 != cur.seq || g_splitIndex > 0)
		return 0;
	// Not while the renderer is drawing into VRAM (render to texture): the replay
	// would leave that state behind the game's back.
	if (g_PreviousOffscreenState)
		return 0;
	{
		bool onScreen = false;
		for (const InterpBatch& b : cur.batches)
			for (const GPUDrawSplit& sp : b.splits)
				onScreen = onScreen || sp.drawenv.dfe;
		if (!onScreen)
			return 0;
	}

	LainInterp_IndexPrev(prev);
	s_ireplaying = true;
	GR_InterpBegin();

	for (const InterpOp& op : cur.ops)
	{
		if (op.batch < 0)
		{
			GR_Clear(op.x, op.y, op.w, op.h, op.r, op.g, op.b);
			continue;
		}
		const InterpBatch& b = cur.batches[op.batch];
		s_iverts = b.verts;
		for (const InterpGroup& g : b.groups)
		{
			auto it = s_iprevIndex.find(g.id);
			if (it == s_iprevIndex.end() || it->second.first < 0)
				continue;
			const InterpBatch& pb = prev.batches[it->second.first];
			const InterpGroup& pg = pb.groups[it->second.second];
			if (pg.n != g.n || g.v0 + g.n > (int)s_iverts.size() || pg.v0 + pg.n > (int)pb.verts.size())
				continue;
#if !USE_PGXP
			// 3D geometry follows its transforms, whatever happens to its texture.
			if (g.r0 >= 0 && pg.r0 >= 0 &&
				LainInterp_ScrewGroup(&pb.verts[pg.v0], &pb.recs[pg.r0], &s_iverts[g.v0], &b.recs[g.r0], g.n, t, g.ox, g.oy))
				continue;
#endif
			// Something that jumped (a different use of the same packet, a cut)
			// is drawn where it is now rather than swept across the screen.
			bool jump = false;
			for (int k = 0; k < g.n && !jump; k++)
			{
				const GrVertex& a = pb.verts[pg.v0 + k];
				const GrVertex& c = s_iverts[g.v0 + k];
				jump = fabsf((float)c.x - (float)a.x) > 96.0f || fabsf((float)c.y - (float)a.y) > 96.0f;
			}
			if (jump || LainInterp_TextureUploaded(&s_iverts[g.v0], g.n, prev, cur))
				continue;
			// A 2D picture with a new image (Lain's animation) steps like on the
			// PS1: the offsets that come with each image are part of it, and
			// sliding them reads as a wobble. One that only moves glides.
			if (g.sprite)
			{
				bool same = true;
				for (int k = 0; k < g.n && same; k++)
				{
					const GrVertex& a = pb.verts[pg.v0 + k];
					const GrVertex& c = s_iverts[g.v0 + k];
					same = a.u == c.u && a.v == c.v && a.page == c.page && a.clut == c.clut;
				}
				if (!same)
					continue;
			}
			for (int k = 0; k < g.n; k++)
			{
				const GrVertex& a = pb.verts[pg.v0 + k];
				GrVertex& c = s_iverts[g.v0 + k];
#if USE_PGXP
				c.x = a.x + (c.x - a.x) * t;
				c.y = a.y + (c.y - a.y) * t;
				c.z = a.z + (c.z - a.z) * t;
#else
				LainInterp_SetXY(c, a.x + (c.x - a.x) * t, a.y + (c.y - a.y) * t);
#endif
			}
		}
		GR_UpdateVertexBuffer(s_iverts.data(), (int)s_iverts.size());
		for (GPUDrawSplit split : b.splits)
		{
			if (!split.drawenv.dfe)
				continue; // drawing into VRAM (render to texture): not part of the picture
			split.textureId = GR_InterpTexture(split.textureId, b.snap);
			DrawSplit(split);
		}
	}

	GR_InterpEnd();
	if (g_lain_onInterpPresent)
		g_lain_onInterpPresent();
	GR_SwapWindow();
	GR_InterpRestore();
	s_ireplaying = false;
	return 1;
}
