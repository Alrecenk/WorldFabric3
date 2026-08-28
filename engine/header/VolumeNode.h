#ifndef _VOLUME_NODE_H_
#define _VOLUME_NODE_H_ 1


#include "RadialVolume.h"
#include "Polygon.h"
#include "GLTF.h"
#include <vector>
#include <memory>
#include <string>
#include "glm/glm.hpp"

class VolumeNode {
public:
	
	//Polygons cut from the original model
	std::vector<Polygon> true_shape;
	//approximate convex hull for this node
	std::vector<Polygon> hull_shape;
	double hull_volume = -1.0f;
	std::vector<std::pair<glm::dvec3, double>> hull_planes ;

	bool leaf = true;

	//These are only defined if it is not a leaf
	std::pair<glm::dvec3, double> split_plane ;
	std::unique_ptr<VolumeNode> inner, outer;
	
	glm::dvec3 min = glm::dvec3(FLT_MAX, FLT_MAX,FLT_MAX);
	glm::dvec3 max = glm::dvec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	
	static constexpr double EPSILON = 1e-4;

	// Used for hull detail of component pieces
	static inline int hull_faces = 10 ;
	static inline int hull_detail = 3 ;

	VolumeNode();

	VolumeNode(std::vector<Polygon>& poly);

	// Split this node on the given plane
	void split(glm::dvec3 normal, double d);


	void recurseToDepth(int depth) ;

	//Returns a plane through the deepest point within the convex hull pointing outward
	std::pair<glm::dvec3, double> getDeepCuttingPlane() ;


	void collectHulls(std::vector<std::vector<Polygon>>& hulls);


	std::vector<std::vector<Polygon>> getHulls();

};
#endif // #ifndef _VOLUME_NODE_H_