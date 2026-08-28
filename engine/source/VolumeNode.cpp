#include "VolumeNode.h"
#include "Utilities.h" // Allows hashing of pairs
#include "ConvexShape.h"


VolumeNode::VolumeNode(std::vector<Polygon>& poly) {
	true_shape = poly ;


	std::vector<glm::dvec3> points;
	for (auto& face : true_shape) {
		for (auto& v : face.p) {
			points.emplace_back(v);
		}
	}

	// Get axis aligned bounding box of points
	
	for (auto& x : points) {
		min.x = fmin(min.x, x.x);
		min.y = fmin(min.y, x.y);
		min.z = fmin(min.z, x.z);
		max.x = fmax(max.x, x.x);
		max.y = fmax(max.y, x.y);
		max.z = fmax(max.z, x.z);
	}
	if(max.x - min.x < EPSILON || max.y - min.y < EPSILON ||  max.z - min.z < EPSILON){
		printf("Box with no volume!\n");
		hull_volume = 0 ;
	}else{

		// cut away from box expanded from bounding box to build approximate convex hull
		hull_planes = RadialVolume::getHullPlanes(points, hull_faces, hull_detail);
		hull_volume = RadialVolume::last_hull_volume;
		hull_shape = ConvexShape::makeAxisAlignedBox(min *1.02- max*0.02, max * 1.02 - min*0.02).getPolygons(); // TODO remove dependence on old ConvexShape
		
		glm::dvec3 mid = (min + max)*0.5 ;
		for(auto& face : hull_shape){
			double dp = glm::dot(face.my_plane.first, mid) + face.my_plane.second ;
			if(fabs(glm::length(face.my_plane.first) - 1.0) > EPSILON){
				printf("unnormalized nomal on poly\n");
			}
			if( dp >= 0){
				printf("Wrong facing face in box!\n");
			}
		}

		for (auto& plane : hull_planes) {
			hull_shape = Polygon::splitOnPlane(hull_shape, plane).first;
		}

	}

}

// Split this node on the given plane
void VolumeNode::split(glm::dvec3 normal, double d){
	split_plane.first = normal ;
	split_plane.second = d;
	std::vector<Polygon> left_poly;
	std::vector<Polygon> right_poly;
	for (int k = 0; k < true_shape.size(); k++) {
		std::pair<Polygon, Polygon> ps = true_shape[k].splitOnPlane(split_plane);
		if (ps.first.p.size() > 0 && !ps.first.on_last_plane) {
			left_poly.push_back(ps.first);
		}
		if (ps.second.p.size() > 0 && !ps.second.on_last_plane) {
			right_poly.push_back(ps.second);
		}

	}
	printf("Splitting: %d on left and %d on right\n", (int)left_poly.size(), (int)right_poly.size()) ;
	inner = std::unique_ptr<VolumeNode>(new VolumeNode(left_poly));
	outer = std::unique_ptr<VolumeNode>(new VolumeNode(right_poly));
	leaf = false;
}


//Returns a plane through the deepest point within the convex hull pointing outward
std::pair<glm::dvec3, double> VolumeNode::getDeepCuttingPlane(){
	
	/*
	glm::dvec3 best_p ;
	glm::dvec3 best_normal ;
	double best_score = EPSILON ;
	std::vector<glm::dvec3> points;
	for (auto& face : true_shape) {
		for (auto& v : face.p) {
			glm::dvec3 p = v ;
			double closest_face = FLT_MAX ; 
			glm::dvec3 closest_normal;
			double score = 0 ;
			for(auto& h : hull_planes){
				double distance = fabs(glm::dot(h.first, p) + h.second);
				score += distance ;
				if(distance < closest_face){ 
					//score= distance ;
					closest_face = distance ;
					closest_normal = h.first ;
					//printf("Closest face: %lf\n", closest_face) ;
				}
				
			}
			//best point has furthest closest distance to hull
			if (score > best_score) {
				best_score =score;
				best_normal = glm::normalize(glm::vec3(randomFloat()-0.5f, randomFloat() - 0.5f, randomFloat() - 0.5f)) ;
				//best_normal = glm::normalize(closest_normal);
				best_p = p;
				//printf("Best score: %lf\n", best_score);
			}

		}
	}
	if(best_score > EPSILON){
	printf("Found plane: (%f,%f,%f)*x + %f\n", best_normal.x, best_normal.y, best_normal.z, -glm::dot(best_p, best_normal)) ;
		return {best_normal, -glm::dot(best_p,best_normal) };
	}else{
		printf("No plane found\n");
		return { glm::dvec3(0,0,0), 0.0} ;
	}
	*/


	glm::dvec3 best_normal ;
	double best_d  = 0;
	double best_score = FLT_MAX;
	int tries=  10 ;
	for(int k=0;k<tries;k++){
		glm::dvec3 normal = glm::normalize(glm::vec3(randomFloat() - 0.5f, randomFloat() - 0.5f, randomFloat() - 0.5f));
		//glm::dvec3 point = glm::vec3( min.x + randomFloat() *( max.x-min.x), min.y + randomFloat() * (max.y - min.y), min.z + randomFloat() * (max.z - min.z));
		//glm::dvec3 point = (max + min)*0.5 ;
		glm::dvec3 point = glm::vec3(min.x + (randomFloat()*0.5+0.25) * (max.x - min.x), min.y + (randomFloat() * 0.5 + 0.25) * (max.y - min.y), min.z + (randomFloat() * 0.5 + 0.25) * (max.z - min.z));
		double d = -glm::dot(point, normal) ;
		split(normal, d);
		if(!leaf){ // split succeeded
			
			double score = inner->hull_volume + outer->hull_volume ;
			printf("Try %d score: %f\n", k, (float)score);
			if(score < best_score){
				best_score = score;
				best_d = d ;
				best_normal = normal ;
				
			}
		}
	}

	printf("Best Score : %f\n", (float)best_score);
		//glm::dvec3 point = (min+ max)*0.5 ;
		return { best_normal, best_d  };
	
}

void VolumeNode::recurseToDepth(int depth){
	if(depth == 1 || hull_volume < EPSILON){
		return ;
	}
	std::pair<glm::dvec3, double> plane = getDeepCuttingPlane();
	if(glm::length(plane.first) < 0.5f){
		return ;
	}

	split(plane.first, plane.second) ;
	inner->recurseToDepth(depth - 1);
	outer->recurseToDepth(depth - 1);
}


void VolumeNode::collectHulls(std::vector<std::vector<Polygon>>& hulls){
	if(hull_volume < EPSILON){
		return ;
	}
	if(leaf){
		hulls.push_back(hull_shape) ;
	}else{
		inner->collectHulls(hulls);
		outer->collectHulls(hulls);
	}
}

std::vector<std::vector<Polygon>> VolumeNode::getHulls(){
	std::vector<std::vector<Polygon>> hulls;
	collectHulls(hulls) ;
	return hulls;
}