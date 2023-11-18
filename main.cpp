#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>
typedef std::chrono::high_resolution_clock Clock;

// Structure to represent a fork with locking mechanisms
struct Fork {
    std::mutex lock;                            // Mutex for locking critical sections
    std::condition_variable condition_req_fork; // Condition variable for fork request
    std::condition_variable condition_fork;     // Condition variable for fork availability
    volatile bool hold;                         // Flag indicating if the fork is held by a philosopher
    volatile bool reqf;                         // Flag indicating if a philosopher has requested the fork
    volatile bool dirty = true;                 // Flag indicating if the fork is dirty (needs cleaning)
};

// Structure to represent a bottle with locking mechanisms
struct Bottle {
    std::mutex lock;                            // Mutex for locking critical sections
    std::condition_variable condition{};        // Condition variable for bottle availability
    volatile bool hold;                         // Flag indicating if the bottle is held by a philosopher
    volatile bool reqb;                         // Flag indicating if a philosopher has requested the bottle
};

// Structure to represent the shared resources (forks and bottles)
struct Resource {
    Fork fork;
    Bottle bottle;
};

// Enum to represent the state of a philosopher's dining activity
enum class Dine {
    THINKING = 1, HUNGRY, EATING
};

// Enum to represent the state of a philosopher's drinking activity
enum class Drink {
    TRANQUIL = 1, THIRSTY, DRINKING
};

// Function declarations
void *philosopher(void *pid);
void tranquil(long id);
void drink(long id);
void send_fork_request(long from, long to);
void send_fork(long from, long to);
void send_bottle_request(long from, long to);
void send_bottle(long from, long to);

// Main function for parsing command line arguments
int parser(int argc, char **argv);

// Function to initialize the dining graph
std::vector<std::vector<std::pair<int, Resource*>>> graph_initialize(int mode);

// Constants for time intervals
constexpr long TRANQUIL_MIN = 1, TRANQUIL_MAX = 1000;  
constexpr long DRINKING_MIN = 1, DRINKING_MAX = 1000; 
constexpr long TRANQUIL_RANGE = TRANQUIL_MAX - TRANQUIL_MIN;
constexpr long DRINKING_RANGE = DRINKING_MAX - DRINKING_MIN;

// Global variables
bool debug = false;
int p_cnt;
int count_session = 20;
std::string path;
std::atomic_bool start;
std::vector<Dine> dineState;
std::vector<Drink> drinkState;
std::vector<std::vector<std::pair<int, Resource*>>> graph;
std::vector<unsigned int> rand_seeds;
std::mutex print_lock;


// Function to parse command line arguments
int parser(int argc, char **argv) {
    int opt;

    // Definition of command line options
    static struct option opts[] = {
            {"session",  required_argument, nullptr, 's'},
            {"filename", required_argument, nullptr, 'f'},
            {nullptr,    no_argument,       nullptr, 0},
    };

    // Loop through command line options using getopt_long
    while ((opt = getopt_long(argc, argv, ":s:f:-d", opts, nullptr)) != EOF) {
        switch (opt) {
            // Case for handling the 'session' option
            case 's':
                count_session = static_cast<int>(std::strtol(optarg, nullptr, 10));
                break;

            // Case for handling the 'filename' option
            case 'f':
                path = optarg;
                break;

            // Case for handling the 'debug' option
            case 'd':
                debug = true;
                break;

            // Case for handling a missing value for an option
            case ':':
                std::cerr << "INVALID : needs value" << opt << std::endl;
                break;

            // Case for handling an unknown option
            case '?':
                std::cout << "USAGE: philosophers -s <session_count> -f <filename> [-]" << std::endl;
                exit(-1);
            default:
                break;
        }
    }

    // If debug mode is enabled, print the session count
    if (debug) {
        std::cout << "SESSIONS COUNT:   " << (count_session = count_session < 1 ? 20 : count_session) << std::endl;
    }

    // Check for any remaining arguments after processing options
    for (; optind < argc; optind++) {
        // If a hyphen is encountered, return 2 indicating a special case
        if (!strcmp(argv[optind], "-")) {
            return 2;
        }
    }

    // If a filename is provided, print it in debug mode and return 1
    if (path.length()) {
        if (debug) {
            std::cout << " PATH: " << path << std::endl;
        }
        return 1;
    }

    // Return 0 indicating successful parsing
    return 0;
}

