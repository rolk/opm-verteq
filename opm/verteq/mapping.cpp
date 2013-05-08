#include <opm/verteq/mapping.hpp>
#include <opm/core/grid.h>
#include <opm/verteq/topsurf.hpp>
#include <boost/foreach.hpp>

using namespace Opm;
using namespace std;

UpscaleMapping::UpscaleMapping (
		const UnstructuredGrid& fg, /* fine grid */
		const TopSurf& cg) /* coarse grid */
	: num_dims (cg.dimensions)
	, num_cols (cg.number_of_cells)
	, num_elems (fg.number_of_cells)
	, fine_grid (fg)
	, coarse_grid (cg) {
	/*
	 * Gather volumetrics which we'll use to integrate fluid properties
	 * along each column.
	 */

	// allocate memory for the running accumulation
	acc_vol.resize (fine_grid.number_of_cells, 0.);

	// allocate memory for the column total
	tot_vol.resize (coarse_grid.number_of_cells, 0.);

	// gather statistics for each column separately
	for (int col = 0; col < coarse_grid.number_of_cells; ++col) {
		// reset the accumulator for each new column
		double running_total = 0.;

		// loop over the fine elements in this column, generating a
		// running accumulation
		BOOST_FOREACH (int fine_elem_ndx, coarse_grid.column (col)) {
			running_total += fine_grid.cell_volumes[fine_elem_ndx];

			// put total *after* we have included the element
			acc_vol[fine_elem_ndx] = running_total;
		}

		// total sum of this column
		tot_vol[col] = running_total;
	}
}

void
UpscaleMapping::vol_avg (
		const double* fine_data, int fine_ofs, int fine_stride,
		double* col_data, int col_ofs, int col_stride) const {
	// volume-weigh the average for every column individually
	for (int col = 0; col < coarse_grid.number_of_cells; ++col) {
		double sum = 0.;
		BOOST_FOREACH (int fine_elem_ndx, coarse_grid.column (col)) {
			const int fine_ndx = fine_elem_ndx * fine_stride + fine_ofs;
			sum += fine_data[fine_ndx] * fine_grid.cell_volumes[fine_ndx];
		}
		const int coarse_ndx = col * col_stride + col_ofs;
		col_data[coarse_ndx] = sum / tot_vol[coarse_ndx];
	}
}
