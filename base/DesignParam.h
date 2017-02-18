/** @file
 * This file contains the base code for the DesignNote Parameter containers
 * and wrapper classes.
 *
 * @author Chris Page &lt;chris@starforge.co.uk&gt;
 *
 */
/* Okay, let's try to get this right this time. The full description of
 * what at design note can include is given in docs/design_note.abnf
 * Some observations from that:
 *
 * - string is the only parameter type that absolutely can never depend on the
 *   qvar rule directly or indirectly.
 *
 * - object is like int, except it also supports names for objects
 *
 * - target only allows for qvar-eq in distance arguments
 *
 * - boolean and time have specific fiddliness involved:
 *   - bool can either be t/f/y/n followed by any arbitrary text, or
 *     an implicitly integer qvar-eq
 *   - time can either be a positive float or int followed by s or m,
 *     or an implicitly integer qvar-eq
 *
 * - the remaining design note parameter types are integer, float, and
 *   floatvec. The integer and float ones are implicit integer or float
 *   qvar-eqs with no special treatment, while the floatvec type is
 *   three comma-separated implicitly float qvar-eq
 *
 * In an ideal world, the scripts should not need to care about any of
 * these issues: they should be able to just fetch the value of a
 * parameter and just get a value of the type appropriate for that
 * parameter. Inevitably, doing this is going to involve creating
 * classes that abstract away the ghastly implementation details for
 * each type of parameter. Hence we probably need a hierarchy of
 * parameter types, with one class per parameter type and a base
 * class that implements any common code.
 *
 * object, boolean, and time are specific sub-types of integer: they
 * all ultimately result in an integer result after some initial
 * type-specific handling (boolean may need to do a one-time translate
 * of t/f/y/n to 1 or 0, time may need to do multiplication of s or m
 * suffixed floats - but as we can't have fractional milliseconds, the
 * *value* of time is always integer milliseconds, and object may need
 * to do a one-time translate from object name to interger ID.
 *
 * target is a sub-type of string that must be able to register a qvar
 * change listener if appropriate. It should return the parameter
 * string as-is if a string is requested, and a float value for the
 * radius (or 0.0) if a float is requested?
 *
 * float-vec is really just three floats wrapped up in something that
 * makes them easily accessible (say overload [] and treat them as
 * arrays of three values?)
 *
 * string, integer and float are, obviously, 'base' types here. What's
 * arguable is whether int should be considered as a sub-tye of float
 * (as any qvar-eq may internally produce a float result).
 * *
 * StringParameter
 * FloatParameter <- TargetParameter?
 *       ^-- IntParameter <- ObjectParameter
 *              ^    ^-- TimeParameter
 *              `-- BooleanParameter
 *
 * with FloatVecParameter off to the side somewhere using three
 * FloatParameters.
 *
 * Note to self: the get_scriptparam_valuefalloff() and get_scriptparam_countmode
 * functions *ARE NOT* suitable for turning into first-order design parameter
 * classes: they are really formed from multiple separate design parameters.
 */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */

#include <string>


/** A base class for design note parameters. This collects the common
 *  code for all design note parameter types, and maintains the
 *  information that all design note parameter types need to keep
 *  track of.
 */
class DesignParam
{
public:
    /** Create a new DesignParam object. Generally this will never be called
     *  directly, but as part of constructing a child of this class.
     *
     * @param script The name of the script the parameter is attached to.
     * @param name   The name of the parameter.
     */
    DesignParam(const std::string& script, const std::string& name) : script_name(script), param_name(name), fullname(script + name), set(false)
        { /* fnord */ }


    /** Ensure that destruction works correctly through inheritance.
     */
    virtual ~DesignParam()
        { /* fnord */ }


    /** Was the value of this parameter set in the design note?
     *
     * @return true if the value of this parameter was set in the design note, false
     *         if it was not. If called before calling init(), this will always
     *         return false.
     */
    const bool is_set(void) const
        { return set; }

protected:
    /** Set whether this design parameter has been set in the design note.
     *
     * @param status true if the parameter has been set in the design note, false otherwise.
     */
    void is_set(const bool status)
        { set = status; }


    /** Fetch the value of the parameter as a string, and store the string in the
     *  provided string reference. Note that this does not modify the 'set' value,
     *  as subclasses may do additional validation on the string that determines
     *  whether a valid value has be set.
     *
     * @param parameter A reference to a string to store the parameter in.
     * @reture true if the parameter was set in the design note, false if it was not
     *         or could not be parsed from it.
     */
    bool get_param_string(const std::string& design_note, std::string& parameter);

private:
    std::string script_name; //!< The name of the script the parameter is for.
    std::string param_name;  //!< The name of parameter this represents.
    std::string fullname;    //!< The full name of the parameter (script_name + param_name)
    bool        set;         //!< Was the value of this parameter set in the design note?
};


/** A design note parameter class to handle string parameters. This implements
 *  the basic operations needed to initialise and fetch the value of a basic
 *  string parameter that will not update after initialisation is completed.
 *
 * @note If the design note is changed after init is complete - say some other
 *       script modifies the string to store data, or change the behaviour
 *       of another script, *this will not pick up the change*. Whether this
 *       is "correct" behaviour is arguable, but it's the one I'm using. If
 *       picking up the change is really needed, the client /could/ call
 *       init() before each get() to get the desired behaviour.
 */
class DesignParamString : public DesignParam
{
public:
    /** Create a new DesignParamString. Note that little or no work can or
     *  should be done by this constructor: the client code should call
     *  init(), passing the design note data to initialise the parameter
     *  with after object creation is complete.
     *
     * @param script The name of the script the parameter is attached to.
     * @param name   The name of the parameter.
     */
    DesignParamString(const std::string& script, const std::string& name) : DesignParam(script, name), data("")
        { /* fnord */ }


    /** Initialise the DesignParamString based on the values specified.
     *
     * @param design_note   A reference to a string containing the design note to parse
     * @param default_value The default value to set for the string. If not specified,
     *                      the empty string is used.
     * @return true on successful init (which may include when no parameter was set
     *         in the design note!), false if init failed.
     */
    bool init(const std::string& design_note, const std::string& default_value = "");


    /** Obtain the value of this design parameter. This will return the current
     *  design parameter value, which may be an empty string if the parameter was not
     *  set in the design note.
     *
     * @return A reference to a string containing the design note parameter value.
     */
    const std::string& value() const
        { return data; }

private:
    std::string data; //!< The current value of the parameter.
};


/**
 */
