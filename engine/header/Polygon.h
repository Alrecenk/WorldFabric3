#ifndef _POLYGON_H_
#define _POLYGON_H_ 1

#include <vector>
#include <memory>
#include <utility>
#include "glm/glm.hpp"
#include "GLTF.h"

class Polygon{
    public:
        std::vector<glm::dvec3> p;
        std::pair<glm::dvec3, double> my_plane ;
        bool on_last_plane = false;
        static constexpr double EPSILON = 0.0000001;

        Polygon();
        Polygon(std::vector<glm::dvec3> np);

        // Returns N and d such that N*x+d = 0 for plane and N points out of shape
        void setPlane();

        // Returns two polygons of this polygon split on the given plane
        // Note: either polygon may be empty
        std::pair<Polygon, Polygon> splitOnPlane(const std::pair<glm::dvec3, double>& plane) const;

        std::vector<glm::dvec3> getPlaneIntersections(const std::pair<glm::dvec3, double>& plane) const;

        glm::dvec3 getCenter() const;

        // Splits a convex polyhedron on a plane 
        static std::pair<std::vector<Polygon>, std::vector<Polygon>> splitOnPlane(std::vector<Polygon>& surface, const std::pair<glm::dvec3, double>& plane);

        struct SortablePoint{
            double x;
            double y;
            glm::dvec3 p ;
        };

        // A Comparator to sort points into a clean winding order 
        static bool radialSort(const Polygon::SortablePoint& a, const Polygon::SortablePoint& b);

        static bool checkConvex(std::vector<Polygon>& surface);

        // Returns all closed surfaces as separate polygon lists so BSP trees can be built from them
        // Mesh is assumed to be made of triangles with correct winding order
        // triangles not in closed surfaces will be ignored
        static std::vector<std::vector<Polygon>> collectClosedSurfaces(std::shared_ptr<GLTF> mesh);

        static std::pair<int,int> sortpair(int a, int b);

        // Reduce the polygons in a closed triangle surface by repeatedly collapsing the shortest edge
        static std::vector<Polygon> reduce(std::vector<Polygon> surface, int triangle_budget);

        // Builds an approximate convex hull of the given point with up to the given number of faces
        // Detail level is sphere extrapolation used, it improves the quality but also increases the time taken exponentially
        static std::vector<Polygon> buildApproximateHull(std::vector<glm::dvec3>& points, int hull_faces, int detail_level);

        // splits a polyhedron in half on each axis until bo piece is larger than the given extent in any axis
        static std::vector<std::vector<Polygon>> splitToMaximumExtent(std::vector<Polygon> surface, float target_extent);

};
#endif // #ifndef _POLYGON_H_