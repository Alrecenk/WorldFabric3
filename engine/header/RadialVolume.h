#ifndef _RADIAL_VOLUME_H_
#define _RADIAL_VOLUME_H_ 1

#include <vector>
#include <memory>
#include <utility>
#include "glm/glm.hpp"


class RadialVolume{
    public:
        // Bounding sphere
        glm::dvec3 center ;
        double radius;
        // planes stored relative to center
        std::vector<glm::dvec3> N; 
        std::vector<double> d;
        
        // Creates a raidla volume representing the given sphere
        RadialVolume(glm::dvec3 center, double radius, int detail);
        
        // Creates a minimal volume (for the given detail) that fully contains the proper convex hull of the given points
        RadialVolume(std::vector<glm::dvec3> points, int detail);

        // Returns approximate volume of the solid
        double getVolume();

        // Returns number of planes
        int size();

        // Returns one of this volumes planes generated in world space
        std::pair<glm::dvec3,double> getPlane(int k);

        // Given a plane  in world space, returns a new radial volume with the volume beyond that plane removed
        RadialVolume cut(std::pair<glm::dvec3,double> plane);

};
#endif // #ifndef _RADIAL_VOLUME_H_