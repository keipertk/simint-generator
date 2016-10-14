#include <math.h> // for fabs()

#include "simint/simint_eri.h"
#include "simint/shell/shell_screen.h"
#include "simint/vectorization/vectorization.h"

double
simint_shellscreen_schwarz_max(struct simint_shell const * A,
                               struct simint_shell const * B)
{
    const int ncart1 = ((A->am+1) * (A->am+2))/2;
    const int ncart2 = ((B->am+1) * (B->am+2))/2;
    const int ncart12 = ncart1*ncart2;
    const int ncart1234 = ncart12*ncart12;

    // holds the calculated integrals
    double integrals[ncart1234] SIMINT_ALIGN_ARRAY_DBL;

    struct simint_multi_shellpair P;
    simint_initialize_multi_shellpair(&P);
    simint_create_multi_shellpair(1, A, 1, B, &P, 0);

    // calculate (ab|ab)
    simint_compute_eri(&P, &P, 0.0, integrals);

    // find the max value
    double max = 0;
    for(int n = 0; n < ncart1234; n++)
    {
        double abint = fabs(integrals[n]);
        max = ( abint > max ? abint : max );
    }

    simint_free_multi_shellpair(&P);

    return max;
}


double
simint_primscreen_schwarz_max(struct simint_shell const * A,
                              struct simint_shell const * B,
                              double * out)
{
    const int ncart1 = ((A->am+1) * (A->am+2))/2;
    const int ncart2 = ((B->am+1) * (B->am+2))/2;
    const int ncart12 = ncart1*ncart2;
    const int ncart1234 = ncart12*ncart12;

    // holds the calculated integrals
    double integrals[ncart1234] SIMINT_ALIGN_ARRAY_DBL;

    // here, we basically uncontract the shells
    struct simint_shell new_A, new_B;
    simint_initialize_shell(&new_A);
    simint_initialize_shell(&new_B);

    const int same_shell = compare_shell(A, B);

    struct simint_multi_shellpair P;
    simint_initialize_multi_shellpair(&P);


    double total_max = 0.0;

    int idx = 0;
    for(int i = 0; i < A->nprim; i++)
    {
        simint_create_shell(1, A->am, A->x, A->y, A->z,
                            A->alpha + i, A->coef + i,
                            &new_A);

        const int Bend = (same_shell ? (i+1) : B->nprim);
        for(int j = 0; j < Bend; j++)
        {
            // I don't think we need a factor of 2 for the off-diagonal,
            // do we?
            simint_create_shell(1, B->am, B->x, B->y, B->z,
                                B->alpha + j, B->coef + j,
                                &new_B);

            // create a shell pair
            // Note - we aren't screening this. That would be a infinite loop.
            simint_create_multi_shellpair(1, &new_A, 1, &new_B, &P, 0); 

            // calculate (ab|ab)
            simint_compute_eri(&P, &P, 0.0, integrals);

            // find the maximum value and store in the output array
            double max = 0;
            for(int m = 0; m < ncart1; m++)
            {
                const int idx_m = m*ncart2*ncart1*ncart2 +
                                  m*ncart2;

                for(int n = 0; n < ncart2; n++)
                {
                    const int idx_mn = idx_m +
                                    n*ncart1*ncart2 +
                                    n;

                    double abint = fabs(integrals[idx_mn]);
                    max = ( abint > max ? abint : max );
                }
            }

            // we avoid doing the sqrt by squaring the tolerance later
            if(out != NULL)
                out[idx++] = max;

            if(max > total_max)
                total_max = max;
        }

    }

    simint_free_multi_shellpair(&P);
    simint_free_shell(&new_A);
    simint_free_shell(&new_B);

    return total_max;
}