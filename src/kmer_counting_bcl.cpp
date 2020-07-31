/*
 * kmer_counting_bcl.cpp
 *
 *  Created on: Jul 30, 2020
 *      Author: bo
 */

#include <string>
#include <exception>
#include <vector>
#include <algorithm>    // std::random_shuffle
#include <random>
#include <ctime>        // std::time
#include <cstdlib>
#include <string>
#include <iostream>
#include <unistd.h>

#include <bcl/bcl.hpp>
#include <bcl/containers/HashMap.hpp>

#include "argagg.hpp"
#include "gzstream.h"
#include "sparc/utils.h"
#include "kmer.h"
#include "sparc/log.h"
#include "sparc/config.h"
#include "bcl/bclhelper.h"

using namespace std;
using namespace sparc;
#define KMER_SEND_BATCH_SIZE (100*1000)

BCL::HashMap<std::string, uint32_t> *g_map = 0;

size_t g_n_sent = 0;

struct Config: public BaseConfig {
	bool without_canonical_kmer;
	int kmer_length;

	void print() {
		BaseConfig::print();
		myinfo("config: kmer_length=%d", kmer_length);
		myinfo("config: canonical_kmer=%d", !without_canonical_kmer ? 1 : 0);
	}
};

void check_arg(argagg::parser_results &args, char *name) {
	if (!args[name]) {
		cerr << name << " is missing" << endl;
		exit(-1);
	}

}

int run(const std::vector<std::string> &input, Config &config);

