#include "BSPNode.h"
#include "ConvexShape.h"
#include <limits>

using glm::dvec3;
using glm::vec3 ;
using std::vector;

BSPNode::BSPNode(std::shared_ptr<GLTF> mesh){

    vec3 max(-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max());
    vec3 min(std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max());
    for(int k=0;k<mesh->vertices.size();k++){
        vec3& x = mesh->vertices[k].transformed_position ;
        max.x = fmax(max.x,x.x);
        min.x = fmin(min.x,x.x);
        max.y = fmax(max.y,x.y);
        min.y = fmin(min.y,x.y);
        max.z = fmax(max.z,x.z);
        min.z = fmin(min.z,x.z);
    }

    shape = ConvexShape::makeAxisAlignedBox(min - vec3(EPSILON,EPSILON,EPSILON), max + vec3(EPSILON,EPSILON,EPSILON)).getPolygons();

    vector<Polygon> poly;
    for(int k=0;k<mesh->triangles.size();k++){
        vector<dvec3> p ;
        p.push_back(mesh->vertices[mesh->triangles[k].A].transformed_position);
        p.push_back(mesh->vertices[mesh->triangles[k].B].transformed_position);
        p.push_back(mesh->vertices[mesh->triangles[k].C].transformed_position);
        poly.emplace_back(p);
        poly[poly.size()-1].setPlane();
    }
    build(poly);
}

BSPNode::BSPNode(){}

BSPNode::BSPNode(const std::vector<Polygon>& poly, std::vector<Polygon>& s){
    shape = s ;
    build(poly);
}

// Builds a tree from a closed surface defined by polygons
// Root shape is created as a bounding box
BSPNode::BSPNode(std::vector<Polygon>& poly){
    vec3 max(-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max());
    vec3 min(std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max());
    for(Polygon& p : poly){
        for(dvec3& x : p.p){
            max.x = fmax(max.x,x.x);
            min.x = fmin(min.x,x.x);
            max.y = fmax(max.y,x.y);
            min.y = fmin(min.y,x.y);
            max.z = fmax(max.z,x.z);
            min.z = fmin(min.z,x.z);
        }
    }

    shape = ConvexShape::makeAxisAlignedBox(min - vec3(10*EPSILON,10*EPSILON,10*EPSILON), max + vec3(10*EPSILON,10*EPSILON,10*EPSILON)).getPolygons();
    build(poly);
}

void BSPNode::build(const std::vector<Polygon>& poly){
    if(poly.size() > 0){
        int split_index = rand() % poly.size();
        Polygon split_poly = poly[split_index];
        std::pair<dvec3,double> split_plane = split_poly.my_plane;
        N = split_plane.first;
        d = split_plane.second;
        vector<Polygon> left_poly;
        vector<Polygon> right_poly;
        for(int k=0;k<poly.size();k++){
            std::pair<Polygon,Polygon> ps = poly[k].splitOnPlane(split_plane);
            if(ps.first.p.size() > 0 && !ps.first.on_last_plane){
                left_poly.push_back(ps.first);
            }
            if(ps.second.p.size() > 0 && !ps.second.on_last_plane){
                right_poly.push_back(ps.second);
            }
            
        }
        std::pair<std::vector<Polygon>, std::vector<Polygon>> child_shape = Polygon::splitOnPlane(shape, split_plane);
        inner = std::unique_ptr<BSPNode>(new BSPNode(left_poly, child_shape.first));
        left_poly = vector<Polygon>(); // prevent build up of const parameter memory
        inner->parent = this;
        inner->leaf_inside = true;
        outer = std::unique_ptr<BSPNode>(new BSPNode(right_poly, child_shape.second));
        outer->parent = this;
        outer->leaf_inside = false;
        leaf = false;
    }

}

double BSPNode::rayTrace(const glm::dvec3& p, const glm::dvec3& v) const{
    return rayTrace(p, v, 0);
}

double BSPNode::rayTrace(const glm::dvec3& p, const glm::dvec3& v, double enter_t) const{
    if(leaf){
        //printf("in a leaf\n");
        if(leaf_inside){ // full leaf is a hit
            return enter_t;
        }else{// If in an empty leaf
            // Find closest plane cross
            double best_t = std::numeric_limits<float>::max() ;
            BSPNode* best = nullptr;
            BSPNode* next = parent;
            while(next){
                double nv = glm::dot(next->N,v);
		        double cross_t = fabs(nv) >= EPSILON ? (next->d + glm::dot(p,next->N))/-nv : std::numeric_limits<float>::max() ;
                if(cross_t > enter_t && cross_t < best_t){
                    best_t = cross_t;
                    best = next;
                }
                next = next->parent;
            }
            if(best == nullptr){
                return -1.0; // No hit
            }else{
                return best->rayTrace(p, v, best_t + EPSILON);
            }
        }
    }else{ // If in a branch, find the leaf
        if(glm::dot(N,p + (v * enter_t)) + d < 0){
            return inner->rayTrace(p, v, enter_t);
        }else{
            return outer->rayTrace(p, v, enter_t);
        }
    }
}

bool BSPNode::inside(const glm::dvec3& p) const{
    //printf("called inside\n");
    if(leaf){
        return leaf_inside ;
    }else{
        if(glm::dot(N,p) + d < 0){
            //printf("inside inner\n");
            return inner->inside(p);
        }else{
            //printf("inside outer\n");
            return outer->inside(p);
        }
    }
}


void BSPNode::computeVolumeInside(){
    if(leaf){
        ConvexShape s = ConvexShape(shape) ;
        double volume = s.getVolume() ;
        if(leaf_inside){
            volume_inside = volume ;
            volume_outside = 0 ;
        }else{
            volume_inside = 0 ;
            volume_outside = volume ;
        }
    }else{
        inner->computeVolumeInside();
        outer->computeVolumeInside();
        volume_inside = inner->volume_inside + outer->volume_inside;
        volume_outside = inner->volume_outside + outer->volume_outside;
    }
}