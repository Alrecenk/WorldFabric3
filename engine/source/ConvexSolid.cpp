
#include "ConvexSolid.h"

using glm::vec3 ;
using glm::vec4 ;
using glm::mat3 ;
using glm::mat4 ;
using glm::quat ;
using std::vector ;
using std::map;
using std::string;

ConvexSolid::ConvexSolid(){
}

ConvexSolid::ConvexSolid(std::shared_ptr<ConvexShape> shape, float m, glm::vec3 p, glm::quat r){
    this->shape = shape ;
    mass = m ;
    position = p ;
    velocity = vec3(0,0,0);
    orientation = r ;
    angular_velocity = vec3(0,0,0);
}

ConvexSolid::ConvexSolid(glm::vec3 nposition, float nmass, std::shared_ptr<ConvexShape> shape, glm::vec3 nvelocity, glm::quat norientation, glm::vec3 nangular_velocity){
    position = nposition;
    this->shape = shape;
    velocity = nvelocity;
    orientation = norientation;
    angular_velocity = nangular_velocity;
    mass = nmass;
}

ConvexSolid::~ConvexSolid(){
    
}

// Returns the matrix mapping the shape's local points into world space
glm::mat4 ConvexSolid::getTransform(){
    mat4 transform = mat4(1) ;
    transform = glm::translate(transform, position);
    transform *= glm::mat4_cast(orientation);
    return transform ;
}

//set the current position
void ConvexSolid::setPosition(glm::vec3 new_position) {
    position = new_position;
    needs_world_planes_update = true;
}

//set the orientation
void ConvexSolid::setOrientation(glm::quat new_orientation) {
    orientation = new_orientation;
    needs_world_planes_update = true;
}

// Sets the rotation and orientation to those extracted form the given matrix
void ConvexSolid::setTransform(glm::mat4 transform) {
   orientation = glm::quat_cast(transform);
   position = transform * glm::vec4(0, 0, 0, 1);
   needs_world_planes_update = true;
}

//Set velocity
void ConvexSolid::setVelocity(glm::vec3 new_velocity) {
    velocity = new_velocity;
}

//Set angular velocity
void ConvexSolid::setAngularVelocity(glm::vec3 new_angular_velocity) {
    angular_velocity = new_angular_velocity;
}


// Steps this solid forward by the given amount of time
void ConvexSolid::move(double dt){
    position += velocity*(float)dt;
    float angular_speed = glm::length(angular_velocity);
    if(angular_speed > 0.00001){
        float da = angular_speed  * (float)dt ;
        quat dr = glm::angleAxis(da, angular_velocity/angular_speed );
        orientation = dr * orientation ;
    }
    orientation = glm::normalize(orientation);
    needs_world_planes_update = true;
}


void ConvexSolid::computeWorldPlanes(){
    if (needs_world_planes_update) {
        glm::mat mat = getTransform();
        if (world_vertex.size() == 0) { // construct world planes from scratch
            world_vertex = vector<vec3>();
            world_vertex.reserve(shape->vertex.size());

            world_plane = vector<std::pair<glm::vec3, float>>();
            world_plane.reserve(shape->face.size());
            
            for (int k = 0; k < shape->vertex.size(); k++) {
                world_vertex.push_back(mat * vec4(shape->vertex[k], 1.0f));
            }

            for (int k = 0; k < shape->face.size(); k++) {
                const vec3& A = world_vertex[shape->face[k][0]];
                const vec3& B = world_vertex[shape->face[k][1]];
                const vec3& C = world_vertex[shape->face[k][2]];
                // How normal is defined determines which winding order is "correct"
                vec3 world_face_normal = glm::normalize(glm::cross(B - A, C - A));
                float world_face_d = -glm::dot(A, world_face_normal);
                world_plane.push_back(std::make_pair(world_face_normal, world_face_d));
            }
        } else { // overwrite existing world planes
            for (int k = 0; k < shape->vertex.size(); k++) {
                world_vertex[k] = mat * vec4(shape->vertex[k], 1.0f);
            }

            for (int k = 0; k < shape->face.size(); k++) {
                const vec3& A = world_vertex[shape->face[k][0]];
                const vec3& B = world_vertex[shape->face[k][1]];
                const vec3& C = world_vertex[shape->face[k][2]];
                // How normal is defined determines which winding order is "correct"
                vec3 world_face_normal = glm::normalize(glm::cross(B - A, C - A));
                float world_face_d = -glm::dot(A, world_face_normal);
                world_plane[k] = std::make_pair(world_face_normal, world_face_d);
            }
        }

        
    }
    needs_world_planes_update = false;
}

// computes the inertia and inverse inertia and stores them on this shape instance (they will not be saved in the timeline)
void ConvexSolid::computeInertia(){
    inertia = shape->getInertia(mass);
    inverse_inertia = glm::inverse(inertia);
}