// Function to initialize the dining graph based on the selected mode
std::vector<std::vector<std::pair<int, Resource*>>> graph_initialize(int mode) {
    // Mode 1: Read graph from file
    if (mode == 1) {
        std::ifstream file(path);
        if (file.good()) {
            int p1, p2, n = 0;
            
            // Read the number of philosophers from the file
            file >> p_cnt;
            
            // Initialize the graph vector
            std::vector<std::vector<std::pair<int, Resource*>>> graph(static_cast<unsigned long>(p_cnt));
            
            // Read edge pairs from the file and create resources for forks and bottles
            for (int i = 0; i < p_cnt; i++) {
                file >> p1 >> p2;
                
                // Validate edge pairs
                if (p1 < 1 || p2 < 1 || p1 > p_cnt || p2 > p_cnt) {
                    std::cerr << "ERROR: invalid graph" << std::endl;
                    exit(-1);
                }

                // Create positive and negative resources for forks and bottles
                Resource *pos = new Resource, *neg = new Resource;
                pos->fork.hold = pos->bottle.hold = true;
                neg->fork.reqf = neg->bottle.reqb = true;

                // Add the edge pairs to the graph
                graph[p1 - 1].push_back(std::make_pair(p2 - 1, p1 < p2 ? pos : neg));
                graph[p2 - 1].push_back(std::make_pair(p1 - 1, p1 < p2 ? neg : pos));

                n++;
            }

            // Validate the number of edges in the graph
            if (n < p_cnt - 1 || n > (p_cnt * (p_cnt - 1) / 2)) {
                std::cerr << "ERROR: invalid graph" << std::endl;
                exit(-1);
            }

            return graph;
        } else {
            std::cerr << "ERROR: file '" << path << "' not found" << std::endl;
            exit(-1);
        }

    // Mode 2: Manually input graph
    } else if (mode == 2) {
        int p1, p2, n = 0;
        
        // Prompt user for the number of philosophers
        std::cout << "NUM PHILOSOPHERS: ";
        std::cin >> p_cnt;

        // Initialize the graph vector
        std::vector<std::vector<std::pair<int, Resource*>>> graph(static_cast<unsigned long>(p_cnt));

        // Prompt user for edge pairs until '0' is entered
        std::cout << "EDGE PAIRS (0 to exit):" << std::endl;
        while (true) {
            std::cin >> p1 >> p2;
            
            // Exit loop if '0' is entered
            if (p1 < 1 || p2 < 1 || p1 > p_cnt || p2 > p_cnt) break;

            // Create positive and negative resources for forks and bottles
            Resource *pos = new Resource(), *neg = new Resource();
            pos->fork.hold = pos->bottle.hold = true;
            neg->fork.reqf = neg->bottle.reqb = true;

            // Add the edge pairs to the graph
            graph[p1 - 1].push_back(std::make_pair(p2 - 1, p1 < p2 ? pos : neg));
            graph[p2 - 1].push_back(std::make_pair(p1 - 1, p1 < p2 ? neg : pos));

            n++;
        }

        // Validate the number of edges in the graph
        if (n < p_cnt - 1 || n > (p_cnt * (p_cnt - 1) / 2)) {
            std::cerr << "ERROR: invalid graph" << std::endl;
            exit(-1);
        }

        return graph;

    // Default: Use a predefined graph for mode 3
    } else {
        p_cnt = 5; // Number of philosophers 
        // Create resources for forks and bottles for a predefined graph
        Resource *r1a = new Resource, *r2a = new Resource, *r3a = new Resource, *r4a = new Resource, *r5a = new Resource;
        Resource *r1b = new Resource, *r2b = new Resource, *r3b = new Resource, *r4b = new Resource, *r5b = new Resource;
        r1a->fork.hold = r2a->fork.hold = r3a->fork.hold = r4a->fork.hold = r5a->fork.hold = true;
        r1b->fork.reqf = r2b->fork.reqf = r3b->fork.reqf = r4b->fork.reqf = r5b->fork.reqf = true;
        r1a->bottle.hold = r2a->bottle.hold = r3a->bottle.hold = r4a->bottle.hold = r5a->bottle.hold = true;
        r1b->bottle.reqb = r2b->bottle.reqb = r3b->bottle.reqb = r4b->bottle.reqb = r5b->bottle.reqb = true;
        
        // Return the predefined graph
        return {{std::make_pair(1, r1a), std::make_pair(4, r5a)},
                {std::make_pair(2, r2a), std::make_pair(0, r1b)},
                {std::make_pair(3, r3a), std::make_pair(1, r2b)},
                {std::make_pair(4, r4a), std::make_pair(2, r3b)},
                {std::make_pair(0, r5b), std::make_pair(3, r4b)}};
    }
}
// Function representing the behavior of a philosopher
void *philosopher(void *pid) {
    // Wait until the start signal is received
    while (!start.load());
    
    // Get the philosopher's ID
    long id = (long) pid;

    // Loop through dining sessions
    for (int session = 0; session < count_session;) {
        // Obtain references to the resources associated with the philosopher
        std::vector<std::pair<int, Resource*>> refs = graph[id];

        // Drink state switch
        switch (drinkState[id]) {
            // Case when the philosopher is in TRANQUIL state
            case Drink::TRANQUIL:
                // Iterate through the philosopher's neighbors and check for available bottles
                for (std::pair<int, Resource*> ref_pair : refs) {
                    Resource *resource = ref_pair.second;
                    resource->bottle.lock.lock();
                    if (resource->bottle.hold && resource->bottle.reqb && !resource->fork.hold) {
                        send_bottle(id, ref_pair.first);
                        resource->bottle.hold = false;
                    }
                    resource->bottle.lock.unlock();
                }
                tranquil(id);
                drinkState[id] = Drink::THIRSTY;
                break;

            // Case when the philosopher is in THIRSTY state
            case Drink::THIRSTY:
                // Iterate through the philosopher's neighbors and handle bottle requests
                for (std::pair<int, Resource*> ref_pair : refs) {
                    Resource *resource = ref_pair.second;
                    resource->bottle.lock.lock();
                    if (resource->bottle.hold && resource->bottle.reqb && !resource->fork.hold) {
                        send_bottle(id, ref_pair.first);
                        resource->bottle.hold = false;
                    }
                    // If the bottle is not held, wait for a bottle request and send a request
                    if (!resource->bottle.hold) {
                        while (!resource->bottle.reqb) {
                            std::unique_lock<std::mutex> lk(resource->bottle.lock);
                            resource->bottle.condition.wait(lk);
                        }
                        send_bottle_request(id, ref_pair.first);
                        resource->bottle.reqb = false;
                    }
                    resource->bottle.lock.unlock();
                }
                drinkState[id] = Drink::DRINKING;
                break;

            // Case when the philosopher is in DRINKING state
            case Drink::DRINKING:
                drink(id);
                print_lock.lock();
                std::cout << "philosopher " << id + 1 << " drinking" << std::endl;
                print_lock.unlock();
                drinkState[id] = Drink::TRANQUIL;
                session++;
                // If the session limit is reached, transition to THINKING state
                if (session == count_session) {
                    dineState[id] = Dine::THINKING;
                }
                break;
        }

        // Dining state switch
        switch (dineState[id]) {
            // Case when the philosopher is in THINKING state
            case Dine::THINKING: 
                print_lock.lock(); 
                std::cout << "Thinking time for philosopher " << id + 1 << std::endl;
                print_lock.unlock();

                // Iterate through the philosopher's neighbors and handle fork requests
                for (std::pair<int, Resource*> ref_pair : refs) {
                    auto to = std::find_if(graph[id].begin(), graph[id].end(), [ref_pair](std::pair<int, Resource*> pair) -> bool {return ref_pair.first == pair.first;});
                    to->second->fork.lock.lock();
                    // If it's the last session or the fork is held and dirty, send the fork
                    if (session == count_session || (to->second->fork.hold && to->second->fork.dirty && to->second->fork.reqf)) {
                        send_fork(id, ref_pair.first);
                        to->second->fork.hold = false;
                        to->second->fork.dirty = false;
                    }
                    to->second->fork.lock.unlock();
                }
                // If it's the last session, break out of the loop
                if (session == count_session) {
                    break;
                }

                // (D1) A thinking, thirsty philosopher becomes hungry
                if (drinkState[id] == Drink::THIRSTY) {
                    dineState[id] = Dine::HUNGRY;
                }
                break;

            // Case when the philosopher is in HUNGRY state
            case Dine::HUNGRY:
                // Iterate through the philosopher's neighbors and handle fork requests
                for (std::pair<int, Resource*> ref_pair : refs) {
                    auto to = std::find_if(graph[id].begin(), graph[id].end(), [ref_pair](std::pair<int, Resource*> pair) -> bool {return ref_pair.first == pair.first;});
                    if (to->second->fork.hold && to->second->fork.dirty && to->second->fork.reqf) {
                        send_fork(id, ref_pair.first);
                        to->second->fork.hold  = false;
                        to->second->fork.dirty = false;
                    }
                    // If the fork is not held, wait for a fork request and send a request
                    if (!to->second->fork.hold) {
                        while (!to->second->fork.reqf) {
                            std::unique_lock<std::mutex> lk(to->second->fork.lock);
                            to->second->fork.condition_req_fork.wait(lk);
                        }
                        send_fork_request(id, ref_pair.first);
                        to->second->fork.reqf = false;
                    }
                }

                // Wait for all forks to be held
                for (std::pair<int, Resource*> ref_pair : refs) {
                    auto to = std::find_if(graph[id].begin(), graph[id].end(), [ref_pair](std::pair<int, Resource*> pair) -> bool {return ref_pair.first == pair.first;});
                    if (!to->second->fork.hold && !to->second->fork.reqf) {
                        std::unique_lock<std::mutex> lk(to->second->fork.lock);
                        to->second->fork.condition_fork.wait(lk);
                    }
                }

                // Transition to EATING state
                dineState[id] = Dine::EATING;
                break;

            // Case when the philosopher is in EATING state
            case Dine::EATING:
                // Set the forks as dirty after eating
                for (std::pair<int, Resource*> ref_pair : refs) {
                    auto to = std::find_if(graph[id].begin(), graph[id].end(), [ref_pair](std::pair<int, Resource*> pair) -> bool {return ref_pair.first == pair.first;});
                    to->second->fork.dirty = true; 
                }
                // If the philosopher is not THIRSTY, transition to THINKING state
                if (drinkState[id] != Drink::THIRSTY) {
                    dineState[id] = Dine::THINKING;
                }
                break;
        }
    }
    return nullptr;
}
// Function to send a fork request from one philosopher to another
void send_fork_request(long from, long to) {
    // Find the reverse edge in the graph to access the target philosopher's resource
    auto it = std::find_if(graph[to].begin(), graph[to].end(), [from](std::pair<int, Resource*> ref_pair) -> bool {
        return from == ref_pair.first;
    });

    // Check if the reverse edge exists
    if (it == graph[to].end()) {
        std::cerr << "WARN: reverse edge for <" << from << "> not found" << std::endl;
        return;
    }

    // Set the fork request flag and notify the target philosopher
    it->second->fork.reqf = true;
    it->second->fork.condition_req_fork.notify_one();
}

