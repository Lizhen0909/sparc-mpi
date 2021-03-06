/*
 * edge_generating_mpi.cpp
 *
 *  Created on: Jul 13, 2020
 *      Author:
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
#include <mpi.h>

#include "argagg.hpp"
#include "gzstream.h"
#include "sparc/utils.h"
#include "kmer.h"
#include "sparc/log.h"
#include "sparc/KmerCountingListener.h"
#include "sparc/KmerCountingClient.h"
#include "sparc/DBHelper.h"
#include "sparc/config.h"
#include "mpihelper.h"
using namespace std;
using namespace sparc;

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
	int rank, size;

	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided == MPI_THREAD_SINGLE) {
		fprintf(stderr, "Could not initialize with thread support\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	Config config;
	config.program = argv[0];
	config.rank = rank;
	config.nprocs = size;
	config.mpi_hostname = MPI_get_hostname();
	config.mpi_ipaddress = get_ip_adderss(config.mpi_hostname);
	set_spdlog_pattern(config.mpi_hostname.c_str(), rank);

	if (rank == 0) {
		myinfo("Welcome to Sparc!");
	}

	argagg::parser argparser { {

	{ "help", { "-h", "--help" }, "shows this help message", 0 },

	{ "input", { "-i", "--input" },
			"input folder which contains read sequences", 1 },

	{ "port", { "-p", "--port" }, "port number", 1 },

	{ "scratch_dir", { "-s", "--scratch" },
			"scratch dir where to put temp data", 1 },

	{ "dbtype", { "--db" }, "dbtype (leveldb,rocksdb or default memdb)", 1 },

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

	if (args["without_canonical_kmer"]) {
		config.without_canonical_kmer = true;
	} else {
		config.without_canonical_kmer = false;
	}

	if (args["zip_output"]) {
		config.zip_output = true;
	} else {
		config.zip_output = false;
	}

	if (args["dbtype"]) {
		std::string dbtype = args["dbtype"].as<std::string>();
		if (dbtype == "memdb") {
			config.dbtype = DBHelper::MEMORY_DB;
		} else if (dbtype == "leveldb") {
#ifdef BUILD_WITH_LEVELDB
			config.dbtype = DBHelper::LEVEL_DB;
#else
			cerr
					<< "was not compiled with leveldb suppot. Please cmake with BUILD_WITH_LEVELDB=ON"
					<< dbtype << endl;
			return EXIT_FAILURE;
#endif
		} else if (dbtype == "rocksdb") {
			config.dbtype = DBHelper::ROCKS_DB;
			cerr << "rocksdb was removed due to always coredump" << endl;
		} else {
			cerr << "Error, unknown dbtype:  " << dbtype << endl;
			return EXIT_FAILURE;
		}
	} else {
		config.dbtype = DBHelper::MEMORY_DB;
	}

	check_arg(args, (char*) "kmer_length");
	config.kmer_length = args["kmer_length"].as<int>();
	if (config.kmer_length < 1) {
		cerr << "Error, length of kmer :  " << config.kmer_length << endl;
		return EXIT_FAILURE;
	}

	if (args["scratch_dir"]) {
		config.scratch_dir = args["scratch_dir"].as<string>();
	} else {
		config.scratch_dir = get_working_dir();
	}

	if (args["port"]) {
		config.port = args["port"].as<int>();
	} else {
		config.port = 7979;
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

	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == 0) {
		if (dir_exists(outputpath.c_str())) {
			cerr << "Error, output dir exists:  " << outputpath << endl;
			return EXIT_FAILURE;
		}
	}

	if (rank == 0) {
		if (make_dir(outputpath.c_str()) < 0) {
			cerr << "Error, mkdir dir failed for " << outputpath << endl;
			return EXIT_FAILURE;
		}
	}

	std::vector<std::string> myinput = get_my_files(inputpath, rank, size);

	myinfo("#of my inputs = %ld", myinput.size());
	run(myinput, config);
	MPI_Finalize();

	return 0;
}

void get_peers_information(Config &config) {
	int rank = config.rank;

	for (int p = 0; p < config.nprocs; p++) { //p:  a peer
		int buf;
		if (rank == p) {
			buf = config.get_my_port();
		}

		/* everyone calls bcast, data is taken from root and ends up in everyone's buf */
		MPI_Bcast(&buf, 1, MPI_INT, p, MPI_COMM_WORLD);
		config.peers_ports.push_back(buf);
	}

	for (int p = 0; p < config.nprocs; p++) { //p:  a peer
		int LENGTH = 256;
		char buf[LENGTH];
		bzero(buf, LENGTH);

		if (rank == p) {
			strcpy(buf, config.mpi_ipaddress.c_str());
		}

		MPI_Bcast(&buf, LENGTH, MPI_CHAR, p, MPI_COMM_WORLD);
		config.peers_hosts.push_back(buf);
	}

}

void reshuffle_rank(Config &config) {

	int nproc = config.nprocs;
	int buf[nproc];
	if (config.rank == 0) {
		std::vector<int> v;
		for (int i = 0; i < nproc; i++) {
			v.push_back(i);
		}
		shuffle(v);
		for (int i = 0; i < nproc; i++) {
			buf[i] = v.at(i);
		}
	}

	MPI_Bcast(&buf, nproc, MPI_INT, 0, MPI_COMM_WORLD);
	for (int i = 0; i < nproc; i++) {
		config.hash_rank_mapping.push_back(buf[i]);
		if (config.rank == 0) {
			myinfo("hash %d will be sent to rank %d", i, buf[i]);
		}
	}
}

int run(const std::vector<std::string> &input, Config &config) {
	if (config.rank == 0) {
		config.print();
	}
	reshuffle_rank(config);
	get_peers_information(config);
	KmerCountingListener listener(config.rank, config.nprocs,
			config.mpi_ipaddress, config.get_my_port(), config.get_dbpath(),
			config.dbtype);
	myinfo("Starting listener");
	int status = listener.start();
	if (status != 0) {
		myerror("Start listener failed.");
		MPI_Abort( MPI_COMM_WORLD, -1);
	}

	//wait for all server is ready
	MPI_Barrier(MPI_COMM_WORLD);
	myinfo("Starting client");
	KmerCountingClient client(config.rank, config.peers_ports,
			config.peers_hosts, config.hash_rank_mapping, false);
	if (client.start() != 0) {
		myerror("Start client failed");
		MPI_Abort( MPI_COMM_WORLD, -1);
	}

	for (size_t i = 0; i < input.size(); i++) {
		myinfo("processing %s", input.at(i).c_str());
		client.process_seq_file(input.at(i), config.kmer_length,
				config.without_canonical_kmer);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	client.stop();
	myinfo("Total sent %ld kmers", client.get_n_sent());
	MPI_Barrier(MPI_COMM_WORLD);

	//cleanup listener and db
	listener.stop();
	myinfo("Total recved %ld kmers", listener.get_n_recv());
	listener.dumpdb(config.get_my_output());
	listener.removedb();

	return 0;
}

