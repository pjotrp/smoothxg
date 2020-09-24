#include <cstring>
#include "smooth.hpp"

namespace smoothxg {

    // do msa for a set of input sequences
// @function:
//    generate consensus sequence
//    generate rc-msa (row column multiple sequence alignment)
// @para:
//    ab/abpt: abpoa related variable and parameter
//    seq_n: number of input sequences
//    seq_len: array of input sequence length, size: seq_n
//    seqs: array of input sequences, 0123 for ACGT, size: seq_n * seq_len[]
int my_abpoa_msa(abpoa_t *ab, abpoa_para_t *abpt, int n_seqs, char **seq_names, int *seq_lens, uint8_t **seqs, FILE *out_fp, uint8_t ***cons_seq, int ***cons_cov, int **cons_l, int *cons_n, uint8_t ***msa_seq, int *msa_l) {
    if ((!abpt->out_msa && !abpt->out_cons && !abpt->out_gfa) || n_seqs <= 0) return 0;
    int i, tot_n = n_seqs;
    uint8_t *is_rc = (uint8_t*)_err_malloc(n_seqs * sizeof(uint8_t));
    abpoa_reset_graph(ab, abpt, seq_lens[0]);
    for (i = 0; i < n_seqs; ++i) {
        abpoa_res_t res;
        res.graph_cigar = 0, res.n_cigar = 0, res.is_rc = 0;
        abpoa_align_sequence_to_graph(ab, abpt, seqs[i], seq_lens[i], &res);
        abpoa_add_graph_alignment(ab, abpt, seqs[i], seq_lens[i], res, i, n_seqs);
        is_rc[i] = res.is_rc;
        if (res.n_cigar) free(res.graph_cigar);
    }
    if (abpt->out_gfa) {
        abpoa_generate_gfa(ab, abpt, seq_names, is_rc, n_seqs, out_fp);
    } else {
        if (abpt->out_cons) {
            abpoa_generate_consensus(ab, abpt, tot_n, out_fp, cons_seq, cons_cov, cons_l, cons_n);
            if (ab->abg->is_called_cons == 0)
                err_printf("Warning: no consensus sequence generated.\n");
        }
        if (abpt->out_msa) {
            abpoa_generate_rc_msa(ab, abpt, seq_names, is_rc, tot_n, out_fp, msa_seq, msa_l);
        }
    }
    free(is_rc);
    return 1;
}

