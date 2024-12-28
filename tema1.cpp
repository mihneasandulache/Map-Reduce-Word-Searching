#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include <iostream>

using namespace std;

typedef struct mapArgs {
    char** files;
    // fisierele pe care vom aplica map reduce
    int num_files;
    pthread_barrier_t* barrier;
    // bariera pentru a sincroniza thread-urile mappers si reducers
    pthread_mutex_t* file_mutex;
    int* current_file_index;
    vector<map<string, int>> initial_map;
    // dictioanrele partiale rezultate din map
    map<char, map<string, vector<int>>> final_map;
    // dictionarul final rezultat din reduce
    vector<pthread_mutex_t> final_map_locks;
    // mutex-uri pentru fiecare litera din alfabet
    pthread_mutex_t* final_file_mutex;
    int* final_current_file_index;
    pthread_mutex_t* final_letter_mutex;
    int* current_letter_index;
    pthread_barrier_t* final_barrier;
} mapArgs;

void parse_input_file(const char *input_file, mapArgs *args) {
    FILE *f = fopen(input_file, "r");
    if (f == NULL) {
        printf("File %s does not exist.\n", input_file);
        exit(-1);
    }

    int i = 0;
    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, f) != -1) {
        // citesc linie cu linie fisierul de intrare
        if (i == 0) {
            args->num_files = atoi(line);
            // pe prima linie se afla numarul de fisiere de procesat
            args->files = (char **)malloc(args->num_files * sizeof(char *));
            for (int j = 0; j < args->num_files; j++) {
                args->files[j] = (char *)malloc(1000 * sizeof(char));
            }
        } else {
            line[strcspn(line, "\n")] = '\0';
            strcpy(args->files[i - 1], line);
            // retin numele fisierelor de procesat
        }
        i++;
    }
    fclose(f);
}

void process_word(char *word, mapArgs* args, int file_index) {
    // functie pentru procesarea cuvintelor
    char cleaned_word[1000] = {0};
    int j = 0;
    for (int i = 0; word[i] != '\0'; i++) {
        if (isalpha(word[i])) {
            cleaned_word[j++] = tolower(word[i]);
            // daca este caracter il adaug in cuvantul procesat
        }
    }
    cleaned_word[j] = '\0';
    if (strlen(cleaned_word) > 0) {
        args->initial_map[file_index][string(cleaned_word)]++;
        // daca nu este gol il marchez ca aparut in fisierul curent
    }
}

void* mapper(void* arg) {
    mapArgs* args = (mapArgs*)arg;
    int file_index;
    while (1) {
        pthread_mutex_lock(args->file_mutex);
        if (*(args->current_file_index) >= args->num_files) {
            // daca nu mai sunt fisiere de procesat iesim
            pthread_mutex_unlock(args->file_mutex);
            break;
        }

        file_index = (*(args->current_file_index));
        /* variabila ce indica fisierul curent de procesat care este preluat de cel mai rapid thread,
        astfel realizandu-se o impartire optima dinamica */
        (*(args->current_file_index)) = (*(args->current_file_index)) + 1;
        // se incrementeaza fisierul curent de procesat in mutex pentru a nu exista race condition
        pthread_mutex_unlock(args->file_mutex);

        FILE *f = fopen(args->files[file_index], "r");
        if (f == NULL) {
            printf("File %s does not exist.\n", args->files[file_index]);
            exit(-1);
        }
        char buffer[100000];
        char word[1000] = {0};
        int index = 0;

        while (fgets(buffer, 100000, f)) {
            for (int i = 0; buffer[i] != '\0'; i++) {
                // citim pana la finalul buffer-ului curent
                if (isspace(buffer[i])) {
                    // daca este orice fel de spatiu(tab, spatiu, linie noua)
                    if (index > 0) {
                        word[index] = '\0';
                        process_word(word, args, file_index);
                        // daca avem deja un cuvant memorat si dam de spatiu, inseamna ca am ajuns la finalul cuvantului
                        index = 0;
                        // resetam index-ul
                    }
                } else {
                    word[index++] = buffer[i];
                    // adaugam literele in cuvantul curent
                }
            }
        }

        if (index > 0) {
            word[index] = '\0';
            process_word(word, args, file_index);
            // procesam ultimul cuvant
        }

        fclose(f);
    }

    pthread_barrier_wait(args->barrier);
    // se asteapta ca toate thread-urile mapper sa termine de procesat

    return NULL;
}

bool compare_words(const pair<string, vector<int>>& a, const pair<string, vector<int>>& b) {
    if (a.second.size() != b.second.size())
        return a.second.size() > b.second.size();
    return a.first < b.first;
    // comparam doua cuvinte in functie de numarul de fisiere in care apar si lexicografic
}

