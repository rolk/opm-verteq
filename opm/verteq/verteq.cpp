// Copyright (C) 2013 Uni Research AS
// This file is licensed under the GNU General Public License v3.0
#include <opm/verteq/mapping.hpp>
#include <opm/verteq/props.hpp>
#include <opm/verteq/topsurf.hpp>
#include <opm/verteq/verteq.hpp>
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
						 const IncompPropertiesInterface& fullProps);
	virtual const UnstructuredGrid& grid();
	virtual const IncompPropertiesInterface& props();

	auto_ptr <TopSurf> ts;
	auto_ptr <UpscaleMapping> map;
	auto_ptr <VertEqProps> fl;
};

VertEq*
VertEq::create (const string& title,
								const ParameterGroup& args,
								const UnstructuredGrid& fullGrid,
								const IncompPropertiesInterface& fullProps) {
	// we don't provide any parameters to do tuning yet
	auto_ptr <VertEqImpl> impl (new VertEqImpl ());
	impl->init (fullGrid, fullProps);
	return impl.release();
}

void
VertEqImpl::init(const UnstructuredGrid& fullGrid,
								 const IncompPropertiesInterface& fullProps) {
	// generate a two-dimensional upscaling as soon as we get the grid
	ts = auto_ptr <TopSurf> (TopSurf::create (fullGrid));
	map = auto_ptr <UpscaleMapping> (new UpscaleMapping (fullGrid, *ts));
	fl = auto_ptr <VertEqProps> (VertEqProps::create (*map, fullProps));
}

const UnstructuredGrid&
VertEqImpl::grid () {
	// simply return the standard part of the grid
	return *(ts.get ());
}

const IncompPropertiesInterface&
VertEqImpl::props () {
	// simply return the standard part of the grid
	return *(fl.get ());
}