// Checks if there is a collision between this solid and another
// Assumes both solids have computed up to date world planes
// Returns the minimal projection vector to move this object to no longer collide
// If there was a collision the second element will be the point of collision, and third will be normal
// If there is not a collision returns empty vector
std::vector<glm::vec3> ConvexSolid::checkCollision(std::shared_ptr<ConvexSolid> other){
    if(world_vertex.size() == 0 || other->world_vertex.size() ==0){
        printf("Collision check on undefined world_data!\n");
    }

    float best_move = std::numeric_limits<float>::max();
    vec3 best_normal = vec3(0,0,0);
    vec3 best_point = vec3(0,0,0);

    vec3 to_other = other->position - position ;

    //Check their faces for a separating axis
    for (int k = 0; k < other->world_plane.size(); k++) {
        const vec3 &N = other->world_plane[k].first;
        const float d = other->world_plane[k].second;
        if(glm::dot(N, to_other) < 0 ){// early out trick per face if the face points away from the other mesh
            float correction = std::numeric_limits<float>::max();
            // Get the most intersecting point on the other polygon (most behind plane)
            vec3 cp = vec3(0,0,0);
            float nbm = -best_move ;
            for (auto& vertex : world_vertex) {
                float nd = glm::dot(vertex, N) + d ;
                /*if(nd < nbm){ // plane is already worse than our best so we're done with this plane
                    correction = nd ;
                    break;
                }else*/ if(nd < correction){
                    correction = nd ;
                    cp = vertex ;
                }
            }
            if (correction >= 0) { // worst collision is no collision
                return vector<vec3>();  // no collision, we're done
            } else if (-correction < best_move) {
                // keep track of smallest correction
                best_move = -correction;
                best_normal = N;
                best_point = cp + N*(best_move*0.5f); // collision point halfway between plane and point
            }
        }
    }

    // Check our faces for a separating axis
    for (int k = 0; k < world_plane.size(); k++) {
        const vec3 &N = world_plane[k].first;
        const float d = world_plane[k].second;
        
        if(glm::dot(N, to_other) > 0 ){// early out trick per face if the face points away from the other mesh
            float correction = std::numeric_limits<float>::max();
            // Get the most intersecting point on the other polygon (most behind plane)
            vec3 cp = vec3(0,0,0);
            float nbm = -best_move ;
            for (auto& j : other->world_vertex) {
                float nd = glm::dot(j, N) + d ;
                if(nd < nbm){ // plane is already worse than our best so we're done with this plane
                    correction = nd ;
                    break;
                }else if(nd < correction){
                    correction = nd ;
                    cp = j ;
                }
            }
            if (correction >= 0) { // worst collision is no collision
                return vector<vec3>();  // no collision, we're done
            } else if (-correction < best_move) {
                // keep track of smallest correction
                best_move = -correction;
                best_normal = N * -1.0f;
                best_point = cp + N*(best_move*0.5f); // collision point halfway between plane and point
            }
        }
    }

    vector<vec3> result ;
    result.push_back(best_normal * best_move );
    result.push_back(best_point);
    result.push_back(best_normal);
    return result ;
}

// Checks if the given point in world space collides with this solid
// Assumes this solid has computed up to date world planes
// Returns the minimal projection vector to move the point to no longer collide
// returns (0,0,0) if no collision
glm::vec3 ConvexSolid::checkCollision(const glm::vec3& point) {

    float best_move = -std::numeric_limits<float>::max();
    vec3 best_normal = vec3(0, 0, 0);

    // Check our faces for a separating axis
    for (int k = 0; k < world_plane.size(); k++) {
        auto& wp = world_plane[k];
        const vec3& N = wp.first;
        const float& d = wp.second;

        float correction = point.x * N.x + point.y * N.y + point.z * N.z + d;
        if (correction > best_move) {
            if (correction > 0) {
                return glm::vec3(); // found seaprating axis, no collision
            }else {
                best_move = correction;
                best_normal = N;
            }
        }

    }

    return best_normal * (-1.0f * best_move);

}

// Given an object that does collide with the collision point and normal
// return the impulse to be applied to this object to resolve the collision (negative should be applied to other)
glm::vec3 ConvexSolid::getCollisionImpulse(std::shared_ptr<ConvexSolid> other, const vec3& collision_point, const vec3& collision_normal, double elasticity){
    // Formula from Game Physics by David H Eberly, Chapter 5 pg 248
    vec3 rA = collision_point - position;
    vec3 kA = glm::cross(rA,collision_normal);
    vec3 uA = inverse_inertia * kA;
    
    if(other->moveable){
        vec3 rB = collision_point - other->position;
        vec3 kB = glm::cross(rB,collision_normal);
        vec3 uB = other->inverse_inertia * kB;
        double f_num = -(1.0+elasticity)* (glm::dot(collision_normal, velocity - other->velocity) + glm::dot(angular_velocity,kA) - glm::dot(other->angular_velocity,kB));
        double f_den = 1.0/mass + 1.0/other->mass + glm::dot(kA, uA) + glm::dot(kB, uB) ;
        double f = f_num/f_den;
        vec3 impulse = collision_normal*(float)f;
        return impulse;
    }else{
        double f_num = -(1.0+elasticity)* (glm::dot(collision_normal, velocity - other->velocity) + glm::dot(angular_velocity,kA) );
        double f_den = 1.0/mass + glm::dot(kA, uA) ;
        double f = f_num/f_den;
        vec3 impulse = collision_normal*(float)f;
        return impulse;
    }
    
}

// apply an impulse (momentum change) at the given point in world cooredinates
void ConvexSolid::applyImpulse(const vec3& impulse, const vec3& point){
    vec3 rA = point - position;
    velocity += impulse/mass;
    angular_velocity += inverse_inertia * glm::cross(rA, impulse) ;
}

