#include "Polygon.h"

#include <stdio.h>
#include <algorithm>
#include <stack>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "RadialVolume.h"
#include "ConvexShape.h"

using glm::dvec3 ;
using std::vector ;
using std::stack ;
using std::map ;
using std::unordered_map;
using std::unordered_set;

Polygon::Polygon(){}

Polygon::Polygon(std::vector<glm::dvec3> np){
    p = np;
}

// Returns N and d such that N*x+d = 0 for plane and N points out of shape
void Polygon::setPlane(){
    dvec3 AB = p[1] - p[0];
    dvec3 AC = p[2] - p[0];
    dvec3 N = glm::normalize(glm::cross(AB,AC)) ;
    double d = -glm::dot(N, p[0]);
    my_plane = std::make_pair(N, d);
}

// Returns two polygons of this polygon split on the given plane
// Note: either polygon may be empty
std::pair<Polygon, Polygon> Polygon::splitOnPlane(const std::pair<glm::dvec3, double>& plane) const{
    const dvec3& N = plane.first;
    double d = plane.second;
    //printf("N: %f, %f, %f  d: %f\n", N.x, N.y, N.z, d);
	vector<dvec3> above ; // points above
	vector<dvec3> below ; // points below
    bool has_inside = false;
    bool has_outside = false;
	for(int k = 0 ;k < p.size(); k++){
        //printf("p: %f, %f, %f\n", p[k].x, p[k].y, p[k].z);
		const dvec3 &a = p[k] ;
		const dvec3 &b = p[(k+1)%p.size()];
		double ad = glm::dot(N, a) + d ;
		
		// Add A
		if(fabs(ad) < EPSILON){ // if on plane
			below.push_back(a);
			above.push_back(a);
		}else if(ad < 0){
			below.push_back(a);
            has_inside = true;
		}else{
			above.push_back(a);
            has_outside = true;
		}
		
		double  bd = glm::dot(N, b) + d ;
		if(ad*bd < 0){ // if next line crosses plane
			ad = fabs(ad);
			bd = fabs(bd);
            if(ad > EPSILON && bd > EPSILON){ // skip adding new points if one is already on the plane
                double td = bd + ad;	
                dvec3 np = (a*bd + b*ad)/td ;		
                above.push_back(np); // Add to both polygons
                below.push_back(np);
            }
		}
	}
    if(has_inside && has_outside){ // actually getting a spli
        auto pair = std::make_pair(Polygon(below), Polygon(above));
        pair.first.my_plane = my_plane ; // copy plane to prevent it from changing due ot rounding
        pair.second.my_plane = my_plane ;
        pair.first.on_last_plane = false;
        pair.second.on_last_plane = false;
        return pair ;
    }else if(!has_inside && !has_outside){ // Lies on plane, return on both sides
        //printf("  on plane%f to %f \n", mind, maxd);
        auto pair = std::pair<Polygon, Polygon>( 
            *this,
            *this
        ) ;
        pair.first.on_last_plane = true;
        pair.second.on_last_plane = true;
        return pair;
    }else if(has_inside){
        auto pair = std::pair<Polygon, Polygon>( 
            *this,
            Polygon()
        ) ;
        pair.first.on_last_plane = false;
        pair.second.on_last_plane = true;
        return pair;
    }else{
        auto pair = std::pair<Polygon, Polygon>( 
            Polygon(),
            *this
        ) ;
        pair.first.on_last_plane = true;
        pair.second.on_last_plane = false;
        return pair;
    }
}

std::vector<glm::dvec3> Polygon::getPlaneIntersections(const std::pair<glm::dvec3, double>& plane) const{
    std::vector<glm::dvec3> plane_points; 
    const dvec3 &N = plane.first;
    const double &d = plane.second;
    int prev_index = (int)( p.size()-1 );
    double prev_signed_distance = glm::dot(p[prev_index],N)+d ;
    double signed_distance;
    for(int k=0; k<p.size(); k++){
      signed_distance = glm::dot(p[k],N)+d ;
      if(fabs(signed_distance)<EPSILON){//if point on plane
        plane_points.push_back(p[k]);
      }else if((signed_distance < -EPSILON && prev_signed_distance > EPSILON) || //if crosses from front to back
          (signed_distance > EPSILON && prev_signed_distance < -EPSILON)  ){ //if crosses from back to front
        const dvec3 &a = p[prev_index];
        const dvec3 &b = p[k];
        //use similar triangles to find parameter of intersection point
        double am = fabs(signed_distance)/(fabs(prev_signed_distance) + fabs(signed_distance)) ;
        double bm = 1 - am ;
        dvec3 np = a*am + b*bm ;//calculate the intersection point with the plane
		plane_points.push_back(np);
      }
      prev_signed_distance = signed_distance ;
      prev_index = k ;
    }
    return plane_points;
}

