#ifndef _OPTIMIZATION_PROBLEM_H_
#define _OPTIMIZATION_PROBLEM_H_ 1

#include <vector>

class OptimizationProblem {
  public:

    // virtual destructor makes sure the right destructor is called on child classes
    virtual ~OptimizationProblem();

    // Return the current x for this object
    virtual std::vector<float> getX();

    // Set this object to a given x
    virtual void setX(const std::vector<float> x);

    // Returns the error to be minimized for the given input
    virtual float error(const std::vector<float> x);

    // Returns the gradient of error about a given input
    virtual std::vector<float> gradient(const std::vector<float> x);

    //Returns the gradient calculated numerically using the error function
    std::vector<float> numericalGradient(const std::vector<float> x, float epsilon);


    //Returns the local minimum by using the LBFGS method starting from x0
	//maxiter is the maximum iterations of LBFGS to perform
	//stepiter is the maximum iterations to perform in line search
	//tolerance maximum gradient norm of error used an an exit condition
	//min_improvement is a fraction of the error removed in this step considered as an exit condition
    std::vector<float> minimizeByLBFGS(const std::vector<float> x0, int m, int maxiter, int stepiter, float tolerance, float min_improvement);

    //starting from w0 searches for a weight vector using gradient descent
	//and Wolfe condition line-search until the gradient magnitude is below tolerance
	//or a maximum number of iterations is reached
    std::vector<float> minimumByGradientDescent(const std::vector<float> x0, float tolerance, int maxiter, int stepiter);

    //Performs a binary search to satisfy the Wolfe conditions
	//returns alpha where next x = x0 + alpha*d 
	//guarantees convergence as long as search direction is bounded away from being orthogonal with gradient
	//x0 is starting point, d is search direction, alpha is starting step size, maxit is max iterations
	//c1 and c2 are the constants of the Wolfe conditions (0.1 and 0.9 can work)
    float stepSize(const std::vector<float> x0, const std::vector<float> d, float alpha, int maxit, float c1, float c2);

    static std::vector<float> add(std::vector<float> a, std::vector<float> b);
    static std::vector<float> subtract(std::vector<float> a, std::vector<float> b);
    static float dot(std::vector<float> a, std::vector<float> b);
    static std::vector<float> scale(std::vector<float> a, float s);
    static double norm(std::vector<float> a);

    static bool containsNan(const std::vector<float> x0);

};

#endif // #ifndef _OPTIMIZATION_PROBLEM_H_