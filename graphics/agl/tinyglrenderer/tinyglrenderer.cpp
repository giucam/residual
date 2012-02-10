
#include "common/system.h"
#include "common/rect.h"
#include "common/foreach.h"

#include "graphics/pixelbuffer.h"

#include "math/vector3d.h"
#include "math/rect2d.h"

#include "graphics/agl/manager.h"
#include "graphics/agl/bitmap2d.h"
#include "graphics/agl/target.h"
#include "graphics/agl/shadowplane.h"
#include "graphics/agl/texture.h"
#include "graphics/agl/label.h"
#include "graphics/agl/font.h"
#include "graphics/agl/primitive.h"

#include "graphics/tinygl/zgl.h"

#include "graphics/agl/tinyglrenderer/tinyglrenderer.h"
#include "graphics/agl/tinyglrenderer/tglmesh.h"
#include "graphics/agl/tinyglrenderer/tglbitmap2d.h"

namespace AGL {

class TGLPrimitive : public Primitive {
public:
	TGLPrimitive()
		: Primitive() {

	}

	void draw(float x, float y) {
		const int screenWidth = AGLMan.getTarget()->getWidth();
		const int screenHeight = AGLMan.getTarget()->getHeight();

		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglOrtho(0, screenWidth, screenHeight, 0, 0, 1);
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglTranslatef(x, y, 0);

		tglDisable(TGL_LIGHTING);
		tglDisable(TGL_DEPTH_TEST);

		TGLenum mode;
		switch(getMode()) {
			case Points:
				mode = TGL_POINTS;
				break;
			case Lines:
				mode = TGL_LINES;
				break;
			case LineLoop:
				mode = TGL_LINE_LOOP;
				break;
			case Quads:
				mode = TGL_QUADS;
		}

		const bool globalColor = useGlobalColor();
		if (globalColor) {
			tglColor4ub(getGlobalColor().getRed(), getGlobalColor().getGreen(), getGlobalColor().getBlue(), getGlobalColor().getAlpha());
		}

		uint num = getNumSubs();
		for (uint i = 0; i < num; ++i) {
			const byte *colors = getColorPointer(i);
			const float *vertices = getVertexPointer(i);
			const uint numVertices = getNumVertices(i);

			tglBegin(mode);
			for (uint j = 0; j < numVertices; ++j) {
				if (!globalColor) {
					tglColor4ub(colors[0], colors[1], colors[2], colors[3]);
					colors += 4;
				}
				tglVertex2f(vertices[0], vertices[1]);
				vertices += 2;
			}
			tglEnd();
		}

		tglColor3f(1.0f, 1.0f, 1.0f);

		tglEnable(TGL_DEPTH_TEST);
		tglEnable(TGL_LIGHTING);
	}
};

class TGLFont : public Font {
public:
	TGLFont(FontMetric *metric, const Graphics::PixelBuffer &buffer, int width, int height)
		: Font(metric),
		  _buffer(buffer),
		  _width(width),
		  _height(height) {
	}

	int getCharPos(unsigned char c, int line) const {
		Math::Rect2d rect(getMetric()->getCharTextureRect(c));
		int cx = rect.getTopLeft().getX() * _width;
		int cy = rect.getTopLeft().getY() * _height;

		return (cy + line) * _width + cx;
	}

	Graphics::PixelBuffer _buffer;
	int _width;
	int _height;
};

class TGLLabel : public Label {
public:
	TGLLabel()
		: Label() {

	}

