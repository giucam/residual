
#ifndef AGL_MANAGER_H
#define AGL_MANAGER_H

#include "common/singleton.h"

#include "math/mathfwd.h"

#include "graphics/agl/bitmap2d.h"

namespace Common {
class String;
}

namespace Graphics {
class PixelBuffer;
}

namespace AGL {

class Renderer;
class Target;
class Mesh;
class Texture;

class Manager : public Common::Singleton<Manager> {
public:
	Manager();
	void init(const Common::String &renderer);

	Target *setupScreen(int screenW, int screenH, bool fullscreen, int bpp);
	Target *getTarget() const { return _target; }

	void setupCamera(float fov, float nclip, float fclip, float roll);
	void positionCamera(const Math::Vector3d &pos, const Math::Vector3d &interest);
	void flipBuffer();

	Bitmap2D *createBitmap2D(Bitmap2D::Type type, const Graphics::PixelBuffer &buf, int width, int height);
	Texture *createTexture(const Graphics::PixelBuffer &buf, int width, int height);
	Mesh *createMesh();

// private:
	Renderer *_renderer;
	Target *_target;
};

#define AGLMan AGL::Manager::instance()

}

#endif