glm::dvec3 Polygon::getCenter() const{
    dvec3 total(0,0,0);
    for( auto a : p){
        total +=a;
    }
    return total/(double)p.size();
}

std::pair<std::vector<Polygon>, std::vector<Polygon>> Polygon::splitOnPlane(std::vector<Polygon>& surface, const std::pair<glm::dvec3, double>& plane){
    vector<Polygon> left_surface;
    vector<Polygon> right_surface;
    vector<dvec3> plane_points;
    dvec3 plane_center(0,0,0);
    int center_amount = 0 ;
    for(int k=0;k<surface.size();k++){
        // Split the existing surfaces on the split plane.
        std::pair<Polygon, Polygon> split_surface = surface[k].splitOnPlane(plane);
        //printf("split: %d -> %d + %d\n", (int)surface[k].p.size(),(int)split_surface.first.p.size(),(int)split_surface.second.p.size());

        if(split_surface.first.p.size() > 2){
            left_surface.push_back(split_surface.first);
        }
        if(split_surface.second.p.size() >2){
            right_surface.push_back(split_surface.second);
        }
        // face actually crosses plane
        if(split_surface.first.p.size() > 2 && split_surface.second.p.size() > 2){
            // Collect the points of the boundary that intersect the split plane
            vector<dvec3> plane_hits = surface[k].getPlaneIntersections(plane);
            for(dvec3& p : plane_hits){
                plane_points.push_back(p);
                plane_center += p;
                center_amount++;
            }
        }
    }

    if(right_surface.size() <= 1){
        return std::pair<std::vector<Polygon>, std::vector<Polygon>>(surface, std::vector<Polygon>());
    }
    if(left_surface.size() <= 1){
        return std::pair<std::vector<Polygon>, std::vector<Polygon>>(std::vector<Polygon>(), surface);
    }
    // If it has both surfaces then make polygon for surface intersection

    // Create a center and axis around which to sort plane points
    plane_center /= center_amount;
    dvec3 x_axis(0.2,0.3,0.4);
    dvec3 y_axis = glm::normalize(glm::cross(x_axis, plane.first));
    x_axis = glm::normalize(glm::cross(y_axis, plane.first)) ;

    vector<SortablePoint> sp ;
    for(dvec3& p : plane_points){
        dvec3 d = p-plane_center;
        double x = glm::dot(x_axis, d);
        double y = glm::dot(y_axis, d);
        sp.push_back(Polygon::SortablePoint{x, y, p});
    }

    std::sort(sp.begin(), sp.end(), Polygon::radialSort);
    plane_points = vector<dvec3>();
    plane_points.push_back(sp[0].p);
    for(int k=1;k<sp.size();k++){
        dvec3 d = sp[k].p-sp[k-1].p;
        if(glm::dot(d,d) > Polygon::EPSILON*Polygon::EPSILON){ // skip duplicates
            plane_points.push_back(sp[k].p);
        }
    }

    Polygon plane_poly (plane_points);
    plane_poly.my_plane = plane ;
    left_surface.push_back(plane_poly);
    right_surface.push_back(plane_poly);

    return std::pair<std::vector<Polygon>, std::vector<Polygon>>(left_surface, right_surface);
}


// A Comparator to sort points into a clean winding order 
bool Polygon::radialSort(const Polygon::SortablePoint& a, const Polygon::SortablePoint& b){
    if (a.x >= 0 && b.x< 0){
        return true;
    }
    if (a.x < 0 && b.x >= 0){
        return false;
    }
    if (a.x == 0 && b.x  == 0) {
        if (a.y >= 0 || b.y  >= 0){
            return a.y > b.y ? true: false;
        }
        return b.y > a.y ? true :false;
    }

    // compute the cross product of the relative vectors 
    double det = a.x  * b.y  - b.x * a.y;
    if (det < 0)
        return true;
    return false;
}

