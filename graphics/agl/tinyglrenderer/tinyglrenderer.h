
#ifndef AGL_TINYGLRENDERER_H
#define AGL_TINYGLRENDERER_H

#include "graphics/tinygl/gl.h"

#include "graphics/agl/renderer.h"

namespace TinyGL {
struct ZBuffer;
}

namespace AGL {

class TGLTexture;

class TinyGLRenderer : public Renderer {
public:
	TinyGLRenderer();

	Target *setupScreen(int screenW, int screenH, bool fullscreen, int bpp);
	void setupCamera(float fov, float nclip, float fclip, float roll);
	void positionCamera(const Math::Matrix3x3 &worldRot, const Math::Vector3d &pos, const Math::Vector3d &interest);

	void enableLighting();
	void disableLighting();

	Bitmap2D *createBitmap2D(Bitmap2D::Type type, const Graphics::PixelBuffer &buf, int width, int height);
	Texture *createTexture(const Graphics::PixelBuffer &buf, int width, int height);
	Mesh *createMesh();
	Light *createLight(Light::Type type);
	Primitive *createPrimitive();
	ShadowPlane *createShadowPlane();
	Font *createFont(FontMetric *metric, const Graphics::PixelBuffer &buf, int width, int height);
	Label *createLabel();
	Sprite *createSprite(float width, float height);

	void pushMatrix();
	void translate(float x, float y, float z);
	void rotate(float deg, float x, float y, float z);
	void scale(float x, float y, float z);
	void popMatrix();

	Common::String prettyName() const;
	Common::String getName() const;
	bool isHardwareAccelerated() const;

	static TGLenum drawMode(DrawMode mode);

	TinyGL::ZBuffer *_zb;
	bool _shadowActive;
};

}

#endif
