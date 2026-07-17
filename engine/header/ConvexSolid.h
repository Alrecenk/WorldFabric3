#ifndef _CONVEX_SOLID_H_
#define _CONVEX_SOLID_H_ 1

#include "ConvexShape.h"
#include "glm/vec3.hpp"
#include <string>

class ConvexSolid {

    public:
        bool moveable = true;

        //temporary computed variables
        std::vector<glm::vec3> world_vertex;
        std::vector<std::pair<glm::vec3, float>> world_plane;
        int status = 0 ;// for debug render 0 = no collison, 1 = sphere collision, 2 = full collision

        std::shared_ptr<ConvexShape> shape;
        glm::vec3 position;
        glm::vec3 velocity;
        glm::quat orientation;
        glm::vec3 angular_velocity;
        float mass;

        glm::mat3 inertia ;
        glm::mat3 inverse_inertia ;

        ConvexSolid();

        ConvexSolid(std::shared_ptr<ConvexShape> shape, float mass, glm::vec3 p, glm::quat orientation);

        ConvexSolid(glm::vec3 nposition, float nmass, std::shared_ptr<ConvexShape> shape, glm::vec3 nvelocity, glm::quat norientation, glm::vec3 nangular_velocity);
    
        ~ConvexSolid();


        // Returns the matrix mapping the shape's local points into world space
        glm::mat4 getTransform();

        //set the current position
        void setPosition(glm::vec3 new_position);

        //set the orientation
        void setOrientation(glm::quat new_orientation);

        // Sets the rotation and orientation to those extracted form the given matrix
        void setTransform(glm::mat4 transform);

        //Set velocity
        void setVelocity(glm::vec3 new_velocity);

        //Set angular velocity
        void setAngularVelocity(glm::vec3 new_angular_velocity);


        // Steps this solid forward by the given amount of time
        void move(double dt);

        void computeWorldPlanes();

        void computeInertia();

        // Checks if there is a collision between this solid and another
        // Assumes both solids have computed up to date world planes
        // Returns the minimal projection vector to move this object to no longer collide
        // If there was a collision the second element will be the point of collision, and third will be normal
        // If there is not a collision returns empty vector
        std::vector<glm::vec3> checkCollision(std::shared_ptr<ConvexSolid> other);

        // Checks if the given point in world space collides with this solid
        // Assumes this solid has computed up to date world planes
        // Returns the minimal projection vector to move the point to no longer collide
        // returns (0,0,0) if no collision
        glm::vec3 checkCollision(const glm::vec3& point);

        // Given an object that does collide with the collision point and normal
        // return the impulse to be applied to this object to resolve the collision (negative should be applied to other)
        glm::vec3 getCollisionImpulse(std::shared_ptr<ConvexSolid> other,const glm::vec3& collision_point, const glm::vec3& collision_normal, double elasticity);

        // apply an impulse (momentum change) at the given point in world cooredinates
        void applyImpulse(const glm::vec3& impulse, const glm::vec3& point);


        

    private:
        bool needs_world_planes_update = true;
      
};
#endif // #ifndef _CONVEX_SOLID_H_