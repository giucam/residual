
#ifndef AGL_GLTARGET_H
#define AGL_GLTARGET_H

#include "graphics/agl/target.h"

namespace AGL {

class GLTarget : public Target {
public:
	GLTarget(int width, int height, int bpp);

	void clear();
	void dim(float amount);

	void storeContent();
	void restoreContent();

	byte *_storedDisplay;
};

}

#endif