// Function to send a fork from one philosopher to another
void send_fork(long from, long to) {
    // Find the reverse edge in the graph to access the target philosopher's resource
    auto it = std::find_if(graph[to].begin(), graph[to].end(), [from](std::pair<int, Resource*> ref_pair) -> bool {
        return from == ref_pair.first;
    });

    // Check if the reverse edge exists
    if (it == graph[to].end()) {
        std::cerr << "WARN: reverse edge for <" << from << "> not found" << std::endl;
        return;
    }

    // Set the fork as not dirty and hold, then notify the target philosopher
    it->second->fork.dirty = false;
    it->second->fork.hold = true;
    it->second->fork.condition_fork.notify_one();
}

// Function to send a bottle request from one philosopher to another
void send_bottle_request(long from, long to) {
    // Find the reverse edge in the graph to access the target philosopher's resource
    auto it = std::find_if(graph[to].begin(), graph[to].end(), [from](std::pair<int, Resource*> ref_pair) -> bool {
        return from == ref_pair.first;
    });

    // Check if the reverse edge exists
    if (it == graph[to].end()) {
        std::cerr << "WARN: reverse edge for <" << from << "> not found" << std::endl;
        return;
    }

    // Lock the bottle resource, set the bottle request flag, notify the target philosopher, and unlock the bottle
    it->second->bottle.lock.lock();
    it->second->bottle.reqb = true;
    it->second->bottle.condition.notify_one();
    it->second->bottle.lock.unlock();
}

