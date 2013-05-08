#ifndef OPM_VERTEQ_MAPPING_HPP_INCLUDED
#define OPM_VERTEQ_MAPPING_HPP_INCLUDED

// Copyright (C) 2013 Uni Research AS
// This file is licensed under the GNU General Public License v3.0

#include <vector>

// forward declarations
struct UnstructuredGrid;

namespace Opm {

// forward declaration
struct TopSurf;

struct UpscaleMapping {
	/**
	 * Number of dimensions (in coarse grid)
	 */
	const int num_dims;

	/**
	 * Number of columns (elements in coarse grid)
	 */
	const int num_cols;

	/**
	 * Number of fine elements
	 */
	const int num_elems;

	/**
	 * Initialize structure necessary to map from elements at the fine
	 * scale to columns on the coarse scale.
	 *
	 * @param fg Fine grid, three-dimensional.
	 * @param cg Coarse grid, two-dimensional. This must be a top surface
	 * of the fine grid.
	 */
	UpscaleMapping (const UnstructuredGrid& fg,
                    const TopSurf& cg);

	/**
	 * Create volume-weighted average of a property in the fine grid.
	 *
	 * This is used to create a coarse-scale version of the property
	 * in the upscaled top-surface grid.
	 *
	 * @param fine_data Input data; one item per 3D element.
	 * @param col_data Output data; one item per 2D element. Note: Must
	 * be preallocated to the correct size.
	 * @param offset Index of the first element to be processed.
	 * @param stride Number of items (not bytes) between each output.
	 */
	void vol_avg (const double* fine_data, int fine_ofs, int fine_stride,
                  double* col_data, int col_ofs, int col_stride) const;
protected:
	/**
	 * Accumulated volume from top of column and down to including this
	 * fine grid element.
	 *
	 * This has the same format as the col_cells member of the top surface,
	 * i.e. one value for each fine element, elements in the same column are
	 * located together.
	 *
	 * Valid after gather_vol_stat () has been called.
	 *
	 * @see TopSurf::col_cells
	 */
	std::vector <double> acc_vol;

	/**
	 * Total volume in each column.
	 *
	 * This has the same format as cell_volumes in the fine grid, but
	 * that member only has the *area* of the top surface (since the
	 * domain is now 2D); this array has the *real* volumes.
	 *
	 * Valid after gather_vol_stat () has been called.
	 */
	std::vector <double> tot_vol;

	/**
	 * Get the grid information from here
	 */
	const UnstructuredGrid& fine_grid;
	const TopSurf& coarse_grid;
};

} // namespace Opm

#endif // OPM_VERTEQ_MAPPING_HPP_INCLUDED
