
#ifndef AGL_SHADOWPLANE_H
#define AGL_SHADOWPLANE_H

#include "common/array.h"

#include "math/vector3d.h"

namespace Graphics {
class Color;
}

namespace AGL {

class ShadowPlane {
public:
	typedef Common::Array<Math::Vector3d> Vertices;

	ShadowPlane();
	virtual ~ShadowPlane();

	void addSector(const Vertices &vertices);

	virtual void enable(const Math::Vector3d &pos, const Graphics::Color &color) = 0;
	virtual void disable() = 0;

protected:
	struct Sector {
		Vertices _vertices;
		Math::Vector3d _normal;
	};

	inline const Common::Array<Sector> &getSectors() const { return _sectors; }

private:
	Common::Array<Sector> _sectors;
};

}

#endif
