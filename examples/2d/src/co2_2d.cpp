/* -*- mode: c++; tab-width: 2; indent-tabs-mode: t; truncate-lines: t -*- */
/* vim: set filetype=cpp autoindent tabstop=2 shiftwidth=2 noexpandtab softtabstop=2 nowrap: */
#include <opm/core/utility/parameters/ParameterGroup.hpp>
#include <opm/core/io/eclipse/EclipseGridParser.hpp>
#include <opm/core/grid/GridManager.hpp>
#include <opm/core/props/IncompPropertiesFromDeck.hpp>
#include <opm/core/simulator/initState.hpp>
#include <opm/core/simulator/TwophaseState.hpp>
#include <opm/core/wells/WellsManager.hpp>
#include <opm/core/simulator/WellState.hpp>
#include <opm/core/pressure/FlowBCManager.hpp>
#include <opm/core/simulator/SimulatorTimer.hpp>
#include <opm/core/linalg/LinearSolverFactory.hpp>
#include <opm/core/simulator/SimulatorIncompTwophase.hpp>
#include <opm/core/simulator/SimulatorReport.hpp>
#include <opm/verteq/verteq.hpp>

#include <boost/scoped_ptr.hpp>

#include <iostream>
#include <vector>

using namespace Opm;
using namespace Opm::parameter;
using namespace boost;
using namespace std;

int main (int argc, char *argv[]) {
	// read parameters from command-line
	ParameterGroup param (argc, argv, false);

	// parse keywords from the input file specified
	// TODO: requirement that file exists
	const string filename = param.get <string> ("filename");
	cout << "Reading deck: " << filename << endl;
	const EclipseGridParser parser (filename);
	const string title = parser.getTITLE ().name ();

	// extract grid from the parse tree
	const GridManager gridMan (parser);
	const UnstructuredGrid& fine_grid = *gridMan.c_grid ();

	// extract fluid, rock and two-phase properties from the parse tree
	IncompPropertiesFromDeck fine_fluid (parser, fine_grid);

	// initial state of the reservoir
	const double gravity [] = { 0., 0., Opm::unit::gravity };
	TwophaseState state;
	initStateFromDeck (fine_grid, fine_fluid, parser, gravity [3], state);

	// setup wells from input, using grid and rock properties read earlier
	WellsManager wells (parser, fine_grid, fine_fluid.permeability());
	WellState wellState; wellState.init (wells.c_wells(), state);

	// upscale to a top surface
	scoped_ptr <VertEq> ve (VertEq::create (title,
																					param,
																					fine_grid,
																					fine_fluid));
	const UnstructuredGrid& grid = ve->grid ();
	const IncompPropertiesInterface& fluid = ve->props ();

	// no sources and no-flow boundary conditions
	vector <double> src (grid.number_of_cells, 0.);
	FlowBCManager bc;

	// run schedule
	SimulatorTimer stepping;
	stepping.init (parser);

	// pressure and transport solvers
	LinearSolverFactory linsolver (param);
	SimulatorIncompTwophase sim (param, grid, fluid, 0, wells,
	                             src, bc.c_bcs(), linsolver, gravity);

	// if some parameters were unused, it may be that they're spelled wrong
	if (param.anyUnused ()) {
		cerr << "Unused parameters:" << endl;
		param.displayUsage ();
	}

	// loop solvers until final time has arrived
	sim.run (stepping, state, wellState);

	// done
	return 0;
}