bool Polygon::checkConvex(std::vector<Polygon>& surface){
    vector<dvec3> all_points ;
    for(auto& s : surface){
        for(auto& p : s.p){
            all_points.push_back(p);
        }
    }

    //printf("points: %d \n", (int)all_points.size());
    
    for(auto& s : surface){
        dvec3 N = s.my_plane.first;
        double d = s.my_plane.second ;
        s.setPlane();
        if(glm::dot(N,s.my_plane.first) < 0){
            s.my_plane.first*=-1;
            s.my_plane.second*=-1;
        }

        /*
        if(glm::length(s.my_plane.first-N) > EPSILON || fabs(s.my_plane.second - d) > EPSILON){
            printf ("Surface plane incorrect. Saved: (%f,%f,%f)x + %f  Calculated: (%f,%f,%f)x + %f\n", N.x, N.y, N.z, d, s.my_plane.first.x, s.my_plane.first.y,s.my_plane.first.z, s.my_plane.second);
            return false;
        }
        */
        bool front = false;
        bool back = false ;
        for(auto& p : all_points){
            double sd = glm::dot(N,p) + d ;
            //printf("sd: %f\n", sd);
            front = front || (sd > EPSILON) ;
            back = back || sd < -EPSILON ;
        }

        if(front && back){
            printf("Convex surface is not convex!\n");
            return false;
        }

        if(!front && !back){
            printf("Convex surface is flat!\n");
            return false;
        }
    }
    return true ;

}


// Returns all closed surfaces as separate polygon lists so BSP trees can be built from them
// Mesh is assumed to be made of triangles with correct winding order
// triangles not in closed surfaces will be ignored
std::vector<std::vector<Polygon>> Polygon::collectClosedSurfaces(std::shared_ptr<GLTF> mesh){

    // First we need a quick way to collapse matching vertices into single indices
    std::unordered_map<int,int> v ; // maps each vertex index to its lowest indixed matching point
    std::unordered_map<int,int> hash_vertex; // map hash to lowest index
    for(int k=0;k<mesh->vertices.size();k++){ 
        int hash = Variant(mesh->vertices[k].transformed_position).hash();
        if(hash_vertex.find(hash) == hash_vertex.end()){
            hash_vertex[hash] = (int)hash_vertex.size()  ;
        }
        v[k] = hash_vertex[hash] ;
    }

    printf("Starting Vertices: %d, Unique vertices: %d\n", (int)mesh->vertices.size(), (int)hash_vertex.size());
            
    // create map from edges to triangles containing them
    std::map<std::pair<int,int>,vector<int>> edge_to_triangles ;
    for(int k=0;k<mesh->triangles.size();k++){ 
        GLTF::Triangle& t = mesh->triangles[k]; 
        edge_to_triangles[sortpair(v[t.A],v[t.B])].push_back(k) ;
        edge_to_triangles[sortpair(v[t.B],v[t.C])].push_back(k) ;
        edge_to_triangles[sortpair(v[t.C],v[t.A])].push_back(k) ;

        //printf("T %d: %d, %d, %d\n", k, v[t.A], v[t.B], v[t.C]);
    }
    // Use edge map to create map from triangles to their neighbors
    // Also collect a list of all triangles with an open edge (any connected triangles are on an open surface)
    std::unordered_map<int,vector<int>> triangle_graph ;
    std::unordered_set<int> broken ;
    for(int k=0;k<mesh->triangles.size();k++){ 
        GLTF::Triangle& t = mesh->triangles[k];
        triangle_graph[k] = vector<int>();;
        vector<int> nt = edge_to_triangles[sortpair(v[t.A],v[t.B])];
        if(nt.size() != 2){
            broken.insert(k) ;
        }else{
            triangle_graph[k].push_back(nt[0] + nt[1] - k);
        }

        nt = edge_to_triangles[sortpair(v[t.B], v[t.C])];
        if(nt.size() != 2){
            broken.insert(k) ;
        }else{
            triangle_graph[k].push_back(nt[0] + nt[1] - k);
        }

        nt = edge_to_triangles[sortpair(v[t.C], v[t.A])];
        if(nt.size() != 2){
            broken.insert(k) ;
        }else{
            triangle_graph[k].push_back(nt[0] + nt[1] - k);
        }
    }
    printf("edge triangles found: %d\n", (int)broken.size());
    //Remove all triangles on an open surface from the map
    while(!broken.empty()){
        stack<int> to_remove;
        to_remove.push(*broken.begin()) ; // start from arbitrary broken triangle
        unordered_map<int,bool> visited ;
        while(!to_remove.empty()){ // flood fill to fnd all triangle on its open surface
            int t = to_remove.top();
            to_remove.pop();
            vector<int> neighbors = triangle_graph[t];
            for(int k=0;k<neighbors.size();k++){
                if(!visited[neighbors[k]]){
                    visited[neighbors[k]] = true;
                    to_remove.push(neighbors[k]);
                }
            }
            triangle_graph.erase(t);
            broken.erase(t);
        }
    }

    printf("closed surface triangles found: %d\n", (int)triangle_graph.size());
    vector<vector<Polygon>> surfaces ;
    while(!triangle_graph.empty()){
        stack<int> to_add;
        to_add.push((*triangle_graph.begin()).first); // start from arbitrary first triangle
        vector<Polygon> surface ;
        unordered_map<int,bool> visited ;
        while(!to_add.empty()){ // flood fill to collect surface
            int t = to_add.top();
            to_add.pop();
            vector<int> neighbors = triangle_graph[t];
            for(int k=0;k<neighbors.size();k++){
                if(!visited[neighbors[k]]){
                    visited[neighbors[k]] = true;
                    to_add.push(neighbors[k]);
                }
            }
            triangle_graph.erase(t);

            vector<dvec3> p ;
            p.push_back(mesh->vertices[mesh->triangles[t].A].transformed_position);
            p.push_back(mesh->vertices[mesh->triangles[t].B].transformed_position);
            p.push_back(mesh->vertices[mesh->triangles[t].C].transformed_position);
            surface.emplace_back(p);
            surface[surface.size()-1].setPlane();
        }
        surfaces.push_back(surface);
    }
    printf("Collected %d surfaces.\n", (int)surfaces.size());
    return surfaces ;

}

