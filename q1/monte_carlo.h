#ifndef _MONTE_CARLO_H_
#define _MONTE_CARLO_H_

/* Function: mc_integrate_1d
 * Find the integral of func from x_min to x_max.  The accuracy is determined
 * by the number of points (n_points) used - the higher the better.
 * Returns NaN on error - test with isnan(value)
 */
double mc_integrate_1d(double (*func)(double), int n_points, 
                       double x_min, double x_max);

#endif
