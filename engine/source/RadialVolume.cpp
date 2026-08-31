#include "RadialVolume.h"
#include "ConvexShape.h"

#include <stdio.h>
#include <algorithm>
#include <stack>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>

using glm::dvec3 ;
using glm::vec3 ;
using std::vector ;


// Creates a raidla volume representing the given sphere
RadialVolume::RadialVolume(glm::dvec3 new_center, double new_radius, int detail){
    center = new_center;
    radius = new_radius ;
    ConvexShape sphere = ConvexShape::makeSphere(vec3(0,0,0), 1.0f, detail) ;
    N = vector<dvec3>();
    d = vector<double>();
    N.reserve(sphere.vertex.size());
    d.reserve(sphere.vertex.size());
    for(vec3& n : sphere.vertex){
        N.push_back(n);
        d.push_back(-radius);
    }
}

// Creates a minimal volume (for the given detail) that fully contains the proper convex hull of the given points
RadialVolume::RadialVolume(std::vector<glm::dvec3> points, int detail){
    center = dvec3(0,0,0);
    for(dvec3& p : points){
        center += p;
    }
    center/=points.size();
    radius = 0 ;
    ConvexShape sphere = ConvexShape::makeSphere(vec3(0,0,0), 1.0f, detail) ;
    N = vector<dvec3>();
    d = vector<double>();
    N.reserve(sphere.vertex.size());
    d.reserve(sphere.vertex.size());
    for(vec3& n : sphere.vertex){
        N.push_back(n);
    }
    // Find the d value that makes the plane contain all points given
    for(dvec3& n : N){
        double max_d = 0 ;
        for(dvec3& p : points){
            double this_d = glm::dot(p-center, n) ;
            if(this_d > max_d){
                max_d = this_d ;
            }
        }
        if(max_d > radius){
            radius = max_d ;
        }
        d.push_back(-max_d);
    }
}

// Returns approximate volume of the solid
double RadialVolume::getVolume(){
    double total_d = 0 ;
    for(int k=0;k<d.size();k++){
        total_d-=d[k];
    }
    return 4*3.141592653589793 *total_d/(3*d.size());
}

// Returns number of planes
int RadialVolume::size(){
    return (int)N.size();
}

// Returns one of this volumes planes generated in world space
std::pair<dvec3,double> RadialVolume::getPlane(int k){
    return std::pair<dvec3,double>(N[k], d[k] - glm::dot(center,N[k])); // TODO + - might be flipped
}

// Given a plane  in world space, returns a new radial volume with the volume beyond that plane removed
RadialVolume RadialVolume::cut(std::pair<dvec3,double> absolute_plane){
    dvec3 cut_N = absolute_plane.first;
    double cut_d = absolute_plane.second + glm::dot(center,cut_N); // TODO + - might be flipped
    RadialVolume result = *this;// copy current
    for(int k=0;k<N.size();k++){
        double dot = glm::dot(N[k], cut_N);
        if(dot > 0){ // cut is on side with this normal
            result.d[k] = fmax(result.d[k], cut_d * dot); // cut result d (fmax cause they are negative)
        }
    }
    return result ;
}


std::vector<std::pair<dvec3, double>> RadialVolume::getHullPlanes(std::vector<dvec3>& points, int hull_faces, int detail_level){
	RadialVolume optimal(points, detail_level);
	RadialVolume sphere(optimal.center, optimal.radius * 2.0f, detail_level);

	RadialVolume current = sphere;
	std::vector<std::pair<dvec3, double>> planes;
	while (planes.size() < hull_faces) {
		RadialVolume best = current;
		double best_volume = best.getVolume();
		int best_plane = -1; ;
		for (int k = 0; k < current.size(); k++) {
			std::pair<dvec3, double> candidate = optimal.getPlane(k);
			RadialVolume test = current.cut(candidate);
			double test_volume = test.getVolume();
			if (test_volume < best_volume) {
				best = test;
				best_volume = test_volume;
				best_plane = k;
			}
		}
		if (best_plane < 0) {
			break;
		}
		planes.push_back(optimal.getPlane(best_plane));
		current = best;
		last_hull_volume = best_volume ;
	}
	return planes ;

}