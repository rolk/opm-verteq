#ifndef OPM_VERTEQ_OUTPUT_HPP_INCLUDED
#define OPM_VERTEQ_OUTPUT_HPP_INCLUDED

// Copyright (C) 2013 Uni Research AS
// This file is licensed under the GNU General Public License v3.0

#include <functional> // function
#include <memory>  // unique_ptr
#include <vector>

#ifndef OPM_VERTEQ_VISIBILITY_HPP_INCLUDED
#include <opm/verteq/visibility.hpp>
#endif /* OPM_VERTEQ_VISIBILITY_HPP_INCLUDED */

// DataMap is a typedef, so we need the header; no forward decl.
#ifndef OPM_DATAMAP_HEADER_INCLUDED
#include <opm/core/utility/DataMap.hpp>
#endif /* OPM_DATAMAP_HEADER_INCLUDED */

// forward declaration
class UnstructuredGrid;

namespace Opm {

// forward declarations from opm-core
class SimulatorTimer;
class TwophaseState;

namespace parameter {
class ParameterGroup;
}

// forward declaration due to mutual relationship
struct OutputFormat;

/**
 * Interface for writing simulation state to disk.
 */
struct OPM_VERTEQ_PUBLIC OutputWriter {
	/**
	 * Write the simulation state into the preconfigured location.
	 *
	 * @param g Geometry of the grid which the data is applicable to.
	 *          It is always assumed that the block numbers in the data
	 *          corresponds to block numbers given here.
	 *
	 * @param d State data. Currently only saturation and pressure is
	 *          written.
	 *
	 * @param t Current time information; describes how far the
	 *          simulation has progressed. Only timesteps marked for
	 *          reporting may be written.
	 */
	virtual void write (UnstructuredGrid& g,
	                      DataMap& d,
	                      SimulatorTimer& t) = 0;

	/**
	 * Create an output writer which fits the suggestions from the user,
	 * passed through parameters.
	 *
	 * @param formats List of all eligible formats, in prioritized order. This
	 *                can be OutputFormat::ALL, or a custom list.
	 *
	 * @param p User-supplied parameters which configure the output.
	 *
	 * @return Object which can be used to write simulation results in the
	 *         format(s) desired by the user. More than one format may be
	 *         used (it will be multiplexed automatically).
	 *
	 * @example
	 * @code{.cpp}
	 *  ParameterGroup p (argc, argv, false);
	 *	unique_ptr <OutputWriter> w = OutputWriter::create (OutputFormat.ALL, p);
	 *	w->write (g, d, t);
	 * @endcode
	 */
	static std::unique_ptr <OutputWriter>
	create (const std::vector<const OutputFormat*>& formats,
	        parameter::ParameterGroup& p);
};

/**
 * Interface for state writing formats.
 */
struct OPM_VERTEQ_PUBLIC OutputFormat {
	/// List of all known (to this library) output parameters
	static const std::vector <const OutputFormat*> ALL;

	/// Test whether the parameters indicate that this format should be used
	virtual bool match (parameter::ParameterGroup& p) const = 0;

	/// Create a new writer to dump files based on locations in the parameters
	virtual std::unique_ptr <OutputWriter> create (parameter::ParameterGroup& p) const = 0;
};

/**
 * Encapsulate output writing from simulators. This is an object so
 * that it can hold curried arguments.
 *
 * @example
 * @code{.cpp}
 *	// state ends up here
 *	TwophaseState state;
 *
 *	// timestep ends up here
 *	SimulatorTimer timer;
 *
 *	// set up simulation
 *	SimulatorIncompTwophase sim (param, grid, ... );
 *
 *	// use this to dump state to disk
 *	SimulationOutputter output (param, grid, timer, state);
 *
 *	// connect simulation with output writer
 *	sim.timestep_completed ().add (output);
 *
 *	// start simulation
 *	sim.run (timer, state, ... )
 * @endcode
 *
 * @todo
 * This functionality could be incorporated directly into a simulator
 * object.
 */
class OPM_VERTEQ_PUBLIC SimulationOutputter {
	/**
	 * Curry arguments for the output writer. These arguments are passed
	 * to the simulator, but is not passed on to the event handler so it
	 * need to pick them up from the object members.
	 *
	 * @note
	 * The lifetime of the objects passed must encompass the lifetime of
	 * this object, i.e. assume that this object refer to them at any time.
	 */
	SimulationOutputter (parameter::ParameterGroup& p,
	                     UnstructuredGrid& g,
	                     SimulatorTimer& t,
	                     TwophaseState& s);

	/**
	 * Conversion operator which allows the object to be directly passed
	 * into an Event and used as a handler.
	 *
	 * @see Opm::SimulatorIncompTwophase::timestep_completed
	 */
	operator std::function <void ()> ();

protected:
	/// Just hold a reference to these objects that are owned elsewhere.
	UnstructuredGrid& grid_;
	SimulatorTimer& timer_;

	/// Created locally and destructed together with us
	std::unique_ptr <DataMap> state_;
	std::unique_ptr <OutputWriter> handler_;

	/// Wrap the applicable fields of a state in a data map (used by the writers)
	static std::unique_ptr <DataMap> mapState (const TwophaseState& state);

	/// Call the writers that were created based on the parameters
	virtual void writeOutput ();
};

} /* namespace Opm */

#endif /* OPM_VERTEQ_OUTPUT_HPP_INCLUDED */
