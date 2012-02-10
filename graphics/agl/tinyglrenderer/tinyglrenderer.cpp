
#include "common/system.h"
#include "common/rect.h"
#include "common/foreach.h"

#include "graphics/pixelbuffer.h"

#include "math/vector3d.h"

#include "graphics/agl/manager.h"
#include "graphics/agl/bitmap2d.h"
#include "graphics/agl/target.h"
#include "graphics/agl/shadowplane.h"
#include "graphics/agl/texture.h"

#include "graphics/tinygl/zgl.h"

#include "graphics/agl/tinyglrenderer/tinyglrenderer.h"
#include "graphics/agl/tinyglrenderer/tglmesh.h"

namespace AGL {

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
		tglSetShadowColor(color.getRed(), color.getGreen(), color.getBlue());
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

class TGLBitmap2D : public Bitmap2D {
public:
	TGLBitmap2D(Bitmap2D::Type type, const Graphics::PixelBuffer &buf, int width, int height)
		: Bitmap2D(type, width, height) {
		_buf.create(buf.getFormat(), width * height, DisposeAfterUse::YES);
		_buf.copyBuffer(0, width * height, buf);
	}

	void draw(int x, int y) {
		_renderer->_zb->pbuf.copyBuffer(0, getWidth()*getHeight(), _buf);
	}

	TinyGLRenderer *_renderer;
	Graphics::PixelBuffer _buf;
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
	TGLBitmap2D *b = new TGLBitmap2D(type, buf, width, height);
	b->_renderer = this;
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
	return NULL;
}

ShadowPlane *TinyGLRenderer::createShadowPlane() {
	return new TGLShadowPlane();
}

Label *TinyGLRenderer::createLabel() {
	return NULL;
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
