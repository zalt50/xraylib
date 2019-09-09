/*
Copyright (c) 2009-2018, Tom Schoonjans
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * The names of the contributors may not be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY Tom Schoonjans ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Tom Schoonjans BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "config.h"
#include <math.h>
#include <stddef.h>
#include "splint.h"
#include "xrayglob.h"
#include "xraylib.h"
#include "xraylib-error-private.h"
#include "xrf_cross_sections_aux.h"

static int LB_LINE_MACROS[] = {
  L2M4_LINE,
  L2M3_LINE,
  L3N5_LINE,
  L3O4_LINE,
  L3O5_LINE,
  L3O45_LINE,
  L3N1_LINE,
  L3O1_LINE,
  L3N6_LINE,
  L3N7_LINE,
  L3N4_LINE,
  L1M3_LINE,
  L1M2_LINE,
  L1M5_LINE,
  L1M4_LINE
};

/*/////////////////////////////////////////////////////////
//                                                       //
//        Photoelectric cross section  (barns/atom)      //
//                  Using the Kissel data                //
//                                                       //
//    Z : atomic number                                  //
//    E : energy (keV)                                   //
//                                                       //
//////////////////////////////////////////////////////// */
double CSb_Photo_Total(int Z, double E, xrl_error **error) {
  int shell;
  double rv = 0.0;

  if (Z < 1 || Z > ZMAX || NE_Photo_Total_Kissel[Z] < 0) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, Z_OUT_OF_RANGE);
    return 0.0;
  }
  if (E <= 0.0) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, NEGATIVE_ENERGY);
    return 0.0;
  }

  for (shell = K_SHELL ; shell <= Q3_SHELL ; shell++) {
    if (Electron_Config_Kissel[Z][shell] > 1.0E-06) {
  	rv += CSb_Photo_Partial(Z, shell, E, NULL) * Electron_Config_Kissel[Z][shell];
    }
  }
  if (rv == 0.0) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, UNAVAILABLE_PHOTO_CS);
    return 0.0;
  }

  return rv;
}

/*/////////////////////////////////////////////////////////
//                                                       //
//        Photoelectric cross section  (cm2/g)           //
//                  Using the Kissel data                //
//                                                       //
//    Z : atomic number                                  //
//    E : energy (keV)                                   //
//                                                       //
//////////////////////////////////////////////////////// */

double CS_Photo_Total(int Z, double E, xrl_error **error) {
  double cs = CSb_Photo_Total(Z, E, error);
  if (cs == 0.0)
    return 0.0;
  return cs * AVOGNUM / AtomicWeight_arr[Z];
}


/*/////////////////////////////////////////////////////////
//                                                       //
//   Partial Photoelectric cross section  (barns/elec)   //
//                  Using the Kissel data                //
//                                                       //
//    Z : atomic number                                  //
//    shell : shell                                      //
//    E : energy (keV)                                   //
//                                                       //
//////////////////////////////////////////////////////// */

double CSb_Photo_Partial(int Z, int shell, double E, xrl_error **error) {
  double ln_E, ln_sigma, sigma;
  double x0, x1, y0, y1;
  double m;

  if (Z < 1 || Z > ZMAX) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, Z_OUT_OF_RANGE);
    return 0.0;
  }

  if (shell < 0 || shell >= SHELLNUM_K) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, UNKNOWN_SHELL);
    return 0.0;
  }

  if (E <= 0.0) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, NEGATIVE_ENERGY);
    return 0.0;
  }

  if (Electron_Config_Kissel[Z][shell] < 1.0E-06 || EdgeEnergy_arr[Z][shell] <= 0.0){
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, INVALID_SHELL);
    return 0.0;
  } 
  
  if (EdgeEnergy_arr[Z][shell] > E) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, TOO_LOW_EXCITATION_ENERGY);
    return 0.0;
  } 

  ln_E = log(E);
  if (EdgeEnergy_Kissel[Z][shell] > EdgeEnergy_arr[Z][shell] && E < EdgeEnergy_Kissel[Z][shell]) {
    /*
     * use log-log extrapolation 
     */
    x0 = E_Photo_Partial_Kissel[Z][shell][0];
    x1 = E_Photo_Partial_Kissel[Z][shell][1];
    y0 = Photo_Partial_Kissel[Z][shell][0];
    y1 = Photo_Partial_Kissel[Z][shell][1];
    /*
     * do not allow "extreme" slopes... force them to be within -1;1
     */
    m = (y1 - y0) / (x1 - x0);
    if (m > 1.0)
      m = 1.0;
    else if (m < -1.0)
      m = -1.0;
    ln_sigma = y0 + m * (ln_E - x0);
  }
  else {
    int splint_rv = splint(E_Photo_Partial_Kissel[Z][shell] - 1, Photo_Partial_Kissel[Z][shell] - 1, Photo_Partial_Kissel2[Z][shell] - 1, NE_Photo_Partial_Kissel[Z][shell], ln_E, &ln_sigma, error);
    if (!splint_rv)
      return 0;
  }
  sigma = exp(ln_sigma);

  return sigma; 
}

