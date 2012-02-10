
#ifndef AGL_TGLMESH_H
#define AGL_TGLMESH_H

#include "common/array.h"

#include "math/vector3d.h"

#include "graphics/agl/mesh.h"
#include "graphics/agl/meshface.h"

namespace AGL {

class MeshFace;
class Texture;

class TGLMesh : public Mesh {
public:
	void pushVertex(float x, float y, float z);
	void pushTexVertex(float u, float v);
	void pushNormal(float x, float y, float z);

	MeshFace *createFace();

	void draw(Texture *texture);

	bool calculate2DBoundingBox(Common::Rect *rect) const;

	Common::Array<float> _vertices;
	Common::Array<float> _textures;
	Common::Array<float> _normals;
};

class TGLMeshFace : public MeshFace {
public:
	TGLMeshFace(TGLMesh *parent);

	void prepare(uint size);
	void setNormal(float x, float y, float z);

	void vertex(int index);
	void texture(int index);
	void normal(int index);

	void draw(Texture *texture);

	TGLMesh *_parent;
	Math::Vector3d _normal;
	int *_vertices;
	int *_textures;
	int *_normals;
	int _i;
};

}

#endif
