#include <opm/verteq/props.hpp>
#include <opm/verteq/utility/exc.hpp>
#include <opm/core/grid.h>
#include <opm/verteq/mapping.hpp>
#include <opm/verteq/topsurf.hpp>
#include <boost/foreach.hpp>
#include <memory> // auto_ptr
#include <vector>
using namespace Opm;
using namespace boost;
using namespace std;

struct VertEqPropsImpl : public VertEqProps {
	/**
	 * Get the grid information from here
	 */
	const UpscaleMapping& mapping;
	const IncompPropertiesInterface& fine_props;

	// it is surprisingly hard to do a strided copy using regular STL
	template <typename T>
	void strided_copy (T* src, int count, T* dst, int stride) {
		for (int i = 0; i < count; ++i, src += stride, dst += stride) {
			*dst = *src;
		}
	}

	/**
	 * Upscaled petrophysical rock properties. We cannot use valarray
	 * for these because that class does not allow us to directly return
	 * the data.
	 */
	vector <double> poro;
	vector <double> absperm;

	// constant to avoid a bunch of "magic" constants in the code
	static const int TWO_DIMS = 2;
	static const int THREE_DIMS = 3;

	VertEqPropsImpl (const UpscaleMapping& m,
									 const IncompPropertiesInterface& fp)
		: mapping (m)
		, fine_props (fp) {

		// allocate memory. the permeability matrix is multi-valued per element
		poro.resize (mapping.num_cols);
		const int PERM_MATRIX_2D = TWO_DIMS * TWO_DIMS;
		const int PERM_MATRIX_3D = THREE_DIMS * THREE_DIMS;
		absperm.resize (mapping.num_cols * PERM_MATRIX_2D);

		// average the porosity
		const double* fine_poro = fp.porosity ();
		mapping.vol_avg (fine_poro, 0, 1, poro.data (), 0, 1);

		// offsets when indexing into the permeability matrix
		const int kxx_ofs_3d = 0 * THREE_DIMS + 0; // (x, x), x = 0
		const int kxy_ofs_3d = 0 * THREE_DIMS + 1; // (x, y), x = 0, y = 1
		const int kyy_ofs_3d = 1 * THREE_DIMS + 1; // (y, y), y = 1

		const int kxx_ofs_2d = 0 * TWO_DIMS + 0; // (x, x), x = 0
		const int kxy_ofs_2d = 0 * TWO_DIMS + 1; // (x, y), x = 0, y = 1
		const int kyy_ofs_2d = 1 * TWO_DIMS + 1; // (y, y), y = 1

		// average each element of the permeability matrix by itself, but
		// put everything into one common block of memory
		const double* fine_perm = fp.permeability ();
		mapping.vol_avg (fine_perm, kxx_ofs_3d, PERM_MATRIX_3D,
										 absperm.data (), kxx_ofs_2d, PERM_MATRIX_2D);
		mapping.vol_avg (fine_perm, kxy_ofs_3d, PERM_MATRIX_3D,
										 absperm.data (), kxy_ofs_2d, PERM_MATRIX_2D);
		mapping.vol_avg (fine_perm, kyy_ofs_3d, PERM_MATRIX_3D,
										 absperm.data (), kyy_ofs_2d, PERM_MATRIX_2D);

		// we'll need to provide this element as well, in case someone is
		// using it. fill it in by copying data from the mirror element.
		const int kyx_ofs_2d = 1 * TWO_DIMS + 0; // (y, x), x = 0, y = 1
		strided_copy (absperm.data () + kxy_ofs_2d, mapping.num_cols,
									absperm.data () + kyx_ofs_2d, TWO_DIMS);
	}

	/* rock properties; use volume-weighted averages */
	virtual int numDimensions () const {
		return mapping.num_dims;
	}

	virtual int numCells () const {
		return mapping.num_cols;
	}

	virtual const double* porosity () const {
		return poro.data ();
	}

	virtual const double* permeability () const {
		return absperm.data ();
	}

	/* fluid properties; these don't change when upscaling */
	virtual int numPhases () const {
		return fine_props.numPhases ();
	}

	virtual const double* viscosity () const {
		return fine_props.viscosity ();
	}

	virtual const double* density () const {
		return fine_props.density ();
	}

	virtual const double* surfaceDensity () const {
		return fine_props.surfaceDensity ();
	}

	/* hydrological (unsaturated zone) properties */
	virtual void relperm (const int n,
												const double *s,
												const int *cells,
												double *kr,
												double *dkrds) const {
		throw OPM_EXC ("Not implemented yet");
	}
	virtual void capPress (const int n,
												 const double *s,
												 const int *cells,
												 double *pc,
												 double *dpcds) const {
		throw OPM_EXC ("Not implemented yet");
	}
	virtual void satRange (const int n,
												 const int *cells,
												 double *smin,
												 double *smax) const {
		throw OPM_EXC ("Not implemented yet");
	}
};

VertEqProps*
VertEqProps::create (const UpscaleMapping& mapping,
										 const IncompPropertiesInterface& fineProps) {
	// construct real object which contains all the implementation details
	auto_ptr <VertEqProps> props (new VertEqPropsImpl (mapping,
																										 fineProps));

	// client owns pointer to constructed fluid object from this point
	return props.release ();
}
