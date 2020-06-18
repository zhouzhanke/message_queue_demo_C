
#include "monte_carlo.h"

#include <stdlib.h>
#include <math.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_monte.h>
#include <gsl/gsl_monte_plain.h>

#define DIMS 1

/* Function: g
 * This is just a wrapper function to convert a simple 1d function to
 * the GNU Scientific Library's n-dimensional function requirement.
 * The function the user supplies will be passed as 'params'.
 * Note: dim==1 and x will be a 1-element array - the 1d x value.
 */
static double g (double *x, size_t dim, void *params)
{
    /* Cast params to a 1d double-to-double function */
    double (*f)(double);
    f = params;

    /* Evaluate the function for the given x value and return it */
    return f(x[0]);
}


/* Function: mc_integrate_1d
 * Find the integral of func from x_min to x_max.  The accuracy is determined
 * by the number of points (n_points) used - the higher the better.
 * Returns NaN on error - test with isnan(value)
 * Implementation uses the GNU Scientific Library.
 * See https://www.gnu.org/software/gsl/doc/html/index.html
 */
double mc_integrate_1d(double (*func)(double), int n_points, 
                       double x_min, double x_max) {
    double err;
    double result;

    /* Describe the function - it's g, is 1-dimensional, and func will
     * be passed as the params argument 
     */
    gsl_monte_function G = { &g, DIMS, func };

    /* Create and initialize a random number generator */
    gsl_rng *rng;
    gsl_rng_env_setup();
    rng = gsl_rng_alloc(gsl_rng_default);

    if (rng == NULL)
        return nan("Allocation Error");
    
    /* Allocate an object to hold the integrator's state */
    gsl_monte_plain_state *s = gsl_monte_plain_alloc(DIMS);
    if (s == NULL) {
        gsl_rng_free(rng);
        return nan("Allocation Error");
        }
  
    /* Do the integration */
    gsl_monte_plain_integrate(&G, &x_min, &x_max, DIMS, n_points, rng, s, 
                              &result, &err); 

    /* Free the number generator and state, and return the result.  We're
     * not going to return the error bounds, just the integration estimate.
     */
    gsl_monte_plain_free(s);
    gsl_rng_free(rng);

    return result;
    }


