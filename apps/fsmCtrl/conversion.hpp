/** \file conversion.hpp
 * \brief Conversion maths from tip/tilt/piston to DACs & back
 *
 * \ingroup fsmCtrl_files
 */

#pragma once

namespace MagAOX
{
    namespace app
    {

        // (dac1, dac2, dac3) ---> (tip, tilt, piston)

        double get_tip(double dac1, double dac2, double dac3, double a)
        {
            return 1. / a * (dac1 - 0.5 * (dac2 + dac3));
        }

        double get_tilt(double dac2, double dac3, double b)
        {
            return 1. / b * (dac2 - dac3);
        }

        double get_piston(double dac1, double dac2, double dac3)
        {
            return 1. / 3. * (dac1 + dac2 + dac3);
        }

        // (tip, tilt, piston) ---> (dac1, dac2, dac3)

        double get_dac1(double tip, double piston, double a)
        {
            return piston + 2. / 3. * a * tip;
        }

        double get_dac2(double tip, double tilt, double piston, double a, double b)
        {
            return 0.5 * b * tilt + piston - 1. / 3. * a * tip;
        }

        double get_dac3(double tip, double tilt, double piston, double a, double b)
        {
            return piston - 1. / 3. * a * tip - 1. / 2. * b * tilt;
        }

        // vectors

        double DAC_to_angles(double dac1, double dac2, double dac3, double a, double b)
        {
            return get_tip(dac1, dac2, dac3, a), get_tilt(dac2, dac3, b), get_piston(dac1, dac2, dac3);
        }

        double angles_to_DAC(double tip, double tilt, double piston, double a, double b)
        {
            return get_dac1(tip, piston, a), get_dac2(tip, tilt, piston, a, b), get_dac3(tip, tilt, piston, a, b);
        }

        // constraints

        double get_tip_min(double dac1_min, double dac2_max, double dac3_max, double a)
        {
            return 1. / a * (dac1_min - 0.5 * (dac2_max + dac3_max));
        }

        double get_tip_max(double dac1_max, double dac2_min, double dac3_min, double a)
        {
            return 1. / a * (dac1_max - 0.5 * (dac2_min + dac3_min));
        }

        double get_tilt_min(double dac2_min, double dac3_max, double b)
        {
            return 1. / b * (dac2_min - dac3_max);
        }

        double get_tilt_max(double dac2_max, double dac3_min, double b)
        {
            return 1. / b * (dac2_max - dac3_min);
        }

        ////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////

        // (v1, v2, v3) ---> (dac1, dac2, dac3)
        double get_dac1(double v1)
        {
            return v1 / ((4.096 / pow(2.0, 24)) * 60);
        }

        double get_dac2(double v2)
        {
            return v2 / ((4.096 / pow(2.0, 24)) * 60);
        }

        double get_dac3(double v3)
        {
            return v3 / ((4.096 / pow(2.0, 24)) * 60);
        }

        // (dac1, dac2, dac3) ---> (v1, v2, v3)
        double get_v1(double dac1)
        {
            return dac1 * ((4.096 / pow(2.0, 24)) * 60);
        }

        double get_v2(double dac2)
        {
            return dac2 * ((4.096 / pow(2.0, 24)) * 60);
        }

        double get_v3(double dac3)
        {
            return dac3 * ((4.096 / pow(2.0, 24)) * 60);
        }

    } // namespace app
} // namespace MagAOX