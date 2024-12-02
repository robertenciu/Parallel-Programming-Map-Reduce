#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <bits/stdc++.h>

using namespace std;


    typedef struct {
    vector<pair<string, int>> inputFiles;
    vector<pair<string, int>> *allWords;
    map<string, set<int>> *result; // Changed to map<string, set<int>>
    pthread_barrier_t* barrier;
    pthread_mutex_t* mutex;
    int id;
    int M, R;
} thread_data_t;

// From Geeks for geeks
vector<string> split_sentence(string sen) {
    stringstream ss(sen);
    string word;
    vector<string> words;
    while (ss >> word) {
        words.push_back(word);
    }
    return words;
}

bool cmp(pair<string, set<int>>& a, pair<string, set<int>>& b) { 
    if (a.second.size() != b.second.size()) {
        return a.second.size() > b.second.size();
    } 
    return a.first < b.first;
}
std::string remove_extra_spaces(const std::string& input) {
    std::istringstream iss(input);
    std::ostringstream oss;
    std::string word;

    // Read words from input and reconstruct the string with single spaces
    while (iss >> word) {
        if (!oss.str().empty()) {
            oss << " ";
        }
        oss << word;
    }

    return oss.str();
}

void *Mapper(void *arg) {
    thread_data_t* data = (thread_data_t*)arg;

    if (data->id < data->M) {
        for (size_t i = data->id; i < data->inputFiles.size(); i += data->M) {
            const auto& [filename, id] = data->inputFiles[i];
            ifstream file(filename);
            if (!file.is_open()) {
                cerr << "Error opening file: " << filename << endl;
                pthread_exit(NULL);
            }

            stringstream buffer;
            buffer << file.rdbuf();
            string file_content = buffer.str();
            transform(file_content.begin(), file_content.end(), file_content.begin(), [](unsigned char c) { return tolower(c); });file_content.erase(
                std::remove_if(file_content.begin(), file_content.end(), 
                            [](unsigned char c) { return !std::isalpha(c) && c != ' ' && c != '\n'; }),
                file_content.end()
            );
            vector<string> words = split_sentence(file_content);
            pthread_mutex_lock(data->mutex);
            for (const auto& word : words) {
                data->allWords->emplace_back(make_pair(word, id));
            }
            pthread_mutex_unlock(data->mutex);
            file.close();
        }
    }

    int r = pthread_barrier_wait(data->barrier);

    if (data->id >= data->M) {
        int start = (data->id - data->M);
        pthread_mutex_lock(data->mutex);
        for (size_t i = start; i < data->allWords->size(); i += data->R) {
            const auto& [word, id] = (*data->allWords)[i];
            (*data->result)[word].insert(id);
        }
        pthread_mutex_unlock(data->mutex);
    }

    r = pthread_barrier_wait(data->barrier);

    if (data->id >= data->M) {
        char start = 'a' + (data->id - data->M);
        for (int ch = start; ch <= 'z'; ch += data->R) {
            char current_char = static_cast<char>(ch);
            string out_name(1, current_char);
            ofstream out(out_name + ".txt");

            vector<pair<string, set<int>>> words;
            for (auto& [word, ids] : *data->result) {
                if (word[0] == ch) {
                    words.emplace_back(word, ids);
                }
            }

            sort(words.begin(), words.end(), cmp);
            for (auto& [word, ids] : words) {
                out << word << ":[";

                auto it = ids.begin();
                for (; it != prev(ids.end()); ++it) {
                    out << *it << " ";
                }
                out << *it << "]" << endl;
            }

            out.close();
        }
    }

    pthread_exit(NULL);
}


int main(int argc, char **argv)
{
    int M = stoi(argv[1]);
    int R = stoi(argv[2]);
    const string entry_file = argv[3];

    vector<pair<string, int>> inputFiles;

    ifstream entry_data(entry_file);
    int numberOfFiles;
    if (entry_data.is_open()) {
        string line;
        if (getline(entry_data, line)) {
            numberOfFiles = stoi(line);
        }

        for (size_t id = 1; id <= numberOfFiles; ++id) {
            if (getline(entry_data, line)) {
                inputFiles.emplace_back(make_pair(line, id));
            }
        }
        entry_data.close();
    }


    pthread_t threads[M + R];
    int r;

    // Initialize allWords as an empty vector of maps
    vector<pair<string, int>> allWords;
    
    map<string, set<int>> result;

    // Initialize barrier
    pthread_barrier_t barrier;
    r = pthread_barrier_init(&barrier, NULL, M + R);

    pthread_mutex_t mutex;
    r = pthread_mutex_init(&mutex, NULL);

    vector<thread_data_t> thread_data_array(M + R);
    auto it = inputFiles.begin();

    for (size_t i = 0; i < M + R; ++i) {
        thread_data_array[i].inputFiles = inputFiles;
        thread_data_array[i].barrier = &barrier;
        thread_data_array[i].mutex = &mutex;
        thread_data_array[i].id = i;
        thread_data_array[i].allWords = &allWords;
        thread_data_array[i].M = M;
        thread_data_array[i].R = R;
        thread_data_array[i].result = &result;

        r = pthread_create(&threads[i], NULL, Mapper, &thread_data_array[i]);

        if (r) {
            printf("Error creating thread %zu\n", i);
            exit(-1);
        }
    }

    void* status;
    for (size_t i = 0; i < M + R ; i++) {
        r = pthread_join(threads[i], &status);
        if (r) {
            printf("Error joining thread %zu\n", i);
            exit(-1);
        }
    }
    r = pthread_mutex_destroy(&mutex);
    r = pthread_barrier_destroy(&barrier);
    return 0;
}