/*/////////////////////////////////////////////////////////
//                                                       //
//   Partial Photoelectric cross section  (cm2/g)        //
//                  Using the Kissel data                //
//                                                       //
//    Z : atomic number                                  //
//    shell : shell                                      //
//    E : energy (keV)                                   //
//                                                       //
//////////////////////////////////////////////////////// */


double CS_Photo_Partial(int Z, int shell, double E, xrl_error **error) {
  double cs = CSb_Photo_Partial(Z, shell, E, error);
  if (cs == 0.0)
    return 0.0;
  return cs * Electron_Config_Kissel[Z][shell] * AVOGNUM / AtomicWeight_arr[Z];
}


/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (cm2/g)        //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CS_FluorLine_Kissel(int Z, int line, double E, xrl_error **error) {
  return CS_FluorLine_Kissel_Cascade(Z, line, E, error);
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (barns/atom)   //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CSb_FluorLine_Kissel(int Z, int line, double E, xrl_error **error) {
  double cs = CS_FluorLine_Kissel_Cascade(Z, line, E, error);
  if (cs == 0.0)
    return 0.0;
  return cs * AtomicWeight_arr[Z] / AVOGNUM;
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                  Total cross section  (cm2/g)                    //
//         (Photoelectric (Kissel) + Compton + Rayleigh)            //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

static double (*cs_total_kissel_components[])(int, double, xrl_error **) = {CS_Photo_Total, CS_Rayl, CS_Compt};

double CS_Total_Kissel(int Z, double E, xrl_error **error) { 
  int i;
  double rv = 0.0;

  if (Z < 1 || Z > ZMAX || NE_Photo_Total_Kissel[Z] < 0 || NE_Rayl[Z] < 0 || NE_Compt[Z] < 0) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, Z_OUT_OF_RANGE);
    return 0.0;
  }

  if (E <= 0.0) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, NEGATIVE_ENERGY);
    return 0.0;
  }

  for (i = 0 ; i < 3 ; i++) {
    double tmp = cs_total_kissel_components[i](Z, E, error);
    if (tmp == 0)
      return 0.0;
    rv += tmp;
  }

  return rv;
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                  Total cross section  (barn/atom)                //
//         (Photoelectric (Kissel) + Compton + Rayleigh)            //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CSb_Total_Kissel(int Z, double E, xrl_error **error) {
  double cs = CS_Total_Kissel(Z, E, error);
  if (cs == 0.0)
    return 0.0;
  return cs * AtomicWeight_arr[Z] / AVOGNUM;
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                  Electronic configuration                        //
//         		According to Lynn Kissel                    //
//                                                                  //
//          Z : atomic number                                       //
//          shell : shell macro                                     //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double ElectronConfig(int Z, int shell, xrl_error **error) {
  double rv = 0.0;
  if (Z < 1 || Z > ZMAX) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, Z_OUT_OF_RANGE);
    return 0.0;
  }

  if (shell < 0 || shell >= SHELLNUM_K ) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, UNKNOWN_SHELL);
    return 0.0;
  }

  rv = Electron_Config_Kissel[Z][shell];

  if (rv <= 0.0) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, INVALID_SHELL);
    return 0.0;
  }

  return rv;
}


