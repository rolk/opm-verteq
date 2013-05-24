#include <opm/verteq/mapping.hpp>
#include <opm/verteq/utility/exc.hpp>
#include <opm/core/grid.h>
#include <opm/verteq/topsurf.hpp>
#include <boost/foreach.hpp>

using namespace Opm;
using namespace std;

int
UpscaleMapping::find_face (int glob_elem_id, const Side3D& s) {
    // this is the tag we are looking for
    const int target_tag = s.facetag ();

    // this is the matrix we are looking in
    RunLenView <int> cell_facetag (
                fine_grid.number_of_cells,
                fine_grid.cell_facepos,
                fine_grid.cell_facetag);

    // we are returning values from this matrix
    RunLenView <int> cell_faces (
                fine_grid.number_of_cells,
                fine_grid.cell_facepos,
                fine_grid.cell_faces);


    // loop through all faces for this element; face_ndx is the local
    // index amongst faces for just this one element.
    for (int local_face = 0;
         local_face < cell_facetag.size (glob_elem_id);
         ++local_face) {

        // if we found a match, then return this; don't look any more
        if (cell_facetag[glob_elem_id][local_face] == target_tag) {

            // return the (global) index of the face, not the tag!
            return cell_faces[glob_elem_id][local_face];
        }
    }

    // in a structured grid we expect to find every face
    throw OPM_EXC ("Element %d does not have face #%d", glob_elem_id, target_tag);
}

double
UpscaleMapping::find_height (int glob_elem_id) {

    // find which face is the top and which is the bottom for this element
    const int up_face = find_face (glob_elem_id, UP);
    const int down_face = find_face (glob_elem_id, DOWN);

    // get the z-coordinate for each of them
    const double up_z =
            fine_grid.face_centroids[up_face * Dim3D::COUNT + Dim3D::Z.val];
    const double down_z =
            fine_grid.face_centroids[down_face * Dim3D::COUNT + Dim3D::Z.val];

    // the side that is down should have the z coordinate with highest magnitude
    const double height = down_z - up_z;
    return height;
}

UpscaleMapping::UpscaleMapping (
		const UnstructuredGrid& fg, /* fine grid */
		const TopSurf& cg) /* coarse grid */
	: num_dims (cg.dimensions)
	, num_cols (cg.number_of_cells)
	, num_elems (fg.number_of_cells)
    , blk_id (cg.number_of_cells, cg.col_cellpos, cg.col_cells)
    , hgt (cg.number_of_cells, cg.col_cellpos)
    , acc_hgt (cg.number_of_cells, cg.col_cellpos)
	, fine_grid (fg)
	, coarse_grid (cg) {
	/*
	 * Gather volumetrics which we'll use to integrate fluid properties
	 * along each column.
	 */

    // find the height of each and every fine element; store this and
    // accumulated height
    for (int col = 0; col < blk_id.cols (); ++col) {
        double accum = 0.;
        for (int col_elem = 0; col_elem < blk_id.size (col); ++col_elem) {
            hgt[col][col_elem] = accum += find_height (blk_id[col][col_elem]);
            acc_hgt[col][col_elem] = accum;
        }
    }

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
