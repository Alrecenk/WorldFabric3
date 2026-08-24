
#include "ConvexShape.h"

using std::vector;
using glm::dvec3 ;
using glm::vec3;
using std::map;
using std::string;
using std::pair;


ConvexShape::ConvexShape(const std::vector<glm::dvec3> &vertices, const std::vector<std::vector<int>> &faces) {
    //vertex = vertices;
    vertex = std::vector<glm::vec3>();
    for(const vec3& v : vertices){
        vertex.emplace_back(v);
    }
    face = faces;
    
}

ConvexShape::ConvexShape(const std::vector<glm::vec3> &vertices, const std::vector<std::vector<int>> &faces) {
    vertex = vertices;
    face = faces;
}

ConvexShape::ConvexShape(){
   
}

ConvexShape::ConvexShape(std::vector<Polygon>& polygons){
    //deduplicate the points to match reduced shape format
    vertex = std::vector<glm::vec3>();
    face = std::vector<std::vector<int>>();
    for(int k=0;k<polygons.size();k++){
        Polygon& poly = polygons[k];
        if(poly.p.size() >=3){
            vector<int> f ;
            for(int j=0;j<poly.p.size();j++){
                int index = -1;
                for(int i=0;i<vertex.size();i++){ // TODO could make this much faster
                    if(glm::length(poly.p[j]-dvec3(vertex[i])) < Polygon::EPSILON){
                        index = i;
                        break;
                    }
                }
                if(index == -1){
                    vec3 v = poly.p[j] ;
                    vertex.push_back(v);
                    index = (int)(vertex.size()-1);
                }
                f.push_back(index);
            }
            face.push_back(f);
        }
    }

    //flip any faces pointing inward (Polygon doesn't have a winding oder guarantee)
    for(int k=0;k<face.size();k++){
        //printf("fs:%d\n",(int)face.size() );
        vec3& A = vertex[face[k][0]] ;
        vec3& B = vertex[face[k][1]] ;
        vec3& C = vertex[face[k][2]] ;
        //printf("ABC:%d,%d,%d, size:%d\n",face[k][0],face[k][1],face[k][2], (int)vertex.size() );
        vec3 normal = glm::normalize(glm::cross(B - A, C - A));
        if(glm::dot(normal,A) < 0){
            vector<int> new_face ;
            for(int j=(int)(face[k].size()-1); j >=0; j--){
                new_face.push_back(face[k][j]);
            }
            face[k] = new_face ;
        }
    }

}

ConvexShape::~ConvexShape(){
    
}

// Return the center of mass of this shape
glm::vec3 ConvexShape::getCentroid(){
    // Get a point on the inside
    vec3 inner_point(0,0,0) ;
    for(const auto& v : vertex){
        inner_point+=v;
    }
    inner_point/=vertex.size();
    vec3 centroid(0,0,0);
    float  volume = 0 ;
    for(const auto& f : face){
        for(int k=1; k < f.size()-1;k++){
            vec3& a = vertex[f[0]];
            vec3& b = vertex[f[k]];
            vec3& c = vertex[f[k+1]];
            vec3& d = inner_point ;
            float vol = computeTetraVolume(a, b, c, d);
            vec3 ctr =  computeTetraCentroid(a, b, c, d);
            volume += vol;
            centroid += ctr*vol;
        }
    }
    return centroid/volume;

}

// Moves this shape so the origin aligns with the centroid and returns the move that was made
glm::vec3 ConvexShape::centerOnCentroid() {
    vec3 centroid = getCentroid();
    for (vec3& v : vertex) {
        v -= centroid;
    }
    return -centroid;
}

float ConvexShape::getVolume(){
    // Get a point on the inside
    vec3 inner_point(0,0,0) ;
    for(const auto& v : vertex){
        inner_point+=v;
    }
    inner_point/=vertex.size();
    float volume = 0 ;
    for(const auto& f : face){
        for(int k=1; k < f.size()-1;k++){
            vec3& a = vertex[f[0]];
            vec3& b = vertex[f[k]];
            vec3& c = vertex[f[k+1]];
            vec3& d = inner_point ;
            volume += ConvexShape::computeTetraVolume(a, b, c, d);
        }
    }
    return volume;
}

