#include "VolumeNode.h"
#include "ConvexShape.h" // TODO remove dependence on old ConvexShape
#include "Utilities.h" // Allows hashing of pairs
#include <limits>

using glm::dvec3;
using glm::vec3;
using std::vector;


VolumeNode::VolumeNode(std::vector<Polygon>& poly) {
	true_shape = poly ;


	std::vector<glm::dvec3> points;
	for (auto& face : true_shape) {
		for (auto& v : face.p) {
			points.emplace_back(v);
		}
	}

	// Get axis aligned bounding box of points
	dvec3 min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	dvec3 max(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
	for (auto& x : points) {
		min.x = fmin(min.x, x.x);
		min.y = fmin(min.y, x.y);
		min.z = fmin(min.z, x.z);
		max.x = fmax(max.x, x.x);
		max.y = fmax(max.y, x.y);
		max.z = fmax(max.z, x.z);
	}

	// cut away from box expanded from bounding box to build approximate convex hull
	hull_planes = RadialVolume::getHullPlanes(points, hull_faces, hull_detail);
	hull_volume = RadialVolume::last_hull_volume;
	hull_shape = ConvexShape::makeAxisAlignedBox(min * 2.0 - max, max * 2.0 - min).getPolygons(); // TODO remove dependence on old ConvexShape
	for (auto& plane : hull_planes) {
		hull_shape = Polygon::splitOnPlane(hull_shape, plane).first;
	}

}

// Split this node on the given plane
void VolumeNode::split(glm::dvec3 normal, float d){
	split_plane.first = normal ;
	split_plane.second = d;
	vector<Polygon> left_poly;
	vector<Polygon> right_poly;
	for (int k = 0; k < true_shape.size(); k++) {
		std::pair<Polygon, Polygon> ps = true_shape[k].splitOnPlane(split_plane);
		if (ps.first.p.size() > 0 && !ps.first.on_last_plane) {
			left_poly.push_back(ps.first);
		}
		if (ps.second.p.size() > 0 && !ps.second.on_last_plane) {
			right_poly.push_back(ps.second);
		}

	}
	inner = std::unique_ptr<VolumeNode>(new VolumeNode(left_poly));
	outer = std::unique_ptr<VolumeNode>(new VolumeNode(right_poly));
	leaf = false;
}

