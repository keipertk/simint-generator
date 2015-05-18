#include <string.h>
#include <math.h>

#include "vectorization.h"
#include "constants.h"
#include "eri/shell.h"


int eri_FOcombined_pppp(struct multishell_pair const P,
                        struct multishell_pair const Q,
                        double * const restrict S_1_1_1_1)
{

    ASSUME_ALIGN(P.x);
    ASSUME_ALIGN(P.y);
    ASSUME_ALIGN(P.z);
    ASSUME_ALIGN(P.PA_x);
    ASSUME_ALIGN(P.PA_y);
    ASSUME_ALIGN(P.PA_z);
    ASSUME_ALIGN(P.bAB_x);
    ASSUME_ALIGN(P.bAB_y);
    ASSUME_ALIGN(P.bAB_z);
    ASSUME_ALIGN(P.alpha);
    ASSUME_ALIGN(P.prefac);

    ASSUME_ALIGN(Q.x);
    ASSUME_ALIGN(Q.y);
    ASSUME_ALIGN(Q.z);
    ASSUME_ALIGN(Q.PA_x);
    ASSUME_ALIGN(Q.PA_y);
    ASSUME_ALIGN(Q.PA_z);
    ASSUME_ALIGN(Q.bAB_x);
    ASSUME_ALIGN(Q.bAB_y);
    ASSUME_ALIGN(Q.bAB_z);
    ASSUME_ALIGN(Q.alpha);
    ASSUME_ALIGN(Q.prefac);
    ASSUME_ALIGN(integrals)

    const int nshell1234 = P.nshell12 * Q.nshell12;

    memset(S_1_1_1_1, 0, nshell1234*81*sizeof(double));

    // Holds AB_{xyz} and CD_{xyz} in a flattened fashion for later
    double AB_x[nshell1234];  double CD_x[nshell1234];
    double AB_y[nshell1234];  double CD_y[nshell1234];
    double AB_z[nshell1234];  double CD_z[nshell1234];

    int ab, cd, abcd;
    int i, j;

    // Workspace for contracted integrals
    double S_1_0_1_0[nshell1234 * 9];
    memset(S_1_0_1_0, 0, (nshell1234 * 9) * sizeof(double));

    double S_1_0_2_0[nshell1234 * 18];
    memset(S_1_0_2_0, 0, (nshell1234 * 18) * sizeof(double));

    double S_1_1_1_0[nshell1234 * 27];
    memset(S_1_1_1_0, 0, (nshell1234 * 27) * sizeof(double));

    double S_1_1_2_0[nshell1234 * 54];
    memset(S_1_1_2_0, 0, (nshell1234 * 54) * sizeof(double));

    double S_2_0_1_0[nshell1234 * 18];
    memset(S_2_0_1_0, 0, (nshell1234 * 18) * sizeof(double));

    double S_2_0_2_0[nshell1234 * 36];
    memset(S_2_0_2_0, 0, (nshell1234 * 36) * sizeof(double));



    ////////////////////////////////////////
    // Loop over shells and primitives
    ////////////////////////////////////////
    for(ab = 0, abcd = 0; ab < P.nshell12; ++ab)
    {
        const int abstart = P.primstart[ab];
        const int abend = P.primend[ab];

        // this should have been set/aligned in fill_multishell_pair or something else
        ASSUME(abstart%SIMD_ALIGN_DBL == 0);

        for(cd = 0; cd < Q.nshell12; ++cd, ++abcd)
        {
            const int cdstart = Q.primstart[cd];
            const int cdend = Q.primend[cd];

            // this should have been set/aligned in fill_multishell_pair or something else
            ASSUME(cdstart%SIMD_ALIGN_DBL == 0);

            // Store for later
            AB_x[abcd] = P.AB_x[ab];  CD_x[abcd] = Q.AB_x[cd];
            AB_y[abcd] = P.AB_y[ab];  CD_y[abcd] = Q.AB_y[cd];
            AB_z[abcd] = P.AB_z[ab];  CD_z[abcd] = Q.AB_z[cd];

            for(i = abstart; i < abend; ++i)
            {
                for(j = cdstart; j < cdend; ++j)
                {

                    // Holds the auxiliary integrals ( i 0 | 0 0 )^m in the primitive basis
                    // with m as the slowest index
                    // AM = 0: Needed from this AM: 1
                    double AUX_S_0_0_0_0[5 * 1];

                    // AM = 1: Needed from this AM: 3
                    double AUX_S_1_0_0_0[4 * 3];

                    // AM = 2: Needed from this AM: 6
                    double AUX_S_2_0_0_0[3 * 6];

                    // AM = 3: Needed from this AM: 10
                    double AUX_S_3_0_0_0[2 * 10];

                    // AM = 4: Needed from this AM: 15
                    double AUX_S_4_0_0_0[1 * 15];



                    const double PQalpha_mul = P.alpha[i] * Q.alpha[j];
                    const double PQalpha_sum = P.alpha[i] + Q.alpha[j];

                    const double pfac = TWO_PI_52 / (PQalpha_mul * sqrt(PQalpha_sum));

                    /* construct R2 = (Px - Qx)**2 + (Py - Qy)**2 + (Pz -Qz)**2 */
                    const double PQ_x = P.x[i] - Q.x[j];
                    const double PQ_y = P.y[i] - Q.y[j];
                    const double PQ_z = P.z[i] - Q.z[j];
                    const double R2 = PQ_x*PQ_x + PQ_y*PQ_y + PQ_z*PQ_z;

                    // collected prefactors
                    const double allprefac =  pfac * P.prefac[i] * Q.prefac[j];

                    // various factors
                    const double alpha = PQalpha_mul/PQalpha_sum;   // alpha from MEST
                    // for VRR
                    const double one_over_p = 1.0 / P.alpha[i];
                    const double a_over_p =  alpha * one_over_p;     // a/p from MEST
                    const double one_over_2p = 0.5 * one_over_p;  // gets multiplied by i in VRR
                    // for electron transfer
                    const double one_over_q = 1.0 / Q.alpha[j];
                    const double one_over_2q = 0.5 * one_over_q;
                    const double p_over_q = P.alpha[i] * one_over_q;

                    const double etfac[3] = {
                                             -(P.bAB_x[i] + Q.bAB_x[j]) * one_over_q,
                                             -(P.bAB_y[i] + Q.bAB_y[j]) * one_over_q,
                                             -(P.bAB_z[i] + Q.bAB_z[j]) * one_over_q,
                                            };


                    //////////////////////////////////////////////
                    // Boys function section
                    // Maximum v value: 4
                    //////////////////////////////////////////////
                    // The paremeter to the boys function
                    const double F_x = R2 * alpha;


                    AUX_S_0_0_0_0[0] = allprefac
                             * pow(
                                     (
                                       (
                                                   1.
                                         + F_x * ( 0.414016243000866299
                                         + F_x * ( 0.130448682735044324
                                         + F_x * ( 0.0281490811816026161
                                         + F_x * ( 0.00462868463720416281
                                         + F_x * ( 0.00062025147610678493
                                         + F_x * ( 0.0000686770885390617984
                                         + F_x * ( 6.28488230669978749e-6
                                         + F_x * ( 5.01986197619830788e-7
                                         + F_x * ( 3.96915046153987083e-8
                                         + F_x * ( 1.14619057438675389e-9
                                         + F_x * ( 2.21422857239286206e-10
                                         + F_x * ( -3.47087628137958658e-12
                                         + F_x * ( 5.26907054399378694e-13
                                                 )))))))))))))
                                       )
                                       /
                                       (
                                                   1.0
                                         + F_x * ( 1.08068290966581548
                                         + F_x * ( 0.539792844780494916
                                         + F_x * ( 0.166084230769033217
                                         + F_x * ( 0.035790298646556066
                                         + F_x * ( 0.00589703243578234382
                                         + F_x * ( 0.000789592116312117277
                                         + F_x * ( 0.0000874456178357265776
                                         + F_x * ( 8.00211107603171383e-6
                                         + F_x * ( 6.39149165582055646e-7
                                         + F_x * ( 5.05367903369666356e-8
                                         + F_x * ( 1.45937517486452486e-9
                                         + F_x * ( 2.81924337930412797e-10
                                         + F_x * ( -4.4192569363292127e-12
                                         + F_x * ( 6.70878898061210528e-13
                                                 ))))))))))))))
                                       )
                                     ), 0.0+0.5);

                    AUX_S_0_0_0_0[1] = allprefac
                             * pow(
                                     (
                                       (
                                                   0.480749856769136127
                                         + F_x * ( 0.0757107453935371611
                                         + F_x * ( 0.0207544733443622468
                                         + F_x * ( 0.00296686757159093428
                                         + F_x * ( 0.000385086850988198076
                                         + F_x * ( 0.0000396245291118678106
                                         + F_x * ( 3.44653527568129186e-6
                                         + F_x * ( 2.60728584781887378e-7
                                         + F_x * ( 1.64651276793705422e-8
                                         + F_x * ( 1.24924114646889903e-9
                                         + F_x * ( -4.06609033385506782e-13
                                         + F_x * ( 6.69244577908522819e-12
                                         + F_x * ( -1.74969643368118421e-13
                                         + F_x * ( 9.69901476816967128e-15
                                                 )))))))))))))
                                       )
                                       /
                                       (
                                                   1.0
                                         + F_x * ( 0.557484696706444012
                                         + F_x * ( 0.16330778039059299
                                         + F_x * ( 0.0332854355906960737
                                         + F_x * ( 0.00522222434942978777
                                         + F_x * ( 0.000658578219691024581
                                         + F_x * ( 0.0000682687148667475058
                                         + F_x * ( 5.92820704830126442e-6
                                         + F_x * ( 4.486040686836368e-7
                                         + F_x * ( 2.83282406028920981e-8
                                         + F_x * ( 2.14933012694134515e-9
                                         + F_x * ( -6.99576077110757546e-13
                                         + F_x * ( 1.15144066901510615e-11
                                         + F_x * ( -3.01036676011995688e-13
                                         + F_x * ( 1.66872327689919498e-14
                                                 ))))))))))))))
                                       )
                                     ), 1.0+0.5);

                    AUX_S_0_0_0_0[2] = allprefac
                             * pow(
                                     (
                                       (
                                                   0.525305560880753447
                                         + F_x * ( 0.110492812543561698
                                         + F_x * ( 0.0191075521270522133
                                         + F_x * ( 0.00308864958825646759
                                         + F_x * ( 0.000365092211441395786
                                         + F_x * ( 0.0000386927386117543446
                                         + F_x * ( 3.43285176619925111e-6
                                         + F_x * ( 2.60432969408429629e-7
                                         + F_x * ( 1.81615413272499949e-8
                                         + F_x * ( 8.79574269616801187e-10
                                         + F_x * ( 8.17788745331821633e-11
                                         + F_x * ( 9.41377749237483758e-13
                                         + F_x * ( 1.10425964672642921e-13
                                         + F_x * ( 6.7330075025747763e-15
                                                 )))))))))))))
                                       )
                                       /
                                       (
                                                   1.0
                                         + F_x * ( 0.496054363546458276
                                         + F_x * ( 0.128217363190316964
                                         + F_x * ( 0.0237743767099492677
                                         + F_x * ( 0.00352539772392101481
                                         + F_x * ( 0.00043510113976810022
                                         + F_x * ( 0.0000454073941207125778
                                         + F_x * ( 4.04510802860155619e-6
                                         + F_x * ( 3.06619642129059298e-7
                                         + F_x * ( 2.13853590636569169e-8
                                         + F_x * ( 1.03568903196894899e-9
                                         + F_x * ( 9.62938758302946861e-11
                                         + F_x * ( 1.10846362277666573e-12
                                         + F_x * ( 1.3002555622660695e-13
                                         + F_x * ( 7.92805431387221855e-15
                                                 ))))))))))))))
                                       )
                                     ), 2.0+0.5);

                    AUX_S_0_0_0_0[3] = allprefac
                             * pow(
                                     (
                                       (
                                                   0.573513198744647626
                                         + F_x * ( 0.0884965100710721081
                                         + F_x * ( 0.0125945026937261292
                                         + F_x * ( 0.0018131346755066484
                                         + F_x * ( 0.000185501511589852242
                                         + F_x * ( 0.0000179665588981785096
                                         + F_x * ( 1.4663716751615509e-6
                                         + F_x * ( 1.07200007061958399e-7
                                         + F_x * ( 6.13886384827129358e-9
                                         + F_x * ( 5.0757503319816481e-10
                                         + F_x * ( -3.89402001604364884e-12
                                         + F_x * ( 3.54266040828850291e-12
                                         + F_x * ( -1.30207429069121938e-13
                                         + F_x * ( 6.46345036567436585e-15
                                                 )))))))))))))
                                       )
                                       /
                                       (
                                                   1.0
                                         + F_x * ( 0.376528191549264251
                                         + F_x * ( 0.0764525025412929857
                                         + F_x * ( 0.0117343580663816836
                                         + F_x * ( 0.00149763967922572319
                                         + F_x * ( 0.000163513443673380925
                                         + F_x * ( 0.0000154572094390025412
                                         + F_x * ( 1.26979191105616802e-6
                                         + F_x * ( 9.27046437188452642e-8
                                         + F_x * ( 5.30988729375523733e-9
                                         + F_x * ( 4.39022184757595294e-10
                                         + F_x * ( -3.36809850708008399e-12
                                         + F_x * ( 3.06419443797353572e-12
                                         + F_x * ( -1.12621824892382023e-13
                                         + F_x * ( 5.59050724287616386e-15
                                                 ))))))))))))))
                                       )
                                     ), 3.0+0.5);

                    AUX_S_0_0_0_0[4] = allprefac
                             * pow(
                                     (
                                       (
                                                   0.613685849032916035
                                         + F_x * ( 0.00903173132995705395
                                         + F_x * ( 0.0069284473671474595
                                         + F_x * ( 0.000435156706530149163
                                         + F_x * ( 0.0000488085495651258633
                                         + F_x * ( 3.66096953726522648e-6
                                         + F_x * ( 2.59751177273597696e-7
                                         + F_x * ( 1.71762934556763709e-8
                                         + F_x * ( 6.36469395210182763e-10
                                         + F_x * ( 9.79098463143548107e-11
                                         + F_x * ( -5.17453551995652055e-12
                                         + F_x * ( 7.31427836739056021e-13
                                         + F_x * ( -2.84387961377947079e-14
                                         + F_x * ( 8.85040027079679332e-16
                                                 )))))))))))))
                                       )
                                       /
                                       (
                                                   1.0
                                         + F_x * ( 0.196535371894127447
                                         + F_x * ( 0.0279517601730765951
                                         + F_x * ( 0.00336319077005095874
                                         + F_x * ( 0.000348115741765303429
                                         + F_x * ( 0.0000314536554282140664
                                         + F_x * ( 2.50784176166426963e-6
                                         + F_x * ( 1.75165336933914539e-7
                                         + F_x * ( 1.16200064640254918e-8
                                         + F_x * ( 4.30354125246412109e-10
                                         + F_x * ( 6.62084177654364664e-11
                                         + F_x * ( -3.49910640701992041e-12
                                         + F_x * ( 4.94603523474290392e-13
                                         + F_x * ( -1.92307812889565772e-14
                                         + F_x * ( 5.98478610349119398e-16
                                                 ))))))))))))))
                                       )
                                     ), 4.0+0.5);


                    //////////////////////////////////////////////
                    // Primitive integrals: Vertical recurrance
                    //////////////////////////////////////////////

                    int idx = 0;

                    // Forming AUX_S_1_0_0_0[4 * 3];
                    // Needed from this AM:
                    //    P_100
                    //    P_010
                    //    P_001
                    idx = 0;
                    for(int m = 0; m < 4; m++)  // loop over orders of boys function
                    {
                        //P_100 : STEP: x
                        AUX_S_1_0_0_0[idx++] = P.PA_x[i] * AUX_S_0_0_0_0[m * 1 + 0] - a_over_p * PQ_x * AUX_S_0_0_0_0[(m+1) * 1 + 0];

                        //P_010 : STEP: y
                        AUX_S_1_0_0_0[idx++] = P.PA_y[i] * AUX_S_0_0_0_0[m * 1 + 0] - a_over_p * PQ_y * AUX_S_0_0_0_0[(m+1) * 1 + 0];

                        //P_001 : STEP: z
                        AUX_S_1_0_0_0[idx++] = P.PA_z[i] * AUX_S_0_0_0_0[m * 1 + 0] - a_over_p * PQ_z * AUX_S_0_0_0_0[(m+1) * 1 + 0];

                    }


                    // Forming AUX_S_2_0_0_0[3 * 6];
                    // Needed from this AM:
                    //    D_200
                    //    D_110
                    //    D_101
                    //    D_020
                    //    D_011
                    //    D_002
                    idx = 0;
                    for(int m = 0; m < 3; m++)  // loop over orders of boys function
                    {
                        //D_200 : STEP: x
                        AUX_S_2_0_0_0[idx++] = P.PA_x[i] * AUX_S_1_0_0_0[m * 3 + 0] - a_over_p * PQ_x * AUX_S_1_0_0_0[(m+1) * 3 + 0]
                                     + 1 * one_over_2p * ( AUX_S_0_0_0_0[m * 1 +  0] - a_over_p * AUX_S_0_0_0_0[(m+1) * 1 + 0] );

                        //D_110 : STEP: y
                        AUX_S_2_0_0_0[idx++] = P.PA_y[i] * AUX_S_1_0_0_0[m * 3 + 0] - a_over_p * PQ_y * AUX_S_1_0_0_0[(m+1) * 3 + 0];

                        //D_101 : STEP: z
                        AUX_S_2_0_0_0[idx++] = P.PA_z[i] * AUX_S_1_0_0_0[m * 3 + 0] - a_over_p * PQ_z * AUX_S_1_0_0_0[(m+1) * 3 + 0];

                        //D_020 : STEP: y
                        AUX_S_2_0_0_0[idx++] = P.PA_y[i] * AUX_S_1_0_0_0[m * 3 + 1] - a_over_p * PQ_y * AUX_S_1_0_0_0[(m+1) * 3 + 1]
                                     + 1 * one_over_2p * ( AUX_S_0_0_0_0[m * 1 +  0] - a_over_p * AUX_S_0_0_0_0[(m+1) * 1 + 0] );

                        //D_011 : STEP: z
                        AUX_S_2_0_0_0[idx++] = P.PA_z[i] * AUX_S_1_0_0_0[m * 3 + 1] - a_over_p * PQ_z * AUX_S_1_0_0_0[(m+1) * 3 + 1];

                        //D_002 : STEP: z
                        AUX_S_2_0_0_0[idx++] = P.PA_z[i] * AUX_S_1_0_0_0[m * 3 + 2] - a_over_p * PQ_z * AUX_S_1_0_0_0[(m+1) * 3 + 2]
                                     + 1 * one_over_2p * ( AUX_S_0_0_0_0[m * 1 +  0] - a_over_p * AUX_S_0_0_0_0[(m+1) * 1 + 0] );

                    }


                    // Forming AUX_S_3_0_0_0[2 * 10];
                    // Needed from this AM:
                    //    F_300
                    //    F_210
                    //    F_201
                    //    F_120
                    //    F_111
                    //    F_102
                    //    F_030
                    //    F_021
                    //    F_012
                    //    F_003
                    idx = 0;
                    for(int m = 0; m < 2; m++)  // loop over orders of boys function
                    {
                        //F_300 : STEP: x
                        AUX_S_3_0_0_0[idx++] = P.PA_x[i] * AUX_S_2_0_0_0[m * 6 + 0] - a_over_p * PQ_x * AUX_S_2_0_0_0[(m+1) * 6 + 0]
                                     + 2 * one_over_2p * ( AUX_S_1_0_0_0[m * 3 +  0] - a_over_p * AUX_S_1_0_0_0[(m+1) * 3 + 0] );

                        //F_210 : STEP: y
                        AUX_S_3_0_0_0[idx++] = P.PA_y[i] * AUX_S_2_0_0_0[m * 6 + 0] - a_over_p * PQ_y * AUX_S_2_0_0_0[(m+1) * 6 + 0];

                        //F_201 : STEP: z
                        AUX_S_3_0_0_0[idx++] = P.PA_z[i] * AUX_S_2_0_0_0[m * 6 + 0] - a_over_p * PQ_z * AUX_S_2_0_0_0[(m+1) * 6 + 0];

                        //F_120 : STEP: x
                        AUX_S_3_0_0_0[idx++] = P.PA_x[i] * AUX_S_2_0_0_0[m * 6 + 3] - a_over_p * PQ_x * AUX_S_2_0_0_0[(m+1) * 6 + 3];

                        //F_111 : STEP: z
                        AUX_S_3_0_0_0[idx++] = P.PA_z[i] * AUX_S_2_0_0_0[m * 6 + 1] - a_over_p * PQ_z * AUX_S_2_0_0_0[(m+1) * 6 + 1];

                        //F_102 : STEP: x
                        AUX_S_3_0_0_0[idx++] = P.PA_x[i] * AUX_S_2_0_0_0[m * 6 + 5] - a_over_p * PQ_x * AUX_S_2_0_0_0[(m+1) * 6 + 5];

                        //F_030 : STEP: y
                        AUX_S_3_0_0_0[idx++] = P.PA_y[i] * AUX_S_2_0_0_0[m * 6 + 3] - a_over_p * PQ_y * AUX_S_2_0_0_0[(m+1) * 6 + 3]
                                     + 2 * one_over_2p * ( AUX_S_1_0_0_0[m * 3 +  1] - a_over_p * AUX_S_1_0_0_0[(m+1) * 3 + 1] );

                        //F_021 : STEP: z
                        AUX_S_3_0_0_0[idx++] = P.PA_z[i] * AUX_S_2_0_0_0[m * 6 + 3] - a_over_p * PQ_z * AUX_S_2_0_0_0[(m+1) * 6 + 3];

                        //F_012 : STEP: y
                        AUX_S_3_0_0_0[idx++] = P.PA_y[i] * AUX_S_2_0_0_0[m * 6 + 5] - a_over_p * PQ_y * AUX_S_2_0_0_0[(m+1) * 6 + 5];

                        //F_003 : STEP: z
                        AUX_S_3_0_0_0[idx++] = P.PA_z[i] * AUX_S_2_0_0_0[m * 6 + 5] - a_over_p * PQ_z * AUX_S_2_0_0_0[(m+1) * 6 + 5]
                                     + 2 * one_over_2p * ( AUX_S_1_0_0_0[m * 3 +  2] - a_over_p * AUX_S_1_0_0_0[(m+1) * 3 + 2] );

                    }


                    // Forming AUX_S_4_0_0_0[1 * 15];
                    // Needed from this AM:
                    //    G_400
                    //    G_310
                    //    G_301
                    //    G_220
                    //    G_211
                    //    G_202
                    //    G_130
                    //    G_121
                    //    G_112
                    //    G_103
                    //    G_040
                    //    G_031
                    //    G_022
                    //    G_013
                    //    G_004
                    idx = 0;
                    for(int m = 0; m < 1; m++)  // loop over orders of boys function
                    {
                        //G_400 : STEP: x
                        AUX_S_4_0_0_0[idx++] = P.PA_x[i] * AUX_S_3_0_0_0[m * 10 + 0] - a_over_p * PQ_x * AUX_S_3_0_0_0[(m+1) * 10 + 0]
                                     + 3 * one_over_2p * ( AUX_S_2_0_0_0[m * 6 +  0] - a_over_p * AUX_S_2_0_0_0[(m+1) * 6 + 0] );

                        //G_310 : STEP: y
                        AUX_S_4_0_0_0[idx++] = P.PA_y[i] * AUX_S_3_0_0_0[m * 10 + 0] - a_over_p * PQ_y * AUX_S_3_0_0_0[(m+1) * 10 + 0];

                        //G_301 : STEP: z
                        AUX_S_4_0_0_0[idx++] = P.PA_z[i] * AUX_S_3_0_0_0[m * 10 + 0] - a_over_p * PQ_z * AUX_S_3_0_0_0[(m+1) * 10 + 0];

                        //G_220 : STEP: y
                        AUX_S_4_0_0_0[idx++] = P.PA_y[i] * AUX_S_3_0_0_0[m * 10 + 1] - a_over_p * PQ_y * AUX_S_3_0_0_0[(m+1) * 10 + 1]
                                     + 1 * one_over_2p * ( AUX_S_2_0_0_0[m * 6 +  0] - a_over_p * AUX_S_2_0_0_0[(m+1) * 6 + 0] );

                        //G_211 : STEP: z
                        AUX_S_4_0_0_0[idx++] = P.PA_z[i] * AUX_S_3_0_0_0[m * 10 + 1] - a_over_p * PQ_z * AUX_S_3_0_0_0[(m+1) * 10 + 1];

                        //G_202 : STEP: z
                        AUX_S_4_0_0_0[idx++] = P.PA_z[i] * AUX_S_3_0_0_0[m * 10 + 2] - a_over_p * PQ_z * AUX_S_3_0_0_0[(m+1) * 10 + 2]
                                     + 1 * one_over_2p * ( AUX_S_2_0_0_0[m * 6 +  0] - a_over_p * AUX_S_2_0_0_0[(m+1) * 6 + 0] );

                        //G_130 : STEP: x
                        AUX_S_4_0_0_0[idx++] = P.PA_x[i] * AUX_S_3_0_0_0[m * 10 + 6] - a_over_p * PQ_x * AUX_S_3_0_0_0[(m+1) * 10 + 6];

                        //G_121 : STEP: z
                        AUX_S_4_0_0_0[idx++] = P.PA_z[i] * AUX_S_3_0_0_0[m * 10 + 3] - a_over_p * PQ_z * AUX_S_3_0_0_0[(m+1) * 10 + 3];

                        //G_112 : STEP: y
                        AUX_S_4_0_0_0[idx++] = P.PA_y[i] * AUX_S_3_0_0_0[m * 10 + 5] - a_over_p * PQ_y * AUX_S_3_0_0_0[(m+1) * 10 + 5];

                        //G_103 : STEP: x
                        AUX_S_4_0_0_0[idx++] = P.PA_x[i] * AUX_S_3_0_0_0[m * 10 + 9] - a_over_p * PQ_x * AUX_S_3_0_0_0[(m+1) * 10 + 9];

                        //G_040 : STEP: y
                        AUX_S_4_0_0_0[idx++] = P.PA_y[i] * AUX_S_3_0_0_0[m * 10 + 6] - a_over_p * PQ_y * AUX_S_3_0_0_0[(m+1) * 10 + 6]
                                     + 3 * one_over_2p * ( AUX_S_2_0_0_0[m * 6 +  3] - a_over_p * AUX_S_2_0_0_0[(m+1) * 6 + 3] );

                        //G_031 : STEP: z
                        AUX_S_4_0_0_0[idx++] = P.PA_z[i] * AUX_S_3_0_0_0[m * 10 + 6] - a_over_p * PQ_z * AUX_S_3_0_0_0[(m+1) * 10 + 6];

                        //G_022 : STEP: z
                        AUX_S_4_0_0_0[idx++] = P.PA_z[i] * AUX_S_3_0_0_0[m * 10 + 7] - a_over_p * PQ_z * AUX_S_3_0_0_0[(m+1) * 10 + 7]
                                     + 1 * one_over_2p * ( AUX_S_2_0_0_0[m * 6 +  3] - a_over_p * AUX_S_2_0_0_0[(m+1) * 6 + 3] );

                        //G_013 : STEP: y
                        AUX_S_4_0_0_0[idx++] = P.PA_y[i] * AUX_S_3_0_0_0[m * 10 + 9] - a_over_p * PQ_y * AUX_S_3_0_0_0[(m+1) * 10 + 9];

                        //G_004 : STEP: z
                        AUX_S_4_0_0_0[idx++] = P.PA_z[i] * AUX_S_3_0_0_0[m * 10 + 9] - a_over_p * PQ_z * AUX_S_3_0_0_0[(m+1) * 10 + 9]
                                     + 3 * one_over_2p * ( AUX_S_2_0_0_0[m * 6 +  5] - a_over_p * AUX_S_2_0_0_0[(m+1) * 6 + 5] );

                    }




                    //////////////////////////////////////////////
                    // Primitive integrals: Electron transfer
                    //////////////////////////////////////////////

                    // ( S_000 S_000 | P_100 S_000 )^0 = x * ( S_000 S_000 | S_000 S_000 )^0_{e} - ( P_100 S_000 | S_000 S_000 )^0_{e}
                    const double Q_0_0_1_0__0_0_0_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_1_0_0_0[0];

                    // ( S_000 S_000 | P_010 S_000 )^0 = y * ( S_000 S_000 | S_000 S_000 )^0_{e} - ( P_010 S_000 | S_000 S_000 )^0_{e}
                    const double Q_0_0_1_0__0_0_0_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_1_0_0_0[1];

                    // ( S_000 S_000 | P_001 S_000 )^0 = z * ( S_000 S_000 | S_000 S_000 )^0_{e} - ( P_001 S_000 | S_000 S_000 )^0_{e}
                    const double Q_0_0_1_0__0_0_0_0_0_0_0_0_1_0_0_0 = etfac[2] * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_1_0_0_0[2];

                    // ( P_100 S_000 | P_100 S_000 )^0 = x * ( P_100 S_000 | S_000 S_000 )^0_{e} + ( S_000 S_000 | S_000 S_000 )^0_{e} - ( D_200 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 0] += etfac[0] * AUX_S_1_0_0_0[0] + 1 * one_over_2q * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[0];

                    // ( P_100 S_000 | P_010 S_000 )^0 = y * ( P_100 S_000 | S_000 S_000 )^0_{e} - ( D_110 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 1] += etfac[1] * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[1];

                    // ( P_100 S_000 | P_001 S_000 )^0 = z * ( P_100 S_000 | S_000 S_000 )^0_{e} - ( D_101 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 2] += etfac[2] * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[2];

                    // ( P_010 S_000 | P_100 S_000 )^0 = x * ( P_010 S_000 | S_000 S_000 )^0_{e} - ( D_110 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 3] += etfac[0] * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_2_0_0_0[1];

                    // ( P_010 S_000 | P_010 S_000 )^0 = y * ( P_010 S_000 | S_000 S_000 )^0_{e} + ( S_000 S_000 | S_000 S_000 )^0_{e} - ( D_020 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 4] += etfac[1] * AUX_S_1_0_0_0[1] + 1 * one_over_2q * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[3];

                    // ( P_010 S_000 | P_001 S_000 )^0 = z * ( P_010 S_000 | S_000 S_000 )^0_{e} - ( D_011 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 5] += etfac[2] * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_2_0_0_0[4];

                    // ( P_001 S_000 | P_100 S_000 )^0 = x * ( P_001 S_000 | S_000 S_000 )^0_{e} - ( D_101 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 6] += etfac[0] * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_2_0_0_0[2];

                    // ( P_001 S_000 | P_010 S_000 )^0 = y * ( P_001 S_000 | S_000 S_000 )^0_{e} - ( D_011 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 7] += etfac[1] * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_2_0_0_0[4];

                    // ( P_001 S_000 | P_001 S_000 )^0 = z * ( P_001 S_000 | S_000 S_000 )^0_{e} + ( S_000 S_000 | S_000 S_000 )^0_{e} - ( D_002 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 8] += etfac[2] * AUX_S_1_0_0_0[2] + 1 * one_over_2q * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[5];

                    // ( D_200 S_000 | P_100 S_000 )^0 = x * ( D_200 S_000 | S_000 S_000 )^0_{e} + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( F_300 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 0] += etfac[0] * AUX_S_2_0_0_0[0] + 2 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[0];

                    // ( D_200 S_000 | P_010 S_000 )^0 = y * ( D_200 S_000 | S_000 S_000 )^0_{e} - ( F_210 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 1] += etfac[1] * AUX_S_2_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[1];

                    // ( D_200 S_000 | P_001 S_000 )^0 = z * ( D_200 S_000 | S_000 S_000 )^0_{e} - ( F_201 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 2] += etfac[2] * AUX_S_2_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[2];

                    // ( D_110 S_000 | P_100 S_000 )^0 = x * ( D_110 S_000 | S_000 S_000 )^0_{e} + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( F_210 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 3] += etfac[0] * AUX_S_2_0_0_0[1] + 1 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_3_0_0_0[1];

                    // ( D_110 S_000 | P_010 S_000 )^0 = y * ( D_110 S_000 | S_000 S_000 )^0_{e} + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( F_120 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 4] += etfac[1] * AUX_S_2_0_0_0[1] + 1 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[3];

                    // ( D_110 S_000 | P_001 S_000 )^0 = z * ( D_110 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 5] += etfac[2] * AUX_S_2_0_0_0[1] - p_over_q * AUX_S_3_0_0_0[4];

                    // ( D_101 S_000 | P_100 S_000 )^0 = x * ( D_101 S_000 | S_000 S_000 )^0_{e} + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( F_201 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 6] += etfac[0] * AUX_S_2_0_0_0[2] + 1 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_3_0_0_0[2];

                    // ( D_101 S_000 | P_010 S_000 )^0 = y * ( D_101 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 7] += etfac[1] * AUX_S_2_0_0_0[2] - p_over_q * AUX_S_3_0_0_0[4];

                    // ( D_101 S_000 | P_001 S_000 )^0 = z * ( D_101 S_000 | S_000 S_000 )^0_{e} + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( F_102 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 8] += etfac[2] * AUX_S_2_0_0_0[2] + 1 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[5];

                    // ( D_020 S_000 | P_100 S_000 )^0 = x * ( D_020 S_000 | S_000 S_000 )^0_{e} - ( F_120 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 9] += etfac[0] * AUX_S_2_0_0_0[3] - p_over_q * AUX_S_3_0_0_0[3];

                    // ( D_020 S_000 | P_010 S_000 )^0 = y * ( D_020 S_000 | S_000 S_000 )^0_{e} + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( F_030 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 10] += etfac[1] * AUX_S_2_0_0_0[3] + 2 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_3_0_0_0[6];

                    // ( D_020 S_000 | P_001 S_000 )^0 = z * ( D_020 S_000 | S_000 S_000 )^0_{e} - ( F_021 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 11] += etfac[2] * AUX_S_2_0_0_0[3] - p_over_q * AUX_S_3_0_0_0[7];

                    // ( D_011 S_000 | P_100 S_000 )^0 = x * ( D_011 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 12] += etfac[0] * AUX_S_2_0_0_0[4] - p_over_q * AUX_S_3_0_0_0[4];

                    // ( D_011 S_000 | P_010 S_000 )^0 = y * ( D_011 S_000 | S_000 S_000 )^0_{e} + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( F_021 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 13] += etfac[1] * AUX_S_2_0_0_0[4] + 1 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_3_0_0_0[7];

                    // ( D_011 S_000 | P_001 S_000 )^0 = z * ( D_011 S_000 | S_000 S_000 )^0_{e} + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( F_012 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 14] += etfac[2] * AUX_S_2_0_0_0[4] + 1 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_3_0_0_0[8];

                    // ( D_002 S_000 | P_100 S_000 )^0 = x * ( D_002 S_000 | S_000 S_000 )^0_{e} - ( F_102 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 15] += etfac[0] * AUX_S_2_0_0_0[5] - p_over_q * AUX_S_3_0_0_0[5];

                    // ( D_002 S_000 | P_010 S_000 )^0 = y * ( D_002 S_000 | S_000 S_000 )^0_{e} - ( F_012 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 16] += etfac[1] * AUX_S_2_0_0_0[5] - p_over_q * AUX_S_3_0_0_0[8];

                    // ( D_002 S_000 | P_001 S_000 )^0 = z * ( D_002 S_000 | S_000 S_000 )^0_{e} + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( F_003 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 17] += etfac[2] * AUX_S_2_0_0_0[5] + 2 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_3_0_0_0[9];

                    // ( F_300 S_000 | P_100 S_000 )^0 = x * ( F_300 S_000 | S_000 S_000 )^0_{e} + ( D_200 S_000 | S_000 S_000 )^0_{e} - ( G_400 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__3_0_0_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[0] + 3 * one_over_2q * AUX_S_2_0_0_0[0] - p_over_q * AUX_S_4_0_0_0[0];

                    // ( F_210 S_000 | P_100 S_000 )^0 = x * ( F_210 S_000 | S_000 S_000 )^0_{e} + ( D_110 S_000 | S_000 S_000 )^0_{e} - ( G_310 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__2_1_0_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[1] + 2 * one_over_2q * AUX_S_2_0_0_0[1] - p_over_q * AUX_S_4_0_0_0[1];

                    // ( F_210 S_000 | P_010 S_000 )^0 = y * ( F_210 S_000 | S_000 S_000 )^0_{e} + ( D_200 S_000 | S_000 S_000 )^0_{e} - ( G_220 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__2_1_0_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[1] + 1 * one_over_2q * AUX_S_2_0_0_0[0] - p_over_q * AUX_S_4_0_0_0[3];

                    // ( F_201 S_000 | P_100 S_000 )^0 = x * ( F_201 S_000 | S_000 S_000 )^0_{e} + ( D_101 S_000 | S_000 S_000 )^0_{e} - ( G_301 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__2_0_1_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[2] + 2 * one_over_2q * AUX_S_2_0_0_0[2] - p_over_q * AUX_S_4_0_0_0[2];

                    // ( F_201 S_000 | P_010 S_000 )^0 = y * ( F_201 S_000 | S_000 S_000 )^0_{e} - ( G_211 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__2_0_1_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[2] - p_over_q * AUX_S_4_0_0_0[4];

                    // ( F_201 S_000 | P_001 S_000 )^0 = z * ( F_201 S_000 | S_000 S_000 )^0_{e} + ( D_200 S_000 | S_000 S_000 )^0_{e} - ( G_202 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__2_0_1_0_0_0_0_0_1_0_0_0 = etfac[2] * AUX_S_3_0_0_0[2] + 1 * one_over_2q * AUX_S_2_0_0_0[0] - p_over_q * AUX_S_4_0_0_0[5];

                    // ( F_120 S_000 | P_100 S_000 )^0 = x * ( F_120 S_000 | S_000 S_000 )^0_{e} + ( D_020 S_000 | S_000 S_000 )^0_{e} - ( G_220 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__1_2_0_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[3] + 1 * one_over_2q * AUX_S_2_0_0_0[3] - p_over_q * AUX_S_4_0_0_0[3];

                    // ( F_120 S_000 | P_010 S_000 )^0 = y * ( F_120 S_000 | S_000 S_000 )^0_{e} + ( D_110 S_000 | S_000 S_000 )^0_{e} - ( G_130 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__1_2_0_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[3] + 2 * one_over_2q * AUX_S_2_0_0_0[1] - p_over_q * AUX_S_4_0_0_0[6];

                    // ( F_111 S_000 | P_100 S_000 )^0 = x * ( F_111 S_000 | S_000 S_000 )^0_{e} + ( D_011 S_000 | S_000 S_000 )^0_{e} - ( G_211 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__1_1_1_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[4] + 1 * one_over_2q * AUX_S_2_0_0_0[4] - p_over_q * AUX_S_4_0_0_0[4];

                    // ( F_111 S_000 | P_010 S_000 )^0 = y * ( F_111 S_000 | S_000 S_000 )^0_{e} + ( D_101 S_000 | S_000 S_000 )^0_{e} - ( G_121 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__1_1_1_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[4] + 1 * one_over_2q * AUX_S_2_0_0_0[2] - p_over_q * AUX_S_4_0_0_0[7];

                    // ( F_111 S_000 | P_001 S_000 )^0 = z * ( F_111 S_000 | S_000 S_000 )^0_{e} + ( D_110 S_000 | S_000 S_000 )^0_{e} - ( G_112 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__1_1_1_0_0_0_0_0_1_0_0_0 = etfac[2] * AUX_S_3_0_0_0[4] + 1 * one_over_2q * AUX_S_2_0_0_0[1] - p_over_q * AUX_S_4_0_0_0[8];

                    // ( F_102 S_000 | P_100 S_000 )^0 = x * ( F_102 S_000 | S_000 S_000 )^0_{e} + ( D_002 S_000 | S_000 S_000 )^0_{e} - ( G_202 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__1_0_2_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[5] + 1 * one_over_2q * AUX_S_2_0_0_0[5] - p_over_q * AUX_S_4_0_0_0[5];

                    // ( F_102 S_000 | P_010 S_000 )^0 = y * ( F_102 S_000 | S_000 S_000 )^0_{e} - ( G_112 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__1_0_2_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[5] - p_over_q * AUX_S_4_0_0_0[8];

                    // ( F_102 S_000 | P_001 S_000 )^0 = z * ( F_102 S_000 | S_000 S_000 )^0_{e} + ( D_101 S_000 | S_000 S_000 )^0_{e} - ( G_103 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__1_0_2_0_0_0_0_0_1_0_0_0 = etfac[2] * AUX_S_3_0_0_0[5] + 2 * one_over_2q * AUX_S_2_0_0_0[2] - p_over_q * AUX_S_4_0_0_0[9];

                    // ( F_030 S_000 | P_100 S_000 )^0 = x * ( F_030 S_000 | S_000 S_000 )^0_{e} - ( G_130 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_3_0_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[6] - p_over_q * AUX_S_4_0_0_0[6];

                    // ( F_030 S_000 | P_010 S_000 )^0 = y * ( F_030 S_000 | S_000 S_000 )^0_{e} + ( D_020 S_000 | S_000 S_000 )^0_{e} - ( G_040 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_3_0_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[6] + 3 * one_over_2q * AUX_S_2_0_0_0[3] - p_over_q * AUX_S_4_0_0_0[10];

                    // ( F_021 S_000 | P_100 S_000 )^0 = x * ( F_021 S_000 | S_000 S_000 )^0_{e} - ( G_121 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_2_1_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[7] - p_over_q * AUX_S_4_0_0_0[7];

                    // ( F_021 S_000 | P_010 S_000 )^0 = y * ( F_021 S_000 | S_000 S_000 )^0_{e} + ( D_011 S_000 | S_000 S_000 )^0_{e} - ( G_031 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_2_1_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[7] + 2 * one_over_2q * AUX_S_2_0_0_0[4] - p_over_q * AUX_S_4_0_0_0[11];

                    // ( F_021 S_000 | P_001 S_000 )^0 = z * ( F_021 S_000 | S_000 S_000 )^0_{e} + ( D_020 S_000 | S_000 S_000 )^0_{e} - ( G_022 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_2_1_0_0_0_0_0_1_0_0_0 = etfac[2] * AUX_S_3_0_0_0[7] + 1 * one_over_2q * AUX_S_2_0_0_0[3] - p_over_q * AUX_S_4_0_0_0[12];

                    // ( F_012 S_000 | P_100 S_000 )^0 = x * ( F_012 S_000 | S_000 S_000 )^0_{e} - ( G_112 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_1_2_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[8] - p_over_q * AUX_S_4_0_0_0[8];

                    // ( F_012 S_000 | P_010 S_000 )^0 = y * ( F_012 S_000 | S_000 S_000 )^0_{e} + ( D_002 S_000 | S_000 S_000 )^0_{e} - ( G_022 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_1_2_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[8] + 1 * one_over_2q * AUX_S_2_0_0_0[5] - p_over_q * AUX_S_4_0_0_0[12];

                    // ( F_012 S_000 | P_001 S_000 )^0 = z * ( F_012 S_000 | S_000 S_000 )^0_{e} + ( D_011 S_000 | S_000 S_000 )^0_{e} - ( G_013 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_1_2_0_0_0_0_0_1_0_0_0 = etfac[2] * AUX_S_3_0_0_0[8] + 2 * one_over_2q * AUX_S_2_0_0_0[4] - p_over_q * AUX_S_4_0_0_0[13];

                    // ( F_003 S_000 | P_100 S_000 )^0 = x * ( F_003 S_000 | S_000 S_000 )^0_{e} - ( G_103 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_0_3_0_0_0_1_0_0_0_0_0 = etfac[0] * AUX_S_3_0_0_0[9] - p_over_q * AUX_S_4_0_0_0[9];

                    // ( F_003 S_000 | P_010 S_000 )^0 = y * ( F_003 S_000 | S_000 S_000 )^0_{e} - ( G_013 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_0_3_0_0_0_0_1_0_0_0_0 = etfac[1] * AUX_S_3_0_0_0[9] - p_over_q * AUX_S_4_0_0_0[13];

                    // ( F_003 S_000 | P_001 S_000 )^0 = z * ( F_003 S_000 | S_000 S_000 )^0_{e} + ( D_002 S_000 | S_000 S_000 )^0_{e} - ( G_004 S_000 | S_000 S_000 )^0_{e}
                    const double Q_3_0_1_0__0_0_3_0_0_0_0_0_1_0_0_0 = etfac[2] * AUX_S_3_0_0_0[9] + 3 * one_over_2q * AUX_S_2_0_0_0[5] - p_over_q * AUX_S_4_0_0_0[14];

                    // ( P_100 S_000 | P_100 S_000 )^0_{t} = x * ( P_100 S_000 | S_000 S_000 )^0_{e} + ( S_000 S_000 | S_000 S_000 )^0_{e} - ( D_200 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 0] += etfac[0] * AUX_S_1_0_0_0[0] + 1 * one_over_2q * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[0];

                    // ( P_100 S_000 | P_010 S_000 )^0_{t} = y * ( P_100 S_000 | S_000 S_000 )^0_{e} - ( D_110 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 1] += etfac[1] * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[1];

                    // ( P_100 S_000 | P_001 S_000 )^0_{t} = z * ( P_100 S_000 | S_000 S_000 )^0_{e} - ( D_101 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 2] += etfac[2] * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[2];

                    // ( P_010 S_000 | P_100 S_000 )^0_{t} = x * ( P_010 S_000 | S_000 S_000 )^0_{e} - ( D_110 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 3] += etfac[0] * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_2_0_0_0[1];

                    // ( P_010 S_000 | P_010 S_000 )^0_{t} = y * ( P_010 S_000 | S_000 S_000 )^0_{e} + ( S_000 S_000 | S_000 S_000 )^0_{e} - ( D_020 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 4] += etfac[1] * AUX_S_1_0_0_0[1] + 1 * one_over_2q * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[3];

                    // ( P_010 S_000 | P_001 S_000 )^0_{t} = z * ( P_010 S_000 | S_000 S_000 )^0_{e} - ( D_011 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 5] += etfac[2] * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_2_0_0_0[4];

                    // ( P_001 S_000 | P_100 S_000 )^0_{t} = x * ( P_001 S_000 | S_000 S_000 )^0_{e} - ( D_101 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 6] += etfac[0] * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_2_0_0_0[2];

                    // ( P_001 S_000 | P_010 S_000 )^0_{t} = y * ( P_001 S_000 | S_000 S_000 )^0_{e} - ( D_011 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 7] += etfac[1] * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_2_0_0_0[4];

                    // ( P_001 S_000 | P_001 S_000 )^0_{t} = z * ( P_001 S_000 | S_000 S_000 )^0_{e} + ( S_000 S_000 | S_000 S_000 )^0_{e} - ( D_002 S_000 | S_000 S_000 )^0_{e}
                    S_1_0_1_0[abcd * 9 + 8] += etfac[2] * AUX_S_1_0_0_0[2] + 1 * one_over_2q * AUX_S_0_0_0_0[0] - p_over_q * AUX_S_2_0_0_0[5];

                    // ( P_100 S_000 | D_200 S_000 )^0_{t} = x * ( P_100 S_000 | P_100 S_000 )^0 + ( S_000 S_000 | P_100 S_000 )^0 + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( D_200 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 0] += etfac[0] * S_1_0_1_0[0] + 1 * one_over_2q * Q_0_0_1_0__0_0_0_0_0_0_1_0_0_0_0_0 + 1 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * S_2_0_1_0[0];

                    // ( P_100 S_000 | D_110 S_000 )^0_{t} = y * ( P_100 S_000 | P_100 S_000 )^0 - ( D_110 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 1] += etfac[1] * S_1_0_1_0[0] - p_over_q * S_2_0_1_0[3];

                    // ( P_100 S_000 | D_101 S_000 )^0_{t} = z * ( P_100 S_000 | P_100 S_000 )^0 - ( D_101 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 2] += etfac[2] * S_1_0_1_0[0] - p_over_q * S_2_0_1_0[6];

                    // ( P_100 S_000 | D_020 S_000 )^0_{t} = y * ( P_100 S_000 | P_010 S_000 )^0 + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( D_110 S_000 | P_010 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 3] += etfac[1] * S_1_0_1_0[1] + 1 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * S_2_0_1_0[4];

                    // ( P_100 S_000 | D_011 S_000 )^0_{t} = z * ( P_100 S_000 | P_010 S_000 )^0 - ( D_101 S_000 | P_010 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 4] += etfac[2] * S_1_0_1_0[1] - p_over_q * S_2_0_1_0[7];

                    // ( P_100 S_000 | D_002 S_000 )^0_{t} = z * ( P_100 S_000 | P_001 S_000 )^0 + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( D_101 S_000 | P_001 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 5] += etfac[2] * S_1_0_1_0[2] + 1 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * S_2_0_1_0[8];

                    // ( P_010 S_000 | D_200 S_000 )^0_{t} = x * ( P_010 S_000 | P_100 S_000 )^0 + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( D_110 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 6] += etfac[0] * S_1_0_1_0[3] + 1 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * S_2_0_1_0[3];

                    // ( P_010 S_000 | D_110 S_000 )^0_{t} = y * ( P_010 S_000 | P_100 S_000 )^0 + ( S_000 S_000 | P_100 S_000 )^0 - ( D_020 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 7] += etfac[1] * S_1_0_1_0[3] + 1 * one_over_2q * Q_0_0_1_0__0_0_0_0_0_0_1_0_0_0_0_0 - p_over_q * S_2_0_1_0[9];

                    // ( P_010 S_000 | D_101 S_000 )^0_{t} = z * ( P_010 S_000 | P_100 S_000 )^0 - ( D_011 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 8] += etfac[2] * S_1_0_1_0[3] - p_over_q * S_2_0_1_0[12];

                    // ( P_010 S_000 | D_020 S_000 )^0_{t} = y * ( P_010 S_000 | P_010 S_000 )^0 + ( S_000 S_000 | P_010 S_000 )^0 + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( D_020 S_000 | P_010 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 9] += etfac[1] * S_1_0_1_0[4] + 1 * one_over_2q * Q_0_0_1_0__0_0_0_0_0_0_0_1_0_0_0_0 + 1 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * S_2_0_1_0[10];

                    // ( P_010 S_000 | D_011 S_000 )^0_{t} = z * ( P_010 S_000 | P_010 S_000 )^0 - ( D_011 S_000 | P_010 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 10] += etfac[2] * S_1_0_1_0[4] - p_over_q * S_2_0_1_0[13];

                    // ( P_010 S_000 | D_002 S_000 )^0_{t} = z * ( P_010 S_000 | P_001 S_000 )^0 + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( D_011 S_000 | P_001 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 11] += etfac[2] * S_1_0_1_0[5] + 1 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * S_2_0_1_0[14];

                    // ( P_001 S_000 | D_200 S_000 )^0_{t} = x * ( P_001 S_000 | P_100 S_000 )^0 + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( D_101 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 12] += etfac[0] * S_1_0_1_0[6] + 1 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * S_2_0_1_0[6];

                    // ( P_001 S_000 | D_110 S_000 )^0_{t} = y * ( P_001 S_000 | P_100 S_000 )^0 - ( D_011 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 13] += etfac[1] * S_1_0_1_0[6] - p_over_q * S_2_0_1_0[12];

                    // ( P_001 S_000 | D_101 S_000 )^0_{t} = z * ( P_001 S_000 | P_100 S_000 )^0 + ( S_000 S_000 | P_100 S_000 )^0 - ( D_002 S_000 | P_100 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 14] += etfac[2] * S_1_0_1_0[6] + 1 * one_over_2q * Q_0_0_1_0__0_0_0_0_0_0_1_0_0_0_0_0 - p_over_q * S_2_0_1_0[15];

                    // ( P_001 S_000 | D_020 S_000 )^0_{t} = y * ( P_001 S_000 | P_010 S_000 )^0 + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( D_011 S_000 | P_010 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 15] += etfac[1] * S_1_0_1_0[7] + 1 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * S_2_0_1_0[13];

                    // ( P_001 S_000 | D_011 S_000 )^0_{t} = z * ( P_001 S_000 | P_010 S_000 )^0 + ( S_000 S_000 | P_010 S_000 )^0 - ( D_002 S_000 | P_010 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 16] += etfac[2] * S_1_0_1_0[7] + 1 * one_over_2q * Q_0_0_1_0__0_0_0_0_0_0_0_1_0_0_0_0 - p_over_q * S_2_0_1_0[16];

                    // ( P_001 S_000 | D_002 S_000 )^0_{t} = z * ( P_001 S_000 | P_001 S_000 )^0 + ( S_000 S_000 | P_001 S_000 )^0 + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( D_002 S_000 | P_001 S_000 )^0
                    S_1_0_2_0[abcd * 18 + 17] += etfac[2] * S_1_0_1_0[8] + 1 * one_over_2q * Q_0_0_1_0__0_0_0_0_0_0_0_0_1_0_0_0 + 1 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * S_2_0_1_0[17];

                    // ( D_200 S_000 | P_100 S_000 )^0_{t} = x * ( D_200 S_000 | S_000 S_000 )^0_{e} + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( F_300 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 0] += etfac[0] * AUX_S_2_0_0_0[0] + 2 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[0];

                    // ( D_200 S_000 | P_010 S_000 )^0_{t} = y * ( D_200 S_000 | S_000 S_000 )^0_{e} - ( F_210 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 1] += etfac[1] * AUX_S_2_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[1];

                    // ( D_200 S_000 | P_001 S_000 )^0_{t} = z * ( D_200 S_000 | S_000 S_000 )^0_{e} - ( F_201 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 2] += etfac[2] * AUX_S_2_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[2];

                    // ( D_110 S_000 | P_100 S_000 )^0_{t} = x * ( D_110 S_000 | S_000 S_000 )^0_{e} + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( F_210 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 3] += etfac[0] * AUX_S_2_0_0_0[1] + 1 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_3_0_0_0[1];

                    // ( D_110 S_000 | P_010 S_000 )^0_{t} = y * ( D_110 S_000 | S_000 S_000 )^0_{e} + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( F_120 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 4] += etfac[1] * AUX_S_2_0_0_0[1] + 1 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[3];

                    // ( D_110 S_000 | P_001 S_000 )^0_{t} = z * ( D_110 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 5] += etfac[2] * AUX_S_2_0_0_0[1] - p_over_q * AUX_S_3_0_0_0[4];

                    // ( D_101 S_000 | P_100 S_000 )^0_{t} = x * ( D_101 S_000 | S_000 S_000 )^0_{e} + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( F_201 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 6] += etfac[0] * AUX_S_2_0_0_0[2] + 1 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_3_0_0_0[2];

                    // ( D_101 S_000 | P_010 S_000 )^0_{t} = y * ( D_101 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 7] += etfac[1] * AUX_S_2_0_0_0[2] - p_over_q * AUX_S_3_0_0_0[4];

                    // ( D_101 S_000 | P_001 S_000 )^0_{t} = z * ( D_101 S_000 | S_000 S_000 )^0_{e} + ( P_100 S_000 | S_000 S_000 )^0_{e} - ( F_102 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 8] += etfac[2] * AUX_S_2_0_0_0[2] + 1 * one_over_2q * AUX_S_1_0_0_0[0] - p_over_q * AUX_S_3_0_0_0[5];

                    // ( D_020 S_000 | P_100 S_000 )^0_{t} = x * ( D_020 S_000 | S_000 S_000 )^0_{e} - ( F_120 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 9] += etfac[0] * AUX_S_2_0_0_0[3] - p_over_q * AUX_S_3_0_0_0[3];

                    // ( D_020 S_000 | P_010 S_000 )^0_{t} = y * ( D_020 S_000 | S_000 S_000 )^0_{e} + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( F_030 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 10] += etfac[1] * AUX_S_2_0_0_0[3] + 2 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_3_0_0_0[6];

                    // ( D_020 S_000 | P_001 S_000 )^0_{t} = z * ( D_020 S_000 | S_000 S_000 )^0_{e} - ( F_021 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 11] += etfac[2] * AUX_S_2_0_0_0[3] - p_over_q * AUX_S_3_0_0_0[7];

                    // ( D_011 S_000 | P_100 S_000 )^0_{t} = x * ( D_011 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 12] += etfac[0] * AUX_S_2_0_0_0[4] - p_over_q * AUX_S_3_0_0_0[4];

                    // ( D_011 S_000 | P_010 S_000 )^0_{t} = y * ( D_011 S_000 | S_000 S_000 )^0_{e} + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( F_021 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 13] += etfac[1] * AUX_S_2_0_0_0[4] + 1 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_3_0_0_0[7];

                    // ( D_011 S_000 | P_001 S_000 )^0_{t} = z * ( D_011 S_000 | S_000 S_000 )^0_{e} + ( P_010 S_000 | S_000 S_000 )^0_{e} - ( F_012 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 14] += etfac[2] * AUX_S_2_0_0_0[4] + 1 * one_over_2q * AUX_S_1_0_0_0[1] - p_over_q * AUX_S_3_0_0_0[8];

                    // ( D_002 S_000 | P_100 S_000 )^0_{t} = x * ( D_002 S_000 | S_000 S_000 )^0_{e} - ( F_102 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 15] += etfac[0] * AUX_S_2_0_0_0[5] - p_over_q * AUX_S_3_0_0_0[5];

                    // ( D_002 S_000 | P_010 S_000 )^0_{t} = y * ( D_002 S_000 | S_000 S_000 )^0_{e} - ( F_012 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 16] += etfac[1] * AUX_S_2_0_0_0[5] - p_over_q * AUX_S_3_0_0_0[8];

                    // ( D_002 S_000 | P_001 S_000 )^0_{t} = z * ( D_002 S_000 | S_000 S_000 )^0_{e} + ( P_001 S_000 | S_000 S_000 )^0_{e} - ( F_003 S_000 | S_000 S_000 )^0_{e}
                    S_2_0_1_0[abcd * 18 + 17] += etfac[2] * AUX_S_2_0_0_0[5] + 2 * one_over_2q * AUX_S_1_0_0_0[2] - p_over_q * AUX_S_3_0_0_0[9];

                    // ( D_200 S_000 | D_200 S_000 )^0_{t} = x * ( D_200 S_000 | P_100 S_000 )^0 + ( P_100 S_000 | P_100 S_000 )^0 + ( D_200 S_000 | S_000 S_000 )^0_{e} - ( F_300 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 0] += etfac[0] * S_2_0_1_0[0] + 2 * one_over_2q * S_1_0_1_0[0] + 1 * one_over_2q * AUX_S_2_0_0_0[0] - p_over_q * Q_3_0_1_0__3_0_0_0_0_0_1_0_0_0_0_0;

                    // ( D_200 S_000 | D_110 S_000 )^0_{t} = y * ( D_200 S_000 | P_100 S_000 )^0 - ( F_210 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 1] += etfac[1] * S_2_0_1_0[0] - p_over_q * Q_3_0_1_0__2_1_0_0_0_0_1_0_0_0_0_0;

                    // ( D_200 S_000 | D_101 S_000 )^0_{t} = z * ( D_200 S_000 | P_100 S_000 )^0 - ( F_201 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 2] += etfac[2] * S_2_0_1_0[0] - p_over_q * Q_3_0_1_0__2_0_1_0_0_0_1_0_0_0_0_0;

                    // ( D_200 S_000 | D_020 S_000 )^0_{t} = y * ( D_200 S_000 | P_010 S_000 )^0 + ( D_200 S_000 | S_000 S_000 )^0_{e} - ( F_210 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 3] += etfac[1] * S_2_0_1_0[1] + 1 * one_over_2q * AUX_S_2_0_0_0[0] - p_over_q * Q_3_0_1_0__2_1_0_0_0_0_0_1_0_0_0_0;

                    // ( D_200 S_000 | D_011 S_000 )^0_{t} = z * ( D_200 S_000 | P_010 S_000 )^0 - ( F_201 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 4] += etfac[2] * S_2_0_1_0[1] - p_over_q * Q_3_0_1_0__2_0_1_0_0_0_0_1_0_0_0_0;

                    // ( D_200 S_000 | D_002 S_000 )^0_{t} = z * ( D_200 S_000 | P_001 S_000 )^0 + ( D_200 S_000 | S_000 S_000 )^0_{e} - ( F_201 S_000 | P_001 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 5] += etfac[2] * S_2_0_1_0[2] + 1 * one_over_2q * AUX_S_2_0_0_0[0] - p_over_q * Q_3_0_1_0__2_0_1_0_0_0_0_0_1_0_0_0;

                    // ( D_110 S_000 | D_200 S_000 )^0_{t} = x * ( D_110 S_000 | P_100 S_000 )^0 + ( P_010 S_000 | P_100 S_000 )^0 + ( D_110 S_000 | S_000 S_000 )^0_{e} - ( F_210 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 6] += etfac[0] * S_2_0_1_0[3] + 1 * one_over_2q * S_1_0_1_0[3] + 1 * one_over_2q * AUX_S_2_0_0_0[1] - p_over_q * Q_3_0_1_0__2_1_0_0_0_0_1_0_0_0_0_0;

                    // ( D_110 S_000 | D_110 S_000 )^0_{t} = y * ( D_110 S_000 | P_100 S_000 )^0 + ( P_100 S_000 | P_100 S_000 )^0 - ( F_120 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 7] += etfac[1] * S_2_0_1_0[3] + 1 * one_over_2q * S_1_0_1_0[0] - p_over_q * Q_3_0_1_0__1_2_0_0_0_0_1_0_0_0_0_0;

                    // ( D_110 S_000 | D_101 S_000 )^0_{t} = z * ( D_110 S_000 | P_100 S_000 )^0 - ( F_111 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 8] += etfac[2] * S_2_0_1_0[3] - p_over_q * Q_3_0_1_0__1_1_1_0_0_0_1_0_0_0_0_0;

                    // ( D_110 S_000 | D_020 S_000 )^0_{t} = y * ( D_110 S_000 | P_010 S_000 )^0 + ( P_100 S_000 | P_010 S_000 )^0 + ( D_110 S_000 | S_000 S_000 )^0_{e} - ( F_120 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 9] += etfac[1] * S_2_0_1_0[4] + 1 * one_over_2q * S_1_0_1_0[1] + 1 * one_over_2q * AUX_S_2_0_0_0[1] - p_over_q * Q_3_0_1_0__1_2_0_0_0_0_0_1_0_0_0_0;

                    // ( D_110 S_000 | D_011 S_000 )^0_{t} = z * ( D_110 S_000 | P_010 S_000 )^0 - ( F_111 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 10] += etfac[2] * S_2_0_1_0[4] - p_over_q * Q_3_0_1_0__1_1_1_0_0_0_0_1_0_0_0_0;

                    // ( D_110 S_000 | D_002 S_000 )^0_{t} = z * ( D_110 S_000 | P_001 S_000 )^0 + ( D_110 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | P_001 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 11] += etfac[2] * S_2_0_1_0[5] + 1 * one_over_2q * AUX_S_2_0_0_0[1] - p_over_q * Q_3_0_1_0__1_1_1_0_0_0_0_0_1_0_0_0;

                    // ( D_101 S_000 | D_200 S_000 )^0_{t} = x * ( D_101 S_000 | P_100 S_000 )^0 + ( P_001 S_000 | P_100 S_000 )^0 + ( D_101 S_000 | S_000 S_000 )^0_{e} - ( F_201 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 12] += etfac[0] * S_2_0_1_0[6] + 1 * one_over_2q * S_1_0_1_0[6] + 1 * one_over_2q * AUX_S_2_0_0_0[2] - p_over_q * Q_3_0_1_0__2_0_1_0_0_0_1_0_0_0_0_0;

                    // ( D_101 S_000 | D_110 S_000 )^0_{t} = y * ( D_101 S_000 | P_100 S_000 )^0 - ( F_111 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 13] += etfac[1] * S_2_0_1_0[6] - p_over_q * Q_3_0_1_0__1_1_1_0_0_0_1_0_0_0_0_0;

                    // ( D_101 S_000 | D_101 S_000 )^0_{t} = z * ( D_101 S_000 | P_100 S_000 )^0 + ( P_100 S_000 | P_100 S_000 )^0 - ( F_102 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 14] += etfac[2] * S_2_0_1_0[6] + 1 * one_over_2q * S_1_0_1_0[0] - p_over_q * Q_3_0_1_0__1_0_2_0_0_0_1_0_0_0_0_0;

                    // ( D_101 S_000 | D_020 S_000 )^0_{t} = y * ( D_101 S_000 | P_010 S_000 )^0 + ( D_101 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 15] += etfac[1] * S_2_0_1_0[7] + 1 * one_over_2q * AUX_S_2_0_0_0[2] - p_over_q * Q_3_0_1_0__1_1_1_0_0_0_0_1_0_0_0_0;

                    // ( D_101 S_000 | D_011 S_000 )^0_{t} = z * ( D_101 S_000 | P_010 S_000 )^0 + ( P_100 S_000 | P_010 S_000 )^0 - ( F_102 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 16] += etfac[2] * S_2_0_1_0[7] + 1 * one_over_2q * S_1_0_1_0[1] - p_over_q * Q_3_0_1_0__1_0_2_0_0_0_0_1_0_0_0_0;

                    // ( D_101 S_000 | D_002 S_000 )^0_{t} = z * ( D_101 S_000 | P_001 S_000 )^0 + ( P_100 S_000 | P_001 S_000 )^0 + ( D_101 S_000 | S_000 S_000 )^0_{e} - ( F_102 S_000 | P_001 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 17] += etfac[2] * S_2_0_1_0[8] + 1 * one_over_2q * S_1_0_1_0[2] + 1 * one_over_2q * AUX_S_2_0_0_0[2] - p_over_q * Q_3_0_1_0__1_0_2_0_0_0_0_0_1_0_0_0;

                    // ( D_020 S_000 | D_200 S_000 )^0_{t} = x * ( D_020 S_000 | P_100 S_000 )^0 + ( D_020 S_000 | S_000 S_000 )^0_{e} - ( F_120 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 18] += etfac[0] * S_2_0_1_0[9] + 1 * one_over_2q * AUX_S_2_0_0_0[3] - p_over_q * Q_3_0_1_0__1_2_0_0_0_0_1_0_0_0_0_0;

                    // ( D_020 S_000 | D_110 S_000 )^0_{t} = y * ( D_020 S_000 | P_100 S_000 )^0 + ( P_010 S_000 | P_100 S_000 )^0 - ( F_030 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 19] += etfac[1] * S_2_0_1_0[9] + 2 * one_over_2q * S_1_0_1_0[3] - p_over_q * Q_3_0_1_0__0_3_0_0_0_0_1_0_0_0_0_0;

                    // ( D_020 S_000 | D_101 S_000 )^0_{t} = z * ( D_020 S_000 | P_100 S_000 )^0 - ( F_021 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 20] += etfac[2] * S_2_0_1_0[9] - p_over_q * Q_3_0_1_0__0_2_1_0_0_0_1_0_0_0_0_0;

                    // ( D_020 S_000 | D_020 S_000 )^0_{t} = y * ( D_020 S_000 | P_010 S_000 )^0 + ( P_010 S_000 | P_010 S_000 )^0 + ( D_020 S_000 | S_000 S_000 )^0_{e} - ( F_030 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 21] += etfac[1] * S_2_0_1_0[10] + 2 * one_over_2q * S_1_0_1_0[4] + 1 * one_over_2q * AUX_S_2_0_0_0[3] - p_over_q * Q_3_0_1_0__0_3_0_0_0_0_0_1_0_0_0_0;

                    // ( D_020 S_000 | D_011 S_000 )^0_{t} = z * ( D_020 S_000 | P_010 S_000 )^0 - ( F_021 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 22] += etfac[2] * S_2_0_1_0[10] - p_over_q * Q_3_0_1_0__0_2_1_0_0_0_0_1_0_0_0_0;

                    // ( D_020 S_000 | D_002 S_000 )^0_{t} = z * ( D_020 S_000 | P_001 S_000 )^0 + ( D_020 S_000 | S_000 S_000 )^0_{e} - ( F_021 S_000 | P_001 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 23] += etfac[2] * S_2_0_1_0[11] + 1 * one_over_2q * AUX_S_2_0_0_0[3] - p_over_q * Q_3_0_1_0__0_2_1_0_0_0_0_0_1_0_0_0;

                    // ( D_011 S_000 | D_200 S_000 )^0_{t} = x * ( D_011 S_000 | P_100 S_000 )^0 + ( D_011 S_000 | S_000 S_000 )^0_{e} - ( F_111 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 24] += etfac[0] * S_2_0_1_0[12] + 1 * one_over_2q * AUX_S_2_0_0_0[4] - p_over_q * Q_3_0_1_0__1_1_1_0_0_0_1_0_0_0_0_0;

                    // ( D_011 S_000 | D_110 S_000 )^0_{t} = y * ( D_011 S_000 | P_100 S_000 )^0 + ( P_001 S_000 | P_100 S_000 )^0 - ( F_021 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 25] += etfac[1] * S_2_0_1_0[12] + 1 * one_over_2q * S_1_0_1_0[6] - p_over_q * Q_3_0_1_0__0_2_1_0_0_0_1_0_0_0_0_0;

                    // ( D_011 S_000 | D_101 S_000 )^0_{t} = z * ( D_011 S_000 | P_100 S_000 )^0 + ( P_010 S_000 | P_100 S_000 )^0 - ( F_012 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 26] += etfac[2] * S_2_0_1_0[12] + 1 * one_over_2q * S_1_0_1_0[3] - p_over_q * Q_3_0_1_0__0_1_2_0_0_0_1_0_0_0_0_0;

                    // ( D_011 S_000 | D_020 S_000 )^0_{t} = y * ( D_011 S_000 | P_010 S_000 )^0 + ( P_001 S_000 | P_010 S_000 )^0 + ( D_011 S_000 | S_000 S_000 )^0_{e} - ( F_021 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 27] += etfac[1] * S_2_0_1_0[13] + 1 * one_over_2q * S_1_0_1_0[7] + 1 * one_over_2q * AUX_S_2_0_0_0[4] - p_over_q * Q_3_0_1_0__0_2_1_0_0_0_0_1_0_0_0_0;

                    // ( D_011 S_000 | D_011 S_000 )^0_{t} = z * ( D_011 S_000 | P_010 S_000 )^0 + ( P_010 S_000 | P_010 S_000 )^0 - ( F_012 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 28] += etfac[2] * S_2_0_1_0[13] + 1 * one_over_2q * S_1_0_1_0[4] - p_over_q * Q_3_0_1_0__0_1_2_0_0_0_0_1_0_0_0_0;

                    // ( D_011 S_000 | D_002 S_000 )^0_{t} = z * ( D_011 S_000 | P_001 S_000 )^0 + ( P_010 S_000 | P_001 S_000 )^0 + ( D_011 S_000 | S_000 S_000 )^0_{e} - ( F_012 S_000 | P_001 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 29] += etfac[2] * S_2_0_1_0[14] + 1 * one_over_2q * S_1_0_1_0[5] + 1 * one_over_2q * AUX_S_2_0_0_0[4] - p_over_q * Q_3_0_1_0__0_1_2_0_0_0_0_0_1_0_0_0;

                    // ( D_002 S_000 | D_200 S_000 )^0_{t} = x * ( D_002 S_000 | P_100 S_000 )^0 + ( D_002 S_000 | S_000 S_000 )^0_{e} - ( F_102 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 30] += etfac[0] * S_2_0_1_0[15] + 1 * one_over_2q * AUX_S_2_0_0_0[5] - p_over_q * Q_3_0_1_0__1_0_2_0_0_0_1_0_0_0_0_0;

                    // ( D_002 S_000 | D_110 S_000 )^0_{t} = y * ( D_002 S_000 | P_100 S_000 )^0 - ( F_012 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 31] += etfac[1] * S_2_0_1_0[15] - p_over_q * Q_3_0_1_0__0_1_2_0_0_0_1_0_0_0_0_0;

                    // ( D_002 S_000 | D_101 S_000 )^0_{t} = z * ( D_002 S_000 | P_100 S_000 )^0 + ( P_001 S_000 | P_100 S_000 )^0 - ( F_003 S_000 | P_100 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 32] += etfac[2] * S_2_0_1_0[15] + 2 * one_over_2q * S_1_0_1_0[6] - p_over_q * Q_3_0_1_0__0_0_3_0_0_0_1_0_0_0_0_0;

                    // ( D_002 S_000 | D_020 S_000 )^0_{t} = y * ( D_002 S_000 | P_010 S_000 )^0 + ( D_002 S_000 | S_000 S_000 )^0_{e} - ( F_012 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 33] += etfac[1] * S_2_0_1_0[16] + 1 * one_over_2q * AUX_S_2_0_0_0[5] - p_over_q * Q_3_0_1_0__0_1_2_0_0_0_0_1_0_0_0_0;

                    // ( D_002 S_000 | D_011 S_000 )^0_{t} = z * ( D_002 S_000 | P_010 S_000 )^0 + ( P_001 S_000 | P_010 S_000 )^0 - ( F_003 S_000 | P_010 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 34] += etfac[2] * S_2_0_1_0[16] + 2 * one_over_2q * S_1_0_1_0[7] - p_over_q * Q_3_0_1_0__0_0_3_0_0_0_0_1_0_0_0_0;

                    // ( D_002 S_000 | D_002 S_000 )^0_{t} = z * ( D_002 S_000 | P_001 S_000 )^0 + ( P_001 S_000 | P_001 S_000 )^0 + ( D_002 S_000 | S_000 S_000 )^0_{e} - ( F_003 S_000 | P_001 S_000 )^0
                    S_2_0_2_0[abcd * 36 + 35] += etfac[2] * S_2_0_1_0[17] + 2 * one_over_2q * S_1_0_1_0[8] + 1 * one_over_2q * AUX_S_2_0_0_0[5] - p_over_q * Q_3_0_1_0__0_0_3_0_0_0_0_0_1_0_0_0;


                 }
            }
        }
    }


    //////////////////////////////////////////////
    // Contracted integrals: Horizontal recurrance
    // Bra part
    // Steps: 9
    //////////////////////////////////////////////

    for(abcd = 0; abcd < nshell1234; ++abcd)
    {
        // form S_1_1_1_0
        for(int ni = 0; ni < 3; ++ni)
        {
            // (P_100 P_100|_{i} = (D_200 S_000|_{t} + x_ab * (P_100 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 0 * 3 + ni] = S_2_0_1_0[abcd * 18 + 0 * 3 + ni] + ( AB_x[abcd] * S_1_0_1_0[abcd * 9 + 0 * 3 + ni] );

            // (P_100 P_010|_{i} = (D_110 S_000|_{t} + y_ab * (P_100 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 1 * 3 + ni] = S_2_0_1_0[abcd * 18 + 1 * 3 + ni] + ( AB_y[abcd] * S_1_0_1_0[abcd * 9 + 0 * 3 + ni] );

            // (P_100 P_001|_{i} = (D_101 S_000|_{t} + z_ab * (P_100 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 2 * 3 + ni] = S_2_0_1_0[abcd * 18 + 2 * 3 + ni] + ( AB_z[abcd] * S_1_0_1_0[abcd * 9 + 0 * 3 + ni] );

            // (P_010 P_100|_{i} = (D_110 S_000|_{t} + x_ab * (P_010 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 3 * 3 + ni] = S_2_0_1_0[abcd * 18 + 1 * 3 + ni] + ( AB_x[abcd] * S_1_0_1_0[abcd * 9 + 1 * 3 + ni] );

            // (P_010 P_010|_{i} = (D_020 S_000|_{t} + y_ab * (P_010 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 4 * 3 + ni] = S_2_0_1_0[abcd * 18 + 3 * 3 + ni] + ( AB_y[abcd] * S_1_0_1_0[abcd * 9 + 1 * 3 + ni] );

            // (P_010 P_001|_{i} = (D_011 S_000|_{t} + z_ab * (P_010 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 5 * 3 + ni] = S_2_0_1_0[abcd * 18 + 4 * 3 + ni] + ( AB_z[abcd] * S_1_0_1_0[abcd * 9 + 1 * 3 + ni] );

            // (P_001 P_100|_{i} = (D_101 S_000|_{t} + x_ab * (P_001 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 6 * 3 + ni] = S_2_0_1_0[abcd * 18 + 2 * 3 + ni] + ( AB_x[abcd] * S_1_0_1_0[abcd * 9 + 2 * 3 + ni] );

            // (P_001 P_010|_{i} = (D_011 S_000|_{t} + y_ab * (P_001 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 7 * 3 + ni] = S_2_0_1_0[abcd * 18 + 4 * 3 + ni] + ( AB_y[abcd] * S_1_0_1_0[abcd * 9 + 2 * 3 + ni] );

            // (P_001 P_001|_{i} = (D_002 S_000|_{t} + z_ab * (P_001 S_000|_{t}
            S_1_1_1_0[abcd * 27 + 8 * 3 + ni] = S_2_0_1_0[abcd * 18 + 5 * 3 + ni] + ( AB_z[abcd] * S_1_0_1_0[abcd * 9 + 2 * 3 + ni] );

        }

        // form S_1_1_2_0
        for(int ni = 0; ni < 6; ++ni)
        {
            // (P_100 P_100|_{i} = (D_200 S_000|_{t} + x_ab * (P_100 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 0 * 6 + ni] = S_2_0_2_0[abcd * 36 + 0 * 6 + ni] + ( AB_x[abcd] * S_1_0_2_0[abcd * 18 + 0 * 6 + ni] );

            // (P_100 P_010|_{i} = (D_110 S_000|_{t} + y_ab * (P_100 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 1 * 6 + ni] = S_2_0_2_0[abcd * 36 + 1 * 6 + ni] + ( AB_y[abcd] * S_1_0_2_0[abcd * 18 + 0 * 6 + ni] );

            // (P_100 P_001|_{i} = (D_101 S_000|_{t} + z_ab * (P_100 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 2 * 6 + ni] = S_2_0_2_0[abcd * 36 + 2 * 6 + ni] + ( AB_z[abcd] * S_1_0_2_0[abcd * 18 + 0 * 6 + ni] );

            // (P_010 P_100|_{i} = (D_110 S_000|_{t} + x_ab * (P_010 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 3 * 6 + ni] = S_2_0_2_0[abcd * 36 + 1 * 6 + ni] + ( AB_x[abcd] * S_1_0_2_0[abcd * 18 + 1 * 6 + ni] );

            // (P_010 P_010|_{i} = (D_020 S_000|_{t} + y_ab * (P_010 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 4 * 6 + ni] = S_2_0_2_0[abcd * 36 + 3 * 6 + ni] + ( AB_y[abcd] * S_1_0_2_0[abcd * 18 + 1 * 6 + ni] );

            // (P_010 P_001|_{i} = (D_011 S_000|_{t} + z_ab * (P_010 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 5 * 6 + ni] = S_2_0_2_0[abcd * 36 + 4 * 6 + ni] + ( AB_z[abcd] * S_1_0_2_0[abcd * 18 + 1 * 6 + ni] );

            // (P_001 P_100|_{i} = (D_101 S_000|_{t} + x_ab * (P_001 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 6 * 6 + ni] = S_2_0_2_0[abcd * 36 + 2 * 6 + ni] + ( AB_x[abcd] * S_1_0_2_0[abcd * 18 + 2 * 6 + ni] );

            // (P_001 P_010|_{i} = (D_011 S_000|_{t} + y_ab * (P_001 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 7 * 6 + ni] = S_2_0_2_0[abcd * 36 + 4 * 6 + ni] + ( AB_y[abcd] * S_1_0_2_0[abcd * 18 + 2 * 6 + ni] );

            // (P_001 P_001|_{i} = (D_002 S_000|_{t} + z_ab * (P_001 S_000|_{t}
            S_1_1_2_0[abcd * 54 + 8 * 6 + ni] = S_2_0_2_0[abcd * 36 + 5 * 6 + ni] + ( AB_z[abcd] * S_1_0_2_0[abcd * 18 + 2 * 6 + ni] );

        }


    }


    //////////////////////////////////////////////
    // Contracted integrals: Horizontal recurrance
    // Ket part
    // Steps: 9
    // Forming final integrals
    //////////////////////////////////////////////

    for(abcd = 0; abcd < nshell1234; ++abcd)
    {
        for(int abi = 0; abi < 9; ++abi)
        {
            // |P_100 P_100)_{i} = |D_200 S_000)_{t} + x_cd * |P_100 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 0] = S_1_1_2_0[abcd * 54 + abi * 9 + 0] + ( CD_x[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 0] );

            // |P_100 P_010)_{i} = |D_110 S_000)_{t} + y_cd * |P_100 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 1] = S_1_1_2_0[abcd * 54 + abi * 9 + 1] + ( CD_y[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 0] );

            // |P_100 P_001)_{i} = |D_101 S_000)_{t} + z_cd * |P_100 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 2] = S_1_1_2_0[abcd * 54 + abi * 9 + 2] + ( CD_z[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 0] );

            // |P_010 P_100)_{i} = |D_110 S_000)_{t} + x_cd * |P_010 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 3] = S_1_1_2_0[abcd * 54 + abi * 9 + 1] + ( CD_x[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 1] );

            // |P_010 P_010)_{i} = |D_020 S_000)_{t} + y_cd * |P_010 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 4] = S_1_1_2_0[abcd * 54 + abi * 9 + 3] + ( CD_y[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 1] );

            // |P_010 P_001)_{i} = |D_011 S_000)_{t} + z_cd * |P_010 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 5] = S_1_1_2_0[abcd * 54 + abi * 9 + 4] + ( CD_z[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 1] );

            // |P_001 P_100)_{i} = |D_101 S_000)_{t} + x_cd * |P_001 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 6] = S_1_1_2_0[abcd * 54 + abi * 9 + 2] + ( CD_x[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 2] );

            // |P_001 P_010)_{i} = |D_011 S_000)_{t} + y_cd * |P_001 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 7] = S_1_1_2_0[abcd * 54 + abi * 9 + 4] + ( CD_y[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 2] );

            // |P_001 P_001)_{i} = |D_002 S_000)_{t} + z_cd * |P_001 S_000)_{t}
            S_1_1_1_1[abcd * 81 + abi * 9 + 8] = S_1_1_2_0[abcd * 54 + abi * 9 + 5] + ( CD_z[abcd] * S_1_1_1_0[abcd * 27 + abi * 9 + 2] );

        }
    }


    return nshell1234;
}