// Returns the inertia tensor of the entire shape about the origin
glm::mat3 ConvexShape::getInertia(const float mass){
    float total_volume = getVolume();
    // Get a point on the inside
    vec3 inner_point(0,0,0) ;
    for(const auto& v : vertex){
        inner_point+=v;
    }
    inner_point/=vertex.size();
    glm::mat3 inertia = glm::mat3(0);
    float volume = 0 ;
    for(const auto& f : face){
        for(int k=1; k < f.size()-1;k++){
            vec3& a = vertex[f[0]];
            vec3& b = vertex[f[k]];
            vec3& c = vertex[f[k+1]];
            vec3& d = inner_point ;
            float vol = computeTetraVolume(a, b, c, d);
            inertia += ConvexShape::computeTetraInertia(mass*vol/total_volume, a, b, c, d);
        }
    }
    return inertia;
}
// return the volume of the given tetrahedron
float ConvexShape::computeTetraVolume(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d){
    return fabs(glm::dot(a-d, glm::cross(b-d,c-d)))/6.0f ;
}

// Returns the center of mass of the given tetrahedron
glm::vec3 ConvexShape::computeTetraCentroid(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d){
    return (a+b+c+d)/4.0f;
}

// Returns the inertia tensor of the given tetrahedron about the origin
glm::mat3 ConvexShape::computeTetraInertia(const float mass, const vec3& A, const vec3& B, const vec3& C, const vec3& D){
    // pulled from "explicit Exact Formulas for the 3D tetrahedron inertia tensor in terms of vertex coordinates"
    // by F. Tonon, Journal of Mathematics and Statistics
    //float volume = ConvexShape::computeTetraVolume(a, b, c, d);
    //float detj = 6*volume ;
    //float density = mass/volume ;

    const double x1 = A.x, y1 = A.y, z1 = A.z ;
    const double x2 = B.x, y2 = B.y, z2 = B.z ;
    const double x3 = C.x, y3 = C.y, z3 = C.z ;
    const double x4 = D.x, y4 = D.y, z4 = D.z ;
    double mu = 6.0*mass ;

    double x_group = x1*x1 + x1*x2 + x2*x2 + x1*x3 + x2*x3 + x3*x3 + x1*x4 + x2*x4 + x3*x4 + x4*x4 ;
    double y_group = y1*y1 + y1*y2 + y2*y2 + y1*y3 + y2*y3 + y3*y3 + y1*y4 + y2 *y4 + y3*y4 + y4*y4 ;
    double z_group = z1*z1 + z1*z2 + z2*z2 + z1*z3 + z2*z3 + z3*z3 + z1*z4 + z2*z4 + z3*z4 + z4*z4 ;

    double a = mu*(y_group+z_group)/60.0;
    double b = mu*(x_group+z_group)/60.0;
    double c = mu*(x_group+y_group)/60.0;

    double ap = mu*(2*y1*z1 + y2*z1 + y3*z1 + y4*z1 + y1*z2 + 2*y2*z2 + y3*z2 + y4*z2 + y1*z3 + y2*z3 + 2*y3*z3 + y4*z3 + y1*z4 + y2*z4 + y3*z4 + 2*y4*z4)/120.0 ;
    double bp = mu*(2*x1*z1 + x2*z1 + x3*z1 + x4*z1 + x1*z2 + 2*x2*z2 + x3*z2 + x4*z2 + x1*z3 + x2*z3 + 2*x3*z3 + x4*z3 + x1*z4 + x2*z4 + x3*z4 + 2*x4*z4)/120.0 ;
    double cp = mu*(2*x1*y1 + x2*y1 + x3*y1 + x4*y1 + x1*y2 + 2*x2*y2 + x3*y2 + x4*y2 + x1*y3 + x2*y3 + 2*x3*y3 + x4*y3 + x1*y4 + x2*y4 + x3*y4 + 2*x4*y4)/120.0 ;

    glm::mat3 J;
    J[0][0] = (float)a;
    J[0][1] = (float)-bp;
    J[0][2] = (float)-cp;
    J[1][0] = (float)-bp;
    J[1][1] = (float)b;
    J[1][2] = (float)-ap;
    J[2][0] = (float)-cp;
    J[2][1] = (float)-ap;
    J[2][2] = (float)c;

    return J;
}

