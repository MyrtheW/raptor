//#include <raptor/search/search_single.hpp> // to make sure index_structure::hibf_compresse exists here.
#include <raptor/update/load_hibf.hpp>
#include <raptor/update/insertions.hpp>
#include <raptor/build/store_index.hpp>
#include <chopper/detail_apply_prefix.hpp>
#include <chopper/layout/execute.hpp>
#include <chopper/count/execute.hpp>
#include <chopper/set_up_parser.hpp>
#include <seqan3/test/tmp_filename.hpp>
#include <raptor/update/rebuild.hpp>

#include <raptor/build/hibf/chopper_build.hpp>
#include <raptor/build/hibf/create_ibfs_from_chopper_pack.hpp> // when adding this, it recognizes std as raptor::std

namespace raptor
{

// // METHOD 1
//void split_ibf(size_t ibf_idx,
//                  raptor_index<index_structure::hibf> & index, upgrade_arguments const & arguments)
//{   int number_of_splits = 2;
//    std::vector<std::tuple <uint64_t, uint64_t>> index_tuples;  // or initialize with size {number_of_splits};
//    index_tuples[0] = index.ibf().previous_ibf_id[ibf_idx];     // get indices of the current merged bin on the higher level
//    index.ibf().delete_tbs(std::get<0>(index_tuples[0]), std::get<1>(index_tuples[0]));    // empty it
//
////    for (int split = 0; split < number_of_splits; split++){             // get indices of the empty bins on the higher level to serve as new merged bins.
////            index_tuples[split] = find_empty_bin_idx(index, ibf_idx);   //import from insertions.
////    }
//    // get empty bin(s) on higher level, for number of splits -1
//    std::vector<std::vector<size_t>> split_indices = find_best_split();
//    // create new IBFs
//
//        // OPTION 1: resize the two new IBFs. Calculate new size.
//        // Delete original IBF
//        for (int split = 0; split < number_of_splits; split++){
//            index_tuples[split] = find_empty_bin_idx(index, ibf_idx);   // get indices of the empty bins on the higher level to serve as new merged bins.. import function from insertions.
//            // create new IBF
//            robin_hood::unordered_flat_set<size_t> parent_kmers{};
//            auto && ibf = construct_ibf(parent_kmers, kmers, max_bin_tbs, current_node, data, arguments, false); // for now use current size. perhaps create a new construct_ibf function.
//            // add ibf to ibf_vector
//                //new_ibf_idx  =
//            for (auto bin_idx in split_indices[split];){
//                auto filenames = find_filename(ibf_idx, bin_idx); // get filenames
//                auto number_of_bins = is_split_bin(ibf_idx, bin_idx);
//                kmers  =  // load kmers
//                insert_into_ibf(parent_kmers, kmers, std::make_tuple(new_ibf_idx, bin_idx, number_of_bins), index, false);  // insert in new IBF in
//                //bin_idx ++ number_of_bins
//            }
//            //for each split, also insert 'parent_kmers' in the merged bin at index_tuples[split];
//            insert_into_ibf(parent_kmers, std::make_tuple(std::get<0>(index_tuples[0]), std::get<1>(index_tuples[0]), 1), index);
//        }
//
//
//        // OPTION 2: or only pull them apart?
//        // using bit shifts
//
//
//    //update auxiliiary data structures.
//    //      next_ibf_id
//    //      previous_ibf_id
//    //      occupancy table
//}
//
//std::vector<std::vector<size_t>> find_best_split(ibf_idx, number_of_splits=2){
//    std::vector<std::vector<size_t>> tb_idxs; //array of (number_of_splits) arrays, with bin_idxes
//    // finds best split of an IBF based on kmer counts or
//    // make sure split bins remain together
//    // default/easy: just split in 2.
//    // group/sort based on the occupancy table.
//
//    // can you use a function from chopper for this?
//
//    return (tb_idxs);
//}
//
//
//// METHOD 2
//

/*!\brief Does a partial rebuild for the subtree at index ibf_idx
 * \details The algorithm rebuilds a subtree of the hierarchical interleaved bloom filter completely.
 * (1) It computes the set of filenames belonging to all user bins in the subtree, and stores those in a text file
 * (2) It computes a good layout by calling Choppers's layout algorithm.
 * (3) It rebuilds the subtree by calling hierachical build function.
 * (4) It merges the index of the HIBF of the newly obtained subtree with the original index.
 * \param[in] ibf_idx the location of the subtree in the HIBF
 * \param[in] index the original HIBF
 * \param[in] update_arguments the arguments that were passed with the update that was to be done on the HIBF.
 * \author Myrthe
 */
void partial_rebuild(size_t ibf_idx,
                  raptor_index<index_structure::hibf> & index, update_arguments const & update_arguments)
{
    //1) obtain filenames from all lower bins and kmer counts. Perhaps using occupancy table.
    auto filenames_subtree = index.ibf().filenames_children(ibf_idx);
    //1.2) Store filenames as text file.
    std::string subtree_bin_paths_filename{"subtree_bin_paths.txt"};
    write_filenames(subtree_bin_paths_filename, filenames_subtree);
    chopper::configuration layout_arguments = layout_config(subtree_bin_paths_filename, update_arguments); // create the arguments to run the layout algorithm with.
    //1.3) Possibly obtain the kmer count and store together with the filenames as text file.
    get_kmer_counts(index, filenames_subtree, layout_arguments.count_filename); // this works
    //2) call chopper layout on the stored filenames.
    //call_layout(ibf_idx, index, layout_arguments);
    //3) call hierarchical build.
    raptor_index<index_structure::hibf> subindex{}; //create the empty HIBF of the subtree.
    build_arguments build_arguments = build_config(subtree_bin_paths_filename, update_arguments); // create the arguments to run the build algorithm with.
    call_build(build_arguments, subindex);    //todo test if this works
    //4) merge the index of the HIBF of the newly obtained subtree with the original index.
    merge_indexes(index, subindex, ibf_idx);

}

/*!\brief Store kmer or minimizer counts for each specified file stored within the HIBF.
 * \details The algorithm obtains the total kmer count for each file and stores those counts together with the
 * filenames to a text file.
 * \param[in] filenames a set of filenames of which the counts should be stored.
 * \param[in] index the original HIBF
 * \param[in] count_filename the filename to which the counts should be stored.
 * \author Myrthe
 */
void get_kmer_counts(raptor_index<index_structure::hibf> & index, std::set<std::string> filenames, std::filesystem::path count_filename){
    // alternatively, create/extract counts for these filenames by calling Chopper count
    std::ofstream out_stream(count_filename.c_str()); // or std::filesystem::u8path(subtree_bin_paths);

    for (std::string const & filename : filenames){        //for (auto & filename : index.ibf().user_bins){// for each file in index.user_bins, perhaps I have to use .user_bin_filenames, but this is private... or index.filenames
        if (std::filesystem::path(filename).extension() !=".empty_bin"){ //no matching function for call to
                int kmer_count = index.ibf().get_occupancy_file(const_cast<std::string &>(filename));
                out_stream << filename + ' ' + std::to_string(kmer_count) + '\n';
        }
    }
}

/*!\brief Writes each of the specified files to a text file
 * \param[in] filenames a set of filenames of which the counts should be stored.
 * \param[in] user_bin_filenames the filename to which the counts should be stored.
 * \author Myrthe
 */
void write_filenames(std::string bin_path, std::set<std::string> user_bin_filenames){
    std::ofstream out_stream(bin_path.c_str());
    for (auto const & filename : user_bin_filenames)
        { if (std::filesystem::path(filename).extension() !=".empty_bin"){
                out_stream << filename + '\n';
            }
        }
    }