// Function to send a bottle from one philosopher to another
void send_bottle(long from, long to) {
    // Find the reverse edge in the graph to access the target philosopher's resource
    auto it = std::find_if(graph[to].begin(), graph[to].end(), [from](std::pair<int, Resource*> ref_pair) -> bool {
        return from == ref_pair.first;
    });

    // Check if the reverse edge exists
    if (it == graph[to].end()) {
        std::cerr << "WARN: reverse edge for <" << from << "> not found" << std::endl;
        return;
    }

    // Lock the bottle resource, set the bottle as held, and unlock the bottle
    it->second->bottle.lock.lock();
    it->second->bottle.hold = true;
    it->second->bottle.lock.unlock();
}
// Function to simulate tranquil time for a philosopher
void tranquil(long id) {
    // Sleep for a random duration within the tranquil time range
    usleep(static_cast<useconds_t>(TRANQUIL_MIN + rand_r(&rand_seeds[id]) % TRANQUIL_RANGE));
}

// Function to simulate drinking time for a philosopher
void drink(long id) {
    // Sleep for a random duration within the drinking time range
    usleep(static_cast<useconds_t>(DRINKING_MIN + rand_r(&rand_seeds[id]) % DRINKING_RANGE));
}

int main(int argc, char **argv) {
    // Initialize the dining graph based on command line arguments
    graph = graph_initialize(parser(argc, argv));

    // Debug information display
    if (debug) {
        std::cout << "press any key to continue." << std::endl;
        getchar();

        std::cout << "graph initialization:" << std::endl;
        for (int i = 0; i < graph.size(); i++) {
            std::cout << i << ": ";
            for (const auto &adjacent : graph[i]) {
                std::cout << adjacent.first << " (" << adjacent.second << ") ";
            }
            std::cout << std::endl;
        }
        printf("CONFIG: %d philosophers DRINK  %d times.\n\n", p_cnt, count_session);
    }

    // Initialize vectors to track the state of philosophers' dining and drinking
    dineState.resize(static_cast<unsigned long>(p_cnt), Dine::THINKING);
    drinkState.resize(static_cast<unsigned long>(p_cnt), Drink::TRANQUIL);

    // Initialize an array of threads for philosophers
    pthread_t threads[p_cnt];

    // Initialize random seeds and threads for philosophers
    rand_seeds.resize(static_cast<unsigned long>(p_cnt));
    srand(static_cast<unsigned int>(time(nullptr)));
    for (long i = 0; i < p_cnt; i++) {
        pthread_create(&threads[i], nullptr, philosopher, (void *) i);
        rand_seeds[i] = static_cast<unsigned int>(rand());
    }

    // Signal the start of simulation
    start = true;

    // Wait for all philosopher threads to finish
    for (int i = 0; i < p_cnt; i++) {
        pthread_join(threads[i], nullptr);
    }

    return 0;
}