	void draw(int x, int y) const {
		const int numLines = getNumLines();
		const TGLFont *font = static_cast<TGLFont *>(getFont());
		const FontMetric *metric = font->getMetric();
		const Graphics::Color &fgColor = getTextColor();
		Graphics::PixelBuffer src(font->_buffer);

		for (int j = 0; j < numLines; j++) {
			const Common::String &currentLine = getLine(j);

			int width = metric->getStringLength(currentLine) + 1;
			int height = metric->getHeight();

			Common::Rect lineRect = getLineRect(j);

			int lx = x + lineRect.left;
			int ly = y + lineRect.top;

			uint8 *_textBitmap = new uint8[height * width];
			memset(_textBitmap, 0, height * width);

			int startOffset = 0;
			for (unsigned int d = 0; d < currentLine.size(); d++) {
				unsigned char ch = currentLine[d];

				Math::Rect2d quadrect = metric->getCharQuadRect(ch);

				int8 startingLine = quadrect.getTopLeft().getY();
				int8 startingCol = quadrect.getTopLeft().getX();
				int32 charDataWidth = quadrect.getWidth();
				int32 charWidth = metric->getCharWidth(ch);

				int32 charDataHeight = quadrect.getHeight();

				//FIXME TODO: save this into a bitmap.
				for (int i = 0; i < charDataHeight; ++i) {
					for (int row = 0; row < charDataWidth; ++row) {
						int pos = (i + startingLine + ly) * 640 + startOffset + startingCol + lx + row;
						uint8 a, r, g, b;
						src.getARGBAt(font->getCharPos(ch, i) + row, a, r, g, b);
						a = (a * fgColor.getAlpha()) / 256;
						r = (r * fgColor.getRed()) / 256;
						g = (g * fgColor.getGreen()) / 256;
						b = (b * fgColor.getBlue()) / 256;
						if (a > 0.f)
							_renderer->_zb->pbuf.setPixelAt(pos, r, g, b);
					}
				}

				startOffset += charWidth;
			}
		}
	}

	TinyGLRenderer *_renderer;
};

class TGLLight : public Light {
public:
	TGLLight(Light::Type type)
		: Light(type),
		  _id(-1) { }

	void enable() {
		if (_id == -1) {
			// Find a free id.
			int max;
			tglGetIntegerv(TGL_MAX_LIGHTS, &max);
			for (int i = 0; i < max; ++i) {
				if (!tglIsEnabled(TGL_LIGHT0 + i)) {
					_id = i;
					break;
				}
			}
		}

		if (_id == -1) {
			warning("Cannot init light.");
			return;
		}
// 		assert(_id > -1);

		tglEnable(TGL_LIGHTING);
		float lightColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		float lightPos[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		float lightDir[] = { 0.0f, 0.0f, -1.0f };

		float intensity = getIntensity() / 1.3f;
		lightColor[0] = ((float)getColor().getRed() / 15.0f) * intensity;
		lightColor[1] = ((float)getColor().getGreen() / 15.0f) * intensity;
		lightColor[2] = ((float)getColor().getBlue() / 15.0f) * intensity;

		if (getType() == Light::Point) {
			memcpy(lightPos, getPosition().getData(), 3 * sizeof(float));
		} else if (getType() == Light::Directional) {
			lightPos[0] = -getDirection().x();
			lightPos[1] = -getDirection().y();
			lightPos[2] = -getDirection().z();
			lightPos[3] = 0;
		} else if (getType() == Light::Spot) {
			memcpy(lightPos, getPosition().getData(), 3 * sizeof(float));
			memcpy(lightDir, getDirection().getData(), 3 * sizeof(float));
		}

		tglDisable(TGL_LIGHT0 + _id);
		tglLightfv(TGL_LIGHT0 + _id, TGL_DIFFUSE, lightColor);
		tglLightfv(TGL_LIGHT0 + _id, TGL_POSITION, lightPos);
		tglLightfv(TGL_LIGHT0 + _id, TGL_SPOT_DIRECTION, lightDir);
		tglLightf(TGL_LIGHT0 + _id, TGL_SPOT_CUTOFF, getCutoff());
		tglEnable(TGL_LIGHT0 + _id);
	}
	void disable() {
		if (_id < 0)
			return;

		tglDisable(TGL_LIGHT0 + _id);
		_id = -1;
	}

	int _id;
};

static void tglShadowProjection(Math::Vector3d light, Math::Vector3d plane, Math::Vector3d normal, bool dontNegate) {
	// Based on GPL shadow projection example by
	// (c) 2002-2003 Phaetos <phaetos@gaffga.de>
	float d, c;
	float mat[16];
	float nx, ny, nz, lx, ly, lz, px, py, pz;

	nx = normal.x();
	ny = normal.y();
	nz = normal.z();
	// for some unknown for me reason normal need negation
	if (!dontNegate) {
		nx = -nx;
		ny = -ny;
		nz = -nz;
	}
	lx = light.x();
	ly = light.y();
	lz = light.z();
	px = plane.x();
	py = plane.y();
	pz = plane.z();

	d = nx * lx + ny * ly + nz * lz;
	c = px * nx + py * ny + pz * nz - d;

	mat[0] = lx * nx + c;
	mat[4] = ny * lx;
	mat[8] = nz * lx;
	mat[12] = -lx * c - lx * d;

	mat[1] = nx * ly;
	mat[5] = ly * ny + c;
	mat[9] = nz * ly;
	mat[13] = -ly * c - ly * d;

	mat[2] = nx * lz;
	mat[6] = ny * lz;
	mat[10] = lz * nz + c;
	mat[14] = -lz * c - lz * d;

	mat[3] = nx;
	mat[7] = ny;
	mat[11] = nz;
	mat[15] = -d;

	tglMultMatrixf(mat);
}

class TGLShadowPlane : public ShadowPlane {
public:
	TGLShadowPlane()
		: ShadowPlane() {

		_shadowMaskSize = AGLMan.getTarget()->getWidth() * AGLMan.getTarget()->getHeight();
		_shadowMask = new byte[_shadowMaskSize];

		tglEnable(TGL_SHADOW_MASK_MODE);
		memset(_shadowMask, 0, _shadowMaskSize);

		tglSetShadowMaskBuf(_shadowMask);
		foreach (const Sector &s, getSectors()) {
			tglBegin(TGL_POLYGON);
			int num = s._vertices.size();
			for (int k = 0; k < num; k++) {
				tglVertex3fv(s._vertices[k].getData());
			}
			tglEnd();
		}
		tglSetShadowMaskBuf(NULL);
		tglDisable(TGL_SHADOW_MASK_MODE);
	}

