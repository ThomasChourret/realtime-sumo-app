#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <sys/mman.h>   // mmap, PROT_READ, PROT_WRITE, MAP_SHARED, MAP_FAILED
#include <sys/stat.h>   // permissions (0666)
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <unistd.h>     // ftruncate

#include <libsumo/libtraci.h>

//#include "Connection.h"

using namespace libtraci;

char* light_state = nullptr;
int* edges_state = nullptr;

std::vector<std::string> EdgesIDs = {"-E3", "E2", "-E4"};

void createSharedMemory() {
    // Mémoire pour le feu
    int shm_light = shm_open("light", O_CREAT | O_RDWR, 0666);
    if (shm_light == -1) {
        std::cerr << "Erreur lors de la création de la mémoire partagée pour le feu" << std::endl;
        return;
    }
    ftruncate(shm_light, sizeof(char) * 7);
    light_state = (char*)mmap(0, sizeof(char) * 6, PROT_READ | PROT_WRITE, MAP_SHARED, shm_light, 0);
    if (light_state == MAP_FAILED) {
        std::cerr << "Erreur mmap pour le feu" << std::endl;
        return;
    }

    // Mémoire pour les voies
    int shm_edges = shm_open("edges", O_CREAT | O_RDWR, 0666);
    if (shm_edges == -1) {
        std::cerr << "Erreur lors de la création de la mémoire partagée pour les voies" << std::endl;
        return;
    }
    ftruncate(shm_edges, sizeof(int) * 3);
    edges_state = (int*)mmap(0, sizeof(int) * 3, PROT_READ | PROT_WRITE, MAP_SHARED, shm_edges, 0);
    if (edges_state == MAP_FAILED) {
        std::cerr << "Erreur mmap pour les voies" << std::endl;
        return;
    }

    // Initialisation des valeurs
    for (int i = 0; i < 6; ++i) {
        light_state[i] = 'r';
    }
    light_state[6] = '\0'; // Terminateur de chaîne
    for (int i = 0; i < 3; ++i) {
        edges_state[i] = 0;
    }
}

int countVehiclesOnEdge(const std::string& edgeID) {
    return Edge::getLastStepVehicleIDs(edgeID).size();
}

void analyzeTraffic() {
    for (size_t i = 0; i < EdgesIDs.size(); ++i) {
        const auto& edge = EdgesIDs[i];
        //std::cout << "Analyzing edge: " << edge << std::endl;
        edges_state[i] = countVehiclesOnEdge(edge);
    }
}

void updateLightState() {
    //std::cout << "Updating light state: " << light_state << std::endl;
    TrafficLight::setRedYellowGreenState("J4", light_state);
}

int main(int argc, char* argv[]) {
    createSharedMemory();

    Simulation::start({
        "sumo-gui",
        "-n", "test.net.xml",
        "-r", "test.rou.xml",
        "--num-clients", "1",
        "--step-length", "0.05",
        "--junction-taz",
        "--start"
    }, 8888);

    const std::string trafficLightID = "J4";
    TrafficLight::setRedYellowGreenState(trafficLightID, "rrrrrr");

    while (true) {
        Simulation::step();
        analyzeTraffic();
        updateLightState();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    Simulation::close();
    return 0;
}