    /*!\brief Creates a configuration object which is passed to chopper's layout algorithm.
 * \param[in] subtree_bin_paths the file containing all paths to the user bins for which a layout should be computed
 * \author Myrthe
 */
chopper::configuration layout_config(std::string subtree_bin_paths, update_arguments const & arguments){
    seqan3::test::tmp_filename const input_prefix{"test"};
    seqan3::test::tmp_filename const layout_file{"layout.tsv"}; //std::filesystem::path(filename)

    chopper::configuration config{};     // raptor layout --num-hash-functions 2 --false-positive-rate 0.05 --input-file all_bin_paths.txt --output-filename hibf_12_12_ebs.layout --rearrange-user-bins --kmer-size 20 --tmax 64 --update-UBs 0.10
    // files: input of bins paths or count file. For now bin_paths.
    config.input_prefix = input_prefix.get_path();
    config.output_prefix = config.input_prefix; //todo Could not open file /tmp/seqan_test_XXXFZoTz/test.count for writing.
    config.output_filename = layout_file.get_path();

    //const std::filesystem::path subtree_bin_paths = std::filesystem::u8path(subtree_bin_paths);
    config.data_file =  std::filesystem::u8path(subtree_bin_paths); // or should this be without txt? //std::filesystem::cx_11path arguments.bin_file; //data_filename.get_path();
    chopper::detail::apply_prefix(config.output_prefix, config.count_filename, config.sketch_directory);//here the count filename is created.

    config.rearrange_user_bins = false; //arguments.similarity ==> should indicate whether updates should account for user bin's similarities.
    config.update_ubs = 0.1; //arguments.empty_bin_percentage percentage of empty bins drawn from distributions //makes sure to use empty bins.

    config.tmax = 64; // index.tmax --> store tmax and fpr_max as a parameters of the index.
    config.false_positive_rate = 0.05;  //index.false_positive_rate

    return config;
}