	~TGLShadowPlane() {
		delete[] _shadowMask;
	}

	void enable(const Math::Vector3d &pos, const Graphics::Color &color) {
		tglSetShadowColor(color.getRed() / 255.f, color.getGreen() / 255.f, color.getBlue() / 255.f);
		tglSetShadowMaskBuf(_shadowMask);
		tglPushMatrix();
		tglShadowProjection(pos, getSectors()[0]._vertices[0], getSectors()[0]._normal, false);
	}

	void disable() {
		tglPopMatrix();
		tglSetShadowMaskBuf(NULL);
	}

	byte *_shadowMask;
	int _shadowMaskSize;

};

class TGLTarget : public Target {
public:
	TGLTarget(int width, int height, int bpp, const Graphics::PixelFormat &format)
		: Target(width, height, bpp) {

		_screenSize = width * height;
		_storedDisplay.create(format, _screenSize, DisposeAfterUse::YES);
		_storedDisplay.clear(_screenSize);
	}
	void clear() {
		_renderer->_zb->pbuf.clear(_screenSize);
		memset(_renderer->_zb->zbuf, 0, _screenSize * 2);
		memset(_renderer->_zb->zbuf2, 0, _screenSize * 4);
	}

	void dim(float amount) {
		for (int l = 0; l < _screenSize; l++) {
			uint8 r, g, b;
			_storedDisplay.getRGBAt(l, r, g, b);
			uint32 color = (r + g + b) * amount;
			_storedDisplay.setPixelAt(l, color, color, color);
		}
	}

	void dimRegion(int x, int y, int w, int h, float level) {

	}

	void storeContent() {
		_storedDisplay.copyBuffer(0, _screenSize, _renderer->_zb->pbuf);
	}
	void restoreContent() {
		_renderer->_zb->pbuf.copyBuffer(0, _screenSize, _storedDisplay);
	}

