#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define INFO_TAG 1
#define DOWNLOAD_TAG 2
#define CHUNK_TAG 3
#define REQ_TAG 4
#define DOWNLOAD_ACK_TAG 5

#define REQUEST_FINISH 10
#define REQUEST_INFO_TRACKER 11
#define REQUEST_DONWLOAD 12
#define REQUEST_FINISH_UPLOAD 13
#define REQUEST_SWARM 14
#define REQUEST_ADD_TO_SWARM 15

typedef struct {
    int rank;
    std::vector<std::string> wanted_files;
} download_arg_t;

int count_upload = 0;

typedef struct {
    int rank;
    std::unordered_map<std::string, std::vector<std::string>> file_chunks;
} upload_arg_t;

std::unordered_map<std::string, std::vector<std::string>> owned_file_chunks;

void get_file_chunks(std::unordered_map<std::string, std::vector<std::string>> *chunks) {
    int request = REQUEST_INFO_TRACKER;

    MPI_Send(&request, 1, MPI_INT, TRACKER_RANK, REQ_TAG, MPI_COMM_WORLD);

    int nr_files;
    MPI_Recv(&nr_files, 1, MPI_INT, TRACKER_RANK, CHUNK_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    for (int i = 0; i < nr_files; i++) {
        char filename[MAX_FILENAME];
        MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, CHUNK_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        int nr_chunks;
        MPI_Recv(&nr_chunks, 1, MPI_INT, TRACKER_RANK, CHUNK_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (int j = 0; j < nr_chunks; j++) {
            char chunk_hash[HASH_SIZE];
            MPI_Recv(chunk_hash, HASH_SIZE, MPI_CHAR, TRACKER_RANK, CHUNK_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            chunk_hash[HASH_SIZE] = '\0';

            (*chunks)[std::string(filename)].push_back(std::string(chunk_hash));
        }
    }
}

void get_file_swarm(std::unordered_map<std::string, std::vector<int>> *file_swarm) {
    int nr_files;
    MPI_Recv(&nr_files, 1, MPI_INT, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    for (int i = 0; i < nr_files; i++) {
        char filename[MAX_FILENAME];
        MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        int nr_peers;
        MPI_Recv(&nr_peers, 1, MPI_INT, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (int j = 0; j < nr_peers; j++) {
            int peer;
            MPI_Recv(&peer, 1, MPI_INT, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            (*file_swarm)[std::string(filename)].push_back(peer);
        }
    }
}

void send_file_chunks(std::unordered_map<std::string, std::vector<std::string>> file_chunks, int peer_rank) {
    int nr_files = file_chunks.size();

    MPI_Send(&nr_files, 1, MPI_INT, peer_rank, CHUNK_TAG, MPI_COMM_WORLD);

    for (auto it = file_chunks.begin(); it != file_chunks.end(); it++) {
        char filename[MAX_FILENAME];
        strcpy(filename, it->first.c_str());
        int nr_chunks = it->second.size();

        MPI_Send(filename, MAX_FILENAME, MPI_CHAR, peer_rank, CHUNK_TAG, MPI_COMM_WORLD);
        MPI_Send(&nr_chunks, 1, MPI_INT, peer_rank, CHUNK_TAG, MPI_COMM_WORLD);

        for (int i = 0; i < nr_chunks; i++) {
            char chunk_hash[HASH_SIZE];
            strcpy(chunk_hash, it->second[i].c_str());

            MPI_Send(chunk_hash, HASH_SIZE, MPI_CHAR, peer_rank, CHUNK_TAG, MPI_COMM_WORLD);
        }
    }
}

void send_file_swarm(std::unordered_map<std::string, std::vector<int>> file_swarms, int peer_rank) {
    int nr_files = file_swarms.size();

    MPI_Send(&nr_files, 1, MPI_INT, peer_rank, INFO_TAG, MPI_COMM_WORLD);

    for (auto it = file_swarms.begin(); it != file_swarms.end(); it++) {
        char filename[MAX_FILENAME];
        strcpy(filename, it->first.c_str());
        int nr_peers = it->second.size();

        MPI_Send(filename, MAX_FILENAME, MPI_CHAR, peer_rank, INFO_TAG, MPI_COMM_WORLD);
        MPI_Send(&nr_peers, 1, MPI_INT, peer_rank, INFO_TAG, MPI_COMM_WORLD);

        for (int i = 0; i < nr_peers; i++) {
            int peer = it->second[i];
            MPI_Send(&peer, 1, MPI_INT, peer_rank, INFO_TAG, MPI_COMM_WORLD);
        }
    }
}

void send_download_request(int rank, std::vector<int> swarm, std::string filename, std::unordered_map<std::string, std::vector<std::string>> file_chunks, int &count, bool isLeecher) {
    std::vector<std::string> downloaded_chunks;

    bool add_to_swarm = true;

    int size_swarm = swarm.size();

    for (int i = 0; i < file_chunks[filename].size(); i++) {
        char chunk_hash[HASH_SIZE];
        strcpy(chunk_hash, file_chunks[filename][i].c_str());

        if (isLeecher) {
            strcpy(chunk_hash, file_chunks[filename][file_chunks[filename].size() - i - 1].c_str());
        }

        int count_it = 0;
        int peer = swarm[(i + count_it) % size_swarm];

        while(1) {
            count_it++;

            int request = REQUEST_DONWLOAD;
            int response, response_ack;

            MPI_Send(&request, 1, MPI_INT, peer, REQ_TAG, MPI_COMM_WORLD);

            MPI_Send(filename.c_str(), MAX_FILENAME, MPI_CHAR, peer, DOWNLOAD_TAG, MPI_COMM_WORLD);

            MPI_Send(chunk_hash, HASH_SIZE, MPI_CHAR, peer, DOWNLOAD_TAG, MPI_COMM_WORLD);

            MPI_Recv(&response_ack, 1, MPI_INT, peer, DOWNLOAD_ACK_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (response_ack == 3) {
                downloaded_chunks.push_back(std::string(chunk_hash));
                owned_file_chunks[filename].push_back(std::string(chunk_hash));
                count++;

                if (add_to_swarm) {
                    add_to_swarm = false;

                    int request = REQUEST_ADD_TO_SWARM;
                    MPI_Send(&request, 1, MPI_INT, TRACKER_RANK, REQ_TAG, MPI_COMM_WORLD);

                    MPI_Send(filename.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, REQ_TAG, MPI_COMM_WORLD);
                }

                if (downloaded_chunks.size() == file_chunks[filename].size()) {
                    std::string out_filename = std::string("client") + std::to_string(rank) + std::string("_") + filename;
                    std::ofstream fout(out_filename);

                    for (int i = 0; i < downloaded_chunks.size(); i++) {

                        if (isLeecher) {
                            fout << downloaded_chunks[downloaded_chunks.size() - i - 1] << "\n";
                        } else {
                            fout << downloaded_chunks[i] << "\n";
                        }
                    }

                    return;
                }

                if (count % 10 == 0 && count > 0) {
                    request = REQUEST_SWARM;

                    MPI_Send(&request, 1, MPI_INT, TRACKER_RANK, REQ_TAG, MPI_COMM_WORLD);

                    std::unordered_map <std::string, std::vector<int>> file_swarms;
                    get_file_swarm(&file_swarms);
                    swarm = file_swarms[filename];

                    size_swarm = swarm.size();
                }

                break;
            } else {
                peer = swarm[(i + count_it) % size_swarm];
            }
        }
    }
}

void *download_thread_func(void *arg)
{
    download_arg_t *download_arg = (download_arg_t*) arg; 

    int rank = download_arg->rank;
    std::vector<std::string> wanted_files = download_arg->wanted_files;
    

    std::unordered_map<std::string, std::vector<std::string>> file_chunks;
    std::unordered_map<std::string, std::vector<int>> file_swarms;

    bool isLeecher = false;

    if (owned_file_chunks.size() == 0) {
        isLeecher = true;
    }

    get_file_chunks(&file_chunks);
    get_file_swarm(&file_swarms);

    int count = 0;

    for (int i = 0; i < wanted_files.size(); i++) {
        if (file_swarms.find(wanted_files[i]) == file_swarms.end()) {
            std::cout << "File " << wanted_files[i] << " not found in swarm\n";
            continue;
        }

        std::vector<int> swarm = file_swarms[wanted_files[i]];

        send_download_request(rank, swarm, wanted_files[i], file_chunks, count, isLeecher);
    }

    int request = REQUEST_FINISH;
    MPI_Send(&request, 1, MPI_INT, TRACKER_RANK, REQ_TAG, MPI_COMM_WORLD);

    return NULL;
}

void *upload_thread_func(void *arg)
{
    upload_arg_t *upload_arg = (upload_arg_t *) arg;

    int rank = upload_arg->rank;
    std::unordered_map<std::string, std::vector<std::string>> file_chunks = upload_arg->file_chunks;

    while(1) {
        int request;
        MPI_Status status;

        MPI_Recv(&request, 1, MPI_INT, MPI_ANY_SOURCE, REQ_TAG, MPI_COMM_WORLD, &status);

        if (request == REQUEST_DONWLOAD) {
            char filename[MAX_FILENAME];
            MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, DOWNLOAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            char chunk_hash[HASH_SIZE];
            MPI_Recv(chunk_hash, HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, DOWNLOAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            chunk_hash[HASH_SIZE] = '\0';

            int response;
            
            // if (std::find(file_chunks[std::string(filename)].begin(), file_chunks[std::string(filename)].end(), std::string(chunk_hash)) != file_chunks[std::string(filename)].end()) {
            if (std::find(owned_file_chunks[std::string(filename)].begin(), owned_file_chunks[std::string(filename)].end(), std::string(chunk_hash)) != owned_file_chunks[std::string(filename)].end()) {
                response = 3;
                MPI_Send(&response, 1, MPI_INT, status.MPI_SOURCE, DOWNLOAD_ACK_TAG, MPI_COMM_WORLD);

                count_upload++;
            } else {
                response = 4;
                MPI_Send(&response, 1, MPI_INT, status.MPI_SOURCE, DOWNLOAD_ACK_TAG, MPI_COMM_WORLD);
            }
        } else if (request == REQUEST_FINISH_UPLOAD) {
            break;
        }
    }

    return NULL;
}

void tracker(int numtasks, int rank) {
    std::unordered_map<std::string, std::vector<int>> file_swarms;
    std::unordered_map<std::string, std::vector<std::string>> file_chunks;

    for (int i = 1; i < numtasks; i++) {

        int nr_files;

        MPI_Recv(&nr_files, 1, MPI_INT, i, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (int j = 0; j < nr_files; j++) {
            char filename[MAX_FILENAME];
            MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, i, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            int nr_chunks;
            MPI_Recv(&nr_chunks, 1, MPI_INT, i, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            std::vector<std::string> chunk_vector;

            for (int k = 0; k < nr_chunks; k++) {
                char chunk_hash[HASH_SIZE];
                MPI_Recv(chunk_hash, HASH_SIZE, MPI_CHAR, i, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                chunk_hash[HASH_SIZE] = '\0';
                chunk_vector.push_back(std::string(chunk_hash));
            }

            std::cout << "Received info about file " << filename << " from peer " << i << "\n";

            if (file_chunks.find(std::string(filename)) == file_chunks.end()) {
                file_chunks[std::string(filename)] = chunk_vector;
            }

            std::string filename_str(filename);
            file_swarms[filename_str].push_back(i);
        }
    }

    std::vector<int> finished_peers(numtasks, 0);

    for (int i = 1; i < numtasks; i++) {
        char message[] = "FILES ACK";

        MPI_Send(message, 10, MPI_CHAR, i, INFO_TAG, MPI_COMM_WORLD);
    }

    while(1) {
        int request;
        MPI_Status status;
        MPI_Recv(&request, 1, MPI_INT, MPI_ANY_SOURCE, REQ_TAG, MPI_COMM_WORLD, &status);

        // std::cout << "Received request " << request << " from peer " << status.MPI_SOURCE << "\n";

        if (request == REQUEST_FINISH) {
            finished_peers[status.MPI_SOURCE] = 1;

            bool all_finished = true;

            for (int i = 1; i < numtasks; i++) {
                if (!finished_peers[i]) {
                    all_finished = false;
                    break;
                }
            }

            if (all_finished) {
                for (int i = 1; i < numtasks; i++) {
                    int request = REQUEST_FINISH_UPLOAD;
                    MPI_Send(&request, 1, MPI_INT, i, REQ_TAG, MPI_COMM_WORLD);
                }

                break;
            }
        } else if (request == REQUEST_SWARM) {
            send_file_swarm(file_swarms, status.MPI_SOURCE);

        } else if (request == REQUEST_ADD_TO_SWARM) {
            char filename[MAX_FILENAME];
            MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, REQ_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            file_swarms[std::string(filename)].push_back(status.MPI_SOURCE);

        } else if (request == REQUEST_INFO_TRACKER) {
            send_file_chunks(file_chunks, status.MPI_SOURCE);
            send_file_swarm(file_swarms, status.MPI_SOURCE);
        }
    }

    for (auto it = file_swarms.begin(); it != file_swarms.end(); it++) {
        std::cout << "File " << it->first << " is shared by peers: ";
        for (int i = 0; i < it->second.size(); i++) {
            std::cout << it->second[i] << " ";
        }
        std::cout << "\n";
    }
}

void send_info_to_tracker(int rank, std::vector<std::string> *wanted_files, std::unordered_map<std::string, std::vector<std::string>> *file_chunks) {
    std::string in_filename = std::string("in") + std::to_string(rank) + std::string(".txt");
    int nr_files;
    int nr_files_wanted;

    std::ifstream fin(in_filename);

    fin >> nr_files;

    MPI_Send(&nr_files, 1, MPI_INT, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD);

    for (int i = 0; i < nr_files; i++) {
        char filename[MAX_FILENAME];
        int nr_chunks;
        fin >> filename >> nr_chunks;

        MPI_Send(filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD);

        MPI_Send(&nr_chunks, 1, MPI_INT, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD);

        for (int j = 0; j < nr_chunks; j++) {
            char chunk_hash[HASH_SIZE];
            fin >> chunk_hash;
            MPI_Send(chunk_hash, HASH_SIZE, MPI_CHAR, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD);

            (*file_chunks)[std::string(filename)].push_back(std::string(chunk_hash));
            owned_file_chunks[std::string(filename)].push_back(std::string(chunk_hash));
        }
    }

    fin >> nr_files_wanted;

    for (int i = 0; i < nr_files_wanted; i++) {
        char filename[MAX_FILENAME];
        fin >> filename;

        (*wanted_files).push_back(std::string(filename));
    }

}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    std::vector<std::string> wanted_files;
    std::unordered_map<std::string, std::vector<std::string>> file_chunks;

    send_info_to_tracker(rank, &wanted_files, &file_chunks);

    char message[10];
    MPI_Recv(message, 10, MPI_CHAR, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (!strcmp(message, "FILES ACK")) {
        std::cout << "Received ACK from tracker\n";
    } else {
        std::cout << "Received unknown message from tracker\n";
        exit(-1);
    }

    download_arg_t download_arg;
    download_arg.rank = rank;
    download_arg.wanted_files = wanted_files;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &download_arg);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    upload_arg_t upload_arg;
    upload_arg.rank = rank;
    upload_arg.file_chunks = file_chunks;

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &upload_arg);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }

    std::cout << "Peer " << rank << " has uploaded " << count_upload << " chunks to other clients\n";
 }
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
