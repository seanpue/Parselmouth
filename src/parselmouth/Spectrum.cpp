/*
 * Copyright (C) 2017  Yannick Jadoul
 *
 * This file is part of Parselmouth.
 *
 * Parselmouth is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Parselmouth is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Parselmouth.  If not, see <http://www.gnu.org/licenses/>
 */

#include "Parselmouth.h"

#include "utils/SignatureCast.h"

#include "dwtools/Spectrum_extensions.h"
#include "fon/Spectrum.h"
#include "fon/Spectrum_and_Spectrogram.h"
#include "fon/Sound_and_Spectrum.h"

#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace py::literals;

namespace parselmouth {

using std::experimental::optional;
using std::experimental::nullopt;

void Binding<Spectrum>::init() {
	using signature_cast_placeholder::_;

	def("__init__",
	    [](py::handle self, py::array_t<double, 0> values, Positive<double> maximumFrequency) {
		    auto ndim = values.ndim();
		    if (ndim > 2) {
			    throw py::value_error("Cannot create Spectrum from an array with more than 2 dimensions");
		    }
		    if (ndim == 2 && values.shape(1) > 2) {
			    throw py::value_error("Cannot create Spectrum from 2D array where the second dimension is greater than 2");
		    }

		    auto n = values.shape(0);
		    auto result = Spectrum_create(maximumFrequency, n);

		    if (ndim == 2) {
			    auto unchecked = values.unchecked<2>();
			    for (ssize_t i = 0; i < n; ++i) {
				    result->z[1][i + 1] = unchecked(i, 0);
				    result->z[2][i + 1] = (values.shape(1) == 2) ? unchecked(i, 1) : 0.0;
			    }
		    }
		    else {
			    auto unchecked = values.unchecked<1>();
			    for (ssize_t i = 0; i < n; ++i) {
				    result->z[1][i + 1] = unchecked(i);
				    result->z[2][i + 1] = 0;
			    }
		    }

		    constructInstanceHolder<Binding<Spectrum>>(self, std::move(result));
	    },
	    "values"_a, "maximum_frequency"_a);

	def("__init__",
	    [](py::handle self, py::array_t<std::complex<double>, 0> values, Positive<double> maximumFrequency) {
		    auto ndim = values.ndim();
		    if (ndim > 1) {
			    throw py::value_error("Cannot create Spectrum from a complex array with more than 1 dimension");
		    }

		    auto n = values.shape(0);
		    auto result = Spectrum_create(maximumFrequency, n);

		    auto unchecked = values.unchecked<1>();
		    for (ssize_t i = 0; i < n; ++i) {
			    result->z[1][i + 1] = unchecked(i).real();
			    result->z[2][i + 1] = unchecked(i).imag();
		    }

		    constructInstanceHolder<Binding<Spectrum>>(self, std::move(result));
	    },
	    "values"_a, "maximum_frequency"_a);

	// TODO Constructor from Sound?

	// TODO Tabulate - List ?

	def("get_lowest_frequency",
	    [](Spectrum self) { return self->xmin; });

	def_readonly("lowest_frequency",
	             static_cast<double structSpectrum::*>(&structSpectrum::xmin)); // TODO Remove when not required by pybind11 anymore

	def_readonly("fmin",
	             static_cast<double structSpectrum::*>(&structSpectrum::xmin)); // TODO Remove when not required by pybind11 anymore

	def("get_highest_frequency",
	    [](Spectrum self) { return self->xmax; });

	def_readonly("highest_frequency",
	             static_cast<double structSpectrum::*>(&structSpectrum::xmax)); // TODO Remove when not required by pybind11 anymore

	def_readonly("fmax",
	             static_cast<double structSpectrum::*>(&structSpectrum::xmax)); // TODO Remove when not required by pybind11 anymore

	def("get_number_of_bins",
	    [](Spectrum self) { return self->nx; });

	def_readonly("num_bins",
	             static_cast<int32 structSpectrum::*>(&structSpectrum::nx)); // TODO Remove when not required by pybind11 anymore

	def_readonly("nf",
	             static_cast<int32 structSpectrum::*>(&structSpectrum::nx)); // TODO Remove when not required by pybind11 anymore

	def("get_bin_width",
	    [](Spectrum self) { return self->dx; });

	def_readonly("bin_width",
	             static_cast<double structSpectrum::*>(&structSpectrum::dx)); // TODO Remove when not required by pybind11 anymore

	def_readonly("df",
	             static_cast<double structSpectrum::*>(&structSpectrum::dx)); // TODO Remove when not required by pybind11 anymore

	def("get_frequency_from_bin_number", // TODO Somehow link with "Sampled" class?
	    [](Spectrum self, Positive<long> bandNumber) { return Sampled_indexToX(self, bandNumber); },
	    "band_number"_a);

	def("get_bin_number_from_frequency", // TODO Somehow link with "Sampled" class?
	    [](Spectrum self, double frequency) { return Sampled_xToIndex(self, frequency); },
	    "frequency"_a);

	def("get_real_value_in_bin",
	    [](Spectrum self, Positive<long> binNumber) {
		    if (binNumber > self->nx)
			    Melder_throw (U"Bin number must not exceed number of bins.");
		    return self->z[1][binNumber];
	    },
	    "bin_number"_a);

	def("get_imaginary_value_in_bin",
	    [](Spectrum self, Positive<long> binNumber) {
		    if (binNumber > self->nx)
			    Melder_throw (U"Bin number must not exceed number of bins.");
		    return self->z[2][binNumber];
	    },
	    "bin_number"_a);

	def("get_value_in_bin",
	    [](Spectrum self, Positive<long> binNumber) {
		    if (binNumber > self->nx)
			    Melder_throw (U"Bin number must not exceed number of bins.");
		    return std::complex<double>(self->z[1][binNumber], self->z[2][binNumber]);
	    },
	    "bin_number"_a);

	def("__getitem__",
	    [](Spectrum self, long index) {
		    if (index < 0 || index >= self->nx)
			    throw py::index_error("bin index out of range");
		    return std::complex<double>(self->z[1][index+1], self->z[2][index+1]);
	    },
	    "index"_a);

	def("set_real_value_in_bin",
	    [](Spectrum self, Positive<long> binNumber, double value) {
		    if (binNumber > self->nx)
			    Melder_throw (U"Bin number must not exceed number of bins.");
		    self->z[1][binNumber] = value;
	    },
	    "bin_number"_a, "value"_a);

	def("get_imaginary_value_in_bin",
	    [](Spectrum self, Positive<long> binNumber, double value) {
		    if (binNumber > self->nx)
			    Melder_throw (U"Bin number must not exceed number of bins.");
		    self->z[2][binNumber] = value;
	    },
	    "bin_number"_a, "value"_a);

	def("get_value_in_bin",
	    [](Spectrum self, Positive<long> binNumber, std::complex<double> value) {
		    if (binNumber > self->nx)
			    Melder_throw (U"Bin number must not exceed number of bins.");
		    self->z[1][binNumber] = value.real();
		    self->z[2][binNumber] = value.imag();
	    },
	    "bin_number"_a, "value"_a);

	def("__setitem__",
	    [](Spectrum self, long index, std::complex<double> value) {
		    if (index < 0 || index >= self->nx)
			    throw py::index_error("bin index out of range");
		    self->z[1][index+1] = value.real();
		    self->z[2][index+1] = value.imag();
	    },
	    "index"_a, "value"_a);

	def("get_band_energy",
	    [](Spectrum self, optional<double> bandFloor, optional<double> bandCeiling) { return Spectrum_getBandEnergy(self, bandFloor.value_or(self->xmin), bandCeiling.value_or(self->xmax)); },
	    "band_floor"_a = nullopt, "band_ceiling"_a = nullopt);

	def("get_band_energy",
	    [](Spectrum self, std::pair<optional<double>, optional<double>> band) { return Spectrum_getBandEnergy(self, band.first.value_or(self->xmin), band.second.value_or(self->xmax)); },
	    "band"_a = std::make_pair(nullopt, nullopt));

	def("get_band_density",
	    [](Spectrum self, optional<double> bandFloor, optional<double> bandCeiling) { return Spectrum_getBandDensity(self, bandFloor.value_or(self->xmin), bandCeiling.value_or(self->xmax)); },
	    "band_floor"_a = nullopt, "band_ceiling"_a = nullopt);

	def("get_band_density",
	    [](Spectrum self, std::pair<optional<double>, optional<double>> band) { return Spectrum_getBandDensity(self, band.first.value_or(self->xmin), band.second.value_or(self->xmax)); },
	    "band"_a = std::make_pair(nullopt, nullopt));

	def("get_band_energy_difference",
	    [](Spectrum self, optional<double> lowBandFloor, optional<double> lowBandCeiling, optional<double> highBandFloor, optional<double> highBandCeiling) { return Spectrum_getBandEnergyDifference(self, lowBandFloor.value_or(self->xmin), lowBandCeiling.value_or(self->xmax), highBandFloor.value_or(self->xmin), highBandCeiling.value_or(self->xmax)); },
	    "low_band_floor"_a = nullopt, "low_band_ceiling"_a = nullopt, "high_band_floor"_a = nullopt, "high_band_ceiling"_a = nullopt);

	def("get_band_energy_difference",
	    [](Spectrum self, std::pair<optional<double>, optional<double>> lowBand, std::pair<optional<double>, optional<double>> highBand) { return Spectrum_getBandEnergyDifference(self, lowBand.first.value_or(self->xmin), lowBand.second.value_or(self->xmax), highBand.first.value_or(self->xmin), highBand.second.value_or(self->xmax)); },
	    "low_band"_a = std::make_pair(nullopt, nullopt), "high_band"_a = std::make_pair(nullopt, nullopt));

	def("get_band_density_difference",
	    [](Spectrum self, optional<double> lowBandFloor, optional<double> lowBandCeiling, optional<double> highBandFloor, optional<double> highBandCeiling) { return Spectrum_getBandDensityDifference(self, lowBandFloor.value_or(self->xmin), lowBandCeiling.value_or(self->xmax), highBandFloor.value_or(self->xmin), highBandCeiling.value_or(self->xmax)); },
	    "low_band_floor"_a = nullopt, "low_band_ceiling"_a = nullopt, "high_band_floor"_a = nullopt, "high_band_ceiling"_a = nullopt);

	def("get_band_density_difference",
	    [](Spectrum self, std::pair<optional<double>, optional<double>> lowBand, std::pair<optional<double>, optional<double>> highBand) { return Spectrum_getBandDensityDifference(self, lowBand.first.value_or(self->xmin), lowBand.second.value_or(self->xmax), highBand.first.value_or(self->xmin), highBand.second.value_or(self->xmax)); },
	    "low_band"_a = std::make_pair(nullopt, nullopt), "high_band"_a = std::make_pair(nullopt, nullopt));

	def("get_centre_of_gravity",
	    signature_cast<_ (_, Positive<double>)>(Spectrum_getCentreOfGravity),
	    "power"_a = 2.0);

	def("get_center_of_gravity",
	    signature_cast<_ (_, Positive<double>)>(Spectrum_getCentreOfGravity),
	    "power"_a = 2.0);

	def("get_standard_deviation",
	    signature_cast<_ (_, Positive<double>)>(Spectrum_getStandardDeviation),
	    "power"_a = 2.0);

	def("get_skewness",
	    signature_cast<_ (_, Positive<double>)>(Spectrum_getSkewness),
	    "power"_a = 2.0);

	def("get_kurtosis",
	    signature_cast<_ (_, Positive<double>)>(Spectrum_getKurtosis),
	    "power"_a = 2.0);

	def("get_central_moment",
	    signature_cast<_ (_, Positive<double>, Positive<double>)>(Spectrum_getCentralMoment),
	    "moment"_a, "power"_a = 2.0);

	// TODO Filter Hann bands

	def("cepstral_smoothing",
	    signature_cast<_ (_, Positive<double>)>(Spectrum_cepstralSmoothing),
	    "bandwidth"_a = 500.0);

	def("lpc_smoothing",
	    signature_cast<_ (_, Positive<int>, Positive<double>)>(Spectrum_lpcSmoothing),
	    "num_peaks"_a = 5, "pre_emphasis_from"_a = 50.0);

	def("to_sound",
	    &Spectrum_to_Sound);

	def("to_spectrogram",
	    &Spectrum_to_Spectrogram);

	// TODO More stuff in praat_David_init, for some reason
}

} // namespace parselmouth