	Graphics::Surface *getScreenshot(const Graphics::PixelFormat &format, int width, int height) const {

	}

	TinyGLRenderer *_renderer;
	int _screenSize;
	Graphics::PixelBuffer _storedDisplay;
};

class TGLTexture : public Texture {
public:
	TGLTexture(const Graphics::PixelBuffer &buf, int width, int height)
		: Texture(buf.getFormat(), width, height) {
		TGLuint format = 0;
		TGLuint internalFormat = 0;
// 		if (material->_colorFormat == BM_RGBA) {
			format = TGL_RGBA;
			internalFormat = TGL_RGBA;
// 		} else {	// The only other colorFormat we load right now is BGR
// 		format = GL_BGR;
// 		internalFormat = GL_RGB;
// 		}

		tglGenTextures(1, &_texId);
		tglBindTexture(TGL_TEXTURE_2D, _texId);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_S, TGL_REPEAT);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_T, TGL_REPEAT);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MAG_FILTER, TGL_LINEAR);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MIN_FILTER, TGL_LINEAR);
		tglTexImage2D(TGL_TEXTURE_2D, 0, 3, width, height, 0, format, TGL_UNSIGNED_BYTE, buf.getRawBuffer());
	}

	void bind() {
		tglBindTexture(TGL_TEXTURE_2D, _texId);
	}

	TGLuint _texId;
};