/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (cm2/g)        //
//                       without cascade effects                    //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CS_FluorLine_Kissel_no_Cascade(int Z, int line, double E, xrl_error **error) {

  if (Z < 1 || Z > ZMAX) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, Z_OUT_OF_RANGE);
    return 0.0;
  }

  if (E <= 0.0) {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, NEGATIVE_ENERGY);
    return 0.0;
  }

  if (line >= KN5_LINE && line <= KB_LINE) {
    /*
     * K lines -> never cascade effect!
     */
    double cs, yield, rr;
    cs = CS_Photo_Partial(Z, K_SHELL, E, error);
    if (cs == 0.0)
      return 0.0;
    yield = FluorYield(Z, K_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return cs * yield * rr;
  }
  else if (line>=L1P5_LINE && line<=L1M1_LINE) {
    /*
     * L1 lines
     */
    double yield, rr;
    double PL1 = PL1_pure_kissel(Z, E, error);
    if (PL1 == 0.0)
      return 0.0;
    yield = FluorYield(Z, L1_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return PL1 * yield * rr;
  }
  else if (line >= L2Q1_LINE && line <= L2M1_LINE) {
    /*
     * L2 lines
     */
    double PL2, yield, rr;
    {
      double PL1 = PL1_pure_kissel(Z, E, NULL);
      PL2 = PL2_pure_kissel(Z, E, PL1, error);
    }
    if (PL2 == 0.0)
      return 0.0;
    yield = FluorYield(Z, L2_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return PL2 * yield * rr;
  }
  else if (line >= L3Q1_LINE && line <= L3M1_LINE) {
    /*
     * L3 lines
     */
    double PL3, yield, rr;
    {
      double PL1, PL2;
      PL1 = PL1_pure_kissel(Z, E, NULL);
      PL2 = PL2_pure_kissel(Z, E, PL1, NULL);
      PL3 = PL3_pure_kissel(Z, E, PL1, PL2, error);
    }
    if (PL3 == 0.0)
      return 0.0;
    yield = FluorYield(Z, L3_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return PL3 * yield * rr;
  }
  else if (line == LA_LINE) {
    double rv = CS_FluorLine_Kissel_no_Cascade(Z, L3M4_LINE, E, NULL) + CS_FluorLine_Kissel_no_Cascade(Z, L3M5_LINE, E, NULL);
    if (rv == 0.0)
      xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, TOO_LOW_EXCITATION_ENERGY);
    return rv; 
  }
  else if (line == LB_LINE) {
    double rv = 0.0;
    int i;
    for (i = 0 ; i < sizeof(LB_LINE_MACROS)/sizeof(LB_LINE_MACROS[0]) ; i++)
      rv += CS_FluorLine_Kissel_no_Cascade(Z, LB_LINE_MACROS[i], E, NULL);
    if (rv == 0.0)
      xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, TOO_LOW_EXCITATION_ENERGY);
    return rv; 
  }
  else if (line >= M1P5_LINE && line <= M1N1_LINE) {
    /*
     * M1 lines
     */
    double PM1, yield, rr;
    PM1 = PM1_pure_kissel(Z, E, error);
    if (PM1 == 0.0)
      return 0.0;
    yield = FluorYield(Z, M1_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return PM1 * yield * rr;
  }
  else if (line >= M2P5_LINE && line <= M2N1_LINE) {
    /*
     * M2 lines
     */
    double PM2, yield, rr;
    {
      double PM1 = PM1_pure_kissel(Z, E, NULL);
      PM2 = PM2_pure_kissel(Z, E, PM1, error);
    }
    if (PM2 == 0.0)
      return 0.0;
    yield = FluorYield(Z, M2_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return PM2 * yield * rr;
  }
  else if (line >= M3Q1_LINE && line <= M3N1_LINE) {
    /*
     * M3 lines
     */
    double PM3, yield, rr;
    {
      double PM1, PM2;
      PM1 = PM1_pure_kissel(Z, E, NULL);
      PM2 = PM2_pure_kissel(Z, E, PM1, NULL);
      PM3 = PM3_pure_kissel(Z, E, PM1, PM2, error);
    }
    if (PM3 == 0.0)
      return 0.0;
    yield = FluorYield(Z, M3_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return PM3 * yield * rr;
  }
  else if (line >= M4P5_LINE && line <= M4N1_LINE) {
    /*
     * M4 lines
     */
    double PM4, yield, rr;
    {
      double PM1, PM2, PM3;
      PM1 = PM1_pure_kissel(Z, E, NULL);
      PM2 = PM2_pure_kissel(Z, E, PM1, NULL);
      PM3 = PM3_pure_kissel(Z, E, PM1, PM2, NULL);
      PM4 = PM4_pure_kissel(Z, E, PM1, PM2, PM3, error);
    }
    if (PM4 == 0.0)
      return 0.0;
    yield = FluorYield(Z, M4_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return PM4 * yield * rr;
  }
  else if (line >= M5P5_LINE && line <= M5N1_LINE) {
    /*
     * M5 lines
     */
    double PM5, yield, rr;
    {
      double PM1, PM2, PM3, PM4;
      PM1 = PM1_pure_kissel(Z, E, NULL);
      PM2 = PM2_pure_kissel(Z, E, PM1, NULL);
      PM3 = PM3_pure_kissel(Z, E, PM1, PM2, NULL);
      PM4 = PM4_pure_kissel(Z, E, PM1, PM2, PM3, NULL);
      PM5 = PM5_pure_kissel(Z, E, PM1, PM2, PM3, PM4, error);
    }
    if (PM5 == 0.0)
      return 0.0;
    yield = FluorYield(Z, M5_SHELL, error);
    if (yield == 0.0)
      return 0.0;
    rr = RadRate(Z, line, error);
    if (rr == 0.0)
      return 0.0;
    return PM5 * yield * rr;
  }
  else {
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, INVALID_LINE);
    return 0.0;
  }  
}

#define CS_FLUORLINE_CASCADE_BODY(kind) \
  if (Z < 1 || Z > ZMAX) { \
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, Z_OUT_OF_RANGE); \
    return 0.0; \
  } \
  \
  if (E <= 0.0) { \
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, NEGATIVE_ENERGY); \
    return 0.0; \
  } \
  \
  if (line >= KN5_LINE && line <= KB_LINE) { \
    /* \
     * K lines -> never cascade effect! \
     */ \
    double cs, yield, rr; \
    cs = CS_Photo_Partial(Z, K_SHELL, E, error); \
    if (cs == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, K_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return cs * yield * rr; \
  } \
  else if (line >= L1P5_LINE && line <= L1M1_LINE) { \
    /* \
     * L1 lines \
     */ \
    double PL1, yield, rr; \
    { \
      double PK; \
      PK = CS_Photo_Partial(Z, K_SHELL, E, NULL); \
      PL1 = PL1_ ## kind ## _cascade_kissel(Z, E, PK, error); \
    } \
    if (PL1 == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, L1_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return PL1 * yield * rr; \
  } \
  else if (line>=L2Q1_LINE && line<=L2M1_LINE) { \
    /* \
     * L2 lines \
     */ \
    double PL2, yield, rr; \
    { \
      double PK, PL1; \
      PK = CS_Photo_Partial(Z, K_SHELL, E, NULL); \
      PL1 = PL1_ ## kind ## _cascade_kissel(Z, E, PK, NULL); \
      PL2 = PL2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, error); \
    } \
    if (PL2 == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, L2_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return PL2 * yield * rr; \
  } \
  else if (line>=L3Q1_LINE && line<=L3M1_LINE) { \
    /* \
     * L3 lines \
     */ \
    double PL3, yield, rr; \
    { \
      double PK, PL1, PL2; \
      PK = CS_Photo_Partial(Z, K_SHELL, E, NULL); \
      PL1 = PL1_ ## kind ## _cascade_kissel(Z, E, PK, NULL); \
      PL2 = PL2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, NULL); \
      PL3 = PL3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, error); \
    } \
    if (PL3 == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, L3_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return PL3 * yield * rr; \
  } \
  else if (line == LA_LINE) { \
    double rv = CS_FluorLine_Kissel_Radiative_Cascade(Z, L3M4_LINE, E, NULL) + CS_FluorLine_Kissel_Radiative_Cascade(Z, L3M5_LINE, E, NULL); \
    if (rv == 0.0) \
      xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, TOO_LOW_EXCITATION_ENERGY); \
    return rv;  \
  } \
  else if (line == LB_LINE) { \
    double rv = 0.0; \
    int i; \
    for (i = 0 ; i < sizeof(LB_LINE_MACROS)/sizeof(LB_LINE_MACROS[0]) ; i++) \
      rv += CS_FluorLine_Kissel_no_Cascade(Z, LB_LINE_MACROS[i], E, NULL); \
    if (rv == 0.0) \
      xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, TOO_LOW_EXCITATION_ENERGY); \
    return rv;  \
  } \
  else if (line>=M1P5_LINE && line<=M1N1_LINE) { \
    /* \
     * M1 lines \
     */ \
    double PM1, yield, rr; \
    { \
      double PK, PL1, PL2, PL3; \
      PK = CS_Photo_Partial(Z, K_SHELL, E, NULL); \
      PL1 = PL1_ ## kind ## _cascade_kissel(Z, E, PK, NULL); \
      PL2 = PL2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, NULL); \
      PL3 = PL3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, NULL); \
      PM1 = PM1_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, error); \
    } \
    if (PM1 == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, M1_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return PM1 * yield * rr; \
  } \
  else if (line>=M2P5_LINE && line<=M2N1_LINE) { \
    /* \
     * M2 lines \
     */ \
    double PM2, yield, rr; \
    { \
      double PK, PL1, PL2, PL3, PM1; \
      PK = CS_Photo_Partial(Z, K_SHELL, E, NULL); \
      PL1 = PL1_ ## kind ## _cascade_kissel(Z, E, PK, NULL); \
      PL2 = PL2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, NULL); \
      PL3 = PL3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, NULL); \
      PM1 = PM1_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, NULL); \
      PM2 = PM2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, error); \
    } \
    if (PM2 == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, M2_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return PM2 * yield * rr; \
  } \
  else if (line>=M3Q1_LINE && line<=M3N1_LINE) { \
    /* \
     * M3 lines \
     */ \
    double PM3, yield, rr; \
    { \
      double PK, PL1, PL2, PL3, PM1, PM2; \
      PK = CS_Photo_Partial(Z, K_SHELL, E, NULL); \
      PL1 = PL1_ ## kind ## _cascade_kissel(Z, E, PK, NULL); \
      PL2 = PL2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, NULL); \
      PL3 = PL3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, NULL); \
      PM1 = PM1_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, NULL); \
      PM2 = PM2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, NULL); \
      PM3 = PM3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, PM2, error); \
    } \
    if (PM3 == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, M3_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return PM3 * yield * rr; \
  } \
  else if (line>=M4P5_LINE && line<=M4N1_LINE) { \
    /* \
     * M4 lines \
     */ \
    double PM4, yield, rr; \
    { \
      double PK, PL1, PL2, PL3, PM1, PM2, PM3; \
      PK = CS_Photo_Partial(Z, K_SHELL, E, NULL); \
      PL1 = PL1_ ## kind ## _cascade_kissel(Z, E, PK, NULL); \
      PL2 = PL2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, NULL); \
      PL3 = PL3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, NULL); \
      PM1 = PM1_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, NULL); \
      PM2 = PM2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, NULL); \
      PM3 = PM3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, PM2, NULL); \
      PM4 = PM4_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, PM2, PM3, error); \
    } \
    if (PM4 == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, M4_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return PM4 * yield * rr; \
  } \
  else if (line>=M5P5_LINE && line<=M5N1_LINE) { \
    /* \
     * M5 lines \
     */ \
    double PM5, yield, rr; \
    { \
      double PK, PL1, PL2, PL3, PM1, PM2, PM3, PM4; \
      PK = CS_Photo_Partial(Z, K_SHELL, E, NULL); \
      PL1 = PL1_ ## kind ## _cascade_kissel(Z, E, PK, NULL); \
      PL2 = PL2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, NULL); \
      PL3 = PL3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, NULL); \
      PM1 = PM1_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, NULL); \
      PM2 = PM2_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, NULL); \
      PM3 = PM3_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, PM2, NULL); \
      PM4 = PM4_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, PM2, PM3, NULL); \
      PM5 = PM5_ ## kind ## _cascade_kissel(Z, E, PK, PL1, PL2, PL3, PM1, PM2, PM3, PM4, error); \
    } \
    if (PM5 == 0.0) \
      return 0.0; \
    yield = FluorYield(Z, M5_SHELL, error); \
    if (yield == 0.0) \
      return 0.0; \
    rr = RadRate(Z, line, error); \
    if (rr == 0.0) \
      return 0.0; \
    return PM5 * yield * rr; \
  } \
  else { \
    xrl_set_error_literal(error, XRL_ERROR_INVALID_ARGUMENT, INVALID_LINE); \
    return 0.0; \
  }  

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (cm2/g)        //
//                       with radiative cascade effects             //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CS_FluorLine_Kissel_Radiative_Cascade(int Z, int line, double E, xrl_error **error) {
  CS_FLUORLINE_CASCADE_BODY(rad)
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (cm2/g)        //
//                       with non-radiative cascade effects         //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CS_FluorLine_Kissel_Nonradiative_Cascade(int Z, int line, double E, xrl_error **error) {
  CS_FLUORLINE_CASCADE_BODY(auger)
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (cm2/g)        //
//                       with cascade effects                       //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CS_FluorLine_Kissel_Cascade(int Z, int line, double E, xrl_error **error) {
  CS_FLUORLINE_CASCADE_BODY(full)
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (barns/atom)   //
//                       with cascade effects                       //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CSb_FluorLine_Kissel_Cascade(int Z, int line, double E, xrl_error **error) {
  double cs = CS_FluorLine_Kissel_Cascade(Z, line, E, error);
  if (cs == 0.0)
    return 0.0;
  return cs * AtomicWeight_arr[Z] / AVOGNUM;
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (barns/atom)   //
//                       with non-radiative cascade effects         //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CSb_FluorLine_Kissel_Nonradiative_Cascade(int Z, int line, double E, xrl_error **error) {
  double cs = CS_FluorLine_Kissel_Nonradiative_Cascade(Z, line, E, error);
  if (cs == 0.0)
    return 0.0;
  return cs * AtomicWeight_arr[Z] / AVOGNUM;
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (barns/atom)   //
//                       with radiative cascade effects             //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CSb_FluorLine_Kissel_Radiative_Cascade(int Z, int line, double E, xrl_error **error) {
  double cs = CS_FluorLine_Kissel_Radiative_Cascade(Z, line, E, error);
  if (cs == 0.0)
    return 0.0;
  return cs * AtomicWeight_arr[Z] / AVOGNUM;
}

/*////////////////////////////////////////////////////////////////////
//                                                                  //
//                    Fluorescent line cross section (barns/atom)   //
//                       with non-radiative cascade effects         //
//                                                                  //
//          Z : atomic number                                       //
//          E : energy (keV)                                        //
//          line :                                                  //
//            KA_LINE 0                                             //
//            KB_LINE 1                                             //
//            LA_LINE 2                                             //
//            LB_LINE 3                                             //
//                                                                  //
/////////////////////////////////////////////////////////////////// */

double CSb_FluorLine_Kissel_no_Cascade(int Z, int line, double E, xrl_error **error) {
  double cs = CS_FluorLine_Kissel_no_Cascade(Z, line, E, error);
  if (cs == 0.0)
    return 0.0;
  return cs * AtomicWeight_arr[Z] / AVOGNUM;
}