void* reducer(void* arg) {
    mapArgs* args = (mapArgs*)arg;
    pthread_barrier_wait(args->barrier);
    // se asteapta ca mapperii sa termine de procesat pentru ca reducerii sa inceapa
    int file_index;
    while (true) {
        pthread_mutex_lock(args->final_file_mutex);
        if (*(args->final_current_file_index) >= args->num_files) {
            pthread_mutex_unlock(args->final_file_mutex);
            break;
        }
        file_index = *(args->final_current_file_index);
        *(args->final_current_file_index) = *(args->final_current_file_index) + 1;
        pthread_mutex_unlock(args->final_file_mutex);
        // pentru procesare dictionarelor de la mapperi aplicam aceeasi strategie cu index-ul comun

        for (auto entry : args->initial_map[file_index]) {
            // pentru fiecare cuvant din dictionarul partial
            pthread_mutex_lock(&args->final_map_locks[entry.first[0] - 'a']);
            // blocam intrarea din dictionarul final care corespunde cu prima litera a cuvantului curent
            if (args->final_map[entry.first[0]].find(entry.first) == args->final_map[entry.first[0]].end()) {
                args->final_map[entry.first[0]][entry.first] = {file_index + 1};
                // daca e cuvant nou il punem in dictionar cu valoarea fisierului curent
            } else {
                args->final_map[entry.first[0]][entry.first].push_back(file_index + 1);
                // altfel adaugam fisierul curent la lista de fisiere in care apare cuvantul
            }
            pthread_mutex_unlock(&args->final_map_locks[entry.first[0] - 'a']);
        }
    }

    pthread_barrier_wait(args->final_barrier);
    // asteptam ca toate thread-urile reducer sa termine de introdus cuvintele in dictionarul final

    while (true) {
        pthread_mutex_lock(args->final_letter_mutex);
        if (*(args->current_letter_index) >= 26) {
            pthread_mutex_unlock(args->final_letter_mutex);
            break;
        }
        char letter = 'a' + (*(args->current_letter_index));
        (*(args->current_letter_index)) = (*(args->current_letter_index)) + 1;
        pthread_mutex_unlock(args->final_letter_mutex);
        // litera curenta care va reprezenta lista pe care un thread o va sorta si scrie in fisier

        auto& letter_map = args->final_map[letter];
        vector<pair<string, vector<int>>> sorted_entries(letter_map.begin(), letter_map.end());
        // transformam dictionarul corespunzator literei curente in vector pentru a-l sorta
        sort(sorted_entries.begin(), sorted_entries.end(), compare_words);
        // sortam cu functia de sortare definita anterior
        string filename = string(1, letter) + ".txt";
        FILE* file = fopen(filename.c_str(), "w");
        // creeam fisierul pentru litera curenta
        if (!file) {
            perror(("Error opening file " + filename).c_str());
            exit(EXIT_FAILURE);
        }

        if (sorted_entries.empty()) {
            fclose(file);
            continue;
        }
        // daca nu avem cuvinte cu litera curenta nu scriem nimic in fisier

        for (auto& entry : sorted_entries) {
            fprintf(file, "%s:[", entry.first.c_str());
            sort(entry.second.begin(), entry.second.end());
            // sortam si indecsii fisierelor
            for (int i = 0; i < entry.second.size(); ++i) {
                fprintf(file, "%d", entry.second[i]);
                if (i < entry.second.size() - 1) {
                    fprintf(file, " ");
                }
            }
            fprintf(file, "]\n");
            //scriem cuvintele in fisier
        }

        fclose(file);
    }

    return NULL;
}



int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: %s <num_mappers> <num_reducers> <input_file>\n", argv[0]);
        return -1;
    }

    int num_mappers = atoi(argv[1]);
    int num_reducers = atoi(argv[2]);
    char *input_file = argv[3];
    // extrag argumentele din linia de comanda

    if (num_mappers <= 0 || num_reducers <= 0) {
        printf("Enter valid number of mappers and reducers\n");
        return -1;
    }

    mapArgs args;
    args.files = NULL;
    args.num_files = 0;
    parse_input_file(input_file, &args);
    args.barrier = (pthread_barrier_t *) malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(args.barrier, NULL, num_mappers + num_reducers);
    args.file_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(args.file_mutex, NULL);
    args.current_file_index = (int *)malloc(sizeof(int));
    *(args.current_file_index) = 0;
    // pornim de la index-ul 0 pentru procesarea fisierelor din map
    args.initial_map.resize(args.num_files);
    args.final_map_locks.resize(26);
    args.final_file_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(args.final_file_mutex, NULL);
    args.final_current_file_index = (int *)malloc(sizeof(int));
    *(args.final_current_file_index) = 0;
    args.final_letter_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(args.final_letter_mutex, NULL);
    args.current_letter_index = (int*)malloc(sizeof(int));
    *(args.current_letter_index) = 0;
    args.final_barrier = (pthread_barrier_t*) malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(args.final_barrier, NULL, num_reducers);
    for (int i = 0; i < 26; i++) {
        pthread_mutex_init(&args.final_map_locks[i], NULL);
    }
    // initializam cate un mutex pentru fiecare litera din alfabetul final
    pthread_t *threads = (pthread_t*) malloc((num_mappers + num_reducers) * sizeof(pthread_t));

    for (int i = 0; i < num_mappers + num_reducers; i++) {
        if (i < num_mappers) {
            pthread_create(&threads[i], NULL, mapper, &args);
        } else {
            pthread_create(&threads[i], NULL, reducer, &args);
        }
        // creem thread-urile
    }
    
    for (int i = 0; i < num_mappers + num_reducers; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}


