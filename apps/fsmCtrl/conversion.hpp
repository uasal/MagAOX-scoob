/** \file conversion.hpp
 * \brief Conversion maths from alpha/beta/z to DACs & back
 *
 * \ingroup fsmCtrl_files
 */

#pragma once

#include <cmath>  // For M_PI

namespace MagAOX
{
    namespace app
    {
        ////////////////////////////////////////////////////////
        /// V -> DAC
        ////////////////////////////////////////////////////////

        // (v1, v2, v3) ---> (dac1, dac2, dac3)
        double vi_to_daci(double vi, double v)
        {
            return vi / v;
        }

        ////////////////////////////////////////////////////////
        /// DAC -> V
        ////////////////////////////////////////////////////////

        // (dac1, dac2, dac3) ---> (v1, v2, v3)
        double daci_to_vi(double daci, double v)
        {
            return daci * v;
        }

        ////////////////////////////////////////////////////////
        /// TTP -> V
        ////////////////////////////////////////////////////////

        // (alpha/tip, beta/tilt, z/piston) ---> (v1, v2, v3)

        // alpha in arsecs, B & L in um, D_per_V in um/V 
        double ttp_to_v1(double alpha, double z, double B, double D_per_V)
        {
            alpha = (alpha / 3600. ) * M_PI / 180.;

            double dA = z + 2. / 3. * B * alpha;

            return dA / D_per_V;
        }

        // alpha & beta in arsecs, B & L in um, D_per_V in um/V
        double ttp_to_v2(double alpha, double beta, double z, double B, double L, double D_per_V)
        {
            alpha = (alpha / 3600. ) * M_PI / 180.;
            beta = (beta / 3600. ) * M_PI / 180.;

            double dB = 0.5 * L * beta + z - 1. / 3. * B * alpha;

            return dB / D_per_V;
        }

        // alpha & beta in arsecs, B & L in um, D_per_V in um/V
        double ttp_to_v3(double alpha, double beta, double z, double B, double L, double D_per_V)
        {
            alpha = (alpha / 3600. ) * M_PI / 180.;
            beta = (beta / 3600. ) * M_PI / 180.;

            double dC = z - 1. / 3. * B * alpha - 1. / 2. * L * beta;

            return dC / D_per_V;
        }

        ////////////////////////////////////////////////////////
        /// V -> TTP
        ////////////////////////////////////////////////////////

        // (v1, v2, v3) ---> (alpha/tip, beta/tilt, z/piston)

        double vi_to_tip(double dvA, double dvB, double dvC, double B, double D_per_V)
        {
            double dA = dvA * D_per_V;
            double dB = dvB * D_per_V;
            double dC = dvC * D_per_V;
            return (1. / B * (dA - 0.5 * (dB + dC))) * (3600 * 180 / M_PI);
        }

        double vi_to_tilt(double dvB, double dvC, double L, double D_per_V)
        {
            double dB = dvB * D_per_V;
            double dC = dvC * D_per_V;
            return (1. / L * (dB - dC)) * (3600 * 180 / M_PI);
        }

        double vi_to_piston(double dvA, double dvB, double dvC, double D_per_V)
        {
            double dA = dvA * D_per_V;
            double dB = dvB * D_per_V;
            double dC = dvC * D_per_V;
            return (1. / 3. * (dA + dB + dC)) * (3600 * 180 / M_PI);
        }     

        ////////////////////////////////////////////////////////
        /// TTP -> DAC
        ////////////////////////////////////////////////////////

        // (alpha/tip, beta/tilt, z/piston) ---> (dac1, dac2, dac3)

        // alpha in arsecs, B & L in um, D_per_V in um/V 
        double ttp_to_dac1(double alpha, double z, double B, double D_per_V, double v)
        {
            double dvA = ttp_to_v1(alpha, z, B, D_per_V);
            return vi_to_daci(dvA, v);
        }

        // alpha & beta in arsecs, B & L in um, D_per_V in um/V
        double ttp_to_dac2(double alpha, double beta, double z, double B, double L, double D_per_V, double v)
        {

            double dvB = ttp_to_v2(alpha, beta, z, B, L, D_per_V);
            return vi_to_daci(dvB, v);
        }

        // alpha & beta in arsecs, B & L in um, D_per_V in um/V
        double ttp_to_dac3(double alpha, double beta, double z, double B, double L, double D_per_V, double v)
        {
            double dvC = ttp_to_v3(alpha, beta, z, B, L, D_per_V);
            return vi_to_daci(dvC, v);
        }

    
        ////////////////////////////////////////////////////////
        /// DAC - > TTP
        ////////////////////////////////////////////////////////

        // (dac1, dac2, dac3) ---> (alpha/tip, beta/tilt, z/piston)

        double daci_to_tip(double dac1, double dac2, double dac3, double B, double D_per_V, double v)
        {
            double dvA = daci_to_vi(dac1, v);
            double dvB = daci_to_vi(dac2, v);
            double dvC = daci_to_vi(dac3, v);
            return vi_to_tip(dvA, dvB, dvC, B, D_per_V);
        }

        double daci_to_tilt(double dac2, double dac3, double L, double D_per_V, double v)
        {
            double dvB = daci_to_vi(dac2, v);
            double dvC = daci_to_vi(dac3, v);            
            return vi_to_tilt(dvB, dvC, L, D_per_V);
        }

        double daci_to_piston(double dac1, double dac2, double dac3, double D_per_V, double v)
        {
            double dvA = daci_to_vi(dac1, v);
            double dvB = daci_to_vi(dac2, v);
            double dvC = daci_to_vi(dac3, v);            
            return vi_to_piston(dvA, dvB, dvC, D_per_V);
        }        

        // // vectors

        // double DAC_to_ttp(double dac1, double dac2, double dac3, double B, double L)
        // {
        //     return get_alpha(dac1, dac2, dac3, B), get_beta(dac2, dac3, L), get_z(dac1, dac2, dac3);
        // }

        // double ttp_to_DAC(double alpha, double beta, double z, double B, double L)
        // {
        //     return ttp_to_dac1(alpha, z, B), ttp_to_dac2(alpha, beta, z, B, L), ttp_to_dac3(alpha, beta, z, B, L);
        // }

        // // constraints

        // double get_alpha_min(double dac1_min, double dac2_max, double dac3_max, double B)
        // {
        //     return 1. / B * (dac1_min - 0.5 * (dac2_max + dac3_max));
        // }

        // double get_alpha_max(double dac1_max, double dac2_min, double dac3_min, double B)
        // {
        //     return 1. / B * (dac1_max - 0.5 * (dac2_min + dac3_min));
        // }

        // double get_beta_min(double dac2_min, double dac3_max, double L)
        // {
        //     return 1. / L * (dac2_min - dac3_max);
        // }

        // double get_beta_max(double dac2_max, double dac3_min, double L)
        // {
        //     return 1. / L * (dac2_max - dac3_min);
        // }


    } // namespace app
} // namespace MagAOX