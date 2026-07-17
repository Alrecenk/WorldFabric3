#include "OptimizationProblem.h"
#include <math.h>
#include <limits>
#include <stdio.h>

using std::vector ;

/*The functionality for this class was converted to C++ from a Java library in Alrecenk's structure from motion repository.
https://github.com/Alrecenk/StructureFromMotion/blob/master/src/Utility.java
*/


//Returns the gradient calculated numerically using the error function
std::vector<float> OptimizationProblem::numericalGradient(const std::vector<float> x, float epsilon){
    vector<float> gradient ;
    double xerror = this->error(x);
    vector<float> xo = x ;
    for(int k=0;k<x.size();k++){
        xo[k] = x[k] + epsilon;
        double xplus = this->error(xo);
        xo[k] = x[k];
        gradient.push_back((float)((xplus-xerror)/(epsilon)));
    }
    return gradient ;
}

std::vector<float> OptimizationProblem::minimizeByLBFGS(const std::vector<float> x0, int m, int maxiter, int stepiter, float tolerance, float min_improvement){
    int iter = 0;
    std::vector<float> x = x0;
    float error = this->error(x0);
    float lasterror = std::numeric_limits<float>::max();
    vector<float> gradient = this->gradient(x0);
    //Console.Out.WriteLine("Start error: " + error + "  gradient norm :" + norm(gradient)) ;
    //error = feval(fun,x.p,1);
    //gradient = feval(fun,x.p,2);
    vector<float> g = gradient;
    //preallocate arrays
    int k = 0, j;
    vector<vector<float>> s ;
    vector<float> rho ;
    vector<vector<float>> y ;
    vector<float> nw ;
    vector<float> r, q;
    float B;

    for(int k=0;k<m;k++){
        s.push_back(vector<float>());
        rho.push_back(0);
        y.push_back(vector<float>());
        nw.push_back(0);
    }

    //quit when acceptable accuracy reached or iterations exceeded
    while (iter < maxiter && error > tolerance && lasterror-error > min_improvement*error){
        lasterror = error;
        iter = iter + 1;
        k = k + 1;
        g = gradient;
        //%initial pass just use gradient descent
        if (k == 1 || x.size() ==1){
            r = g;
        }else{
            //two loop formula
            q = g;
            int i = k - 1;
            while (i >= k - m && i > 0){
                j = i % m; //%index into array operating as a fixed size queue
                nw[j] = rho[j] * dot(s[j], q);
                q = subtract(q, scale(y[j], nw[j]));
                i = i - 1;
            }
            j = (k - 1) % m;
            r = scale(q, dot(s[j], y[j]) / dot(y[j], y[j]));// % gamma,k * q = H0k q
            i = std::max(k - m, 1);
            while (i <= k - 1){
                j = i % m; //%index into array operating as a fixed size queue
                B = rho[j] * dot(y[j], r);

                r = add(r, scale(s[j], nw[j] - B));
                i = i + 1;
            }
        }
        //% get step size
        // alfa = StepSize(fun, x, -r, 1,struct('c1',10^-4,'c2',.9,'maxit',100)) ;
        float alfa = (float)stepSize(x, scale(r, -1), 1, stepiter, .1f, .9f);

        //%apply step and update arrays
        j = k % m;
        s[j] = scale(r, -alfa);
        
        if (containsNan(s[j]) || alfa <= 0){
            printf("Invalid exit condition in LBFGS! %f \n" , alfa);
            return x0;
        }
        x = add(x, s[j]);
        gradient = this->gradient(x);
        error = this->error(x);
        y[j] = subtract(gradient, g);
        rho[j] = 1.0f / dot(y[j], s[j]);
    }
    return x;


}

