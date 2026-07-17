#ifndef _HIGHLIGHT_PARTICLE_H_
#define _HIGHLIGHT_PARTICLE_H_ 1


#include "ParticlePlugin.h"

namespace Narball {


	class HighlightParticle {
	public:
		glm::vec3 X, Y, Z;
		float spin_rate = 1.0f;
		float particle_size = 1.0f;
		double start_time, end_time;
		int particle_id = -1;
		glm::vec3 position;
		bool stopped = false;

		HighlightParticle(double start, double end, float spin, float size, const glm::vec4& color) {
			start_time = start;
			end_time = end;
			spin_rate = spin;
			particle_size = size;
			ParticlePlugin* particles = getTool<ParticlePlugin>();
			particle_id = particles->createParticle(0);
			particles->setColor(particle_id, color);
			//Generate a randomly rotated coordinate system

			X = glm::vec3(randomFloat() - 0.5f, randomFloat() - 0.5f, randomFloat() - 0.5f);
			//X = glm::vec3(0, 1, 0);
			Y = glm::vec3(randomFloat() - 0.5f, randomFloat() - 0.5f, randomFloat() - 0.5f);
			Y -= X * glm::dot(X, Y);
			Z = glm::cross(X, Y);
			X /= glm::length(X);
			Y /= glm::length(Y);
			Z /= glm::length(Z);

		}

		//updates hte particle and returns true if it is still alive
		//when this returns false you can delete this object
		bool update(ParticlePlugin* particles, double time, const glm::vec3& center, float radius) {
			if (!stopped) {
				position = center;

			}
			if (time > end_time) {
				if (particle_id != -1) {
					particles->destroyParticle(particle_id);
					particle_id = -1;
				}
				return false;
			}
			float t = (float)((time - start_time) / (end_time - start_time));
			stopped = t > 0.3f;
			float x = (t - 0.5f) * 2.0f;
			float r = sqrtf(1.0f - x * x);
			float y = r * sinf((float)time * spin_rate);
			float z = r * cosf((float)time * spin_rate);

			glm::vec3 render_position = position + (X * x + Y * y + Z * z) * radius;

			particles->setPose(particle_id, render_position, particle_size * r);
			return true;
		}

	};

}


#endif // #ifndef _HIGHLIGHT_PARTICLE_H_