int main(int argc, char **argv) {
	BCL::init();

	Config config;
	config.program = argv[0];
	config.rank = BCL::rank();
	config.nprocs = BCL::nprocs();
	config.mpi_hostname = sparc::get_hostname();
	set_spdlog_pattern(config.mpi_hostname.c_str(), config.rank);

	if (config.rank == 0) {
		myinfo("Welcome to Sparc!");
	}
	argagg::parser argparser { {

	{ "help", { "-h", "--help" }, "shows this help message", 0 },

	{ "input", { "-i", "--input" },
			"input folder which contains read sequences", 1 },

	{ "zip_output", { "-z", "--zip" }, "zip output files", 0 },

	{ "kmer_length", { "-k", "--kmer-length" }, "length of kmer", 1 },

	{ "output", { "-o", "--output" }, "output folder", 1 },

	{ "without_canonical_kmer", { "--without-canonical-kmer" },
			"do not use canonical kmer", 0 },

	} };

	argagg::parser_results args;
	try {
		args = argparser.parse(argc, argv);
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	if (args["help"]) {
		std::cerr << argparser;
		return EXIT_SUCCESS;
	}

	if (!args.pos.empty()) {
		std::cerr << "no positional argument is allowed" << endl;
		return EXIT_SUCCESS;
	}

	config.without_canonical_kmer = args["without_canonical_kmer"];
	config.zip_output = args["zip_output"];

	check_arg(args, (char*) "kmer_length");
	config.kmer_length = args["kmer_length"].as<int>();
	if (config.kmer_length < 1) {
		cerr << "Error, length of kmer :  " << config.kmer_length << endl;
		return EXIT_FAILURE;
	}
	check_arg(args, (char*) "input");
	string inputpath = args["input"].as<string>();
	config.inputpath.push_back(inputpath);
	check_arg(args, (char*) "output");
	string outputpath = args["output"].as<string>();
	config.outputpath = outputpath;

	if (!dir_exists(inputpath.c_str())) {
		cerr << "Error, input dir does not exists:  " << inputpath << endl;
		return EXIT_FAILURE;
	}

	BCL::barrier();

	if (BCL::rank() == 0) {
		if (dir_exists(outputpath.c_str())) {
			cerr << "Error, output dir exists:  " << outputpath << endl;
			return EXIT_FAILURE;
		}
	}

	if (BCL::rank() == 0) {
		if (make_dir(outputpath.c_str()) < 0) {
			cerr << "Error, mkdir dir failed for " << outputpath << endl;
			return EXIT_FAILURE;
		}
	}

	std::vector<std::string> myinput = get_my_files(inputpath);

	myinfo("#of my inputs = %ld", myinput.size());
	run(myinput, config);
	BCL::finalize();

	return 0;
}

inline void map_line(const string &line, int kmer_length,
		bool without_canonical_kmer, std::vector<std::string> &kmers) {
	std::vector<std::string> arr;
	split(arr, line, "\t");
	if (arr.empty()) {
		return;
	}
	if (arr.size() != 3) {
		mywarn("Warning, ignore line (s=%ld): %s", arr.size(), line.c_str());
		return;
	}
	string seq = arr.at(2);
	trim(seq);
	//uint32_t nodeid = std::stoul(arr.at(0));

	std::vector<std::string> v = generate_kmer(seq, kmer_length, "N",
			!without_canonical_kmer);

	for (size_t i = 0; i < v.size(); i++) {
		std::string s = kmer_to_base64(v[i]);
		kmers.push_back(s);
	}
}

void update_batch(std::vector<std::string> &kmers) {
	for (auto &s : kmers) {
		auto result = g_map->find(s);
		uint32_t n=0;
    		if(result != g_map->end()){
			n=*result;}
		g_map->insert_or_assign(s,n+1);
		g_n_sent++;
	}
	kmers.clear();
}

int process_seq_file(const std::string &filepath, int kmer_length,
		bool without_canonical_kmer) {

	std::vector<std::string> kmers;
	if (endswith(filepath, ".gz")) {
		igzstream file(filepath.c_str());
		std::string line;
		while (std::getline(file, line)) {
			map_line(line, kmer_length, without_canonical_kmer, kmers);
			if (kmers.size() >= KMER_SEND_BATCH_SIZE) {
				update_batch(kmers);
			}
		}
	} else {
		std::ifstream file(filepath);
		std::string line;
		while (std::getline(file, line)) {
			map_line(line, kmer_length, without_canonical_kmer, kmers);
			if (kmers.size() >= KMER_SEND_BATCH_SIZE) {
				update_batch(kmers);
			}
		}
	}
	if (kmers.size() > 0) {
		update_batch(kmers);
	}

	return 0;
}

int dump(const std::string &filepath, char sep) {
	int stat = 0;
	std::ostream *myfile_pointer = 0;
	if (sparc::endswith(filepath, ".gz")) {
		myfile_pointer = new ogzstream(filepath.c_str());

	} else {
		myfile_pointer = new std::ofstream(filepath.c_str());
	}
	std::ostream &myfile = *myfile_pointer;

	uint64_t n = 0;
	BCL::LocalHashMapIterator<BCL::HashMap<std::string, uint32_t>> itor =
			g_map->local_begin();
	for (; itor != g_map->local_end(); itor++) {
		BCL::HashMap<std::string, uint32_t>::value_type kv = (*itor);
		myfile << kv.first << sep << kv.second << std::endl;
		++n;
	}

	if (sparc::endswith(filepath, ".gz")) {
		((ogzstream&) myfile).close();
	} else {
		((std::ofstream&) myfile).close();
	}
	delete myfile_pointer;
	myinfo("Wrote %ld records", n);
	return stat;

}

int run(const std::vector<std::string> &input, Config &config) {
	if (config.rank == 0) {
		config.print();
	}

	g_map = new BCL::HashMap<std::string, uint32_t>(BCL::nprocs() * 1000);

	for (size_t i = 0; i < input.size(); i++) {
		myinfo("processing %s", input.at(i).c_str());
		process_seq_file(input.at(i), config.kmer_length,
				config.without_canonical_kmer);
	}

	BCL::barrier();

	myinfo("Total sent %ld kmers", g_n_sent);

	dump(config.get_my_output(), '\t');

	delete g_map;

	return 0;
}