std::pair<int,int> Polygon::sortpair(int a, int b){
    if(a <= b){
        return std::pair<int,int>(a, b);
    }else{
        return std::pair<int,int>(b, a);
    }
}

// Reduce the polygons in a closed triangle surface by repeatedly collapsing the shortest edge
std::vector<Polygon> Polygon::reduce(std::vector<Polygon> surface, int triangle_budget){
    // First we need to collapse matching vertices into single indices
    std::unordered_map<int,int> hash_vertex; // map hash to index
    std::vector<dvec3> vertices ; // map index back to vec3
    vector<vector<int>> faces ;
    vector<bool> removed ;
    for(Polygon& poly: surface){ 
        vector<int> face ;
        for(dvec3& x: poly.p){

            int hash = Variant(glm::vec3(x)).hash();
            if(hash_vertex.find(hash) == hash_vertex.end()){
                hash_vertex[hash] = (int)vertices.size()  ;
                vertices.push_back(x);
            }
            face.push_back(hash_vertex[hash]);
        }
        faces.push_back(face);
        removed.push_back(false);
    }

    hash_vertex.clear(); // don't need this anymore once we've indexed everything
    int total_removed = 0 ;
    while(faces.size()-total_removed > triangle_budget){
        // Find shortest edge on surface
        int a=-1, b=-1;
        double best_length = 1E30f ;
        for(int j=0;j<faces.size();j++){
            vector<int>& face = faces[j] ;
            if(!removed[j]){
                for(int k=0; k < face.size();k++){
                    int ta = face[k];
                    int tb = face[(k+1)%face.size()];
                    double length = glm::dot(vertices[ta]-vertices[tb], vertices[ta]-vertices[tb]);
                    if(length < best_length){
                        best_length = length;
                        a = ta ;
                        b = tb ;
                    }
                }
            }
        }
        // Make new point on middle of edge
        dvec3 cx = (vertices[a]+vertices[b])*0.5;
        int c = (int)vertices.size();
        vertices.push_back(cx);

        for(int j=0;j<faces.size();j++){
            vector<int>& face = faces[j] ;
            if(!removed[j]){
                bool found_a = false;
                bool found_b = false;
                //replace instances of A and B both with the new midpoint
                for(int k=0; k < face.size();k++){
                    if(face[k] == a){
                        face[k] = c ;
                        found_a = true;
                    }
                    if(face[k] == b){
                        face[k] = c ;
                        found_b = true;
                    }
                }
                // if found both, triangle had the edge and is now a line
                if(found_a && found_b){
                    removed[j] = true;
                    total_removed++;
                }
            }
        }
    }// end while too many truiangles


    //Convert indexed faces back to flat polygons
    std::vector<Polygon> new_surface ;
    for(int j=0;j<faces.size();j++){
        if(!removed[j]){
            vector<int>& face = faces[j] ;
            vector<dvec3> points;
            for(int k=0; k < face.size();k++){
                points.push_back(vertices[face[k]]);
            }
            new_surface.emplace_back(points);
            new_surface[new_surface.size()-1].setPlane();
        }
    }
    return new_surface ;
}



