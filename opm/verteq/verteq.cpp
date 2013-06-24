// Copyright (C) 2013 Uni Research AS
// This file is licensed under the GNU General Public License v3.0
#include <opm/verteq/props.hpp>
#include <opm/verteq/topsurf.hpp>
#include <opm/verteq/verteq.hpp>
#include <opm/core/simulator/TwophaseState.hpp>
#include <opm/core/utility/parameters/ParameterGroup.hpp>
#include <memory>           // auto_ptr

using namespace Opm;
using namespace Opm::parameter;
using namespace std;

// Actual implementation of the upscaling
struct VertEqImpl : public VertEq {
	// public methods defined in the interface
	virtual ~VertEqImpl () {}
	void init (const UnstructuredGrid& fullGrid,
	           const IncompPropertiesInterface& fullProps,
	           const double* gravity);
	virtual const UnstructuredGrid& grid();
	virtual const IncompPropertiesInterface& props();
	virtual void upscale (const TwophaseState& fineScale,
	                      TwophaseState& coarseScale);
	virtual void notify (const TwophaseState& coarseScale);

	auto_ptr <TopSurf> ts;
	auto_ptr <VertEqProps> pr;
};

VertEq*
VertEq::create (const string& title,
                const ParameterGroup& args,
                const UnstructuredGrid& fullGrid,
                const IncompPropertiesInterface& fullProps,
                const double* gravity) {
	// we don't provide any parameters to do tuning yet
	auto_ptr <VertEqImpl> impl (new VertEqImpl ());
	impl->init (fullGrid, fullProps, gravity);
	return impl.release();
}

void
VertEqImpl::init(const UnstructuredGrid& fullGrid,
                 const IncompPropertiesInterface& fullProps,
                 const double* gravity) {
	// generate a two-dimensional upscaling as soon as we get the grid
	ts = auto_ptr <TopSurf> (TopSurf::create (fullGrid));
	pr = auto_ptr <VertEqProps> (VertEqProps::create (fullProps, *ts, gravity));
}

const UnstructuredGrid&
VertEqImpl::grid () {
	// simply return the standard part of the grid
	return *(ts.get ());
}

const IncompPropertiesInterface&
VertEqImpl::props () {
	// simply return the standard part of the grid
	return *(pr.get ());
}

void
VertEqImpl::upscale (const TwophaseState& fineScale,
                     TwophaseState& coarseScale) {
	// dimension state object to the top grid
	coarseScale.init (*ts, pr->numPhases ());

	// TODO: set the initial state from the fine-scale state

	// update the properties from the initial state (the
	// simulation object won't call this method before the
	// first timestep; it assumes that the state is initialized
	// accordingly (which is what we do here now)
	notify (coarseScale);
}

void
VertEqImpl::notify (const TwophaseState& coarseScale) {
	// forward this request to the properties we have stored
	pr->upd_res_sat (&coarseScale.saturation()[0]);
}
