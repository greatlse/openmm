#ifndef OPENMM_CUSTOMGBFORCE_H_
#define OPENMM_CUSTOMGBFORCE_H_

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
#include <set>
#include <utility>
#include <vector>
#include "internal/windowsExport.h"

namespace OpenMM {

/**
 * This class implements complex, multiple stage nonbonded interactions between particles.  It is designed primarily
 * for implementing Generalized Born implicit solvation models, although it is not strictly limited to that purpose.
 * The interaction is specified as a series of computations, each defined by an arbitrary algebraic expression.
 * It also allows tabulated functions to be defined and used with the computations.  It optionally supports periodic boundary
 * conditions and cutoffs for long range interactions.
 *
 * The computation consists of calculating some number of per-particle <i>computed values</i>, followed by one or more
 * <i>energy terms</i>.  A computed value is a scalar value that is computed for each particle in the system.  It may
 * depend on an arbitrary set of global and per-particle parameters, and well as on other computed values that have
 * been calculated before it.  Once all computed values have been calculated, the energy terms and their derivatives
 * are evaluated to determine the system energy and particle forces.  The energy terms may depend on global parameters,
 * per-particle parameters, and per-particle computed values.
 *
 * When specifying a computed value or energy term, you provide an algebraic expression to evaluate and a <i>computation type</i>
 * describing how the expression is to be evaluated.  There are two main types of computations:
 *
 * <ul>
 * <li><b>Single Particle</b>: The expression is evaluated once for each particle in the System.  In the case of a computed
 * value, this means the value for a particle depends only on other properties of that particle (its parameters and other
 * computed values).  In the case of an energy term, it means each particle makes an independent contribution to the System
 * energy.</li>
 * <li><b>Particle Pairs</b>: The expression is evaluated for every pair of particles in the system.  In the case of a computed
 * value, the value for a particular particle is calculated by pairing it with every other particle in the system, evaluating
 * the expression for each pair, and summing them.  For an energy term, each particle pair makes an independent contribution to
 * the System energy.  (Note that energy terms are assumed to be symmetric with respect to the two interacting particles, and
 * therefore are evaluated only once per pair.  In contrast, expressions for computed values need not be symmetric and therefore are calculated
 * twice for each pair: once when calculating the value for the first particle, and again when calculating the value for the
 * second particle.)</li>
 * </ul>
 *
 * Be aware that, although this class is extremely general in the computations it can define, particular Platforms may only support
 * more restricted types of computations.  In particular, all currently existing Platforms require that the first computed value
 * <i>must</i> be a particle pair computation, and all computed values after the first <i>must</i> be single particle computations.
 * This is sufficient for most Generalized Born models, but might not permit some other types of calculations to be implemented.
 *
 * This is a complicated class to use, and an example may help to clarify it.  The following code implements the OBC variant
 * of the GB/SA solvation model, using the ACE approximation to estimate surface area:
 *
 * <tt><pre>
 * CustomGBForce* custom = new CustomGBForce();
 * custom->addPerParticleParameter("q");
 * custom->addPerParticleParameter("radius");
 * custom->addPerParticleParameter("scale");
 * custom->addGlobalParameter("solventDielectric", obc->getSolventDielectric());
 * custom->addGlobalParameter("soluteDielectric", obc->getSoluteDielectric());
 * custom->addComputedValue("I", "step(r+sr2-or1)*0.5*(1/L-1/U+0.25*(1/U^2-1/L^2)*(r-sr2*sr2/r)+0.5*log(L/U)/r+C);"
 *                               "U=r+sr2;"
 *                               "C=2*(1/or1-1/L)*step(sr2-r-or1);"
 *                               "L=step(or1-D)*or1+(1-step(or1-D))*D;"
 *                               "D=step(r-sr2)*(r-sr2)+(1-step(r-sr2))*(sr2-r);"
 *                               "sr2 = scale2*or2;"
 *                               "or1 = radius1-0.009; or2 = radius2-0.009", CustomGBForce::ParticlePairNoExclusions);
 * custom->addComputedValue("B", "1/(1/or-tanh(1*psi-0.8*psi^2+4.85*psi^3)/radius);"
 *                               "psi=I*or; or=radius-0.009", CustomGBForce::SingleParticle);
 * custom->addEnergyTerm("28.3919551*(radius+0.14)^2*(radius/B)^6-0.5*138.935456*(1/soluteDielectric-1/solventDielectric)*q^2/B",
 *                       CustomGBForce::SingleParticle);
 * custom->addEnergyTerm("-138.935456*(1/soluteDielectric-1/solventDielectric)*q1*q2/f;"
 *                       "f=sqrt(r^2+B1*B2*exp(-r^2/(4*B1*B2)))", CustomGBForce::ParticlePair);
 * </pre></tt>
 *
 * It begins by defining three per-particle parameters (charge, atomic radius, and scale factor) and two global parameters
 * (the dielectric constants for the solute and solvent).  It then defines a computed value "I" of type ParticlePair.  The
 * expression for evaluating it is a complicated function of the distance between each pair of particles (r), their atomic
 * radii (radius1 and radius2), and their scale factors (scale1 and scale2).  Very roughly speaking, it is a measure of the
 * distance between each particle and other nearby particles.
 *
 * Next a computation is defined for the Born Radius (B).  It is computed independently for each particle, and is a function of
 * that particle's atomic radius and the intermediate value I defined above.
 *
 * Finally, two energy terms are defined.  The first one is computed for each particle and represents the surface area term,
 * as well as the self interaction part of the polarization energy.  The second term is calculated for each pair of particles,
 * and represents the screening of electrostatic interactions by the solvent.
 *
 * After defining the force as shown above, you should then call addParticle() once for each particle in the System to set the
 * values of its per-particle parameters (q, radius, and scale).  The number of particles for which you set parameters must be
 * exactly equal to the number of particles in the System, or else an exception will be thrown when you try to create a Context.
 * After a particle has been added, you can modify its parameters by calling setParticleParameters().
 *
 * CustomNonbondedForce also lets you specify "exclusions", particular pairs of particles whose interactions should be
 * omitted from calculations.  This is most often used for particles that are bonded to each other.  Even if you specify exclusions,
 * however, you can use the computation type ParticlePairNoExclusions to indicate that exclusions should not be applied to a
 * particular piece of the computation.
 *
 * Expressions may involve the operators + (add), - (subtract), * (multiply), / (divide), and ^ (power), and the following
 * functions: sqrt, exp, log, sin, cos, sec, csc, tan, cot, asin, acos, atan, sinh, cosh, tanh, step.  All trigonometric functions
 * are defined in radians, and log is the natural logarithm.  step(x) = 0 if x is less than 0, 1 otherwise.  In expressions for
 * particle pair calculations, the names of per-particle parameters and computed values
 * have the suffix "1" or "2" appended to them to indicate the values for the two interacting particles.  As seen in the above example,
 * an expression may also involve intermediate quantities that are defined following the main expression, using ";" as a separator.
 *
 * In addition, you can call addFunction() to define a new function based on tabulated values.  You specify a vector of
 * values, and an interpolating or approximating spline is created from them.  That function can then appear in expressions.
 */

class OPENMM_EXPORT CustomGBForce : public Force {
public:
    /**
     * This is an enumeration of the different methods that may be used for handling long range nonbonded forces.
     */
    enum NonbondedMethod {
        /**
         * No cutoff is applied to nonbonded interactions.  The full set of N^2 interactions is computed exactly.
         * This necessarily means that periodic boundary conditions cannot be used.  This is the default.
         */
        NoCutoff = 0,
        /**
         * Interactions beyond the cutoff distance are ignored.
         */
        CutoffNonPeriodic = 1,
        /**
         * Periodic boundary conditions are used, so that each particle interacts only with the nearest periodic copy of
         * each other particle.  Interactions beyond the cutoff distance are ignored.
         */
        CutoffPeriodic = 2,
    };
    /**
     * This is an enumeration of the different ways in which a computed value or energy term can be calculated.
     */
    enum ComputationType {
        /**
         * The value is computed independently for each particle, based only on the parameters and computed values for that particle.
         */
        SingleParticle = 0,
        /**
         * The value is computed as a sum over all pairs of particles, except those which have been added as exclusions.
         */
        ParticlePair = 1,
        /**
         * The value is computed as a sum over all pairs of particles.  Unlike ParticlePair, the list of exclusions is ignored
         * and all pairs are included in the sum, even those marked as exclusions.
         */
        ParticlePairNoExclusions = 2
    };
    /**
     * Create a CustomGBForce.
     */
    CustomGBForce();
    /**
     * Get the number of particles for which force field parameters have been defined.
     */
    int getNumParticles() const {
        return particles.size();
    }
    /**
     * Get the number of particle pairs whose interactions should be excluded.
     */
    int getNumExclusions() const {
        return exclusions.size();
    }
    /**
     * Get the number of per-particle parameters that the interaction depends on.
     */
    int getNumPerParticleParameters() const {
        return parameters.size();
    }
    /**
     * Get the number of global parameters that the interaction depends on.
     */
    int getNumGlobalParameters() const {
        return globalParameters.size();
    }
    /**
     * Get the number of tabulated functions that have been defined.
     */
    int getNumFunctions() const {
        return functions.size();
    }
    /**
     * Get the number of per-particle computed values the interaction depends on.
     */
    int getNumComputedValues() const {
        return computedValues.size();
    }
    /**
     * Get the number of terms in the energy computation.
     */
    int getNumEnergyTerms() const {
        return energyTerms.size();
    }
    /**
     * Get the method used for handling long range nonbonded interactions.
     */
    NonbondedMethod getNonbondedMethod() const;
    /**
     * Set the method used for handling long range nonbonded interactions.
     */
    void setNonbondedMethod(NonbondedMethod method);
    /**
     * Get the cutoff distance (in nm) being used for nonbonded interactions.  If the NonbondedMethod in use
     * is NoCutoff, this value will have no effect.
     */
    double getCutoffDistance() const;
    /**
     * Set the cutoff distance (in nm) being used for nonbonded interactions.  If the NonbondedMethod in use
     * is NoCutoff, this value will have no effect.
     */
    void setCutoffDistance(double distance);
    /**
     * Add a new per-particle parameter that the interaction may depend on.
     *
     * @param name     the name of the parameter
     * @return the index of the parameter that was added
     */
    int addPerParticleParameter(const std::string& name);
    /**
     * Get the name of a per-particle parameter.
     *
     * @param index     the index of the parameter for which to get the name
     * @return the parameter name
     */
    const std::string& getPerParticleParameterName(int index) const;
    /**
     * Set the name of a per-particle parameter.
     *
     * @param index          the index of the parameter for which to set the name
     * @param name           the name of the parameter
     */
    void setPerParticleParameterName(int index, const std::string& name);
    /**
     * Add a new global parameter that the interaction may depend on.
     *
     * @param name             the name of the parameter
     * @param defaultValue     the default value of the parameter
     * @return the index of the parameter that was added
     */
    int addGlobalParameter(const std::string& name, double defaultValue);
    /**
     * Get the name of a global parameter.
     *
     * @param index     the index of the parameter for which to get the name
     * @return the parameter name
     */
    const std::string& getGlobalParameterName(int index) const;
    /**
     * Set the name of a global parameter.
     *
     * @param index          the index of the parameter for which to set the name
     * @param name           the name of the parameter
     */
    void setGlobalParameterName(int index, const std::string& name);
    /**
     * Get the default value of a global parameter.
     *
     * @param index     the index of the parameter for which to get the default value
     * @return the parameter default value
     */
    double getGlobalParameterDefaultValue(int index) const;
    /**
     * Set the default value of a global parameter.
     *
     * @param index          the index of the parameter for which to set the default value
     * @param name           the default value of the parameter
     */
    void setGlobalParameterDefaultValue(int index, double defaultValue);
    /**
     * Add the nonbonded force parameters for a particle.  This should be called once for each particle
     * in the System.  When it is called for the i'th time, it specifies the parameters for the i'th particle.
     *
     * @param parameters    the list of parameters for the new particle
     * @return the index of the particle that was added
     */
    int addParticle(const std::vector<double>& parameters);
    /**
     * Get the nonbonded force parameters for a particle.
     *
     * @param index       the index of the particle for which to get parameters
     * @param parameters  the list of parameters for the specified particle
     */
    void getParticleParameters(int index, std::vector<double>& parameters) const;
    /**
     * Set the nonbonded force parameters for a particle.
     *
     * @param index       the index of the particle for which to set parameters
     * @param parameters  the list of parameters for the specified particle
     */
    void setParticleParameters(int index, const std::vector<double>& parameters);
    /**
     * Add a computed value to calculate for each particle.
     *
     * @param name        the name of the value
     * @param expression  an algebraic expression to evaluate when calculating the computed value.  If the
     *                    ComputationType is SingleParticle, the expression is evaluated independently
     *                    for each particle, and may depend on the per-particle parameters and previous
     *                    computed values for that particle.  If the ComputationType is ParticlePair or
     *                    ParticlePairNoExclusions, the expression is evaluated once for every other
     *                    particle in the system and summed to get the final value.  In the latter case,
     *                    the expression may depend on the distance r between the two particles, and on
     *                    the per-particle parameters and previous computed values for each of them.
     *                    Append "1" to a variable name to indicate the parameter for the particle whose
     *                    value is being calculated, and "2" to indicate the particle it is interacting with.
     * @param type        the method to use for computing this value
     */
    int addComputedValue(const std::string& name, const std::string& expression, ComputationType type);
    /**
     * Get the properties of a computed value.
     *
     * @param index       the index of the computed value for which to get parameters
     * @param name        the name of the value
     * @param expression  an algebraic expression to evaluate when calculating the computed value.  If the
     *                    ComputationType is SingleParticle, the expression is evaluated independently
     *                    for each particle, and may depend on the per-particle parameters and previous
     *                    computed values for that particle.  If the ComputationType is ParticlePair or
     *                    ParticlePairNoExclusions, the expression is evaluated once for every other
     *                    particle in the system and summed to get the final value.  In the latter case,
     *                    the expression may depend on the distance r between the two particles, and on
     *                    the per-particle parameters and previous computed values for each of them.
     *                    Append "1" to a variable name to indicate the parameter for the particle whose
     *                    value is being calculated, and "2" to indicate the particle it is interacting with.
     * @param type        the method to use for computing this value
     */
    void getComputedValueParameters(int index, std::string& name, std::string& expression, ComputationType& type) const;
    /**
     * Set the properties of a computed value.
     *
     * @param index       the index of the computed value for which to set parameters
     * @param name        the name of the value
     * @param expression  an algebraic expression to evaluate when calculating the computed value.  If the
     *                    ComputationType is SingleParticle, the expression is evaluated independently
     *                    for each particle, and may depend on the per-particle parameters and previous
     *                    computed values for that particle.  If the ComputationType is ParticlePair or
     *                    ParticlePairNoExclusions, the expression is evaluated once for every other
     *                    particle in the system and summed to get the final value.  In the latter case,
     *                    the expression may depend on the distance r between the two particles, and on
     *                    the per-particle parameters and previous computed values for each of them.
     *                    Append "1" to a variable name to indicate the parameter for the particle whose
     *                    value is being calculated, and "2" to indicate the particle it is interacting with.
     * @param type        the method to use for computing this value
     */
    void setComputedValueParameters(int index, const std::string& name, const std::string& expression, ComputationType type);
    /**
     * Add a term to the energy computation.
     *
     * @param expression  an algebraic expression to evaluate when calculating the energy.  If the
     *                    ComputationType is SingleParticle, the expression is evaluated once
     *                    for each particle, and may depend on the per-particle parameters and
     *                    computed values for that particle.  If the ComputationType is ParticlePair or
     *                    ParticlePairNoExclusions, the expression is evaluated once for every pair of
     *                    particles in the system.  In the latter case,
     *                    the expression may depend on the distance r between the two particles, and on
     *                    the per-particle parameters and computed values for each of them.
     *                    Append "1" to a variable name to indicate the parameter for the first particle
     *                    in the pair and "2" to indicate the second particle in the pair.
     * @param type        the method to use for computing this value
     */
    int addEnergyTerm(const std::string& expression, ComputationType type);
    /**
     * Get the properties of a term to the energy computation.
     *
     * @param index       the index of the term for which to get parameters
     * @param expression  an algebraic expression to evaluate when calculating the energy.  If the
     *                    ComputationType is SingleParticle, the expression is evaluated once
     *                    for each particle, and may depend on the per-particle parameters and
     *                    computed values for that particle.  If the ComputationType is ParticlePair or
     *                    ParticlePairNoExclusions, the expression is evaluated once for every pair of
     *                    particles in the system.  In the latter case,
     *                    the expression may depend on the distance r between the two particles, and on
     *                    the per-particle parameters and computed values for each of them.
     *                    Append "1" to a variable name to indicate the parameter for the first particle
     *                    in the pair and "2" to indicate the second particle in the pair.
     * @param type        the method to use for computing this value
     */
    void getEnergyTermParameters(int index, std::string& expression, ComputationType& type) const;
    /**
     * Set the properties of a term to the energy computation.
     *
     * @param index       the index of the term for which to set parameters
     * @param expression  an algebraic expression to evaluate when calculating the energy.  If the
     *                    ComputationType is SingleParticle, the expression is evaluated once
     *                    for each particle, and may depend on the per-particle parameters and
     *                    computed values for that particle.  If the ComputationType is ParticlePair or
     *                    ParticlePairNoExclusions, the expression is evaluated once for every pair of
     *                    particles in the system.  In the latter case,
     *                    the expression may depend on the distance r between the two particles, and on
     *                    the per-particle parameters and computed values for each of them.
     *                    Append "1" to a variable name to indicate the parameter for the first particle
     *                    in the pair and "2" to indicate the second particle in the pair.
     * @param type        the method to use for computing this value
     */
    void setEnergyTermParameters(int index, const std::string& expression, ComputationType type);
    /**
     * Add a particle pair to the list of interactions that should be excluded.
     *
     * @param particle1  the index of the first particle in the pair
     * @param particle2  the index of the second particle in the pair
     * @return the index of the exclusion that was added
     */
    int addExclusion(int particle1, int particle2);
    /**
     * Get the particles in a pair whose interaction should be excluded.
     *
     * @param index      the index of the exclusion for which to get particle indices
     * @param particle1  the index of the first particle in the pair
     * @param particle2  the index of the second particle in the pair
     */
    void getExclusionParticles(int index, int& particle1, int& particle2) const;
    /**
     * Set the particles in a pair whose interaction should be excluded.
     *
     * @param index      the index of the exclusion for which to set particle indices
     * @param particle1  the index of the first particle in the pair
     * @param particle2  the index of the second particle in the pair
     */
    void setExclusionParticles(int index, int particle1, int particle2);
    /**
     * Add a tabulated function that may appear in the energy expression.
     *
     * @param name           the name of the function as it appears in expressions
     * @param values         the tabulated values of the function f(x) at uniformly spaced values of x between min and max.
     *                       The function is assumed to be zero for x &lt; min or x &gt; max.
     * @param min            the value of the independent variable corresponding to the first element of values
     * @param max            the value of the independent variable corresponding to the last element of values
     * @param interpolating  if true, an interpolating (Catmull-Rom) spline will be used to represent the function.
     *                       If false, an approximating spline (B-spline) will be used.
     * @return the index of the function that was added
     */
    int addFunction(const std::string& name, const std::vector<double>& values, double min, double max, bool interpolating);
    /**
     * Get the parameters for a tabulated function that may appear in the energy expression.
     *
     * @param index          the index of the function for which to get parameters
     * @param name           the name of the function as it appears in expressions
     * @param values         the tabulated values of the function f(x) at uniformly spaced values of x between min and max.
     *                       The function is assumed to be zero for x &lt; min or x &gt; max.
     * @param min            the value of the independent variable corresponding to the first element of values
     * @param max            the value of the independent variable corresponding to the last element of values
     * @param interpolating  if true, an interpolating (Catmull-Rom) spline will be used to represent the function.
     *                       If false, an approximating spline (B-spline) will be used.
     */
    void getFunctionParameters(int index, std::string& name, std::vector<double>& values, double& min, double& max, bool& interpolating) const;
    /**
     * Set the parameters for a tabulated function that may appear in algebraic expressions.
     *
     * @param index          the index of the function for which to set parameters
     * @param name           the name of the function as it appears in expressions
     * @param values         the tabulated values of the function f(x) at uniformly spaced values of x between min and max.
     *                       The function is assumed to be zero for x &lt; min or x &gt; max.
     * @param min            the value of the independent variable corresponding to the first element of values
     * @param max            the value of the independent variable corresponding to the last element of values
     * @param interpolating  if true, an interpolating (Catmull-Rom) spline will be used to represent the function.
     *                       If false, an approximating spline (B-spline) will be used.
     */
    void setFunctionParameters(int index, const std::string& name, const std::vector<double>& values, double min, double max, bool interpolating);
protected:
    ForceImpl* createImpl();
private:
    class ParticleInfo;
    class PerParticleParameterInfo;
    class GlobalParameterInfo;
    class ExclusionInfo;
    class FunctionInfo;
    class ComputationInfo;
    NonbondedMethod nonbondedMethod;
    double cutoffDistance;
    std::string energyExpression;
    std::vector<PerParticleParameterInfo> parameters;
    std::vector<GlobalParameterInfo> globalParameters;
    std::vector<ParticleInfo> particles;
    std::vector<ExclusionInfo> exclusions;
    std::vector<FunctionInfo> functions;
    std::vector<ComputationInfo> computedValues;
    std::vector<ComputationInfo> energyTerms;
};

/**
 * This is an internal class used to record information about a particle.
 * @private
 */
class CustomGBForce::ParticleInfo {
public:
    std::vector<double> parameters;
    ParticleInfo() {
    }
    ParticleInfo(const std::vector<double>& parameters) : parameters(parameters) {
    }
};

/**
 * This is an internal class used to record information about a per-particle parameter.
 * @private
 */
class CustomGBForce::PerParticleParameterInfo {
public:
    std::string name;
    PerParticleParameterInfo() {
    }
    PerParticleParameterInfo(const std::string& name) : name(name) {
    }
};

/**
 * This is an internal class used to record information about a global parameter.
 * @private
 */
class CustomGBForce::GlobalParameterInfo {
public:
    std::string name;
    double defaultValue;
    GlobalParameterInfo() {
    }
    GlobalParameterInfo(const std::string& name, double defaultValue) : name(name), defaultValue(defaultValue) {
    }
};

/**
 * This is an internal class used to record information about an exclusion.
 * @private
 */
class CustomGBForce::ExclusionInfo {
public:
    int particle1, particle2;
    ExclusionInfo() {
        particle1 = particle2 = -1;
    }
    ExclusionInfo(int particle1, int particle2) :
        particle1(particle1), particle2(particle2) {
    }
};

/**
 * This is an internal class used to record information about a tabulated function.
 * @private
 */
class CustomGBForce::FunctionInfo {
public:
    std::string name;
    std::vector<double> values;
    double min, max;
    bool interpolating;
    FunctionInfo() {
    }
    FunctionInfo(const std::string& name, const std::vector<double>& values, double min, double max, bool interpolating) :
        name(name), values(values), min(min), max(max), interpolating(interpolating) {
    }
};

/**
 * This is an internal class used to record information about a computed value or energy term.
 * @private
 */
class CustomGBForce::ComputationInfo {
public:
    std::string name;
    std::string expression;
    CustomGBForce::ComputationType type;
    ComputationInfo() {
    }
    ComputationInfo(const std::string& name, const std::string& expression, CustomGBForce::ComputationType type) :
        name(name), expression(expression), type(type) {
    }
};

} // namespace OpenMM

#endif /*OPENMM_CUSTOMGBFORCE_H_*/
