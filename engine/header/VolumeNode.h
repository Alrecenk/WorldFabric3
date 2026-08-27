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
	float hull_volume = -1.0f;
	std::vector<std::pair<glm::dvec3, double>> hull_planes ;

	bool leaf = true;

	//These are only defined if it is not a leaf
	std::pair<glm::dvec3, double> split_plane ;
	std::unique_ptr<VolumeNode> inner, outer;
	
	
	
	static constexpr double EPSILON = 1e-4;

	// Used for hull detail of component pieces
	static inline int hull_faces = 12 ;
	static inline int hull_detail = 4 ;

	VolumeNode();

	VolumeNode(std::vector<Polygon>& poly);

	// Split this node on the given plane
	void split(glm::dvec3 normal, float d);

	static int getIndex(glm::dvec3& p, std::vector<glm::dvec3>& list);

	//Returns a plane through the deepest edge on the true shape whose neghboring faces point toward each other
	std::pair<glm::dvec3, double> getDeepestConcaveBisector();

};
#endif // #ifndef _VOLUME_NODE_H_