/** \file conversion.hpp
 * \brief Conversion maths from alpha/beta/z to DACs & back
 *
 * \ingroup fsmCtrl_files
 */

#pragma once

namespace MagAOX
{
    namespace app
    {

        // (dac1, dac2, dac3) ---> (alpha, beta, z)

        double get_alpha(double dac1, double dac2, double dac3, double a)
        {
            return 1. / a * (dac1 - 0.5 * (dac2 + dac3));
        }

        double get_beta(double dac2, double dac3, double b)
        {
            return 1. / b * (dac2 - dac3);
        }

        double get_z(double dac1, double dac2, double dac3)
        {
            return 1. / 3. * (dac1 + dac2 + dac3);
        }

        // (alpha, beta, z) ---> (dac1, dac2, dac3)

        double angles_to_dac1(double alpha, double z, double a)
        {
            return z + 2. / 3. * a * alpha;
        }

        double angles_to_dac2(double alpha, double beta, double z, double a, double b)
        {
            return 0.5 * b * beta + z - 1. / 3. * a * alpha;
        }

        double angles_to_dac3(double alpha, double beta, double z, double a, double b)
        {
            return z - 1. / 3. * a * alpha - 1. / 2. * b * beta;
        }

        // vectors

        double DAC_to_angles(double dac1, double dac2, double dac3, double a, double b)
        {
            return get_alpha(dac1, dac2, dac3, a), get_beta(dac2, dac3, b), get_z(dac1, dac2, dac3);
        }

        double angles_to_DAC(double alpha, double beta, double z, double a, double b)
        {
            return angles_to_dac1(alpha, z, a), angles_to_dac2(alpha, beta, z, a, b), angles_to_dac3(alpha, beta, z, a, b);
        }

        // constraints

        double get_alpha_min(double dac1_min, double dac2_max, double dac3_max, double a)
        {
            return 1. / a * (dac1_min - 0.5 * (dac2_max + dac3_max));
        }

        double get_alpha_max(double dac1_max, double dac2_min, double dac3_min, double a)
        {
            return 1. / a * (dac1_max - 0.5 * (dac2_min + dac3_min));
        }

        double get_beta_min(double dac2_min, double dac3_max, double b)
        {
            return 1. / b * (dac2_min - dac3_max);
        }

        double get_beta_max(double dac2_max, double dac3_min, double b)
        {
            return 1. / b * (dac2_max - dac3_min);
        }

        ////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////

        // (v1, v2, v3) ---> (dac1, dac2, dac3)
        double v1_to_dac1(double v1, double v)
        {
            return v1 / v;
        }

        double v2_to_dac2(double v2, double v)
        {
            return v2 / v;
        }

        double v3_to_dac3(double v3, double v)
        {
            return v3 / v;
            // ((4.096 / pow(2.0, 24)) * 60);
        }

        // (dac1, dac2, dac3) ---> (v1, v2, v3)
        double get_v1(double dac1, double v)
        {
            return dac1 * v;
        }

        double get_v2(double dac2, double v)
        {
            return dac2 * v;
        }

        double get_v3(double dac3, double v)
        {
            return dac3 * v;
        }

    } // namespace app
} // namespace MagAOX