// Builds an approximate convex hull of the given point with up to the given number of faces
// Detail level is sphere extrapolation used, it improves the quality but also increases the time taken exponentially
std::vector<Polygon> Polygon::buildApproximateHull(std::vector<dvec3>& points, int hull_faces, int detail_level){
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
	
	// cut away from box expanded from bounding box
	std::vector<std::pair<glm::dvec3, double>> planes = RadialVolume::getHullPlanes(points, hull_faces, detail_level) ;
    vector<Polygon> surface = ConvexShape::makeAxisAlignedBox(min, max).getPolygons();
    for(auto& plane : planes){
        surface = Polygon::splitOnPlane(surface, plane).first;
    }
    return surface ;
}

std::vector<Polygon> Polygon::buildApproximateHull(std::vector<Polygon> surface, int hull_faces, int detail_level){
	std::vector<glm::dvec3> points ;
	for(auto& face : surface){
		for(auto& v : face.p){
			points.emplace_back(v);
		}
	}
	return buildApproximateHull(points, hull_faces, detail_level) ;
}


// splits a polyhedron in half on each axis until bo piece is larger than the given extent in any axis
std::vector<std::vector<Polygon>> Polygon::splitToMaximumExtent(std::vector<Polygon> surface, float target_extent){
    // Get the bounding box
    dvec3 min(std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max());
    dvec3 max(-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max());
    for(auto& poly : surface){
        for(auto& x : poly.p){
            min.x = fmin(min.x, x.x);
            min.y = fmin(min.y, x.y);
            min.z = fmin(min.z, x.z);
            max.x = fmax(max.x, x.x);
            max.y = fmax(max.y, x.y);
            max.z = fmax(max.z, x.z);
        }
    }

    // split on any extent bigger than the allowed limit
    std::vector<std::vector<Polygon>> result ;
    std::pair<std::vector<Polygon>, std::vector<Polygon>> split ;
    if(max.x - min.x > target_extent){
        split = Polygon::splitOnPlane(surface, std::pair<glm::dvec3, double>(dvec3(1,0,0), (min.x+max.x)*-0.5));
    }else if(max.y - min.y > target_extent){
        split = Polygon::splitOnPlane(surface, std::pair<glm::dvec3, double>(dvec3(0,1,0), (min.y+max.y)*-0.5));
    }else if(max.z - min.z > target_extent){
        split = Polygon::splitOnPlane(surface, std::pair<glm::dvec3, double>(dvec3(0,0,1), (min.z+max.z)*-0.5));
    }else{ // no splits, we're done
        result.push_back(surface);
        return result ;
    }

    // recurse and merge results
    result = Polygon::splitToMaximumExtent(split.first, target_extent);
    std::vector<std::vector<Polygon>> r2 = Polygon::splitToMaximumExtent(split.second, target_extent);
    for(auto& s : r2){
        result.push_back(s);
    }
    return result ;
}