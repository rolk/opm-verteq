#include <opm/verteq/io/output.hpp>

#include <string>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif /* __clang__ */
#include <boost/algorithm/string/case_conv.hpp> // to_upper_copy
#include <boost/filesystem.hpp>       // path
#include <opm/core/simulator/SimulatorTimer.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif /* __clang__ */
#include <opm/core/utility/parameters/ParameterGroup.hpp>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif /* __clang__ */
#include <opm/core/io/eclipse/writeECLData.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif /* __clang__ */

using namespace std;
using namespace boost::algorithm;
using namespace boost::filesystem;
using namespace Opm;
using namespace Opm::parameter;

struct EclipseOutputWriter : public OutputWriter {
	string outputDir_;	/// directory name of files
	string baseName_;		/// casename without extension

	/**
	 * Construct an writer that outputs in the same directory as the
	 * input deck.
	 *
	 * @param p Parameters containing information about where the files
	 *          should be created.
	 */
	EclipseOutputWriter (ParameterGroup& p) {
		// get the base name from the name of the deck
		path deck (p.get <string> ("deck_filename"));
		if (to_upper_copy (deck.extension ().string ()) == ".DATA") {
			baseName_ = deck.stem ().string ();
		}
		else {
			baseName_ = deck.filename ().string ();
		}

		// output files in the same directory as the input deck
		outputDir_ = deck.parent_path ().string ();
	}

	/**
	 * Use the existing ERT.ECL library to do the heavy lifting of actually
	 * writing the results.
	 */
	virtual void write (UnstructuredGrid& g, DataMap& d, SimulatorTimer& t) {
		writeECLData (g, d, t.currentStepNum (), t.currentTime (),
		              t.currentDateTime (), outputDir_, baseName_);
	}
};

/**
 * Propagate writing to several output sinks, presenting them as one
 * to the client code.
 */
struct MultiplexOutputWriter : public OutputWriter {
	/// Shorthand for a list of owned OutputWriters
	typedef vector <unique_ptr <OutputWriter> > writers_t;

	/// Owned list of all writers that should be used
	unique_ptr <writers_t> writers_;

	/// Adopt the list of writers (presenting them as a Multiplex)
	MultiplexOutputWriter (unique_ptr <writers_t> writers)
	  : writers_ (std::move (writers)) {
	}

	/// Write to all the writers on the list
	virtual void write (UnstructuredGrid& g, DataMap& d, SimulatorTimer& t) {
		typedef writers_t::iterator it_t;
		for (it_t it = writers_->begin(); it != writers_->end (); ++it) {
			(*it)->write (g, d, t);
		}
	}
};

/**
 * Generic factory: If the keyword supplied to the constructor matches,
 * then create a writer of the parameterized type.
 */
template <typename Writer>
struct FormatFactory : public OutputFormat {
	const char* keyword_;
	FormatFactory (const char* keyword) : keyword_ (keyword) { }

	virtual bool match (ParameterGroup& p) const {
		return p.getDefault (keyword_, false);
	}

	virtual unique_ptr <OutputWriter> create (ParameterGroup& p) const {
		return unique_ptr <OutputWriter> (new Writer (p));
	}
};

// singleton for this format factory; used to instatiate all writers
static FormatFactory <EclipseOutputWriter> ecl_fmt ("output_ecl");
const OutputFormat& eclipseFormat = ecl_fmt;

// list of all formats known in this compilation unit
const vector <const OutputFormat*> OutputFormat::ALL { &ecl_fmt };

unique_ptr <OutputWriter>
OutputWriter::create (
	const vector<const OutputFormat*>& formats,
	parameter::ParameterGroup& p) {

	// construct a new (empty) list of possible writers
	unique_ptr <MultiplexOutputWriter::writers_t> list (
	    new MultiplexOutputWriter::writers_t ());

	// loop through all formats and try to find matches; all matches are
	// put on the list
	typedef vector <const OutputFormat*>::const_iterator fmt_t;
	for (fmt_t it = formats.cbegin (); it != formats.cend (); ++it) {
		if ((*it)->match (p)) {
			list->push_back (unique_ptr <OutputWriter> ((*it)->create (p)));
		}
	}

	// create a multiplex writer of the list (with possible no writers)
	// and return this as a common interface. if there is exactly one
	// writer, then return this directly without any wrapper
	if (1 == list->size ()) {
		return unique_ptr <OutputWriter> (std::move ((*list)[0]));
	}
	else {
		return unique_ptr <OutputWriter> (
		      new MultiplexOutputWriter (std::move (list)));
	}
}