std::vector<float> OptimizationProblem::minimumByGradientDescent(const std::vector<float> x0, float tolerance, int maxiter, int stepiter){
    vector<float> x = x0 ;
    vector<float> gradient = this->gradient(x0) ;
    int iteration = 0 ;
    while(dot(gradient,gradient) > tolerance*tolerance && iteration < maxiter){
        iteration++ ;
        //calculate step-size in direction of negative gradient
        float alpha = (float)stepSize(x, scale(gradient,-1), 1.0f, stepiter, 0.1f, 0.9f) ;
        x = add( x, scale(gradient, -alpha)) ; // apply step
        gradient = this->gradient(x) ; // get new gradient
    }
    return x ;

}

float OptimizationProblem::stepSize(const std::vector<float> x0, const std::vector<float> d, float alpha, int maxit, float c1, float c2){
    //get error and gradient at starting point  
    float fx0 = this->error(x0);
    float gx0 = dot(this->gradient(x0), d);
    //bound the solution
    float alphaL = 0;
    float alphaR = 1000;
    for (int iter = 1; iter <= maxit; iter++){
        vector<float> xp = add(x0, scale(d, alpha)); // get the point at this alpha
        float erroralpha = this->error(xp); //get the error at that point
        if (isnan(erroralpha) || erroralpha >= fx0 + alpha * c1 * gx0)	{ // if error is not sufficiently reduced
            alphaR = alpha;//move halfway between current alpha and lower alpha
            alpha = (alphaL + alphaR)/2.0f;
        }else{//if error is sufficiently decreased 
            float slopealpha = dot(this->gradient(xp), d); // then get slope along search direction
            if (slopealpha <= c2 * fabs(gx0)){ // if slope sufficiently closer to 0
                return alpha;//then this is an acceptable point
            }else if ( slopealpha >= c2 * gx0) { // if slope is too steep and positive then go to the left
                alphaR = alpha;//move halfway between current alpha and lower alpha
                alpha = (alphaL+ alphaR)/2;
            }else{//if slope is too steep and negative then go to the right of this alpha
                alphaL = alpha;//move halfway between current alpha and upper alpha
                alpha = (alphaL+ alphaR)/2;
            }
        }
    }
    //if ran out of iterations then return the best thing we got
    return alpha;
}

std::vector<float> OptimizationProblem::add(const std::vector<float> a, const std::vector<float> b){
    std::vector<float> c(a.size(),0) ;
    for(int k=0;k<a.size();k++){
        c[k] = a[k] + b[k];
    }
    return c ;
}
std::vector<float> OptimizationProblem::subtract(const std::vector<float> a,const std::vector<float> b){
    std::vector<float> c(a.size(),0) ;
    for(int k=0;k<a.size();k++){
        c[k] = a[k] - b[k];
    }
    return c ;
}
float OptimizationProblem::dot(const std::vector<float> a, const std::vector<float> b){
    float dot = 0 ;
    for(int k=0;k<a.size();k++){
        dot += a[k] * b[k] ;
    }
    return dot ;
}
std::vector<float> OptimizationProblem::scale(const std::vector<float> a, float s){
    std::vector<float> c(a.size(),0) ;
    for(int k=0;k<a.size();k++){
        c[k] = a[k] *s ;
    }
    return c ;
}

double OptimizationProblem::norm(const std::vector<float> a){
    return sqrt(dot(a,a));
}


OptimizationProblem::~OptimizationProblem(){}

// Return the current x for this object
std::vector<float> OptimizationProblem::getX(){
    printf("Called optimization get, but it wasn't defined for this object!\n");
    return std::vector<float>();
}

// Set this object to a given x
void OptimizationProblem::setX(const std::vector<float> x){
    printf("Called optimization set,but it wasn't defined for this object!\n");
}

// Returns the error to be minimized for the given input
float OptimizationProblem::error(const std::vector<float> x){
    printf("Called optimization error,but it wasn't defined for this object!\n");
    return 0;
}

// Returns the gradient of error about a given input
std::vector<float> OptimizationProblem::gradient(const std::vector<float> x){
    return numericalGradient(x, 0.001f);
}

bool OptimizationProblem::containsNan(const std::vector<float> x0){
    for(const auto& x : x0){
        if(isnan(x)){
            return true;
        }
    }
    return false;
}