// below funcs lookAt, transformPoint and tgluProject are from Mesa glu sources
static void lookAt(TGLfloat eyex, TGLfloat eyey, TGLfloat eyez, TGLfloat centerx,
				   TGLfloat centery, TGLfloat centerz, TGLfloat upx, TGLfloat upy, TGLfloat upz) {
	TGLfloat m[16];
	TGLfloat x[3], y[3], z[3];
	TGLfloat mag;

	z[0] = eyex - centerx;
	z[1] = eyey - centery;
	z[2] = eyez - centerz;
	mag = sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
	if (mag) {
		z[0] /= mag;
		z[1] /= mag;
		z[2] /= mag;
	}

	y[0] = upx;
	y[1] = upy;
	y[2] = upz;

	x[0] = y[1] * z[2] - y[2] * z[1];
	x[1] = -y[0] * z[2] + y[2] * z[0];
	x[2] = y[0] * z[1] - y[1] * z[0];

	y[0] = z[1] * x[2] - z[2] * x[1];
	y[1] = -z[0] * x[2] + z[2] * x[0];
	y[2] = z[0] * x[1] - z[1] * x[0];

	mag = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
	if (mag) {
		x[0] /= mag;
		x[1] /= mag;
		x[2] /= mag;
	}

	mag = sqrt(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
	if (mag) {
		y[0] /= mag;
		y[1] /= mag;
		y[2] /= mag;
	}

#define M(row,col)  m[col * 4 + row]
	M(0, 0) = x[0];
	M(0, 1) = x[1];
	M(0, 2) = x[2];
	M(0, 3) = 0.0f;
	M(1, 0) = y[0];
	M(1, 1) = y[1];
	M(1, 2) = y[2];
	M(1, 3) = 0.0f;
	M(2, 0) = z[0];
	M(2, 1) = z[1];
	M(2, 2) = z[2];
	M(2, 3) = 0.0f;
	M(3, 0) = 0.0f;
	M(3, 1) = 0.0f;
	M(3, 2) = 0.0f;
	M(3, 3) = 1.0f;
#undef M
	tglMultMatrixf(m);

	tglTranslatef(-eyex, -eyey, -eyez);
}


TinyGLRenderer::TinyGLRenderer() {
}

Target *TinyGLRenderer::setupScreen(int screenW, int screenH, bool fullscreen, int bpp) {
	Graphics::PixelBuffer buf = g_system->setupScreen(screenW, screenH, fullscreen, false);
// 	byte *buffer = buf.getRawBuffer();

// 	_pixelFormat = buf.getFormat();
	_zb = TinyGL::ZB_open(screenW, screenH, buf);
	TinyGL::glInit(_zb);

// 	_screenSize = 640 * 480 * _pixelFormat.bytesPerPixel;
// 	_storedDisplay.create(_pixelFormat, 640 * 480, DisposeAfterUse::YES);
// 	_storedDisplay.clear(640 * 480);

// 	_currentShadowArray = NULL;

	TGLfloat ambientSource[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	tglLightModelfv(TGL_LIGHT_MODEL_AMBIENT, ambientSource);

	TGLTarget *t = new TGLTarget(screenW, screenH, bpp, buf.getFormat());
	t->_renderer = this;
	return t;
}

void TinyGLRenderer::setupCamera(float fov, float nclip, float fclip, float roll) {
	tglMatrixMode(TGL_PROJECTION);
	tglLoadIdentity();

	float right = nclip * tan(fov / 2 * (LOCAL_PI / 180));
	tglFrustum(-right, right, -right * 0.75, right * 0.75, nclip, fclip);

	tglMatrixMode(TGL_MODELVIEW);
	tglLoadIdentity();

	tglRotatef(roll, 0, 0, -1);
}

void TinyGLRenderer::positionCamera(const Math::Vector3d &pos, const Math::Vector3d &interest) {
	Math::Vector3d up_vec(0, 0, 1);

	if (pos.x() == interest.x() && pos.y() == interest.y())
		up_vec = Math::Vector3d(0, 1, 0);

	lookAt(pos.x(), pos.y(), pos.z(), interest.x(), interest.y(), interest.z(), up_vec.x(), up_vec.y(), up_vec.z());
}

void TinyGLRenderer::enableLighting() {
	tglEnable(TGL_LIGHTING);
}

void TinyGLRenderer::disableLighting() {
	tglDisable(TGL_LIGHTING);
}

Bitmap2D *TinyGLRenderer::createBitmap2D(Bitmap2D::Type type, const Graphics::PixelBuffer &buf, int width, int height) {
	TGLBitmap2D *b = new TGLBitmap2D(this, type, buf, width, height);
	return b;
}

Texture *TinyGLRenderer::createTexture(const Graphics::PixelBuffer &buf, int width, int height) {
	return new TGLTexture(buf, width, height);
}

Mesh *TinyGLRenderer::createMesh() {
	return new TGLMesh();
}

Light *TinyGLRenderer::createLight(Light::Type type) {
	return new TGLLight(type);
}

Primitive *TinyGLRenderer::createPrimitive() {
	return new TGLPrimitive();
}

ShadowPlane *TinyGLRenderer::createShadowPlane() {
	return new TGLShadowPlane();
}

Font *TinyGLRenderer::createFont(FontMetric *metric, const Graphics::PixelBuffer &buf, int width, int height) {
	return new TGLFont(metric, buf, width, height);
}

Label *TinyGLRenderer::createLabel() {
	TGLLabel *l = new TGLLabel();
	l->_renderer = this;
	return l;
}

void TinyGLRenderer::pushMatrix() {
	tglMatrixMode(TGL_MODELVIEW);
	tglPushMatrix();
}
void TinyGLRenderer::translate(float x, float y, float z) {
	tglTranslatef(x, y, z);
}
void TinyGLRenderer::rotate(float deg, float x, float y, float z) {
	tglRotatef(deg, x, y, z);
}
void TinyGLRenderer::popMatrix() {
	tglPopMatrix();
}

const char *TinyGLRenderer::prettyString() const {
	return "ResidualVM: Software 3D Renderer";
}




class TinyGLPlugin : public RendererPluginObject {
public:
	TinyGLRenderer *createInstance() {
		return new TinyGLRenderer();
	}

	const char *getName() const {
		return "TinyGL";
	}
};

}

REGISTER_PLUGIN_STATIC(TinyGL, PLUGIN_TYPE_AGL_RENDERER, AGL::TinyGLPlugin);