    void call_layout(size_t ibf_idx, // perhaps parse an argument with a filename/base, before calling the layout, store the descendent childeren of the layout.
                  raptor_index<index_structure::hibf> & index, chopper::configuration & config){
    int exit_code{};
    try
    {  // todo solve error with writing to test.count file in chopper::count. same happens with chopper::execute.
        //exit_code |= chopper::count::execute(config); // make sure this file is already stored, when similarity does not need to be taken into account.
        if (config.rearrange_user_bins){ // if similarity must be taken into account, then use count to calculate sketches.
            exit_code |= chopper::count::execute(config);
        }
        exit_code |= chopper::layout::execute(config);
    }
    catch (sharg::parser_error const & ext){    // GCOVR_EXCL_START
        std::cerr << "[CHOPPER ERROR] " << ext.what() << '\n';
    }
}

    /*!\brief Creates a configuration object which is passed to hierarchical build function.
 * \param[in] subtree_bin_paths the file containing all paths to the user bins for which a layout should be computed
 * \author Myrthe
 */
build_arguments build_config(std::string subtree_bin_paths, update_arguments const & update_arguments){
    seqan3::test::tmp_filename const input_prefix{"test"};
    seqan3::test::tmp_filename const layout_file{"layout.tsv"};
    build_arguments build_arguments{};

    build_arguments.kmer_size = 20;
    build_arguments.window_size = 23;
    build_arguments.fpr = 0.05; //index.false_positive_rate
    build_arguments.is_hibf = true;
    build_arguments.bin_file = "layout.tsv";//layout_file; // should contain the layout file.
    //--output = hibf_tmax_storeocc_8_12.index
    //--hibf /mnt/c/Users/myrth/Desktop/coding/raptor/lib/chopper/build/bin/hibf_tmax.layout

//    chopper::configuration config{};     // raptor layout --num-hash-functions 2 --false-positive-rate 0.05 --input-file all_bin_paths.txt --output-filename hibf_12_12_ebs.layout --rearrange-user-bins --kmer-size 20 --tmax 64 --update-UBs 0.10
//    // files: input of bins paths or count file. For now bin_paths.
//    config.input_prefix = input_prefix.get_path();
//    config.output_prefix = config.input_prefix;
//    config.output_filename = layout_file.get_path();
//
//    //const std::filesystem::path subtree_bin_paths = std::filesystem::u8path(subtree_bin_paths);
//    config.data_file =  std::filesystem::u8path(subtree_bin_paths); //std::filesystem::cx_11path arguments.bin_file; //data_filename.get_path();
//    chopper::detail::apply_prefix(config.output_prefix, config.count_filename, config.sketch_directory);
//
//    config.rearrange_user_bins = false; //arguments.similarity
//    config.update_ubs = 0.1; //arguments.empty_bin_percentage percentage of empty bins drawn from distributions
//
//    config.tmax = 64; // index.tmax --> store tmax and fpr_max as a parameters of the index.
//    config.false_positive_rate = 0.05;  //index.false_positive_rate
    return build_arguments;
}


template <seqan3::data_layout data_layout_mode>
void call_build(build_arguments & arguments, raptor_index<hierarchical_interleaved_bloom_filter<data_layout_mode>> & index){
    hibf::build_data<data_layout_mode> data{};
    raptor::hibf::create_ibfs_from_chopper_pack(data, arguments);
    std::vector<std::vector<std::string>> bin_path{};
    for (size_t i{0}; i < data.hibf.user_bins.num_user_bins(); ++i)
        bin_path.push_back(std::vector<std::string>{data.hibf.user_bins.filename_of_user_bin(i)});
    index.ibf() = std::move(data.hibf); //instead of creating the index object here.
}

template <typename T> void remove_indices(std::unordered_set<size_t> indices_to_remove, std::vector<T> & vector) {
        for (int i : indices_to_remove) {
            vector.erase(vector.begin() + i);
        }
    }

void merge_indexes(raptor_index<index_structure::hibf> & index, raptor_index<index_structure::hibf> & subindex, size_t ibf_idx){
    // without splitting:
    auto index_tuple = index.ibf().previous_ibf_id[ibf_idx];

    // 1 updating original indexes --> this has to be done once, since both new subindexes share the same original ibfs.
    // 1.1 which original indices in the IBF were the subindex that had to be rebuild?
    std::unordered_set<size_t> indices_to_remove = index.ibf().ibf_indices_childeren(ibf_idx);
    // 1.2 create a map, mapping remaining indices to their remove those indices in next_ibf and previous_ibf
    std::vector<int> indices_map; int counter = 0;// Initialize the result vector
    for (int i = 1; i <= index.ibf().ibf_vector.size(); i++) {
        if (indices_to_remove.find(i) == indices_to_remove.end()) {  // If the current element is not in indices_to_remove.
            indices_map[i] = counter; // Add it to the result vector
            counter += 1;
        }
    };

    // 1.3 remove vectors of indices of subindex
    remove_indices(indices_to_remove, index.ibf().ibf_vector); // do it for all at once?           auto Printer = [](auto&& remove_indices, std::unordered_set<size_t>&& indices_to_remove, auto&&... args) { (remove_indices(indices_to_remove, args),... );  }; Printer(remove_indices, indices_to_remove, index.ibf().next_ibf_id, index.ibf().previous_ibf_id); // https://ngathanasiou.wordpress.com/2015/12/15/182/
    remove_indices(indices_to_remove, index.ibf().next_ibf_id);
    remove_indices(indices_to_remove, index.ibf().previous_ibf_id);
    remove_indices(indices_to_remove, index.ibf().fpr_table);
    remove_indices(indices_to_remove, index.ibf().occupancy_table);
    // 1.4 and replace the indices that have to be replaced.
    for (int ibf_idx{0}; ibf_idx < index.ibf().next_ibf_id.size(); ibf_idx++) {
        for (int i{0}; i < index.ibf().next_ibf_id[ibf_idx].size(); ++i){
            auto & next_ibf_idx = index.ibf().next_ibf_id[ibf_idx][i]; // todo: check if it changes as should.
            next_ibf_idx = indices_map[next_ibf_idx];
        }
    }
    for (int ibf_idx{0}; ibf_idx < index.ibf().previous_ibf_id.size(); ++ibf_idx){
        auto & previous_ibf_idx = std::get<0>(index.ibf().previous_ibf_id[ibf_idx]);
        previous_ibf_idx = indices_map[previous_ibf_idx];
    }


    // 1.5 add the new rows, do this for each subindex!
    size_t ibf_count_before_appending = index.ibf().ibf_count();
    for (int ibf_idx{0}; ibf_idx < subindex.ibf().next_ibf_id.size(); ++ibf_idx){ // add size of index.ibf().ibf_vector to all in subindex's next_ibf_id.
        std::for_each(subindex.ibf().next_ibf_id[ibf_idx].begin(), subindex.ibf().next_ibf_id[ibf_idx].end(),
                      [ibf_count_before_appending](int d) {d += ibf_count_before_appending; ;});
    }
    for (int ibf_idx{0}; ibf_idx < subindex.ibf().next_ibf_id.size(); ++ibf_idx){ // add size of index.ibf().ibf_vector to the ibf_incices present in previous_ibf_id of nex.
        std::for_each(subindex.ibf().next_ibf_id[ibf_idx].begin(), subindex.ibf().next_ibf_id[ibf_idx].end(),
                      [ibf_count_before_appending](int d) {d += ibf_count_before_appending; ;}); // todo: check if adding int to size_t gives no problems.
    }
    auto append_to_vector = [] (auto index_vector, auto subindex_vector){
        index_vector.insert(index_vector.end(), subindex_vector.begin(), subindex_vector.end());
    };
    append_to_vector(index.ibf().ibf_vector, subindex.ibf().ibf_vector);
    append_to_vector(index.ibf().next_ibf_id, subindex.ibf().next_ibf_id);
    append_to_vector(index.ibf().previous_ibf_id, subindex.ibf().previous_ibf_id);
    append_to_vector(index.ibf().fpr_table, subindex.ibf().fpr_table);
    append_to_vector(index.ibf().occupancy_table, subindex.ibf().occupancy_table);


    // 1.6 update the pointer in the 1 or 2 cells that should contain our new index.
    // without splitting:
    ibf_idx = std::get<0>(index_tuple);
    auto bin_idx = std::get<1>(index_tuple);
    // with splitting:
    index.ibf().next_ibf_id[ibf_idx][bin_idx] = ibf_count_before_appending;
    index.ibf().previous_ibf_id[ibf_count_before_appending] = std::make_tuple(ibf_idx, bin_idx);



}

} // namespace raptor

