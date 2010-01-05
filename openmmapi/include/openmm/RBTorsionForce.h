#ifndef OPENMM_RBTORSIONFORCE_H_
#define OPENMM_RBTORSIONFORCE_H_

/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2008-2009 Stanford University and the Authors.      *
 * Authors: Peter Eastman                                                     *
 * Contributors:                                                              *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

#include "Force.h"
#include "Vec3.h"
#include <map>
#include <vector>
#include "internal/windowsExport.h"

namespace OpenMM {

/**
 * This class implements an interaction between groups of four particles that varies with the torsion angle between them
 * according to the Ryckaert-Bellemans potential.  To use it, create an RBTorsionForce object then call addTorsion() once
 * for each torsion.  After a torsion has been added, you can modify its force field parameters by calling setTorsionParameters().
 */

class OPENMM_EXPORT RBTorsionForce : public Force {
public:
    /**
     * Create a RBTorsionForce.
     */
    RBTorsionForce();
    /**
     * Get the number of Ryckaert-Bellemans torsion terms in the potential function
     */
    int getNumTorsions() const {
        return rbTorsions.size();
    }
    /**
     * Add a Ryckaert-Bellemans torsion term to the force field.
     *
     * @param particle1    the index of the first particle forming the torsion
     * @param particle2    the index of the second particle forming the torsion
     * @param particle3    the index of the third particle forming the torsion
     * @param particle3    the index of the fourth particle forming the torsion
     * @param c0           the coefficient of the constant term
     * @param c1           the coefficient of the 1st order term
     * @param c2           the coefficient of the 2nd order term
     * @param c3           the coefficient of the 3rd order term
     * @param c4           the coefficient of the 4th order term
     * @param c5           the coefficient of the 5th order term
     * @return the index of the torsion that was added
     */
    int addTorsion(int particle1, int particle2, int particle3, int particle4, double c0, double c1, double c2, double c3, double c4, double c5);
    /**
     * Get the force field parameters for a Ryckaert-Bellemans torsion term.
     * 
     * @param index        the index of the torsion for which to get parameters
     * @param particle1    the index of the first particle forming the torsion
     * @param particle2    the index of the second particle forming the torsion
     * @param particle3    the index of the third particle forming the torsion
     * @param particle3    the index of the fourth particle forming the torsion
     * @param c0           the coefficient of the constant term
     * @param c1           the coefficient of the 1st order term
     * @param c2           the coefficient of the 2nd order term
     * @param c3           the coefficient of the 3rd order term
     * @param c4           the coefficient of the 4th order term
     * @param c5           the coefficient of the 5th order term
     */
    void getTorsionParameters(int index, int& particle1, int& particle2, int& particle3, int& particle4, double& c0, double& c1, double& c2, double& c3, double& c4, double& c5) const;
    /**
     * Set the force field parameters for a Ryckaert-Bellemans torsion term.
     * 
     * @param index        the index of the torsion for which to set parameters
     * @param particle1    the index of the first particle forming the torsion
     * @param particle2    the index of the second particle forming the torsion
     * @param particle3    the index of the third particle forming the torsion
     * @param particle3    the index of the fourth particle forming the torsion
     * @param c0           the coefficient of the constant term
     * @param c1           the coefficient of the 1st order term
     * @param c2           the coefficient of the 2nd order term
     * @param c3           the coefficient of the 3rd order term
     * @param c4           the coefficient of the 4th order term
     * @param c5           the coefficient of the 5th order term
     */
    void setTorsionParameters(int index, int particle1, int particle2, int particle3, int particle4, double c0, double c1, double c2, double c3, double c4, double c5);
protected:
    ForceImpl* createImpl();
private:
    class RBTorsionInfo;
    std::vector<RBTorsionInfo> rbTorsions;
};

/**
 * This is an internal class used to record information about a torsion.
 * @private
 */
class RBTorsionForce::RBTorsionInfo {
public:
    int particle1, particle2, particle3, particle4;
    double c[6];
    RBTorsionInfo() {
        particle1 = particle2 = particle3 = particle4 = -1;
        c[0] = c[1] = c[2] = c[3] = c[4] = c[5] = 0.0;
    }
    RBTorsionInfo(int particle1, int particle2, int particle3, int particle4, double c0, double c1, double c2, double c3, double c4, double c5) :
            particle1(particle1), particle2(particle2), particle3(particle3), particle4(particle4) {
        c[0] = c0;
        c[1] = c1;
        c[2] = c2;
        c[3] = c3;
        c[4] = c4;
        c[5] = c5;
    }
};

} // namespace OpenMM

#endif /*OPENMM_RBTORSIONFORCE_H_*/
