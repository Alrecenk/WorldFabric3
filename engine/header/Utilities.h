#ifndef _UTILITIES_H_
#define _UTILITIES_H_ 1

#include "SDL3/SDL.h"
#include "glew.h"
#include "SDL3/SDL_opengl.h"
#include <GL/glu.h>
#include <stdio.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>

#include "openvr.h"
#include "glm/glm.hpp"


inline std::string loadTextFile(std::string path) {
	std::ifstream file(path);
	if (file.fail()) {
		printf("Failed to load file: %s\n", path.c_str());
	}
	std::string str;
	std::string file_contents;
	while (std::getline(file, str))
	{
		file_contents += str;
		file_contents.push_back('\n');
	}
	return file_contents;
}


inline long timeMilliseconds() {
	return (long)(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
}

inline long timeNanoseconds() {
	return (long)(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
}

inline std::chrono::high_resolution_clock::time_point now() {
	return std::chrono::high_resolution_clock::now();
}

inline int millisBetween(std::chrono::high_resolution_clock::time_point start, std::chrono::high_resolution_clock::time_point end) {
	return (int)(std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count() * 1000);
}

inline int microsBetween(std::chrono::high_resolution_clock::time_point start, std::chrono::high_resolution_clock::time_point end) {
	return (int)(std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count() * 1000000);
}


inline float randomFloat() {
	srand(rand() ^ timeNanoseconds()); // need to srand every call or multithreaded calls will reset
	return (float)((rand() % 32767) / 32767.0);
}

// Overriding std::hash allows data types to be used as keys in unordered maps and sets
template<>
struct std::hash<glm::ivec3>
{
	std::size_t operator()(const glm::ivec3& p) const noexcept
	{
		std::size_t h1 = std::hash<int>{}(p.x);
		std::size_t h2 = std::hash<int>{}(p.y);
		std::size_t h3 = std::hash<int>{}(p.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2); 
	}
};


// Overriding std::hash allows data types to be used as keys in unordered maps and sets
template<>
struct std::hash<glm::vec2>
{
	std::size_t operator()(const glm::vec2& p) const noexcept
	{
		std::size_t h1 = std::hash<float>{}(p.x);
		std::size_t h2 = std::hash<float>{}(p.y);
		return h1 ^ (h2 << 1) ;
	}
};

template<typename A,typename B> struct std::hash<std::pair<A, B>> {
	std::size_t operator()(const std::pair<A, B>& s) const noexcept {
		std::size_t h1 = std::hash<A>{}(s.first);
		std::size_t h2 = std::hash<B>{}(s.second);
		return h1 ^ (h2 << 1);
	}
};


// Returns the value at (x,y) of a smooth wavelet centered at (cx,cy) with amplitude a, and radius r 
inline float waveletValue(float x, float y, float cx, float cy, float a, float r) {
	float d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);

	float x2 = d2 / (r * r);
	if (x2 > 1.0f) {
		return 0;
	}
	float x4 = x2 * x2;
	return (x4 - 2 * x2 + 1.0f) * a;
}


//concatenate a string and an integer
inline std::string concat(std::string s, int value) {
	std::stringstream ss;
	ss << s << value;
	return ss.str();
}

inline bool isInt(const std::string& s){
	const char* first = s.data();
	const char* last = first + s.size();
	int value = 0;
	std::from_chars_result r = std::from_chars(first, last, value, 10);
	return r.ec == std::errc() && r.ptr == last ; // Blank error and read the entire string
}

//Do a find and replace of all instances in a string
inline std::string replaceAll(const std::string& s, std::string const& to_replace, std::string const& replace_with) {
	std::string buf;
	std::size_t pos = 0;
	std::size_t prevPos;

	// Reserves rough estimate of final size of string.
	buf.reserve(s.size());

	while (true) {
		prevPos = pos;
		pos = s.find(to_replace, pos);
		if (pos == std::string::npos)
			break;
		buf.append(s, prevPos, pos - prevPos);
		buf += replace_with;
		pos += to_replace.size();
	}

	buf.append(s, prevPos, s.size() - prevPos);
	return buf;
}

// Allow global tools to be fetched by type
template <typename T>
std::shared_ptr<T> tool = nullptr ;

template <typename T>
T* getTool(){
	return tool<T>.get();
}

template <typename T>
void addTool(std::shared_ptr<T> t) {
	tool<T> = t ;
}

template <typename T>
void removeTool() {
	tool<T>.reset() ;
}






class ComplexNumber {
public:
	double real = 0;
	double imaginary = 0;

	ComplexNumber() {}; // 0 construction by default

	ComplexNumber(double r, double i) {
		real = r;
		imaginary = i;
	}

	ComplexNumber operator +(const ComplexNumber& x) const {
		ComplexNumber r;
		r.real = real + x.real;
		r.imaginary = imaginary + x.imaginary;
		return r;
	}

	ComplexNumber operator +(const double& xr) const {
		ComplexNumber r;
		r.real = real + xr;
		r.imaginary = imaginary;
		return r;
	}

	ComplexNumber operator +=(const ComplexNumber& x) {
		real += x.real;
		imaginary += x.imaginary;
		return *this;
	}

	ComplexNumber operator +=(const double& r) {
		real += r;
		return *this;
	}

	ComplexNumber operator *(const ComplexNumber& x) const {
		ComplexNumber r;
		r.real = real * x.real - imaginary * x.imaginary;
		r.imaginary = real * x.imaginary + imaginary * x.real;
		return r;
	}

	ComplexNumber operator *(const double& xr) const {
		ComplexNumber r;
		r.real = real * xr;
		r.imaginary = imaginary * xr;
		return r;
	}

	ComplexNumber operator *= (const ComplexNumber& x) {
		ComplexNumber r;
		r.real = real * x.real - imaginary * x.imaginary;
		r.imaginary = real * x.imaginary + imaginary * x.real;
		real = r.real;
		imaginary = r.imaginary;
		return *this;
	}

	ComplexNumber operator *=(const double& xr) {
		real *= xr;
		imaginary *= xr;
		return *this;
	}



};

//A 1 dimensional polynomial with real valued coefficients
class Polynomial {
public:
	//value is sum of coef[i] * x^i 
	std::vector<double> coefficients;

	Polynomial(const std::vector<double>& coef) {
		int first_coef = (int)coef.size() - 1;
		while (coef[first_coef] == 0 && first_coef > 0) {
			first_coef--;
		}
		coefficients = std::vector<double>(first_coef + 1, 0.0);
		for (int k = 0; k <= first_coef; k++) {
			coefficients[k] = coef[k];
		}

	}

	double operator [](const float& x) {
		double y = 0;
		double xi = 1;
		for (double& c : coefficients) {
			y += xi * c;
			xi *= x;
		}
		return y;
	}

	ComplexNumber operator[](const ComplexNumber& x) {
		apply(x);
	}

	ComplexNumber apply(const ComplexNumber& x) {
		ComplexNumber y;
		ComplexNumber xi(1.0f, 0.0f);
		for (double& c : coefficients) {
			y += xi * c;
			xi *= x;
		}
		return y;
	}

	std::string toString() const {
		std::stringstream ss;

		ss << coefficients[0];
		for (int k = 1; k < coefficients.size(); k++) {
			ss << " + " << coefficients[k] << " *x^" << k;
		}
		return ss.str();
	}

	//Note: default parameters are optimized for roots and coefficients around magnitude 1 (like audio LPC results)
	//For general use, parameters may require tweakng to converge reliably
	std::vector<ComplexNumber> findRoots(double max_step = 2.0, int max_iterations = 1000, double tolerance = 1e-9) const {
		double max_step2 = max_step * max_step;
		int degree = (int)coefficients.size() - 1;
		std::vector<ComplexNumber> all_roots;
		//base cases for degree 0 to 2
		if (degree <= 0) return all_roots;
		if (degree == 1) {
			all_roots.push_back(ComplexNumber(-coefficients[0] / coefficients[1], 0.0));
			return all_roots;
		}
		if (degree == 2) {
			double a = coefficients[2];
			double b = coefficients[1];
			double c = coefficients[0];
			double disc = b * b - 4.0 * a * c;

			if (disc >= 0) {
				all_roots.push_back(ComplexNumber((-b + sqrt(disc)) / (2.0 * a), 0.0));
				all_roots.push_back(ComplexNumber((-b - sqrt(disc)) / (2.0 * a), 0.0));
			}
			else {
				all_roots.push_back(ComplexNumber(-b / (2.0 * a), sqrt(-disc) / (2.0 * a)));
				all_roots.push_back(ComplexNumber(-b / (2.0 * a), -sqrt(-disc) / (2.0 * a)));
			}
			return all_roots;
		}

		// Normalize polynomial so that the leading coefficient a[n] = 1.0
		std::vector<double> a_norm(degree + 1);
		double lead = coefficients[degree];
		for (int i = 0; i <= degree; i++) {
			a_norm[i] = coefficients[i] / lead;
		}

		double u = (randomFloat() - 0.5);
		double v = (randomFloat() - 0.5); // Initial guesses for x^2 - ux - v
		bool converged = false;

		for (int iter = 0; iter < max_iterations; ++iter) {
			std::vector<double> b(degree + 1), du(degree + 1), dv(degree + 1);

			b[degree] = 1.0;
			b[degree - 1] = a_norm[degree - 1] + u * b[degree];
			du[degree] = 0.0;
			du[degree - 1] = b[degree];
			dv[degree] = 0.0;
			dv[degree - 1] = 0.0;

			for (int i = degree - 2; i >= 0; --i) {
				b[i] = a_norm[i] + u * b[i + 1] + v * b[i + 2];
				du[i] = b[i + 1] + u * du[i + 1] + v * du[i + 2];
				dv[i] = b[i + 2] + u * dv[i + 1] + v * dv[i + 2];
			}

			// Check if remainder b[1]x + b[0] is approximately zero
			if (std::abs(b[0]) < tolerance && std::abs(b[1]) < tolerance) {
				converged = true;
				break;
			}

			// 2x2 Jacobian system for Newton-Raphson
			// [ du[1]  dv[1] ] [ step_u ] = [ -b[1] ]
			// [ du[0]  dv[0] ] [ step_v ] = [ -b[0] ]
			double det = du[1] * dv[0] - dv[1] * du[0];
			if (std::abs(det) < tolerance) {
				u += (randomFloat() - 0.5) * max_step * 0.1; // jiggle at random to get unstuck
				v += (randomFloat() - 0.5) * max_step * 0.1;
				continue;
			}
			double step_u = (-b[1] * dv[0] + b[0] * dv[1]) / det;
			double step_v = (du[0] * b[1] - du[1] * b[0]) / det;

			double magnitude = step_u * step_u + step_v * step_v;
			if (magnitude > max_step2) {
				double scale = max_step / sqrt(magnitude);
				step_u *= scale; // still go in that direction but limited to the step
				step_v *= scale;
				step_u += (randomFloat() - 0.5) * max_step * 0.1; // also jiggle at random to get out of singularities
				step_v += (randomFloat() - 0.5) * max_step * 0.1;
			}

			u += step_u;
			v += step_v;

			/*
			if (iter == max_iterations - 1) {
				printf("Polynomial root solver hit max iterations, answer is probably wrong!\n");
			}
			*/
		}

		// Solve the found quadratic factor (x^2 - ux - v = 0)
		// and add its roots to the list
		double disc = u * u + 4.0 * v;
		if (disc >= 0) {
			all_roots.push_back(ComplexNumber((u + sqrt(disc)) / 2.0, 0.0));
			all_roots.push_back(ComplexNumber((u - sqrt(disc)) / 2.0, 0.0));
		}
		else {
			all_roots.push_back(ComplexNumber(u / 2.0, sqrt(-disc) / 2.0));
			all_roots.push_back(ComplexNumber(u / 2.0, -sqrt(-disc) / 2.0));
		}

		// Deflate the polynomial to remove the found quadratic factor
		std::vector<double> next_coefs;
		next_coefs.reserve(degree - 1);

		// Repeat the synthetic division one last time to get clean b values
		std::vector<double> b_final(degree + 1);
		b_final[degree] = a_norm[degree];
		b_final[degree - 1] = a_norm[degree - 1] + u * b_final[degree];
		for (int i = degree - 2; i >= 0; --i) {
			b_final[i] = a_norm[i] + u * b_final[i + 1] + v * b_final[i + 2];
		}

		for (int i = 2; i <= degree; i++) {
			next_coefs.push_back(b_final[i]);
		}

		// Recursive call to find the remaining roots in the deflated polynomial
		Polynomial deflated_poly(next_coefs);
		std::vector<ComplexNumber> remaining_roots = deflated_poly.findRoots();
		all_roots.insert(all_roots.end(), remaining_roots.begin(), remaining_roots.end());

		return all_roots;
	}



	void testRoots() {
		std::vector<ComplexNumber> roots = findRoots();

		std::string p_string = toString();
		printf("%s\n", p_string.c_str());
		for (int k = 0; k < roots.size(); k++) {
			ComplexNumber value = apply(roots[k]);
			printf("root %d : %f + %f * i  - >  value: %f + %f i \n", k, (float)roots[k].real, (float)roots[k].imaginary, (float)value.real, (float)value.imaginary);
		}
	}
};




// Given a ray in model space (p + v*t) return the t value of the nearest collision with the bounding box defined by min and max
// return negative if no collision
inline float rayTraceBoundingBox(const glm::vec3& p, const glm::vec3& v, const glm::vec3& min, const glm::vec3& max){

	float enter_t = -std::numeric_limits<float>::max();
	float exit_t = std::numeric_limits<float>::max();

	if (fabs(v.x) > 0.0001f) {
		float t1 = (min.x - p.x) / v.x;
		float t2 = (max.x - p.x) / v.x;
		if (t2 < t1) {
			float temp = t1;
			t1 = t2;
			t2 = temp;
		}
		enter_t = fmax(t1, enter_t);
		exit_t = fmin(t2, exit_t);
	}
	else if (p.x < min.x || p.x > max.x) { // np change in axis means either always in or always out
		return -1.0f;
	}

	if (fabs(v.y) > 0.0001f) {
		float t1 = (min.y - p.y) / v.y;
		float t2 = (max.y - p.y) / v.y;
		if (t2 < t1) {
			float temp = t1;
			t1 = t2;
			t2 = temp;
		}
		enter_t = fmax(t1, enter_t);
		exit_t = fmin(t2, exit_t);
	}
	else if (p.y < min.y || p.y > max.y) { // np change in axis means either always in or always out
		return -1.0f;
	}

	if (fabs(v.z) > 0.0001f) {
		float t1 = (min.z - p.z) / v.z;
		float t2 = (max.z - p.z) / v.z;
		if (t2 < t1) {
			float temp = t1;
			t1 = t2;
			t2 = temp;
		}
		enter_t = fmax(t1, enter_t);
		exit_t = fmin(t2, exit_t);
	}
	else if (p.z < min.z || p.z > max.z) { // np change in axis means either always in or always out
		return -1.0f;
	}

	if (exit_t < 0 || exit_t < enter_t) {
		return -1.0f;
	}
	else {
		return enter_t;
	}

}

inline bool boundingBoxCollision(const glm::vec3& min_1, const glm::vec3& max_1, const glm::vec3& min_2, const glm::vec3& max_2){
	return min_1.x <= max_2.x && min_1.y <= max_2.y && min_1.z <= max_2.z && min_2.x <= max_1.x && min_2.y <= max_1.y && min_2.z <= max_2.z  ;
}

#endif // #ifndef _UTILITIES_H_