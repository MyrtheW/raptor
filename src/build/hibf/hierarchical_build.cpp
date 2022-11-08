// --------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2022, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2022, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/raptor/blob/main/LICENSE.md
// --------------------------------------------------------------------------------------------------

#include <lemon/list_graph.h> /// Must be first include.

#include <raptor/build/hibf/compute_kmers.hpp>
#include <raptor/build/hibf/construct_ibf.hpp>
#include <raptor/build/hibf/hierarchical_build.hpp>
#include <raptor/build/hibf/initialise_max_bin_kmers.hpp>
#include <raptor/build/hibf/insert_into_ibf.hpp>
#include <raptor/build/hibf/loop_over_children.hpp>
#include <raptor/build/hibf/update_user_bins.hpp>

namespace raptor::hibf
{

template <seqan3::data_layout data_layout_mode>
size_t hierarchical_build(robin_hood::unordered_flat_set<size_t> & parent_kmers,
                          lemon::ListDigraph::Node const & current_node,
                          build_data<data_layout_mode> & data,
                          build_arguments const & arguments,
                          bool is_root)
{
    auto & current_node_data = data.node_map[current_node];

    size_t const ibf_pos{data.request_ibf_idx()};

    std::vector<int64_t> ibf_positions(current_node_data.number_of_technical_bins, ibf_pos);
    std::vector<int64_t> filename_indices(current_node_data.number_of_technical_bins, -1);
    robin_hood::unordered_flat_set<size_t> kmers{};


    //myrthe: loop here
            // Instead I could use the estimated file sizes, i.e.:  size_t const kmers_per_bin{record.estimated_sizes[0]};
        // this would be an option, but then I would need  to adapt parse_chopper_pack_line and saving the layout, because this information is currently not stored. Perhaps I could also store it in the filename.
        // now there could exist a case where all BFs in the IBF are empty.
    for (; current_node_data.max_bin_index < ibf_positions.size(); current_node_data.max_bin_index++){
        // or perhaps set node.max_bin_index +=1, but I might be mininterpreting it: is max_bin_index the index of the maximum sized bin in the IBF, or is it the index of the IBF in the HIBF, which seems from "ibf_positions[node_data.max_bin_index]", but std::vector<int64_t> ibf_positions(current_node_data.number_of_technical_bins, ibf_pos);
        // todo: ask Svenja: what is ibf_positions?
        // it could be that remaining records and bin_indices are not synchonized.
        if ( current_node_data.remaining_records[current_node_data.max_bin_index].filenames[0] != "empty_bin" //current_node.record.filenames[current_node_data.max_bin_index] != "empty_bin"
        ){ // also add a case for if all BFs are empty bins?

            // initialize lower level IBF
            size_t const max_bin_tbs =
            initialise_max_bin_kmers(kmers, ibf_positions, filename_indices, current_node, data, arguments); // or add max_bin_index
            auto && ibf = construct_ibf(parent_kmers, kmers, max_bin_tbs, current_node, data, arguments, is_root);
            kmers.clear(); // reduce memory peak

            // We assume that max_bin_index corresponds to the first entry in remaining records. Can we also assume that the remaining records are further sorted on size, and based on in what ibf they should go?
            // if yes
            // you should increase max_bin_index by  //max_bin_index += .remaining_records[current_node_data.max_bin_index].
            // if no: option 1 )if max bin index == empty bin, then loop over remaining records to find a non empty bin by:( can also be done in initilialize_max_bin..
                // update max bin index
                // remove first record
//                for record_idx=0; record_idx < remaining_records.size(); record_idx
            // but again this only works if remaining_records are sorted.

            // option 2) save the approximate #kmers --> change layout and change construct_ibf
           // option 3) move empty IBFs below the max_bin; i.e. make sure that the max_bin_id in chopper layout is not pointed to an empty bin . Probably not the easiest, because this comes out of the layout production...
            // all of above options have the issue that you lose space if the empty bin(s) were supposed to be larger. To solve this, you should store hom much larger the value is then the next IBF.


    // parse all other children (merged bins) of the current ibf
    loop_over_children(parent_kmers, ibf, ibf_positions, current_node, data, arguments, is_root);

    // If max bin was a merged bin, process all remaining records, otherwise the first one has already been processed
    size_t const start{(current_node_data.favourite_child != lemon::INVALID) ? 0u : 1u};
    for (size_t i = start; i < current_node_data.remaining_records.size(); ++i)
    {
        auto const & record = current_node_data.remaining_records[i];
        if (record.filenames[0] != "empty_bin"){ // only the first entry of filenames stores something, hence the [0]
            if (is_root && record.number_of_bins.back() == 1) // no splitting needed
            {
                insert_into_ibf(arguments, record, ibf);
            }
            else
            {
                compute_kmers(kmers, arguments, record);
                insert_into_ibf(parent_kmers, kmers, record.number_of_bins.back(), record.bin_indices.back(), ibf, is_root);
            }

            update_user_bins(data, filename_indices, record); //this would also not be needed for empty bins in my opinion.
            kmers.clear();
        }
    }

    data.hibf.ibf_vector[ibf_pos] = std::move(ibf);
    data.hibf.next_ibf_id[ibf_pos] = std::move(ibf_positions);
    data.hibf.user_bins.bin_indices_of_ibf(ibf_pos) = std::move(filename_indices);

    break;
        }
    }
    return ibf_pos;
}

template size_t hierarchical_build<seqan3::data_layout::uncompressed>(robin_hood::unordered_flat_set<size_t> &,
                                                                      lemon::ListDigraph::Node const &,
                                                                      build_data<seqan3::data_layout::uncompressed> &,
                                                                      build_arguments const &,
                                                                      bool);

template size_t hierarchical_build<seqan3::data_layout::compressed>(robin_hood::unordered_flat_set<size_t> &,
                                                                    lemon::ListDigraph::Node const &,
                                                                    build_data<seqan3::data_layout::compressed> &,
                                                                    build_arguments const &,
                                                                    bool);

} // namespace raptor::hibf
