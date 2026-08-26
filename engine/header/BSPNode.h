#ifndef _BSP_NODE_H_
#define _BSP_NODE_H_ 1


#include "Polygon.h"
#include "GLTF.h"
#include <vector>
#include <memory>
#include <string>
#include "glm/glm.hpp"

class BSPNode{
    public:
        BSPNode* parent = nullptr;
        glm::dvec3 N ;
        double d ;
        std::unique_ptr<BSPNode> inner, outer;
        bool leaf = true;
        bool leaf_inside = false;
        std::vector<Polygon> shape;
        static constexpr double EPSILON = 0.0001;
        double volume_inside;
        double volume_outside;

        BSPNode();
        // Builds a tree from a mesh
        // Assumes mesh is a single closed surface
        BSPNode(std::shared_ptr<GLTF> mesh);

        // Used internal to recursively build a tree from a soup of polygons fro ma closed mesh
        //shape is the current convex boundary of this node
        BSPNode(const std::vector<Polygon>& poly, std::vector<Polygon>& shape);

        // Builds a tree from a closed surface defined by polygons
        // Root shape is created as a bounding box
        BSPNode(std::vector<Polygon>& poly);

        void build(const std::vector<Polygon>& poly);

        double rayTrace(const glm::dvec3& p, const glm::dvec3& v) const;
        double rayTrace(const glm::dvec3& p, const glm::dvec3& v, double enter_t) const;

        bool inside(const glm::dvec3& p) const;   

        void computeVolumeInside();   

};
#endif // #ifndef _BSP_NODE_H_