// Returns a shape for an axis aligned bounding box
ConvexShape ConvexShape::makeAxisAlignedBox(glm::vec3 min, glm::vec3 max){
    vector<vec3> vertices;
    vertices.emplace_back(min.x, min.y, min.z); // 0
    vertices.emplace_back(max.x, min.y, min.z); // 1
    vertices.emplace_back(min.x, max.y, min.z); // 2
    vertices.emplace_back(max.x, max.y, min.z); // 3
    vertices.emplace_back(min.x, min.y, max.z); // 4
    vertices.emplace_back(max.x, min.y, max.z); // 5
    vertices.emplace_back(min.x, max.y, max.z); // 6
    vertices.emplace_back(max.x, max.y, max.z); // 7

    vector<vector<int>> faces;
    faces.push_back(vector<int>({0, 4, 6, 2})); // min x
    faces.push_back(vector<int>({1, 3, 7, 5})); // max x
    faces.push_back(vector<int>({0, 1, 5, 4})); // min y
    faces.push_back(vector<int>({2, 6, 7, 3})); // max y
    faces.push_back(vector<int>({0, 2, 3, 1})); // min z
    faces.push_back(vector<int>({4, 5, 7, 6})); // max z
    return ConvexShape(vertices, faces);
}

//Alternate form that always centers on the origin
ConvexShape ConvexShape::makeAxisAlignedBox(glm::vec3 size){
    return makeAxisAlignedBox(size * -0.5f, size * 0.5f);
}

// Makes a shape for a generic box
ConvexShape ConvexShape::makeBox(glm::vec3 corner, glm::vec3 X, glm::vec3 Y, glm::vec3 Z) {
    vector<vec3> vertices;
    vertices.push_back(corner);// 0
    vertices.push_back(corner + X);// 1
    vertices.push_back(corner + Y);// 2
    vertices.push_back(corner + + X + Y);// 3
    vertices.push_back(corner + Z);// 4
    vertices.push_back(corner + X + Z);// 5
    vertices.push_back(corner + Y + Z);// 6
    vertices.push_back(corner + X + Y + Z);// 2

    vector<vector<int>> faces;
    faces.push_back(vector<int>({ 0, 4, 6, 2 })); // min x
    faces.push_back(vector<int>({ 1, 3, 7, 5 })); // max x
    faces.push_back(vector<int>({ 0, 1, 5, 4 })); // min y
    faces.push_back(vector<int>({ 2, 6, 7, 3 })); // max y
    faces.push_back(vector<int>({ 0, 2, 3, 1 })); // min z
    faces.push_back(vector<int>({ 4, 5, 7, 6 })); // max z
    return ConvexShape(vertices, faces);

}

// Returns a shape for a sphere with 8*(4^detail) triangles
ConvexShape ConvexShape::makeSphere(glm::vec3 center, float radius, int detail){
    //Start with an octahedron
    vector<vec3> vertices;
    vertices.emplace_back(center.x, center.y - radius, center.z); // 0
    vertices.emplace_back(center.x - radius, center.y, center.z); // 1
    vertices.emplace_back(center.x, center.y, center.z - radius); // 2
    vertices.emplace_back(center.x, center.y + radius, center.z); // 3
    vertices.emplace_back(center.x + radius, center.y, center.z); // 4
    vertices.emplace_back(center.x, center.y, center.z + radius); // 5

    vector<vector<int>> faces;
    faces.push_back(vector<int>({0, 1, 2}));
    faces.push_back(vector<int>({0, 2, 4}));
    faces.push_back(vector<int>({1, 0, 5}));
    faces.push_back(vector<int>({5, 0, 4}));
    faces.push_back(vector<int>({1, 5, 3}));
    faces.push_back(vector<int>({5, 4, 3}));
    faces.push_back(vector<int>({2, 1, 3}));
    faces.push_back(vector<int>({4, 2, 3}));

    vector<vector<int>> new_faces;

    for (int det = 0; det < detail; det++) { // For each level of detail
        // split every triangle into 4 triangles
        for (auto &face : faces) {
            int a = face[0], b = face[1], c = face[2];
            int d = (int)vertices.size();
            vertices.push_back(makeSpherePoint(vertices[a], vertices[b], center, radius));
            int e = (int)vertices.size();
            vertices.push_back(makeSpherePoint(vertices[b], vertices[c], center, radius));
            int f = (int)vertices.size();
            vertices.push_back(makeSpherePoint(vertices[c], vertices[a], center, radius));
            new_faces.push_back(vector<int>({d, e, f}));
            new_faces.push_back(vector<int>({a, d, f}));
            new_faces.push_back(vector<int>({f, e, c}));
            new_faces.push_back(vector<int>({d, b, e}));
        }
        faces = new_faces;
        new_faces.clear();
    }

    return ConvexShape(vertices, faces);
}

