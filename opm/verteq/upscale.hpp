#ifndef OPM_VERTEQ_UPSCALE_HPP_INCLUDED
#define OPM_VERTEQ_UPSCALE_HPP_INCLUDED

// Copyright (C) 2013 Uni Research AS
// This file is licensed under the GNU General Public License v3.0

#include <utility> // pair

#ifndef OPM_VERTEQ_VISIBILITY_HPP_INCLUDED
#include <opm/verteq/visibility.hpp>
#endif /* OPM_VERTEQ_VISIBILITY_HPP_INCLUDED */

#ifndef OPM_VERTEQ_TOPSURF_HPP_INCLUDED
#include <opm/verteq/topsurf.hpp>
#endif /* OPM_VERTEQ_TOPSURF_HPP_INCLUDED */

namespace Opm {

/**
 * Elevation is a discretized version of height.
 *
 * Instead of having the height is absolute units, it is more efficient
 * to store the number of blocks so that we can find the appropriate
 * property with a simple table lookup.
 *
 * Note that an Elevation object will refer to two different heights in
 * two different columns; the top surface is not located at the same z,
 * and the heights of the blocks may be different.
 */
struct Elevation : private std::pair <int, double> {
	/**
	 * Initialize an elevation carrier from its parts.
	 *
	 * @param block Number of whole blocks skipped before the height.
	 *
	 * @param fraction Fractional part of the last block. The invariant
	 *                 0. <= fraction < 1. should be observed.
	 */
	Elevation (int block, double fraction)
	  : std::pair <int, double> (block, fraction) {
	}

	/**
	 * Number of whole blocks that should be skipped before reaching this
	 * height. The height is in the block associated with row number
	 * returned from this function.
	 */
	int block () const { return this->first; }

	/**
	 * Fraction of the block in which the height is. Although the sides of
	 * the block may not be perfectly vertical, we assume that the fraction
	 * was generated by scanning some property and can thus be used to find
	 * the amount to include of another property at the same height.
	 */
	double fraction () const { return this->second; }
};

/**
 * Extension of the top surface that does integration across columns.
 * The extension is done by aggregation since these methods really are
 * orthogonal to how the grid is created.
 */
struct VertEqUpscaler {
	/**
	 * Create a layer on top of a top surface that can upscale properties
	 * in columns.
	 *
	 * @param topSurf Grid that provides the weights in the integrations.
	 */
	VertEqUpscaler (const TopSurf& topSurf)
	  : ts (topSurf) {
	}

	/**
	 * Retrieve a property from the fine grid for a certain column of blocks.
	 *
	 * The property is stored in records of 'stride' length, 'offset' elements
	 * from the start. (This is used to index into a permeability tensor).
	 *
	 * @param col Index of the column to retrieve. This must be a valid cell
	 *            number in the top surface.
	 *
	 * @param buf Array that will receive the data. It is assumed that this
	 *            is preallocated, and is large enough to hold the data for
	 *            this column.
	 *
	 * @param data Array that contains the property for the *entire* fine grid.
	 *             This is typically something that was retrieved from a fluid
	 *             object associated with the fine grid.
	 *
	 * @param stride Number of values (not bytes!) between each entry for the
	 *               property. Use one for simple arrays.
	 *
	 * @param offset Number of values (not bytes!) before the first entry for
	 *               this property. Use zero for simple arrays.
	 */
	void gather (int col, double* buf, const double* data, int stride, int offset) const;

	/**
	 * Depth fraction weighted by an expression specified as piecewise values
	 * downwards a column from the top.
	 *
	 * Returned for each block is the integral from the top down to and
	 * including each block, divided by the total height of the column. The
	 * last value will thus contain the average for the entire column.
	 *
	 * @param col Index of the column for which the expression should be
	 *            integrated.
	 *
	 * @param val Integrand values for each block in the column.
	 *
	 * @param res Array that will receive the result. The caller must
	 *            preallocate storage and pass it as input here. A good idea
	 *            is to use the view of a column from an rlw_double.
	 */
	void wgt_dpt (int col, const double* val, double* res) const;

	/**
	 * Depth-average of a property discretized for each block.
	 *
	 * @param col Index of the column for which values should be averaged.
	 *
	 * @param val Value for each block in the column.
	 *
	 * @return Values weighted by the depth in each block, divided by the
	 *         total depth of the column.
	 */
	double dpt_avg (int col, const double* val) const;

	/**
	 * Sum a property such as a source term down the column.
	 *
	 * Use this method for upscaling things that are specified independent
	 * of the block size (source terms is specified as a volumetric flux
	 * (see computePorevolume in opm/core/utility/miscUtilities.cpp)).
	 *
	 * @param col Index of the column for which values should be summed.
	 *
	 * @param val Value for each block in the *entire* grid, same as the
	 *            data argument to gather(). The reason for using this
	 *            method is to avoid the extra copy.
	 *
	 * @return Sum of values for this block, regardless of depth.
	 */
	double sum (int col, const double* val) const;

	/**
	 * Number of rows in a specified column. Use this method to get the
	 * expected length of arrays containing properties for it. (However, use
	 * ts.max_vert_res to preallocate space!)
	 */
	int num_rows (int col) const;

	/**
	 * Get the elevation of the bottom of a column. This is usable when we
	 * want to get the depth-average for the entire column.
	 */
	Elevation bottom (int col) const;

	/**
	 * Perform table lookup in an array of depth-averaged values.
	 *
	 * Given a height in the form of a discretized elevation, quickly find
	 * the depth-averaged value at this point.
	 *
	 * @param col Column that the elevation is specified for.
	 *
	 * @param dpt Depth fractions weighted with a certain property. This
	 *            should be calculated with wgt_dpt().
	 *
	 * @param zeta The elevation to find. Use the interp() routine to get
	 *             an elevation for an upscaled saturation.
	 *
	 * @return Depth-weighted value of the property from the top down to
	 *         the specified height.
	 */
	double eval (int col, const double* dpt, const Elevation zeta) const;

	/**
	 * Find the elevation where an integrated property has a certain value.
	 *
	 * The method solves the equation
	 *
	 *     \int_{elevation}^{top} property dz = target
	 *
	 * with respect to elevation, where the dpt argument contains
	 * precalculated values for the integral down to and including each
	 * block in the column.
	 *
	 * @param col Index of the column to search, this is the coarse grid
	 *            block number
	 *
	 * @param dpt Depth fractions weighted with a certain property.
	 *            This should be calculated with wgt_dpt().
	 *
	 * @param target Value the integral from top down to the goal height
	 *               should have.
	 *
	 * @return Elevation where the integral specified in dpt down to this
	 *         point becomes the target.
	 */
	Elevation find (int col, const double* dpt, const double target) const;

protected:
	const TopSurf& ts;
};

} // namespace Opm

#endif // OPM_VERTEQ_UPSCALE_HPP_INCLUDED