    odgi::graph_t smooth_abPOA(const xg::XG& graph,
                               const block_t& block,
                               const uint64_t block_id,
                               const std::string& consensus_name) {
        // collect sequences
        std::vector<std::string> seqs_;
        std::vector<std::string> names;
        for (auto &path_range : block.path_ranges) {
            seqs_.emplace_back();
            auto &seq = seqs_.back();
            for (step_handle_t step = path_range.begin;
                 step != path_range.end;
                 step = graph.get_next_step(step)) {
                seq.append(graph.get_sequence(graph.get_handle_of_step(step)));
            }
            std::stringstream namess;
            namess << graph.get_path_name(graph.get_path_handle_of_step(path_range.begin))
                   << "_" << graph.get_position_of_step(path_range.begin);
            names.push_back(namess.str());
        }

        std::cerr << "n_seqs: " << seqs_.size() << std::endl;

        // initialize abPOA
        abpoa_t *ab = abpoa_init();
        // initialize abPOA parameters
        abpoa_para_t *abpt = abpoa_init_para();
        // we want to do local alignments
        abpt->align_mode = ABPOA_LOCAL_MODE;
        // possible other parameters to set?!
        // FIXME just for testing
        abpt->out_msa = 1;

        // finalize parameters
        abpoa_post_set_para(abpt);

        // AaCcGgTtNn ==> 0,1,2,3,4
        unsigned char nst_nt4_table[256] = {
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5 /*'-'*/, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
        };

        // char seqs = std::transform(seqs_.begin(), seqs_.end(), std::back_inserter(seqs_), convert);
        // collect sequence length, transform ACGT to 0123
        int n_seqs = seqs_.size();
        int *seq_lens = (int*)malloc(sizeof(int) * n_seqs);
        uint8_t **bseqs = (uint8_t**)malloc(sizeof(uint8_t*) * n_seqs);
        for (int i = 0; i < n_seqs; ++i) {
            seq_lens[i] = seqs_[i].size();
            bseqs[i] = (uint8_t*)malloc(sizeof(uint8_t) * seq_lens[i]);
            for (int j = 0; j < seq_lens[i]; ++j) {
                bseqs[i][j] = nst_nt4_table[(int)seqs_[i].c_str()[j]]; // TODO we make a c_str for every char in the string
            }
        }

        uint8_t **cons_seq; int **cons_cov, *cons_l, cons_n=0;
        uint8_t **msa_seq; int msa_l=0;
        // perform abpoa-msa
        abpoa_reset_graph(ab, abpt, seq_lens[0]);
        abpoa_msa(ab, abpt, n_seqs, NULL, seq_lens, bseqs, stdout, NULL, NULL, NULL, NULL, NULL, NULL); // WORKS
        abpoa_reset_graph(ab, abpt, seq_lens[0]);
        my_abpoa_msa(ab, abpt, n_seqs, NULL, seq_lens, bseqs, stdout, NULL, NULL, NULL, NULL, NULL, NULL); // WORKS NOT
        /* int i, tot_n = n_seqs;
        uint8_t *is_rc = (uint8_t*)_err_malloc(n_seqs * sizeof(uint8_t));
        abpoa_reset_graph(ab, abpt, seq_lens[0]);
        for (i = 0; i < n_seqs; ++i) {
            abpoa_res_t res;
            res.graph_cigar = 0, res.n_cigar = 0, res.is_rc = 0;
            abpoa_align_sequence_to_graph(ab, abpt, bseqs[i], seq_lens[i], &res);
            abpoa_add_graph_alignment(ab, abpt, bseqs[i], seq_lens[i], res, i, n_seqs);
            is_rc[i] = res.is_rc;
            if (res.n_cigar) free(res.graph_cigar);
        }

        fprintf(stdout, "=== output to variables ===\n");
        for (int i = 0; i < cons_n; ++i) {
            fprintf(stdout, ">Consensus_sequence\n");
            for (int j = 0; j < cons_l[i]; ++j)
                fprintf(stdout, "%c", "ACGTN"[cons_seq[i][j]]);
            fprintf(stdout, "\n");
        }
        fprintf(stdout, ">Multiple_sequence_alignment\n");
        for (i = 0; i < n_seqs; ++i) {
            for (int j = 0; j < msa_l; ++j) {
                fprintf(stdout, "%c", "ACGTN-"[msa_seq[i][j]]);
            }
            fprintf(stdout, "\n");
        }

        if (cons_n) {
            for (i = 0; i < cons_n; ++i) {
                free(cons_seq[i]);
                free(cons_cov[i]);
            }
            free(cons_seq);
            free(cons_cov);
            free(cons_l);
        }
        if (msa_l) {
            for (i = 0; i < n_seqs; ++i) free(msa_seq[i]);
            free(msa_seq);
        }
         */


        std::size_t max_sequence_size = 0;
        for (auto &seq : seqs_) {
            max_sequence_size = std::max(max_sequence_size, seq.size());
        }
        odgi::graph_t output_graph;
        // if the graph would be empty, bail out
        if (max_sequence_size == 0) {
            return output_graph;
        }

        return output_graph;
    }

odgi::graph_t smooth(const xg::XG& graph,
                     const block_t& block,
                     const uint64_t& block_id,
                     std::unique_ptr<spoa::AlignmentEngine>& alignment_engine,
                     std::int8_t poa_m,
                     std::int8_t poa_n,
                     std::int8_t poa_g,
                     std::int8_t poa_e,
                     std::int8_t poa_q,
                     std::int8_t poa_c,
                     const std::string& consensus_name) {

    auto poa_graph = spoa::createGraph();
    // collect sequences
    std::vector<std::string> seqs;
    std::vector<std::string> names;
    for (auto& path_range : block.path_ranges) {
        seqs.emplace_back();
        auto& seq = seqs.back();
        for (step_handle_t step = path_range.begin;
             step != path_range.end;
             step = graph.get_next_step(step)) {
            seq.append(graph.get_sequence(graph.get_handle_of_step(step)));
        }
        std::stringstream namess;
        namess << graph.get_path_name(graph.get_path_handle_of_step(path_range.begin))
               << "_" << graph.get_position_of_step(path_range.begin);
        names.push_back(namess.str());
    }
    /*
    std::string s = "smoothxg_block_" + std::to_string(block_id) + ".fa";
    std::ofstream fasta(s.c_str());
    for (uint64_t i = 0; i < seqs.size(); ++i) {
        fasta << ">" << names[i] << " " << seqs[i].size() << std::endl
              << seqs[i] << std::endl;
    }
    fasta.close();
    */
    // set up POA
    // done...
    // run POA
    std::size_t max_sequence_size = 0;
    for (auto& seq : seqs) {
        max_sequence_size = std::max(max_sequence_size, seq.size());
    }
    odgi::graph_t output_graph;
    // if the graph would be empty, bail out
    if (max_sequence_size == 0) {
        return output_graph;
    }
    // preallocation does not seem to help, and it consumes a lot of memory relative to progressive allocation
    //alignment_engine->prealloc(max_sequence_size, 4);
    std::vector<bool> aln_is_reverse;
    int i = 0;
    for (auto& seq : seqs) {
        //std::cerr << names[i++] << "\t" << seq << std::endl;
        // TODO determine alignment orientation somehow!!!!!!!!
        // or try both orientations here
        // we'll need to record the orientation in the path somehow
        // to structure the lacing
        std::int32_t score_fwd = 0;
        auto alignment_fwd = alignment_engine->align(seq, poa_graph, &score_fwd);
        auto rev_seq = odgi::reverse_complement(seq);
        std::int32_t score_rev = 0;
        auto alignment_rev = alignment_engine->align(rev_seq, poa_graph, &score_rev);
        try {
            // could give weight here to influence consensus
            if (score_fwd >= score_rev) {
                poa_graph->add_alignment(alignment_fwd, seq);
                aln_is_reverse.push_back(false);
            } else {
                poa_graph->add_alignment(alignment_rev, rev_seq);
                aln_is_reverse.push_back(true);
            }
        } catch(std::invalid_argument& exception) {
            std::cerr << exception.what() << std::endl;
            assert(false);
        }
    }
    // todo make the consensus generation optional
    // ...
    // force consensus genertion for graph annotation
    std::string consensus = poa_graph->generate_consensus();
    aln_is_reverse.push_back(false);
    // write the graph, with consensus as a path
    //odgi::graph_t output_graph;
    // convert the poa graph into our output format
    //poa_graph->print_gfa(std::cout, names, true);
    build_odgi(poa_graph,
               output_graph,
               names,
               aln_is_reverse,
               consensus_name,
               !consensus_name.empty());
    // normalize the representation, allowing for nodes > 1bp
    odgi::algorithms::unchop(output_graph);
    // order the graph
    output_graph.apply_ordering(odgi::algorithms::topological_order(&output_graph), true);
    //output_graph.to_gfa(out);
    return output_graph;
}

odgi::graph_t smooth_and_lace(const xg::XG& graph,
                              const std::vector<block_t>& blocks,
                              std::int8_t poa_m,
                              std::int8_t poa_n,
                              std::int8_t poa_g,
                              std::int8_t poa_e,
                              std::int8_t poa_q,
                              std::int8_t poa_c,
                              const std::string& consensus_base_name) {

    //
    // record the start and end points of all the path ranges and the consensus
    //
    std::vector<odgi::graph_t> block_graphs;
    block_graphs.resize(blocks.size());
    std::vector<path_position_range_t> path_mapping;
    std::vector<path_position_range_t> consensus_mapping;
    bool add_consensus = !consensus_base_name.empty();
    std::mutex path_mapping_mutex, consensus_mapping_mutex, logging_mutex;
    uint64_t thread_count = odgi::get_thread_count();
    std::uint8_t poa_algorithm = 0;
    // setup our alignment engines
    /*
    std::vector<std::unique_ptr<spoa::AlignmentEngine>> alignment_engines;
    // todo make configurable?

    for (uint64_t i = 0; i < thread_count; ++i) {
        try {
            alignment_engines.emplace_back();
            auto& alignment_engine = alignment_engines.back();
            alignment_engine = spoa::createAlignmentEngine(
                static_cast<spoa::AlignmentType>(poa_algorithm),
                poa_m, poa_n, poa_g, poa_e, poa_q, poa_c);
            //alignment_engines.push_back(alignment_engine);
        } catch(std::invalid_argument& exception) {
            std::cerr << exception.what() << std::endl;
            assert(false);
        }
    }
    */

    paryfor::parallel_for<uint64_t>(
        0, blocks.size(),
        thread_count,
        [&](uint64_t block_id, int tid) {
            { //if (block_id % 100 == 0) {
                std::lock_guard<std::mutex> guard(logging_mutex);
                std::cerr << "[smoothxg::smooth_and_lace] applying spoa to block "
                          << block_id << "/" << blocks.size() << " "
                          << std::fixed << std::showpoint << std::setprecision(3)
                          << (float) block_id / (float) blocks.size() * 100 << "%\r";
            }
            auto& block = blocks[block_id];
            std::string consensus_name = consensus_base_name + std::to_string(block_id);
            //std::cerr << "on block " << block_id+1 << " of " << blocks.size() << std::endl;
            auto& block_graph = block_graphs[block_id];
            std::unique_ptr<spoa::AlignmentEngine> alignment_engine;
            try {
                alignment_engine = spoa::createAlignmentEngine(
                    static_cast<spoa::AlignmentType>(poa_algorithm),
                    poa_m, poa_n, poa_g, poa_e, poa_q, poa_c);
            } catch(std::invalid_argument& exception) {
                std::cerr << exception.what() << std::endl;
                assert(false);
            }
            /*block_graph = smooth(graph,
                                 block,
                                 block_id,
                                 alignment_engine,
                                 poa_m,
                                 poa_n,
                                 poa_g,
                                 poa_e,
                                 poa_q,
                                 poa_c,
                                 consensus_name);
                                 */

            block_graph = smooth_abPOA(graph,
                                       block,
                                       block_id,
                                       /* TODO add abPOA parameters
                                        */
                                       consensus_name);

            std::cerr << std::endl;
            std::cerr << "After smooth. Exiting for now....." << std::endl;
            exit(0);
            if (block_graph.get_node_count() > 0) {
                //auto& block_graph = block_graphs.back();
                // record the start and end paths
                // nb: the path order is the same in the input block and output graph
                uint64_t path_id = 0;
                for (auto& path_range : block.path_ranges) {
                    auto path_handle = graph.get_path_handle_of_step(path_range.begin);
                    auto last_step = graph.get_previous_step(path_range.end);
                    {
                        std::lock_guard<std::mutex> guard(path_mapping_mutex);
                        path_mapping.push_back({
                                path_handle, // target path
                                graph.get_position_of_step(path_range.begin), // start position
                                (graph.get_position_of_step(last_step) // end position
                                 + graph.get_length(graph.get_handle_of_step(last_step))),
                                path_range.begin,
                                path_range.end,
                                as_path_handle(++path_id),
                                block_id
                            });
                    }
                }
                // make the graph
        
                // record the consensus path
                if (add_consensus) {
                    auto consensus_handle = block_graph.get_path_handle(consensus_name);
                    uint64_t path_end = 0;
                    step_handle_t empty_step;
                    as_integers(empty_step)[0] = 0;
                    as_integers(empty_step)[1] = 0;
                    block_graph.for_each_step_in_path(
                        consensus_handle,
                        [&](const step_handle_t& step) {
                            path_end += block_graph.get_length(block_graph.get_handle_of_step(step));
                        });
                    {
                        std::lock_guard<std::mutex> guard(consensus_mapping_mutex);
                        consensus_mapping.push_back({
                                as_path_handle(0), // consensus = 0 path handle
                                0, // start position
                                path_end, // end position
                                empty_step,
                                empty_step,
                                consensus_handle,
                                block_id
                            });
                    }
                }
            }
        });

    std::cerr << "[smoothxg::smooth_and_lace] applying spoa to block "
              << blocks.size() << "/" << blocks.size() << " "
              << std::fixed << std::showpoint << std::setprecision(3)
              << 100.0 << "%" << std::endl;

    std::cerr << "[smoothxg::smooth_and_lace] sorting path_mappings" << std::endl;
    // sort the path range mappings by path handle id, then start position
    // this will allow us to walk through them in order
    ips4o::parallel::sort(
        path_mapping.begin(),
        path_mapping.end(),
        [](const path_position_range_t& a,
           const path_position_range_t& b) {
            auto& a_id = as_integer(a.base_path);
            auto& b_id = as_integer(b.base_path);
            return (a_id < b_id || a_id == b_id && a.start_pos < b.start_pos);
        });
    // build the sequence and edges into the output graph
    odgi::graph_t smoothed;
    // add the nodes and edges to the graph
    std::vector<uint64_t> id_mapping;
    std::cerr << "[smoothxg::smooth_and_lace] building final graph" << std::endl;
    uint64_t j = 0;
    for (auto& block : block_graphs) {
        uint64_t id_trans = smoothed.get_node_count();
        { //if (j % 100 == 0) {
            std::cerr << "[smoothxg::smooth_and_lace] adding graph "
                      << j << "/" << block_graphs.size() << " "
                      << std::fixed << std::showpoint << std::setprecision(3)
                      << (float)j / (float)block_graphs.size() << "%\r";
        }
        ++j;
        // record the id translation
        id_mapping.push_back(id_trans);
        if (block.get_node_count() == 0) {
            continue;
        }
        block.for_each_handle(
            [&](const handle_t& h) {
                smoothed.create_handle(block.get_sequence(h));
            });
        block.for_each_edge(
            [&](const edge_t& e) {
                smoothed.create_edge(
                    smoothed.get_handle(id_trans + block.get_id(e.first)),
                    smoothed.get_handle(id_trans + block.get_id(e.second))
                    );
            });
    }
    std::cerr << "[smoothxg::smooth_and_lace] adding graph " << j++ << "/" << block_graphs.size() << " 100.000%" << std::endl;
    // then for each path, ensure that it's embedded in the graph by walking through its block segments in order
    // and linking them up in the output graph
    for (uint64_t i = 0; i < path_mapping.size(); ++i) {
        {
            std::cerr << "[smoothxg::smooth_and_lace] embedding path fragment " << i << "/" << path_mapping.size() << "\r";
        }
        path_position_range_t* pos_range = &path_mapping[i];
        path_position_range_t* last_pos_range = nullptr;
        step_handle_t last_step = {0, 0};
        uint64_t last_end_pos = 0;
        // add the path to the graph
        path_handle_t smoothed_path = smoothed.create_path_handle(graph.get_path_name(pos_range->base_path));
        // walk the path from start to end
        while (true) {
            // if we find a segment that's not included in any block, we'll add it to the final graph and link it in
            // to do so, we detect a gap in length, collect the sequence in the gap and add it to the graph as a node
            // then add it as a traversal to the path
            if (pos_range->start_pos - last_end_pos > 0) {
                step_handle_t last;
                if (last_pos_range != nullptr) {
                    // we can iterate from the last path end
                    last = last_pos_range->end_step;
                } else {
                    // iterate from the path start to here
                    last = graph.path_begin(pos_range->base_path);
                }
                // 1) collect sequence
                std::string seq;
                for (step_handle_t step = last;
                     step != pos_range->start_step;
                     step = graph.get_next_step(step)) {
                    seq.append(graph.get_sequence(graph.get_handle_of_step(step)));
                }
                // 2) create node
                handle_t h = smoothed.create_handle(seq);
                // 3) append to path in smoothed
                smoothed.append_step(smoothed_path, h);
                if (as_integers(last_step)[0] != 0) {
                    smoothed.create_edge(smoothed.get_handle_of_step(last_step), h);
                }
                last_step = smoothed.path_back(smoothed_path);
            }
            // write the path steps into the graph using the id translation
            auto& block = block_graphs[pos_range->target_graph_id];
            auto& id_trans = id_mapping[pos_range->target_graph_id];
            bool first = true;
            block.for_each_step_in_path(
                pos_range->target_path,
                [&](const step_handle_t& step) {
                    handle_t h = block.get_handle_of_step(step);
                    handle_t t = smoothed.get_handle(
                        block.get_id(h) + id_trans,
                        block.get_is_reverse(h));
                    smoothed.append_step(smoothed_path, t);
                    if (first) {
                        first = false;
                        // create edge between last and curr
                        if (as_integers(last_step)[0] != 0) {
                            smoothed.create_edge(smoothed.get_handle_of_step(last_step), t);
                        }
                    }
                });
            last_step = smoothed.path_back(smoothed_path);
            last_pos_range = pos_range;
            last_end_pos = pos_range->end_pos;
            if (i+1 == path_mapping.size()
                || path_mapping[i+1].base_path != pos_range->base_path) {
                break;
            } else {
                ++i;
                pos_range = &path_mapping[i];
            }
        }
        // now add in any final sequence in the path
        // and add it to the path, add the edge
        if (graph.get_path_length(pos_range->base_path) > last_end_pos) {
            std::string seq;
            for (step_handle_t step = pos_range->end_step;
                 step != graph.path_end(pos_range->base_path);
                 step = graph.get_next_step(step)) {
                seq.append(graph.get_sequence(graph.get_handle_of_step(step)));
            }
            handle_t h = smoothed.create_handle(seq);
            smoothed.create_edge(smoothed.get_handle_of_step(last_step), h);
            smoothed.append_step(smoothed_path, h);
        }
    }
    std::cerr << "[smoothxg::smooth_and_lace] embedding path fragment " << path_mapping.size() << "/" << path_mapping.size() << std::endl;
    // now verify that smoothed has paths that are equal to the base graph
    // and that all the paths are fully embedded in the graph
    std::cerr << "[smoothxg::smooth_and_lace] verifying paths" << std::endl;
    smoothed.for_each_path_handle(
        [&](const path_handle_t& path) {
            // collect sequence
            std::string orig_seq, smoothed_seq;
            graph.for_each_step_in_path(
                graph.get_path_handle(smoothed.get_path_name(path)),
                [&](const step_handle_t& step) {
                    orig_seq.append(graph.get_sequence(graph.get_handle_of_step(step)));
                });
            smoothed.for_each_step_in_path(
                path,
                [&](const step_handle_t& step) {
                    smoothed_seq.append(smoothed.get_sequence(smoothed.get_handle_of_step(step)));
                });
            if (orig_seq != smoothed_seq) {
                std::cerr << "[smoothxg] error! path " << smoothed.get_path_name(path) << " was corrupted in the smoothed graph" << std::endl
                          << "original\t" << orig_seq << std::endl
                          << "smoothed\t" << smoothed_seq << std::endl;
                exit(1);
            }
            assert(orig_seq == smoothed_seq);
        });

    if (consensus_mapping.size()) {
        std::cerr << "[smoothxg::smooth_and_lace] sorting consensus" << std::endl;
    }
    // consensus path and connections
    ips4o::parallel::sort(
        consensus_mapping.begin(),
        consensus_mapping.end(),
        [](const path_position_range_t& a,
           const path_position_range_t& b) {
            auto& a_id = as_integer(a.base_path);
            auto& b_id = as_integer(b.base_path);
            return (a_id < b_id || a_id == b_id && a.start_pos < b.start_pos);
        });

    // by definition, the consensus paths are embedded in our blocks, which simplifies things
    // we'll still need to add a new path for each consensus path
    if (consensus_mapping.size()) {
        std::cerr << "[smoothxg::smooth_and_lace] embedding consensus" << std::endl;
    }
    for (auto& pos_range : consensus_mapping) {
        auto& block = block_graphs[pos_range.target_graph_id];
        path_handle_t smoothed_path = smoothed.create_path_handle(block.get_path_name(pos_range.target_path));
        auto& id_trans = id_mapping[pos_range.target_graph_id];
        block.for_each_step_in_path(
            pos_range.target_path,
            [&](const step_handle_t& step) {
                handle_t h = block.get_handle_of_step(step);
                handle_t t = smoothed.get_handle(
                    block.get_id(h) + id_trans,
                    block.get_is_reverse(h));
                smoothed.append_step(smoothed_path, t);
                // nb: by definition of our construction of smoothed
                // the consensus paths should have all their edges embedded
            });
    }
    // todo: validate the consensus paths as well
    return smoothed;
}

void write_gfa(std::unique_ptr<spoa::Graph>& graph,
               std::ostream& out,
               const std::vector<std::string>& sequence_names,
               bool include_consensus) {

    auto& nodes = graph->nodes();
    std::vector<std::int32_t> in_consensus(nodes.size(), -1);
    std::int32_t rank = 0;
    auto consensus = graph->consensus();
    for (const auto& id: consensus) {
        in_consensus[id] = rank++;
    }

    out << "H" << "\t" << "VN:Z:1.0" << std::endl;

    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        out << "S" << "\t" << i+1 << "\t" << static_cast<char>(graph->decoder(nodes[i]->code()));
        if (in_consensus[i] != -1) {
            out << "\t" << "ic:Z:true";
        }
        out << std::endl;
        for (const auto& edge: nodes[i]->out_edges()) {
            out << "L" << "\t" << i+1 << "\t" << "+" << "\t" << edge->end_node_id()+1 << "\t" << "+" << "\t" << "0M" << "\t"
                << "ew:f:" << edge->total_weight();
            if (in_consensus[i] + 1 == in_consensus[edge->end_node_id()]) {
                out << "\t" << "ic:Z:true";
            }
            out << std::endl;
        }
    }