// Helper method for sphere extrapolation
// Averages two points then pushes result to lie on the sphere.
glm::vec3 ConvexShape::makeSpherePoint(glm::vec3 a, glm::vec3 b, glm::vec3 center, float radius){
    vec3 avg = (a + b) * 0.5f;
    vec3 rel = avg - center;
    float s = radius / glm::length(rel);
    return center + (rel * s);
}

// Returns a shapefor a cylinder with center of ends and A and B
ConvexShape ConvexShape::makeCylinder(glm::vec3 A, glm::vec3 B, float radius, int sides){
    vec3 Z = B - A; // get axis_ along cylinder ((0,0,0) = A, (0,0,1) = B)
    vec3 X = glm::normalize(glm::cross(vec3(1, .8, .7), Z)) *
             radius; // Get an arbitrary axis orthogonal to Z
    vec3 Y = glm::normalize(glm::cross(X, Z)) * radius; // Get final axis
    vector<vec3> vertices;
    vector<vector<int>> faces;
    vector<int> top, bottom;
    const float twopi = 6.28318530718f;
    for (int side = 0; side < sides; side++) {
        float angle = side * twopi / sides;
        float dx = sin(angle);
        float dy = cos(angle);
        vertices.push_back(A + X * dx + Y * dy);
        vertices.push_back(B + X * dx + Y * dy);
        faces.emplace_back(
                vector<int>({2 * side + 1, 2 * side, (2 * side + 2) % (sides * 2), (2 * side + 3) % (sides * 2)}));
        top.push_back(side * 2 + 1);
        bottom.push_back((sides - 1 - side) * 2); // flip order for bottom face
    }
    //Top and bottom face
    faces.emplace_back(top);
    faces.emplace_back(bottom);
    return ConvexShape(vertices, faces);
}


// Returns a shape for a Tetrahedron with the given points
ConvexShape ConvexShape::makeTetra(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D){
    vector<vec3> vertices;
    vertices.push_back(A);
    vertices.push_back(B);
    vertices.push_back(C);
    vertices.push_back(D);

    vector<vector<int>> faces;
    faces.push_back(vector<int>({0, 1, 2}));
    faces.push_back(vector<int>({0, 1, 3}));
    faces.push_back(vector<int>({0, 3, 2}));
    faces.push_back(vector<int>({3, 1, 2}));

    // Fix winding order so normals face out
    vec3 center = (A+B+C+D)*0.25f;
    for(int k=0;k<faces.size();k++){
        vec3& a = vertices[faces[k][0]];
        vec3& b = vertices[faces[k][1]];
        vec3& c = vertices[faces[k][2]];

        vec3 n = glm::cross(b-a,c-a);
        if(glm::dot(a-center,n) < 0){
            int t = faces[k][1];
            faces[k][1] = faces[k][2];
            faces[k][2] = t ;
        }
    }
    return ConvexShape(vertices, faces);
}

std::vector<Polygon> ConvexShape::getPolygons(){
    vector<Polygon> surface ;
    for(int k=0;k<face.size();k++){
        vector<dvec3> points;
        for(int j=0;j<face[k].size();j++){
            points.push_back(vertex[face[k][j]]);
        }
        surface.emplace_back(points);
        surface[surface.size()-1].setPlane();
    }
    return surface ;
}