    for (std::uint32_t i = 0; i < sequence_names.size(); ++i) {
        out << "P" << "\t" << sequence_names[i] << "\t";
        std::uint32_t node_id = graph->sequences_begin_nodes_ids()[i];
        while (true) {
            out << node_id+1 << "+";
            if (!nodes[node_id]->successor(node_id, i)) {
                break;
            } else {
                out << ",";
            }
        }
        out << "\t" << "*" << std::endl;
    }

    if (include_consensus) {
        out << "P" << "\t" << "Consensus" << "\t";
        for (auto& id : graph->consensus()) {
            out << id+1 << "+";
        }
        out << "\t" << "*" << std::endl;
    }
}

void build_odgi(std::unique_ptr<spoa::Graph>& graph,
                odgi::graph_t& output,
                const std::vector<std::string>& sequence_names,
                const std::vector<bool>& aln_is_reverse,
                const std::string& consensus_name,
                bool include_consensus) {

    auto& nodes = graph->nodes();
    std::vector<std::int32_t> in_consensus(nodes.size(), -1);
    std::int32_t rank = 0;
    auto consensus = graph->consensus();
    for (const auto& id: consensus) {
        in_consensus[id] = rank++;
    }

    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        std::string seq = std::string(1, static_cast<char>(graph->decoder(nodes[i]->code())));
        output.create_handle(seq, i+1);
    }

    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        for (const auto& edge: nodes[i]->out_edges()) {
            output.create_edge(output.get_handle(i+1), output.get_handle(edge->end_node_id()+1));
        }
    }

    for (std::uint32_t i = 0; i < sequence_names.size(); ++i) {
        path_handle_t p = output.create_path_handle(sequence_names[i]);
        std::uint32_t node_id = graph->sequences_begin_nodes_ids()[i];
        std::vector<handle_t> steps;
        while (true) {
            steps.push_back(output.get_handle(node_id+1));
            if (!nodes[node_id]->successor(node_id, i)) {
                break;
            }
        }
        if (aln_is_reverse[i]) {
            for (auto handle_itr = steps.rbegin(); handle_itr != steps.rend(); ++handle_itr) {
                output.append_step(p, output.flip(*handle_itr));
            }
        } else {
            for (auto& handle : steps) {
                output.append_step(p, handle);
            }
        }
    }

    if (include_consensus) {
        path_handle_t p = output.create_path_handle(consensus_name);
        for (auto& id : graph->consensus()) {
            output.append_step(p, output.get_handle(id+1));
        }
    }
}


}
